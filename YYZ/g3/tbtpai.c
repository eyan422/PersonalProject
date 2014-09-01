
#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Interface/tbtpai.c 0.2 2014/08/26 17:05:44:14SGT fya Exp  $";
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
/*#include "urno_fn.h"*/

/*fya 0.1*/
#include "tbthdl.h"
#include "tbthdl_customized.c"

/*#include "urno_fn.inc"*/
#include "tbtpai.h"

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
static char cgConfigFile[512] = "\0";
/*char  cgHopo[8] = "\0";*/                         /* default home airport    */
static char  cgTabEnd[8] ="\0";                       /* default table extension */

static long lgEvtCnt = 0;

static char pcgConfigFile[512];
static char pcgCurrentTime[32];
static char pcgDeletionStatusIndicator[512];
static char pcgDelValue[64];

/*fya 0.1*/
static char pcgTimeWindowRefField_AFTTAB_Arr[8];
static char pcgTimeWindowRefField_AFTTAB_Dep[8];
static int igTimeWindowUpperLimit;
static int igTimeWindowLowerLimit;
static int igGetDataFromSrcTable;
static int igEnableCodeshare;
static int igMsgID;
static char pcgMsgId[16];

static char pcgTimeWindowLowerLimit[64];
static char pcgTimeWindowUpperLimit[64];
static int igTimeWindowInterval[64];

static char pcgTwStart[256] = "\0";
static char pcgTwEnd[256] = "\0";
static int  igDestId;
static char pcgDestCmd [8] = "\0";
static char pcgRmkDelayed[1024] = "\0";
static char pcgRmkCancelled[1024] = "\0";
static char pcgRmkDiverted[1024] = "\0";
static char pcgRmkArr[1024] = "\0";
static char pcgRmkDep[1024] = "\0";

static char pcgMsgHeader[8] = "\0";
static char pcgSourceFieldList[2048] ="URNO,ADID,STOA,STOD,FLNO,CSGN,RTYP,JFNO,ALC2,ALC3,FLTN,FLNS,FLTI,FCLS,ETOA,ETAI,ETDI,ETOD,DES3,DES4,ORG3,ORG4,FTYP,REM1,REM2,TTYP,VIAN,VIAL,ACT3,ACT5,ONBL,OFBL,AIRB,LAND,RWYD,RWYA,TMOA,PAXT,PAX1,PAX2,PAX3,STYP,DCD2,REMP,IFRA,IFRD,PAID,SLOT,BAGN,BAGW,MAIL,MTOW,CGOT,JCNT,FCAL,BOAC,PSTA,PABS,PAES,PABA,PAEA,PSTD,PDBS,PDES,PDBA,PDEA,BLT1,BLT2,GTA1,GA1B,GA1E,GA1X,GA1Y,GTA2,GA2B,GA2E,GA2X,GA2Y,GTD1,GD1B,GD1E,GD1X,GD1Y,GTD2,GD2B,GD2E,GD2X,GD2Y,WRO1,WRO2,EXT1,EXT2,TIFA,TIFD,STEV,TGA1,TGD1,CDAT";

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

/*fya 0.1*/
static int getConfig();
static void showDepBody(_DEP_MSG_BODY *rpDepBody);
static void showArrBody(_ARR_MSG_BODY *rpArrBody);
int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData);
static int isInTimeWindow(char *pcpTimeVal, char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList,
                        _RULE *rpRule, int ipTotalLineOfRule, char * pcpSelection, char *pcpAdid, int ilIsMaster,
                        char *pcpHardcodeShare_DestFieldList, _HARDCODE_SHARE pcpHardcodeShare, _QUERY *pcpQuery);
void buildSelQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection);
static int handleNewUpdFlight(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpAdidValue, char *pcpCommand);
static int buildDataArray(char *pcpFields, char *pcpNewData, char (*pcpDatalist)[LISTLEN], char *pcpSelection);
static int refreshTable(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int iniTables(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit,int ipOptionToTruncate);
static int getFieldValue(char *pcpFields, char *pcpNewData, char *pcpFieldName, char *pcpFieldValue);
static int getRefTimeFiledValueFromSource(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpTimeWindowRefFieldVal, char *pcpTimeWindowRefField);
static void deleteCodeShareFligths(char *pcpDestTable ,char *pcpDestKey, char *pcpUrno);
static void deleteFlight(char *pcpDestTable, char *pcpDestKey, char *pcpUrno);
static void showDataArray(int ipNo, char (*pcpDatalist)[LISTLEN]);
static void buildMsgHeader(_HEAD *rpHead, char *pcpCommand);
static void buildAndSendMessage(int ipNo, char *pcpFieldList, char (*pcpDatalist)[LISTLEN], char *pcpAdidValue, char *pcpCommand, char *pcpSelection, char *pcpTable);
static void storeMasterData(_MASTER *rpMaster, char *pcpFieldList, char *pcpDatalist);
static void buildArrMsgBody(_ARR_MSG_BODY *rpArrBody, char *pcpFieldList, char *pcpDatalist, int ipCount, int ipNo, _MASTER *rpMaster);
static void buildDepMsgBody(_DEP_MSG_BODY *rpArrBody, char *pcpFieldList, char *pcpDatalist, int ipCount, int ipNo, _MASTER *rpMaster);
static void updateTimeWindow(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int getSourceFieldData(char *pclTable, char * pcpSourceFieldList, char * pcpSelection, char * pclSourceDataList);
static int getVial(char *pcpVial, char (*pcpVialArray)[LISTLEN]);
static void sendArrMsg(_HEAD *rpHead, _MASTER *rpMaster, _ARR_MSG_BODY *rpArrBody, char *pcpTable);
static void sendDepMsg(_HEAD *rpHead ,_MASTER *rpMaster, _DEP_MSG_BODY *rpDepBody, char *pcpTable);
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

	if (ilRc != RC_SUCCESS)
	{
	    dbg(TRACE,"Init_Process failed");
	}

	dbg(TRACE,"%s Update the time window according to the received TMW command from ntisch",pclFunc);
    updateTimeWindow(pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit);

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

	ilRc = iGetConfigEntry(pcgConfigFile,"EXCHDL","TWS",CFG_STRING,pcgTwStart);
    if (ilRc != RC_SUCCESS)
        strcpy(pcgTwStart,prlCmdblk->tw_start);
    strcpy(pcgTwEnd,prlCmdblk->tw_end);

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

	if ( strstr("DFR,IFR,UFR",prlCmdblk->command) != 0)
    {
        /*if(!strcmp(clTable,"AFTTAB"))*/
        if( !strcmp(clTable,"AFTTAB"))
        {
            /*getting the ADID*/
            getFieldValue(pclFields, pclNewData, "ADID", pclAdidValue);
        }
        else
        {
            dbg(TRACE,"%s The update is not from AFTTAB, return",pclFunc);
            return RC_FAIL;
        }

        if(strlen(pclUrnoSelection)==0)
        {
            getFieldValue(pclFields, pclNewData, "URNO", pclUrnoSelection);
        }

        handleNewUpdFlight(clTable, pclFields, pclNewData, pclUrnoSelection, pclAdidValue, prlCmdblk->command);
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
        if ( strcmp(pclDebugLevel,"TRACE") == 0 )
        {
            debug_level = TRACE;
        }
        else
        {
            if ( strcmp(pclDebugLevel,"NULL") == 0 )
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

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","Message_Header",CFG_STRING,pcgMsgHeader);
    if (ilRC == RC_SUCCESS)
    {
        dbg(DEBUG,"pcgMsgHeader<%s>",pcgMsgHeader);
    }
    else
    {
        strcpy(pcgMsgHeader,"IED");
        dbg(DEBUG,"pcgMsgHeader<%s>",pcgMsgHeader);
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

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DEST_ID",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
    {
        if( atoi(pclTmpBuf) > 0 )
        {
            igDestId = atoi(pclTmpBuf);
        }
    }
    else
    {
        igDestId = 8891;
    }
    dbg(DEBUG,"%s igDestId<%d>", pclFunc, igDestId);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DEST_CMD",CFG_STRING,pcgDestCmd);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgDestCmd,"XMLO");
    }
    dbg(DEBUG,"%s pcgDestCmd<%s>", pclFunc, pcgDestCmd);

    ilRC = iGetConfigEntry(pcgConfigFile,"REMARK","DELAYED",CFG_STRING,pcgRmkDelayed);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgRmkDelayed,"DLG,DLT,DLU,DEC,DEL,DEC");
    }
    dbg(DEBUG,"%s pcgRmkDelayed<%s>", pclFunc, pcgRmkDelayed);

    ilRC = iGetConfigEntry(pcgConfigFile,"REMARK","CANCELLED",CFG_STRING,pcgRmkCancelled);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgRmkCancelled,"CNL,CXL,CXX");
    }
    dbg(DEBUG,"%s pcgRmkCancelled<%s>", pclFunc, pcgRmkCancelled);

    ilRC = iGetConfigEntry(pcgConfigFile,"REMARK","DIVERTED",CFG_STRING,pcgRmkDiverted);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgRmkDiverted,"DIV");
    }
    dbg(DEBUG,"%s pcgRmkDiverted<%s>", pclFunc, pcgRmkDiverted);

    ilRC = iGetConfigEntry(pcgConfigFile,"REMARK","ARRIVAL",CFG_STRING,pcgRmkArr);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgRmkArr,"ARR");
    }
    dbg(DEBUG,"%s pcgRmkArr<%s>", pclFunc, pcgRmkArr);

    ilRC = iGetConfigEntry(pcgConfigFile,"REMARK","DEPARTURE",CFG_STRING,pcgRmkDep);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgRmkDep,"DEP");
    }
    dbg(DEBUG,"%s pcgRmkDep<%s>", pclFunc, pcgRmkDep);

    return RC_SUCCESS;
} /* End of getConfig() */

static void showDepBody(_DEP_MSG_BODY *rpDepBody)
{
    char pclFunc[] = "showDepBody:";

   dbg(DEBUG, "%s PLC:<%s>,PLN:<%s>,PLX:<%s>,SDT:<%s>,STD:<%s>,SST:<%s>,MPC:<%s>,MPN:<%s>,MPX:<%s>,STOP1:<%s>,STOP2:<%s>,STOP3:<%s>,STOP4:<%s>,TYP:<%s>,TYS:<%s>,LDD:<%s>,LTD:<%s>,NA:<%s>,GAT:<%s>,RmkTermId:<%s>,RmkFltStatus:<%s>,TimeSync:<%s>,PreGate:<%s>,OverbookedCompensation:<%s>,FreFlyerMiles:<%s>,FltDelayRsnCode:<%s>,TimeDuration<%s>,MealService<%s>,DaysOfOper:<%s>,EffDate:<%s>,DisDate:<%s>", pclFunc,
    rpDepBody->pclPLC,
    rpDepBody->pclPLN,
    rpDepBody->pclPLX,
    rpDepBody->pclSDT,
    rpDepBody->pclSTD,
    rpDepBody->pclSST,
    rpDepBody->pclMPC,
    rpDepBody->pclMPN,
    rpDepBody->pclMPX,
    rpDepBody->pclStop1,
    rpDepBody->pclStop2,
    rpDepBody->pclStop3,
    rpDepBody->pclStop4,
    rpDepBody->pclTYP,
    rpDepBody->pclTYS,
    rpDepBody->pclLDD,
    rpDepBody->pclLTD,
    rpDepBody->pclNA,
    rpDepBody->pclGAT,
    rpDepBody->pclRmkTermId,
    rpDepBody->pclRmkFltStatus,
    rpDepBody->pclTimeSync,
    rpDepBody->pclPreGate,
    rpDepBody->pclOverbookedCompensation,
    rpDepBody->pclFreFlyerMiles,
    rpDepBody->pclFltDelayRsnCode,
    rpDepBody->pclTimeDuration,
    rpDepBody->pclMealService,
    rpDepBody->pclDaysOfOper,
    rpDepBody->pclEffDate,
    rpDepBody->pclDisDate
    );
}

