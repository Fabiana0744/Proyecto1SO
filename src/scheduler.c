#include "scheduler.h"
#include <stdlib.h>
#include <stdio.h>


// --- Referencias externas de cada scheduler específico ---
extern void rr_init();
extern void rr_add(tcb*);
extern tcb* rr_next();
extern void rr_yield();
extern void rr_end();
extern void rr_run();

extern void lottery_init();
extern void lottery_add(tcb*);
extern tcb* lottery_next();
extern void lottery_yield();
extern void lottery_end();
extern void lottery_run();

extern void realtime_init();
extern void realtime_add(tcb*);
extern tcb* realtime_next();
extern void realtime_yield();
extern void realtime_end();
extern void realtime_run();

// --- Variable global del hilo actual ---
extern tcb* current; // ✅ Correcto

// --- Tipo de scheduler global actual ---
static scheduler_type_t current_type = SCHED_RR;  // Round Robin por defecto

// --- Setter del tipo de scheduler global ---
void scheduler_set_type(scheduler_type_t type) {
    current_type = type;
}

// --- Inicializa el scheduler según tipo actual ---
void scheduler_init() {
    switch (current_type) {
        case SCHED_RR:
            rr_init(); break;
        case SCHED_LOTTERY:
            lottery_init(); break;
        case SCHED_REALTIME:
            realtime_init(); break;
    }
}

// --- Añade un hilo a la cola del scheduler actual ---
void scheduler_add(tcb* thread) {
    switch (current_type) {
        case SCHED_RR:
            rr_add(thread); break;
        case SCHED_LOTTERY:
            lottery_add(thread); break;
        case SCHED_REALTIME:
            realtime_add(thread); break;
    }
}

// --- Obtiene el siguiente hilo a ejecutar según el scheduler actual ---
tcb* scheduler_next() {
    switch (current_type) {
        case SCHED_RR:
            return rr_next();
        case SCHED_LOTTERY:
            return lottery_next();
        case SCHED_REALTIME:
            return realtime_next();
    }
    return NULL;
}

// --- Realiza yield usando el scheduler actual ---
void scheduler_yield() {
    switch (current_type) {
        case SCHED_RR:
            rr_yield(); break;
        case SCHED_LOTTERY:
            lottery_yield(); break;
        case SCHED_REALTIME:
            realtime_yield(); break;
    }
}

// --- Finaliza el hilo actual y pasa al siguiente ---
void scheduler_end() {
    switch (current_type) {
        case SCHED_RR:
            rr_end(); break;
        case SCHED_LOTTERY:
            lottery_end(); break;
        case SCHED_REALTIME:
            realtime_end(); break;
    }
}

void scheduler_run() {
    printf("🕹️ Usando scheduler tipo: %d\n", current_type);  // 🐞 Depuración

    switch (current_type) {
        case SCHED_RR:
            printf("🚦 Ejecutando scheduler Round Robin...\n");
            rr_run(); break;
        case SCHED_LOTTERY:
            printf("🎟️ Ejecutando scheduler Lottery...\n");
            lottery_run(); break;
        case SCHED_REALTIME:
            printf("⏱️ Ejecutando scheduler RealTime...\n");
            realtime_run(); break;
    }
}

