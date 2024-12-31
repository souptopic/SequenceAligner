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
} Files;

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

static char* compute_base_path(const char* binary_path) {
    static char base_path[MAX_PATH + 1];
    static char resolved_path[MAX_PATH + 1];
    memset(base_path, 0, sizeof(base_path));
    memset(resolved_path, 0, sizeof(resolved_path));

    if (strlen(binary_path) >= MAX_PATH) {
        fprintf(stderr, "Error: Program path too long\n");
        exit(1);
    }

    #ifdef _WIN32
    if (!_fullpath(resolved_path, binary_path, MAX_PATH)) {
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

    if (strlen(cwd) + strlen(binary_path) + 2 >= MAX_PATH) {
        fprintf(stderr, "Error: Combined path too long\n");
        exit(1);
    }

    snprintf(base_path, MAX_PATH + 1, "%s/%s", cwd, binary_path);
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

INLINE Files get_files(char* argv) {
    Files files = {0};

    const char* input_file = strstr(argv, "mt") || strstr(argv, "batch") ? INPUT_MT_FILE : INPUT_FILE;
    #if MODE_WRITE == 1
    const char* output_file = strstr(argv, "mt") ? OUTPUT_MT_FILE : OUTPUT_FILE;
    #endif
    
    char* base_path = compute_base_path(argv);
    char input_path[MAX_PATH];
    snprintf(input_path, MAX_PATH, "%s/%s", base_path, input_file);
    
    #if MODE_WRITE == 1
    char output_path[MAX_PATH];
    snprintf(output_path, MAX_PATH, "%s/%s", base_path, output_file);
    #endif

    if (strlen(input_path) >= MAX_PATH) {
        fprintf(stderr, "Error: Computed input path too long\n");
        exit(1);
    }

    #if MODE_WRITE == 1
    if (strlen(output_path) >= MAX_PATH) {
        fprintf(stderr, "Error: Computed output path too long\n");
        exit(1);
    }
    #endif

    #ifdef _WIN32
    files.hFile = CreateFileA(input_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    files.hMapping = CreateFileMapping(files.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    files.file_data = (char*)MapViewOfFile(files.hMapping, FILE_MAP_READ, 0, 0, 0);
    LARGE_INTEGER file_size;
    GetFileSizeEx(files.hFile, &file_size);
    files.data_size = file_size.QuadPart;
    #else
    files.fd = open(input_path, O_RDONLY);
    struct stat sb;
    fstat(files.fd, &sb);
    files.data_size = sb.st_size;
    files.file_data = mmap(NULL, files.data_size, PROT_READ, MAP_PRIVATE, files.fd, 0);
    madvise(files.file_data, files.data_size, MADV_SEQUENTIAL);
    #endif

    #if MODE_WRITE == 1
    #ifdef _WIN32
    HANDLE hFileOut = CreateFileA(output_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    files.writer.handle = hFileOut;
    #else
    files.writer.fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    #endif
    files.writer.buffer = (char*)mat_aligned_alloc(CACHE_LINE, WRITE_BUF);
    files.writer.pos = files.writer.buffer;
    files.writer.remaining = WRITE_BUF;
    files.writer.total_size = WRITE_BUF;
    const char* header = WRITE_CSV_HEADER;
    size_t header_len = strlen(header);
    memcpy(files.writer.pos, header, header_len);
    files.writer.pos += header_len;
    files.writer.remaining -= header_len;
    #endif
    return files;
}

INLINE void free_files(Files* files) {
    #ifdef _WIN32
    UnmapViewOfFile(files->file_data);
    CloseHandle(files->hMapping);
    CloseHandle(files->hFile);
    #else
    munmap(files->file_data, files->data_size);
    close(files->fd);
    #endif

    #if MODE_WRITE == 1
    flush_buffer(&files->writer);
    #ifdef _WIN32
    CloseHandle(files->writer.handle);
    #else
    close(files->writer.fd);
    #endif
    mat_aligned_free(files->writer.buffer);
    #endif
}

#endif