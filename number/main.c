#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

void CopyNumber(char *pcpText);

int main()
{
    char pclText[] = "WHERE FLNU = 2139994038 AND AREA = 'A' ORDER BY LINO";

    //printf("Hello world!\n");

    CopyNumber(pclText);

    return 0;
}

void CopyNumber(char *pcpText)
{
    int i = 0;
    char pclNum[64] = "\0";

    while ('\0' != *pcpText)
    {
        if(!isdigit(*pcpText))
        {
            printf("*pcpText<%c> is invalid number\n",*pcpText);
            pcpText++;
        }
        else
        {
            printf("*pcpText<%c> is valid number\n",*pcpText);
            pclNum[i++] = *pcpText++;
        }
    }
    printf("pclNum<%s>\n",pclNum);
}
