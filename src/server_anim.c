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

// --- 🔧 Variables globales ---

// Representa el canvas principal donde se dibujan los objetos.
// Entrada: modificada por los hilos de animación.
// Salida: enviada a los monitores.
char canvas[CANVAS_HEIGHT][CANVAS_WIDTH];

// Controla qué objeto es dueño de cada celda del canvas.
// Entrada: se actualiza durante las animaciones.
// Salida: usada para detectar colisiones o interferencias.
int canvas_owner[CANVAS_HEIGHT][CANVAS_WIDTH]; // 0 = libre, >0 = ID del objeto

// Referencias a los objetos activos animados, indexados por ID.
// Entrada: llenado al cargar los objetos desde archivo.
// Salida: usado por los hilos de animación.
AnimatedObject* active_objects[MAX_OBJECTS];

// Mutex para proteger el acceso concurrente al canvas.
// Entrada: usado por los hilos al modificar el canvas.
// Salida: asegura sincronización.
my_mutex_t canvas_mutex;

int clients[MAX_MONITORS];  // Almacena los sockets de cada cliente/monitor conectado.
int num_monitors = 0;       // Cantidad actual de monitores conectados.
int canvas_width = 0;       // Ancho total del canvas.
int canvas_height = 0;      // Alto total del canvas.

// --- 🟢 Enviar canvas dividido a los monitores ---

// Envía una porción del canvas a cada monitor conectado.
// Entrada: matriz canvas completa con contenido actual.
// Salida: cada cliente recibe su sección correspondiente.
void send_client_canvas(char canvas[][CANVAS_WIDTH]) {
    printf("📤 Enviando canvas de tamaño %dx%d dividido entre %d monitores\n",
           canvas_width, canvas_height, num_monitors);

    int section_width = canvas_width / num_monitors;

    for (int i = 0; i < num_monitors; i++) {
        NetPacket pkt;
        pkt.x = i * section_width;
        pkt.y = 0;
        pkt.width = section_width;
        pkt.height = canvas_height;

        printf("➡️  Sección para cliente %d → x=%d, ancho=%d, alto=%d\n",
               i, pkt.x, pkt.width, pkt.height);

        // Copia la sección del canvas global al paquete para enviar
        for (int row = 0; row < pkt.height; row++) {
            for (int col = 0; col < pkt.width; col++) {
                pkt.data[row][col] = canvas[row][col + pkt.x];
            }
        }

        send_packet(clients[i], &pkt);
    }
}

// Rota una figura en su matriz según el ángulo especificado (90, 180 o 270 grados).
// Entrada: puntero a ShapeMatrix y ángulo deseado.
// Salida: la matriz es modificada en lugar.
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

        default:  // Si el ángulo es 0, no se modifica la forma.
            break;
    }
}

// Verifica si una figura puede colocarse en una posición sin invadir otras.
// Entrada: posición (x, y), puntero a la figura, puntero al objeto correspondiente.
// Salida: true si el área está libre o solo contiene el mismo objeto, false si hay colisión.
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
                return false;
            }
        }
    }
    return true;
}

// Asigna las celdas ocupadas por una figura a su ID en canvas_owner.
// Entrada: posición (x, y), puntero a figura, ID del objeto.
// Salida: actualiza la matriz canvas_owner.
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

// Libera las celdas ocupadas por un objeto en la matriz canvas_owner.
// Entrada: posición (x, y), figura (forma), y su ID.
// Salida: las celdas correspondientes se marcan como libres (0).
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

// Espera hasta que llegue el tiempo de inicio del objeto.
// Entrada: puntero al objeto animado.
// Salida: la función bloquea hasta que get_current_time_ms >= time_start.
void animation_start_wait(AnimatedObject* obj) {
    long start_ms = obj->time_start * 1000;
    while (get_current_time_ms() < start_ms) {
        my_thread_yield();
        busy_wait_ms(50);
    }
}

// Borra la figura anterior del canvas visual (solo caracteres).
// Entrada: posición (x, y) y figura a borrar.
// Salida: las celdas ocupadas por la figura se reemplazan con '.'.
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

// Dibuja una figura en el canvas visual con sus caracteres.
// Entrada: posición (x, y) y figura a dibujar.
// Salida: las celdas del canvas se llenan con los caracteres de la figura.
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

// Borra completamente la figura del objeto del canvas (propiedad + visual).
// Entrada: puntero al objeto animado.
// Salida: se elimina visualmente y se liberan sus celdas de propiedad.
void erase_final_figure(AnimatedObject* obj) {
    object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);
    erase_prev_figure(obj->current_x, obj->current_y, &obj->shape);
    printf("🧼 Limpiando figura de obj %d\n", obj->id);
}

// Dibuja una animación de explosión en una posición central.
// Entrada: coordenadas (x, y) para centrar el texto "* * BOOM * *".
// Salida: la explosión se muestra en el canvas.
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