static void showArrBody(_ARR_MSG_BODY *rpArrBody)
{
    char pclFunc[] = "showArrBody:";

    dbg(DEBUG, "%s PLC:<%s>,PLN:<%s>,PLX:<%s>,SDT:<%s>,STA:<%s>,SST:<%s>,MPC:<%s>,MPN:<%s>,MPX:<%s>,STOP1:<%s>,STOP2:<%s>,STOP3:<%s>,STOP4:<%s>,TYP:<%s>,TYS:<%s>,LDA:<%s>,LTA:<%s>,NA:<%s>,CA1:<%s>,GAT:<%s>,RmkTermId:<%s>,RmkFltStatus:<%s>,TimeSync:<%s>,PreGate:<%s>,OverbookedCompensation:<%s>,FreFlyerMiles:<%s>,FltDelayRsnCode:<%s>,DaysOfOper:<%s>,EffDate:<%s>,DisDate:<%s>", pclFunc,
    rpArrBody->pclPLC,
    rpArrBody->pclPLN,
    rpArrBody->pclPLX,
    rpArrBody->pclSDT,
    rpArrBody->pclSTA,
    rpArrBody->pclSST,
    rpArrBody->pclMPC,
    rpArrBody->pclMPN,
    rpArrBody->pclMPX,
    rpArrBody->pclStop1,
    rpArrBody->pclStop2,
    rpArrBody->pclStop3,
    rpArrBody->pclStop4,
    rpArrBody->pclTYP,
    rpArrBody->pclTYS,
    rpArrBody->pclLDA,
    rpArrBody->pclLTA,
    rpArrBody->pclNA,
    rpArrBody->pclCA1,
    rpArrBody->pclGAT,
    rpArrBody->pclRmkTermId,
    rpArrBody->pclRmkFltStatus,
    rpArrBody->pclTimeSync,
    rpArrBody->pclPreGate,
    rpArrBody->pclOverbookedCompensation,
    rpArrBody->pclFreFlyerMiles,
    rpArrBody->pclFltDelayRsnCode,
    rpArrBody->pclDaysOfOper,
    rpArrBody->pclEffDate,
    rpArrBody->pclDisDate
    );
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

void buildSelQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection)
{
    char *pclFunc = "buildSelQuery";

    sprintf(pcpSqlBuf,"SELECT %s FROM %s WHERE URNO=%s", pcpSourceFieldList, pcpTable, pcpSelection);
    dbg(TRACE,"%s pcpSqlBuf<%s>",pclFunc, pcpSqlBuf);
}

