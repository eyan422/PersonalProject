#ifndef _DEF_mks_version
	#define _DEF_mks_version
	#include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
	static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/genif.c 1.0 2013/11/18 10:43:14SGT cst Exp  $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* UFIS Program Skeleton                                                      */
/*                                                                            */
/* Author         :  Christian Stybert                                        */
/* Date           :  November 18, 2013                                        */
/* Description    :  File and HTTP based event interface                      */
/*                   The process is generic and is supposed to be used as as  */
/*                   template for various new interfaces                      */
/*                   Currently the process supports these commands:           */
/*                                                                            */
/*                   XMLO from exchdl will cause the XML to be written to file*/
/*                   AASO is used to send HTTP GET to the BPN AAS system      */
/*                   CSVI is used for generating events based on nflhdl events*/
/*                                                                            */
/* Update history :                                                           */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */

static char sccs_version[] ="@(#) UFIS45 (c) ABB AAT/I genif.c 45.3 / 18.11.2013 CST";

/* be careful with strftime or similar functions !!!                          */
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
#include <sys/socket.h>
#include <netinet/in.h>
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
#include "syslib.h"
#include "db_if.h"
#include <time.h>
#include <cedatime.h>

#ifdef _HPUX_SOURCE
	extern int	daylight;
	extern long timezone;
	extern char *tzname[2];
#else
	extern int	_daylight;
	extern long _timezone;
	extern char *_tzname[2];
#endif

int debug_level = 0;
/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */
static int		igItemLen     = 0;                   /* length of incoming item */
static EVENT *prgOutEvent  = NULL;

static char	cgProcessName[20] = "\0";
static char	cgConfigFile[512] = "\0";
static char	cgHopo[8] = "\0";                         /* default home airport    */
static char	cgTabEnd[8] ="\0";                        /* default table extension */

/* General global event variables */

static int		igTimeDiff = 0;
static char	cgCommand[16] = "\0";
static char	cgSelection[2048] = "\0";
static char	cgFields[4096] = "\0";
static char	cgData[16384] = "\0";
static char	cgOldData[16384] = "\0";
static char	cgHMS[8] = "\0";
static char	cgDestination[256] = "\0";
static int		igSaveURNO;


/* Command based event variables */

/* For XMLO */

static char	cgXMLOCommand[16] = "\0";
static char	cgXMLOSelection[1024] = "\0";
static char	cgXMLOFileNameFormat[512] = "\0";
static long	lgXMLOSeqNo = 1;
static int		igXMLOModId = 0;

/* For AASO */

static char	pcgCodeSharesFLNO[256] = "\0";
static char	pcgVias[64] = "\0";
static char	pcgWebHost[128] = "\0";
static int		igWebPort = 80;
static char	cgAASCommands[5][256];
static char	cgAASFormats[5][256];
static int		igAASFormatsLoaded = 0;
static char	pcgAASPoll[256];
static int		igAASSock = 0;
static int		igAASConnected = 0;

/* For CSVI (BAS/BIS) */

static char	pcgCSVIFld[512] = "\0";
static char	pcgCSVILen[512] = "\0";
static char	pcgCSVITab[16]  = "\0";
static int		igNbrCSVIFields = 0;
static int		igDestination = 0;
static char	pcgSendCommand[16] = "\0";

static long	lgEvtCnt = 0;

/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
extern int 	SetSignals(void (*HandleSignal)(int));
static int    Init_Process();
static int    ReadConfigEntry(char *pcpSection,char *pcpKeyword,
					char *pcpCfgBuffer);

static int	   Reset(void);                       /* Reset program          */
static void	Terminate(int ipSleep);            /* Terminate program      */
static void	HandleSignal(int);                 /* Handles signals        */
static void	HandleErr(int);                    /* Handles general errors */
static void   HandleQueErr(int);                 /* Handles queuing errors */
static void   HandleQueues(void);                /* Waiting for Sts.-switch*/


static int	   HandleData(EVENT *prpEvent);       /* Handles event data     */
static int    GetCommand(char *pcpCommand, int *pipCmd);
static int 	HandleXMLO(void);
static int		HandleAASO(void);
static int		HandleAASP(void);
static int		HandleCSVI(void);
static int		GetJFNO_VIA(char *pcpSep1, char *pcpSep2, char *pcpSep3);
static int		SendGETRequest(char *pcpGETRequest);
static int		Create_Socket (int type, char *Pservice);
static int		Open_Connection (int socket, char *pcpHostIP, int ipPort);
static void	TrimRight(char *pcpBuffer);
static void 	TrimLeftRight(char *pcpBuffer);

