#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>

#include "ncurses.h"

#include "common.h"

int handle_bind(char *server_port,char *server_addr);

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

struct info_client{
		char nickname[NICK_LEN];
		char salon[NICK_LEN];
		char nom_fichier[NICK_LEN];
};

//variable globale
int height;
int width;
int display_height;
int input_height;
int zone_width;

struct ListMsg {
	struct ListMsg * ancienNoeud;
	char message[MSG_LEN+NICK_LEN+3];
	struct ListMsg * prochainNoeud;
};

struct ListMsg* cree_noeud() {
    struct ListMsg* nouveauNoeud = (struct ListMsg*)malloc(sizeof(struct ListMsg));
    memset(nouveauNoeud, 0, sizeof(struct ListMsg)); //mise à zéro des éléments
	nouveauNoeud->ancienNoeud = NULL;
    nouveauNoeud->prochainNoeud = NULL;
    return nouveauNoeud;
}

struct ListMsg* initNoeud(struct ListMsg* noeudParent,char* buff){
	struct ListMsg* nouveauNoeud=cree_noeud();
	nouveauNoeud->ancienNoeud=noeudParent;
	noeudParent->prochainNoeud=nouveauNoeud;
	strncpy(nouveauNoeud->message,buff,MSG_LEN+NICK_LEN+3);
	return nouveauNoeud;
}

void update_input(WINDOW* input_win ,char* preamb){
	werase(input_win);
	box(input_win, 0, 0);
	mvwprintw(input_win, 0, 1, " Zone de saisie | /help ");
	mvwprintw(input_win, 1, 1, "%s",preamb);
    wrefresh(input_win);
}

void update_display(WINDOW* display_win,struct ListMsg* noeudActuel){
	int nb_ligne=strlen(noeudActuel->message)/ zone_width+1;
	while((nb_ligne < display_height - 1) && (noeudActuel->ancienNoeud != NULL)){
		noeudActuel=noeudActuel->ancienNoeud;
		nb_ligne=nb_ligne+strlen(noeudActuel->message)/ zone_width+1;
	}
	if (noeudActuel->ancienNoeud==NULL){
		noeudActuel=noeudActuel->prochainNoeud;
	}
	werase(display_win);
    mvwprintw(display_win, 0, 1, "Zone d'affichage");
	int y_position = 1; // Commencer à la ligne 1 après le titre
	while((noeudActuel!=NULL) && (y_position <= nb_ligne)){
        mvwprintw(display_win, y_position, 1, "%s", noeudActuel->message); 
        y_position += strlen(noeudActuel->message)/ zone_width+1;
		noeudActuel=noeudActuel->prochainNoeud;
    }
	wrefresh(display_win);
}

char* trouve_nom(char* buff){
	char* target=(char*)malloc(NICK_LEN);
	memset(target,0, NICK_LEN);
	int head=0;
	while ((buff[head] != ' ') && (head< MSG_LEN)){
		target[head]=buff[head];
		head++;
	}
	target[head]='\0';
	return target;
}

int send_struct(struct message* p_msgstruct,int sfd){	
	int ret=0;
	int envoye=0;
	do {
		ret = send(sfd, (char *)p_msgstruct+envoye, sizeof(struct message)-envoye, 0);
		if (ret <=0) {
			perror("send_struct");
			return 0;
		}
		else {
			envoye += ret;
		}
	} while (envoye != sizeof(struct message));
	return 1;
}

