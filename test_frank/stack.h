#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test.h"

typedef struct Mystack *Stack;

struct Mystack {
    int Capacity;       /* 栈的容量 */
    int Top_of_stack;   /* 栈顶下标 */
    double *Array;     /* 存放栈中元素的数组 */
};

/* 栈的创建 */
Stack CreateStack(int Max)
{
    Stack S;
    S = (Stack)malloc(sizeof(struct Mystack));
    if (S == NULL)
        printf("Create stack error!\n");

    S->Array = (double *)malloc(sizeof(double) * Max);
    if (S->Array == NULL)
        printf("Create stack error!\n");

    S->Capacity = Max;
    S->Top_of_stack = 0;
    return S;
}

/* 释放栈 */
void DisposeStack(Stack S)
{
    if (S != NULL)
    {
        free(S->Array);
        free(S);
    }
}

/* 判断一个栈是否是空栈 */
int IsEmpty(Stack S)
{
    return !S->Top_of_stack;
}

/* 判断一个栈是否满栈 */
int IsFull(Stack S)
{
    if (S->Top_of_stack == S->Capacity - 1)
        return 1;
    else
        return 0;
}


/* 数据入栈 */
int Push(double x, Stack S)
{
    if (IsFull(S))
        printf("The Stack is full!\n");
    else
        S->Array[S->Top_of_stack++] = (double)x;

    return 0;
}

/* 数据出栈 */
double Pop(Stack S)
{
    if (IsEmpty(S))
        printf("The Stack is empty!\n");
    else
        S->Top_of_stack--;

    return 0;
}

/* 将栈顶返回 */
double Top(Stack S)
{
    if (!IsEmpty(S))
        return S->Array[S->Top_of_stack-1];
    printf("The Stack is empty!\n");
    return 0;
}

int RPN(char *str, double *pfResult)
{
    int len;
    int ilEle = 0;
    int ilCount = 0;
    char *pclFunc = "RPN";
    char pclEle[256] = "\0";
    /*char str[100];
    printf("Please input the postfix that you want to compute: \n");
    scanf("%s", str);*/
    len = strlen(str);

    ilEle = GetNoOfElements(str,' ');

    struct Mystack *my_stack = CreateStack(ilEle+1);
    for(ilCount = 1; ilCount <= ilEle; ilCount++)
    {
        get_item(ilCount, str, pclEle, 0, " ", "\0", "\0");
        //printf("%s pclEle<%s>\n",pclFunc, pclEle);

        if (pclEle[0] == '+')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y =Top(my_stack);
            Pop(my_stack);

            //printf("+ x<%.5f>, y<%.5f>\n",x,y);
            Push(x+y, my_stack);
            //printf("%d\n", Top(my_stack));
        }
        else if (pclEle[0] == '-')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            //printf("- x<%.5f>, y<%.5f>\n",x,y);
            Push(x-y, my_stack);
            //printf("%d\n", Top(my_stack));
        }

        else if (strcmp(pclEle,"*")==0)
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            //printf("* x<%.5f>, y<%.5f>\n",x,y);
            Push(x*y, my_stack);
            //printf("%d\n", Top(my_stack));
        }
        else if (pclEle[0] == '/')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            //printf("/ x<%.5f>, y<%.5f>\n",x,y);
            double tmp = y/x;
            //printf("tmp %.5f\n",tmp);
            Push(tmp, my_stack);
            //printf("yop %.5f\n", Top(my_stack));
        }
        else
        {
            Push(atof(pclEle), my_stack);
        }
    }
    *pfResult = Top(my_stack);
    //printf("The result is: %.5f\n", *pfResult);
    Pop(my_stack);

#if 0
    /* 根据序列的长度来创建栈 */
    struct Mystack *my_stack = CreateStack(len+1);
    for (i = 0; i < len; i++)
    {
        /*if (str[i] >= '0' && str[i] <= '9')
            Push((int)str[i]-48, my_stack);
        else*/ if (str[i] == '+')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y =Top(my_stack);
            Pop(my_stack);

            printf("+ x<%.5f>, y<%.5f>\n",x,y);
            Push(x+y, my_stack);
            //printf("%d\n", Top(my_stack));
        }
        else if (str[i] == '-')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            printf("- x<%.5f>, y<%.5f>\n",x,y);
            Push(x-y, my_stack);
            //printf("%d\n", Top(my_stack));
        }

        else if (str[i] == '*')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            printf("* x<%.5f>, y<%.5f>\n",x,y);
            Push(x*y, my_stack);
            //printf("%d\n", Top(my_stack));
        }
        else if (str[i] == '/')
        {
            double x = Top(my_stack);
            Pop(my_stack);
            double y = Top(my_stack);
            Pop(my_stack);

            printf("/ x<%.5f>, y<%.5f>\n",x,y);
            Push(x/y, my_stack);
            //printf("%d\n", Top(my_stack));
        }
        else
        {
            Push((int)str[i]-48, my_stack);
        }
    }
    *pfResult = Top(my_stack);
    printf("The result is: %.5f\n", *pfResult);
    Pop(my_stack);
    /* A bug */
//  DisposeStack(my_stack);
#endif
    return 0;

}
