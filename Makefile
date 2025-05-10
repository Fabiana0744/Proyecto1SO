CC = gcc
CFLAGS = -Iinclude

OBJS_SERVER = src/server_anim.c src/net.c src/parser_config.c
OBJS_CLIENT = src/client_anim.c src/net.c

server_anim: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

client_anim: $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f server_anim client_anim
