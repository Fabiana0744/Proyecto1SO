#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

static tcb* rr_queue = NULL;          // Cola de hilos del planificador Round Robin.

extern ucontext_t main_context;       // Contexto principal del programa.
extern tcb* current;                  // Hilo actualmente en ejecución.

#define RR_QUANTUM_MS  200  // Define el tiempo de ejecución asignado a cada hilo (quantum).

// Inicializa el planificador Round Robin.
// Entrada: ninguna.
// Salida: la cola se vacía y queda lista para uso.
void rr_init() {
    rr_queue = NULL;
}

// Devuelve todos los hilos registrados en la cola Round Robin.
// Entrada: ninguna.
// Salida: puntero al primer hilo en la cola.
tcb* get_all_rr_threads(void) {
    return rr_queue;
}

// Añade un hilo a la cola del planificador Round Robin.
// Entrada: puntero al hilo a añadir.
// Salida: el hilo se encola al final de la lista.
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

// Selecciona el siguiente hilo listo que cumplió su tiempo de inicio.
// Entrada: ninguna.
// Salida: hilo listo para ejecutar, o NULL si no hay ninguno.
tcb* rr_next(void) {
    if (!rr_queue) return NULL;

    long now = get_current_time_ms();
    tcb* prev = NULL;
    tcb* curr = rr_queue;

    while (curr) {
        if (curr->state == READY && now >= curr->time_start * 1000) {
            // Se remueve el hilo de la cola
            if (prev) {
                prev->next = curr->next;
            } else {
                rr_queue = curr->next;
            }
            curr->next = NULL;

            // Se actualizan marcas de tiempo para control de quantum
            curr->last_start_ms = now;
            curr->consumed_ms = 0;

            return curr;
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

// Cede la CPU si el hilo actual agotó su quantum.
// Entrada: ninguna.
// Salida: realiza cambio de contexto si se agota el quantum; si no, solo actualiza el tiempo.
void rr_yield(void) {
    long now = get_current_time_ms();
    long elapsed = now - current->last_start_ms;
    current->consumed_ms += elapsed;

    if (current->consumed_ms >= RR_QUANTUM_MS) {
        rr_add(current);  // Reencola al final
        tcb* next = rr_next();
        if (next) {
            tcb* prev = current;
            current = next;
            swapcontext(&prev->context, &next->context);
        }
    } else {
        // Si no agotó su quantum, solo actualiza la marca de tiempo
        current->last_start_ms = now;
    }
}

// Finaliza el hilo actual y transfiere el control al siguiente disponible.
// Entrada: ninguna.
// Salida: realiza cambio de contexto o vuelve al hilo principal si no hay más hilos.
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

// Inicia la ejecución del primer hilo listo en Round Robin.
// Entrada: ninguna.
// Salida: cambia el contexto al primer hilo o muestra mensaje si no hay hilos listos.
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
