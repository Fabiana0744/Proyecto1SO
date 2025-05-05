#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/anim.h"
#include <unistd.h>  // for usleep()

void load_canvas(FILE *file, Canvas *canvas) {
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "[canvas]", 8) == 0) {
            while (fgets(line, sizeof(line), file) && line[0] != '[') {
                if (sscanf(line, "width=%d", &canvas->width) == 1) continue;
                if (sscanf(line, "height=%d", &canvas->height) == 1) continue;
            }
            break;
        }
    }
}

void load_monitor(FILE *file, Monitor *monitor) {
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "[monitor1]", 10) == 0) {
            while (fgets(line, sizeof(line), file) && line[0] != '[') {
                if (sscanf(line, "name=%s", monitor->name) == 1) continue;
                if (sscanf(line, "x=%d", &monitor->x) == 1) continue;
                if (sscanf(line, "y=%d", &monitor->y) == 1) continue;
                if (sscanf(line, "width=%d", &monitor->width) == 1) continue;
                if (sscanf(line, "height=%d", &monitor->height) == 1) continue;
            }
            break;
        }
    }
}

void load_object(FILE *file, AnimatedObject *obj) {
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "[object1]", 9) == 0) {
            while (fgets(line, sizeof(line), file) && line[0] != '[') {
                if (sscanf(line, "name=%s", obj->name) == 1) continue;
                if (sscanf(line, "scheduler=%s", obj->scheduler_type) == 1) continue;
                if (sscanf(line, "shape=%s", obj->shape_path) == 1) continue;
                if (sscanf(line, "start_x=%d", &obj->start_x) == 1) continue;
                if (sscanf(line, "start_y=%d", &obj->start_y) == 1) continue;
                if (sscanf(line, "end_x=%d", &obj->end_x) == 1) continue;
                if (sscanf(line, "end_y=%d", &obj->end_y) == 1) continue;
                if (sscanf(line, "time_start=%d", &obj->time_start) == 1) continue;
                if (sscanf(line, "time_end=%d", &obj->time_end) == 1) continue;
                if (sscanf(line, "rotation_start=%d", &obj->rotation_start) == 1) continue;
                if (sscanf(line, "rotation_end=%d", &obj->rotation_end) == 1) continue;
            }
            break;
        }
    }
}

// Loads the ASCII shape from a file into a Shape struct
int load_shape(const char *path, Shape *shape) {
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Failed to open shape file");
        return -1;
    }

    shape->num_lines = 0;
    while (fgets(shape->lines[shape->num_lines], MAX_LINE_LENGTH, file)) {
        // Remove newline
        shape->lines[shape->num_lines][strcspn(shape->lines[shape->num_lines], "\n")] = 0;
        shape->num_lines++;

        if (shape->num_lines >= MAX_SHAPE_LINES)
            break;
    }

    fclose(file);
    return 0;
}


// Draws a shape on the canvas at a given position
void draw_shape_on_canvas(Canvas *canvas, Shape *shape, int pos_x, int pos_y) {
    char canvas_matrix[canvas->height][canvas->width + 1];  // +1 for null terminator

    // Initialize canvas with spaces
    for (int i = 0; i < canvas->height; i++) {
        for (int j = 0; j < canvas->width; j++) {
            canvas_matrix[i][j] = ' ';
        }
        canvas_matrix[i][canvas->width] = '\0';  // null terminator
    }

    // Overlay the shape into the canvas
    for (int i = 0; i < shape->num_lines; i++) {
        int canvas_y = pos_y + i;
        if (canvas_y >= canvas->height) break;

        for (int j = 0; j < strlen(shape->lines[i]); j++) {
            int canvas_x = pos_x + j;
            if (canvas_x >= canvas->width) break;

            char ch = shape->lines[i][j];
            if (ch != ' ') {
                canvas_matrix[canvas_y][canvas_x] = ch;
            }
        }
    }

    // Print canvas
    printf("\n📋 Canvas View (%dx%d):\n", canvas->width, canvas->height);
    for (int i = 0; i < canvas->height; i++) {
        printf("%s\n", canvas_matrix[i]);
    }
}

void simulate_animation(Canvas *canvas, Shape *shape, AnimatedObject *obj) {
    int steps = obj->end_x - obj->start_x;
    if (steps == 0) steps = 1;  // avoid divide by zero

    printf("\n🎞 Starting animation from x=%d to x=%d\n", obj->start_x, obj->end_x);

    for (int step = 0; step <= steps; step++) {
        int current_x = obj->start_x + step;

        // Clear screen (terminal only)
        printf("\033[2J\033[H");  // ANSI escape codes to clear screen and move cursor to home

        printf("Frame %d (x=%d):\n", step, current_x);
        draw_shape_on_canvas(canvas, shape, current_x, obj->start_y);

        usleep(200000);  // 200ms = 0.2s delay per frame
    }

    printf("\n✅ Animation finished.\n");
}



int main() {
    FILE *f = fopen("config/animation.ini", "r");
    if (!f) {
        perror("Failed to open .ini file");
        return 1;
    }

    Canvas canvas;
    Monitor monitor1;
    AnimatedObject obj;
    Shape shape;

    load_canvas(f, &canvas);
    rewind(f);
    load_monitor(f, &monitor1);
    rewind(f);
    load_object(f, &obj);
    fclose(f);

    // Output parsed data
    printf("Canvas: %dx%d\n", canvas.width, canvas.height);
    printf("Monitor: %s at (%d,%d), size: %dx%d\n",
           monitor1.name, monitor1.x, monitor1.y, monitor1.width, monitor1.height);
    printf("Object: %s moves (%d,%d) -> (%d,%d), time %d–%d, shape: %s\n",
           obj.name, obj.start_x, obj.start_y, obj.end_x, obj.end_y,
           obj.time_start, obj.time_end, obj.shape_path);

    // Load and preview shape
    if (load_shape(obj.shape_path, &shape) == 0) {
        printf("\nPreview of shape '%s':\n", obj.name);
        for (int i = 0; i < shape.num_lines; i++) {
            printf("%s\n", shape.lines[i]);
        }
    }

    draw_shape_on_canvas(&canvas, &shape, obj.start_x, obj.start_y);

    simulate_animation(&canvas, &shape, &obj);



    return 0;
}

