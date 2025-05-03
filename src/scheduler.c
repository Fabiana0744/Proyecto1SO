#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stddef.h>

static my_thread_t *try_realtime(void) { return scheduler_next_realtime(); }
static my_thread_t *try_lottery(void)  { return scheduler_next_lottery(); }
static my_thread_t *try_rr(void)       { return scheduler_next_thread(); }

my_thread_t* my_scheduler_next(void)
{
    // 1. Primero Tiempo Real
    my_thread_t *t = try_realtime();
    if (t) return t;

    // 2. Luego Sorteo
    t = try_lottery();
    if (t) return t;

    // 3. Finalmente Round Robin
    return try_rr();  // puede ser NULL si todos terminaron
}
