
#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/tbthdl.c 1.22 2014/07/09 17:05:44:14SGT fya Exp  $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* ABB ACE/FC Program Skeleton                                                */
/*                                                                            */
/* Author         :                                                           */
/* Date           :                                                           */
/* Description    :                                                           */
/*                                                                            */
/* Update history :                                                           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */

static char sccs_version[] ="@(#) UFIS44 (c) ABB AAT/I skeleton.c 44.1.0 / 11.12.2002 HEB";

/* be carefule with strftime or similar functions !!!                         */
/*                                                                            */
/******************************************************************************/
/* This program is a MIKE main program */

#define U_MAIN
#define UGCCS_PRG
#define STH_USE

#define _TEST_
#define _DEBUG_


#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "ugccsma.h"
#include "msgno.h"
#include "glbdef.h"
#include "quedef.h"
#include "uevent.h"
#include "sthdef.h"
#include "debugrec.h"
#include "hsbsub.h"
#include "fditools.h"
#include "libccstr.h"
#include "l_ccstime.h"
#include "rmschkrul.h"
#include "syslib.h"
#include "db_if.h"
#include <time.h>
#include <cedatime.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include "tools.h"
#include "helpful.h"
#include "timdef.h"
#include "urno_fn.h"

/*fya 0.1*/
#include "tbthdl.h"
#include "tbthdl_sub.c"
#include "tbthdl_customized.c"

/*#include "urno_fn.inc"*/
#include "tbthdl_sub_group2.c"
#include "ght_hash_table.h"
#include "hash_table.c"
#include "hash_functions.c"

#if defined(_HPUX_SOURCE)  || defined(_AIX)
	extern int daylight;
	extern long timezone;
	extern char *tzname[2];
#else
	extern int _daylight;
	extern long _timezone;
	extern char *_tzname[2];
#endif

int debug_level = 0;
/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */
static int   igItemLen     = 0;                   /* length of incoming item */
static EVENT *prgOutEvent  = NULL;

static char cgProcessName[20] = "\0";
static char  cgConfigFile[512] = "\0";
char  cgHopo[8] = "\0";                         /* default home airport    */
static char  cgTabEnd[8] ="\0";                       /* default table extension */

static long lgEvtCnt = 0;

/*entry's from configfile*/
static char pcgConfigFile[512];
static char pcgCfgBuffer[512];
static char pcgCurrentTime[32];
static int igDiffUtcToLocal;
static char pcgServerChars[110];
static char pcgClientChars[110];
static char pcgUfisConfigFile[512];
static char pcgDeletionStatusIndicator[512];
static char pcgDelValue[64];

ght_hash_table_t *pcgHash_table = NULL;

/*fya 0.1*/
_RULE rgRule;
_LINE rgGroupInfo[ARRAYNUMBER];
static int igTotalLineOfRule;
static int igTotalNumbeoOfRule;
static int igRuleSearchMethod;
static int igEnableRotation;

static char pcgSourceFiledSet[GROUPNUMER][LISTLEN];
static char pcgSourceFiledList[GROUPNUMER][LISTLEN];
static char pcgDestFiledList[GROUPNUMER][LISTLEN];
static char pcgSourceConflictFiledSet[GROUPNUMER][LISTLEN];
static char pcgNullableIndicator[64];

static char pcgCurrent_Arrivals[64];
static char pcgCurrent_Departures[64];

static char pcgTimeWindowRefField_AFTTAB_Arr[8];
static char pcgTimeWindowRefField_AFTTAB_Dep[8];
static char pcgTimeWindowRefField_CCATAB[8];
static int igTimeWindowUpperLimit;
static int igTimeWindowLowerLimit;
static int igGetDataFromSrcTable;
static int igEnableCodeshare;
static int igInitTable;
static int igDataListDelimiter;
static char pcgDataListDelimiter[2];
extern char pcgDateFormatDelimiter[4];
extern char pcgMultiSrcFieldDelimiter[4];
static char pcgDestFlnoField[64];
/*static int igTimeDifference;*/

static char pcgTimeWindowLowerLimit[64];
static char pcgTimeWindowUpperLimit[64];
static int igTimeWindowInterval[64];
/*------- tvo_add -------------------------------------------------*/
#define MAX_GROUP_RULE   16

typedef struct
{
	char pclGroupConfStr[512];
	int  pilGroupId;
} _GROUPID;
int  igTotalGroupConf;
_GROUPID  rgGroupIdConf[MAX_GROUP_RULE + 1];
/*extern int  igTransCmd;
extern int  igEnableIgnoreDelete;
extern char cgTransTypeValue[128]; */

static char cgTransFieldName[128];
static void showCodeFunc();
static char pcgTimeWindowDeleteFieldName[128];
static int  igEnableUpdatePartial;
static int  igEnableInsertPartial;

static int  buildDeleteQueryByTimeWindow(char *pcpSqlBuf, int ipRuleGroup, char* pcpDestFieldName, char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimitCurrent);
static void buildUpdateQueryPartial(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData, char * pcpSelection, int pipRuleGroup);
static void buildInsertQueryPartial(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData);
/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
static int  Init_Process();
static int  ReadConfigEntry(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer);
static int	Reset(void);                       /* Reset program          */
static void	Terminate(int ipSleep);            /* Terminate program      */
static void	HandleSignal(int);                 /* Handles signals        */
static void	HandleErr(int);                    /* Handles general errors */
static void HandleQueErr(int);                 /* Handles queuing errors */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

static int	HandleData(EVENT *prpEvent);       /* Handles event data     */
static int  GetCommand(char *pcpCommand, int *pipCmd);

/*fya 0.1*/
static int getConfig();
static int GetRuleSchema(_RULE *rpRule);
static void getOneline(_LINE *rpLine, char *pcpLine);
static void showLine(_LINE *rpLine);
static void showRule(_RULE *rpRule, int ipTotalLineOfRule);
static void storeRule(_LINE *rpRuleLine, _LINE *rpSingleLine);
/*static int collectSourceFieldSet(char *pcpFiledSet, char * pcpConflictFiledSet, char *pcpSourceField, int ipCount);*/
static int collectSourceFieldSet(char *pcpFiledSet, char * pcpConflictFiledSet, char *pcpSourceField, int ipGroupNumer);
static int collectFieldList(char *pcpFiledSet, char *pcpDestField, int ipGroupNumer);
static int isDisabledLine(char *pcpLine);
int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData);
static int isCommentLine(char *pcpLine);
static int isInTimeWindow(char *pcpTimeVal, char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int toDetermineAppliedRuleGroup(char * pcpTable, char * pcpFields, char * pcpData, char *pcpAdidValue);
static void showFieldByGroup(char (*pcpSourceFiledSet)[LISTLEN], char (*pcpConflictFiledSet)[LISTLEN], char (* pcpSourceFiledList)[LISTLEN], char (* pcpDestFiledSet)[LISTLEN]);
static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList,
                        _RULE *rpRule, int ipTotalLineOfRule, char * pcpSelection, char *pcpAdid, int ilIsMaster,
                        char *pcpHardcodeShare_DestFieldList, _HARDCODE_SHARE pcpHardcodeShare, _QUERY *pcpQuery);
