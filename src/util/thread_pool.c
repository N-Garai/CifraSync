#include "util/thread_pool.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE cs_thread_handle_t;
typedef CRITICAL_SECTION cs_mutex_handle_t;
typedef HANDLE cs_event_handle_t;
#else
#include <pthread.h>
typedef pthread_t cs_thread_handle_t;
typedef pthread_mutex_t cs_mutex_handle_t;
typedef pthread_cond_t cs_event_handle_t;
#endif

#define CS_THREAD_POOL_QUEUE_MAX 10000

typedef struct {
	cs_task_fn_t fn;
	void *context;
	int valid;
} cs_queue_task_t;

typedef struct cs_thread_pool {
	cs_thread_handle_t *threads;
	size_t num_threads;
	cs_queue_task_t *queue;
	size_t queue_size;
	size_t queue_front;
	size_t queue_back;
	cs_mutex_handle_t mutex;
	cs_event_handle_t task_available;
	cs_event_handle_t all_done;
	int shutdown;
	int pending_tasks;
} cs_thread_pool_t;

#ifdef _WIN32
static DWORD WINAPI cs_worker_thread(LPVOID arg) {
	cs_thread_pool_t *pool = (cs_thread_pool_t *)arg;
	cs_queue_task_t task;
	
	while (1) {
		WaitForSingleObject(pool->task_available, INFINITE);
		EnterCriticalSection(&pool->mutex);
		
		if (pool->shutdown && pool->queue_front == pool->queue_back) {
			LeaveCriticalSection(&pool->mutex);
			break;
		}
		
		if (pool->queue_front != pool->queue_back) {
			task = pool->queue[pool->queue_front];
			pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
			LeaveCriticalSection(&pool->mutex);
			
			if (task.valid && task.fn != NULL) {
				task.fn(task.context);
			}
			
			EnterCriticalSection(&pool->mutex);
			pool->pending_tasks--;
			if (pool->pending_tasks == 0) {
				SetEvent(pool->all_done);
			}
			LeaveCriticalSection(&pool->mutex);
		} else {
			LeaveCriticalSection(&pool->mutex);
		}
	}
	
	return 0;
}

cs_thread_pool_t *cs_thread_pool_create(size_t num_threads) {
	cs_thread_pool_t *pool;
	size_t i;
	
	if (num_threads == 0) {
		return NULL;
	}
	
	pool = (cs_thread_pool_t *)malloc(sizeof(*pool));
	if (pool == NULL) {
		return NULL;
	}
	
	memset(pool, 0, sizeof(*pool));
	pool->num_threads = num_threads;
	pool->queue_size = CS_THREAD_POOL_QUEUE_MAX;
	pool->queue = (cs_queue_task_t *)malloc(pool->queue_size * sizeof(cs_queue_task_t));
	if (pool->queue == NULL) {
		free(pool);
		return NULL;
	}
	memset(pool->queue, 0, pool->queue_size * sizeof(cs_queue_task_t));
	
	pool->threads = (cs_thread_handle_t *)malloc(num_threads * sizeof(cs_thread_handle_t));
	if (pool->threads == NULL) {
		free(pool->queue);
		free(pool);
		return NULL;
	}
	
	InitializeCriticalSection(&pool->mutex);
	pool->task_available = CreateEventA(NULL, FALSE, FALSE, NULL);
	pool->all_done = CreateEventA(NULL, TRUE, TRUE, NULL);
	
	if (pool->task_available == NULL || pool->all_done == NULL) {
		DeleteCriticalSection(&pool->mutex);
		if (pool->task_available != NULL) CloseHandle(pool->task_available);
		if (pool->all_done != NULL) CloseHandle(pool->all_done);
		free(pool->threads);
		free(pool->queue);
		free(pool);
		return NULL;
	}
	
	for (i = 0; i < num_threads; ++i) {
		pool->threads[i] = CreateThread(NULL, 0, cs_worker_thread, pool, 0, NULL);
		if (pool->threads[i] == NULL) {
			pool->shutdown = 1;
			cs_thread_pool_destroy(pool);
			return NULL;
		}
	}
	
	return pool;
}

