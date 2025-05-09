#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <sys/time.h>

#include "../include/anim.h"
#include "../include/mypthreads.h"

FILE *log_file = NULL;


/* --------------------------------------------------
 * Parámetros globales
 * --------------------------------------------------*/
#define MAX_OBJECTS   10
#define MAX_CANVAS    120          /* límite para buffer+mapa */
#define BASE_STEP_US  100000       /* 100 ms entre cuadros */

Canvas          canvas;
AnimatedObject  objects[MAX_OBJECTS];
int             obj_count = 0;

char canvas_buffer[MAX_CANVAS][MAX_CANVAS];   /* buffer ASCII visible   */
int  occ_map[MAX_CANVAS][MAX_CANVAS];         /* -1 libre | ≥0 id hilo */

my_mutex_t draw_mutex;                        /* protege buffer+mapa    */

/* --------------------------------------------------
 * Utilidades de dibujo y ocupación
 * --------------------------------------------------*/
static void load_shape(const char *path, Shape *s)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("shape"); exit(EXIT_FAILURE); }
    s->num_lines = 0;
    while (fgets(s->lines[s->num_lines], MAX_LINE_LENGTH, f)) {
        s->lines[s->num_lines][strcspn(s->lines[s->num_lines], "\n")] = '\0';
        if (++s->num_lines >= MAX_SHAPE_LINES) break;
    }
    fclose(f);
}

static void draw_frame(void)
{
    int x1 = canvas.width  + 1;
    int y1 = canvas.height + 1;
    for (int x = 0; x <= x1; x++) { mvaddch(0,  x,  '-'); mvaddch(y1, x, '-'); }
    for (int y = 0; y <= y1; y++) { mvaddch(y, 0, '|');  mvaddch(y,  x1, '|'); }
    mvaddch(0, 0, '+'); mvaddch(0, x1, '+'); mvaddch(y1, 0, '+'); mvaddch(y1, x1, '+');
}

static void buffer_clear_shape(Shape *s, int x, int y)
{
    for (int i = 0; i < s->num_lines; i++)
        for (int j = 0; j < (int)strlen(s->lines[i]); j++) {
            int cx = x + j, cy = y + i;
            if (cx>=0 && cy>=0 && cx<canvas.width && cy<canvas.height)
                canvas_buffer[cy][cx] = ' ';
        }
}
static void buffer_draw_shape(Shape *s, int x, int y)
{
    for (int i = 0; i < s->num_lines; i++)
        for (int j = 0; j < (int)strlen(s->lines[i]); j++) {
            int cx = x + j, cy = y + i;
            if (cx>=0 && cy>=0 && cx<canvas.width && cy<canvas.height)
                canvas_buffer[cy][cx] = s->lines[i][j];
        }
}

static void mark_region(Shape *s, int x, int y, int id, int occupy)
{
    for (int i = 0; i < s->num_lines; i++)
        for (int j = 0; j < (int)strlen(s->lines[i]); j++) {
            int cx = x + j, cy = y + i;
            if (cx>=0 && cy>=0 && cx<canvas.width && cy<canvas.height)
                occ_map[cy][cx] = occupy ? id : -1;
        }
}


static void render(void)
{
    clear();
    draw_frame();
    for (int y = 0; y < canvas.height; y++)
        for (int x = 0; x < canvas.width; x++)
            mvaddch(y+1, x+1, canvas_buffer[y][x]);
    refresh();
}


static int region_blocker(Shape *s, int x, int y, int self_id)
{
    for (int i = 0; i < s->num_lines; i++)
        for (int j = 0; j < (int)strlen(s->lines[i]); j++) {
            int cx = x + j, cy = y + i;
            if (cx < 0 || cy < 0 || cx >= canvas.width || cy >= canvas.height)
                continue;
            int occ = occ_map[cy][cx];
            if (occ != -1 && occ != self_id) return occ;
        }
    return -1; // libre o solo yo
}


void shape_to_matrix(Shape *s, char matrix[MAX_SHAPE_LINES][MAX_LINE_LENGTH], int *rows, int *cols) {
    *rows = s->num_lines;
    *cols = 0;
    for (int i = 0; i < *rows; i++) {
        int len = strlen(s->lines[i]);
        if (len > *cols) *cols = len;
    }

    // Rellenar con espacios para que todas las filas tengan igual longitud
    for (int i = 0; i < *rows; i++) {
        for (int j = 0; j < *cols; j++) {
            if (j < (int)strlen(s->lines[i]))
                matrix[i][j] = s->lines[i][j];
            else
                matrix[i][j] = ' ';
        }
    }
}

void rotate_matrix(char src[MAX_SHAPE_LINES][MAX_LINE_LENGTH],
    char dst[MAX_SHAPE_LINES][MAX_LINE_LENGTH],
    int rows, int cols, int angle) {

    switch (angle) {
    case 90:
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                dst[j][rows - 1 - i] = src[i][j];
        break;

    case 180:
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                dst[rows - 1 - i][cols - 1 - j] = src[i][j];
        break;

    case 270:
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                dst[cols - 1 - j][i] = src[i][j];
        break;

    default:
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                dst[i][j] = src[i][j];
    }
}


