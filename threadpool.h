#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <pthread.h>

//	creates the thread pool
struct thread_pool *fill_pool(int thread_count);

//	deletes the pool
void empty_pool(struct thread_pool *worker_pool);

//	creates a new job request
//	accepts a socket connection fd
void hire_worker(struct thread_pool *worker_pool, void *arg);

#endif
