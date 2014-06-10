#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char pcgVial1[4096]=" SINWSSS20121120030000                                          20121120031000                                           ";

char pcgVial2[4096]=" SINWSSS20121120030000                                          20121120031000                                           DXBOMDB20121120071500                                          20121120072000                                           ";

char pcgVial3[4096] =" SINWSSS20121120030000                                          20121120031000                                           DXBOMDB20121120071500                                          20121120072000                                           BLRVOBL20121120055500                                          20121120060000";

int main()
{
    int ilCount = 0;

    char pclBuffer[4096] = "\0";

    char *pclVial = NULL;
    //pclVial= pcgVial1;
    //pclVial= pcgVial2;
    pclVial= pcgVial3;

    strcpy(pclBuffer, pclVial+1);

    int ilNO = strlen(pclBuffer) / 121;
    char pclTmp[16] = "\0";

    printf("len<%d> ilNo<%d>\n", strlen(pclBuffer),ilNO);

    for (ilCount = 0; ilCount <= ilNO; ilCount++)
    {
        strncpy(pclTmp,pclBuffer+ilCount*120,3);
        printf("%d pclTmp<%s>\n",ilCount,pclTmp);
    }

    return 0;
}
