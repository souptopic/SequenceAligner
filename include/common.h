#ifndef COMMON_H
#define COMMON_H

#define       MAX_PATH (260)

#define     INPUT_FILE "testing/datasets/avpdb.csv"
#define  INPUT_FILE_MT "testing/datasets/avpdb_mega.csv"
#define    OUTPUT_FILE "results/results.csv"
#define OUTPUT_FILE_MT "results/results_mega.csv"

#ifdef _WIN32

#include <windows.h>
#include <io.h>
#include <winioctl.h>
#include <Shlwapi.h>
#include <malloc.h>

#define write(...) _write(__VA_ARGS__)
#define  open(...)  _open(__VA_ARGS__)
#define close(...) _close(__VA_ARGS__)
#define  read(...)  _read(__VA_ARGS__)

typedef HANDLE pthread_t;

#define T_Func DWORD WINAPI
#define T_Ret(x) return (DWORD)(size_t)(x)

#define pthread_create(t, _, sr, a) (void)(*t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)sr, a, 0, NULL))
#define          pthread_join(t, _) WaitForSingleObject(t, INFINITE)
#define            PIN_THREAD(t_id) SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << t_id)
#define         SET_HIGH_PRIORITY() SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)

#define mat_aligned_free(p) _aligned_free(p)

#undef max
#undef min

#else

#define _GNU_SOURCE
#define    O_BINARY (0)

#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <pthread.h>

#define T_Func    void*
#define  T_Ret(x) return (x)

#define PIN_THREAD(t_id) do { \
    cpu_set_t cpuset; \
    CPU_ZERO(&cpuset); \
    CPU_SET(t_id, &cpuset); \
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset); \
} while(0)
#define SET_HIGH_PRIORITY() nice(-20)  // requires root I think

#define mat_aligned_free(p) free(p)

#endif

#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define max(a, b) ((a) + (((b) - (a)) & ((b) - (a)) >> 31))
#define min(a, b) ((a) - (((a) - (b)) & ((a) - (b)) >> 31))

#define   LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define PREFETCH(x) _mm_prefetch((const char*)(x), _MM_HINT_T0)

#define  CACHE_LINE (64)
#define      INLINE static inline
#define       ALIGN __attribute__((aligned(CACHE_LINE)))

#define BLOSUM_SIZE (20)
#define MAX_SEQ_LEN (64)
#define     SEQ_BUF (MAX_SEQ_LEN * 2)

#define         KiB (1ULL << 10)
#define         MiB (KiB  << 10)
#define         GiB (MiB  << 10)

#define   WRITE_BUF (2 * MiB)

INLINE void* mat_aligned_alloc(size_t alignment, size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

INLINE double get_time(void) {
#ifdef _WIN32
    static double freq_inv = 0.0;
    if (freq_inv == 0.0) {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        freq_inv = 1.0 / (double)freq.QuadPart;
    }
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * freq_inv;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

typedef struct ALIGN {
    int matrix[BLOSUM_SIZE][BLOSUM_SIZE];
} ScoringMatrix;

typedef struct {
    char seq1_aligned[SEQ_BUF] ALIGN;
    char seq2_aligned[SEQ_BUF] ALIGN;
    int score;
} Alignment;

#endif