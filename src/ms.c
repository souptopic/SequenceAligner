#include "utils.h"

int main(int argc, char** argv) {
    PIN_THREAD(0);
    SET_HIGH_PRIORITY();

	if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
    	printf("Usage: %s [input] [output]\n"
           "Input: CSV file (default: %s)\n"
           #ifdef MODE_WRITE
           "Output: CSV file (default: %s)\n"
           #endif
           , argv[0], INPUT_FILE
           #ifdef MODE_WRITE
           , OUTPUT_FILE
           #endif
		);
		return 0;
	}

	char base_path[MAX_PATH];
    strncpy(base_path, argv[0], MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';

	for (char* p = base_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

	char* last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

	last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

	char full_input_path[MAX_PATH];
    char full_output_path[MAX_PATH];

	snprintf(full_input_path, MAX_PATH, "%s/%s", base_path, INPUT_FILE);
    snprintf(full_output_path, MAX_PATH, "%s/%s", base_path, OUTPUT_FILE);

    const char* input_file = argv[1] ? argv[1] : full_input_path;
    
    #ifdef MODE_WRITE
    const char* output_file = argc > 2 ? argv[2] : full_output_path;
    #endif

    #ifdef _WIN32
    if (GetFileAttributesA(input_file) == INVALID_FILE_ATTRIBUTES) {
    #else
    if (access(input_file, F_OK) != 0) {
    #endif
        printf("Error: Input file '%s' does not exist\n", input_file);
        return 1;
    }

    #ifdef _WIN32
    HANDLE hFile = CreateFileA(input_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    char* file_data = (char*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(hFile, &file_size);
    size_t data_size = file_size.QuadPart;
    #else
    int fd = open(input_file, O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    size_t data_size = sb.st_size;
    char* file_data = mmap(NULL, data_size, PROT_READ, MAP_PRIVATE, fd, 0);
    madvise(file_data, data_size, MADV_SEQUENTIAL);
    #endif

    #ifdef MODE_WRITE
    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
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