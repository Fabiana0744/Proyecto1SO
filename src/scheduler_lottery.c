// src/scheduler_lottery.c
#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdlib.h>
#include <time.h>

static my_thread_t *lottery_queue = NULL;

void scheduler_lottery_add(my_thread_t *thread) {
    thread->next = NULL;

    if (!lottery_queue) {
        lottery_queue = thread;
    } else {
        my_thread_t *temp = lottery_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

// Función para obtener el siguiente hilo tipo LOTTERY
my_thread_t *scheduler_next_lottery() {
    my_thread_t *t = lottery_queue;
    int total_tickets = 0;

    // Contar los tickets de hilos vivos
    while (t) {
        if (!t->finished)
            total_tickets += (t->tickets > 0 ? t->tickets : 1);
        t = t->next;
    }

    if (total_tickets == 0)
        return NULL;

    int winner = (rand() % total_tickets) + 1;
    int count = 0;

    // Buscar el hilo ganador
    my_thread_t *prev = NULL;
    t = lottery_queue;
    while (t) {
        if (!t->finished) {
            count += (t->tickets > 0 ? t->tickets : 1);
            if (count >= winner) {
                // Mover hilo al final (Round-Robin-like fairness)
                if (prev) prev->next = t->next;
                else lottery_queue = t->next;
                t->next = NULL;
                scheduler_lottery_add(t);  // Reencolar al final
                return t;
            }
        }
        prev = t;
        t = t->next;
    }

    return NULL;
}
