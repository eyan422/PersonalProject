#include "Queue.h"
#include "Queue.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIX_LEN 36
#define LEN 110

static int TrimZero( char *pcpInStr );

void print(Item i)
{
    printf("The data in this node is %d\n",i);
    //dbg(TRACE,"The data in this node is %d",i);
}

int main()
{
    Queue *pq = InitQueue();
    //int i,item;
    int item;

    /*
    printf("0-9 is added into the queue and output as below:\n");

    for(i = 0; i < 10; i++)
    {
        EnQueue(pq, i);
        GetRear(pq, &item);
        printf("%d ", item);
    }
    */

    //
    int ilCount = 0;
    int ilcursor = 36;

    char *pclTmpStart = NULL;
    char pclTmp[16] = "\0";

    char *pclString_1 = "ACK_sps_20140508073602..............00000000027017........................................................ETX_";

    char *pclString_2 = "ACK_sps_20140508073602..............00000000027015........................................................ETX_ACK_sps_20140508073602..............00000000027016........................................................ETX_";

    char *pclString_3 = "ACK_sps_20140508073602..............00000000027011........................................................ETX_ACK_sps_20140508073602..............00000000027012........................................................ETX_ACK_sps_20140508073602..............00000000027013........................................................ETX_ACK_sps_20140508073602..............00000000027014........................................................ETX_";

    printf("length_1: %d\n\n",strlen(pclString_1));
    printf("length_2: %d\n\n",strlen(pclString_2));
    printf("length_3: %d\n\n",strlen(pclString_3));

    //pclTmpStart = strstr(pclString_2,"ACK_");
    //pclTmpStart = strstr(pclString_3,"ACK_");
    //

    EnQueue(pq,27011);
    EnQueue(pq,27012);
    EnQueue(pq,27013);
    EnQueue(pq,27014);

    printf("\nTraverse all element in queue:\n");
    QueueTraverse(pq,print);

    int m = GetSize(pq);
    printf("\nSize: %d\n", m);

    pclTmpStart = strstr(pclString_3,"ACK_");
    for (ilCount = 0; ilCount < (strlen(pclString_3) / LEN); ilCount++)
    {
            memset(pclTmp,0,sizeof(pclTmp));
            if (ilCount == 0)
            {
                    strncpy(pclTmp, pclTmpStart + ilcursor, 14);
            }
            else
            {
                    ilcursor += LEN;
                    if (ilcursor <= strlen(pclString_3))
						strncpy(pclTmp, pclTmpStart + ilcursor, 14);
            }

            TrimZero(pclTmp);
            //printf("%s\n\n",pclString_3+ilcursor);

            //printf("<%d> ilcursor <%d> \tMsgID is <%s>\n", ilCount, ilcursor, pclTmp);

            if (strlen(pclTmp) > 0 && atoi(pclTmp) != 0)
            {
                DeQueue(pq,&item);
                printf("The item in the queue is <%d>\n",item);

                if( atoi(pclTmp) == item)
                {
                    printf("The received msgid<%d> == the one from queue<%d>\n", item, atoi(pclTmp));
                }
                else
                {
                    printf("The received msgid<%d> != the one from queue<%d>\n", item, atoi(pclTmp));
                }
            }
    }

    /*
    printf("\nDequeue all element and print them out one by one:\n");
    for(i = 0; i< m; i++)
    {
        DeQueue(pq,&item);
        printf("%d ",item);
    }
    */

    ClearQueue(pq);
    if(IsEmpty(pq))
        printf("\nMake the queue empty\n");
    DestroyQueue(pq);
    printf("Destruct the queue\n");

    return 0;
}

static int TrimZero( char *pcpInStr )
{
        int ili = 0;
        int ilLen;
        char *pclOutStr = NULL;
        char *pclP;
        //char *pclFunc = "TrimZero";

        ilLen = strlen( pcpInStr );
        if( ilLen <= 0 )
                return -1;

        pclOutStr = (char *)malloc(ilLen + 10);
        pclP = pcpInStr;

        /* Trim front spaces */
        while( pclP && *pclP == '0' )
                pclP++;

        while( *pclP )
        {
                pclOutStr[ili++] = *pclP;
                pclP++;
        }

        pclOutStr[ili] = '\0';

        strcpy( pcpInStr, pclOutStr );
        if( pclOutStr != NULL )
                free( (char *)pclOutStr );

        return 0;
}
