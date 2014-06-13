/*fya v0.1*/
#define ARRAYNUMBER 16
#define GROUPNUMER  16
#define TIMEFORMAT  16
#define LISTLEN     2048
#define MAX_GROUP_LINES 4000

#define FYA

#define INSERT 9
#define UPDATE 9
#define MASTER_RECORD 0

static char pcgMasterSST[4] = "SL";

typedef struct
{
    char pclActive[64];              /*ACTIVE*/
    char pclRuleGroup[64];           /*RulGrp*/

    char pclSourceTable[64];         /*SrcTab*/
    char pclSourceKey[64];           /*SrcKey*/
    char pclSourceField[64];         /*SrcFld*/
    //char pclSourceFieldValue[126];  /*SrcFldVal*/
    char pclSourceFieldType[64];     /*SrcTyp*/

    char pclDestTable[64];           /*Destab*/
    char pclDestKey[64];             /*DesKey*/
    char pclDestField[64];           /*Desfld*/
    //char pclDestFieldValue[126];    /*DesfldVal*/
    char pclDestFieldLen[64];        /*Deslen*/
    char pclDestFieldType[64];       /*Destyp*/

    char pclDestFieldOperator[64];   /*Operate*/
    char pclCond1[128];      /*Con1*/
    char pclCond2[128];      /*Con2*/
}_LINE;

typedef struct {
    _LINE rlLine[MAX_GROUP_LINES];
} _RULE;

typedef struct {
    char pclAlc2[64];
    char pclAlc3[64];
    char pclFlno[64];
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
