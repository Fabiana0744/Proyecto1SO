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
#define MAX_OBJETOS 32
#define CANVAS_WIDTH 200
#define CANVAS_HEIGHT 100

#define COBERTURA_UMBRAL 0.85f
#define TIEMPO_UMBRAL_SEG 2

// 🔧 Variables globales
char canvas[CANVAS_HEIGHT][CANVAS_WIDTH];
int canvas_owner[CANVAS_HEIGHT][CANVAS_WIDTH]; // 0 = libre, >0 = ID del objeto
ObjetoAnimado* objetos_activos[MAX_OBJETOS]; // indexado por id - 1


my_mutex_t canvas_mutex;

int clients[MAX_MONITORS];
int num_monitors = 0;
int canvas_width = 0;
int canvas_height = 0;


// 🟢 Enviar canvas dividido a los monitores
void enviar_canvas_a_clientes(char canvas[][CANVAS_WIDTH]) {
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

        // Copiar contenido de canvas al paquete
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


float progreso_objeto(ObjetoAnimado* obj, int x, int y) {
    float dx_total = abs(obj->x_end - obj->x_start);
    float dy_total = abs(obj->y_end - obj->y_start);
    float dist_total = dx_total + dy_total;

    float dx_avance = abs(obj->x_end - x);
    float dy_avance = abs(obj->y_end - y);
    float dist_restante = dx_avance + dy_avance;

    return dist_total > 0 ? 1.0f - (dist_restante / dist_total) : 1.0f;
}


bool area_libre_para_objeto(int x, int y, ShapeMatrix* shape, ObjetoAnimado* obj) {
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



void asignar_area_a_objeto(int x, int y, ShapeMatrix* shape, int id) {
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


void limpiar_area_de_objeto(int x, int y, ShapeMatrix* shape, int id) {
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


void esperar_inicio_animacion(ObjetoAnimado* obj) {
    long inicio_ms = obj->time_start * 1000;
    while (get_current_time_ms() < inicio_ms) {
        my_thread_yield();
        busy_wait_ms(50);
    }
}


void borrar_figura_anterior(int x, int y, ShapeMatrix* shape) {
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

void dibujar_figura_actual(int x, int y, ShapeMatrix* shape) {
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

void borrar_figura_final(ObjetoAnimado* obj) {
    limpiar_area_de_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);
    borrar_figura_anterior(obj->current_x, obj->current_y, &obj->shape);
    printf("🧼 Limpiando figura de obj %d\n", obj->id);
}


void dibujar_explosion(int x, int y) {
    const char* texto = "* * BOOM * *";
    int len = strlen(texto);
    int cx = x - len / 2;
    int cy = y;

    for (int i = 0; i < len; i++) {
        if (cx + i >= 0 && cx + i < canvas_width && cy >= 0 && cy < canvas_height) {
            canvas[cy][cx + i] = texto[i];
        }
    }
}

void borrar_explosion(int x, int y) {
    const char* texto = "* * BOOM * *";
    int len = strlen(texto);
    int cx = x - len / 2;
    int cy = y;

    for (int i = 0; i < len; i++) {
        if (cx + i >= 0 && cx + i < canvas_width && cy >= 0 && cy < canvas_height) {
            canvas[cy][cx + i] = '.';  // reemplazar por fondo
        }
    }
}




void* animar_objeto(void* arg) {
    ObjetoAnimado* obj = (ObjetoAnimado*) arg;

    int dx_total = abs(obj->x_end - obj->x_start);
    int dy_total = abs(obj->y_end - obj->y_start);
    int pasos = dx_total > dy_total ? dx_total : dy_total;
    if (pasos == 0) pasos = 1;

    float dx = (float)(obj->x_end - obj->x_start) / pasos;
    float dy = (float)(obj->y_end - obj->y_start) / pasos;

    float fx = obj->x_start;
    float fy = obj->y_start;
    int prev_x = (int)fx;
    int prev_y = (int)fy;

    int rot_start = obj->rotation_start % 360;
    int rot_end   = obj->rotation_end % 360;
    int total_rotation = (rot_end - rot_start + 360) % 360;
    int num_rotaciones = total_rotation / 90;
    int steps_per_rotation = (num_rotaciones > 0) ? pasos / num_rotaciones : pasos + 1;
    int rotaciones_aplicadas = 0;
    int current_rotation = rot_start;

    if (current_rotation > 0)
        rotate_shape_matrix(&obj->shape, current_rotation);

    esperar_inicio_animacion(obj);

    int delay_por_paso = 150;
    if (obj->scheduler == SCHED_REALTIME) {
        long now = get_current_time_ms();
        long tiempo_restante_ms = (obj->time_end * 1000) - now;
        if (tiempo_restante_ms <= 0) {
            printf("❌ OBJ %d: Tiempo insuficiente para iniciar animación\n", obj->id);
            goto skip_loop;
        }
        delay_por_paso = tiempo_restante_ms / pasos;
        if (delay_por_paso < 10) delay_por_paso = 10;
    }

    int pasos_realizados = 0;
    bool exploto = false;

    for (int p = 0; p < pasos; ) {
        long now = get_current_time_ms();
        if (now > obj->time_end * 1000) {
            printf("🛑 Hilo obj=%d superó time_end\n", obj->id);
            if (obj->scheduler == SCHED_REALTIME) {
                exploto = true;
                break;
            } else {
                printf("⚠️ OBJ %d continúa después de time_end (sched=%d)\n", obj->id, obj->scheduler);
            }
        }

        int cur_x = (int)(fx + dx);
        int cur_y = (int)(fy + dy);

        my_mutex_lock(&canvas_mutex);

        limpiar_area_de_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);

        if (!area_libre_para_objeto(cur_x, cur_y, &obj->shape, obj)) {
            asignar_area_a_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);
            my_mutex_unlock(&canvas_mutex);
            busy_wait_ms(50);
            my_thread_yield();
            continue;
        }

        borrar_figura_anterior(prev_x, prev_y, &obj->shape);

        if (num_rotaciones > 0 && p > 0 && p % steps_per_rotation == 0 && rotaciones_aplicadas < num_rotaciones) {
            ShapeMatrix copia = obj->shape;
            rotate_shape_matrix(&copia, 90);

            int cx_old = obj->shape.cols / 2;
            int cy_old = obj->shape.rows / 2;
            int cx_new = copia.cols / 2;
            int cy_new = copia.rows / 2;
            int delta_x = cx_old - cx_new;
            int delta_y = cy_old - cy_new;

            int new_x = obj->current_x + delta_x;
            int new_y = obj->current_y + delta_y;

            if (!area_libre_para_objeto(new_x, new_y, &copia, obj)) {
                asignar_area_a_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);
                my_mutex_unlock(&canvas_mutex);
                busy_wait_ms(50);
                my_thread_yield();
                continue;
            }

            obj->shape = copia;
            obj->current_x = new_x;
            obj->current_y = new_y;
            rotaciones_aplicadas++;
            current_rotation = (current_rotation + 90) % 360;
        }

        fx += dx;
        fy += dy;
        obj->current_x = cur_x;
        obj->current_y = cur_y;

        dibujar_figura_actual(cur_x, cur_y, &obj->shape);
        asignar_area_a_objeto(cur_x, cur_y, &obj->shape, obj->id);

        my_mutex_unlock(&canvas_mutex);

        enviar_canvas_a_clientes(canvas);
        busy_wait_ms(delay_por_paso);
        my_thread_yield();

        prev_x = cur_x;
        prev_y = cur_y;
        pasos_realizados++;
        p++;
    }

skip_loop:
    my_mutex_lock(&canvas_mutex);
    if (exploto) {
        printf("💥 EXPLOSIÓN: obj=%d no completó su animación\n", obj->id);
    
        limpiar_area_de_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);
        borrar_figura_anterior(obj->current_x, obj->current_y, &obj->shape);
    
        dibujar_explosion(obj->current_x, obj->current_y);
        enviar_canvas_a_clientes(canvas);
        busy_wait_ms(1000); // mostrar la explosión 1s
    
        borrar_explosion(obj->current_x, obj->current_y);
        enviar_canvas_a_clientes(canvas); // mostrar canvas limpio
    } else {
        printf("🧹 OBJ %d — Entrando a cleanup en (%d,%d)\n", obj->id, obj->current_x, obj->current_y);
        borrar_figura_final(obj);
    }
    my_mutex_unlock(&canvas_mutex);

    enviar_canvas_a_clientes(canvas);

    float porcentaje = (float)pasos_realizados * 100 / pasos;
    printf("✅ OBJ %d finalizó en posición (%d, %d), avance: %.1f%%\n",
           obj->id, obj->current_x, obj->current_y, porcentaje);

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
    ObjetoAnimado objetos[MAX_OBJETOS];
    int total_objetos = 0;
    if (cargar_objetos_desde_ini(cfg, objetos, &total_objetos) < 0) {
        fprintf(stderr, "❌ Error cargando objetos desde %s\n", cfg);
        exit(EXIT_FAILURE);
    }

    printf("📦 Objetos cargados (%d):\n", total_objetos);
    for (int i = 0; i < total_objetos; i++) {
        ObjetoAnimado* o = &objetos[i];
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

    for (int i = 0; i < total_objetos; i++) {
        ObjetoAnimado* copia = malloc(sizeof(ObjetoAnimado));
        if (!copia) {
            fprintf(stderr, "❌ Memoria insuficiente para objeto %d\n", i);
            continue;
        }

        *copia = objetos[i];
        copia->id = i + 1;
        objetos_activos[copia->id - 1] = copia;

        // 🚀 Determinar si debe usar LOTTERY según velocidad requerida
        int dx = abs(copia->x_end - copia->x_start);
        int dy = abs(copia->y_end - copia->y_start);
        float distancia = dx > dy ? dx : dy; // pasos requeridos (como en animación)

        long duracion = copia->time_end - copia->time_start;
        if (duracion == 0) duracion = 1; // prevenir división por cero

        float velocidad_requerida = distancia / (float)duracion; // pasos por segundo
        float umbral_velocidad = 20.0f; // si supera esto, usar LOTTERY

        if (velocidad_requerida > umbral_velocidad) {
            printf("🚀 OBJ %d necesita velocidad alta (%.1f pasos/s) → usando LOTTERY\n",
                   i, velocidad_requerida);
            copia->scheduler = SCHED_LOTTERY;
            copia->tickets = 50;
        }

        my_thread_t tid;
        int rc = my_thread_create(&tid,
                                  animar_objeto,
                                  copia,
                                  copia->scheduler,
                                  copia->tickets,
                                  copia->time_start,
                                  copia->time_end,
                                  copia->deadline);
        if (rc != 0) {
            fprintf(stderr, "❌ Error creando hilo para objeto %d\n", i);
            free(copia);
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

