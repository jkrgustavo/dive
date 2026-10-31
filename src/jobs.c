#include "jobs.h"

#include <ba_buffer.h>
#include <unistd.h>

static void queue_init(struct JobQueue *queue) {
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    queue->head = queue->tail = NULL;
    queue->shutdown = false;
}

static void queue_destroy(struct JobQueue *queue) {
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->lock);
}

static void queue_push(struct JobQueue *queue, struct Job *job) {
    job->next = NULL;
    pthread_mutex_lock(&queue->lock);
    if (queue->tail) {
        queue->tail->next = job;
    } else {
        queue->head = job;
    }
    queue->tail = job;
    // wake a sleeping worker
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

// pop from the queue, caller must hold the lock
static struct Job *queue_take_locked(struct JobQueue *queue) {
    struct Job *job = queue->head;
    if (job) {
        queue->head = job->next;
        if (!queue->head) queue->tail = NULL;
        job->next = NULL;
    }
    return job;
}

// sleeps until a job is pushed, returns NULL on shutdown
static struct Job *queue_pop_wait(struct JobQueue *queue) {
    pthread_mutex_lock(&queue->lock);

    // wait can return without a signal. wait allows main thread to push
    while (!queue->head && !queue->shutdown) {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }
    struct Job *job = queue->shutdown ? NULL : queue_take_locked(queue);
    pthread_mutex_unlock(&queue->lock);
    return job;
}

// try popping without waiting
static struct Job *queue_try_pop(struct JobQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    struct Job *job = queue_take_locked(queue);
    pthread_mutex_unlock(&queue->lock);
    return job;
}

// worker runs until shutdown
static void *worker_main(void *arg) {
    struct JobSystem *job_sys = arg;
    struct Job *job;
    while ((job = queue_pop_wait(&job_sys->pending)) != NULL) {
        job->run(job);
        queue_push(&job_sys->done, job);
    }
    return NULL;
}

void jobs_init(struct JobSystem *job_sys, u32 thread_count) {
    if (thread_count == 0) {
        u64 cores = sysconf(_SC_NPROCESSORS_ONLN);
        // leave a core for the main thread
        thread_count = cores > 1 ? (u32)cores - 1 : 1;
    }

    queue_init(&job_sys->pending);
    queue_init(&job_sys->done);

    job_sys->thread_count = thread_count;
    job_sys->threads = malloc(sizeof(pthread_t) * thread_count);
    bs_assert(job_sys->threads, "Failed to allocate threadpool");

    for (u32 i = 0; i < thread_count; i++) {
        i32 err = pthread_create(&job_sys->threads[i], NULL, worker_main, job_sys);
        bs_assert(err == 0, "Error creating thread. Code: %d", err);
    }
}

void jobs_submit(struct JobSystem *job_sys, struct Job *job) {
    bs_assert(job && job->run, "NULL job, or NULL job->run");
    queue_push(&job_sys->pending, job);
}

struct Job *jobs_poll(struct JobSystem *job_sys) {
    return queue_try_pop(&job_sys->done);
}

void jobs_destroy(struct JobSystem *job_sys) {
    pthread_mutex_lock(&job_sys->pending.lock);
    job_sys->pending.shutdown = true;
    // wake workers so they see the flag
    pthread_cond_broadcast(&job_sys->pending.not_empty);
    pthread_mutex_unlock(&job_sys->pending.lock);

    for (u32 i = 0; i < job_sys->thread_count; i++) {
        pthread_join(job_sys->threads[i], NULL);
    }

    free(job_sys->threads);
    job_sys->threads = NULL;
    job_sys->thread_count = 0;

    queue_destroy(&job_sys->pending);
    queue_destroy(&job_sys->done);
}
