#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdlib.h>
#include <stdio.h>

static int thread_counter = 0;
static my_thread_t *current_thread = NULL;
static my_thread_t *thread_list = NULL;



void enqueue_thread(my_thread_t *new_thread) {
    if (!thread_list) {
        thread_list = new_thread;
        new_thread->next = new_thread;
    } else {
        my_thread_t *temp = thread_list;
        while (temp->next != thread_list) {
            temp = temp->next;
        }
        temp->next = new_thread;
        new_thread->next = thread_list;
    }
}

my_thread_t* get_current_thread() {
    return current_thread;
}

void set_current_thread(my_thread_t *thread) {
    current_thread = thread;
}


// --------------- FUNCIONES SILARES PARA CONSIDERAR O NO EL HILO PRINCIPAL ---------------------

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

        enqueue_thread(&main_thread);  // 👈 Esto es CLAVE para evitar el segfault
        registered = true;
    }
}


void my_thread_start(my_thread_t *thread) {
    my_thread_t *prev = current_thread;
    current_thread = thread;

    if (swapcontext(&prev->context, &thread->context) == -1) {
        perror("swapcontext");
    }
}


// --------------- FUNCIONES PRINCIPALES DE PTHREADS ---------------------

int my_thread_create(my_thread_t **thread, scheduler_type sched, void (*start_routine)(void *), void *arg) {
    *thread = malloc(sizeof(my_thread_t));
    if (!*thread) return -1;

    (*thread)->id = thread_counter++;
    (*thread)->sched = sched;
    (*thread)->finished = false;
    (*thread)->waiting_for_me = NULL;
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

    // Empaquetar la función a ejecutar
    makecontext(&(*thread)->context, (void (*)())start_routine, 1, arg);

    enqueue_thread(*thread);

    // Guardamos el contexto pero aún no se ejecuta, eso lo hace el scheduler
    return 0;
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

    my_thread_yield();  // Nadie nos espera, simplemente ceder CPU
}



void my_thread_yield() {
    my_thread_t *prev = get_current_thread();
    my_thread_t *next = my_scheduler_next();  // usa el scheduler correcto

    if (!next) {
        printf("Todos los hilos han terminado.\n");
        exit(0);
    }

    set_current_thread(next);

    if (swapcontext(&prev->context, &next->context) == -1) {
        perror("swapcontext");
    }
}


int my_thread_join(my_thread_t *thread) {
    if (thread->detached || thread->joined) {
        return -1;  // No se puede hacer join
    }

    thread->joined = true;

    if (thread->finished) {
        return 0;  // Ya terminó
    }

    // Guardar el contexto del que espera
    if (getcontext(&current_thread->join_context) == -1) {
        perror("getcontext join");
        return -1;
    }

    // Enlazar quien espera a quien
    thread->waiting_for_me = current_thread;

    my_thread_yield();  // Ceder CPU hasta que el otro hilo termine

    return 0;
}


int my_thread_detach(my_thread_t *thread) {
    if (thread->joined) return -1; // Ya hay alguien esperando
    thread->detached = true;
    return 0;
}


int my_thread_chsched(my_thread_t *thread, scheduler_type sched) {
    thread->sched = sched;
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
        // Ya está bloqueado, entonces cedo CPU para esperar
        my_thread_yield();
    }

    mutex->owner = current_thread->id;
    return 0;
}


int my_mutex_unlock(my_mutex_t *mutex) {
    if (!mutex->locked || mutex->owner != get_current_thread()->id) {
        return -1;  // Error: no está bloqueado o no somos los dueños
    }

    mutex->owner = -1;
    mutex->locked = false;
    return 0;
}


int my_mutex_trylock(my_mutex_t *mutex) {
    if (__sync_lock_test_and_set(&mutex->locked, true)) {
        return -1;  // Ya está bloqueado
    }

    mutex->owner = get_current_thread()->id;
    return 0;
}



int my_mutex_destroy(my_mutex_t *mutex) {
    if (mutex->locked) return -1;  // No destruir si está en uso
    mutex->owner = -1;
    return 0;
}


