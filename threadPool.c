
#include "threadPool.h"

void* pullJobs(void*);
void* pullOneJob(void*);
void emptyQueue(ThreadPool*);
void printErrorAndExit();
void freeResources(ThreadPool*);


ThreadPool *tpCreate(int numOfThreads) {
    int output;
    //make sure wanted threads number is a normal value
    if (numOfThreads < MINIMAL_THREADS_NUM) {
        return NULL;
    }
    //create a file that will hold the output
    if ((output = open(OUTPUT_FILE, O_CREAT | O_TRUNC | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR)) < SUCCESS) {
        printErrorAndExit();
    }
    close(STDOUT);
    //redirect output to the opened file
    dup(output);
    close(output);

    //allocate memory for the pool
    ThreadPool* pool = (ThreadPool*) malloc(sizeof(ThreadPool));
    if (pool == NULL){
        printErrorAndExit();
    }
    pool->numOfThreads = numOfThreads;
    //create the queue
    pool->jobsQueue = osCreateQueue();
    pool->pullJobs = pullJobs;

    //initialize mutex resources
    if(pthread_mutex_init(&(pool->queueLocking), NULL) != SUCCESS||
       pthread_mutex_init(&(pool->destroyLocking), NULL) != SUCCESS ||
       pthread_cond_init(&(pool->cond), NULL) != SUCCESS){
        printErrorAndExit();
    }

    //update pool's status
    pool->status = RUNNING;
    pool->finishStatus = WAITING_EXIT;

    //allocate space for threads IDs
    pool->threadIDs = (pthread_t*) malloc(sizeof(pthread_t) * numOfThreads);
    if (pool->threadIDs == NULL){
        printErrorAndExit();
    }

    //create N threads
    int i;
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(pool->threadIDs + i, NULL, pullOneJob, (void *) pool) != SUCCESS){
            printErrorAndExit();
        }
    }
    return pool;
}


void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {

    //lock, and make sure destroy wasn't already called
    pthread_mutex_lock(&threadPool->destroyLocking);
    if (threadPool->status == STOPPED){
        return;
    }
    threadPool->status = STOPPED;

    //check if waiting is needed or not
    if (shouldWaitForTasks == DONT_WAIT) {
        threadPool->finishStatus = NON_WAITING_EXIT;
    }
    pthread_mutex_unlock(&threadPool->destroyLocking);

    //handle the existing threads
    int i = 0;
    if (osIsQueueEmpty(threadPool->jobsQueue)){
        for (; i < threadPool->numOfThreads; i++) {
            if (pthread_cancel(threadPool->threadIDs[i]) != SUCCESS) {
                printErrorAndExit();
            }
        }
    } else {
        for (; i < threadPool->numOfThreads; i++) {
            if(pthread_join(threadPool->threadIDs[i], NULL) != SUCCESS){
                printErrorAndExit();
            }
        }
    }
    //empty the queue if needed
    if (threadPool->finishStatus == NON_WAITING_EXIT){
        emptyQueue(threadPool);
    }
    //free all Thread pool's struct resources
    freeResources(threadPool);
}


int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    //make sure insertion of new jobs is available
    pthread_mutex_lock(&threadPool->destroyLocking);
    if (threadPool->status == STOPPED){
        pthread_mutex_unlock(&threadPool->destroyLocking);
        return INSERTION_FAILED;
    }
    pthread_mutex_unlock(&threadPool->destroyLocking);

    //allocate space for new job
    Job *job = (Job*) malloc(sizeof(Job));
    if (job == NULL){
        printErrorAndExit();
    }

    //assign the relevant values
    job->computeFunc = computeFunc;
    job->param = param;
    //lock the queue and and the new job
    pthread_mutex_lock(&(threadPool->queueLocking));
    osEnqueue(threadPool->jobsQueue, job);
    //signal that a new job was inserted
    if(pthread_cond_signal(&(threadPool->cond)) != SUCCESS){
        printErrorAndExit();
    }
    pthread_mutex_unlock(&threadPool->queueLocking);
    return SUCCESS;
}

/**
 * Threads function, pulling jobs from the Thread pool.
 *
 * @param arg thread pool, casted to void*
 * @return VOID
 */
void *pullOneJob(void *arg) {
    ThreadPool *threadPool = (ThreadPool*) arg;
    threadPool->pullJobs(threadPool);
}

/**
 * Emptying the queue
 *
 * @param pool the thread pool
 */
void emptyQueue(ThreadPool* pool) {
    while(!osIsQueueEmpty(pool->jobsQueue)){
        Job *task = (Job *) osDequeue(pool->jobsQueue);
        free(task);
    }
}

/**
 * Printing error message and exiting.
 */
void printErrorAndExit() {
    perror(ERROR_MSG);
    exit(EXIT_ERROR_CODE);
}


/**
 * Freeing the pool's resources
 *
 * @param pool the thread pool
 */
void freeResources(ThreadPool* pool) {
    free(pool->threadIDs);
    osDestroyQueue(pool->jobsQueue);
    pthread_mutex_destroy(&(pool->queueLocking));
    pthread_mutex_destroy(&(pool->destroyLocking));
    pthread_cond_destroy(&(pool->cond));
    free(pool);
}


/**
 * The thread pool function.
 * Giving each thead a job to execute.
 *
 * @param arg the thread pool
 * @return VOID
 */
void* pullJobs(void *arg) {

    int flag = 1;

    ThreadPool* pool = (ThreadPool*) arg;
    while ((pool->finishStatus != NON_WAITING_EXIT) || (flag)) {
        //lock the queue before working on it
        pthread_mutex_lock(&(pool->queueLocking));
        flag = 0;
        //queue is empty
        if (osIsQueueEmpty(pool->jobsQueue)) {
            //pool is empty & should wait for tasks -> same as non waiting exit
            if (pool->finishStatus == WAITING_EXIT){
                pthread_mutex_lock(&pool->destroyLocking);
                pool->finishStatus = NON_WAITING_EXIT;
                pthread_mutex_unlock(&pool->destroyLocking);
            //poll is still running and destroy wasn't called, avoid busy waiting
            } else if (pool->status == RUNNING) {
                if(pthread_cond_wait(&(pool->cond), &(pool->queueLocking)) != SUCCESS){
                    printErrorAndExit();
                }
                flag = 1;
            }
            pthread_mutex_unlock(&(pool->queueLocking));
        //queue is not empty, pull the next job
        } else {
            Job* currJob = (Job*) osDequeue(pool->jobsQueue);
            pthread_mutex_unlock(&(pool->queueLocking));
            //execute
            currJob->computeFunc(currJob->param);
            free(currJob);
        }
    }
}