#include "parser_config.h"
#include <stdio.h>
#include <string.h>

// Lee un archivo de configuración INI y extrae el ancho, alto y cantidad de monitores.
// Entrada: nombre del archivo INI y puntero a la estructura CanvasConfig donde guardar los datos.
// Salida: retorna 0 si se leyó correctamente, o -1 si no se pudo abrir el archivo.
int read_config(const char* filename, CanvasConfig* config) {
    FILE* f = fopen(filename, "r");
    if (!f) return -1;

    char line[256];
    config->num_monitors = 0;

    // Procesa línea por línea el archivo de configuración.
    while (fgets(line, sizeof(line), f)) {
        // Extrae el ancho del canvas si encuentra "width".
        if (strstr(line, "width")) {
            sscanf(line, "width = %d", &config->width);
        }
        // Extrae el alto del canvas si encuentra "height".
        else if (strstr(line, "height")) {
            sscanf(line, "height = %d", &config->height);
        }
        // Cuenta la cantidad de monitores listados después de "list =".
        else if (strstr(line, "list")) {
            char* tok = strtok(line, "=");
            tok = strtok(NULL, ",\n");
            while (tok) {
                config->num_monitors++;
                tok = strtok(NULL, ",\n");
            }
        }
    }

    fclose(f);
    return 0;
}
