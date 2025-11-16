#define MSG_LEN 1024
#define SERV_PORT "9469" //9469
#define SERV_ADDR "127.0.0.1" 
#define CONMAX 25 // car SOMAXCONN est surement trop grand pour être rentré dans poll
//192.168.56.89

#include "msg_struct.h"
#include <time.h>
#include <stdio.h>

struct info{
    short s;
    long l;
};

struct NoeudSock {
    struct sockaddr_in connSock;
    struct NoeudSock* prochainNoeud;
    socklen_t tailleSock;
    char nickname[NICK_LEN];
    int connfd;
    struct tm date_heure;
    char salon[NICK_LEN];
};
