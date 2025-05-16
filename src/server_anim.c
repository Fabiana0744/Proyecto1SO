#include "net.h"
#include "parser_config.h"
#include "parser_objetos.h"
#include "mypthreads.h"
#include "objeto.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PORT 5000
#define MAX_MONITORS 10
#define MAX_OBJETOS 32
#define CANVAS_WIDTH 200
#define CANVAS_HEIGHT 100

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
}

/* Pequeño helper: duerme (busy-wait) y cede CPU al final */
static void my_thread_sleep_ms(long ms)
{
    busy_wait_ms(ms);     /* quema el tiempo */
    my_thread_yield();    /* cede al planificador */
}

/* --------------------------------------------------------------------
 *  animar_objeto — versión con velocidad fija e independiente
 * ------------------------------------------------------------------*/

void* animar_objeto(void* arg)
{
    ObjetoAnimado* obj = (ObjetoAnimado*)arg;

    /* 1. Pasos totales (uno por celda en la dirección dominante) */
    int dx_total = abs(obj->x_end - obj->x_start);
    int dy_total = abs(obj->y_end - obj->y_start);
    int pasos    = (dx_total > dy_total) ? dx_total : dy_total;
    if (pasos == 0) pasos = 1;

    float dx = (float)(obj->x_end - obj->x_start) / pasos;
    float dy = (float)(obj->y_end - obj->y_start) / pasos;

    /* 2. Periodo fijo por tick (ms) */
    long ventana_ms = (long)(obj->time_end - obj->time_start) * 1000L;
    if (ventana_ms <= 0) ventana_ms = 1;
    long periodo_ms = ventana_ms / pasos;
    if (periodo_ms < 1) periodo_ms = 1;          /* mínimo 1 ms */

    /* 3. Reloj absoluto del próximo tick */
    long next_tick_ms = obj->time_start * 1000L; /* comienza exactamente en time_start */

    /* 4. Posición flotante y previa */
    float fx = obj->x_start;
    float fy = obj->y_start;
    int   prev_x = (int)fx;
    int   prev_y = (int)fy;

    /* 5. Rotación por bloques de 90° */
    int total_rot = ( (obj->rotation_end - obj->rotation_start) % 360 + 360 ) % 360;
    int n_rots    = total_rot / 90;
    int steps_per_rot = (n_rots > 0) ? pasos / n_rots : pasos + 1;
    int rots_done = 0;
    if (obj->rotation_start % 360)
        rotate_shape_matrix(&obj->shape, obj->rotation_start % 360);

    /* 6. Esperar hasta time_start */
    esperar_inicio_animacion(obj);

    /* 7. Bucle principal */
    for (int p = 0; p < pasos; ) {

        long now_ms = get_current_time_ms();

        /* 7.1  Explosión si fuera de la ventana */
        if (now_ms > obj->time_end * 1000L) {
            printf("💥 Obj %d EXPLOTA: fuera de tiempo\n", obj->id);
            if (obj->scheduler == SCHED_REALTIME) {
                current->finished     = true;
                current->must_cleanup = true;
            }
            break;
        }

        /* 7.2  Esperar al tick exacto */
        long wait_ms = next_tick_ms - now_ms;
        if (wait_ms > 0) {           /* aún no toca — espera y vuelve */
            busy_wait_ms(wait_ms);
            my_thread_yield();
            continue;
        }

        /* 7.3  Intentar avanzar un paso */
        int cur_x = (int)(fx + dx);
        int cur_y = (int)(fy + dy);

        my_mutex_lock(&canvas_mutex);
        limpiar_area_de_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);

        if (!area_libre_para_objeto(cur_x, cur_y, &obj->shape, obj)) {
            /* Cede paso: tick perdido */
            asignar_area_a_objeto(obj->current_x, obj->current_y, &obj->shape, obj->id);
            my_mutex_unlock(&canvas_mutex);
            busy_wait_ms(50);
            my_thread_yield();
            next_tick_ms += periodo_ms;   /* consume el tick */
            continue;
        }

        /* 7.4  Borrar figura previa y rotar si corresponde */
        borrar_figura_anterior(prev_x, prev_y, &obj->shape);

        if (n_rots && (p + 1) % steps_per_rot == 0 && rots_done < n_rots) {
            ShapeMatrix rot = obj->shape;
            rotate_shape_matrix(&rot, 90);

            /* Ajuste para mantener centro aproximado */
            int dx_c = (obj->shape.cols - rot.cols) / 2;
            int dy_c = (obj->shape.rows - rot.rows) / 2;
            int rx = cur_x + dx_c;
            int ry = cur_y + dy_c;

            if (area_libre_para_objeto(rx, ry, &rot, obj)) {
                obj->shape = rot;
                cur_x = rx;
                cur_y = ry;
                rots_done++;
            }
        }

        /* 7.5  Avanzar posición */
        fx += dx;
        fy += dy;
        obj->current_x = cur_x;
        obj->current_y = cur_y;

        /* 7.6  Dibujar y registrar ocupación */
        dibujar_figura_actual(cur_x, cur_y, &obj->shape);
        asignar_area_a_objeto(cur_x, cur_y, &obj->shape, obj->id);

        my_mutex_unlock(&canvas_mutex);
        enviar_canvas_a_clientes(canvas);

        /* 7.7  Preparar siguiente estado */
        prev_x = cur_x;
        prev_y = cur_y;
        p++;
        next_tick_ms += periodo_ms;          /* avanza el metrónomo */
        my_thread_yield();
    }

    /* 8. Fin de animación o explosión */
    printf("🔚 Obj %d (tid=%d) terminó en (%d,%d)\n",
           obj->id, current->tid, obj->current_x, obj->current_y);

    my_mutex_lock(&canvas_mutex);
    borrar_figura_final(obj);
    my_mutex_unlock(&canvas_mutex);
    enviar_canvas_a_clientes(canvas);

    free(obj);
    my_thread_end(NULL);
    return NULL;
}







