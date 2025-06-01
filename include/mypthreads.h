#ifndef MYPTHREADS_H
#define MYPTHREADS_H

#include <ucontext.h>
#include <stdlib.h>
#include <stdbool.h>

#define STACK_SIZE 64 * 1024  // Define el tamaño de pila para cada hilo.

// Enumera los tipos de planificadores disponibles.
// Entrada: ninguna.
// Salida: constantes identificadoras para Round Robin, Lottery y RealTime.
typedef enum {
    SCHED_RR,      // Round Robin
    SCHED_LOTTERY, // Sorteo
    SCHED_REALTIME // Tiempo Real
} scheduler_type_t;

// Define los posibles estados de un hilo.
// Entrada: ninguna.
// Salida: constante READY, RUNNING, FINISHED o DETACHED.
typedef enum { READY, RUNNING, FINISHED, DETACHED } thread_state;

typedef int my_thread_t;  // Alias para representar el ID de un hilo.

// Estructura que representa el bloque de control de un hilo.
// Entrada: atributos de ejecución y control del hilo.
// Salida: datos usados para planificar y administrar el hilo.
typedef struct my_thread_control_block {
    my_thread_t tid;
    ucontext_t context;
    thread_state state;
    void* retval;
    struct my_thread_control_block* waiting_for_me;
    struct my_thread_control_block* next;
    int tickets;  // cantidad de tickets para scheduler LOTTERY
    long time_start;     // tiempo de inicio permitido (en segundos)
    long time_end;       // tiempo de final permitido (en segundos)
    long deadline;       // para EDF
    bool must_cleanup;   // bandera para saber si debe terminar
    bool finished;       // marca de hilo finalizado

    scheduler_type_t sched_type;  // tipo de planificador asignado al hilo

    long last_start_ms;     // instante en que tomó la CPU
    long consumed_ms;       // milisegundos gastados dentro del quantum actual

} tcb;

// Crea un nuevo hilo con el planificador especificado.
// Entrada: puntero al ID del hilo, función a ejecutar, argumento, tipo de scheduler, tickets, tiempo de inicio y fin, y deadline.
// Salida: 0 si se creó exitosamente, o -1 si hubo error.
int my_thread_create(my_thread_t* thread,
    void* (*start_routine)(void*),
    void* arg,
    scheduler_type_t sched,
    int tickets,
    long time_start,
    long time_end,
    long deadline);

// Finaliza el hilo actual y devuelve un valor.
// Entrada: puntero a valor de retorno.
// Salida: no retorna al llamador (termina el hilo).
void my_thread_end(void* retval);

// Cede voluntariamente el uso del procesador al planificador.
// Entrada: ninguna.
// Salida: 0 si se cedió correctamente, o -1 si falló.
int my_thread_yield(void);

// Espera a que el hilo especificado finalice.
// Entrada: ID del hilo a esperar y puntero para obtener su valor de retorno.
// Salida: 0 si se unió exitosamente, o -1 si hubo error.
int my_thread_join(my_thread_t thread, void** retval);

// Desasocia un hilo para que sus recursos se liberen automáticamente al finalizar.
// Entrada: ID del hilo a desacoplar.
// Salida: 0 si se logró correctamente, o -1 si falló.
int my_thread_detach(my_thread_t thread);

// Cambia el planificador y parámetros de tiempo de un hilo existente.
// Entrada: ID del hilo, nuevo tipo de scheduler, tickets y nuevos tiempos de ejecución.
// Salida: 0 si se modificó exitosamente, o -1 si hubo error.
int my_thread_chsched(my_thread_t tid, scheduler_type_t new_sched,
    int tickets, long time_start, long time_end, long deadline);


// Realiza una espera activa por la cantidad de milisegundos indicada.
// Entrada: tiempo en milisegundos.
// Salida: ninguna.
void busy_wait_ms(int ms);


// MUTEX

// Estructura que representa un mutex simple para sincronización entre hilos.
// Entrada: ninguna.
// Salida: estructura con estado de bloqueo y propietario.
typedef struct {
    int locked;         // 0 libre, 1 ocupado
    my_thread_t owner;  // hilo que tiene el candado
} my_mutex_t;

// Inicializa un mutex.
// Entrada: puntero al mutex.
// Salida: 0 si tuvo éxito, o -1 si falló.
int my_mutex_init(my_mutex_t* mutex);

// Bloquea el mutex; si está ocupado, espera.
// Entrada: puntero al mutex.
// Salida: 0 si se adquirió el bloqueo, o -1 si falló.
int my_mutex_lock(my_mutex_t* mutex);

// Intenta bloquear el mutex sin esperar si está ocupado.
// Entrada: puntero al mutex.
// Salida: 0 si lo adquirió, o -1 si ya estaba ocupado.
int my_mutex_trylock(my_mutex_t* mutex);

// Libera un mutex previamente bloqueado.
// Entrada: puntero al mutex.
// Salida: 0 si se liberó, o -1 si falló.
int my_mutex_unlock(my_mutex_t* mutex);

// Destruye un mutex (opcional en esta implementación).
// Entrada: puntero al mutex.
// Salida: 0 si se destruyó correctamente, o -1 si falló.
int my_mutex_destroy(my_mutex_t* mutex);

// Estructura auxiliar para envolver la función y el argumento del hilo.
// Entrada: función a ejecutar y su argumento.
// Salida: estructura para uso interno al crear hilos.
typedef struct {
    void* (*func)(void*);
    void* arg;
} my_thread_wrapper_args;

#endif