/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
	int	ilRc = RC_SUCCESS;			/* Return code			*/
	int	ilCnt = 0;
	int   ilOldDebugLevel = 0;

	INITIALIZE;			               /* General initialization	*/
	
	/* signal handling of SIGPIPE,SIGCHLD,SIGALRM,SIGTERM */
  SetSignals(HandleSignal);


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
	}
	else
	{
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
	dbg(TRACE,"Entering Main Loop...");
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

			lgEvtCnt++;

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

static int Init_Process()
{
	int	ilRc = RC_SUCCESS;			/* Return code */
	char	clSection[64] = "\0";
	char	clKeyword[64] = "\0";
	int	ilOldDebugLevel = 0;
	long	pclAddFieldLens[12];
	char	pclAddFields[256] = "\0";
	char	pclSqlBuf[2560] = "\0";
	char	pclSelection[1024] = "\0";
	short slCursor = 0;
	short slSqlFunc = 0;
	char  clBreak[24] = "\0";
	struct tm *TimeNow;
	char	clCfgValue[128] = "\0";

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
	
	if (ilRc == RC_SUCCESS)
	{
		ilRc = ReadConfigEntry("MAIN","Local_time",clCfgValue);
	
		if (ilRc == RC_SUCCESS)
		{
		igTimeDiff = atoi(clCfgValue);
		}
		
		ilRc = RC_SUCCESS;
	}
	
	if (ilRc != RC_SUCCESS)
	{
			dbg(TRACE,"Init_Process failed");
			return(ilRc);
	}
	
	ilRc = ReadConfigEntry("MAIN","Debug_level",clCfgValue);
	
	if (ilRc != RC_SUCCESS)
	{
		debug_level = TRACE;
	}
	else
	{
		if (strcmp(clCfgValue,"DEBUG") == 0)
		{
			debug_level = DEBUG;
		}
		else if (strcmp(clCfgValue,"NULL") == 0)
		{
			debug_level = 0;
		}
		else
		{
			debug_level = TRACE;
		}
	}
			
	return(RC_SUCCESS);

} /* end of initialize */

/******************************************************************************/
/* The Read configuration routine                                             */
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
		dbg(DEBUG,"ReadConfigEntry: Not found in %s: <%s> <%s>",cgConfigFile,clSection,clKeyword);
	} /* end of if */
	else
	{
		dbg(TRACE,"ReadConfigEntry: Config Entry <%s>,<%s>:<%s> found in %s",
			clSection, clKeyword ,pcpCfgBuffer, cgConfigFile);
	}/* end of else */
	
	return ilRc;
}


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{
	int	ilRc = RC_SUCCESS;				/* Return code */

	dbg(TRACE,"Reset: now resetting");
	
	cgHMS[0] = 0x00;
	Init_Process();

	return ilRc;

} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(int ipSleep)
{

	dbg(TRACE,"Terminate: now leaving ...");

	fclose(outp);
	
	if (igAASConnected == 1)
	{
		shutdown(igAASSock,2);
		close(igAASSock);
		igAASConnected = 0;
		igAASSock = 0;
	}

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
		case SIGALRM:
			break;
		case SIGPIPE:
			break;
		case SIGCHLD:
			break;
		case SIGTERM:
			Terminate(1);
			break;
		default    :
			Terminate(10);
			break;
	} /* end of switch */
    
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
		dbg(TRACE,"HandleQueues: init failed!");
	}


} /* end of HandleQueues */

/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData(EVENT *prpEvent)
{
	int	   ilRc           = RC_SUCCESS;			/* Return code */
	int      ilCmd          = 0;

	BC_HEAD *prlBchead       = NULL;
	CMDBLK  *prlCmdblk       = NULL;
	char    *pclSelection    = NULL;
	char    *pclFields       = NULL;
	char    *pclData         = NULL;
	char    *pclOldData      = NULL;
	char    *pclTmpPtr       = NULL;	
	char    *pclRow          = NULL;
	char 		clUrnoList[2400];
	char 		clTable[34];
	int		ilUpdPoolJob = TRUE;

	prlBchead    = (BC_HEAD *) ((char *)prpEvent + sizeof(EVENT));
	prlCmdblk    = (CMDBLK *)  ((char *)prlBchead->data);
	pclSelection = prlCmdblk->data;
	pclFields    = pclSelection + strlen(pclSelection) + 1;
	pclData      = pclFields + strlen(pclFields) + 1;


	strcpy(clTable,prlCmdblk->obj_name);

	dbg(TRACE,"========== START <%10.10d> ==========",lgEvtCnt);

	/****************************************
	DebugPrintBchead(TRACE,prlBchead);
	DebugPrintCmdblk(TRACE,prlCmdblk);
	20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command"); */
	
	dbg(TRACE,"Command: <%s>",prlCmdblk->command);
	dbg(DEBUG,"originator follows event = %p ",prpEvent);

	dbg(TRACE,"originator<%d>",prpEvent->originator);
	dbg(DEBUG,"selection follows Selection = %p ",pclSelection);
	dbg(TRACE,"selection <%s>",pclSelection);
	dbg(TRACE,"fields    <%s>",pclFields);
	dbg(TRACE,"data      <%s>",pclData);
	
	/****************************************/
	/* This process is meant to be a multi purpose tool to transfer messages    */
	/* contained in the data section via files. At this point XML/WEB transfers */
	/* are supported.
	/****************************************/
	
	strcpy(cgCommand, prlCmdblk->command);
	strcpy(cgSelection, pclSelection);
  pclTmpPtr = strstr(cgSelection,"\n");
  if (pclTmpPtr != NULL)
  {
     *pclTmpPtr = '\0';
     pclTmpPtr++;
     igSaveURNO = atoi(pclTmpPtr);
  }
  else
  {
  	igSaveURNO = 0;
  }
	strcpy(cgFields, pclFields);
	strcpy(cgData, pclData);
	pclTmpPtr = strstr(cgData,"\n");
	if (pclTmpPtr != NULL && strstr(cgCommand,"CSVI") == NULL)
	{
		*pclTmpPtr = '\0';
		pclTmpPtr++;
		strcpy(cgOldData,pclTmpPtr);
	}
	else
	{
		cgOldData[0] = 0x00;
	}
	
	dbg(TRACE,"Command   <%s>",cgCommand);
	dbg(TRACE,"URNO      <%d>",igSaveURNO);
	dbg(TRACE,"Selection <%s>",cgSelection);
	dbg(TRACE,"Fields    <%s>",cgFields);
	dbg(TRACE,"Data      <%s>",cgData);
	dbg(TRACE,"Old Data  <%s>",cgOldData);


	if (strstr("XMLO",cgCommand) != NULL)
	{
		ilRc = HandleXMLO();
	}
	else if (strstr("AASO",cgCommand) != NULL)
	{
		ilRc = HandleAASO();
	}
	else if (strstr("AASP",cgCommand) != NULL)
	{
		ilRc = HandleAASP();
	}
	else if (strstr("CSVI",cgCommand) != NULL)
	{
		ilRc = HandleCSVI();
	}
	
	else
	{
		dbg(TRACE,"Unsupported command: %s",cgCommand);
		ilRc = RC_FAIL;
	}
	
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"Handling of Command <%s> failed",cgCommand);
	}

	dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);

	return(RC_SUCCESS);

} /* end of HandleData */

