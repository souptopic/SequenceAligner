#include "utils.h"
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
    int label = 0;
    int prev_label;
    
    char* current = args.file_data;
    char* end = args.file_data + args.data_size;
    
    current = skip_header(current, end);
	
    init_length_lookup();

    parse1(&current, prev_seq, &prev_label);

    Alignment* result = (Alignment*)mat_aligned_alloc(CACHE_LINE, sizeof(Alignment));
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    double start = get_time();
    while (current < end && *current) {
        PREFETCH(current + MAX_LINE_SIZE * 2);

        #ifdef MODE_WRITE
        PREFETCH(buf_pos + MAX_LINE_SIZE * 2);
        #endif
        
        parse1(&current, seq, &label);
        align_sequences(prev_seq, (int)strlen(prev_seq), seq, (int)strlen(seq), scoring, result);

        #ifdef MODE_WRITE
        if (UNLIKELY((size_t)(buf_pos - write_buffer) > WRITE_BUFFER_SIZE - MAX_LINE_SIZE * 4)) {
            write(fd_out, write_buffer, buf_pos - write_buffer);
            buf_pos = write_buffer;
        }

        buf_pos = write_alignment_output(buf_pos, prev_seq, strlen(prev_seq), seq, strlen(seq), prev_label, label, result);
        #endif

        copy_full_sequence(prev_seq, seq);
        prev_label = label;
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


