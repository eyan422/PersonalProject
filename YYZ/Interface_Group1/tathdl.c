
#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/skeleton.c 1.3 2003/11/18 19:43:14SGT heb Exp  $";
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

/*fya 0.1*/
RULE rgRule;

/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
static int  Init_Process();
static int ReadConfigEntry(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer);
static int	Reset(void);                       /* Reset program          */
static void	Terminate(int ipSleep);            /* Terminate program      */
static void	HandleSignal(int);                 /* Handles signals        */
static void	HandleErr(int);                    /* Handles general errors */
static void   HandleQueErr(int);                 /* Handles queuing errors */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

static int	HandleData(EVENT *prpEvent);       /* Handles event data     */
static int  GetCommand(char *pcpCommand, int *pipCmd);

/*fya 0.1*/
static int GetConfig();
static int GetRuleSchema(RULE *rpRule);
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
    if (ilRc = GetConfig() == RC_FAIL)
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
	int      ilCmd          = 0;

	BC_HEAD *prlBchead       = NULL;
	CMDBLK  *prlCmdblk       = NULL;
	char    *pclSelection    = NULL;
	char    *pclFields       = NULL;
	char    *pclData         = NULL;
	char    *pclRow          = NULL;
	char 		clUrnoList[2400];
	char 		clTable[34];
	int			ilUpdPoolJob = TRUE;

	prlBchead    = (BC_HEAD *) ((char *)prpEvent + sizeof(EVENT));
	prlCmdblk    = (CMDBLK *)  ((char *)prlBchead->data);
	pclSelection = prlCmdblk->data;
	pclFields    = pclSelection + strlen(pclSelection) + 1;
	pclData      = pclFields + strlen(pclFields) + 1;


	strcpy(clTable,prlCmdblk->obj_name);

	dbg(TRACE,"========== START <%10.10d> ==========",lgEvtCnt);

	/****************************************/
	DebugPrintBchead(TRACE,prlBchead);
	DebugPrintCmdblk(TRACE,prlCmdblk);
	/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command"); */
	dbg(TRACE,"Command: <%s>",prlCmdblk->command);
	dbg(TRACE,"originator follows event = %p ",prpEvent);

	dbg(TRACE,"originator<%d>",prpEvent->originator);
	dbg(TRACE,"selection follows Selection = %p ",pclSelection);
	dbg(TRACE,"selection <%s>",pclSelection);
	dbg(TRACE,"fields    <%s>",pclFields);
	dbg(TRACE,"data      <%s>",pclData);
	/****************************************/
	dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);

	return(RC_SUCCESS);

} /* end of HandleData */

/* fya 0.1***************************************************************/
/* Following the GetConfig function										*/
/* Reads the .cfg file and fill in the configuration structure			*/
/* ******************************************************************** */
static int GetConfig()
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
    char *pclFunc = "GetConfig";
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

    /* Get Client and Server chars */
    sprintf(pcgUfisConfigFile,"%s/ufis_ceda.cfg",getenv("CFG_PATH"));
    ilRC = iGetConfigEntry(pcgUfisConfigFile,"CHAR_PATCH","CLIENT_CHARS",
                CFG_STRING, pclClieBuffer);
    if (ilRC == RC_SUCCESS)
    {
        ilRC = iGetConfigEntry(pcgUfisConfigFile,"CHAR_PATCH","SERVER_CHARS",
                CFG_STRING, pclServBuffer);
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

    ilRC = GetRuleSchema();

    return RC_SUCCESS;
} /* End of GetConfig() */

