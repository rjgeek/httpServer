/**
 * threadpool.c
 * Implementation of threadpool. Swimming in threads.
 */

#include "thread_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "../memory/list.h"

static void *threadpool_func (void *threadpool);

struct future {
    thread_pool_callable_func_t future_function;
    void *data;
    void *result;
    sem_t fsem;
    struct list_elem elem;
};

struct ethread {
    pthread_t *thread;
    struct list_elem elem;
};

struct thread_pool {
    int number_of_threads;
    int shutdown;
    struct list thread_list;
    struct list futures_list;
    pthread_mutex_t lock;
    pthread_cond_t condition;
};

/* Create a new thread pool with n threads. */
struct thread_pool *thread_pool_new(int nthreads) {
	
	printf("create thread pool, the length is  %d\n",nthreads);
    struct thread_pool *pool = (struct thread_pool *) malloc(sizeof(struct thread_pool));
	
    list_init(&pool->thread_list);
    list_init(&pool->futures_list);
    pool->number_of_threads = nthreads;
    pool->shutdown = 0;
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->condition, NULL);
	
    int i;
    for (i = 0; i < nthreads; i++) {
	struct ethread *tempEThread = (struct ethread *)malloc(sizeof(struct ethread));
	pthread_t *tempPThread = (pthread_t *)malloc(sizeof (pthread_t));
	
	tempEThread->thread = tempPThread;
	list_push_back(&pool->thread_list, &tempEThread->elem);
    }
    
    struct list_elem *e;
    for (e = list_begin (&pool->thread_list); e != list_end (&pool->thread_list); e = list_next (e)) {
	void *vpool = pool;
	struct ethread *initThread = list_entry (e, struct ethread, elem);
	pthread_create(initThread->thread, NULL, threadpool_func, vpool);
    }
    
    return pool;
}

static void *threadpool_func (void *threadpool) {
    
    struct thread_pool *pool = (struct thread_pool*)threadpool;
    
    for (;;) {

	pthread_mutex_lock(&pool->lock);
	
	while (list_empty(&pool->futures_list)) {
	    pthread_cond_wait(&pool->condition,&pool->lock);
	    
	    if (pool->shutdown) {
		pthread_mutex_unlock(&pool->lock);
		pthread_exit(NULL);
	    }
	}

	struct future *initFuture = list_entry (list_pop_front(&pool->futures_list), struct future, elem);
	
	pthread_mutex_unlock(&pool->lock);
	initFuture->result = (*(initFuture->future_function))(initFuture->data);
	sem_post(&initFuture->fsem);
    }
    
    return NULL;
}

struct future * thread_pool_submit(struct thread_pool *pool, thread_pool_callable_func_t callable, void * callable_data) {

	printf("create a new thread\n\n");
    // adding new future
    
    // pool or callable null
    if (pool == NULL || callable == NULL)
	return NULL;

    // no lock in submit
    if (pthread_mutex_lock(&pool->lock) != 0)
        return NULL;

    // shutting down in submit
    if (pool->shutdown)
	return NULL;
    
    struct future *newFuture = (struct future *)malloc(sizeof(struct future));
    newFuture->future_function = callable;
    newFuture->data = callable_data;
    sem_init(&newFuture->fsem, 0, 0);
    list_push_back(&pool->futures_list, &newFuture->elem);
    pthread_cond_signal(&pool->condition);
    pthread_mutex_unlock(&pool->lock);
	
    return newFuture;
}

void thread_pool_shutdown(struct thread_pool *pool) {

    // shutting down threadpool
	
    if (pool == NULL)
	return;
	
    if (pthread_mutex_lock(&pool->lock) != 0)
        return;

    // already shutting down
    if (pool->shutdown)
	return;
	
    pool->shutdown = 1;

    // condition or lock is zero
    if ((pthread_cond_broadcast(&pool->condition) != 0) || (pthread_mutex_unlock(&pool->lock) != 0))
	return;
	
    // joining threads together
    struct list_elem *e;
    for (e = list_begin (&pool->thread_list); e != list_end (&pool->thread_list); e = list_next (e)) {
	struct ethread *joinThread = list_entry (e, struct ethread, elem);
	pthread_join(*joinThread->thread, NULL);
    }
    
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->condition);
    free(pool);
    return;
}

void * future_get(struct future *getFuture) {
    // printf("Future get: %i\n", getFuture->future_id);
    sem_wait(&getFuture->fsem);
    return getFuture->result;
}

void future_free(struct future *freeFuture) {
    // printf("Future free: %i\n", freeFuture->future_id);
    free(freeFuture);
}