// Borra el texto de explosión del canvas visual.
// Entrada: coordenadas (x, y) donde se dibujó previamente la explosión.
// Salida: las celdas se reemplazan por '.'.
void erase_explotion(int x, int y) {
    const char* text = "* * BOOM * *";
    int len = strlen(text);
    int cx = x - len / 2;
    int cy = y;

    for (int i = 0; i < len; i++) {
        if (cx + i >= 0 && cx + i < canvas_width && cy >= 0 && cy < canvas_height) {
            canvas[cy][cx + i] = '.';
        }
    }
}

// Ejecuta la animación de un objeto: movimiento, rotación y control de tiempo.
// Entrada: puntero a la estructura AnimatedObject (pasada como void*).
// Salida: ejecuta la animación paso a paso y termina el hilo.
void* object_animate(void* arg)
{
    AnimatedObject* obj = (AnimatedObject*)arg;

    /* ----------  PRE-CÁLCULOS DE MOVIMIENTO  ---------- */
    int dx_total = abs(obj->x_end - obj->x_start);
    int dy_total = abs(obj->y_end - obj->y_start);
    int steps    = (dx_total > dy_total ? dx_total : dy_total);
    if (steps == 0) steps = 1;

    float dx = (float)(obj->x_end - obj->x_start) / steps;
    float dy = (float)(obj->y_end - obj->y_start) / steps;

    float fx = obj->x_start;
    float fy = obj->y_start;
    obj->current_x = (int)fx;
    obj->current_y = (int)fy;

    /* ----------  PRE-CÁLCULOS DE ROTACIÓN  ---------- */
    int rot_start        = obj->rotation_start % 360;
    int rot_end          = obj->rotation_end   % 360;
    int total_rotation   = (rot_end - rot_start + 360) % 360;   /* 0-270 */
    int num_rotations    = total_rotation / 90;                 /* ¼-vueltas */
    int steps_per_rot    = (num_rotations ? steps / num_rotations : steps+1);
    int applied_rotations = 0;

    if (rot_start)                  /* si arranca girada */
        rotate_shape_matrix(&obj->shape, rot_start);

    /* ----------  ESPERA HASTA time_start ---------- */
    animation_start_wait(obj);

    /* ----------  DELAY ENTRE FRAMES  ---------- */
    int delay_step = 150;           /* por defecto 150 ms */

    if (obj->scheduler == SCHED_REALTIME) {
        long now          = get_current_time_ms();
        long remain_ms    = (obj->time_end*1000) - now;
        if (remain_ms <= 0) {
            printf("❌ OBJ %d sin tiempo para iniciar\n", obj->id);
            my_thread_end(NULL);
            return NULL;
        }
        delay_step = remain_ms / steps;
        if (delay_step < 10) delay_step = 10;
    }

    /* ============================================================ */
    bool  exploded = false;
    int   p        = 0;             /* paso actual */

    while (p < steps) {

        long now = get_current_time_ms();
        if (now > obj->time_end*1000) {
            printf("🛑 OBJ %d superó time_end\n", obj->id);
            if (obj->scheduler == SCHED_REALTIME) { exploded = true; break; }
        }

        /* ----------  CALCULAR PRÓXIMO ESTADO (pos + forma)  ---------- */
        int next_x = (int)(fx + dx);
        int next_y = (int)(fy + dy);

        bool will_rotate = (num_rotations &&
                            p && (p % steps_per_rot == 0) &&
                            applied_rotations < num_rotations);

        ShapeMatrix cand_shape = obj->shape;      /* copia temporal */
        int dest_x = next_x;
        int dest_y = next_y;

        if (will_rotate) {
            rotate_shape_matrix(&cand_shape, 90); /* gira la copia */

            /* compensar desplazamiento del centro                     */
            int cx_old = obj->shape.cols/2, cy_old = obj->shape.rows/2;
            int cx_new = cand_shape.cols/2, cy_new = cand_shape.rows/2;
            dest_x += cx_old - cx_new;
            dest_y += cy_old - cy_new;
        }

        /* --------------------  SECCIÓN CRÍTICA  -------------------- */
        my_mutex_lock(&canvas_mutex);

        /* 1️⃣  ¿ÁREA DESTINO DISPONIBLE? (sin liberar la vieja aún)   */
        if (!free_object_area(dest_x, dest_y, &cand_shape, obj)) {
            my_mutex_unlock(&canvas_mutex);
            busy_wait_ms(50);
            my_thread_yield();
            continue;                       /* reintentar paso    */
        }

        /* 2️⃣  Liberar área antigua y dibujar la nueva               */
        object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);
        erase_prev_figure(obj->current_x, obj->current_y, &obj->shape);

        obj->shape      = cand_shape;       /* commit forma nueva */
        obj->current_x  = dest_x;
        obj->current_y  = dest_y;

        if (will_rotate) applied_rotations++;

        draw_curr_figure(dest_x, dest_y, &obj->shape);
        object_area_assignment(dest_x, dest_y, &obj->shape, obj->id);

        my_mutex_unlock(&canvas_mutex);
        /* ---------------------------------------------------------- */

        send_client_canvas(canvas);

        /* avanzar acumuladores flotantes para el próximo ciclo       */
        fx += dx;
        fy += dy;
        ++p;

        busy_wait_ms(delay_step);
        my_thread_yield();
    }

    /* ---------------  POST-LOOP: LIMPIEZA / EXPLOSIÓN -------------- */
    my_mutex_lock(&canvas_mutex);

    if (exploded) {
        printf("💥 EXPLOSIÓN: obj=%d no completó su animación\n", obj->id);

        object_area_clean(obj->current_x, obj->current_y, &obj->shape, obj->id);
        erase_prev_figure(obj->current_x, obj->current_y, &obj->shape);

        explotion_draw(obj->current_x, obj->current_y);
        send_client_canvas(canvas);
        busy_wait_ms(1000);
        erase_explotion(obj->current_x, obj->current_y);
    } else {
        printf("🧹 OBJ %d — Entrando a cleanup en (%d,%d)\n", obj->id, obj->current_x, obj->current_y);
        erase_final_figure(obj);
    }

    my_mutex_unlock(&canvas_mutex);
    send_client_canvas(canvas);

    float percentage = (float)p * 100 / steps;
    printf("✅ OBJ %d finalizó en posición (%d, %d), avance: %.1f%%\n",
           obj->id, obj->current_x, obj->current_y, percentage);

    free(obj);
    my_thread_end(NULL);
    return NULL;
}

