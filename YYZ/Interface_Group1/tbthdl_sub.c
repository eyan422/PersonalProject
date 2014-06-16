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
extern int getCodeShare(char *pcpFields, char *pcpData, _VIAL *pcpCodeShare, char *pcpFormat, char *pcpOption, char *pcpSelection);

static int UtcToLocal(char *pcpTime)
{
	int c;
    char year[5], month[3], day[3], hour[3], minute[3],second[3];
    struct tm TimeBuffer, *final_result;
    time_t time_result;

    /********** Extract the Year off CEDA timestamp **********/
    for(c=0; c<= 3; ++c)
    {
        year[c] = pcpTime[c];
    }
    year[4] = '\0';
    /********** Extract month, day, hour and minute off CEDA timestamp **********/
    for(c = 0; c <= 1; ++c)
    {
        month[c]  = pcpTime[c + 4];
        day[c]    = pcpTime[c + 6];
        hour[c]   = pcpTime[c + 8];
        minute[c] = pcpTime[c + 10];
        second[c] = pcpTime[c + 12];
    }
    /********** Terminate the Buffer strings **********/
    month[2]  = '\0';
    day[2]    = '\0';
    hour[2]   = '\0';
    minute[2] = '\0';
    second[2] = '\0';


    /***** Fill a broken-down time structure incl. string to integer *****/
    TimeBuffer.tm_year  = atoi(year) - 1900;
    TimeBuffer.tm_mon   = atoi(month) - 1;
    TimeBuffer.tm_mday  = atoi(day);
    TimeBuffer.tm_hour  = atoi(hour);
    TimeBuffer.tm_min   = atoi(minute);
    TimeBuffer.tm_sec   = atoi(second);
    TimeBuffer.tm_isdst = 0;
    /***** Make secondbased timeformat and correct mktime *****/
    time_result = mktime(&TimeBuffer) - timezone;
    /***** Reconvert into broken-down time structure *****/
    final_result = localtime(&time_result);

    sprintf(pcpTime,"%d%.2d%.2d%.2d%.2d%.2d"
        ,final_result->tm_year+1900
        ,final_result->tm_mon+1
        ,final_result->tm_mday
        ,final_result->tm_hour
        ,final_result->tm_min
        ,final_result->tm_sec);

    return(0); /**** DONE WELL ****/
}

int codeshareFormat(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "codeshareFormat";
    int ilJcnt = 0;
    int ilCount =0;
    int ilDestLen = 0;
    char pclFields[LISTLEN] = "\0";
    char pclData[LISTLEN] = "\0";
    char pcpFormat[LISTLEN] = "\0";
    _VIAL pclCodeShare[ARRAYNUMBER];

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    /*if (strlen(pcpSourceValue) == 0 || ilDestLen == 0)*/
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

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
    ilJcnt = strlen(pcpSourceValue) / CODESHARE_LEN + 1;
    dbg(DEBUG,"%s len<%d> ilNo<%d>", pclFunc, strlen(pcpSourceValue),ilJcnt);

    strcat(pclFields,"JCNT,JFNO");
    sprintf(pclData,"%d,%s", ilJcnt, pcpSourceValue);

    ilCount = getCodeShare(pclFields,pclData,pclCodeShare,pcpFormat,"ALC3", pcpSelection);
    dbg(DEBUG,"%s ilCount<%d> pcpFormat<%s>",pclFunc,ilCount, pcpFormat);
    /*
    for(ili = 0; ili < ilCount; ili++)
    {
        dbg(DEBUG,"%s ili<%d>",pclFunc,ili);
        dbg(DEBUG,"%s ALC2<%s> ALC3<%s> FLNO<%s>",pclFunc,pclCodeShare[ili].pclAlc2, pclCodeShare[ili].pclAlc3, pclCodeShare[ili].pclFlno);
    }
    */
    ilRC = getDestSourceLen(ilDestLen, pcpFormat);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pcpFormat);
    }
    else
    {
        strncpy(pcpDestValue, pcpFormat, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }

    return RC_SUCCESS;
}

