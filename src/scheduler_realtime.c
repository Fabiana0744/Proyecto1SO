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

// Inserta un hilo en la cola EDF ordenado por deadline
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

static tcb* dequeue_next_ready(void) {
    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    while (curr) {
        if (curr->state == READY) {
            if (prev) prev->next = curr->next;
            else realtime_queue = curr->next;
            curr->next = NULL;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

void realtime_init(void) {
    realtime_queue = NULL;
}

void realtime_add(tcb* thread) {
    printf("⏱️ [REALTIME] Añadiendo hilo tid=%d con deadline=%ld, start=%ld, end=%ld\n",
           thread->tid, thread->deadline, thread->time_start, thread->time_end);
    enqueue_edf(thread);
}

void realtime_yield(void) {
    long now = get_current_time_ms();

    if (current && !current->finished) {
        if (now > current->time_end * 1000) {
            printf("\n💥 Hilo %d explotó (now=%ld > end=%ld)\n", current->tid, now, current->time_end * 1000);
            current->finished = true;
            current->must_cleanup = true;
        } else {
            realtime_add(current);
        }
    }

    tcb* next = dequeue_next_ready();
    if (next) {
        tcb* prev = current;
        current = next;
        swapcontext(&prev->context, &next->context);
    }
}

void realtime_end(void) {
    current->finished = true;

    tcb* next = dequeue_next_ready();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[REALTIME] No hay más hilos activos. Finalizando...\n");
        setcontext(&main_context);
    }
}

void realtime_run(void) {
    tcb* next = dequeue_next_ready();
    if (next) {
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[REALTIME] Nada para ejecutar.\n");
    }
}

// ✅ Adaptación para el scheduler mixto
tcb* realtime_next() {
    return dequeue_next_ready();
}