static int HandleXMLO(void)
{
  /* exchdl uses the XMLO command as trigger and the selection field is used to */
  /* identify the destination (queue in case of wmqcdi). For XMLO handling the  */
  /* selection will be used to identify the section configured in the .cfg file.*/
  /* This means that the process can dynamically handle more destinations. The  */
  /* configuration contains the destination and optionally a module id and      */
  /* command (for, e.g., the ftphdl)                                            */
  
  int	ilRc = RC_SUCCESS;
  char	clCfgValue[128] = "\0";
  char	pclLocTime[16] = "\0";
  char	clSeqNo[32] = "\0";
  char	clFileName[128] = "\0";
  FILE	*FlFp = 0;
  char	pclTwStart[32];
  char	pclTwEnd[64];
  char	pclSelection[256] = "\0";
    
	if (strcmp(cgSelection,cgXMLOSelection) != 0)
	{
		/* Selection change, reset configuration */
		
		strcpy(pclSelection,cgSelection);	
		ilRc = ReadConfigEntry(pclSelection,"Destination",clCfgValue);
			
		if (ilRc != RC_SUCCESS)
		{
			/* Selection not configured */
			
			dbg(TRACE,"HandleXMLO: Section <%s> or Destination not found in configuration file",cgSelection);
			return RC_FAIL;
		}
			
		strcpy(cgXMLOSelection,cgSelection);
		strcpy(cgDestination,clCfgValue);
		
		if (cgDestination[strlen(cgDestination)-1] != '/')
		{
			strcat(cgDestination,"/");
		}
		
		ilRc = ReadConfigEntry(pclSelection,"Filename",clCfgValue);
			
		if (ilRc != RC_SUCCESS)
		{
			/* Use default file name format */
				
			strcpy(cgXMLOFileNameFormat, "%sXMLO%s.xml");
		}
		else
		{
			strcpy(cgXMLOFileNameFormat, clCfgValue);
		}
		dbg(TRACE,"FileNameFormat: <%s>",cgXMLOFileNameFormat);
			
		ilRc = ReadConfigEntry(pclSelection,"Modid",clCfgValue);
			
		if (ilRc != RC_SUCCESS)
		{
			/* No modid specified, set to 0 */
				
			igXMLOModId = 0;
		}
		else
		{
			igXMLOModId = atoi(clCfgValue);
		}
				
		ilRc = ReadConfigEntry(pclSelection,"Command",clCfgValue);
			
		if (ilRc != RC_SUCCESS)
		{
			/* Blank cgCommand and clear igXMLOModId */
				
			cgXMLOCommand[0] = 0x00;
			igXMLOModId = 0;
		}
		else
		{
			strcpy(cgXMLOCommand, clCfgValue);
		}
	} /* end if */
	
	/* Everything ready to write the file */
	
	GetServerTimeStamp("LOC",1,igTimeDiff,pclLocTime);
	sprintf(clSeqNo,"%s-%i",pclLocTime,lgXMLOSeqNo++);
	sprintf(clFileName,cgXMLOFileNameFormat,cgDestination,clSeqNo);
	
	FlFp = fopen(clFileName,"w");
	
	if (FlFp != NULL)
	{
		ilRc = fprintf(FlFp,"%s",cgData);
		
		if (ilRc < 0)
		{
			dbg(TRACE,"HandleXMLO: Error writing to file: <%s>",clFileName);
			fclose(FlFp);
			ilRc = RC_FAIL;
		}
		else
		{
			ilRc = RC_SUCCESS;
			
			dbg(TRACE,"HandleXMLO: XML written to file: <%s> \n",clFileName);
			
			if (igXMLOModId > 0)
			{
				strcpy(pclTwStart, "XMLO");
				sprintf(pclTwEnd,"%s,%s,%s", cgHopo, cgTabEnd, mod_name);
				strcpy(pclSelection,"LOCALPATH:");
				strcat(pclSelection,cgDestination);
				
        	ilRc = SendCedaEvent(igXMLOModId,0,mod_name,"CEDA",
                             	pclTwStart,pclTwEnd,
                             	cgXMLOCommand,"",pclSelection,
                             	"","","",3,NETOUT_NO_ACK);
           if (ilRc != RC_SUCCESS)
           {
           	dbg(TRACE,"HandleXMLO: Error sending: <%s> to modid: <%i>",cgXMLOCommand,igXMLOModId);
				}
			}
		}
	}
	else
	{
		dbg(TRACE,"HandleXMLO: Error opening file: <%s>",clFileName);
		ilRc = RC_FAIL;
	}
	
	fclose(FlFp);

	return ilRc;
} /* End of HandleXMLO() */	