int cs_thread_pool_submit(cs_thread_pool_t *pool, cs_task_fn_t fn, void *context) {
	size_t next;
	
	if (pool == NULL || fn == NULL) {
		return -1;
	}
	
	EnterCriticalSection(&pool->mutex);
	
	if (pool->shutdown) {
		LeaveCriticalSection(&pool->mutex);
		return -1;
	}
	
	next = (pool->queue_back + 1) % pool->queue_size;
	if (next == pool->queue_front) {
		LeaveCriticalSection(&pool->mutex);
		return -1;
	}
	
	pool->queue[pool->queue_back].fn = fn;
	pool->queue[pool->queue_back].context = context;
	pool->queue[pool->queue_back].valid = 1;
	pool->queue_back = next;
	pool->pending_tasks++;
	ResetEvent(pool->all_done);
	
	LeaveCriticalSection(&pool->mutex);
	SetEvent(pool->task_available);
	
	return 0;
}

int cs_thread_pool_wait(cs_thread_pool_t *pool) {
	if (pool == NULL) {
		return -1;
	}
	
	WaitForSingleObject(pool->all_done, INFINITE);
	return 0;
}

void cs_thread_pool_destroy(cs_thread_pool_t *pool) {
	size_t i;
	
	if (pool == NULL) {
		return;
	}
	
	EnterCriticalSection(&pool->mutex);
	pool->shutdown = 1;
	LeaveCriticalSection(&pool->mutex);
	
	for (i = 0; i < pool->num_threads; ++i) {
		SetEvent(pool->task_available);
	}
	
	for (i = 0; i < pool->num_threads; ++i) {
		WaitForSingleObject(pool->threads[i], INFINITE);
		CloseHandle(pool->threads[i]);
	}
	
	DeleteCriticalSection(&pool->mutex);
	CloseHandle(pool->task_available);
	CloseHandle(pool->all_done);
	
	free(pool->threads);
	free(pool->queue);
	free(pool);
}

size_t cs_thread_pool_size(cs_thread_pool_t *pool) {
	if (pool == NULL) {
		return 0;
	}
	return pool->num_threads;
}

size_t cs_thread_pool_queue_size(cs_thread_pool_t *pool) {
	size_t size;
	
	if (pool == NULL) {
		return 0;
	}
	
	EnterCriticalSection(&pool->mutex);
	if (pool->queue_back >= pool->queue_front) {
		size = pool->queue_back - pool->queue_front;
	} else {
		size = pool->queue_size - pool->queue_front + pool->queue_back;
	}
	LeaveCriticalSection(&pool->mutex);
	
	return size;
}

#else /* POSIX implementation */

static void *cs_worker_thread(void *arg) {
	cs_thread_pool_t *pool = (cs_thread_pool_t *)arg;
	cs_queue_task_t task;
	
	while (1) {
		pthread_mutex_lock(&pool->mutex);
		
		while (pool->queue_front == pool->queue_back && !pool->shutdown) {
			pthread_cond_wait(&pool->task_available, &pool->mutex);
		}
		
		if (pool->shutdown && pool->queue_front == pool->queue_back) {
			pthread_mutex_unlock(&pool->mutex);
			break;
		}
		
		if (pool->queue_front != pool->queue_back) {
			task = pool->queue[pool->queue_front];
			pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
			pthread_mutex_unlock(&pool->mutex);
			
			if (task.valid && task.fn != NULL) {
				task.fn(task.context);
			}
			
			pthread_mutex_lock(&pool->mutex);
			pool->pending_tasks--;
			if (pool->pending_tasks == 0) {
				pthread_cond_broadcast(&pool->all_done);
			}
			pthread_mutex_unlock(&pool->mutex);
		} else {
			pthread_mutex_unlock(&pool->mutex);
		}
	}
	
	return NULL;
}

