#include "Queue.h"
#include "Queue.c"
#include<stdio.h>

void print(Item i)
{
    printf("The data in this node is %d\n",i);
    //dbg(TRACE,"The data in this node is %d",i);
}

int main()
{
    Queue *pq = InitQueue();
    int i,item;

    printf("0-9 is added into the queue and output as below:\n");

    for(i = 0; i < 10; i++)
    {
        EnQueue(pq, i);
        GetRear(pq, &item);
        printf("%d ", item);
    }

    EnQueue(pq,5801);

    printf("\nTraverse all element in queue:\n");
    QueueTraverse(pq,print);

    int m = GetSize(pq);
    printf("\nSize: %d\n", m);

    printf("\nDequeue all element and print them out one by one:\n");
    for(i = 0; i< m; i++)
    {
        DeQueue(pq,&item);
        printf("%d ",item);
    }

    ClearQueue(pq);
    if(IsEmpty(pq))
        printf("\nMake the queue empty\n");
    DestroyQueue(pq);
    printf("Destruct the queue\n");

    return 0;
}
