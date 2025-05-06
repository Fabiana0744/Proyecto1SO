#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>

static my_thread_t *rr_queue = NULL;

void scheduler_rr_add(my_thread_t *thread) {
    thread->next = NULL;

    if (!rr_queue) {
        rr_queue = thread;
    } else {
        my_thread_t *temp = rr_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

my_thread_t* scheduler_next_thread() {
    static my_thread_t *last_rr = NULL;
    if (!last_rr) last_rr = rr_queue;

    my_thread_t *start = last_rr;
    my_thread_t *current = last_rr;

    do {
        current = current->next ? current->next : rr_queue;
        if (!current->finished) {
            last_rr = current;
            return current;
        }
    } while (current != start);

    return NULL;  // Todos terminaron
}
