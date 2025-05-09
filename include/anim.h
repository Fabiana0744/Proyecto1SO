#ifndef ANIM_H
#define ANIM_H

#include "mypthreads.h"  // Para scheduler_type enum

#define MAX_SHAPE_LINES 20
#define MAX_LINE_LENGTH 100

#define MAX_MONITORS 10


typedef struct {
    char lines[MAX_SHAPE_LINES][MAX_LINE_LENGTH];
    int num_lines;
} Shape;

typedef struct {
    int width;
    int height;
} Canvas;

typedef struct {
    char name[20];
    int x, y;
    int width, height;
} Monitor;

typedef struct {
    char name[20];
    char scheduler_type[10];     // Texto original del scheduler (RR, LOTTERY, REALTIME)
    scheduler_type sched;        // Enum usado por mypthreads
    char shape_path[100];
    int start_x, start_y;
    int end_x, end_y;
    int time_start;
    int time_end;
    int rotation_start;
    int rotation_end;
    int tickets;
    int deadline;                // Para SCHED_REALTIME

    my_thread_t *thread;         // 🧵 Puntero al hilo asociado (nuevo)
} AnimatedObject;

int parse_ini(const char *path, Canvas *cv, AnimatedObject objs[], int *count);

#endif
