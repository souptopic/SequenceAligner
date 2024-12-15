#ifndef UTILS_H
#define UTILS_H

#include "align.h"

//define MAX_SEQ_LEN in common.h, line 88, by default it's (64)

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
 * The unimportant data columns will be copied in the result column in a way that there will be the least amount of columns.*/
#ifdef MODE_WRITE
#define RESULT_CSV_HEADER "sequence1,sequence2,label1,label2,score,alignment\n"
#define RESULT_CSV_SEQ1_POS (0)
#define RESULT_CSV_SEQ2_POS (1)
#define RESULT_CSV_SCORE_POS (4)
#define RESULT_CSV_ALIGN_POS (5)
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
    
    if (n == 0) {
        *ptr++ = '0';
        return ptr;
    }
    
    // Reverse digits directly into output buffer
    char* start = ptr;
    do {
        uint32_t q = n / 10;
        uint32_t r = n - (q * 10); // mod
        *ptr++ = (char)('0' + r);
        n = q;
    } while (n);
    
    // Reverse in-place
    char* end = ptr - 1;
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
    
    return ptr;
}

static uint8_t length_lookup[256] ALIGN;

INLINE void init_length_lookup() {
    for (int i = 0; i < 256; i++) {
        length_lookup[i] = (uint8_t)__builtin_popcount(i);
    }
}

// a bit faster for singlethreaded
INLINE char* parse1(char** current, char* seq, int* label) {
    char* p = *current;
    
    PREFETCH(p + 64);
    PREFETCH(p + 128);
    PREFETCH(p + 192);
    PREFETCH(p + 256);
    PREFETCH(p + 384);

    __m256i data1 = _mm256_loadu_si256((__m256i*)p);
    __m256i data2 = _mm256_loadu_si256((__m256i*)(p + 32));
    
    const __m256i delim_vec = _mm256_set1_epi8(',');
    const __m256i nl_vec = _mm256_set1_epi8('\n');
    
    __m256i is_delim1 = _mm256_or_si256(
        _mm256_cmpeq_epi8(data1, delim_vec),
        _mm256_cmpeq_epi8(data1, nl_vec)
    );
    __m256i is_delim2 = _mm256_or_si256(
        _mm256_cmpeq_epi8(data2, delim_vec),
        _mm256_cmpeq_epi8(data2, nl_vec)
    );
    
    uint64_t mask1 = _mm256_movemask_epi8(is_delim1);
    uint64_t mask2 = (uint64_t)_mm256_movemask_epi8(is_delim2) << 32;

    // Path for very short sequences (<32 bytes)
    if (LIKELY(mask1)) {
        uint32_t pos = (uint32_t)__builtin_ctz(mask1);
        
        _mm256_storeu_si256((__m256i*)seq, data1);
        seq[pos] = '\0';
        
        p += pos + 1;
        *label = (*p - '0') & 1;
        
        __m256i nl_check = _mm256_loadu_si256((__m256i*)p);
        uint32_t nl_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(nl_check, nl_vec));
        p += __builtin_ctz(nl_mask) + 1;
        
        *current = p;
        return p;
    }

    // Path for medium sequences (32-64 bytes)
    if (LIKELY(mask2)) {
        uint32_t pos = __builtin_ctz(mask2 >> 32) + 32;
        
        _mm256_storeu_si256((__m256i*)seq, data1);
        _mm256_storeu_si256((__m256i*)(seq + 32), data2);
        seq[pos] = '\0';
        
        p += pos + 1;
        *label = (*p - '0') & 1;
        
        __m256i nl_check = _mm256_loadu_si256((__m256i*)p);
        uint32_t nl_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(nl_check, nl_vec));
        p += __builtin_ctz(nl_mask) + 1;
        
        *current = p;
        return p;
    }

    // Path for long sequences
    char* seq_ptr = seq;
    _mm256_storeu_si256((__m256i*)seq_ptr, data1);
    _mm256_storeu_si256((__m256i*)(seq_ptr + 32), data2);
    p += 64;
    seq_ptr += 64;
    
    while (1) {
        PREFETCH(p + 512);
        
        data1 = _mm256_loadu_si256((__m256i*)p);
        is_delim1 = _mm256_or_si256(
            _mm256_cmpeq_epi8(data1, delim_vec),
            _mm256_cmpeq_epi8(data1, nl_vec)
        );
        mask1 = _mm256_movemask_epi8(is_delim1);
        
        if (mask1) {
            uint32_t pos = __builtin_ctz(mask1);
            memcpy(seq_ptr, p, pos);
            seq_ptr[pos] = '\0';
            
            p += pos + 1;
            *label = (*p - '0') & 1;
            
            __m256i nl_check = _mm256_loadu_si256((__m256i*)p);
            uint32_t nl_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(nl_check, nl_vec));
            p += __builtin_ctz(nl_mask) + 1;
            break;
        }
        
        _mm256_storeu_si256((__m256i*)seq_ptr, data1);
        p += 32;
        seq_ptr += 32;
    }
    
    *current = p;
    return p;
}

