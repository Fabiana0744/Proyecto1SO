#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "mypthreads.h"

// Prototipos de funciones para cada scheduler
my_thread_t* scheduler_next_thread();      // Round Robin
my_thread_t* scheduler_next_lottery();     // Lottery

// Función principal que decide según el tipo
my_thread_t* my_scheduler_next();

#endif
