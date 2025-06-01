#include "scheduler.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// Cola de hilos gestionados por el planificador tipo LOTTERY.
// Entrada: se llena dinámicamente con hilos READY.
// Salida: usada para seleccionar aleatoriamente quién ejecuta.
static tcb* lottery_queue = NULL;

extern tcb* current;                   // Hilo en ejecución actual.
extern ucontext_t main_context;       // Contexto del hilo principal.
extern long get_current_time_ms();    // Función externa para obtener el tiempo actual.

// Devuelve la lista de todos los hilos gestionados por el scheduler LOTTERY.
// Entrada: ninguna.
// Salida: puntero al primer hilo en la cola.
tcb* get_all_lottery_threads(void) {
    return lottery_queue;
}

// Inicializa la cola del planificador LOTTERY.
// Entrada: ninguna.
// Salida: la cola se limpia y se inicializa el generador de números aleatorios.
void lottery_init() {
    lottery_queue = NULL;
    srand(time(NULL));
}

// Agrega un hilo a la cola del scheduler LOTTERY si no está ya presente.
// Entrada: puntero al hilo a agregar.
// Salida: la cola se actualiza con el nuevo hilo si aplica.
void lottery_add(tcb* thread) {
    // Evita agregar duplicados
    for (tcb* t = lottery_queue; t; t = t->next) {
        if (t == thread) return;
    }

    printf("🎟️ [LOTTERY] Añadiendo hilo tid=%d con %d tickets\n", thread->tid, thread->tickets);
    thread->next = NULL;

    if (!lottery_queue) {
        lottery_queue = thread;
    } else {
        tcb* tmp = lottery_queue;
        while (tmp->next) tmp = tmp->next;
        tmp->next = thread;
    }
}

// Selecciona el siguiente hilo a ejecutar según el algoritmo de sorteo (lottery).
// Entrada: ninguna.
// Salida: puntero al hilo ganador o NULL si no hay ninguno listo.
tcb* lottery_next() {
    long now = get_current_time_ms();
    int total_tickets = 0;

    // Cuenta todos los tickets de hilos listos cuyo time_start ya pasó.
    for (tcb* t = lottery_queue; t; t = t->next) {
        if (t->state == READY && now >= t->time_start * 1000) {
            total_tickets += (t->tickets > 0 ? t->tickets : 1);
        }
    }

    if (total_tickets == 0) return NULL;

    int winning_ticket = (rand() % total_tickets) + 1;
    int counter = 0;

    tcb* prev = NULL;
    tcb* curr = lottery_queue;

    // Recorre la cola hasta encontrar el hilo ganador y lo remueve de la lista.
    while (curr) {
        if (curr->state == READY && now >= curr->time_start * 1000) {
            counter += (curr->tickets > 0 ? curr->tickets : 1);
            if (counter >= winning_ticket) {
                if (prev) prev->next = curr->next;
                else lottery_queue = curr->next;
                curr->next = NULL;
                return curr;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

// Cede voluntariamente la CPU al siguiente hilo seleccionado por sorteo.
// Entrada: ninguna.
// Salida: realiza un cambio de contexto si hay otro hilo listo.
void lottery_yield() {
    if (!current || current->finished) return;

    tcb* prev = current;
    lottery_add(current);  // Vuelve a poner el hilo en la cola

    while (1) {
        tcb* next = lottery_next();

        if (next) {
            current = next;
            swapcontext(&prev->context, &next->context);
            return;
        }

        // Espera si aún no hay hilos listos por restricciones de tiempo
        if (get_all_lottery_threads()) {
            busy_wait_ms(20);
            continue;
        }

        // No hay hilos listos ni en espera
        return;
    }
}

// Finaliza el hilo actual y transfiere el control al siguiente disponible.
// Entrada: ninguna.
// Salida: cambia de contexto o vuelve al hilo principal si no hay hilos pendientes.
void lottery_end() {
    tcb* next = lottery_next();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[LOTTERY] No hay más hilos. Volviendo a main.\n");
        setcontext(&main_context);
    }
}

// Inicia la ejecución inicial con el primer hilo sorteado.
// Entrada: ninguna.
// Salida: realiza un cambio de contexto al primer hilo elegido, o finaliza si no hay hilos listos.
void lottery_run() {
    while (1) {
        tcb* next = lottery_next();
        if (next) {
            printf("🚀 [LOTTERY] Iniciando ejecución con tid=%d\n", next->tid);
            current = next;
            swapcontext(&main_context, &next->context);
            return;
        }

        // Espera si hay hilos pero aún no cumplen su time_start
        if (get_all_lottery_threads()) {
            busy_wait_ms(20);
            continue;
        }

        // No hay nada más por hacer
        printf("[LOTTERY] No hay hilos listos.\n");
        return;
    }
}
