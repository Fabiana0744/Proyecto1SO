#include "../include/mypthreads.h"
#include "../include/scheduler.h"
#include <stdlib.h>          // rand()
#include <stdio.h>


/* Sortea un entero en [1..N] */
static inline int rand_1_N(int N) { return (rand() % N) + 1; }

my_thread_t* scheduler_next_lottery(void)
{
    my_thread_t *head = get_current_thread();
    my_thread_t *t    = head;
    int total = 0;

    /* 1) Sumar tiquetes de hilos vivos que usan SCHED_LOTTERY */
    do {
        if (!t->finished && t->sched == SCHED_LOTTERY)
            total += (t->tickets > 0 ? t->tickets : 1);
        t = t->next;
    } while (t != head);

    if (total == 0)          // no hay hilos lottery listos
        return NULL;

    /* 2) Sacar un número ganador */
    int ganador = rand_1_N(total);

    printf("🎟️  Total tickets: %d\n", total);
    printf("🎯 Número ganador: %d\n", ganador);


    /* 3) Recorrer de nuevo y hallar al afortunado */
    t = head;
    int acumulado = 0;
    do {
        if (!t->finished && t->sched == SCHED_LOTTERY) {
            acumulado += (t->tickets > 0 ? t->tickets : 1);
            if (acumulado >= ganador)
                return t;
        }
        t = t->next;
    } while (t != head);



    return NULL;             // seguridad
}