void matrix_to_shape(char matrix[MAX_SHAPE_LINES][MAX_LINE_LENGTH], Shape *s, int rows, int cols) {
    s->num_lines = rows;
    for (int i = 0; i < rows; i++) {
        memcpy(s->lines[i], matrix[i], cols);
        s->lines[i][cols] = '\0';  // terminador nulo
    }
}


void rotate_shape(Shape *s, int angle) {
    char src[MAX_SHAPE_LINES][MAX_LINE_LENGTH];
    char dst[MAX_SHAPE_LINES][MAX_LINE_LENGTH];
    int rows, cols;

    shape_to_matrix(s, src, &rows, &cols);
    rotate_matrix(src, dst, rows, cols, angle);

    int new_rows = (angle == 90 || angle == 270) ? cols : rows;
    int new_cols = (angle == 90 || angle == 270) ? rows : cols;

    matrix_to_shape(dst, s, new_rows, new_cols);
}




/* --------------------------------------------------
 * Hilo de animación
 * --------------------------------------------------*/
static void animate(void *arg)
{
    AnimatedObject *obj = (AnimatedObject*)arg;
    Shape shape; load_shape(obj->shape_path, &shape);


    if (obj->rotation != 0)
    rotate_shape(&shape, obj->rotation);


    int dx_total = abs(obj->end_x - obj->start_x);
    int dy_total = abs(obj->end_y - obj->start_y);
    int steps    = dx_total > dy_total ? dx_total : dy_total;
    if (!steps) steps = 1;
    float dx = (float)(obj->end_x - obj->start_x) / steps;
    float dy = (float)(obj->end_y - obj->start_y) / steps;

    int cur_x = obj->start_x;
    int cur_y = obj->start_y;

    my_thread_t *self = get_current_thread();

    for (int i = 0; i <= steps; i++) {
        if (self->finished) break;

        int next_x = obj->start_x + (int)(i * dx);
        int next_y = obj->start_y + (int)(i * dy);

        int moved = 0;
        while (!moved) {
            my_mutex_lock(&draw_mutex);
        
            int blocker = region_blocker(&shape, next_x, next_y, self->id);
        
            if (blocker == -1 || blocker > self->id) {
                // Podemos movernos
                mark_region(&shape, cur_x, cur_y, self->id, 0);
                buffer_clear_shape(&shape, cur_x, cur_y);
        
                mark_region(&shape, next_x, next_y, self->id, 1);
                buffer_draw_shape(&shape, next_x, next_y);
                render();
                my_mutex_unlock(&draw_mutex);
        
                cur_x = next_x;
                cur_y = next_y;
                moved = 1;
            } else {
                // Nos toca esperar
                mark_region(&shape, cur_x, cur_y, self->id, 1); // aseguramos que seguimos ocupando
                buffer_draw_shape(&shape, cur_x, cur_y);
                render();
                my_mutex_unlock(&draw_mutex);
                usleep(40000);
                my_thread_yield();
            }
        }
        

        usleep(BASE_STEP_US);
        my_thread_yield();
    }

    /* limpieza final */
    my_mutex_lock(&draw_mutex);
    mark_region(&shape, cur_x, cur_y, self->id, 0);
    buffer_clear_shape(&shape, cur_x, cur_y);
    render();
    my_mutex_unlock(&draw_mutex);

    my_thread_end();
}


/* --------------------------------------------------
 * Comprobación de finalización global
 * --------------------------------------------------*/
static int all_done(void)
{
    for (int i = 0; i < obj_count; i++)
        if (!objects[i].thread || !objects[i].thread->finished)
            return 0;
    return 1;
}

/* --------------------------------------------------
 * main
 * --------------------------------------------------*/
int main(void)
{
    log_file = fopen("rotations.txt", "w");
    if (!log_file) {
        perror("No se pudo abrir logs/rotations.txt");
        return EXIT_FAILURE;
    }

    extern long program_start_time;
    struct timeval tv; gettimeofday(&tv, NULL);
    program_start_time = tv.tv_sec * 1000L + tv.tv_usec / 1000L;

    srand(time(NULL));

    if (parse_ini("config/animation.ini", &canvas, objects, &obj_count)) {
        fprintf(stderr, "Error al cargar .ini\n");
        return EXIT_FAILURE;
    }

    if (canvas.width >= MAX_CANVAS || canvas.height >= MAX_CANVAS) {
        fprintf(stderr, "Canvas demasiado grande (max %dx%d)\n", MAX_CANVAS, MAX_CANVAS);
        return EXIT_FAILURE;
    }

    memset(canvas_buffer, ' ', sizeof(canvas_buffer));
    for (int y = 0; y < MAX_CANVAS; y++)
        for (int x = 0; x < MAX_CANVAS; x++)
            occ_map[y][x] = -1;

    initscr(); noecho(); curs_set(0);

    my_mutex_init(&draw_mutex);
    my_thread_register_main();

    for (int i = 0; i < obj_count; i++) {
        my_thread_t *t;
        my_thread_create(&t, objects[i].sched, animate, &objects[i]);
        objects[i].thread = t;
        t->tickets    = objects[i].tickets;
        t->deadline   = objects[i].deadline;
        t->time_start = objects[i].time_start;
        t->time_end   = objects[i].time_end;
    }

    while (!all_done()) {
        my_thread_yield();
        usleep(10000);
    }

    endwin();
    fclose(log_file);

    return EXIT_SUCCESS;
}
