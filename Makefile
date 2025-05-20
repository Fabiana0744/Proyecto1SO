# ------------------------------------------------------------
# Compilador y flags
CC      := gcc
CFLAGS  := -Wall -Wextra -Iinclude -Iexternal/inih
LDFLAGS :=        # (vacío: no usamos -lpthread)

# Fuentes comunes a servidor y cliente
SRCS_COMMON = \
    src/net.c \
    src/mypthreads.c \
    src/scheduler.c \
    src/scheduler_rr.c \
    src/scheduler_lottery.c \
    src/scheduler_realtime.c \
    src/parser_config.c \
    src/parser_objetos.c \
    external/inih/ini.c

# Wrapper con main()
SRCS_WRAPPER = src/animar.c \
               src/server_anim.c \
               src/client_anim.c

# Objetivo final
animar: $(SRCS_COMMON) $(SRCS_WRAPPER)
	$(CC) $(CFLAGS) -o $@ $^

# Limpiar binarios y objetos
clean:
	rm -f animar *.o
# ------------------------------------------------------------
