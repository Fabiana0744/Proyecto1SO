#include "../include/mypthreads.h"
#include <stdio.h>

#define ITERACIONES 1000

my_mutex_t m;
int contador = 0;

void sumar(void *arg) {
    int id = *(int *)arg;

    for (int i = 0; i < ITERACIONES; ++i) {
        my_mutex_lock(&m);

        contador++;
        printf("Hilo %d incrementó: contador = %d\n", id, contador);  // 👈 Ahora sí usás `id`

        my_mutex_unlock(&m);
        my_thread_yield();
    }

    my_thread_end();
}


int main() {
    my_thread_register_main();

    my_thread_t *t1, *t2;
    int id1 = 1, id2 = 2;

    my_mutex_init(&m);

    my_thread_create(&t1, SCHED_RR, sumar, &id1);
    my_thread_create(&t2, SCHED_RR, sumar, &id2);

    // Ejecutar scheduler manualmente desde main
    while (!t1->finished || !t2->finished) {
        my_thread_yield();
    }

    printf("Valor final del contador: %d (esperado: %d)\n", contador, 2 * ITERACIONES);

    my_mutex_destroy(&m);

    return 0;
}
