#include "scheduler.h"
#include <stdio.h>
#include <limits.h>

static tcb* realtime_queue = NULL;
extern ucontext_t main_context;
extern tcb* current;

extern long get_current_time_ms(); // si está en utils

void realtime_init() {
    realtime_queue = NULL;
}

void realtime_add(tcb* thread) {
    printf("⏱️ [REALTIME] Añadiendo hilo tid=%d con deadline=%ld, start=%ld, end=%ld\n",
        thread->tid, thread->deadline, thread->time_start, thread->time_end);
    thread->next = NULL;
    if (!realtime_queue) {
        realtime_queue = thread;
    } else {
        tcb* temp = realtime_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

tcb* realtime_next() {
    long now = get_current_time_ms();
    tcb* curr = realtime_queue;
    tcb* best = NULL;
    long best_deadline = LONG_MAX;

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
                printf("\n💥 Hilo %d explotó (now=%ld > end=%ld)\n", curr->tid, now, end_ms);
                return curr;
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

    return best;
}

void realtime_yield() {
    long now = get_current_time_ms();

    if (current && !current->finished) {
        realtime_add(current);
    }

    tcb* next = realtime_next();
    if (next) {
        tcb* prev = current;
        current = next;
        swapcontext(&prev->context, &next->context);
    }
}

void realtime_end() {
    current->finished = true;

    tcb* next = realtime_next();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[REALTIME] No hay más hilos activos. Finalizando...\n");
        setcontext(&main_context);
    }
}

void realtime_run() {
    tcb* next = realtime_next();
    if (next) {
        printf("🚀 [REALTIME] Iniciando ejecución con tid=%d\n", next->tid);
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[REALTIME] Nada para ejecutar.\n");
    }
}
