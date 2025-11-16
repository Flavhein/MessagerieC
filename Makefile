CC = gcc
CFLAGS = -Wall -Werror
# LDFLAGS = -lpthread

EXECS = client server

all: $(EXECS)

client: client.o handle_bind.o
	$(CC) $(CFLAGS) -o client client.o handle_bind.o -lncurses

# Ajout de la dépendance serveur_send.o pour server
server: server.o serveur_fonctions.o handle_bind.o
	$(CC) $(CFLAGS) -o server server.o serveur_fonctions.o handle_bind.o -lncurses

# Compilation de chaque fichier .c séparément
client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c -lncurses

server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c

serveur_fonctions.o: serveur_fonctions.c common.h
	$(CC) $(CFLAGS) -c serveur_fonctions.c

handle_bind.o: handle_bind.c 
	$(CC) $(CFLAGS) -c handle_bind.c

# Compilation de chaque cible
%: %.c
	$(CC) $(CFLAGS) -o $@ $< -lncurses

clean:
	rm -f client server *.o
