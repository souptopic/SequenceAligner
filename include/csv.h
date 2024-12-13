#ifndef CSV_H
#define CSV_H

#include "align.h"

typedef struct {
    const char* seq;
    const char* other_data;
    size_t      len;
} Data;

typedef struct {
    size_t  seq_pos;
    size_t* data_pos;
    size_t  data_count;
} ReadFormat;

typedef struct {
    size_t  seq1_pos;
    size_t  seq2_pos;
    size_t  score_pos;
    size_t  align_pos;
    size_t* data1_pos;
    size_t* data2_pos;
    size_t  data_count;
} WriteFormat;

static ReadFormat read_format;
static WriteFormat write_format;

INLINE void init_format() {
    // Read format analysis and validation
    if (!strstr(READ_CSV_HEADER, ",") || !strstr(READ_CSV_HEADER, "\n")) {
        fprintf(stderr, "Error: Invalid read CSV header format\n");
        exit(1);
    }

    char* header = READ_CSV_HEADER;
    size_t read_cols = 1;
    for(const char* p = header; *p && *p != '\n'; p++) {
        if(*p == ',') read_cols++;
    }
    
    read_format.seq_pos = READ_CSV_SEQ_POS;
    read_format.data_count = read_cols - 1;
    read_format.data_pos = malloc(sizeof(size_t) * read_format.data_count);

    if (read_format.seq_pos >= read_cols) {
        fprintf(stderr, "Error: Read sequence column position out of bounds\n");
        exit(1);
    }
    
    size_t idx = 0;
    for(size_t i = 0; i < read_cols; i++) {
        if(i != read_format.seq_pos) {
            read_format.data_pos[idx++] = i;
        }
    }

    bool* read_used = calloc(read_cols, sizeof(bool));
    if (!read_used) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    
    read_used[read_format.seq_pos] = true;
    for (size_t i = 0; i < read_format.data_count; i++) {
        if (read_format.data_pos[i] >= read_cols) {
            fprintf(stderr, "Error: Read data column position out of bounds\n");
            free(read_used);
            exit(1);
        }
        if (read_used[read_format.data_pos[i]]) {
            fprintf(stderr, "Error: Duplicate column position in read format\n");
            free(read_used);
            exit(1);
        }
        read_used[read_format.data_pos[i]] = true;
    }

    free(read_used);

    #if MODE_WRITE == 1
    // Write format analysis and validation
    if (!strstr(WRITE_CSV_HEADER, ",") || !strstr(WRITE_CSV_HEADER, "\n")) {
        fprintf(stderr, "Error: Invalid write CSV header format\n");
        exit(1);
    }

    const char *first = strstr(WRITE_CSV_ALIGN_FMT, "%s");
    if (!first) {
        fprintf(stderr, "Error: Format string must contain two %%s specifiers\n");
        exit(1);
    }

    const char *second = strstr(first + 2, "%s");
    if (!second) {
        fprintf(stderr, "Error: Format string must contain two %%s specifiers\n");
        exit(1);
    }

    if (strstr(second + 2, "%s")) {
        fprintf(stderr, "Error: Format string must contain exactly two %%s specifiers\n");
        exit(1);
    }

    header = WRITE_CSV_HEADER;
    size_t write_cols = 1;
    for(const char* p = header; *p && *p != '\n'; p++) {
        if(*p == ',') write_cols++;
    }
    
    write_format.seq1_pos = WRITE_CSV_SEQ1_POS;
    write_format.seq2_pos = WRITE_CSV_SEQ1_POS + 1;
    write_format.score_pos = WRITE_CSV_SCORE_POS;
    write_format.align_pos = WRITE_CSV_ALIGN_POS;
    
    write_format.data_count = read_format.data_count;
    write_format.data1_pos = malloc(sizeof(size_t) * write_format.data_count);
    write_format.data2_pos = malloc(sizeof(size_t) * write_format.data_count);
    
    bool* write_used = calloc(write_cols, sizeof(bool));
    if (!write_used) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }

    write_used[write_format.seq1_pos] = true;
    write_used[write_format.seq2_pos] = true;
    write_used[write_format.score_pos] = true;
    write_used[write_format.align_pos] = true;
    
    idx = 0;
    for(size_t i = 0; i < write_cols && idx < write_format.data_count; i++) {
        if(!write_used[i]) {
            write_format.data1_pos[idx] = i;
            for(size_t j = i + 1; j < write_cols && idx < write_format.data_count; j++) {
                if(!write_used[j]) {
                    write_format.data2_pos[idx] = j;
                    write_used[i] = write_used[j] = true;
                    idx++;
                    break;
                }
            }
        }
    }

    free(write_used);

    if (write_format.data_count * 2 > write_cols - 4) {
        fprintf(stderr, "Error: Not enough columns for data pairs\n");
        exit(1);
    }

    if (write_format.seq1_pos >= write_cols || 
        write_format.seq2_pos >= write_cols ||
        write_format.score_pos >= write_cols ||
        write_format.align_pos >= write_cols) {
        fprintf(stderr, "Error: Column position out of bounds\n");
        exit(1);
    }

    for(size_t i = 0; i < write_format.data_count; i++) {
        if (write_format.data1_pos[i] >= write_format.data2_pos[i]) {
            fprintf(stderr, "Error: Data column from first row must come before column from second row for each pair\n");
            exit(1);
        }
    }

    for(size_t i = 0; i < write_format.data_count; i++) {
        size_t pos1 = write_format.data1_pos[i];
        size_t pos2 = write_format.data2_pos[i];
        if ((pos1 < write_format.seq1_pos && pos2 > write_format.seq1_pos) ||
            (pos1 < write_format.seq2_pos && pos2 > write_format.seq2_pos) ||
            (pos1 < write_format.score_pos && pos2 > write_format.score_pos) ||
            (pos1 < write_format.align_pos && pos2 > write_format.align_pos)) {
            fprintf(stderr, "Error: Data pairs cannot be split by sequence/score/alignment columns\n");
            exit(1);
        }
    }

    if (write_format.seq1_pos == write_format.seq2_pos ||
        write_format.seq1_pos == write_format.score_pos ||
        write_format.seq1_pos == write_format.align_pos ||
        write_format.seq2_pos == write_format.score_pos ||
        write_format.seq2_pos == write_format.align_pos ||
        write_format.score_pos == write_format.align_pos) {
        fprintf(stderr, "Error: Special columns cannot share same position\n");
        exit(1);
    }

    if (MAX_CSV_LINE < 32 || MAX_SEQ_LEN < 1) {
        fprintf(stderr, "Error: Maximum length constants too low\n");
        exit(1);
    }

    #endif
}