int main() {
    
    memset(canvas_owner, 0, sizeof(canvas_owner));

    // Leer configuración
    CanvasConfig config;
    if (read_config("config/animation.ini", &config) != 0) {
        printf("❌ No se pudo leer config/animation.ini\n");
        return 1;
    }

    ObjetoAnimado objetos[MAX_OBJETOS];
    int total_objetos = 0;
    if (cargar_objetos_desde_ini("config/animation.ini", objetos, &total_objetos) < 0) {
        printf("❌ Error cargando objetos desde el .ini\n");
        return 1;
    }

    printf("📦 Objetos cargados (%d):\n", total_objetos);
    for (int i = 0; i < total_objetos; i++) {
        ObjetoAnimado* o = &objetos[i];
        printf("🔸 Obj %d: (%d,%d) → (%d,%d) | Sched=%d | Tck=%d | ⏱️ %ld→%ld | Vent=%lds | D=%ld\n",
            i, o->x_start, o->y_start, o->x_end, o->y_end,
               o->scheduler, o->tickets,
               o->time_start, o->time_end,
+              (o->time_end - o->time_start),
                                    
               o->deadline);
            
    }
    init_timer();  // ⏱️ iniciar contador relativo desde 0

    canvas_width = config.width;
    canvas_height = config.height;
    num_monitors = config.num_monitors;

    // Inicializar canvas
    for (int i = 0; i < canvas_height; i++)
        memset(canvas[i], '.', canvas_width);

    // Inicializar mutex
    my_mutex_init(&canvas_mutex);
    printf("✅ canvas_mutex inicializado en: %p\n", (void*)&canvas_mutex);

    // Iniciar sockets
    int server_fd = create_server_socket(PORT);

    printf("🖼️ Canvas: %dx%d | Monitores: %d | Objetos: %d\n", canvas_width, canvas_height, num_monitors, total_objetos);
    printf("🔌 Esperando %d monitores...\n", num_monitors);
    for (int i = 0; i < num_monitors; i++) {
        clients[i] = accept_client(server_fd);
        printf("✅ Monitor %d conectado.\n", i);
    }

    // Inicializar todos los schedulers
    scheduler_init();

    // Crear hilos para cada objeto
    for (int i = 0; i < total_objetos; i++) {
        printf("🔄 Creando hilo para objeto %d...\n", i);

        ObjetoAnimado* copia = malloc(sizeof(ObjetoAnimado));
        if (!copia) {
            printf("❌ Error al reservar memoria para objeto %d\n", i);
            continue;
        }
        memset(copia, 0, sizeof(ObjetoAnimado));
        *copia = objetos[i];
        copia->id = i + 1;
        objetos_activos[copia->id - 1] = copia;


        copia->shape = objetos[i].shape;  

        printf("📦 Objeto %d copiado: y_start=%d x_start=%d primera fila='%.*s'\n",
       i, copia->y_start, copia->x_start,
       copia->shape.cols, copia->shape.data[0]);

        my_thread_t t;
        int res = my_thread_create(&t, animar_objeto, copia);
        if (res != 0) {
            printf("❌ Error creando hilo para objeto %d\n", i);
            free(copia);
            continue;
        }
       

        printf("✅ Hilo creado para objeto %d (tid = %d)\n", i, t);

        int sched = my_thread_chsched(t,
            copia->scheduler,
            copia->tickets,
            copia->time_start,
            copia->time_end,
            copia->deadline
        );

        if (sched != 0) {
            printf("⚠️  Error asignando scheduler a objeto %d\n", i);
        }
    }

    // Ejecutar scheduler mixto
    scheduler_run();

    while (1) {
        busy_wait_ms(100); 
    }

    close(server_fd);
    return 0;
}
