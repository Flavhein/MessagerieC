#include"serveur_header.h"

static char* msg_type_str[] = {
	"NICKNAME_NEW",
	"NICKNAME_LIST",
	"NICKNAME_INFOS",
	"ECHO_SEND",
	"UNICAST_SEND", 
	"BROADCAST_SEND",
	"MULTICAST_CREATE",
	"MULTICAST_LIST",
	"MULTICAST_JOIN",
	"MULTICAST_SEND",
	"MULTICAST_QUIT",
	"FILE_REQUEST",
	"FILE_ACCEPT",
	"FILE_REJECT",
	"FILE_SEND",
	"FILE_ACK"
};

// Toute les fonctions sont dans serveur_fonctions.c

int gere_client(struct pollfd* fds,int indice_client,struct NoeudSock* premierNoeud) {
	int sockfd=fds[indice_client].fd;
	char buff[MSG_LEN];
	struct message msgstruct;

	// Cleaning memory
	memset(buff, 0, MSG_LEN);
	memset(&msgstruct, 0, sizeof(struct message));

	printf("[SERVER] : je vais recevoir une msgstruct (recv)\n");

	// Receiving structure
	int received=0;
	int ret=0;
	do {
		ret=recv(sockfd, (char*)&msgstruct+received, sizeof(struct message)-received, 0);
		if (ret <= 0) {
			perror("recv_struct");
			return 0;
		}
		else {
			received+=ret;
		}
	} while (received != sizeof(struct message));
	//printf("[SERVER] : msgstruct recu\n");
	printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);

	// Receiving message
	if (msgstruct.type != NICKNAME_NEW && msgstruct.type != NICKNAME_LIST && msgstruct.type != NICKNAME_INFOS && msgstruct.pld_len!=0)  {
		received=0;
		ret=0;
		do {
			ret=recv(sockfd, buff+received, msgstruct.pld_len-received, 0);
			if (ret <= 0) {
				perror("recv");
				return 0;
			}
			else {
				received+=ret;
				if (strchr(buff, '\n') != NULL) {
   		         	break; //si on trouve '\n' on sort du whilen on a reçu tout le message
        		}
			}
			//if (ret==0){
			//	printf("taille du message reçu de 0");
			//	break;
			//}
		} while (received != msgstruct.pld_len);

		printf("Received: %s\n", buff);
	}

	if (msgstruct.type==NICKNAME_NEW){  // a remplacé par un case pour tous les types
		nick(sockfd,&msgstruct,premierNoeud);
		printf("après nick\n");
	}

	if (msgstruct.type==NICKNAME_LIST){
		affiche_list(sockfd,msgstruct.nick_sender,premierNoeud);
	}

	if (msgstruct.type==FILE_REJECT){
		char* target=msgstruct.infos;
		int target_fd=from_name_find_fd(target,premierNoeud);
		send_reject(target_fd,msgstruct.nick_sender);
	}

	if (msgstruct.type==FILE_REQUEST || msgstruct.type== FILE_ACCEPT){
		char* target=msgstruct.infos;
		printf("target : %s\n",target);

		if ((strstr(buff,"127.0.0")!=NULL) && (msgstruct.type=FILE_ACCEPT)){
			printf("je suis après le contrôle de loopback...\n");
			char* ip=ip_sock(fds[0].fd);
			printf("ip : %s",ip);
			int head=0;
			while (buff[head]!=':'){
				if (buff[head]=='\0'){
					break;
				}
				head++;
			}
			char temp_port[NICK_LEN];
			memset(temp_port,0,NICK_LEN);
			strcpy(temp_port,buff+head);
			memset(buff,0,MSG_LEN);
			sprintf(buff,"%s:%s",ip,temp_port);
			free(ip);
		}

		int target_fd=from_name_find_fd(target,premierNoeud);
		if (target_fd==-1){
			send_general(sockfd,"SERVER","erreur request ou accept\n",msgstruct.type,"\0");
		} else {
			send_general(target_fd,msgstruct.nick_sender,buff,msgstruct.type,"\0");
		}
	}

	if (msgstruct.type==BROADCAST_SEND){
		send_broadcast(msgstruct.nick_sender,premierNoeud,buff);
	}

	if (msgstruct.type==UNICAST_SEND){
		send_unicast(msgstruct.nick_sender,msgstruct.infos,premierNoeud,buff);
	}

	if (msgstruct.type==NICKNAME_INFOS){
		whois(sockfd,&msgstruct,premierNoeud);
	}

	if (msgstruct.type==MULTICAST_SEND){
		send_salon(&msgstruct,premierNoeud,buff);
	}

	if (msgstruct.type==MULTICAST_CREATE){
		create(sockfd,&msgstruct,premierNoeud,1);
	}

	if (msgstruct.type==MULTICAST_JOIN){
		join(sockfd,&msgstruct,premierNoeud);
	}

	if (msgstruct.type==MULTICAST_LIST){
		list(sockfd,&msgstruct,premierNoeud);
	}

	int quitsalon = 0;
	if (msgstruct.type==MULTICAST_QUIT){
		quitsalon = 1;
		join(sockfd,&msgstruct,premierNoeud);
	}

	if (strncmp(buff, "/quit", strlen("/quit")) == 0 && quitsalon == 0){
		return 0;
	} 

	if (msgstruct.type==ECHO_SEND && msgstruct.pld_len!=0){
		if (send_general(sockfd,"SERVER",buff,ECHO_SEND,"\0")==0){
			return 0;
		}
	}
	
	return 1;
}


