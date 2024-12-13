#include "args.h"

int main(int argc, char** argv) {
    PIN_THREAD(0);
    SET_HIGH_CLASS();

    Args args = parse_args(argc, argv);

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

        #if MODE_WRITE == 1
        if (UNLIKELY(args.writer.remaining < MAX_CSV_LINE * 4)) {
            flush_buffer(&args.writer);
        }

        Data prev = {prev_seq, prev_data, strlen(prev_seq)};
        Data curr = {seq, data, strlen(seq)};
        char* start_pos = args.writer.pos;
        char* end_pos = buffer_output(args.writer.pos, &prev, &curr, result);
        size_t written = (size_t)(end_pos - start_pos);
        args.writer.pos += written;
        args.writer.remaining -= written;
        #endif

        strcpy(prev_data, data);
        strcpy(prev_seq, seq);
    }

    #if MODE_WRITE == 1
    flush_buffer(&args.writer);
    #endif
    
    double endt = get_time();

    free_args(&args);

    mat_aligned_free(seq);
    mat_aligned_free(prev_seq);
    mat_aligned_free(scoring);
    mat_aligned_free(result);
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}


