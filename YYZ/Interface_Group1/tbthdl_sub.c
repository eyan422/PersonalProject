/*
tbthdl_sub.c

For subroutine
*/

#define RC_NUMBER 7
#define OPER_CODE 64
#define OPER_LEN  64
#define MIN 60
#define VIAL_LEN 121
#define CODESHARE_LEN 9

extern int igTimeDifference;
extern void buildSelQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection);
extern int getRotationFlightData(char *pcpTable, char *pcpUrnoSelection, char *pcpFields, char (*pcpRotationData)[LISTLEN], char *pcpAdid);
extern void showRotationFlight(char (*pclRotationData)[LISTLEN]);
/*extern int getCodeShare(char *pcpFields, char *pcpData, char (*pcpCodeShare)[LISTLEN]);*/
extern int getCodeShare(char *pcpFields, char *pcpData, _VIAL *pcpCodeShare, char *pcpFormat, char *pcpOption);

int codeshareFormat(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "codeshareFormat";
    int ilJcnt = 0;
    int ilCount =0;
    char pclFields[LISTLEN] = "\0";
    char pclData[LISTLEN] = "\0";
    char pcpFormat[LISTLEN] = "\0";
    _VIAL pclCodeShare[ARRAYNUMBER];

    strncpy(pcpDestValue, "", 1);

    if(strlen(pcpSourceValue) == 0 || strncmp(pcpSourceValue," ",1) == 0 )
    {
        dbg(TRACE,"%s pcpSourceValue<%s> is invalid",pclFunc,pcpSourceValue);
        return ilRC;
    }

    /*
    build field and data list for getCodeShare function
    1-JCNT
    2-JFNO
    */
    ilJcnt = strlen(pcpSourceValue) / CODESHARE_LEN;
    dbg(DEBUG,"len<%d> ilNo<%d>\n", strlen(pcpSourceValue),ilJcnt);

    strcat(pclFields,"JCNT");
    strcat(pclFields,"JFNO");
    sprintf(pclData,"%d,%s", ilJcnt, pcpSourceValue);

    ilCount = getCodeShare(pclFields,pclData,pclCodeShare,pcpFormat,"ALC3");
    dbg(DEBUG,"%s ilCount<%d> pcpFormat<%s>",pclFunc,ilCount, pcpFormat);
    /*
    for(ili = 0; ili < ilCount; ili++)
    {
        dbg(DEBUG,"%s ili<%d>",pclFunc,ili);
        dbg(DEBUG,"%s ALC2<%s> ALC3<%s> FLNO<%s>",pclFunc,pclCodeShare[ili].pclAlc2, pclCodeShare[ili].pclAlc3, pclCodeShare[ili].pclFlno);
    }
    */
    strcpy(pcpDestValue, pcpFormat);
}

int zon(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "zon";

    strncpy(pcpDestValue, "", 1);

    if(strlen(pcpAdid) == 0 || strncmp(pcpAdid," ",1) ==0 )
    {
        dbg(TRACE,"%s pcpAdid<%s> is invalid",pclFunc,pcpAdid);
        return ilRC;
    }

    if(strncmp(pcpAdid,"A",1) != 0 && strncmp(pcpAdid,"D",1) != 0 )
    {
        dbg(TRACE,"%s pcpAdid<%s> is invalid",pclFunc,pcpAdid);
        return ilRC;
    }

    if( strncmp(pcpAdid,"A",1) == 0 )
    {
        strcpy(pcpDestValue, pcpSourceValue);
        ilRC = RC_SUCCESS;
    }

    return ilRC;
}

int getVial(char *pcpVial, char (*pcpVialArray)[LISTLEN])
{
    int ilNO=0;
    int ilCount = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "getVial";

    char *pclVial = NULL;
    char pclTmp[16] = "\0";
    char pclBuffer[LISTLEN] = "\0";

    strcpy(pclBuffer, pclVial+1);
    ilNO = strlen(pclBuffer) / VIAL_LEN ;

    dbg(TRACE,"%s len<%d> ilNo<%d>\n", pclFunc, strlen(pclBuffer), ilNO);

    for (ilCount = 0; ilCount <= ilNO; ilCount++)
    {
        strncpy(pclTmp, pclBuffer + ilCount * (VIAL_LEN - 1), 3);
        dbg(DEBUG,"%s %d pclTmp<%s>\n",ilCount,pclTmp);

        strcpy(pcpVialArray[ilCount], pclTmp);
    }

    return ilNO;
}

