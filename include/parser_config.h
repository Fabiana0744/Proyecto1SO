#ifndef PARSER_CONFIG_H
#define PARSER_CONFIG_H

typedef struct {
    int width;
    int height;
    int num_monitors;
} CanvasConfig;

int read_config(const char* filename, CanvasConfig* config);

#endif
