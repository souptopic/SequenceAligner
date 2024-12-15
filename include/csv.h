#ifndef UTILS_H
#define UTILS_H

#include "align.h"

#define MAX_CSV_LINE (256)

/* Format for the CSV file that is being read.
 * The CSV lines must be consistent (i.e. same number of columns and same data types in each column).
 * A CSV line must only have one sequence, where the index must be provided.
 * It can contain any number other data columns, as those will be ignored. */
#define READ_CSV_HEADER "sequence,label\n"
#define READ_CSV_SEQ_POS (0)

/* Format for the CSV file that is being written.
 * Here, you can choose what the result CSV will look like.
 * You must provide indexes for the sequences, score and alignment.
 * If, for example, the fifth line is being read, that means SEQ1 is your 5th sequence, SEQ2 will be the 6th.
 * The unimportant data columns will be copied so that empty columns are filled first, then at the end.
 * You can use as much unimportant data columns as you want, however the results data columns must be 2x read data columns */
#ifdef MODE_WRITE
#define RESULT_CSV_HEADER "sequence1,sequence2,label1,label2,score,alignment\n"
#define RESULT_CSV_SEQ1_POS (0)
#define RESULT_CSV_SEQ2_POS (1)
#define RESULT_CSV_SCORE_POS (4)
#define RESULT_CSV_ALIGN_POS (5)
#define RESULT_CSV_ALIGN_FORMAT "\"('%s', '%s')\""
#define ALIGN_STRL (3 + SEQ_BUF + 4 + SEQ_BUF + 3) // SEQ_BUF = MAX_SEQ_LEN * 2 // because of extra possible '-' when aligning
#endif

/* The result of this example format will be exactly like this
READ_CSV
sequence,data
KPVSLS,0
LNNSRA,0
HCKFWF,1
...

RESULT_CSV
sequence1,sequence2,data1,data2,score,alignment
KPVSLS,LNNSRA,0,0,-5,"('KPVSLS', 'LNNSRA')"
LNNSRA,HCKFWF,0,1,-14,"('LNNSRA', 'HCKFWF')"
...

If we say that result positions are like this:
SEQ1 = 0, SEQ2 = 1, SCORE = 2, ALIGN = 3
Then the result will be like this:
KPVSLS,LNNSRA,-5,"('KPVSLS', 'LNNSRA')",0,0

You can see that the data columns were copied to the last column untouched.
You can have as many data columns as you want, they will be copied to whatever empty space is available.
However, the result format must have two times the data columns (data1, data2)
This is because the algorithm aligns the current sequence with the next one and must write the data for both sequences.
*/

typedef struct {
    const char* seq;
    const char* other_data;
    size_t len;
} Data;

typedef struct {
    size_t seq_pos;
    size_t* data_pos;
    size_t data_count;
} ReadFormat;

typedef struct {
    size_t seq1_pos;
    size_t seq2_pos;
    size_t score_pos;
    size_t align_pos;
    size_t* data1_pos;
    size_t* data2_pos;
    size_t data_count;
} WriteFormat;

static ReadFormat read_format;
static WriteFormat write_format;

INLINE char* skip_header(char* current, char* end) {
    __m256i newline = _mm256_set1_epi8('\n');
    while (current < end) {
        __m256i data = _mm256_loadu_si256((__m256i*)current);
        int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(data, newline));
        if (mask) {
            return current + __builtin_ctz(mask) + 1;
        }
        current += 32;
    }
    return current;
}

INLINE char* fast_strcpy(char* dst, const char* src, size_t len) {
    size_t i = 0;
    if (len >= 32) {
        for (; i <= len-32; i += 32) {
            _mm256_storeu_si256((__m256i*)(dst+i), _mm256_loadu_si256((__m256i*)(src+i)));
        }
    }
    if (len & 16) {
        _mm_storeu_si128((__m128i*)(dst+i), _mm_loadu_si128((__m128i*)(src+i)));
        i += 16;
    }
    if (len & 8) {
        *(uint64_t*)(dst+i) = *(uint64_t*)(src+i);
        i += 8;
    }
    if (len & 4) {
        *(uint32_t*)(dst+i) = *(uint32_t*)(src+i);
        i += 4;
    }
    if (len & 2) {
        *(uint16_t*)(dst+i) = *(uint16_t*)(src+i);
        i += 2;
    }
    if (len & 1) {
        dst[i] = src[i];
    }
    return dst + len;
}

INLINE char* int_to_str(char* str, int num) {
    char* ptr = str;
    uint32_t n = (num ^ (num >> 31)) - (num >> 31); // Convert to unsigned with abs
    uint32_t neg = (num >> 31) & 1;
    *ptr = '-';
    ptr += neg;
    
    // Buffer for digits (max 10 digits for 32-bit int + sign)
    char digits[12];
    char* digit_ptr = digits;
    
    if (n == 0) {
        *ptr++ = '0';
        return ptr;
    }
    
    // Store digits in forward order
    do {
        uint32_t q = n / 10;
        uint32_t r = n - (q * 10); // mod
        *digit_ptr++ = (char)('0' + r);
        n = q;
    } while (n);
    
    // Copy digits in reverse order
    while (digit_ptr > digits) {
        *ptr++ = *--digit_ptr;
    }
    
    return ptr;
}

