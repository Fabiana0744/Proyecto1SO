//scheduler_lottery.c:
#include "scheduler.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

static tcb* lottery_queue = NULL;
extern tcb* current;
extern ucontext_t main_context;
extern long get_current_time_ms();

tcb* get_all_lottery_threads(void) {
    return lottery_queue;
}

void lottery_init() {
    lottery_queue = NULL;
    srand(time(NULL)); // Inicializa rand()
}

void lottery_add(tcb* thread) {
    // Ya está en cola? No lo agregues de nuevo
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

tcb* lottery_next() {
    long now = get_current_time_ms();
    int total_tickets = 0;

    // Contar tickets solo de hilos listos y cuyo time_start ya pasó
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

    while (curr) {
        if (curr->state == READY && now >= curr->time_start * 1000) {
            counter += (curr->tickets > 0 ? curr->tickets : 1);
            if (counter >= winning_ticket) {
                // Remover de la lista
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

void lottery_yield() {
    if (!current || current->finished) return;

    tcb* prev = current;
    lottery_add(current);  // volver a ponerlo en la cola

    while (1) {
        tcb* next = lottery_next();

        if (next) {
            current = next;
            swapcontext(&prev->context, &next->context);
            return;
        }

        // Si hay hilos en cola pero ninguno está listo (por time_start), esperar
        if (get_all_lottery_threads()) {
            busy_wait_ms(20);
            continue;
        }

        // No hay más hilos, terminar yield
        return;
    }
}

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

void lottery_run() {
    while (1) {
        tcb* next = lottery_next();
        if (next) {
            printf("🚀 [LOTTERY] Iniciando ejecución con tid=%d\n", next->tid);
            current = next;
            swapcontext(&main_context, &next->context);
            return;
        }

        // Si hay hilos pero aún no alcanzaron time_start
        if (get_all_lottery_threads()) {
            busy_wait_ms(20);
            continue;
        }

        // Nada más por hacer
        printf("[LOTTERY] No hay hilos listos.\n");
        return;
    }
}