int send_msg(struct message* p_msgstruct,int sfd,char* buff){
	
	int envoye=0;
	int ret=0;
	do {
		ret = send(sfd, buff+envoye, p_msgstruct->pld_len-envoye, 0);
		if (ret <0) {
			perror("send_msg");
			return 0;
		}
		else {
			envoye += ret;
		}
		if (ret==0){
			break;
		}
	} while (envoye != p_msgstruct->pld_len);

	return 1;
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


int is_valid_nickname(const char* nickname) {
    if (strlen(nickname) >= NICK_LEN) {
        return 0; // invalide
    }
    for (int i = 0; nickname[i] != '\0'; i++) { //on vérifie chacun des caractères
        if (isspace(nickname[i]) || !isalnum(nickname[i])) {
            return 0; 
        }
    }
    return 1;
}

int nick(char* oldname,char* newname,int sfd,struct ListMsg* noeudActuel,WINDOW* display_win){
	struct message msgstruct;
	memset(&msgstruct, 0, sizeof(struct message));
	strncpy(msgstruct.nick_sender, oldname, NICK_LEN);

	if (is_valid_nickname(newname)==1) {

		strncpy(msgstruct.infos, newname, NICK_LEN-1); //copie
        msgstruct.infos[NICK_LEN - 1] = '\0'; // S'assurer que c'est bien terminé
		msgstruct.type =  NICKNAME_NEW;

		if (send_struct(&msgstruct,sfd)==0){ //on envoie une struct message avec le pseudo pour vérifier si il est pas déjà pris
			return 0;
		}

		int recu=0;
		int ret=0;
		memset(&msgstruct, 0, sizeof(struct message));
		do {	//on reçoit la réponse du server pour savoir si le pseudo est pas déjà pris
			ret = recv(sfd, (char *)&msgstruct+recu, sizeof(struct message)-recu, 0);
			if (ret <= 0) {
				noeudActuel=initNoeud(noeudActuel,"Le serveur est inaccessible\n");
				update_display(display_win,noeudActuel);
				return 0;
			}
			else {
				recu += ret;
			}
		} while (recu != sizeof(struct message));

		if (strncmp(msgstruct.infos, "1", strlen("1")) == 0){
			noeudActuel=initNoeud(noeudActuel,"Pseudo valide\n");
			strncpy(oldname, newname, NICK_LEN);
			return 1;
		} else {
			noeudActuel=initNoeud(noeudActuel,"[SERVER] : Pseudo déjà pris, faites /nick <votre pseudo> ou écrivez après Nouveau pseudo :\n");
			
			return 0;
		}
    } else {
		noeudActuel=initNoeud(noeudActuel,"[SERVER] : Pseudo Invalide, faites /nick <votre pseudo> ou écrivez après Nouveau pseudo :\n");
		return 0;
    }
}

int send_echo(char* nickname,int sfd,char* buff){
	struct message msgstruct=init_msgstruct(nickname,buff,ECHO_SEND,"\0");
	// Sending structure
	if (send_struct(&msgstruct,sfd)==0){
		return 0;
	}

	// Sending message
	if (send_msg(&msgstruct,sfd,buff)==0){
		return 0;
	}
	return 1; //tout s'est bien passé
}

int handle_connect(char *server_addr,char *server_port) {
	struct addrinfo hints, *result, *rp;
	int sfd;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(server_addr, server_port, &hints, &result) != 0) {
		perror("getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}
		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(sfd);
	}
	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);
	return sfd;
}

