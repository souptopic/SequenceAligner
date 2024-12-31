#include "csv.h"
#include "similarity.h"

int main(int argc, char** argv) {
    if (argc > 1) { return 1; }
    PIN_THREAD(0);
    SET_HIGH_CLASS();

    Files files = get_files(argv[0]);

    char seq[MAX_SEQ_LEN];
    char prev_seq[MAX_SEQ_LEN];
    char data[MAX_CSV_LINE - MAX_SEQ_LEN];
    char prev_data[MAX_CSV_LINE - MAX_SEQ_LEN];
    
    char* current = files.file_data;
    char* end = files.file_data + files.data_size;
    
    current = skip_header(current, end);
    
    init_format();
    ScoringMatrix scoring;
    init_scoring_matrix(&scoring);

    double start = get_time();

    size_t prev_len = parse_csv_line(&current, prev_seq, prev_data);
    while (current < end && *current) {
        size_t curr_len = parse_csv_line(&current, seq, data);
        Alignment result = align_sequences(prev_seq, prev_len, seq, curr_len, &scoring);
        if (result.score < -1000000000) {
            // Will never happen but prevents compiler from removing unused result when not writing
            printf("Unexpected score (-1000000000)!\n");
        }
        #if SIMILARITY_ANALYSIS == 1
        similarity_analysis(&result, curr_len);
        #endif

        #if MODE_WRITE == 1
        if (files.writer.pos >= WRITE_BUF - MAX_CSV_LINE * 2) {
            flush_buffer(&files.writer);
        }
        Data prev = {prev_seq, prev_data, prev_len};
        Data curr = {seq, data, curr_len};
        files.writer.pos += buffer_output(files.writer.buffer, files.writer.pos, &prev, &curr, &result);
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
    
    #if SIMILARITY_ANALYSIS == 1
    printf("Alignment and analysis time: %f seconds\n", endt - start);
    #else
    printf("Alignment time: %f seconds\n", endt - start);
    #endif
    return 0;
}


