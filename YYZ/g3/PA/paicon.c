#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/paicon.c 0.3 2014/08/19 2:27:07 PM Exp  $";
#endif /* _DEF_mks_version */

/******************************************************************************/
/*                                                                            */
/* UFIS AS HWECON.C                                                           */
/*                                                                            */
/* Author         : Yan Feng                                         */
/* Date           : Aug 2014                                            */
/* Description    : Process to receive and send messages from/to FAS */
/*                                                                            */
/* Update history :                                                           */
/*                                                                            */
/* 20140819 FYA: The code was from hwecon.c which is modified for YYZ group3 PA interface
					1)The sent heartbeat message format is updated
					2)The received heartbeat message format is updated
					3)tcp->udp
					*/
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */
static char sccs_hwecon[]="%Z% UFIS 4.5 (c) ABB AAT/I %M% %I% / %E% %U% / AKL";
/* be carefule with strftime or similar functions !!!                         */
/*                                                                            */
/******************************************************************************/
/* This program is a MIKE main program */
#define U_MAIN
#define UGCCS_PRG
#define STH_USE

#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include "debugrec.h"
#include "hsbsub.h"
#include "tools.h"
#include "helpful.h"
#include "timdef.h"
#include "goshdl.h"

#define XS_BUFF  128
#define S_BUFF   512
#define M_BUFF   1024
#define L_BUFF   2048
#define XL_BUFF  4096
#define XXL_BUFF 8192

#define BUFF 10000000

#define RC_ACKTIMEOUT  2
#define RC_RECVTIMEOUT 3
#define RC_SENDTIMEOUT 4

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
FILE *outp       = NULL;
int  debug_level = TRACE;
/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int	SetSignals(void (*HandleSignal)(int));
extern int	DebugPrintItem(int,ITEM *);
extern int	DebugPrintEvent(int,EVENT *);
extern void	snap(char*,int,FILE*);
extern void	HandleRemoteDB(EVENT*);
extern int	GetDataItem(char *pcpResult, char *pcpInput, int ipNum, char cpDel,char *pcpDef,char *pcpTrim);
extern int	GetNoOfElements(char *s, char c);
extern long	nap(long);

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM	*prgItem		= NULL;			/* The queue item pointer  */
static EVENT *prgEvent		= NULL;			/* The event pointer       */
static int	igItemLen		= 0;			/* length of incoming item */
static int	igInitOK		= FALSE;		/* Flag for init */

static char	pcgConfFile[S_BUFF];			/* buffer for config-file name */
static int	igModID_Rcvr  = 7920;			/* MOD-ID of Receiver  */

static char	pcgHomeAp[XS_BUFF];				/* buffer for home airport */
static char	pcgTabEnd[XS_BUFF];				/* buffer for TABEND */
static char	pcgTwStart[XS_BUFF] = "";
static char	pcgTwEnd[XS_BUFF];

static unsigned int igMsgNoForHeartbeat = 0;
static unsigned int igMsgNoForACK = 0;
static unsigned int igMsgNo = 0;
static char pcgMsgNoForACK[10] = "\0";
static char pcgIP[64];
static char pcgPort[64];
static fd_set readfds,writefds,exceptfds;

/*entry's from configfile*/
static char	pcgConfigFile[512];
static char	pcgCfgBuffer[512];
static EVENT *prgOutEvent = NULL;
static char	pcgServerChars[110];
static char	pcgClientChars[110];
static char	pcgUfisConfigFile[512];
static char pcgKeepAliveFormat[M_BUFF];
static char pcgSendAckFormat[M_BUFF];

#ifndef _SOLARIS
static struct sockaddr_in my_client;
#else
static struct sockaddr my_client;
#endif

//static BOOL	bgSocketOpen = FALSE;		/* global flag for connection state */
static BOOL	bgAlarm = FALSE;				/* global flag for time-out socket-read */
static int	igSock = 0;							/* global tcp/ip socket for send/receive */

//static int	igRecvTimeout = 0;			/* number of secs to wait for a valid telegram */
//static int	igWaitForAck;						/* Time to wait for an ACK */
//static int	igMaxSends;							/* Max retry count (send) */
static int	igKeepAlive;						/* The Keep Alive interval (0 = no Keep Alive) */
static int	igSendAck;							/* Flag for sending ACKs (0 or 1) */
//static BOOL bgRemoveSuffix = FALSE;	/* Flag to remove suffix from stand numbers. DXB specific */
//static BOOL	bgRemoveZero = FALSE;		/* Flag to remove zeros from stand numbers. DXB specific */
//static int	igStandTagLen;					/* Length of the stand tag. DXB specific */
static CFG	prgCfg;
//static char	pcgGOS_Host[64];				/* buffer for the host that is connected */
//static char	pcgRecvBuffer[M_BUFF];	/* global buffer for socket-read */
//static char	pcgSendBuffer[M_BUFF];	/* global buffer for socket-write */
//static int	igHeaderSize;
static FILE	*pgReceiveLogFile = NULL;	/* log file pointer for all received messages */
static FILE	*pgSendLogFile = NULL;		/* log file pointer for all sent messages */

//Frank 20130107
static int igConnectionNo = 0;

static int	igReconIntv = 15;           /* global socket Reconnect interval Time (Second) */
static int	igReconMax = 3;             /* global socket try Max Times per Reconnection*/
static int	igHeartBeatIntv = 5;        /* global socket heartbeat interval Time (second) */
static int	igHeartBeatTimOut = 15;     /* global socket heartbeat TimeOut Time(second) */
static int	igSendAckEnable = TRUE;     /* Send Ack to server Flag */

static int	igReSendMax = 3;           /* global socket resend Max Times */
static int  igSckTryACKCMax = 3;
static int  igSckACKCWait = 3;
static int	igSckWaitACK = FALSE;       /* global socket ACK status from Server wait for ACK or not*/
static int	igSckTryACKCnt = 0;         /* global socket Wait ACK receive retry Count  */
static char	pcgSckWaitACKExpTime[64] = "\0";  /* global Send heartbeat expect time */

static char	pcgCurSendData[4096] = "\0"; /* global CurSendData */
static char	pcgSendHeartBeatExpTime[64] = "\0";  /* global Send heartbeat expect time */
static char	pcgRcvHeartBeatExpTime[64] = "\0";   /* global Receive heartbeat expire time */
static char	pcgReconExpTime[64] = "\0";   /* global Receive heartbeat expect time */

static char *pcgConnType = NULL;

static char	pcgSendMsgId[16] = "\0";

static int	igModID_ConMgr = 0;
static int	igModID_ConSTB = 0;

static int	igMgrDly = 50;
static int	igConDly = 50;

static int	igConnected = FALSE;   /* global socket Connect status  */
static int	igOldCnnt = FALSE;

/*fya paicon 0.1*/
static int igObjID = 0;
static int igMsgID = 0;
static char * pcgHB_Header = NULL;

static int	Init_Handler();
static int	Reset(void);                        /* Reset program          */
static void	Terminate(int);                     /* Terminate program      */
static void	HandleSignal(int);                  /* Handles signals        */
static void	HandleErr(int *);                     /* Handles general errors */
static void	HandleQueErr(int);                  /* Handles queuing errors */
static int	HandleInternalData(void);           /* Handles event data     */
static void	HandleQueues(void);                 /* Waiting for Sts.-switch*/

/******************************************************************************/
/* Function prototypes														  */
/******************************************************************************/

/*FYA paicon 0.1*/
static void buildHB_message(char *pcpDataBuf,char *pcpKeepAliveFormat, char *pcpHB_Flag,int ipObjID, int ipMsgID,int ipMsgNoForHeartbeat);

static int	GetQueues();
static int	GetConfig();
static void	CloseTCP(void);
static int	CheckData(char *pcpData, int ipIdx);
static int	GetData(char *pcpMethod, int ipIdx, char *pcpName);
static int	poll_q_and_sock();
static int	Receive_data(int ipSock, int ipAlarm);
static int	Send_data(int ipSock, char *pcpData);
static void	snapit(void *pcpBuffer, long lpDataLen, FILE *pcpDbgFile);
static int	GetCfgEntry(char *pcpFile,char *pcpSection,char *pcpEntry,short spType,char **pcpDest,
				int ipValueType,char *pcpDefVal);
static int	CheckValue(char *pcpEntry,char *pcpValue,int ipType);
static int	SendAckMsg();
static int	GetMsgno(char *pcpBuff);

static int tcp_socket();
static int	SendHeartbeatMsg(char *pcpLastTimeSendingTime);
void FormatTime(char *pcpTime, char *pcpLastTimeSendingTime);
static int ReceiveACK(int ipSock,int ipTimeOut);
static int Sockt_Reconnect(void);
static int tcp_open_connection (int socket, char *Pservice, char *Phostname);
static int FindMsgId(char *pcpData, char *pcpMsgId, int Mode);
//
static int udp_socket(int *ilAcceptSocket);
static void SendUDPMessage(char *pIP, int pPort, char *pMessage);
static int resetUDP(int socket, char *Pservice, char *Phostname);
static void CloseUDP(char *pcpCaller);
static int udp_open_connection (int socket, char *Pservice, char *Phostname);
//
/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/

