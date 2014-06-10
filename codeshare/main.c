#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char pclTmp[16] = "\0";
    char pcgCodeshare[4096]="AF 0405 1UA 0505 25Y 0605 3";

    int ilCount = 0;
    int ilNO = strlen(pcgCodeshare) / 9;

    printf("len<%d> ilNo<%d>\n", strlen(pcgCodeshare),ilNO);

    for (ilCount = 0; ilCount < ilNO; ilCount++)
    {
        strncpy(pclTmp,pcgCodeshare+ilCount*9,3);
        printf("%d pclTmp<%s>\n",ilCount,pclTmp);
    }

    return 0;
}