/* fya 0.1***************************************************************/
/* Following the GetRuleSchema function										*/
/* Reads the .csv file and fill in the configuration structure			*/
/* ******************************************************************** */
static int GetRuleSchema(RULE *rpRule);
{
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "GetRuleSchema:";
    char pclCfgFile[128];
    FILE *fp;
    int ilCurLine;
    char pclLine[2048];
    int ilNoEle;
    int ilI;
    int ilCurLevel;
    char pclTag[16];
    char pclPrevTag[16];
    char pclName[32];
    //char pclType[8]; //Frank v1.52 20121227
    char pclType[32];

    char pclBasTab[8];
    char pclBasFld[8];
    char pclFlag[64];
    int ilInfoFldFlag;
    char pclNameStack[100][1024];
    char pclItem1[32];
    char pclItem2[32];
    char pclItem3[32];
    int ilJ;
    int ilNoMethod;
    char pclTmpBuf[128];

    ilRC = iGetConfigEntry(pcgConfigFile, "MAIN", "RULE_SCHEMA_FILE", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS)
        sprintf(pclCfgFile, "%s/%s", getenv("CFG_PATH"), pclTmpBuf);
    else
        sprintf(pclCfgFile, "%s/%s.csv", getenv("CFG_PATH"), mod_name);

    dbg(TRACE, "%s Rule Config File is <%s>", pclFunc, pclCfgFile);

    if ((fp = (FILE *) fopen(pclCfgFile, "r")) == (FILE *) NULL) {
        dbg(TRACE, "%s Rule Config File <%s> does not exist", pclFunc, pclCfgFile);
        return RC_FAIL;
    }

    igCurInterface = 0;
    igUsingMultiTypeFlag = FALSE;
    memset((char *) &rgXml.rlLine[0].ilLevel, 0x00, sizeof (_LINE) * MAX_XML_LINES);
    strcpy(pclPrevTag, "");
    ilInfoFldFlag = FALSE;
    igCurXmlLines = 0;
    ilCurLevel = 0;
    ilCurLine = 1;

    while (fgets(pclLine, 2048, fp))
    {
        pclLine[strlen(pclLine) - 1] = '\0';
        if (pclLine[strlen(pclLine) - 1] == '\012' || pclLine[strlen(pclLine) - 1] == '\015')
            pclLine[strlen(pclLine) - 1] = '\0';
        ilNoEle = GetNoOfElements(pclLine, ';');
        dbg(DEBUG, "%s Current Line = (%d)<%s>", pclFunc, ilNoEle, pclLine);

        get_item(ilNoEle, pclLine, pclTag, 0, ";", "\0", "\0");
        get_item(1, pclLine, pclTag, 0, ";", "\0", "\0");
        TrimRight(pclTag);
        get_item(2, pclLine, pclName, 0, ";", "\0", "\0");
        TrimRight(pclName);

        //Frank v1.52 20121227
        //dbg(DEBUG,"%s %d pclName<%s>",pclFunc,__LINE__,pclName);

        get_item(3, pclLine, pclType, 0, ";", "\0", "\0");
        TrimRight(pclType);
        get_item(4, pclLine, pclBasTab, 0, ";", "\0", "\0");
        TrimRight(pclBasTab);
        get_item(5, pclLine, pclBasFld, 0, ";", "\0", "\0");
        TrimRight(pclBasFld);
        get_item(6, pclLine, pclFlag, 0, ";", "\0", "\0");
        TrimRight(pclFlag);
        if (strcmp(pclTag, "TITLE") != 0) {
            if (strcmp(pclTag, "STR_BGN") == 0) {
                if (strcmp(pclPrevTag, "STR_END") != 0)
                    ilCurLevel++;
                if (ilInfoFldFlag == TRUE)
                    ilCurLevel--;
                ilInfoFldFlag = FALSE;
                strcpy(&pclNameStack[ilCurLevel - 1][0], pclName);
            } else {
                if (strcmp(pclTag, "STR_END") == 0) {
                    ilCurLevel--;
                    ilInfoFldFlag = FALSE;
                    if (strcmp(pclName, &pclNameStack[ilCurLevel - 1][0]) != 0) {
                        dbg(TRACE, "%s Wrong Name at STR_END <%s><%s> specified!!! Line = %d",
                                pclFunc, pclName, &pclNameStack[ilCurLevel - 1][0], ilCurLine);
                        return ilRC;
                    }
                } else {
                    if (strcmp(pclTag, "HDR") == 0) {
                        if (ilInfoFldFlag == FALSE) {
                            ilCurLevel++;
                            ilInfoFldFlag = TRUE;
                        }
                    } else {
                        if (strcmp(pclTag, "KEY") == 0) {
                            if (strcmp(pclPrevTag, "STR_END") == 0)
                                ilCurLevel--;
                            if (ilInfoFldFlag == FALSE) {
                                ilCurLevel++;
                                ilInfoFldFlag = TRUE;
                            }
                        } else {
                            if (strcmp(pclTag, "DAT") == 0) {
                                if (strcmp(pclPrevTag, "STR_END") == 0)
                                    ilCurLevel--;
                                if (ilInfoFldFlag == FALSE) {
                                    ilCurLevel++;
                                    ilInfoFldFlag = TRUE;
                                }
                            } else {
                                dbg(TRACE, "%s Wrong Tag <%s> specified!!!", pclFunc, pclTag);
                                return ilRC;
                            }
                        }
                    }
                }
            }
            rgXml.rlLine[igCurXmlLines].ilLevel = ilCurLevel;
            strcpy(&rgXml.rlLine[igCurXmlLines].pclTag[0], pclTag);

            strcpy(&rgXml.rlLine[igCurXmlLines].pclName[0], pclName);
            //Frank v1.52 20121227
            //dbg(DEBUG,"%s %d pclName<%s>&rgXml.rlLine[igCurXmlLines].pclName[0]<%s>",pclFunc,__LINE__,pclName,&rgXml.rlLine[igCurXmlLines].pclName[0]);

            strcpy(&rgXml.rlLine[igCurXmlLines].pclType[0], pclType);
            if (strcmp(pclType, "MULTI") == 0) {
                igUsingMultiTypeFlag = TRUE;
            }
            if (strcmp(pcgMode, "OUTPUT") == 0 && strcmp(pclTag, "STR_BGN") != 0 && strcmp(pclTag, "STR_END") != 0) {
                if (*pclBasTab == ' ')
                    strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0], pclName);
                else
                    strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0], pclBasTab);
                if (*pclBasFld == ' ')
                    strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0], pclName);
                else
                    strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0], pclBasFld);
            } else {
                strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0], pclBasTab);
                strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0], pclBasFld);
            }
            strcpy(&rgXml.rlLine[igCurXmlLines].pclFlag[0], pclFlag);
            ilNoEle = GetNoOfElements(pclLine, ';');
            for (ilI = 7; ilI <= ilNoEle; ilI += 3) {
                get_item(ilI, pclLine, pclItem1, 0, ";", "\0", "\0");
                TrimRight(pclItem1);
                get_item(ilI + 1, pclLine, pclItem2, 0, ";", "\0", "\0");
                TrimRight(pclItem2);
                get_item(ilI + 2, pclLine, pclItem3, 0, ";", "\0", "\0");
                TrimRight(pclItem3);
                if (strcmp(pclItem1, "{END}") != 0) {
                    if (*pclItem1 != ' ') {
                        if (strcmp(&rgXml.rlLine[igCurXmlLines].pclTag[0], "HDR") == 0 &&
                                strcmp(&rgXml.rlLine[igCurXmlLines].pclName[0], pcgMessageOrigin) == 0) {
                            strcpy(&rgInterface[igCurInterface].pclIntfName[0], pclItem1);
                            rgInterface[igCurInterface].ilIntfIdx = (ilI - 7) / 3;
                            igCurInterface++;
                        } else {
                            strcpy(&rgXml.rlLine[igCurXmlLines].pclMethod[(ilI - 7) / 3][0], pclItem1);
                            rgXml.rlLine[igCurXmlLines].ilNoMethod++;
                        }
                    }
                    if (*pclItem2 != ' ') {
                        strcpy(&rgXml.rlLine[igCurXmlLines].pclFieldA[(ilI - 7) / 3][0], pclItem2);
                        rgXml.rlLine[igCurXmlLines].ilNoFieldA++;
                    }
                    if (*pclItem3 != ' ') {
                        strcpy(&rgXml.rlLine[igCurXmlLines].pclFieldD[(ilI - 7) / 3][0], pclItem3);
                        rgXml.rlLine[igCurXmlLines].ilNoFieldD++;
                    }
                }
            }
            igCurXmlLines++;
            ilCurLine++;
            strcpy(pclPrevTag, pclTag);
        }
    }
    fclose(fp);

    dbg(TRACE, "---------------------------------------------------------");

    return ilRC;
} /* End of GetXMLSchema */

void showRule(RULE *rpRule, int ilNumOfRule)
{
    char pclFunc[] = "showRule:";

    for (ilI = 0; ilI < ilNumOfRule; ilI++)
    {
        dbg(DEBUG, "%s %s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", pclFunc,
        &rpRule.rlLine[ilI].pclActive,
        &rpRule.rlLine[ilI].pclRuleGroup,
        &rpRule.rlLine[ilI].pclSourceTable,
        &rpRule.rlLine[ilI].pclSourceKey,
        &rpRule.rlLine[ilI].pclSourceField,
        &rpRule.rlLine[ilI].pclSourceFieldType,
        &rpRule.rlLine[ilI].pclDestTable,
        &rpRule.rlLine[ilI].pclDestKey,
        &rpRule.rlLine[ilI].pclDestField,
        &rpRule.rlLine[ilI].pclDestFieldLen,
        &rpRule.rlLine[ilI].pclDestFieldType,
        &rpRule.rlLine[ilI].pclDestFieldOperator,
        &rpRule.rlLine[ilI].pclCond1,
        &rpRule.rlLine[ilI].pclCond2);
    }
}