MAIN
{
	char *pclFunc = "MAIN";
	int 	ilCount = 0;
	int		ilRc = RC_SUCCESS;		/* Return code            	*/
	int		ilCnt = 0;
	int		ilItemFlag=TRUE;
	time_t	now = 0;
    char pclCurrentTime[64] = "\0";
	INITIALIZE;						/* General initialization	*/

	/* signal handling of SIGPIPE,SIGCHLD,SIGALRM,SIGTERM */
	SetSignals(HandleSignal);

	dbg(TRACE,"------------------------------------------");
	dbg(TRACE,"MAIN: version <%s>",sccs_hwecon);

	/* Attach to the MIKE queues */
	do
	{
		ilRc = init_que();

		if(ilRc != RC_SUCCESS)
		{
			dbg(TRACE,"MAIN: init_que() failed! waiting 6 sec ...");
			sleep(6);
			ilCnt++;
		}/* end if */
	} while((ilCnt < 10) && (ilRc != RC_SUCCESS));

	if(ilRc != RC_SUCCESS)
  {
		dbg(TRACE,"MAIN: init_que() failed! waiting 60 sec ...");
		sleep(60);
		exit(1);
  } /* end if */
  else
  {
		dbg(TRACE,"MAIN: init_que() OK!");
		dbg(TRACE,"MAIN: mod_id   <%d>",mod_id);
		dbg(TRACE,"MAIN: mod_name <%s>",mod_name);
  }/* end else */

	*pcgConfFile = 0x00;
	sprintf(pcgConfFile,"%s/%s",getenv("BIN_PATH"),mod_name);
	ilRc = TransferFile(pcgConfFile);
	if(ilRc != RC_SUCCESS)
  {
		dbg(TRACE,"MAIN: TransferFile(%s) failed!",pcgConfFile);
  } /* end if */

	dbg(TRACE,"MAIN: Binary-file = <%s>",pcgConfFile);
	ilRc = SendRemoteShutdown(mod_id);
	if(ilRc != RC_SUCCESS)
  {
	dbg(TRACE,"MAIN: SendRemoteShutdown(%d) failed!",mod_id);
  }

	if((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
  {
		dbg(TRACE,"MAIN: waiting for status switch ...");
		HandleQueues();
		dbg(TRACE,"MAIN: now running ...");
  }/* end of if */

	if((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
  {
		dbg(TRACE,"MAIN: initializing ...");
		dbg(TRACE,"------------------------------------------");

		if(igInitOK == FALSE)
		{
			ilRc = Init_Handler();

			if(ilRc == RC_SUCCESS)
			{
				dbg(TRACE,"");
				dbg(TRACE,"------------------------------------------");
				dbg(TRACE,"MAIN: initializing OK");
				igInitOK = TRUE;
			}
		}
	}
  else
  {
		Terminate(1);
  }

	dbg(TRACE,"------------------------------------------");

	if (igInitOK == TRUE)
	{
        ilRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"INI","","","","","","",NETOUT_NO_ACK);

        igSock = 0;
  	    ilRc = OpenServerSocket();
        if (ilRc == RC_SUCCESS)
        {
          igConnected = TRUE;
          igOldCnnt = TRUE;
        	ilRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"CONX","","","","","","",NETOUT_NO_ACK);
        }
        else
        {
          igConnected = FALSE;
          igOldCnnt = FALSE;
        	ilRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"DROP","","","","","","",NETOUT_NO_ACK);
        }

		now = time(NULL);
		while(TRUE)
		{
			ilCount = 0;
			if ((ilRc = poll_q_and_sock()) == RC_FAIL)
			{
				dbg(TRACE,"MAIN: Poll_Q_And_Sock LOOP");
				GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

				if (igSock <= 0)
				{
					dbg(DEBUG,"MAIN: Current Time<%s>, Exp Time<%s> Connect Status<%d>", pclCurrentTime, pcgReconExpTime, igConnected);

					if( igConnectionNo < igReconMax )
					{
						ilRc = Sockt_Reconnect();
					}
					else
					{
                        if ( ((igConnected == FALSE) && (strcmp(pclCurrentTime, pcgReconExpTime) >= 0)) || (igConnected == TRUE) )
                        {
                            ilRc = Sockt_Reconnect();
						}
					}
				}
//			    sleep(1);
			}
			now = time(NULL);
		}
	}
	else
	{
		dbg(TRACE,"MAIN: Init_Handler() failed with <%d> Sleeping 15 sec.! Then terminating ...",ilRc);
		sleep(15);
	}

	exit(0);
	return 0;
} /* end of MAIN */

/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/

static int Init_Handler()
{
	int	ilRc = RC_SUCCESS;            /* Return code */

	GetQueues();

	/* reading default home-airport from sgs.tab */
	memset(pcgHomeAp,0x00,sizeof(pcgHomeAp));
	ilRc = tool_search_exco_data("SYS","HOMEAP",pcgHomeAp);
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"Init_Handler : No HOMEAP entry in sgs.tab: EXTAB! Please add!");
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"Init_Handler : HOMEAP = <%s>",pcgHomeAp);
	}

	/* reading default table-extension from sgs.tab */
	memset(pcgTabEnd,0x00,sizeof(pcgTabEnd));
	ilRc = tool_search_exco_data("ALL","TABEND",pcgTabEnd);
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"Init_Handler : No TABEND entry in sgs.tab: EXTAB! Please add!");
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"Init_Handler : TABEND = <%s>",pcgTabEnd);
		memset(pcgTwEnd,0x00,XS_BUFF);
		sprintf(pcgTwEnd,"%s,%s,%s",pcgHomeAp,pcgTabEnd,mod_name);
		dbg(TRACE,"Init_Handler : TW_END = <%s>",pcgTwEnd);
	}

	if (ilRc = GetConfig() == RC_FAIL)
	{
		dbg(TRACE,"Init_handler: Configuration error, terminating");
		Terminate(30);
	}

	return(ilRc);
} /* end of Init_Handler */

/*********************************************************************
Function : GetQueues()
Paramter :
Return Code: RC_SUCCESS,RC_FAIL
Result:
Description: Gets all necessary queue-ID's for CEDA-internal
             communication!
*********************************************************************/

static int GetQueues()
{
	int	ilRc = RC_FAIL;

	/* get mod-id of hweexci */
	if ((igModID_Rcvr = tool_get_q_id("hweexci")) == RC_NOT_FOUND ||
		igModID_Rcvr == RC_FAIL || igModID_Rcvr == 0)
  {
		dbg(TRACE,"GetQueues: tool_get_q_id(hweexci) returns: <%d>",igModID_Rcvr);
		ilRc = RC_FAIL;
	}
	else
	{
		dbg(TRACE,"GetQueues: <hweexci> mod_id <%d>",igModID_Rcvr);
		ilRc = RC_SUCCESS;
	}
	return ilRc;
} /* end of GetQueues */

/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/

static int Reset()
{

	int	ilRc = RC_SUCCESS;    /* Return code */

	dbg(TRACE,"Reset: now reseting ...");

	return ilRc;

} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/

static void Terminate(int ipSleep)
{
	dbg(TRACE,"Terminate: now leaving ...");

	if( strcmp(pcgConnType,"tcp") == 0 )
    {
        CloseTCP();
    }
    else if( strcmp(pcgConnType,"udp") == 0 )
    {
        CloseUDP("Terminate");
    }

	if (pgReceiveLogFile != NULL)
	{
		fclose(pgReceiveLogFile);
	} /* end if */


	if (pgSendLogFile != NULL)
	{
		fclose(pgSendLogFile);
	} /* end if */

	sleep(ipSleep);

	exit(0);

} /* end of Terminate */

/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/

