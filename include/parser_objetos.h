#ifndef PARSER_OBJETOS_H
#define PARSER_OBJETOS_H

#include "objeto.h"

#define MAX_OBJETOS 32

int cargar_objetos_desde_ini(const char* archivo_ini, ObjetoAnimado objetos[], int* total);

#endif
