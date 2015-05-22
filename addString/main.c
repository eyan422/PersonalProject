#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define RC_FAIL     -1
#define RC_SUCCESS  0

static int IsNumber(char *pcpNum);
static int AddString( char *pcpDest, char *pcpSrc, int ipDigit );

int main()
{
    char pclFirst[126] = "11111111119999";
    char pclSecond[126] = "321";

    AddString(pclFirst,"1",2);
    printf("1-pclSecond<%s>\n",pclFirst);

    /*
    AddString(pclSecond,pclFirst,2);
    printf("2-pclSecond<%s>\n",pclSecond);

    AddString(pclSecond,pclFirst,3);
    printf("3-pclSecond<%s>\n",pclSecond);

    strcpy(pclFirst,"12a");
    strcpy(pclSecond,"23d");
    AddString(pclSecond,pclFirst,3);
    printf("4-pclSecond<%s>\n",pclSecond);
    */
    return RC_SUCCESS;
}

static int IsNumber(char *pcpNum)
{
    if ( NULL == pcpNum || strlen(pcpNum) < 1 )
    {
        printf("pcpNum<%s> is null or less than 1\n",pcpNum);
        return RC_FAIL;
    }

    while ('\0' != *pcpNum)
    {
        //if ((*pcpNum < '0') || (*pcpNum > '9'))
        if(!isdigit(*pcpNum))
        {
            printf("pcpNum<%s> is invalid number\n",pcpNum);
            return RC_FAIL;
        }

        pcpNum++;
    }
    return RC_SUCCESS;
}

static int AddString( char *pcpDest, char *pcpSrc, int ipDigit )
{
    char pclFrom[256];
    char pclTo[256];
    int ilLenSrc = 0;
    int ilLenTo = 0;
    int foo = 0;
    int ilInc = RC_FAIL;
    int ilASC = 48;

    if ( NULL == pcpDest || NULL == pcpSrc || ipDigit < 1 )
    {
        printf("pcpDest<%s> is null or less than 1\n",pcpDest);
        return RC_FAIL;
    }

    if ( RC_FAIL == IsNumber(pcpDest) || RC_FAIL == IsNumber(pcpSrc) )
    {
        printf("pcpDest<%s> or pcpSrc<%s> is invalid number\n",pcpDest,pcpSrc);
        return RC_FAIL;
    }

    memset(pclTo, '0', sizeof pclTo);

    ilLenSrc = (strlen(pcpSrc) > ipDigit) ? (strlen(pcpSrc)-ipDigit) : 0;
    strcpy(pclFrom, &pcpSrc[ilLenSrc]);
    ilLenSrc = strlen(pclFrom);

    ilLenTo = (strlen(pcpDest) < ipDigit) ? (ipDigit-strlen(pcpDest)) : 0;
    strcpy(&pclTo[ilLenTo], pcpDest);
    ilLenTo = strlen(pclTo);

    for ( foo = ilLenSrc-1; foo >= 0 && (ilLenSrc-foo-1)< ipDigit; foo-- )
    {
        ilLenTo--;
        pclTo[ilLenTo] = pclTo[ilLenTo] + pclFrom[foo] - ilASC;

        if ( RC_SUCCESS == ilInc )
        {
            pclTo[ilLenTo] += 1;
        }

        if ( (pclTo[ilLenTo]) > '9' )
        {
            pclTo[ilLenTo] -= 10;
            ilInc = RC_SUCCESS;
        }
        else
        {
            ilInc = RC_FAIL;
        }
    }

    if ( RC_SUCCESS == ilInc && (strlen(pclTo)-(ilLenTo--)) < ipDigit )
    {
        pclTo[ilLenTo] = ('9' == pclTo[ilLenTo]) ? '0' : pclTo[ilLenTo]+1;
    }
    strcpy(pcpDest, pclTo);

    return RC_SUCCESS;
}
