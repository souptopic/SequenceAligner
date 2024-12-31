#include "thread.h"
#include "files.h"

#define MIN_BATCH_SIZE (4096)
#define MAX_BATCH_SIZE (524288)
#define TUNING_ROWS (4000000)

typedef struct {
    size_t batch_size;
    double time;
} BatchTiming;

INLINE BatchTiming measure_batch_performance(char* start, char* end, size_t batch_size, ScoringMatrix* scoring) {
    char* current = start;
    size_t rows_processed = 0;
    size_t seq_count = 1;
    
    char** seqs = (char**)mat_aligned_alloc(CACHE_LINE, sizeof(char*) * batch_size);
    char** other_data = (char**)mat_aligned_alloc(CACHE_LINE, sizeof(char*) * batch_size);
    for (int i = 0; i < batch_size; i++) {
        seqs[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
        other_data[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    }
    size_t* seq_lens = (size_t*)mat_aligned_alloc(CACHE_LINE, sizeof(size_t) * batch_size);
    
    double start_time = get_time();
    
    seq_lens[0] = parse_csv_line(&current, seqs[0], other_data[0]);
    while (current < end && rows_processed < TUNING_ROWS) {
        while (seq_count < batch_size && current < end && rows_processed < TUNING_ROWS) {
            seq_lens[seq_count] = parse_csv_line(&current, seqs[seq_count], other_data[seq_count]);
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
                .len1 = seq_lens[i],
                .len2 = seq_lens[i + 1],
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

        strcpy(seqs[0], seqs[seq_count - 1]);
        strcpy(other_data[0], other_data[seq_count - 1]);
        seq_lens[0] = seq_lens[seq_count - 1];
        seq_count = 1;

        free(tasks);
        free(results);
    }
    
    double time_taken = get_time() - start_time;
    
    for (int i = 0; i < batch_size; i++) {
        mat_aligned_free(seqs[i]);
        mat_aligned_free(other_data[i]);
    }
    mat_aligned_free(seqs);
    mat_aligned_free(other_data);
    mat_aligned_free(seq_lens);
    
    return (BatchTiming){batch_size, time_taken};
}

int main(int argc, char** argv) {
    if (argc > 1) { return 1; }
    SET_HIGH_CLASS();

    Files files = get_files(argv[0]);

    char* current = files.file_data;
    char* end = files.file_data + files.data_size;
    
    init_format();
    init_thread_pool();
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    printf("\nTesting batch sizes from %d to %d\n", MIN_BATCH_SIZE, MAX_BATCH_SIZE);
    printf("Batch Size\tTime (s)\tRows/sec\n");
    printf("-----------------------------------------\n");
    fflush(stdout);

    BatchTiming best = {0, 999999.0};
    
    for (size_t size = MIN_BATCH_SIZE; size <= MAX_BATCH_SIZE; size *= 2) {
        current = skip_header(files.file_data, end);
        BatchTiming timing = measure_batch_performance(current, end, size, scoring);
        double rows_per_sec = TUNING_ROWS / timing.time;
        printf("%8zu\t%.8f\t%.0f\n", size, timing.time, rows_per_sec);
        fflush(stdout);
        if (timing.time < best.time) best = timing;
    }
    
    printf("\nOptimal batch size: %zu (%.3f seconds)\n", best.batch_size, best.time);
    printf("Run this program multiple times to get a more accurate result\n");
    printf("\nYou can modify the Batch size to use this value\n");
    fflush(stdout);

    free_files(&files);

    mat_aligned_free(scoring);
    
    destroy_thread_pool();
    return 0;
}