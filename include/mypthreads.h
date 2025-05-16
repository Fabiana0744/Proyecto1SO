#ifndef MYPTHREADS_H
#define MYPTHREADS_H

#include <ucontext.h>
#include <stdlib.h>
#include <stdbool.h>



#define STACK_SIZE 64 * 1024
typedef enum {
    SCHED_RR,      // Round Robin
    SCHED_LOTTERY, // Sorteo
    SCHED_REALTIME // Tiempo Real
} scheduler_type_t;


typedef enum { READY, RUNNING, FINISHED, DETACHED } thread_state;

typedef int my_thread_t;

typedef struct my_thread_control_block {
    my_thread_t tid;
    ucontext_t context;
    thread_state state;
    void* retval;
    struct my_thread_control_block* waiting_for_me;
    struct my_thread_control_block* next;
    int tickets;  // cantidad de tickets para scheduler LOTTERY
    long time_start;     // tiempo de inicio permitido (en segundos)
    long time_end;       // tiempo de final permitido (en segundos)
    long deadline;       // para EDF
    bool must_cleanup;   // bandera para saber si debe terminar
    bool finished;       // marca de hilo finalizado

    scheduler_type_t sched_type;  // nuevo campo para saber qué tipo usa cada hilo

    long last_start_ms;     /* → instante en que tomó la CPU           */
    long consumed_ms;       /* → ms gastados dentro del quantum actual */

} tcb;

int my_thread_create(my_thread_t* thread,
    void* (*start_routine)(void*),
    void* arg,
    scheduler_type_t sched,
    int tickets,
    long time_start,
    long time_end,
    long deadline);

void my_thread_end(void* retval);
int my_thread_yield(void);
int my_thread_join(my_thread_t thread, void** retval);
int my_thread_detach(my_thread_t thread);
int my_thread_chsched(my_thread_t tid, scheduler_type_t new_sched,
    int tickets, long time_start, long time_end, long deadline);


// util.h
void busy_wait_ms(int ms);



// MUTEX
typedef struct {
    int locked;         // 0 libre, 1 ocupado
    my_thread_t owner;  // hilo que tiene el candado
} my_mutex_t;

int my_mutex_init(my_mutex_t* mutex);
int my_mutex_lock(my_mutex_t* mutex);
int my_mutex_trylock(my_mutex_t* mutex);
int my_mutex_unlock(my_mutex_t* mutex);
int my_mutex_destroy(my_mutex_t* mutex);


#endif
