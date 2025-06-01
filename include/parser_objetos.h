#ifndef PARSER_OBJETOS_H
#define PARSER_OBJETOS_H

#include "objeto.h"

#define MAX_OBJECTS 32  // Define el número máximo de objetos que se pueden cargar desde el archivo INI.

// Carga objetos animados definidos en un archivo INI.
// Entrada: ruta del archivo INI, arreglo donde se almacenarán los objetos y puntero a la cantidad total.
// Salida: retorna el número de objetos cargados correctamente o -1 si ocurrió un error.
int object_load_from_ini(const char* archivo_ini, AnimatedObject objetos[], int* total);

#endif
