/* anim/main.c ---------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/anim.h"
#include "../include/mypthreads.h"
#include <time.h>


#define MAX_OBJECTS     10
#define FRAME_DELAY_US  120000   /* 0.12 s */

/* ======== CANVAS GLOBAL (buffer + mutex) ======================== */
static char **g_buf = NULL;
static int    g_w = 0, g_h = 0;
static my_mutex_t canvas_lock;

/* -- buffer helpers --------------------------------------------- */
static void canvas_clear(void){
    for (int y = 0; y < g_h; y++) memset(g_buf[y], ' ', g_w);
}
static void canvas_overlay(const Shape *sh, int px, int py){
    for (int r = 0; r < sh->num_lines; r++){
        int y = py + r; if (y < 0 || y >= g_h) continue;
        for (int c = 0; c < (int)strlen(sh->lines[r]); c++){
            int x = px + c; if (x < 0 || x >= g_w) continue;
            if (sh->lines[r][c] != ' ') g_buf[y][x] = sh->lines[r][c];
        }
    }
}
static void canvas_print(long id){
    putchar('+'); for (int i = 0; i < g_w; i++) putchar('-'); puts("+");
    for (int y = 0; y < g_h; y++){
        putchar('|'); fwrite(g_buf[y], 1, g_w, stdout); puts("|");
    }
    putchar('+'); for (int i = 0; i < g_w; i++) putchar('-');
    printf("+  <frame %ld>\n", id);
}

/* -- init/free --------------------------------------------------- */
static void canvas_init(int w, int h){
    g_w = w; g_h = h;
    g_buf = malloc(g_h * sizeof *g_buf);
    for (int y = 0; y < g_h; y++){
        g_buf[y] = malloc(g_w + 1); g_buf[y][g_w] = '\0';
    }
    my_mutex_init(&canvas_lock);
    canvas_clear();
}
static void canvas_free(void){
    for (int y = 0; y < g_h; y++) free(g_buf[y]);
    free(g_buf);
}

/* ======== PARSER =============================================== */
static void unread_line(FILE *f, const char *l){
    fseek(f, -(long)strlen(l), SEEK_CUR);
}
static int parse_ini(const char *path, Canvas *cv,
                     AnimatedObject objs[], int *count){
    FILE *f = fopen(path, "r"); if (!f) return -1;
    char line[256]; *count = 0;

    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "[canvas]", 8)) {
            while (fgets(line, sizeof line, f) && line[0] != '['){
                sscanf(line,"width=%d",  &cv->width);
                sscanf(line,"height=%d", &cv->height);
            }
            unread_line(f,line);
        }
        else if (!strncmp(line,"[object",7) && *count < MAX_OBJECTS) {
            AnimatedObject *o = &objs[(*count)++];
            memset(o, 0, sizeof *o);
            while (fgets(line, sizeof line, f) && line[0] != '['){
                sscanf(line, "name=%s", o->name);
                sscanf(line, "scheduler=%s", o->scheduler_type);
                sscanf(line, "shape=%s", o->shape_path);
                sscanf(line, "start_x=%d", &o->start_x);
                sscanf(line, "start_y=%d", &o->start_y);
                sscanf(line, "end_x=%d", &o->end_x);
                sscanf(line, "end_y=%d", &o->end_y);
                sscanf(line, "time_start=%d", &o->time_start);
                sscanf(line, "time_end=%d", &o->time_end);
                sscanf(line, "rotation_start=%d", &o->rotation_start);
                sscanf(line, "rotation_end=%d", &o->rotation_end);
                sscanf(line, "tickets=%d", &o->tickets);
            }
            unread_line(f,line);
        }
    }
    fclose(f);
    return 0;
}

/* ======== HILO ANIMADOR ======================================== */
typedef struct { AnimatedObject *obj; } ThreadArg;

