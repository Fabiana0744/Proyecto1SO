#include "../include/mypthreads.h"
#include <stdio.h>

#define VUELTAS 3

void tarea(void *arg) {
    int id = *(int*)arg;

    for (int i = 0; i < VUELTAS; ++i) {
        printf("🔧 Hilo TiempoReal %d ejecuta paso %d\n", id, i + 1);
        my_thread_yield();
    }

    my_thread_end();
}

int main() {
    my_thread_register_main();

    my_thread_t *a, *b, *c;
    int id1 = 1, id2 = 2, id3 = 3;

    my_thread_create(&a, SCHED_REALTIME, tarea, &id1); a->deadline = 10;
    my_thread_create(&b, SCHED_REALTIME, tarea, &id2); b->deadline = 3;
    my_thread_create(&c, SCHED_REALTIME, tarea, &id3); c->deadline = 5;

    while (!a->finished || !b->finished || !c->finished)
        my_thread_yield();

    puts("✅ Todos los hilos de Tiempo Real han terminado.");
    return 0;
}