int main(int argc, char *argv[]) {

	char *server_port;
	if (argc>1){
		server_port=argv[1];
	}
	else {
		server_port=SERV_PORT;
	}

	int sfd;//, connfd;
	sfd = handle_bind(server_port,NULL); //nous sort une socket, la socket d'écoute

	if ((listen(sfd, CONMAX)) != 0) {   //etape listen
		perror("listen()\n");
		exit(EXIT_FAILURE);
	}

	struct pollfd fds[CONMAX];
	fds[0].fd=sfd;
	fds[0].events=POLLIN;
	fds[0].revents=0;
	for (int j=1; j<CONMAX;j++) {
		fds[j].fd=-1;
		fds[j].events=0;
		fds[j].revents=0;
	}

	//printf("%i\n",SOMAXCONN); //4096

	struct NoeudSock* premierNoeud=cree_noeud(); //initialisation de la liste chaine
	struct NoeudSock* noeudActuel=premierNoeud;	

	printf("[SERVER] : Prêt à écouter\n");

	while (1) {   
		int ret = poll(fds,CONMAX,-1); //poll et verification de poll
		if (ret < 0){
			perror("poll");
			exit(EXIT_FAILURE);
		}
		// on cherche un noeud libre pour stocker les informations du client
		
		for (int j=0; j<CONMAX; j++){
			if (j==0 && (fds[j].revents & POLLIN)==POLLIN) {
				printf("[SERVER] : il y a un evenement sur la socket d'écoute\n");

				struct NoeudSock* noeudLibre=cree_noeud();
				noeudActuel->prochainNoeud=noeudLibre;
				noeudActuel=noeudLibre;	
				int fd_accept=-1;
				noeudActuel->tailleSock = sizeof(noeudActuel->connSock);

				if (( fd_accept = accept(fds[0].fd, (struct sockaddr*) &noeudActuel->connSock, &(noeudActuel->tailleSock))) < 0) {  //etape accept
					perror("accept()\n");
					exit(EXIT_FAILURE);
				}
				time_t current_time = time(NULL);
				if (current_time == -1) {
					perror("Erreur lors de l'obtention du temps");
					return 1;
				}
				struct tm local_time = *localtime(&current_time);
				memcpy(&noeudActuel->date_heure, &local_time, sizeof(struct tm));
				strncpy(noeudActuel->salon,"all",4);
				noeudActuel->connfd=fd_accept;
				printf("fd_accept : %i\n",fd_accept);

				int y=0;
				while (y < CONMAX && fds[y].fd != -1) {
					y++;
				}
				fds[y].fd=fd_accept;	
				fds[y].events=POLLIN;
				fds[y].revents=0;
				fds[0].revents=0;

				

				printf("[SERVER] : utilisateur accepté et connecté\n");
			}
			else if ((fds[j].revents & POLLIN) == POLLIN){
				//printf("[SERVER] : Je suis après le else if pour poll\n");
				//fflush(stdout);
				if (gere_client(fds,j,premierNoeud)==0){   //fct qui affiche ce que recoit la socket connfd
				//remise à zéro du descripteur et fermeture de la socket
					struct NoeudSock* parent = find_noeud_parent(fds[j].fd,premierNoeud);
					if (parent == NULL) {
    					printf("[SERVER] : erreur : parent introuvable\n");
					} else {
						struct NoeudSock* enfant=parent->prochainNoeud;
						if (enfant != NULL) {
							if (enfant->prochainNoeud==NULL){
								noeudActuel=parent;
							}
    						printf("[SERVER] : le client %s va être déconnecté\n", enfant->nickname);
    						parent->prochainNoeud = enfant->prochainNoeud;
    						free(enfant);
							
						} else {
   	 						printf("[SERVER] : problème struct NoeudSock pour réafecter un noeud\n");
						}
					}
					
					if (fds[j].fd >= 0) {
    					close(fds[j].fd);
					}					
					fds[j].fd=-1;
					fds[j].events=0;
					

				}
				fds[j].revents=0;
			}
		}
		
	}
	return EXIT_SUCCESS;
}

