#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define SERV_PORT 4000
#define MAXLINE 1024
#define ERR_EXIT(m) \
        do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)

typedef struct sockaddr SA;
void dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen)
{
    int     n;
    char    sendline[MAXLINE], recvline[MAXLINE + 1];
/////////////////////////////////////////////////////////////////////////
    struct sockaddr_in  servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, "192.168.2.103", &servaddr.sin_addr);
/////////////////////////////////////////////////////////////////////////////
    connect(sockfd, (SA *) pservaddr, servlen);

    while (fgets(sendline, MAXLINE, fp) != NULL) {

        n = write(sockfd, sendline, strlen(sendline));
        //n = sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen);
        //n = sendto(sockfd, sendline, strlen(sendline), 0, &servaddr, sizeof(servaddr));
        //n = sendto(sockfd, sendline, strlen(sendline), 0, NULL, 0);
        if (n == -1)
        {
            if (errno == EISCONN)
                ERR_EXIT("sendto");
            else
                perror("sendto huangcheng");
        }


        //struct sockaddr_in preply_addr;
        //socklen_t addrlen;
        n = read(sockfd, recvline, MAXLINE);
        //n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);
        //n = recvfrom(sockfd, recvline, MAXLINE, 0, (SA*)&preply_addr, &addrlen);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            ERR_EXIT("recvfrom");
        }
        //printf("reply from %s \n",inet_ntoa(preply_addr.sin_addr));
        recvline[n] = 0;    /* null terminate */
        fputs(recvline, stdout);
    }
}

int main(int argc, char **argv)
{
    int                 sockfd;
    struct sockaddr_in  servaddr;

    if (argc != 2)
        ERR_EXIT("usage: udpcli <IPaddress>");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    dg_cli(stdin, sockfd, (SA *) &servaddr, sizeof(servaddr));

    exit(0);
}
