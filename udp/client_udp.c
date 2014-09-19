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
    unsigned int fromLen;
    char recvBuffer[128];
    //sendto中使用的对方地址
    struct sockaddr_in toAddr;
    //在recvfrom中使用的对方主机地址
    struct sockaddr_in fromAddr;

    if(argc < 2)
    {
        //printf("请输入要传送的内容.\r\n");
        printf("Input the content:\r\n");
        exit(0);
    }

    sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(sock < 0)
    {
        //printf("创建套接字失败了.\r\n");
        printf("sockect creation fails.\r\n");
        exit(1);
    }
    else
    {
        printf("Socket is created\r\n");
    }

    memset(&toAddr,0,sizeof(toAddr));
    toAddr.sin_family=AF_INET;
    toAddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    toAddr.sin_port = htons(4000);

    if(sendto(sock,argv[1],strlen(argv[1]),0,(struct sockaddr*)&toAddr,sizeof(toAddr)) != strlen(argv[1]))
    {
        //printf("sendto() 函数使用失败了.\r\n");
        printf("sendto() fails\r\n");
        close(sock);
        exit(1);
    }
    else
    {
        printf("Send <%s> to server\r\n", argv[1]);
    }

    fromLen = sizeof(fromAddr);

    if(recvfrom(sock,recvBuffer,1024,0,(struct sockaddr*)&fromAddr,&fromLen)<0)
    {
        //printf("recvfrom()函数使用失败了.\r\n");
        printf("recvfrom() fails\r\n");
        close(sock);
        exit(1);
    }

    printf("recvfrom() result:%s\r\n",recvBuffer);
    close(sock);
}
