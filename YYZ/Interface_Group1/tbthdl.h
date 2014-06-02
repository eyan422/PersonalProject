/*fya v0.1*/

#define GROUPNUMER  16
#define TIMEFORMAT  16
#define LISTLEN     2048
#define MAX_GROUP_LINES 4000

typedef struct
{
    char pclActive[8];              /*ACTIVE*/
    char pclRuleGroup[8];           /*RulGrp*/

    char pclSourceTable[8];         /*SrcTab*/
    char pclSourceKey[8];           /*SrcKey*/
    char pclSourceField[8];         /*SrcFld*/
    //char pclSourceFieldValue[126];  /*SrcFldVal*/
    char pclSourceFieldType[8];     /*SrcTyp*/

    char pclDestTable[8];           /*Destab*/
    char pclDestKey[8];             /*DesKey*/
    char pclDestField[8];           /*Desfld*/
    //char pclDestFieldValue[126];    /*DesfldVal*/
    char pclDestFieldLen[8];        /*Deslen*/
    char pclDestFieldType[8];       /*Destyp*/

    char pclDestFieldOperator[8];   /*Operate*/
    char pclCond1[126];      /*Con1*/
    char pclCond2[126];      /*Con2*/
}_LINE;

typedef struct {
    _LINE rlLine[MAX_GROUP_LINES];
} _RULE;

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
