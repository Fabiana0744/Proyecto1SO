#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

static tcb* rr_queue = NULL;
extern ucontext_t main_context;
extern tcb* current;

/* scheduler_rr.c */
#define RR_QUANTUM_MS  200        /* ⇦  ajuste fácil */


void rr_init() {
    rr_queue = NULL;
}

void rr_add(tcb* thread) {
    printf("🌀 [RR] Añadiendo hilo tid=%d al scheduler Round Robin\n", thread->tid);
    thread->next = NULL;
    if (!rr_queue) {
        rr_queue = thread;
    } else {
        tcb* temp = rr_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

/* scheduler_rr.c */
tcb* rr_next(void)
{
    if (!rr_queue) return NULL;

    tcb* t = rr_queue;
    rr_queue = rr_queue->next;

    /* ⏱️  marca de arranque */
    t->last_start_ms = get_current_time_ms();
    t->consumed_ms   = 0;
    return t;
}

/* scheduler_rr.c */
void rr_yield(void)
{
    long now      = get_current_time_ms();
    long elapsed  = now - current->last_start_ms;
    current->consumed_ms += elapsed;

    /* ¿agotó su quantum? */
    if (current->consumed_ms >= RR_QUANTUM_MS) {
        rr_add(current);                /* ➜ al final de la cola RR   */
        tcb* next = rr_next();          /*   (puede ser él mismo si era el único) */
        if (next) {
            tcb* prev = current;
            current   = next;
            swapcontext(&prev->context, &next->context);
        }
    } else {
        /* Quantum NO agotado: sólo actualiza marca y regresa */
        current->last_start_ms = now;   /* sigue ejecutándose */
    }
}


void rr_end() {
    tcb* next = rr_next();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[RR] No hay más hilos. Regresando al main.\n");
        setcontext(&main_context);
    }
}

void rr_run() {
    tcb* next = rr_next();
    if (next) {
        printf("🚀 [ROUND ROBIN] Iniciando ejecución con tid=%d\n", next->tid);
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[RR] No hay hilos listos para ejecutar.\n");
    }
}
