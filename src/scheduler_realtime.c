//scheduler_realtime.c:
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

tcb* get_all_realtime_threads(void) {
    return realtime_queue;
}


void realtime_init(void) {
    realtime_queue = NULL;
}

/* Reemplaza la función completa por la siguiente versión estable */
static void enqueue_edf(tcb* thread)
{
    thread->next = NULL;

    /* Cola vacía ⇒ primer elemento */
    if (!realtime_queue) {
        realtime_queue = thread;
        return;
    }

    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    /* 1️⃣ Avanzar mientras el deadline sea MENOR (prioridad más alta)        */
    while (curr && curr->deadline < thread->deadline) {
        prev = curr;
        curr = curr->next;
    }

    /* 2️⃣ Avanzar CON IGUAL deadline para quedar **después** del último       */
    while (curr && curr->deadline == thread->deadline) {
        prev = curr;
        curr = curr->next;
    }

    /* Inserción */
    if (prev) prev->next = thread;
    else      realtime_queue = thread;

    thread->next = curr;
}

void realtime_add(tcb* thread) {
    printf("⏱️ [REALTIME] Añadiendo hilo tid=%d con deadline=%ld, start=%ld, end=%ld\n",
           thread->tid, thread->deadline, thread->time_start, thread->time_end);
    enqueue_edf(thread);
}

static tcb* dequeue_next_valid_thread(void)
{
    long now = get_current_time_ms();

    tcb* prev = NULL;
    tcb* curr = realtime_queue;

    while (curr) {
        long start_ms = curr->time_start * 1000;
        long end_ms   = curr->time_end   * 1000;

        /* Expirado sin ejecutarse → explota y se elimina                 */
        if (now > end_ms) {
            printf("💥 [REALTIME] Hilo %d EXPLOTÓ (now=%ld > end=%ld)\n",
                   curr->tid, now, end_ms);
            curr->finished    = true;
            curr->must_cleanup = true;

            /* quitar de la cola */
            if (prev) prev->next = curr->next;
            else      realtime_queue = curr->next;

            curr = (prev ? prev->next : realtime_queue);
            continue;
        }

        /* READY y ya alcanzó su instante de inicio ⇒ es el elegido       */
        if (curr->state == READY && now >= start_ms) {
            if (prev) prev->next = curr->next;
            else      realtime_queue = curr->next;

            curr->next = NULL;
            return curr;
        }

        /* siguiente nodo */
        prev = curr;
        curr = curr->next;
    }
    return NULL;        /* nada listo todavía */
}


tcb* realtime_next() {
    while (1) {
        tcb* best = dequeue_next_valid_thread();
        if (best) return best;

        // Si ningún hilo está listo, pero hay alguno en cola, puede que aún no haya alcanzado su time_start
        if (realtime_queue) {
            busy_wait_ms(20);     // espera un poco
            my_thread_yield();    // cede el CPU y vuelve a intentar
            continue;
        }

        // Si la cola está vacía, no hay nada más que hacer
        return NULL;
    }
}


void realtime_yield(void)
{
    long now = get_current_time_ms();
    if (!current || current->finished) return;

    long end_ms = current->time_end * 1000;
    if (now > end_ms) {
        printf("💥 [REALTIME] Hilo %d venció time_end\n", current->tid);
        current->finished = true;
        return;                 /* deja que el hilo haga cleanup */
    }

    /* 1️⃣ Antes de ceder CPU, busquemos si hay *alguien* con deadline menor */
    tcb *candidate = realtime_queue;        /* no lo modificamos */
    long best_dl   = LONG_MAX;

    while (candidate) {
        if (candidate->state == READY &&
            now >= candidate->time_start * 1000 &&
            candidate->deadline < best_dl)
            best_dl = candidate->deadline;

        candidate = candidate->next;
    }

    /* 2️⃣ Si no hay uno MEJOR, simplemente seguimos ejecutando */
    if (best_dl >= current->deadline) {
        return;                 /* → el yield se ignora */
    }

    /* 3️⃣ Hay un hilo con deadline menor ⇒ sí cambiamos */
    realtime_add(current);              /* re-encola al final del sub-grupo */
    tcb *next = dequeue_next_valid_thread();   /* será el “mejor” */
    if (next) {
        tcb *prev = current;
        current   = next;
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
