# Compilador y flags
CC = gcc
CFLAGS = -Iinclude -Iexternal/inih

# Archivos fuente
SRCS_COMMON = src/net.c
SRCS_SERVER = src/server_anim.c src/parser_config.c src/parser_objetos.c \
              src/mypthreads.c src/scheduler.c \
              src/scheduler_rr.c src/scheduler_lottery.c src/scheduler_realtime.c \
              external/inih/ini.c

SRCS_CLIENT = src/client_anim.c

# Objetivos
server_anim: $(SRCS_SERVER) $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

client_anim: $(SRCS_CLIENT) $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

# Limpiar binarios
clean:
	rm -f server_anim client_anim
