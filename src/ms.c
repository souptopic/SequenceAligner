#include "utils.h"

int main(void) {
    PIN_THREAD(0);
    SET_HIGH_PRIORITY();

    #ifdef _WIN32
    HANDLE hFile = CreateFileA(INPUT_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* file_data = (char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(hFile, &file_size);
    size_t data_size = file_size.QuadPart;
    #else
    int fd = open(INPUT_FILE, O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    size_t data_size = sb.st_size;
    char* file_data = mmap(NULL, data_size, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(file_data, data_size, MADV_SEQUENTIAL);
    #endif

    #ifdef MODE_WRITE
    int fd_out = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    char* write_buffer = (char*)mat_aligned_alloc(CACHE_LINE, WRITE_BUFFER_SIZE);
    char* buf_pos = write_buffer;
    const char* header = "sequence1,sequence2,label1,label2,score,alignment\n";
    buf_pos = fast_strcpy(write_buffer, header, strlen(header));
    #endif

    char* seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    char* prev_seq = (char*)mat_aligned_alloc(CACHE_LINE, MAX_SEQ_LEN);
    int label = 0;
    int prev_label;
    
    char* current = file_data;
    char* end = file_data + data_size;
    
    // Skip header
    __m256i newline = _mm256_set1_epi8('\n');
    while (current < end) {
        __m256i data = _mm256_loadu_si256((__m256i*)current);
        int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(data, newline));
        if (mask) {
            current += __builtin_ctz(mask) + 1;
            break;
        }
        current += 32;
    }

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

    double endt = get_time();

    #ifdef MODE_WRITE
    write(fd_out, write_buffer, (unsigned int)(buf_pos - write_buffer));
    close(fd_out);
    mat_aligned_free(write_buffer);
    #endif
    
    mat_aligned_free(seq);
    mat_aligned_free(prev_seq);
    mat_aligned_free(result);
    mat_aligned_free(scoring);

    #ifdef _WIN32
    UnmapViewOfFile(file_data);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    #else
    munmap(file_data, data_size);
    close(fd);
    #endif
    
    printf("Alignment time: %f seconds\n", endt - start);
    return 0;
}