static int matchFieldListOnGroup(int ilRuleGroup, char *pcpSourceFieldList);
static int getSourceFieldData(char *pclTable, char * pcpSourceFieldList, char * pcpSelection, char * pclSourceDataList);
void buildSelQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection);
static int convertSrcValToDestVal(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _LINE * rpLine, char * pcpDestFieldValue, char * pcpSelection, char *pcpAdid);
static void buildInsertQuery(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData);
static void buildUpdateQuery(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData, char * pcpSelection, int ipRuleGroup,int ipMasterFlag);
static void buildDestTabWhereClause( char *pcpSelection, char *pcpDestKey, char *pcpDestFiledList, char *pclDestDataList);
int getRotationFlightData(char *pcpTable, char *pcpUrnoSelection, char *pcpFields, char (*pcpRotationData)[LISTLEN], char *pcpAdid);
void showRotationFlight(char (*pclRotationData)[LISTLEN]);
static int mapping(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpAdidValue, int ipCodeShareChange);
static int buildDataArray(char *pcpFields, char *pcpNewData, char (*pcpDatalist)[LISTLEN], char *pcpSelection);
static int flightSearch(char *pcpTable, char *pcpField, char *pcpKey, char *pcpSelection);
static void getHardcodeShare_DataList(char *pcpFields, char *pcpData, _HARDCODE_SHARE *pcpHardcodeShare);
static int checkCodeShareChange(char *pcpFields, char *pcpNewData, char *pcpOldData, int *ipCodeShareChange);
static int refreshTable(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int iniTables(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit,int ipOptionToTruncate);
static int getFieldValue(char *pcpFields, char *pcpNewData, char *pcpFieldName, char *pcpFieldValue);
static int truncateTable(int ipRuleGroup);
static int deleteFlightsOutOfTimeWindow(char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimit);
static int deleteFlightsOutOfTimeWindowByGroup(int ipRuleGroup, char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimit);
static void insertFligthsNewInTimeWindow(char *pcpTimeWindowUpperLimitOriginal, char *pcpTimeWindowUpperLimitCurrent);
static void updateTimeWindow(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int showGroupInfo(_LINE *rlGroupInfo, int ipGroupNO);
static int getRefTimeFiledValueFromSource(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpTimeWindowRefFieldVal, char *pcpTimeWindowRefField);
static void replaceDelimiter(char *pcpConvertedDataList, char *pcpOriginalDataList, char pcpOrginalDelimiter, char pcpConvertedDelimiter);
static int runTestData();
static int getRuleIndex_BrutalForce(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _RULE *rpRule, int ipRuleGroup, char *pcpFields,int ipTotalLineOfRule,char *pcpData);
static void buildConvertedDataList(char *pcpDetType, char *pcpDestDataList, char *pcpDestFieldValue, int ipOption);
static int buildHash(int ipTotalLineOfRule, _RULE rpRule, ght_hash_table_t **pcpHash_table);
static int getRuleIndex_Hash(int ipRuleGroup, char *pcpDestFieldName, char *pcpFields, char *pcpData, char *pcpSourceFieldName, char *pcpSourceFieldValue);
static int checkNullable(char *pcpDestFieldValue, _LINE *rpLine);
static void insertFligthts(int ipStartIndex, int ipDataListNo, _QUERY *pcpQuery);
static int updateAllFlights(_QUERY *pcpQuery, int ipDataListNo, int ipIsCodeShareChange);
static void deleteCodeShareFligths(char *pcpDestTable ,char *pcpDestKey, char *pcpUrno);
static void deleteFlight(char *pcpDestTable, char *pcpDestKey, char *pcpUrno);
/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
	int	ilRc = RC_SUCCESS;			/* Return code			*/
	int	ilCnt = 0;
	int ilOldDebugLevel = 0;

	INITIALIZE;			/* General initialization	*/

	debug_level = TRACE;

	strcpy(cgProcessName,argv[0]);

	dbg(TRACE,"MAIN: version <%s>",sccs_version);

	/* Attach to the MIKE queues */
	do
	{
		ilRc = init_que();
		if(ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"MAIN: init_que() failed! waiting 6 sec ...");
			sleep(6);
			ilCnt++;
		}/* end of if */
	}while((ilCnt < 10) && (ilRc != RC_SUCCESS));

	if(ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: init_que() failed! waiting 60 sec ...");
		sleep(60);
		exit(1);
	}else{
		dbg(TRACE,"MAIN: init_que() OK! mod_id <%d>",mod_id);
	}/* end of if */

	do
	{
		ilRc = init_db();
		if (ilRc != RC_SUCCESS)
		{
			check_ret(ilRc);
			dbg(TRACE,"MAIN: init_db() failed! waiting 6 sec ...");
			sleep(6);
			ilCnt++;
		} /* end of if */
	} while((ilCnt < 10) && (ilRc != RC_SUCCESS));

	if(ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: init_db() failed! waiting 60 sec ...");
		sleep(60);
		exit(2);
	}else{
		dbg(TRACE,"MAIN: init_db() OK!");
	} /* end of if */

	/* logon to DB is ok, but do NOT use DB while ctrl_sta == HSB_COMING_UP !!! */

	sprintf(cgConfigFile,"%s/%s",getenv("BIN_PATH"),mod_name);
	dbg(TRACE,"ConfigFile <%s>",cgConfigFile);
	ilRc = TransferFile(cgConfigFile);
	if(ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: TransferFile(%s) failed!",cgConfigFile);
	} /* end of if */

	sprintf(cgConfigFile,"%s/%s.cfg",getenv("CFG_PATH"),mod_name);
	dbg(TRACE,"ConfigFile <%s>",cgConfigFile);
	ilRc = TransferFile(cgConfigFile);
	if(ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: TransferFile(%s) failed!",cgConfigFile);
	} /* end of if */

	ilRc = SendRemoteShutdown(mod_id);
	if(ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: SendRemoteShutdown(%d) failed!",mod_id);
	} /* end of if */

	if((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
	{
		dbg(DEBUG,"MAIN: waiting for status switch ...");
		HandleQueues();
	}/* end of if */

	if((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
	{
		dbg(TRACE,"MAIN: initializing ...");
			ilRc = Init_Process();
			if(ilRc != RC_SUCCESS)
			{
				dbg(TRACE,"Init_Process: init failed!");
			} /* end of if */

	} else {
		Terminate(30);
	}/* end of if */

	dbg(TRACE,"=====================");
	dbg(TRACE,"MAIN: initializing OK");
	dbg(TRACE,"Vorsicht, ich bin nur ein Skeleton und tue nichts.......... ");
	dbg(TRACE,"=====================");

    /*
    runTestData();    // run test data for AFTTAB and CCATAB
    */

	for(;;)
	{
		ilRc = que(QUE_GETBIG,0,mod_id,PRIORITY_3,igItemLen,(char *)&prgItem);
		/* Acknowledge the item */
		ilRc = que(QUE_ACK,0,mod_id,0,0,NULL);
		if( ilRc != RC_SUCCESS )
		{
			/* handle que_ack error */
			HandleQueErr(ilRc);
		} /* fi */

		/* depending on the size of the received item  */
		/* a realloc could be made by the que function */
		/* so do never forget to set event pointer !!! */

		prgEvent = (EVENT *) prgItem->text;

		if( ilRc == RC_SUCCESS )
		{
			/*lgEvtCnt++;*/

			switch( prgEvent->command )
			{
			case	HSB_STANDBY	:
				ctrl_sta = prgEvent->command;
				HandleQueues();
				break;
			case	HSB_COMING_UP	:
				ctrl_sta = prgEvent->command;
				HandleQueues();
				break;
			case	HSB_ACTIVE	:
				ctrl_sta = prgEvent->command;
				break;
			case	HSB_ACT_TO_SBY	:
				ctrl_sta = prgEvent->command;
				/* CloseConnection(); */
				HandleQueues();
				break;
			case	HSB_DOWN	:
				/* whole system shutdown - do not further use que(), send_message() or timsch() ! */
				ctrl_sta = prgEvent->command;
				Terminate(1);
				break;
			case	HSB_STANDALONE	:
				ctrl_sta = prgEvent->command;
				ResetDBCounter();
				break;
			case	REMOTE_DB :
				/* ctrl_sta is checked inside */
				HandleRemoteDB(prgEvent);
				break;
			case	SHUTDOWN	:
				/* process shutdown - maybe from uutil */
				Terminate(1);
				break;

			case	RESET		:
				ilRc = Reset();
				break;

			case	EVENT_DATA	:
				if((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
				{
					ilRc = HandleData(prgEvent);
					if(ilRc != RC_SUCCESS)
					{
						HandleErr(ilRc);
					}/* end of if */
				}else{
					dbg(TRACE,"MAIN: wrong hsb status <%d>",ctrl_sta);
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
				}/* end of if */
				break;


			case	TRACE_ON :
				dbg_handle_debug(prgEvent->command);
				break;
			case	TRACE_OFF :
				dbg_handle_debug(prgEvent->command);
				break;
			default			:
				dbg(TRACE,"MAIN: unknown event");
				DebugPrintItem(TRACE,prgItem);
				DebugPrintEvent(TRACE,prgEvent);
				break;
			} /* end switch */

		} else {
			/* Handle queuing errors */
			HandleQueErr(ilRc);
		} /* end else */

	} /* end for */

} /* end of MAIN */
/*-----------------------------------------------------------------------------*/
static int runTestData()
{
	char pclTimeNow[TIMEFORMAT] = "\0";
	char clTestTime[20];
	char clSelection[256];
    char clFields[512] = "URNO,ADID,STOA,STOD,TIFA,TIFD,ALC2";
    char clTable[32] = "AFTTAB";
    char clData[1024];
	char pclSqlBuf[512];

	debug_level = DEBUG;        /* set debug level to FULL mode */

	GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
	sprintf(clTestTime, "20140624%s", &pclTimeNow[8]);
    /*--------- tvo_test ------------------------------------------------------*/
    /* select URNO,ADID,STOA,STOD,TIFA,TIFD,ALC2 from afttab where (ADID = 'D') and (STOD like '20140612%') and (rownum < 5); */
    /* select URNO,ADID,STOA,STOD,TIFA,TIFD,ALC2 from afttab where (ADID = 'A') and (STOA like '20140612%') and (rownum < 5); */

    #if 0
	/* azapp test departure flight */
	strcpy(clSelection,"WHERE URNO = 971250116\n971250116");
    strcpy(clData,"971250116,D,20140611185500,20140611101500,20140611185501,20140611101500,AZ\n");

	sprintf(clSelection, "WHERE URNO = %s\n%s", "1882985177","1882985177");   /* satsvm test departure */
    strcpy(clData,"1882985177,D,20140612065000,20140612052000,20140612065000,20140612052000,MI\n");
    /*----route_id, orig_id, Pdst_name, Porg_name, Prcv_name, Pseq_id, Pref_seq_id, Ptw_start, Ptw_end,  -------------------*/
    ilRc = tools_send_info_flag( mod_id, mod_id , mod_name , mod_name, mod_name, "","","","", "UFR", "AFTTAB",clSelection, clFields, clData, NETOUT_NO_ACK);

    /* azapp test arrival flight */
    sprintf(clSelection, "WHERE URNO = %s\n971286966", "971286966");
    strcpy(clData,"971286966,A,20140611100501,20140611075000,20140611100500,20140611075000,KL\n971286966,A,20140611100500,20140611075000,20140611100500,20140611075000,KL");
    sprintf(clSelection,"WHERE URNO = %s\n%s","1889950130","1889950130");   /* satsvm test arrival */
    strcpy(clData,"1889950130,A,20140612001000,20140611223500,20140612001000,20140611223500,SQ\n");
    ilRc = tools_send_info_flag( mod_id, mod_id , mod_name , mod_name, mod_name, "","","","","UFR", "AFTTAB",clSelection, clFields, clData, NETOUT_NO_ACK);
	#endif

	#if 0     /* yyzvm test UFR */
	strcpy(pclSqlBuf,"update afttab set remp = 'ABC', GTD1 = 'E76', GD1X = '20140401010539', WRO1 = 'SKL', BLT1 = 'RD04'  where URNO=932911970");
	int ilRc = RunSQL(pclSqlBuf, clData);
	if (ilRc == RC_SUCCESS)
		dbg(DEBUG,"<tvo> RunSQL ok, clData = <%s>", clData);

	strcpy(pclSqlBuf,"select mff from current_departures where did = 932121610");
	ilRc = RunSQL(pclSqlBuf, clData);
	if (ilRc == RC_SUCCESS)
		dbg(DEBUG,"<tvo> RunSQL ok, clData = <%s>", clData);

    sprintf(clSelection, "WHERE URNO = 932911970");		/* yyzvm departure flight */
	sprintf(clData,"932911970,D,,%s,%s,%s,LH", clTestTime,clTestTime,clTestTime);
    /*----route_id, orig_id, Pdst_name, Porg_name, Prcv_name, Pseq_id, Pref_seq_id, Ptw_start, Ptw_end,  -------------------*/
	ilRc = tools_send_info_flag( 7800, mod_id , mod_name , mod_name, mod_name, "","","","", "UFR", "AFTTAB",clSelection, clFields, clData, NETOUT_NO_ACK);

	sprintf(clSelection, "WHERE URNO = 933334954");   /* yyzvm arrival flight */
	sprintf(clData,"933334954,A,%s,,%s,%s,5Y", clTestTime,clTestTime,clTestTime);
    ilRc = tools_send_info_flag( 7800, mod_id , mod_name , mod_name, mod_name, "","","","","UFR", "AFTTAB",clSelection, clFields, clData, NETOUT_NO_ACK);
	#endif

	#if 0      /* testing IFR for arrival */
	strcpy(clTable, "AFTTAB");
	strcpy(clFields, "ACT3,ACT5,ALC2,ALC3,DES3,DES4,FLNO,FLTN,FTYP,JCNT,ORG3,ORG4,RTYP,STOA,STTY,TTYP,REM1");
	sprintf(clData,"A4F,A124,9W,JAI,YYZ,CYYZ,9W 44444 ,44444,S,0,ATH,LGAV,S,%s,U,03,tvo_test_arrival", pclTimeNow);
	strcpy(clSelection, "WHERE 20140701,20140701,2,1,20140701");   /* the same for arrival and departure */
	ilRc = tools_send_info_flag( 7805, mod_id , mod_name , mod_name, mod_name, "","","","","ISF", clTable, clSelection, clFields, clData, NETOUT_NO_ACK);

	strcpy(clFields, "ACT3,ACT5,ALC2,ALC3,DES3,DES4,DSSF,FLNO,FLTN,FTYP,HOPO,JCNT,ORG3,ORG4,PDBS,PDES,RTYP,STOD,STTY,STYP,TTYP,VIAN,NOSE,BAO1,BAC1,BAO4,BAC4,ADID,CSGN,ESBT,SESB,REM1");
    /* "A81,A148,AC,ACA,YYC,CYYC,  U,ACA11111 ,11111,S,YYZ,0,YYZ,CYYZ,20140701003100,20140701010100,S,20140701010100,0,J,03,0,000, , , , ,D,ACA1163,20140701003100,S" */
	sprintf(clData, "A81,A148,AC,ACA,YYC,CYYC,  U,ACA11111 ,11111,S,YYZ,0,YYZ,CYYZ,%s,%s,S,%s,0,J,03,0,000, , , , ,D,ACA1163,%s,S,tvo_test_departure", pclTimeNow,pclTimeNow,pclTimeNow);
	ilRc = tools_send_info_flag( 7805, mod_id , mod_name , mod_name, mod_name, "","","","","ISF", clTable, clSelection, clFields, clData, NETOUT_NO_ACK);
	#endif

	#if 0
    strcpy(clTable,"CCATAB");
    strcpy(clSelection,"WHERE URNO = 934377037\n934377037");
    strcpy(clFields,"URNO,CKBS,CKES");
    sprintf(clData,"934377037,%s,%s\n",clTestTime,clTestTime);
    tools_send_info_flag( 506, mod_id , mod_name , mod_name, mod_name, "","","","","URT", clTable,clSelection, clFields, clData, NETOUT_NO_ACK);
	#endif

	#if 0     /* test hash table */
	ght_hash_table_t *p_table;
	int  *p_data;
	int  *p_he;
	char clHashKey[128];
	int  ilCount;

	p_table = ght_create(igTotalLineOfRule);

	dbg(DEBUG,"<tvo> igTotalLineOfRule = <%d>, igTotalNumbeoOfRule = <%d>", igTotalLineOfRule, igTotalNumbeoOfRule);

	for (ilCount = 0; ilCount < igTotalLineOfRule; ilCount++)
	{
		if ( !(p_data = (int*)malloc(sizeof(int))) )
				return -1;
		/* Assign the data a value */
		*p_data = ilCount;
		sprintf(clHashKey,"%s%s", rgRule.rlLine[ilCount].pclRuleGroup, rgRule.rlLine[ilCount].pclDestField);
		/* Insert "blabla" into the hash table */
		ght_insert(p_table, p_data, sizeof(char)*strlen(clHashKey), clHashKey);
		dbg(DEBUG, "<tvo> clHashKey = <%s>, ilCount = <%d>", clHashKey, ilCount);
	}
	/* Search for dest field */
	strcpy(clHashKey, "2 LATEST_DATE");
	if ( (p_he = ght_get(p_table,sizeof(char)*strlen(clHashKey), clHashKey)) != NULL )
		dbg(DEBUG,"Found <%s> at %d\n", clHashKey, *p_he);
	else
		dbg(DEBUG,"Did not find <%s> !\n", clHashKey);

	strcpy(clHashKey, "2LATEST_DATE");
	if ( (p_he = ght_get(p_table,sizeof(char)*strlen(clHashKey), clHashKey)) != NULL )
		dbg(DEBUG,"Found <%s> at %d\n", clHashKey, *p_he);
	else
		dbg(DEBUG,"Did not find <%s> !\n", clHashKey);
	/* Remove the hash table */
	ght_finalize(p_table);
	#endif

	#if 0
	showCodeFunc();
	#endif

	strcpy(clFields, "ACFR,ACT3,ACTO,CDAT,DETY,GATE,HOPO,JOUR,LSTU,PLFR,PLTO,POSI,REGN,STAT,TEXT,UAFT,UAID,UALO,UDEL,UDRD,UDSR,UJTY,URNO,USEC,USEU,USTF,TTGF,TTGT,UEQU,UTPL,ALID,ALOC,FCCO,UDEM,UGHS");
	strcpy(clData, "20140707045000,320,20140707060000,20140707121127,2,     ,SIN,1324338230,              ,20140707045000,20140707060000,     ,            ,P 0       ,                                                                                                                                ,2121101107,          ,1114713   ,0         ,0         ,1324281995,2000      ,1328166755,JOBHDL/JFK                      ,                                ,127015970 ,300  ,300  ,0         ,113682507 ,              ,PST       ,          ,2121102930,          ");

	char pclTmpField[10];
    char pclTmpData[256];
	int  ilNoEle = (int) GetNoOfElements(clFields, ',');
	int  ilIndex;

	for (ilIndex = 1; ilIndex <= ilNoEle; ilIndex++)
	{
		get_real_item(pclTmpField,clFields,ilIndex);
		get_real_item(pclTmpData,clData,ilIndex);

		dbg(DEBUG, "ilIndex = <%d>, <%s> = <%s>", ilIndex, pclTmpField, pclTmpData);
	}
	dbg(DEBUG, "-------------------- end of tvo_test ---------------------------------");
    /*-------------------------------------------------------------------------*/
}
/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/



static int ReadConfigEntry(char *pcpSection,char *pcpKeyword, char *pcpCfgBuffer)
{
    int ilRc = RC_SUCCESS;

    char clSection[124] = "\0";
    char clKeyword[124] = "\0";

    strcpy(clSection,pcpSection);
    strcpy(clKeyword,pcpKeyword);

    ilRc = iGetConfigEntry(cgConfigFile,clSection,clKeyword,
			   CFG_STRING,pcpCfgBuffer);
    if(ilRc != RC_SUCCESS)
    {
	dbg(TRACE,"Not found in %s: <%s> <%s>",cgConfigFile,clSection,clKeyword);
    }
    else
    {
	dbg(DEBUG,"Config Entry <%s>,<%s>:<%s> found in %s",
	    clSection, clKeyword ,pcpCfgBuffer, cgConfigFile);
    }/* end of if */
    return ilRc;
}

static int Init_Process()
{
	int  ilRc = RC_SUCCESS;			/* Return code */
    char clSection[64] = "\0";
    char clKeyword[64] = "\0";
    int ilOldDebugLevel = 0;
	long pclAddFieldLens[12];
	char pclAddFields[256] = "\0";
	char pclSqlBuf[2560] = "\0";
	char pclSelection[1024] = "\0";
 	short slCursor = 0;
	short slSqlFunc = 0;
	char  clBreak[24] = "\0";
    char *pclFunc = "Init_Process";

	if(ilRc == RC_SUCCESS)
	{
		/* read HomeAirPort from SGS.TAB */
		ilRc = tool_search_exco_data("SYS", "HOMEAP", cgHopo);
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"<Init_Process> EXTAB,SYS,HOMEAP not found in SGS.TAB");
			Terminate(30);
		} else {
			dbg(TRACE,"<Init_Process> home airport <%s>",cgHopo);
		}
	}

	if(ilRc == RC_SUCCESS)
	{

		ilRc = tool_search_exco_data("ALL","TABEND", cgTabEnd);
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"<Init_Process> EXTAB,ALL,TABEND not found in SGS.TAB");
			Terminate(30);
		} else {
			dbg(TRACE,"<Init_Process> table extension <%s>",cgTabEnd);
		}
	}

	/*fya 0.1*/
    if (ilRc = getConfig() == RC_FAIL)
	{
		dbg(TRACE,"<%s>: Configuration error, terminating",pclFunc);
		Terminate(30);
	}

	buildHash(igTotalLineOfRule, rgRule, &pcgHash_table);

	if (ilRc != RC_SUCCESS)
	{
	    dbg(TRACE,"Init_Process failed");
	}

	dbg(TRACE,"%s Update the time window according to the received TMW command from ntisch",pclFunc);
    updateTimeWindow(pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit);

    if(igInitTable == TRUE)
    {
        iniTables(pcgTimeWindowLowerLimit,pcgTimeWindowUpperLimit,TRUE);
    }

    return(ilRc);

} /* end of initialize */

/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{
	int	ilRc = RC_SUCCESS;				/* Return code */

	dbg(TRACE,"Reset: now resetting");

	return ilRc;

} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(int ipSleep)
{
	/* unset SIGCHLD ! DB-Child will terminate ! */

	dbg(TRACE,"Terminate: now DB logoff ...");

	signal(SIGCHLD,SIG_IGN);

	logoff();

	dbg(TRACE,"Destroy Hashtable");
	ght_finalize(pcgHash_table);

	dbg(TRACE,"Terminate: now sleep(%d) ...",ipSleep);
	dbg(TRACE,"Terminate: now leaving ...");
	fclose(outp);

	sleep(ipSleep < 1 ? 1 : ipSleep);

	exit(0);

} /* end of Terminate */

/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/

static void HandleSignal(int pipSig)
{
	dbg(TRACE,"HandleSignal: signal <%d> received",pipSig);

	switch(pipSig)
	{
	default	:
		Terminate(1);
		break;
	} /* end of switch */

	exit(1);

} /* end of HandleSignal */

/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int pipErr)
{
	dbg(TRACE,"HandleErr: <%d> : unknown ERROR",pipErr);

	return;
} /* end of HandleErr */

/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr(int pipErr)
{
	switch(pipErr) {
	case	QUE_E_FUNC	:	/* Unknown function */
		dbg(TRACE,"<%d> : unknown function",pipErr);
		break;
	case	QUE_E_MEMORY	:	/* Malloc reports no memory */
		dbg(TRACE,"<%d> : malloc failed",pipErr);
		break;
	case	QUE_E_SEND	:	/* Error using msgsnd */
		dbg(TRACE,"<%d> : msgsnd failed",pipErr);
		break;
	case	QUE_E_GET	:	/* Error using msgrcv */
		dbg(TRACE,"<%d> : msgrcv failed",pipErr);
		break;
	case	QUE_E_EXISTS	:
		dbg(TRACE,"<%d> : route/queue already exists ",pipErr);
		break;
	case	QUE_E_NOFIND	:
		dbg(TRACE,"<%d> : route not found ",pipErr);
		break;
	case	QUE_E_ACKUNEX	:
		dbg(TRACE,"<%d> : unexpected ack received ",pipErr);
		break;
	case	QUE_E_STATUS	:
		dbg(TRACE,"<%d> :  unknown queue status ",pipErr);
		break;
	case	QUE_E_INACTIVE	:
		dbg(TRACE,"<%d> : queue is inaktive ",pipErr);
		break;
	case	QUE_E_MISACK	:
		dbg(TRACE,"<%d> : missing ack ",pipErr);
		break;
	case	QUE_E_NOQUEUES	:
		dbg(TRACE,"<%d> : queue does not exist",pipErr);
		break;
	case	QUE_E_RESP	:	/* No response on CREATE */
		dbg(TRACE,"<%d> : no response on create",pipErr);
		break;
	case	QUE_E_FULL	:
		dbg(TRACE,"<%d> : too many route destinations",pipErr);
		break;
	case	QUE_E_NOMSG	:	/* No message on queue */
		dbg(TRACE,"<%d> : no messages on queue",pipErr);
		break;
	case	QUE_E_INVORG	:	/* Mod id by que call is 0 */
		dbg(TRACE,"<%d> : invalid originator=0",pipErr);
		break;
	case	QUE_E_NOINIT	:	/* Queues is not initialized*/
		dbg(TRACE,"<%d> : queues are not initialized",pipErr);
		break;
	case	QUE_E_ITOBIG	:
		dbg(TRACE,"<%d> : requestet itemsize to big ",pipErr);
		break;
	case	QUE_E_BUFSIZ	:
		dbg(TRACE,"<%d> : receive buffer to small ",pipErr);
		break;
	default			:	/* Unknown queue error */
		dbg(TRACE,"<%d> : unknown error",pipErr);
		break;
	} /* end switch */

	return;

} /* end of HandleQueErr */

/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues()
{
	int	ilRc = RC_SUCCESS;			/* Return code */
	int	ilBreakOut = FALSE;

	do
	{
		ilRc = que(QUE_GETBIG,0,mod_id,PRIORITY_3,igItemLen,(char *)&prgItem);
		/* depending on the size of the received item  */
		/* a realloc could be made by the que function */
		/* so do never forget to set event pointer !!! */
		prgEvent = (EVENT *) prgItem->text;

		if( ilRc == RC_SUCCESS )
		{
			/* Acknowledge the item */
			ilRc = que(QUE_ACK,0,mod_id,0,0,NULL);
			if( ilRc != RC_SUCCESS )
			{
				/* handle que_ack error */
				HandleQueErr(ilRc);
			} /* fi */

			switch( prgEvent->command )
			{
			case	HSB_STANDBY	:
				ctrl_sta = prgEvent->command;
				break;

			case	HSB_COMING_UP	:
				ctrl_sta = prgEvent->command;
				break;

			case	HSB_ACTIVE	:
				ctrl_sta = prgEvent->command;
				ilBreakOut = TRUE;
				break;

			case	HSB_ACT_TO_SBY	:
				ctrl_sta = prgEvent->command;
				break;

			case	HSB_DOWN	:
				/* whole system shutdown - do not further use que(), send_message() or timsch() ! */
				ctrl_sta = prgEvent->command;
				Terminate(1);
				break;

			case	HSB_STANDALONE	:
				ctrl_sta = prgEvent->command;
				ResetDBCounter();
				ilBreakOut = TRUE;
				break;

			case	REMOTE_DB :
				/* ctrl_sta is checked inside */
				HandleRemoteDB(prgEvent);
				break;

			case	SHUTDOWN	:
				Terminate(1);
				break;

			case	RESET		:
				ilRc = Reset();
				break;

			case	EVENT_DATA	:
				dbg(TRACE,"HandleQueues: wrong hsb status <%d>",ctrl_sta);
				DebugPrintItem(TRACE,prgItem);
				DebugPrintEvent(TRACE,prgEvent);
				break;

			case	TRACE_ON :
				dbg_handle_debug(prgEvent->command);
				break;

			case	TRACE_OFF :
				dbg_handle_debug(prgEvent->command);
				break;

			default			:
				dbg(TRACE,"HandleQueues: unknown event");
				DebugPrintItem(TRACE,prgItem);
				DebugPrintEvent(TRACE,prgEvent);
				break;
			} /* end switch */
		} else {
			/* Handle queuing errors */
			HandleQueErr(ilRc);
		} /* end else */
	} while (ilBreakOut == FALSE);

			ilRc = Init_Process();
			if(ilRc != RC_SUCCESS)
			{
				dbg(TRACE,"InitDemhdl: init failed!");
			}


} /* end of HandleQueues */

/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData(EVENT *prpEvent)
{
	int	   ilRc           = RC_SUCCESS;			/* Return code */
	int    ilCmd          = 0;
	char *pclFunc = "HandleData";

	BC_HEAD *prlBchead       = NULL;
	CMDBLK  *prlCmdblk       = NULL;
	char    *pclSelection    = NULL;
	char    *pclFields       = NULL;
	char    *pclData         = NULL;
	char    *pclRow          = NULL;
	char 	clUrnoList[2400];
	char 	clTable[34];
	int		ilUpdPoolJob = TRUE;

    int ilRuleGroup = 0;
	int ilCount = 0;
    int ilCodeShareChange = FALSE;
    char pclAdidValue[2] = "\0";
	char pclTimeWindowRefField[8] = "\0";
    char pclWhereClaues[2048] = "\0";

    char *pclTmpPtr = NULL;
    char pclUrnoSelection[256] = "\0";
    char pclSelectionTmp[256] = "\0";
    char pclNewData[512000] = "\0";
    char pclOldData[512000] = "\0";
    char pclRotationData[ARRAYNUMBER][LISTLEN] = {"\0", "\0"};

	prlBchead    = (BC_HEAD *) ((char *)prpEvent + sizeof(EVENT));
	prlCmdblk    = (CMDBLK *)  ((char *)prlBchead->data);
	pclSelection = prlCmdblk->data;
	pclFields    = (char *)pclSelection + strlen(pclSelection) + 1;
	pclData      = (char *)pclFields + strlen(pclFields) + 1;

    pclTmpPtr = strstr(pclSelection, "\n");
 	if (pclTmpPtr != NULL)
 	{
        *pclTmpPtr = '\0';
        pclTmpPtr++;
        strcpy(pclUrnoSelection, pclTmpPtr);
    }

    strcpy(pclNewData, pclData);

    pclTmpPtr = strstr(pclNewData, "\n");
    if (pclTmpPtr != NULL)
    {
        *pclTmpPtr = '\0';
        pclTmpPtr++;
        strcpy(pclOldData, pclTmpPtr);
    }

	strcpy(clTable,prlCmdblk->obj_name);
	dbg(TRACE,"========== START <%10.10d> ==========",lgEvtCnt);

	/****************************************/
	/*DebugPrintBchead(TRACE,prlBchead);
	DebugPrintCmdblk(TRACE,prlCmdblk);
	 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command"); */
	dbg(TRACE,"Command: <%s>",prlCmdblk->command);
	dbg(TRACE,"Originator follows event = %p ",prpEvent);

	dbg(TRACE,"Originator<%d>",prpEvent->originator);
	/*dbg(TRACE,"selection follows Selection = %p ",pclSelection);*/
	dbg(DEBUG,"Selection: <%s> URNO<%s>",pclSelection,pclUrnoSelection);
	dbg(TRACE,"Fields    <%s>",pclFields);
	dbg(DEBUG,"New Data:  <%s>",pclNewData);
	dbg(DEBUG,"Old Data:  <%s>",pclOldData);
    dbg(DEBUG,"Table:  <%s>",clTable);

    if (strstr(pclSelection,"WHERE") == 0)
    {
        sprintf(pclSelectionTmp, "WHERE URNO=%s",pclUrnoSelection);
    }
    else
    {
        strcpy(pclSelectionTmp,pclSelection);
    }

	/*lgEvtCnt++;*/
	if (strcmp(prlCmdblk->command,"RFH") == 0)
	{
        /*update the time window according to the received RFH command from ntisch*/
        dbg(TRACE,"%s Update the time window according to the received RFH command from ntisch, and refresh the destination table",pclFunc);
        refreshTable(pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit);
	}
    else if (strcmp(prlCmdblk->command,"INI") == 0)
    {
        /*
        1-Delete all records in destination table
        2-Insert into all flights in time-window
        */
        iniTables(pcgTimeWindowLowerLimit,pcgTimeWindowUpperLimit,TRUE);
    }
    else  if ( strcmp(prlCmdblk->command,"DFR") == 0 || strcmp(prlCmdblk->command,"DRT") == 0 )/*DFR command*/
    {
        if( !strcmp(clTable,"AFTTAB"))
        {
            /*getting the ADID*/
            getFieldValue(pclFields, pclNewData, "ADID", pclAdidValue);
        }

        ilRuleGroup = toDetermineAppliedRuleGroup(clTable, pclFields, pclNewData, pclAdidValue);
        if ( ilRuleGroup == 0 )
        {
            dbg(TRACE,"%s ilRuleGroup == 0 -> There is no applied rules", pclFunc);
            ilRc = RC_FAIL;
        }
        else
        {
            dbg(TRACE,"%s Applied rule group is %d", pclFunc, ilRuleGroup);
        }
        deleteFlight(rgGroupInfo[ilRuleGroup].pclDestTable, rgGroupInfo[ilRuleGroup].pclDestKey, pclUrnoSelection);
    }
    else /*The normal IFR and UFR command*/
    {
        /*if(!strcmp(clTable,"AFTTAB"))*/
        if( !strcmp(clTable,"AFTTAB"))
        {
            /*getting the ADID*/
            getFieldValue(pclFields, pclNewData, "ADID", pclAdidValue);

            /*
            So far, the multiple codeshare flights are only applied for group1.
            The critera is that the source table is afttab, and destination tables are Current_Arrivals or Current_Departures
            */
            for (ilCount = 0; ilCount <= igTotalNumbeoOfRule; ilCount++)
            {
                if ( strcmp(rgGroupInfo[ilCount].pclDestTable, "Current_Arrivals"  ) == 0 ||
                     strcmp(rgGroupInfo[ilCount].pclDestTable, "Current_Departures") == 0 )
                {
                    break;
                }
            }

            if(ilCount <= igTotalNumbeoOfRule)
            {
                /*group1*/
                checkCodeShareChange(pclFields,pclNewData,pclOldData,&ilCodeShareChange);
            }
            else
            {
                ilCodeShareChange = CODESHARE_NONCHANGE;
            }
        }
        else
        {
            ilCodeShareChange = CODESHARE_NONCHANGE;
        }

		/* tvo_debug: insert, update, delete transaction type  */
		getTransactionCommand(prlCmdblk->command);

        mapping(clTable, pclFields, pclNewData, pclSelectionTmp, pclAdidValue, ilCodeShareChange);

        if (igEnableRotation == TRUE)
        {
            if(!strcmp(clTable,"AFTTAB"))
            {

                for (ilCount = 0; ilCount <= igTotalNumbeoOfRule; ilCount++)
                {
                    if ( strcmp(rgGroupInfo[ilCount].pclDestTable, "Current_Arrivals"  ) == 0 ||
                         strcmp(rgGroupInfo[ilCount].pclDestTable, "Current_Departures") == 0 )
                    {
                        break;
                    }
                }

                if(ilCount <= igTotalNumbeoOfRule)
                {
                    /*#ifndef FYA*/
                    /*getting the roataion flight data beforehand, optimize this part later*/
                    getRotationFlightData(clTable, pclUrnoSelection, pclFields, pclRotationData, pclAdidValue);

                    if (ilRc == RC_SUCCESS)
                    {
                        showRotationFlight(pclRotationData);

                        /*handle rotation data*/
                        for(ilCount = 0; ilCount < ARRAYNUMBER; ilCount++)
                        {
                            if (strlen(pclRotationData[ilCount]) > 0)
                            {
                                ilRc = extractField(pclUrnoSelection, "URNO", pclFields, pclRotationData[ilCount]);
                                sprintf(pclSelectionTmp, "WHERE URNO=%s", pclUrnoSelection);
                                dbg(DEBUG,"%s <%d> Rotation Flight<%s>-<%s>", pclFunc, ilCount, pclRotationData[ilCount], pclSelectionTmp);
                                /*
                                Since the rotation flight has no old data, then set ilCodeShareChange = CODESHARE_NONCHANGE, the update only bases on its own changes;
                                checkCodeShareChange(pclFields,pclRotationData[ilCount],"",ilCodeShareChange);
                                */
                                ilCodeShareChange = CODESHARE_NONCHANGE;
                                if (strcmp(pclAdidValue, "A") == 0 )
                                    mapping(clTable, pclFields, pclRotationData[ilCount], pclSelectionTmp, "D", ilCodeShareChange);
                                else if (strcmp(pclAdidValue, "D") == 0 )
                                    mapping(clTable, pclFields, pclRotationData[ilCount], pclSelectionTmp, "A", ilCodeShareChange);
                            }
                        }
                    }
                    else
                    {
                        dbg(TRACE,"%s No Rotation Flights",pclFunc);
                    }
                }
            }
        }
        /*#endif*/
    }

    /****************************************/
	dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);
    lgEvtCnt++;

	return RC_SUCCESS;

} /* end of HandleData */