static void HandleSignal(int pipSig)
{
	switch(pipSig)
    {
		case SIGALRM:
			bgAlarm = TRUE;
			break;
		case SIGPIPE:
			if (igSock != 0)
                {
                    if( strcmp(pcgConnType,"tcp") == 0 )
                    {
                        CloseTCP();
                    }
                    else if( strcmp(pcgConnType,"udp") == 0 )
                    {
                        CloseUDP("HandleSignal");
                    }
                }
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

static void HandleErr(int *pipErr)
{
		char *pclFunc = "HandleErr";
		*pipErr = 0;

		dbg(TRACE,"<%s> pipErr has been reset to zero",pclFunc);
    return;
} /* end of HandleErr */

/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/

static void HandleQueErr(int pipErr)
{
    int    ilRc = RC_SUCCESS;

    switch(pipErr) {
    case    QUE_E_FUNC    :    /* Unknown function */
        dbg(TRACE,"<%d> : unknown function",pipErr);
        break;
    case    QUE_E_MEMORY    :    /* Malloc reports no memory */
        dbg(TRACE,"<%d> : malloc failed",pipErr);
        break;
    case    QUE_E_SEND    :    /* Error using msgsnd */
            dbg(TRACE,"<%d> : msgsnd failed",pipErr);
            break;
    case    QUE_E_GET    :    /* Error using msgrcv */
            if(pipErr != 4)
             dbg(DEBUG,"<%d> : msgrcv failed",pipErr);
        break;
    case    QUE_E_EXISTS    :
        dbg(TRACE,"<%d> : route/queue already exists ",pipErr);
        break;
    case    QUE_E_NOFIND    :
        dbg(TRACE,"<%d> : route not found ",pipErr);
        break;
    case    QUE_E_ACKUNEX    :
        dbg(TRACE,"<%d> : unexpected ack received ",pipErr);
        break;
    case    QUE_E_STATUS    :
        dbg(TRACE,"<%d> :   unknown queue status ",pipErr);
        break;
    case    QUE_E_INACTIVE    :
        dbg(TRACE,"<%d> : queue is inaktive ",pipErr);
        break;
    case    QUE_E_MISACK    :
        dbg(TRACE,"<%d> : missing ack ",pipErr);
        break;
    case    QUE_E_NOQUEUES    :
        dbg(TRACE,"<%d> : queue does not exist",pipErr);
        break;
    case    QUE_E_RESP    :    /* No response on CREATE */
        dbg(TRACE,"<%d> : no response on create",pipErr);
        break;
    case    QUE_E_FULL    :
        dbg(TRACE,"<%d> : too many route destinations",pipErr);
        break;
    case    QUE_E_NOMSG    :    /* No message on queue */
        /*dbg(TRACE,"<%d> : no messages on queue",pipErr);*/
        break;
    case    QUE_E_INVORG    :    /* Mod id by que call is 0 */
        dbg(TRACE,"<%d> : invalid originator=0",pipErr);
        break;
    case    QUE_E_NOINIT    :    /* Queues is not initialized*/
        dbg(TRACE,"<%d> : queues are not initialized",pipErr);
        break;
    case    QUE_E_ITOBIG    :
        dbg(TRACE,"<%d> : requestet itemsize to big ",pipErr);
        break;
    case    QUE_E_BUFSIZ    :
        dbg(TRACE,"<%d> : receive buffer to small ",pipErr);
        break;
    case    QUE_E_PRIORITY    :
        dbg(TRACE,"<%d> : wrong priority was send ",pipErr);
        break;
    default            :    /* Unknown queue error */
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
	int	ilRc = RC_SUCCESS;            /* Return code */
	int	ilBreakOut = FALSE;

	do{
		memset(prgItem,0x00,igItemLen);
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
				case    HSB_STANDBY    :
					ctrl_sta = prgEvent->command;
					break;
				case    HSB_COMING_UP    :
					ctrl_sta = prgEvent->command;
					break;
				case    HSB_ACTIVE    :
					ctrl_sta = prgEvent->command;
					ilBreakOut = TRUE;
					break;
				case    HSB_ACT_TO_SBY    :
					ctrl_sta = prgEvent->command;
					break;
				case    HSB_DOWN    :
					/* whole system shutdown - do not further use que(), send_message() or timsch() ! */
					ctrl_sta = prgEvent->command;
					Terminate(10);
					break;
				case    HSB_STANDALONE    :
					ctrl_sta = prgEvent->command;
					ResetDBCounter();
					ilBreakOut = TRUE;
					break;
				case    REMOTE_DB :
					/* ctrl_sta is checked inside */
					HandleRemoteDB(prgEvent);
					break;
				case    SHUTDOWN    :
					Terminate(1);
					break;
				case    RESET        :
					ilRc = Reset();
					break;
				case    EVENT_DATA    :
					dbg(TRACE,"HandleQueues: wrong hsb status <%d>",ctrl_sta);
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
					break;
				case    TRACE_ON :
					dbg_handle_debug(prgEvent->command);
					break;
				case    TRACE_OFF :
					dbg_handle_debug(prgEvent->command);
					break;
				default            :
					dbg(TRACE,"HandleQueues: unknown event");
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
				break;
			} /* end switch */
		}
		else
		{
			/* Handle queuing errors */
			HandleQueErr(ilRc);
		} /* end else */
	} while (ilBreakOut == FALSE);

	if(igInitOK == FALSE)
    {
		ilRc = Init_Handler();

		if(ilRc == RC_SUCCESS)
		{
			dbg(TRACE,"HandleQueues: Init_Handler() OK!");
			igInitOK = TRUE;
		} /* end if */
		else
		{
			dbg(TRACE,"HandleQueues: Init_Handler() failed!");
			igInitOK = FALSE;
		} /* end else */
	}/* end of if */
} /* end of HandleQueues */

/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/

static int HandleInternalData()
{
	int  ilRC = RC_SUCCESS;			/* Return code */
	int  ilQueRc = RC_SUCCESS;			/* Return code */
	char *pclSelection = NULL;
	char *pclFields = NULL;
	char *pclData = NULL;
	char *pclTmpPtr=NULL;
	BC_HEAD *bchd = NULL;			/* Broadcast header*/
	CMDBLK  *cmdblk = NULL;
	int ilLen;
	char *pclFunc = "HandleInternalData";

	char pclDataBuf[M_BUFF];

	int ilCount = 0;

	memset(pclDataBuf,0,sizeof(pclDataBuf));

	bchd  = (BC_HEAD *) ((char *)prgEvent + sizeof(EVENT));
	cmdblk= (CMDBLK  *) ((char *)bchd->data);

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TWS",CFG_STRING,pcgTwStart);

	if (ilRC != RC_SUCCESS)
	{
		strcpy(pcgTwStart,cmdblk->tw_start);
	} /* end if */
	strcpy(pcgTwEnd,cmdblk->tw_end);

	/***************************************/
    /* DebugPrintItem(DEBUG,prgItem);	   */
	/* DebugPrintEvent(DEBUG,prgEvent);	   */
	/***************************************/

	pclSelection = cmdblk->data;
	pclFields = (char *)pclSelection + strlen(pclSelection) + 1;
	pclData = (char *)pclFields + strlen(pclFields) + 1;

	dbg(DEBUG,"========================= START ===============================");
	if (strcmp(cmdblk->command,"XMLO") == 0)
	{
		dbg(DEBUG,"Command:   <%s>",cmdblk->command);
		dbg(DEBUG,"Selection: <%s>",pclSelection);
		dbg(DEBUG,"Fields:    <%s>",pclFields);
		dbg(DEBUG,"Data:      \n<%s>",pclData);
		dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
		dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);

        /*
		if( igMsgNo >= 65535)
		{
			igMsgNo = 0;
		}
		sprintf(pclDataBuf,pclData,igMsgNo);
		igMsgNo++;

		memset(pclData,0,sizeof(pclData));
		strcpy(pclData,pclDataBuf);
        */
		//dbg(DEBUG,"Data:      \n<%s>",pclDataBuf);

        strcpy(pcgCurSendData, pclData);
        strcpy(pcgSendMsgId,pclSelection);

        for (ilCount = 0; ilCount < igReSendMax; ilCount++)
        {
		    if (igSock > 0)
            {
                    ilRC = Send_data(igSock,pclData);
		        	dbg(DEBUG, "<%s>ilRC<%d>",pclFunc,ilRC);

		        if (ilRC == RC_SUCCESS)
		        {
                    igSckWaitACK = TRUE;
                    igSckTryACKCnt = 0;

                    GetServerTimeStamp( "UTC", 1, 0, pcgSckWaitACKExpTime);
                    AddSecondsToCEDATime(pcgSckWaitACKExpTime, igSckACKCWait, 1);
                    break;
		        }
                else if(ilRC == RC_FAIL)
		        {
			       dbg(DEBUG, "<%s>Send_data error",pclFunc);
                   ilRC = Sockt_Reconnect();
		        }
		        else if(ilRC == RC_SENDTIMEOUT)
		        {
			       dbg(DEBUG,"<%s>Send_data timeout, Re send again",pclFunc);
		        }
            }
            else
            {
                if ((igConnected == TRUE) || (igOldCnnt == TRUE))
                    ilRC = Sockt_Reconnect();
                else
                	  ilRC = RC_FAIL;
            }
        }

		if( ilCount >= igReSendMax)
		{
			dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);
			return RC_FAIL;
		}
	}
	else if (strcmp(cmdblk->command,"RACK") == 0)
    {
    	dbg(DEBUG,"RACK command");
        if (strcmp(pcgSendMsgId, pclSelection) == 0)
        {
            igSckWaitACK = FALSE;
            igSckTryACKCnt = 0;
        }
        else
    	    dbg(DEBUG,"RACK command Recived MsgId<%s>, NOT for current Waiting MsgId<%s>", pclSelection, pcgSendMsgId);
       ilRC = RC_SUCCESS;
    }
	else if (strcmp(cmdblk->command,"SACK") == 0)
    {
    	dbg(DEBUG,"SACK command");
        strcpy(pcgMsgNoForACK, pclSelection);
	    SendAckMsg();
        ilRC = RC_SUCCESS;
    }
	else if (strcmp(cmdblk->command,"CONX") == 0)
    {
    	dbg(DEBUG,"CONX command");

        if (igConnected == TRUE)
	        ilQueRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"CONX","","","","","","",NETOUT_NO_ACK);
        else
	        ilQueRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"DROP","","","","","","",NETOUT_NO_ACK);
    }
	else
	{
		dbg(TRACE,"HandleInternalData: Invalid command <%s>",cmdblk->command);
		ilRC = RC_FAIL;
	}
    dbg(DEBUG,"========================= END ================================");
  return ilRC;
} /* end of HandleInternalData() */

/* ******************************************************************** */
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
	char	clKeepAliveFormatF[256];
	char	clAckFormatF[256];
	FILE	*fplFp;
	char pclSelection[1024] = "\0";

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

	/*fya paicon 0.1*/
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HB_HEADER",CFG_STRING,&pcgHB_Header,CFG_PRINT,"IED"))
                        != RC_SUCCESS)
		return RC_FAIL;

    if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","CONN_TYPE",CFG_STRING,&pcgConnType,CFG_PRINT,"udp"))
                        != RC_SUCCESS)
		return RC_FAIL;


	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HEARTBEAT",CFG_STRING,&prgCfg.keep_alive,CFG_NUM,"0"))
                        != RC_SUCCESS)
		return RC_FAIL;
	else
		igKeepAlive = atoi(prgCfg.keep_alive);


	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HEARTBEAT_FORMAT",CFG_STRING,&prgCfg.keep_alive_format,CFG_PRINT,"tcpkeepalive.txt"))
                        != RC_SUCCESS)
		return RC_FAIL;
	else
		sprintf(clKeepAliveFormatF,"%s/%s",getenv("CFG_PATH"),prgCfg.keep_alive_format);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","HWECON_MGR",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igModID_ConMgr = atoi(pclTmpBuf);
    else
        igModID_ConMgr = 1005;
	dbg(TRACE,"Connect Mgr Modid <%d>", igModID_ConMgr);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","HWECON_STB",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igModID_ConSTB = atoi(pclTmpBuf);
    else
        igModID_ConSTB = 1003;
	dbg(TRACE,"Parallel Connect  Modid <%d>", igModID_ConSTB);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CLT_RECNCT_INTERVAL",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igReconIntv = atoi(pclTmpBuf);
    else
        igReconIntv = 15;
	dbg(TRACE,"Retry to connect Interval Time <%d> Second", igReconIntv);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CLT_RECNCT_TIMES",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igReconMax = atoi(pclTmpBuf);
    else
        igReconMax = 3;
	dbg(TRACE,"Try Max <%d> Times for each connection", igReconMax);


    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","HEARTBEAT_INTERVAL",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igHeartBeatIntv = atoi(pclTmpBuf);
    else
        igHeartBeatIntv = 5;
	dbg(TRACE,"HeartBeat Send Interval <%d> Second", igHeartBeatIntv);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","HEARTBEAT_TIMEOUT",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igHeartBeatTimOut = atoi(pclTmpBuf);
    else
        igHeartBeatTimOut = 15;
	dbg(TRACE,"HeartBeat Send TIMEOUT set to <%d> Second", igHeartBeatTimOut);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ACK_SEND_ENABLE",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, "NO") == 0))
    {
        igSendAckEnable = FALSE;
	    dbg(TRACE,"Send ACK to Server Disable");
    }
    else
    {
        igSendAckEnable = TRUE;
	    dbg(TRACE,"Send ACK to Server Enable");
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ACK_WAIT_TIME",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igSckACKCWait = atoi(pclTmpBuf);
    else
        igSckACKCWait = 3;
	dbg(TRACE,"Waiting ACK Time setting <%d> Second", igSckACKCWait);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","WACK_TRY_TIMES",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igSckTryACKCMax = atoi(pclTmpBuf);
    else
        igSckTryACKCMax = 3;
	dbg(TRACE,"Waiting ACK Resend data Max <%d> Times", igSckACKCWait);

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CON_DLY",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igConDly = atoi(pclTmpBuf);
    else
        igConDly = 30;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","MGR_DLY",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
        igMgrDly = atoi(pclTmpBuf);
    else
        igMgrDly = 30;

	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","SEND_ACK",CFG_STRING,&prgCfg.send_ack,CFG_NUM,"1"))
                        != RC_SUCCESS)

	{
		return RC_FAIL;
  }
  else
  {
  	igSendAck = atoi(prgCfg.send_ack);
	}

	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","ACK_FORMAT",CFG_STRING,&prgCfg.ack_format,CFG_PRINT,"tcpack.txt"))
                        != RC_SUCCESS)
	{
			return RC_FAIL;
	}
	else
	{
			sprintf(clAckFormatF,"%s/%s",getenv("CFG_PATH"),prgCfg.ack_format);
	}

	if(igSendAck)
	{
  	if ((fplFp = (FILE *) fopen(clAckFormatF,"r")) == (FILE *)NULL)
		{
			dbg(TRACE,"SendAck Format File <%s> does not exist",clAckFormatF);
			return RC_FAIL;
		}
		else
		{
			ilLen = fread(pcgSendAckFormat,1,1023,fplFp);
			pcgSendAckFormat[ilLen] = '\0';
			fclose(fplFp);
		}
	}

  //ilRC = ReadConfigEntry( pcgConfigFile, "MAIN","IP",pclSelection);
  //ilRC = GetCfgEntry(pcgConfigFile,"MAIN","IP",CFG_STRING,pcgIP,CFG_PRINT,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","IP",CFG_STRING,pcgIP);
  if(strlen(pcgIP)==0)
  {
  	dbg(TRACE,"IP is null<%s>",pcgIP);
  }
  else
  {
  	dbg(TRACE,"IP is not null<%s>",pcgIP);
  }

  //ilRC = ReadConfigEntry( pcgConfigFile, "MAIN","PORT",pclSelection);
  //ilRC = GetCfgEntry(pcgConfigFile,"MAIN","PORT",CFG_STRING,pclSelection,CFG_PRINT,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","PORT",CFG_STRING,pcgPort);
	if(strlen(pcgPort)==0)
	  {
	  	dbg(TRACE,"Port is null<%s>",pcgPort);
	  }
	  else
	  {
	  	dbg(TRACE,"Port is not null<%s>",pcgPort);
	  }

  if(igKeepAlive)
  {
	  if((fplFp = (FILE *)fopen(clKeepAliveFormatF,"r")) == (FILE *)NULL)
		{
			dbg(TRACE,"Heartbeat Format File <%s> does not exist",clKeepAliveFormatF);
			return RC_FAIL;
		}
		else
		{
			ilLen = fread(pcgKeepAliveFormat,1,1024,fplFp);
			pcgKeepAliveFormat[ilLen-1] = '\0';
			fclose(fplFp);
		}
  }
  return RC_SUCCESS;
} /* End of GetConfig() */

