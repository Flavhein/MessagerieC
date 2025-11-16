#include"serveur_header.h"



struct NoeudSock* cree_noeud() {
    struct NoeudSock* nouveauNoeud = (struct NoeudSock*)malloc(sizeof(struct NoeudSock));
    memset(nouveauNoeud, 0, sizeof(struct NoeudSock)); //mise à zéro des éléments
    nouveauNoeud->prochainNoeud = NULL;
	nouveauNoeud->nickname[NICK_LEN-1]='\0';
	strcpy(nouveauNoeud->salon,"Hall");
    return nouveauNoeud;
}

struct NoeudSock* find_noeud(int fd,struct NoeudSock* noeud){
	struct NoeudSock* trans = noeud;
    while (trans != NULL) {
		printf("trans->connfd : %i   fd : %i\n",trans->connfd,fd);
        if (trans->connfd == fd) {
            return trans; 
        }
        trans = trans->prochainNoeud; 
    }
    return NULL; 
}

struct message init_msgstruct(char* nick_sender,char* buff,enum msg_type type,char* infos){
	struct message msgstruct;
	memset(&msgstruct, 0, sizeof(struct message));  // Cleaning memory
	msgstruct.pld_len=strlen(buff);  // Filling structure
	msgstruct.type=type;
	strncpy(msgstruct.nick_sender, nick_sender, strlen(nick_sender));
	strncpy(msgstruct.infos, infos, strlen(infos));
	return msgstruct;
}

int from_name_find_fd(char* nickname,struct NoeudSock* noeud){
	struct NoeudSock* trans = noeud;
    while (trans != NULL) {
        if (strncmp(trans->nickname,nickname,strlen(nickname)) == 0 && strlen(trans->nickname)==strlen(nickname)) {
            return trans->connfd; 
        }
        trans = trans->prochainNoeud; 
    }
    return -1; 
}

struct NoeudSock* find_noeud_parent(int fd, struct NoeudSock* noeud) {
    struct NoeudSock* parent = NULL; 
    struct NoeudSock* trans = noeud;

    while (trans != NULL) {
        if (trans->connfd == fd) {
            return parent; 
        }
        parent = trans; 
        trans = trans->prochainNoeud;
    }
    return NULL; // Retournez NULL si le noeud n'est pas trouvé
}


int affiche_list(int sockfd,char* nickname,struct NoeudSock* premierNoeud){
	char buff[MSG_LEN];
	// Cleaning memory
	memset(buff, 0, MSG_LEN);
	buff[MSG_LEN-1]='\0';
	int head=0;

	struct NoeudSock* trans=premierNoeud;
	while(trans != NULL){
		if ((strncmp(trans->nickname, nickname,strlen(nickname)) != 0) && (head+1 < MSG_LEN)){
			if (head + strlen(trans->nickname) + 1 < MSG_LEN) {
            	if (head > 0) {
                	buff[head++] = ','; // Ajout d'un séparateur
            	}
            strcpy(buff + head, trans->nickname);
            head += strlen(trans->nickname);
        	}
		}
		trans=trans->prochainNoeud;
	}
	printf("buff : %s",buff);
	if (strlen(buff)==0){
		strcpy(buff,"[SERVER] : Pas d'autres clients");
	}

	struct message msgstruct = init_msgstruct("SERVER",buff,NICKNAME_LIST,"\0");

	if (send_struct_and_msg(sockfd,&msgstruct,buff)==0){
		printf("[SERVER] : problème au niveau de l'envoie affiche_list\n");
		return 0;
	}
	return 1;
}


int send_general(int sockfd, char* nick_sender,char* buff,enum msg_type type,char* infos){
	struct message msgstruct = init_msgstruct(nick_sender,buff,type,infos);	

	if (send_struct_and_msg(sockfd,&msgstruct,buff)==0){
		printf("[SERVER] : Problème au niveau de l'envoie de send_general\n");
		return 0;
	}
	printf("Message sent!\n\n");
	return 1;
}



int send_unicast(char* nicknameSender, char* nicknameTarget,struct NoeudSock* premierNoeud,char* buff){
	struct message msgstruct = init_msgstruct(nicknameSender,buff,UNICAST_SEND,"\0");

	int fdTarget=from_name_find_fd(nicknameTarget,premierNoeud);
	int fdSender=from_name_find_fd(nicknameSender,premierNoeud);

	if (fdTarget < 0){
		if (send_warning(fdSender)==0){
			return 0;
		}
	}

	if (send_struct_and_msg(fdTarget,&msgstruct,buff)==0){
			printf("[SERVER] : problème au niveau du send unicast");
			return 0;
	}
	
	return 1;
}

