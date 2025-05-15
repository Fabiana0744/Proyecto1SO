#ifndef OBJETO_H
#define OBJETO_H
#include "scheduler.h"

typedef struct {
    char name[32];
    char** shape;
    int shape_width;
    int shape_height;

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
} ObjetoAnimado;

#endif