cs_thread_pool_t *cs_thread_pool_create(size_t num_threads) {
	cs_thread_pool_t *pool;
	size_t i;
	int ret;
	
	if (num_threads == 0) {
		return NULL;
	}
	
	pool = (cs_thread_pool_t *)malloc(sizeof(*pool));
	if (pool == NULL) {
		return NULL;
	}
	
	memset(pool, 0, sizeof(*pool));
	pool->num_threads = num_threads;
	pool->queue_size = CS_THREAD_POOL_QUEUE_MAX;
	pool->queue = (cs_queue_task_t *)malloc(pool->queue_size * sizeof(cs_queue_task_t));
	if (pool->queue == NULL) {
		free(pool);
		return NULL;
	}
	memset(pool->queue, 0, pool->queue_size * sizeof(cs_queue_task_t));
	
	pool->threads = (cs_thread_handle_t *)malloc(num_threads * sizeof(cs_thread_handle_t));
	if (pool->threads == NULL) {
		free(pool->queue);
		free(pool);
		return NULL;
	}
	
	pthread_mutex_init(&pool->mutex, NULL);
	pthread_cond_init(&pool->task_available, NULL);
	pthread_cond_init(&pool->all_done, NULL);
	
	for (i = 0; i < num_threads; ++i) {
		ret = pthread_create(&pool->threads[i], NULL, cs_worker_thread, pool);
		if (ret != 0) {
			pool->shutdown = 1;
			cs_thread_pool_destroy(pool);
			return NULL;
		}
	}
	
	return pool;
}

int cs_thread_pool_submit(cs_thread_pool_t *pool, cs_task_fn_t fn, void *context) {
	size_t next;
	
	if (pool == NULL || fn == NULL) {
		return -1;
	}
	
	pthread_mutex_lock(&pool->mutex);
	
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutex);
		return -1;
	}
	
	next = (pool->queue_back + 1) % pool->queue_size;
	if (next == pool->queue_front) {
		pthread_mutex_unlock(&pool->mutex);
		return -1;
	}
	
	pool->queue[pool->queue_back].fn = fn;
	pool->queue[pool->queue_back].context = context;
	pool->queue[pool->queue_back].valid = 1;
	pool->queue_back = next;
	pool->pending_tasks++;
	
	pthread_mutex_unlock(&pool->mutex);
	pthread_cond_signal(&pool->task_available);
	
	return 0;
}

int cs_thread_pool_wait(cs_thread_pool_t *pool) {
	if (pool == NULL) {
		return -1;
	}
	
	pthread_mutex_lock(&pool->mutex);
	while (pool->pending_tasks > 0) {
		pthread_cond_wait(&pool->all_done, &pool->mutex);
	}
	pthread_mutex_unlock(&pool->mutex);
	
	return 0;
}

void cs_thread_pool_destroy(cs_thread_pool_t *pool) {
	size_t i;
	
	if (pool == NULL) {
		return;
	}
	
	pthread_mutex_lock(&pool->mutex);
	pool->shutdown = 1;
	pthread_mutex_unlock(&pool->mutex);
	
	pthread_cond_broadcast(&pool->task_available);
	
	for (i = 0; i < pool->num_threads; ++i) {
		pthread_join(pool->threads[i], NULL);
	}
	
	pthread_cond_destroy(&pool->task_available);
	pthread_cond_destroy(&pool->all_done);
	pthread_mutex_destroy(&pool->mutex);
	
	free(pool->threads);
	free(pool->queue);
	free(pool);
}

size_t cs_thread_pool_size(cs_thread_pool_t *pool) {
	if (pool == NULL) {
		return 0;
	}
	return pool->num_threads;
}

size_t cs_thread_pool_queue_size(cs_thread_pool_t *pool) {
	size_t size;
	
	if (pool == NULL) {
		return 0;
	}
	
	pthread_mutex_lock(&pool->mutex);
	if (pool->queue_back >= pool->queue_front) {
		size = pool->queue_back - pool->queue_front;
	} else {
		size = pool->queue_size - pool->queue_front + pool->queue_back;
	}
	pthread_mutex_unlock(&pool->mutex);
	
	return size;
}

#endif
