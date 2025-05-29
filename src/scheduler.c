//scheduler.c:
#include "scheduler.h"
#include <stdlib.h>
#include <stdio.h>

// --- Referencias externas de cada scheduler específico ---
extern void rr_init();
extern void rr_add(tcb*);
extern tcb* rr_next();
extern void rr_yield();
extern void rr_end();

extern void lottery_init();
extern void lottery_add(tcb*);
extern tcb* lottery_next();
extern void lottery_yield();
extern void lottery_end();

extern void realtime_init();
extern void realtime_add(tcb*);
extern tcb* realtime_next();
extern void realtime_yield();
extern void realtime_end();

// --- Referencia al hilo actual ---
extern tcb* current;
extern ucontext_t main_context;


// --- Inicializa todos los schedulers ---
void scheduler_init() {
    rr_init();
    lottery_init();
    realtime_init();
}

// --- Añade un hilo a su scheduler correspondiente ---
void scheduler_add(tcb* thread) {
    switch (thread->sched_type) {
        case SCHED_RR:       rr_add(thread); break;
        case SCHED_LOTTERY:  lottery_add(thread); break;
        case SCHED_REALTIME: realtime_add(thread); break;
    }
}

// --- Devuelve el siguiente hilo listo para ejecutar (orden de prioridad) ---
tcb* scheduler_next() {
    tcb* t = realtime_next();   if (t) return t;
    t = lottery_next();         if (t) return t;
    return rr_next();
}

// --- Yield para el hilo actual según su tipo ---
void scheduler_yield() {
    if (current == NULL) {
        printf("⚠️ [scheduler_yield] current es NULL\n");
        return;
    }

    printf("[scheduler_yield] tid=%d sched=%d\n", current->tid, current->sched_type);

    switch (current->sched_type) {
        case SCHED_RR:       rr_yield(); break;
        case SCHED_LOTTERY:  lottery_yield(); break;
        case SCHED_REALTIME: realtime_yield(); break;
        default:
            printf("❌ [scheduler_yield] Tipo de scheduler no reconocido: %d\n", current->sched_type);
    }
}


// --- End para el hilo actual según su tipo ---
void scheduler_end() {
    switch (current->sched_type) {
        case SCHED_RR:       rr_end(); break;
        case SCHED_LOTTERY:  lottery_end(); break;
        case SCHED_REALTIME: realtime_end(); break;
    }
}

bool hay_hilos_en_schedulers() {
    extern tcb* get_all_realtime_threads();
    extern tcb* get_all_lottery_threads();
    extern tcb* get_all_rr_threads();

    return get_all_realtime_threads() || get_all_lottery_threads() || get_all_rr_threads();
}


// --- Inicia el scheduler mixto ---
void scheduler_run() {
    printf("🕹️ Ejecutando scheduler mixto (RR + Lottery + RealTime)\n");

    while (1) {
        tcb* next = scheduler_next();

        if (next) {
            current = next;
            swapcontext(&main_context, &next->context);
            // ❌ return;  // ← ¡No debes salir del bucle aquí!
            continue;
        }

        if (hay_hilos_en_schedulers()) {
            busy_wait_ms(50);  // pequeño delay
            continue;
        }

        printf("⚠️ No hay más hilos ni pendientes. Finalizando.\n");
        return;
    }
}
