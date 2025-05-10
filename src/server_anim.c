#include "net.h"
#include "parser_config.h"
#include "parser_objetos.h"
#include "objeto.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PORT 5000
#define MAX_MONITORS 10
#define MAX_OBJETOS 32

int main() {
    // Leer configuración del canvas
    CanvasConfig config;
    if (read_config("config/animation.ini", &config) != 0) {
        printf("❌ No se pudo leer config/animation.ini\n");
        return 1;
    }

    // Leer objetos animados
    ObjetoAnimado objetos[MAX_OBJETOS];
    int total_objetos = 0;
    if (cargar_objetos_desde_ini("config/animation.ini", objetos, &total_objetos) < 0) {
        printf("❌ No se pudieron cargar objetos desde el .ini\n");
        return 1;
    }

    printf("🖼️ Canvas: %dx%d | Monitores: %d | Objetos: %d\n",
           config.width, config.height, config.num_monitors, total_objetos);

    // Crear canvas vacío
    char canvas[config.height][config.width];
    for (int i = 0; i < config.height; i++)
        memset(canvas[i], '.', config.width);

    // Dibujar cada objeto en su posición inicial
    for (int k = 0; k < total_objetos; k++) {
        ObjetoAnimado* obj = &objetos[k];
        obj->current_x = obj->x_start;
        obj->current_y = obj->y_start;

        for (int i = 0; i < obj->shape_height; i++) {
            for (int j = 0; j < obj->shape_width; j++) {
                int y = obj->current_y + i;
                int x = obj->current_x + j;

                if (y >= 0 && y < config.height && x >= 0 && x < config.width) {
                    if (obj->shape[i][j] != ' ') {
                        canvas[y][x] = obj->shape[i][j];
                    }
                }
            }
        }
    }

    // Inicializar servidor y esperar monitores
    int server_fd = create_server_socket(PORT);
    int clients[MAX_MONITORS];

    printf("🔌 Esperando %d monitores...\n", config.num_monitors);
    for (int i = 0; i < config.num_monitors; i++) {
        clients[i] = accept_client(server_fd);
        printf("✅ Monitor %d conectado.\n", i);
    }

    // Dividir el canvas horizontalmente por monitor
    int section_width = config.width / config.num_monitors;

    for (int i = 0; i < config.num_monitors; i++) {
        NetPacket pkt;
        pkt.x = i * section_width;
        pkt.y = 0;
        pkt.width = section_width;
        pkt.height = config.height;

        for (int row = 0; row < pkt.height; row++) {
            for (int col = 0; col < pkt.width; col++) {
                pkt.data[row][col] = canvas[row][col + pkt.x];
            }
        }

        send_packet(clients[i], &pkt);
    }

    close(server_fd);
    printf("✅ Canvas enviado a todos los monitores.\n");
    return 0;
}
