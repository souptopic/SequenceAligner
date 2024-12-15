#include "utils.h"

#ifdef _WIN32
#include <synchapi.h>
typedef HANDLE sem_t;
#define sem_init(sem, shared, value) (*sem = CreateSemaphore(NULL, 0, LONG_MAX, NULL))
#define sem_post(sem) ReleaseSemaphore(*sem, 1, NULL)
#define sem_wait(sem) WaitForSingleObject(*sem, INFINITE)
#define sem_destroy(sem) CloseHandle(*sem)
#else
#include <semaphore.h>
#endif

#define MAX_THREADS 16
#define MIN_BATCH_SIZE 4096
#define MAX_BATCH_SIZE 524288
#define TUNING_ROWS 4000000

typedef struct {
    size_t batch_size;
    double time;
} BatchTiming;

typedef struct {
    const char* seq1;
    const char* seq2;
    int prev_label;
    int label;
    ScoringMatrix* scoring;
    Alignment* result;
} AlignTask;

typedef struct {
    AlignTask* tasks;
    size_t start;
    size_t end;
    sem_t* work_ready;
    sem_t* work_done;
    int active;
} ThreadWork;

static ThreadWork* g_thread_work;
static pthread_t* g_threads;
static int g_num_threads;

INLINE int get_num_threads(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors > MAX_THREADS ? MAX_THREADS : sysinfo.dwNumberOfProcessors;
    #else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return nprocs > MAX_THREADS ? MAX_THREADS : nprocs;
    #endif
}

INLINE T_Func thread_pool_worker(void* arg) {
    ThreadWork* work = (ThreadWork*)arg;

	int thread_id = work - g_thread_work;
	PIN_THREAD(thread_id);
    
    while (1) {
        sem_wait(work->work_ready);
        if (!work->active) break;
        
        for (size_t i = work->start; i < work->end; i++) {
            AlignTask* task = &work->tasks[i];
            align_sequences(task->seq1, strlen(task->seq1), 
                          task->seq2, strlen(task->seq2), 
                          task->scoring, task->result);
        }
        
        sem_post(work->work_done);
    }
	T_Ret(NULL);
}

INLINE void init_thread_pool(void) {
    g_num_threads = get_num_threads();
    g_threads = malloc(sizeof(pthread_t) * g_num_threads);
    g_thread_work = malloc(sizeof(ThreadWork) * g_num_threads);
    
    for (int t = 0; t < g_num_threads; t++) {
        g_thread_work[t].work_ready = malloc(sizeof(sem_t));
        g_thread_work[t].work_done = malloc(sizeof(sem_t));
        sem_init(g_thread_work[t].work_ready, 0, 0);
        sem_init(g_thread_work[t].work_done, 0, 0);
        g_thread_work[t].active = 1;
        pthread_create(&g_threads[t], NULL, thread_pool_worker, &g_thread_work[t]);
    }
}

INLINE void destroy_thread_pool(void) {
    for (int t = 0; t < g_num_threads; t++) {
        g_thread_work[t].active = 0;
        sem_post(g_thread_work[t].work_ready);
        pthread_join(g_threads[t], NULL);
        sem_destroy(g_thread_work[t].work_ready);
        sem_destroy(g_thread_work[t].work_done);
        free(g_thread_work[t].work_ready);
        free(g_thread_work[t].work_done);
    }
    free(g_thread_work);
    free(g_threads);
}

