#include "parser_objetos.h"
#include "../external/inih/ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Puntero al objeto actualmente procesado.
// Entrada: establecido dentro del handler.
// Salida: usado para modificar campos del objeto correspondiente.
static AnimatedObject* current_obj = NULL;

// Lista de objetos a llenar desde el archivo INI.
// Entrada: definida por el usuario al llamar a object_load_from_ini.
// Salida: modificada por el handler con los datos del archivo.
static AnimatedObject* list = NULL;

// Puntero al contador total de objetos cargados.
// Entrada: inicializado por el usuario.
// Salida: actualizado automáticamente durante el parseo.
static int* object_total = NULL;

// Manejador que procesa cada sección y clave-valor del archivo INI.
// Entrada: sección actual, nombre de la clave y su valor asociado.
// Salida: 1 si la clave fue reconocida correctamente, 0 si hubo error.
static int handler(void* user, const char* section, const char* name, const char* value) {
    (void)user;
    if (strncmp(section, "Objeto", 6) == 0) {
        int index = atoi(&section[6]) - 1;
        if (index >= MAX_OBJECTS) return 0;

        current_obj = &list[index];
        if (index + 1 > *object_total) *object_total = index + 1;

        if (strcmp(name, "shape") == 0) {
            // Carga la forma del objeto desde un archivo externo.
            strncpy(current_obj->name, section, sizeof(current_obj->name));
            FILE* f = fopen(value, "r");
            if (!f) return 0;

            char buffer[100][200];
            int h = 0, w = 0;

            // Lee línea por línea y calcula dimensiones de la figura.
            while (fgets(buffer[h], sizeof(buffer[h]), f)) {
                int len = strlen(buffer[h]);
                if (buffer[h][len - 1] == '\n') buffer[h][len - 1] = '\0';
                if (len > w) w = len;
                h++;
            }
            fclose(f);

            current_obj->shape.rows = h;
            current_obj->shape.cols = w;

            // Copia la figura leída en la matriz de forma del objeto.
            for (int i = 0; i < h; i++) {
                int len = strlen(buffer[i]);
                for (int j = 0; j < w; j++) {
                    current_obj->shape.data[i][j] = (j < len) ? buffer[i][j] : ' ';
                }
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
            // Asigna el tipo de scheduler según el texto leído.
            if (strcmp(value, "RR") == 0) current_obj->scheduler = SCHED_RR;
            else if (strcmp(value, "LOTTERY") == 0) current_obj->scheduler = SCHED_LOTTERY;
            else if (strcmp(value, "REALTIME") == 0) current_obj->scheduler = SCHED_REALTIME;
        }
    }

    return 1;
}

// Carga un conjunto de objetos animados definidos en un archivo INI.
// Entrada: ruta del archivo INI, arreglo de objetos a llenar, puntero a total.
// Salida: número de claves procesadas, o <0 si ocurrió un error al abrir el archivo.
int object_load_from_ini(const char* archivo_ini, AnimatedObject objects[], int* total) {
    list = objects;
    object_total = total;
    *total = 0;
    return ini_parse(archivo_ini, handler, NULL);
}
