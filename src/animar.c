// Punto de entrada principal del programa animador.
// Entrada: argumentos de línea de comandos para especificar modo y parámetros.
// Salida: ejecuta el servidor o cliente según los flags; retorna 0 si tuvo éxito o EXIT_FAILURE si hubo error.

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

/* prototipos que viven en server_anim.c y client_anim.c */
void run_server(const char* cfg);
void run_client(const char* host, int port);

int main(int argc, char* argv[])
{
    const char* cfg  = NULL;   // Almacena la ruta al archivo .ini si se ejecuta como servidor (-c).
    const char* host = NULL;   // Almacena la IP del servidor si se ejecuta como cliente (-m).
    int  port = 5000;          // Puerto para conectarse al servidor (opcional, por defecto 5000).

    // Procesa los argumentos de línea de comandos con getopt.
    // Entrada: argc y argv.
    // Salida: inicializa las variables cfg, host y port con los valores correspondientes.
    int opt;
    while ((opt = getopt(argc, argv, "c:m:p:")) != -1) {
        switch (opt) {
        case 'c': cfg  = optarg; break;           // Se especifica archivo de configuración.
        case 'm': host = optarg; break;           // Se especifica modo monitor (cliente) con IP.
        case 'p': port = atoi(optarg); break;     // Se especifica puerto.
        default:
            // Muestra mensaje de uso en caso de error de argumentos.
            fprintf(stderr,
              "uso:\n"
              "  animar -c archivo.ini               # servidor\n"
              "  animar -m host [-p puerto]          # cliente/monitor\n");
            return EXIT_FAILURE;
        }
    }

    // Ejecuta como servidor si se especificó -c y no -m.
    if (cfg && !host) {
        run_server(cfg);
    }
    // Ejecuta como cliente si se especificó -m y no -c.
    else if (host && !cfg) {
        run_client(host, port);
    }
    // Si se dan ambos o ninguno, muestra error.
    else {
        fprintf(stderr,
          "Debes elegir exactamente UNA opción: -c o -m\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