static void animar_objeto(void *arg){
    AnimatedObject *o = ((ThreadArg*)arg)->obj;

    /* cargar shape */
    Shape sh = {0};
    FILE *sf = fopen(o->shape_path, "r");
    if (!sf){ perror("shape"); my_thread_end(); return; }
    while (fgets(sh.lines[sh.num_lines], MAX_LINE_LENGTH, sf)
           && sh.num_lines < MAX_SHAPE_LINES){
        sh.lines[sh.num_lines]
            [strcspn(sh.lines[sh.num_lines], "\n")] = '\0';
        sh.num_lines++;
    }
    fclose(sf);

    int steps = o->end_x - o->start_x; if (steps <= 0) steps = 1;
    for (int s = 0; s <= steps; s++){
        my_mutex_lock(&canvas_lock);
        canvas_overlay(&sh, o->start_x + s, o->start_y);
        my_mutex_unlock(&canvas_lock);

        usleep(FRAME_DELAY_US / 2);
        my_thread_yield();
    }
    my_thread_end();
}

/* ======== CREA HILOS =========================================== */
static void spawn_object_threads(int n, AnimatedObject objs[],
    my_thread_t *thr[], ThreadArg arg[]) {
    my_thread_register_main();

    for (int i = 0; i < n; i++) {
    arg[i].obj = &objs[i];

    scheduler_type st = SCHED_RR;  // valor por defecto
    if (strcmp(objs[i].scheduler_type, "LOTTERY") == 0)
    st = SCHED_LOTTERY;
    else if (strcmp(objs[i].scheduler_type, "REALTIME") == 0)
    st = SCHED_REALTIME;

    my_thread_create(&thr[i], st, animar_objeto, &arg[i]);
    printf("✅ Creado hilo %s (sched=%s, tickets=%d)\n",
       objs[i].name, objs[i].scheduler_type, objs[i].tickets);


    if (st == SCHED_LOTTERY)
    thr[i]->tickets = objs[i].tickets > 0 ? objs[i].tickets : 1;

    if (st == SCHED_REALTIME)
    thr[i]->deadline = objs[i].time_end;
    }
}


/* ======== PAINTER LOOP (versión correcta) ====================== */
static void painter_loop(int n, my_thread_t *thr[])
{
    printf("🎬 Entrando al painter_loop\n");
    long frame_id = 1;
    while (1) {
    
        /* 👣 1. Dar tiempo a los hilos para dibujar sobre el buffer vigente */
        my_thread_yield();
    
        /* 🧐 2. Verificar si todos terminaron DESPUÉS del yield */
        int ended = 1;
        for (int i = 0; i < n; i++)
            if (!thr[i]->finished) { ended = 0; break; }
    
        /* 🎥 3. Cuando AÚN hay hilos activos, imprime y luego limpia */
        if (!ended) {
            printf("🎬 Entrando al IFFF DE painter_loop\n");

            /* 3a. Mostrar el contenido actual */
            my_mutex_lock(&canvas_lock);
            printf("\033[2J\033[H");
            canvas_print(frame_id++);
            my_mutex_unlock(&canvas_lock);
    
            usleep(FRAME_DELAY_US);      /* retardo normal */
    
            /* 3b. Limpiar buffer para preparar el próximo cuadro */
            my_mutex_lock(&canvas_lock);
            canvas_clear();
            my_mutex_unlock(&canvas_lock);
        } else {
            /* ✅ Todos terminaron: deja el último cuadro y sal SIN limpiar */
            break;
        }
    }
    
}


/* ====================== MAIN =================================== */
int main(void)
{
    srand(time(NULL)); 
    Canvas cv; AnimatedObject objs[MAX_OBJECTS];
    int obj_count;

    if (parse_ini("config/animation.ini", &cv, objs, &obj_count) != 0){
        perror("ini"); return 1;
    }

    canvas_init(cv.width, cv.height);

    my_thread_t *thr[MAX_OBJECTS];
    ThreadArg    arg[MAX_OBJECTS];
    spawn_object_threads(obj_count, objs, thr, arg);

    
    painter_loop(obj_count, thr);

    /* dejar último frame y liberar recursos */
    canvas_free();
    printf("✅ Terminé el main\n");

    return 0;
}