int send_warning(int sockfd){
	char* buff="User n'existe pas";

	struct message msgstruct = init_msgstruct("SERVER",buff,UNICAST_SEND,"\0");

	if (send_struct_and_msg(sockfd,&msgstruct,buff)==0){
		printf("[SERVER] : problème au niveau du send unicast du warning");
		return 0;
	}
	return 1;
}

int send_reject(int sockfd,char* nickname){
	char buff[MSG_LEN];
	sprintf(buff,"%s vous a refusé l'envoie du fichier",nickname);
	struct message msgstruct = init_msgstruct(nickname,buff,FILE_REJECT,"\0");

	if (send_struct_and_msg(sockfd,&msgstruct,buff)==0){
		printf("[SERVER] : problème au niveau du send reject");
		return 0;
	}
	return 1;
}

int send_broadcast(char* nickname,struct NoeudSock* premierNoeud,char* buff){
	struct message msgstruct = init_msgstruct(nickname,buff,BROADCAST_SEND,"\0");

	struct NoeudSock* trans=premierNoeud;
	trans=trans->prochainNoeud;
	while (trans != NULL){
		if (strncmp(trans->nickname, nickname,strlen(nickname)) != 0){
			if (send_struct_and_msg(trans->connfd,&msgstruct,buff)==0){
				printf("[SERVER] : problème au niveau du send broadcast");
				return 0;
			}
		}
		trans=trans->prochainNoeud;
	}
	return 1;
}

int send_struct_and_msg(int sockfd, struct message* p_msgstruct,char* buff){
	// Sending structure
	int ret=0;
	int envoye=0;
	do {
		ret=send(sockfd, (char*)p_msgstruct+envoye, sizeof(struct message)-envoye, 0); 
		//printf("Sending structure: %ld bytes\n", sizeof(struct message) - envoye);
		printf("Sending message: %d bytes\n", p_msgstruct->pld_len - envoye);

		if (ret < 0){
			perror("send_struct");
			return 0;
		}
		else  {
			envoye+=ret;
		}
	} while (envoye != sizeof(struct message));


	// Sending message
	envoye=0;
	ret=0;
	do {
		ret=send(sockfd, buff+envoye, p_msgstruct->pld_len-envoye, 0); //envoie sur la socket ce qu'il a reçu 
		if (ret < 0){
			perror("send_message");
			return 0;
		}
		else  {
			envoye+=ret;
		}
	} while (envoye != p_msgstruct->pld_len);
	return 1;
}


int whois(int sockfd,struct message* p_msgstruct,struct NoeudSock* premierNoeud) {
	printf("[SERVER] : fonction whois appelé\n");
	char pseudo_looked[NICK_LEN];
	strcpy(pseudo_looked,p_msgstruct->infos);
	struct NoeudSock* trans=premierNoeud->prochainNoeud;
	while(trans != NULL){ 
		if (strncmp(trans->nickname, pseudo_looked,strlen(pseudo_looked)) == 0){
			break;
		} else {trans=trans->prochainNoeud;}
	}
	if (trans!=NULL){
		// Le pseudo a été trouvé
		// Init buff
		char buff[MSG_LEN];
		memset(buff, 0, MSG_LEN);
		buff[MSG_LEN-1] = '\0';
		// Init struct message
		struct message msgstruct = init_msgstruct("SERVER",buff,NICKNAME_INFOS,"\0");
		// Récupération info à transmettre
		struct tm* local_time = &trans->date_heure;
		struct sockaddr_in clientAddr = trans->connSock;
		printf("%s\n",trans->salon);
		printf("%s Connecté le %02d/%02d/%d %02d:%02d:%02d avec adresse IP: %s et Port : %d\n",trans->nickname,local_time->tm_mday,local_time->tm_mon + 1,local_time->tm_year + 1900,local_time->tm_hour,local_time->tm_min,local_time->tm_sec,inet_ntoa(clientAddr.sin_addr),ntohs(clientAddr.sin_port));
		sprintf(buff,"%s Connecté le %02d/%02d/%d %02d:%02d:%02d avec adresse IP: %s et Port : %d",trans->nickname,local_time->tm_mday,local_time->tm_mon + 1,local_time->tm_year + 1900,local_time->tm_hour,local_time->tm_min,local_time->tm_sec,inet_ntoa(clientAddr.sin_addr),ntohs(clientAddr.sin_port));
		msgstruct.pld_len = strlen(buff);
		send_struct_and_msg(sockfd,&msgstruct,buff);
		return 1;
	}
	printf("[SERVER] : Pseudo (%s) non trouvé",pseudo_looked);
	return 0;
}

