#include "parser_objetos.h"
#include "../external/inih/ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjetoAnimado* current_obj = NULL;
static ObjetoAnimado* lista = NULL;
static int* total_objetos = NULL;

static int handler(void* user, const char* section, const char* name, const char* value) {
    if (strncmp(section, "Objeto", 6) == 0) {
        int index = atoi(&section[6]) - 1;
        if (index >= MAX_OBJETOS) return 0;

        current_obj = &lista[index];
        if (index + 1 > *total_objetos) *total_objetos = index + 1;

        if (strcmp(name, "shape") == 0) {
            strncpy(current_obj->name, section, sizeof(current_obj->name));
            FILE* f = fopen(value, "r");
            if (!f) return 0;
            char buffer[100][200];
            int h = 0, w = 0;
            while (fgets(buffer[h], sizeof(buffer[h]), f)) {
                int len = strlen(buffer[h]);
                if (buffer[h][len - 1] == '\n') buffer[h][len - 1] = '\0';
                if (len > w) w = len;
                h++;
            }
            fclose(f);

            current_obj->shape_height = h;
            current_obj->shape_width = w;
            current_obj->shape = malloc(h * sizeof(char*));
            for (int i = 0; i < h; i++) {
                current_obj->shape[i] = strdup(buffer[i]);
            }
        } else if (strcmp(name, "x_start") == 0) current_obj->x_start = atoi(value);
        else if (strcmp(name, "y_start") == 0) current_obj->y_start = atoi(value);
        else if (strcmp(name, "x_end") == 0) current_obj->x_end = atoi(value);
        else if (strcmp(name, "y_end") == 0) current_obj->y_end = atoi(value);
        else if (strcmp(name, "rotation_start") == 0) current_obj->rotation_start = atoi(value);
        else if (strcmp(name, "rotation_end") == 0) current_obj->rotation_end = atoi(value);
        else if (strcmp(name, "tickets") == 0) current_obj->tickets = atoi(value);
        else if (strcmp(name, "time_start") == 0) current_obj->time_start = atol(value);
        else if (strcmp(name, "time_end") == 0) current_obj->time_end = atol(value);
        else if (strcmp(name, "deadline") == 0) current_obj->deadline = atol(value);
        else if (strcmp(name, "scheduler") == 0) {
            if (strcmp(value, "RR") == 0) current_obj->scheduler = SCHED_RR;
            else if (strcmp(value, "LOTTERY") == 0) current_obj->scheduler = SCHED_LOTTERY;
            else if (strcmp(value, "REALTIME") == 0) current_obj->scheduler = SCHED_REALTIME;
        }
    }

    return 1;
}

int cargar_objetos_desde_ini(const char* archivo_ini, ObjetoAnimado objetos[], int* total) {
    lista = objetos;
    total_objetos = total;
    *total = 0;
    return ini_parse(archivo_ini, handler, NULL);
}
