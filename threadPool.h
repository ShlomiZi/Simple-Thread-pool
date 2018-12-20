
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include "osqueue.h"
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>


#define INSERTION_FAILED -1
#define SUCCESS 0
#define EXIT_ERROR_CODE 1
#define STDOUT 1
#define OUTPUT_FILE "out.txt"
#define ERROR_MSG "Error in system call"
#define DONT_WAIT 0
#define MINIMAL_THREADS_NUM 1


enum PoolStatus {RUNNING, STOPPED};                         //pool status: running or not
enum FinishMode {WAITING_EXIT, NON_WAITING_EXIT};           //type of exit


typedef struct job {
    void (*computeFunc)(void *);
    void *param;
} Job;



typedef struct thread_pool {

    OSQueue *jobsQueue;
    void* (*pullJobs)(void *args);
    int numOfThreads;
    pthread_t* threadIDs;
    enum PoolStatus status;
    pthread_mutex_t queueLocking;
    pthread_mutex_t destroyLocking;
    pthread_cond_t cond;
    int tpOutputDescriptor;
    enum FinishMode finishStatus;

} ThreadPool;

/**
 * Creating a new thread pool, containing
 * N(numOfThreads) threads.
 *
 * @param numOfThreads number of threads
 * @return pointer to the thread pool
 */
ThreadPool *tpCreate(int numOfThreads);

/**
 * Destroying of the thread pool
 *
 * @param threadPool the thread pool
 * @param shouldWaitForTasks 1 for execution of all tasks, 0 otherwise
 */
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks);

/**
 * Insert a new task to the thread pool
 *
 * @param threadPool the thread pool
 * @param computeFunc the task
 * @param param its argumnets
 * @return 0 if insertion succeeded, -1 otherwise
 */
int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param);

#endif