int viaref(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ili = 0;
    int ilCount = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "via";
    char pclTmp[1024] = "\0";
    char pclSqlBuf[1024] = "\0";
    char pclSqlData[1024] = "\0";
    char pclSelection[1024] = "\0";
    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    strncpy(pcpDestValue, "", 1);

    if(strlen(pcpSourceValue) == 0 || strncmp(pcpSourceValue," ",1) == 0 )
    {
        dbg(TRACE,"%s pcpSourceValue<%s> is invalid",pclFunc,pcpSourceValue);
        return ilRC;
    }

    ilCount = getVial(pcpSourceValue, pclVial);
    ili = (int)((rpLine->pclDestField)[3] - '0') - 1;
    if (ili > ARRAYNUMBER || ili <= 0)
    {
        dbg(TRACE,"%s ili<%d> is invalid",pclFunc,ili);
        return ilRC;
    }

    strcpy(pclTmp, pclVial[ili]);

    sprintf(pclSelection, "WHERE %s=%s", rpLine->pclCond1, pclTmp);
    buildSelQuery(pclSqlBuf, rpLine->pclSourceTable, rpLine->pclCond2, pclSelection);
    ilRC = RunSQL(pclSqlBuf, pclSqlData);
    if (ilRC != DB_SUCCESS)
    {
        dbg(TRACE, "<%s>: Retrieving source data - Fails", pclFunc);
        return RC_FAIL;
    }

    switch(ilRC)
    {
        case NOTFOUND:
            dbg(TRACE, "<%s> Retrieving source data - Not Found", pclFunc);
            ilRC = RC_FAIL;
            break;
        default:
            dbg(TRACE, "<%s> Retrieving source data - Found\n <%s>", pclFunc, pclSqlData);
            BuildItemBuffer(pclSqlData, NULL, 1, ",");
            strcpy(pcpDestValue, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

int via(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ili = 0;
    int ilCount = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "via";

    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    strncpy(pcpDestValue, "", 1);

    if(strlen(pcpSourceValue) == 0 || strncmp(pcpSourceValue," ",1) == 0 )
    {
        dbg(TRACE,"%s pcpSourceValue<%s> is invalid",pclFunc,pcpSourceValue);
        return ilRC;
    }

    /*getting the vial information*/
    ilCount = getVial(pcpSourceValue, pclVial);
    ili = (int)((rpLine->pclDestField)[3] - '0') - 1;
    strcpy(pcpDestValue,pclVial[ili]);

    ilRC = RC_SUCCESS;
    return ilRC;
}

int rotation(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilCount = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "rotation";

    char *pclTmp = NULL;
    char pclTmpSelection[64] = "\0";
    char pclUrnoSelection[64] = "\0";
    char pclRotationData[ARRAYNUMBER][LISTLEN] = {"\0"};


    dbg(DEBUG,"%s ++++++++++++++++++",pclFunc);

    strcpy(pclTmpSelection, pcpSelection);
    dbg(DEBUG,"%s pclTmpSelection<%s>",pclFunc,pclTmpSelection);

    pclTmp = strstr(pclTmpSelection, "=");

    dbg(DEBUG,"%s pclTmp<%s>",pclFunc,pclTmp);

    strcpy(pclUrnoSelection, pclTmp+1);

    dbg(DEBUG,"%s pclUrnoSelection<%s>",pclFunc,pclUrnoSelection);

    strncpy(pcpDestValue, "", 1);

    ilRC = getRotationFlightData(rpLine->pclSourceTable, pclUrnoSelection, rpLine->pclSourceField, pclRotationData, pcpAdid);
    if (ilRC == RC_SUCCESS)
    {
        showRotationFlight(pclRotationData);

        /*handle rotation data*/
        for(ilCount = 0; ilCount < ARRAYNUMBER; ilCount++)
        {
            if (strlen(pclRotationData[ilCount]) > 0)
            {
                dbg(DEBUG,"%s <%d> Rotation Flight<%s>", pclFunc, ilCount, pclRotationData[ilCount]);

                strcpy(pcpDestValue, pclRotationData[ilCount]);
                ilRC = RC_SUCCESS;
                break;
            }
        }
    }
    return ilRC;
}

int refTable(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "refTable";

    char pclSqlBuf[1024] = "\0";
    char pclSqlData[1024] = "\0";
    char pclSelection[1024] = "\0";

    if ( strcmp(rpLine->pclSourceFieldType, "NUMBER") == 0 )
    {
        sprintf(pclSelection, "WHERE %s=%s", rpLine->pclCond1, pcpSourceValue);
    }
    else
    {
        sprintf(pclSelection, "WHERE %s='%s'", rpLine->pclCond1, pcpSourceValue);
    }

    buildSelQuery(pclSqlBuf, rpLine->pclSourceTable, rpLine->pclCond2, pclSelection);
    ilRC = RunSQL(pclSqlBuf, pclSqlData);
    if (ilRC != DB_SUCCESS)
    {
        dbg(TRACE, "<%s>: Retrieving source data - Fails", pclFunc);
        strncpy(pcpDestValue,"",1);
        return RC_FAIL;
    }

    switch(ilRC)
    {
        case NOTFOUND:
            dbg(TRACE, "<%s> Retrieving source data - Not Found", pclFunc);
            strncpy(pcpDestValue," ",1);
            ilRC = RC_FAIL;
            break;
        default:
            dbg(TRACE, "<%s> Retrieving source data - Found\n <%s>", pclFunc, pclSqlData);
            BuildItemBuffer(pclSqlData, NULL, 1, ",");
            strcpy(pcpDestValue, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

int operDay(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "operDay";

    /*change the operation day into local one later*/
    strcpy(pcpDestValue, pcpSourceValue);
}

int weekDay(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "weekDay";

    /*change the weekday into local one later*/
    strcpy(pcpDestValue, pcpSourceValue);
}

int dateLoc(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "dateLoc";
    char pclTmpTime[64] = "\0";

    strcpy(pclTmpTime, pcpSourceValue);
    AddSecondsToCEDATime(pclTmpTime, igTimeDifference * MIN, 1);

    /*
    if (strlen(pclTmpTime) > atoi(rpLine->pclDestFieldLen))
    {
        strncpy(pcpDestValue, pclTmpTime, atoi(rpLine->pclDestFieldLen));
    }
    else
    */
    {
        strcpy(pcpDestValue, pclTmpTime);
    }

    ilRC = RC_SUCCESS;

    return ilRC;
}

int number(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "number";

    int ilTmp = 0;

    if (atoi(pcpSourceValue) > 0)
    {
        /*
        if (strlen(pcpSourceValue) > atoi(rpLine->pclDestFieldLen))
        {
            strncpy(pcpDestValue, pcpSourceValue, atoi(rpLine->pclDestFieldLen));
        }
        else
        */
        {
            strcpy(pcpDestValue, pcpSourceValue);
        }

        ilRC = RC_NUMBER;
    }
    else
    {
        dbg(TRACE, "%s The atoi(pcpSourceValue)<%d> is invalid -> blank value", pclFunc, atoi(pcpSourceValue));
    }

    return ilRC;
}

int timeFormat(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "timeFormat";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";
    char pclTmp[64] = "\0";

    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "%s The operator is blank -> default action",pclFunc);

    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;

    strncpy(pclYear,pcpSourceValue,4);
    strncpy(pclMonth,pcpSourceValue+4,2);
    strncpy(pclDay,pcpSourceValue+6,2);
    strncpy(pclHour,pcpSourceValue+8,2);
    strncpy(pclMin,pcpSourceValue+10,2);
    strncpy(pclSec,pcpSourceValue+12,2);

    if ( strlen(rpLine->pclCond1) == 0 || strcmp(rpLine->pclCond1," ") == 0)
    {
        sprintf(pclTmp,"%s/%s/%s %s:%s:%s", pclDay, pclMonth, pclYear, pclHour, pclMin, pclSec);
    }
    else if ( strcmp(rpLine->pclCond1,"DAY") == 0)
    {
        sprintf(pclTmp,"%s%s%s",pclYear, pclMonth, pclDay);
    }
    else if ( strcmp(rpLine->pclCond1,"HOUR") == 0)
    {
        sprintf(pclTmp,"%s%s",pclHour,pclMin);
    }

    /*
    if (strlen(pcpSourceValue) > atoi(rpLine->pclDestFieldLen))
    {
        strncpy(pcpDestValue, pclTmp, atoi(rpLine->pclDestFieldLen));
    }
    else
    */
    {
        strcpy(pcpDestValue, pclTmp);
    }

    ilRC = RC_SUCCESS;

    return ilRC;
}

int defaultOperator(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "defaultOperator";
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "%s The operator is blank -> default action",pclFunc);

    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;
    /*
    if(sizeof(pcpSourceValue) > atoi(rpLine->pclDestFieldLen))
    {
        strncpy(pcpDestValue, pcpSourceValue, atoi(rpLine->pclDestFieldLen));
    }
    else
    */
    {
        strcpy(pcpDestValue, pcpSourceValue);
    }

    ilRC = RC_SUCCESS;
    /*
    else
    {
        dbg(TRACE,"%s The dest length is less than the source one -> do not copy",pclFunc);
        ilRC = RC_FAIL;
    }
    */
    return ilRC;
}

int getUrno(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{

}

int getCurrentTime(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{

}

struct codeFunc
{
    char pclOperCode[OPER_LEN];
    int (*pflOpeFunc)(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char *pcpSelection, char *pcpAdid);
}
CODEFUNC[OPER_CODE] =
{
    {" ",defaultOperator},
    {"TIMEFORMAT",timeFormat},
    {"NUMBER",number},
    {"DATELOC",dateLoc},
    {"WEEKDAY",weekDay},
    {"OPERDAY",operDay},
    {"REF",refTable},
    {"ROTATION",rotation},
    {"VIA",via},
    {"VIAREF",viaref},
    {"ZON",zon},
    {"CODESHAREFORMAT",codeshareFormat},
    {"URNO",getUrno},
    {"CURRENTTIME",getCurrentTime}
    /*{"",}*/
};
