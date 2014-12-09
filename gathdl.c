#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Interface/gathdl.c 1.3a 2014/12/09 14:11 PM Exp  $";
#endif /* _DEF_mks_version */

/******************************************************************************/
/*                                                                            */
/* UFIS AS LIGHDL.C                                                           */
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
#include <cedatime.h>
#include "goshdl.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netdb.h>
#include "tools.h"
#include "helpful.h"
#include "timdef.h"
#include "gathdl.h"
/*#include "urno_fn.h"*/
#include "Queue.h"
#include "Queue.c"

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

/*fya 20140218*/
#define TIMEFORMAT 16
#define DB_SUCCESS RC_SUCCESS
#define POS_LEN	4
#define ID_LEN	14
#define NO_END	1
#define NO_START 2
#define START_FOUND 3
#define TEST

#define FIX_LEN 36
#define LEN 56
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
static char pcgMsgNo[10] = "\0";
static char pcgIP[64];
static char pcgPort[64];
static char pcgAdidFilter[64] = "\0";
static fd_set readfds,writefds,exceptfds;

/*static char  cgHopo[8];*/

/*entry's from configfile*/
static char	pcgConfigFile[512];
static char	pcgCfgBuffer[512];
static EVENT *prgOutEvent = NULL;
static char	pcgServerChars[110];
static char	pcgClientChars[110];
static char	pcgUfisConfigFile[512];
static char pcgSendAckFormat[M_BUFF];
static char	pcgTest[512];

static char pcgTimeWindowLowerLimit[64];
static char pcgTimeWindowUpperLimit[64];

#ifndef _SOLARIS
static struct sockaddr_in my_client;
#else
static struct sockaddr my_client;
#endif

static BOOL	bgAlarm = FALSE;				/* global flag for time-out socket-read */
static int	igSock = 0;							/* global tcp/ip socket for send/receive */

static int	igSendAck;							/* Flag for sending ACKs (0 or 1) */

static CFG	prgCfg;

static FILE	*pgReceiveLogFile = NULL;	/* log file pointer for all received messages */
static FILE	*pgSendLogFile = NULL;		/* log file pointer for all sent messages */


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

static int igConfigShift;

static char	pcgCurSendData[4096] = "\0"; /* global CurSendData */
static char	pcgSendHeartBeatExpTime[64] = "\0";  /* global Send heartbeat expect time */
static char	pcgRcvHeartBeatExpTime[64] = "\0";   /* global Receive heartbeat expire time */
static char	pcgReconExpTime[64] = "\0";   /* global Receive heartbeat expect time */

static char	pcgSendMsgId[1024] = "\0";

static int	igModID_ConMgr = 0;
static int	igModID_ConSTB = 0;

static int	igConnected = FALSE;   /* global socket Connect status  */
static int	igOldCnnt = FALSE;

static char pcgCurrentTime[TIMEFORMAT] = "\0";
static int igTimeRange = 0;
static long lgEvtCnt = 0;
static int igBatch = 0;
static int igDisableAck = 0;
Queue *pcgQueue = NULL;

static int igSentNullMsg;

/*
@fya 20140310
*/
static int igNextAllocArrUpperRange = 0;
static int igNextAllocDepUpperRange = 0;

static int	Init_Handler();
static int	Reset(void);                        /* Reset program          */
static void	Terminate(int);                     /* Terminate program      */
static void	HandleSignal(int);                  /* Handles signals        */
static void	HandleErr(int *);                     /* Handles general errors */
static void	HandleQueErr(int);                  /* Handles queuing errors */
static int	HandleInternalData(void);           /* Handles event data     */
static void	HandleQueues(void);                 /* Waiting for Sts.-switch*/
static char pcgHbtPrefix[16] = "\0";
static char	pcgSuffix[16] = "\0";
static char	pcgSeparator[16] = "\0";
extern char  cgHopo[8];
static int igTimeWindowUpperLimit;
static int igTimeWindowLowerLimit;
/******************************************************************************/
/* Function prototypes														  */
/******************************************************************************/

static int	GetQueues();
static int	GetConfig();
static void	CloseTCP(void);
static int	CheckData(char *pcpData, int ipIdx);
static int	GetData(char *pcpMethod, int ipIdx, char *pcpName);
static int	poll_q_and_sock();
static int	Receive_data(int ipSock, int ipAlarm);
static int	Send_data(int ipSock, char *pcpData);
static void	snapit(void *pcpBuffer, long lpDataLen, FILE *pcpDbgFile);
static int	GetCfgEntry(char *pcpFile,char *pcpSection,char *pcpEntry,short spType,char **pcpDest,int ipValueType,char *pcpDefVal);
static int	CheckValue(char *pcpEntry,char *pcpValue,int ipType);
static int	SendKeepAliveMsg();
static int	SendAckMsg();
static int	GetMsgno(char *pcpBuff);
static int tcp_socket();
static int SendHeartbeatMsg(char *pcpLastTimeSendingTime);
void FormatTime(char *pcpTime, char *pcpLastTimeSendingTime);
static int ReceiveACK(int ipSock,int ipTimeOut);
static int Sockt_Reconnect(void);
static int tcp_open_connection (int socket, char *Pservice, char *Phostname);
static int FindMsgId(char *pcpData, char *pcpMsgId, int Mode);
static int SendRST_Command(void);
static int TrimSpace( char *pcpInStr );
static void BuildDepPart(SENT_MSG *rpSentMsg, char *pcpPstdNewData, char *pcpStodaNewData, char *pcpEtdiNewData, char *pcpOfblNewData);
static void ShowMsgStruct(SENT_MSG rpSentMsg);
static void PutDefaultValue(SENT_MSG *rpSentMsg);
static int GetSeqFlight(SENT_MSG *rpSentMsg,char *pcpADFlag);
/*static int RunSQL(char *pcpSelection, char *pcpData );*/
static void BuildSentData(char *pcpDataSent,SENT_MSG rpSentMsg);
static void BuildAckData(char *pcpAckSent, char *pclRecordURNO);
static void GetAllValidGatesBuildWhereClause(char *pcpWhere);
static void GetAllValidGatesBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void GetAllValidGates(char *pcpAllValidSlots,int *ipSoltNumber);
static int StoreSentData(char *pcpDataSent, char *pcpUaft, SENT_MSG rpMsg, char *pcpRecordURNO);
static int StoreAckData(char *pcpAckSent, char *pcpRecordURNO);
static void SearchTowingFlightBuildWhereClause(char *pcpRkey, char *pcpRegn, char *pcpWhere, char *pcpRefTime);
static void SearchTowingFlightBuildFullQuery(char *pcpSqlBuf, char *pcpWhere);
static void Upd_ACK_FlightBuildWhereClause(char *pcpWhere,char *pclRecvBuffer);
static void Upd_ACK_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void Get_FlightDataBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static int TrimZero( char *pcpInStr );
static void PutDefaultValueWithPos(SENT_MSG *rpSentMsg, char *pcpParkingStand);
static void print(Item i);
static void Get_FlightDataWhereClause(char *pcpWhere,char *pcpUrno);
static int Send_data_wo_separator(int ipSock,char *pcpData);
static int putData(int ipSock,char *pcpData);
static void BuildHbtData(char *pcpAckSent);
static int adidFilter(char *pcpAdidNewData);
static int SendBatchFlights(void);
static int calculateTimeWindow(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
static int isInTimeWindow(char *pcpTimeVal, char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit);
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

        igSock = 0;
  	    ilRc = OpenServerSocket();
        if (ilRc == RC_SUCCESS)
        {
          igConnected = TRUE;
          igOldCnnt = TRUE;

        }
        else
        {
          igConnected = FALSE;
          igOldCnnt = FALSE;

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
						/*SendRST_Command();*/
					}
					else
					{
                        if ( ((igConnected == FALSE) && (strcmp(pclCurrentTime, pcgReconExpTime) >= 0)) || (igConnected == TRUE) )
                        {
                            ilRc = Sockt_Reconnect();
                            /*SendRST_Command();*/
						}
					}
				}

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
	int ilCnt = 0 ;
	int	ilRc = RC_SUCCESS;            /* Return code */
	char *pclFunc = "Init_Handler";

	/*fya 20140221 Init DB*/
	do
  {
	  ilRc = init_db();
	  if (ilRc != RC_SUCCESS)
	  {
	      check_ret(ilRc);
	      dbg(TRACE,"<%s>: init_db() failed! waiting 6 sec ...",pclFunc);
	      sleep(6);
	      ilCnt++;
	  } /* end of if */
  }
  while((ilCnt < 10) && (ilRc != RC_SUCCESS));

  if(ilRc != RC_SUCCESS)
  {
      dbg(TRACE,"<%s>: init_db() failed! waiting 60 sec ...",pclFunc);
      sleep(60);
      exit(2);
  }
  else
  {
      dbg(TRACE,"<%s>: init_db() OK!",pclFunc);
  } /* end of if */

	GetQueues();

	/* reading default home-airport from sgs.tab */
	memset(pcgHomeAp,0x00,sizeof(pcgHomeAp));
	ilRc = tool_search_exco_data("SYS","HOMEAP",pcgHomeAp);
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"<%s> : No HOMEAP entry in sgs.tab: EXTAB! Please add!",pclFunc);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"<%s> : HOMEAP = <%s>",pcgHomeAp,pclFunc);
	}

	/* reading default table-extension from sgs.tab */
	memset(pcgTabEnd,0x00,sizeof(pcgTabEnd));
	ilRc = tool_search_exco_data("ALL","TABEND",pcgTabEnd);
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE,"<%s> : No TABEND entry in sgs.tab: EXTAB! Please add!",pclFunc);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"<%s> : TABEND = <%s>",pclFunc,pcgTabEnd);
		memset(pcgTwEnd,0x00,XS_BUFF);
		sprintf(pcgTwEnd,"%s,%s,%s",pcgHomeAp,pcgTabEnd,mod_name);
		dbg(TRACE,"<%s> : TW_END = <%s>",pclFunc,pcgTwEnd);
	}

	if (ilRc = GetConfig() == RC_FAIL)
	{
		dbg(TRACE,"<%s>: Configuration error, terminating",pclFunc);
		Terminate(30);
	}

	calculateTimeWindow(pcgTimeWindowLowerLimit,pcgTimeWindowUpperLimit);

    /*Initial internal queue for message id*/
	pcgQueue = InitQueue();

	return (ilRc);
} /* end of Init_Handler */

