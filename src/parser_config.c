#include "parser_config.h"
#include <stdio.h>
#include <string.h>

int read_config(const char* filename, CanvasConfig* config) {
    FILE* f = fopen(filename, "r");
    if (!f) return -1;

    char line[256];
    config->num_monitors = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "width")) {
            sscanf(line, "width = %d", &config->width);
        } else if (strstr(line, "height")) {
            sscanf(line, "height = %d", &config->height);
        } else if (strstr(line, "list")) {
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