int nick(int sockfd,struct message* p_msgstruct,struct NoeudSock* premierNoeud) {
	printf("[SERVER] : la fonction nick est appelé\n");

	char* nickname=p_msgstruct->infos;

	struct NoeudSock* trans=premierNoeud;
	int isvalide=1; //valide

	while(trans != NULL){  //on regarde si le pseudo est pas déjà pris
		//printf("%s compare %s\n",trans->nickname,nickname);
		if (strncmp(trans->nickname, nickname,strlen(nickname)) == 0 && strlen(trans->nickname)==strlen(nickname)){
			isvalide=0; //invalide;
			printf("[SERVER] : le pseudo est similaire\n");
			break;
		}
		else {
			trans=trans->prochainNoeud;
		}
	}

	struct message msgstruct = init_msgstruct("SERVER","\0",NICKNAME_NEW,isvalide ? "1" : "0");

	// Sending structure (Nickname)   //on prévient le client pour lui dire si son pseudo est pris ou non
	int ret=0;
	int envoye=0;
	do {
		ret=send(sockfd, (char*)&msgstruct+envoye, sizeof(msgstruct)-envoye, 0); 
		if (ret <= 0){
			perror("send_struct_nick\n");
			return 0;
		}
		else  {
			envoye+=ret;
		}
	} while (envoye != sizeof(msgstruct));

	//printf("nickname dans nick : %s\n",nickname);
	if (isvalide){
		struct NoeudSock* noeud=find_noeud(sockfd,premierNoeud);
		if (noeud == NULL) {
    		printf("[SERVER] : erreur : noeud introuvable pour fd: %d\n", sockfd);
    	return 0;
		}
		strncpy(noeud->nickname, nickname, NICK_LEN);
		printf("[SERVER] : pseudo entré dans la liste chainé : %s\n",noeud->nickname);
	}
	return 1;
}

int send_salon(struct message* p_msgstruct, struct NoeudSock* premierNoeud, char* buff){
	printf("[SERVER] : fonction send_salon appelé\n");
	struct NoeudSock* trans=premierNoeud->prochainNoeud;
	char* salonlooked = p_msgstruct->infos;
	char* pseudosender = p_msgstruct->nick_sender;
	//char result[NICK_LEN];
	//sprintf(result,"[%s] %s",salonlooked,pseudosender);
	while(trans != NULL){
		if (strncmp(trans->salon,salonlooked,strlen(salonlooked)) == 0){
			//msgstruct init
			struct message msgstruct = init_msgstruct(pseudosender,buff,MULTICAST_SEND,"\0");
			//envoie de l'info
			if (send_struct_and_msg(trans->connfd,&msgstruct,buff) == 0){
				return 0;
			}
		}
		trans = trans->prochainNoeud;
	}
	return 1;
}

int estAlphaNumeriqueSansEspace(const char *chaine) {
    // Parcourir chaque caractère de la chaîne
    for (int i = 0; chaine[i] != '\0'; i++) {
        // Vérifier que chaque caractère est soit une lettre, soit un chiffre
        if (!isalpha(chaine[i]) && !isdigit(chaine[i])) {
            return 0; // Renvoie 0 si un caractère invalide est trouvé
        }
    }
    return 1; // Renvoie 1 si tous les caractères sont valides
}

