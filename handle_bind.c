#include "serveur_header.h"


int handle_bind(char *server_port, char *server_addr) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;      //parametre de la socket
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (getaddrinfo(server_addr, server_port, &hints, &result) != 0) { //test si une struct qui contient les infos de connexion est renvoyé
		perror("getaddrinfo()\n");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,     //creation d'une socket
		rp->ai_protocol);
		if (sfd == -1) {  //test si la socket est bien créé
			continue;
		}
		
		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0) {  //etape bind
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