/* ******************************************************************** */
/* Following the poll_q_and_sock function								*/
/* Waits for input on the socket and polls the QCP for messages			*/
/* ******************************************************************** */

static int poll_q_and_sock()
{
	int ilRc;
	int ilRc_Connect = RC_FAIL;
	char pclTmpBuf[128];
	int ilI;
	int ilReadQueCnt = 0;

	char *pclFunc = "poll_q_and_sock";
	char pclLastTimeSendingHeartbeat[64] = "\0";
	char pclCurrentTime[64] = "\0";

	do
	{
		nap(20); /* waiting because of NOT-waiting QUE_GETBIGNW */
		/*---------------------------*/
		/* now looking on ceda-queue */
		/*---------------------------*/

		// dbg(DEBUG,"time before que<%d>",time(0));
        ilReadQueCnt++;
		if ( (ilRc = que(QUE_GETBIGNW,0,mod_id,PRIORITY_3,igItemLen,
				(char *) &prgItem)) == RC_SUCCESS)
		{
                        ilReadQueCnt = 0;
		//	dbg(DEBUG,"time after1 que<%d>",time(0));
			/* depending on the size of the received item  */
			/* a realloc could be made by the que function */
			/* so do never forget to set event pointer !!! */

			dbg(TRACE,"***********************");

			prgEvent = (EVENT *) prgItem->text;

			/* Acknowledge the item */
			ilRc = que(QUE_ACK,0,mod_id,0,0,NULL);

			if (ilRc != RC_SUCCESS)
			{
				/* handle que_ack error */
				HandleQueErr(ilRc);
			} /* end if */

			switch(prgEvent->command)
			{
				case HSB_STANDBY	:
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_STANDBY event!");
					HandleQueues();
					break;
				case HSB_COMING_UP	:
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_COMING_UP event!");
					HandleQueues();
					break;
				case HSB_ACTIVE	:
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_ACTIVE event!");
					break;
				case HSB_ACT_TO_SBY	:
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_ACT_TO_SBY event!");
					HandleQueues();
					break;
				case HSB_DOWN	:
					/* whole system shutdown - do not further use que(), */
					/* send_message() or timsch() ! */
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_DOWN event!");
					Terminate(FALSE);
					break;
				case HSB_STANDALONE	:
					ctrl_sta = prgEvent->command;
					dbg(TRACE,"PQS: received HSB_STANDALONE event!");
					break;
				case REMOTE_DB :
					/* ctrl_sta is checked inside */
					/*HandleRemoteDB(prgEvent);*/
					break;
				case SHUTDOWN	:
					/* process shutdown - maybe from uutil */
					dbg(TRACE,"PQS: received SHUTDOWN event!");
					Terminate(FALSE);
					break;
				case RESET		:
					dbg(TRACE,"PQS: received RESET event!");
					ilRc = Reset();
					if (ilRc == RC_FAIL)
					Terminate(FALSE);
					break;
				case EVENT_DATA	:
					if ((ctrl_sta == HSB_STANDALONE) ||
						(ctrl_sta == HSB_ACTIVE) ||
						(ctrl_sta == HSB_ACT_TO_SBY))
					{
					//	if (igSock > 0)
						{
							dbg(TRACE,"Calling HandleInternalData");
							ilRc = HandleInternalData();
							if (ilRc == RC_FAIL)
							{
								dbg(TRACE,"PQS: HandleInternalData failed <%d>",ilRc);
				//				HandleErr(&igSock);
							}
						}
                     /*
						else
						{
							dbg(TRACE,"No connection! Ignoring event!");
							//dbg(TRACE,"PQS: No GOS-connection! Ignoring event!");
						}
                      */
					}
					else
					{
						dbg(TRACE,"poll_q_and_sock: wrong HSB-status <%d>",ctrl_sta);
						DebugPrintItem(TRACE,prgItem);
						DebugPrintEvent(TRACE,prgEvent);
					}
					break;
				case TRACE_ON :
					dbg_handle_debug(prgEvent->command);
					break;
				case TRACE_OFF :
					dbg_handle_debug(prgEvent->command);
					break;
				default:
					dbg(TRACE,"MAIN: unknown event");
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
				break;
			}
		}
		else
		{
         //   dbg(DEBUG,"Read Que Result <%d>, Retry Read QueCnt<%d>",ilRc, ilReadQueCnt);
         //   dbg(DEBUG,"time after2 que<%d>",time(0));
            if (ilReadQueCnt > 10)
               nap(300);
            if (ilReadQueCnt >= 80)
               ilReadQueCnt=0;
         //   dbg(DEBUG,"time after3 que<%d>",time(0));
		}
		ilRc = RC_FAIL; /* proceed */

		if (igSock > 0)
		{
			//ilStartTime = time( 0 );
  		    //ilEndTime = ilStartTime + (ilDuration * 60);
  		    //memset(pclLastTimeSendingHeartbeat,0,sizeof(pclLastTimeSendingHeartbeat));

  		    GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
  	        //	dbg(TRACE,"pclCurrentTime<%s>,pclLastTimeSendingHeartbeat<%s>",pclCurrentTime,pclLastTimeSendingHeartbeat);

  		    //Frank v0.62 Comparing the day
         //		dbg(TRACE,"pclTmpCurrent<%s>,pclTmpLast<%s>",pclTmpCurrent,pclTmpLast);
         //		dbg(TRACE,"ilTmpCurrent<%d>,ilTmpLast<%d>",ilTmpCurrent,ilTmpLast);

  		    if (strcmp(pclCurrentTime, pcgSendHeartBeatExpTime) >= 0)
  		    {
	            ilRc = SendHeartbeatMsg(pclLastTimeSendingHeartbeat);
                if (ilRc == RC_FAIL)
			    {
		   	        dbg(TRACE,"<%s>SendHeratbeatMsg fails",pclFunc);
			 	    HandleErr(&igSock);
			    }
			    else if (ilRc == RC_SUCCESS)
			    {
				    dbg(TRACE,"<%s>SendHeratbeatMsg succeed",pclFunc);
			    }
			    else if (ilRc == RC_SENDTIMEOUT)
			    {
				    dbg(TRACE,"<%s> HeartBeat sending timeout",pclFunc);
				    return RC_SENDTIMEOUT;
			    }
  		        strcpy(pcgSendHeartBeatExpTime, pclLastTimeSendingHeartbeat);
  		        AddSecondsToCEDATime(pcgSendHeartBeatExpTime,igHeartBeatIntv, 1);
			}
			else
			{
			   ; //	dbg(TRACE,"<%s>(ilTmpCurrent<%d> - ilTmpLast<%d>) <= 5",pclFunc,ilTmpCurrent,ilTmpLast);
			}

			if (igSock > 0)
			{
				ilRc = Receive_data(igSock,1);
			}
		}
		else
		{
			dbg(TRACE,"igSock<%d>ilRc<%d> Connection breaks",igSock,ilRc);
			return RC_FAIL;
		}

	} while ((ilRc == RC_SUCCESS || ilRc == RC_RECVTIMEOUT || ilRc == RC_SENDTIMEOUT) && (igSock > 0));

	return ilRc;
} /* end of poll_q_and_sock() */

static int SendHeartbeatMsg(char *pcpLastTimeSendingTime)
{
	int ilRC = RC_SUCCESS;
	char pclFunc[] = "SendHeratbeatMsg:";
	//char pclTmp[128]="\0";
	//char pclCurrentTime[128]="\0";
	char pclFormatTime[128]="\0";
	char pclDataBuf[L_BUFF];

	memset(pclDataBuf,0,sizeof(pclDataBuf));

	FormatTime(pclFormatTime,pcpLastTimeSendingTime);

	dbg(DEBUG,"<%s>========================= START-HeartBeat ===============================",pclFunc);
	dbg(DEBUG,"<%s> Send Heartbeat Msg pclFormatTime<%s>",pclFunc,pclFormatTime);

	//Frank 20130107
	//sprintf(pclDataBuf,pcgKeepAliveFormat,igMsgNoForHeartbeat,pclFormatTime);

	if ( strcmp(pcgHB_Header, "IED") == 0 )
	{
		buildHB_message(pclDataBuf,pcgKeepAliveFormat,pcgHB_Header,igObjID,igObjID,igObjID);
	}
	else
	{
		/*Original HB builder*/
		sprintf(pclDataBuf,pcgKeepAliveFormat,pclFormatTime);
	}

	/*fya paicon 0.1*/

	if(igObjID == 255)
	{
		igObjID = 0;
	}
	igObjID++;

    /*
	if(igMsgID == 65535)
	{
		igMsgID = 0;
	}
	igMsgID++;

	//Frank 20130107
	if(igMsgNoForHeartbeat == 65535)
	{
		igMsgNoForHeartbeat = 0;
	}
	igMsgNoForHeartbeat++;
    */

	ilRC = Send_data(igSock,pclDataBuf);

	if(ilRC == RC_SUCCESS)
	{
		dbg(TRACE,"<%s> HeartBeat sending successfully,pclDataBuf<%s>",pclFunc,pclDataBuf);
	}
	else if(ilRC == RC_SENDTIMEOUT)
	{
		dbg(TRACE,"<%s> HeartBeat sending timeout",pclFunc);
	}
	else if(ilRC == RC_FAIL)
	{
		dbg(TRACE,"<%s>HeartBeat sending fails",pclFunc);
	}
	dbg(DEBUG,"<%s>========================= END-HeartBeat ================================",pclFunc);

	return ilRC;
} /* end of SendHeratbeatMsg() */

