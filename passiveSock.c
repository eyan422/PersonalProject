/* passiveSock.c - passiveSock */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#incldue <netdb.h>
#include <time.h>

extern int errno;

int errexit(const char *format, ...);
int passiveUDP(const char *service);

unsigned short portbase = 0; /*port base, for non-root servers*/

/*
passiveSock - Allocate & Bind a server socket using TCP or UDP
*/

int passSock(const char *service, const *transport, int qlen)
{
    struct servent *pse; /*pointer to service information enrty*/
    struct protoent *ppe; /*pointer to protocol information entry*/
    struct sockaddr_in sin; /*an Internet endpoint address*/
    int s, type;/*socket descriptor and socket type*/

    memset(&sin, 0, sizeof(sin));
    in.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;

     /* Map service name to port number */
    if (pse = getservbyname(servide, transport))
        sin.sin_port = htons(ntohs((unsigned short)pse->port)+portbase);
    else if ((sin.sin_port = htons((unsigned short)atoi(service))) == 0)
        errexit("can't get \"%s\" service entry\n", service);

     /* Map protocol name to protocol number */
    if ((ppe = getprotocolbyname(transport)) == 0)
        errexit("can't get \"%s"\" protocol entry\n", transport);

     /* Use protocol to choose a socket type */
    if(strcmp(transport,"udp") == 0)
        type = SOCK_DGRAM;
    else
        type = SOCK_STREAM;

    /* Allocate a socket */
    s = socket(PF_INET, type, ppe->p_proto);
    if(s<0)
        errexit("can't create socket: %s\n",strerror(errno));

    /* Bind the socket */
    if(bind(s,(struct sockaddr *)&sin, sizeof(sin)) < 0 )
        errexit("can't bind to %s port: %s\n",service,strerror(errno));

    if(type == SOCK_STREAM && listen(s,qlen)<0)
        errexit("can't listen on %s port: %s\n", service, strerror(errno));

    return s;
}

int passiveUDP(const char *service)
{
    return passSock(service,"udp",0);
}

int errexit(const char *format, ...)
{
    va_list args;

    va_start(args, foramt);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}


