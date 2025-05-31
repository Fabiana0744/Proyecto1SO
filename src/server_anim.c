#include "net.h"
#include "parser_config.h"
#include "parser_objetos.h"
#include "mypthreads.h"
#include "objeto.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define PORT 5000
#define MAX_MONITORS 10
#define MAX_OBJECTS 32
#define CANVAS_WIDTH 200
#define CANVAS_HEIGHT 100

#define COVERAGE_THRESHOLD 0.85f
#define TIME_THRESHOLD 2

// 🔧 Variables globales
char canvas[CANVAS_HEIGHT][CANVAS_WIDTH];
int canvas_owner[CANVAS_HEIGHT][CANVAS_WIDTH]; // 0 = libre, >0 = ID del objeto
AnimatedObject* active_objects[MAX_OBJECTS]; // indexado por id - 1


my_mutex_t canvas_mutex;

int clients[MAX_MONITORS];
int num_monitors = 0;
int canvas_width = 0;
int canvas_height = 0;


// 🟢 Enviar canvas dividido a los monitores
void send_client_canvas(char canvas[][CANVAS_WIDTH]) {
    // 🔍 Mostrar configuración antes de dividir
    printf("📤 Enviando canvas de tamaño %dx%d dividido entre %d monitores\n",
           canvas_width, canvas_height, num_monitors);

    int section_width = canvas_width / num_monitors;

    for (int i = 0; i < num_monitors; i++) {
        NetPacket pkt;
        pkt.x = i * section_width;
        pkt.y = 0;
        pkt.width = section_width;
        pkt.height = canvas_height;

        // 🔍 Mostrar información de la sección enviada
        printf("➡️  Sección para cliente %d → x=%d, ancho=%d, alto=%d\n",
               i, pkt.x, pkt.width, pkt.height);

        // copyr contenido de canvas al paquete
        for (int row = 0; row < pkt.height; row++) {
            for (int col = 0; col < pkt.width; col++) {
                pkt.data[row][col] = canvas[row][col + pkt.x];
            }
        }

        send_packet(clients[i], &pkt);
    }
}


void rotate_shape_matrix(ShapeMatrix* shape, int angle) {
    ShapeMatrix temp = *shape;
    int r = temp.rows;
    int c = temp.cols;

    switch (angle) {
        case 90:
            shape->rows = c;
            shape->cols = r;
            for (int i = 0; i < c; i++)
                for (int j = 0; j < r; j++)
                    shape->data[i][j] = temp.data[r - 1 - j][i];
            break;

        case 180:
            for (int i = 0; i < r; i++)
                for (int j = 0; j < c; j++)
                    shape->data[i][j] = temp.data[r - 1 - i][c - 1 - j];
            break;

        case 270:
            shape->rows = c;
            shape->cols = r;
            for (int i = 0; i < c; i++)
                for (int j = 0; j < r; j++)
                    shape->data[i][j] = temp.data[j][c - 1 - i];
            break;

        default:  // 0 grados, sin cambios
            break;
    }
}

bool free_object_area(int x, int y, ShapeMatrix* shape, AnimatedObject* obj) {
    for (int i = 0; i < shape->rows; i++) {
        for (int j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] == ' ') continue;

            int cx = x + j;
            int cy = y + i;

            if (cx < 0 || cx >= canvas_width || cy < 0 || cy >= canvas_height)
                continue;

            int ocupante = canvas_owner[cy][cx];
            if (ocupante != 0 && ocupante != obj->id) {
                // NO permitir paso sobre otro objeto
                return false;
            }
        }
    }
    return true;
}



void object_area_assignment(int x, int y, ShapeMatrix* shape, int id) {
    for (int i = 0; i < shape->rows; i++) {
        for (int j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] == ' ') continue;
            int cx = x + j;
            int cy = y + i;

            if (cx >= 0 && cx < canvas_width && cy >= 0 && cy < canvas_height) {
                canvas_owner[cy][cx] = id;
            }
        }
    }
}