void FormatTime(char *pcpTime, char *pcpLastTimeSendingTime)
{
	char pclTmp[128]="\0";
	char pclCurrentTime[64]="\0";
	char pclFormatTime[128]="\0";

	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
	strcpy(pcpLastTimeSendingTime,pclCurrentTime);

  strncpy(pclFormatTime,pclCurrentTime,4);
  strcat(pclFormatTime,"-");

  strncpy(pclTmp,pclCurrentTime+4,2);\
  strcat(pclFormatTime,pclTmp);
  strcat(pclFormatTime,"-");

  strncpy(pclTmp,pclCurrentTime+6,2);
  strcat(pclFormatTime,pclTmp);
  strcat(pclFormatTime,"T");

  strncpy(pclTmp,pclCurrentTime+8,2);
  strcat(pclFormatTime,pclTmp);
  strcat(pclFormatTime,":");

  strncpy(pclTmp,pclCurrentTime+10,2);
  strcat(pclFormatTime,pclTmp);
  strcat(pclFormatTime,":");

  strncpy(pclTmp,pclCurrentTime+12,2);
  strcat(pclFormatTime,pclTmp);
  strcat(pclFormatTime,".");

  strcpy(pclTmp,"000Z");
  strcat(pclFormatTime,pclTmp);

  strcpy(pcpTime,pclFormatTime);
}

static int ReceiveACK(int ipSock,int ipTimeOut)
{
	int		ilRC = RC_SUCCESS;
	char	pclFunc[] = "ReceiveACK:";
	int		ilNo;
	int 	ilFound = 0;
	struct timeval rlTimeout;
	static char	pclRecvBuffer[M_BUFF];

	rlTimeout.tv_sec = ipTimeOut;
  rlTimeout.tv_usec = 0;

	memset(pclRecvBuffer,0,sizeof(pclRecvBuffer));

	FD_ZERO( &readfds );
	FD_ZERO( &writefds );
	FD_ZERO( &exceptfds );
	FD_SET( igSock, &readfds );
	FD_SET( igSock, &writefds );
	FD_SET( igSock, &exceptfds );

	ilFound = select( ipSock+1, (fd_set *)&readfds, (fd_set *)0, (fd_set *)&exceptfds, &rlTimeout );

	switch(ilFound)
	{
		case 0:
			dbg(TRACE,"<%s>-------------------Select time out",pclFunc);
			//return RC_SUCCESS;
			//CloseTCP();
			return RC_ACKTIMEOUT;
			break;
		case -1:
			dbg( TRACE,"<%s>ERROR Code<%d>Description<%s>!! SELECT fails!",pclFunc, errno, strerror(errno) );
			return RC_FAIL;
			break;
		default:
			if(FD_ISSET( ipSock, &exceptfds ))
			{
					dbg(TRACE,"<%s> ipSock exception",pclFunc);
					return RC_FAIL;
			}
			else
			{
				if(FD_ISSET( ipSock, &readfds))
				{
					memset( pclRecvBuffer, 0, sizeof(pclRecvBuffer) );
		    	ilNo = recv( ipSock, pclRecvBuffer, sizeof(pclRecvBuffer), 0);

		    	if( ilNo > 0 )
		      {
		      	dbg(TRACE,"<%s> Len<%d>Complete message:\n<%s>",pclFunc,ilNo,pclRecvBuffer);

		      	if(strstr(pclRecvBuffer,"ack")!=0)
						{
							dbg(TRACE,"<%s> Received the ack message",pclFunc);
							return RC_SUCCESS;
		      	}
		      	else
		      	{
		      		dbg(TRACE,"<%s>Does not receive the ack message",pclFunc);
		      		return RC_FAIL;
		      	}
		      }
		      /*
		      else if( ilNo == 0 )
					{
						dbg(TRACE,"<%s>Server disconnect the connection",pclFunc);
						CloseTCP();
						return RC_FAIL;
					}
					*/
					//sleep(1);
				}
		  }
			break;
	}
}


/* ***************************************************************** */
/* The Receive_data routine                                          */
/* Rec data from GOS into global buffer (and into a file )           */
/* ***************************************************************** */
static int Receive_data(int ipSock,int ipTimeOut)
{
	int		ilRC = RC_SUCCESS;
	char	pclFunc[] = "Receive_data:";
	int		ilNo;
	//int 	fd;
	int		ilMsgNo = 0;
	int 	ilFound = 0;
	struct timeval rlTimeout;
	//static char	pclRecvBuffer[M_BUFF];
	static char	pclRecvBuffer[BUFF];
    char pclCurrentTime[64] = "\0";

    char *pclMsgIdEnd = NULL;
    char *pclMsgIdBgn = NULL;
    char pclTmpBuf[10] = "\0";

    struct sockaddr_in si_other;
    int slen = sizeof(si_other);

    /*
    if (igSckWaitACK == TRUE)
	    rlTimeout.tv_sec = 2;
    else
	    rlTimeout.tv_sec = ipTimeOut;
    */
	rlTimeout.tv_sec = 0;
    rlTimeout.tv_usec = 10;  /* blocked for 0 useconds */

	memset(pclRecvBuffer,0,sizeof(pclRecvBuffer));

	FD_CLR( igSock, &readfds );
	FD_CLR( igSock, &writefds );
	FD_CLR( igSock, &exceptfds );
	FD_ZERO( &readfds );
	FD_ZERO( &writefds );
	FD_ZERO( &exceptfds );
	FD_SET( igSock, &readfds );
	FD_SET( igSock, &writefds );
	FD_SET( igSock, &exceptfds );

	ilFound = select( ipSock+1, (fd_set *)&readfds, (fd_set *)0, (fd_set *)&exceptfds, &rlTimeout );

	switch(ilFound)
	{
        case 0:
            dbg(DEBUG, "<%s>-------------------Select time out -> no msg received",pclFunc);
            ilRC = RC_RECVTIMEOUT;
			//return RC_FAIL;
			//return RC_RECVTIMEOUT;
			//CloseTCP();
			//return RC_FAIL;
			break;
		case -1:
			dbg( TRACE,"<%s>ERROR Code<%d>Description<%s>!! SELECT fails!",pclFunc, errno, strerror(errno) );
			igSock = -1;
            ilRC = RC_FAIL;
			// return RC_FAIL;
			break;
		default:
			if (FD_ISSET( ipSock, &exceptfds ))
			{
                dbg(TRACE,"<%s> ipSock exception",pclFunc);
                ilRC = RC_FAIL;
			}
			else
			{
				if(FD_ISSET( ipSock, &readfds))
				{
					memset( pclRecvBuffer, 0, sizeof(pclRecvBuffer) );
                    dbg(DEBUG,"time before recv<%d>",time(0));
                    if( strcmp(pcgConnType,"tcp") == 0 )
                    {
                        ilNo = recv( ipSock, pclRecvBuffer, sizeof(pclRecvBuffer), 0);
                    }
                    else if( strcmp(pcgConnType,"udp") == 0 )
                    {

                        /*Since connect() is also used in UDP, then read() is adopted instead of recvfrom() with IP and port info*/
                        /*ilNo = recvfrom(ipSock, pclRecvBuffer, sizeof(pclRecvBuffer, 0, (struct sockaddr *)&si_other, &slen);*/
                        ilNo = read(ipSock, pclRecvBuffer, sizeof(pclRecvBuffer));
                        dbg(TRACE, "UDP Recieved from <%s> : <%s>", inet_ntoa(si_other.sin_addr), pclRecvBuffer);
                    }
		    	    dbg(DEBUG,"time after recv<%d>",time(0));

                    if( ilNo > 0 )
                    {
                        //if(strstr(pclRecvBuffer,"heartbeat") == 0)
                        dbg(TRACE,"<%s> Len<%d>Complete message<%s>",pclFunc,ilNo,pclRecvBuffer);

                        if (strstr(pclRecvBuffer,"ack")!=0)
                        {
                            dbg(DEBUG,"<%s> Received the ack message",pclFunc);
                            (void) FindMsgId(pclRecvBuffer, pclTmpBuf, 1);
                            dbg(DEBUG,"<%s> Received the ack messageId<%s> : Wait ACK MsgId<%s>",pclFunc, pclTmpBuf,pcgSendMsgId);
                            if (strcmp(pcgSendMsgId, pclTmpBuf) == 0)
                            {
                                 igSckWaitACK = FALSE;
                                 igSckTryACKCnt = 0;
                                 ilRC = SendCedaEvent(igModID_ConSTB,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"RACK","",pcgSendMsgId,"","","","",NETOUT_NO_ACK);
                       nap(igConDly);
                                 ilRC = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"RACK","",pcgSendMsgId,"","","","",NETOUT_NO_ACK);
                       nap(igMgrDly);
                            }
                            // return RC_SUCCESS;
                        }
                        else if (strstr(pclRecvBuffer,pcgHB_Header) != 0)
                        {
                            if (ilNo > 10)
                            {
                                dbg(DEBUG,"<%s> Received the ack message",pclFunc);
                                (void) FindMsgId(pclRecvBuffer, pclTmpBuf, 1);
                                dbg(DEBUG,"<%s> Received the ack messageId<%s> : Wait ACK MsgId<%s>",pclFunc, pclTmpBuf,pcgSendMsgId);
                                if (strcmp(pcgSendMsgId, pclTmpBuf) == 0)
                                {
                                     igSckWaitACK = FALSE;
                                     igSckTryACKCnt = 0;
                                     ilRC = SendCedaEvent(igModID_ConSTB,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"RACK","",pcgSendMsgId,"","","","",NETOUT_NO_ACK);
                           nap(igConDly);
                                     ilRC = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"RACK","",pcgSendMsgId,"","","","",NETOUT_NO_ACK);
                           nap(igMgrDly);
                                }
                                else
                                {
                                    dbg(TRACE,"%s <%s> does not match <%s>",pclFunc, pcgSendMsgId, pclTmpBuf);
                                }
                            }
                            else
                            {
                                dbg(TRACE,"%s IED heartbeat",pclFunc);
                            }
                        }
                        else if(strstr(pclRecvBuffer, "heartbeat") == 0 )/*fya paicon 0.1*/
                        {
                            (void) FindMsgId(pclRecvBuffer, pclTmpBuf, 0);
                            strcpy(pcgMsgNoForACK, pclTmpBuf);
                            if (strcmp(pcgMsgNoForACK, "0") != 0 )
                            {
                                // Send the ACK
                                dbg(DEBUG,"<%s> Sending ACK message",pclFunc);
                                SendAckMsg();
                                ilRC = SendCedaEvent(igModID_ConSTB,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"SACK","",pcgMsgNoForACK,"","","","",NETOUT_NO_ACK);
                                ilRC = SendCedaEvent(igModID_Rcvr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"XMLI","",pcgMsgNoForACK,"",pclRecvBuffer,"","",NETOUT_NO_ACK);
                                if (ilRC == RC_SUCCESS)
                                {
                                     dbg(DEBUG,"<%s>SendCedaEvent<%d> executes successfully",pclFunc,igModID_Rcvr);
                                }
                                else
                                {
                                    dbg(TRACE,"<%s>SendCedaEvent<%d> executes unsuccessfully",pclFunc,igModID_Rcvr);
                                }
                                /* return RC_SUCCESS; */
                            }
                        }

                        ilRC = RC_SUCCESS;
                        GetServerTimeStamp( "UTC", 1, 0, pcgRcvHeartBeatExpTime);
                        AddSecondsToCEDATime(pcgRcvHeartBeatExpTime, igHeartBeatTimOut, 1);
                    }
                    else if( ilNo == 0 )
                    {
                        ilRC = RC_SUCCESS;
                    }
				}
		    }
			break;
	}

    if (igSckWaitACK == TRUE)
    {
       if (igSckTryACKCnt < igSckTryACKCMax)
       {
            GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime);
            if (strcmp(pclCurrentTime, pcgSckWaitACKExpTime) >= 0)
            {
       		    dbg(DEBUG,"%s ++++++++++Resending count<%d>++++++++++++",pclFunc,igSckTryACKCnt);
       		    dbg(DEBUG,"%s CurrentTime <%s>, ReSend ExpTime<%s>",pclFunc,pclCurrentTime,pcgSckWaitACKExpTime);

                igSckTryACKCnt++;
                if (igSock <= 0)
                   ilRC = Sockt_Reconnect();

                if (igSock > 0)
                {
           	        dbg(DEBUG,"%s %d send data",pclFunc,__LINE__);
	         	    ilRC = Send_data(igSock,pcgCurSendData);
       	        }
                GetServerTimeStamp( "UTC", 1, 0, pcgSckWaitACKExpTime);
                AddSecondsToCEDATime(pcgSckWaitACKExpTime, igSckACKCWait, 1);
           }
       }
       else
       {
       		dbg(DEBUG,"%s ++++++++++Resending three times fail ++++++++++",pclFunc);

           igSckWaitACK = FALSE;
           igSckTryACKCnt = 0;
           ilRC = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"NACK","","","","","","",NETOUT_NO_ACK);
           ilRC = Sockt_Reconnect();
           GetServerTimeStamp( "UTC", 1, 0, pcgSckWaitACKExpTime);
           AddSecondsToCEDATime(pcgSckWaitACKExpTime, igSckACKCWait, 1);
       }
    }
    GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime);
    if (strcmp(pclCurrentTime, pcgRcvHeartBeatExpTime) >= 0)
    {
       dbg(DEBUG,"Current Time<%s>, Heartbeat Exp Rec Time<%s>", pclCurrentTime, pcgRcvHeartBeatExpTime);
       (void) Sockt_Reconnect();
    }
    return ilRC;
} /* end of Receive_data() */

