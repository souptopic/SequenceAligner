#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define TEST_FILE_SIZE (128 * 1024 * 1024)  // 100MB test file
#define MIN_BUFFER (4 * 1024)               // 4KB
#define MAX_BUFFER (128 * 1024 * 1024)      // 128MB

static double get_time(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
}

void generate_test_data(const char* filename) {
    FILE* f = fopen(filename, "wb");
    char* buffer = malloc(64 * 1024);       // 64KB chunks for generation
    
    for (size_t i = 0; i < TEST_FILE_SIZE / (64 * 1024); i++) {
        for (int j = 0; j < 64 * 1024; j++) {
            buffer[j] = (char)(rand() % 26 + 'a');
        }
        fwrite(buffer, 1, 64 * 1024, f);
    }
    
    free(buffer);
    fclose(f);
}

double test_buffer_size(size_t buffer_size, const char* src, const char* dst) {
    FILE *fsrc = fopen(src, "rb");
    FILE *fdst = fopen(dst, "wb");
    char* buffer = malloc(buffer_size);
    
    double start = get_time();
    
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, buffer_size, fsrc)) > 0) {
        fwrite(buffer, 1, bytes_read, fdst);
    }
    
    double elapsed = get_time() - start;
    
    free(buffer);
    fclose(fsrc);
    fclose(fdst);
    
    return elapsed;
}

int main() {
    const char* test_file = "test_input.dat";
    const char* output_file = "test_output.dat";

    generate_test_data(test_file);
    
    printf("\nBuffer Size      Time(s)    Throughput(MB/s)\n");
    printf("----------------------------------------\n");
    
    for (size_t size = MIN_BUFFER; size <= MAX_BUFFER; size *= 2) {
        double time = test_buffer_size(size, test_file, output_file);
        double throughput = TEST_FILE_SIZE / (1024.0 * 1024.0) / time;
        
        printf("%6zu KB %12.3f %15.2f\n", size / 1024, time, throughput);
    }
    
    remove(test_file);
    remove(output_file);
    
    return 0;
}