#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <ctype.h>

#include "common.h"

int handle_bind(char *server_port, char *server_addr);

struct message init_msgstruct(char* nick_sender,char* buff,enum msg_type type,char* infos);

char* ip_sock(int sockfd);

// Création d'un nouveau noeud
struct NoeudSock* cree_noeud();

// Recherche un noeud correspondant au descripteur de fichier
struct NoeudSock* find_noeud(int fd, struct NoeudSock* noeud);

// Trouve le descripteur de fichier correspondant à un pseudo
int from_name_find_fd(char* nickname, struct NoeudSock* noeud);

// Recherche le parent du noeud correspondant au descripteur de fichier
struct NoeudSock* find_noeud_parent(int fd, struct NoeudSock* noeud);

// Affiche la liste des noeuds dans le buff, envoie au client par sockfd
int affiche_list(int sockfd, char* nickname, struct NoeudSock* premierNoeud);

// Envoie 
int send_general(int sockfd, char* nick_sender,char* buff,enum msg_type type,char* infos);
//int send_echo(int sockfd, char* buff);

// Envoie un message unicast
int send_unicast(char* nicknameSender, char* nicknameTarget, struct NoeudSock* premierNoeud, char* buff);

int send_reject(int sockfd,char* nickname);

// Envoie un avertissement au client
int send_warning(int sockfd);

// Envoie un message en broadcast
int send_broadcast(char* nickname, struct NoeudSock* premierNoeud, char* buff);

// Envoie la structure et le message au client
int send_struct_and_msg(int sockfd, struct message* p_msgstruct, char* buff);

// Envoie les informations "whois" d'un utilisateur
int whois(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud);

// Définit le pseudo d'un utilisateur, envoie un message d'approbation ou de rejet
int nick(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud);


int send_salon(struct message* p_msgstruct, struct NoeudSock* premierNoeud, char* buff);

int estAlphaNumeriqueSansEspace(const char *chaine);

int create(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud, int affectation);

void ajouter_chaine(char ***tableau, int *taille, const char *chaine);

void liberer_tableau(char **tableau, int taille);

int list(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud);

int join(int sockfd, struct message* p_msgstruct, struct NoeudSock* premierNoeud);


