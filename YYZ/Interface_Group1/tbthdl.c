
#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/tbthdl.c 0.4 2014/06/03 15:27:14SGT fya Exp  $";
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
/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command")     */
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
static char  cgHopo[8] = "\0";                         /* default home airport    */
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

/*fya 0.1*/
_RULE rgRule;
static int igTotalLineOfRule;

static char pcgSourceFiledSet[GROUPNUMER][LISTLEN];
static char pcgSourceFiledList[GROUPNUMER][LISTLEN];
static char pcgDestFiledList[GROUPNUMER][LISTLEN];
static char pcgSourceConflictFiledSet[GROUPNUMER][LISTLEN];

static char pcgTimeWindowRefField[8];
static int igTimeWindowUpperLimit;
static int igTimeWindowLowerLimit;
static int igGetDataFromSrcTable;

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
//static int collectSourceFieldSet(char *pcpFiledSet, char * pcpConflictFiledSet, char *pcpSourceField, int ipCount);
static int collectSourceFieldSet(char *pcpFiledSet, char * pcpConflictFiledSet, char *pcpSourceField, int ipGroupNumer);
static int collectFieldList(char *pcpFiledSet, char *pcpDestField, int ipGroupNumer);
static int isDisabledLine(char *pcpLine);
static int isCommentLine(char *pcpLine);
static int extractTimeWindowRefField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData);
static int isInTimeWindow(char *pcpTimeVal, int ilTimeWindowUpperLimit, int ilTimeWindowLowerLimit);
static int toDetermineAppliedRuleGroup(char * pcpTable, char * pcpFields, char * pcpNewData);
static void showFieldByGroup(char (*pcpSourceFiledSet)[LISTLEN], char (*pcpConflictFiledSet)[LISTLEN], char (* pcpSourceFiledList)[LISTLEN], char (* pcpDestFiledSet)[LISTLEN]);
static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList, _RULE *rpRule, int ipTotalLineOfRule);
static int matchFieldListOnGroup(int ilRuleGroup, char *pcpSourceFieldList);
static int getSourceFieldData(char *pclTable, char * pcpSourceFieldList, char * pcpSelection, char * pclSourceDataList);
static void buildQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection);
static int convertSrcValToDestVal(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _LINE * rpLine, char * pcpDestFieldValue);
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
			//lgEvtCnt++;

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

	int ilRuleGroup = 0;
	char *pclFunc = "HandleData";

	BC_HEAD *prlBchead       = NULL;
	CMDBLK  *prlCmdblk       = NULL;
	char    *pclSelection    = NULL;
	char    *pclFields       = NULL;
	char    *pclData         = NULL;
	char    *pclRow          = NULL;
	char 		clUrnoList[2400];
	char 		clTable[34];
	int			ilUpdPoolJob = TRUE;

    char pclSourceFieldList[2048] = "\0";
    char pclSourceDataList[2048] = "\0";
    char pclWhereClaues[2048] = "\0";

    char *pclTmpPtr = NULL;
    char pclUrnoSelection[50] = "\0";
    char pclTimeWindowRefFieldVal[32] = "\0";
    char pclNewData[512000] = "\0";
    char pclOldData[512000] = "\0";

	char pclTimeWindowRefField[8] = "\0";

	prlBchead    = (BC_HEAD *) ((char *)prpEvent + sizeof(EVENT));
	prlCmdblk    = (CMDBLK *)  ((char *)prlBchead->data);
	pclSelection = prlCmdblk->data;
	pclFields    = pclSelection + strlen(pclSelection) + 1;
	pclData      = pclFields + strlen(pclFields) + 1;

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
	//DebugPrintBchead(TRACE,prlBchead);
	//DebugPrintCmdblk(TRACE,prlCmdblk);
	/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command"); */
	dbg(TRACE,"Command: <%s>",prlCmdblk->command);
	dbg(TRACE,"Originator follows event = %p ",prpEvent);

	dbg(TRACE,"Originator<%d>",prpEvent->originator);
	//dbg(TRACE,"selection follows Selection = %p ",pclSelection);
	dbg(DEBUG,"Selection: <%s> URNO<%s>",pclSelection,pclUrnoSelection);
	dbg(TRACE,"Fields    <%s>",pclFields);
	dbg(DEBUG,"New Data:  <%s>",pclNewData);
	dbg(DEBUG,"Old Data:  <%s>",pclOldData);
    dbg(DEBUG,"Table:  <%s>",clTable);

	lgEvtCnt++;

    if (strcmp(prlCmdblk->command,"INI") == 0)
    {

    }
    else if (strcmp(prlCmdblk->command,"RFH") == 0)
    {

    }
    else /*The normal DFR, IFR and UFR command*/
    {
        ilRc = extractTimeWindowRefField(pclTimeWindowRefFieldVal, pcgTimeWindowRefField, pclFields, pclNewData);
        if (ilRc == RC_FAIL)
        {
            ilRc = RC_FAIL;
        }

        ilRc = isInTimeWindow(pclTimeWindowRefFieldVal, igTimeWindowUpperLimit, igTimeWindowLowerLimit);
        if(ilRc == RC_FAIL)
        {
            ilRc = RC_FAIL;
        }

        /*  fya 0.2
            To determine the applied rule group
        */
        ilRuleGroup = toDetermineAppliedRuleGroup(clTable, pclFields, pclNewData);
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
                ilRc = RC_FAIL;
            }
            else
            {
                dbg(TRACE,"%s The source field list is <%s>", pclFunc, pclSourceFieldList);
            }

            ilRc = getSourceFieldData(clTable, pclSourceFieldList, pclSelection, pclSourceDataList);
            if (ilRc == RC_FAIL)
            {
                dbg(TRACE,"%s The source field data is not found - using the original field and data list", pclFunc);
                appliedRules( ilRuleGroup, pclFields, pclNewData, pcgSourceFiledList[ilRuleGroup], pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule);
            }
            else
            {
                dbg(TRACE,"%s The source field list is found <%s>", pclFunc, pclSourceFieldList);

                dbg(DEBUG,"%s pcgSourceFiledList[%d] <%s>", pclFunc, ilRuleGroup, pcgSourceFiledList[ilRuleGroup]);
                dbg(DEBUG,"%s pcgDestFiledList[%d] <%s>", pclFunc, ilRuleGroup, pcgDestFiledList[ilRuleGroup]);

                appliedRules( ilRuleGroup, pclSourceFieldList, pclSourceDataList, pcgSourceFiledList[ilRuleGroup], pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule);
            }
        }
        else
        {
            /*using the original field and data list*/
            dbg(TRACE,"%s The getting source data config option is not set - using the original field and data list", pclFunc);
            appliedRules( ilRuleGroup, pclFields, pclNewData, pcgSourceFiledList[ilRuleGroup], pcgDestFiledList[ilRuleGroup], &rgRule, igTotalLineOfRule);
        }
    }

    /****************************************/
	dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);

	return ilRc;

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

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_WINDOW_REFERENCE_FIELD",CFG_STRING,pcgTimeWindowRefField);
    if (ilRC == RC_SUCCESS)
    {
        dbg(DEBUG,"pcgTimeWindowRefField<%s>",pcgTimeWindowRefField);
    }
    else
    {
        strcpy(pcgTimeWindowRefField,"STOA");
        dbg(DEBUG,"pcgTimeWindowRefField<%s>",pcgTimeWindowRefField);
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
        dbg(DEBUG,"igGetDataFromSrcTable<%d>",igGetDataFromSrcTable);
    }
    else
    {
        igGetDataFromSrcTable = FALSE;
        dbg(DEBUG,"Default igGetDataFromSrcTable<%d>",igGetDataFromSrcTable);
    }

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
    char pclCfgFile[128];
    FILE *fp;
    int ilCurLine;
    char pclLine[2048];

    int ilNoLine = 0;
    int ilI;
    int ilJ;
    char pclTmpBuf[128];
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
            dbg(DEBUG,"%s Disabled Rule -> Ignore & Continue", pclFunc);
            continue;
        }

        getOneline(&rlLine, pclLine);
        //showLine(&rlLine);

        /*store the required and conflicted field list group by group*/
        collectSourceFieldSet(pcgSourceFiledSet[atoi(rlLine.pclRuleGroup)], pcgSourceConflictFiledSet[atoi(rlLine.pclRuleGroup)], rlLine.pclSourceField, atoi(rlLine.pclRuleGroup));

        collectFieldList(pcgSourceFiledList[atoi(rlLine.pclRuleGroup)], rlLine.pclSourceField, atoi(rlLine.pclRuleGroup));
        collectFieldList(pcgDestFiledList[atoi(rlLine.pclRuleGroup)], rlLine.pclDestField, atoi(rlLine.pclRuleGroup));

        /*Copy to global structure*/
        storeRule( &(rpRule->rlLine[ilNoLine++]), &rlLine);
    }
    igTotalLineOfRule = ilNoLine;
    showRule(rpRule, igTotalLineOfRule);

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
}