int create(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud, int affectation){
	printf("[SERVER] : fonction create appelé\n");
	//verif chaine caractère
	char* nouveausalon = p_msgstruct->infos;
	if (estAlphaNumeriqueSansEspace(nouveausalon) == 0){
		return 0;
	}
	//Savoir si le salon existe déjà
	struct NoeudSock* trans=premierNoeud->prochainNoeud;

	while(trans != NULL){
		if (strncmp(trans->salon,nouveausalon,NICK_LEN) == 0){
			//msgstruct et buff init
			char buff[MSG_LEN];
			memset(buff, 0, MSG_LEN);	
			sprintf(buff,"Le salon existe déjà !");
			struct message msgstruct = init_msgstruct("SERVER",buff,MULTICAST_CREATE,"\0");
			//envoie de l'info
			if (send_struct_and_msg(sockfd,&msgstruct,buff) == 0){
				return 0;
			}
		}
		trans = trans->prochainNoeud;
	}
	//Trouver la personne qui a créer le salon et lui affecter
	if (affectation == 1){
		trans=premierNoeud->prochainNoeud;
		char* pseudo_looked = p_msgstruct->nick_sender;
		while(trans != NULL){ 
			if (strncmp(trans->nickname, pseudo_looked,strlen(pseudo_looked)) == 0){
				break;
			} else {trans=trans->prochainNoeud;}
		}
		char salonquit[NICK_LEN];
		strncpy(salonquit,trans->salon,NICK_LEN);
		strncpy(trans->salon,nouveausalon,NICK_LEN);
		char buff[MSG_LEN];
		memset(buff, 0, MSG_LEN);
		sprintf(buff,"%s a quitté le salon %s ;(",pseudo_looked,salonquit);
		struct message msgstruct = init_msgstruct(pseudo_looked,buff,MULTICAST_SEND,salonquit);
		send_salon(&msgstruct,premierNoeud,buff);
	}
	//Envoyer la confirmation
	char buff[MSG_LEN];
	memset(buff, 0, MSG_LEN);
	sprintf(buff,"Salon %s créé avec succée, tu es dedans",nouveausalon);
	struct message msgstruct = init_msgstruct("SERVER",buff, MULTICAST_CREATE, "\0");
	send_struct_and_msg(sockfd,&msgstruct,buff);
	return 1;
}

// Fonction pour ajouter une chaîne de caractères au tableau dynamique
void ajouter_chaine(char ***tableau, int *taille, const char *chaine) {
    // Redimensionner le tableau pour accueillir une nouvelle chaîne
    char **temp = realloc(*tableau, (*taille + 1) * sizeof(char *));
    if (temp == NULL) {
        perror("Erreur d'allocation mémoire");
        exit(EXIT_FAILURE);
    }
    *tableau = temp;

    // Allouer de la mémoire pour la nouvelle chaîne et la copier
    (*tableau)[*taille] = malloc(strlen(chaine) + 1); // +1 pour le caractère nul '\0'
    if ((*tableau)[*taille] == NULL) {
        perror("Erreur d'allocation mémoire pour la chaîne");
        exit(EXIT_FAILURE);
    }
    strcpy((*tableau)[*taille], chaine);

    // Incrémenter la taille du tableau
    (*taille)++;
}

// Fonction pour libérer la mémoire du tableau dynamique de chaînes
void liberer_tableau(char **tableau, int taille) {
    for (int i = 0; i < taille; i++) {
        free(tableau[i]); // Libérer chaque chaîne
    }
    free(tableau); // Libérer le tableau lui-même
}

int list(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud){
	struct NoeudSock* trans=premierNoeud->prochainNoeud;
    char **tableau = NULL; // Tableau de chaînes initialement vide
    int taille = 0;        // Nombre de chaînes dans le tableau
	int skip = 0;
	char buff[MSG_LEN];
	memset(buff, 0, MSG_LEN);	
	strcpy(buff,"Ces salons sont disponibles : ");
	int i;
	int head=strlen("Ces salons sont disponibles : ");
	while (trans != NULL){
		if (taille>=1){
			i = 0;
			while (i<taille) {
				printf("Chaîne %d : %s\n", i + 1, tableau[i]);
				if (strcmp(tableau[i],trans->salon) == 0){
					skip = 1;
				}
				i = i + 1;
			}
		}
		if (skip == 0) {
			// affecter le salon au tableau
			ajouter_chaine(&tableau, &taille, trans->salon);
			if (head + strlen(trans->salon) + 1 < MSG_LEN) {
            	if (head > 0) {
                	buff[head++] = ','; // Ajout d'un séparateur
            	}
            strcpy(buff + head, trans->salon);
            head += strlen(trans->salon);
        	}
			
		}
		skip = 0;
		trans = trans->prochainNoeud;
	}
    struct message msgstruct = init_msgstruct("SERVER",buff,MULTICAST_LIST,"\0");
	if (send_struct_and_msg(sockfd,&msgstruct,buff) == 0){//envoie de l'info
		return 0;
	}

	liberer_tableau(tableau,taille);
	return 1;
}

