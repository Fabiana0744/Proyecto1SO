#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char *config_file = NULL;
    char *monitors = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "c:m:")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'm':
                monitors = optarg;
                break;
            default:
                fprintf(stderr, "Uso: %s -c archivo_config [-m monitor1,monitor2,...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!config_file) {
        fprintf(stderr, "El parámetro -c (archivo de configuración) es obligatorio\n");
        exit(EXIT_FAILURE);
    }

    printf("Archivo de configuración: %s\n", config_file);
    if (monitors) {
        printf("Monitores a observar: %s\n", monitors);
    } else {
        printf("No se especificaron monitores\n");
    }

    // Aquí continuarás: leer el archivo de configuración y usar tus hilos

    return 0;
}
