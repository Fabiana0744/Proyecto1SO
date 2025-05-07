#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>


static my_thread_t *realtime_queue = NULL;

void scheduler_realtime_add(my_thread_t *thread) {
    thread->next = NULL;

    if (!realtime_queue || thread->deadline < realtime_queue->deadline) {
        thread->next = realtime_queue;
        realtime_queue = thread;
        return;
    }

    my_thread_t *prev = NULL;
    my_thread_t *curr = realtime_queue;

    while (curr && thread->deadline >= curr->deadline) {
        prev = curr;
        curr = curr->next;
    }

    prev->next = thread;
    thread->next = curr;
}

my_thread_t *scheduler_next_realtime() {
    my_thread_t *curr = realtime_queue;
    while (curr) {
        if (!curr->finished) return curr;
        curr = curr->next;
    }
    return NULL;
}
