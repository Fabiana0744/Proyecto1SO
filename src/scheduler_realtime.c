#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <limits.h>
#include <sys/time.h>
#include <stdbool.h>

static tcb* realtime_queue = NULL;  // Cola de hilos gestionados por el scheduler de tiempo real.

extern ucontext_t main_context;     // Contexto del hilo principal.
extern tcb* current;                // Hilo actualmente en ejecución.
extern long get_current_time_ms();  // Función para obtener tiempo actual relativo.

// Retorna todos los hilos registrados en la cola de tiempo real.
// Entrada: ninguna.
// Salida: puntero al primer hilo en la cola.
tcb* get_all_realtime_threads(void) {
    return realtime_queue;
}

// Inicializa el planificador de tiempo real (EDF).
// Entrada: ninguna.
// Salida: la cola se limpia para iniciar vacía.
void realtime_init(void) {
    realtime_queue = NULL;
}

// Inserta un hilo en la cola EDF según su deadline.
// Entrada: puntero al hilo a insertar.
// Salida: la cola se reorganiza para mantener orden por deadline.
static void enqueue_edf(tcb* thread)
{
    thread->next = NULL;

    if (!realtime_queue) {
        realtime_queue = thread;
        return;
    }

    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    // Inserta antes del primer hilo con deadline mayor.
    while (curr && curr->deadline < thread->deadline) {
        prev = curr;
        curr = curr->next;
    }

    // Si hay empate de deadline, inserta al final del subgrupo.
    while (curr && curr->deadline == thread->deadline) {
        prev = curr;
        curr = curr->next;
    }

    if (prev) prev->next = thread;
    else      realtime_queue = thread;

    thread->next = curr;
}

// Añade un hilo a la cola de planificación de tiempo real.
// Entrada: puntero al hilo a añadir.
// Salida: el hilo se inserta ordenadamente según EDF.
void realtime_add(tcb* thread) {
    printf("⏱️ [REALTIME] Añadiendo hilo tid=%d con deadline=%ld, start=%ld, end=%ld\n",
           thread->tid, thread->deadline, thread->time_start, thread->time_end);
    enqueue_edf(thread);
}

// Extrae el siguiente hilo válido según EDF y restricciones temporales.
// Entrada: ninguna.
// Salida: puntero al mejor hilo listo, o NULL si no hay disponible.
static tcb* dequeue_next_valid_thread(void)
{
    long now = get_current_time_ms();
    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    while (curr) {
        long start_ms = curr->time_start * 1000;
        long end_ms   = curr->time_end   * 1000;

        // Hilo expirado sin ejecutarse ⇒ se descarta.
        if (now > end_ms) {
            printf("💥 [REALTIME] Hilo %d EXPLOTÓ (now=%ld > end=%ld)\n",
                   curr->tid, now, end_ms);
            curr->finished = true;
            curr->must_cleanup = true;

            if (prev) prev->next = curr->next;
            else      realtime_queue = curr->next;

            curr = (prev ? prev->next : realtime_queue);
            continue;
        }

        // Hilo listo y dentro del rango de tiempo ⇒ se selecciona.
        if (curr->state == READY && now >= start_ms) {
            if (prev) prev->next = curr->next;
            else      realtime_queue = curr->next;

            curr->next = NULL;
            return curr;
        }

        // Avanza al siguiente nodo.
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

// Selecciona el siguiente hilo a ejecutar según EDF, esperando si es necesario.
// Entrada: ninguna.
// Salida: puntero al hilo seleccionado o NULL si no hay más hilos.
tcb* realtime_next() {
    while (1) {
        tcb* best = dequeue_next_valid_thread();
        if (best) return best;

        if (realtime_queue) {
            busy_wait_ms(20);
            my_thread_yield();  // Permite que otros hilos avancen.
            continue;
        }

        return NULL;
    }
}

// Cede la CPU si hay otro hilo con menor deadline que ya pueda ejecutarse.
// Entrada: ninguna.
// Salida: realiza cambio de contexto si corresponde, o se ignora si no aplica.
void realtime_yield(void)
{
    long now = get_current_time_ms();
    if (!current || current->finished) return;

    long end_ms = current->time_end * 1000;
    if (now > end_ms) {
        printf("💥 [REALTIME] Hilo %d venció time_end\n", current->tid);
        current->finished = true;
        return;
    }

    // Busca si hay otro hilo con deadline menor listo para ejecutarse.
    tcb *candidate = realtime_queue;
    long best_dl = LONG_MAX;

    while (candidate) {
        if (candidate->state == READY &&
            now >= candidate->time_start * 1000 &&
            candidate->deadline < best_dl)
            best_dl = candidate->deadline;

        candidate = candidate->next;
    }

    // Si el hilo actual tiene menor o igual deadline ⇒ sigue ejecutando.
    if (best_dl >= current->deadline) {
        return;
    }

    // Hay un hilo con mejor deadline ⇒ se cede el CPU.
    realtime_add(current);
    tcb *next = dequeue_next_valid_thread();
    if (next) {
        tcb *prev = current;
        current = next;
        swapcontext(&prev->context, &next->context);
    }
}

// Marca el hilo actual como finalizado y transfiere el control al siguiente.
// Entrada: ninguna.
// Salida: cambio de contexto al siguiente hilo o retorno al hilo principal.
void realtime_end(void) {
    current->finished = true;
    tcb* next = dequeue_next_valid_thread();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[REALTIME] No hay más hilos activos. Finalizando...\n");
        setcontext(&main_context);
    }
}

// Inicia la ejecución de hilos según el orden definido por EDF.
// Entrada: ninguna.
// Salida: cambia el contexto al primer hilo listo o finaliza si no hay hilos válidos.
void realtime_run(void) {
    tcb* next = dequeue_next_valid_thread();
    if (next) {
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[REALTIME] Nada para ejecutar.\n");
    }
}
