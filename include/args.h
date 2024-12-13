#ifndef ARGS_H
#define ARGS_H

#include "csv.h"

typedef struct {
    #ifdef _WIN32
    HANDLE handle;
    DWORD bytes_written;
    #else
    int fd;
    #endif
    char* buffer;
    char* pos;
    size_t remaining;
    size_t total_size;
} WriteBuffer;

typedef struct {
    char* file_data;
    size_t data_size;
    #ifdef _WIN32
    HANDLE hFile;
    HANDLE hMapping;
    #else
    int fd;
    #endif
    #if MODE_WRITE == 1
    WriteBuffer writer;
    #endif
} Args;

INLINE void flush_buffer(WriteBuffer* wb) {
    const size_t to_write = wb->pos - wb->buffer;
    #ifdef _WIN32
    WriteFile(wb->handle, wb->buffer, (DWORD)to_write, &wb->bytes_written, NULL);
    #else
    write(wb->fd, wb->buffer, to_write);
    #endif
    wb->pos = wb->buffer;
    wb->remaining = wb->total_size;
}

static void validate_path(const char* path, const char* type) {
    if (strlen(path) >= MAX_PATH) {
        fprintf(stderr, "Error: %s path too long\n", type);
        exit(1);
    }

    const char* ext = strrchr(path, '.');
    if (!ext || strcmp(ext, ".csv") != 0) {
        fprintf(stderr, "Error: Only .csv files allowed\n");
        fprintf(stderr, "%s\n", path);
        exit(1);
    }

    const char* invalid_chars = "<>:\"|?*";
    for (const char* c = invalid_chars; *c; c++) {
        if (strchr(path, *c)) {
            fprintf(stderr, "Error: Invalid character in path: '%c'\n", *c);
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
            fprintf(stderr, "Error: Symlinks not allowed\n");
            exit(1);
        }
        FindClose(hFind);
    }
    #else
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            fprintf(stderr, "Error: Symlinks not allowed\n");
            exit(1);
        }
    }
    #endif
}

static void check_directory(const char* path, const char* type) {
    #ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        fprintf(stderr, "Error: %s path is a directory\n", type);
        exit(1);
    }
    #else
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s path is a directory\n", type);
        exit(1);
    }
    #endif
}

static char* compute_base_path(const char* argv0) {
    static char base_path[MAX_PATH];
    static char resolved_path[MAX_PATH];
    memset(base_path, 0, sizeof(base_path));
    memset(resolved_path, 0, sizeof(resolved_path));

    if (strlen(argv0) >= MAX_PATH) {
        fprintf(stderr, "Error: Program path too long\n");
        exit(1);
    }

    #ifdef _WIN32
    if (!_fullpath(resolved_path, argv0, MAX_PATH)) {
        fprintf(stderr, "Error: Could not resolve program path\n");
        exit(1);
    }
    strncpy(base_path, resolved_path, MAX_PATH - 1);
    
    for (char* p = base_path; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    #else 

    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "Error: Could not get current directory\n");
        exit(1);
    }

    if (strlen(cwd) + strlen(argv0) + 2 >= MAX_PATH) {
        fprintf(stderr, "Error: Combined path too long\n");
        exit(1);
    }

    snprintf(base_path, MAX_PATH + 1, "%s/%s", cwd, argv0);
    #endif

    base_path[MAX_PATH - 1] = '\0';

    char* last_slash = strrchr(base_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        
        // Check for bin directory
        last_slash = strrchr(base_path, '/');
        if (last_slash && strcmp(last_slash + 1, "bin") == 0) {
            *last_slash = '\0';
        }
    }

    return base_path;
}

static void show_help(const char* prog_name, const char* input_default
    #if MODE_WRITE == 1
    , const char* output_default
    #endif
) {
    #if MODE_WRITE == 1
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

    const char* input_default = strstr(argv[0], "mt") || strstr(argv[0], "batch") ? INPUT_MT_FILE : INPUT_FILE;
    #if MODE_WRITE == 1
    const char* output_default = strstr(argv[0], "mt") ? OUTPUT_MT_FILE : OUTPUT_FILE;
    #endif
    
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help(argv[0], input_default
        #if MODE_WRITE == 1
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

    #if MODE_WRITE == 1
    if (argc > 2) {
        validate_path(argv[2], "output");
        check_symlink(argv[2]);
    }
    #endif
    
    char* base_path = compute_base_path(argv[0]);
    char full_input_path[MAX_PATH];
    snprintf(full_input_path, MAX_PATH, "%s/%s", base_path, input_default);
    
    #if MODE_WRITE == 1
    char full_output_path[MAX_PATH];
    snprintf(full_output_path, MAX_PATH, "%s/%s", base_path, output_default);
    #endif

    if (strlen(full_input_path) >= MAX_PATH) {
        fprintf(stderr, "Error: Computed input path too long\n");
        exit(1);
    }

    #if MODE_WRITE == 1
    if (strlen(full_output_path) >= MAX_PATH) {
        fprintf(stderr, "Error: Computed output path too long\n");
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
        fprintf(stderr, "Error: Input file '%s' does not exist\n", input_file);
        exit(1);
    }

    #if MODE_WRITE == 1
    const char* output_file = argc > 2 ? argv[2] : full_output_path;
    check_directory(output_file, "output");
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

    #if MODE_WRITE == 1
    #ifdef _WIN32
    HANDLE hFileOut = CreateFileA(output_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    args.writer.handle = hFileOut;
    #else
    args.writer.fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
    #endif
    args.writer.buffer = (char*)mat_aligned_alloc(CACHE_LINE, WRITE_BUF);
    args.writer.pos = args.writer.buffer;
    args.writer.remaining = WRITE_BUF;
    args.writer.total_size = WRITE_BUF;
    const char* header = WRITE_CSV_HEADER;
    size_t header_len = strlen(header);
    memcpy(args.writer.pos, header, header_len);
    args.writer.pos += header_len;
    args.writer.remaining -= header_len;
    #endif
    return args;
}

INLINE void free_args(Args* args) {
    #ifdef _WIN32
    UnmapViewOfFile(args->file_data);
    CloseHandle(args->hMapping);
    CloseHandle(args->hFile);
    #else
    munmap(args->file_data, args->data_size);
    close(args->fd);
    #endif

    #if MODE_WRITE == 1
    flush_buffer(&args->writer);
    #ifdef _WIN32
    CloseHandle(args->writer.handle);
    #else
    close(args->writer.fd);
    #endif
    mat_aligned_free(args->writer.buffer);
    #endif
}

#endif