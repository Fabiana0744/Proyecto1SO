#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>  // Para NULL


my_thread_t* scheduler_next_thread() {
    my_thread_t *start = get_current_thread();
    my_thread_t *current = start;

    do {
        current = current->next;
    } while (current->finished && current != start);

    if (current->finished) {
        return NULL;  // Todos terminaron
    }

    return current;
}
