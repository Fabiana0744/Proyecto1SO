#include "../include/mypthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_TURNS 60000        // total de sorteos que queremos medir

/*  Contadores de victorias */
volatile int wins1 = 0, wins2 = 0, wins3 = 0;
volatile int total  = 0;       // turnos globales consumidos

void tarea(void *arg)
{
    volatile int *mywins = (volatile int*)arg;   // apunta a wins1,2,3

    while (__sync_fetch_and_add(&total, 1) < MAX_TURNS) {
        __sync_fetch_and_add(mywins, 1);        // 1 victoria para este hilo
        my_thread_yield();                      // ceder CPU
    }
    my_thread_end();
}

int main()
{
    srand((unsigned)time(NULL));
    my_thread_register_main();

    my_thread_t *t1, *t2, *t3;

    /* Crear hilos Lottery con 1‑3‑6 tickets */
    my_thread_create(&t1, SCHED_LOTTERY, tarea, (void*)&wins1);
    t1->tickets = 1;

    my_thread_create(&t2, SCHED_LOTTERY, tarea, (void*)&wins2);
    t2->tickets = 3;

    my_thread_create(&t3, SCHED_LOTTERY, tarea, (void*)&wins3);
    t3->tickets = 6;

    /* Ejecutar hasta alcanzar MAX_TURNS */
    while (total < MAX_TURNS || !t1->finished || !t2->finished || !t3->finished)
        my_thread_yield();

    /* Mostrar resultados */
    printf("\n🏁 Resultados sobre %d sorteos\n", MAX_TURNS);
    printf("Hilo 1  ( 1 ticket) → %d   (%.2f %%)\n", wins1, wins1*100.0/MAX_TURNS);
    printf("Hilo 2  ( 3 tickets) → %d   (%.2f %%)\n", wins2, wins2*100.0/MAX_TURNS);
    printf("Hilo 3  ( 6 tickets) → %d   (%.2f %%)\n", wins3, wins3*100.0/MAX_TURNS);

    return 0;
}