static void showLine(_LINE *rpLine)
{
    char pclFunc[] = "showLine:";

    dbg(DEBUG, "%s %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,
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
    rpLine->pclCond2);
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
}

static void showRule(_RULE *rpRule, int ipTotalLineOfRule)
{
    char pclFunc[] = "showRule:";

    int ilCount = 0;

    dbg(DEBUG,"%s There are <%d> lines", pclFunc, ipTotalLineOfRule);

    for (ilCount = 0; ilCount < ipTotalLineOfRule; ilCount++)
    {
        dbg(DEBUG, "%s %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,
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
        rpRule->rlLine[ilCount].pclCond2);
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
        dbg(TRACE,"%s GROUP<%d> pcpSourceField<%s> is already in pcpFiledSet<%s>", pclFunc, ipGroupNumer, pcpSourceField, pcpFiledSet);

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

static int extractTimeWindowRefField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData)
{
    char *pclFunc = "extractTimeWindowRefField";

    int ilItemNo = 0;

    ilItemNo = get_item_no(pcpFields, pcpFieldName, 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No <%s> Found in the field list, can't do anything", pclFunc, pcpFieldName);
        return RC_FAIL;
    }

    get_real_item(pcpFieldVal, pcpNewData, ilItemNo);
    dbg(DEBUG,"<%s> The New %s is <%s>", pclFunc, pcpFieldName, pcpFieldVal);

    if ( atoi(pcpFieldVal) != 0 )
    {
        dbg(TRACE, "<%s> %s = %s", pclFunc, pcpFieldName, pcpFieldVal);
        return RC_SUCCESS;
    }
    else
    {
        return RC_FAIL;
    }
}

static int isInTimeWindow(char *pcpTimeVal, int igTimeWindowUpperLimit, int igTimeWindowLowerLimit)
{
    char *pclFunc = "isInTimeWindow";

    char pclTimeLowerLimit[TIMEFORMAT] = "\0";
    char pclTimeUpperLimit[TIMEFORMAT] = "\0";
    char pclTimeNow[TIMEFORMAT] = "\0";

    GetServerTimeStamp( "UTC", 1, 0, pclTimeNow);
    dbg(TRACE,"<%s> Currnt time is <%s>",pclFunc, pclTimeNow);

    strcpy(pclTimeLowerLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeLowerLimit, igTimeWindowLowerLimit * 24 * 60 * 60, 1);

    strcpy(pclTimeUpperLimit,pclTimeNow);
    AddSecondsToCEDATime(pclTimeUpperLimit, igTimeWindowUpperLimit * 24 * 60 * 60, 1);

    dbg(TRACE,"The time range is <%s> ~ <%s>", pclTimeLowerLimit, pclTimeUpperLimit);

    if ( strcmp(pcpTimeVal,pclTimeLowerLimit) >= 0 && strcmp(pcpTimeVal,pclTimeUpperLimit) <= 0)
    {
        dbg(TRACE,"%s <%s> is in the range", pclFunc, pcpTimeVal);

        return RC_SUCCESS;
    }
    else
    {
        dbg(TRACE,"%s <%s> is out of the range", pclFunc, pcpTimeVal);
        return RC_FAIL;
    }
}

static int toDetermineAppliedRuleGroup(char * pcpTable, char * pcpFields, char * pcpData)
{
    int ilRuleNumber = 0;
    int ilItemNo = 0;
    char *pclFunc = "toDetermineAppliedRuleGroup";
    char pclADID[16] = "\0";

    ilItemNo = get_item_no(pcpFields, "ADID", 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No <%s> Not found in the field list, can't do anything", pclFunc, "ADID");
        return RC_FAIL;
    }
    get_real_item(pclADID, pcpData, ilItemNo);
    dbg(DEBUG,"<%s> The New ADID is <%s>", pclFunc, pclADID);

    if ( strcmp(pcpTable,"AFTTAB") == 0 )
    {
        if (strcmp(pclADID, "A") == 0 )
        {
            ilRuleNumber = 1;
        }
        else if (strcmp(pclADID, "D") == 0 )
        {
            ilRuleNumber = 2;
        }
    }
    else if ( strcmp(pcpTable,"CCATAB") == 0 )
    {
        ilRuleNumber = 3;
    }

    return ilRuleNumber;
}

static int appliedRules( int ipRuleGroup, char *pcpFields, char *pcpData, char *pcpSourceFiledList, char *pcpDestFiledList, _RULE *rpRule, int ipTotalLineOfRule)
{
    int ilRC = RC_SUCCESS;
    int ilItemNo = 0;
    int ilEleCount = 0;
    int ilRuleCount = 0;
    int ilNoEleSource = 0;
    int ilNoEleDest = 0;
    char *pclFunc = "appliedRules";
    char pclTmpSourceFieldName[256] = "\0";
    char pclTmpDestFieldName[256] = "\0";

    char pclTmpSourceFieldValue[256] = "\0";
    char pclTmpDestFieldValue[256] = "\0";
    char pclDestDataList[256] = "\0";

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

	dbg(TRACE,"%s The number of source and dest list are <%d> and <%d>", pclFunc, ilNoEleSource, ilNoEleDest);

	if (ilNoEleDest == ilNoEleSource)
	{
	    dbg(TRACE,"%s The number of source and dest list euqals <%d>==<%d>", pclFunc, ilNoEleSource, ilNoEleDest);

        for(ilEleCount = 1; ilEleCount <= ilNoEleDest; ilEleCount++)
        {
            get_item(ilEleCount, pcpSourceFiledList, pclTmpSourceFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpSourceFieldName);

            get_item(ilEleCount, pcpDestFiledList, pclTmpDestFieldName, 0, ",", "\0", "\0");
            TrimRight(pclTmpDestFieldName);

            dbg(DEBUG,"%s <%d> The filed from source is <%s> - from dest is <%s>",pclFunc, ilEleCount, pclTmpSourceFieldName, pclTmpDestFieldName);

            for (ilRuleCount = 0; ilRuleCount <= ipTotalLineOfRule; ilRuleCount++)
            {
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
                        get_real_item(pclTmpSourceFieldValue, pcpData, ilItemNo);
                        dbg(DEBUG,"<%s> Source - The value for %s is <%s>", pclFunc, pclTmpSourceFieldName, pclTmpSourceFieldValue);

                        /*getting the operator and condition*/
                        dbg(TRACE,"%s The operator is <%s> and con1<%s>, con2<%s>",pclFunc, rpRule->rlLine[ilRuleCount].pclDestFieldOperator,
                        rpRule->rlLine[ilRuleCount].pclCond1, rpRule->rlLine[ilRuleCount].pclCond2);

                        convertSrcValToDestVal(pclTmpSourceFieldName, pclTmpSourceFieldValue, pclTmpDestFieldName, rpRule->rlLine+ilRuleCount, pclTmpDestFieldValue);

                        if ( strlen(pclDestDataList) == 0 )
                        {
                            strcat(pclDestDataList,pclTmpDestFieldValue);
                        }
                        else
                        {
                            strcat(pclDestDataList,",");
                            strcat(pclDestDataList,pclTmpDestFieldValue);
                        }
                    }
                }
                else
                {
                    //dbg(DEBUG,"%s The group number is not matched - ipRuleGroup<%d> and <%d>",pclFunc, ipRuleGroup, atoi(rpRule->rlLine[ilRuleCount].pclRuleGroup));
                    continue;
                }
            }
        }
        dbg(TRACE, "%s The manipulated dest data list <%s> field number<%d>", pclFunc, pclDestDataList, GetNoOfElements(pclDestDataList,','));

        if ( GetNoOfElements(pclDestDataList,',') != ilNoEleSource)
        {
            dbg(TRACE,"The number of field in dest data list and source field is not matched",pclFunc);
        }

        /*building dest table insert and update query clause*/

	}
	else
	{
        dbg(TRACE,"%s The number of source and dest list does not euqal <%d>!=<%d>", pclFunc, ilNoEleSource, ilNoEleDest);
        ilRC = RC_FAIL;
	}

	return ilRC;
}

