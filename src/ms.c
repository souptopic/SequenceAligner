#include "files.h"
#include "similarity.h"

int main(int argc, char** argv) {
    if (argc > 1) { return 1; }
    PIN_THREAD(0);
    SET_HIGH_CLASS();

    Files files = get_files(argv[0]);

    char* seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    char* prev_seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    char* data = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    char* prev_data = (char*)mat_aligned_alloc(CACHE_LINE, MAX_CSV_LINE - MAX_SEQ_LEN);
    
    char* current = files.file_data;
    char* end = files.file_data + files.data_size;
    
    current = skip_header(current, end);
    
    init_format();
    Alignment* result = (Alignment*)mat_aligned_alloc(CACHE_LINE, sizeof(Alignment));
    ScoringMatrix* scoring = (ScoringMatrix*)mat_aligned_alloc(CACHE_LINE, sizeof(ScoringMatrix));
    init_scoring_matrix(scoring);

    double start = get_time();

    size_t prev_len = parse_csv_line(&current, prev_seq, prev_data);
    while (current < end && *current) {
        size_t curr_len = parse_csv_line(&current, seq, data);
        align_sequences(prev_seq, prev_len, seq, curr_len, scoring, result);
        #if SIMILARITY_ANALYSIS == 1
        similarity_analysis(result, curr_len);
        #endif

        #if MODE_WRITE == 1
        if (UNLIKELY(files.writer.remaining < MAX_CSV_LINE * 4)) {
            flush_buffer(&files.writer);
        }

        Data prev = {prev_seq, prev_data, prev_len};
        Data curr = {seq, data, curr_len};
        char* start_pos = files.writer.pos;
        char* end_pos = buffer_output(files.writer.pos, &prev, &curr, result);
        size_t written = (size_t)(end_pos - start_pos);
        files.writer.pos += written;
        files.writer.remaining -= written;
        #endif

        strcpy(prev_data, data);
        strcpy(prev_seq, seq);
        prev_len = curr_len;
    }

    #if MODE_WRITE == 1
    flush_buffer(&files.writer);
    #endif
    
    double endt = get_time();

    free_files(&files);

    mat_aligned_free(seq);
    mat_aligned_free(prev_seq);
    mat_aligned_free(scoring);
    mat_aligned_free(result);
    
    #if SIMILARITY_ANALYSIS == 1
    printf("Alignment and analysis time: %f seconds\n", endt - start);
    #else
    printf("Alignment time: %f seconds\n", endt - start);
    #endif
    return 0;
}


