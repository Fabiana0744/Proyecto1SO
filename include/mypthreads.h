#ifndef MYPTHREADS_H
#define MYPTHREADS_H

#include <ucontext.h>
#include <stdbool.h>

#define STACK_SIZE 8192

typedef enum {
    SCHED_RR,
    SCHED_LOTTERY,
    SCHED_REALTIME
} scheduler_type;

typedef struct my_thread {
    int id;
    ucontext_t context;
    void *stack;
    bool finished;
    struct my_thread *waiting_for_me;
    scheduler_type sched;
    int tickets;        // para sorteo
    int deadline;       // para tiempo real
    struct my_thread *next;

    bool detached;                 // Para saber si está separado
    bool joined;                   // Si alguien lo está esperando
    ucontext_t join_context;       // Contexto para volver cuando termine

} my_thread_t;

typedef struct my_mutex {
    bool locked;                    // ya lo tiene alguien
    int owner;                      // ID del hilo que lo tiene
} my_mutex_t;

void my_thread_register_main();
void my_thread_start(my_thread_t *thread);

// Acceso a current_thread (para schedulers)
my_thread_t* get_current_thread();
void set_current_thread(my_thread_t *thread);


// Declaraciones principales
int my_thread_create(my_thread_t **thread, scheduler_type sched, void (*start_routine)(void *), void *arg);
void my_thread_end();
void my_thread_yield();
int my_thread_join(my_thread_t *thread);
int my_thread_detach(my_thread_t *thread);
int my_thread_chsched(my_thread_t *thread, scheduler_type sched);

// Mutex
int my_mutex_init(my_mutex_t *mutex);
int my_mutex_lock(my_mutex_t *mutex);
int my_mutex_trylock(my_mutex_t *mutex);
int my_mutex_unlock(my_mutex_t *mutex);
int my_mutex_destroy(my_mutex_t *mutex);

#endif