/* For the AAS interface in BPN a HTML GET request is required. The interface   */
/* is triggered by a change in REMP and forwarded from the action handler.      */
/* All parameters will be sent with each GET request, so this function will do  */
/* a DB read each time a AASO command is received.                              */
/* The request format is configured in a text file and used as the              */
/* format string in a sprintf() call.                                           */
 
static int HandleAASO(void)
{
	int ilRc = RC_SUCCESS;
	
	int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char	pclSqlBuf[512];
  char	pclDataBuf[4096];
  char	pclTmpBuf[4096];
  char	*pclTmpPtr;
  int	ilIndex;
  char	clSelection[32];

	int	ilURNO;
	char	clREMP[16];
	char	clFLNO[16];
	char	clADID[8];
	char	clORG3[8];
	char	clDES3[8];
	char	clTIFA[16];
	char	clTIFD[16];
	char	clGTD1[16];
	char	clDIVR[16];
	char	clFLTI[8];
	char	clOldGate[16];
	char	clGETString[512];
	char	clFlightno[256];
	char	clScope[8];
	char	clAirport[64];
	char	clTime[8];
	int	ilFormat;
	
	
	/* Read the AAS fields from AFTTAB                                            */
	/* The fields are: REMP/FLNO/ADID/ORG3/DES3/TIFA/TIFD/GTD1/DIVR               */

  sprintf(pclSqlBuf,"SELECT REMP,FLNO,ADID,ORG3,DES3,TIFA,TIFD,GTD1,DIVR,FLTI FROM AFTTAB WHERE URNO = %i",igSaveURNO);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"HandleAASO: SQL = <%s>",pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  
  if (ilRCdb == DB_SUCCESS)
  {
  	/* Got the data, fill in the local variables */
  	
  	pclTmpBuf[0] =0x00;
  	BuildItemBuffer(pclDataBuf,"",10,",");
  	get_real_item(pclTmpBuf,pclDataBuf,1);
  	strcpy(clREMP,"ON_");
  	strcat(clREMP,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,2);
   	memset(clFLNO,0x00,sizeof(clFLNO));
   	strncpy(clFLNO,pclTmpBuf,3);
   	TrimRight(clFLNO);
   	strcat(clFLNO,"-");
   	pclTmpPtr = &pclTmpBuf[3];
   	ilIndex= strlen(clFLNO);
   	
   	while (*pclTmpPtr != 0x00)
   	{
   		if (*pclTmpPtr != ' ')
   		{
   			clFLNO[ilIndex] = *pclTmpPtr;
   			ilIndex++;
   		}
   		pclTmpPtr++;
   	}
   	clFLNO[ilIndex] = 0x00;
   	
   	get_real_item(pclTmpBuf,pclDataBuf,3);
   	strcpy(clADID,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,4);
   	strcpy(clORG3,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,5);
   	strcpy(clDES3,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,6);
   	strcpy(clTIFA,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,7);
   	strcpy(clTIFD,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,8);
   	strcpy(clGTD1,pclTmpBuf);
   	TrimRight(clGTD1);
   	get_real_item(pclTmpBuf,pclDataBuf,9);
   	strcpy(clDIVR,pclTmpBuf);
   	get_real_item(pclTmpBuf,pclDataBuf,10);
   	strcpy(clFLTI,pclTmpBuf);
   	
   	/* Get the possible codeshares and vias */
   	
   	GetJFNO_VIA("/","-","/");
   	
   	/* All data extracted, build the HTML GET request */
   	/* GET request is of the following format:        */
   	/* GET /index.htm HTTP/1.1                        */
   	/* host: www.esqsoft.globalservers.com            */
   	/* <line feed>                                    */
   	/* The /index.htm should be replaced with data in the following format      */
   	/* /announcer/?cmd=ON_NBD&flightno=GA-123&airport=SUB/BDJ&time=1250&gate=A1 */
   	
   	/* First check for Gate change. There we might not get a REMP code.         */
   	
   	ilRc = get_item_no(cgFields,"GTD1",5);
   	
   	if (ilRc >= 0)
   	{
   		/* We have a GTD1 in old data */
   		
   		get_real_item(pclTmpBuf,cgOldData,ilRc + 1);
   		TrimRight(pclTmpBuf);
   		if ((strstr(pclTmpBuf,clGTD1) == NULL) && pclTmpBuf[0] != ' ')
   		{
   			/* We have a gate change!! */
   			
   			strcpy(clREMP,"ON_GCH");
   			strcpy(clOldGate,pclTmpBuf);
   		}
		}
   	
   	/* Get the host name */
		
		if (strlen(pcgWebHost) == 0)
		{	
			ilRc = ReadConfigEntry("AASO","Hostname",pcgWebHost);
		
			if (ilRc != RC_SUCCESS)
			{
				dbg(TRACE,"HandleAASO: Hostname not configured. Terminating!!!");
				Terminate(60);
			}
			
			/* Get the port number (default is 80) */
			
			ilRc = ReadConfigEntry("AASO","Portnumber",pclTmpBuf);
			
			if (ilRc != RC_SUCCESS)
			{
				igWebPort = 80;
			}
			else
			{
				igWebPort = atoi(pclTmpBuf);
			}
			ilRc = RC_SUCCESS;
		}
		
		/* Set the mandatory fields */
		
		strcpy(clFlightno,clFLNO);
		
		if (strlen(pcgCodeSharesFLNO) > 0)
		{
			strcat(clFlightno,"/");
			strcat(clFlightno,pcgCodeSharesFLNO);
		}
		
		if (strcmp(clFLTI,"I") == 0)
		{
			strcpy(clScope,"int");
		}
		else
		{
			strcpy(clScope,"dom");
		}
		
		if (strcmp(clADID,"A") == 0)
		{
			strcpy(clAirport,clORG3);
			
			if (strlen(pcgVias) > 0)
			{
				strcat(clAirport,"/");
				strcat(clAirport,pcgVias);
			}
			
			AddSecondsToCEDATime(clTIFA,igTimeDiff,1);
			pclTmpPtr = &clTIFA[8];
			strncpy(clTime,pclTmpPtr,4);
		}
		else if (strcmp(clADID,"D") == 0)
		{
			strcpy(clAirport,clDES3);
			
			if (strlen(pcgVias) > 0)
			{
				strcat(clAirport,"/");
				strcat(clAirport,pcgVias);
			}
			
			AddSecondsToCEDATime(clTIFD,igTimeDiff,1);
			pclTmpPtr = &clTIFD[8];
			strncpy(clTime,pclTmpPtr,4);
		}
		else
		{
			dbg(TRACE,"HandleAASO: Invalid ADID, ignoring event. ADID = <%s>",clADID);
			return RC_FAIL;
		}
		
		if (igAASFormatsLoaded != 1)
		{
			for (ilIndex = 0 ; ilIndex < 5; ilIndex++)
			{
				pclTmpPtr = (char *) &cgAASCommands[ilIndex][0];
				*pclTmpPtr = 0x00;
				sprintf(clSelection,"Commands%i",ilIndex + 1);
				ReadConfigEntry("AASO",clSelection,pclTmpPtr);
				pclTmpPtr = (char *) &cgAASFormats[ilIndex][0];
				*pclTmpPtr = 0x00;
				sprintf(clSelection,"Format%i",ilIndex + 1);
				ReadConfigEntry("AASO",clSelection,pclTmpPtr);
			}
			igAASFormatsLoaded = 1;
		}
		
		ilFormat = 0;
		
		for (ilIndex = 0 ; ilIndex < 5; ilIndex++)
		{
			pclTmpPtr = (char *) &cgAASCommands[ilIndex][0];
			
			if (strstr(pclTmpPtr,clREMP) != NULL)
			{
				ilFormat = ilIndex + 1;
				pclTmpPtr = (char *) &cgAASFormats[ilIndex][0];				
				sprintf(pclTmpBuf,"GET %s HTTP/1.1\nhost: %s\n\n",pclTmpPtr,pcgWebHost);
				dbg(TRACE,"HandleAASO: GET format is: <%s>",pclTmpBuf);
				break;
			}
		}
		
		switch (ilFormat)
		{
			case 1:
				sprintf(clGETString,pclTmpBuf,clREMP,clFlightno,clAirport,clScope);
				break;
			case 2:
				sprintf(clGETString,pclTmpBuf,clREMP,clFlightno,clAirport,clScope,clGTD1);
				break;
			case 3:
				sprintf(clGETString,pclTmpBuf,clREMP,clFlightno,clAirport,clScope,clTime);
				break;
			case 4:
				sprintf(clGETString,pclTmpBuf,clREMP,clFlightno,clAirport,clScope,clTime,clDIVR);
				break;
			case 5:
				sprintf(clGETString,pclTmpBuf,clREMP,clFlightno,clAirport,clScope,clOldGate,clGTD1);
				break;
			default:
				dbg(TRACE,"HandleAASO: No format defined for command: <%s>",clREMP);
				return RC_FAIL;
				break;
		} /* end switch */
		
		/* We have a GET string. Send to the AAS host */
		
		dbg(TRACE,"HandleAASO: GET string is: <%s> ",clGETString);
		ilRc = SendGETRequest(clGETString);
	} /* end if */
	else
	{
		/* DB error */
		
		dbg(TRACE,"HandleAASO: DB error. Return code: <%i>",ilRCdb);
		ilRc = RC_FAIL;
	} /* end else */

	return ilRc;
} /* end of HandleAASO() */

