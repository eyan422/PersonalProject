/*fya v0.1*/
#define ARRAYNUMBER 16
#define GROUPNUMER  16
#define TIMEFORMAT  16
#define LISTLEN     2048
#define MAX_GROUP_LINES 4000

#define FYA

#define BRUTAL 4
#define HASH 5
#define NONBLANK 2
#define INSERT 9
#define UPDATE 10
#define MASTER_RECORD 0

static char pcgCodeShareSST[4] = "SL";

typedef struct
{
    char pclActive[128];              /*ACTIVE*/
    char pclRuleGroup[128];           /*RulGrp*/

    char pclSourceTable[128];         /*SrcTab*/
    char pclSourceKey[128];           /*SrcKey*/
    char pclSourceField[128];         /*SrcFld*/
    //char pclSourceFieldValue[128];  /*SrcFldVal*/
    char pclSourceFieldType[128];     /*SrcTyp*/

    char pclDestTable[128];           /*Destab*/
    char pclDestKey[128];             /*DesKey*/
    char pclDestField[128];           /*Desfld*/
    //char pclDestFieldValue[126];    /*DesfldVal*/
    char pclDestFieldLen[128];        /*Deslen*/
    char pclDestFieldType[128];       /*Destyp*/

    char pclDestFieldOperator[128];   /*Operate*/
    char pclCond1[128];      /*Con1*/
    char pclCond2[128];      /*Con2*/
    char pclCond3[128];
}_LINE;

typedef struct {
    _LINE rlLine[MAX_GROUP_LINES];
} _RULE;

typedef struct {
    char pclAlc2[64];
    char pclAlc3[64];
    char pclFlno[64];
    char pclSuffix[64];
} _VIAL;

static void TrimRight(char *pcpBuffer) {
    char *pclBlank = &pcpBuffer[strlen(pcpBuffer) - 1];

    if (strlen(pcpBuffer) == 0) {
        strcpy(pcpBuffer, " ");
    } else {
        while (isspace(*pclBlank) && pclBlank != pcpBuffer) {
            *pclBlank = '\0';
            pclBlank--;
        }
    }
} /* End of TrimRight */

typedef struct {
    char pclInsertQuery[4096];
    char pclUpdateQuery[4096];
} _QUERY;