/* fya 0.1***************************************************************/
/* Following the getConfig function										*/
/* Reads the .cfg file and fill in the configuration structure			*/
/* ******************************************************************** */
static int getConfig()
{
    int		ilRC = RC_SUCCESS;
    int		ilI;
    int		ilJ;
    char	pclTmpBuf[128];
    char	pclDebugLevel[128];
    int		ilLen;
    char	pclBuffer[10];
    char	pclClieBuffer[100];
    char	pclServBuffer[100];
    char *pclFunc = "getConfig";
    FILE	*fplFp;

    sprintf(pcgConfigFile,"%s/%s.cfg",getenv("CFG_PATH"),mod_name);
    dbg(TRACE,"Config File is <%s>",pcgConfigFile);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DEBUG_LEVEL",CFG_STRING,pclDebugLevel);
    if (strcmp(pclDebugLevel,"DEBUG") == 0)
    {
        debug_level = DEBUG;
    }
    else
    {
        if (strcmp(pclDebugLevel,"TRACE") == 0)
        {
            debug_level = TRACE;
        }
        else
        {
            if (strcmp(pclDebugLevel,"NULL") == 0)
            {
                debug_level = 0;
            }
            else
            {
                debug_level = TRACE;
            }
        }
    }
    dbg(TRACE,"DEBUG_LEVEL = %s",pclDebugLevel);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_REFERENCE_FIELD_AFTTAB_ARR",CFG_STRING,pcgTimeWindowRefField_AFTTAB_Arr);
    if (ilRC == RC_SUCCESS)
    {
        dbg(DEBUG,"pcgTimeWindowRefField_AFTTAB_Arr<%s>",pcgTimeWindowRefField_AFTTAB_Arr);
    }
    else
    {
        strcpy(pcgTimeWindowRefField_AFTTAB_Arr,"STOA");
        dbg(DEBUG,"pcgTimeWindowRefField_AFTTAB_Arr<%s>",pcgTimeWindowRefField_AFTTAB_Arr);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_REFERENCE_FIELD_AFTTAB_DEP",CFG_STRING,pcgTimeWindowRefField_AFTTAB_Dep);
    if (ilRC == RC_SUCCESS)
    {
        dbg(DEBUG,"pcgTimeWindowRefField_AFTTAB_Dep<%s>",pcgTimeWindowRefField_AFTTAB_Dep);
    }
    else
    {
        strcpy(pcgTimeWindowRefField_AFTTAB_Dep,"STOD");
        dbg(DEBUG,"pcgTimeWindowRefField_AFTTAB_Dep<%s>",pcgTimeWindowRefField_AFTTAB_Dep);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_REFERENCE_FIELD_CCATAB",CFG_STRING,pcgTimeWindowRefField_CCATAB);
    if (ilRC == RC_SUCCESS)
    {
        dbg(DEBUG,"pcgTimeWindowRefField_CCATAB<%s>",pcgTimeWindowRefField_CCATAB);
    }
    else
    {
        strcpy(pcgTimeWindowRefField_CCATAB,"CKBS");
        dbg(DEBUG,"pcgTimeWindowRefField_CCATAB<%s>",pcgTimeWindowRefField_CCATAB);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_LOWER_LIMIT",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        igTimeWindowLowerLimit = atoi(pclTmpBuf);
        dbg(DEBUG,"igTimeWindowLowerLimit<%d>",igTimeWindowLowerLimit);
    }
    else
    {
        igTimeWindowLowerLimit = -1;
        dbg(DEBUG,"Default igTimeWindowLowerLimit<%s>",igTimeWindowLowerLimit);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_UPPER_LIMIT",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        igTimeWindowUpperLimit = atoi(pclTmpBuf);
        dbg(DEBUG,"igTimeWindowUpperLimit<%d>",igTimeWindowUpperLimit);
    }
    else
    {
        igTimeWindowUpperLimit = -1;
        dbg(DEBUG,"Default igTimeWindowUpperLimit<%s>",igTimeWindowUpperLimit);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","GET_DATA_FROM_SOURCE_TABLE",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if (strncmp(pclTmpBuf,"Y",1) == 0)
        {
            igGetDataFromSrcTable = TRUE;
        }
        else
        {
            igGetDataFromSrcTable = FALSE;
        }
        dbg(DEBUG,"igGetDataFromSrcTable<%s>",igGetDataFromSrcTable==FALSE?"FALSE":"TRUE");
    }
    else
    {
        igGetDataFromSrcTable = TRUE;
        dbg(DEBUG,"igGetDataFromSrcTable<%s>",igGetDataFromSrcTable==FALSE?"FALSE":"TRUE");
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","INIT_TABLE_AFTER_RESTART",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if (strncmp(pclTmpBuf,"Y",1) == 0)
        {
            igInitTable = TRUE;
        }
        else
        {
            igInitTable = FALSE;
        }
        dbg(DEBUG,"igInitTable<%d>",igInitTable);
    }
    else
    {
        igInitTable = FALSE;
        dbg(DEBUG,"Default igInitTable<%d>",igInitTable);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DATALIST_DELIMITER",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        igDataListDelimiter = strtol(pclTmpBuf,NULL,16);
        pcgDataListDelimiter[0] = igDataListDelimiter;
        dbg(DEBUG,"pclTmpBuf<%s>igDataListDelimiter<%d>pcgDataListDelimiter<%s>",pclTmpBuf,igDataListDelimiter,pcgDataListDelimiter);
    }
    else
    {
        pcgDataListDelimiter[0] = 0x1F;    /* tvo_fix */
        dbg(DEBUG,"pclTmpBuf<%s>pcgDataListDelimiter<%s>",pclTmpBuf,pcgDataListDelimiter);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DATE_FORMAT_DELIMITER",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgDateFormatDelimiter,pclTmpBuf);
        dbg(DEBUG,"pcgDateFormatDelimiter<%s>",pcgDateFormatDelimiter);
    }
    else
    {
        strcpy(pcgDateFormatDelimiter,"-");
        dbg(DEBUG,"pcgDateFormatDelimiter<%s>",pcgDateFormatDelimiter);
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","RULE_SEARCH_METHOD",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if (!strcmp(pclTmpBuf,"BRUTAL"))
        {
            igRuleSearchMethod = BRUTAL;
        }
        else if (!strcmp(pclTmpBuf,"HASH"))
        {
            igRuleSearchMethod = HASH;
        }
        else
        {
            igRuleSearchMethod = HASH;
        }
    }
    else
    {
        igRuleSearchMethod = HASH;
    }
    dbg(DEBUG,"igRuleSearchMethod<%s>",igRuleSearchMethod==HASH?"HASH":"BRUTAL");

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DELETION_STATUS_FIELDNAME",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgDeletionStatusIndicator, pclTmpBuf);
    }
    else
    {
        strcpy(pcgDeletionStatusIndicator, "");    /* tvo_fix: change "DELETE" to null */
    }
    dbg(DEBUG,"pcgDeletionStatusIndicator<%s>",pcgDeletionStatusIndicator);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DELETION_STATUS_VALUE",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgDelValue, pclTmpBuf);
    }
    else
    {
        strcpy(pcgDelValue, "DELETE");
    }
    dbg(DEBUG,"pcgDelValue<%s>",pcgDelValue);


    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","NULLABLE_INDICATOR",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgNullableIndicator, pclTmpBuf);
    }
    else
    {
        strcpy(pcgNullableIndicator, "NOTNULL");
    }
    dbg(DEBUG,"pcgNullableIndicator<%s>",pcgNullableIndicator);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","MULTI_SOURCE_FIELD_DELIMITER",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgMultiSrcFieldDelimiter, pclTmpBuf);
    }
    else
    {
        strcpy(pcgMultiSrcFieldDelimiter, "-");
    }
    dbg(DEBUG,"pcgMultiSrcFieldDelimiter<%s>",pcgMultiSrcFieldDelimiter);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ENABLE_ROTATION",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if (strncmp(pclTmpBuf,"Y",1) == 0 || strncmp(pclTmpBuf,"y",1) == 0)
            igEnableRotation = TRUE;
        else
            igEnableRotation = FALSE;
    }
    else
    {
        igEnableRotation = FALSE;
    }
    dbg(DEBUG,"igEnableRotation<%d>",igEnableRotation);

    ilRC = iGetConfigEntry(pcgConfigFile,"URNO","CURRENT_ARRIVALS",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgCurrent_Arrivals,pclTmpBuf);
    }
    else
    {
        strcpy(pcgCurrent_Arrivals,"CurrentArr");
    }
    dbg(DEBUG,"pcgCurrent_Arrivals<%s>",pcgCurrent_Arrivals);

    ilRC = iGetConfigEntry(pcgConfigFile,"URNO","CURRENT_DEPARTURES",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgCurrent_Departures,pclTmpBuf);
    }
    else
    {
        strcpy(pcgCurrent_Departures,"CurrentDep");
    }
    dbg(DEBUG,"pcgCurrent_Departures<%s>",pcgCurrent_Departures);

    ilRC = iGetConfigEntry(pcgConfigFile,"URNO","DEST_FLNO_FIELD",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgDestFlnoField,pclTmpBuf);
    }
    else
    {
        strcpy(pcgDestFlnoField,"FLT");
    }
    dbg(DEBUG,"pcgDestFlnoField<%s>",pcgDestFlnoField);

    ilRC = iGetConfigEntry(pcgConfigFile,"CUSTOM","ENABLE_CODESHARE",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if (strncmp(pclTmpBuf,"Y",1) == 0)
        {
            igEnableCodeshare = TRUE;
        }
        else
        {
            igEnableCodeshare = FALSE;
        }
        dbg(DEBUG,"igEnableCodeshare<%d>",igEnableCodeshare);
    }
    else
    {
        igEnableCodeshare = FALSE;
        dbg(DEBUG,"Default igEnableCodeshare<%d>",igEnableCodeshare);
    }

    #ifdef TEST
    /* Get Client and Server chars */
    sprintf(pcgUfisConfigFile,"%s/ufis_ceda.cfg",getenv("CFG_PATH"));
    ilRC = iGetConfigEntry(pcgUfisConfigFile,"CHAR_PATCH","CLIENT_CHARS",
                CFG_STRING, pclClieBuffer);
    if (ilRC == RC_SUCCESS)
    {
        ilRC = iGetConfigEntry(pcgUfisConfigFile,"CHAR_PATCH","SERVER_CHARS",CFG_STRING, pclServBuffer);
        if (ilRC == RC_SUCCESS)
        {
            ilI = GetNoOfElements(pclClieBuffer,',');
            if (ilI == GetNoOfElements(pclServBuffer,','))
            {
                memset(pcgServerChars,0x00,100*sizeof(char));
                memset(pcgClientChars,0x00,100*sizeof(char));
                for (ilJ=0; ilJ < ilI; ilJ++)
                {
                    GetDataItem(pclBuffer,pclServBuffer,ilJ+1,',',"","\0\0");
                    pcgServerChars[ilJ*2] = atoi(pclBuffer);
                    pcgServerChars[ilJ*2+1] = ',';
                } /* end for */
                pcgServerChars[(ilJ-1)*2+1] = 0x00;
                for (ilJ=0; ilJ < ilI; ilJ++)
                {
                    GetDataItem(pclBuffer,pclClieBuffer,ilJ+1,',',"","\0\0");
                    pcgClientChars[ilJ*2] = atoi(pclBuffer);
                    pcgClientChars[ilJ*2+1] = ',';
                } /* end for */
                pcgClientChars[(ilJ-1)*2+1] = 0x00;
                dbg(DEBUG,"New Clientchars <%s> dec <%s>",pcgClientChars,pclClieBuffer);
                dbg(DEBUG,"New Serverchars <%s> dec <%s>",pcgServerChars,pclServBuffer);
                dbg(DEBUG,"Serverchars <%d> ",strlen(pcgServerChars));
                dbg(DEBUG,"%s  and count <%d> ",pclBuffer,strlen(pcgServerChars));
                dbg(DEBUG,"ilI <%d>",ilI);
            } /* end if */
        } /* end if */
        else
        {
            ilRC = RC_SUCCESS;
            dbg(DEBUG,"Use standard (old) serverchars");
        } /* end else */
    } /* end if */
    else
    {
        dbg(DEBUG,"Use standard (old) serverchars");
        ilRC = RC_SUCCESS;
    } /* end else */
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgClientChars,"\042\047\054\012\015");
        strcpy(pcgServerChars,"\260\261\262\263\263");
        ilRC = RC_SUCCESS;
    }
    #endif
	/*-------- tvo_add for store transaction type -------------------------------------------*/
	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_DELETE_FIELD_NAME",CFG_STRING,pclTmpBuf);
	if (ilRC == RC_SUCCESS)
    {
		strcpy(pcgTimeWindowDeleteFieldName, pclTmpBuf);
    }else
	{
		dbg(DEBUG, "CONFIG ERROR: Time window delete field name not found");
		strcpy(pcgTimeWindowDeleteFieldName, "");
	}

	igEnableUpdatePartial = TRUE;    /* default = build update partial */
	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ENABLE_UPDATE_PARTIAL",CFG_STRING,pclTmpBuf);
	if (ilRC == RC_SUCCESS)
	{
		if (strncmp(pclTmpBuf, "N", 1) == 0 || strncmp(pclTmpBuf, "n", 1) == 0)
			igEnableUpdatePartial = FALSE;
	}

	igEnableInsertPartial = TRUE;	/* default = build insert partial */
	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ENABLE_INSERT_PARTIAL",CFG_STRING,pclTmpBuf);
	if (ilRC == RC_SUCCESS)
	{
		if (strncmp(pclTmpBuf, "N", 1) == 0 || strncmp(pclTmpBuf, "n", 1) == 0)
			igEnableInsertPartial = FALSE;
	}

	ilRC = iGetConfigEntry(pcgConfigFile,"RULE","TRANS_TYPE_FIELD_NAME",CFG_STRING,pclTmpBuf);   /* TRANSACTION_TYPE = I:insert, U:update, D:Delete */
	if (ilRC == RC_SUCCESS)
    {
		strcpy(cgTransFieldName, pclTmpBuf);
		igEnableIgnoreDelete = TRUE;
    }
    else  /* don't store transaction indicator */
    {
		strcpy(cgTransFieldName,"");
        igEnableIgnoreDelete = FALSE;
    }
	ilRC = iGetConfigEntry(pcgConfigFile,"RULE","TRANS_TYPE_VALUE",CFG_STRING,pclTmpBuf);   /* TRANSACTION_VALUE = I,U,D */
	if (ilRC == RC_SUCCESS)
    {
		strcpy(cgTransTypeValue, pclTmpBuf);
    }
    else
    {
		strcpy(cgTransTypeValue,"I,U,D");
    }
	dbg(DEBUG,"Default igEnableIgnoreDelete<%d>, cgTransFieldName<%s>, cgTransTypeValue<%s>", igEnableIgnoreDelete, cgTransFieldName, cgTransTypeValue);

	ilRC = iGetConfigEntry(pcgConfigFile,"RULE","INSERT_ONLY_FLAG",CFG_STRING,pclTmpBuf);   /* INSERT_ONLY_FLAG = YES */
	igEnableInsertOnly = FALSE;    /* default insert,update */
	if (ilRC == RC_SUCCESS)
    {
		if (strncmp(pclTmpBuf,"Y",1) == 0)
		{
			igEnableInsertOnly = TRUE;
		}
    }

	ilRC = iGetConfigEntry(pcgConfigFile,"RULE","TOTAL_NUMBER_OF_GROUP",CFG_STRING,pclTmpBuf);
	igTotalGroupConf = 0;     /* default no group */
	if (ilRC == RC_SUCCESS)
	{
		igTotalGroupConf = atoi(pclTmpBuf);
		if (igTotalGroupConf >= MAX_GROUP_RULE)
		{
			igTotalGroupConf = MAX_GROUP_RULE;
			dbg(DEBUG,"CONFIG ERROR: Many rule group, only support up to <%d> group", MAX_GROUP_RULE);
		}
	}else
	{
		dbg(DEBUG,"CONFIG ERROR: No group is provided in config file");
	}

	int  ilCounter;
	char clGroupTmp[128];
	char clTmpValue[128];
	memset(rgGroupIdConf, 0x0, sizeof(rgGroupIdConf));
	for (ilCounter = 1; ilCounter <= igTotalGroupConf; ilCounter++)
	{
		sprintf(clGroupTmp, "GROUP%d_RULE", ilCounter);
		ilRC = iGetConfigEntry(pcgConfigFile,"RULE",clGroupTmp,CFG_STRING,pclTmpBuf);
		if (ilRC == RC_SUCCESS)
		{
			strcpy(rgGroupIdConf[ilCounter].pclGroupConfStr, pclTmpBuf);
			get_item(1, pclTmpBuf, clTmpValue, 0, ";", "\0", "\0");   /* group id */
			rgGroupIdConf[ilCounter].pilGroupId = atoi(clTmpValue);
			get_item(2, pclTmpBuf, clTmpValue, 0, ";", "\0", "\0");   /* source table */
			if (strlen(clTmpValue) == 0)
			{
				dbg(DEBUG,"CONFIG ERROR: %s missing source table_<%s>", clGroupTmp, pclTmpBuf);
			}else
			{
				get_item(3, pclTmpBuf, clTmpValue, 0, ";", "\0", "\0");   /* dest table */
				if (strlen(clTmpValue) == 0)
				{
					dbg(DEBUG,"CONFIG ERROR: %s missing dest table_<%s>", clGroupTmp, pclTmpBuf);
				}
			}

		}else
		{
			dbg(DEBUG,"CONFIG ERROR: %s is invalid_<%s>", clGroupTmp, pclTmpBuf);
		}
	}
	dbg(DEBUG,"Default igEnableIgnoreDelete<%d>, cgTransFieldName<%s>, cgTransTypeValue<%s>", igEnableIgnoreDelete, cgTransFieldName, cgTransTypeValue);
	/*------------------------------------------------------------------------------------*/
    ilRC = GetRuleSchema(&rgRule);

    return RC_SUCCESS;
} /* End of getConfig() */

/* fya 0.1***************************************************************/
/* Following the GetRuleSchema function										*/
/* Reads the .csv file and fill in the configuration structure			*/
/* ******************************************************************** */
static int GetRuleSchema(_RULE *rpRule)
{
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "GetRuleSchema:";
    FILE *fp;
    int ilNoLine = 0;
    int ilI;
    int ilJ;
    char pclRuleGroupNO[16] = "\0";
    char pclCfgFile[128];
    char pclTmpBuf[128];
    char pclLine[2048];
    _LINE rlLine;

    ilRC = iGetConfigEntry(pcgConfigFile, "MAIN", "RULE_SCHEMA_FILE", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        sprintf(pclCfgFile, "%s/%s", getenv("CFG_PATH"), pclTmpBuf);
    }
    else
    {
        sprintf(pclCfgFile, "%s/%s.csv", getenv("CFG_PATH"), mod_name);
    }

    dbg(TRACE, "%s Rule Config File is <%s>", pclFunc, pclCfgFile);

    if ((fp = (FILE *) fopen(pclCfgFile, "r")) == (FILE *) NULL)
    {
        dbg(TRACE, "%s Rule Config File <%s> does not exist", pclFunc, pclCfgFile);
        return RC_FAIL;
    }

    while (fgets(pclLine, 2048, fp))
    {
        /*Getting the rule string one by one from csv file*/
        pclLine[strlen(pclLine) - 1] = '\0';
        if (pclLine[strlen(pclLine) - 1] == '\012' || pclLine[strlen(pclLine) - 1] == '\015')
        {
            pclLine[strlen(pclLine) - 1] = '\0';
        }

        /*Ignore comment*/
        if (isCommentLine(pclLine))
        {
            continue;
        }

        /*Ignore disabled rule line*/
        if (isDisabledLine(pclLine))
        {
            continue;
        }

        if (pclLine[0] == '#')
        {
            dbg(DEBUG,"%s Comment Line -> Ignore & Continue", pclFunc);
            continue;
        }

        if (pclLine[0] == 'N')
        {
            /*dbg(DEBUG,"%s Disabled Rule -> Ignore & Continue", pclFunc);*/
            continue;
        }

		if ((pclLine[0] == ' ') || (pclLine[0] == '\n') || strlen(pclLine) == 0 )    /* tvo_debug: fix blank line */
        {
            /*dbg(DEBUG,"%s Disabled Rule -> Ignore & Continue", pclFunc);*/
            continue;
        }

        getOneline(&rlLine, pclLine);
        /*showLine(&rlLine);*/
        if( strcmp(pclRuleGroupNO, rlLine.pclRuleGroup) != 0 )
        {
            strcpy(pclRuleGroupNO,rlLine.pclRuleGroup);
            /*record the group info by rule group number*/
            getOneline(rgGroupInfo+atoi(rlLine.pclRuleGroup), pclLine);
        }

        /*store the required and conflicted field list group by group*/
        if( atoi(rlLine.pclRuleGroup) > 0 )
        {
            collectSourceFieldSet(pcgSourceFiledSet[atoi(rlLine.pclRuleGroup)], pcgSourceConflictFiledSet[atoi(rlLine.pclRuleGroup)], rlLine.pclSourceField, atoi(rlLine.pclRuleGroup));

            collectFieldList(pcgSourceFiledList[atoi(rlLine.pclRuleGroup)], rlLine.pclSourceField, atoi(rlLine.pclRuleGroup));
            collectFieldList(pcgDestFiledList[atoi(rlLine.pclRuleGroup)], rlLine.pclDestField, atoi(rlLine.pclRuleGroup));
        }
        else
        {
            dbg(TRACE,"%s atoi(rlLine.pclRuleGroup)<%d> ==0",pclFunc);
            continue;
        }

        /*Copy to global structure*/
        storeRule( &(rpRule->rlLine[ilNoLine++]), &rlLine);

        if( atoi(rlLine.pclRuleGroup) > igTotalNumbeoOfRule)
        {
            igTotalNumbeoOfRule = atoi(rlLine.pclRuleGroup);
        }
    }
    /*igTotalLineOfRule = ilNoLine + 1;*/
    igTotalLineOfRule = ilNoLine;
    showRule(rpRule, igTotalLineOfRule);

    ilRC = showGroupInfo(rgGroupInfo, igTotalNumbeoOfRule);
    showFieldByGroup(pcgSourceFiledSet, pcgSourceConflictFiledSet, pcgSourceFiledList, pcgDestFiledList);
    fclose(fp);

    dbg(TRACE, "---------------------------------------------------------");

    return ilRC;
} /* End of GetRuleSchema */

static void getOneline(_LINE *rpLine, char *pcpLine)
{
	char *pclFunc = "getOneline";

	int ilNoEle = 0;

	ilNoEle = GetNoOfElements(pcpLine, ';');
    /*dbg(DEBUG, "%s Current Line = (%d)<%s>", pclFunc, ilNoEle, pcpLine);*/

    get_item(ilNoEle, pcpLine, rpLine->pclActive, 0, ";", "\0", "\0");
    get_item(1, pcpLine, rpLine->pclActive, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclActive);

    get_item(2, pcpLine, rpLine->pclRuleGroup, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclRuleGroup);
    get_item(3, pcpLine, rpLine->pclSourceTable, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclSourceTable);
    get_item(4, pcpLine, rpLine->pclSourceKey, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclSourceKey);
    get_item(5, pcpLine, rpLine->pclSourceField, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclSourceField);
    get_item(6, pcpLine, rpLine->pclSourceFieldType, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclSourceFieldType);
    get_item(7, pcpLine, rpLine->pclDestTable, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestTable);
    get_item(8, pcpLine, rpLine->pclDestKey, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestKey);
    get_item(9, pcpLine, rpLine->pclDestField, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestField);
    get_item(10, pcpLine, rpLine->pclDestFieldLen, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestFieldLen);
    get_item(11, pcpLine, rpLine->pclDestFieldType, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestFieldType);
    get_item(12, pcpLine, rpLine->pclDestFieldOperator, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclDestFieldOperator);
    get_item(13, pcpLine, rpLine->pclCond1, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclCond1);
    get_item(14, pcpLine, rpLine->pclCond2, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclCond2);
    get_item(15, pcpLine, rpLine->pclCond3, 0, ";", "\0", "\0");
    TrimRight(rpLine->pclCond3);
}

static void showLine(_LINE *rpLine)
{
    char pclFunc[] = "showLine:";

    dbg(DEBUG, "%s %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,
    rpLine->pclActive,
    rpLine->pclRuleGroup,
    rpLine->pclSourceTable,
    rpLine->pclSourceKey,
    rpLine->pclSourceField,
    rpLine->pclSourceFieldType,
    rpLine->pclDestTable,
    rpLine->pclDestKey,
    rpLine->pclDestField,
    rpLine->pclDestFieldLen,
    rpLine->pclDestFieldType,
    rpLine->pclDestFieldOperator,
    rpLine->pclCond1,
    rpLine->pclCond2,
    rpLine->pclCond3);
}

static void storeRule(_LINE *rpRuleLine, _LINE *rpSingleLine)
{
    char pclFunc[] = "storeRule:";

    memset(rpRuleLine,0x00,sizeof(rpRuleLine));

    strcpy(rpRuleLine->pclActive, rpSingleLine->pclActive);
    strcpy(rpRuleLine->pclRuleGroup, rpSingleLine->pclRuleGroup);
    strcpy(rpRuleLine->pclSourceTable,rpSingleLine->pclSourceTable);
    strcpy(rpRuleLine->pclSourceKey,rpSingleLine->pclSourceKey);
    strcpy(rpRuleLine->pclSourceField,rpSingleLine->pclSourceField);
    strcpy(rpRuleLine->pclSourceFieldType,rpSingleLine->pclSourceFieldType);
    strcpy(rpRuleLine->pclDestTable,rpSingleLine->pclDestTable);
    strcpy(rpRuleLine->pclDestKey,rpSingleLine->pclDestKey);
    strcpy(rpRuleLine->pclDestField,rpSingleLine->pclDestField);
    strcpy(rpRuleLine->pclDestFieldLen,rpSingleLine->pclDestFieldLen);
    strcpy(rpRuleLine->pclDestFieldType,rpSingleLine->pclDestFieldType);
    strcpy(rpRuleLine->pclDestFieldOperator,rpSingleLine->pclDestFieldOperator);
    strcpy(rpRuleLine->pclCond1,rpSingleLine->pclCond1);
    strcpy(rpRuleLine->pclCond2,rpSingleLine->pclCond2);
    strcpy(rpRuleLine->pclCond3,rpSingleLine->pclCond3);
}

static void showRule(_RULE *rpRule, int ipTotalLineOfRule)
{
    char pclFunc[] = "showRule:";

    int ilCount = 0;

    dbg(DEBUG,"%s There are <%d> lines, the disabled rules are not shown", pclFunc, ipTotalLineOfRule);

    for (ilCount = 0; ilCount < ipTotalLineOfRule; ilCount++)
    {
        if(strcmp(rpRule->rlLine[ilCount].pclActive, " ") != 0 && strlen(rpRule->rlLine[ilCount].pclActive) > 0)
        {
            dbg(DEBUG, "%s [%d]%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,ilCount,   /* tvo_fix: missing %s */
            rpRule->rlLine[ilCount].pclActive,
            rpRule->rlLine[ilCount].pclRuleGroup,
            rpRule->rlLine[ilCount].pclSourceTable,
            rpRule->rlLine[ilCount].pclSourceKey,
            rpRule->rlLine[ilCount].pclSourceField,
            rpRule->rlLine[ilCount].pclSourceFieldType,
            rpRule->rlLine[ilCount].pclDestTable,
            rpRule->rlLine[ilCount].pclDestKey,
            rpRule->rlLine[ilCount].pclDestField,
            rpRule->rlLine[ilCount].pclDestFieldLen,
            rpRule->rlLine[ilCount].pclDestFieldType,
            rpRule->rlLine[ilCount].pclDestFieldOperator,
            rpRule->rlLine[ilCount].pclCond1,
            rpRule->rlLine[ilCount].pclCond2,
            rpRule->rlLine[ilCount].pclCond3);
        }
    }
}

static int collectFieldList(char *pcpFiledSet, char *pcpDestField, int ipGroupNumer)
{
    char pclFunc[] = "collectFieldList:";

    if (strlen(pcpFiledSet) == 0)
    {
        strcat(pcpFiledSet, pcpDestField);
    }
    else
    {
        strcat(pcpFiledSet, ",");
        strcat(pcpFiledSet, pcpDestField);
    }
}

static int collectSourceFieldSet(char *pcpFiledSet, char * pcpConflictFiledSet, char *pcpSourceField, int ipGroupNumer)
{
    char pclFunc[] = "collectSourceFieldSet:";

    if (strstr(pcpFiledSet, pcpSourceField) == 0)
    {
        /*
        if (ipCount != 0)
        {
            strcat(pcpFiledSet, ",");
        }
        strcat(pcpFiledSet, pcpSourceField);
        */
        if (strlen(pcpFiledSet) == 0)
        {
            strcat(pcpFiledSet, pcpSourceField);
        }
        else
        {
            strcat(pcpFiledSet, ",");
            strcat(pcpFiledSet, pcpSourceField);
        }
    }
    else
    {
        /*dbg(DEBUG,"%s GROUP<%d> pcpSourceField<%s> is already in pcpFiledSet<%s>", pclFunc, ipGroupNumer, pcpSourceField, pcpFiledSet);*/

        if (strlen(pcpConflictFiledSet) == 0)
        {
            strcat(pcpConflictFiledSet, pcpSourceField);
        }
        else
        {
            strcat(pcpConflictFiledSet, ",");
            strcat(pcpConflictFiledSet, pcpSourceField);
        }
    }
    return;
}

static int isCommentLine(char *pcpLine)
{
    int ilRC = FALSE;
    char *pclFunc = "isCommentLine";

    if (pcpLine[0] == '#')
    {
        dbg(TRACE,"%s <%s> Comment Line -> Ignore & Continue", pclFunc, pcpLine);
        ilRC = TRUE;
    }
    else
    {
        ilRC = FALSE;
    }

    return ilRC;
}

static int isDisabledLine(char *pcpLine)
{
    int ilRC = FALSE;
    char *pclFunc = "isDisabledLine";

    if (pcpLine[0] == 'N')
    {
        dbg(TRACE,"%s <%s> Disabled Rule -> Ignore & Continue", pclFunc,pcpLine);
        ilRC = TRUE;
    }
    else
    {
        ilRC = FALSE;
    }

    return ilRC;
}

int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData)
{
    char *pclFunc = "extractField";

    int ilItemNo = 0;

    ilItemNo = get_item_no(pcpFields, pcpFieldName, 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No <%s> Found in the field list, return", pclFunc, pcpFieldName);
        return RC_FAIL;
    }

    get_real_item(pcpFieldVal, pcpNewData, ilItemNo);
    dbg(TRACE,"<%s> The New %s is <%s>", pclFunc, pcpFieldName, pcpFieldVal);

    return RC_SUCCESS;
}