static void showFieldByGroup(char (*pcpSourceFiledSet)[LISTLEN], char (*pcpConflictFiledSet)[LISTLEN], char (* pcpSourceFiledList)[LISTLEN], char (* pcpDestFiledSet)[LISTLEN])
{
    int ilCount = 0;
    char *pclFunc = "showFieldByGroup";

    for (ilCount = 0; ilCount < GROUPNUMER; ilCount++)
    {
        if (strlen(pcpSourceFiledSet[ilCount]) > 0)
        {
            dbg(TRACE,"%s Group[%d] Source Field Set <%s>", pclFunc, ilCount, (pcpSourceFiledSet+ilCount)[0]);
            dbg(TRACE,"%s Group[%d] Conflicted Source Field List <%s>", pclFunc, ilCount, (pcpConflictFiledSet+ilCount)[0]);
            dbg(TRACE,"%s Group[%d] Source Field List <%s>", pclFunc, ilCount, (pcpSourceFiledList+ilCount)[0]);
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
            dbg(DEBUG,"%s Group[%d] - The field list is <%s>", pclFunc, ilRuleGroup, pcgSourceFiledSet[ilRuleGroup]);
            strcpy(pcpSourceFieldList, pcgSourceFiledSet[ilRuleGroup]);
            ilRC = RC_SUCCESS;
        }
        else
        {
            dbg(DEBUG,"%s Group[%d] - The field list is blank", pclFunc, ilRuleGroup);
            ilRC = RC_FAIL;
        }
    }
    return ilRC;
}