// Bandera de ejecución controlada por señal (Ctrl-C).
// Entrada: modificada externamente por el handler de señal.
// Salida: se consulta en el bucle principal para seguir corriendo o detener.
static volatile sig_atomic_t running = 1;


// Punto de entrada principal del servidor de animación.
// Entrada: nombre del archivo de configuración `.ini`.
// Salida: lanza el servidor, carga objetos, inicia animaciones y atiende monitores.
void run_server(const char* cfg)
{
    // Limpia el mapa de ocupación de canvas.
    memset(canvas_owner, 0, sizeof(canvas_owner));

    /* 1. Leer configuración general (dimensiones del canvas y monitores) */
    CanvasConfig config;
    if (read_config(cfg, &config) != 0) {
        fprintf(stderr, "❌ No se pudo leer %s\n", cfg);
        exit(EXIT_FAILURE);
    }

    /* 2. Cargar objetos desde el archivo .ini */
    AnimatedObject objects[MAX_OBJECTS];
    int objetct_total = 0;
    if (object_load_from_ini(cfg, objects, &objetct_total) < 0) {
        fprintf(stderr, "❌ Error cargando objetos desde %s\n", cfg);
        exit(EXIT_FAILURE);
    }

    // Mostrar resumen de los objetos cargados
    printf("📦 Objetos cargados (%d):\n", objetct_total);
    for (int i = 0; i < objetct_total; i++) {
        AnimatedObject* o = &objects[i];
        printf("🔸 Obj %d: (%d,%d) → (%d,%d) | Sched=%d | Tck=%d | ⏱️ %ld→%ld | D=%ld\n",
               i, o->x_start, o->y_start, o->x_end, o->y_end,
               o->scheduler, o->tickets, o->time_start, o->time_end, o->deadline);
    }

    // Asignar configuración global
    canvas_width  = config.width;
    canvas_height = config.height;
    num_monitors  = config.num_monitors;

    /* 4. Inicializar canvas: rellenar con puntos */
    for (int r = 0; r < canvas_height; r++)
        memset(canvas[r], '.', canvas_width);

    /* 5. Inicializar mutex global del canvas */
    my_mutex_init(&canvas_mutex);

    /* 6. Crear socket servidor y aceptar conexiones */
    int server_fd = create_server_socket(PORT);

    printf("🖼️ Canvas: %dx%d | Monitores esperados: %d\n",
           canvas_width, canvas_height, num_monitors);
    printf("🔌 Esperando %d monitores...\n", num_monitors);

    for (int i = 0; i < num_monitors; i++) {
        clients[i] = accept_client(server_fd);
        printf("✅ Monitor %d conectado.\n", i);
    }

    /* 3. Inicializar temporizador global */
    init_timer();

    /* 7. Iniciar schedulers y lanzar hilos de animación */
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

        // Determina si usar LOTTERY basado en velocidad requerida
        int dx = abs(copy->x_end - copy->x_start);
        int dy = abs(copy->y_end - copy->y_start);
        float distance = dx > dy ? dx : dy;

        long duration = copy->time_end - copy->time_start;
        if (duration == 0) duration = 1;

        float required_speed = distance / (float)duration;
        float speed_threshold = 20.0f;

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

    /* 8. Ejecutar el scheduler mixto (RT + Lottery + RR) */
    scheduler_run();

    /* 9. Mantener el servidor vivo hasta que se reciba Ctrl-C */
    while (running) {
        busy_wait_ms(100);
    }

    /* 10. Cierre ordenado del servidor */
    printf("\n🔻 Cerrando servidor...\n");
    for (int i = 0; i < num_monitors; i++)
        if (clients[i] > 0) close(clients[i]);
    close(server_fd);

    return;
}