int join(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud){
	printf("[SERVER] : fonction join appelé\n");
	//vérification si le salon existe
	struct NoeudSock* trans=premierNoeud->prochainNoeud;
	char* salonjoin = p_msgstruct->infos;
	char salonquit[NICK_LEN] = "all";
	int toggle = 0;
	while(trans != NULL){
		//printf("%s\n",trans->salon);
		//printf("%i\n",strncmp(trans->salon,salonjoin,strlen(salonjoin)));
		if (strncmp(trans->salon,salonjoin,strlen(salonjoin)) == 0){
			toggle = 1;
		}
		trans = trans->prochainNoeud;
	}
	if (toggle == 0){
		if (strcmp(salonjoin,"all") == 0){
			create(sockfd, p_msgstruct,premierNoeud,0);
		} else {
			char buff[MSG_LEN];
			memset(buff, 0, MSG_LEN);	
			sprintf(buff,"Le salon n'existe pas !");
			//msgstruct et buff init
			struct message msgstruct = init_msgstruct("SERVER",buff,MULTICAST_SEND,"\0");
			//envoie de l'info
			if (send_struct_and_msg(sockfd,&msgstruct,buff) == 0){
				return 0;
			}
			return 0;
		}
	}
	//affectation du salon 
	trans=premierNoeud->prochainNoeud;
	char* pseudo_looked = p_msgstruct->nick_sender;
	while(trans != NULL){
		if (strncmp(trans->nickname,pseudo_looked,strlen(pseudo_looked)) == 0){
			strncpy(salonquit,trans->salon,NICK_LEN);
			memset(trans->salon, 0, NICK_LEN);	
			strncpy(trans->salon,p_msgstruct->infos,NICK_LEN);
			//msgstruct et buff init
			char buff[MSG_LEN];
			memset(buff, 0, MSG_LEN);
			sprintf(buff,"Tu as rejoint le salon %s",trans->salon);
			struct message msgstruct = init_msgstruct("SERVER",buff, MULTICAST_SEND, "\0");
			//envoie de l'info
			if (send_struct_and_msg(sockfd,&msgstruct,buff) == 0){
				return 0;
			}
		}

		trans = trans->prochainNoeud;
	}
	// Dernier à partir ?
	trans=premierNoeud->prochainNoeud;
	int dernier = 1;
	while (trans != NULL){
		if (strcmp(trans->salon,salonquit) == 0){
			dernier = 0;
		}
		trans = trans->prochainNoeud;
	}
	char buff[MSG_LEN];
	//struct message msgstruct;
	if (dernier == 1) {
		// Prévenir de la destruction du salon
		memset(buff, 0, MSG_LEN);
		sprintf(buff,"Tu es seul a quitter le salon %s, il est détruit",salonquit);
		struct message msgstruct = init_msgstruct("SERVER",buff, MULTICAST_JOIN,"\0");
		if (send_struct_and_msg(sockfd,&msgstruct,buff) == 0){
			return 0;
		}
	} else {
		//Prévenir qu'il part
		memset(buff, 0, MSG_LEN);
		sprintf(buff,"%s a quitté le salon %s ;(",pseudo_looked,salonquit);
        struct message msgstruct = init_msgstruct(pseudo_looked,buff, MULTICAST_SEND,salonquit);
		send_salon(&msgstruct,premierNoeud,buff);
	}
	//Prévenir qu'il vient
	memset(buff, 0, MSG_LEN);
	sprintf(buff,"%s a rejoint le salon %s, dites bonjour !",pseudo_looked, salonjoin);
    struct message msgstruct = init_msgstruct(pseudo_looked,buff, MULTICAST_SEND,salonjoin);
	send_salon(&msgstruct,premierNoeud,buff);
	return 1;
}

char* ip_sock(int sockfd){
	struct sockaddr_in local_addr;
	socklen_t addr_len=sizeof(local_addr);
	char* ip=(char*)malloc(INET_ADDRSTRLEN*sizeof(char*));
    memset(ip,0,INET_ADDRSTRLEN);
					
	if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) == -1) {
    	perror("getsockname");
    }
	strcpy(ip,inet_ntoa(local_addr.sin_addr));
	return ip;
}