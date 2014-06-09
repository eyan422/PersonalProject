#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPER_CODE 2

void print1(int x)
{
    char *pclFunc = "print1";
    printf("%s\n", pclFunc);
}

void print2(int x)
{
    char *pclFunc = "print2";
    printf("%s\n", pclFunc);
}

int main()
{
    int ilCount = 0;
    char pclOperCode[OPER_CODE][1024] = {"print1","print2"};
    void (*pflOpeFunc[OPER_CODE])() = {print1, print2};

    /*test*/

    char pclTest[1024] = "print1";

    for (ilCount = 0; ilCount < OPER_CODE; ilCount++)
    {
        if( strcmp(pclTest,pclOperCode[ilCount]) == 0 )
        {
            printf("pclOperCode[%d] - %s\n", ilCount, pclOperCode[ilCount]);
            pflOpeFunc[ilCount]();
        }
    }

    return 0;
}
