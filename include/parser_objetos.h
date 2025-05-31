#ifndef PARSER_OBJETOS_H
#define PARSER_OBJETOS_H

#include "objeto.h"

#define MAX_OBJECTS 32

int object_load_from_ini(const char* archivo_ini, AnimatedObject objetos[], int* total);

#endif