INLINE void init_format() {
    // Read format analysis
    char* header = READ_CSV_HEADER;
    size_t read_cols = 1;
    for(const char* p = header; *p && *p != '\n'; p++) {
        if(*p == ',') read_cols++;
    }
    
    read_format.seq_pos = READ_CSV_SEQ_POS;
    read_format.data_count = read_cols - 1;
    read_format.data_pos = malloc(sizeof(size_t) * read_format.data_count);
    
    // Map all non-sequence columns as data
    size_t idx = 0;
    for(size_t i = 0; i < read_cols; i++) {
        if(i != read_format.seq_pos) {
            read_format.data_pos[idx++] = i;
        }
    }

    // Write format analysis
    header = RESULT_CSV_HEADER;
    size_t write_cols = 1;
    for(const char* p = header; *p && *p != '\n'; p++) {
        if(*p == ',') write_cols++;
    }
    
    write_format.seq1_pos = RESULT_CSV_SEQ1_POS;
    write_format.seq2_pos = RESULT_CSV_SEQ2_POS;
    write_format.score_pos = RESULT_CSV_SCORE_POS;
    write_format.align_pos = RESULT_CSV_ALIGN_POS;
    
    // Calculate how many data column pairs we need
    write_format.data_count = read_format.data_count;
    write_format.data1_pos = malloc(sizeof(size_t) * write_format.data_count);
    write_format.data2_pos = malloc(sizeof(size_t) * write_format.data_count);
    
    // Find available positions for data columns
    bool* used = calloc(write_cols, sizeof(bool));
    used[write_format.seq1_pos] = true;
    used[write_format.seq2_pos] = true;
    used[write_format.score_pos] = true;
    used[write_format.align_pos] = true;
    
    // Map data columns to unused positions
    idx = 0;
    for(size_t i = 0; i < write_cols && idx < write_format.data_count; i++) {
        if(!used[i]) {
            write_format.data1_pos[idx] = i;
            for(size_t j = i + 1; j < write_cols && idx < write_format.data_count; j++) {
                if(!used[j]) {
                    write_format.data2_pos[idx] = j;
                    used[i] = used[j] = true;
                    idx++;
                    break;
                }
            }
        }
    }

    free(used);
}

INLINE char* parse_csv_line(char** current, char* seq, char* other_data) {
    char* p = *current;
    char* write_pos = seq;
    size_t col = 0;
    
    // Skip whitespace
    while (*p && (*p == ' ' || *p == '\r' || *p == '\n')) p++;
    
    // Process each column
    while (*p && *p != '\n' && *p != '\r') {
        if (*p == ',') {
            *write_pos = '\0';
            col++;
            write_pos = (col == read_format.seq_pos) ? seq : other_data;
            p++;
            continue;
        }
        *write_pos++ = *p++;
    }
    *write_pos = '\0';
    
    while (*p && (*p == '\n' || *p == '\r')) p++;
    *current = p;
    return p;
}


INLINE char* write_alignment_output(char* buf_pos, const Data* prev, const Data* curr, const Alignment* result) {
    size_t total_cols = 4 + write_format.data_count * 2;
    bool first = true;
    
    for (size_t col = 0; col < total_cols; col++) {
        // Add comma between fields
        if (!first) {
            *buf_pos++ = ',';
        }
        first = false;

        if (col == write_format.seq1_pos) {
            // Write first sequence
            size_t len = strlen(prev->seq);
            memcpy(buf_pos, prev->seq, len);
            buf_pos += len;
        }
        else if (col == write_format.seq2_pos) {
            // Write second sequence
            size_t len = strlen(curr->seq);
            memcpy(buf_pos, curr->seq, len);
            buf_pos += len;
        }
        else if (col == write_format.score_pos) {
            // Write alignment score
            buf_pos = int_to_str(buf_pos, result->score);
        }
        else if (col == write_format.align_pos) {
            // Write alignment result
            buf_pos += sprintf(buf_pos, RESULT_CSV_ALIGN_FORMAT,
                             result->seq1_aligned, result->seq2_aligned);
        }
        else {
            // Handle data columns
            for (size_t i = 0; i < write_format.data_count; i++) {
                if (col == write_format.data1_pos[i]) {
                    size_t len = strlen(prev->other_data);
                    memcpy(buf_pos, prev->other_data, len);
                    buf_pos += len;
                    break;
                }
                else if (col == write_format.data2_pos[i]) {
                    size_t len = strlen(curr->other_data);
                    memcpy(buf_pos, curr->other_data, len);
                    buf_pos += len;
                    break;
                }
            }
        }
    }
    
    *buf_pos++ = '\n';
    *buf_pos = '\0';
    return buf_pos;
}

#endif