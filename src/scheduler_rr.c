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

tcb* rr_next(void) {
    if (!rr_queue) return NULL;

    long now = get_current_time_ms();
    tcb* prev = NULL;
    tcb* curr = rr_queue;

    // Buscar primer hilo listo y con time_start cumplido
    while (curr) {
        if (curr->state == READY && now >= curr->time_start * 1000) {
            // Sacar de la cola
            if (prev) {
                prev->next = curr->next;
            } else {
                rr_queue = curr->next;
            }
            curr->next = NULL;

            // ⏱️ Marca de arranque
            curr->last_start_ms = now;
            curr->consumed_ms = 0;

            return curr;
        }

        prev = curr;
        curr = curr->next;
    }

    // Ningún hilo con time_start cumplido
    return NULL;
}

void rr_yield(void) {
    long now = get_current_time_ms();
    long elapsed = now - current->last_start_ms;
    current->consumed_ms += elapsed;

    // ¿agotó su quantum?
    if (current->consumed_ms >= RR_QUANTUM_MS) {
        rr_add(current);  // ➜ al final de la cola
        tcb* next = rr_next();
        if (next) {
            tcb* prev = current;
            current = next;
            swapcontext(&prev->context, &next->context);
        }
    } else {
        // Quantum no agotado: solo actualiza marca
        current->last_start_ms = now;
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
