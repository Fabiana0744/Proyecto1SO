#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

static tcb* rr_queue = NULL;
extern ucontext_t main_context;
extern tcb* current;

void rr_init() {
    rr_queue = NULL;
}

void rr_add(tcb* thread) {
    thread->next = NULL;
    if (!rr_queue) {
        rr_queue = thread;
    } else {
        tcb* temp = rr_queue;
        while (temp->next) temp = temp->next;
        temp->next = thread;
    }
}

tcb* rr_next() {
    if (!rr_queue) return NULL;
    tcb* t = rr_queue;
    rr_queue = rr_queue->next;
    return t;
}

void rr_yield() {
    tcb* prev = current;
    rr_add(prev);
    tcb* next = rr_next();
    if (next) {
        current = next;
        swapcontext(&prev->context, &next->context);
    }
}

void rr_end() {
    tcb* next = rr_next();
    if (next) {
        current = next;
        setcontext(&next->context);
    } else {
        printf("[RR] No hay más hilos. Regresando al main.\n");
        setcontext(&main_context);
    }
}

void rr_run() {
    tcb* next = rr_next();
    if (next) {
        current = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[RR] No hay hilos listos para ejecutar.\n");
    }
}
