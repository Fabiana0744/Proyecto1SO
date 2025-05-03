#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdio.h>

#include <stddef.h>  
/* src/scheduler.c */
#include "../include/scheduler.h"

static my_thread_t *try_rr(void)      { return scheduler_next_thread(); }
static my_thread_t *try_lottery(void) { return scheduler_next_lottery(); }

my_thread_t* my_scheduler_next(void)
{
    /* 1) Intenta escoger un hilo Lottery disponible */
    my_thread_t *t = try_lottery();
    if (t) return t;

    /* 2) Si no hay, usa Round‑Robin (sirve para main u otros RR) */
    t = try_rr();
    return t;              /* puede ser NULL si todos terminaron */
}
