#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "mypthreads.h"


// Inicialización del scheduler global
void scheduler_init();

// Añadir un hilo a la cola correspondiente
void scheduler_add(tcb* thread);

// Obtener el siguiente hilo a ejecutar
tcb* scheduler_next();

// Yield actual
void scheduler_yield();

// Finalizar el hilo actual y cambiar contexto
void scheduler_end();

// Correr el primer hilo
void scheduler_run();

// Cambiar tipo de scheduler (si se soporta en tiempo de ejecución)
void scheduler_set_type(scheduler_type_t type);

extern tcb* current_thread;



#endif
