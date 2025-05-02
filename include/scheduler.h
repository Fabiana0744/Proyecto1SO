#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "mypthreads.h"

// Función genérica que llama al scheduler correspondiente
my_thread_t* my_scheduler_next();

// Función específica del Round Robin
my_thread_t* scheduler_next_thread();

#endif
