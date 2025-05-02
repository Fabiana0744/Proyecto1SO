#include "../include/mypthreads.h"
#include "../include/scheduler.h"

// Esta función escoge el siguiente hilo basado en su scheduler
my_thread_t* my_scheduler_next() {
    switch (get_current_thread()->sched) {
        case SCHED_RR:
        default:
            return scheduler_next_thread();
    }
}