/* The following routine sends a Keepalive message to AAS                       */

static int	HandleAASP(void)
{
	int ilRc = RC_SUCCESS;
  char	pclTmpBuf[1024];

	/* Make sure we have the hostname/port */
	  
	if (strlen(pcgWebHost) == 0)
	{	
		ilRc = ReadConfigEntry("AASO","Hostname",pcgWebHost);
		
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"HandleAASO: Hostname not configured. Terminating!!!");
			Terminate(60);
		}
			
		/* Get the port number (default is 80) */
			
		ilRc = ReadConfigEntry("AASO","Portnumber",pclTmpBuf);
			
		if (ilRc != RC_SUCCESS)
		{
			igWebPort = 80;
		}
		else
		{
			igWebPort = atoi(pclTmpBuf);
		}
		ilRc = RC_SUCCESS;
	}
  
  if (strlen(pcgAASPoll) == 0)
  {
		ilRc = ReadConfigEntry("AASO","Keepalive",pcgAASPoll);
			
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"HandleAASP: Keepalive command not configured!!");
			return RC_FAIL;
		}
	}
		
	sprintf(pclTmpBuf,"GET %s HTTP/1.1\nhost: %s\n\n",pcgAASPoll,pcgWebHost);
	ilRc = SendGETRequest(pclTmpBuf);

	return ilRc;
} /* end of HandleAASP() */

