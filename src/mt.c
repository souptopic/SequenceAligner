#include "utils.h"
#include "thread.h"
#include "args.h"

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
            if ((size_t)(buf_pos - write_buffer) > WRITE_BUF - MAX_LINE * 4) {
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