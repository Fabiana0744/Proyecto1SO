#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>

// Tiempo en que inició el programa (ms desde epoch)
long program_start_time = 0;

// Devuelve milisegundos desde que inició el programa
long get_current_time_ms() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec * 1000L + now.tv_usec / 1000L) - program_start_time;
}



static int thread_counter = 0;
static my_thread_t *current_thread = NULL;

void enqueue_thread(my_thread_t *new_thread) {
    switch (new_thread->sched) {
        case SCHED_RR:
            scheduler_rr_add(new_thread);
            break;
        case SCHED_LOTTERY:
            scheduler_lottery_add(new_thread);
            break;
        case SCHED_REALTIME:
            scheduler_realtime_add(new_thread);
            break;
    }
}

my_thread_t* get_current_thread() {
    return current_thread;
}

void set_current_thread(my_thread_t *thread) {
    current_thread = thread;
}

// --------------- HILO PRINCIPAL ---------------------

void my_thread_register_main() {
    static my_thread_t main_thread;
    static bool registered = false;

    if (!registered) {
        main_thread.id = -1;
        main_thread.finished = false;
        main_thread.detached = false;
        main_thread.joined = false;
        main_thread.stack = NULL;
        main_thread.waiting_for_me = NULL;
        main_thread.sched = SCHED_RR;

        current_thread = &main_thread;

        scheduler_rr_add(&main_thread);
        registered = true;
    }
}

// --------------- FUNCIONES PRINCIPALES DE MYPTHREADS ---------------------

int my_thread_create(my_thread_t **thread, scheduler_type sched, void (*start_routine)(void *), void *arg) {
    *thread = malloc(sizeof(my_thread_t));
    if (!*thread) return -1;

    (*thread)->id = thread_counter++;
    (*thread)->sched = sched;
    (*thread)->finished = false;
    (*thread)->waiting_for_me = NULL;
    (*thread)->detached = false;
    (*thread)->joined = false;
    (*thread)->tickets = 1;
    (*thread)->deadline = 0;
    (*thread)->next = NULL;

    // Reservar espacio para la pila
    (*thread)->stack = malloc(STACK_SIZE);
    if (!(*thread)->stack) {
        free(*thread);
        return -1;
    }

    // Obtener contexto actual como base
    if (getcontext(&(*thread)->context) == -1) {
        perror("getcontext");
        free((*thread)->stack);
        free(*thread);
        return -1;
    }

    // Configurar nuevo contexto
    (*thread)->context.uc_link = NULL;
    (*thread)->context.uc_stack.ss_sp = (*thread)->stack;
    (*thread)->context.uc_stack.ss_size = STACK_SIZE;
    (*thread)->context.uc_stack.ss_flags = 0;

    makecontext(&(*thread)->context, (void (*)())start_routine, 1, arg);

    enqueue_thread(*thread);  // Encolar en su scheduler

    return 0;
}

void my_thread_start(my_thread_t *thread) {
    my_thread_t *prev = current_thread;
    current_thread = thread;

    if (swapcontext(&prev->context, &thread->context) == -1) {
        perror("swapcontext");
    }
}

void my_thread_end() {
    current_thread->finished = true;

    if (current_thread->waiting_for_me) {
        my_thread_t *waiting = current_thread->waiting_for_me;
        current_thread->waiting_for_me = NULL;

        current_thread = waiting;
        setcontext(&waiting->join_context);
        return;
    }

    my_thread_t *next = my_scheduler_next();
    if (next && next != current_thread) {
        set_current_thread(next);
        setcontext(&next->context);  // salto sin volver
    } else {
        exit(0);   // fin correcto si ya no queda nada
    }
}


void my_thread_yield() {
    my_thread_t *prev = get_current_thread();
    my_thread_t *next = my_scheduler_next();

    if (!next || next == prev) return;

    set_current_thread(next);

    if (swapcontext(&prev->context, &next->context) == -1) {
        perror("swapcontext");
    }
}

int my_thread_join(my_thread_t *thread) {
    if (thread->detached || thread->joined) return -1;

    thread->joined = true;

    if (thread->finished) return 0;

    if (getcontext(&current_thread->join_context) == -1) {
        perror("getcontext join");
        return -1;
    }

    thread->waiting_for_me = current_thread;

    my_thread_yield();  // Esperar a que termine

    return 0;
}

int my_thread_detach(my_thread_t *thread) {
    if (thread->joined) return -1;
    thread->detached = true;
    return 0;
}

int my_thread_chsched(my_thread_t *thread, scheduler_type sched) {
    thread->sched = sched;
    enqueue_thread(thread);  // Reagregar al nuevo scheduler
    return 0;
}

// --------------- MUTEX ---------------------

int my_mutex_init(my_mutex_t *mutex) {
    mutex->locked = false;
    mutex->owner = -1;
    return 0;
}

int my_mutex_lock(my_mutex_t *mutex) {
    while (__sync_lock_test_and_set(&mutex->locked, true)) {
        my_thread_yield();  // Esperar hasta que se libere
    }

    mutex->owner = current_thread->id;
    return 0;
}

int my_mutex_unlock(my_mutex_t *mutex) {
    if (!mutex->locked || mutex->owner != get_current_thread()->id) {
        return -1;
    }

    mutex->owner = -1;
    mutex->locked = false;
    return 0;
}

int my_mutex_trylock(my_mutex_t *mutex) {
    if (__sync_lock_test_and_set(&mutex->locked, true)) {
        return -1;
    }

    mutex->owner = get_current_thread()->id;
    return 0;
}

int my_mutex_destroy(my_mutex_t *mutex) {
    if (mutex->locked) return -1;
    mutex->owner = -1;
    return 0;
}