void boucle_client(struct info_client* infos,struct pollfd* fds,struct ListMsg* noeudActuel,WINDOW* input_win,WINDOW* display_win) {
	char buff[MSG_LEN];
	memset(buff, 0, MSG_LEN);

	//int toggle = 0;
	strncpy(infos->salon,"all",4);
    while (1) {
		memset(buff, 0, MSG_LEN);
		update_display(display_win,noeudActuel);
		update_input(input_win,"Message : ");

        int ret = poll(fds, 4, -1); 
        if (ret == -1) {
            perror("poll()");
            break;
        }


        if (fds[0].revents & POLLIN) { //le client veut écrire un truc
            
			
			wgetnstr(input_win, buff, MSG_LEN-1);
			if (buff[0]=='\0'){
				continue;
			}
			buff[MSG_LEN-1]='\0';

			int iscommand=0;

			char *resultat = strstr(buff, "/nick");
			if (resultat != NULL){
				iscommand=1;
				if (nick(infos->nickname,buff+6,fds[1].fd,noeudActuel,display_win)==0){
					noeudActuel=initNoeud(noeudActuel,"[SERVER] : le pseudo n'a pas été changé\n");
				} else {
					noeudActuel=initNoeud(noeudActuel,"[SERVER] : le pseudo a été accepté\n");
				}
				update_display(display_win,noeudActuel);
			}
			int toggle = 0;
			if (strncmp(buff, "/whois", strlen("/whois")) == 0) {
				iscommand=1;
				toggle = 1;
				if (strlen(buff)>strlen("/whois ")){
					if (is_valid_nickname(buff+7)==1){
						struct message msgstruct=init_msgstruct(infos->nickname,"\0",NICKNAME_INFOS,buff+7);
						if (send_struct(&msgstruct,fds[1].fd)==0){
							noeudActuel=initNoeud(noeudActuel,"Erreur whois\n");
						}
					} else {noeudActuel=initNoeud(noeudActuel,"Pseudo invalide");}
				}
    		}
			

			if (strncmp(buff, "/who", strlen("/who")) == 0 && toggle == 0) {
				iscommand=1;
        		struct message msgstruct=init_msgstruct(infos->nickname,"\0",NICKNAME_LIST,"\0");
				if (send_struct(&msgstruct,fds[1].fd)==0){
					noeudActuel=initNoeud(noeudActuel,"Erreur who\n");
				}
    		}

			toggle = 0;
			if (strstr(buff,"/msgall") != NULL){
				iscommand=1;
				if (strlen(buff)>strlen("/msgall ")){
					struct message msgstruct=init_msgstruct(infos->nickname,buff+8,BROADCAST_SEND,"\0");
					if (send_struct(&msgstruct,fds[1].fd)==0){
						noeudActuel=initNoeud(noeudActuel,"Problème, send_strcut de msgall\n");
					}
					if (send_msg(&msgstruct,fds[1].fd,buff+8)==0){
						noeudActuel=initNoeud(noeudActuel,"Problème, send_msg de msgall\n");
					}
				}
			}

			if (strstr(buff,"/help") !=NULL){
				iscommand=1;
				noeudActuel=initNoeud(noeudActuel,"Commande : /nick <pseudo> ; changement de pseudo | /who ; liste les clients | whois <pseudo> | /msg <pseudo> message | /msgall message | /quit | /create salon | /join salon | /channel_list salon | /send <pseudo> nom_fichier | /ping : ping le serveur\n");
			}

			if (strstr(buff,"/send")!=NULL){
				iscommand=1;
				if (strlen(buff)>strlen("/send a ")){
					char* target=trouve_nom(buff+strlen("/send "));
					if (is_valid_nickname(target)==1){
						int fd_file=open(buff+strlen("/send ")+strlen(target)+1, O_RDONLY);
						if (fd_file != -1){
							struct message msgstruct=init_msgstruct(infos->nickname,buff+strlen("/send ")+strlen(target)+1,FILE_REQUEST,target);
							strncpy(infos->nom_fichier,buff+strlen("/send ")+strlen(target)+1,NICK_LEN);
							noeudActuel=initNoeud(noeudActuel,infos->nom_fichier);
							update_display(display_win,noeudActuel);

							if (send_struct(&msgstruct,fds[1].fd)==0){
								noeudActuel=initNoeud(noeudActuel,"Problème, send_strcut de send fichier\n");
							}
							if (send_msg(&msgstruct,fds[1].fd,infos->nom_fichier)==0){
								noeudActuel=initNoeud(noeudActuel,"Problème, send_msg de sen de fichier\n");
							}
						} else {noeudActuel=initNoeud(noeudActuel,"Pseudo invalide");}
						close(fd_file);
					} else {
						noeudActuel=initNoeud(noeudActuel,"Nom de fichier incorrect\n");
					}
					free(target);
				}
			}

			if (strstr(buff,"/msg ") != NULL){
				iscommand=1;
				if (strlen(buff)>strlen("/msg a ")){
					char* target=trouve_nom(buff+strlen("/msg "));
					if (is_valid_nickname(target)==1){
						if (buff[5+strlen(target)+1]!='\0'){
							struct message msgstruct=init_msgstruct(infos->nickname,buff+5+strlen(target)+1,UNICAST_SEND,target);
							if (msgstruct.pld_len==0){
								noeudActuel=initNoeud(noeudActuel,"Vous n'avez écris aucun message\n");
							} else {
								if (send_struct(&msgstruct,fds[1].fd)==0){
									noeudActuel=initNoeud(noeudActuel,"Problème, send_strcut de msg\n");
								}
								if (send_msg(&msgstruct,fds[1].fd,buff+strlen("/msg ")+strlen(target)+1)==0){
									noeudActuel=initNoeud(noeudActuel,"Problème, send_msg de msg\n");
								}
							}
						}
					} else {noeudActuel=initNoeud(noeudActuel,"Pseudo invalide");}
					free(target);
				} else {noeudActuel=initNoeud(noeudActuel,"format de /msg incorrect\n");}
				update_display(display_win,noeudActuel);
			}

			if (strncmp(buff, "/ping", strlen("/ping")) == 0){
				iscommand = 1;
				if (send_echo(infos->nickname,fds[1].fd,"ping") == 0){
					noeudActuel=initNoeud(noeudActuel,"Problème au niveau de send_echo\n");
				}
			}

			if (strncmp(buff, "/create", strlen("/create")) == 0){
				iscommand = 1;
				//récupération info après "/create "
				char* start = strstr(buff," ")+1;
				char result[NICK_LEN];
				memset(result,0,NICK_LEN);
				strncpy(result, start,NICK_LEN);
				//init msgstruct
				struct message msgstruct=init_msgstruct(infos->nickname,"\0",MULTICAST_CREATE,result);
				if (send_struct(&msgstruct,fds[1].fd)==0){
					noeudActuel=initNoeud(noeudActuel,"Erreur msgstruct create\n");
				}
				memset(infos->salon,0,NICK_LEN);
				strncpy(infos->salon,result,strlen(result));
			}

			if (strncmp(buff, "/join", strlen("/join")) == 0){
				iscommand = 1;
				//récupération info après "/join "
				if (strlen(buff)>strlen("/join ")){
					char* start = strstr(buff," ")+1;
					if (*start!=' '){
						char result[NICK_LEN];
						memset(result,0,NICK_LEN);
						strncpy(result, start,NICK_LEN);
						//init msgstruct
						struct message msgstruct=init_msgstruct(infos->nickname,"\0",MULTICAST_JOIN,result);
						if (send_struct(&msgstruct,fds[1].fd)==0){
							noeudActuel=initNoeud(noeudActuel,"Erreur msgstruct join\n");
						}
						memset(infos->salon,0,NICK_LEN);
						strncpy(infos->salon,result,strlen(result));
					}
				}
			}

			if (strncmp(buff, "/channel_list", strlen("/channel_list")) == 0){
				iscommand=1;
				struct message msgstruct=init_msgstruct(infos->nickname,"\0",MULTICAST_LIST,"\0");
				if (send_struct(&msgstruct,fds[1].fd)==0){
					noeudActuel=initNoeud(noeudActuel,"Erreur msgstruct channel_list\n");
				}
			}

			if (strncmp(buff, "/quit", strlen("/quit")) == 0) {
				if (strcmp(infos->salon, "all") == 0){
					noeudActuel=initNoeud(noeudActuel,"Tu vas être déconnecté\n");
					update_display(display_win,noeudActuel);
					break;					
				}
				else {
					iscommand = 1;
					struct message msgstruct=init_msgstruct(infos->nickname,"\0",MULTICAST_QUIT,"all");
					if (send_struct(&msgstruct,fds[1].fd)==0){
						noeudActuel=initNoeud(noeudActuel,"Erreur msgstruct quit\n");
					}
					memset(infos->salon,0,NICK_LEN);
					strncpy(infos->salon,"all",4);
				}
			}

			if (iscommand == 0){
				//init msgstruct
				struct message msgstruct=init_msgstruct(infos->nickname,buff,MULTICAST_SEND,infos->salon);
				/*    //récupération info après "/salon "
				char* start = strstr(buff," ")+1;
				char result[MSG_LEN];
				memset(result,0,MSG_LEN);
				strncpy(result, start,MSG_LEN); */
				if (send_struct(&msgstruct,fds[1].fd)==0){
					noeudActuel=initNoeud(noeudActuel,"Erreur msgstruct salon\n");
				}
				if (send_msg(&msgstruct,fds[1].fd,buff)==0){
					noeudActuel=initNoeud(noeudActuel,"Erreur sendmsg salon\n");
				}
			}
    						
        }

        if (fds[1].revents & POLLIN) { //le client reçois un message du server
            // Cleaning memory
            memset(buff, 0, MSG_LEN);

			struct message msgstruct;
			memset(&msgstruct, 0, sizeof(struct message));;
           
			// Receiving structure
			int recu=0;
			int ret=0;
			do {	
				ret = recv(fds[1].fd, (char *)&msgstruct+recu, sizeof(struct message)-recu, 0);
				if (ret <= 0) {
					noeudActuel=initNoeud(noeudActuel,"Le serveur est inaccessible\n");
					update_display(display_win,noeudActuel);
					//printf("Tu ne reçois rien du server\n");
					return;
				}
				else {
					recu += ret;
				}
			} while (recu != sizeof(struct message));

			//printf("pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
			char temp_aff[1024];
			sprintf(temp_aff,"pld_len: %i / nick_sender: %s / type: %s / infos: %s\n", msgstruct.pld_len, msgstruct.nick_sender, msg_type_str[msgstruct.type], msgstruct.infos);
			noeudActuel=initNoeud(noeudActuel,temp_aff);

			// Receiving message from server
			recu=0;
			ret=0;
			do {	
				ret = recv(fds[1].fd, buff+recu, msgstruct.pld_len-recu, 0);
				if (ret <= 0) {
					noeudActuel=initNoeud(noeudActuel,"Le serveur est inaccessible\n");
					update_display(display_win,noeudActuel);
					//printf("Tu ne reçois rien du server\n");
					return;
				}
				else {
					recu += ret;
					if (strchr(buff, '\n') != NULL) {
						break; //si on trouve '\n' on sort du whilen on a reçu tout le message
					}
				}
			} while (recu != msgstruct.pld_len);

			char temp_buff[MSG_LEN+NICK_LEN+3];
			memset(temp_buff,0,MSG_LEN+NICK_LEN+3);

			if (msgstruct.type==FILE_ACCEPT){
				//noeudActuel=initNoeud(noeudActuel,"Je suis rentré dans le if file accept\n");
				update_display(display_win,noeudActuel);
				char addr_new[NICK_LEN];
				memset(addr_new,0,NICK_LEN);
				int head=0;
				while (buff[head]!=':'){
					addr_new[head]=buff[head];
					if (buff[head]=='\0'){
						break;
					}
					head++;
				}
				addr_new[head]='\0';
				noeudActuel=initNoeud(noeudActuel,"Port:adresse de connection pour la transmission du fichier ");
				noeudActuel=initNoeud(noeudActuel,addr_new);
				update_display(display_win,noeudActuel);

				fds[2].fd=handle_connect(addr_new,buff+head+1);
				struct stat stat_fichier;
				if (stat(infos->nom_fichier, &stat_fichier) == -1) {
        			noeudActuel=initNoeud(noeudActuel,"Erreur lors de la récupération des informations sur le fichier");
					update_display(display_win,noeudActuel);
        			//return 1; 
    			} else {
					off_t taille_fichier=stat_fichier.st_size;
					int file_fd = open(infos->nom_fichier, O_RDONLY);
					if (file_fd == -1) {
						noeudActuel=initNoeud(noeudActuel,"Erreur lors de l'ouverture du fichier");
						update_display(display_win,noeudActuel);
					} else {
						char chunk[MSG_LEN];
						size_t alire=MSG_LEN;
						if (taille_fichier<MSG_LEN){
							alire=taille_fichier;
						}
						ssize_t bytes_read;
						ssize_t tot_read;
						while ((bytes_read = read(file_fd, chunk, alire)) > 0) {
							//ssize_t bytes_sent = 0;
							tot_read+=bytes_read;
							if (taille_fichier-tot_read<MSG_LEN){
								alire=taille_fichier-tot_read;
							}
							noeudActuel=initNoeud(noeudActuel,"Je vais envoyer ce que j'ai lu\n");
							update_display(display_win,noeudActuel);
							struct message msgstruct=init_msgstruct(infos->nickname,chunk,FILE_SEND,infos->nom_fichier);
							if (send_struct(&msgstruct,fds[2].fd)==0){
								noeudActuel=initNoeud(noeudActuel,"erreur envoie struct fds[2].fd\n");
							}
							if (send_msg(&msgstruct,fds[2].fd,chunk)==0){
								noeudActuel=initNoeud(noeudActuel,"erreur envoie struct fds[2].fd\n");
							}
							noeudActuel=initNoeud(noeudActuel,"J'ai envoyé ce que j'ai lu\n");
							update_display(display_win,noeudActuel);
							memset(chunk,0,MSG_LEN);
						}
					}
				}
				
			}

			if (msgstruct.type==FILE_REQUEST){
				char temp_addrport[NICK_LEN];
				int head=0;
				while(buff[head]!='\0'){
					temp_addrport[head]=buff[head];
					head++;
				}
				sprintf(temp_buff,"%s souhaite vous envoyer le fichier %s, acceptez vous ? [Y/N]\n",msgstruct.nick_sender,temp_addrport);
				noeudActuel=initNoeud(noeudActuel,temp_buff);
				update_display(display_win,noeudActuel);
				update_input(input_win,"Accept : ");
				char result[2]={'\0','\0'};
				wgetnstr(input_win, result, 1);
				if (result[0]!='Y'){
					noeudActuel=initNoeud(noeudActuel,"Vous avez rejetez le fichier\n");
					update_display(display_win,noeudActuel);
					struct message msgstruct_reject=init_msgstruct(infos->nickname,"\0",FILE_REJECT,msgstruct.nick_sender);
					if (send_struct(&msgstruct_reject,fds[1].fd)==0){
						noeudActuel=initNoeud(noeudActuel,"Problème, send_strcut de FILE_REJECT\n");
					}

				} else {
					noeudActuel=initNoeud(noeudActuel,"Vous avez acceptez le fichier\n");
					update_display(display_win,noeudActuel);
					
					struct sockaddr_in local_addr;
					socklen_t addr_len=sizeof(local_addr);
					char ip[INET_ADDRSTRLEN];
                    memset(ip,0,INET_ADDRSTRLEN);
					
					if (getsockname(fds[1].fd, (struct sockaddr *)&local_addr, &addr_len) == -1) {
       					perror("getsockname");
       					//return 1;
    				}
					strcpy(ip,inet_ntoa(local_addr.sin_addr));

					fds[3].fd=handle_bind("0",ip); //welcome sock
					if ((listen(fds[3].fd, 2)) != 0) {   //etape listen
						perror("listen()\n");
						exit(EXIT_FAILURE);
					}
					
					char addrport_buff[MSG_LEN];
					if (getsockname(fds[3].fd, (struct sockaddr *)&local_addr, &addr_len) == -1) {
       					perror("getsockname");
       					//return 1;
    				}
					//inet_ntop(AF_INET, &local_addr.sin_addr, ip, INET_ADDRSTRLEN);
					sprintf(addrport_buff,"%s:%d",ip,ntohs(local_addr.sin_port));
					noeudActuel=initNoeud(noeudActuel,addrport_buff);
					struct message msgstruct_accept=init_msgstruct(infos->nickname,addrport_buff,FILE_ACCEPT,msgstruct.nick_sender);
					
					if (send_struct(&msgstruct_accept,fds[1].fd)==0){
						noeudActuel=initNoeud(noeudActuel,"Problème, send_struct de FILE_ACCEPT\n");
					}
					if (send_msg(&msgstruct_accept,fds[1].fd,addrport_buff)==0){
						noeudActuel=initNoeud(noeudActuel,"Problème, send_msg de FILE_ACCEPT\n");
					}
					update_display(display_win,noeudActuel);
					//fds[2].revents=0;					 	
				} 

			} else {
				snprintf(temp_buff,MSG_LEN+NICK_LEN+3,"%s : %s\n",msgstruct.nick_sender, buff);
				noeudActuel=initNoeud(noeudActuel,temp_buff);
				update_display(display_win,noeudActuel);
			}
			fds[1].revents=0;
		}

		if ((fds[3].revents & POLLIN)==POLLIN){
			struct sockaddr_in temp_sock_in;
			socklen_t taille_temp_sock;
			fds[2].fd=accept(fds[3].fd,(struct sockaddr*)&temp_sock_in,&taille_temp_sock);
		}
			
		if (fds[2].revents & POLLIN) {  //reçoit un fichier ou un ack
			char buff[MSG_LEN];
			memset(buff, 0, MSG_LEN);
			struct message msgstruct;
			memset(&msgstruct, 0, sizeof(struct message));;
		
			// Receiving structure
			int recu=0; int ret=0;
			do {	
				ret = recv(fds[2].fd, (char *)&msgstruct+recu, sizeof(struct message)-recu, 0);
				if (ret <= 0) {
					noeudActuel=initNoeud(noeudActuel,"Client inaccessible\n");
					update_display(display_win,noeudActuel);
				}
				else {
					recu += ret;
				}
			} while (recu != sizeof(struct message));

			if (msgstruct.type==FILE_ACK){
				noeudActuel=initNoeud(noeudActuel,"Le Destinataire a reçu le fichier");
				close(fds[2].fd);
				fds[2].fd=-1;
			}

			if (msgstruct.type==FILE_SEND){
				int fd_doc = open(msgstruct.infos, O_WRONLY | O_APPEND | O_CREAT, 0644);

				if (fd_doc == -1) {
					noeudActuel=initNoeud(noeudActuel,"Erreur d'ouverture du fichier");
				} else {

					// Receiving fichier
					recu=0; ret=0;
					do {
						ret = recv(fds[2].fd, buff+recu, msgstruct.pld_len-recu, 0);
						if (ret <= 0) {
							noeudActuel=initNoeud(noeudActuel,"Le serveur est inaccessible\n");
							update_display(display_win,noeudActuel);
							//printf("Tu ne reçois rien du server\n");
							return;
						}
						else {
							recu += ret;
							
						}
					} while (recu != msgstruct.pld_len);

					ssize_t bytes_written = write(fd_doc, buff, msgstruct.pld_len);
					if (bytes_written == -1) {
						perror("Erreur lors de l'écriture dans le fichier");
						close(fd_doc);  // Fermer le fichier avant de quitter en cas d'erreur
						//return 1;
					}
					if (msgstruct.pld_len<MSG_LEN){
						struct message msgstruct_ack=init_msgstruct(infos->nickname,"\0",FILE_ACK,msgstruct.infos);
						send_struct(&msgstruct_ack,fds[2].fd);
						close(fd_doc);
						close(fds[2].fd);
						fds[2].fd=-1;
					}
				}
			}		
		}
    }
}


