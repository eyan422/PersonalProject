#ifndef RC_NOTFOUND
#define RC_NOTFOUND 1
#endif
#define SEQUENCE_NUMBER     (1)
#define CHECKIN_COUNTERS    (2)
#define CHECKIN_OPEN_TIME   (3)
#define CHECKIN_CLOSE_TIME  (4)

static char *prgCmdTxt[] = {  "SEQUENCE_NUMBER", "CHECKIN_COUNTERS", "CHECKIN_OPEN_TIME", "CHECKIN_CLOSE_TIME", NULL };
static int    rgCmdDef[] = {  SEQUENCE_NUMBER, CHECKIN_COUNTERS, CHECKIN_OPEN_TIME, CHECKIN_CLOSE_TIME, 0 };

/*---------------------------------------------------------------------------------------------------------*/
/* returns the number of a command like input = 'XYZ' --> output = n (n | 0..n+1)                            */ 
/***********************************************************************************************************/
static int GetCommand(char *pcpCommand, int *pipCmd)
{
 int  ilRC   = RC_SUCCESS ;   /* Return code */
 int  ilLoop = 0 ;
 char clCommand [128] ;
    
 memset (&clCommand[0], 0x00, sizeof(clCommand)) ;

 ilRC = get_real_item (&clCommand[0], pcpCommand, 1) ;
 if (ilRC > 0)
 {
  ilRC = RC_FAIL ;
 } /* end of if */

 while ((ilRC != 0) && (prgCmdTxt[ilLoop] != NULL))
 {
  ilRC = strcmp(&clCommand[0], prgCmdTxt[ilLoop]) ;
  ilLoop++ ;
 } /* end of while */

 if (ilRC == 0)
 {
  ilLoop-- ; 
  
  *pipCmd = rgCmdDef[ilLoop] ;
 }
 else
 {
  dbg (TRACE, "GetCommand: <%s> is not valid", &clCommand[0]) ;
  ilRC = RC_FAIL ;
 } /* end of if */

 return (ilRC) ;
    
} /* end of GetCommand */
/*------- DoSingleSelect ----------------------------------------------------------------------*/
static int DoSingleSelect(char *pcpTable,char *pcpSelection,char *pcpFields,char *pcpDataArea)
{
	int ilRc = RC_SUCCESS;
	static char clSqlBuf[4096];

	short slCursor = 0;
	short slFunction;
	char *pclDataArea;
	int ilLc,ilItemCount;
    char clErrMsg[4096];

	sprintf(clSqlBuf,"SELECT %s FROM %s %s",pcpFields,pcpTable,pcpSelection);
    dbg(TRACE,"SQL: <%s>",clSqlBuf);
    *pcpDataArea = '\0';
	ilRc = sql_if(START,&slCursor,clSqlBuf,pcpDataArea);
	if(ilRc != RC_SUCCESS && ilRc != NOTFOUND)
	{
		/* there must be a database error */
		get_ora_err(ilRc, clErrMsg);
		dbg(TRACE,"DoSingleSelect failed RC=%d <%s>",ilRc,clErrMsg);		
	}
	close_my_cursor(&slCursor);
	slCursor = 0;

	ilItemCount = get_no_of_items(pcpFields); 
	pclDataArea = pcpDataArea;
	for(ilLc =  1; ilLc < ilItemCount; ilLc++)
	{
		while(*pclDataArea != '\0')
		{
			pclDataArea++;
		}
		*pclDataArea = ',';
	}
    if (ilRc == RC_SUCCESS)
	{
		dbg(DEBUG,"DoSingleSelect: RC=%d <%s>",ilRc,pcpDataArea);
	}
	else
	{
		dbg(DEBUG,"DoSingleSelect: RC=%d",ilRc);
	}
	return(ilRc);
}
/*------- PutSequence --------------------------------------------------------------------*/
static void PutSequence(char *pcpKey,long lpSequence)
{
	int ilRc;

	short slCursor = 0;
	char  clSqlBuf[256];
	char  clDataArea[1024]="\0";
    char  clErrMsg[4096];

	sprintf(clSqlBuf,"UPDATE NUMTAB SET ACNU='%10d' WHERE KEYS='%s' "
 		             " AND HOPO='%s'",lpSequence,pcpKey,cgHopo);
	dbg(DEBUG, "PutSequence: clSqlBuf = <%s>, clDataArea = <%s>", clSqlBuf, clDataArea);
	ilRc = sql_if(START,&slCursor,clSqlBuf,clDataArea);
    dbg(DEBUG, "PutSequence: after sql_if clSqlBuf = <%s>, clDataArea = <%s>", clSqlBuf, clDataArea);
    
	if(ilRc == RC_NOTFOUND)
	{   dbg(DEBUG, "PutSequence: not found pcpKey = <%s>", pcpKey);
		/* sequence not yet written, create new one */
		sprintf(clSqlBuf, "INSERT INTO NUMTAB (ACNU,HOPO,KEYS,MAXN,MINN) "
		                  "VALUES(%d,'%s','%s',2147483647,1)", lpSequence,cgHopo,pcpKey);
		close_my_cursor(&slCursor);
		slCursor = 0;
		ilRc = sql_if(START,&slCursor,clSqlBuf,clDataArea);
		if (ilRc != RC_SUCCESS)
		{
			/* there must be a database error */
	 		get_ora_err (ilRc, clErrMsg);
			dbg(TRACE,"Inserting Sequence <%s> failed RC=%d",pcpKey,ilRc);
		}
		close_my_cursor(&slCursor);
    }
	else if (ilRc != DB_SUCCESS)
	{ 
		/* there must be a database error */
	 	get_ora_err(ilRc, clErrMsg);
		dbg(TRACE,"Reading Sequence <%s> failed RC=%d",pcpKey,ilRc);
	}else {
        commit_work();    
    }	
	close_my_cursor(&slCursor);
	slCursor = 0;
}
/*------- GetSequence ----------------------------------------------------------------------*/
static long GetSequence(char *pcpKey, long plpDefaultValue)
{
	int ilRc;
	long llSequence = 0;
	char clSelection[200];
	char clSequence[24];
	char clTimeStamp[24];
	char clFlag[32];
	char clDataArea[1024]="\0";

	sprintf(clSelection,"WHERE KEYS='%s' AND HOPO='%s'",pcpKey,cgHopo);
	if ((ilRc = DoSingleSelect("NUMTAB",clSelection,"ACNU,FLAG",clDataArea)) == RC_SUCCESS)
	{
		GetDataItem(clSequence,clDataArea,1,',',"","\0\0");
		GetDataItem(clFlag,clDataArea,2,',',"","\0\0");

		dbg(DEBUG,"GetSequence: Sequence <%s> clFlag <%s>",clSequence,clFlag);

		llSequence = atol(clSequence);		
    }
	else if(ilRc == RC_NOTFOUND)
	{
		PutSequence(pcpKey,plpDefaultValue);
		llSequence = plpDefaultValue;
	} 
	else
	{ 
		/* there must be a database error */
		dbg(TRACE,"Reading Sequence <%s> failed RC=%d",pcpKey,ilRc);
	}
	return llSequence;
}
/*-----------------------------------------------------------------------------------------*/
int extractUaftFromSource(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "extractUaftFromSource";
    
    char pclUaft[20] = "\0";
    char *pclTmpPtr = NULL;
	char pclSelection[256];
    
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    if (strcmp(rpLine->pclSourceField, "URNO") == 0)
    {   /* get UAFT from pcpSourceValue if pclSourceField = URNO */
        strcpy(pclUaft, pcpSourceValue);
        TrimRight(pclUaft);
    }
    else {    /* get UAFT from pcpSelection, example: WHERE URNO=971250116 */
        strcpy(pclSelection,pcpSelection); 
        pclTmpPtr = strstr(pclSelection, "=");    
        if (pclTmpPtr != NULL)
        {
            *pclTmpPtr = '\0';
            pclTmpPtr++;
            while (*pclTmpPtr == ' ') pclTmpPtr++;
            strcpy(pclUaft, pclTmpPtr);
            TrimRight(pclUaft);
        }
    }
    
    dbg(TRACE, "<%s> pclUaft = <%s>",pclFunc, pclUaft);
	strcpy(pcpDestValue,pclUaft);
    ilRC = RC_SUCCESS;
    return ilRC;
}
/*-----------------------------------------------------------------------------------------
#BMS_TNEW_DAILY.AOD = 'ARR' or 'DEP' depend AFTTAB.ADID = 'A' or 'D' 
Y;2;AFTTAB;URNO;ADID;CHAR;BMS_TNEW_DAILY;UREF;AOD;3;STR;MAP;A,D;ARR,DEP;
/*-----------------------------------------------------------------------------------------*/
int map(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "map";
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s, pcpSourceValue = <%s>, pcpSelection = <%s>, pcpAdid = <%s>",
                                     pclFunc,pclFunc,pcpSourceValue,pcpSelection,pcpAdid);

    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;
    
    if (strcmp(pcpAdid, "A") == 0)
        strcpy(pcpDestValue, "ARR");
    if (strcmp(pcpAdid, "D") == 0)
        strcpy(pcpDestValue, "DEP");

    ilRC = RC_SUCCESS;

    return ilRC;
}
/*--------------------------------------------------------------------------------------------------- 
#BMS_TNEW_DAILY.SCHEDULE_DATE = AFTTAB.STOA if AFTTAB.ADID = 'A' else BMS_TNEW_DAILY.SCHEDULE_DATE = AFTTAB.STOD
Y;2;AFTTAB;URNO;STOA,STOD;DATE;BMS_TNEW_DAILY;UREF;SCHEDULE_DATE;;DATELOC;CHECK;ADID;A,D
//Y;2;AFTTAB;URNO;STOD;DATE;BMS_TNEW_DAILY;UREF;SCHEDULE_DATE;;DATELOC;CHECK;ADID;'D';
---------------------------------------------------------------------------------------------------*/
int check(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "check";
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s",pclFunc,pclFunc);
    
    if (strcmp(pcpAdid,"D") != 0)   /* condition not satisfy */     
        return RC_FAIL;
    if (strcmp("DATELOC",rpLine->pclCond2) == 0)
        ilRC = dateLoc(pcpDestValue,pcpSourceValue,rpLine,pcpSelection,pcpAdid);
    
    return ilRC;
}
/*----------------------------------------------------------------------------------------------------*/
int merge(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "merge";
    
    char pclUaft[20] = "\0";
    char *pclTmpPtr = NULL;
    char pclSelection[128] = "\0"; 
    char pclTable[128] = "\0"; 
    char pclFields[256] = "\0"; 
    char pclSqlData[1024] = "\0";
    char pclFormatString[128]; 
    
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s, pcpSourceValue = <%s>, pcpSelection = <%s>",pclFunc,pclFunc,pcpSourceValue,pcpSelection);
    
    extractUaftFromSource(pclUaft, pcpSourceValue, rpLine, pcpSelection);    
	strcpy(pclFields, rpLine->pclCond1); 
    strcpy(pclFormatString,rpLine->pclCond2); 
	strcpy(pclTable,"AFTTAB");
	sprintf(pclSelection,"WHERE URNO = '%s'",pclUaft);
	
	if ((ilRC = DoSingleSelect(pclTable,pclSelection,pclFields,pclSqlData)) == RC_SUCCESS)
    {
		char pclValue[512] = "\0";
		char pclTmpValue[512];
		int  ilFndCnt = get_no_of_items(pclFields);
		int  ilIdx; 
		
		for (ilIdx = 1; ilIdx <= ilFndCnt; ilIdx++)
		{
			GetDataItem(pclTmpValue,pclSqlData,ilIdx,',',"","\0\0");			
			if (!pclValue[0])
				strcpy(pclValue,pclTmpValue);
			else{
				strcat(pclValue,",");
				strcat(pclValue,pclTmpValue);			
			}
		}
		PrintValueInFormat(pcpDestValue,pclValue,pclFormatString);
	}
    
    return ilRC;
}
/*--------------------------------------------------------------------------------------------------- 
#the value of field SEQUENCE_NUMBER,CHECKIN_COUNTERS will be set by hardcode
---------------------------------------------------------------------------------------------------*/
int hardcode(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "hardcode";
    char pclUaft[20] = "\0";
    char *pclTmpPtr = NULL;
    char pclSelection[128] = "\0"; 
    char pclTable[128] = "\0"; 
    char pclFields[256] = "\0"; 
    char pclSqlData[1024] = "\0";
    
    
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s, pclDestField = <%s>, pcpSelection = <%s>, pcpSourceValue = <%s>",
               pclFunc, pclFunc, rpLine->pclDestField, pcpSelection, pcpSourceValue);
    
    if (pcpSourceValue != NULL) {   /* get UAFT from pcpSourceValue */
        strcpy(pclUaft, pcpSourceValue);
        TrimRight(pclUaft);
    }
    else {    
        strcpy(pclSelection,pcpSelection); 
        pclTmpPtr = strstr(pclSelection, "=");    /* get UAFT from WHERE URNO=971250116 */
        if (pclTmpPtr != NULL)
        {
            *pclTmpPtr = '\0';
            pclTmpPtr++;
            while (*pclTmpPtr == ' ') pclTmpPtr++;
            strcpy(pclUaft, pclTmpPtr);
            TrimRight(pclUaft);
        }
    }
    dbg(TRACE, "<%s> pclUaft = <%s>",pclFunc, pclUaft);
    
    int ilCmd = 0; 
    ilRC = GetCommand (rpLine->pclDestField, &ilCmd);
    if (ilRC == RC_FAIL) return ilRC;

    switch (ilCmd)
    {
        case SEQUENCE_NUMBER: 
            dbg(TRACE,"<%s> SEQUENCE_NUMBER of %s", pclFunc, rpLine->pclDestTable);
            strcpy(pclTable, rpLine->pclDestTable); 
            sprintf(pclSelection, "WHERE UREF = '%s'", pclUaft);
            strcpy(pclFields, "SEQUENCE_NUMBER,UREF");
            if ((ilRC = DoSingleSelect(pclTable,pclSelection,pclFields,pclSqlData)) == RC_SUCCESS)  /* keep the same SEQUENCE_NUMBER if exist */
            {
                GetDataItem(pcpDestValue,pclSqlData,1,',',"","\0\0");   
            }else
            {
                char *pclKey = rpLine->pclCond1;
                long llSequence = atoi(rpLine->pclCond2); 
                
                llSequence = GetSequence(pclKey,llSequence);
                sprintf(pcpDestValue,"%d",llSequence);
                llSequence++;
                PutSequence(pclKey,llSequence);
            }            
            break;
        case CHECKIN_COUNTERS:
            dbg(TRACE,"<tvo> CHECKIN_COUNTERS");  
            ilRC = GetDepartureCheckinCounter(pclUaft,pcpDestValue,0,0);
            break;
        case CHECKIN_OPEN_TIME:
            dbg(TRACE,"<tvo> CHECKIN_OPEN_TIME");
            ilRC = GetDepartureCheckinCounter(pclUaft,0,pcpDestValue,0);
            break;
        case CHECKIN_CLOSE_TIME:
            dbg(TRACE,"<tvo> CHECKIN_CLOSE_TIME");
            ilRC = GetDepartureCheckinCounter(pclUaft,0,0,pcpDestValue);
            break;
        default:
            dbg(TRACE, "<tvo> unknown field <%s>", rpLine->pclDestField);
    }

    return ilRC;
}
/*----------------------------------------------------------------------------------------------------*/
int GetDepartureCheckinCounter(char *pcpUaft, char *pcpCKIC, char * pcpFirstOpen, char *pcpLastClose)
{   /* identify check-in counter by CKES < STOD < CKES + 30min */ 
    int ilRC = RC_FAIL;
    char *pclFunc = "GetDepartureCheckinCounter";
    char pclCkicList[20];
    char pclFirstOpen[5];
    char pclLastClose[5];
   
    char *pclTmpPtr = NULL;
    char pclSelection[128] = "\0"; 
    char pclTable[128] = "\0"; 
    char pclFields[256] = "\0"; 
    char pclSqlData[1024] = "\0";
    char pclTmpValue[64] = "\0";
    char pclStod[20] = "\0";
    char pclCkif[10] = "\0";
    char pclCkit[10] = "\0";
    
    
    memset(pclCkicList, 0x0, sizeof(pclCkicList)); 
    memset(pclFirstOpen, 0x0, sizeof(pclFirstOpen)); 
    memset(pclLastClose, 0x0, sizeof(pclLastClose)); 
    
    strcpy(pclTable,"AFTTAB");
    strcpy(pclFields,"STOD,CKIF,CKIT"); 
    sprintf(pclSelection,"WHERE URNO = '%s'",pcpUaft);
    
    if ((ilRC = DoSingleSelect(pclTable,pclSelection,pclFields,pclSqlData)) != RC_SUCCESS)
        return ilRC;
    
    GetDataItem(pclStod,pclSqlData,1,',',"","\0\0");
    GetDataItem(pclCkif,pclSqlData,2,',',"","\0\0");
    GetDataItem(pclCkit,pclSqlData,3,',',"","\0\0");
    
    TrimRight(pclStod); if (strcmp(pclStod," ") == 0) pclStod[0] = '\0';
    TrimRight(pclCkif); if (strcmp(pclCkif," ") == 0) pclCkif[0] = '\0';
    TrimRight(pclCkit); if (strcmp(pclCkit," ") == 0) pclCkit[0] = '\0';
    dbg(TRACE, "<%s> STOD = <%s>, CKIF = <%s>, CKIT = <%s>", pclFunc, pclStod, pclCkif, pclCkit);
    if ((strlen(pclStod) == 0) || (strlen(pclCkif) == 0) || (strlen(pclCkit) == 0))  /* STOD or CKIF or CKIT are blank */
    {
        if (pcpCKIC != NULL)
            strcpy(pcpCKIC," ");
        if (pcpFirstOpen != NULL)
            strcpy(pcpFirstOpen," ");
        if (pcpLastClose != NULL)
            strcpy(pcpLastClose," ");
        return RC_SUCCESS;
    }
    sprintf(pclCkicList, "%s - %s", pclCkif, pclCkit);

    /*-- query CCATAB to get check-in counter information --------------*/
    char pclStod30min[20], pclStod120min[20]; 
    
    strcpy(pclStod30min,pclStod);     
    AddSecondsToCEDATime(pclStod30min, (-30) * 60, 1);
   
    strcpy(pclTable,"CCATAB");
    strcpy(pclFields,"min(CKBS),max(CKES)");     
    sprintf(pclSelection,"WHERE (CKES BETWEEN '%s' AND '%s')", pclStod30min, pclStod);
        
    if ((ilRC = DoSingleSelect(pclTable,pclSelection,pclFields,pclSqlData)) == RC_SUCCESS)
    {  
        GetDataItem(pclTmpValue,pclSqlData,1,',',"","\0\0");
        strncpy(pclFirstOpen, &pclTmpValue[8],4);
        GetDataItem(pclTmpValue,pclSqlData,2,',',"","\0\0");
        strncpy(pclLastClose, &pclTmpValue[8],4);        
    }
    /*-----------------------------------------------------------------*/
    if (pcpCKIC != NULL)
        strcpy(pcpCKIC,pclCkicList);
    if (pcpFirstOpen != NULL)
        strcpy(pcpFirstOpen,pclFirstOpen);
    if (pcpLastClose != NULL)
        strcpy(pcpLastClose,pclLastClose);
        
    dbg(TRACE, "<%s> CHECKIN_COUNTERS = <%s>, CHECKIN_OPEN_TIME = <%s>, CHECKIN_CLOSE_TIME = <%s>", pclFunc, pclCkicList, pclFirstOpen, pclLastClose);
    ilRC = RC_SUCCESS;
    return ilRC;
}
/*-------------------------------------------------------------------------------------*/
int PrintValueInFormat(char* pcpDestValue, char* pclValue, char* pclFormatString) 
{	
	int  ilRC = RC_FAIL;
	char pcpTmpFormat[128];	
	char pcpTmpValue[128];	
	int ilIdx = 0, ilCol, ilPos; 	
	int ilFndCnt = get_no_of_items(pclValue);
	int ilNumber = FALSE;    /* default is string */ 
	int ilLength = 0; 
		
	for (ilIdx = 1; ilIdx <= ilFndCnt; ilIdx++)
	{
		GetDataItem(pcpTmpValue,pclValue,ilIdx,',',"","\0\0");	
		get_real_item(pcpTmpFormat,pclFormatString,ilIdx);
		ilLength = atoi(pcpTmpFormat);
		pcpTmpValue[ilLength] = 0; 
	
		if (!pcpDestValue[0])
		{
			strcpy(pcpDestValue,pcpTmpValue); 				
		}else
		{	
			strcat(pcpDestValue,pcpTmpValue);
		}		
	}
	
	ilRC = RC_SUCCESS;	
	return ilRC;
}
int notyet(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
	int ilRC = RC_FAIL;    
    char *pclFunc = "notyet";
	
	dbg(TRACE,"%s: pcpSourceValue = <%s>, pclCond1 = <%s>, pclCond2 = <%s>", pclFunc, pcpSourceValue, rpLine->pclCond1, rpLine->pclCond2); 
	return ilRC;
}