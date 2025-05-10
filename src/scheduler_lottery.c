#include "scheduler.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

static tcb* lottery_queue = NULL;
extern tcb* current_thread;
extern ucontext_t main_context;

void lottery_init() {
    lottery_queue = NULL;
    srand(time(NULL)); // Inicializa rand()
}

void lottery_add(tcb* thread) {
    thread->next = NULL;
    if (!lottery_queue) {
        lottery_queue = thread;
    } else {
        tcb* tmp = lottery_queue;
        while (tmp->next) tmp = tmp->next;
        tmp->next = thread;
    }
}

tcb* lottery_next() {
    int total_tickets = 0;
    for (tcb* t = lottery_queue; t; t = t->next) {
        if (t->state == READY) {
            total_tickets += (t->tickets > 0 ? t->tickets : 1);
        }
    }

    if (total_tickets == 0) return NULL;

    int winning_ticket = (rand() % total_tickets) + 1;
    int counter = 0;

    tcb* prev = NULL;
    tcb* curr = lottery_queue;

    while (curr) {
        if (curr->state == READY) {
            counter += (curr->tickets > 0 ? curr->tickets : 1);
            if (counter >= winning_ticket) {
                if (prev) prev->next = curr->next;
                else lottery_queue = curr->next;
                curr->next = NULL;
                return curr;
            }
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

void lottery_yield() {
    tcb* prev = current_thread;
    lottery_add(current_thread);
    tcb* next = lottery_next();

    if (next) {
        current_thread = next;
        swapcontext(&prev->context, &next->context);
    }
}

void lottery_end() {
    tcb* next = lottery_next();
    if (next) {
        current_thread = next;
        setcontext(&next->context);
    } else {
        printf("[LOTTERY] No hay más hilos. Volviendo a main.\n");
        setcontext(&main_context);
    }
}

void lottery_run() {
    tcb* next = lottery_next();
    if (next) {
        current_thread = next;
        swapcontext(&main_context, &next->context);
    } else {
        printf("[LOTTERY] No hay hilos listos.\n");
    }
}
