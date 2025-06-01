// scheduler.h:
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

extern tcb* current;

long get_current_time_ms();    //función para obtener tiempo actual en ms
extern long start_time_ms;     //variable global compartida

// utils.h
void init_timer();

// Acceso a las colas internas (para saber si hay hilos pendientes)
tcb* get_all_realtime_threads(void);
tcb* get_all_rr_threads(void);
tcb* get_all_lottery_threads(void);


#endif