// a bit faster for multithreaded
INLINE char* parse2(char** current, char* seq, int* label) {
    char* p = *current;
    
    PREFETCH(p + 64);
    PREFETCH(p + 128);
    PREFETCH(p + 192);
    PREFETCH(p + 256);
    PREFETCH(p + 384);
    
    __m256i data = _mm256_loadu_si256((__m256i*)p);
    
    const __m256i delim_vec = _mm256_set1_epi8(',');
    const __m256i nl_vec = _mm256_set1_epi8('\n');
    __m256i is_delim = _mm256_or_si256(
        _mm256_cmpeq_epi8(data, delim_vec),
        _mm256_cmpeq_epi8(data, nl_vec)
    );
    uint32_t mask = _mm256_movemask_epi8(is_delim);

    if (LIKELY(mask)) {
        uint32_t pos = __builtin_ctz(mask);
        _mm256_storeu_si256((__m256i*)seq, data);
        seq[pos] = '\0';
        p += pos + 1;
        *label = (*p - '0') & 1;
        while (*p != '\n' && *p) p++;
        if (*p) p++;
        *current = p;
        return p;
    }

    char* seq_ptr = seq;
    _mm256_storeu_si256((__m256i*)seq_ptr, data);
    p += 32;
    seq_ptr += 32;
    
    data = _mm256_loadu_si256((__m256i*)p);
    is_delim = _mm256_or_si256(
        _mm256_cmpeq_epi8(data, delim_vec),
        _mm256_cmpeq_epi8(data, nl_vec)
    );
    mask = _mm256_movemask_epi8(is_delim);
    
    if (LIKELY(mask)) {
        uint32_t pos = __builtin_ctz(mask);
        
        uint64_t* dst64 = (uint64_t*)seq_ptr;
        uint64_t* src64 = (uint64_t*)p;
        *dst64 = *src64;
        *(dst64+1) = *(src64+1);
        seq_ptr[pos] = '\0';
        
        p += pos + 1;
        *label = (*p - '0') & 1;
        
        while (*p != '\n' && *p) p++;
        if (*p) p++;

        *current = p;
        return p;
    }

    do {
        _mm256_storeu_si256((__m256i*)seq_ptr, data);
        p += 32;
        seq_ptr += 32;
        
        PREFETCH(p + 256);
        
        data = _mm256_loadu_si256((__m256i*)p);
        is_delim = _mm256_or_si256(
            _mm256_cmpeq_epi8(data, delim_vec),
            _mm256_cmpeq_epi8(data, nl_vec)
        );
        mask = _mm256_movemask_epi8(is_delim);
    } while (!mask);

    uint32_t pos = __builtin_ctz(mask);
    memcpy(seq_ptr, p, pos);
    seq_ptr[pos] = '\0';
    
    p += pos + 1;
    *label = (*p - '0') & 1;
    
    data = _mm256_loadu_si256((__m256i*)p);
    mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(data, nl_vec));
    p += __builtin_ctz(mask) + 1;
    
    *current = p;
    return p;
}

INLINE char* write_alignment_output(char* buf_pos, const char* prev_seq, size_t prev_len, const char* seq, size_t seq_len, int prev_label, int label, const Alignment* result) {
    buf_pos = fast_strcpy(buf_pos, prev_seq, prev_len); *buf_pos++ = ',';
    buf_pos = fast_strcpy(buf_pos, seq, seq_len); *buf_pos++ = ',';
    *buf_pos++ = (char)('0' + prev_label); *buf_pos++ = ',';
    *buf_pos++ = (char)('0' + label); *buf_pos++ = ',';
    buf_pos = int_to_str(buf_pos, result->score);
    memcpy(buf_pos, ",\"('", 4); buf_pos += 4;
    buf_pos = fast_strcpy(buf_pos, result->seq1_aligned, strlen(result->seq1_aligned));
    memcpy(buf_pos, "', '", 4); buf_pos += 4;
    buf_pos = fast_strcpy(buf_pos, result->seq2_aligned, strlen(result->seq2_aligned));
    memcpy(buf_pos, "')\"\n", 4);
    return buf_pos + 4;
}

INLINE void copy_full_sequence(char* dst, const char* src) {
	for (size_t i = 0; i < MAX_SEQ_LEN; i += 32) {
		_mm256_store_si256((__m256i*)(dst + i), _mm256_load_si256((__m256i*)(src + i)));
	}
}

#endif