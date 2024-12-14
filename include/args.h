#ifndef ARGS_H
#define ARGS_H

#include "common.h"

typedef struct {
	char* file_data;
	size_t data_size;
	#ifdef _WIN32
	HANDLE hFile;
	HANDLE hMapping;
	#else
	int fd;
	#endif
    #ifdef MODE_WRITE
    int fd_out;
    char* write_buffer;
    char* buf_pos;
    #endif
} Args;

static void validate_path(const char* path, const char* type) {
    if (strlen(path) >= MAX_PATH) {
        printf("Error: %s path too long\n", type);
        exit(1);
    }

    const char* ext = strrchr(path, '.');
    if (!ext || strcmp(ext, ".csv") != 0) {
        printf("Error: Only .csv files allowed\n");
		printf("%s\n", path);
        exit(1);
    }

    const char* invalid_chars = "<>:\"|?*";
    for (const char* c = invalid_chars; *c; c++) {
        if (strchr(path, *c)) {
            printf("Error: Invalid character in path: '%c'\n", *c);
            exit(1);
        }
    }
}

static void check_symlink(const char* path) {
    #ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            FindClose(hFind);
            printf("Error: Symlinks not allowed\n");
            exit(1);
        }
        FindClose(hFind);
    }
    #else
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            printf("Error: Symlinks not allowed\n");
            exit(1);
        }
    }
    #endif
}

static void check_directory(const char* path, const char* type) {
    #ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("Error: %s path is a directory\n", type);
        exit(1);
    }
    #else
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Error: %s path is a directory\n", type);
        exit(1);
    }
    #endif
}

static char* compute_base_path(const char* argv0) {
    static char base_path[MAX_PATH];
    strncpy(base_path, argv0, MAX_PATH - 1);
    base_path[MAX_PATH - 1] = '\0';

    for (char* p = base_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    char* last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

    last_slash = strrchr(base_path, '/');
    if (last_slash) *last_slash = '\0';

    return base_path;
}

static void show_help(const char* prog_name, const char* input_default
    #ifdef MODE_WRITE
    , const char* output_default
    #endif
) {
    #ifdef MODE_WRITE
    printf("Usage: %s [input] [output]\n"
           "Input: CSV file (default: %s)\n"
           "Output: CSV file (default: %s)\n"
           , prog_name, input_default, output_default);
    #else
    printf("Usage: %s [input]\n"
           "Input: CSV file (default: %s)\n"
           , prog_name, input_default);
    #endif
    exit(0);
}

INLINE Args parse_args(int argc, char** argv) {
    Args args = {0};

	const char* input_default = strstr(argv[0], "mt") ? INPUT_FILE_MT : INPUT_FILE;
	#ifdef MODE_WRITE
	const char* output_default = strstr(argv[0], "mt") ? OUTPUT_FILE_MT : OUTPUT_FILE;
	#endif
    
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help(argv[0], input_default
        #ifdef MODE_WRITE
            , output_default
        #endif
        );
    }

    if (argc > 1 && argv[1][0] != '-') {
        if (!argv[1]) {
            printf("Error: Invalid input argument\n");
            exit(1);
        }
        validate_path(argv[1], "input");
        check_symlink(argv[1]);
    }

    #ifdef MODE_WRITE
    if (argc > 2) {
        validate_path(argv[2], "output");
        check_symlink(argv[2]);
    }
    #endif
    
    char* base_path = compute_base_path(argv[0]);
    char full_input_path[MAX_PATH];
    char full_output_path[MAX_PATH];

    snprintf(full_input_path, MAX_PATH, "%s/%s", base_path, input_default);
    #ifdef MODE_WRITE
    snprintf(full_output_path, MAX_PATH, "%s/%s", base_path, output_default);
    #endif

    if (strlen(full_input_path) >= MAX_PATH) {
        printf("Error: Computed input path too long\n");
        exit(1);
    }

    #ifdef MODE_WRITE
    if (strlen(full_output_path) >= MAX_PATH) {
        printf("Error: Computed output path too long\n");
        exit(1);
    }
    #endif

    const char* input_file = argv[1] ? argv[1] : full_input_path;
    check_directory(input_file, "input");
    
	#ifdef _WIN32
    if (GetFileAttributesA(input_file) == INVALID_FILE_ATTRIBUTES) {
    #else
    if (access(input_file, F_OK) != 0) {
    #endif
        printf("Error: Input file '%s' does not exist\n", input_file);
        exit(1);
    }

    #ifdef MODE_WRITE
    const char* output_file = argc > 2 ? argv[2] : full_output_path;
    check_directory(output_file, "output");
    #endif

	printf("Input file: %s\n", input_file);
	#ifdef MODE_WRITE
	printf("Output file: %s\n", output_file);
	#endif

    #ifdef _WIN32
    args.hFile = CreateFileA(input_file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    args.hMapping = CreateFileMapping(args.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    args.file_data = (char*)MapViewOfFile(args.hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(args.hFile, &file_size);
    args.data_size = file_size.QuadPart;
    #else
    args.fd = open(input_file, O_RDONLY);
    struct stat sb;
    fstat(args.fd, &sb);
    args.data_size = sb.st_size;
    args.file_data = mmap(NULL, args.data_size, PROT_READ, MAP_PRIVATE, args.fd, 0);
    madvise(args.file_data, args.data_size, MADV_SEQUENTIAL);
    #endif

    #ifdef MODE_WRITE
    args.fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    args.write_buffer = (char*)mat_aligned_alloc(CACHE_LINE, WRITE_BUFFER_SIZE);
    args.buf_pos = args.write_buffer;
    const char* header = "sequence1,sequence2,label1,label2,score,alignment\n";
    args.buf_pos = fast_strcpy(args.write_buffer, header, strlen(header));
    #endif

    return args;
}

INLINE void free_args(Args* args) {
	#ifdef MODE_WRITE
	close(args->fd_out);
	mat_aligned_free(args->write_buffer);
	#endif

	#ifdef _WIN32
	UnmapViewOfFile(args->file_data);
	CloseHandle(args->hMapping);
	CloseHandle(args->hFile);
	#else
	munmap(args->file_data, args->data_size);
	close(args->fd);
	#endif
}

#endif