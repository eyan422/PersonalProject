#include "connectSock.c"
#include <unistd.h>

#define BUFSIZE 64
#define UNIXEPOCH 220898800UL
#define MSG "what time is it?\n"

extern int errno;

int connectUDP(const char *host, const char *service);

#define LINELEN 128

int main(int argc, char *argv[])
{
    char *host = "localhost";
    char *service = "time";
    time_t now;
    int s,n;

    switch(argc)
    {
        case 1:
            host = "localhost";
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

    s = connectUDP(host,service);

    (void) write(s,MSG,strlen(MSG));

    /* Read the time */

    n = read(s,(char *)&now, sizeof(now));
    if(n<0)
        errexit("read failed: %s\n", strerror(errno));

    now = ntohl(unsigned long)now;

    now -= UNIXEPOCH;

    printf("%s",ctime(&now));

    exit(0);
}
