#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
        char *pclString;
}_NODE;

_NODE *rgArray[26];

int main()
{
        int ilCountRow = 0;
        int ilCountCol = 0;
        int ilCount = 0;

        //rgArray = (_NODE **)malloc(sizeof(_NODE *) * 2);
        for(ilCount = 0; ilCount < 26; ilCount++)
        {
                rgArray[ilCount] = (_NODE *)malloc(sizeof(_NODE));
        }

        strcpy(rgArray[0]->pclString,"a");
        printf("%s",rgArray[0]->pclString);

        return 0;
}
