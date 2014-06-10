#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPER_CODE 10
#define OPER_LEN  64

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

struct codeFunc
{
    char pclOperCode[OPER_LEN];
    void (*pflOpeFunc)();
}
CODEFUNC[OPER_CODE] =
{
    {"print1",print1},
    {"print2",print2}
};

int main()
{
    int ilCount = 0;
    char pclTest[1024] = "print1";

    for (ilCount = 0; ilCount < OPER_CODE; ilCount++)
    {
        if( strcmp(pclTest, CODEFUNC[ilCount].pclOperCode) == 0 )
        {
            printf("pclOperCode[%d] - %s\n", ilCount, CODEFUNC[ilCount].pclOperCode);
            (CODEFUNC[ilCount].pflOpeFunc)(ilCount);
        }
    }
    return 0;
}