static int isInTimeWindow(char *pcpTimeVal, char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit)
{
    char *pclFunc = "isInTimeWindow";

    if ( strcmp(pcpTimeVal, pcpTimeWindowLowerLimit) >= 0 && strcmp(pcpTimeVal, pcpTimeWindowUpperLimit) <= 0)
    {
        dbg(TRACE,"%s <%s> is in the range <%s~%s>", pclFunc, pcpTimeVal, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);

        return RC_SUCCESS;
    }
    else
    {
        dbg(TRACE,"%s <%s> is out of the range <%s~%s>", pclFunc, pcpTimeVal, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        return RC_FAIL;
    }
}

static int toDetermineAppliedRuleGroup(char * pcpTable, char * pcpFields, char * pcpData, char *pcpAdidValue)
{
    int ilCount = 0;
    int ilRuleNumber = 0;
    int ilItemNo = 0;
    char *pclFunc = "toDetermineAppliedRuleGroup";
    char pclADID[16] = "\0";
    char pclTmp[64] = "\0";

	char pclSourceTable[128];
	char pclDestTable[128];


    strcpy(pclADID, pcpAdidValue);
    dbg(DEBUG,"<%s> The New ADID is <%s>", pclFunc, pclADID);
	#if 0
    /*rewrite below code to determine the applied group by modname instead of hard-coded criteria*/
    if (strcmp(mod_name, "tbthdl") == 0)
    {
        if ( strcmp(pcpTable,"AFTTAB") == 0 )
        {
            if (strcmp(pclADID, "A") == 0)
            {
                strcpy(pclTmp,"Current_Arrivals");
            }
            else if (strcmp(pclADID, "D") == 0)
            {
                strcpy(pclTmp,"Current_Departures");
            }
        }
    }
    else if (strcmp(mod_name, "sac1hdl") == 0)
    {
        strcpy(pclTmp,"T1_SAC_ALCS");
    }
    else if (strcmp(mod_name, "sac2hdl") == 0)
    {
        strcpy(pclTmp,"T3_SAC_ALCS");
    }
    else if (strcmp(mod_name, "tbtgrp2") == 0)
    {
        strcpy(pclTmp,"BMS_TNEW_DAILY");
    }

    dbg(DEBUG,"%s pclTmp<%s>",pclFunc,pclTmp);
	#endif
	#if 0
    for (ilCount = 0; ilCount <= igTotalNumbeoOfRule; ilCount++)
    {
		strcpy(pclSourceTable, rgGroupInfo[ilCount].pclSourceTable);
		strcpy(pclDestTable,   rgGroupInfo[ilCount].pclDestTable);

        if (strlen(pclSourceTable) > 0)
        {
            dbg(DEBUG, "%s <%s><%s><%s>", pclFunc,pclSourceTable, pclDestTable, pcpTable);
            if ( strcmp(pclSourceTable, pcpTable ) == 0)     /* the same source table */
            {
                if (strcmp(pclSourceTable, "AFTTAB") == 0)  /* special case of source table */
                {
                    if (strcmp(pclADID, "A") == 0)         /* arrival rule */
                    {
                        ilRuleNumber = 1;
                    }
                    else if (strcmp(pclADID, "D") == 0)	   /* departure rule */
                    {
                        ilRuleNumber = 2;
                    }
                }
                else  /* choose the first matching group */
                {
                    ilRuleNumber = ilCount;
                }
                dbg(DEBUG,"%s ilRuleNumber<%d> break",pclFunc,ilRuleNumber);
                break;
            }
        }
    }
	#endif
	/*------ tvo_add: new determine group id  -----------------------------------------------*/
	ilRuleNumber = 0;     /* return group id */
	char clTmpBuf[512];
	int  ilCounter2;
	/* 1;AFTTAB;BHS_TNEW_DEPARTURE_LATERALS;ADID=A; */


	for (ilCount = 1; ilCount <= igTotalNumbeoOfRule; ilCount++)
	{
		strcpy(pclDestTable, rgGroupInfo[ilCount].pclDestTable);
		strcpy(pclSourceTable, rgGroupInfo[ilCount].pclSourceTable);

		if (strcmp(pclSourceTable, pcpTable) != 0)	/* source table in this rule group diff from table in event then skip */
			continue;
		if (strcmp(pcpTable, "AFTTAB") == 0)    /* if source table from AFTTAB then add value of ADID */
			sprintf(clTmpBuf, "%s;%s;ADID=%s", pcpTable, pclDestTable, pcpAdidValue);
		else
			sprintf(clTmpBuf, "%s;%s;", pcpTable, pclDestTable);
		dbg(DEBUG,"<tvo_rule> clTmpBuf=<%s>", clTmpBuf);

		/* search in configuration to find the group ID */
		for (ilCounter2 = 1; ilCounter2 <= igTotalGroupConf; ilCounter2++)
		{
			if (strstr(rgGroupIdConf[ilCounter2].pclGroupConfStr, clTmpBuf) != NULL)   /* found group id */
			{
				ilRuleNumber = rgGroupIdConf[ilCounter2].pilGroupId;
				break;
			}
		}
		if (ilRuleNumber > 0)
			break;
	}

    return ilRuleNumber;
}

static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList,
                        _RULE *rpRule, int ipTotalLineOfRule, char * pcpSelection, char *pcpAdid, int ilIsMaster,
                        char *pcpHardcodeShare_DestFieldList, _HARDCODE_SHARE pcpHardcodeShare, _QUERY *pcpQuery)
{
    int ilRC = RC_SUCCESS;
    int ilItemNo = 0;
    int ilEleCount = 0;
    int ilRuleCount = 0;
    int ilNoEleSource = 0;
    int ilNoEleDest = 0;
    int ili = 0;
    char *pclFunc = "appliedRules";
    char pclDestKey[64] = "\0";

    char pclBlank[2] = "";
    char pclSST[64] = "\0";
    char pclMFC[64] = "\0";
    char pclMFN[64] = "\0";
    char pclMFX[64] = "\0";
    char pclMFF[64] = "\0";

    char pclKey[256] = "\0";
    char pclSelection[256] = "\0";
    char pclTmpSourceFieldName[256] = "\0";
    char pclTmpDestFieldName[256] = "\0";
    char pclTmpSourceFieldValue[256] = "\0";
    char pclTmpDestFieldValue[256] = "\0";
    char pclDestDataList[4096] = "\0";
    char pclSqlBuf[4096] = "\0";
    char pclSqlData[4096] = "\0";
    char pclSqlUpdateBuf[4096] = "\0";
    char pclSqlInsertBuf[4096] = "\0";

    char pclTmpInsert[8192] = "\0";
    char pclTmpUpdate[8192] = "\0";
    char pclDestFiledListWithCodeshare[8192] = "\0";
    char pclDestDataListWithCodeshareInsert[8192] = "\0";
    char pclDestDataListWithCodeshareUpdate[8192] = "\0";
    char pclConvertedDataList[8192] = "\0";
    /*
    for each item in the field list:
    1 search for the single rule in the applied group using source field
    2 check the concrete data type using source filed type
    3 manipulate the source value according to operator with con1 & con2
    4 store the operated destination field into a list
    */

    /*
    getting one from the source and dest list, then applying the operation based on condition
    */

	ilNoEleSource = GetNoOfElements(pcpSourceFiledList, ',');
	ilNoEleDest   = GetNoOfElements(pcpDestFiledList, ',');

	/*dbg(TRACE,"%s The number of source and dest list are <%d> and <%d>", pclFunc, ilNoEleSource, ilNoEleDest);*/

	if (ilNoEleDest == ilNoEleSource)
	{
	    dbg(TRACE,"%s The number of source and dest list euqals <%d>==<%d>", pclFunc, ilNoEleSource, ilNoEleDest);

        for(ilEleCount = 1; ilEleCount <= ilNoEleDest; ilEleCount++)
        {
            memset(pclTmpSourceFieldName,0,sizeof(pclTmpSourceFieldName));
            memset(pclTmpDestFieldName,0,sizeof(pclTmpDestFieldName));

            get_item(ilEleCount, pcpSourceFiledList, pclTmpSourceFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpSourceFieldName);

            get_item(ilEleCount, pcpDestFiledList, pclTmpDestFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpDestFieldName);

            dbg(DEBUG,"%s <%d> The filed from source is <%s> - from dest is <%s>",pclFunc, ilEleCount, pclTmpSourceFieldName, pclTmpDestFieldName);

            /*hashtable search - fya*/
            if (igRuleSearchMethod == BRUTAL)
            {
                ilRuleCount = getRuleIndex_BrutalForce(pclTmpSourceFieldName, pclTmpSourceFieldValue, pclTmpDestFieldName, rpRule, ipRuleGroup, pcpFields, ipTotalLineOfRule, pcpData);
            }
            else if (igRuleSearchMethod == HASH)
            {
                /*build the key: group number+dest field name*/
                ilRuleCount = getRuleIndex_Hash(ipRuleGroup, pclTmpDestFieldName, pcpFields, pcpData, pclTmpSourceFieldName, pclTmpSourceFieldValue);
            }

            if(ilRuleCount == RC_FAIL)
            {
                dbg(DEBUG,"%s Found index fails -> return",pclFunc);
                return RC_FAIL;
                /*buildConvertedDataList(pclDestDataList, pclTmpDestFieldValue, BLANK);*/
            }
            else
            {
                /*getting the operator and condition*/
                /*dbg(TRACE,"%s The operator is <%s> and con1<%s>, con2<%s>",pclFunc, rpRule->rlLine[ilRuleCount].pclDestFieldOperator,
                rpRule->rlLine[ilRuleCount].pclCond1, rpRule->rlLine[ilRuleCount].pclCond2);*/
				if (strlen(pclTmpSourceFieldValue) > 0)  /* source value not null then do convert */
                ilRC = convertSrcValToDestVal(pclTmpSourceFieldName, pclTmpSourceFieldValue, pclTmpDestFieldName, rpRule->rlLine+ilRuleCount, pclTmpDestFieldValue, pcpSelection, pcpAdid);
				else /* if source value = null then dest value = null */
				{
					strcpy(pclTmpDestFieldValue,"");
					ilRC = RC_SUCCESS;
					dbg(DEBUG,"convertSrcValToDestVal: source field <%s> = <%s> then dest = <%s>, no need to convert",
							  pclTmpSourceFieldName, pclTmpSourceFieldValue, pclTmpDestFieldValue);
				}

                /**/
                /*if (strcmp(rpRule->rlLine[ilRuleCount].pclCond3, pcgNullableIndicator) == 0)*/
                if (strcmp((rpRule->rlLine+ilRuleCount)->pclCond3, pcgNullableIndicator) == 0)
                {
					dbg(DEBUG, "%s <%s> <%s> <%d:%d>", pclFunc,(rpRule->rlLine+ilRuleCount)->pclCond3, pcgNullableIndicator, ilRuleCount, RC_FAIL);
                    ilRC = checkNullable(pclTmpDestFieldValue, rpRule->rlLine+ilRuleCount);
                    if (ilRC == RC_FAIL)
                    {
                        memset(pcpQuery->pclInsertQuery, 0,sizeof(pcpQuery->pclInsertQuery));
                        memset(pcpQuery->pclUpdateQuery, 0,sizeof(pcpQuery->pclUpdateQuery));

                        return RC_FAIL;
                    }
                }
                else
                {
                    dbg(TRACE,"%s NULL VALUE is allowed",pclFunc);
                }

                buildConvertedDataList(rpRule->rlLine[ilRuleCount].pclDestFieldType, pclDestDataList, pclTmpDestFieldValue, NONBLANK);
            }
        }/*end of dest field for loop*/

        ili = GetNoOfElements(pclDestDataList,pcgDataListDelimiter[0]);
        dbg(TRACE, "%s The manipulated dest data list <%s> field number<%d>\n",
            pclFunc, pclDestDataList, ili);
        /*dbg(TRACE, "%s The manipulated dest data list <%s>\n", pclFunc, pclDestDataList);*/

        /* tvo_fix: do not convert the delimiter into comma here */
        /*
        if (pcgDataListDelimiter[0] != ',')
        {
            replaceDelimiter(pclConvertedDataList, pclDestDataList, pcgDataListDelimiter[0], ',');
        }
		*/
		dbg(DEBUG, "%s: igEnableInsertPartial = <%d:%d,%d>, igEnableUpdatePartial = <%d:%d,%d>",
				pclFunc,igEnableInsertPartial, TRUE, FALSE, igEnableUpdatePartial, TRUE, FALSE);

        if ( ili != ilNoEleSource)
        {
            dbg(TRACE,"%s The number of field in dest data list<%d> and source field<%d> is not matched",pclFunc, ili);
        }
        else
        {
            strcpy(pclDestFiledListWithCodeshare, pcpDestFiledList);

            /**/
            if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                 strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
            {
                    strcat(pclDestFiledListWithCodeshare,",");
                    strcat(pclDestFiledListWithCodeshare, pcpHardcodeShare_DestFieldList);
                }
            /**/

            if ( ilIsMaster == MASTER_RECORD )
            {
                /**/
                if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                     strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
                {
                    /*Insert data list, tvo_fix */
                    /* sprintf(pclTmpInsert,"'%s',%s,%s,%s,%s",pcpHardcodeShare.pclSST,"''","''","''","''"); */
                    sprintf(pclTmpInsert,"'%s'%s%s%s%s%s%s%s%s",
							strlen(pcpHardcodeShare.pclSST)==0?"0":pcpHardcodeShare.pclSST,pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''");

                    strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                    strcat(pclDestDataListWithCodeshareInsert,pcgDataListDelimiter); /* strcat(pclDestDataListWithCodeshareInsert,","); */
                        strcat(pclDestDataListWithCodeshareInsert,pclTmpInsert);

                    /*Update data list*/
                    sprintf(pclTmpUpdate,"'%s'%s%s%s%s%s%s%s%s",strlen(pcpHardcodeShare.pclSST)==0?"0":pcpHardcodeShare.pclSST,pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''");

                    strcpy(pclDestDataListWithCodeshareUpdate, pclDestDataList);
                        strcat(pclDestDataListWithCodeshareUpdate, pcgDataListDelimiter);
                        strcat(pclDestDataListWithCodeshareUpdate, pclTmpUpdate);
                    }
                else
                {
                    strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                    strcpy(pclDestDataListWithCodeshareUpdate, pclDestDataList);
                }
                /**/

                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList);*/
                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare);*/
                dbg(DEBUG, "%s __%d__before_build_insert\n pclDestDataListWithCodeshareInsert=<%s>\n pclDestDataListWithCodeshareUpdate=<%s>",
							pclFunc, __LINE__ , pclDestDataListWithCodeshareInsert, pclDestDataListWithCodeshareUpdate);

				if (igEnableInsertPartial == FALSE)       /* if not insert partial  */
					buildInsertQuery(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);
                else
					buildInsertQueryPartial(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);

                dbg(DEBUG, "%s __%d__insert<%s>", pclFunc, __LINE__ ,pclSqlInsertBuf);
                strcpy(pcpQuery->pclInsertQuery, pclSqlInsertBuf);

                /*buildUpdateQuery(pclSqlUpdateBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList, pclSelection);*/
                /*buildUpdateQuery(pclSqlUpdateBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare,pclSelection);*/
                if (igEnableInsertOnly == FALSE)  /* tvo_add: insert only case */
				{
					if (igEnableUpdatePartial == TRUE)
						buildUpdateQueryPartial(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareUpdate,pcpSelection,ipRuleGroup);
					else
						buildUpdateQuery(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareUpdate,pcpSelection,ipRuleGroup, TRUE); /* tvo_fix: pclSelection --> pcpSelection */

                	strcpy(pcpQuery->pclUpdateQuery, pclSqlUpdateBuf);
				}
				else /* if igEnableInsertOnly == TRUE, dont build update query */
				{
					memset(pclSqlUpdateBuf, 0x0, sizeof(pclSqlUpdateBuf));
				}
                dbg(DEBUG, "%s igEnableInsertOnly = <%d:%d,%d>, update = <%s>", pclFunc, igEnableInsertOnly, TRUE, FALSE, pclSqlUpdateBuf);

                /*
                buildUpdateQuery(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pcpDestFiledList, pclDestDataList, pcpSelection,ipRuleGroup,FALSE);
                strcpy(pcpQuery->pclUpdateQuery_Codeshare, pclSqlUpdateBuf);
                */
            }
            else
            {
                /**/
                if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                     strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
                {
                    /*sprintf(pclTmpInsert,"'%s','%s','%s','%s','%s','%s'",pcpHardcodeShare.pclMFC, pcpHardcodeShare.pclMFN,pcpHardcodeShare.pclMFX,pcpHardcodeShare.pclMFF, pcgCodeShareSST);
                    sprintf(pclTmpInsert,"'%s','%s','%s','%s','%s'",pcgCodeShareSST,pcpHardcodeShare.pclMFC, pcpHardcodeShare.pclMFN,pcpHardcodeShare.pclMFX,pcpHardcodeShare.pclMFF);
                    */
                    sprintf(pclTmpInsert,"'%s'%s'%s'%s'%s'%s'%s'%s'%s'",pcgCodeShareSST,pcgDataListDelimiter,pcpHardcodeShare.pclMFC, pcgDataListDelimiter, pcpHardcodeShare.pclMFN,pcgDataListDelimiter,pcpHardcodeShare.pclMFX,pcgDataListDelimiter,pcpHardcodeShare.pclMFF);

                    strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                    strcat(pclDestDataListWithCodeshareInsert,pcgDataListDelimiter); /* strcat(pclDestDataListWithCodeshareInsert,","); */
                    strcat(pclDestDataListWithCodeshareInsert,pclTmpInsert);
                }
                else
                {
                     strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                }
                /**/

                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList);*/
                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare);*/
                dbg(DEBUG, "%s __%d__before_build_insert\n pclDestDataListWithCodeshareInsert=<%s>\n pclDestDataListWithCodeshareUpdate=<%s>",
							pclFunc, __LINE__ , pclDestDataListWithCodeshareInsert, pclDestDataListWithCodeshareUpdate);

				if (igEnableInsertPartial == FALSE)       /* if not insert partial  */
                	buildInsertQuery(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);
				else
					buildInsertQueryPartial(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);

                dbg(DEBUG, "%s __%d__insert<%s>", pclFunc, __LINE__, pclSqlInsertBuf);

                strcpy(pcpQuery->pclInsertQuery, pclSqlInsertBuf);

                if (igEnableInsertOnly == FALSE)  /* tvo_add: insert only case */
				{
					if (igEnableUpdatePartial == TRUE)
						buildUpdateQueryPartial(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pcpDestFiledList, pclDestDataList,pcpSelection,ipRuleGroup);
					else
						 buildUpdateQuery(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pcpDestFiledList, pclDestDataList, pcpSelection,ipRuleGroup,FALSE);

                	strcpy(pcpQuery->pclUpdateQuery, pclSqlUpdateBuf);
				}
				else /* if igEnableInsertOnly == TRUE, dont build update query */
				{
					memset(pclSqlUpdateBuf, 0x0, sizeof(pclSqlUpdateBuf));
				}
				dbg(DEBUG, "%s igEnableInsertOnly = <%d:%d,%d>, update = <%s>", pclFunc, igEnableInsertOnly, TRUE, FALSE, pclSqlUpdateBuf);
            }
        }
	}
	else
	{
        dbg(TRACE,"%s The number of source and dest list does not euqal <%d>!=<%d>", pclFunc, ilNoEleSource, ilNoEleDest);
        ilRC = RC_FAIL;
	}

	return ilRC;
}


#if 0
/*Original appliedRules*/
static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList,
                        _RULE *rpRule, int ipTotalLineOfRule, char * pcpSelection, char *pcpAdid, int ilIsMaster,
                        char *pcpHardcodeShare_DestFieldList, _HARDCODE_SHARE pcpHardcodeShare, _QUERY *pcpQuery)
{
    int ilRC = RC_SUCCESS;
    int ilItemNo = 0;
    int ilEleCount = 0;
    int ilRuleCount = 0;
    int ilNoEleSource = 0;
    int ilNoEleDest = 0;
    int ili = 0;
    char *pclFunc = "appliedRules";
    char pclDestKey[64] = "\0";

    char pclBlank[2] = "";
    char pclSST[64] = "\0";
    char pclMFC[64] = "\0";
    char pclMFN[64] = "\0";
    char pclMFX[64] = "\0";
    char pclMFF[64] = "\0";

    char pclTmp[256] = "\0";
    char pclSelection[256] = "\0";
    char pclTmpSourceFieldName[256] = "\0";
    char pclTmpDestFieldName[256] = "\0";
    char pclTmpSourceFieldValue[256] = "\0";
    char pclTmpDestFieldValue[256] = "\0";
    char pclDestDataList[4096] = "\0";
    char pclSqlBuf[4096] = "\0";
    char pclSqlData[4096] = "\0";
    char pclSqlUpdateBuf[4096] = "\0";
    char pclSqlInsertBuf[4096] = "\0";

    char pclTmpInsert[4096] = "\0";
    char pclTmpUpdate[4096] = "\0";
    char pclDestFiledListWithCodeshare[4096] = "\0";
    char pclDestDataListWithCodeshareInsert[4096] = "\0";
    char pclDestDataListWithCodeshareUpdate[4096] = "\0";
    char pclConvertedDataList[8192] = "\0";
    /*
    for each item in the field list:
    1 search for the single rule in the applied group using source field
    2 check the concrete data type using source filed type
    3 manipulate the source value according to operator with con1 & con2
    4 store the operated destination field into a list
    */

    /*
    getting one from the source and dest list, then applying the operation based on condition
    */

	ilNoEleSource = GetNoOfElements(pcpSourceFiledList, ',');
	ilNoEleDest   = GetNoOfElements(pcpDestFiledList, ',');

	/*dbg(TRACE,"%s The number of source and dest list are <%d> and <%d>", pclFunc, ilNoEleSource, ilNoEleDest);*/

	if (ilNoEleDest == ilNoEleSource)
	{
	    dbg(TRACE,"%s The number of source and dest list euqals <%d>==<%d>", pclFunc, ilNoEleSource, ilNoEleDest);

        for(ilEleCount = 1; ilEleCount <= ilNoEleDest; ilEleCount++)
        {
            memset(pclTmpSourceFieldName,0,sizeof(pclTmpSourceFieldName));
            memset(pclTmpDestFieldName,0,sizeof(pclTmpDestFieldName));

            get_item(ilEleCount, pcpSourceFiledList, pclTmpSourceFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpSourceFieldName);

            get_item(ilEleCount, pcpDestFiledList, pclTmpDestFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpDestFieldName);

            dbg(DEBUG,"%s <%d> The filed from source is <%s> - from dest is <%s>",pclFunc, ilEleCount, pclTmpSourceFieldName, pclTmpDestFieldName);

            /*hashtable search - fya*/
            for (ilRuleCount = 0; ilRuleCount <= ipTotalLineOfRule; ilRuleCount++)
            {
                memset(pclTmpSourceFieldValue,0,sizeof(pclTmpSourceFieldValue));
                /*matched the group number*/
                if ( atoi(rpRule->rlLine[ilRuleCount].pclRuleGroup) == ipRuleGroup)
                {
                    if (strcmp(rpRule->rlLine[ilRuleCount].pclDestField, pclTmpDestFieldName) == 0)
                    {
                        ilItemNo = get_item_no(pcpFields, pclTmpSourceFieldName, 5) + 1;
                        if (ilItemNo <= 0 )
                        {
                            dbg(TRACE, "<%s> No <%s> Not found in the field list, can't do anything", pclFunc, pclTmpSourceFieldName);
                            return RC_FAIL;
                        }
                        else
                        {
                            get_real_item(pclTmpSourceFieldValue, pcpData, ilItemNo);
                        }
                        dbg(DEBUG,"<%s> Source - The value for <%s> is <%s>", pclFunc, pclTmpSourceFieldName, pclTmpSourceFieldValue);

                        /*getting the operator and condition*/
                        /*dbg(TRACE,"%s The operator is <%s> and con1<%s>, con2<%s>",pclFunc, rpRule->rlLine[ilRuleCount].pclDestFieldOperator,
                        rpRule->rlLine[ilRuleCount].pclCond1, rpRule->rlLine[ilRuleCount].pclCond2);*/

                        ilRC = convertSrcValToDestVal(pclTmpSourceFieldName, pclTmpSourceFieldValue, pclTmpDestFieldName, rpRule->rlLine+ilRuleCount, pclTmpDestFieldValue, pcpSelection, pcpAdid);

                        /*NUMBER*/
                        if(ilRC == RC_NUMBER)
                        {
                            if ( strlen(pclDestDataList) == 0 )
                            {
                                strcat(pclDestDataList, pclTmpDestFieldValue);
                            }
                            else
                            {
                                /*strcat(pclDestDataList,",");*/
                                strcat(pclDestDataList, pcgDataListDelimiter);
                                strcat(pclDestDataList, pclTmpDestFieldValue);
                            }
                        }/*CHAR*/
                        else
                        {
                            if ( strlen(pclDestDataList) == 0 )
                            {
                                /*if(strstr(pclTmpDestFieldValue,"-") != 0 )*/
                                if(strstr(pclTmpDestFieldValue,pcgDateFormatDelimiter) != 0 )
                                {
                                    /*to_date('%s','YYYY-MM-DD HH24:MI:SS')*/

                                    strcat(pclDestDataList, "to_date(");
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                    /*strcat(pclDestDataList, ",'YYYY-MM-DD HH24-MI-SS')");*/
                                    sprintf(pclTmp,",'YYYY%sMM%sDD HH24%sMI%sSS')", pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter);
                                    strcat(pclDestDataList, pclTmp);
                                    /*
                                    strcat(pclDestDataList, "date");
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                    */
                                }
                                else
                                {
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                }

                            }
                            else
                            {
                                /*strcat(pclDestDataList,",");*/
                                strcat(pclDestDataList, pcgDataListDelimiter);
                                /*if(strstr(pclTmpDestFieldValue,"-") != 0 )*/
                                if(strstr(pclTmpDestFieldValue,pcgDateFormatDelimiter) != 0 )
                                {
                                    strcat(pclDestDataList, "to_date(");
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                    /*strcat(pclDestDataList, ",'YYYY-MM-DD HH24-MI-SS')");*/
                                    sprintf(pclTmp,",'YYYY%sMM%sDD HH24%sMI%sSS')", pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter);
                                    strcat(pclDestDataList, pclTmp);
                                    /*
                                    strcat(pclDestDataList, "date");
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                    */
                                }
                                else
                                {
                                    strcat(pclDestDataList, "'");
                                    strcat(pclDestDataList, pclTmpDestFieldValue);
                                    strcat(pclDestDataList, "'");
                                }
                            }
                        }
                    }
                }
                else
                {
                    /*dbg(DEBUG,"%s The group number is not matched - ipRuleGroup<%d> and <%d>",pclFunc, ipRuleGroup, atoi(rpRule->rlLine[ilRuleCount].pclRuleGroup));*/
                    continue;
                }
            }
        }/*end of for loop*/

        ili = GetNoOfElements(pclDestDataList,pcgDataListDelimiter[0]);
        dbg(TRACE, "%s The manipulated dest data list <%s> field number<%d>\n",
            pclFunc, pclDestDataList, ili);
        /*dbg(TRACE, "%s The manipulated dest data list <%s>\n", pclFunc, pclDestDataList);*/

        /*convert the delimiter into comma*/
        if (pcgDataListDelimiter[0] != ',')
        {
            replaceDelimiter(pclConvertedDataList, pclDestDataList, pcgDataListDelimiter[0], ',');
        }

        if ( ili != ilNoEleSource)
        {
            dbg(TRACE,"%s The number of field in dest data list<%d> and source field<%d> is not matched",pclFunc, ili, ilNoEleSource);
        }
        else
        {
            strcpy(pclDestFiledListWithCodeshare, pcpDestFiledList);

            /**/
            if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                 strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
            {
                strcat(pclDestFiledListWithCodeshare,",");
                strcat(pclDestFiledListWithCodeshare,pcpHardcodeShare_DestFieldList);
            }
            /**/

            if ( ilIsMaster == MASTER_RECORD )
            {
                /**/
                if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                     strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
                {
                    /*Insert data list*/
                    sprintf(pclTmpInsert,"'%s',%s,%s,%s,%s",pcpHardcodeShare.pclSST,"''","''","''","''");

                    strcpy(pclDestDataListWithCodeshareInsert, pclConvertedDataList);
                    strcat(pclDestDataListWithCodeshareInsert,",");
                    strcat(pclDestDataListWithCodeshareInsert,pclTmpInsert);

                    /*Update data list*/
                   sprintf(pclTmpUpdate,"'%s'%s%s%s%s%s%s%s%s",pcpHardcodeShare.pclSST,pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''",pcgDataListDelimiter,"''");

                    strcpy(pclDestDataListWithCodeshareUpdate, pclDestDataList);
                    strcat(pclDestDataListWithCodeshareUpdate, pcgDataListDelimiter);
                    strcat(pclDestDataListWithCodeshareUpdate, pclTmpUpdate);
                }
                else
                {
                    strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                    strcpy(pclDestDataListWithCodeshareUpdate, pclDestDataList);
                }
                /**/

                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList);*/
                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare);*/
                buildInsertQuery(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);
                dbg(DEBUG, "%s __%d__insert<%s>", pclFunc, __LINE__ ,pclSqlInsertBuf);

                /*buildUpdateQuery(pclSqlUpdateBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList, pclSelection);*/
                /*buildUpdateQuery(pclSqlUpdateBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare,pclSelection);*/
                if (igEnableUpdatePartial == TRUE)
					buildUpdateQueryPartial(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareUpdate,pcpSelection,ipRuleGroup);
				else
                buildUpdateQuery(pclSqlUpdateBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareUpdate,pcpSelection,ipRuleGroup); /* tvo_fix: pclSelection --> pcpSelection */
                dbg(DEBUG, "%s update<%s>", pclFunc, pclSqlUpdateBuf);

                strcpy(pcpQuery->pclInsertQuery, pclSqlInsertBuf);
                strcpy(pcpQuery->pclUpdateQuery, pclSqlUpdateBuf);
            }
            else
            {
                /**/
                if ( strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Arrivals"  ) == 0 ||
                     strcmp(rgGroupInfo[ipRuleGroup].pclDestTable, "Current_Departures") == 0 )
                {
                    sprintf(pclTmpInsert,"'%s','%s','%s','%s','%s'","SL",pcpHardcodeShare.pclMFC, pcpHardcodeShare.pclMFN,pcpHardcodeShare.pclMFX,pcpHardcodeShare.pclMFF);
                    strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                    strcat(pclDestDataListWithCodeshareInsert,",");
                    strcat(pclDestDataListWithCodeshareInsert,pclTmpInsert);
                }
                else
                {
                     strcpy(pclDestDataListWithCodeshareInsert, pclDestDataList);
                }
                /**/

                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pcpDestFiledList, pclDestDataList);*/
                /*buildInsertQuery(pclSqlInsertBuf, rpRule->rlLine[0].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshare);*/
                buildInsertQuery(pclSqlInsertBuf, rgGroupInfo[ipRuleGroup].pclDestTable, pclDestFiledListWithCodeshare, pclDestDataListWithCodeshareInsert);
                dbg(DEBUG, "%s __%d__insert<%s>", pclFunc, __LINE__, pclSqlInsertBuf);

                strcpy(pcpQuery->pclInsertQuery, pclSqlInsertBuf);
            }
        }
	}
	else
	{
        dbg(TRACE,"%s The number of source and dest list does not euqal <%d>!=<%d>", pclFunc, ilNoEleSource, ilNoEleDest);
        ilRC = RC_FAIL;
	}

	return ilRC;
}
#endif