static int HandleCSVI(void)
{
	int	ilRc = RC_SUCCESS;
	int	ilLoop = 0;
	int	ilNbrElements = 0;
	char	clData[16384];
	char	*pclData = NULL;
	char	pclCSVIData[1024];
	char	pclTmpBuf[256];
	char	pclLimit[8];
	int	ilCCount;
  char	pclTwStart[32];
  char	pclTwEnd[64];
		
	/* First check that we have read the configuration */
	
	if (strlen(pcgCSVITab) == 0)
	{
		/* Read the configuration */
		
		ilRc = ReadConfigEntry("CSVI","Table",pcgCSVITab);
		
		if (ilRc != RC_SUCCESS)
		{
			strcpy(pcgCSVITab,"ACSTAB");
		}
		
		dbg(TRACE,"HandleCSVI: Building information table : <%s>",pcgCSVITab);

		ilRc = ReadConfigEntry("CSVI","Fields",pcgCSVIFld);
		
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"HandleCSVI: Field list not configured!! Terminating in 20 sec");
			Terminate(20);
		}
		
		dbg(TRACE,"HandleCSVI: Building information fields: \n<%s>",pcgCSVIFld);
		igNbrCSVIFields = GetNoOfElements(pcgCSVIFld,',');

		ilRc = ReadConfigEntry("CSVI","Lengths",pcgCSVILen);
		
		if (ilRc == RC_SUCCESS)
		{
			/* We have to check that Fld/Len have the same number of elements */
			
			ilNbrElements = GetNoOfElements(pcgCSVILen,',');
			
			if (igNbrCSVIFields != ilNbrElements)
			{
				dbg(TRACE,"HandleCSVI: Field/Length mismatch!! Terminating in 20 sec");
				Terminate(20);
			}
			
			dbg(TRACE,"HandleCSVI: Building information field lengths: \n<%s>",pcgCSVILen);
		}
		else
		{
			dbg(TRACE,"HandleCSVI: (INFO) No building information field lengths configured");
		}
		
		ilRc = ReadConfigEntry("CSVI", "Destination", pclTmpBuf);
		
		if (ilRc == RC_SUCCESS)
		{
			igDestination = atoi(pclTmpBuf);
		}
		else
		{
			dbg(TRACE,"HandleCSVI: Destination not configured!! Terminating in 20 sec");
			Terminate(20);
		}
		
		ilRc = ReadConfigEntry("CSVI", "Command", pcgSendCommand);

		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"HandleCSVI: Command not configured!! Terminating in 20 sec");
			Terminate(20);
		}

	} /* end of configuration check */
		
	/* Copy cgData to a local variable because we need to work with strtok */
	
	strcpy(clData,cgData);
	
	/* No need to consider cr/nl (DOS format) because whitespaces are trimmed away */
	pclData = strtok(clData,"\n");
	
	/* Now build the data buffer(s) with possible trimming */
	
	while (pclData != NULL)
	{
		/* Check if nbr of elements in the received data is consistent with the configuration */
	
		ilNbrElements = GetNoOfElements(pclData,',');
	
		if (igNbrCSVIFields != ilNbrElements)
		{
			dbg(TRACE,"HandleCSVI: Nbr of element mismatch (data/configuration). Skipping!! \n<%s>",pclData);
			
			/* Could be a single bad line, so try to get the next line */
			pclData = strtok(NULL,"\n");
			continue;
		}

		memset(pclCSVIData,0x00,sizeof(pclCSVIData));
	
		get_real_item(pclTmpBuf,pclData,1);
		TrimLeftRight(pclTmpBuf);
		strcpy(pclCSVIData,pclTmpBuf);

		for (ilLoop = 2; ilLoop <= igNbrCSVIFields; ilLoop++)
		{
			get_real_item(pclTmpBuf,pclData,ilLoop);
			TrimLeftRight(pclTmpBuf);
			strcat(pclCSVIData,",");
		
			if (strlen(pcgCSVILen) > 0)
			{
				get_real_item(pclLimit,pcgCSVILen,ilLoop);
				ilCCount = atoi(pclLimit);
				strncat(pclCSVIData,pclTmpBuf,ilCCount);
			}
			else
			{
				strcat(pclCSVIData,pclTmpBuf);
			}
		} /*end for */
	
		/* Ready.. Send to destination */
	
		strcpy(pclTwStart, "CSVI");
		sprintf(pclTwEnd,"%s,%s,%s", cgHopo, cgTabEnd, mod_name);

		ilRc = tools_send_info_flag(igDestination,0,mod_name,"","CEDA","","",pclTwStart,pclTwEnd,
											pcgSendCommand,pcgCSVITab,"",pcgCSVIFld,pclCSVIData,0);
		if (ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"HandleCSVI: Tools_send to <%i> failed",igDestination);
		}
		
		/* Read the next event line */
		
		pclData = strtok(NULL,"\n");
	} /* end while */		
	return ilRc;
} /* end of HandleCSVI() */


/* This routine extracts the FLNO from JFNO and VIAs from VIAL                  */
/* It takes 3 parameters, which are all separators:                             */
/*                                                                              */
/* pcpSep1 is the separator  between FLNOs (e.g. "/")                            */
/* pcpSep2 is the separator  between ALC and the flight number (e.g. "-")        */
/* pcpSep3 is the separator between the VIAs (e.g. "/")                         */

