#ifndef PARSER_CONFIG_H
#define PARSER_CONFIG_H

// Representa la configuración del canvas global.
// Entrada: ninguna.
// Salida: estructura con ancho, alto y número de monitores.
typedef struct {
    int width;
    int height;
    int num_monitors;
} CanvasConfig;

// Lee un archivo de configuración y carga los valores en una estructura CanvasConfig.
// Entrada: nombre del archivo de configuración y puntero a la estructura CanvasConfig.
// Salida: 0 si se leyó correctamente, o -1 si hubo error.
int read_config(const char* filename, CanvasConfig* config);

#endif
