#ifndef THREAD_H
#define THREAD_H

#include "align.h"

#define MAX_THREADS (16)
#define  BATCH_SIZE (65536)

#ifdef _WIN32

#include <synchapi.h>

typedef HANDLE sem_t;

#define sem_init(sem, _, value) *sem = CreateSemaphore(NULL, value, LONG_MAX, NULL)
#define           sem_post(sem) ReleaseSemaphore(*sem, 1, NULL)
#define           sem_wait(sem) WaitForSingleObject(*sem, INFINITE)
#define        sem_destroy(sem) CloseHandle(*sem)

#else

#include <semaphore.h>

#endif

typedef struct {
       const char* seq1;
       const char* seq2;
    ScoringMatrix* scoring;
        Alignment* result;
} AlignTask;

typedef struct {
    AlignTask* tasks;
       size_t  start;
       size_t  end;
        sem_t* work_ready;
        sem_t* work_done;
          int  active;
} ThreadWork;

static ThreadWork* g_thread_work;
static  pthread_t* g_threads;
static        int  g_num_threads;

INLINE int get_num_threads(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors > MAX_THREADS ? MAX_THREADS : sysinfo.dwNumberOfProcessors;
    #else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return nprocs > MAX_THREADS ? MAX_THREADS : nprocs;
    #endif
}

INLINE T_Func thread_pool_worker(void* arg) {
    ThreadWork* work = (ThreadWork*)arg;

    int thread_id = work - g_thread_work;
    PIN_THREAD(thread_id);
    
    while (1) {
        sem_wait(work->work_ready);
        if (!work->active) break;
        
        for (size_t i = work->start; i < work->end; i++) {
            AlignTask* task = &work->tasks[i];
            align_sequences(task->seq1, strlen(task->seq1), task->seq2, strlen(task->seq2), task->scoring, task->result);
        }
        
        sem_post(work->work_done);
    }
    T_Ret(NULL);
}

INLINE void init_thread_pool(void) {
    g_num_threads = get_num_threads();
    g_threads = malloc(sizeof(pthread_t) * g_num_threads);
    g_thread_work = malloc(sizeof(ThreadWork) * g_num_threads);
    
    for (int t = 0; t < g_num_threads; t++) {
        g_thread_work[t].work_ready = malloc(sizeof(sem_t));
        g_thread_work[t].work_done = malloc(sizeof(sem_t));
        sem_init(g_thread_work[t].work_ready, 0, 0);
        sem_init(g_thread_work[t].work_done, 0, 0);
        g_thread_work[t].active = 1;
        pthread_create(&g_threads[t], NULL, thread_pool_worker, &g_thread_work[t]);
    }
}

INLINE void destroy_thread_pool(void) {
    for (int t = 0; t < g_num_threads; t++) {
        g_thread_work[t].active = 0;
        sem_post(g_thread_work[t].work_ready);
        pthread_join(g_threads[t], NULL);
        sem_destroy(g_thread_work[t].work_ready);
        sem_destroy(g_thread_work[t].work_done);
        free(g_thread_work[t].work_ready);
        free(g_thread_work[t].work_done);
    }
    free(g_thread_work);
    free(g_threads);
}

#endif