void object_area_clean(int x, int y, ShapeMatrix* shape, int id) {
    for (int i = 0; i < shape->rows; i++) {
        for (int j = 0; j < shape->cols; j++) {
            if (shape->data[i][j] == ' ') continue;

            int cx = x + j;
            int cy = y + i;

            if (cx >= 0 && cx < canvas_width && cy >= 0 && cy < canvas_height) {
                if (canvas_owner[cy][cx] == id)
                    canvas_owner[cy][cx] = 0;
            }
        }
    }
}


void animation_start_wait(AnimatedObject* obj) {
    long start_ms = obj->time_start * 1000;
    while (get_current_time_ms() < start_ms) {
        my_thread_yield();
        busy_wait_ms(50);
    }
}


void erase_prev_figure(int x, int y, ShapeMatrix* shape) {
    for (int i = 0; i < shape->rows; i++) {
        for (int j = 0; j < shape->cols; j++) {
            int cy = y + i;
            int cx = x + j;
            if (shape->data[i][j] != ' ' &&
                cx >= 0 && cx < canvas_width &&
                cy >= 0 && cy < canvas_height) {
                canvas[cy][cx] = '.';
            }
        }
    }
}

void draw_curr_figure(int x, int y, ShapeMatrix* shape) {
    for (int i = 0; i < shape->rows; i++) {
        for (int j = 0; j < shape->cols; j++) {
            int cy = y + i;
            int cx = x + j;
            if (shape->data[i][j] != ' ' &&
                cx >= 0 && cx < canvas_width &&
                cy >= 0 && cy < canvas_height) {
                canvas[cy][cx] = shape->data[i][j];
            }
        }
    }
}

void erase_final_figure(AnimatedObject* obj) {
    object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);
    erase_prev_figure(obj->current_x, obj->current_y, &obj->shape);
    printf("🧼 Limpiando figura de obj %d\n", obj->id);
}


void explotion_draw(int x, int y) {
    const char* text = "* * BOOM * *";
    int len = strlen(text);
    int cx = x - len / 2;
    int cy = y;

    for (int i = 0; i < len; i++) {
        if (cx + i >= 0 && cx + i < canvas_width && cy >= 0 && cy < canvas_height) {
            canvas[cy][cx + i] = text[i];
        }
    }
}

void erase_explotion(int x, int y) {
    const char* text = "* * BOOM * *";
    int len = strlen(text);
    int cx = x - len / 2;
    int cy = y;

    for (int i = 0; i < len; i++) {
        if (cx + i >= 0 && cx + i < canvas_width && cy >= 0 && cy < canvas_height) {
            canvas[cy][cx + i] = '.';  // reemplazar por fondo
        }
    }
}




