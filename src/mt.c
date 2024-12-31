#include "thread.h"
#include "files.h"

int main(int argc, char** argv) {
    if (argc > 1) { return 1; }
    SET_HIGH_CLASS();

    Files files = get_files(argv[0]);
    
    char** seqs = (char**)mat_aligned_alloc(CACHE_LINE, sizeof(char*) * BATCH_SIZE);
    char** other_data = (char**)mat_aligned_alloc(CACHE_LINE, sizeof(char*) * BATCH_SIZE);
    for (int i = 0; i < BATCH_SIZE; i++) {
        seqs[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
        other_data[i] = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    }

    char* current = files.file_data;
    char* end = files.file_data + files.data_size;
    
    current = skip_header(current, end);

    size_t* seq_lens = (size_t*)mat_aligned_alloc(CACHE_LINE, sizeof(size_t) * BATCH_SIZE);
    size_t seq_count = 1;

    init_format();
    init_thread_pool();
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    double start = get_time();

    seq_lens[0] = parse_csv_line(&current, seqs[0], other_data[0]);
    while (current < end && *current) {
        while (seq_count < BATCH_SIZE && current < end && *current) {
            seq_lens[seq_count] = parse_csv_line(&current, seqs[seq_count], other_data[seq_count]);
            seq_count++;
        }

        // Process batch
        size_t num_pairs = seq_count - 1;
        AlignTask* tasks = (AlignTask*)malloc(sizeof(AlignTask) * num_pairs);
        Alignment* results = (Alignment*)malloc(sizeof(Alignment) * num_pairs);

        // Setup tasks
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

        for (int t = 0; t < g_num_threads; t++) sem_wait(g_thread_work[t].work_done);

        #if MODE_WRITE == 1
        for (size_t i = 0; i < num_pairs; i++) {
            if (UNLIKELY(files.writer.remaining < MAX_CSV_LINE * 4)) {
                flush_buffer(&files.writer);
            }

            Data prev = {seqs[i], other_data[i], seq_lens[i]};
            Data curr = {seqs[i + 1], other_data[i + 1], seq_lens[i + 1]};
            char* start_pos = files.writer.pos;
            char* end_pos = buffer_output(files.writer.pos, &prev, &curr, &results[i]);
            size_t written = (size_t)(end_pos - start_pos);
            files.writer.pos += written;
            files.writer.remaining -= written;
        }
        #endif
        // Keep last sequence for next batch
        strcpy(seqs[0], seqs[seq_count - 1]);
        strcpy(other_data[0], other_data[seq_count - 1]);
        seq_lens[0] = seq_lens[seq_count - 1];
        seq_count = 1;

        free(tasks);
        free(results);
    }

    #if MODE_WRITE == 1
    flush_buffer(&files.writer);
    #endif

    double endt = get_time();

    free_files(&files);

    for (int i = 0; i < BATCH_SIZE; i++) {
        mat_aligned_free(seqs[i]);
        mat_aligned_free(other_data[i]);
    }
    mat_aligned_free(seqs);
    mat_aligned_free(other_data);
    mat_aligned_free(scoring);
    mat_aligned_free(seq_lens);

    destroy_thread_pool();
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}