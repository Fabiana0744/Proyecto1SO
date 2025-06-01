#include "mypthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

#define MAX_THREADS 128  // Define el número máximo de hilos que el sistema puede manejar.

tcb* current = NULL;                 // Hilo actualmente en ejecución.
static tcb* ready_queue = NULL;     // Cola de hilos listos para ejecutar.
ucontext_t main_context;            // Contexto del hilo principal.
static int next_tid = 1;            // Contador para asignar IDs únicos a los hilos.
static tcb* all_threads[MAX_THREADS] = { NULL };  // Arreglo global de punteros a todos los hilos.

static long program_start_ms = 0;   // Marca de tiempo de inicio del programa en milisegundos.

// Inicializa el temporizador global al momento de arranque del programa.
// Entrada: ninguna.
// Salida: almacena el tiempo base en program_start_ms.
void init_timer() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    program_start_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Retorna el tiempo actual en milisegundos desde el inicio del programa.
// Entrada: ninguna.
// Salida: tiempo actual relativo al inicio del programa.
long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return now - program_start_ms;
}

// Realiza una espera activa durante un número específico de milisegundos.
// Entrada: tiempo a esperar.
// Salida: ninguna.
void busy_wait_ms(int ms) {
    long start = get_current_time_ms();
    while ((get_current_time_ms() - start) < ms);
}

// Función contenedora que ejecuta la rutina del hilo y finaliza.
// Entrada: puntero empaquetado con función y argumento.
// Salida: invoca my_thread_end con el valor de retorno.
static void thread_start_wrapper(uintptr_t arg_ptr) {
    my_thread_wrapper_args* wrapper = (my_thread_wrapper_args*) arg_ptr;
    void* ret = wrapper->func(wrapper->arg);
    free(wrapper);
    my_thread_end(ret);
}

// Extrae el siguiente hilo listo de la cola de ejecución.
// Entrada: ninguna.
// Salida: puntero al siguiente hilo o NULL si la cola está vacía.
static tcb* dequeue() {
    if (!ready_queue) return NULL;
    tcb* t = ready_queue;
    ready_queue = ready_queue->next;
    return t;
}

// Crea un nuevo hilo con el planificador y tiempos indicados.
// Entrada: puntero a ID, rutina, argumento, tipo de scheduler, tickets y restricciones de tiempo.
// Salida: 0 si se creó con éxito, -1 si falló.
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
    new_thread->time_start = time_start;
    new_thread->time_end = time_end;

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

    my_thread_wrapper_args* wrapper = malloc(sizeof(my_thread_wrapper_args));
    if (!wrapper) return -1;
    wrapper->func = start_routine;
    wrapper->arg  = arg;

    makecontext(&new_thread->context, (void (*)())thread_start_wrapper, 1, (uintptr_t)wrapper);

    scheduler_add(new_thread);
    return 0;
}

// Cede voluntariamente el uso del procesador a otro hilo.
// Entrada: ninguna.
// Salida: 0 si se cedió correctamente.
int my_thread_yield(void) {
    if (current == NULL) return 0;
    scheduler_yield();
    return 0;
}

// Finaliza el hilo actual y libera sus recursos.
// Entrada: valor de retorno del hilo.
// Salida: transfiere el control al siguiente hilo o al hilo principal.
void my_thread_end(void* retval) {
    current->state = FINISHED;
    current->retval = retval;
    current->finished = true;

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

// Espera a que el hilo especificado finalice.
// Entrada: ID del hilo y puntero para capturar el valor de retorno.
// Salida: 0 si se unió correctamente, -1 si falló.
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

// Marca un hilo como detached para liberar recursos automáticamente.
// Entrada: ID del hilo a desacoplar.
// Salida: 0 si se marcó correctamente, -1 si falló.
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

// Cambia dinámicamente el tipo de planificador y parámetros temporales de un hilo.
// Entrada: ID del hilo, nuevo scheduler y parámetros asociados.
// Salida: 0 si se aplicaron los cambios, -1 si hubo error.
int my_thread_chsched(my_thread_t tid, scheduler_type_t new_sched,
    int tickets, long time_start, long time_end, long deadline) {
    if (tid <= 0 || tid >= MAX_THREADS || !all_threads[tid]) {
        return -1;
    }

    tcb* target = all_threads[tid];
    target->sched_type = new_sched;

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

    return 0;
}

// Inicializa un mutex.
// Entrada: puntero al mutex.
// Salida: 0 si se inicializó correctamente, -1 si falló.
int my_mutex_init(my_mutex_t* mutex) {
    if (!mutex) return -1;
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}

// Intenta adquirir un mutex de forma bloqueante.
// Entrada: puntero al mutex.
// Salida: 0 si se adquirió, -1 si falló.
int my_mutex_lock(my_mutex_t* mutex) {
    if (!mutex) {
        fprintf(stderr, "[ERROR] mutex es NULL en my_mutex_lock()\n");
        return -1;
    }

    if ((void*)mutex < (void*)0x1000) {
        fprintf(stderr, "[ERROR] mutex apunta a dirección inválida: %p\n", (void*)mutex);
        return -1;
    }

    printf("🔐 [LOCK] Intentando mutex %p | locked=%d | owner=%d\n", 
           (void*)mutex, mutex->locked, mutex->owner);

    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        my_thread_yield();
    }

    mutex->owner = current->tid;
    printf("✅ [LOCK] mutex %p adquirido por tid=%d\n", (void*)mutex, current ? current->tid : -1);

    return 0;
}

// Intenta adquirir un mutex sin bloquear.
// Entrada: puntero al mutex.
// Salida: 0 si se adquirió, 1 si estaba ocupado, -1 si hubo error.
int my_mutex_trylock(my_mutex_t* mutex) {
    if (!mutex) return -1;

    if (__sync_lock_test_and_set(&mutex->locked, 1) == 0) {
        mutex->owner = current->tid;
        return 0;
    }
    return 1;
}

// Libera un mutex.
// Entrada: puntero al mutex.
// Salida: 0 si se liberó, -1 si hubo error.
int my_mutex_unlock(my_mutex_t* mutex) {
    if (!mutex || mutex->owner != current->tid) return -1;
    mutex->owner = -1;
    __sync_lock_release(&mutex->locked);
    return 0;
}

// Destruye un mutex (opcional en esta implementación).
// Entrada: puntero al mutex.
// Salida: 0 si se limpió correctamente, -1 si falló.
int my_mutex_destroy(my_mutex_t* mutex) {
    if (!mutex) return -1;
    mutex->locked = 0;
    mutex->owner = -1;
    return 0;
}