static int handleNewUpdFlight(char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpSelection, char *pcpAdidValue, char *pcpCommand)
{
    int ilRc = 0;
    int ilStatusMaster = 0;
    int ilDataListNo = 0;
    int ilCount = 0;
    int ilRuleGroup = 0;
    char *pclFunc = "handleNewUpdFlight";
    char *pclTmp = NULL;
    short slLocalCursor = 0, slFuncCode = 0;
    char pclTimeWindowRefFieldVal[32] = "\0";
    char pclUrnoSelection[64] = "\0";
    char pclTmpSelection[64] = "\0";
    char pclHardcodeShare_DestFieldList[256] = "\0";
    char pclSqlBuf[4016] = "\0";       /* tvo_fix: 2048 -> 4016 */
    char pclSqlData[2048] = "\0";
    //char pclSourceFieldList[2048] = "\0";
    char pclSourceDataList[4096] = "\0";
    char pclDatalist[ARRAYNUMBER][LISTLEN] = {"\0"};
    _QUERY pclQuery[ARRAYNUMBER] = {"\0"};

    _HEAD rlHead;

    if ( strcmp(pcpAdidValue,"A") == 0 && strcmp(pcpTable,"AFTTAB") == 0 )
    {
        getRefTimeFiledValueFromSource(pcpTable, pcpFields, pcpNewData, pcpSelection, pclTimeWindowRefFieldVal, pcgTimeWindowRefField_AFTTAB_Arr);
    }
    else if ( strcmp(pcpAdidValue,"D") == 0 && strcmp(pcpTable,"AFTTAB") == 0 )
    {
        getRefTimeFiledValueFromSource(pcpTable, pcpFields, pcpNewData, pcpSelection, pclTimeWindowRefFieldVal, pcgTimeWindowRefField_AFTTAB_Dep);
    }

    if( isInTimeWindow(pclTimeWindowRefFieldVal, pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit) == RC_FAIL )
    {
        return RC_FAIL;
    }

    if ( igGetDataFromSrcTable == TRUE)
    {
        ilRc = getSourceFieldData(pcpTable, pcgSourceFieldList, pcpSelection, pclSourceDataList);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The source field data is not found - using the original field and data list", pclFunc);
            ilDataListNo = buildDataArray(pcpFields, pcpNewData, pclDatalist, pcpSelection);
            buildAndSendMessage(ilDataListNo, pcpFields, pclDatalist, pcpAdidValue, pcpCommand, pcpSelection, pcpTable);
        }
        else/*this way*/
        {
            dbg(TRACE,"%s The source field list is found", pclFunc);
            ilDataListNo = buildDataArray(pcgSourceFieldList, pclSourceDataList, pclDatalist, pcpSelection);
            buildAndSendMessage(ilDataListNo, pcgSourceFieldList, pclDatalist, pcpAdidValue, pcpCommand, pcpSelection, pcpTable);
        }
    }
    else
    {
        /*using the original field and data list*/
        dbg(TRACE,"%s The getting source data config option is not set - using the original field and data list", pclFunc);
        ilDataListNo = buildDataArray(pcpFields, pcpNewData, pclDatalist, pcpSelection);
        buildAndSendMessage(ilDataListNo, pcpFields, pclDatalist, pcpAdidValue, pcpCommand, pcpSelection, pcpTable);
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
    return ilRC;
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

/*----------------------------------------------------------------------------------------------------*/
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

static void showDataArray(int ipNo, char (*pcpDatalist)[LISTLEN])
{
    int ilCount = 0;
    char *pclFunc = "showDataArray";

    if (ipNo < 0)
    {
        if(strlen(pcpDatalist[0])>0)
                dbg(DEBUG,"%s ++++++++++pcpDatalist[0] <%s>++++++++++++++++\n\n",pclFunc, pcpDatalist[0]);
    }
    else
    {
        for(ilCount = 0; ilCount < ipNo; ilCount++)
        {
            if(strlen(pcpDatalist[ilCount])>0)
                dbg(DEBUG,"%s ++++++++++pcpDatalist[%d] <%s>++++++++++++++++\n\n",pclFunc, ilCount, pcpDatalist[ilCount]);
        }
    }
}

static void buildAndSendMessage(int ipNo, char *pcpFieldList, char (*pcpDatalist)[LISTLEN], char *pcpAdidValue, char *pcpCommand, char *pcpSelection, char *pcpTable)
{
    int ilNo = 0;
    int ilCount = 0;
    char *pclFunc = "buildAndSendMessage";
    _HEAD rlHead;
    _ARR_MSG_BODY rlArrBody;
    _DEP_MSG_BODY rlDepBody;
    _MASTER       rlMaster;

    if (ipNo < 0)
    {
        ilNo = 0;
    }
    else
    {
        ilNo = ipNo;
    }

    storeMasterData(&rlMaster, pcpFieldList, pcpDatalist[MASTER_RECORD]);

    /*master and codeshare flights*/
    for(ilCount = 0; ilCount <= ilNo; ilCount++)
    {
        memset(&rlArrBody,0,sizeof(rlArrBody));
        memset(&rlDepBody,0,sizeof(rlDepBody));

        if( strlen(pcpDatalist[ilCount]) > 0 )
        {
            buildMsgHeader(&rlHead, pcpCommand);
            dbg(DEBUG,"%s ++++++++++pcpDatalist[%d] <%s>++++++++++++++++\n\n",pclFunc, ilCount, pcpDatalist[ilCount]);

            if(strcmp(pcpAdidValue,"A") == 0)
            {
                buildArrMsgBody(&rlArrBody, pcpFieldList, pcpDatalist[ilCount], ilCount, ipNo, &rlMaster);
                showArrBody(&rlArrBody);
                sendArrMsg(&rlHead, &rlMaster,&rlArrBody,pcpTable);
            }
            else
            {
                buildDepMsgBody(&rlDepBody, pcpFieldList, pcpDatalist[ilCount], ilCount, ipNo, &rlMaster);
                showDepBody(&rlDepBody);
                sendDepMsg(&rlHead, &rlMaster,&rlDepBody,pcpTable);
            }
        }
    }
}

static void buildMsgHeader(_HEAD *rpHead, char *pcpCommand)
{
    int ilSelection = 0;
    char *pclFunc = "buildMsgHeader";
    strcpy(rpHead->pclMsgHeader, pcgMsgHeader);

    sprintf(rpHead->pclObjId,"%02x",igMsgID);
    sprintf(rpHead->pclMsgId,"%02x",igMsgID);
    sprintf(rpHead->pclMsgNo,"%02x",igMsgID);

    strncpy(rpHead->pclMsgType, pcpCommand, 1);
    sprintf(pcgMsgId,"%02x",igMsgID);
}

static void fillField(char *pcpDest, char *pcpOrigin, int ipLen)
{
    int ilCount = 0;

    if (strlen(pcpOrigin) == 0)
    {
        for(ilCount = 0; ilCount < ipLen; ilCount++)
        {
            strcat(pcpDest," ");
        }
    }
    else if ( strlen(pcpOrigin) < ipLen )
    {
        for(ilCount = 0; ilCount < (ipLen - strlen(pcpOrigin)); ilCount++)
        {
            strcat(pcpDest," ");
        }
        strcat(pcpDest,pcpOrigin);
    }
    else
    {
        strncpy(pcpDest,pcpOrigin,ipLen);
    }
}

static void buildArrMsgBody(_ARR_MSG_BODY *rpArrBody, char *pcpFieldList, char *pcpDatalist, int ipCount, int ipNo, _MASTER *rpMaster)
{
    int ilNo = 0;
    int ilCount = 0;
    int ilRc = RC_SUCCESS;
    char *pclFunc = "buildArrMsgBody";
    char pclTmp[1024] = "\0";
    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    /*1-PLC-ALC2*/
    ilRc = extractField(pclTmp, "ALC3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The ALC3 value<%s> is invalid", pclFunc, pclTmp);
        fillField(rpArrBody->pclPLC, "", 3);
    }
    else
    {
        dbg(DEBUG,"%s The ALC3 value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclPLC, pclTmp, 3);
    }

    /*2-PLN-FLTN*/
    ilRc = extractField(pclTmp, "FLTN", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The FLTN value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclPLN,0,sizeof(rpArrBody->pclPLN));
        fillField(rpArrBody->pclPLN, "", 4);
    }
    else
    {
        dbg(DEBUG,"%s The FLTN value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclPLN, pclTmp, 4);
    }

    /*3-PLX-FLNS*/
    ilRc = extractField(pclTmp, "FLNS", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The FLNS value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclPLX,0,sizeof(rpArrBody->pclPLX));
        fillField(rpArrBody->pclPLX, "", 1);
    }
    else
    {
        dbg(DEBUG,"%s The FLNS value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclPLX, pclTmp, 1);
    }

    /*4-SDT-STOA*/
    /*5-STA-STOA*/
    ilRc = extractField(pclTmp, "STOA", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The SDT-STA value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclSDT,0,sizeof(rpArrBody->pclSDT));
        //memset(rpArrBody->pclSTA,0,sizeof(rpArrBody->pclSTA));
        fillField(rpArrBody->pclSDT, "", 8);
        fillField(rpArrBody->pclSTA, "", 4);
    }
    else
    {
        dbg(DEBUG,"%s The SDT-STA value<%s> is valid", pclFunc, pclTmp);
        strncpy(rpArrBody->pclSDT,pclTmp,8);
        strncpy(rpArrBody->pclSTA,pclTmp+8,4);
        dbg(DEBUG,"%s SDT<%s> STA<%s>", pclFunc, rpArrBody->pclSDT, rpArrBody->pclSTA);
    }

    /*6-SST-Code share flight indicator*/
    if(ipCount == 0)
    {
        /*master*/
        if (ipNo - 1 <= 0)
            strcpy(rpArrBody->pclSST,"00");
        else
            sprintf(rpArrBody->pclSST,"0%d",ipNo);
    }
    else
    {
        /*codeshare*/
        strcpy(rpArrBody->pclSST,"SL");
    }

    /*7-MPC-Master Carrier*/
    /*8-MPN-Master Flight Number*/
    /*9-MPX-Master Flight Suffix*/
    /*
    if(ipCount == 0)
    {
        memset(rpArrBody->pclMPC,0,sizeof(rpArrBody->pclMPC));
        memset(rpArrBody->pclMPN,0,sizeof(rpArrBody->pclMPN));
        memset(rpArrBody->pclMPX,0,sizeof(rpArrBody->pclMPX));
    }
    else
    */
    {
        /*
        strcpy(rpArrBody->pclMPC,rpMaster->pclMPC);
        strcpy(rpArrBody->pclMPN,rpMaster->pclMPN);
        strcpy(rpArrBody->pclMPX,rpMaster->pclMPX);
        */

        fillField(rpArrBody->pclMPC, rpMaster->pclMPC, 3);
        fillField(rpArrBody->pclMPN, rpMaster->pclMPN, 4);
        fillField(rpArrBody->pclMPX, rpMaster->pclMPX, 1);
    }

    /*10,11,12,13-Stop1,Stop2,Stop3,Stop4*/
    /*
    memset(rpArrBody->pclStop1,0,sizeof(rpArrBody->pclStop1));
    memset(rpArrBody->pclStop2,0,sizeof(rpArrBody->pclStop2));
    memset(rpArrBody->pclStop3,0,sizeof(rpArrBody->pclStop3));
    memset(rpArrBody->pclStop4,0,sizeof(rpArrBody->pclStop4));
    */
    fillField(rpArrBody->pclStop1, "", 3);
    fillField(rpArrBody->pclStop2, "", 3);
    fillField(rpArrBody->pclStop3, "", 3);
    fillField(rpArrBody->pclStop4, "", 3);
    ilRc = extractField(pclTmp, "VIAL", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The VIAL value<%s> is invalid", pclFunc, pclTmp);
    }
    else
    {
        dbg(DEBUG,"%s The VIAL value<%s> is valid", pclFunc, pclTmp);
        ilNo = getVial(pclTmp, pclVial);

        ilRc = extractField(pclTmp, "ORG3", pcpFieldList, pcpDatalist);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The ORG3 value<%s> is invalid", pclFunc, pclTmp);
        }
        else
        {
            dbg(DEBUG,"%s The ORG3 value<%s> is valid", pclFunc, pclTmp);
        }

        switch(ilNo)
        {
            case 0:
                strncpy(rpArrBody->pclStop1, pclTmp,3);
                break;
            case 1:
                strncpy(rpArrBody->pclStop1, pclVial[0],3);
                strncpy(rpArrBody->pclStop2, pclTmp,3);
                break;
            case 2:
                strncpy(rpArrBody->pclStop1, pclVial[1],3);
                strncpy(rpArrBody->pclStop2, pclVial[0],3);
                strncpy(rpArrBody->pclStop3, pclTmp,3);
                break;
            case 3:
            case 4:
            case 5:
            case 6:
                strncpy(rpArrBody->pclStop1, pclVial[2],3);
                strncpy(rpArrBody->pclStop2, pclVial[1],3);
                strncpy(rpArrBody->pclStop3, pclVial[0],3);
                strncpy(rpArrBody->pclStop4, pclTmp,3);
                break;
            default:
                dbg(DEBUG,"%s ilNo<%d>is more than six or less then zero",pclFunc, ilNo);
                break;
        }
    }

    /*14-TYP-ACT3*/
    ilRc = extractField(pclTmp, "ACT3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The TYP value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclTYP,0,sizeof(rpArrBody->pclTYP));
        fillField(rpArrBody->pclTYP, "", 3);
    }
    else
    {
        dbg(DEBUG,"%s The TYP value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclTYP, pclTmp, 3);
    }

    /*15-TYS-ACT3*/
    ilRc = extractField(pclTmp, "ACT3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The TYS value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclTYS,0,sizeof(rpArrBody->pclTYS));
        fillField(rpArrBody->pclTYS, "", 8);
    }
    else
    {
        dbg(DEBUG,"%s The TYS value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclTYS, pclTmp, 8);
    }

    /*16-LDA-TIFA*/
    /*17-LTA-TIFA*/
    ilRc = extractField(pclTmp, "TIFA", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The LDA-LTA value<%s> is invalid", pclFunc, pclTmp);
        fillField(rpArrBody->pclLDA,"",8);
        fillField(rpArrBody->pclLTA,"",4);
    }
    else
    {
        dbg(DEBUG,"%s The LDA-LTA value<%s> is valid", pclFunc, pclTmp);
        strncpy(rpArrBody->pclLDA, pclTmp, 8);
        strncpy(rpArrBody->pclLTA, pclTmp+8, 4);
    }

    /*18-Na*/
    strncpy(rpArrBody->pclNA," ",1);

    /*19-CA1*/
    ilRc = extractField(pclTmp, "BLT1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The CA1 value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclCA1,0,sizeof(rpArrBody->pclCA1));
        fillField(rpArrBody->pclCA1,"",3);
    }
    else
    {
        dbg(DEBUG,"%s The CA1 value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclCA1,pclTmp,3);
    }

    /*20-GAT*/
    ilRc = extractField(pclTmp, "GTA1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The GAT value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclGAT,0,sizeof(rpArrBody->pclGAT));
        fillField(rpArrBody->pclGAT,"",3);
    }
    else
    {
        dbg(DEBUG,"%s The GAT value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclGAT,pclTmp,3);
    }

    /*21-Remarks_Terminal_Id-Remarks_Flight_Status_Portion*/
    ilRc = extractField(pclTmp, "TRMA", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The Terminal value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclRmkTermId,0,sizeof(rpArrBody->pclRmkTermId));
        fillField(rpArrBody->pclRmkTermId,"",3);
    }
    else
    {
        dbg(DEBUG,"%s The Terminal value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclRmkTermId,pclTmp,3);
    }

    //memset(rpArrBody->pclRmkFltStatus,0,sizeof(rpArrBody->pclRmkFltStatus));
    ilRc = extractField(pclTmp, "REMP", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The REMP value<%s> is invalid", pclFunc, pclTmp);
        //strcpy(rpArrBody->pclRmkFltStatus,"  ");
        fillField(rpArrBody->pclRmkFltStatus,"",2);
    }
    else
    {
        dbg(DEBUG,"%s The Terminal value<%s> is valid", pclFunc, pclTmp);

        if (strlen(pclTmp) == 0)
        {
            /*No remark->On time*/
            strcpy(rpArrBody->pclRmkFltStatus,"  1");
        }
        else
        {
            if ( strstr(pcgRmkDelayed,pclTmp) != 0 )
            {
                strcpy(rpArrBody->pclRmkFltStatus,"  3");
            }
            else if ( strstr(pcgRmkCancelled,pclTmp) != 0 )
            {
                strcpy(rpArrBody->pclRmkFltStatus,"  5");
            }
            else if ( strstr(pcgRmkDiverted,pclTmp) != 0 )
            {
                strcpy(rpArrBody->pclRmkFltStatus,"  9");
            }
            else if ( strstr(pcgRmkArr,pclTmp) != 0 )
            {
                strcpy(rpArrBody->pclRmkFltStatus," 10");
            }
            else if ( strstr(pcgRmkDep,pclTmp) != 0 )
            {
                strcpy(rpArrBody->pclRmkFltStatus," 32");
            }
            else
            {
                strcpy(rpArrBody->pclRmkFltStatus,"   ");
            }
        }
    }

    /*22-Time Sync*/
    //strcpy(rpArrBody->pclTimeSync,"    ");
    fillField(rpArrBody->pclTimeSync,"",4);

    /*23-Previous Gate*/
    //strcpy(rpArrBody->pclPreGate,"    ");
    fillField(rpArrBody->pclPreGate,"",4);

    /*24-Overbooked Compensation*/
    //strcpy(rpArrBody->pclOverbookedCompensation,"   ");
    fillField(rpArrBody->pclOverbookedCompensation,"",3);

    /*25-Frequent Flyer Miles*/
    ilRc = extractField(pclTmp, "EXT1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The frequent flyer miles value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclFreFlyerMiles,0,sizeof(rpArrBody->pclFreFlyerMiles));
        fillField(rpArrBody->pclFreFlyerMiles,"",5);
    }
    else
    {
        dbg(DEBUG,"%s The frequent flyer miles value<%s> is valid", pclFunc, pclTmp);
        fillField(rpArrBody->pclFreFlyerMiles,pclTmp,5);
    }

    /*26-Flight delay reason code*/
    //strcpy(rpArrBody->pclFltDelayRsnCode,"     ");
    fillField(rpArrBody->pclFltDelayRsnCode,"",5);

    /*27-Days of operation*/
    strcpy(rpArrBody->pclDaysOfOper,"1234567");

    /*28-Effective Date*/
    //strcpy(rpArrBody->pclEffDate,"        ");
    fillField(rpArrBody->pclEffDate,"",5);

    /*29-Discontinue Date*/
    //strcpy(rpArrBody->pclDisDate,"        ");
    fillField(rpArrBody->pclDisDate,"",5);
}

static void buildDepMsgBody(_DEP_MSG_BODY *rpDepBody, char *pcpFieldList, char *pcpDatalist, int ipCount, int ipNo, _MASTER *rpMaster)
{
    int ilNo = 0;
    int ilCount = 0;
    int ilRc = RC_SUCCESS;
    char *pclFunc = "buildDepMsgBody";
    char pclTmp[1024] = "\0";
    char pclVial[ARRAYNUMBER][LISTLEN] = {"\0"};

    /*1-PLC-ALC2*/
    ilRc = extractField(pclTmp, "ALC3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The ALC3 value<%s> is invalid", pclFunc, pclTmp);
        fillField(rpDepBody->pclPLC, "", 3);
    }
    else
    {
        dbg(DEBUG,"%s The ALC3 value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclPLC, pclTmp, 3);
    }

    /*2-PLN-FLTN*/
    ilRc = extractField(pclTmp, "FLTN", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The FLTN value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclPLN,0,sizeof(rpArrBody->pclPLN));
        fillField(rpDepBody->pclPLN, "", 4);
    }
    else
    {
        dbg(DEBUG,"%s The FLTN value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclPLN, pclTmp, 4);
    }

    /*3-PLX-FLNS*/
    ilRc = extractField(pclTmp, "FLNS", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The FLNS value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclPLX,0,sizeof(rpArrBody->pclPLX));
        fillField(rpDepBody->pclPLX, "", 1);
    }
    else
    {
        dbg(DEBUG,"%s The FLNS value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclPLX, pclTmp, 1);
    }

    /*4-SDT-STOD*/
    /*5-STD-STOD*/
    ilRc = extractField(pclTmp, "STOD", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The SDT-STD value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpDepBody->pclSDT,0,sizeof(rpDepBody->pclSDT));
        //memset(rpDepBody->pclSTD,0,sizeof(rpDepBody->pclSTD));
        fillField(rpDepBody->pclSDT, "", 8);
        fillField(rpDepBody->pclSTD, "", 4);
    }
    else
    {
        dbg(DEBUG,"%s The SDT-STD value<%s> is valid", pclFunc, pclTmp);
        strncpy(rpDepBody->pclSDT, pclTmp, 8);
        strncpy(rpDepBody->pclSTD, pclTmp+8, 4);
        dbg(DEBUG,"%s SDT<%s> STA<%s>", pclFunc, rpDepBody->pclSDT, rpDepBody->pclSTD);
    }

    /*6-SST-Code share flight indicator*/
    if(ipCount == 0)
    {
        /*master*/
        if (ipNo - 1 <= 0)
            strcpy(rpDepBody->pclSST,"00");
        else
            sprintf(rpDepBody->pclSST,"0%d",ipNo);
    }
    else
    {
        /*codeshare*/
        strcpy(rpDepBody->pclSST,"SL");
    }

    /*7-MPC-Master Carrier*/
    /*8-MPN-Master Flight Number*/
    /*9-MPX-Master Flight Suffix*/
    /*
    if(ipCount == 0)
    {
        memset(rpDepBody->pclMPC,0,sizeof(rpDepBody->pclMPC));
        memset(rpDepBody->pclMPN,0,sizeof(rpDepBody->pclMPN));
        memset(rpDepBody->pclMPX,0,sizeof(rpDepBody->pclMPX));
    }
    else
    */
    {
        /*
        strcpy(rpDepBody->pclMPC,rpMaster->pclMPC);
        strcpy(rpDepBody->pclMPN,rpMaster->pclMPN);
        strcpy(rpDepBody->pclMPX,rpMaster->pclMPX);
        */
        fillField(rpDepBody->pclMPC, rpMaster->pclMPC, 3);
        fillField(rpDepBody->pclMPN, rpMaster->pclMPN, 4);
        fillField(rpDepBody->pclMPX, rpMaster->pclMPX, 1);
    }

    /*10,11,12,13-Stop1,Stop2,Stop3,Stop4*/
    /*
    memset(rpDepBody->pclStop1,0,sizeof(rpDepBody->pclStop1));
    memset(rpDepBody->pclStop2,0,sizeof(rpDepBody->pclStop2));
    memset(rpDepBody->pclStop3,0,sizeof(rpDepBody->pclStop3));
    memset(rpDepBody->pclStop4,0,sizeof(rpDepBody->pclStop4));
    */
    fillField(rpDepBody->pclStop1, "", 3);
    fillField(rpDepBody->pclStop2, "", 3);
    fillField(rpDepBody->pclStop3, "", 3);
    fillField(rpDepBody->pclStop4, "", 3);
    ilRc = extractField(pclTmp, "VIAL", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The VIAL value<%s> is invalid", pclFunc, pclTmp);
    }
    else
    {
        dbg(DEBUG,"%s The VIAL value<%s> is valid", pclFunc, pclTmp);
        ilNo = getVial(pclTmp, pclVial);

        ilRc = extractField(pclTmp, "DES3", pcpFieldList, pcpDatalist);
        if (ilRc == RC_FAIL)
        {
            dbg(TRACE,"%s The DES3 value<%s> is invalid", pclFunc, pclTmp);
        }
        else
        {
            dbg(DEBUG,"%s The DES3 value<%s> is valid", pclFunc, pclTmp);
        }

        switch(ilNo)
        {
            case 0:
                strncpy(rpDepBody->pclStop1, pclTmp,3);
                break;
            case 1:
                strncpy(rpDepBody->pclStop1, pclVial[0],3);
                strncpy(rpDepBody->pclStop2, pclTmp,3);
                break;
            case 2:
                strncpy(rpDepBody->pclStop1, pclVial[0],3);
                strncpy(rpDepBody->pclStop2, pclVial[1],3);
                strncpy(rpDepBody->pclStop3, pclTmp,3);
                break;
            case 3:
            case 4:
            case 5:
            case 6:
                strncpy(rpDepBody->pclStop1, pclVial[0],3);
                strncpy(rpDepBody->pclStop2, pclVial[1],3);
                strncpy(rpDepBody->pclStop3, pclVial[2],3);
                strncpy(rpDepBody->pclStop4, pclTmp, 3);
                break;
            default:
                dbg(DEBUG,"%s ilNo<%d>is more than six or less then zero",pclFunc, ilNo);
                break;
        }
    }

    /*14-TYP-ACT3*/
    ilRc = extractField(pclTmp, "ACT3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The TYP value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclTYP,0,sizeof(rpArrBody->pclTYP));
        fillField(rpDepBody->pclTYP, "", 3);
    }
    else
    {
        dbg(DEBUG,"%s The TYP value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclTYP, pclTmp, 3);
    }

    /*15-TYS-ACT3*/
    ilRc = extractField(pclTmp, "ACT3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The TYS value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclTYS,0,sizeof(rpArrBody->pclTYS));
        fillField(rpDepBody->pclTYS, "", 8);
    }
    else
    {
        dbg(DEBUG,"%s The TYS value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclTYS, pclTmp, 8);
    }

    /*16-LDD-TIFD*/
    /*17-LTD-TIFD*/
    ilRc = extractField(pclTmp, "TIFD", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The LDD-LTD value<%s> is invalid", pclFunc, pclTmp);
        fillField(rpDepBody->pclLDD,"",8);
        fillField(rpDepBody->pclLTD,"",4);
    }
    else
    {
        dbg(DEBUG,"%s The LDD-LTD value<%s> is valid", pclFunc, pclTmp);
        strncpy(rpDepBody->pclLDD, pclTmp, 8);
        strncpy(rpDepBody->pclLTD, pclTmp+8, 4);
    }

    /*18-Na*/
    strncpy(rpDepBody->pclNA," ",1);

    /*19-CA1
    ilRc = extractField(rpDepBody->pclCA1, "BLT1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The CA1 value<%s> is invalid", pclFunc, rpDepBody->pclCA1);
        memset(rpDepBody->pclCA1,0,sizeof(rpDepBody->pclCA1));
    }
    else
    {
        dbg(DEBUG,"%s The CA1 value<%s> is valid", pclFunc, rpDepBody->pclCA1);
    }
    */

    /*20-GAT*/
    ilRc = extractField(pclTmp, "GTD1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The GAT value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclGAT,0,sizeof(rpArrBody->pclGAT));
        fillField(rpDepBody->pclGAT,"",3);
    }
    else
    {
        dbg(DEBUG,"%s The GAT value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclGAT,pclTmp,3);
    }

    /*21-Remarks_Terminal_Id-Remarks_Flight_Status_Portion*/
    ilRc = extractField(pclTmp, "TRMD", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The Terminal value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclRmkTermId,0,sizeof(rpArrBody->pclRmkTermId));
        fillField(rpDepBody->pclRmkTermId,"",3);
    }
    else
    {
        dbg(DEBUG,"%s The Terminal value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclRmkTermId,pclTmp,3);
    }

    memset(rpDepBody->pclRmkFltStatus,0,sizeof(rpDepBody->pclRmkFltStatus));
    ilRc = extractField(pclTmp, "REMP", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The REMP value<%s> is invalid", pclFunc, pclTmp);
        //strcpy(rpDepBody->pclRmkTermId,"  ");
        fillField(rpDepBody->pclRmkFltStatus,"",2);
    }
    else
    {
        dbg(DEBUG,"%s The Terminal value<%s> is valid", pclFunc, pclTmp);

        if (strlen(pclTmp) == 0)
        {
            /*No remark->On time*/
            strcpy(rpDepBody->pclRmkFltStatus,"  1");
        }
        else
        {
            if ( strstr(pcgRmkDelayed,pclTmp) != 0 )
            {
                strcpy(rpDepBody->pclRmkFltStatus,"  3");
            }
            else if ( strstr(pcgRmkCancelled,pclTmp) != 0 )
            {
                strcpy(rpDepBody->pclRmkFltStatus,"  5");
            }
            else if ( strstr(pcgRmkDiverted,pclTmp) != 0 )
            {
                strcpy(rpDepBody->pclRmkFltStatus,"  9");
            }
            else if ( strstr(pcgRmkArr,pclTmp) != 0 )
            {
                strcpy(rpDepBody->pclRmkFltStatus," 10");
            }
            else if ( strstr(pcgRmkDep,pclTmp) != 0 )
            {
                strcpy(rpDepBody->pclRmkFltStatus," 32");
            }
            else
            {
                strcpy(rpDepBody->pclRmkFltStatus,"   ");
            }
        }
    }

    /*22-Time Sync*/
    //strcpy(rpDepBody->pclTimeSync,"    ");
    fillField(rpDepBody->pclTimeSync,"",4);

    /*23-Previous Gate*/
    //strcpy(rpDepBody->pclPreGate,"    ");
    fillField(rpDepBody->pclPreGate,"",4);

    /*24-Overbooked Compensation*/
    //strcpy(rpDepBody->pclOverbookedCompensation,"   ");
    fillField(rpDepBody->pclOverbookedCompensation,"",3);

    /*25-Frequent Flyer Miles*/
    ilRc = extractField(pclTmp, "EXT1", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The frequent flyer miles value<%s> is invalid", pclFunc, pclTmp);
        //memset(rpArrBody->pclFreFlyerMiles,0,sizeof(rpArrBody->pclFreFlyerMiles));
        fillField(rpDepBody->pclFreFlyerMiles,"",5);
    }
    else
    {
        dbg(DEBUG,"%s The frequent flyer miles value<%s> is valid", pclFunc, pclTmp);
        fillField(rpDepBody->pclFreFlyerMiles,pclTmp,5);
    }

    /*26-Flight delay reason code*/
    //strcpy(rpDepBody->pclFltDelayRsnCode,"     ");
    fillField(rpDepBody->pclFltDelayRsnCode,"",5);

    /*27-Time_Duration*/
    //strcpy(rpDepBody->pclTimeDuration,"    ");
    fillField(rpDepBody->pclTimeDuration,"",4);

    /*28-Meal_Service*/
    strcpy(rpDepBody->pclMealService,"    ");
    fillField(rpDepBody->pclMealService,"",1);

    /*27-Days of operation*/
    strcpy(rpDepBody->pclDaysOfOper,"1234567");

    /*28-Effective Date*/
    //strcpy(rpDepBody->pclEffDate,"        ");
    fillField(rpDepBody->pclEffDate,"",5);

    /*29-Discontinue Date*/
    //strcpy(rpDepBody->pclDisDate,"        ");
    fillField(rpDepBody->pclDisDate,"",5);
}

static void storeMasterData(_MASTER *rpMaster, char *pcpFieldList, char *pcpDatalist)
{
    int ilRc = RC_FAIL;
    char *pclFunc = "storeMasterData";
    char pclTmp[1024] = "\0";

    ilRc = extractField(rpMaster->pclMPC, "ALC3", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The MPC value<%s> is invalid", pclFunc, rpMaster->pclMPC);
        memset(rpMaster->pclMPC,0,sizeof(rpMaster->pclMPC));
    }
    else
    {
        dbg(DEBUG,"%s The MPC value<%s> is valid", pclFunc, rpMaster->pclMPC);
    }

    ilRc = extractField(rpMaster->pclMPN, "FLTN", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The MPN value<%s> is invalid", pclFunc, rpMaster->pclMPN);
        memset(rpMaster->pclMPN,0,sizeof(rpMaster->pclMPN));
    }
    else
    {
        dbg(DEBUG,"%s The MPN value<%s> is valid", pclFunc, rpMaster->pclMPN);
    }

    ilRc = extractField(rpMaster->pclMPX, "FLNS", pcpFieldList, pcpDatalist);
    if (ilRc == RC_FAIL)
    {
        dbg(TRACE,"%s The MPX value<%s> is invalid", pclFunc, rpMaster->pclMPX);
        memset(rpMaster->pclMPX,0,sizeof(rpMaster->pclMPX));
    }
    else
    {
        dbg(DEBUG,"%s The MPX value<%s> is valid", pclFunc, rpMaster->pclMPX);
    }
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
            BuildItemBuffer(pclSqlData, NULL, ilNoEle, ",");
            dbg(TRACE, "<%s> Retrieving source data - Found <%s>", pclFunc, pclSqlData);
            strcpy(pcpSourceDataList, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

static int getVial(char *pcpVial, char (*pcpVialArray)[LISTLEN])
{
    int ilNO=0;
    int ilCount = 0;
    int ilRC = RC_FAIL;
    char *pclFunc = "getVial";
    char pclTmp[16] = "\0";
    char pclBuffer[LISTLEN] = "\0";

    strcpy(pclBuffer, pcpVial+1);
    ilNO = strlen(pclBuffer) / VIAL_LEN ;

    dbg(TRACE,"%s len<%d> ilNo<%d>", pclFunc, strlen(pclBuffer), ilNO);

    for (ilCount = 0; ilCount <= ilNO; ilCount++)
    {
        strncpy(pclTmp, pclBuffer + ilCount * (VIAL_LEN - 1), 3);
        /*dbg(DEBUG,"%s %d pclTmp<%s>", pclFunc, ilCount, pclTmp);*/

        strcpy(pcpVialArray[ilCount], pclTmp);
    }

    return ilNO;
}

static void sendArrMsg(_HEAD *rpHead ,_MASTER *rpMaster, _ARR_MSG_BODY *rpArrBody, char *pcpTable)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "sendArrMsg";
    char pclArrData[4096] = "\0";

    sprintf(pclArrData,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
    rpHead->pclMsgHeader,
    rpHead->pclObjId,
    rpHead->pclMsgId,
    rpHead->pclMsgNo,
    rpHead->pclMsgType,
    rpArrBody->pclPLC,
    rpArrBody->pclPLN,
    rpArrBody->pclPLX,
    rpArrBody->pclSDT,
    rpArrBody->pclSTA,
    rpArrBody->pclSST,
    rpArrBody->pclMPC,
    rpArrBody->pclMPN,
    rpArrBody->pclMPX,
    rpArrBody->pclStop1,
    rpArrBody->pclStop2,
    rpArrBody->pclStop3,
    rpArrBody->pclStop4,
    rpArrBody->pclTYP,
    rpArrBody->pclTYS,
    rpArrBody->pclLDA,
    rpArrBody->pclLTA,
    rpArrBody->pclNA,
    rpArrBody->pclCA1,
    rpArrBody->pclGAT,
    rpArrBody->pclRmkTermId,
    rpArrBody->pclRmkFltStatus,
    rpArrBody->pclTimeSync,
    rpArrBody->pclPreGate,
    rpArrBody->pclOverbookedCompensation,
    rpArrBody->pclFreFlyerMiles,
    rpArrBody->pclFltDelayRsnCode,
    rpArrBody->pclDaysOfOper,
    rpArrBody->pclEffDate,
    rpArrBody->pclDisDate
    );

    ilRC = SendCedaEvent(igDestId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,pcgDestCmd,pcpTable,pcgMsgId,"",pclArrData,"","",NETOUT_NO_ACK);

    igMsgID++;
    if(igMsgID >= 255)
    {
        igMsgID = 0;
    }
}

static void sendDepMsg(_HEAD *rpHead ,_MASTER *rpMaster, _DEP_MSG_BODY *rpDepBody, char *pcpTable)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "sendDepMsg";
    char pclDepData[4096] = "\0";

    sprintf(pclDepData, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
    rpHead->pclMsgHeader,
    rpHead->pclObjId,
    rpHead->pclMsgId,
    rpHead->pclMsgNo,
    rpHead->pclMsgType,
    rpDepBody->pclPLC,
    rpDepBody->pclPLN,
    rpDepBody->pclPLX,
    rpDepBody->pclSDT,
    rpDepBody->pclSTD,
    rpDepBody->pclSST,
    rpDepBody->pclMPC,
    rpDepBody->pclMPN,
    rpDepBody->pclMPX,
    rpDepBody->pclStop1,
    rpDepBody->pclStop2,
    rpDepBody->pclStop3,
    rpDepBody->pclStop4,
    rpDepBody->pclTYP,
    rpDepBody->pclTYS,
    rpDepBody->pclLDD,
    rpDepBody->pclLTD,
    rpDepBody->pclNA,
    rpDepBody->pclGAT,
    rpDepBody->pclRmkTermId,
    rpDepBody->pclRmkFltStatus,
    rpDepBody->pclTimeSync,
    rpDepBody->pclPreGate,
    rpDepBody->pclOverbookedCompensation,
    rpDepBody->pclFreFlyerMiles,
    rpDepBody->pclFltDelayRsnCode,
    rpDepBody->pclTimeDuration,
    rpDepBody->pclMealService,
    rpDepBody->pclDaysOfOper,
    rpDepBody->pclEffDate,
    rpDepBody->pclDisDate
    );

    ilRC = SendCedaEvent(igDestId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,pcgDestCmd,pcpTable,pcgMsgId,"",pclDepData,"","",NETOUT_NO_ACK);

    igMsgID++;
    if(igMsgID >= 255)
    {
        igMsgID = 0;
    }
}