/* ***************************************************************** */
/* The OpenServerSocket routine                                      */
/* ***************************************************************** */
int OpenServerSocket()
{
	int	ilRC;
	int	ilLen;
	char *pclFunc = "OpenServerSocket";

	if (igSock > 0)
	{
		dbg(TRACE,"<%s>: Client socket already open",pclFunc);
		ilRC = RC_SUCCESS;
	}
	else // Create socket
	{
	    if( strcmp(pcgConnType,"tcp") == 0 )
	    {
            ilRC = tcp_socket(&igSock);
	    }
	    else if( strcmp(pcgConnType,"udp") == 0 )
	    {
            ilRC = udp_socket(&igSock);
	    }

	    if(ilRC == RC_FAIL || igSock <= 0)
        {
            dbg(TRACE,"<%s>tcp_socket return RC_FAIL: Error code<%d>description<%s>",pclFunc,errno,strerror(errno));
            ilRC = RC_FAIL;
        }
        else
        {
            FD_ZERO( &readfds );
            FD_ZERO( &writefds );
            FD_ZERO( &exceptfds );
            FD_SET( igSock, &readfds );
            FD_SET( igSock, &writefds );
            FD_SET( igSock, &exceptfds );
            dbg(TRACE,"<%s>:Socket opened successfully, igSock<%d>",pclFunc,igSock);
            ilRC = RC_SUCCESS;
        }
	}
	return ilRC;
}/* end OpenServerSocket */

static void my_alarm()
{
}

static int udp_socket(int *ilAcceptSocket)
{
    int ilRc = RC_SUCCESS;
    char *pclFunc = "udp_socket";
    int ilUdpfd = 0;
    struct sigaction act_alarm;
    struct sigaction act_default;

    dbg( TRACE,"<%s>Going to run UDP(Client) connecting to IP<%s> at PORT<%s>",pclFunc,pcgIP,pcgPort);

    act_alarm.sa_handler = my_alarm;
    sigemptyset( &act_alarm.sa_mask );
    act_alarm.sa_flags = 0;
    sigaction( SIGALRM, &act_alarm, &act_default );

    ilUdpfd = socket( AF_INET, SOCK_DGRAM, 0);
    if( ilUdpfd <= 0 )
    {
        dbg( TRACE,"<%s>Unable to create socket! ERROR!",pclFunc);
        return RC_FAIL;
    }
    else
    {
        dbg( TRACE,"<%s>Socket created ilUdpfd<%d>",pclFunc,ilUdpfd);
    }

    alarm(CONNECT_TIMEOUT);
    ilRc = udp_open_connection(ilUdpfd, pcgPort , pcgIP );
    alarm(0);

    //Pay attention to this line
    (*ilAcceptSocket) = ilUdpfd;

    return ilRc;
}

static int tcp_socket(int	*ilAcceptSocket)
{
    int ilRc = RC_SUCCESS;
    char *pclFunc = "tcp_socket";
    int ilTcpfd = 0;
    struct sigaction act_alarm;
    struct sigaction act_default;

    dbg( TRACE,"<%s>Going to run TCP(Client) connecting to IP<%s> at PORT<%s>",pclFunc,pcgIP,pcgPort);

    act_alarm.sa_handler = my_alarm;
    sigemptyset( &act_alarm.sa_mask );
    act_alarm.sa_flags = 0;
    sigaction( SIGALRM, &act_alarm, &act_default );

    ilTcpfd = socket( AF_INET, SOCK_STREAM, PF_UNSPEC );
    if( ilTcpfd <= 0 )
    {
        dbg( TRACE,"<%s>Unable to create socket! ERROR!",pclFunc);
        return RC_FAIL;
    }
    else
    {
        dbg( TRACE,"<%s>Socket created ilTcpfd<%d>",pclFunc,ilTcpfd);
    }

    alarm(CONNECT_TIMEOUT);
    ilRc = tcp_open_connection(ilTcpfd, pcgPort , pcgIP );
    alarm(0);

    //Pay attention to this line
    (*ilAcceptSocket) = ilTcpfd;

    return ilRc;
}

/* ***************************************************************** */
/* The CloseTCP routine                                              */
/* ***************************************************************** */

static void CloseTCP()
{
	int ilRc = RC_SUCCESS;
	char *pclFunc = "CloseTCP";
	dbg(DEBUG,"%s is called,igSock<%d>",pclFunc, igSock);

	if (igSock > 0)
	{
		FD_CLR( igSock, &readfds );
		FD_CLR( igSock, &writefds );
		FD_CLR( igSock, &exceptfds );
		FD_ZERO( &readfds );
		FD_ZERO( &writefds );
		FD_ZERO( &exceptfds );
		igMsgNoForHeartbeat=0;
		igMsgNoForACK = 0;
		igMsgNo = 0;
		ilRc = close(igSock);
		dbg(TRACE,"%s: connection closed!ilRc<%d>",pclFunc, ilRc);
	}
} /* end of CloseTCP() */

static void CloseUDP(char *pcpCaller)
{
	int ilRc = RC_SUCCESS;
	char *pclFunc = "CloseUDP";
	dbg(DEBUG,"%s is called, @@Caller<%s>, igSock<%d>",pclFunc, pcpCaller, igSock);

	if (igSock > 0)
	{
		FD_CLR( igSock, &readfds );
		FD_CLR( igSock, &writefds );
		FD_CLR( igSock, &exceptfds );
		FD_ZERO( &readfds );
		FD_ZERO( &writefds );
		FD_ZERO( &exceptfds );
		igMsgNoForHeartbeat=0;
		igMsgNoForACK = 0;
		igMsgNo = 0;

        ilRc = resetUDP(igSock, pcgPort , pcgIP);
        dbg(TRACE,"resetUDP: connection reset, ilRc<%d>",ilRc);
		ilRc = close(igSock);
		dbg(TRACE,"%s: connection closed!ilRc<%d>",pclFunc,ilRc);
	}
} /* end of CloseUDP() */

/* **************************************************************** */
/* The snapit routine                                               */
/* snaps data if the debug-level is set to DEBUG                    */
/* **************************************************************** */

static void snapit(void *pcpBuffer,long lpDataLen,FILE *pcpDbgFile)
{
	if (debug_level==DEBUG)
	snap((char*)pcpBuffer,(long)lpDataLen,(FILE*)pcpDbgFile);
} /* end of snapit() */

/* ******************************************************************** */
/* The GetCfgEntry() routine                                            */
/* ******************************************************************** */

static int GetCfgEntry(char *pcpFile,char *pcpSection,char *pcpEntry,short spType,char **pcpDest,
                       int ipValueType,char *pcpDefVal)
{
	int ilRc = RC_SUCCESS;
	char pclCfgLineBuffer[L_BUFF];

	memset(pclCfgLineBuffer,0x00,L_BUFF);

	if ((ilRc=iGetConfigRow(pcpFile,pcpSection,pcpEntry,spType,pclCfgLineBuffer)) != RC_SUCCESS)
	{
		dbg(TRACE,"GetCfgEntry: reading entry <%s> failed.",pcpEntry);
		dbg(TRACE,"GetCfgEntry: EMPTY <%s>! Use default <%s>.",pcpEntry,pcpDefVal);
		strcpy(pclCfgLineBuffer,pcpDefVal);
	} /* end if */
	else
	{
		if (strlen(pclCfgLineBuffer) < 1)
		{
			dbg(TRACE,"GetCfgEntry: EMPTY <%s>! Use default <%s>.",pcpEntry,pcpDefVal);
			strcpy(pclCfgLineBuffer,pcpDefVal);
		} /* end if */
	} /* end else */
	*pcpDest = malloc(strlen(pclCfgLineBuffer)+1);
	strcpy(*pcpDest,pclCfgLineBuffer);
	dbg(TRACE,"GetCfgEntry: %s = <%s>",pcpEntry,*pcpDest);

	if ((ilRc = CheckValue(pcpEntry,*pcpDest,ipValueType)) != RC_SUCCESS)
	{
		dbg(TRACE,"GetCfgEntry: please correct value <%s>!",pcpEntry);
	}
	return ilRc;
} /* end of GetCfgEntry() */

