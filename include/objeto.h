#ifndef OBJETO_H
#define OBJETO_H
#include "scheduler.h"

#define MAX_SHAPE_ROWS 20
#define MAX_SHAPE_COLS 20

typedef struct {
    char data[MAX_SHAPE_ROWS][MAX_SHAPE_COLS];
    int rows;  // Altura actual (activa)
    int cols;  // Anchura actual (activa)
} ShapeMatrix;


typedef struct {
    char name[32];

    int x_start, y_start;
    int x_end, y_end;

    int current_x, current_y;

    int rotation_start;

    int rotation_end;

    scheduler_type_t scheduler;
    int tickets;

    long time_start;
    long time_end;
    long deadline;
    int id;
    ShapeMatrix shape;

} ObjetoAnimado;

#endif
