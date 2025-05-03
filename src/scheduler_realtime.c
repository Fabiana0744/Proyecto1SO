#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>  // Para NULL
#include <limits.h>  // para INT_MAX

my_thread_t* scheduler_next_realtime(void) {
    my_thread_t *head = get_current_thread();
    my_thread_t *t = head;
    my_thread_t *min_deadline_thread = NULL;
    int min_deadline = INT_MAX;

    do {
        if (!t->finished && t->sched == SCHED_REALTIME) {
            if (t->deadline < min_deadline) {
                min_deadline = t->deadline;
                min_deadline_thread = t;
            }
        }
        t = t->next;
    } while (t != head);

    return min_deadline_thread;
}
