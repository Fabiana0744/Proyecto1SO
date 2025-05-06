#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ncurses.h>
#include "../include/anim.h"
#include "../include/mypthreads.h"

#define MAX_OBJECTS 10

Canvas canvas;
AnimatedObject objects[MAX_OBJECTS];
int obj_count = 0;

char canvas_buffer[100][100];  // Buffer para el canvas
my_mutex_t draw_mutex;

// Cargar figura ASCII
void load_shape(const char *path, Shape *shape) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("No se pudo abrir shape");
        exit(1);
    }

    shape->num_lines = 0;
    while (fgets(shape->lines[shape->num_lines], MAX_LINE_LENGTH, f)) {
        shape->lines[shape->num_lines][strcspn(shape->lines[shape->num_lines], "\n")] = '\0';
        shape->num_lines++;
        if (shape->num_lines >= MAX_SHAPE_LINES) break;
    }
    fclose(f);
}

void draw_canvas_frame() {
    int x0 = 0, y0 = 0;
    int x1 = canvas.width + 1;
    int y1 = canvas.height + 1;

    for (int x = x0; x <= x1; x++) {
        mvaddch(y0, x, '-');
        mvaddch(y1, x, '-');
    }
    for (int y = y0; y <= y1; y++) {
        mvaddch(y, x0, '|');
        mvaddch(y, x1, '|');
    }
    mvaddch(y0, x0, '+');
    mvaddch(y0, x1, '+');
    mvaddch(y1, x0, '+');
    mvaddch(y1, x1, '+');
}

void erase_shape_from_buffer(Shape *shape, int x, int y) {
    for (int i = 0; i < shape->num_lines; i++) {
        for (int j = 0; j < strlen(shape->lines[i]); j++) {
            int cx = x + j;
            int cy = y + i;
            if (cx >= 0 && cy >= 0 && cx < canvas.width && cy < canvas.height) {
                canvas_buffer[cy][cx] = ' ';
            }
        }
    }
}

void draw_shape_in_buffer(Shape *shape, int x, int y) {
    for (int i = 0; i < shape->num_lines; i++) {
        for (int j = 0; j < strlen(shape->lines[i]); j++) {
            int cx = x + j;
            int cy = y + i;
            if (cx >= 0 && cy >= 0 && cx < canvas.width && cy < canvas.height) {
                canvas_buffer[cy][cx] = shape->lines[i][j];
            }
        }
    }
}

void render_canvas() {
    clear();
    draw_canvas_frame();
    for (int y = 0; y < canvas.height; y++) {
        for (int x = 0; x < canvas.width; x++) {
            mvaddch(y + 1, x + 1, canvas_buffer[y][x]);
        }
    }
    refresh();
}

void animate(void *arg) {
    AnimatedObject *obj = (AnimatedObject *)arg;
    Shape shape;
    load_shape(obj->shape_path, &shape);

    int steps = 30;
    int dx = (obj->end_x - obj->start_x) / steps;
    int dy = (obj->end_y - obj->start_y) / steps;

    int prev_x = obj->start_x;
    int prev_y = obj->start_y;

    for (int i = 0; i <= steps; i++) {
        int x = obj->start_x + i * dx;
        int y = obj->start_y + i * dy;

        my_mutex_lock(&draw_mutex);
        erase_shape_from_buffer(&shape, prev_x, prev_y);
        draw_shape_in_buffer(&shape, x, y);
        render_canvas();
        my_mutex_unlock(&draw_mutex);

        prev_x = x;
        prev_y = y;

        usleep(100000);
        my_thread_yield();
    }

    my_thread_end();
}

int all_finished() {
    for (int i = 0; i < obj_count; i++) {
        if (!objects[i].thread || !objects[i].thread->finished)
            return 0;
    }
    return 1;
}

int main() {
    if (parse_ini("config/animation.ini", &canvas, objects, &obj_count) != 0) {
        fprintf(stderr, "Error al cargar .ini\n");
        return 1;
    }

    memset(canvas_buffer, ' ', sizeof(canvas_buffer));
    initscr();
    noecho();
    curs_set(0);

    my_mutex_init(&draw_mutex);
    my_thread_register_main();

    for (int i = 0; i < obj_count; i++) {
        my_thread_t *t;
        my_thread_create(&t, objects[i].sched, animate, &objects[i]);
        objects[i].thread = t;
        t->tickets = objects[i].tickets;
        t->deadline = objects[i].deadline;
    }

    while (!all_finished()) {
        my_thread_yield();
        usleep(10000);
    }

    endwin();
    return 0;
}