static void showFieldByGroup(char (*pcpSourceFiledSet)[LISTLEN], char (*pcpConflictFiledSet)[LISTLEN], char (* pcpSourceFiledList)[LISTLEN], char (* pcpDestFiledSet)[LISTLEN])
{
    int ilCount = 0;
    char *pclFunc = "showFieldByGroup";

    for (ilCount = 0; ilCount < GROUPNUMER; ilCount++)
    {
        if (strlen(pcpSourceFiledSet[ilCount]) > 0 && strcmp(pcpSourceFiledSet[ilCount]," ") != 0 )
        {
            dbg(TRACE,"\n%s Group[%d] Source Field Set <%s>\n", pclFunc, ilCount, (pcpSourceFiledSet+ilCount)[0]);
            dbg(TRACE,"%s Group[%d] Conflicted Source Field List <%s>\n", pclFunc, ilCount, (pcpConflictFiledSet+ilCount)[0]);
            dbg(TRACE,"%s Group[%d] Source Field List <%s>\n", pclFunc, ilCount, (pcpSourceFiledList+ilCount)[0]);
            dbg(TRACE,"%s Group[%d] Destination Field List <%s>\n", pclFunc, ilCount, (pcpDestFiledSet+ilCount)[0]);
        }
    }
}

static int matchFieldListOnGroup(int ilRuleGroup, char *pcpSourceFieldList)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "matchFieldListOnGroup";
    memset(pcpSourceFieldList,0,sizeof(pcpSourceFieldList));

    if (ilRuleGroup < 0 || ilRuleGroup > GROUPNUMER)
    {
        dbg(TRACE,"%s The group number<%d> is invalid",pclFunc,ilRuleGroup);
        return RC_FAIL;
    }
    else
    {
        if(strlen(pcgSourceFiledSet[ilRuleGroup]) > 0)
        {
            /*dbg(DEBUG,"%s Group[%d] - The field list is <%s>", pclFunc, ilRuleGroup, pcgSourceFiledSet[ilRuleGroup]);*/
            strcpy(pcpSourceFieldList, pcgSourceFiledSet[ilRuleGroup]);
            ilRC = RC_SUCCESS;
        }
        else
        {
            dbg(TRACE,"%s Group[%d] - The field list is blank", pclFunc, ilRuleGroup);
            ilRC = RC_FAIL;
        }
    }
    return ilRC;
}

static int getSourceFieldData(char *pclTable, char * pcpSourceFieldList, char * pcpSelection, char * pcpSourceDataList)
{
    int ilRC = RC_SUCCESS;
    int ilNoEle = 0;
    char *pclFunc = "getSourceFieldData";

    char pclSqlBuf[4096] = "\0";
    char pclSqlData[4096] = "\0";

    ilNoEle = GetNoOfElements(pcpSourceFieldList, ',');

    buildSelQuery(pclSqlBuf, pclTable, pcpSourceFieldList, pcpSelection);

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
            dbg(TRACE, "<%s> Retrieving source data - Found <%s>", pclFunc, pclSqlData);
            BuildItemBuffer(pclSqlData, NULL, ilNoEle, ",");
            strcpy(pcpSourceDataList, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

void buildSelQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection)
{
    char *pclFunc = "buildSelQuery";

    sprintf(pcpSqlBuf,"SELECT %s FROM %s %s", pcpSourceFieldList, pcpTable, pcpSelection);
    dbg(TRACE,"%s pcpSqlBuf<%s>",pclFunc, pcpSqlBuf);
}

static void buildDestTabWhereClause( char *pcpSelection, char *pcpDestKey, char *pcpDestFiledList, char *pclDestDataList)
{
    int ilRC = RC_SUCCESS;
    int ilCount = 0;
    char *pclFunc = "buildDestTabWhereClause";

    char pclTmpField[256] = "\0";
    char pclTmpData[256] = "\0";

    memset(pcpSelection,0,sizeof(pcpSelection));

    dbg(DEBUG,"%s pcpDestKey<%s>", pclFunc, pcpDestKey);

    sprintf(pcpSelection, "WHERE %s=", pcpDestKey);

    for (ilCount = 1; ilCount <= GetNoOfElements(pclDestDataList,','); ilCount++)
    {

        memset(pclTmpField, 0, sizeof(pclTmpField));
        memset(pclTmpData, 0, sizeof(pclTmpData));

        get_item(ilCount, pcpDestFiledList, pclTmpField, 0, ",", "\0", "\0");
        TrimRight(pclTmpField);

        get_item(ilCount, pclDestDataList, pclTmpData, 0, ",", "\0", "\0");
        TrimRight(pclTmpData);

        if ( strcmp(pclTmpField, pcpDestKey) == 0)
        {
            dbg(TRACE,"%s The key value is found", pclFunc);
            strcat(pcpSelection, pclTmpData);
            break;
        }
    }

    if(strlen(pcpSelection) > 0)
    {
        dbg(TRACE,"%s pcpSelection<%s>", pclFunc, pcpSelection);
    }
}

static void buildInsertQuery(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData)
{
    int ilNextUrno = 0;
    char *pclFunc = "buildInsertQuery";
    char pclTimeNow[TIMEFORMAT] = "\0";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";
    char pclTmp[64] = "\0";
	char pclConvertedDataList[8192] = "\0";

	if (pcgDataListDelimiter[0] != ',')    /* convert delimiter  to comma for insert query */
    {
        replaceDelimiter(pclConvertedDataList, pcpDestFieldData, pcgDataListDelimiter[0], ',');
		strcpy(pcpDestFieldData, pclConvertedDataList);
    }

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);
    UtcToLocal(pclTimeNow);

    strncpy(pclYear,pclTimeNow,4);
    strncpy(pclMonth,pclTimeNow+4,2);
    strncpy(pclDay,pclTimeNow+6,2);
    strncpy(pclHour,pclTimeNow+8,2);
    strncpy(pclMin,pclTimeNow+10,2);
    strncpy(pclSec,pclTimeNow+12,2);

    sprintf(pclTmp,"to_date('%s%s%s%s%s %s%s%s%s%s','YYYY%sMM%sDD HH24%sMI%sSS')",pclYear, pcgDataListDelimiter,pclMonth, pcgDataListDelimiter,pclDay,pclHour,pcgDataListDelimiter, pclMin,pcgDataListDelimiter,pclSec,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter);

    /*sprintf(pcpSqlBuf,"INSERT INTO %s (%s,CDAT,LSTU,URNO) VALUES(%s,to_date('%s','YYYY-MM-DD HH24:MI:SS'),to_date('%s','YYYY-MM-DD HH24:MI:SS'),%d)", pcpTable, pcpDestFieldList, pcpDestFieldData,pclTmp,pclTmp,ilNextUrno);*/

    if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 ||
         strcmp(pcpTable, "Current_Departures") == 0 )
    {
        if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 )
            ilNextUrno = NewUrnos(pcgCurrent_Arrivals,1);
        else
            ilNextUrno = NewUrnos(pcgCurrent_Departures,1);

        if (ilNextUrno <= 0)
            ilNextUrno = NewUrnos("SNOTAB",1);

        sprintf(pcpSqlBuf,"INSERT INTO %s (%s,CDAT,LSTU,URNO) VALUES(%s,%s,%s,%d)", pcpTable, pcpDestFieldList, pcpDestFieldData,pclTmp,pclTmp,ilNextUrno);
    }
    else /// if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 )   /* tvo_debug: bug of insert error */
    {
        sprintf(pcpSqlBuf,"INSERT INTO %s (%s) VALUES(%s)", pcpTable, pcpDestFieldList, pcpDestFieldData);
    }
}

static void buildInsertQueryPartial(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData)
{
    int ilNextUrno = 0;
    char *pclFunc = "buildInsertQueryPartial";
    char pclTimeNow[TIMEFORMAT] = "\0";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";
    char pclTmp[64] = "\0";

	char pclTmpFieldList[4096] = "\0";
	char pclTmpDataList[8192] = "\0";
	char pclTmpField[256] = "\0";
    char pclTmpData[256] = "\0";
	int  ilCount;

	memset(pclTmpFieldList, 0x0, sizeof(pclTmpFieldList));
	memset(pclTmpDataList, 0x0, sizeof(pclTmpDataList));

	for (ilCount = 1; ilCount <= GetNoOfElements(pcpDestFieldData,pcgDataListDelimiter[0]); ilCount++)
    {
        memset(pclTmpField, 0, sizeof(pclTmpField));
        memset(pclTmpData, 0, sizeof(pclTmpData));
        memset(pclTmp, 0, sizeof(pclTmp));

        get_item(ilCount, pcpDestFieldList, pclTmpField, 0, ",", "\0", "\0");   				   /* get field item */
        TrimRight(pclTmpField);

        get_item(ilCount, pcpDestFieldData, pclTmpData, 0, pcgDataListDelimiter, "\0", "\0");	   /* get value of field */
        TrimRight(pclTmpData);
        /*dbg(DEBUG,"%s %s=%s",pclFunc, pclTmpField,pclTmpData);*/

        if (strcmp(pclTmpData,"''") == 0)   /* skip null value */
			continue;

		if (strlen(pclTmpFieldList) > 0)         /* if field list is not empty */
        {
			strcat(pclTmpFieldList, ",");
			strcat(pclTmpDataList, ",");
		}
		strcat(pclTmpFieldList, pclTmpField);
		strcat(pclTmpDataList, pclTmpData);
    }


	GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);
    UtcToLocal(pclTimeNow);

    strncpy(pclYear,pclTimeNow,4);
    strncpy(pclMonth,pclTimeNow+4,2);
    strncpy(pclDay,pclTimeNow+6,2);
    strncpy(pclHour,pclTimeNow+8,2);
    strncpy(pclMin,pclTimeNow+10,2);
    strncpy(pclSec,pclTimeNow+12,2);

    sprintf(pclTmp,"to_date('%s%s%s%s%s %s%s%s%s%s','YYYY%sMM%sDD HH24%sMI%sSS')",pclYear, pcgDataListDelimiter,pclMonth, pcgDataListDelimiter,pclDay,pclHour,pcgDataListDelimiter, pclMin,pcgDataListDelimiter,pclSec,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter);

    /*sprintf(pcpSqlBuf,"INSERT INTO %s (%s,CDAT,LSTU,URNO) VALUES(%s,to_date('%s','YYYY-MM-DD HH24:MI:SS'),to_date('%s','YYYY-MM-DD HH24:MI:SS'),%d)", pcpTable, pcpDestFieldList, pcpDestFieldData,pclTmp,pclTmp,ilNextUrno);*/

    if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 ||
         strcmp(pcpTable, "Current_Departures") == 0 )
    {
        if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 )
            ilNextUrno = NewUrnos(pcgCurrent_Arrivals,1);
        else
            ilNextUrno = NewUrnos(pcgCurrent_Departures,1);

        if (ilNextUrno <= 0)
            ilNextUrno = NewUrnos("SNOTAB",1);

        sprintf(pcpSqlBuf,"INSERT INTO %s (%s,CDAT,LSTU,URNO) VALUES(%s,%s,%s,%d)", pcpTable, pclTmpFieldList, pclTmpDataList,pclTmp,pclTmp,ilNextUrno);
    }
    else /// if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 )   /* tvo_debug: bug of insert error */
    {
        sprintf(pcpSqlBuf,"INSERT INTO %s (%s) VALUES(%s)", pcpTable, pclTmpFieldList, pclTmpDataList);
    }
}

static void buildUpdateQuery(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData, char * pcpSelection, int pipRuleGroup, int ipMasterFlag)
{
    int ilCount = 0;
    char *pclFunc = "buildupdateQuery";
    char pclTimeNow[TIMEFORMAT] = "\0";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";

    char pclTmp[256] = "\0";
    char *pclTmp1 = "\0";
    char pclTmpSelection[256] = "\0";
    char pclTmpFlnoSelection[256] = "\0";
    char pclUrnoSelection[256] = "\0";
    char pclTmpTime[256] = "\0";
    char pclTmpField[256] = "\0";
    char pclTmpData[256] = "\0";
    char pclString[4096] = "\0";

    strcpy(pclTmpSelection, pcpSelection);
    pclTmp1 = strstr(pclTmpSelection, "=");
    strcpy(pclUrnoSelection, pclTmp1+1);

    /*ilCount = GetNoOfElements(pcpDestFieldList,',');*/

    /*dbg(DEBUG,"%s FieldList<%s>",pclFunc,pcpDestFieldList);
    dbg(DEBUG,"%s DataList<%s>",pclFunc,pcpDestFieldData);*/

    for (ilCount = 1; ilCount <= GetNoOfElements(pcpDestFieldData,pcgDataListDelimiter[0]); ilCount++)
    {
        memset(pclTmpField, 0, sizeof(pclTmpField));
        memset(pclTmpData, 0, sizeof(pclTmpData));
        memset(pclTmp, 0, sizeof(pclTmp));

        get_item(ilCount, pcpDestFieldList, pclTmpField, 0, ",", "\0", "\0");
        TrimRight(pclTmpField);

        /*get_item(ilCount, pcpDestFieldData, pclTmpData, 0, ",", "\0", "\0");*/
        get_item(ilCount, pcpDestFieldData, pclTmpData, 0, pcgDataListDelimiter, "\0", "\0");
        TrimRight(pclTmpData);
        /*dbg(DEBUG,"%s %s=%s",pclFunc, pclTmpField,pclTmpData);*/

        if ( ilCount == 1)
        {
            sprintf(pclTmp, "%s=%s", pclTmpField, pclTmpData);
            strcat(pclString, pclTmp);
        }
        else
        {
            strcat(pclString, ",");
            sprintf(pclTmp, "%s=%s", pclTmpField, pclTmpData);
            strcat(pclString, pclTmp);
        }

        if ( strcmp(pclTmpField, pcgDestFlnoField) == 0 )
        {
            strcpy(pclTmpFlnoSelection,pclTmp);
        }
    }

    if (ipMasterFlag == TRUE)
    {
        sprintf(pclTmpSelection,"WHERE %s=%s AND SST!='%s'",rgGroupInfo[pipRuleGroup].pclDestKey,pclUrnoSelection,pcgCodeShareSST);
    }
    else
    {
        sprintf(pclTmpSelection,"WHERE %s=%s AND SST='%s' AND '%s'!='%s' AND %s", rgGroupInfo[pipRuleGroup].pclDestKey,pclUrnoSelection,pcgCodeShareSST, pcgDeletionStatusIndicator, pcgDelValue, pclTmpFlnoSelection);
    }

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);
    UtcToLocal(pclTimeNow);

    memset(pclTmp, 0, sizeof(pclTmp));
    strncpy(pclYear,pclTimeNow,4);
    strncpy(pclMonth,pclTimeNow+4,2);
    strncpy(pclDay,pclTimeNow+6,2);
    strncpy(pclHour,pclTimeNow+8,2);
    strncpy(pclMin,pclTimeNow+10,2);
    strncpy(pclSec,pclTimeNow+12,2);

    sprintf(pclTmp,"to_date('%s%s%s%s%s %s%s%s%s%s','YYYY%sMM%sDD HH24%sMI%sSS')",pclYear, pcgDataListDelimiter,pclMonth, pcgDataListDelimiter,pclDay,pclHour,pcgDataListDelimiter, pclMin,pcgDataListDelimiter,pclSec,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter);
    /*sprintf(pclTmp,"%s-%s-%s", pclYear, pclMonth, pclDay);*/

    if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 ||
         strcmp(pcpTable, "Current_Departures") == 0 )
    {
        sprintf(pclTmpTime,"%s=%s","LSTU",pclTmp);
        sprintf(pcpSqlBuf,"UPDATE %s SET %s,%s %s", pcpTable, pclString, pclTmpTime, pclTmpSelection);
    }
    else  /* tvo_fix: use UREF not URNO */
    {
    	sprintf(pcpSqlBuf,"UPDATE %s SET %s WHERE %s=%s", pcpTable, pclString, rgGroupInfo[pipRuleGroup].pclDestKey,pclUrnoSelection);   /* replace pcpSelection by pclTmpSelection */
    }

}

/*--------------------------------------------------------------------------------------------------------------------------------*/
static void buildUpdateQueryPartial(char *pcpSqlBuf, char * pcpTable, char * pcpDestFieldList, char *pcpDestFieldData, char * pcpSelection, int pipRuleGroup)
{
    int ilCount = 0;
    char *pclFunc = "buildUpdateQueryPartial";
    char pclTimeNow[TIMEFORMAT] = "\0";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";

    char pclTmp[256] = "\0";
    char *pclTmp1 = "\0";
    char pclTmpSelection[256] = "\0";
    char pclUrnoSelection[256] = "\0";
    char pclTmpTime[256] = "\0";
    char pclTmpField[256] = "\0";
    char pclTmpData[256] = "\0";
    char pclString[4096] = "\0";

    strcpy(pclTmpSelection, pcpSelection);
    pclTmp1 = strstr(pclTmpSelection, "=");
    strcpy(pclUrnoSelection, pclTmp1+1);

    sprintf(pclTmpSelection,"WHERE %s=%s",rgGroupInfo[pipRuleGroup].pclDestKey,pclUrnoSelection);

    for (ilCount = 1; ilCount <= GetNoOfElements(pcpDestFieldData,pcgDataListDelimiter[0]); ilCount++)
    {
        memset(pclTmpField, 0, sizeof(pclTmpField));
        memset(pclTmpData, 0, sizeof(pclTmpData));
        memset(pclTmp, 0, sizeof(pclTmp));

        get_item(ilCount, pcpDestFieldList, pclTmpField, 0, ",", "\0", "\0");
        TrimRight(pclTmpField);

        get_item(ilCount, pcpDestFieldData, pclTmpData, 0, pcgDataListDelimiter, "\0", "\0");
        TrimRight(pclTmpData);

		if (strcmp(pclTmpData,"''") == 0)   /* skip null value */
			continue;

        if (strlen(pclString) == 0)    /* first field  */
        {
            sprintf(pclTmp, "%s=%s", pclTmpField, pclTmpData);
            strcat(pclString, pclTmp);
        }
        else
        {
            strcat(pclString, ",");
            sprintf(pclTmp, "%s=%s", pclTmpField, pclTmpData);
            strcat(pclString, pclTmp);
        }
    }

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);
    UtcToLocal(pclTimeNow);

    memset(pclTmp, 0, sizeof(pclTmp));
    strncpy(pclYear,pclTimeNow,4);
    strncpy(pclMonth,pclTimeNow+4,2);
    strncpy(pclDay,pclTimeNow+6,2);
    strncpy(pclHour,pclTimeNow+8,2);
    strncpy(pclMin,pclTimeNow+10,2);
    strncpy(pclSec,pclTimeNow+12,2);

    sprintf(pclTmp,"to_date('%s%s%s%s%s %s%s%s%s%s','YYYY%sMM%sDD HH24%sMI%sSS')",pclYear, pcgDataListDelimiter,pclMonth, pcgDataListDelimiter,pclDay,pclHour,pcgDataListDelimiter, pclMin,pcgDataListDelimiter,pclSec,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter,pcgDataListDelimiter);
    /*sprintf(pclTmp,"%s-%s-%s", pclYear, pclMonth, pclDay);*/

    if ( strcmp(pcpTable, "Current_Arrivals"  ) == 0 ||
         strcmp(pcpTable, "Current_Departures") == 0 )
    {
        sprintf(pclTmpTime,"%s=%s","LSTU",pclTmp);
        sprintf(pcpSqlBuf,"UPDATE %s SET %s,%s %s", pcpTable, pclString, pclTmpTime, pclTmpSelection);
    }
    else  /* tvo_fix: use UREF not URNO */
		sprintf(pcpSqlBuf,"UPDATE %s SET %s %s", pcpTable, pclString, pclTmpSelection);   /* replace pcpSelection by pclTmpSelection */
}
/*----------------------------------------------------------------------------------------------------------*/
static int convertSrcValToDestVal(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _LINE * rpLine, char * pcpDestFieldValue, char * pcpSelection, char *pcpAdid)
{
    int ilRC = RC_SUCCESS;
    int ilCount = 0;
    char *pclFunc = "convertSrcValToDestVal";
    char pclOperator[64] = "\0";

    memset(pcpDestFieldValue,0,sizeof(pcpDestFieldValue));

    strcpy(pclOperator,rpLine->pclDestFieldOperator);
    TrimRight(pclOperator);
	dbg(DEBUG, "%s: pclOperator = <%s>, pcpDestFieldName = <%s>", pclFunc, pclOperator, pcpDestFieldName);
    /*Changing it binary search later*/
    for (ilCount = 0; ilCount < OPER_CODE; ilCount++)
    {
        if( strcmp(pclOperator, CODEFUNC[ilCount].pclOperCode) == 0 )
        {
            dbg(TRACE,"%s pclOperCode[%d] - <%s>", pclFunc, ilCount, strncmp(CODEFUNC[ilCount].pclOperCode," ",1) == 0 ? "DefaultCopy" : CODEFUNC[ilCount].pclOperCode);
            ilRC = (CODEFUNC[ilCount].pflOpeFunc)(pcpDestFieldValue, pcpSourceFieldValue, rpLine, pcpSelection, pcpAdid);
        }
    }

    dbg(TRACE,"SourceField<%s> SourceValue<%s> DestinationField<%s> DestinationValue<%s>\n",
        pcpSourceFieldName, pcpSourceFieldValue, pcpDestFieldName, pcpDestFieldValue);

    return ilRC;
}

