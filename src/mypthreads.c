#include "mypthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // para usleep (opcional, evita busy wait excesivo)

#define MAX_THREADS 128

static tcb* current = NULL;
static tcb* ready_queue = NULL;
static ucontext_t main_context;
static int next_tid = 1;
static tcb* all_threads[MAX_THREADS] = { NULL };

#include <sys/time.h>

long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000L);
}


static void enqueue(tcb* thread) {
    thread->next = NULL;
    if (!ready_queue) {
        ready_queue = thread;
    } else {
        tcb* temp = ready_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

static tcb* dequeue() {
    if (!ready_queue) return NULL;
    tcb* t = ready_queue;
    ready_queue = ready_queue->next;
    return t;
}

int my_thread_create(my_thread_t* thread, void* (*start_routine)(void*), void* arg) {
    tcb* new_thread = malloc(sizeof(tcb));
    if (!new_thread) return -1;

    getcontext(&new_thread->context);
    new_thread->context.uc_link = 0;
    new_thread->context.uc_stack.ss_sp = malloc(STACK_SIZE);
    new_thread->context.uc_stack.ss_size = STACK_SIZE;
    new_thread->context.uc_stack.ss_flags = 0;

    if (!new_thread->context.uc_stack.ss_sp) {
        free(new_thread);
        return -1;
    }

    new_thread->tid = next_tid++;
    new_thread->state = READY;
    new_thread->retval = NULL;
    new_thread->waiting_for_me = NULL;
    new_thread->next = NULL;

    *thread = new_thread->tid;
    all_threads[new_thread->tid] = new_thread;

    makecontext(&new_thread->context, (void (*)())start_routine, 1, arg);
    enqueue(new_thread);
    return 0;
}

int my_thread_yield(void) {
    if (!ready_queue) return 0;
    tcb* prev = current;
    tcb* next = dequeue();
    enqueue(prev);
    current = next;
    swapcontext(&prev->context, &next->context);
    return 0;
}

void my_thread_end(void* retval) {
    current->state = FINISHED;
    current->retval = retval;

    if (current->waiting_for_me) {
        enqueue(current->waiting_for_me);
    }

    if (current->state == DETACHED || current->waiting_for_me == NULL) {
        free(current->context.uc_stack.ss_sp);
        all_threads[current->tid] = NULL;
        free(current);
    }

    tcb* next = dequeue();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        setcontext(&main_context);
    }
}

int my_thread_join(my_thread_t thread, void** retval) {
    if (thread <= 0 || thread >= MAX_THREADS || !all_threads[thread]) return -1;
    tcb* target = all_threads[thread];

    if (target->state == FINISHED) {
        if (retval) *retval = target->retval;
        return 0;
    }

    current->state = READY;
    target->waiting_for_me = current;

    tcb* next = dequeue();
    if (next) {
        current = next;
        swapcontext(&target->waiting_for_me->context, &next->context);
    }

    if (retval) *retval = target->retval;
    return 0;
}

int my_thread_detach(my_thread_t thread) {
    if (thread <= 0 || thread >= MAX_THREADS || !all_threads[thread]) return -1;
    tcb* target = all_threads[thread];

    if (target->state == FINISHED) {
        free(target->context.uc_stack.ss_sp);
        free(target);
        all_threads[thread] = NULL;
        return 0;
    }

    target->state = DETACHED;
    return 0;
}

#include <unistd.h>  // para usleep (opcional, evita busy wait excesivo)

int my_mutex_init(my_mutex_t* mutex) {
    if (!mutex) return -1;
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}

int my_mutex_lock(my_mutex_t* mutex) {
    if (!mutex) return -1;

    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        // Espera activa (busy wait)
        my_thread_yield();  // alternativa a sleep
    }

    mutex->owner = current->tid;
    return 0;
}

int my_mutex_trylock(my_mutex_t* mutex) {
    if (!mutex) return -1;

    if (__sync_lock_test_and_set(&mutex->locked, 1) == 0) {
        mutex->owner = current->tid;
        return 0;  // éxito
    }
    return 1;  // ocupado
}

int my_mutex_unlock(my_mutex_t* mutex) {
    if (!mutex || mutex->owner != current->tid) return -1;
    mutex->owner = -1;
    __sync_lock_release(&mutex->locked);
    return 0;
}

int my_mutex_destroy(my_mutex_t* mutex) {
    if (!mutex) return -1;
    // No necesitamos liberar nada, es una estructura simple
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}

