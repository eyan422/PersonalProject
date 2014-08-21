#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int sock;
    //sendto中使用的对方地址
    struct sockaddr_in toAddr;
    //在recvfrom中使用的对方主机地址
    struct sockaddr_in fromAddr;
    int recvLen;
    unsigned int addrLen;
    char recvBuffer[1024] = "\0";

    sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(sock < 0)
    {
        //printf("创建套接字失败了.\r\n");
        printf("sockect creation fails.\r\n");
        exit(0);
    }
    else
    {
        printf("Socket is created\r\n");
    }

    memset(&fromAddr,0,sizeof(fromAddr));
    fromAddr.sin_family=AF_INET;
    fromAddr.sin_addr.s_addr=htonl(INADDR_ANY);
    fromAddr.sin_port = htons(4000);
    if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
    {
        //printf("bind() 函数使用失败了.\r\n");
        printf("bind() fails\r\n");
        close(sock);
        exit(1);
    }
    else
    {
        printf("Socket is bined\r\n");
    }

    while(1)
    {
        memset(recvBuffer,0x00,1024);
        addrLen = sizeof(toAddr);
        if((recvLen = recvfrom( sock,recvBuffer,1024,0,(struct sockaddr*)&toAddr,&addrLen)) < 0 )
        {
            //printf("()recvfrom()函数使用失败了.\r\n");
            printf("recvfrom() fails.\r\n");
            close(sock);
            exit(1);
        }
        else
        {
            printf("recvfrom() <%s>\r\n",recvBuffer);
        }

        if(sendto(sock,recvBuffer,recvLen,0,(struct sockaddr*)&toAddr,sizeof(toAddr))!=recvLen)
        {
            printf("sendto fails\r\n");
            close(sock);
            exit(0);
        }
        else
        {
            printf("sendto() <%s>\r\n",recvBuffer);
        }
        return 0;
    }
}