int getRotationFlightData(char *pcpTable, char *pcpUrnoSelection, char *pcpFields, char (*pcpRotationData)[LISTLEN], char *pcpAdid)
{
    int ilCount = 0;
    int ilNoEle = 0;
    int ilRC = RC_FAIL;
    short slLocalCursor = 0, slFuncCode = 0;
    char *pclFunc = "getRotationFlightData";
    char pclAdidTmp[2] = "\0";
    char pclTmp[128] = "\0";
    char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0";

    if( strncmp(pcpAdid,"A",1) != 0 && strncmp(pcpAdid,"D",1) != 0 )
    {
        dbg(TRACE, "%s The adid is neither A nor D", pclFunc);
        return ilRC;
    }
    else
    {
        dbg(DEBUG, "%s The adid is <%s>", pclFunc, pcpAdid);
    }

    if( strncmp(pcpAdid, "A", 1) == 0 )
    {
        strcpy(pclTmp,"AND ADID='D'");
    }
    else
    {
        strcpy(pclTmp,"AND ADID='A'");
        sprintf(pclWhere, "WHERE URNO = %s ", pcpUrnoSelection);

        buildSelQuery(pclSqlBuf, pcpTable, "RKEY", pclWhere);
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
                dbg(TRACE, "<%s> Retrieving source data - Found <%s>", pclFunc, pclSqlData);
                BuildItemBuffer(pclSqlData, NULL, 1, ",");
                strcpy(pcpUrnoSelection, pclSqlData);
                ilRC = RC_SUCCESS;
                break;
        }
    }
    sprintf(pclWhere, "WHERE RKEY = %s ", pcpUrnoSelection);
    strcat(pclWhere, pclTmp);

    /*
    1-build select query
    2-store the found rotation flight data
    */

    buildSelQuery(pclSqlBuf, pcpTable, pcpFields, pclWhere);
    ilNoEle = GetNoOfElements(pcpFields, ',');
    dbg(DEBUG, "%s select<%s> field NUmber<%d>", pclFunc, pclSqlBuf, ilNoEle);

    slLocalCursor = 0;
	slFuncCode = START;
    while( sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS )
    {
        slFuncCode = NEXT;

        BuildItemBuffer(pclSqlData, NULL, ilNoEle, ",");
        dbg(TRACE,"%s pclSqlData<%s>", pclFunc, pclSqlData);

        if (ilCount < ARRAYNUMBER)
        {
            strcpy(pcpRotationData[ilCount], pclSqlData);
        }
        else
        {
            dbg(TRACE,"%s The number of rotation flights exceeds the array", pclFunc);
            break;
        }
        ilCount++;
    }
    close_my_cursor(&slLocalCursor);

    if (ilCount == 0)
    {
        return RC_FAIL;
    }
    else
    {
       return RC_SUCCESS;
    }
}

void showRotationFlight(char (*pclRotationData)[LISTLEN])
{
    int ilCount = 0;
    char *pclFunc = "showRotationFlight";

    for(ilCount = 0; ilCount < ARRAYNUMBER; ilCount++)
    {
        if (strlen(pclRotationData[ilCount]) > 0)
        {
            dbg(DEBUG,"%s <%d> Rotation Flight<%s>", pclFunc, ilCount, pclRotationData[ilCount]);
        }
    }
}

static int mapping(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpAdidValue, int ipIsCodeShareChange)
{
    int ilRc = 0;
    int ilFlag = FALSE;
    int ilStatusMaster = 0;
    int ilDataListNo = 0;
    int ilCount = 0;
    int ilRuleGroup = 0;
    char *pclFunc = "mapping";
    char *pclTmp = NULL;
    short slLocalCursor = 0, slFuncCode = 0;
    char pclTimeWindowRefFieldVal[32] = "\0";
    char pclUrnoSelection[64] = "\0";
    char pclTmpSelection[64] = "\0";
    char pclHardcodeShare_DestFieldList[256] = "\0";
    char pclSqlBuf[4016] = "\0";       /* tvo_fix: 2048 -> 4016 */
    char pclSqlData[2048] = "\0";
    char pclSourceFieldList[2048] = "\0";
    char pclSourceDataList[2048] = "\0";
    _HARDCODE_SHARE pclHardcodeShare;
    char pclDatalist[ARRAYNUMBER][LISTLEN] = {"\0"};
    _QUERY pclQuery[ARRAYNUMBER] = {"\0"};

    if ( strcmp(pcpAdidValue,"A") == 0 && strcmp(pcpTable,"AFTTAB") == 0 )
    {
        getRefTimeFiledValueFromSource(pcpTable, pcpFields, pcpNewData, pcpSelection, pclTimeWindowRefFieldVal, pcgTimeWindowRefField_AFTTAB_Arr);
    }
    else if ( strcmp(pcpAdidValue,"D") == 0 && strcmp(pcpTable,"AFTTAB") == 0 )
    {
        getRefTimeFiledValueFromSource(pcpTable, pcpFields, pcpNewData, pcpSelection, pclTimeWindowRefFieldVal, pcgTimeWindowRefField_AFTTAB_Dep);
    }
    else if ( strcmp(pcpTable,"CCATAB") == 0 )
    {
        getRefTimeFiledValueFromSource(pcpTable, pcpFields, pcpNewData, pcpSelection, pclTimeWindowRefFieldVal, pcgTimeWindowRefField_CCATAB);
    }

    if( isInTimeWindow(pclTimeWindowRefFieldVal, pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit) == RC_FAIL )
    {
        return RC_FAIL;
    }

    /*return master - codeshare flight hardcode field and data list*/
    strcpy(pclHardcodeShare_DestFieldList, "SST,MFC,MFN,MFX,MFF");
    getHardcodeShare_DataList(pcpFields, pcpNewData, &pclHardcodeShare);

    /*fya 0.2 To determine the applied rule group*/
    ilRuleGroup = toDetermineAppliedRuleGroup(pcpTable, pcpFields, pcpNewData, pcpAdidValue);
    if ( ilRuleGroup == 0 )
    {
        dbg(TRACE,"%s ilRuleGroup == 0 -> There is no applied rules", pclFunc);
        ilRc = RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s Applied rule group is %d", pclFunc, ilRuleGroup);
    }

    /*if the required fields are not all filled: then either retrieve them from the source table, or leave them as blank*/
    if ( igGetDataFromSrcTable == TRUE)
    {
        ilRc = matchFieldListOnGroup(ilRuleGroup, pclSourceFieldList);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The source field list is invalid", pclFunc);
            return RC_FAIL;
        }
        else
        {
            /*dbg(DEBUG,"%s The source field list is <%s>", pclFunc, pclSourceFieldList);*/
        }

        ilRc = getSourceFieldData(pcpTable, pclSourceFieldList, pcpSelection, pclSourceDataList);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The source field data is not found - using the original field and data list", pclFunc);
            ilDataListNo = buildDataArray(pcpFields, pcpNewData, pclDatalist, pcpSelection);
        }
        else/*this way*/
        {
            dbg(TRACE,"%s The source field list is found", pclFunc);

            /*dbg(DEBUG,"%s pcgSourceFiledList[%d] <%s>", pclFunc, ilRuleGroup, pcgSourceFiledList[ilRuleGroup]);
            dbg(DEBUG,"%s pcgDestFiledList[%d] <%s>", pclFunc, ilRuleGroup, pcgDestFiledList[ilRuleGroup]);*/
            ilDataListNo = buildDataArray(pclSourceFieldList, pclSourceDataList, pclDatalist, pcpSelection);
            ilFlag = TRUE;
        }
    }
    else
    {
        /*using the original field and data list*/
        dbg(TRACE,"%s The getting source data config option is not set - using the original field and data list", pclFunc);
        ilDataListNo = buildDataArray(pcpFields, pcpNewData, pclDatalist, pcpSelection);
    }

    /**/
    if (ilFlag == FALSE)
    {
        /*
        if (ipIsCodeShareChange == CODESHARE_NONCHANGE)
        {
            appliedRules( ilRuleGroup, pcpFields, pclDatalist[MASTER_RECORD], pcgSourceFiledList[ilRuleGroup],
                             pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule, pcpSelection, pcpAdidValue,
                             MASTER_RECORD, pclHardcodeShare_DestFieldList, pclHardcodeShare, pclQuery+MASTER_RECORD);
        }
        else
        */
        {
            for(ilCount = 0; ilCount < ilDataListNo; ilCount++)
            {
                appliedRules( ilRuleGroup, pcpFields, pclDatalist[ilCount], pcgSourceFiledList[ilRuleGroup],
                             pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule, pcpSelection, pcpAdidValue,
                             ilCount, pclHardcodeShare_DestFieldList, pclHardcodeShare, pclQuery+ilCount);
            }
        }
    }/*this way*/
    else
    {
        /*
        if (ipIsCodeShareChange == CODESHARE_NONCHANGE)
        {
            appliedRules( ilRuleGroup, pclSourceFieldList, pclDatalist[MASTER_RECORD], pcgSourceFiledList[ilRuleGroup],
                        pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule, pcpSelection, pcpAdidValue,
                        MASTER_RECORD, pclHardcodeShare_DestFieldList, pclHardcodeShare, pclQuery+MASTER_RECORD);
        }
        else
        */
        {
            for(ilCount = 0; ilCount < ilDataListNo; ilCount++)
            {
                appliedRules( ilRuleGroup, pclSourceFieldList, pclDatalist[ilCount], pcgSourceFiledList[ilRuleGroup],
                             pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule, pcpSelection, pcpAdidValue,
                             ilCount, pclHardcodeShare_DestFieldList, pclHardcodeShare, pclQuery+ilCount);
            }
        }
    }

    for(ilCount = 0; ilCount < ilDataListNo; ilCount++)
    {
        dbg(DEBUG,"%s pclQuery[%d].pclInsertQuery<%s>\n",pclFunc, ilCount, pclQuery[ilCount].pclInsertQuery);
        dbg(DEBUG,"%s pclQuery[%d].pclUpdateQuery<%s>\n\n",pclFunc, ilCount, pclQuery[ilCount].pclUpdateQuery);
    }
    /*
    build master record searching query
    if master record not found
    {
        insert master and codeshare flights
    }
    else if found
    {
        then, search for the NO. of existed codeshare flight in destination table

        then, 1-update master flight
              2-delete all existed codeshare flights
              3-insert all new existed codeshare flights

        /codeshare flight - delete and insert -> no update action/
    }
    */

    strcpy(pclTmpSelection, pcpSelection);
    pclTmp = strstr(pclTmpSelection, "=");
    if (pclTmp != NULL)    /* tvo_add: not found URNO case */
    {
        pclTmp++;
        while (*pclTmp == ' ') pclTmp++;
        strcpy(pclUrnoSelection, pclTmp);
        TrimRight(pclUrnoSelection);
    }else
	{
		strcpy(pclUrnoSelection,"''");    /* NULL URNO */
	}
    #if 0
    if(pclTmp[1] != ' ')
    {
       strcpy(pclUrnoSelection, pclTmp+1);
    }
    else
    {
        strcpy(pclUrnoSelection, pclTmp+2);
    }
	#endif

    /*ilRc = flightSearch(rgRule.rlLine[0].pclDestTable, rgRule.rlLine[0].pclDestKey, rgRule.rlLine[0].pclDestKey, pclUrnoSelection);*/
    ilRc = flightSearch(rgGroupInfo[ilRuleGroup].pclDestTable, rgGroupInfo[ilRuleGroup].pclDestKey, rgGroupInfo[ilRuleGroup].pclDestKey, pclUrnoSelection);
    switch (ilRc)
    {
        case RC_FAIL:
            /*error -> return*/
            dbg(TRACE,"%s select sql error -> return", pclFunc);
            return RC_FAIL;
            break;
        case NOTFOUND:
            /*insert master and codeshare flights*/
			dbg(DEBUG, "%s notfound: /*insert master and codeshare flights*/",pclFunc);
            ilStatusMaster = INSERT;
            break;
        case RC_SUCCESS:/*FOUND*/
			dbg(DEBUG, "found: update record");
            ilStatusMaster = UPDATE;
            break;
        default:
            dbg(TRACE, "%s Invalid Value -> Return", pclFunc);
            return RC_FAIL;
            break;
    }

	dbg(DEBUG, "%s INSERT = <%d>, UPDATE = <%d>, ilStatusMaster = <%d>",pclFunc, INSERT, UPDATE, ilStatusMaster);
	/* tvo_add: change to fix the case of insert only */
	if (igEnableInsertOnly == TRUE)
	{
		ilStatusMaster = INSERT;
		dbg(DEBUG, "%s INSERT ONLY = <%d:%d,%d>, ilStatusMaster = <%d:%d,%d,%d>",
					pclFunc, igEnableInsertOnly, TRUE, FALSE, ilStatusMaster, INSERT, UPDATE, DELETE);
	}

    if (ilStatusMaster == INSERT)
    {
        insertFligthts(MASTER_RECORD, ilDataListNo, pclQuery);
    }
    else if (ilStatusMaster == UPDATE)
    {
        /*delete the codeshare flights->

        there are two cases under update scenario:
        1-update all
        2-update the master data and insert the other codeshare data

            The critera is the number of codeshare fligths and the change of original JFNO fields
            1-update master data
            2-insert all codeshare data*/

        /*JFNO is not changed, then -> update all master and codeshare fligths in one query*/
        if (ipIsCodeShareChange == CODESHARE_NONCHANGE)
        {
            updateAllFlights(pclQuery,ilDataListNo,ipIsCodeShareChange);
        }
        else /*if (ipIsCodeShareChange == TRUE)*/
        {
            /*later, compare each record, and only delete & insert the new ones*/
            if (ipIsCodeShareChange != CODESHARE_CREATION)
            {
                deleteCodeShareFligths(rgGroupInfo[ilRuleGroup].pclDestTable, rgGroupInfo[ilRuleGroup].pclDestKey, pclUrnoSelection);
            }
            updateAllFlights(pclQuery,ilDataListNo,ipIsCodeShareChange);
            insertFligthts(MASTER_RECORD+1, ilDataListNo, pclQuery);
        }
    }
    return RC_SUCCESS;
}

static int buildDataArray(char *pcpFields, char *pcpNewData, char (*pcpDatalist)[LISTLEN], char *pcpSelection)
{
    int ilRC = 1;
    int ilCodeShareNo = 0;
    int ilFieldListNo = 0;
    int ilDataListNo = 0;
    int ilEleCount = 0;
    int ilCountCodeShare = 0;
    char *pclFunc = "buildDataArray";
    char pclTmpFieldName[64] = "\0";
    char pclTmpData[64] = "\0";
    char pclTmpDataList[LISTLEN] = "\0";
    char pcpFormat[LISTLEN] = "\0";
    _VIAL pclCodeShare[ARRAYNUMBER];

    /*prepare the flight data list including the original one*/
    strcpy(pcpDatalist[0],pcpNewData);

    /*extract codeshare flight info*/
    if( igEnableCodeshare == TRUE )
    {
        ilCodeShareNo = getCodeShare(pcpFields, pcpNewData, pclCodeShare, pcpFormat,"ALC3",pcpSelection);

        if (ilCodeShareNo == RC_FAIL)
        {
            dbg(DEBUG,"%s ilCodeShareNo<%d>", pclFunc, ilCodeShareNo);
            return ilRC;
        }

        ilRC = ilCodeShareNo + 1;
        dbg(DEBUG,"---------------------%s ilCodeShareNo<%d> ilRC<%d> pcpFormat<%s>-------------------",pclFunc,ilCodeShareNo, ilRC, pcpFormat);

        ilFieldListNo = GetNoOfElements(pcpFields, ',');
        ilDataListNo  = GetNoOfElements(pcpNewData, ',');

        if (ilFieldListNo != ilDataListNo)
        {
            dbg(TRACE,"%s The data<%d> and field<%d> list number is not matched", pclFunc, ilDataListNo, ilFieldListNo);
            return RC_FAIL;
        }

        for(ilCountCodeShare = 0; ilCountCodeShare < ilCodeShareNo; ilCountCodeShare++)
        {
            dbg(DEBUG,"%s <%d> ALC2<%s> ALC3<%s> FLNS<%s>",pclFunc,ilCodeShareNo, pclCodeShare[ilCountCodeShare].pclAlc2, pclCodeShare[ilCountCodeShare].pclAlc3, pclCodeShare[ilCountCodeShare].pclFlno);

            memset(pclTmpFieldName,0,sizeof(pclTmpFieldName));
            memset(pclTmpData,0,sizeof(pclTmpData));
            memset(pclTmpDataList,0,sizeof(pclTmpDataList));

            for(ilEleCount = 1; ilEleCount <= ilFieldListNo; ilEleCount++)
            {
                get_item(ilEleCount, pcpFields, pclTmpFieldName, 0, ",", "\0", "\0");
                TrimRight(pclTmpFieldName);

                get_item(ilEleCount, pcpNewData, pclTmpData, 0, ",", "\0", "\0");
                TrimRight(pclTmpData);

                if ( strncmp(pclTmpFieldName,"ALC2",4)==0 )
                {
                    strcpy(pclTmpData,pclCodeShare[ilCountCodeShare].pclAlc2);
                }
                else if ( strncmp(pclTmpFieldName,"ALC3",4)==0 )
                {
                    strcpy(pclTmpData,pclCodeShare[ilCountCodeShare].pclAlc3);
                }
                else if ( strncmp(pclTmpFieldName,"FLTN",4)==0 )
                {
                    strcpy(pclTmpData,pclCodeShare[ilCountCodeShare].pclFlno);
                }
                else if ( strncmp(pclTmpFieldName,"FLNS",4)==0 )
                {
                    strcpy(pclTmpData+3,pclCodeShare[ilCountCodeShare].pclFlno);
                }
                else if ( strncmp(pclTmpFieldName,"FLNO",4)==0 )
                {
                    sprintf(pclTmpData,"%s %s", pclCodeShare[ilCountCodeShare].pclAlc2, pclCodeShare[ilCountCodeShare].pclFlno);
                }
                /*
                else if ( strncmp(pclTmpFieldName,"JFNO",4)==0 )
                {
                    memset(pclTmpData,0,sizeof(pclTmpData));
                }
                */
                else if ( strncmp(pclTmpFieldName,"FKEY",4)==0 )
                {
                    memset(pclTmpData,0,sizeof(pclTmpData));
                }

                if(ilEleCount == 1)
                {
                    strcat(pclTmpDataList,pclTmpData);
                }
                else
                {
                    strcat(pclTmpDataList,",");
                    strcat(pclTmpDataList,pclTmpData);
                }
            }
            strcpy(pcpDatalist[ilCountCodeShare+1],pclTmpDataList);
        }
    }
    else
    {
        return ilRC;
    }

    /*for(ilCountCodeShare = 0; ilCountCodeShare < 10; ilCountCodeShare++)*/
    for(ilCountCodeShare = 0; ilCountCodeShare < ilRC; ilCountCodeShare++)
    {
        if(strlen(pcpDatalist[ilCountCodeShare])>0)
            dbg(DEBUG,"%s ++++++++++pcpDatalist[%d] <%s>++++++++++++++++\n\n",pclFunc, ilCountCodeShare, pcpDatalist[ilCountCodeShare]);
    }

    return ilRC;
}

static int flightSearch(char *pcpTable, char *pcpField, char *pcpKey, char *pcpSelection)
{
    int ilRC = 0;
    char *pclFunc = "flightSearch";

    char pclSqlBuf[2560] = "\0";
    char pclSqlData[2560] = "\0";

    sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE %s=%s", pcpField, pcpTable, pcpKey, pcpSelection);
    dbg(TRACE, "%s pclSqlBuf<%s>", pclFunc, pclSqlBuf);

    ilRC = RunSQL(pclSqlBuf, pclSqlData);
    if (ilRC != DB_SUCCESS)
    {
        if (ilRC != NOTFOUND)	/* tvo_debug: fix the case of not exist table */
		{
        	dbg(TRACE, "<%s>: Retrieving dest data - Fails", pclFunc);
			return RC_FAIL;
		}
    }

    switch(ilRC)
    {
        case NOTFOUND:
            dbg(TRACE, "<%s> Retrieving source data - Not Found", pclFunc);
            ilRC = NOTFOUND;
            break;
        default:
            dbg(TRACE, "<%s> Retrieving source data - Found <%s>", pclFunc, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

static void getHardcodeShare_DataList(char *pcpFields, char *pcpData, _HARDCODE_SHARE *pcpHardcodeShare)
{
    int ilEleCount = 0;
    int ilFieldListNo = 0;
    int ilDataListNo = 0;
    char *pclFunc = "getHardcodeShare_DataList";
    char pclTmpData[256] = "\0";
    char pclTmpFieldName[256] = "\0";

    ilFieldListNo = GetNoOfElements(pcpFields, ',');
    ilDataListNo = GetNoOfElements(pcpData, ',');

    if(ilFieldListNo != ilDataListNo)
    {
        dbg(TRACE,"%s ilFieldListNo<%d>!=ilDataListNo<%d>",pclFunc,ilFieldListNo,ilDataListNo);
    }

    for(ilEleCount = 1; ilEleCount <= ilFieldListNo; ilEleCount++)
    {
        get_item(ilEleCount, pcpFields, pclTmpFieldName, 0, ",", "\0", "\0");
        TrimRight(pclTmpFieldName);

        get_item(ilEleCount, pcpData, pclTmpData, 0, ",", "\0", "\0");
        TrimRight(pclTmpData);

        if ( strncmp(pclTmpFieldName,"JCNT",4)==0 )
        {
            /*to-do*/
            if (strlen(pclTmpData) == 2)
            {
                strcpy(pcpHardcodeShare->pclSST,pclTmpData);
            }
            else if (strlen(pclTmpData) < 2 && strlen(pclTmpData) > 0)
            {
                sprintf(pcpHardcodeShare->pclSST,"0%s",pclTmpData);
            }
        }
        else if ( strncmp(pclTmpFieldName,"ALC3",4)==0 )
        {
            strcpy(pcpHardcodeShare->pclMFC,pclTmpData);
        }
        else if ( strncmp(pclTmpFieldName,"FLTN",4)==0 )
        {
            strcpy(pcpHardcodeShare->pclMFN,pclTmpData);
        }
        else if ( strncmp(pclTmpFieldName,"FLNS",4)==0 )
        {
            strcpy(pcpHardcodeShare->pclMFX,pclTmpData);
        }
        else if ( strncmp(pclTmpFieldName,"FLNO",4)==0 )
        {
            strcpy(pcpHardcodeShare->pclMFF,pclTmpData);
        }
    }
}

static int checkCodeShareChange(char *pcpFields, char *pcpNewData, char *pcpOldData, int *ipCodeShareChange)
{
    int ilRc = RC_SUCCESS;
    int ilNew = FALSE;
    int ilOld = FALSE;
    char *pclFunc = "checkCodeShareChange";
    char pclCodeShareOldValue[8192] = "\0";
    char pclCodeShareNewValue[8192] = "\0";

    /*getting the New VIAL Value*/
    ilRc = extractField(pclCodeShareNewValue, "JFNO", pcpFields, pcpNewData);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The New JFNO value<%s> is invalid", pclFunc, pclCodeShareNewValue);
        ilNew = FALSE;
    }
    else
    {
        ilNew = TRUE;
        dbg(TRACE,"%s The New JFNO value<%s> is valid", pclFunc, pclCodeShareNewValue);
    }

    /*getting the Old VIAL Value*/
    ilRc = extractField(pclCodeShareOldValue, "JFNO", pcpFields, pcpOldData);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The Old JFNO value<%s> is invalid", pclFunc, pclCodeShareOldValue);
        ilOld = FALSE;
    }
    else
    {
        dbg(TRACE,"%s The Old JFNO value<%s> is valid", pclFunc, pclCodeShareOldValue);
        ilOld = TRUE;
    }

    if (strlen(pclCodeShareNewValue) > 0 && strlen(pclCodeShareOldValue) > 0)
    {
        if ( strcmp(pclCodeShareOldValue, pclCodeShareNewValue) != 0 )
        {
            /*codeshare change*/
            *ipCodeShareChange = CODESHARE_MODIFICATION;
        }
        else
        {
            *ipCodeShareChange = CODESHARE_NONCHANGE;
        }
        dbg(TRACE,"%s Codeshare Change <%d>",pclFunc, *ipCodeShareChange);
    }/*codeshare creation*/
    else if (strlen(pclCodeShareNewValue) > 0 && strlen(pclCodeShareOldValue) == 0) /*if (ilNew == TRUE && ilOld == FALSE)*/
    {
        dbg(TRACE,"%s Codeshare Creation",pclFunc);
        *ipCodeShareChange = CODESHARE_CREATION;
    }
    else if (strlen(pclCodeShareNewValue) == 0 && strlen(pclCodeShareOldValue) > 0)/*if (ilNew == FALSE && ilOld == TRUE)*/
    {
        dbg(TRACE,"%s Codeshare Deletion",pclFunc);
        *ipCodeShareChange = CODESHARE_DELETION;
    }
    else /*if (strlen(pclCodeShareNewValue) == 0 && strlen(pclCodeShareOldValue) == 0)*//*if (ilNew == FALSE && ilOld == FALSE)*/
    {
        dbg(TRACE,"%s Codeshare Not Change",pclFunc);
        *ipCodeShareChange = CODESHARE_NONCHANGE;
    }
    dbg(TRACE,"%s ipCodeShareChange<%d>",pclFunc, *ipCodeShareChange);

    return ilRc;
}

static int checkVialChange(char *pcpFields, char *pcpNewData, char *pcpOldData, int *ipVialChange)
{
    int ilRc = RC_SUCCESS;
    char *pclFunc = "checkVialChange";
    char pclVianValue[64] = "\0";
    char pclVialOldValue[8192] = "\0";
    char pclVialNewValue[8192] = "\0";

    /*getting the VIAN*/
    ilRc = extractField(pclVianValue, "VIAN", pcpFields, pcpNewData);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The VIAN value<%s> is invalid", pclFunc, pclVianValue);
        return RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s The VIAN value<%s> is valid", pclFunc, pclVianValue);
    }

    if( atoi(pclVianValue) == 0 )
    {
        *ipVialChange = FALSE;
    }
    else
    {
        /*getting the New VIAL Value*/
        ilRc = extractField(pclVialNewValue, "VIAL", pcpFields, pcpNewData);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The New VIAL value<%s> is invalid", pclFunc, pclVialNewValue);
            return RC_FAIL;
        }
        else
        {
            dbg(TRACE,"%s The New VIAL value<%s> is valid", pclFunc, pclVialNewValue);
        }

        /*getting the Old VIAL Value*/
        ilRc = extractField(pclVialOldValue, "VIAL", pcpFields, pcpOldData);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The Old VIAL value<%s> is invalid", pclFunc, pclVialOldValue);
            return RC_FAIL;
        }
        else
        {
            dbg(TRACE,"%s The Old VIAL value<%s> is valid", pclFunc, pclVialOldValue);
        }

        if ( strcmp(pclVialOldValue,pclVialNewValue) != 0 )
        {
            *ipVialChange = TRUE;
        }
        else
        {
            *ipVialChange = FALSE;
        }
    }

    dbg(TRACE,"%s ipVialChange<%s>",pclFunc, *ipVialChange==FALSE?"FALSE":"TRUE");

    return ilRc;
}

