#ifndef SIMILARITY_H
#define SIMILARITY_H

#if SIMILARITY_ANALYSIS == 1

#include "common.h"

typedef struct {
    int matches;
    int mismatches;
    int gaps;
    double similarity;
} Analysis;

INLINE void similarity_analysis(Alignment* result, size_t len) {
    Analysis analysis = {0};
    char* seq1_aligned = result->seq1_aligned;
    char* seq2_aligned = result->seq2_aligned;
    for (int i = 0; seq1_aligned[i]; i++) {
        if (seq1_aligned[i] == seq2_aligned[i]) {
            analysis.matches++;
        } else if (seq1_aligned[i] == '-') {
            analysis.gaps++;
        } else {
            analysis.mismatches++;
        }
    }
    analysis.similarity = (double)analysis.matches / len;
    #if MODE_WRITE == 1
    printf("Matches: %.2d | Mismatches: %.2d | Gaps: %.2d | Similarity: %.2f%\n", analysis.matches, analysis.mismatches, analysis.gaps, analysis.similarity);
    #endif
}

#endif

#endif