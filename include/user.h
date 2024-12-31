#ifndef USER_H
#define USER_H

// Note: The provided values are defaults and can be changed.

// The maximum length of any line in your CSV file
#define MAX_CSV_LINE 256

// The maximum length of any sequence in your CSV file
#define MAX_SEQ_LEN 64

// The gap penalty for the alignment
#define GAP_PENALTY -4

/* INPUT CSV FORMAT RULES
 * - One sequence per line
 * - Fixed number of columns
 * - Additional columns preserved in output
 * - Names are arbitrary, only positions matter
 * 
 * Example: "animal,sequence,data1,data2" */
#define READ_CSV_HEADER "sequence,label\n"

// Position of sequence column (0-based)
#define READ_CSV_SEQ_POS 0

// Number of columns in the CSV file
#define READ_CSV_COLS 2

/* OUTPUT CSV FORMAT RULES
 * - Each row is paired with the next row
 * - Output columns come in pairs (one from first row, one from second row)
 * - Column pairs must be adjacent in the header
 * - Required special columns: score, alignment
 * - All column names are arbitrary, only positions matter
 * - Number of columns = 2 * number of read columns + 2
 * 
 * Valid examples:
 * "seq_a,seq_b,score,align,name_x,name_y,type1,type2"
 * "score,first,second,alignment,cat1,cat2,id_x,id_y"
 * "type_orig,type_comp,name1,name2,seq_first,seq_second,aligned_sequence,points" */
#define WRITE_CSV_HEADER "sequence1,sequence2,label1,label2,score,alignment\n"

// Sequence 2 position is automatically calculated (this + 1)
#define WRITE_CSV_SEQ1_POS 0
#define WRITE_CSV_SCORE_POS 4
#define WRITE_CSV_ALIGN_POS 5

// Alignment format for printf, only rules are to have two %s and to follow printf syntax
#define WRITE_CSV_ALIGN_FMT "\"('%s', '%s')\""

// Change if you don't want to pass file paths as arguments
// Paths are relative to the executable
#define INPUT_FILE "testing/datasets/avpdb.csv"
#define INPUT_MT_FILE "testing/datasets/avpdb_mt.csv"
#define OUTPUT_FILE "results/results.csv"
#define OUTPUT_MT_FILE "results/results_mt.csv"

// Modes //
#define MODE_WRITE 1
#define SIMILARITY_ANALYSIS 0

// Speed constants //
#define BATCH_SIZE 32768

// Helper constants, do not change //
#define KiB (1ULL << 10)
#define MiB (KiB  << 10)
#define GiB (MiB  << 10)

#define WRITE_BUF (8 * MiB) // Buffer size for writing output
#endif