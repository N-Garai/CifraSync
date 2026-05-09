#ifndef CIFRASYNC_UTIL_THREAD_POOL_H
#define CIFRASYNC_UTIL_THREAD_POOL_H

#include <stddef.h>

typedef void (*cs_task_fn_t)(void *context);

typedef struct {
	cs_task_fn_t fn;
	void *context;
} cs_task_t;

typedef struct cs_thread_pool cs_thread_pool_t;

/* Create a thread pool with num_threads worker threads */
cs_thread_pool_t *cs_thread_pool_create(size_t num_threads);

/* Submit a task to the thread pool */
int cs_thread_pool_submit(cs_thread_pool_t *pool, cs_task_fn_t fn, void *context);

/* Wait for all tasks to complete */
int cs_thread_pool_wait(cs_thread_pool_t *pool);

/* Destroy thread pool and free resources */
void cs_thread_pool_destroy(cs_thread_pool_t *pool);

/* Get the number of worker threads */
size_t cs_thread_pool_size(cs_thread_pool_t *pool);

/* Get approximate number of pending tasks */
size_t cs_thread_pool_queue_size(cs_thread_pool_t *pool);

#endif
