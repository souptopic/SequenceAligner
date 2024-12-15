#include "args.h"

int main(int argc, char** argv) {
    PIN_THREAD(0);
    SET_HIGH_PRIORITY();

    Args args = parse_args(argc, argv);
    #ifdef MODE_WRITE
    int fd_out = args.fd_out;
    char* write_buffer = args.write_buffer;
    char* buf_pos = args.buf_pos;
    #endif

    char* seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    char* prev_seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    char* data = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    char* prev_data = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    
    char* current = args.file_data;
    char* end = args.file_data + args.data_size;
    
    current = skip_header(current, end);
    
	init_format();

    Alignment* result = (Alignment*)mat_aligned_alloc(CACHE_LINE, sizeof(Alignment));
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    double start = get_time();
    parse_csv_line(&current, prev_seq, prev_data);
    while (current < end && *current) {
        parse_csv_line(&current, seq, data);
        align_sequences(prev_seq, (int)strlen(prev_seq), seq, (int)strlen(seq), scoring, result);

        #ifdef MODE_WRITE
        if (UNLIKELY((size_t)(buf_pos - write_buffer) > WRITE_BUF - MAX_CSV_LINE * 4)) {
            write(fd_out, write_buffer, buf_pos - write_buffer);
            buf_pos = write_buffer;
        }

        Data prev = {prev_seq, prev_data, strlen(prev_seq)};
        Data curr = {seq, data, strlen(seq)};
        buf_pos = write_alignment_output(buf_pos, &prev, &curr, result);
        #endif

        strcpy(prev_data, data);
        strcpy(prev_seq, seq);
    }

    // Will be moved below write once buffered writes are implemented
    double endt = get_time();
    
    #ifdef MODE_WRITE
    write(fd_out, write_buffer, buf_pos - write_buffer);
    #endif

    free_args(&args);

    mat_aligned_free(seq);
    mat_aligned_free(prev_seq);
    mat_aligned_free(scoring);
    mat_aligned_free(result);
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}


