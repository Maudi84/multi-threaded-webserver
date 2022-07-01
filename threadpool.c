
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "threadpool.h"
#include "httpserver.h"
//	github.com/pminkov/threadpool help with thread pool pointers and design
// 	some code given by Eugene during section
#define MAX_JOBS 4096

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;

struct job {

    void *arg;
};

struct thread_pool {
    int job_counter;
    int workers;
    int in_job;
    int out_job;

    pthread_t *worker_house;
    struct job job_queue[MAX_JOBS];
};

void *float_worker(void *pool) {
    //	consumer function
    struct thread_pool *worker_pool = (struct thread_pool *) pool;

    while (1) {

        pthread_mutex_lock(&lock);

        while (worker_pool->job_counter == 0) {

            pthread_cond_wait(&fill, &lock);
        }

        struct job new_job = worker_pool->job_queue[worker_pool->out_job];
        worker_pool->out_job = (worker_pool->out_job + 1) % MAX_JOBS;
        worker_pool->job_counter--;

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);
        handle_connection(new_job.arg);
    }
}

void hire_worker(struct thread_pool *worker_pool, void *arg) {
    //	producer function-----

    pthread_mutex_lock(&lock);

    while (worker_pool->job_counter >= MAX_JOBS) {

        pthread_cond_wait(&empty, &lock);
    }
    struct job new_job;
    new_job.arg = arg;
    worker_pool->job_queue[worker_pool->in_job] = new_job;
    worker_pool->in_job = (worker_pool->in_job + 1) % MAX_JOBS;
    worker_pool->job_counter++;
    pthread_cond_signal(&fill);
    pthread_mutex_unlock(&lock);
}

struct thread_pool *fill_pool(int thread_count) {
    // creates a the pool
    struct thread_pool *new_pool = malloc(sizeof(struct thread_pool));
    if (new_pool == NULL) {
        printf("Error creating memory allocation for thread_pool\n");
        exit(1);
    }
    new_pool->in_job = 0;
    new_pool->out_job = 0;
    new_pool->workers = thread_count;
    new_pool->job_counter = 0;

    new_pool->worker_house = malloc(sizeof(pthread_t) * thread_count);
    if (new_pool->worker_house == NULL) {
        printf("Error creating memory allocation for worker_house\n");
        exit(1);
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_create(&new_pool->worker_house[i], NULL, float_worker, new_pool);
    }

    return new_pool;
}

void empty_pool(struct thread_pool *worker_pool) {

    for (int i = 0; i < worker_pool->workers; i++) {

        pthread_cancel(worker_pool->worker_house[i]);
    }

    for (int i = 0; i < worker_pool->workers; i++) {
        pthread_detach(worker_pool->worker_house[i]);
        usleep(100);
    }

    free(worker_pool->worker_house);
    free(worker_pool);
}
