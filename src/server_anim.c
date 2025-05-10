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

// 🧵 Hilo para animar un objeto
void* animar_objeto(void* arg) {
    ObjetoAnimado* obj = (ObjetoAnimado*) arg;
    int pasos = abs(obj->x_end - obj->x_start);
    int dx = (obj->x_end > obj->x_start) ? 1 : -1;

    obj->current_x = obj->x_start;

    for (int p = 0; p <= pasos; p++) {
        my_mutex_lock(&canvas_mutex);
        printf("✅ canvas_mutex inicializado: %p\n", (void*)&canvas_mutex);

        // Borrar figura anterior
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

        // Avanzar una posición
        obj->current_x += dx;

        // Dibujar nueva posición
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

        // 🔁 Enviar canvas actualizado
        enviar_canvas_a_clientes(canvas);
        usleep(150000);  // 150 ms entre cada paso (ajustá como quieras)
        my_thread_yield();
        
    }

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

    // Iniciar scheduler ANTES de agregar hilos
    scheduler_init();

    // Crear hilos para cada objeto
    for (int i = 0; i < total_objetos; i++) {
        my_thread_t t;
        
        my_thread_create(&t, animar_objeto, &objetos[i]);

        my_thread_chsched(t,
            objetos[i].scheduler,
            objetos[i].tickets,
            objetos[i].time_start,
            objetos[i].time_end,
            objetos[i].deadline
        );
    }

    // Ejecutar scheduler
    scheduler_run();


    while (1) {
        usleep(100000); // espera 100ms
    }
    

    close(server_fd);
    return 0;
}
