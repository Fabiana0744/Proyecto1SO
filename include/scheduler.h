#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "mypthreads.h"

// Prototipos de funciones para cada scheduler
my_thread_t* scheduler_next_thread();      // Round Robin
my_thread_t* scheduler_next_lottery();     // Lottery
my_thread_t* scheduler_next_realtime();    // Realtime
void scheduler_lottery_add(my_thread_t *t);
void scheduler_rr_add(my_thread_t *thread);
void scheduler_realtime_add(my_thread_t *thread);




// Función principal que decide según el tipo
my_thread_t* my_scheduler_next();

#endif
