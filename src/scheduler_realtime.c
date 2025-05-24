#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <limits.h>
#include <sys/time.h>
#include <stdbool.h>

static tcb* realtime_queue = NULL;

extern ucontext_t main_context;
extern tcb* current;
extern long get_current_time_ms();

void realtime_init(void) {
    realtime_queue = NULL;
}

static void enqueue_edf(tcb* thread) {
    thread->next = NULL;

    if (!realtime_queue || thread->deadline < realtime_queue->deadline) {
        thread->next = realtime_queue;
        realtime_queue = thread;
        return;
    }

    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    while (curr && thread->deadline >= curr->deadline) {
        prev = curr;
        curr = curr->next;
    }

    prev->next = thread;
    thread->next = curr;
}

void realtime_add(tcb* thread) {
    printf("⏱️ [REALTIME] Añadiendo hilo tid=%d con deadline=%ld, start=%ld, end=%ld\n",
           thread->tid, thread->deadline, thread->time_start, thread->time_end);
    enqueue_edf(thread);
}

static tcb* dequeue_next_valid_thread() {
    long now = get_current_time_ms();
    tcb* prev = NULL;
    tcb* curr = realtime_queue;
    tcb* best = NULL;
    tcb* best_prev = NULL;

    while (curr) {
        long start_ms = curr->time_start * 1000;
        long end_ms   = curr->time_end * 1000;
        tcb* next = curr->next;

        if (curr->state == READY) {
            if (now > end_ms) {
                printf("💥 [REALTIME] Hilo %d EXPLOTÓ sin ejecutarse (now=%ld > end=%ld)\n",
                       curr->tid, now, end_ms);
                curr->finished = true;
                curr->must_cleanup = true;

                if (prev) prev->next = next;
                else realtime_queue = next;

                curr = next;
                continue;
            }

            if (now >= start_ms) {
                if (!best || curr->deadline < best->deadline) {
                    best = curr;
                    best_prev = prev;
                }
            }
        }

        prev = curr;
        curr = next;
    }

    if (best) {
        if (best_prev) best_prev->next = best->next;
        else realtime_queue = best->next;
        best->next = NULL;
        return best;
    }

    return NULL;
}

tcb* realtime_next() {
    return dequeue_next_valid_thread();
}

void realtime_yield(void) {
    long now = get_current_time_ms();
    if (!current || current->finished) return;

    long end_ms = current->time_end * 1000;

    if (now > end_ms) {
        printf("💥 [REALTIME] Hilo %d venció time_end — marcar como terminado, NO eliminar\n",
               current->tid);
        current->finished = true;
        // ❌ NO usar must_cleanup
        // ❌ NO usar scheduler_end
        return;  // permite que el hilo vuelva y haga su cleanup
    }
    

    realtime_add(current);  // Reinsertar si aún puede seguir

    tcb* next = dequeue_next_valid_thread();
    if (next) {
        tcb* prev = current;
        current = next;
        swapcontext(&prev->context, &next->context);
    }
}

void realtime_end(void) {
    current->finished = true;
    tcb* next = dequeue_next_valid_thread();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[REALTIME] No hay más hilos activos. Finalizando...\n");
        setcontext(&main_context);
    }
}

void realtime_run(void) {
    tcb* next = dequeue_next_valid_thread();
    if (next) {
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[REALTIME] Nada para ejecutar.\n");
    }
}