static int getSourceFieldData(char *pclTable, char * pcpSourceFieldList, char * pcpSelection, char * pcpSourceDataList)
{
    int ilRC = RC_SUCCESS;
    char *pclFunc = "getSourceFieldData";

    char pclSqlBuf[4096] = "\0";
    char pclSqlData[4096] = "\0";

    buildQuery(pclSqlBuf, pclTable, pcpSourceFieldList, pcpSelection);

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
            strcpy(pcpSourceDataList, pclSqlData);
            ilRC = RC_SUCCESS;
            break;
    }
    return ilRC;
}

static void buildQuery(char *pcpSqlBuf, char * pcpTable, char * pcpSourceFieldList, char * pcpSelection)
{
    char *pclFunc = "buildQuery";

    strcat(pcpSqlBuf,"SELECT ");
    strcat(pcpSqlBuf, pcpSourceFieldList);
    strcat(pcpSqlBuf, " FROM ");
    strcat(pcpSqlBuf, pcpTable);
    strcat(pcpSqlBuf, " ");
    strcat(pcpSqlBuf, pcpSelection);

    dbg(TRACE,"%s pcpSqlBuf<%s>",pclFunc, pcpSqlBuf);
}

static int convertSrcValToDestVal(char *pcpSourceFieldName, char *pcpSourceFieldValue, char *pcpDestFieldName, _LINE * rpLine, char * pcpDestFieldValue)
{
    int ilRC = RC_SUCCESS;
    char *pclFunc = "convertSrcValToDestVal";
    char pclOperator[16] = "\0";

    memset(pcpDestFieldValue,0,sizeof(pcpDestFieldValue));

    strcpy(pclOperator,rpLine->pclDestFieldOperator);
    TrimRight(pclOperator);

    if ( strlen(pclOperator) == 0 || pclOperator[0] == ' ')
    {
        /*operator is blank, so, using the original value as the dest one*/
        dbg(TRACE, "%s The operator is blank",pclFunc);
        strcpy(pcpDestFieldValue, pcpSourceFieldValue);
        ilRC = RC_SUCCESS;
    }
    else
    {
        dbg(TRACE, "%s The operator is <%s>",pclFunc, pclOperator);

        if ( strcmp(pclOperator,"SUBS") == 0)
        {
            /*
            dbg(TRACE, "%s The con1<%s> and con2<%s> -> getting the substring",pclFunc);

            if( atoi(rpLine->pclDestFieldLen) == (atoi(rpLine->pclCond2) - atoi(rpLine->pclCond1) + 1) )
            {
                strncpy(pcpDestFieldValue, pcpSourceFieldValue + atoi(rpLine->pclCond1), atoi(rpLine->pclDestFieldLen));
            }
            */
            ilRC = RC_SUCCESS;
        }
        else if ( strcmp(pclOperator,"DEP") == 0)
        {
            strcpy(pcpDestFieldValue, pcpSourceFieldValue);
            ilRC = RC_SUCCESS;
        }
        else if ( strcmp(pclOperator,"ARR") == 0)
        {
            strcpy(pcpDestFieldValue, pcpSourceFieldValue);
            ilRC = RC_SUCCESS;
        }
        else
        {
            /*unknown operator -> treat it as default action*/
            strcpy(pcpDestFieldValue, pcpSourceFieldValue);
            ilRC = RC_FAIL;
        }
    }

    dbg(TRACE,"SourceField<%s> SourceValue<%s> DestinationField<%s> DestinationValue<%s>",
        pcpSourceFieldName, pcpSourceFieldValue, pcpDestFieldName, pcpDestFieldValue);

    return ilRC;
}
