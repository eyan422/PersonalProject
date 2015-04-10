#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    char pclTest[] = "SELECT SECU,PSEU,APNA,SECT,SSEC,VINA,SKEY,SVAL,URNO FROM USTTAB WHERE (URNO = \'4952650\')";

    printf("pclTest<%s>\n\n",pclTest);

    char *pclPointer = strstr(pclTest,"'")+1;

    char pclDest[] = "\0";

    strcpy(pclDest,pclPointer);
    pclDest[strlen(pclDest)-2] = '\0';

    printf("pclDest<%s>\n\n",pclDest);
    return 0;
}