/* ******************************************************************** */
/* The CheckValue() routine												*/
/* Validates configuration values										*/
/* ******************************************************************** */

static int CheckValue(char *pcpEntry,char *pcpValue,int ipType)
{
	int ilRc = RC_SUCCESS;

	switch(ipType)
	{
		case CFG_NUM:
			while(*pcpValue != 0x00 && ilRc==RC_SUCCESS)
			{
				if (isdigit(*pcpValue)==0)
				{
					dbg(TRACE,"CheckValue : NOT A NUMBER! <%s>!",pcpEntry);
					ilRc = RC_FAIL;
				} /* end if */
				pcpValue++;
			} /* end while */
			break;
		case CFG_ALPHA:
			while(*pcpValue != 0x00 && ilRc==RC_SUCCESS)
			{
				if (isalpha(*pcpValue)==0)
				{
					dbg(TRACE,"CheckValue : <%c> INVALID in <%s>-entry!",*pcpValue,pcpEntry);
					ilRc = RC_FAIL;
				} /* end if */
				pcpValue++;
			} /* end while */
			break;
		case CFG_ALPHANUM:
			while(*pcpValue != 0x00 && ilRc==RC_SUCCESS)
			{
				if (isalnum(*pcpValue)==0)
				{
					dbg(TRACE,"CheckValue : <%c> INVALID in <%s>-entry!",*pcpValue,pcpEntry);
					ilRc = RC_FAIL;
				} /* end if */
				pcpValue++;
			} /* end while */
			break;
		case CFG_PRINT:
			while(*pcpValue != 0x00 && ilRc==RC_SUCCESS)
			{
				if (isprint(*pcpValue)==0)
				{
					dbg(TRACE,"CheckValue : <%c> INVALID in <%s>-entry!",*pcpValue,pcpEntry);
					ilRc = RC_FAIL;
				} /* end if */
				pcpValue++;
			} /* end while */
			break;
		default:
			break;
	} /* end switch */
	  return ilRc;
} /* end of CheckValue() */

/* **************************************************************** */
/* The Send_data routine                                            */
/* Sends telegrams to the GOS-system                                */
/* **************************************************************** */
static int Send_data(int ipSock, char *pcpData)
{
	int ilRc = RC_SUCCESS;
	int ilBytes = 0;
	char pclCurrentTime[64];
	char *pclP;
	char *pclPend;
	char pclStandName[16];
	int ilStandNameLength = 0;
	BOOL blKeepSuffix = FALSE;
	int ilFound = 0;
	static char	pclSendBuffer[BUFF];
    char pclTmpStr[12] = "\0";

	ilBytes = strlen(pcpData);

	memset(pclSendBuffer,0,sizeof(pclSendBuffer));
	strcpy(pclSendBuffer,pcpData);
    pclTmpStr[0] = 0x04;
    pclTmpStr[1] = '\0';
    strcat( pclSendBuffer, pclTmpStr );

    ilBytes = strlen(pclSendBuffer);

    if (strstr(pclSendBuffer,"heartbeat") == 0)
        dbg(DEBUG,"Send_data: Mesg sent (%s)", pclSendBuffer );

	if (ipSock > 0 && strlen(pclSendBuffer) != 0)
	{
		errno = 0;
		alarm(WRITE_TIMEOUT);
        if( strcmp(pcgConnType,"tcp") == 0 )
        {
            ilRc = write(ipSock,pclSendBuffer,ilBytes);
        }
        else if( strcmp(pcgConnType,"udp") == 0 )
        {
            /*Since connect() is also used in UDP, then read() is adopted instead of recvfrom() with IP and port info*/
            /*SendUDPMessage(pcgIP, pcgPort, pclSendBuffer);*/
            ilRc = write(ipSock,pclSendBuffer,ilBytes);
        }
		alarm(0);

		if (ilRc == -1)
		{
			if (bgAlarm == FALSE)
			{
				dbg(TRACE,"Send_data: Write failed: Socket <%d>! <%d>=<%s>",ipSock,errno,strerror(errno));
				ilRc = RC_FAIL;
			}
            else
            {
                bgAlarm = FALSE;
                ilRc = RC_SENDTIMEOUT;
            }
		}
		else
		{
			dbg(DEBUG,"Send_data: wrote succeed",ilRc,ipSock);
			ilRc = RC_SUCCESS;
		}
    }
    else
    {
        dbg(TRACE,"Send_data: No connection! Can't send!");
        ilRc = RC_FAIL;
    }

  return ilRc;
} /* end of Send_data */

/* **************************************************************** */
/* The Send Ack routine                                             */
/* Acknowledge a message from the peer                              */
/* **************************************************************** */

static int SendAckMsg()
{
	int ilRC = RC_SUCCESS;
	char pclFunc[] = "SendAckMsg:";
	char pclDataBuf[M_BUFF];
	char pclFormatTime[128]="\0";
	char pclLastTimeSendingTime[64] = "\0";

	memset(pclDataBuf,0,sizeof(pclDataBuf));

	//FormatTime(pclFormatTime);
	FormatTime(pclFormatTime,pclLastTimeSendingTime);

	dbg(DEBUG,"<%s>========================= START-ACK ===============================",pclFunc);
	dbg(DEBUG,"<%s> Send Ack Msg MsgNoForACK<%s>pclFormatTime<%s>",pclFunc,pcgMsgNoForACK,pclFormatTime);

	/*Frank 20130107
	if(igMsgNoForACK == 65535)
  {
          igMsgNoForACK = 0;
  }
	sprintf(pclDataBuf,pcgSendAckFormat,igMsgNoForACK,pclFormatTime,pclFormatTime,igMsgNoForACK);
	igMsgNoForACK++;
	sprintf(pclDataBuf,pcgSendAckFormat,igMsgNoForACK,pclFormatTime,pclFormatTime,igMsgNoForACK);
    */
	sprintf(pclDataBuf,pcgSendAckFormat,pcgMsgNoForACK,pclFormatTime,pclFormatTime,pcgMsgNoForACK);

	ilRC = Send_data(igSock,pclDataBuf);
	dbg(DEBUG,"<%s>========================= END-ACK ================================",pclFunc);

	return ilRC;
} /* End of SendAckMsg */

/* **************************************************************** */
/* The Get Msgno routine                                            */
/* Extracts the message number from a message based on a keyword    */
/* **************************************************************** */

static int GetMsgno(char *pcpMsg)
{
	int		ilMsgNo = -1;
	char	*pclP = NULL;
	int		ilLoop;
	BOOL	blFound = FALSE;

	/* First find the keyword */

	pclP = strstr(pcpMsg,prgCfg.msgno_keyword);

	if (pclP == NULL)
	{
		dbg(TRACE,"GetMsgno: Message number keyword not found: <%s>",prgCfg.msgno_keyword);
		ilMsgNo = -1;
	}
	else
		/* Search for the first digit */

		pclP += strlen(prgCfg.msgno_keyword);

		for (ilLoop = 0; ilLoop < 16 && blFound == FALSE; ilLoop++)
		{
			pclP+= ilLoop;

			if (isdigit(*pclP))
			{
				/* Found a digit, translate sequence to integer */

				ilMsgNo = atoi(pclP);
				blFound = TRUE;
			} /* end if */
		} /* end for */


	return ilMsgNo;
} /* End of GetMsgno() */

static int Sockt_Reconnect(void)
{
	char * pclFunc = "Sockt_Reconnect";
	int ilRc = RC_SUCCESS;
	int ilQueRc = RC_SUCCESS;
    int ilCount = 0;

    //Frank 20130107
    igConnectionNo++;

    igOldCnnt = igConnected;

    if( strcmp(pcgConnType,"tcp") == 0 )
    {
        CloseTCP();
    }
    else if( strcmp(pcgConnType,"udp") == 0 )
    {
        CloseUDP("Sockt_Reconnect");
    }

    igSock = 0;
	dbg(TRACE,"Keep trying to connect <%s><%s>",pcgIP,pcgPort);
    //for (ilCount = 0; ilCount < igReconMax; ilCount++)
	//{
	 	 ilRc = OpenServerSocket();
		 dbg(DEBUG,"ilRc<%d>",ilRc);
		 ilCount++;
	 	 if (ilRc != RC_SUCCESS)
         {
            if( strcmp(pcgConnType,"tcp") == 0 )
            {
                CloseTCP();
            }
            else if( strcmp(pcgConnType,"udp") == 0 )
            {
                CloseUDP("Sockt_Reconnect");
            }
            igSock = 0;
         }
	//}

	if(ilRc == RC_SUCCESS)
    {
        dbg(TRACE,"ilCount[%d]igSock<%d>, connection success,reset ilConnectionNo",ilCount,igSock);
        igConnected = TRUE;

        //Frank 20130107
        igConnectionNo = 0;
    }
	else
    {
        dbg(TRACE,"ilCount[%d]igSock<%d>, connection fails",ilCount,igSock);
        igConnected = FALSE;
    }

    if (igOldCnnt != igConnected)
    {
        if (igConnected == TRUE)
	        ilQueRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"CONX","","","","","","",NETOUT_NO_ACK);
        else
	        ilQueRc = SendCedaEvent(igModID_ConMgr,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"DROP","","","","","","",NETOUT_NO_ACK);
    }

    GetServerTimeStamp( "UTC", 1, 0, pcgReconExpTime);
    strcpy(pcgRcvHeartBeatExpTime, pcgReconExpTime);
    AddSecondsToCEDATime(pcgReconExpTime, igReconIntv, 1);
    AddSecondsToCEDATime(pcgRcvHeartBeatExpTime, igHeartBeatTimOut, 1);

   dbg(DEBUG," %s pcgReconExpTime<%s>Heartbeat Exp Rec Time<%s> igHeartBeatTimOut<%d>", pclFunc,pcgReconExpTime,pcgRcvHeartBeatExpTime,igHeartBeatTimOut);

	return ilRc;
}