INLINE BatchTiming measure_batch_performance(char* start, char* end, size_t batch_size, ScoringMatrix* scoring) {
    char* current = start;
    size_t rows_processed = 0;
    size_t seq_count = 1;
    
    char** seqs = (char**)malloc(sizeof(char*) * batch_size);
    for (size_t i = 0; i < batch_size; i++) 
        seqs[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    int* labels = (int*)malloc(sizeof(int) * batch_size);
    
    parse2(&current, seqs[0], &labels[0]);
    
    double start_time = get_time();
    
    while (current < end && rows_processed < TUNING_ROWS) {
        while (seq_count < batch_size && current < end && rows_processed < TUNING_ROWS) {
            parse2(&current, seqs[seq_count], &labels[seq_count]);
            seq_count++;
            rows_processed++;
        }

        size_t num_pairs = seq_count - 1;
        AlignTask* tasks = malloc(sizeof(AlignTask) * num_pairs);
        Alignment* results = malloc(sizeof(Alignment) * num_pairs);

        for (size_t i = 0; i < num_pairs; i++) {
            tasks[i] = (AlignTask){
                .seq1 = seqs[i],
                .seq2 = seqs[i + 1],
                .prev_label = labels[i],
                .label = labels[i + 1],
                .scoring = scoring,
                .result = &results[i]
            };
        }

        size_t tasks_per_thread = num_pairs / g_num_threads;
        for (int t = 0; t < g_num_threads; t++) {
            g_thread_work[t].tasks = tasks;
            g_thread_work[t].start = t * tasks_per_thread;
            g_thread_work[t].end = (t == g_num_threads - 1) ? num_pairs : (t + 1) * tasks_per_thread;
            sem_post(g_thread_work[t].work_ready);
        }

        for (int t = 0; t < g_num_threads; t++) {
            sem_wait(g_thread_work[t].work_done);
        }

        free(tasks);
        free(results);
        seq_count = 1;
    }
    
    double time_taken = get_time() - start_time;
    
    for (size_t i = 0; i < batch_size; i++) 
        mat_aligned_free(seqs[i]);
    free(seqs);
    free(labels);
    
    return (BatchTiming){batch_size, time_taken};
}

int main(void) {
	SET_HIGH_PRIORITY();

    #ifdef _WIN32
    HANDLE hFile = CreateFileA("../testing/datasets/avpdb_mega.csv", GENERIC_READ, FILE_SHARE_READ, NULL, 
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* file_data = (char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(hFile, &file_size);
    size_t data_size = file_size.QuadPart;
    #else
    int fd = open("../testing/datasets/avpdb_mega.csv", O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    size_t data_size = sb.st_size;
    char* file_data = mmap(NULL, data_size, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(file_data, data_size, MADV_SEQUENTIAL);
    #endif

    char* current = file_data;
    char* end = file_data + data_size;
    
    // Skip header
    __m256i newline = _mm256_set1_epi8('\n');
    while (current < end) {
        __m256i data = _mm256_loadu_si256((__m256i*)current);
        int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(data, newline));
        if (mask) {
            current += __builtin_ctz(mask) + 1;
            break;
        }
        current += 32;
    }
    
	init_length_lookup();
    init_thread_pool();
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    printf("Testing batch sizes from %d to %d\n", MIN_BATCH_SIZE, MAX_BATCH_SIZE);
    printf("Batch Size\tTime (s)\tRows/sec\n");
    printf("-----------------------------------------\n");

    BatchTiming best = {0, 999999.0};
    
    for (size_t size = MIN_BATCH_SIZE; size <= MAX_BATCH_SIZE; size *= 2) {
        BatchTiming timing = measure_batch_performance(current, end, size, scoring);
        double rows_per_sec = TUNING_ROWS / timing.time;
        printf("%8zu\t%.8f\t%.0f\n", size, timing.time, rows_per_sec);
        
        if (timing.time < best.time) {
            best = timing;
        }
    }
    
    printf("\nOptimal batch size: %zu (%.3f seconds)\n", best.batch_size, best.time);
	printf("Run this program multiple times to get a more accurate result\n");
	printf("\nYou can modify the BATCH_SIZE in src/mt.c to use this value\n");

    mat_aligned_free(scoring);
    
    #ifdef _WIN32
    UnmapViewOfFile(file_data);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    #else
    munmap(file_data, data_size);
    close(fd);
    #endif
    
    destroy_thread_pool();
    return 0;
}