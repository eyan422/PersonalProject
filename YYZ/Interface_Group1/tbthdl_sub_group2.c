#include "tbthdl.h" 

#ifndef RC_NOTFOUND
#define RC_NOTFOUND 1
#endif
#define SEQUENCE_NUMBER     (1)
#define CHECKIN_COUNTERS    (2)
#define CHECKIN_OPEN_TIME   (3)
#define CHECKIN_CLOSE_TIME  (4)

static char *prgCmdTxt[] = {  "SEQUENCE_NUMBER", "CHECKIN_COUNTERS", "CHECKIN_OPEN_TIME", "CHECKIN_CLOSE_TIME", NULL };
static int    rgCmdDef[] = {  SEQUENCE_NUMBER, CHECKIN_COUNTERS, CHECKIN_OPEN_TIME, CHECKIN_CLOSE_TIME, 0 };
/*
int  igEnableIgnoreDelete;
int  igTransCmd;
char cgTransTypeValue[128];
*/
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
/*-------------------------------------------------------------------------------*/
static int GetCommandVer2(char** prpCmdTxt, int *prpCmdDef, char *pcpCommand, int *pipCmd)
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

 while ((ilRC != 0) && (prpCmdTxt[ilLoop] != NULL))
 {
  ilRC = strcmp(&clCommand[0], prpCmdTxt[ilLoop]) ;
  ilLoop++ ;
 } /* end of while */

 if (ilRC == 0)
 {
  ilLoop-- ; 
  
  *pipCmd = prpCmdDef[ilLoop] ;
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
    char clSourceList[256]; 
	char clDestList[256];
	char *clSplit = ",";
	
	memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s, pcpSourceValue = <%s>, pcpSelection = <%s>, pcpAdid = <%s>",
                                     pclFunc,pclFunc,pcpSourceValue,pcpSelection,pcpAdid);
									 
    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;
	
	if (strcmp(rpLine->pclSourceField, "ADID") == 0)
	{	  
		if (strcmp(pcpAdid, "A") == 0)
			strcpy(pcpDestValue, "ARR");
		if (strcmp(pcpAdid, "D") == 0)
			strcpy(pcpDestValue, "DEP");
	}else
	{	
		strcpy(clSourceList, rpLine->pclCond1); 
		strcpy(clDestList, rpLine->pclCond2);
		
		if (strlen(clSourceList) == 0 || strlen(clDestList) == 0)		
		{
			strcpy(pcpDestValue, pcpSourceValue);      /* can't convert */
		}
		else 
		{
			int  ilIdx, ilCol, ilPos;  
			
			ilRC = FindItemInList(clSourceList,pcpSourceValue,clSplit[0],&ilIdx,&ilCol,&ilPos);
			
			if (ilRC == RC_SUCCESS)
				get_item(ilIdx, clDestList, pcpDestValue, 0, &clSplit[0], "\0", "\0");
			else 
				strcpy(pcpDestValue, pcpSourceValue);   /* can't convert */
		}
	}    

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
/*----------------------------------------------------------------------------------------------------*/
int sequence(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{	
    int ilRC = RC_FAIL;
    char *pclFunc = "sequence";
    char pclUaft[20] = "\0";
    char *pclTmpPtr = NULL;
    char pclSelection[128] = "\0"; 
    char pclTable[128] = "\0"; 
    char pclFields[256] = "\0"; 
    char pclSqlData[1024] = "\0";
	int  ilNewSequence = FALSE; 
    
    
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "<%s> The operator is %s, pcpSelection = <%s>, pcpSourceValue = <%s>", pclFunc, pclFunc, pcpSelection, pcpSourceValue);
    
	
	extractUaftFromSource(pclUaft,pcpSourceValue,rpLine,pcpSelection);
	
    dbg(TRACE, "<%s> pclUaft = <%s>",pclFunc, pclUaft);
	
	strcpy(pclTable, rpLine->pclDestTable); 
    sprintf(pclSelection, "WHERE %s = '%s'", rpLine->pclDestKey, pclUaft);
    strcpy(pclFields, rpLine->pclDestField);
	ilNewSequence = TRUE;
    if ((ilRC = DoSingleSelect(pclTable,pclSelection,pclFields,pclSqlData)) == RC_SUCCESS)  
    {
        GetDataItem(pcpDestValue,pclSqlData,1,',',"","\0\0");   /* keep the same SEQUENCE_NUMBER if exist */
		if ((igEnableInsertOnly == FALSE) && (atoi(rpLine->pclCond2) > 0))    /* tvo_add: insert only case */
			ilNewSequence = FALSE; 		
    }
	if (ilNewSequence == TRUE)
    {
        char *pclKey = rpLine->pclCond1;
        long llSequence = atoi(rpLine->pclCond2); 
                
        llSequence = GetSequence(pclKey,llSequence);
        sprintf(pcpDestValue,"%d",llSequence);
        llSequence++;
        PutSequence(pclKey,llSequence);
    }
	
	ilRC = RC_SUCCESS;	
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
            dbg(TRACE, "<tvo> unknown field <%s>", rpLine->pclDestField, rpLine->pclCond2);
    }

    return ilRC;
}
/*----------------------------------------------------------------------------------------------------*/
static char pcgSplit[2] = "-"; 

