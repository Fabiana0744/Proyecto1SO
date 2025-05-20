/*  src/animar.c  */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

/* prototipos que viven en server_anim.c y client_anim.c */
void run_server(const char* cfg);
void run_client(const char* host, int port);

int main(int argc, char* argv[])
{
    const char* cfg  = NULL;   /* -c */
    const char* host = NULL;   /* -m */
    int  port = 5000;          /* -p (opcional) */

    int opt;
    while ((opt = getopt(argc, argv, "c:m:p:")) != -1) {
        switch (opt) {
        case 'c': cfg  = optarg; break;
        case 'm': host = optarg; break;
        case 'p': port = atoi(optarg); break;
        default:
            fprintf(stderr,
              "uso:\n"
              "  animar -c archivo.ini               # servidor\n"
              "  animar -m host [-p puerto]          # cliente/monitor\n");
            return EXIT_FAILURE;
        }
    }

    if (cfg && !host) {                /* === servidor === */
        run_server(cfg);
    } else if (host && !cfg) {         /* === cliente  === */
        run_client(host, port);
    } else {
        fprintf(stderr,
          "Debes elegir exactamente UNA opción: -c o -m\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
