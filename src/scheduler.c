#include "scheduler.h"
#include <stdlib.h>
#include <stdio.h>

// Referencias externas de cada planificador específico.
// Entrada: cada módulo proporciona sus funciones internas.
// Salida: estas funciones se llaman desde el scheduler general.
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

extern tcb* current;            // Referencia al hilo actualmente en ejecución.
extern ucontext_t main_context; // Contexto principal del programa.

// Inicializa todos los planificadores disponibles.
// Entrada: ninguna.
// Salida: llama a init() de cada scheduler individual.
void scheduler_init() {
    rr_init();
    lottery_init();
    realtime_init();
}

// Añade un hilo al planificador correspondiente según su tipo.
// Entrada: puntero al hilo a registrar.
// Salida: hilo insertado en su cola respectiva.
void scheduler_add(tcb* thread) {
    switch (thread->sched_type) {
        case SCHED_RR:       rr_add(thread); break;
        case SCHED_LOTTERY:  lottery_add(thread); break;
        case SCHED_REALTIME: realtime_add(thread); break;
    }
}

// Devuelve el siguiente hilo listo para ejecutarse, priorizando:
// 1. RealTime, 2. Lottery, 3. Round Robin.
// Entrada: ninguna.
// Salida: puntero al hilo listo o NULL si no hay disponible.
tcb* scheduler_next() {
    tcb* t = realtime_next();   if (t) return t;
    t = lottery_next();         if (t) return t;
    return rr_next();
}

// Cede voluntariamente el CPU, aplicando el mecanismo según el tipo de scheduler del hilo actual.
// Entrada: ninguna.
// Salida: invoca yield del scheduler correspondiente, si current no es NULL.
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

// Marca el hilo actual como finalizado y transfiere el control según su scheduler.
// Entrada: ninguna.
// Salida: se transfiere el control al siguiente hilo o se vuelve al contexto principal.
void scheduler_end() {
    switch (current->sched_type) {
        case SCHED_RR:       rr_end(); break;
        case SCHED_LOTTERY:  lottery_end(); break;
        case SCHED_REALTIME: realtime_end(); break;
    }
}

// Verifica si aún hay hilos pendientes en cualquiera de los planificadores.
// Entrada: ninguna.
// Salida: true si existe al menos un hilo activo, false si todos han terminado.
bool are_threads_pending_in_schedulers() {
    extern tcb* get_all_realtime_threads();
    extern tcb* get_all_lottery_threads();
    extern tcb* get_all_rr_threads();

    return get_all_realtime_threads() || get_all_lottery_threads() || get_all_rr_threads();
}

// Inicia la ejecución del scheduler mixto y coordina los hilos disponibles.
// Entrada: ninguna.
// Salida: ejecuta hilos mientras haya pendientes, y termina cuando todos concluyen.
void scheduler_run() {
    printf("🕹️ Ejecutando scheduler mixto (RR + Lottery + RealTime)\n");

    while (1) {
        tcb* next = scheduler_next();

        if (next) {
            current = next;
            swapcontext(&main_context, &next->context);
            continue;  // Regresa aquí cuando el hilo termina
        }

        // Si no hay hilos listos pero sí pendientes, espera un poco
        if (are_threads_pending_in_schedulers()) {
            busy_wait_ms(50);
            continue;
        }

        // No hay hilos pendientes ni listos
        printf("⚠️ No hay más hilos ni pendientes. Finalizando.\n");
        return;
    }
}