static int refreshTable(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit)
{
    int ilRc = RC_SUCCESS;
    char *pclFunc = "refreshTable";
    char pclTimeNow[TIMEFORMAT] = "\0";
    char pclTimeLowerLimit[TIMEFORMAT] = "\0";
    char pclTimeUpperLimit[TIMEFORMAT] = "\0";

    char pclTimeWindowLowerLimitOriginal[TIMEFORMAT] = "\0";
    char pclTimeWindowUpperLimitOriginal[TIMEFORMAT] = "\0";

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);

    strcpy(pclTimeLowerLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeLowerLimit, igTimeWindowLowerLimit * 24 * 60 * 60, 1);

    strcpy(pclTimeUpperLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeUpperLimit, igTimeWindowUpperLimit * 24 * 60 * 60, 1);

    strcpy(pclTimeWindowLowerLimitOriginal, pcpTimeWindowLowerLimit);
    strcpy(pclTimeWindowUpperLimitOriginal, pcpTimeWindowUpperLimit);

    memset(pcpTimeWindowLowerLimit,0,sizeof(pcpTimeWindowLowerLimit));
    memset(pcpTimeWindowUpperLimit,0,sizeof(pcpTimeWindowUpperLimit));

    strcpy(pcpTimeWindowLowerLimit, pclTimeLowerLimit);
    strcpy(pcpTimeWindowUpperLimit, pclTimeUpperLimit);

    dbg(TRACE,"The original time range is <%s> ~ <%s>", pclTimeWindowLowerLimitOriginal, pclTimeWindowUpperLimitOriginal);
    dbg(TRACE,"The current  time range is <%s> ~ <%s>", pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);

    /*
    delete all flights from lowerlimit - x ~ lowerlimit
    insert all flights from upperlimit -x ~upperlimit
    */
    deleteFlightsOutOfTimeWindow(pclTimeWindowLowerLimitOriginal, pcpTimeWindowLowerLimit);
    insertFligthsNewInTimeWindow(pclTimeWindowUpperLimitOriginal, pcpTimeWindowUpperLimit);

    return ilRc;
}

static int getFieldValue(char *pcpFields, char *pcpNewData, char *pcpFieldName, char *pcpFieldValue)
{
    int ilRc = RC_FAIL;
    char *pclFunc = "getFieldValue";

	dbg(DEBUG, "%s: pcpFields=<%s>, pcpNewData=<%s>, pcpFieldName=<%s>", pclFunc, pcpFields, pcpNewData, pcpFieldName);

    if(strncmp(pcpFieldName," ",1) == 0 || strlen(pcpFieldName) == 0)
    {
        return ilRc;
    }

    ilRc = extractField(pcpFieldValue, pcpFieldName, pcpFields, pcpNewData);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The %s value<%s> is invalid", pclFunc,pcpFieldName, pcpFieldValue);
        ilRc = RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s The %s value<%s> is valid", pclFunc, pcpFieldName, pcpFieldValue);
        ilRc = RC_SUCCESS;
    }
    return ilRc;
}

static void updateTimeWindow(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit)
{
    char *pclFunc = "updateTimeWindow";
    char pclTimeNow[TIMEFORMAT] = "\0";
    char pclTimeLowerLimit[TIMEFORMAT] = "\0";
    char pclTimeUpperLimit[TIMEFORMAT] = "\0";

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);

    strcpy(pclTimeLowerLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeLowerLimit, igTimeWindowLowerLimit * 24 * 60 * 60, 1);

    strcpy(pclTimeUpperLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeUpperLimit, igTimeWindowUpperLimit * 24 * 60 * 60, 1);

    memset(pcpTimeWindowLowerLimit,0,sizeof(pcpTimeWindowLowerLimit));
    memset(pcpTimeWindowUpperLimit,0,sizeof(pcpTimeWindowUpperLimit));
    strcpy(pcpTimeWindowLowerLimit, pclTimeLowerLimit);
    strcpy(pcpTimeWindowUpperLimit, pclTimeUpperLimit);

    dbg(TRACE,"The current  time range is <%s> ~ <%s>", pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
}

