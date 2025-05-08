#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdio.h>
#include <string.h>

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

my_thread_t* scheduler_next_realtime() {
    long now = get_current_time_ms();
    my_thread_t *curr = realtime_queue;
    my_thread_t *best = NULL;
    long best_deadline = __LONG_MAX__;

    while (curr) {
        if (curr->finished) {
            curr = curr->next;
            continue;
        }

        long start_ms = curr->time_start * 1000;
        long end_ms   = curr->time_end   * 1000;

        if (now > end_ms) {
            if (!curr->finished) {
                curr->finished = true;
                curr->must_cleanup = true;
                printf("\n💥 Hilo explotó (now=%ld > end=%ld)\n", now, end_ms);
                return curr;  // le damos chance de limpiar
            }
            curr = curr->next;
            continue;
        }
        

        if (now >= start_ms) {
            if (curr->deadline < best_deadline) {
                best = curr;
                best_deadline = curr->deadline;
            }
        }

        curr = curr->next;
    }

    return best; // Puede ser NULL si ninguno está listo
}
