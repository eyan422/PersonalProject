#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RC_FAIL -1
#define RC_SUCCESS 0

#define MAX_LINES 4000

typedef struct
{
    char pclLine[256];
}_LINE;

typedef struct {
    _LINE rlLine[MAX_LINES];
} _FILE;

typedef struct
{
    int ilRow;
    int ilCol;
}_SPEC;

_FILE rgFile;
_SPEC rgSpec;
static int igTotalLineOfFile = 0;

static char **pcgArray = NULL;

static int checkInput();
static int readSpreadsheet(char *pcpFileName, _FILE *rgFile);
static void storeFile(_LINE *rpLine, char *pcpLine);
static void showFile(_FILE *rgFile, int igTotalLineOfFile);
static void buildArray(char *pcpParameter, _SPEC *rpSpec);
static void initArray(_SPEC rpSpec);

static int checkInput(int argc, char *argv[])
{
    char *pclFunc = "checkInput";

    if (argc < 2)
    {
        printf("%s The argc<%d> is less than 2, exit",pclFunc, argc);
        return RC_FAIL;
    }
    else
    {
        printf("%s The argc is <%d>, filename is <%s>, exit",pclFunc, argc, argv[1]);
    }

    return RC_SUCCESS;
}

static void storeFile(_LINE *rpLine, char *pcpLine)
{
    /*char *pclFunc = "storeFile";*/

    memset(rpLine,0x00,sizeof(rpLine));
    strcpy(rpLine->pclLine,pcpLine);
}

static void showFile(_FILE *rpFile, int ipTotalLineOfRule)
{
    char *pclFunc = "showRule:";
    int ilCount = 0;

    printf("%s There are <%d> lines, the comment are not shown:\n\n", pclFunc, ipTotalLineOfRule);
    for (ilCount = 0; ilCount < ipTotalLineOfRule; ilCount++)
    {
        printf("%s Line[%d] <%s>\n",pclFunc, ilCount, rpFile->rlLine[ilCount].pclLine);
    }
}

static int readSpreadsheet(char *pcpFileName, _FILE *rgFile)
{
    int ilNoLine = 0;
    char *pclFunc = "readSpreadsheet";
    char pclLine[2048];
    FILE *fp;

    if ((fp = (FILE *) fopen(pcpFileName, "r")) == (FILE *) NULL)
    {
        printf("%s Text File <%s> does not exist", pclFunc, pcpFileName);
        return RC_FAIL;
    }

    while (fgets(pclLine, 2048, fp))
    {
        /*Getting the line one by one from text file*/
        pclLine[strlen(pclLine) - 1] = '\0';
        if (pclLine[strlen(pclLine) - 1] == '\012' || pclLine[strlen(pclLine) - 1] == '\015')
        {
            pclLine[strlen(pclLine) - 1] = '\0';
        }

        /*Ignore comment*/
        if (pclLine[0] == '#')
        {
            printf("%s Comment Line -> Ignore & Continue", pclFunc);
            continue;
        }

		if ((pclLine[0] == ' ') || (pclLine[0] == '\n') || strlen(pclLine) == 0 )
        {
            continue;
        }

        storeFile( &(rgFile->rlLine[ilNoLine++]), pclLine);
    }
    igTotalLineOfFile = ilNoLine;
    showFile(rgFile, igTotalLineOfFile);

    return RC_SUCCESS;
}

static void buildArray(char *pcpParameter, _SPEC *rpSpec)
{
    int ilCount = 0;
    int ilRow = 0;
    int ilCol = 0;
    char *pclFunc = "buildArray";

    ilRow = pcpParameter[2] - '0';
    ilCol = pcpParameter[0] - '0';
    printf("\n%s pcpParameter<%s> ilRow<%d> ilCol<%d>",pclFunc, pcpParameter, ilRow, ilCol);

    (*rpSpec).ilRow = ilRow;
    (*rpSpec).ilCol = ilCol;

    /*build two dimention array*/
    pcgArray = (char **) malloc (sizeof(char*) * ilRow);
    for(ilCount = 0; ilCount < ilCol; ilCount++)
    {
        pcgArray[ilCount] = (char *)malloc( sizeof(char) * ilCol);
    }

    //memset(pcgArray,0,sizeof(pcgArray));
}

static void initArray(_SPEC rpSpec)
{
    int ilRowCount = 0;
    int ilColCount = 0;
    int ilLineCount = 1;

    char *pclFunc = "initArray";

    printf("\n");

    for(ilRowCount = 0; ilRowCount < rpSpec.ilRow; ilRowCount++)
    {
        for(ilColCount = 0; ilColCount < rpSpec.ilCol; ilColCount++)
        {
            strcpy( pcgArray[ilRowCount], rgFile.rlLine[ilLineCount++].pclLine);

            printf("%s pcgArray[%d] <%s>\n",pclFunc, ilRowCount, pcgArray[ilRowCount]);
        }
    }
}
#endif
