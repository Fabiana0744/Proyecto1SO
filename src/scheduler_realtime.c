#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>

static my_thread_t *realtime_queue = NULL;

void scheduler_realtime_add(my_thread_t *thread) {
    thread->next = NULL;

    if (!realtime_queue) {
        realtime_queue = thread;
    } else {
        my_thread_t *temp = realtime_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

my_thread_t* scheduler_next_realtime(void) {
    my_thread_t *prev = NULL;
    my_thread_t *curr = realtime_queue;
    my_thread_t *selected = NULL;
    my_thread_t *selected_prev = NULL;

    int best_deadline = __INT_MAX__;

    // Buscar el hilo con menor deadline que esté listo
    while (curr) {
        if (!curr->finished && curr->deadline > 0 && curr->deadline < best_deadline) {
            best_deadline = curr->deadline;
            selected = curr;
            selected_prev = prev;
        }

        prev = curr;
        curr = curr->next;
    }

    // Si no hay candidatos válidos
    if (!selected) return NULL;

    // Removerlo de la cola
    if (selected_prev)
        selected_prev->next = selected->next;
    else
        realtime_queue = selected->next;

    selected->next = NULL;
    return selected;
}
