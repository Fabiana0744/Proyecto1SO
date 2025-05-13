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

void rotate_matrix_once(char** src, int* rows, int* cols, int angle) {
    if (angle == 0) return;

    int new_rows = (angle == 90 || angle == 270) ? *cols : *rows;
    int new_cols = (angle == 90 || angle == 270) ? *rows : *cols;

    // Crear matriz temporal
    char** dst = malloc(sizeof(char*) * new_rows);
    for (int i = 0; i < new_rows; i++) {
        dst[i] = malloc(sizeof(char) * (new_cols + 1));
        memset(dst[i], ' ', new_cols);
        dst[i][new_cols] = '\0';
    }

    // Rotar matriz
    for (int i = 0; i < *rows; i++) {
        for (int j = 0; j < *cols; j++) {
            char c = src[i][j];
            if (angle == 90)
                dst[j][*rows - 1 - i] = c;
            else if (angle == 180)
                dst[*rows - 1 - i][*cols - 1 - j] = c;
            else if (angle == 270)
                dst[*cols - 1 - j][i] = c;
            else
                dst[i][j] = c;
        }
    }

    // Liberar figura original
    for (int i = 0; i < *rows; i++) free(src[i]);
    free(src);

    // Asignar nueva figura
    *rows = new_rows;
    *cols = new_cols;
    src = dst;
}


// 🧵 Hilo para animar un objeto
void* animar_objeto(void* arg) {
    ObjetoAnimado* obj = (ObjetoAnimado*) arg;
    int pasos = abs(obj->x_end - obj->x_start);
    int dx = (obj->x_end > obj->x_start) ? 1 : -1;
    obj->current_x = obj->x_start;
    obj->current_y = obj->y_start;

    // 🔄 Aplicar rotación fija si se indica
    if (obj->rotation != 0) {
        printf("🔄 Rotando objeto tid=%d en %d°\n", current->tid, obj->rotation);

        int new_rows = (obj->rotation == 90 || obj->rotation == 270) ? obj->shape_width : obj->shape_height;
        int new_cols = (obj->rotation == 90 || obj->rotation == 270) ? obj->shape_height : obj->shape_width;

        char** rotated = malloc(sizeof(char*) * new_rows);
        for (int i = 0; i < new_rows; i++) {
            rotated[i] = malloc(new_cols + 1);
            memset(rotated[i], ' ', new_cols);
            rotated[i][new_cols] = '\0';
        }

        for (int i = 0; i < obj->shape_height; i++) {
            for (int j = 0; j < obj->shape_width; j++) {
                char c = obj->shape[i][j];
                if (obj->rotation == 90)
                    rotated[j][obj->shape_height - 1 - i] = c;
                else if (obj->rotation == 180)
                    rotated[obj->shape_height - 1 - i][obj->shape_width - 1 - j] = c;
                else if (obj->rotation == 270)
                    rotated[obj->shape_width - 1 - j][i] = c;
                else
                    rotated[i][j] = c;
            }
        }

        // Liberar la forma original
        for (int i = 0; i < obj->shape_height; i++)
            free(obj->shape[i]);
        free(obj->shape);

        // Asignar la rotada
        obj->shape = rotated;
        obj->shape_height = new_rows;
        obj->shape_width = new_cols;
    }

    // ⏳ Esperar hasta time_start (para todos los schedulers)
    long start_ms = obj->time_start * 1000;
    while (get_current_time_ms() < start_ms) {
        my_thread_yield();
        usleep(50000);
    }

    printf("🚦 Inicia animación de tid=%d (time_start alcanzado)\n", current->tid);

    for (int p = 0; p <= pasos; p++) {
        long now = get_current_time_ms();

        // ⏹️ Salir de escena si pasó el tiempo de fin
        if (now > obj->time_end * 1000) {
            printf("🛑 Objeto tid=%d salió de escena (time_end alcanzado)\n", current->tid);

            if (obj->scheduler == SCHED_REALTIME) {
                printf("💥 EXPLOSIÓN: tid=%d superó su tiempo_end (%ld ms)\n", current->tid, obj->time_end * 1000);
                current->finished = true;
                current->must_cleanup = true;
            }

            break;  // salir del for
        }

        my_mutex_lock(&canvas_mutex);

        // 🧹 Borrar figura anterior
        for (int i = 0; i < obj->shape_height; i++) {
            for (int j = 0; j < obj->shape_width; j++) {
                int y = obj->current_y + i;
                int x = obj->current_x + j;
                if (obj->shape[i][j] != ' ' &&
                    x >= 0 && x < canvas_width &&
                    y >= 0 && y < canvas_height)
                    canvas[y][x] = '.';
            }
        }

        // ➡️ Avanzar
        obj->current_x += dx;

        // 🎨 Dibujar nueva figura
        for (int i = 0; i < obj->shape_height; i++) {
            for (int j = 0; j < obj->shape_width; j++) {
                int y = obj->current_y + i;
                int x = obj->current_x + j;
                if (obj->shape[i][j] != ' ' &&
                    x >= 0 && x < canvas_width &&
                    y >= 0 && y < canvas_height)
                    canvas[y][x] = obj->shape[i][j];
            }
        }

        my_mutex_unlock(&canvas_mutex);

        enviar_canvas_a_clientes(canvas);
        usleep(150000);  // velocidad de animación
        my_thread_yield();
    }

    printf("🏁 Hilo tid=%d terminó su animación\n", current->tid);

    // 🧹 Liberar memoria de figura
    for (int i = 0; i < obj->shape_height; i++) {
        free(obj->shape[i]);
    }
    free(obj->shape);
    free(obj);

    my_thread_end(NULL);
    return NULL;
}



int main() {
    
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
        printf("🔸 Obj %d: (%d,%d) → (%d,%d) | Sched=%d | Tck=%d | ⏱️ %ld→%ld | D=%ld\n",
            i, o->x_start, o->y_start, o->x_end, o->y_end,
            o->scheduler, o->tickets, o->time_start, o->time_end, o->deadline);
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

        copia->shape = malloc(sizeof(char*) * copia->shape_height);
        if (!copia->shape) {
            free(copia);
            continue;
        }

        for (int r = 0; r < copia->shape_height; r++) {
            copia->shape[r] = strdup(objetos[i].shape[r]);
            if (!copia->shape[r]) {
                for (int k = 0; k < r; k++) free(copia->shape[k]);
                free(copia->shape);
                free(copia);
                continue;
            }
        }

        printf("📦 Objeto %d copiado: y_start=%d x_start=%d shape[0]='%s'\n", 
            i, copia->y_start, copia->x_start, copia->shape[0]);

        my_thread_t t;
        int res = my_thread_create(&t, animar_objeto, copia);
        if (res != 0) {
            printf("❌ Error creando hilo para objeto %d\n", i);
            for (int r = 0; r < copia->shape_height; r++) free(copia->shape[r]);
            free(copia->shape);
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
        usleep(100000);
    }

    close(server_fd);
    return 0;
}