static int udp_open_connection (int socket, char *Pservice, char *Phostname)
{
    int rc=RC_FAIL;
    int i = 0;
    struct hostent *hp;
    struct sockaddr_in dummy;
    int serr=0;
    struct servent *sp;
    char *pclFunc = "udp_open_connection";

    if (( hp = gethostbyname( Phostname)) == NULL )
    {
        dbg(TRACE,"%s: ERROR gethostbyname(%s): %s",pclFunc, Phostname, strerror(errno));
        return RC_FAIL;
    }

    dbg(DEBUG,"%s: got hostname %s", pclFunc,Phostname);

    if (( sp = getservbyname(Pservice, NULL) ) == NULL )
    {
        dbg(TRACE,"%s: ERROR getservbyname(%s): %s",pclFunc, Pservice, strerror(errno) );
        return RC_FAIL;
    }

    dbg(DEBUG,"%s: got service %s", pclFunc, Pservice);

    dummy.sin_port = sp->s_port;
    memcpy((char *) &dummy.sin_addr, *hp->h_addr_list, hp->h_length);
    dummy.sin_family = AF_INET;

    rc = connect(socket, (struct sockaddr *) &dummy, sizeof(dummy));
    if (rc < 0)
    {
        dbg(TRACE,"%s: ERROR while connect: %s", pclFunc, strerror(errno));
        return RC_FAIL;
    }

    dbg(DEBUG,"%s: got udp server connection",pclFunc);
    return RC_SUCCESS;
} /* end of udp_open_connection */

static int tcp_open_connection (int socket, char *Pservice, char *Phostname)
{
  int rc=RC_FAIL;
  int i = 0;
  struct hostent *hp;
  struct sockaddr_in dummy;
  int serr=0;
  struct servent *sp;

  if (( hp = gethostbyname( Phostname)) == NULL )
  {
    dbg(TRACE,"tcp_open_connection: ERROR gethostbyname(%s): %s",
               Phostname, strerror(errno));
    return RC_FAIL;
  }

  dbg(DEBUG,"tcp_open_connection: got hostname %s", Phostname);

  if (( sp = getservbyname(Pservice, NULL) ) == NULL )
  {
    dbg(TRACE,"tcp_open_connection: ERROR getservbyname(%s): %s",
               Pservice, strerror(errno) );
    return RC_FAIL;
  }

  dbg(DEBUG,"tcp_open_connection: got service %s", Pservice);

  dummy.sin_port = sp->s_port;
  memcpy((char *) &dummy.sin_addr, *hp->h_addr_list, hp->h_length);
  dummy.sin_family = AF_INET;

  rc = connect(socket, (struct sockaddr *) &dummy, sizeof(dummy));
  if (rc < 0)
  {
    dbg(TRACE,"tcp_open_connection: ERROR while connect: %s",strerror(errno));
    return RC_FAIL;
  }

  dbg(DEBUG,"tcp_open_connection: got connection");
  return RC_SUCCESS;

} /* end of tcp_open_connection */

static void GetFromQue()
{
	int ilRc;
	char * pclFunc = "GetFromQue";

	if ( (ilRc = que(QUE_GETBIGNW,0,mod_id,PRIORITY_3,igItemLen,
				(char *) &prgItem)) == RC_SUCCESS )
	{
		dbg(TRACE,"***********************");

		prgEvent = (EVENT *) prgItem->text;

		ilRc = que(QUE_ACK,0,mod_id,0,0,NULL);

		if (ilRc != RC_SUCCESS)
		{
		HandleQueErr(ilRc);
		}

		switch(prgEvent->command)
		{
			case HSB_STANDBY	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_STANDBY event!");
				HandleQueues();
				break;
			case HSB_COMING_UP	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_COMING_UP event!");
				HandleQueues();
				break;
			case HSB_ACTIVE	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_ACTIVE event!");
				break;
			case HSB_ACT_TO_SBY	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_ACT_TO_SBY event!");
				HandleQueues();
				break;
			case HSB_DOWN	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_DOWN event!");
				Terminate(FALSE);
				break;
			case HSB_STANDALONE	:
				ctrl_sta = prgEvent->command;
				dbg(TRACE,"PQS: received HSB_STANDALONE event!");
				break;
			case REMOTE_DB :
				break;
			case SHUTDOWN	:
				dbg(TRACE,"PQS: received SHUTDOWN event!");
				Terminate(FALSE);
				break;
			case RESET		:
				dbg(TRACE,"PQS: received RESET event!");
				ilRc = Reset();
				if (ilRc == RC_FAIL)
				Terminate(FALSE);
				break;
			case EVENT_DATA	:
				if ((ctrl_sta == HSB_STANDALONE) ||
					(ctrl_sta == HSB_ACTIVE) ||
					(ctrl_sta == HSB_ACT_TO_SBY))
				{
					//ShwoIPCQueMessage();
				}
				else
				{
					dbg(TRACE,"poll_q_and_sock: wrong HSB-status <%d>",ctrl_sta);
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
				}
				break;
			case TRACE_ON :
				dbg_handle_debug(prgEvent->command);
				break;
			case TRACE_OFF :
				dbg_handle_debug(prgEvent->command);
				break;
			default:
				dbg(TRACE,"MAIN: unknown event");
				DebugPrintItem(TRACE,prgItem);
				DebugPrintEvent(TRACE,prgEvent);
			break;
		}
	}
}

static void ShwoIPCQueMessage()
{
	char * pclFunc = "ShwoIPCQueMessage";

	int  ilRC = RC_SUCCESS;
	int  ilQueRc = RC_SUCCESS;
	char *pclSelection = NULL;
	char *pclFields = NULL;
	char *pclData = NULL;
	char *pclTmpPtr=NULL;
	BC_HEAD *bchd = NULL;
	CMDBLK  *cmdblk = NULL;
	int ilLen;

	int ilCount = 0;

	bchd  = (BC_HEAD *) ((char *)prgEvent + sizeof(EVENT));
	cmdblk= (CMDBLK  *) ((char *)bchd->data);

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TWS",CFG_STRING,pcgTwStart);

	if (ilRC != RC_SUCCESS)
	{
		strcpy(pcgTwStart,cmdblk->tw_start);
	}
	strcpy(pcgTwEnd,cmdblk->tw_end);

	pclSelection = cmdblk->data;
	pclFields = (char *)pclSelection + strlen(pclSelection) + 1;
	pclData = (char *)pclFields + strlen(pclFields) + 1;

	dbg(DEBUG,"========================= START ===============================");

	dbg(DEBUG,"Command:   <%s>",cmdblk->command);
	dbg(DEBUG,"Selection: <%s>",pclSelection);
	dbg(DEBUG,"Fields:    <%s>",pclFields);
	dbg(DEBUG,"Data:      \n<%s>",pclData);
	dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
	dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);
}

static int FindMsgId(char *pcpData, char *pcpMsgId, int Mode)
{
	char * pclFunc = "FindMsgId";
	int  ilRC = RC_SUCCESS;
    char pclKeyFnd[32]="\0";
    char pclTmpBuf[32]="\0";
    char *pclMsgIdBgn=NULL;
    char *pclMsgIdEnd=NULL;
    int ilMsgId = 0;

    if (strstr(pcpData,"IED") == 0)
    {
        if (Mode == 1)
            sprintf(pclKeyFnd, "acknowledge messageId=");
        else
            sprintf(pclKeyFnd, "messageId=");

        ilMsgId = 0;
        pclMsgIdBgn = strstr(pcpData, pclKeyFnd);
        if (pclMsgIdBgn != NULL)
        {
            pclMsgIdBgn += strlen(pclKeyFnd)+1;
            pclMsgIdEnd = strstr(pclMsgIdBgn, "\"");
            if ((pclMsgIdBgn != NULL) && (pclMsgIdEnd >= pclMsgIdBgn))
            {
                strncpy(pclTmpBuf, pclMsgIdBgn,(pclMsgIdEnd - pclMsgIdBgn));
                pclTmpBuf[pclMsgIdEnd - pclMsgIdBgn] = '\0';
                ilMsgId = atoi(pclTmpBuf);
                dbg(DEBUG,"<%s> Received the message id <%d>",pclFunc, ilMsgId);
            }
        }

        sprintf(pcpMsgId, "%d", ilMsgId);
    }
    else
    {
        strncpy(pcpMsgId,pcpData+3,2);
    }

	return ilRC;
}

static void buildHB_message(char *pcpDataBuf,char *pcpKeepAliveFormat, char *pcpHB_Header,int ipObjID,int ipMsgID, int ipMsgNoForHeartbeat)
{
	char *pclFunc = "buildHB_message";
	sprintf(pcpDataBuf,pcpKeepAliveFormat,pcpHB_Header,ipObjID,ipMsgID,ipMsgNoForHeartbeat);

	dbg(TRACE,"%s pcpHB_Header<%s> Message<%s>",pclFunc, pcpHB_Header, pcpDataBuf);
}

static void SendUDPMessage(char *pIP, int pPort, char *pMessage)
{
	int sockfd;
	struct sockaddr_in their_addr;
	struct hostent *he;
	int numbytes;

	if ((he = gethostbyname(pIP)) == NULL)
	{
		dbg(TRACE,"SendUDPMessage: gethostbyname error");
		return;
	}

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		dbg(TRACE,"SendUDPMessage: create socket error");
		close(sockfd);
		return;
	}

	their_addr.sin_family = AF_INET;
	/* short, network byte order */
	their_addr.sin_port = htons(pPort);
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);

	/* zero the rest of the struct */
	memset(&(their_addr.sin_zero), '\0', 8);

	if((numbytes = sendto(sockfd, pMessage, strlen(pMessage), 0, (struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1)
	{
		dbg(TRACE,"SendUDPMessage: Send Message error");
	}
	close(sockfd);
}

static int resetUDP(int socket, char *Pservice, char *Phostname)
{
    int rc=RC_FAIL;
    int i = 0;
    struct hostent *hp;
    struct sockaddr_in dummy;
    int serr=0;
    struct servent *sp;
    char *pclFunc = "resetUDP";

    if (( hp = gethostbyname( Phostname)) == NULL )
    {
        dbg(TRACE,"%s: ERROR gethostbyname(%s): %s",pclFunc, Phostname, strerror(errno));
        return RC_FAIL;
    }
    dbg(DEBUG,"%s: got hostname %s", pclFunc,Phostname);
    if (( sp = getservbyname(Pservice, NULL) ) == NULL )
    {
        dbg(TRACE,"%s: ERROR getservbyname(%s): %s",pclFunc, Pservice, strerror(errno) );
        return RC_FAIL;
    }
    dbg(DEBUG,"%s: got service %s", pclFunc, Pservice);

    dummy.sin_port = sp->s_port;
    memcpy((char *) &dummy.sin_addr, *hp->h_addr_list, hp->h_length);
    dummy.sin_family = AF_UNSPEC;

	dbg(TRACE,"%s Set socket sin_famliy as AF_UNSPEC",pclFunc);
    rc = connect(socket, (struct sockaddr *) &dummy, sizeof(dummy));
    if (rc < 0)
    {
        dbg(TRACE,"%s: ERROR reset connect: %s", pclFunc, strerror(errno));
        return RC_FAIL;
    }

    dbg(DEBUG,"%s: got udp server connection",pclFunc);
    return RC_SUCCESS;
}
