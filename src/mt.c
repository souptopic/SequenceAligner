#include "thread.h"
#include "csv.h"

int main(int argc, char** argv) {
    if (argc > 1) { return 1; }
    SET_HIGH_CLASS();

    Files files = get_files(argv[0]);
    
    Sequence* seqs = (Sequence*)malloc(sizeof(Sequence) * BATCH_SIZE);
    OtherData* other = (OtherData*)malloc(sizeof(OtherData) * BATCH_SIZE);

    char* current = files.file_data;
    char* end = files.file_data + files.data_size;
    
    current = skip_header(current, end);

    size_t* seq_lens = (size_t*)malloc(sizeof(size_t) * BATCH_SIZE);
    size_t seq_count = 1;

    init_format();
    ScoringMatrix scoring;
    init_scoring_matrix(&scoring);
    init_thread_pool();

    double start = get_time();

    seq_lens[0] = parse_csv_line(&current, seqs[0].data, other[0].data);
    while (current < end && *current) {
        while (seq_count < BATCH_SIZE && current < end && *current) {
            seq_lens[seq_count] = parse_csv_line(&current, seqs[seq_count].data, other[seq_count].data);
            seq_count++;
        }

        // Process batch
        size_t num_pairs = seq_count - 1;
        AlignTask* tasks = (AlignTask*)malloc(sizeof(AlignTask) * num_pairs);
        Alignment* results = (Alignment*)malloc(sizeof(Alignment) * num_pairs);

        // Setup tasks
        for (size_t i = 0; i < num_pairs; i++) {
            tasks[i] = (AlignTask){
                .seq1 = seqs[i].data,
                .seq2 = seqs[i + 1].data,
                .len1 = seq_lens[i],
                .len2 = seq_lens[i + 1],
                .scoring = &scoring,
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
            if (files.writer.pos >= WRITE_BUF - MAX_CSV_LINE * 2) {
                flush_buffer(&files.writer);
            }

            Data prev = {seqs[i].data, other[i].data, seq_lens[i]};
            Data curr = {seqs[i + 1].data, other[i + 1].data, seq_lens[i + 1]};
            files.writer.pos += buffer_output(files.writer.buffer, files.writer.pos, &prev, &curr, &results[i]);
        }
        #endif
        // Keep last sequence for next batch
        strcpy(seqs[0].data, seqs[seq_count - 1].data);
        strcpy(other[0].data, other[seq_count - 1].data);
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
    free(seqs);
    free(other);
    free(seq_lens);

    destroy_thread_pool();
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}