static int GetJFNO_VIA(char *pcpSep1, char *pcpSep2, char *pcpSep3)
{
	int	ilRCdb = DB_SUCCESS;
  short	slFkt;
  short	slCursor;
  char	pclSqlBuf[512];
  char	pclDataBuf[4096];
  char	pclTmpBuf[4096];
  int	ilCnt = 0;
  int	ilLc = 0;
  char	pclTemp[16];
  char	*pclWorkPtr;
  
  memset(pcgCodeSharesFLNO,0x00,256);
  memset(pcgVias,0x00,64);

  sprintf(pclSqlBuf,"SELECT JCNT,JFNO,VIAN,VIAL FROM AFTTAB WHERE URNO = %i",igSaveURNO);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"GetJFNO_VIA: SQL = <%s>",pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  
  if (ilRCdb == DB_SUCCESS)
  {
  	/* We got data, build the 'pcpSep1' separated lists */
  	
  	pclTmpBuf[0] =0x00;
  	BuildItemBuffer(pclDataBuf,"",4,",");
  	get_real_item(pclTmpBuf,pclDataBuf,1);
  	ilCnt = atoi(pclTmpBuf);
  	
  	if (ilCnt > 0)
  	{
  		get_real_item(pclTmpBuf,pclDataBuf,2);
  		dbg(DEBUG,"GetJFNO_VIA: JFNO = <%s>",pclTmpBuf);
  		
  		for (ilLc = 0 ; ilLc < ilCnt; ilLc++)
  		{
  			pclWorkPtr = pclTmpBuf + (ilLc * 9);
  			strncpy(pclTemp,pclWorkPtr,3);
  			TrimRight(pclTemp);
  			pclWorkPtr += 3;
  			
  			if (pclTemp[0] == ' ')
  			{
  				/* A blank Airline code.. Break out of loop */
  				break;
  			}
  			if (ilLc != 0)
  			{
  				strcat(pcgCodeSharesFLNO,pcpSep1);
  			}
  			strcat(pcgCodeSharesFLNO,pclTemp);
  			strcat(pcgCodeSharesFLNO,pcpSep2);
  			strncpy(pclTemp,pclWorkPtr,6);
  			TrimRight(pclTemp);
  			strcat(pcgCodeSharesFLNO,pclTemp);
  		}
  	}
 
  	pclTmpBuf[0] =0x00;
  	get_real_item(pclTmpBuf,pclDataBuf,3);
  	ilCnt = atoi(pclTmpBuf);
  	
  	if (ilCnt > 0)
  	{
  		get_real_item(pclTmpBuf,pclDataBuf,4);
  		dbg(DEBUG,"GetJFNO_VIA: VIAL = <%s>",pclTmpBuf);
  		
  		for (ilLc = 0 ; ilLc < ilCnt; ilLc++)
  		{
  			if (ilLc != 0)
  			{
  				strcat(pcgVias,pcpSep3);
  			}
  			
  			pclWorkPtr = pclTmpBuf + (ilLc * 120) + 1;
  			strncpy(pclTemp,pclWorkPtr,3);
  			strcat(pcgVias,pclTemp);
  		}
  	}
  }
  
  return ilRCdb;
} /* End of GetJFNO_VIA() */

/* The following routine sends a GET request to a Web service */
/* A new connection is built for every request to avoid dead  */
/* sessions, that could take long time to time out.           */

static int SendGETRequest(char *pcpGETRequest)
{
	int 	ilRc = RC_SUCCESS;
	char	clResponseBuff[4096];
	
	/* First check if we have a connection */
	
	if (igAASSock == 0)
	{
		igAASConnected = 0;
		ilRc = Create_Socket(SOCK_STREAM, NULL);

		if (ilRc == RC_FAIL)
		{
			dbg(TRACE,"SendGETRequest: Failed to open socket");
			return RC_FAIL;
		}
		else
		{
			igAASSock = ilRc;
		}
	}
	
	if (igAASConnected == 0)
	{
		ilRc = Open_Connection(igAASSock,pcgWebHost,igWebPort);
		
		if (ilRc == RC_FAIL)
		{
			close(igAASSock);
			igAASSock = 0;
			dbg(TRACE,"SendGETRequest: Failed to open connection to <%s>",pcgWebHost);
			return RC_FAIL;
		}
		else
		{
			igAASConnected = 1;
		}
	}
	
	/* We have a connection, send the GET request */
	
	dbg(TRACE,"SendGETRequest: The GET request buffer: \n<%s>",pcpGETRequest);
	
	ilRc = write(igAASSock,pcpGETRequest, strlen(pcpGETRequest));
	
	if (ilRc < 0 )
	{
		dbg(TRACE,"SendGETRequest: ERROR %d : %s ",ilRc,strerror(errno));
		
		/* Write failed, shutdown and close the socket */
		
		shutdown(igAASSock,2);
		close(igAASSock);
		igAASConnected = 0;
		igAASSock = 0;
		return RC_FAIL;
	}
	
	/* Data was written, now wait (max 2 sec) for the response (after sleeping 1 sec) */
	
	memset(clResponseBuff,0x00,sizeof(clResponseBuff));
	sleep(1);
	
	alarm(2);	
	ilRc = read(igAASSock, clResponseBuff, sizeof(clResponseBuff));
	alarm(0);
	dbg(DEBUG,"SendGETRequest: Response from AAS: \n <%s>",clResponseBuff);
			
	shutdown(igAASSock,2);
	close(igAASSock);
	igAASConnected = 0;
	igAASSock = 0;
	
	return RC_SUCCESS;
} /* End of SendGETRequest() */