void* object_animate(void* arg) {
    AnimatedObject* obj = (AnimatedObject*) arg;

    int dx_total = abs(obj->x_end - obj->x_start);
    int dy_total = abs(obj->y_end - obj->y_start);
    int steps = dx_total > dy_total ? dx_total : dy_total;
    if (steps == 0) steps = 1;

    float dx = (float)(obj->x_end - obj->x_start) / steps;
    float dy = (float)(obj->y_end - obj->y_start) / steps;

    float fx = obj->x_start;
    float fy = obj->y_start;
    int prev_x = (int)fx;
    int prev_y = (int)fy;

    int rot_start = obj->rotation_start % 360;
    int rot_end   = obj->rotation_end % 360;
    int total_rotation = (rot_end - rot_start + 360) % 360;
    int num_rotations = total_rotation / 90;
    int steps_per_rotation = (num_rotations > 0) ? steps / num_rotations : steps + 1;
    int applied_rotations = 0;
    int current_rotation = rot_start;

    if (current_rotation > 0)
        rotate_shape_matrix(&obj->shape, current_rotation);

    animation_start_wait(obj);

    int delay_step = 150;
    if (obj->scheduler == SCHED_REALTIME) {
        long now = get_current_time_ms();
        long remain_time_ms = (obj->time_end * 1000) - now;
        if (remain_time_ms <= 0) {
            printf("❌ OBJ %d: Tiempo insuficiente para iniciar animación\n", obj->id);
            goto skip_loop;
        }
        delay_step = remain_time_ms / steps;
        if (delay_step < 10) delay_step = 10;
    }

    int step_done = 0;
    bool exploded = false;

    for (int p = 0; p < steps; ) {
        long now = get_current_time_ms();
        if (now > obj->time_end * 1000) {
            printf("🛑 Hilo obj=%d superó time_end\n", obj->id);
            if (obj->scheduler == SCHED_REALTIME) {
                exploded = true;
                break;
            } else {
                printf("⚠️ OBJ %d continúa después de time_end (sched=%d)\n", obj->id, obj->scheduler);
            }
        }

        int cur_x = (int)(fx + dx);
        int cur_y = (int)(fy + dy);

        my_mutex_lock(&canvas_mutex);

        object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);

        if (!free_object_area(cur_x, cur_y, &obj->shape, obj)) {
            object_area_assignment(obj->current_x, obj->current_y, &obj->shape, obj->id);
            my_mutex_unlock(&canvas_mutex);
            busy_wait_ms(50);
            my_thread_yield();
            continue;
        }

        erase_prev_figure(prev_x, prev_y, &obj->shape);

        if (num_rotations > 0 && p > 0 && p % steps_per_rotation == 0 && applied_rotations < num_rotations) {
            ShapeMatrix copy = obj->shape;
            rotate_shape_matrix(&copy, 90);

            int cx_old = obj->shape.cols / 2;
            int cy_old = obj->shape.rows / 2;
            int cx_new = copy.cols / 2;
            int cy_new = copy.rows / 2;
            int delta_x = cx_old - cx_new;
            int delta_y = cy_old - cy_new;

            int new_x = obj->current_x + delta_x;
            int new_y = obj->current_y + delta_y;

            if (!free_object_area(new_x, new_y, &copy, obj)) {
                object_area_assignment(obj->current_x, obj->current_y, &obj->shape, obj->id);
                my_mutex_unlock(&canvas_mutex);
                busy_wait_ms(50);
                my_thread_yield();
                continue;
            }

            obj->shape = copy;
            obj->current_x = new_x;
            obj->current_y = new_y;
            applied_rotations++;
            current_rotation = (current_rotation + 90) % 360;
        }

        fx += dx;
        fy += dy;
        obj->current_x = cur_x;
        obj->current_y = cur_y;

        draw_curr_figure(cur_x, cur_y, &obj->shape);
        object_area_assignment(cur_x, cur_y, &obj->shape, obj->id);

        my_mutex_unlock(&canvas_mutex);

        send_client_canvas(canvas);
        busy_wait_ms(delay_step);
        my_thread_yield();

        prev_x = cur_x;
        prev_y = cur_y;
        step_done++;
        p++;
    }

skip_loop:
    my_mutex_lock(&canvas_mutex);
    if (exploded) {
        printf("💥 EXPLOSIÓN: obj=%d no completó su animación\n", obj->id);
    
        object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);
        erase_prev_figure(obj->current_x, obj->current_y, &obj->shape);
    
        explotion_draw(obj->current_x, obj->current_y);
        send_client_canvas(canvas);
        busy_wait_ms(1000); // mostrar la explosión 1s
    
        erase_explotion(obj->current_x, obj->current_y);
        send_client_canvas(canvas); // mostrar canvas limpio
    } else {
        printf("🧹 OBJ %d — Entrando a cleanup en (%d,%d)\n", obj->id, obj->current_x, obj->current_y);
        erase_final_figure(obj);
    }
    my_mutex_unlock(&canvas_mutex);

    send_client_canvas(canvas);

    float percentage = (float)step_done * 100 / steps;
    printf("✅ OBJ %d finalizó en posición (%d, %d), avance: %.1f%%\n",
           obj->id, obj->current_x, obj->current_y, percentage);

    free(obj);
    my_thread_end(NULL);
    return NULL;
}



static volatile sig_atomic_t running = 1;