/* fya 20140221 */
static void GetAllValidGates(char *pcpAllValidGates,int *ipGateNumber)
{
	int ilRC = RC_SUCCESS;
	char *pclFunc = "GetAllValidGates";
	short slLocalCursor = 0, slFuncCode = 0;
	char pclParkingStand[16] = "\0";
	char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0";

	memset(pcpAllValidGates,0,sizeof(pcpAllValidGates));

	/* Build Where Clause */
	GetAllValidGatesBuildWhereClause(pclWhere);

	/* Build Full Sql query */
	GetAllValidGatesBuildFullQuery(pclSqlBuf,pclWhere);

	slLocalCursor = 0;
	slFuncCode = START;
	while(sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS)
    {
        slFuncCode = NEXT;

        memset(pclParkingStand,0,sizeof(pclParkingStand));
        get_fld(pclSqlData,FIELD_1,STR,20,pclParkingStand);

        TrimSpace(pclParkingStand);
        strcat(pcpAllValidGates,pclParkingStand);
        strcat(pcpAllValidGates,",");

        /*dbg(DEBUG,"<%s>pcpAllValidGates<%s>",pclFunc,pcpAllValidGates);*/
    }
    close_my_cursor(&slLocalCursor);

  pcpAllValidGates[strlen(pcpAllValidGates)-1] = '\0';

  dbg(TRACE,"<%s>\n******************All Slot******************",pclFunc);
  dbg(TRACE,"<%s>pcpAllValidGates<%s>",pclFunc,pcpAllValidGates);
  dbg(TRACE,"<%s>******************All Slot******************\n",pclFunc);

  *ipGateNumber = GetNoOfElements(pcpAllValidGates, ',');
}

