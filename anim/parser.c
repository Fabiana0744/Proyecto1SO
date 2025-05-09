#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/anim.h"
#include "../include/mypthreads.h"
#include "../external/inih/ini.h"

#define MAX_OBJECTS 10

static int obj_index = -1;
static AnimatedObject temp_objs[MAX_OBJECTS];

/* Handler que procesa cada entrada del archivo .ini */
static int handler(void *user, const char *section, const char *name, const char *value) {
    Canvas *cv = (Canvas*)user;

    // Sección [canvas]
    if (strcmp(section, "canvas") == 0) {
        if (strcmp(name, "width") == 0)
            cv->width = atoi(value);
        else if (strcmp(name, "height") == 0)
            cv->height = atoi(value);
    }

    // Secciones [object1], [object2], ...
    else if (strncmp(section, "object", 6) == 0) {
        // Avanzar a siguiente objeto si cambia sección
        if (obj_index == -1 || section[6] - '0' != obj_index + 1) {
            obj_index++;
            if (obj_index >= MAX_OBJECTS) return 0; // Límite alcanzado
            memset(&temp_objs[obj_index], 0, sizeof(AnimatedObject));
        }

        AnimatedObject *o = &temp_objs[obj_index];

        if (strcmp(name, "name") == 0)
            strncpy(o->name, value, sizeof(o->name));
        else if (strcmp(name, "scheduler") == 0) {
            strncpy(o->scheduler_type, value, sizeof(o->scheduler_type));

            if (strcmp(value, "RR") == 0)
                o->sched = SCHED_RR;
            else if (strcmp(value, "LOTTERY") == 0)
                o->sched = SCHED_LOTTERY;
            else if (strcmp(value, "REALTIME") == 0)
                o->sched = SCHED_REALTIME;
            else
                o->sched = SCHED_RR;  // Default fallback
        }
        else if (strcmp(name, "shape") == 0)
            strncpy(o->shape_path, value, sizeof(o->shape_path));
        else if (strcmp(name, "start_x") == 0)
            o->start_x = atoi(value);
        else if (strcmp(name, "start_y") == 0)
            o->start_y = atoi(value);
        else if (strcmp(name, "end_x") == 0)
            o->end_x = atoi(value);
        else if (strcmp(name, "end_y") == 0)
            o->end_y = atoi(value);
        else if (strcmp(name, "time_start") == 0)
            o->time_start = atoi(value);
        else if (strcmp(name, "time_end") == 0)
            o->time_end = atoi(value);
        else if (strcmp(name, "rotation") == 0)
            o->rotation = atoi(value);
        else if (strcmp(name, "tickets") == 0)
            o->tickets = atoi(value);
        else if (strcmp(name, "deadline") == 0)
            o->deadline = atoi(value);
    }

    return 1;
}

/* Función principal para cargar un archivo .ini */
int parse_ini(const char *path, Canvas *cv, AnimatedObject objs[], int *count) {
    obj_index = -1;
    memset(cv, 0, sizeof(Canvas));
    memset(temp_objs, 0, sizeof(temp_objs));

    if (ini_parse(path, handler, cv) < 0) {
        fprintf(stderr, "Error: No se pudo cargar archivo INI %s\n", path);
        return -1;
    }

    *count = obj_index + 1;
    for (int i = 0; i <= obj_index; i++) {
        objs[i] = temp_objs[i];
    }

    return 0;
}