int main(int argc, char *argv[]) {

	char *server_port;
	char *server_addr;
	if (argc>2){
		server_port=argv[2];
		server_addr=argv[1];
	}
	else {
		server_port=SERV_PORT;
		server_addr=SERV_ADDR;
	}

	int sfd;
	sfd = handle_connect(server_addr,server_port); //socket 

	struct pollfd fds[4];
    fds[0].fd = STDIN_FILENO; //entrée standard
	fds[1].fd = sfd;
	fds[2].fd=-1;
	fds[3].fd=-1; //welcome sock
	for (int i=0; i<4; i++){
		fds[i].events = POLLIN; 
		fds[i].revents=0; 
	}

	//Initialisation des fenêtres
	initscr();
    
    getmaxyx(stdscr, height, width);
    
	display_height = height - 5; // Zone d'affichage
    input_height = 3; // Zone de saisie
	zone_width=width - 2;
    
    WINDOW *display_win = newwin(display_height, zone_width, 1, 1);
    mvwprintw(display_win, 0, 1, "Zone d'affichage\n");
    wrefresh(display_win);

    // Créer ou redimensionner la zone de saisie
    WINDOW *input_win = newwin(input_height, zone_width, display_height + 2, 1);	
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 1, "Zone de saisie\n");
    wrefresh(input_win);


	char oldname[NICK_LEN];
	char newname[NICK_LEN];
	memset(oldname, 0, NICK_LEN); //mise à zero
	oldname[NICK_LEN-1]='\0';

	struct ListMsg* noeudActuel=cree_noeud(); //initialisation de la liste chaine
		
	int isvalide=0;
	while (isvalide==0){
		update_input(input_win,"Nouveau pseudo : ");
		wgetnstr(input_win, newname, NICK_LEN);
		isvalide=nick(oldname,newname,sfd,noeudActuel,display_win);
		update_display(display_win,noeudActuel);
	}
	
	struct info_client* infos=(struct info_client*)malloc(sizeof(struct info_client));
	memset(infos,0,sizeof(struct info_client));
	strncpy(infos->nickname,newname,strlen(newname));

	boucle_client(infos,fds,noeudActuel,input_win,display_win); //boucle message
	free(infos);
	//procédure pour désallouer la mémoire alloué aux message
	while(noeudActuel->prochainNoeud != NULL){
		noeudActuel=noeudActuel->prochainNoeud;
	}
	while(noeudActuel->ancienNoeud != NULL){
		noeudActuel=noeudActuel->ancienNoeud;
		free(noeudActuel->prochainNoeud);
	}
	delwin(display_win);
    delwin(input_win);
    endwin();

	close(sfd);
	return EXIT_SUCCESS;
}