int zon(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "zon";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    /*if (strlen(pcpSourceValue) == 0 || ilDestLen == 0)*/
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

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
    int ilDestLen = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "via";
    char pclTmp[1024] = "\0";
    char pclSqlBuf[1024] = "\0";
    char pclSqlData[1024] = "\0";
    char pclSelection[1024] = "\0";
    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    strncpy(pcpDestValue, "", 1);
    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

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

            ilRC = getDestSourceLen(ilDestLen, pclSqlData);
            if (ilRC == RC_SUCCESS)
            {
                strcpy(pcpDestValue, pclSqlData);
            }
            else
            {
                strncpy(pcpDestValue, pclSqlData, ilDestLen);
                pcpDestValue[ilDestLen] = '\0';
            }

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
    int ilDestLen = 0;
    char *pclFunc = "via";

    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    strncpy(pcpDestValue, "", 1);

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    if(strlen(pcpSourceValue) == 0 || strncmp(pcpSourceValue," ",1) == 0 )
    {
        dbg(TRACE,"%s pcpSourceValue<%s> is invalid",pclFunc,pcpSourceValue);
        return ilRC;
    }

    /*getting the vial information*/
    ilCount = getVial(pcpSourceValue, pclVial);
    ili = (int)((rpLine->pclDestField)[3] - '0') - 1;

    ilRC = getDestSourceLen(ilDestLen, pclVial[ili]);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclVial[ili]);
    }
    else
    {
        strncpy(pcpDestValue, pclVial[ili], ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }

    ilRC = RC_SUCCESS;
    return ilRC;
}

int rotation(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilCount = 0;
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "rotation";

    char *pclTmp = NULL;
    char pclTmpSelection[64] = "\0";
    char pclUrnoSelection[64] = "\0";
    char pclRotationData[ARRAYNUMBER][LISTLEN] = {"\0"};

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    strcpy(pclTmpSelection, pcpSelection);
    /*dbg(DEBUG,"%s pclTmpSelection<%s>",pclFunc,pclTmpSelection);*/

    pclTmp = strstr(pclTmpSelection, "=");
    /*dbg(DEBUG,"%s pclTmp<%s>",pclFunc,pclTmp);*/

    strcpy(pclUrnoSelection, pclTmp+1);
    /*dbg(DEBUG,"%s pclUrnoSelection<%s>",pclFunc,pclUrnoSelection);*/

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

                ilRC = getDestSourceLen(ilDestLen, pclRotationData[ilCount]);
                if (ilRC == RC_SUCCESS)
                {
                    strcpy(pcpDestValue,  pclRotationData[ilCount]);
                }
                else
                {
                    strncpy(pcpDestValue, pclRotationData[ilCount], ilDestLen);
                    pcpDestValue[ilDestLen] = '\0';
                }

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
    int ilDestLen = 0;
    char *pclFunc = "refTable";

    char pclTmp[64] = "\0";
    char pclSqlBuf[1024] = "\0";
    char pclSqlData[1024] = "\0";
    char pclSelection[1024] = "\0";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

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
            strncpy(pcpDestValue,"",1);
            ilRC = RC_FAIL;
            break;
        default:
            dbg(TRACE, "<%s> Retrieving source data - Found\n <%s>", pclFunc, pclSqlData);
            BuildItemBuffer(pclSqlData, NULL, 1, ",");
            TrimRight(pclSqlData);

            ilRC = getDestSourceLen(ilDestLen, pclSqlData);
            if (ilRC == RC_SUCCESS)
            {
                strcpy(pcpDestValue,  pclSqlData);
            }
            else
            {
                strncpy(pcpDestValue, pclSqlData, ilDestLen);
                pcpDestValue[ilDestLen] = '\0';
            }

            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

int operDay(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "operDay";
    char pclTmp[64] = "\0";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    strcpy(pclTmp, pcpSourceValue);

    if(strcmp(rpLine->pclCond1, "DATELOC") == 0)
    {
        if(atoi(pcpSourceValue) == 0 || strlen(pcpSourceValue) != 14)
        {
            dbg(TRACE, "%s Source value<%s> is invalid for DATELOC atttibute", pclFunc, pcpSourceValue);
            return RC_FAIL;
        }
        /*UTC -> LOC*/
        UtcToLocal(pclTmp);
    }

    strncpy(pcpDestValue, pclTmp+6,ilDestLen);
    pcpDestValue[ilDestLen] = '\0';
}

int weekDay(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "weekDay";
    char pclTmp[64] = "\0";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    strcpy(pclTmp, pcpSourceValue);

    if(strcmp(rpLine->pclCond1, "DATELOC") == 0)
    {
        if(atoi(pcpSourceValue) == 0 || strlen(pcpSourceValue) != 14)
        {
            dbg(TRACE, "%s Source value<%s> is invalid for DATELOC atttibute", pclFunc, pcpSourceValue);
            return RC_FAIL;
        }
        /*UTC -> LOC*/
        UtcToLocal(pclTmp);
    }

    ilRC = getDestSourceLen(ilDestLen, pclTmp);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclTmp);
    }
    else
    {
        strncpy(pcpDestValue, pclTmp, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }
}