int GetDepartureCheckinCounter(char *pcpUaft, char *pcpCKIC, char * pcpFirstOpen, char *pcpLastClose)
{   /* identify check-in counter by CKES < STOD < CKES + 30min, clSplit should avoid '-' since it's used for date  */ 
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
	
    sprintf(pclCkicList, "%s %s %s", pclCkif, pcgSplit, pclCkit);

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
/*--------------------------------------------------------------------------------------------------------*/

#define NUMBER_TYPE    (1)
#define DATE_TYPE      (2)
#define DATELOC_TYPE   (3)
#define VARCHAR2_TYPE  (4)

static char *prgTypeTxt[] = {  "NUMBER", "DATE", "DATELOC", "VARCHAR2", NULL }; /* NULL to end array  */
static int    rgTypeDef[] = {  NUMBER_TYPE, DATE_TYPE, DATELOC_TYPE, VARCHAR2_TYPE,  0 };

int notyet(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{	
	dbg(TRACE,"<notyet>: %s.%s = <%s>, pcpDestType = <%s>, pclCond1 = <%s>, pclCond2 = <%s>", 
						 rpLine->pclSourceTable, rpLine->pclSourceField, pcpSourceValue, rpLine->pclDestFieldType, rpLine->pclCond1, rpLine->pclCond2);
	int ilRC = RC_FAIL;    
    char *pclFunc = "notyet";
	int ilType = 0; 
	
	strcpy(pcpDestValue,"");
	if (strstr(rpLine->pclDestFieldType,"DATE") != 0)
		strcpy(pcpDestValue,"2014-06-20");

	#if 1 	/* test ACP_ALLOCATIONS table */
	if (strstr(rpLine->pclDestField, "AIRLINES") != NULL)
		strcpy(pcpDestValue, "\"OS\",\"LH\"");
	if (strstr(rpLine->pclDestField, "DOM_INT") != NULL)
		strcpy(pcpDestValue, "D,I,T,P,\"S\"");
	if (strstr(rpLine->pclDestField, "NATURE") != NULL)
		strcpy(pcpDestValue,"\"PAX\",\"CHT\",\"PVT\"");	
	#endif
	
	
	#if 0
	ilRC = GetCommandVer2(prgTypeTxt, rgTypeDef, rpLine->pclDestFieldType, &ilType);
	if (ilRC == RC_FAIL) 
	{
		strcpy(pcpDestValue,"X");
	}else
	{
		switch (ilType)
		{
			case NUMBER_TYPE: 
						strcpy(pcpDestValue,"0");
						break;
			case DATELOC_TYPE: 		
						strcpy(pcpDestValue,"2014-06-20");
						break;
			case DATE_TYPE: 
						strcpy(pcpDestValue,"2014-06-20");
						break;
			default: 								
						strcpy(pcpDestValue,"X");
		}
	}
	#endif
	
	ilRC = RC_SUCCESS;	
	return ilRC;
}
/*----------------------------------------------------------------------------------*/
int notnull(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
	int ilRC = RC_FAIL;    
    char *pclFunc = "notnull";
	int ilType = 0; 	
	
	if (pcpSourceValue == NULL || strlen(pcpSourceValue) == 0) 
	{
		ilRC = GetCommandVer2(prgTypeTxt, rgTypeDef, rpLine->pclDestFieldType, &ilType);
		if (ilRC == RC_FAIL) 
		{
			strcpy(pcpDestValue," ");
		}
		else
		{
			switch (ilType)
			{
				case NUMBER_TYPE: 
						strcpy(pcpDestValue,"0");
						break;
				case DATELOC_TYPE: 		
						strcpy(pcpDestValue,"2014-06-20");
						break;
				case DATE_TYPE: 
						strcpy(pcpDestValue,"2014-06-20");
						break;
				case VARCHAR2_TYPE:											
						strcpy(pcpDestValue," ");
						break;
			}
		}		
		ilRC = RC_SUCCESS;
	}else
	{
		ilRC = defaultOperator(pcpDestValue,pcpSourceValue,rpLine,pcpSelection,pcpAdid);		
	}
	dbg(DEBUG, "%s: pcpDestValue = <%s>, ilRC = <%d:%d,%d>", pclFunc, ilRC, RC_SUCCESS, RC_FAIL);
	return ilRC;
}
/*-------------------------------------------------------------------------------------------*/
int transtype(char *pcpDestValue, char *pcpSourceValue, _LINE * rpLine, char * pcpSelection, char *pcpAdid)
{
	int ilRc = RC_SUCCESS;
	char *pclFunc = "transtype";
	
	dbg(DEBUG,"%s: pclDestField = <%s>, igTransCmd = <%d:%d,%d,%d>, igEnableIgnoreDelete = <%d:%d,%d>, cgTransTypeValue = <%s>", 
			  pclFunc, rpLine->pclDestField, igTransCmd, INSERT, UPDATE, DELETE, igEnableIgnoreDelete, TRUE, FALSE, cgTransTypeValue);
	
	if (igEnableIgnoreDelete == FALSE) /* not store transaction type case */ 
	{
		ilRc = defaultOperator(pcpDestValue, pcpSourceValue, rpLine, pcpSelection, pcpAdid); 
	}else
		switch (igTransCmd)
		{
			case INSERT: 
						get_real_item(pcpDestValue, cgTransTypeValue, 1); 
						break;
			case UPDATE: 
						get_real_item(pcpDestValue, cgTransTypeValue, 2);
						break;
			case DELETE: 
						get_real_item(pcpDestValue, cgTransTypeValue, 3);
						break;
			default: 	
						strcpy(pcpDestValue,""); 
						ilRc = RC_FAIL; 
		}
	dbg(DEBUG, "%s: ilRc = <%d>, pcpDestValue = <%s>", pclFunc, ilRc, pcpDestValue);
	return ilRc;
}