/* fya 20140221 */
static void GetAllValidGatesBuildWhereClause(char *pcpWhere)
{
	char *pclFunc = "GetAllValidGatesBuildWhereClause";

	char pclWhere[2048] = "\0";

	memset(pcgCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

	sprintf(pclWhere,"WHERE (VAFR < '%s' and (VATO > '%s' or VATO = ' ' or VATO IS NULL)) order by PNAM", pcgCurrentTime,pcgCurrentTime);

  strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

/* fya 20140221 */
static void GetAllValidGatesBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "GetAllValidGatesBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	sprintf(pclSqlBuf, "SELECT distinct(PNAM) FROM PSTTAB %s", pcpWhere);

	strcpy(pcpSqlBuf,pclSqlBuf);

	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>\n",pclFunc,pcpSqlBuf);
}

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


    dbg(TRACE,"Make the queue empty");
    ClearQueue(pcgQueue);
    dbg(TRACE,"Destruct the queue");
    DestroyQueue(pcgQueue);

	CloseTCP();

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
				CloseTCP();
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
	char pclDataBuf[M_BUFF] = "\0";

	int ilCount = 0;
	int ilCountTowing = 0;
	int ilCountT = 0;
	int ilCountP = 0;

	int ilItemNo = 0;
	int ilItemNo1 = 0;
	int ilNoEleField = 0;
	int ilNoEleNewData = 0;
	int ilNoEleOldData = 0;
	int ilFlag = 0;
	char pclAdidOldData[2] = "\0";
	char pclAdidNewData[2] = "\0";

	char pclOnblNewData[16] = "\0";
	char pclOnblOldData[16] = "\0";
	char pclOfblNewData[16] = "\0";
	char pclOfblOldData[16] = "\0";

    char pclGta1OldData[16] = "\0";
	char pclGta1NewData[16] = "\0";
	char pclGtd1OldData[16] = "\0";
	char pclGtd1NewData[16] = "\0";

	char pclFltiOldData[16] = "\0";
	char pclFltiNewData[16] = "\0";

	char pclFlnoNewData[16] = "\0";
	char pclFlnoOldData[16] = "\0";

    char pclTifaNewData[16] = "\0";
    char pclTifaOldData[16] = "\0";
    char pclTifdNewData[16] = "\0";
    char pclTifdOldData[16] = "\0";

    char pclTmpTimeUpper[TIMEFORMAT] = "\0";
    char pclTmpTimeLower[TIMEFORMAT] = "\0";
    char pclTmpTimeNow[TIMEFORMAT] = "\0";

    char pclUrnoSelection[50] = "\0";
    char pclRecordURNO[50] = "\0";
	char pclUrnoOldData[50] = "\0";
	char pclUrnoNewData[50] = "\0";
    char pclDataSent[1024] = "\0";
	char pclAckSent[1024] = "\0";
	char pclNewData[512000] = "\0";
    char pclOldData[512000] = "\0";

    SENT_MSG rlSentMsg;

	/*@fya 20140310*/
	int ilNewTowingFlight = FALSE;
	/*
	@fya 20140311
	*/
	char pclUrnoP[16] = "\0";
	char pclRurnP[16] = "\0";
	char pclTypeP[16] = "\0";
	char pclTimeP[16] = "\0";
	char pclStatP[16] = "\0";
	char pclRtabP[16] = "\0";
	char pclRfldP[16] = "\0";
	char pclFval_OFB[512] = "\0";
	char pclFval_ONB[512] = "\0";

	char pclTmpMsg[4096] = "\0";

	char clTmpStr[1] = "\0";
	clTmpStr[0] = 0x04;

	memset(pclRecordURNO,0,sizeof(pclRecordURNO));
	memset(pclDataSent,0,sizeof(pclDataSent));

	memset(&rlSentMsg,0,sizeof(rlSentMsg));

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

	pclTmpPtr = strstr(pclSelection, "\n");
 	if (pclTmpPtr != NULL)
 	{
    *pclTmpPtr = '\0';
  	pclTmpPtr++;
    strcpy(pclUrnoSelection, pclTmpPtr);
  }

  strcpy(pclNewData, pclData);

  pclTmpPtr = strstr(pclNewData, "\n");
  if (pclTmpPtr != NULL) {
      *pclTmpPtr = '\0';
      pclTmpPtr++;
      strcpy(pclOldData, pclTmpPtr);
  }

	dbg(DEBUG,"========================= START <%10.10d> ===============================",lgEvtCnt);

	dbg(DEBUG,"Command:   <%s>",cmdblk->command);
	dbg(DEBUG,"Selection: <%s> URNO<%s>",pclSelection,pclUrnoSelection);
	dbg(DEBUG,"Fields:    <%s>",pclFields);
	dbg(DEBUG,"New Data:  <%s>",pclNewData);
	dbg(DEBUG,"Old Data:  <%s>",pclOldData);
	dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
	dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);

	lgEvtCnt++;

	if (strcmp(cmdblk->command,"RST") == 0)
	{
		dbg(DEBUG,"RST command for sending the batch flights");

		if (igBatch > 0)
		{
			SendBatchFlights();
		}
	}
	else if (strcmp(cmdblk->command,"NTI") == 0)
	{
		dbg(TRACE,"<%s> The command is NTI, calculate the time window",pclFunc);
        calculateTimeWindow(pcgTimeWindowLowerLimit,pcgTimeWindowUpperLimit);
	}
	else if (strcmp(cmdblk->command,"UFR") == 0 || strcmp(cmdblk->command,"IFR") == 0 || strcmp(cmdblk->command,"DFR") == 0)
	{
		dbg(DEBUG,"<%s> %s command is received", pclFunc,cmdblk->command);

		if( isInTimeWindow(pclTimeWindowRefFieldVal, pcgTimeWindowLowerLimit, pcgTimeWindowUpperLimit) == RC_FAIL )
        {
            return RC_FAIL;
        }

		PutDefaultValue(&rlSentMsg);

		/*Common Part->Getting data*/
		TrimSpace(pclUrnoSelection);
		if (strlen(pclUrnoSelection) == 0)
		{
			dbg(TRACE,"<%s>The urno is not included in the selection list, can not proceed",pclFunc);
			return RC_FAIL;
		}

		ilNoEleField    = GetNoOfElements(pclFields, ',');
		ilNoEleNewData  = GetNoOfElements(pclNewData, ',');
		ilNoEleOldData  = GetNoOfElements(pclOldData, ',');

		dbg(DEBUG,"<%s>ilNoEleField<%d> ilNoEleNewData<%d> ilNoEleOldData<%d>",pclFunc,ilNoEleField,ilNoEleNewData,ilNoEleOldData);
		if ((ilNoEleField != ilNoEleNewData) || (ilNoEleNewData != ilNoEleOldData))
		{
			dbg(TRACE,"The number of new/old data list and field list is not matched");
			return RC_FAIL;
		}

		if(getValue(pclFields, "URNO", pclNewData, pclOldData, pclUrnoNewData, pclUrnoOldData) == RC_FAIL)
			return RC_FAIL;

		if(getValue(pclFields, "ADID", pclNewData, pclOldData, pclAdidNewData, pclAdidOldData) == RC_FAIL)
			return RC_FAIL;

        if(getValue(pclFields, "FLNO", pclNewData, pclOldData, pclFlnoNewData, pclFlnoOldData) == RC_FAIL)
			return RC_FAIL;
        strcpy(rlSentMsg.pclFlno,pclFlnoNewData);

        if(adidFilter(pclAdidNewData) == RC_FAIL)
        {
            return RC_FAIL;
        }

		switch(pclAdidNewData[0])
		{
			case 'A':
				strcpy(rlSentMsg.pclAdid,pclAdidNewData);

				/*get GTA1*/
				if(getValue(pclFields, "GTA1", pclNewData, pclOldData, pclGta1NewData, pclGta1OldData) == RC_FAIL)
				    return RC_FAIL;
                strcpy(rlSentMsg.pclGate,pclGta1NewData);

				if(getValue(pclFields, "TIFA", pclNewData, pclOldData, pclTifaNewData, pclTifaOldData) == RC_FAIL)
					return RC_FAIL;
                strcpy(rlSentMsg.pclBestTime,pclTifaNewData);

				break;
			case 'D':
				strcpy(rlSentMsg.pclAdid,pclAdidNewData);

				/*get GTD1*/
				if(getValue(pclFields, "GTD1", pclNewData, pclOldData, pclGtd1NewData, pclGtd1OldData) == RC_FAIL)
					return RC_FAIL;
                strcpy(rlSentMsg.pclGate,pclGtd1NewData);

                if(getValue(pclFields, "TIFD", pclNewData, pclOldData, pclTifdNewData, pclTifdOldData) == RC_FAIL)
					return RC_FAIL;
                strcpy(rlSentMsg.pclBestTime,pclTifdNewData);

				break;
			default:
				dbg(TRACE,"<%s>: Invalid ADID <%s>",pclFunc,pclAdidNewData);
				return RC_FAIL;
				break;
		}

		getValue(pclFields, "FLTI", pclNewData, pclOldData, pclFltiNewData, pclFltiOldData);
        strcpy(rlSentMsg.pclFlightID,pclFltiNewData);

        getValue(pclFields, "ONBL", pclNewData, pclOldData, pclOnblNewData, pclOnblOldData);
        strcpy(rlSentMsg.pclOnbl,pclOnblNewData);

        getValue(pclFields, "OFBL", pclNewData, pclOldData, pclOfblNewData, pclOfblOldData);
        strcpy(rlSentMsg.pclOfbl,pclOfblNewData);


		ShowMsgStruct(rlSentMsg);
        BuildSentData(pclDataSent,rlSentMsg);
		/*strcat(pclDataSent,"\n");*/
        dbg(TRACE,"\n\n<%s> pclDataSent<%s>",pclFunc,pclDataSent);
		StoreSentData(pclDataSent,pclUrnoNewData,rlSentMsg,pclRecordURNO);
        strcpy(pcgSendMsgId,pclRecordURNO);

        BuildAckData(pclAckSent,pclRecordURNO);
        /*strcat(pclAckSent,"\n");*/
        dbg(TRACE,"\n\n<%s> pclAckSent<%s>",pclFunc,pclAckSent);
        StoreAckData(pclAckSent,pclRecordURNO);

        EnQueue(pcgQueue, atoi(pcgSendMsgId));
        dbg(TRACE,"%s Size<%d> Traverse all element in queue:", pclFunc, GetSize(pcgQueue));
        QueueTraverse(pcgQueue,print);

        putData(igSock,pclDataSent);
        Send_data_wo_separator(igSock,pclAckSent);
	}
	else
	{
		dbg(TRACE,"<%s>: Invalid command <%s>",pclFunc,cmdblk->command);
		ilRC = RC_FAIL;
	}
    dbg(DEBUG,"========================= END <%10.10d> ================================", lgEvtCnt);
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
    {
        igHeartBeatIntv = atoi(pclTmpBuf);
    }
    else
    {
        igHeartBeatIntv = 15;
    }
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

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_RANGE",CFG_STRING,pclTmpBuf);
    if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
      igTimeRange = atoi(pclTmpBuf);
    else
      igTimeRange = 30;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","SENT_NULL_MSG",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igSentNullMsg = atoi(pclTmpBuf);
    else
      igSentNullMsg = 1;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","NEXT_ALLOC_DEP_UPPER",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igNextAllocDepUpperRange = atoi(pclTmpBuf);
    else
      igNextAllocDepUpperRange = 3;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","NEXT_ALLOC_ARR_UPPER",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igNextAllocArrUpperRange = atoi(pclTmpBuf);
    else
      igNextAllocArrUpperRange = 3;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","BATCH",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igBatch = atoi(pclTmpBuf);
    else
      igBatch = 0;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","DISABLE_ACK",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igDisableAck = atoi(pclTmpBuf);
    else
      igDisableAck = 0;

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CONFIG_SHIFT",CFG_STRING,pclTmpBuf);
    if (ilRC == RC_SUCCESS)
      igConfigShift = atoi(pclTmpBuf);
    else
      igConfigShift = 1;

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","SUFFIX",CFG_STRING,pcgSuffix);
    if (ilRC != RC_SUCCESS)
    {
    memset(pcgSuffix,0,sizeof(pcgSuffix));
    dbg(TRACE,"SUFFIX is not set");
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","Separator",CFG_STRING,pcgSeparator);
    if (ilRC != RC_SUCCESS)
    {
        memset(pcgSeparator,0,sizeof(pcgSeparator));
        dbg(TRACE,"Separator is not set");
    }

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ADID_FILTER",CFG_STRING,pcgAdidFilter);
    if (ilRC != RC_SUCCESS)
    {
        strcpy(pcgAdidFilter,"A,D");
        dbg(TRACE,"Separator is not set");
    }

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","IP",CFG_STRING,pcgIP);
	if(strlen(pcgIP)==0)
	{
		dbg(TRACE,"IP is null<%s>",pcgIP);
	}
	else
	{
		dbg(TRACE,"IP is not null<%s>",pcgIP);
	}

    ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TEST",CFG_STRING,pcgTest);
	if (ilRC != RC_SUCCESS)
    {
        memset(pcgTest,0,sizeof(pcgTest));
        dbg(TRACE,"Test is not set");
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

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","PORT",CFG_STRING,pcgPort);
	if(strlen(pcgPort)==0)
	{
		dbg(TRACE,"Port is null<%s>",pcgPort);
	}
	else
	{
		dbg(TRACE,"Port is not null<%s>",pcgPort);
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

	char pclHbtData[1024] = "\0";

	do
	{
		nap(20); /* waiting because of NOT-waiting QUE_GETBIGNW */
		/*---------------------------*/
		/* now looking on ceda-queue */
		/*---------------------------*/

		/* dbg(DEBUG,"time before que<%d>",time(0));*/
                ilReadQueCnt++;
		if ( (ilRc = que(QUE_GETBIGNW,0,mod_id,PRIORITY_3,igItemLen,
				(char *) &prgItem)) == RC_SUCCESS)
		{
                        ilReadQueCnt = 0;
		/*	dbg(DEBUG,"time after1 que<%d>",time(0));*/
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
					/*	if (igSock > 0)*/
						{
							dbg(TRACE,"Calling HandleInternalData");
							ilRc = HandleInternalData();
							if (ilRc == RC_FAIL)
							{
								dbg(TRACE,"PQS: HandleInternalData failed <%d>",ilRc);
				/*				HandleErr(&igSock);*/
							}
						}
                     /*
						else
						{
							dbg(TRACE,"No connection! Ignoring event!");
							dbg(TRACE,"PQS: No GOS-connection! Ignoring event!");
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
         /*  dbg(DEBUG,"Read Que Result <%d>, Retry Read QueCnt<%d>",ilRc, ilReadQueCnt);
            dbg(DEBUG,"time after2 que<%d>",time(0));*/
            if (ilReadQueCnt > 10)
               nap(300);
            if (ilReadQueCnt >= 80)
               ilReadQueCnt=0;
         /*   dbg(DEBUG,"time after3 que<%d>",time(0));*/
		}
		ilRc = RC_FAIL; /* proceed */

		if (igSock > 0)
		{
	    GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	    if (strcmp(pclCurrentTime, pcgSendHeartBeatExpTime) >= 0)
	    {

            dbg(DEBUG,"<%s> Send heartbeat: pclCurrentTime<%s> pcgSendHeartBeatExpTime<%s>",pclFunc,pclCurrentTime,pcgSendHeartBeatExpTime);

	      /*ilRc = SendHeartbeatMsg(pclLastTimeSendingHeartbeat);*/
	      BuildHbtData(pclHbtData);
	      ilRc = Send_data_wo_separator(igSock,pclHbtData);
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

	      /*
	      fya 20140227
	      strcpy(pcgSendHeartBeatExpTime, pclLastTimeSendingHeartbeat);
	      */
	      strcpy(pcgSendHeartBeatExpTime, pclCurrentTime);
	      AddSecondsToCEDATime(pcgSendHeartBeatExpTime,igHeartBeatIntv, 1);
			}
			else
			{
	   		dbg(DEBUG,"<%s> DO not sent heartbeat:pclCurrentTime<%s> pcgSendHeartBeatExpTime<%s>",pclFunc,pclCurrentTime,pcgSendHeartBeatExpTime);
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
	char pclFormatTime[128]="\0";
	char pclDataBuf[L_BUFF];
	int ilNumOfZero = 0;
	int ilCount = 0;
	char pclBuffer[33] = "\0";
	char pclMsgNoForHeartbeat[32] = "\0";
	char pclCurrentTime[64] = "\0";

	memset(pclDataBuf,0,sizeof(pclDataBuf));
	memset(pclMsgNoForHeartbeat,0,sizeof(pclMsgNoForHeartbeat));
	/*FormatTime(pclFormatTime,pcpLastTimeSendingTime);*/

	memset(pclCurrentTime,0x00,32);
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	if(igMsgNoForHeartbeat > 0)
	{
		if(itoa(igMsgNoForHeartbeat,pclBuffer,10) == NULL)
		{
			dbg(TRACE,"<%s> pclBuffer is null",pclFunc);
		}
		else
		{
			if (strlen(pclBuffer) < POS_LEN)
            {
                ilNumOfZero = POS_LEN - strlen(pclBuffer);
                for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
                {
                    strcat(pclMsgNoForHeartbeat,"0");
                }
                strcat(pclMsgNoForHeartbeat,pclBuffer);
            }
            else if (strlen(pclBuffer) == POS_LEN)
            {
                strcpy(pclMsgNoForHeartbeat,pclBuffer);
            }
            else
            {
                strncpy(pclMsgNoForHeartbeat,pclBuffer,4);
            }
		}
	}
	else if(igMsgNoForHeartbeat == 0)
	{
		strcpy(pclMsgNoForHeartbeat,"0000");
	}

	dbg(DEBUG,"<%s> igMsgNoForHeartbeat<%d>pclBuffer<%s>pclMsgNoForHeartbeat<%s>",pclFunc,igMsgNoForHeartbeat,pclBuffer,pclMsgNoForHeartbeat);

	dbg(DEBUG,"<%s>========================= START-HeartBeat ===============================",pclFunc);
	/*dbg(DEBUG,"<%s> Send Heartbeat Msg pclFormatTime<%s>",pclFunc,pclFormatTime);*/

	sprintf(pclDataBuf,pcgKeepAliveFormat,pclCurrentTime);
	dbg(TRACE,"<%s> The length of sent data pclDataBuf is <%d>",pclFunc,strlen(pclDataBuf) - 1);
	/*
	sprintf(pclDataBuf,pcgKeepAliveFormat,pclMsgNoForHeartbeat,"ufis",pclCurrentTime,pclCurrentTime);
	dbg(DEBUG,"<%s> Send Ack Msg MsgNoForACK - pclMsgNoForHeartbeat<%s> pclCurrentTime<%s> strlen(pclDataBuf) - 1 <%d>",pclFunc,pclMsgNoForHeartbeat,pclCurrentTime,strlen(pclDataBuf)-1);
	*/

	if(igMsgNoForHeartbeat == 9999)
    {
		igMsgNoForHeartbeat = 0;
    }

    /*sprintf(pclDataBuf,pcgKeepAliveFormat,pclFormatTime);*/
    igMsgNoForHeartbeat++;

	ilRC = Send_data(igSock,pclDataBuf);

	if(ilRC == RC_SUCCESS)
	{
		dbg(TRACE,"<%s> HeartBeat sending successfully,pclDataBuf<\n%s>",pclFunc,pclDataBuf);
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

		      	if(strstr(pclRecvBuffer,"ACK_")!=0)
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
	int 	ilFlag = 0;
	int 	ilCount = 0;
	int		ilMsgNo = 0;
	int 	ilFound = 0;

    int ilcursor = 38;

	Item ilMsgid = 0;
    char pclMsgid[64] = "\0";

	struct timeval rlTimeout;

	static char	pclRecvBuffer[BUFF] = "\0";
    char pclCurrentTime[64] = "\0";
	char pclTmpUrno[16] = "\0";
    char *pclMsgIdEnd = NULL;
    char *pclMsgIdBgn = NULL;
    char pclTmpBuf[10] = "\0";
    char *pclTmpStart = NULL;
    char pclAckRecv[2048] = "\0";
    char pclSqlBuf[2048] = "\0",pclSqlData[2048] = "\0",pclWhere[2048] = "\0";
    char pclCurSendData[4096] = "\0";
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

			break;
		case -1:
			dbg( TRACE,"<%s>ERROR Code<%d>Description<%s>!! SELECT fails!",pclFunc, errno, strerror(errno) );
			igSock = -1;
            ilRC = RC_FAIL;
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
                    ilNo = recv( ipSock, pclRecvBuffer, sizeof(pclRecvBuffer), 0);
                    dbg(DEBUG,"time after recv<%d>",time(0));

                    if(strcmp(pcgTest,"YES") == 0)
                    {
                        BuildAckData(pclRecvBuffer,"111");
                        ilNo = strlen(pclRecvBuffer);
                    }

                    if( ilNo > 0 )
                    {
                        dbg(TRACE,"<%s> Len<%d>Complete message<%s>",pclFunc,ilNo,pclRecvBuffer);

                        if (ilNo != LEN)
                        {
                            dbg(TRACE,"<%s> Length is not 56, reject the received message",pclFunc);
                            return RC_FAIL;
                        }

                        /*if (strstr(pclRecvBuffer,"ack")!=0)*/
                        if (strstr(pclRecvBuffer,"HBT_") != 0 && strstr(pclRecvBuffer,"ETX_") != 0)
                        {
                            dbg(DEBUG,"<%s> Received the hbt message",pclFunc);
                            /*(void) FindMsgId(pclRecvBuffer, pclTmpBuf, 1);*/
                            /*dbg(DEBUG,"<%s> Received the ack messageId<%s> : Wait ACK MsgId<%s>",pclFunc, pclTmpBuf,pcgSendMsgId);*/
                            dbg(DEBUG,"<%s> HBT Msg<%s>",pclFunc,pclRecvBuffer);

                            /*
                            if (strcmp(pcgSendMsgId, pclTmpBuf) == 0)
                            */
                            /*
                            Assume the opponent echo the received message back for ack
                            */
                            GetServerTimeStamp( "UTC", 1, 0, pcgRcvHeartBeatExpTime);
                            AddSecondsToCEDATime(pcgRcvHeartBeatExpTime, igHeartBeatTimOut, 1);
                        }

                        if (strstr(pclRecvBuffer,"ACK_") != 0 && strstr(pclRecvBuffer,"ETX_") != 0 /*&& strstr(pclRecvBuffer,"sps_") != 0*/)
                        {
                            /*(void) FindMsgId(pclRecvBuffer, pclTmpBuf, 0);
                            strcpy(pcgMsgNoForACK, pclTmpBuf);
                            pclTmpHead = strstr(pclRecvBuffer,"HBT_");
                            strncpy(pclTmp,pclTmpHead+4,4);*/

                            /* Find the ACK msgid*/
                            pclTmpStart = strstr(pclRecvBuffer,"ACK_");

                            /*
                            @fya
                            modify the hardcoded number into macros
                            */
                            /************************************/
                            for (ilCount = 0; ilCount < (strlen(pclRecvBuffer) / LEN); ilCount++)
                            {

                                GetServerTimeStamp( "UTC", 1, 0, pcgRcvHeartBeatExpTime);
                                AddSecondsToCEDATime(pcgRcvHeartBeatExpTime, igHeartBeatTimOut, 1);

                                memset(pclTmpUrno,0,sizeof(pclTmpUrno));

                                if (ilCount == 0)
                                    /*strncpy(pclTmpUrno,pclTmpStart + 36,14);*/
                                    strncpy(pclTmpUrno, pclTmpStart + ilcursor, 14);
                                else
                                {
                                    ilcursor += LEN;
                                    if (ilcursor <= strlen(pclRecvBuffer))
                                        strncpy(pclTmpUrno, pclTmpStart + ilcursor, 14);
                                }

                                TrimZero(pclTmpUrno);

                                /*dbg(DEBUG,"<%s> Received the ack message<%s> The ack message id is <%s>",pclFunc,pclRecvBuffer,pclTmpUrno);*/
                                dbg(DEBUG,"<%s> Received the ack message<%s> Msgid<%s>",pclFunc,pclRecvBuffer,pclTmpUrno);

                                /*if (atoi(pclTmpUrno) == 0)*/
                                if (strlen(pclTmpUrno) <= 0 || atoi(pclTmpUrno) == 0)
                                {
                                    dbg(TRACE,"Received ack message id <%s> is invalid number", pclTmpUrno);
                                    ilRC = RC_FAIL;
                                }
                                else
                                {
                                    /*
                                    #ifdef TEST
                                        strcpy(pcgSendMsgId,"5149");
                                    #endif
                                    */
                                    memset(pclMsgid,0,sizeof(pclMsgid));

                                    DeQueue(pcgQueue, &ilMsgid);
                                    itoa(ilMsgid,pclMsgid,10);
                                    dbg(TRACE,"%s The waited MsgId is <%s>", pclFunc, pclMsgid);

                                    if(strcmp(pcgTest,"YES") == 0)
                                    {
                                        strcpy(pclTmpUrno,pclMsgid);
                                    }

                                    /*if ( strlen(pclTmpUrno) > 0 && strlen(pcgSendMsgId) > 0 && strcmp(pclTmpUrno, pcgSendMsgId) == 0)*/
                                    if ( strlen(pclTmpUrno) > 0 && strlen(pclMsgid) > 0 && strcmp(pclTmpUrno, pclMsgid) == 0)
                                    {
                                        igSckWaitACK = FALSE;
                                        igSckTryACKCnt = 0;

                                        dbg(TRACE,"<%s> Received ack message id <%s> equals sent one<%s>", pclFunc, pclTmpUrno, pclMsgid);

                                        Upd_ACK_FlightBuildWhereClause(pclWhere,pclTmpUrno);
                                        Upd_ACK_FlightBuildFullQuery(pclSqlBuf,pclWhere);

                                        ilRC = RunSQL(pclSqlBuf,pclSqlData);
                                        if (ilRC != DB_SUCCESS)
                                        {
                                            dbg(DEBUG, "<%s> Updating DORTAB for ack fails", pclFunc);
                                            ilRC = RC_FAIL;
                                        }
                                        else
                                        {
                                            dbg(DEBUG, "<%s> Updating DORTAB for ack succeeds", pclFunc);
                                        }

                                        /*
                                        @fya 20140303
                                        Comment out below statements, cos once lighdl receives the ack message, no need to send the ack message to sps again*/
                                        /*
                                        dbg(DEBUG,"<%s> Sending ACK message",pclFunc);
                                        SendAckMsg();
                                        */
                                        ilRC = RC_SUCCESS;
                                    }
                                    else
                                    {
                                        /*dbg(TRACE,"<%s> Received ack message id <%s> does not equal sent one<%s>", pclFunc, pclTmpUrno, pcgSendMsgId);*/
                                        dbg(TRACE,"<%s> Received ack message id <%s> does not equal sent one<%s>", pclFunc, pclTmpUrno, pclMsgid);
                                        /*strcpy(pcgMsgNo,pclMsgid);*/

                                        return RC_FAIL;
                                    }

                                    dbg(TRACE,"%s Size<%d> Traverse all element in queue:", pclFunc, GetSize(pcgQueue));
                                    QueueTraverse(pcgQueue,print);
                                }
                            }/*for loop*/
                            /************************************/
                        }

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
                    /*SendRST_Command();*/

                if (igSock > 0)
                {
                  dbg(DEBUG,"%s line<%d> send data",pclFunc,__LINE__);
                  /*
                  @fya 20140304
                  strcat(pcgCurSendData,"\n");
                  */

                    Get_FlightDataWhereClause(pclWhere,pclMsgid);
                    Get_FlightDataBuildFullQuery(pclSqlBuf,pclWhere);

                    ilRC = RunSQL(pclSqlBuf, pclCurSendData);
                    if (ilRC != DB_SUCCESS)
                    {
                        dbg(DEBUG, "<%s> Updating DORTAB for ack fails", pclFunc);
                        ilRC = RC_FAIL;
                    }
                    else
                    {
                        dbg(DEBUG, "<%s> Updating DORTAB for ack succeeds", pclFunc);
                    }

                    if(pclCurSendData[strlen(pclCurSendData)-1] == 0x04)
                    {
                        dbg(TRACE,"%s Resending data-1 <%s>",pclFunc, pclCurSendData);
                        Send_data_wo_separator(igSock,pclCurSendData);
                    }
                    else if(pclCurSendData[strlen(pclCurSendData)-1] == '\n')
                    {
                        dbg(TRACE,"%s Resending data-2 <%s>",pclFunc, pclCurSendData);
                        Send_data_wo_separator(igSock,pclCurSendData);
                    }
                    else
                    {
                        /*strcat(pclCurSendData,"\n");*/
                        dbg(TRACE,"%s Resending data-3 <%s>",pclFunc, pclCurSendData);
                        Send_data_wo_separator(igSock,pclCurSendData);
                    }
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
			/*SendRST_Command();*/
			GetServerTimeStamp( "UTC", 1, 0, pcgSckWaitACKExpTime);
			AddSecondsToCEDATime(pcgSckWaitACKExpTime, igSckACKCWait, 1);
		}
  }

  GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime);
  if (strcmp(pclCurrentTime, pcgRcvHeartBeatExpTime) >= 0)
  {
     dbg(TRACE,"Current Time<%s>, Heartbeat Exp Rec Time<%s>", pclCurrentTime, pcgRcvHeartBeatExpTime);
     (void) Sockt_Reconnect();
      /*SendRST_Command();*/
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
	else /* Create socket*/
	{
		ilRC = tcp_socket(&igSock);

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

static void my_alarm(int x)
{
}

static int tcp_socket(int	*ilAcceptSocket)
{
	int ilRc = RC_SUCCESS;
	char *pclFunc = "tcp_socket";

	int ilTcpfd = 0;

  int ilOn = 1;

  int ilFound = 0;
  int ili = 0;

  struct sockaddr_in rlSin;
  /*struct linger rlLinger;*/

  char pclMessage[2000] = "\0";

  struct sigaction act_alarm;
  struct sigaction act_default;

  int ilProcessId = 0;


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

  /*TRYAGAIN:	ilAttempts = 0;

  Frank 20130107
	while(ilAttempts < ilMaxAttempt )
	{*/
		alarm(CONNECT_TIMEOUT);

		ilRc = tcp_open_connection(ilTcpfd, pcgPort , pcgIP );
		alarm(0);

		/*
		if(ilRc < 0)
		{
			ilAttempts++;
    	dbg( TRACE,"<%s>Error code<%d> Description<%s> in connect to server!! Number of attempt = %d",pclFunc,errno,strerror(errno), ilAttempts );
    	sleep( 1 );
    }
    else if(ilRc == RC_SUCCESS)
    {
    	break;
    }
    */
	/*}*/

	/*Pay attention to this line*/
	(*ilAcceptSocket) = ilTcpfd;

	return ilRc;
}

/* ***************************************************************** */
/* The CloseTCP routine                                              */
/* ***************************************************************** */

static void CloseTCP()
{
	int ilRc = RC_SUCCESS;
	dbg(DEBUG,"CloseTCP is called,igSock<%d>",igSock);

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
		dbg(TRACE,"CloseTCP: connection closed!ilRc<%d>",ilRc);
	}
} /* end of CloseTCP() */


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

static int Send_data_wo_separator(int ipSock,char *pcpData)
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
    /*char pclTmpStr[12] = "\0";*/

	struct timeval rlTimeout;

    rlTimeout.tv_sec = 2;  /* blocked for 5 seconds */
    rlTimeout.tv_usec = 0;  /* blocked for 0 useconds */

	ilBytes = strlen(pcpData);

	memset(pclSendBuffer,0,sizeof(pclSendBuffer));
	strcpy(pclSendBuffer,pcpData);

    /* fya 20140221 */
    /*
	pclTmpStr[0] = 0x04;
    pclTmpStr[1] = '\0';
    strcat(pclSendBuffer, pclTmpStr);
    */
    ilBytes = strlen(pclSendBuffer);

    if (strstr(pclSendBuffer,"heartbeat") == 0)
        dbg(DEBUG,"Send_data: Mesg sent (%s)", pclSendBuffer );

	if (ipSock > 0 && strlen(pclSendBuffer)!=0)
	{
		errno = 0;
		alarm(WRITE_TIMEOUT);
		ilRc = write(ipSock,pclSendBuffer,ilBytes);
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
      /*CloseTCP();
      igSock = 0;*/
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
} /* end of Send_data_wo_separator */

/* **************************************************************** */
/* The Send_data routine                                            */
/* Sends telegrams to the GOS-system                                */
/* **************************************************************** */
static int Send_data(int ipSock,char *pcpData)
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

	struct timeval rlTimeout;

    rlTimeout.tv_sec = 2;  /* blocked for 5 seconds */
    rlTimeout.tv_usec = 0;  /* blocked for 0 useconds */

	ilBytes = strlen(pcpData);

	memset(pclSendBuffer,0,sizeof(pclSendBuffer));
	strcpy(pclSendBuffer,pcpData);

  /* fya 20140221 */
	pclTmpStr[0] = 0x04;
  pclTmpStr[1] = '\0';
  strcat(pclSendBuffer, pclTmpStr);

  ilBytes = strlen(pclSendBuffer);

  if (strstr(pclSendBuffer,"heartbeat") == 0)
      dbg(DEBUG,"Send_data: Mesg sent (%s)", pclSendBuffer );

	if (ipSock > 0 && strlen(pclSendBuffer)!=0)
	{
		errno = 0;
		alarm(WRITE_TIMEOUT);
		ilRc = write(ipSock,pclSendBuffer,ilBytes);
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
      /*CloseTCP();
      igSock = 0;*/
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
/* The Send_KeepAlive routine                                       */
/* To be implemented in the future                                  */
/* **************************************************************** */

static int SendKeepAliveMsg()
{
	return RC_SUCCESS;
} /* end of SendKeepAliveMsg() */

/* **************************************************************** */
/* The Send Ack routine                                             */
/* Acknowledge a message from the peer                              */
/* **************************************************************** */

static int SendAckMsg()
{
	int ilRC = RC_SUCCESS;
	char pclFunc[] = "SendAckMsg:";
	int ilNumOfZero = 0;
	int ilCount = 0;
	char pclDataBuf[M_BUFF];
	char pclFormatTime[128]="\0";
	char pclCurrentTime[32]="\0";
	char pclLastTimeSendingTime[64] = "\0";

	char pclMsgNoForACK[8] = "\0";

	dbg(DEBUG,"<%s> pcgMsgNoForACK <%s>",pclFunc,pcgMsgNoForACK);

	/*For putting 0*/
	if (strlen(pcgSendMsgId) < 14)
  {
	  ilNumOfZero = 14 - strlen(pcgSendMsgId);
	  for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
	  {
	  	strcat(pclMsgNoForACK,"0");
	  }
	  strcat(pclMsgNoForACK,pcgSendMsgId);
  }
  else if (strlen(pcgSendMsgId) == 14)
  {
  	strcpy(pclMsgNoForACK,pcgSendMsgId);
  }
  else
  {
  	strncpy(pclMsgNoForACK,pcgMsgNoForACK,14);
  }


	memset(pclDataBuf,0,sizeof(pclDataBuf));
	memset(pclCurrentTime,0x00,32);
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
	/*FormatTime(pclFormatTime,pclLastTimeSendingTime);*/

	dbg(DEBUG,"<%s>========================= START-ACK ===============================",pclFunc);
	/*dbg(DEBUG,"<%s> Send Ack Msg MsgNoForACK<%s>pclFormatTime<%s>",pclFunc,pcgMsgNoForACK,pclFormatTime);*/

	/*
	sprintf(pclDataBuf,pcgSendAckFormat,pclMsgNoForACK,"ufis",pclCurrentTime,pclCurrentTime);
	dbg(DEBUG,"<%s> Send Ack Msg MsgNoForACK - pcgMsgNoForACK<%s> pclMsgNoForACK<%s> pclCurrentTime<%s> strlen(pclDataBuf) - 1 <%d>",pclFunc,pcgMsgNoForACK,pclMsgNoForACK,pclCurrentTime,strlen(pclDataBuf)-1);
	*/

	sprintf(pclDataBuf,pcgSendAckFormat,pclCurrentTime,pclMsgNoForACK);
	dbg(TRACE,"<%s> The length of sent data pclDataBuf is <%d>",pclFunc,strlen(pclDataBuf) - 1);

	igMsgNoForACK++;

	itoa(igMsgNoForACK,pcgMsgNoForACK,10);

	/*Frank 20130107
	if(igMsgNoForACK == 65535)
  {
          igMsgNoForACK = 0;
  }
	sprintf(pclDataBuf,pcgSendAckFormat,igMsgNoForACK,pclFormatTime,pclFormatTime,igMsgNoForACK);
	igMsgNoForACK++;
	sprintf(pclDataBuf,pcgSendAckFormat,igMsgNoForACK,pclFormatTime,pclFormatTime,igMsgNoForACK);
  */

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

    igConnectionNo++;

    igOldCnnt = igConnected;
    CloseTCP();
    igSock = 0;
	dbg(TRACE,"<%s>Keep trying to connect <%s><%s>",pclFunc,pcgIP,pcgPort);
/*    for (ilCount = 0; ilCount < igReconMax; ilCount++)
{*/
     ilRc = OpenServerSocket();
     dbg(DEBUG,"<%s>ilRc<%d>",pclFunc,ilRc);
     ilCount++;
     if (ilRc != RC_SUCCESS)
     {
        CloseTCP();
            igSock = 0;
     }
/*}*/

	if(ilRc == RC_SUCCESS)
    {
      dbg(TRACE,"<%s>ilCount[%d]igSock<%d>, connection success,reset ilConnectionNo",pclFunc,ilCount,igSock);
      igConnected = TRUE;

      igConnectionNo = 0;

      dbg(TRACE,"<%s> Calling SendRST_Command",pclFunc);
      /*SendRST_Command();*/
    }
	else
    {
      dbg(TRACE,"<%s>ilCount[%d]igSock<%d>, connection fails",pclFunc,ilCount,igSock);
      igConnected = FALSE;
    }

  if (igOldCnnt != igConnected)
  {
      if (igConnected == TRUE)
      	;
      else
      	;

  }

  GetServerTimeStamp( "UTC", 1, 0, pcgReconExpTime);
  strcpy(pcgRcvHeartBeatExpTime, pcgReconExpTime);
  AddSecondsToCEDATime(pcgReconExpTime, igReconIntv, 1);
  AddSecondsToCEDATime(pcgRcvHeartBeatExpTime, igHeartBeatTimOut, 1);

    dbg(DEBUG," <%s> pcgReconExpTime<%s>Heartbeat Exp Rec Time<%s> igHeartBeatTimOut<%d>", pclFunc,pcgReconExpTime,pcgRcvHeartBeatExpTime,igHeartBeatTimOut);

    dbg(TRACE,"Make the queue empty");
    ClearQueue(pcgQueue);

	return ilRc;
}

static int tcp_open_connection (int socket, char *Pservice, char *Phostname)
{
  int rc=RC_FAIL;
  int i = 0;
  struct hostent *hp;
  struct sockaddr_in    dummy;
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
    dbg(TRACE,"tcp_open_connection: ERROR while connect: %s",
               strerror(errno));
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
					/*ShwoIPCQueMessage();*/
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
	return ilRC;
}

/*SendRST_Command*/
/*Input:  Null*/
/*Output: The return value of SendCedaEvent*/
static int SendRST_Command(void)
{
	char * pclFunc = "SendRST_Command";
	int  ilRC = RC_SUCCESS;
	char pclCommand[4] = "RST";

	dbg(DEBUG,"<%s>MOD_NAME<%s> - MOD_ID<%d> - CMD<%s>",pclFunc,mod_name,mod_id,pclCommand);

	ilRC = SendCedaEvent(mod_id,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,"RST","","","","","","",NETOUT_NO_ACK);

	return ilRC;
}

/*
fya 20140218
input:  Null
output: The Return Value of sql_if
*/
static int SendBatchFlights(void)
{
	int ilCount = 0;
	int ilCountP = 0;
	int ilGateNumber = 0;
	char * pclFunc = "SendBatchFlights";
	int  ilRC = RC_SUCCESS;
	short slLocalCursor = 0, slFuncCode = 0;
	char pclUrno[16] = "\0";
	char pclRecordURNO[16] = "\0";
	char pclParkstand[16] = "\0";
	char pclDataSent[1024] = "\0";
	char pclDataSentP[1024] = "\0";
	char pclSqlBuf[2048] = "\0",pclDataArea[2048] = "\0",pclWhere[2048] = "\0";
	static char pclAllValidGates[4096] = "\0";

	SENT_MSG rlSentMsg;

	GetAllValidGates(pclAllValidGates,&ilGateNumber);

	/*for each parking stand, searching the uasge*/

	for (ilCountP = 1; ilCountP <= ilGateNumber; ilCountP++)
	/*for (ilCount = 1; ilCount <= 2; ilCount++)*/
	{
		get_real_item(pclParkstand,pclAllValidGates,ilCountP);
		dbg(DEBUG,"<%s> ilCountP<%d> pclParkstand<%s>",pclFunc,ilCountP,pclParkstand);

		PutDefaultValue(&rlSentMsg);

		/*
		ilRC = function_PST_USAGE(&rlSentMsg, pclParkstand, igConfigTime, pclUrno);


		if(strlen(rlSentMsg.pclPosi) == 0)
		{
			dbg(DEBUG,"<%s> rlSentMsg.pclPosi<%s> is null", pclFunc, rlSentMsg.pclPosi);
			continue;
		}
        */
		/* If the return value is NO_END, then this means the SendMsg is null */
		memset(pclDataSent,0,sizeof(pclDataSent));
		switch(ilRC)
		{
			case NO_END:
                dbg(DEBUG,"<%s> The return value is NO_END",pclFunc);
				/*BuildEmptyData(pclDataSent,rlSentMsg.pclPosi);*/
				break;
			case START_FOUND:
				dbg(DEBUG,"<%s> The return value is START_FOUND",pclFunc);
				BuildSentData(pclDataSent,rlSentMsg);
				break;
			default:
				BuildSentData(pclDataSent,rlSentMsg);
				break;
		}
		dbg(TRACE,"<%s> pclDataSent<%s>",pclFunc,pclDataSent);

		StoreSentData(pclDataSent,pclUrno,rlSentMsg,pclRecordURNO);
        strcpy(pcgSendMsgId,pclRecordURNO);

        EnQueue(pcgQueue, atoi(pcgSendMsgId));
        dbg(TRACE,"%s Size<%d> Traverse all element in queue:", pclFunc, GetSize(pcgQueue));
        QueueTraverse(pcgQueue,print);

        for (ilCount = 0; ilCount < igReSendMax; ilCount++)
        {
            if (igSock > 0)
            {
                ilRC = Send_data(igSock,pclDataSent);
                dbg(DEBUG, "<%s>1-ilRC<%d>",pclFunc,ilRC);

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
                {
                    ilRC = Sockt_Reconnect();
                }
                else
                {
                    ilRC = RC_FAIL;
                }
            }
        }

        for (ilCount = 0; ilCount < igReSendMax; ilCount++)
        {
            if (igSock > 0)
          {
                ilRC = SendAckMsg();
            dbg(DEBUG, "<%s>1-ilRC<%d>",pclFunc,ilRC);

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
            {
                ilRC = Sockt_Reconnect();
            }
            else
                  ilRC = RC_FAIL;
          }
        }

		if( ilCount >= igReSendMax)
		{
			dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);

            dbg(TRACE,"Make the queue empty");
            ClearQueue(pcgQueue);
			return RC_FAIL;
		}

		memset(pclUrno,0,sizeof(pclUrno));
	}/* End of for loop */
}

static int TrimSpace( char *pcpInStr )
{
    int ili = 0;
    int ilLen;
    char *pclOutStr = NULL;
    char *pclP;
    char *pclFunc = "TrimSpace";

    ilLen = strlen( pcpInStr );
    if( ilLen <= 0 )
        return;

    pclOutStr = (char *)malloc(ilLen + 10);
    pclP = pcpInStr;

    /* Trim front spaces */
    while( pclP && *pclP == ' ' )
        pclP++;

    while( *pclP )
    {
       pclOutStr[ili++] = *pclP;
       if( *pclP != ' ' )
           pclP++;
       else
       {
           while( *pclP == ' ' )
               pclP++;
       }
    }
    /* Trim back space */
    if( pclOutStr[ili-1] == ' ' )
        ili--;
    pclOutStr[ili] = '\0';
    strcpy( pcpInStr, pclOutStr );
    if( pclOutStr != NULL )
        free( (char *)pclOutStr );
}

static void PutDefaultValue(SENT_MSG *rpSentMsg)
{
	char *pclFunc = "PutDefaultValue";
	char pclDefauleValueTime[16] = "..............";

	memset(rpSentMsg->pclGate,0,sizeof(rpSentMsg->pclGate));
	memset(rpSentMsg->pclFlightID,0,sizeof(rpSentMsg->pclFlightID));
	memset(rpSentMsg->pclBestTime,0,sizeof(rpSentMsg->pclBestTime));
	memset(rpSentMsg->pclOnbl,0,sizeof(rpSentMsg->pclOnbl));
	memset(rpSentMsg->pclOfbl,0,sizeof(rpSentMsg->pclOfbl));

	strncpy(rpSentMsg->pclBestTime,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOnbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOfbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
}

static void PutDefaultValueWithGat(SENT_MSG *rpSentMsg, char * pcpParkingStand)
{
	char *pclFunc = "PutDefaultValueWithGat";
	char pclDefauleValueTime[16] = "..............";

	memset(rpSentMsg->pclGate,0,sizeof(rpSentMsg->pclGate));
	memset(rpSentMsg->pclFlightID,0,sizeof(rpSentMsg->pclFlightID));
	memset(rpSentMsg->pclBestTime,0,sizeof(rpSentMsg->pclBestTime));
	memset(rpSentMsg->pclOnbl,0,sizeof(rpSentMsg->pclOnbl));
	memset(rpSentMsg->pclOfbl,0,sizeof(rpSentMsg->pclOfbl));

    strncpy(rpSentMsg->pclGate,pclDefauleValueTime,4);
    strncpy(rpSentMsg->pclFlightID,pclDefauleValueTime,1);
	strncpy(rpSentMsg->pclBestTime,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOnbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOfbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
}

static void ShowMsgStruct(SENT_MSG rpSentMsg)
{
	char *pclFunc = "ShowMsgStruct";

	dbg(DEBUG,"<%s> Gate<%s>",pclFunc,rpSentMsg.pclGate);
    dbg(DEBUG,"<%s> Flight ID<%s>",pclFunc,rpSentMsg.pclFlightID);
	dbg(DEBUG,"<%s> Best Time<%s>",pclFunc,rpSentMsg.pclBestTime);
    dbg(DEBUG,"<%s> ONB<%s>",pclFunc,rpSentMsg.pclOnbl);
    dbg(DEBUG,"<%s> OFB<%s>",pclFunc,rpSentMsg.pclOfbl);
}

static void BuildEmptyData(char *pcpDataSent,char *pcpPosi)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	char *pclFunc = "BuildEmptyData";
	char pclDefauleValueTime[16] = "..............";

	memset(pcpDataSent,0,sizeof(pcpDataSent));

	strcat(pcpDataSent,"STX_");

	if (strlen(pcpPosi) < POS_LEN)
  {
	  ilNumOfZero = POS_LEN - strlen(pcpPosi);
	  for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
	  {
	          strcat(pcpDataSent,"0");
	  }
  }
	strcat(pcpDataSent,pcpPosi);

	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);

	strcat(pcpDataSent,pcgSuffix);

	dbg(TRACE,"<%s> Data Sent<%s> strlen(pcpDataSent)<%d>",pclFunc,pcpDataSent,strlen(pcpDataSent));
}

static void BuildSentData(char *pcpDataSent,SENT_MSG rpSentMsg)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	int ilRc = DB_SUCCESS;
	char *pclFunc = "BuildSentData";
	char pclDefauleValueTime[16] = "..............";

	char pclDataSentWithSeparator[1024] = "\0";

	memset(pcpDataSent,0,sizeof(pcpDataSent));
	memset(pclDataSentWithSeparator,0,sizeof(pclDataSentWithSeparator));

	/* Build Prefix */
	strcat(pcpDataSent,"STX_");
	strcat(pclDataSentWithSeparator,"STX_");

	if (strlen(rpSentMsg.pclGate) < POS_LEN)
    {
      ilNumOfZero = POS_LEN - strlen(rpSentMsg.pclGate);
      for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
      {
        strcat(pcpDataSent,"0");
        strcat(pclDataSentWithSeparator,"0");
      }
    }
	strcat(pcpDataSent,rpSentMsg.pclGate);
	strcat(pclDataSentWithSeparator,rpSentMsg.pclGate);
	strcat(pclDataSentWithSeparator,pcgSeparator);
	/*
	dbg(DEBUG,"<%s>1 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>1 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclFlightID) > 0)
	{
	    if(rpSentMsg.pclFlightID[0] == 'S')
	    {
            strcat(pcpDataSent,"SS");
            strcat(pclDataSentWithSeparator,"SS");
            strcat(pclDataSentWithSeparator,pcgSeparator);
	    }
	    else
	    {
            strcat(pcpDataSent,"II");
            strcat(pclDataSentWithSeparator,"II");
            strcat(pclDataSentWithSeparator,pcgSeparator);
	    }
	}
	else
	{
		strcat(pcpDataSent,"II");
        strcat(pclDataSentWithSeparator,"II");
        strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>2 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>2 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclBestTime) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclBestTime);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclBestTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>4 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>4 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclOnbl) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclOnbl);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclOnbl);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>5 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>5 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclOfbl) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclOfbl);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclOfbl);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>8 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>8 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	/* Build suffix*/
	strcat(pcpDataSent,pcgSuffix);
	strcat(pclDataSentWithSeparator,pcgSuffix);

	/*@fya 20140303*/
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
	dbg(TRACE,"<%s> Data Sent<%s> strlen(pcpDataSent)<%d>",pclFunc,pcpDataSent,strlen(pcpDataSent));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);

	dbg(TRACE,"<%s> DataForCheck<%s> strlen(pclDataSentWithSeparator)<%d>",pclFunc,pclDataSentWithSeparator,strlen(pclDataSentWithSeparator));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
}

static int StoreAckData(char *pcpAckSent, char *pcpRecordURNO)
{
    short slCursor = 0;
	short slFkt = 0;
	int ilRCdb = RC_SUCCESS;
	int ilRC   = RC_SUCCESS;
    char *pclFunc = "StoreAckData";
    char pclSqlBuf[2048] = "\0";
	char pclDataBuf[2048] = "\0";

    sprintf(pclSqlBuf,"UPDATE DORTAB SET ACK_MSG='%s' WHERE URNO='%s'",pcpAckSent,pcpRecordURNO);
	slCursor = 0;
	slFkt = START;
	dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
	ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
	if (ilRCdb == DB_SUCCESS)
		commit_work();
	else
	{
		dbg(TRACE,"%s Error inserting into DORTAB",pclFunc);
		ilRC = RC_FAIL;
	}
	close_my_cursor(&slCursor);
}

static int StoreSentData(char *pcpDataSent, char *pcpUaft, SENT_MSG rpMsg, char *pcpRecordURNO)
{
	int ilRCdb = RC_SUCCESS;
	int ilRC   = RC_SUCCESS;
	char *pclFunc = "StoreSentData";
	short slCursor = 0;
	short slFkt = 0;
	int ilNextUrno = 0;
	char pclUrno[16] = "\0";
	char pclbuffer[33] = "\0";
	char pclCurrentTime[64] = "\0";
	char pclSqlBuf[2048] = "\0";
	char pclDataBuf[2048] = "\0";
	char pclFieldList[2048] = "\0";
	char pclDataList[2048] = "\0";

	memset(pclCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	/*Get New Urno*/
	ilNextUrno = NewUrnos( "DORTAB", 1 );
	if(ilNextUrno > 0)
	{
		if(itoa(ilNextUrno,pclbuffer,10) == NULL)
		{
			dbg(DEBUG,"line<%d>Error getting new URNO",__LINE__);
			return RC_FAIL;
		}
		else
		{
		  strncpy(pclUrno,pclbuffer,strlen(pclbuffer));
		}
	}
	else
	{
		dbg(DEBUG,"line<%d>Error getting new URNO",__LINE__);
		return RC_FAIL;
	}

	dbg(DEBUG,"the URNO for DORTAB is <%s>",pclUrno);

	strcpy(pclFieldList,"URNO,UAFT,FLNO,ADID,GTX1,TIFX,TYPE,GAT_MSG,STAT,CDAT");
	sprintf(pclDataList,"%s,'%s','%s','%s','%s','%s','%s','%s','%s','%s'",pclUrno,pcpUaft,rpMsg.pclFlno,rpMsg.pclAdid,rpMsg.pclGate,rpMsg.pclBestTime,"FLIGHT",pcpDataSent,"SENT",pclCurrentTime);

	sprintf(pclSqlBuf,"INSERT INTO DORTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
	slCursor = 0;
	slFkt = START;
	dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
	ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
	if (ilRCdb == DB_SUCCESS)
		commit_work();
	else
	{
		dbg(TRACE,"%s Error inserting into DORTAB",pclFunc);
		ilRC = RC_FAIL;
	}
	close_my_cursor(&slCursor);

	memset(pcpRecordURNO,0,sizeof(pcpRecordURNO));
	strcpy(pcpRecordURNO,pclUrno);

	return ilRC;
}

static void Upd_ACK_FlightBuildWhereClause(char *pcpWhere,char *pcpUrno)
{
	char *pclFunc = "Upd_ACK_FlightBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";

	sprintf(pclWhere,"URNO = '%s'", pcpUrno);

	strcpy(pcpWhere,pclWhere);
	dbg(DEBUG,"Where Clause<%s>",pcpWhere);
}

static void Upd_ACK_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "Upd_ACK_FlightBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "UPDATE DORTAB SET STAT = 'ACKED' WHERE %s", pcpWhere);

	strcpy(pcpSqlBuf,pclSqlBuf);
	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static int TrimZero( char *pcpInStr )
{
  int ili = 0;
  int ilLen;
  char *pclOutStr = NULL;
  char *pclP;
  char *pclFunc = "TrimZero";

  ilLen = strlen( pcpInStr );
  if( ilLen <= 0 )
      return;

  pclOutStr = (char *)malloc(ilLen + 10);
  pclP = pcpInStr;

  /* Trim front spaces */
  while( pclP && *pclP == '0' )
      pclP++;

	while( *pclP )
	{
		pclOutStr[ili++] = *pclP;
		pclP++;
  }

  pclOutStr[ili] = '\0';

  strcpy( pcpInStr, pclOutStr );
  if( pclOutStr != NULL )
  	free( (char *)pclOutStr );
}

static void print(Item i)
{
    /*printf("The data in this node is %d\n",i);*/
    dbg(TRACE,"The data in this node is %d",i);
}

int getValue(char *pcpFields, char *pcpFieldName, char *pcpNewData, char *pcpOldData, char *pcpNewValue, char *pcpOldValue)
{
    int ilItemNo = 0;
    char *pclFunc = "getValue";

    ilItemNo = get_item_no(pcpFields, pcpFieldName, 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No %s Found in the field list, can't do anything", pclFunc, pcpFieldName);
        return RC_FAIL;
    }

    get_real_item(pcpNewValue, pcpNewData, ilItemNo);
    get_real_item(pcpOldValue, pcpOldData, ilItemNo);
    dbg(DEBUG,"<%s>The New %s is <%s>", pclFunc, pcpFieldName, pcpNewValue);
	dbg(DEBUG,"<%s>The Old %s is <%s>", pclFunc, pcpFieldName, pcpOldValue);
	TrimSpace(pcpNewValue);
	TrimSpace(pcpOldValue);

    if (strlen(pcpNewValue) == 0)
	{
		dbg(TRACE,"<%s>New %s is null",pclFunc, pcpFieldName, pcpNewValue);
		return RC_FAIL;
	}

    return RC_SUCCESS;
}

static void Get_FlightDataWhereClause(char *pcpWhere,char *pcpUrno)
{
	char *pclFunc = "Get_FlightDataWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";

	if (strlen(pcpUrno) != 0 )
    {
        sprintf(pclWhere,"URNO = '%s'", pcpUrno);
        strcpy(pcpWhere,pclWhere);
        dbg(DEBUG,"Where Clause<%s>",pcpWhere);
    }
    else
    {
        dbg(TRACE,"%s pcpUrno is null",pclFunc);
    }
}

static void Get_FlightDataBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "Get_FlightDataBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	if (strlen(pcpWhere) == 0)
	{
        sprintf(pclSqlBuf, "SELECT GAT_MSG FROM DORTAB ORDER BY CDAT");
	}
    else
    {
        sprintf(pclSqlBuf, "SELECT GAT_MSG FROM DORTAB WHERE %s", pcpWhere);
    }

	strcpy(pcpSqlBuf,pclSqlBuf);
	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void BuildAckData(char *pcpAckSent, char *pclRecordURNO)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	int ilRc = DB_SUCCESS;
	char *pclFunc = "BuildAckData";
	char pclCurrentTime[TIMEFORMAT] = "\0";
	char pclDefauleValueTime[16] = "..............";

	char pclDataSentWithSeparator[1024] = "\0";

	memset(pcpAckSent,0,sizeof(pcpAckSent));
	memset(pclDataSentWithSeparator,0,sizeof(pclDataSentWithSeparator));

	/* Build Prefix */
	strcat(pcpAckSent,"ACK_");
	strcat(pclDataSentWithSeparator,"ACK_");

	strcat(pcpAckSent,"ufis");
	strcat(pclDataSentWithSeparator,"ufis");
	strcat(pclDataSentWithSeparator,pcgSeparator);

	strcat(pcpAckSent,"..");
	strcat(pclDataSentWithSeparator,"..");
	strcat(pclDataSentWithSeparator,pcgSeparator);

    strcat(pcpAckSent,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pcgSeparator);

    GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
    strcat(pcpAckSent,pclCurrentTime);
    strcat(pclDataSentWithSeparator,pclCurrentTime);
    strcat(pclDataSentWithSeparator,pcgSeparator);

    if (strlen(pclRecordURNO) < ID_LEN)
    {
      ilNumOfZero = ID_LEN - strlen(pclRecordURNO);
      for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
      {
        strcat(pcpAckSent,"0");
        strcat(pclDataSentWithSeparator,"0");
      }
    }
	strcat(pcpAckSent,pclRecordURNO);
	strcat(pclDataSentWithSeparator,pclRecordURNO);
	strcat(pclDataSentWithSeparator,pcgSeparator);

	/* Build suffix*/
	strcat(pcpAckSent,pcgSuffix);
	strcat(pclDataSentWithSeparator,pcgSuffix);

	/*@fya 20140303*/
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
	dbg(TRACE,"<%s> Data Sent<%s> strlen(pcpAckSent)<%d>",pclFunc,pcpAckSent,strlen(pcpAckSent));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);

	dbg(TRACE,"<%s> DataForCheck<%s> strlen(pclDataSentWithSeparator)<%d>",pclFunc,pclDataSentWithSeparator,strlen(pclDataSentWithSeparator));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
}


static int putData(int ipSock,char *pcpData)
{
    int ilCount = 0;
	char * pclFunc = "putData";
	int  ilRC = RC_SUCCESS;

    for (ilCount = 0; ilCount < igReSendMax; ilCount++)
    {
        if (igSock > 0)
        {
            ilRC = Send_data_wo_separator(ipSock,pcpData);
            dbg(DEBUG, "<%s>1-ilRC<%d>",pclFunc,ilRC);

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
            {
                ilRC = Sockt_Reconnect();
            }
            else
            {
                ilRC = RC_FAIL;
            }
        }
    }

    if( ilCount >= igReSendMax)
    {
        dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);

        dbg(TRACE,"Make the queue empty");
        ClearQueue(pcgQueue);
        /*
        dbg(TRACE,"Destruct the queue");
        DestroyQueue(pcgQueue);
        */
        return RC_FAIL;
    }

    return RC_SUCCESS;
}

static void BuildHbtData(char *pcpHbtSent)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	int ilRc = DB_SUCCESS;
	char *pclFunc = "BuildHbtData";
	char pclCurrentTime[TIMEFORMAT] = "\0";
	char pclDefauleValueTime[16] = "..............";

	char pclDataSentWithSeparator[1024] = "\0";

	memset(pcpHbtSent,0,sizeof(pcpHbtSent));
	memset(pclDataSentWithSeparator,0,sizeof(pclDataSentWithSeparator));

	/* Build Prefix */
	strcat(pcpHbtSent,"HBT_");
	strcat(pclDataSentWithSeparator,"HBT_");

	strcat(pcpHbtSent,"ufis");
	strcat(pclDataSentWithSeparator,"ufis");
	strcat(pclDataSentWithSeparator,pcgSeparator);

    strcat(pcpHbtSent,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pcgSeparator);

    strcat(pcpHbtSent,"..");
	strcat(pclDataSentWithSeparator,"..");
	strcat(pclDataSentWithSeparator,pcgSeparator);

    GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
    strcat(pcpHbtSent,pclCurrentTime);
    strcat(pclDataSentWithSeparator,pclCurrentTime);
    strcat(pclDataSentWithSeparator,pcgSeparator);

    strcat(pcpHbtSent,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pclDefauleValueTime);
    strcat(pclDataSentWithSeparator,pcgSeparator);

	/* Build suffix*/
	strcat(pcpHbtSent,pcgSuffix);
	strcat(pclDataSentWithSeparator,pcgSuffix);

	/*@fya 20140303*/
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
	dbg(TRACE,"<%s> Data Sent<%s> strlen(pcpHbtSent)<%d>",pclFunc,pcpHbtSent,strlen(pcpHbtSent));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);

	dbg(TRACE,"<%s> DataForCheck<%s> strlen(pclDataSentWithSeparator)<%d>",pclFunc,pclDataSentWithSeparator,strlen(pclDataSentWithSeparator));
	dbg(TRACE,"<%s>*********************************************************************************",pclFunc);
}

static int adidFilter(char *pcpAdidNewData)
{
    char *pclFunc = "adidFilter";

    if( strstr(pcgAdidFilter,pcpAdidNewData) == 0 )
    {
        dbg(TRACE,"%s Adid check fails",pclFunc);
        return RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s Adid check passes",pclFunc);
        return RC_SUCCESS;
    }
}

static int calculateTimeWindow(char *pcpTimeWindowLowerLimit, char *pcpTimeWindowUpperLimit)
{
    int ilRc = RC_SUCCESS;
    char *pclFunc = "calculateTimeWindow";
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

    memset(pcpTimeWindowLowerLimit,0,sizeof(pcpTimeWindowLowerLimit));
    memset(pcpTimeWindowUpperLimit,0,sizeof(pcpTimeWindowUpperLimit));

    strcpy(pcpTimeWindowLowerLimit, pclTimeLowerLimit);
    strcpy(pcpTimeWindowUpperLimit, pclTimeUpperLimit);

    dbg(TRACE,"The current  time range is <%s> ~ <%s>", pcpTimeWindowLowerLimit, pcpTimeWindowUpperLimit);

    return ilRc;
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