INLINE char* parse_csv_line(char** current, char* seq, char* other_data) {
    char* p = *current;
    char* write_pos = NULL;
    size_t col = 0;
    size_t data_write_pos = 0;

    while (*p && (*p == ' ' || *p == '\r' || *p == '\n')) p++;

    #ifdef USE_AVX
    const veci_t delim_vec = set1_epi8(',');
    const veci_t nl_vec = set1_epi8('\n');
    const veci_t cr_vec = set1_epi8('\r');

    while (*p && *p != '\n' && *p != '\r') {
        write_pos = (col == read_format.seq_pos) ? seq : other_data + data_write_pos;
        while (*p && *p != ',' && *p != '\n' && *p != '\r') {
            veci_t data = loadu((veci_t*)p);
            veci_t is_delim = or_si(
                or_si(
                    cmpeq_epi8(data, delim_vec),
                    cmpeq_epi8(data, nl_vec)
                ),
                cmpeq_epi8(data, cr_vec)
            );
            num_t mask = movemask_epi8(is_delim);

            if (mask) {
                num_t pos = ctz(mask);
                storeu((veci_t*)write_pos, data);
                write_pos[pos] = '\0';
                write_pos += pos;
                p += pos;
                break;
            }

            storeu((veci_t*)write_pos, data);
            p += BYTES;
            write_pos += BYTES;
            
            PREFETCH(p + 256);
        }
    #else
    while (*p && *p != '\n' && *p != '\r') {
        write_pos = (col == read_format.seq_pos) ? seq : other_data + data_write_pos;
        
        while (*p && *p != ',' && *p != '\n' && *p != '\r') {
            *write_pos++ = *p++;
        }
    #endif

        *write_pos = '\0';
        
        if (col != read_format.seq_pos) {
            data_write_pos = write_pos - other_data + 1;
        }
        
        if (*p == ',') { p++; col++; }
    }
    
    while (*p && (*p == '\n' || *p == '\r')) p++;
    *current = p;
    return p;
}

INLINE char* buffer_output(char* buf_pos, const Data* prev, const Data* curr, const Alignment* result) {
    size_t written = 0;
    const size_t max_write = MAX_CSV_LINE * 4;
    char* start = buf_pos;

    #ifdef __AVX2__
    const size_t* positions[] = {
        &write_format.seq1_pos,
        &write_format.seq2_pos,
        &write_format.score_pos,
        &write_format.align_pos
    };
    
    for (size_t col = 0; col < 4 + write_format.data_count * 2; col++) {
        if (col > 0) *buf_pos++ = ',';

        if (col == *positions[0]) {
            buf_pos = fast_strcpy(buf_pos, prev->seq, prev->len);
        }
        else if (col == *positions[1]) {
            buf_pos = fast_strcpy(buf_pos, curr->seq, curr->len);
        }
        else if (col == *positions[2]) {
            buf_pos = int_to_str(buf_pos, result->score);
        }
        else if (col == *positions[3]) {
            const char* align_fmt = WRITE_CSV_ALIGN_FMT;
            size_t fmt_len = strlen(align_fmt);
            size_t seq1_len = strlen(result->seq1_aligned);
            size_t seq2_len = strlen(result->seq2_aligned);
            while (*align_fmt) {
                if (align_fmt[0] == '%' && align_fmt[1] == 's') {
                    if (!written) {
                        buf_pos = fast_strcpy(buf_pos, result->seq1_aligned, seq1_len);
                        written = 1;
                    } else {
                        buf_pos = fast_strcpy(buf_pos, result->seq2_aligned, seq2_len);
                    }
                    align_fmt += 2;
                } else {
                    *buf_pos++ = *align_fmt++;
                }
            }
        }
        else {
            for (size_t i = 0; i < write_format.data_count; i++) {

                if (col == write_format.data1_pos[i]) {
                    buf_pos = fast_strcpy(buf_pos, prev->other_data, strlen(prev->other_data));
                    break;
                }

                else if (col == write_format.data2_pos[i]) {
                    buf_pos = fast_strcpy(buf_pos, curr->other_data, strlen(curr->other_data));
                    break;
                }
            }
        }
    }

    #else

    bool handled = false;
    
    for (size_t col = 0; col < 4 + write_format.data_count * 2; col++) {

        if (col > 0) *buf_pos++ = ',';

        if (col == write_format.seq1_pos) {
            buf_pos = fast_strcpy(buf_pos, prev->seq, prev->len);
            handled = true;
        }

        else if (col == write_format.seq2_pos) {
            buf_pos = fast_strcpy(buf_pos, curr->seq, curr->len);
            handled = true;
        }

        else if (col == write_format.score_pos) {
            buf_pos = int_to_str(buf_pos, result->score);
            handled = true;
        }

        else if (col == write_format.align_pos) {
            const char* fmt = WRITE_CSV_ALIGN_FMT;
            written = 0;
            
            while (*fmt) {
                if (fmt[0] == '%' && fmt[1] == 's') {
                    if (!written) {
                        buf_pos = fast_strcpy(buf_pos, result->seq1_aligned, strlen(result->seq1_aligned));
                        written = 1;
                    } else {
                        buf_pos = fast_strcpy(buf_pos, result->seq2_aligned, strlen(result->seq2_aligned));
                    }
                    fmt += 2;
                } else {
                    *buf_pos++ = *fmt++;
                }
            }
            handled = true;
        }

        if (!handled) {
            for (size_t i = 0; i < write_format.data_count; i++) {
                if (col == write_format.data1_pos[i]) {
                    buf_pos = fast_strcpy(buf_pos, prev->other_data, strlen(prev->other_data));
                    break;
                }
                else if (col == write_format.data2_pos[i]) {
                    buf_pos = fast_strcpy(buf_pos, curr->other_data, strlen(curr->other_data));
                    break;
                }
            }
        }
        handled = false;
    }
    #endif

    *buf_pos++ = '\n';
    *buf_pos = '\0';
    return buf_pos;
}

#endif