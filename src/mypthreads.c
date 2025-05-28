#include "mypthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  // para usleep (opcional, evita busy wait excesivo)
#include "scheduler.h"
#include <stdio.h>  // Para fprintf

#define MAX_THREADS 128

tcb* current = NULL;
static tcb* ready_queue = NULL;
ucontext_t main_context;
static int next_tid = 1;
static tcb* all_threads[MAX_THREADS] = { NULL };


#include <sys/time.h>

static long program_start_ms = 0;

void init_timer() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    program_start_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return now - program_start_ms;
}

void busy_wait_ms(int ms) {
    long start = get_current_time_ms();
    while ((get_current_time_ms() - start) < ms);
}


static tcb* dequeue() {
    if (!ready_queue) return NULL;
    tcb* t = ready_queue;
    ready_queue = ready_queue->next;
    return t;
}


int my_thread_create(my_thread_t* thread,
                     void* (*start_routine)(void*),
                     void* arg,
                     scheduler_type_t sched,
                     int tickets,
                     long time_start,
                     long time_end,
                     long deadline) {
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
    new_thread->must_cleanup = false;
    new_thread->finished = false;

    new_thread->sched_type = sched;

    // ⏱️ Todos los hilos tienen restricciones de tiempo
    new_thread->time_start = time_start;
    new_thread->time_end = time_end;

    // ⚙️ Configurar según scheduler
    switch (sched) {
        case SCHED_RR:
            break;
        case SCHED_LOTTERY:
            new_thread->tickets = (tickets > 0) ? tickets : 1;
            break;
        case SCHED_REALTIME:
            new_thread->deadline = deadline;
            break;
        default:
            fprintf(stderr, "[ERROR] Tipo de scheduler desconocido: %d\n", sched);
            free(new_thread->context.uc_stack.ss_sp);
            free(new_thread);
            return -1;
    }

    *thread = new_thread->tid;
    all_threads[new_thread->tid] = new_thread;

    makecontext(&new_thread->context, (void (*)())start_routine, 1, arg);

    scheduler_add(new_thread);
    return 0;
}



int my_thread_yield(void) {
    scheduler_yield();  // ✅ Aquí se respeta el tipo de scheduler del hilo actual
    return 0;
}


void my_thread_end(void* retval) {
    current->state = FINISHED;
    current->retval = retval;
    current->finished = true;  // 🔁 importante para saber que el hilo terminó

    printf("✅ Hilo con tid = %d terminó su animación\n", current->tid);

    if (current->waiting_for_me) {
        scheduler_add(current->waiting_for_me);
    }

    if (current->state == DETACHED || current->waiting_for_me == NULL) {
        free(current->context.uc_stack.ss_sp);
        all_threads[current->tid] = NULL;
        free(current);
    }

    tcb* next = scheduler_next();
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



int my_thread_chsched(my_thread_t tid, scheduler_type_t new_sched,
    int tickets, long time_start, long time_end, long deadline) {
    if (tid <= 0 || tid >= MAX_THREADS || !all_threads[tid]) {
    return -1;
    }

    tcb* target = all_threads[tid];
    target->sched_type = new_sched; // primero asignamos tipo

    switch (new_sched) {
    case SCHED_RR:
    break;
    case SCHED_LOTTERY:
    target->tickets = (tickets > 0) ? tickets : 1;
    break;
    case SCHED_REALTIME:
    target->time_start = time_start;
    target->time_end = time_end;
    target->deadline = deadline;
    break;
    default:
    return -1;
    }

    //scheduler_add(target);  // aquí ya sabemos su tipo
    return 0;
}


int my_mutex_init(my_mutex_t* mutex) {
    if (!mutex) return -1;
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}


int my_mutex_lock(my_mutex_t* mutex) {
    // Validación de puntero nulo
    if (!mutex) {
        fprintf(stderr, "[ERROR] mutex es NULL en my_mutex_lock()\n");
        return -1;
    }

    // Validación de dirección inválida (opcional pero útil)
    if ((void*)mutex < (void*)0x1000) {
        fprintf(stderr, "[ERROR] mutex apunta a dirección inválida: %p\n", (void*)mutex);
        return -1;
    }

    // Mensaje de depuración antes de intentar bloquear
    printf("🔐 [LOCK] Intentando mutex %p | locked=%d | owner=%d\n", 
           (void*)mutex, mutex->locked, mutex->owner);

    // Busy wait hasta obtener el lock
    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        my_thread_yield();  // Evita consumo innecesario de CPU
    }

    mutex->owner = current->tid;

    // Confirmación de que el lock fue adquirido
    printf("✅ [LOCK] mutex %p adquirido por tid=%d\n", (void*)mutex, current ? current->tid : -1);

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