static int Create_Socket (int type, char *Pservice)
{
	int		ilRC;
	int		rc = RC_SUCCESS;	/* Return code */
  int		sock = 0;
  int		ilKeepalive = 0;
  size_t	ilRecLen = 0;
  struct 	servent 	*sp;
  struct	sockaddr_in	name;

  int opt_bool=1;	/* Arg for setsockopt */
  struct linger ling;	/* Arg for setsockopt */

  sock = socket(AF_INET, type, 0); /* create socket */

  if (sock < 0 )
  {
		dbg(TRACE,"Create_Socket: Error can't open socket: %s", strerror( errno));
		rc = RC_FAIL;
  }
	else
	{
		dbg(DEBUG,"Create_Socket: Socket %d opened", sock);
		if (Pservice != NULL)
		{
			if((sp = getservbyname(Pservice, NULL) ) == NULL )
			{
				dbg(TRACE,"Create_Socket: unknown service %s", Pservice); 
				rc = RC_FAIL;
			}
		}
  }

  if(rc == RC_SUCCESS)
	{
		ilKeepalive = 0;

#if defined(_UNIXWARE) || defined(_SOLARIS) || defined(_LINUX) || defined(_HPUX113_SOURCE)
		ilRecLen = sizeof(int);
#endif

		rc = getsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &ilKeepalive, &ilRecLen);
		if (rc == -1 )
		{
			rc = RC_FAIL;
			dbg(TRACE,"Create_Socket: getsockopt <%s>", strerror(errno));
		}
		else
		{
			if(ilKeepalive == 1)
			{
				dbg(DEBUG,"Create_Socket: SO_KEEPALIVE already set");
			}
			else
			{
				ilKeepalive = 1;
				rc = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *) &ilKeepalive, sizeof(ilKeepalive));
				if (rc == -1 )
				{
					rc = RC_FAIL;
					dbg(TRACE,"Create_Socket: setsockopt <%s>", strerror(errno));
				}
				else
				{
					dbg(DEBUG,"Create_Socket: SO_KEEPALIVE set");
				}/* end of if */
			}/* end of if */
		}/* end of if */
	}/* end of if */


  if (rc == RC_SUCCESS)
  {
		rc = setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (char *) &opt_bool, sizeof (opt_bool));
		if (rc == -1 )
		{	
			dbg(TRACE,"Create_Socket: Error Setsockopt REUSEADDR: %s", strerror(errno));
			rc = RC_FAIL;
		}
  }


  if (rc == RC_SUCCESS)
  {
		ling.l_onoff = 0;
		ling.l_linger = 0;

		rc = setsockopt (sock, SOL_SOCKET, SO_LINGER, (char *) &ling, sizeof (ling));
		if (rc == -1 )
		{	
			dbg(TRACE,"Create_Socket: Error Setsockopt LINGER: %s", strerror(errno));
			rc = RC_FAIL;
		}
	}

  if (Pservice != NULL && rc == RC_SUCCESS)
  {
		dbg(DEBUG,"Create_Socket: got services %s", Pservice); 
  
		name.sin_addr.s_addr = INADDR_ANY;
		name.sin_family = AF_INET;
		name.sin_port   = (u_short) sp->s_port;
  
		errno = 0;
		while ((rc = bind(sock, (struct sockaddr *) &name, sizeof(struct sockaddr_in))) < 0)
		{
			dbg(DEBUG,"Create_Socket: bind returns: %d - sleep 10 seconds now -> <%s>", rc, strerror(errno));
			sleep(10);
		}
	}

  if (rc < 0) /* check if there was a recent error */
	{
		close(sock); /* close the socket to prevent an unlimited file opening! */
	}

  if (rc == RC_SUCCESS)
  {
		dbg(DEBUG,"Create_Socket: return sock %d",sock);
		return sock;
  }
  else
  {
		dbg(DEBUG,"Create_Socket: return RC_FAIL");
		return RC_FAIL;
  }

} /* end of Create_Socket() */

static int Open_Connection (int socket, char *pcpHostIP, int ipPort)
{
  int		rc=RC_FAIL;
  int		i = 0;
  struct	sockaddr_in	dummy;
  int		serr=0;

	dbg(DEBUG,"Open_Connection: got IP: <%s>, Port: <%i>", pcpHostIP,ipPort);

	dummy.sin_port = htons((unsigned short) ipPort);
	inet_pton(AF_INET, pcpHostIP, &dummy.sin_addr);
	dummy.sin_family = AF_INET;

	rc = connect(socket, (struct sockaddr *) &dummy, sizeof(dummy));
	if (rc < 0)
  {
		dbg(TRACE,"Open_Connection: ERROR while connect: %s",
				strerror(errno));
		return RC_FAIL;
	}

	dbg(DEBUG,"Open_Connection: got connection"); 
	return RC_SUCCESS;                                         

} /* end of Open_Connection */

static void TrimRight(char *pcpBuffer)
{
  char *pclBlank = &pcpBuffer[strlen(pcpBuffer)-1];

  if (strlen(pcpBuffer) == 0)
  {
     strcpy(pcpBuffer, " ");
  }
  else
  {
     while (isspace(*pclBlank) && pclBlank != pcpBuffer)
     {
        *pclBlank = '\0';
        pclBlank--;
     }
  }
} /* End of TrimRight */

static void TrimLeftRight(char *pcpBuffer)
{
  char *pclBlank = &pcpBuffer[0];

  if (strlen(pcpBuffer) == 0)
  {
     strcpy(pcpBuffer, " ");
  }
  else
  {
     while (isspace(*pclBlank) && *pclBlank != '\0')
     {
        pclBlank++;
     }
     strcpy(pcpBuffer,pclBlank);
     TrimRight(pcpBuffer);
  }
} /* End of TrimLeftRight */

