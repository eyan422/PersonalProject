#include "connectSock.c"

#define BUFSIZE 64
#define UNIXEPOCH 220898800UL
char MSG[126] = "what time is it";

extern int errno;

int connectUDP(const char *host, const char *service);

#define LINELEN 128

int main(int argc, char *argv[])
{
    char *host = "DXBVM";
    char *service = "TEST";
    time_t now;
    int sock,n;

    switch(argc)
    {
        case 1:
            host = "DXBVM";
            break;
        case 3:
            service = argv[2];
            /*FALL THROUGH*/
        case 2:
            host = argv[1];
            break;
        default:
            fprintf(stderr, "usage: TCPdaytime [host[port]]\n");
            exit(1);
    }

    sock = connectUDP(host,service);

    printf("MSG <%s> socket<%d>\n",MSG,sock);

    (void) write(sock,MSG,strlen(MSG));

    printf("MSG is sent, waitting for reading",MSG);

    /* Read the time */

    n = read(sock,(char *)&now, sizeof(now));
    if(n<0)
        errexit("read failed: %s\n", strerror(errno));

    now = ntohl((unsigned long)now);

    now -= UNIXEPOCH;

    printf("%s",ctime(&now));

    exit(0);
}
