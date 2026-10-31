#ifndef JOB_H
#define JOB_H

#include <pthread.h>
#include <stdbool.h>

#include "utils/util.h"

struct Job {
    // worker threads run this, set in main thread first
    void (*run)(struct Job *job);

    // for job queue
    struct Job *next;
};

struct JobQueue {
    // protects all other fields
    pthread_mutex_t lock;

    // true when a job is pushed
    pthread_cond_t not_empty;

    // start and end of queue
    struct Job *head, *tail;

    // call before destruction
    bool shutdown;
};

struct JobSystem {
    // submitted jobs from the main thread
    struct JobQueue pending;

    // finished jobs submitted from workers
    struct JobQueue done;

    // thread pool
    pthread_t *threads;
    u32 thread_count;
};

// starts workers. if the count is 0, picks cores-1. minimum 1
void jobs_init(struct JobSystem *job_sys, u32 thread_count);

// submit a job to workers. set job->run and fields first
void jobs_submit(struct JobSystem *job_sys, struct Job *job);

// returns a finished job or NULL if none are finished. doesn't block
struct Job *jobs_poll(struct JobSystem *job_sys);

// stops workers and waits for them to exit. workers finish their jobs first.
void jobs_destroy(struct JobSystem *job_sys);

#endif /* JOB_H */
