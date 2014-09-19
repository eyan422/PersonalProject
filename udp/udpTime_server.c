/* UDPtime.c - main */

#include "passiveSock.c"

#define UNIXEPOCH 220898800UL

int main(int agrc, char *argv[])
{
    struct sockaddr_in fsin;
    char *service="TEST";
    char buf[1];
    int sock;
    time_t now;
    unsigned int alen;

    //printf("agrc<%d>\n",agrc);

    switch(agrc)
    {
        case 1:
            break;
        case 2:
            service = argv[1];
            break;
        default:
            errexit("usage: UDPtimeed[port]\n");
    }

    sock = passiveUDP(service);

    printf("socket<%d>\n",sock);

    while(1)
    {
        alen = sizeof(fsin);
        if(recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr *)&fsin, &alen)<0)
            errexit("recvfrom: %s\n",strerror(errno));
        else
            printf("Read into buf<%s>\n",buf);

        (void)time(&now);
        now = htonl((unsigned long)(now+UNIXEPOCH));
        (void) sendto(sock,(char *)&now,sizeof(now),0,(struct sockaddr *)&fsin, sizeof(fsin));
    }
}