void run_server(const char* cfg)
{
    memset(canvas_owner, 0, sizeof(canvas_owner));

    /* 1. Leer configuración global (canvas + monitores) */
    CanvasConfig config;
    if (read_config(cfg, &config) != 0) {
        fprintf(stderr, "❌ No se pudo leer %s\n", cfg);
        exit(EXIT_FAILURE);
    }

    /* 2. Leer objetos desde el mismo .ini */
    AnimatedObject objects[MAX_OBJECTS];
    int objetct_total = 0;
    if (object_load_from_ini(cfg, objects, &objetct_total) < 0) {
        fprintf(stderr, "❌ Error cargando objetos desde %s\n", cfg);
        exit(EXIT_FAILURE);
    }

    printf("📦 Objetos cargados (%d):\n", objetct_total);
    for (int i = 0; i < objetct_total; i++) {
        AnimatedObject* o = &objects[i];
        printf("🔸 Obj %d: (%d,%d) → (%d,%d) | Sched=%d | Tck=%d | ⏱️ %ld→%ld | D=%ld\n",
               i, o->x_start, o->y_start, o->x_end, o->y_end,
               o->scheduler, o->tickets, o->time_start, o->time_end, o->deadline);
    }

    canvas_width  = config.width;
    canvas_height = config.height;
    num_monitors  = config.num_monitors;

    /* 4. Canvas inicial (relleno con puntos) */
    for (int r = 0; r < canvas_height; r++)
        memset(canvas[r], '.', canvas_width);

    /* 5. Mutex global del canvas */
    my_mutex_init(&canvas_mutex);

    /* 6. Socket de escucha */
    int server_fd = create_server_socket(PORT);

    printf("🖼️ Canvas: %dx%d | Monitores esperados: %d\n",
           canvas_width, canvas_height, num_monitors);
    printf("🔌 Esperando %d monitores...\n", num_monitors);

    for (int i = 0; i < num_monitors; i++) {
        clients[i] = accept_client(server_fd);
        printf("✅ Monitor %d conectado.\n", i);
    }

    /* 3. Inicializar temporizador base */
    init_timer();

    /* 7. Inicializar schedulers y crear los hilos-objeto */
    scheduler_init();

    for (int i = 0; i < objetct_total; i++) {
        AnimatedObject* copy = malloc(sizeof(AnimatedObject));
        if (!copy) {
            fprintf(stderr, "❌ Memoria insuficiente para objeto %d\n", i);
            continue;
        }

        *copy = objects[i];
        copy->id = i + 1;
        active_objects[copy->id - 1] = copy;

        // 🚀 Determinar si debe usar LOTTERY según velocidad requerida
        int dx = abs(copy->x_end - copy->x_start);
        int dy = abs(copy->y_end - copy->y_start);
        float distance = dx > dy ? dx : dy; // pasos requeridos (como en animación)

        long duration = copy->time_end - copy->time_start;
        if (duration == 0) duration = 1; // prevenir división por cero

        float required_speed = distance / (float)duration; // pasos por segundo
        float speed_threshold = 20.0f; // si supera esto, usar LOTTERY

        if (required_speed > speed_threshold) {
            printf("🚀 OBJ %d necesita velocidad alta (%.1f pasos/s) → usando LOTTERY\n",
                   i, required_speed);
            copy->scheduler = SCHED_LOTTERY;
            copy->tickets = 50;
        }

        my_thread_t tid;
        int rc = my_thread_create(&tid,
                                  object_animate,
                                  copy,
                                  copy->scheduler,
                                  copy->tickets,
                                  copy->time_start,
                                  copy->time_end,
                                  copy->deadline);
        if (rc != 0) {
            fprintf(stderr, "❌ Error creando hilo para objeto %d\n", i);
            free(copy);
            continue;
        }

        printf("✅ Hilo creado para objeto %d (tid=%d)\n", i, tid);
    }

    /* 8. Arrancar el scheduler mixto */
    scheduler_run();

    /* 9. Bucle principal: mantiene vivo el proceso hasta Ctrl-C */
    while (running) {
        busy_wait_ms(100);
    }

    /* 10. Cierre ordenado: sockets de clientes y servidor */
    printf("\n🔻 Cerrando servidor...\n");
    for (int i = 0; i < num_monitors; i++)
        if (clients[i] > 0) close(clients[i]);
    close(server_fd);

    return;
}

