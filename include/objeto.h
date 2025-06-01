#ifndef OBJETO_H
#define OBJETO_H

#include "scheduler.h"

#define MAX_SHAPE_ROWS 20  // Define el número máximo de filas que puede tener una figura.
#define MAX_SHAPE_COLS 20  // Define el número máximo de columnas que puede tener una figura.

// Representa la forma visual de un objeto mediante una matriz de caracteres.
// Entrada: ninguna directamente, se asigna al definir la figura.
// Salida: matriz de forma con dimensiones activas (filas y columnas).
typedef struct {
    char data[MAX_SHAPE_ROWS][MAX_SHAPE_COLS];
    int rows;  // Altura actual (activa)
    int cols;  // Anchura actual (activa)
} ShapeMatrix;

// Representa un objeto animado con posición, forma, rotación y tiempos de ejecución.
// Entrada: parámetros definidos por configuración (posición, tiempos, scheduler, forma).
// Salida: estructura completa que será animada en el sistema.
typedef struct {
    char name[32];

    int x_start, y_start;      // Posición inicial en el canvas
    int x_end, y_end;          // Posición final en el canvas

    int current_x, current_y;  // Posición actual durante la animación

    int rotation_start;        // Ángulo inicial de rotación
    int rotation_end;          // Ángulo final de rotación

    scheduler_type_t scheduler; // Tipo de planificador asignado
    int tickets;                // Tickets para scheduler tipo LOTTERY

    long time_start;           // Tiempo de inicio permitido
    long time_end;             // Tiempo de final permitido
    long deadline;             // Deadline para políticas de tiempo real

    int id;                    // Identificador único del objeto

    ShapeMatrix shape;         // Forma visual del objeto

} AnimatedObject;

#endif
