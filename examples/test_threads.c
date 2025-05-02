#include "../include/mypthreads.h"
#include <stdio.h>

void rutina(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < 2; i++) {
        printf("[Hilo %d] paso %d\n", id, i + 1);
        my_thread_yield();
    }
    my_thread_end();
}

int main() {
    my_thread_register_main();

    my_thread_t *t[4];
    int ids[4] = {1, 2, 3, 4};

    for (int i = 0; i < 4; i++) {
        my_thread_create(&t[i], SCHED_RR, rutina, &ids[i]);
    }

    my_thread_yield();

    for (int i = 0; i < 4; i++) {
        my_thread_join(t[i]);
    }

    printf("Todos los hilos han terminado correctamente.\n");
    return 0;
}
