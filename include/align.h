#ifndef ALIGN_H
#define ALIGN_H

#include "scoring.h"

#ifdef __cplusplus
#define restrict __restrict
#endif

INLINE void align_sequences(const char* restrict seq1, size_t len1, const char* restrict seq2, size_t len2, ScoringMatrix* restrict scoring, Alignment* restrict result) {
    int matrix_stack[MAX_SEQ_LEN * MAX_SEQ_LEN];
    int* restrict matrix = matrix_stack;
    const int cols = len1 + 1;

    // Initialize first row
    int* restrict curr_row = matrix;
    curr_row[0] = 0;
    
    #ifdef USE_AVX
    veci_t indices = FIRST_ROW_INDICES;
    for (int j = 1; j <= (int)len1; j += NUM_ELEMS) {
        veci_t values = mullo_epi32(indices, GAP_PENALTY_VEC);
        indices = add_epi32(indices, set1_epi32(NUM_ELEMS));
        storeu((veci_t*)&curr_row[j], values);
    }
    #else
    matrix[0] = 0;
    for(int j = 1; j <= (int)len1; j++) {
        matrix[j] = j * GAP_PENALTY;
    }
    #endif

    // Precompute seq1 indices
    int seq1_indices[MAX_SEQ_LEN];
    for (int i = 0; i < (int)len1; ++i) {
        seq1_indices[i] = AMINO_LOOKUP[(int)seq1[i]];
    }

    // Fill matrix
    #pragma GCC unroll 8
    for (int i = 1; i <= (int)len2; ++i) {
        int* restrict prev_row = curr_row;
        curr_row = matrix + i * cols;
        curr_row[0] = i * GAP_PENALTY;
        int c2_idx = AMINO_LOOKUP[(int)seq2[i - 1]];
        #pragma GCC unroll 4
        for (int j = 1; j <= (int)len1; j++) {
            int match = prev_row[j - 1] + scoring->matrix[seq1_indices[j - 1]][c2_idx];
            int del = prev_row[j] + GAP_PENALTY;
            int insert = curr_row[j - 1] + GAP_PENALTY;
            curr_row[j] = match > del ? (match > insert ? match : insert) : (del > insert ? del : insert);
        }
    }

    // Traceback
    char* restrict seq1_aligned = result->seq1_aligned;
    char* restrict seq2_aligned = result->seq2_aligned;
    int pos = len1 + len2;
    seq1_aligned[pos] = seq2_aligned[pos] = '\0';
    
    int i = (int)len2, j = (int)len1;
    while (i > 0 || j > 0) {
        int curr_score = matrix[i * cols + j];
        int move = 0;    // Default to DIAG
        
        if (i > 0 && j > 0) {
            int diag_score = matrix[(i - 1) * cols + (j - 1)];
            int match_score = scoring->matrix[seq1_indices[j - 1]][AMINO_LOOKUP[(int)seq2[i - 1]]];
            if (curr_score != diag_score + match_score) {
                move = (i > 0 && curr_score == matrix[(i - 1) * cols + j] + GAP_PENALTY) ? 1 : 2;
            }
        } else {
            move = (i > 0) ? 1 : 2;
        }
        
        seq1_aligned[--pos] = (move != 1) ? seq1[j-1] : '-';
        seq2_aligned[pos] = (move != 2) ? seq2[i-1] : '-';
        i += next_i[move];
        j += next_j[move];
    }

    size_t align_len = len1 + len2 - pos;
    memmove(seq1_aligned, seq1_aligned + pos, align_len + 1);
    memmove(seq2_aligned, seq2_aligned + pos, align_len + 1);
    result->score = matrix[len2 * cols + len1];
}

#endif