static int iniTables(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit, int ipOptionToTruncate)
{
    int ilCount = 0;
    int ilFlightCount = 0;
    int ilRc = RC_FAIL;
    int ilNoEle = 0;
    int ilRuleGroup = 0;
    int ilDataListNo = 0;
    char *pclFunc = "iniTables";
    short slLocalCursor = 0, slFuncCode = 0;
    short slLocalCursorT = 0, slFuncCodeT = 0;
    char pclSourceFieldList[2048] = "\0";
    char pclAdidValue[2] =  "\0";
    char pclUrnoValue[16] = "\0";
    char pclHardcodeShare_DestFieldList[256] = "\0";
    char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclSqlDataT[4096] = "\0",pclWhere[2048] = "\0", pclSelection[2048] = "\0";
    char pclDatalist[ARRAYNUMBER][LISTLEN] = {"\0"};
    _HARDCODE_SHARE pclHardcodeShare;
    _QUERY pclQuery[ARRAYNUMBER] = {"\0"};


    /*if (igTotalNumbeoOfRule > 2)
    {
        dbg(TRACE,"%s The number of rule group<%d> is invalid", pclFunc, igTotalNumbeoOfRule);
        return RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s The number of rule group<%d> is valid", pclFunc, igTotalNumbeoOfRule);
    }
	*/

    for (ilRuleGroup = 1; ilRuleGroup <= igTotalNumbeoOfRule; ilRuleGroup++)
    {
		/*truncate all existed records in destination table, only truncate not null dest table_tvo_fix */
        if ((ipOptionToTruncate == TRUE)&&(strlen(rgGroupInfo[ilRuleGroup].pclSourceTable) > 0))
            truncateTable(ilRuleGroup);
	}

    for (ilRuleGroup = 1; ilRuleGroup <= igTotalNumbeoOfRule; ilRuleGroup++)
    {
        memset(pclSourceFieldList,0,sizeof(pclSourceFieldList));
		if (strlen(rgGroupInfo[ilRuleGroup].pclSourceTable) == 0)  /* null source table */
			continue; 			/* skip null group rule */

        ilRc = matchFieldListOnGroup(ilRuleGroup, pclSourceFieldList);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The source field list is invalid", pclFunc);
            return RC_FAIL;
        }
        else
        {
            /*dbg(DEBUG,"%s The source field list is <%s>", pclFunc, pclSourceFieldList);*/
			/* tvo_add: ADID field to pclSourceFieldList if not exist */
			if (strcmp(rgGroupInfo[ilRuleGroup].pclSourceTable, "AFTTAB") == 0)
			{
				if (strlen(pclSourceFieldList) == 0)
				{
					sprintf(pclSourceFieldList, "URNO,ADID,%s,%s",pcgTimeWindowRefField_AFTTAB_Arr,pcgTimeWindowRefField_AFTTAB_Dep);
				}
				else
				{
					if (strstr(pclSourceFieldList, "ADID") == NULL)
					{
						strcat(pclSourceFieldList,",ADID");
					}
					if (strstr(pclSourceFieldList, pcgTimeWindowRefField_AFTTAB_Arr) == NULL)
					{
						strcat(pclSourceFieldList,",");
						strcat(pclSourceFieldList,pcgTimeWindowRefField_AFTTAB_Arr);
					}
					if (strstr(pclSourceFieldList, pcgTimeWindowRefField_AFTTAB_Dep) == NULL)
					{
						strcat(pclSourceFieldList,",");
						strcat(pclSourceFieldList,pcgTimeWindowRefField_AFTTAB_Dep);
					}
				}
			}

            ilNoEle = GetNoOfElements(pclSourceFieldList, ',');
        }

        /*change this part later -> group1 == A|D*/
        #if 0
        if( strcmp(rgGroupInfo[ilRuleGroup].pclDestTable, "Current_Arrivals") == 0)
        {
            /*arrival fligths*/
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s' AND ROWNUM < 3", pcgTimeWindowRefField_AFTTAB_Arr, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }
        else if( strcmp(rgGroupInfo[ilRuleGroup].pclDestTable, "Current_Departures") == 0)
        {
             /*departure fligths*/
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s' AND ROWNUM < 3", pcgTimeWindowRefField_AFTTAB_Dep, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }
        else if( strcmp(rgGroupInfo[ilRuleGroup].pclDestTable, "") == 0)
        {
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s' AND ROWNUM < 3", pcgTimeWindowRefField_CCATAB, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }
        #endif

        if( strcmp(rgGroupInfo[ilRuleGroup].pclDestTable, "Current_Arrivals") == 0)
        {
            /*arrival fligths*/
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s'", pcgTimeWindowRefField_AFTTAB_Arr, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }
        else if( strcmp(rgGroupInfo[ilRuleGroup].pclDestTable, "Current_Departures") == 0)
        {
             /*departure fligths*/
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s'", pcgTimeWindowRefField_AFTTAB_Dep, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }
		else if (strcmp(rgGroupInfo[ilRuleGroup].pclSourceTable, "AFTTAB") == 0)    /* tvo_add: where condition for group2 */
		{
			if (ilRuleGroup == 1)       /* arrival flight only */
				sprintf(pclWhere, "WHERE (%s BETWEEN '%s' and '%s')",
							      pcgTimeWindowRefField_AFTTAB_Arr, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
			else if (ilRuleGroup == 2)  /* departure flight */
			{
				sprintf(pclWhere, "WHERE (%s BETWEEN '%s' and '%s')",
								  pcgTimeWindowRefField_AFTTAB_Dep, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
			}
		}
        else if( strcmp(rgGroupInfo[ilRuleGroup].pclSourceTable, "CCATAB") == 0)
        {
            sprintf(pclWhere, "WHERE %s BETWEEN '%s' and '%s'", pcgTimeWindowRefField_CCATAB, pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);
        }else
		{
			dbg(TRACE,"%s Cannot find time window reference field", pclFunc);
			return RC_FAIL;
        }

        buildSelQuery(pclSqlBuf, rgGroupInfo[ilRuleGroup].pclSourceTable, pclSourceFieldList, pclWhere);
        dbg(DEBUG, "%s [%d]select<%s>", pclFunc, ilRuleGroup, pclSqlBuf);
		/* tvo_add: trans type = Insert or New */
		igTransCmd = INSERT;
        slLocalCursor = 0;
        slFuncCode = START;
        ilFlightCount = 0;
        while( sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS )
        {
            slFuncCode = NEXT;
            BuildItemBuffer(pclSqlData, NULL, ilNoEle, ",");
            dbg(TRACE,"%s ilFlightCount<%d> pclSqlData<%s>", pclFunc, ilFlightCount, pclSqlData);

            strcpy(pclHardcodeShare_DestFieldList, "SST,MFC,MFN,MFX,MFF");
            getHardcodeShare_DataList(pclSourceFieldList, pclSqlData, &pclHardcodeShare);

            memset(pclUrnoValue,0,sizeof(pclUrnoValue));
            getFieldValue(pclSourceFieldList, pclSqlData, "URNO", pclUrnoValue);

            memset(pclAdidValue,0,sizeof(pclAdidValue));
            getFieldValue(pclSourceFieldList, pclSqlData, "ADID", pclAdidValue);

            memset(pclSelection,0,sizeof(pclSelection));
            sprintf(pclSelection,"WHERE URNO=%s",pclUrnoValue);

            ilDataListNo = buildDataArray(pclSourceFieldList, pclSqlData, pclDatalist, pclSelection);

            for(ilCount = 0; ilCount < ilDataListNo; ilCount++)
            {
                appliedRules( ilRuleGroup, pclSourceFieldList, pclDatalist[ilCount], pcgSourceFiledList[ilRuleGroup],
                            pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule, pclSelection, pclAdidValue,
                            ilCount, pclHardcodeShare_DestFieldList, pclHardcodeShare, pclQuery+ilCount);
				#ifdef _DEBUG_
                dbg(DEBUG,"\n%s pclQuery[%d].pclInsertQuery<%s>\n",pclFunc, ilCount, pclQuery[ilCount].pclInsertQuery);
                dbg(DEBUG,"%s pclQuery[%d].pclUpdateQuery<%s>\n",pclFunc, ilCount, pclQuery[ilCount].pclUpdateQuery);
				#endif
            }
			/* tvo_debug: missing half of AFTTAB due to truncate table */
            for(ilCount = 0; ilCount < ilDataListNo; ilCount++)
            {
                slLocalCursorT = 0;
                slFuncCodeT = START;

                ilRc = sql_if(slFuncCodeT, &slLocalCursorT, pclQuery[ilCount].pclInsertQuery, pclSqlDataT);
                if( ilRc != DB_SUCCESS )
                {
                    ilRc = RC_FAIL;
                    dbg(TRACE,"%s INSERT - Deletion Error",pclFunc);
                }
                close_my_cursor(&slLocalCursorT);
            }

            ilFlightCount++;
			#ifdef _TEST_  /* for testing only insert first 5 rows */
			if (ilFlightCount >= 5)
				break;
			#endif
        }
        close_my_cursor(&slLocalCursor);
		dbg(TRACE,"%s Group<%d> - Total Fligths Number<%d>", pclFunc, ilRuleGroup, ilFlightCount);
    }


    return RC_SUCCESS;
}

/*----------------------------------------------------------------------------------------------------------*/
static int truncateTable(int ipRuleGroup)
{
    int ilRC = RC_SUCCESS;
    short slLocalCursor = 0, slFuncCode = 0;
    char *pclFunc = "truncateTable";
    char pclTable[64] = "\0";
    char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0", pclSelection[2048] = "\0";

    /*
    if ( ipRuleGroup == 1)
    {
        strcpy(pclTable,"Current_Arrivals");
    }
    else if ( ipRuleGroup == 2)
    {
        strcpy(pclTable,"Current_Departures");
    }
    */

    strcpy(pclTable,rgGroupInfo[ipRuleGroup].pclDestTable);

    sprintf(pclSqlBuf, "TRUNCATE TABLE %s",pclTable);
    dbg(DEBUG, "%s ipRuleGroup<%d><%s>", pclFunc, ipRuleGroup, pclSqlBuf);

    ilRC = RunSQL(pclSqlBuf, pclSqlData);
    if (ilRC != DB_SUCCESS)
    {
        dbg(TRACE, "<%s>: Truncate dest data - Fails", pclFunc);
        ilRC = RC_FAIL;
    }
    return ilRC;
}

static int deleteFlightsOutOfTimeWindow(char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimit)
{
    int    ilRc   = RC_SUCCESS;         /* Return code */
    int ilRuleGroup = 0;
    char *pclFunc = "deleteFlightsOutOfTimeWindow";

    /*
    if (igTotalNumbeoOfRule > 2)
    {
        dbg(TRACE,"%s The number of rule group<%d> is invalid", pclFunc, igTotalNumbeoOfRule);
        return RC_FAIL;
    }
	*/

    for (ilRuleGroup = 1; ilRuleGroup <= igTotalNumbeoOfRule; ilRuleGroup++)
    {
        /*delete existed unrequired records in destination table*/
        deleteFlightsOutOfTimeWindowByGroup(ilRuleGroup,pcpTimeWindowLowerLimitOriginal,pcpTimeWindowLowerLimit);
    }

    return ilRc;
}

static int deleteFlightsOutOfTimeWindowByGroup(int ipRuleGroup, char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimitCurrent)
{
    int    ilRc   = RC_SUCCESS;         /* Return code */
    short slLocalCursor = 0, slFuncCode = 0;
    char *pclFunc = "deleteFlightsOutOfTimeWindowByGroup";

    char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";

    char pclTmpOriginal[64] = "\0";
    char pclTmpCurrent[64] = "\0";

    char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0", pclSelection[2048] = "\0";

#if 0
    memset(pclTmpOriginal, 0, sizeof(pclTmpOriginal));
    strncpy(pclYear,pcpTimeWindowLowerLimitOriginal,4);
    strncpy(pclMonth,pcpTimeWindowLowerLimitOriginal+4,2);
    strncpy(pclDay,pcpTimeWindowLowerLimitOriginal+6,2);
    strncpy(pclHour,pcpTimeWindowLowerLimitOriginal+8,2);
    strncpy(pclMin,pcpTimeWindowLowerLimitOriginal+10,2);
    strncpy(pclSec,pcpTimeWindowLowerLimitOriginal+12,2);
    sprintf(pclTmpOriginal,"to_date('%s-%s-%s %s:%s:%s','yyyy-mm-dd hh:mi:ss')", pclYear, pclMonth, pclDay, pclHour, pclMin, pclSec);

    memset(pclTmpCurrent, 0, sizeof(pclTmpCurrent));
    strncpy(pclYear,pcpTimeWindowLowerLimitCurrent,4);
    strncpy(pclMonth,pcpTimeWindowLowerLimitCurrent+4,2);
    strncpy(pclDay,pcpTimeWindowLowerLimitCurrent+6,2);
    strncpy(pclHour,pcpTimeWindowLowerLimitCurrent+8,2);
    strncpy(pclMin,pcpTimeWindowLowerLimitCurrent+10,2);
    strncpy(pclSec,pcpTimeWindowLowerLimitCurrent+12,2);
    sprintf(pclTmpCurrent,"to_date('%s-%s-%s %s:%s:%s','yyyy-mm-dd hh:mi:ss')", pclYear, pclMonth, pclDay, pclHour, pclMin, pclSec);



    /* if (strcmp(pcgDeletionStatusIndicator,"DELETE") == 0)  tvo_fix: change default "DELETE" to null */
	if (strlen(pcgDeletionStatusIndicator) == 0)
    {
        sprintf(pclSqlBuf, "DELETE FROM %s WHERE CDAT BETWEEN %s AND %s", rgGroupInfo[ipRuleGroup].pclDestTable, pclTmpOriginal,pclTmpCurrent);
    }
    else
    {
        sprintf(pclSqlBuf, "UPDATE %s SET %s='%s' WHERE CDAT BETWEEN %s AND %s", rgGroupInfo[ipRuleGroup].pclDestTable, pcgDeletionStatusIndicator, pcgDelValue, pclTmpOriginal,pclTmpCurrent);
    }
#endif

	/* tvo_add: build delete query function */
	buildDeleteQueryByTimeWindow(pclSqlBuf, ipRuleGroup, pcgTimeWindowDeleteFieldName, pcpTimeWindowLowerLimitOriginal, pcpTimeWindowLowerLimitCurrent);
    dbg(TRACE,"%s [%d]Delete Query<%s>",pclFunc, ipRuleGroup, pclSqlBuf);

    slLocalCursor = 0;
    slFuncCode = START;
    ilRc = sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData);
    close_my_cursor(&slLocalCursor);
    if( ilRc != DB_SUCCESS )
    {
        dbg(TRACE,"%s UPDATE-Deletion not succed",pclFunc);
        return RC_FAIL;
    }

    switch(ilRc)
    {
        case NOTFOUND:
            dbg(TRACE, "<%s> Delete source data - Not Found", pclFunc);
            break;
        default:
            dbg(TRACE, "<%s> Delete source data - Found", pclFunc);
            break;
    }
    return ilRc;
}

static void insertFligthsNewInTimeWindow(char *pcpTimeWindowUpperLimitOriginal, char *pcpTimeWindowUpperLimitCurrent)
{
    int    ilRc   = RC_SUCCESS;         /* Return code */
    char *pclFunc = "insertFligthsNewInTimeWindow";

    iniTables(pcpTimeWindowUpperLimitOriginal, pcpTimeWindowUpperLimitCurrent,FALSE);
}

static int showGroupInfo(_LINE *rlGroupInfo, int ipGroupNO)
{
    int ilCount = 0;
    int ilRc = RC_SUCCESS;
    char *pclFunc = "showGroupInfo";

    dbg(DEBUG, "%s +++++ipGroupNO<%d> ARRAYNUMBER<%d>",pclFunc, ipGroupNO, ARRAYNUMBER);

    if(ipGroupNO > ARRAYNUMBER)
    {
        return RC_FAIL;
    }
    else
    {
        for (ilCount = 0; ilCount <= ipGroupNO; ilCount++)
        {
            if(strcmp(rlGroupInfo[ilCount].pclActive, " ") != 0 && strlen(rlGroupInfo[ilCount].pclActive) > 0)
            {
                dbg(DEBUG, "%s Group[%d] Line 1-%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,ilCount,
                rlGroupInfo[ilCount].pclActive,
                rlGroupInfo[ilCount].pclRuleGroup,
                rlGroupInfo[ilCount].pclSourceTable,
                rlGroupInfo[ilCount].pclSourceKey,
                rlGroupInfo[ilCount].pclSourceField,
                rlGroupInfo[ilCount].pclSourceFieldType,
                rlGroupInfo[ilCount].pclDestTable,
                rlGroupInfo[ilCount].pclDestKey,
                rlGroupInfo[ilCount].pclDestField,
                rlGroupInfo[ilCount].pclDestFieldLen,
                rlGroupInfo[ilCount].pclDestFieldType,
                rlGroupInfo[ilCount].pclDestFieldOperator,
                rlGroupInfo[ilCount].pclCond1,
                rlGroupInfo[ilCount].pclCond2,
                rlGroupInfo[ilCount].pclCond3);
            }
        }
    }
    return ilRc;
}

static int getRefTimeFiledValueFromSource(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpTimeWindowRefFieldVal, char *pcpTimeWindowRefField)
{
    int ilRc = RC_FAIL;
    char *pclFunc = "getRefTimeFiledValueFromSource";
    char pclSqlBuf[2048] = "\0";
    char pclSqlData[2048] = "\0";

    ilRc = extractField(pcpTimeWindowRefFieldVal, pcpTimeWindowRefField, pcpFields, pcpNewData);
    if (ilRc == RC_FAIL)
    {
        /*get from source table usign urno*/
        memset(pclSqlBuf,0,sizeof(pclSqlBuf));
        sprintf(pclSqlBuf, "select %s from %s %s", pcpTimeWindowRefField,pcpTable,pcpSelection);
        dbg(TRACE,"%s: clSql = <%s>", pclFunc, pclSqlBuf);

        ilRc = RunSQL(pclSqlBuf, pclSqlData);
        if (ilRc != DB_SUCCESS)
        {
            dbg(TRACE, "<%s>: Retrieving dest data - Fails", pclFunc);
            /*return RC_FAIL;*/
        }

        switch(ilRc)
        {
            case NOTFOUND:
                dbg(TRACE, "<%s> Retrieving source data - Not Found", pclFunc);
                ilRc = NOTFOUND;
                break;
            default:
                dbg(TRACE, "<%s> Retrieving source data - Found <%s>", pclFunc, pclSqlData);
                get_real_item(pcpTimeWindowRefFieldVal,pclSqlData,1);
                ilRc = RC_SUCCESS;
                break;
        }

        if ( atoi(pcpTimeWindowRefFieldVal) != 0 )
        {
            dbg(TRACE, "<%s> %s = %s", pclFunc, pcpTimeWindowRefField, pcpTimeWindowRefFieldVal);
        }
        else
        {
            dbg(TRACE,"%s The refered time value is invalid", pclFunc);
            return RC_FAIL;
        }
    }
    else
    {
        if ( atoi(pcpTimeWindowRefFieldVal) != 0 )
        {
            dbg(TRACE, "<%s> %s = %s", pclFunc, pcpTimeWindowRefField, pcpTimeWindowRefFieldVal);
        }
        else
        {
            dbg(TRACE,"%s The refered time value is invalid", pclFunc);
            return RC_FAIL;
        }
    }
}

static void replaceDelimiter(char *pcpConvertedDataList, char *pcpOriginalDataList, char pcpOrginalDelimiter, char pcpConvertedDelimiter)
{
	int ilCount = 0;
	int ilLen = 0;
	int ilRc = RC_FAIL;
    char *pclFunc = "replaceDelimiter";

    ilLen = strlen(pcpOriginalDataList);
    for(ilCount=0; ilCount<ilLen; ilCount++)
    {
    	if ( pcpOriginalDataList[ilCount] == pcpOrginalDelimiter)
    	{
    		pcpConvertedDataList[ilCount] = pcpConvertedDelimiter;
    	}
    	else
    	{
    		pcpConvertedDataList[ilCount] = pcpOriginalDataList[ilCount];
    	}
    }
    dbg(DEBUG,"%s DataList<%s>",pclFunc, pcpConvertedDataList);
}
/*----------------------------------------------------------------------------------------------------*/

static int getRuleIndex_BrutalForce(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _RULE *rpRule, int ipRuleGroup, char *pcpFields,int ipTotalLineOfRule,char *pcpData)
{
	int ilRC = RC_SUCCESS;
	int ilRuleCount = 0;
	int ilItemNo = 0;
	char *pclFunc = "getRuleIndex_BrutalForce";

	for (ilRuleCount = 0; ilRuleCount <= ipTotalLineOfRule; ilRuleCount++)
	{
	    memset(pcpSourceFieldValue,0,sizeof(pcpSourceFieldValue));
	    /*matched the group number*/
	    if ( atoi(rpRule->rlLine[ilRuleCount].pclRuleGroup) == ipRuleGroup)
	    {
	        if (strcmp(rpRule->rlLine[ilRuleCount].pclDestField, pcpDestFieldName) == 0)
	        {
	            ilItemNo = get_item_no(pcpFields, pcpSourceFieldName, 5) + 1;
	            if (ilItemNo <= 0 )
	            {
	                dbg(TRACE, "<%s> No <%s> Not found in the field list, can't do anything", pclFunc, pcpSourceFieldName);
	                return RC_FAIL;
	            }
	            else
	            {
	                get_real_item(pcpSourceFieldValue, pcpData, ilItemNo);
	                return ilRuleCount;
	            }
	            dbg(DEBUG,"<%s> Source - The value for <%s> is <%s>", pclFunc, pcpSourceFieldName, pcpSourceFieldValue);
	        }/*end of matching the dest field*/
	    }/*end of matching the rule group*/
	    else
	    {
	        /*dbg(DEBUG,"%s The group number is not matched - ipRuleGroup<%d> and <%d>",pclFunc, ipRuleGroup, atoi(rpRule->rlLine[ilRuleCount].pclRuleGroup));*/
	        continue;
	    }
	}/*end of rule searching for loop*/

	return RC_FAIL;
}

static void buildConvertedDataList(char *pcpDetType, char *pcpDestDataList, char *pcpDestFieldValue, int ipOption)
{
    int ilRC = RC_SUCCESS;
	char *pclFunc = "buildConvertedDataList";
    char pclTmp[256] = "\0";

    if (ipOption == BLANK)    /* tvo_fix: missing '' */
    {
        if ( strlen(pcpDestDataList) == 0 )
        {
            strcat(pcpDestDataList, "' '");
        }
        else
        {
            /*strcat(pclDestDataList,",");*/
            strcat(pcpDestDataList, pcgDataListDelimiter);
            strcat(pcpDestDataList, "' '");
        }
    }
    else
    {
        if (strstr(pcpDetType,"DATE") != NULL)
        {
            if (strlen(pcpDestFieldValue) > 0)
            {
                sprintf(pclTmp,"to_date('%s','YYYY%sMM%sDD HH24%sMI%sSS')", pcpDestFieldValue, pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter);
            }
            else
            {
                strcpy(pclTmp,"''");  /* NULL value */
            }
        }
        else if (strcmp(pcpDetType,"NUMBER") == 0)
        {
            if (strlen(pcpDestFieldValue) > 0)		/* tvo_fix: if nullable then wrong */
            	sprintf(pclTmp,"%s",pcpDestFieldValue);   /* NUMBER: no single quote */
			else
				strcpy(pclTmp,"''");              /* null value */
        }
        else /* VARCHAR2 */
        {
            sprintf(pclTmp,"'%s'",pcpDestFieldValue);
        }

        if ( strlen(pcpDestDataList) == 0 )
        {
            strcpy(pcpDestDataList, pclTmp);
        }else
        {
            strcat(pcpDestDataList, pcgDataListDelimiter);
            strcat(pcpDestDataList, pclTmp);
        }
    }
}
#if 0
{
    int ilRC = RC_SUCCESS;
	char *pclFunc = "buildConvertedDataList";
    char pclTmp[256] = "\0";

    if (ipOption == BLANK)
    {
        if ( strlen(pcpDestDataList) == 0 )
        {
            strcat(pcpDestDataList, " ");
        }
        else
        {
            /*strcat(pclDestDataList,",");*/
            strcat(pcpDestDataList, pcgDataListDelimiter);
            strcat(pcpDestDataList, " ");
        }
    }
    else
    {
        /*NUMBER*/
        if(ipType == RC_NUMBER)
        {
            if ( strlen(pcpDestDataList) == 0 )
            {
                strcat(pcpDestDataList, pcpDestFieldValue);
            }
            else
            {
                /*strcat(pcpDestDataList,",");*/
                strcat(pcpDestDataList, pcgDataListDelimiter);
                strcat(pcpDestDataList, pcpDestFieldValue);
            }
        }/*CHAR*/
        else
        {
            if ( strlen(pcpDestDataList) == 0 )
            {
                /*if(strstr(pcpDestFieldValue,"-") != 0 )*/
                if(strstr(pcpDestFieldValue,pcgDateFormatDelimiter) != 0 )
                {
                    /*to_date('%s','YYYY-MM-DD HH24:MI:SS')*/

                    strcat(pcpDestDataList, "to_date(");
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                    /*strcat(pcpDestDataList, ",'YYYY-MM-DD HH24-MI-SS')");*/
                    sprintf(pclTmp,",'YYYY%sMM%sDD HH24%sMI%sSS')", pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter);
                    strcat(pcpDestDataList, pclTmp);
                    /*
                    strcat(pcpDestDataList, "date");
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                    */
                }
                else
                {
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                }
            }
            else
            {
                /*strcat(pcpDestDataList,",");*/
                strcat(pcpDestDataList, pcgDataListDelimiter);
                /*if(strstr(pcpDestFieldValue,"-") != 0 )*/
                if(strstr(pcpDestFieldValue,pcgDateFormatDelimiter) != 0 )
                {
                    strcat(pcpDestDataList, "to_date(");
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                    /*strcat(pcpDestDataList, ",'YYYY-MM-DD HH24-MI-SS')");*/
                    sprintf(pclTmp,",'YYYY%sMM%sDD HH24%sMI%sSS')", pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter,pcgDateFormatDelimiter);
                    strcat(pcpDestDataList, pclTmp);
                    /*
                    strcat(pcpDestDataList, "date");
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                    */
                }
                else
                {
                    strcat(pcpDestDataList, "'");
                    strcat(pcpDestDataList, pcpDestFieldValue);
                    strcat(pcpDestDataList, "'");
                }
            }
        }
    }
}
#endif

static int buildHash(int ipTotalLineOfRule, _RULE rpRule, ght_hash_table_t **pcpHash_table)
{
	char *pclFunc = "buildHash";
	int  *p_data = NULL;
    int  ilCount = 0;
	char clHashKey[128];

	*pcpHash_table = ght_create(ipTotalLineOfRule);

	dbg(DEBUG,"%s ipTotalLineOfRule = <%d>", pclFunc, ipTotalLineOfRule);
	for (ilCount = 0; ilCount <= ipTotalLineOfRule; ilCount++)
	{
		if ( !(p_data = (int*)malloc(sizeof(int))) )
			return RC_FAIL;

		sprintf(clHashKey,"%s%s", rpRule.rlLine[ilCount].pclRuleGroup, rpRule.rlLine[ilCount].pclDestField);

        if (strlen(clHashKey) > 0)
        {
            /* Assign the data a value */
            /* Insert "blabla" into the hash table */
            *p_data = ilCount;
            ght_insert(*pcpHash_table, p_data, sizeof(char)*strlen(clHashKey), clHashKey);
            /*dbg(DEBUG, "%s clHashKey = <%s>, ilCount = <%d>", pclFunc, clHashKey, ilCount);*/
        }
	}
	return RC_SUCCESS;
}

static int getRuleIndex_Hash(int ipRuleGroup, char *pcpDestFieldName, char *pcpFields, char *pcpData, char *pcpSourceFieldName, char *pcpSourceFieldValue)
{
    int ilItemNo = 0;
    int  *p_he = NULL;
    char *pclFunc = "getRuleIndex_Hash";
    char pclHashKey[128] = "\0";

    /*build the key: group number+dest field name*/
    sprintf(pclHashKey,"%d%s",ipRuleGroup, pcpDestFieldName);

	if ( (p_he = ght_get(pcgHash_table,sizeof(char) * strlen(pclHashKey), pclHashKey)) != NULL )
	{
	    dbg(DEBUG,"%s Found <%s> at %d",pclFunc, pclHashKey, *p_he);

	    ilItemNo = get_item_no(pcpFields, pcpSourceFieldName, 5) + 1;
        if (ilItemNo <= 0 )
        {
            dbg(TRACE, "<%s> No <%s> Not found in the field list, can't do anything", pclFunc, pcpSourceFieldName);
            return RC_FAIL;
        }
        else
        {
            get_real_item(pcpSourceFieldValue, pcpData, ilItemNo);
            return *p_he;
        }
	}
	else
	{
	    dbg(DEBUG,"%s Did not find <%s> !\n",pclFunc, pclHashKey);
	    return RC_FAIL;
	}
}

static int checkNullable(char *pcpDestFieldValue, _LINE *rpLine)
{
    int ilRC = RC_SUCCESS;
    char *pclFunc = "checkNullable";

    if (strlen(pcpDestFieldValue) == 0 )
    {
        if (strcmp(rpLine->pclDestFieldType, "NUMBER") == 0)
        {
            strcpy(pcpDestFieldValue,"0");
        }
        else if (strcmp(rpLine->pclDestFieldType, "DATE") == 0)
        {
            dbg(TRACE, "%s The line<sourceTable-(%s) sourceField-(%s) destTable-(%s) destField-(%s)> ->DATE type can not put null value",pclFunc, rpLine->pclSourceTable, rpLine->pclSourceField, rpLine->pclDestTable, rpLine->pclDestField);
            return RC_FAIL;

        }
        else
        {
            strcpy(pcpDestFieldValue," ");
        }
    }
    return RC_SUCCESS;
}

static void insertFligthts(int ipStartIndex, int ipDataListNo, _QUERY *pcpQuery)
{
	int ilRc = RC_FAIL;
	int ilCount = 0;
	short slLocalCursor = 0, slFuncCode = 0;
	char *pclFunc = "insertFligthts";
	char pclSqlData[2048] = "\0";

	for(ilCount = ipStartIndex; ilCount < ipDataListNo; ilCount++)
	{
	    dbg(DEBUG,"%s pcpQuery[%d].pclInsertQuery<%s>\n",pclFunc, ilCount, pcpQuery[ilCount].pclInsertQuery);
        /*dbg(DEBUG,"%s pcpQuery[%d].pclUpdateQuery<%s>\n",pclFunc, ilCount, pcpQuery[ilCount].pclUpdateQuery);*/

	    if (strlen(pcpQuery[ilCount].pclInsertQuery) > 0)
	    {
	        slLocalCursor = 0;
	        slFuncCode = START;
	        /*dbg(TRACE, "%d_before sql_if \n<%s>", __LINE__, pcpQuery[ilCount].pclInsertQuery);*/
	        ilRc = sql_if(slFuncCode, &slLocalCursor, pcpQuery[ilCount].pclInsertQuery, pclSqlData);
	        if( ilRc != DB_SUCCESS )
	        {
	            ilRc = RC_FAIL;
	            dbg(TRACE,"%s INSERT Error",pclFunc);
	        }
	        close_my_cursor(&slLocalCursor);
	    }
	}
}

static int updateAllFlights(_QUERY *pcpQuery, int ipDataListNo, int ipIsCodeShareChange)
{
    int ilRc = RC_FAIL;
    int ilCount = 0;
    short slLocalCursor = 0, slFuncCode = 0;
    char *pclFunc = "updateAllFlights";
    char pclSqlData[2048] = "\0";

    dbg(TRACE,"%s ipDataListNo<%d>",pclFunc,ipDataListNo);

    for(ilCount = 0; ilCount < ipDataListNo; ilCount++)
    {
        slLocalCursor = 0;
        slFuncCode = START;

        if(strlen(pcpQuery[ilCount].pclUpdateQuery) > 0)
        {
            if (ilCount == MASTER_RECORD)
            {
                dbg(DEBUG, "%s [%d] query=<%s>, MASTER_RECORD",pclFunc, ilCount, pcpQuery[ilCount].pclUpdateQuery);
                ilRc = sql_if(slFuncCode, &slLocalCursor, pcpQuery[ilCount].pclUpdateQuery, pclSqlData);
                if( ilRc != DB_SUCCESS )
                {
                    dbg(TRACE,"%s [%d] Update query fails",pclFunc, ilCount);
                    /*return ilRc;*/
                }
                else
                {
                    dbg(TRACE,"%s [%d] Update succeeds",pclFunc, ilCount);
                }
                close_my_cursor(&slLocalCursor);
            }
            else
            {
                if (ipIsCodeShareChange == CODESHARE_NONCHANGE)
                {
                    dbg(DEBUG, "%s [%d] query=<%s>, CODESHARE_RECORD",pclFunc, ilCount, pcpQuery[ilCount].pclUpdateQuery);
                    ilRc = sql_if(slFuncCode, &slLocalCursor, pcpQuery[ilCount].pclUpdateQuery, pclSqlData);
                    if( ilRc != DB_SUCCESS )
                    {
                        dbg(TRACE,"%s [%d] Update query fails",pclFunc, ilCount);
                        /*return ilRc;*/
                    }
                    else
                    {
                        dbg(TRACE,"%s [%d] Update succeeds",pclFunc, ilCount);
                    }
                    close_my_cursor(&slLocalCursor);
                }

            }
            /*return RC_SUCCESS;*/
        }
    }
    return RC_SUCCESS;
}

static void deleteCodeShareFligths(char *pcpDestTable ,char *pcpDestKey, char *pcpUrno)
{
    int ilRc = RC_FAIL;
    short slLocalCursor = 0;
    short slFuncCode = START;
    char *pclFunc = "deleteCodeShareFligths";
    char pclSqlBuf[2048] = "\0", pclSqlData[2048] = "\0";


    memset(pclSqlBuf,0,sizeof(pclSqlBuf));
    memset(pclSqlData,0,sizeof(pclSqlData));

    dbg(DEBUG, "%s %d_pclDestKey = (<%s>,<%s>), pcgCodeShareSST = <%s>", pclFunc, __LINE__, pcpDestKey, pcpUrno, pcgCodeShareSST);
    if (strlen(pcgDeletionStatusIndicator)==0)
    {
        sprintf(pclSqlBuf, "DELETE FROM %s WHERE %s=%s AND SST='%s'", pcpDestTable, pcpDestKey, pcpUrno, pcgCodeShareSST);
    }
    else
    {
        sprintf(pclSqlBuf, "UPDATE %s SET %s='%s' WHERE %s=%s AND SST='%s'", pcpDestTable, pcgDeletionStatusIndicator, pcgDelValue, pcpDestKey, pcpUrno, pcgCodeShareSST);
    }
    dbg(TRACE,"%s Delete Query<%s>",pclFunc, pclSqlBuf);

    ilRc = sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData);
    if( ilRc != DB_SUCCESS )
    {
        ilRc = RC_FAIL;
        dbg(TRACE,"%s UPDATE-Deletion Error",pclFunc);
    }
    close_my_cursor(&slLocalCursor);
}
/*------tvo_add -------------------------------------------------------------------------------*/
int getTransactionCommand(char* clCmd)
{
	int ilRc = RC_SUCCESS;

	if (strcmp(clCmd, "IFR") == 0)
	{
		igTransCmd = INSERT;
	}else if (strcmp(clCmd, "UFR") == 0)
	{
		igTransCmd = UPDATE;
	}else if (strcmp(clCmd, "DFR") == 0)
	{
		igTransCmd = DELETE;
	}else
	{
		igTransCmd = 0;  /* invalid command */
	}

	return ilRc;
}

static void showCodeFunc()
{
	int ilCount;

    for (ilCount = 0; ilCount < OPER_CODE; ilCount++)
    {
		dbg(DEBUG, "pclOperCode = <%s>", CODEFUNC[ilCount].pclOperCode);
		if (strlen(CODEFUNC[ilCount].pclOperCode) == 0)
		{
			break;
		}
    }
}

static void deleteFlight(char *pcpDestTable, char *pcpDestKey, char *pcpUrno)
{
    int ilRc = RC_FAIL;
    short slLocalCursor = 0;
    short slFuncCode = START;
    char *pclFunc = "deleteFlight";
    char pclSqlBuf[2048] = "\0", pclSqlData[2048] = "\0";

    memset(pclSqlBuf,0,sizeof(pclSqlBuf));
    memset(pclSqlData,0,sizeof(pclSqlData));

    dbg(DEBUG, "%d_pclDestKey = (<%s>,<%s>)", __LINE__, pcpDestKey, pcpUrno);
    if (strlen(pcgDeletionStatusIndicator)==0)
    {
        sprintf(pclSqlBuf, "DELETE FROM %s WHERE %s='%s'", pcpDestTable, pcpDestKey, pcpUrno);
    }
    else
    {
        sprintf(pclSqlBuf, "UPDATE %s SET %s='%s' WHERE %s='%s'", pcpDestTable, pcgDeletionStatusIndicator, pcgDelValue, pcpDestKey, pcpUrno);
    }
    dbg(TRACE,"%s Delete Query<%s>",pclFunc, pclSqlBuf);

    ilRc = sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData);
    if( ilRc != DB_SUCCESS )
    {
        ilRc = RC_FAIL;
        dbg(TRACE,"%s ilRc<%d> Deletion not found or error",pclFunc, ilRc);
    }
    else
    {
         dbg(TRACE,"%s Deletion OK",pclFunc);
    }
    close_my_cursor(&slLocalCursor);
}
/*-------------------------------------------------------------------------------------------------*/
static int buildDeleteQueryByTimeWindow(char *pcpSqlBuf, int ipRuleGroup, char* pcpDestFieldName, char *pcpTimeWindowLowerLimitOriginal, char *pcpTimeWindowLowerLimitCurrent)
{
	int    ilRc   = RC_SUCCESS;         /* Return code */
    short slLocalCursor = 0, slFuncCode = 0;
    char *pclFunc = "buildDeleteQueryByTimeWindow";


    int  *p_he = NULL;
	char pclHashKey[128] = "\0";
	int  ilRuleCount;

	char pclYear[16] = "\0";
    char pclMonth[16] = "\0";
    char pclDay[16] = "\0";
    char pclHour[16] = "\0";
    char pclMin[16] = "\0";
    char pclSec[16] = "\0";

    char pclTmpOriginal[64] = "\0";
    char pclTmpCurrent[64] = "\0";

    char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0", pclSelection[2048] = "\0";

	char clDestType[128];
	char clDestTable[256];

	dbg(DEBUG, "%s: ipRuleGroup = <%d>, pcpDestFieldName = <%s>, pcpTimeWindowLowerLimitOriginal = <%s>, pcpTimeWindowLowerLimitCurrent = <%s>",
			   pclFunc, ipRuleGroup, pcpDestFieldName, pcpTimeWindowLowerLimitOriginal, pcpTimeWindowLowerLimitCurrent);

    sprintf(pclHashKey,"%d%s",ipRuleGroup, pcpDestFieldName);
	if ( (p_he = ght_get(pcgHash_table,sizeof(char) * strlen(pclHashKey), pclHashKey)) != NULL )
	{
		ilRuleCount = *p_he;
    }else
	{
		dbg(DEBUG,"%s: Found index of <%s> fails -> return",pclFunc, pcpDestFieldName);
        return RC_FAIL;
	}
	strcpy(clDestTable, rgGroupInfo[ipRuleGroup].pclDestTable);
	strcpy(clDestType, rgRule.rlLine[ilRuleCount].pclDestFieldType);
	memset(pclTmpOriginal, 0, sizeof(pclTmpOriginal));
	memset(pclTmpCurrent, 0, sizeof(pclTmpCurrent));

	if (strlen(clDestType) == 0 || strstr(clDestType,"VARCHAR") != NULL)
	{
		strcpy(pclTmpOriginal, pcpTimeWindowLowerLimitOriginal);
		strcpy(pclTmpCurrent, pcpTimeWindowLowerLimitCurrent);
	}
	else if (strstr(clDestType,"DATE") != NULL)
	{

		strncpy(pclYear,pcpTimeWindowLowerLimitOriginal,4);
		strncpy(pclMonth,pcpTimeWindowLowerLimitOriginal+4,2);
		strncpy(pclDay,pcpTimeWindowLowerLimitOriginal+6,2);
		strncpy(pclHour,pcpTimeWindowLowerLimitOriginal+8,2);
		strncpy(pclMin,pcpTimeWindowLowerLimitOriginal+10,2);
		strncpy(pclSec,pcpTimeWindowLowerLimitOriginal+12,2);
		sprintf(pclTmpOriginal,"to_date('%s-%s-%s %s:%s:%s','yyyy-mm-dd hh24:mi:ss')", pclYear, pclMonth, pclDay, pclHour, pclMin, pclSec);

		strncpy(pclYear,pcpTimeWindowLowerLimitCurrent,4);
		strncpy(pclMonth,pcpTimeWindowLowerLimitCurrent+4,2);
		strncpy(pclDay,pcpTimeWindowLowerLimitCurrent+6,2);
		strncpy(pclHour,pcpTimeWindowLowerLimitCurrent+8,2);
		strncpy(pclMin,pcpTimeWindowLowerLimitCurrent+10,2);
		strncpy(pclSec,pcpTimeWindowLowerLimitCurrent+12,2);
		sprintf(pclTmpCurrent,"to_date('%s-%s-%s %s:%s:%s','yyyy-mm-dd hh24:mi:ss')", pclYear, pclMonth, pclDay, pclHour, pclMin, pclSec);
	}else
	{
		dbg(DEBUG,"%s: Data type of <%s> = <%s> not time format -> return fail",pclFunc, pcpDestFieldName, clDestType);
        return RC_FAIL;
	}

	if (strlen(pcgDeletionStatusIndicator) == 0)
    {
        /* sprintf(pclSqlBuf, "DELETE FROM %s WHERE %s BETWEEN %s AND %s", clDestTable, pcpDestFieldName, pclTmpOriginal,pclTmpCurrent); */
		sprintf(pclSqlBuf, "DELETE FROM %s WHERE %s <= %s", clDestTable, pcpDestFieldName, pclTmpCurrent);
    }
    else
    {
        /* sprintf(pclSqlBuf, "UPDATE %s SET %s='%s' WHERE %s BETWEEN %s AND %s", clDestTable, pcgDeletionStatusIndicator, pcgDelValue, pcpDestFieldName, pclTmpOriginal,pclTmpCurrent); */
        sprintf(pclSqlBuf, "UPDATE %s SET %s='%s' WHERE %s <= %s", clDestTable, pcgDeletionStatusIndicator, pcgDelValue, pcpDestFieldName, pclTmpCurrent);
    }
	/*------ return delete query -------------------------------------------------------*/
	strcpy(pcpSqlBuf, pclSqlBuf);

	return RC_SUCCESS;
}







