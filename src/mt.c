#include "utils.h"
#include "args.h"

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

	Args args = parse_args(argc, argv);

	#ifdef MODE_WRITE
	int fd_out = args.fd_out;
	char* write_buffer = args.write_buffer;
	char* buf_pos = args.buf_pos;
	#endif
    
    char** seqs = (char**)malloc(sizeof(char*) * BATCH_SIZE);
    for (int i = 0; i < BATCH_SIZE; i++) seqs[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    int* labels = (int*)malloc(sizeof(int) * BATCH_SIZE);

    char* current = args.file_data;
    char* end = args.file_data + args.data_size;
    
    current = skip_header(current, end);

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

	// Will be moved below write once buffered writes are implemented
    double endt = get_time();

    #ifdef MODE_WRITE
    write(fd_out, write_buffer, buf_pos - write_buffer);
    #endif

	free_args(&args);

    for (int i = 0; i < BATCH_SIZE; i++) mat_aligned_free(seqs[i]);
    free(seqs);
    free(labels);
    mat_aligned_free(scoring);
    destroy_thread_pool();
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}