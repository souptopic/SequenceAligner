#include "utils.h"

#define MAX_THREADS 16
#define BATCH_SIZE 65536

#ifdef _WIN32
#include <synchapi.h>
typedef HANDLE sem_t;
#define sem_init(sem, shared, value) (*sem = CreateSemaphore(NULL, value, LONG_MAX, NULL))
#define sem_post(sem) ReleaseSemaphore(*sem, 1, NULL)
#define sem_wait(sem) WaitForSingleObject(*sem, INFINITE)
#define sem_destroy(sem) CloseHandle(*sem)
#else
#include <semaphore.h>
#endif

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
            align_sequences(task->seq1, strlen(task->seq1), task->seq2, strlen(task->seq2), task->scoring, task->result);
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

int main(int argc, char** argv) {
    SET_HIGH_PRIORITY();

	if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    	printf("Usage: %s [input] [output]\n"
           "Input: CSV file (default: %s)\n"
           #ifdef MODE_WRITE
           "Output: CSV file (default: %s)\n"
           #endif
           , argv[0], INPUT_FILE_MT
           #ifdef MODE_WRITE
           , OUTPUT_FILE_MT
           #endif
		);
		return 0;
	}

	char base_path[MAX_PATH];
    strncpy(base_path, argv[0], MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';

	for (char* p = base_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

	char* last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

	last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

    char full_input_path[MAX_PATH];
    char full_output_path[MAX_PATH];

    snprintf(full_input_path, MAX_PATH, "%s/%s", base_path, INPUT_FILE_MT);
    snprintf(full_output_path, MAX_PATH, "%s/%s", base_path, OUTPUT_FILE_MT);

    const char* input_file = argv[1] ? argv[1] : full_input_path;
    
    #ifdef MODE_WRITE
    const char* output_file = argc > 2 ? argv[2] : full_output_path;
    #endif

    #ifdef _WIN32
    if (GetFileAttributesA(input_file) == INVALID_FILE_ATTRIBUTES) {
    #else
    if (access(input_file, F_OK) != 0) {
    #endif
        printf("Error: Input file '%s' does not exist\n", input_file);
        return 1;
    }

    #ifdef _WIN32
    HANDLE hFile = CreateFileA(input_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* file_data = (char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(hFile, &file_size);
    size_t data_size = file_size.QuadPart;
    #else
    int fd = open(input_file, O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    size_t data_size = sb.st_size;
    char* file_data = mmap(NULL, data_size, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(file_data, data_size, MADV_SEQUENTIAL);
    #endif
    
    #ifdef MODE_WRITE
    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    char* write_buffer = (char*)mat_aligned_alloc(CACHE_LINE, WRITE_BUFFER_SIZE);
    char* buf_pos = write_buffer;
    const char* header = "sequence1,sequence2,label1,label2,score,alignment\n";
    buf_pos = fast_strcpy(write_buffer, header, strlen(header));
    #endif
    
    char** seqs = (char**)malloc(sizeof(char*) * BATCH_SIZE);
    for (int i = 0; i < BATCH_SIZE; i++) seqs[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    int* labels = (int*)malloc(sizeof(int) * BATCH_SIZE);

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

    parse2(&current, seqs[0], &labels[0]);
    size_t seq_count = 1;

    init_thread_pool();
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    double start = get_time();
    while (current < end && *current) {
        while (seq_count < BATCH_SIZE && current < end && *current) {
            parse2(&current, seqs[seq_count], &labels[seq_count]);
            seq_count++;
        }

        // Process batch
        size_t num_pairs = seq_count - 1;
        AlignTask* tasks = malloc(sizeof(AlignTask) * num_pairs);
        Alignment* results = malloc(sizeof(Alignment) * num_pairs);

        // Setup tasks
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

        for (int t = 0; t < g_num_threads; t++) sem_wait(g_thread_work[t].work_done);

        #ifdef MODE_WRITE
        for (size_t i = 0; i < num_pairs; i++) {
            if ((size_t)(buf_pos - write_buffer) > WRITE_BUFFER_SIZE - MAX_LINE_SIZE * 4) {
                write(fd_out, write_buffer, buf_pos - write_buffer);
                buf_pos = write_buffer;
            }
            buf_pos = write_alignment_output(buf_pos, tasks[i].seq1, strlen(tasks[i].seq1), tasks[i].seq2, strlen(tasks[i].seq2), tasks[i].prev_label, tasks[i].label, &results[i]);
        }
        #endif

        // Keep last sequence for next batch
        memcpy(seqs[0], seqs[seq_count - 1], strlen(seqs[seq_count - 1]) + 1);
        labels[0] = labels[seq_count - 1];
        seq_count = 1;

        free(tasks);
        free(results);
    }

    double endt = get_time();

    #ifdef MODE_WRITE
    write(fd_out, write_buffer, buf_pos - write_buffer);
    close(fd_out);
    #endif

    for (int i = 0; i < BATCH_SIZE; i++) mat_aligned_free(seqs[i]);
    free(seqs);
    free(labels);
    
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
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}