int timeFormat(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilDestLen = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "timeFormat";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";
    char pclTmp[64] = "\0";
    char pclTmpLoc[64] = "\0";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    /*if (strlen(pcpSourceValue) == 0 || ilDestLen == 0)*/
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    strcpy(pclTmpLoc, pcpSourceValue);

    if(strcmp(rpLine->pclCond1, "DATELOC") == 0)
    {
        if(atoi(pcpSourceValue) == 0 || strlen(pcpSourceValue) != 14)
        {
            dbg(TRACE, "%s Source value<%s> is invalid for DATELOC atttibute", pclFunc, pcpSourceValue);
            return RC_FAIL;
        }
        /*UTC -> LOC*/
        UtcToLocal(pclTmpLoc);
    }

    strncpy(pclYear,pclTmpLoc,4);
    strncpy(pclMonth,pclTmpLoc+4,2);
    strncpy(pclDay,pclTmpLoc+6,2);
    strncpy(pclHour,pclTmpLoc+8,2);
    strncpy(pclMin,pclTmpLoc+10,2);
    strncpy(pclSec,pclTmpLoc+12,2);

    if ( strlen(rpLine->pclCond2) == 0 || strcmp(rpLine->pclCond2," ") == 0)
    {
        sprintf(pclTmp,"%s/%s/%s %s:%s:%s", pclDay, pclMonth, pclYear, pclHour, pclMin, pclSec);
    }
    else if ( strcmp(rpLine->pclCond2,"YEAR_MON_DAY") == 0)
    {
        sprintf(pclTmp,"%s%s%s",pclYear, pclMonth, pclDay);
    }
    else if ( strcmp(rpLine->pclCond2,"HOUR_MIN") == 0)
    {
        sprintf(pclTmp,"%s%s",pclHour,pclMin);
    }

    ilRC = getDestSourceLen(ilDestLen, pclTmp);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclTmp);
    }
    else
    {
        strncpy(pcpDestValue, pclTmp, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }

    ilRC = RC_SUCCESS;

    return ilRC;
}

int defaultOperator(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "defaultOperator";
    char pclTmp[64] = "\0";
    /*dbg(TRACE, "%s The operator is blank -> default action",pclFunc);*/

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    strcpy(pclTmp, pcpSourceValue);

    if(strcmp(rpLine->pclCond1, "DATELOC") == 0)
    {
        if(atoi(pcpSourceValue) == 0 || strlen(pcpSourceValue) != 14)
        {
            dbg(TRACE, "%s Source value<%s> is invalid for DATELOC atttibute", pclFunc, pcpSourceValue);
            return RC_FAIL;
        }
        UtcToLocal(pclTmp);
    }

    dbg(DEBUG,"%s pclTmp<%s>, ilDestLen<%d>, pcpDestValue<%s>, strlen(pcpDestValue)<%d>",pclFunc,pclTmp,ilDestLen,pcpDestValue,strlen(pcpDestValue));

    ilRC = getDestSourceLen(ilDestLen, pclTmp);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclTmp);
    }
    else
    {
        strncpy(pcpDestValue, pclTmp, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }
    return ilRC;
}

int getUrno(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    int ilNextUrno = 0;
    char *pclFunc = "getUrno";
    char pclTmp[64] = "\0";

    ilDestLen = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    ilNextUrno = NewUrnos( rpLine->pclDestTable, 1);

    sprintf(pclTmp,"%d",ilNextUrno);

    ilRC = getDestSourceLen(ilDestLen, pclTmp);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclTmp);
    }
    else
    {
        strncpy(pcpDestValue, pclTmp, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }
}

int getCurrentTime(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    int ilDestLen = 0;
    char *pclFunc = "getCurrentTime";
    char pclTimeNow[TIMEFORMAT] = "\0";
    char pclTmp[64] = "\0";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";

    ilDestLen   = atoi(rpLine->pclDestFieldLen);
    if ( ilDestLen == 0)
    {
        dbg(TRACE, "%s Dest Length<%d> is invalid", pclFunc, ilDestLen);
        return RC_FAIL;
    }

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);

    strcpy(pclTmp, pclTimeNow);

    if(strcmp(rpLine->pclCond1, "DATELOC") == 0)
    {
        if(atoi(pclTimeNow) == 0 || strlen(pclTimeNow) != 14)
        {
            dbg(TRACE, "%s Currnt time value<%s> is invalid for DATELOC atttibute", pclFunc, pclTimeNow);
            return RC_FAIL;
        }

        /*UTC -> LOC*/
        UtcToLocal(pclTmp);
    }

    if (strcmp(rpLine->pclDestFieldType, "DATE") == 0)
    {
        strncpy(pclYear,pclTmp,4);
        strncpy(pclMonth,pclTmp+4,2);
        strncpy(pclDay,pclTmp+6,2);
        strncpy(pclHour,pclTmp+8,2);
        strncpy(pclMin,pclTmp+10,2);
        strncpy(pclSec,pclTmp+12,2);

        if ( strlen(rpLine->pclCond2) == 0 || strcmp(rpLine->pclCond2," ") == 0)
        {
            sprintf(pclTmp,"%s-%s-%s", pclYear,pclMonth,pclDay);
        }
    }

    ilRC = getDestSourceLen(ilDestLen, pclTmp);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcpDestValue,  pclTmp);
    }
    else
    {
        strncpy(pcpDestValue, pclTmp, ilDestLen);
        pcpDestValue[ilDestLen] = '\0';
    }

    return RC_SUCCESS;
}

int getDestSourceLen(int ipDestLen, char *pcpSourceValue)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "getDestSourceLen";

    int ilSourceLen = strlen(pcpSourceValue);
    int ilDestLen   = ipDestLen;

    dbg(DEBUG,"%s ilDestLen<%d> pcpSourceValue<%s> ilSourceLen<%d>", pclFunc, ilDestLen, pcpSourceValue, ilSourceLen);

    if (ilSourceLen <= ilDestLen)
    {
        ilRC = RC_SUCCESS;
    }
    else /*if (ilSourceLen > ilDestLen)*/
    {
        ilRC = RC_FAIL;
    }

    return ilRC;
}

struct codeFunc
{
    char pclOperCode[OPER_LEN];
    int (*pflOpeFunc)(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char *pcpSelection, char *pcpAdid);
}
CODEFUNC[OPER_CODE] =
{
    {" ",           defaultOperator},/**/
    {"TIMEFORMAT",  timeFormat},/**/
    {"WEEKDAY",     weekDay},/**/
    {"OPERDAY",     operDay},/**/
    {"REF",         refTable},/**/
    {"ROTATION",    rotation},/**/
    {"VIA",         via},/**/
    {"VIAREF",      viaref},/**/
    {"ZON",         zon},/**/
    {"CODESHAREFORMAT",codeshareFormat},/**/
    {"URNO",        getUrno},/**/
    {"CURRENTTIME", getCurrentTime}/**/
    /*{"",}*/
};
