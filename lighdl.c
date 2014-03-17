#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/lighdl.c 1.74 3/10/2014 9:17:54 PM Exp  $";
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
/* 20121121 FYA:1.6k4
X-1)For Towing flight, sending out the update
X-2)Adding the notification of new created towing flight -> rsp flisea for sending ETAI & ETDI out
X-3)ATD = AIRB, ATA = LAND
X-4)off-block = OFBL
X-5)fixed the bug on towing record in LIGTAB
X-6)Change condition of sending out the towing records which have off-block time in LIGTAB
7) Get OFNL and ONBL from PDETAB
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
#include "lighdl.h"
#include "urno_fn.h"

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
#define NO_END	1
#define NO_START 2
#define START_FOUND 3
#define TEST
#define TOWING_FLT_NO 30
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

static BOOL	bgAlarm = FALSE;				/* global flag for time-out socket-read */
static int	igSock = 0;							/* global tcp/ip socket for send/receive */

static int	igKeepAlive;						/* The Keep Alive interval (0 = no Keep Alive) */
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

static int igConfigTime;
static int igConfigShift;
static int igConfigEndShiftTime;
static int igActualUseTime;

static char	pcgCurSendData[4096] = "\0"; /* global CurSendData */
static char	pcgSendHeartBeatExpTime[64] = "\0";  /* global Send heartbeat expect time */
static char	pcgRcvHeartBeatExpTime[64] = "\0";   /* global Receive heartbeat expire time */
static char	pcgReconExpTime[64] = "\0";   /* global Receive heartbeat expect time */

static char	pcgSendMsgId[1024] = "\0";

static int	igModID_ConMgr = 0;
static int	igModID_ConSTB = 0;

static int	igMgrDly = 50;
static int	igConDly = 50;

static int	igConnected = FALSE;   /* global socket Connect status  */
static int	igOldCnnt = FALSE;

static char pcgCurrentTime[TIMEFORMAT] = "\0";
static char pcgTimeUpperLimit[TIMEFORMAT] = "\0";
static int igTimeRange = 0;
static long lgEvtCnt = 0;
static int igBatch = 0;

/*
@fya 20140310
*/
static int igNextAllocDepUpperRange = 0;

static int	Init_Handler();
static int	Reset(void);                        /* Reset program          */
static void	Terminate(int);                     /* Terminate program      */
static void	HandleSignal(int);                  /* Handles signals        */
static void	HandleErr(int *);                     /* Handles general errors */
static void	HandleQueErr(int);                  /* Handles queuing errors */
static int	HandleInternalData(void);           /* Handles event data     */
static void	HandleQueues(void);                 /* Waiting for Sts.-switch*/
static char pcgPrefix[16] = "\0";
static char	pcgSuffix[16] = "\0";
static char	pcgSeparator[16] = "\0";
static char  cgHopo[8];
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
static int	GetCfgEntry(char *pcpFile,char *pcpSection,char *pcpEntry,short spType,char **pcpDest,
				int ipValueType,char *pcpDefVal);
static int	CheckValue(char *pcpEntry,char *pcpValue,int ipType);
static int	SendKeepAliveMsg();
static int	SendAckMsg();
static int	GetMsgno(char *pcpBuff);

static int tcp_socket();
static int	SendHeartbeatMsg(char *pcpLastTimeSendingTime);
void FormatTime(char *pcpTime, char *pcpLastTimeSendingTime);
static int ReceiveACK(int ipSock,int ipTimeOut);
static int Sockt_Reconnect(void);
static int tcp_open_connection (int socket, char *Pservice, char *Phostname);
static int FindMsgId(char *pcpData, char *pcpMsgId, int Mode);

static int SendRST_Command(void);
static void BuildCurrentTime(void);
static void BatBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static int SendBatchFlights(void);
static void initMsg(MSG *msg);
static int TrimSpace( char *pcpInStr );
static void BuildArrPart(SENT_MSG *rpSentMsg, char *pcpPstaNewData, char *pcpStoaNewData, char *pcpEtaiNewData, char* pcpTmoaNewData, char *pcpOnblNewData);
static void BuildDepPart(SENT_MSG *rpSentMsg, char *pcpPstdNewData, char *pcpStodaNewData, char *pcpEtdiNewData, char *pcpOfblNewData);
static void ShowMsgStruct(SENT_MSG rpSentMsg);
static void PutDefaultValue(SENT_MSG *rpSentMsg);
static int GetSeqFlight(SENT_MSG *rpSentMsg,char *pcpADFlag);

/*static int RunSQL(char *pcpSelection, char *pcpData );*/
static int UpdBuildWhereClause(SENT_MSG *rpSentMsg, char * pcpWhere, char * pcpADFlag);
static int UpdBuildFullQuery(char *pcpSqlBuf, char *pcpWhere, char *pcpADFlag);
static void BuildSentData(char *pcpDataSent,SENT_MSG rpSentMsg);
static void GetAllValidSlotsBuildWhereClause(char *pcpWhere);
static void GetAllValidSlotsBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void GetAllValidSlots(char *pcpAllValidSlots,int *ipSoltNumber);
static int StoreSentData(char *pcpDataSent,char *pcpUaft,char *pcpFlagTowing, char *pcpRecordURNO);
static void SearchTowingFlightBuildWhereClause(char *pcpRkey, char *pcpRegn, char *pcpWhere);
static void SearchTowingFlightBuildFullQuery(char *pcpSqlBuf, char *pcpWhere);
static void NTI_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void NTI_FlightBuildWhereClause(char *pcpWhere);
static void Upd_NTI_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void Upd_NTI_FlightBuildWhereClause(char *pcpWhere,char *pcpUrno);
static int function_PST_USAGE(SENT_MSG *rpSentMsg, char *pcpParkstand, int ipConfigTime, char *pcpUrno);
static void FindActualUseBuildWhereClause(char *pcpParkstand, char *pcpWhere, int ipConfigTime);
static void FindActualUseBuildFullQuery(char *pcpSqlBuf, char *pcpWhere);
static void FindEndUseBuildWhereClause(TOWING rpIntermediateLatestUse,char *pcpWhere,char *pcpParkstand,int ipActualUseTime);
static void FindEndUseBuildFullQuery(char *pcpSqlBuf, char *pclWhere);
static void FindFinishUsageBuildWhereClause(TOWING rlIntermediateLatestUse,char *pcpWhere,char *pcpParkstand,int ipActualUseTime,int ipConfigEndShiftTime);
static void FindFinishUsageBuildFullQuery(char *pcpSqlBuf,char *pclWhere);
static void FindEST_SCHBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void FindEST_SCHBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpSCHUseStartTime);
static void FindEarliest_Start_LeaveBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void FindEarliest_Start_LeaveBuildWhereClause(char *pcpParkstand,char *pcpWhere,int ipConfigEndShiftTime);
static void Upd_ACK_FlightBuildWhereClause(char *pcpWhere,char *pclRecvBuffer);
static void Upd_ACK_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void BuildEmptyData(char *pcpDataSent,char *pcpPosi);
static void BuildSentDataDep(char *pcpDataSent,SENT_MSG rpSentMsg);
static void BuildSentDataArr(char *pcpDataSent,SENT_MSG rpSentMsg);
static int TrimZero( char *pcpInStr );
static void FindNextAllocDepBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpTifdNewData);
static void FindNextAllocDepBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void FindNextAllocArrBuildFullQuery(char *pcpSqlBuf,char *pcpWhere);
static void FindNextAllocArrBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpFormalTifd, char *pcpNextTifd);
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

	BuildCurrentTime();

	return (ilRc);
} /* end of Init_Handler */

/* fya 20140221 */
static void GetAllValidSlots(char *pcpAllValidSlots,int *ipSoltNumber)
{
	int ilRC = RC_SUCCESS;
	char *pclFunc = "GetAllValidSlots";
	short slLocalCursor = 0, slFuncCode = 0;
	char pclParkingStand[16] = "\0";
	char pclSqlBuf[2048] = "\0",pclSqlData[4096] = "\0",pclWhere[2048] = "\0";

	memset(pcpAllValidSlots,0,sizeof(pcpAllValidSlots));

	/* Build Where Clause */
	GetAllValidSlotsBuildWhereClause(pclWhere);

	/* Build Full Sql query */
	GetAllValidSlotsBuildFullQuery(pclSqlBuf,pclWhere);

	slLocalCursor = 0;
	slFuncCode = START;
	while(sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS)
  {
    slFuncCode = NEXT;

    memset(pclParkingStand,0,sizeof(pclParkingStand));
   	get_fld(pclSqlData,FIELD_1,STR,20,pclParkingStand);

   	TrimSpace(pclParkingStand);
   	strcat(pcpAllValidSlots,pclParkingStand);
   	strcat(pcpAllValidSlots,",");

   	/*dbg(DEBUG,"<%s>pcpAllValidSlots<%s>",pclFunc,pcpAllValidSlots);*/
  }
  close_my_cursor(&slLocalCursor);

  pcpAllValidSlots[strlen(pcpAllValidSlots)-1] = '\0';

  dbg(TRACE,"<%s>\n******************All Slot******************",pclFunc);
  dbg(TRACE,"<%s>pcpAllValidSlots<%s>",pclFunc,pcpAllValidSlots);
  dbg(TRACE,"<%s>******************All Slot******************\n",pclFunc);

  *ipSoltNumber = GetNoOfElements(pcpAllValidSlots, ',');
}

/* fya 20140221 */
static void GetAllValidSlotsBuildWhereClause(char *pcpWhere)
{
	char *pclFunc = "GetAllValidSlotsBuildWhereClause";

	char pclWhere[2048] = "\0";

	memset(pcgCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

	sprintf(pclWhere,"WHERE (VAFR < '%s' and (VATO > '%s' or VATO = ' ' or VATO IS NULL)) order by PNAM", pcgCurrentTime,pcgCurrentTime);

  strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

/* fya 20140221 */
static void GetAllValidSlotsBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "GetAllValidSlotsBuildFullQuery";
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

	MSG rlMsg;

	int ilCount = 0;
	int ilCountTowing = 0;
	int ilCountT = 0;

	int ilItemNo = 0;
	int ilItemNo1 = 0;
	int ilNoEleField = 0;
	int ilNoEleNewData = 0;
	int ilNoEleOldData = 0;
	int ilFlag = 0;
	char pclAdidOldData[2] = "\0";
	char pclAdidNewData[2] = "\0";
	char pclPstaNewData[16] = "\0";
	char pclPstaOldData[16] = "\0";
	char pclPstdNewData[16] = "\0";
	char pclPstdOldData[16] = "\0";
	char pclStoaNewData[16] = "\0";
	char pclStoaOldData[16] = "\0";
	char pclStodNewData[16] = "\0";
	char pclStodOldData[16] = "\0";

	char pclEtaiNewData[16] = "\0";
	char pclEtaiOldData[16] = "\0";
	char pclEtdiNewData[16] = "\0";
	char pclEtdiOldData[16] = "\0";

	char pclTmoaNewData[16] = "\0";
	char pclTmoaOldData[16] = "\0";
	char pclOnblNewData[16] = "\0";
	char pclOnblOldData[16] = "\0";
	char pclOfblNewData[16] = "\0";
	char pclOfblOldData[16] = "\0";
	char pclRkey[16] = "\0";
  char pclRegn[16] = "\0";

  char pclTifaNewData[16] = "\0";
  char pclTifaOldData[16] = "\0";
  char pclTifdNewData[16] = "\0";
  char pclTifdOldData[16] = "\0";

  char pclTifa[16] = "\0";
  char pclTifd[16] = "\0";
  /*URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD*/
  char pclUrno[16] = "\0";
  char pclUaft[16] = "\0";
  char pclType[16] = "\0";
  char pclStat[16] = "\0";
  char pclTrit[16] = "\0";
  char pclCdat[16] = "\0";
	char pclAdid[16] = "\0";
	/*char pclRkey[16] = "\0";*/
	/*char pclRegn[16] = "\0";*/
	char pclAirbNewData[16] = "\0";
	char pclAirbOldData[16] = "\0";
	char pclAirb[16] = "\0";
	char pclStoa[16] = "\0";
	char pclStod[16] = "\0";
	char pclEtai[16] = "\0";
	char pclEtdi[16] = "\0";
	char pclOnbl[16] = "\0";
	char pclOfbl[16] = "\0";
	char pclPsta[16] = "\0";
	char pclPstd[16] = "\0";
	char pclTmoa[16] = "\0";
  char pclUrnoSelection[50] = "\0";
  char pclRecordURNO[50] = "\0";
	char pclUrnoOldData[50] = "\0";
	char pclUrnoNewData[50] = "\0";
  char pclDataSent[1024] = "\0";
	char pclNewData[512000] = "\0";
  char pclOldData[512000] = "\0";
  char pclSqlBuf[2048] = "\0",pclSqlData[2048] = "\0",pclWhere[2048] = "\0";
  char pclSqlBufT[2048] = "\0",pclSqlDataT[2048] = "\0", pclSelectionT[2048] = "\0";
  short slLocalCursor = 0, slFuncCode = 0;
  short slLocalCursorT = 0, slFuncCodeT = 0;

  char pclDataT[1024] = "\0";

  SENT_MSG rlSentMsg;
  TOWING   rlTowing[TOWING_FLT_NO];
	SENT_MSG rlSentMsgTowing[TOWING_FLT_NO];
	char pclDataSentTowing[1024] = "\0";

	/*@fya 20140310*/
	int ilNewTowingFlight = FALSE;
	/*
	@fya 20140311
	*/
	int ilTowingLIGTAB = 0;
	char pclUrnoP[16] = "\0";
	char pclRurnP[16] = "\0";
	char pclTypeP[16] = "\0";
	char pclTimeP[16] = "\0";
	char pclStatP[16] = "\0";
	char pclRtabP[16] = "\0";
	char pclRfldP[16] = "\0";
	char pclFval_OFB[512] = "\0";
	char pclFval_ONB[512] = "\0";

	memset(pclRecordURNO,0,sizeof(pclRecordURNO));
	memset(pclDataSent,0,sizeof(pclDataSent));
	memset(pclDataSentTowing,0,sizeof(pclDataSentTowing));
	memset(pclDataBuf,0,sizeof(pclDataBuf));
	memset(pclSqlBuf,0,sizeof(pclSqlBuf));
	memset(pclSqlData,0,sizeof(pclSqlData));
	memset(pclWhere,0,sizeof(pclWhere));
	memset(pclRkey,0,sizeof(pclRkey));
	memset(pclRegn,0,sizeof(pclRegn));
	memset(pclTifa,0,sizeof(pclTifa));
	memset(pclTifd,0,sizeof(pclTifd));
	memset(pclDataT,0,sizeof(pclDataT));
	memset(pclSelectionT,0,sizeof(pclSelectionT));

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
	else if (strcmp(cmdblk->command,"UFR") == 0 || strcmp(cmdblk->command,"IFR") == 0 || strcmp(cmdblk->command,"DFR") == 0)
	{
		dbg(DEBUG,"<%s> %s command is received", pclFunc,cmdblk->command);

		if (strcmp(cmdblk->command,"DFR") == 0)
		{
			dbg(TRACE,"<%s> The command is DFR, do nothing",pclFunc);
			return RC_SUCCESS;
		}/*@fya 20140310 For new towing record*/
		else if (strcmp(cmdblk->command,"IFR") == 0)
		{
			dbg(TRACE,"<%s> The command is IFR",pclFunc);

			if ( strstr(pclNewData,"TOWING") != 0 )
			{
				dbg(TRACE,"<%s> IFR - Towing Flight",pclFunc);
				ilItemNo = get_item_no(pclFields, "URNO", 5) + 1;
				if (ilItemNo <= 0)
				{
					dbg(TRACE, "<%s> No URNO Found in the field list, can't do anything", pclFunc);
		      return RC_FAIL;
				}

				get_real_item(pclUrnoNewData, pclNewData, ilItemNo);
    		get_real_item(pclUrnoOldData, pclOldData, ilItemNo);
    		dbg(DEBUG,"<%s>The New URNO is <%s>", pclFunc, pclUrnoNewData);
				dbg(DEBUG,"<%s>The Old URNO is <%s>", pclFunc, pclUrnoOldData);

				if ( atoi(pclUrnoNewData) != 0 && atoi(pclUrnoOldData) == 0)
				{
					dbg(TRACE, "<%s> Towing Creation", pclFunc);

					ilNewTowingFlight = TRUE;
				}
			}
		}
		else if (strcmp(cmdblk->command,"UFR") == 0)
		{
			dbg(TRACE,"<%s> The command is UFR",pclFunc);
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
		if ((ilNoEleField == ilNoEleNewData) && (ilNoEleNewData == ilNoEleOldData))
		{
			ilItemNo = get_item_no(pclFields, "URNO", 5) + 1;
			if (ilItemNo <= 0)
			{
				dbg(TRACE, "<%s> No URNO Found in the field list, can't do anything", pclFunc);
	      return RC_FAIL;
			}
			/*
			1) Getting URNO -> Getting the URNO from selection statement
			*/
    	get_real_item(pclUrnoNewData, pclNewData, ilItemNo);
    	get_real_item(pclUrnoOldData, pclOldData, ilItemNo);
    	dbg(DEBUG,"<%s>The New URNO is <%s>", pclFunc, pclUrnoNewData);
			dbg(DEBUG,"<%s>The Old URNO is <%s>", pclFunc, pclUrnoOldData);

			/*
			2)Getting ADID
			*/
			ilItemNo = get_item_no(pclFields, "ADID", 5) + 1;
			if (ilItemNo <= 0)
			{
				dbg(TRACE,"<%s> ADID is not included in the field list, can not proceed",pclFunc);
	    	return RC_FAIL;
			}
			get_real_item(pclAdidNewData, pclNewData, ilItemNo);
			get_real_item(pclAdidOldData, pclOldData, ilItemNo);
			dbg(DEBUG,"<%s>The New ADID is <%s>", pclFunc, pclAdidNewData);
			dbg(DEBUG,"<%s>The Old ADID is <%s>", pclFunc, pclAdidOldData);

			TrimSpace(pclAdidNewData);
			if (strlen(pclAdidNewData) == 0)
			{
				dbg(TRACE,"<%s>New ADID is null, can not proceed",pclFunc);
				return RC_FAIL;
			}

			/*
			3)Getting AIRB
			*/
			ilItemNo = get_item_no(pclFields, "AIRB", 5) + 1;
			if (ilItemNo <= 0)
			{
				dbg(TRACE,"<%s> AIRB is not included in the field list, can not proceed",pclFunc);
	    	return RC_FAIL;
			}
			get_real_item(pclAirbNewData, pclNewData, ilItemNo);
			get_real_item(pclAirbOldData, pclOldData, ilItemNo);
			dbg(DEBUG,"<%s>The New AIRB is <%s>", pclFunc, pclAirbNewData);
			dbg(DEBUG,"<%s>The Old AIRB is <%s>", pclFunc, pclAirbOldData);

			/*
			4)Getting OFBL
			*/
			ilItemNo =  get_item_no(pclFields, "OFBL", 5) + 1;
    	if (ilItemNo <= 0)
			{
				dbg(TRACE,"<%s> OFBL is not included in the field list, can not proceed",pclFunc);
	    	return RC_FAIL;
			}
			get_real_item(pclOfblNewData, pclNewData, ilItemNo);
			get_real_item(pclOfblOldData, pclOldData, ilItemNo);
			dbg(DEBUG,"<%s>The New OFBL is <%s>", pclFunc, pclOfblNewData);
			dbg(DEBUG,"<%s>The Old OFBL is <%s>", pclFunc, pclOfblOldData);

			if ( strcmp(cmdblk->command,"IFR") == 0 || strcmp(cmdblk->command,"UFR") == 0 )
			{
				/*
				@fya 20140304
				For (arrival and) departure flight, if ATD=AIRB is not null, then find the next message for the next allocation(Scheduled)
				*/
				TrimSpace(pclAirbNewData);
				if( atoi(pclAirbNewData) != 0 )
				{
					/*
					AIRB is not null
					*/
					dbg(TRACE,"<%s> AIRB is not null",pclFunc);

					/*if ( strncmp(pclAdidNewData,"A",1) == 0 || strncmp(pclAdidNewData,"D",1) == 0 )*/
					if ( strncmp(pclAdidNewData,"D",1) == 0 )
					{
						dbg(TRACE,"<%s> AIRB is not null and ADID is <%s>, so send the next message for next allocation",pclFunc, pclAdidNewData);

						ilItemNo = get_item_no(pclFields, "PSTD", 5) + 1;
						if( ilItemNo <= 0)
		    		{
		    			dbg(TRACE,"<%s> PSTD is not included in the field list, can not proceed",pclFunc);
							return RC_FAIL;
		    		}
		    		get_real_item(pclPstdNewData, pclNewData, ilItemNo);
		    		get_real_item(pclPstdOldData, pclOldData, ilItemNo);
		    		dbg(DEBUG,"<%s> New PSTD<%s>",pclFunc,pclPstdNewData);
		    		dbg(DEBUG,"<%s> Old PSTD<%s>",pclFunc,pclPstdOldData);

						ilItemNo = get_item_no(pclFields, "TIFD", 5) + 1;
						if( ilItemNo <= 0)
		    		{
		    			dbg(TRACE,"<%s> TIFD is not included in the field list, can not proceed",pclFunc);
							return RC_FAIL;
		    		}
		    		get_real_item(pclTifdNewData, pclNewData, ilItemNo);
		    		get_real_item(pclTifdOldData, pclOldData, ilItemNo);
		    		dbg(DEBUG,"<%s> New TIFD<%s>",pclFunc,pclTifdNewData);
		    		dbg(DEBUG,"<%s> Old TIFD<%s>",pclFunc,pclTifdOldData);

		    		/*
		    		Start Searching for the Next Scheduled Allocation in the same parking stand -> Departure Flight without AIRB

		 				The reason of firstly finding the departure flight without AIRB for the next allocation is that if searching the arrival flight for the next allocation first, then the related departure flight may also has AIRB.
		    		*/
		    		memset(pclWhere,0,sizeof(pclWhere));
		    		memset(pclSqlBuf,0,sizeof(pclSqlBuf));
		    		memset(pclSqlData,0,sizeof(pclSqlData));
		    		FindNextAllocDepBuildWhereClause(pclWhere,pclPstdNewData,pclTifdNewData);
		    		FindNextAllocDepBuildFullQuery(pclSqlBuf,pclWhere);

		    		/*
		    		URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA,AIRB
		    		*/
		    		memset(pclUrno,0,sizeof(pclUrno));
						memset(pclAdid,0,sizeof(pclAdid));
						memset(pclRkey,0,sizeof(pclRkey));
						memset(pclRegn,0,sizeof(pclRegn));
						memset(pclStoa,0,sizeof(pclStoa));
						memset(pclStod,0,sizeof(pclStod));
						memset(pclEtai,0,sizeof(pclEtai));
						memset(pclEtdi,0,sizeof(pclEtdi));
						memset(pclTifa,0,sizeof(pclTifa));
						memset(pclTifd,0,sizeof(pclTifd));
						memset(pclOnbl,0,sizeof(pclOnbl));
						memset(pclOfbl,0,sizeof(pclOfbl));
						memset(pclPsta,0,sizeof(pclPsta));
						memset(pclPstd,0,sizeof(pclPstd));
						memset(pclPstd,0,sizeof(pclTmoa));
						memset(pclAirb,0,sizeof(pclAirb));

						ilRC = RunSQL(pclSqlBuf, pclSqlData);
		    		if (ilRC != DB_SUCCESS)
		    		{
		        	dbg(TRACE, "<%s>: Next Allocation is not found in AFTTAB, line<%d>", pclFunc,__LINE__);
		        	return RC_FAIL;
		    		}

		    		switch(ilRC)
						{
							case NOTFOUND:
								dbg(TRACE, "<%s> The search for next allocation dep flight fails", pclFunc);
								return RC_FAIL;

								break;
							default:/*
											@fya 20140304
											Found
											*/
				    		get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
								get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
								get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
								get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
								get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
								get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
								get_fld(pclSqlData,FIELD_7,STR,20,pclEtai); TrimSpace(pclEtai);
								get_fld(pclSqlData,FIELD_8,STR,20,pclEtdi); TrimSpace(pclEtdi);
								get_fld(pclSqlData,FIELD_9,STR,20,pclTifa); TrimSpace(pclTifa);
								get_fld(pclSqlData,FIELD_10,STR,20,pclTifd); TrimSpace(pclTifd);
								get_fld(pclSqlData,FIELD_11,STR,20,pclOnbl); TrimSpace(pclOnbl);
								get_fld(pclSqlData,FIELD_12,STR,20,pclOfbl); TrimSpace(pclOfbl);
								get_fld(pclSqlData,FIELD_13,STR,20,pclPsta); TrimSpace(pclPsta);
								get_fld(pclSqlData,FIELD_14,STR,20,pclPstd); TrimSpace(pclPstd);
								get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);
								get_fld(pclSqlData,FIELD_16,STR,20,pclAirb); TrimSpace(pclAirb);

								dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
								dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
								dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
								dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
								dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
								dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
								dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
								dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
								dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
								dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
								dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
								dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
								dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
								dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
								dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
								dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);
								dbg(DEBUG,"<%s>pclAirb<%s>",pclFunc,pclAirb);

								if( atoi(pclAirb) != 0 )
								{
									dbg(TRACE,"<%s> Based on the query, AIRB should be null",pclFunc);

									return RC_FAIL;
								}
								else
								{
									/*
									At this stage, the departure part is ready, filling it.
									*/
									memset(&rlSentMsg,0,sizeof(rlSentMsg));
									BuildDepPart(&rlSentMsg, pclPstd, pclStod, pclEtdi, pclOfbl);
									/*
									Then search the corresponding arrival flight
									*/
									memset(pclWhere,0,sizeof(pclWhere));
					    		memset(pclSqlBuf,0,sizeof(pclSqlBuf));
					    		memset(pclSqlData,0,sizeof(pclSqlData));
					    		FindNextAllocArrBuildWhereClause(pclWhere,pclPstdNewData,pclTifdNewData,pclTifd);
					    		FindNextAllocArrBuildFullQuery(pclSqlBuf,pclWhere);

					    		memset(pclUrno,0,sizeof(pclUrno));
									memset(pclAdid,0,sizeof(pclAdid));
									memset(pclRkey,0,sizeof(pclRkey));
									memset(pclRegn,0,sizeof(pclRegn));
									memset(pclStoa,0,sizeof(pclStoa));
									memset(pclStod,0,sizeof(pclStod));
									memset(pclEtai,0,sizeof(pclEtai));
									memset(pclEtdi,0,sizeof(pclEtdi));
									memset(pclTifa,0,sizeof(pclTifa));
									memset(pclTifd,0,sizeof(pclTifd));
									memset(pclOnbl,0,sizeof(pclOnbl));
									memset(pclOfbl,0,sizeof(pclOfbl));
									memset(pclPsta,0,sizeof(pclPsta));
									memset(pclPstd,0,sizeof(pclPstd));
									memset(pclPstd,0,sizeof(pclTmoa));
									memset(pclAirb,0,sizeof(pclAirb));

					    		ilRC = RunSQL(pclSqlBuf, pclSqlData);
					    		if (ilRC != DB_SUCCESS)
					    		{
					        	dbg(TRACE, "<%s>: Next Allocation is not found in AFTTAB, line<%d>", pclFunc,__LINE__);
					        	/*return RC_FAIL;*/
					    		}

					    		switch(ilRC)
									{
										case NOTFOUND:
											dbg(TRACE, "<%s> The search for next allocation arr flight not found", pclFunc);
											/*This means the arrival part is not found, so keep the default value for the arrival part*/
											break;
										default:
											/*This means the arrival part is found, so filling values for the arrival part*/
											get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
											get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
											get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
											get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
											get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
											get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
											get_fld(pclSqlData,FIELD_7,STR,20,pclEtai); TrimSpace(pclEtai);
											get_fld(pclSqlData,FIELD_8,STR,20,pclEtdi); TrimSpace(pclEtdi);
											get_fld(pclSqlData,FIELD_9,STR,20,pclTifa); TrimSpace(pclTifa);
											get_fld(pclSqlData,FIELD_10,STR,20,pclTifd); TrimSpace(pclTifd);
											get_fld(pclSqlData,FIELD_11,STR,20,pclOnbl); TrimSpace(pclOnbl);
											get_fld(pclSqlData,FIELD_12,STR,20,pclOfbl); TrimSpace(pclOfbl);
											get_fld(pclSqlData,FIELD_13,STR,20,pclPsta); TrimSpace(pclPsta);
											get_fld(pclSqlData,FIELD_14,STR,20,pclPstd); TrimSpace(pclPstd);
											get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

											dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
											dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
											dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
											dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
											dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
											dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
											dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
											dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
											dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
											dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
											dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
											dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
											dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
											dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
											dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
											dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);
											dbg(DEBUG,"<%s>pclAirb<%s>",pclFunc,pclAirb);

											strcpy(rlSentMsg.pclStoa, pclStoa);
											strcpy(rlSentMsg.pclEtai, pclEtai);
											strcpy(rlSentMsg.pclTmoa, pclTmoa);
											strcpy(rlSentMsg.pclOnbl, pclOnbl);
											break;
									}
									ShowMsgStruct(rlSentMsg);

									memset(pclDataSent,0,sizeof(pclDataSent));
									BuildSentData(pclDataSent,rlSentMsg);

									StoreSentData(pclDataSent,pclUrnoNewData,"Normal",pclRecordURNO);

							    strcat(pclDataSent,"\n");
									strcpy(pcgCurSendData,pclDataSent);
							    strcpy(pcgSendMsgId,pclRecordURNO);

									/**Send out the message*/
									for (ilCount = 0; ilCount < igReSendMax; ilCount++)
					    		{
							  		if (igSock > 0)
							      {
							      	/*
							      	@fya 20140304
							      	strcat(pclDataSent,"\n");
							      	*/
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
								        /*SendRST_Command();*/
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
							        	/*SendRST_Command();*/
							        }
							        else
							        	  ilRC = RC_FAIL;
							      }
							    }

							    /* fya 20140227 sending ack right after normal message*/
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
								        /*SendRST_Command();*/
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
							        	/*SendRST_Command();*/
							        }
							        else
							        	  ilRC = RC_FAIL;
							      }
							    }

									if( ilCount >= igReSendMax)
									{
										dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);
										return RC_FAIL;
									}

									return RC_SUCCESS;
								}
								break;
						}
					}
					else
					{
						dbg(TRACE,"<%s> Although AIRB is not null, the ADID is <%s> != D",pclFunc, pclAdidNewData);
					}
				}
				else
				{
					dbg(TRACE,"<%s> AIRB is null",pclFunc);
				}

				/*
				@fya 20140304
				For towing flight, if off-block=OFBL is not null, then find the next message for the next allocation(Scheduled) which will be sent
				*/

				TrimSpace(pclOfblNewData);
				if( atoi(pclOfblNewData) != 0 )
				{
					/*
					OFBL is not null
					*/
					dbg(TRACE,"<%s> OFBL is not null",pclFunc);

					if ( strncmp(pclAdidNewData,"B",1) == 0 )
					{
						dbg(TRACE,"<%s> OFBL is not null and ADID is <%s>, so send the next message for next allocation",pclFunc, pclAdidNewData);

						ilItemNo = get_item_no(pclFields, "PSTA", 5) + 1;
						if( ilItemNo <= 0)
		    		{
		    			dbg(TRACE,"<%s> PSTA is not included in the field list, can not proceed",pclFunc);
							return RC_FAIL;
		    		}
		    		get_real_item(pclPstaNewData, pclNewData, ilItemNo);
		    		get_real_item(pclPstaOldData, pclOldData, ilItemNo);
		    		dbg(DEBUG,"<%s> New PSTA<%s>",pclFunc,pclPstaNewData);
		    		dbg(DEBUG,"<%s> Old PSTA<%s>",pclFunc,pclPstaOldData);

						ilItemNo = get_item_no(pclFields, "TIFA", 5) + 1;
						if( ilItemNo <= 0)
		    		{
		    			dbg(TRACE,"<%s> TIFA is not included in the field list, can not proceed",pclFunc);
							return RC_FAIL;
		    		}
		    		get_real_item(pclTifaNewData, pclNewData, ilItemNo);
		    		get_real_item(pclTifaOldData, pclOldData, ilItemNo);
		    		dbg(DEBUG,"<%s> New TIFA<%s>",pclFunc,pclTifaNewData);
		    		dbg(DEBUG,"<%s> Old TIFA<%s>",pclFunc,pclTifaOldData);

						/*
		    		Start Searching for the Next Scheduled Allocation in the same parking stand -> Departure Flight without AIRB

		 				The reason of firstly finding the departure flight without AIRB for the next allocation is that if searching the arrival flight for the next allocation first, then the related departure flight may also has AIRB.
		    		*/
		    		memset(pclWhere,0,sizeof(pclWhere));
		    		memset(pclSqlBuf,0,sizeof(pclSqlBuf));
		    		memset(pclSqlData,0,sizeof(pclSqlData));
		    		FindNextAllocDepBuildWhereClause(pclWhere,pclPstaNewData,pclTifaNewData);
		    		FindNextAllocDepBuildFullQuery(pclSqlBuf,pclWhere);
		    		/*URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA,AIRB*/
		    		memset(pclUrno,0,sizeof(pclUrno));
						memset(pclAdid,0,sizeof(pclAdid));
						memset(pclRkey,0,sizeof(pclRkey));
						memset(pclRegn,0,sizeof(pclRegn));
						memset(pclStoa,0,sizeof(pclStoa));
						memset(pclStod,0,sizeof(pclStod));
						memset(pclEtai,0,sizeof(pclEtai));
						memset(pclEtdi,0,sizeof(pclEtdi));
						memset(pclTifa,0,sizeof(pclTifa));
						memset(pclTifd,0,sizeof(pclTifd));
						memset(pclOnbl,0,sizeof(pclOnbl));
						memset(pclOfbl,0,sizeof(pclOfbl));
						memset(pclPsta,0,sizeof(pclPsta));
						memset(pclPstd,0,sizeof(pclPstd));
						memset(pclPstd,0,sizeof(pclTmoa));
						memset(pclAirb,0,sizeof(pclAirb));

						ilRC = RunSQL(pclSqlBuf, pclSqlData);
		    		if (ilRC != DB_SUCCESS)
		    		{
		        	dbg(TRACE, "<%s>: Next Allocation is not found in AFTTAB, line<%d>", pclFunc,__LINE__);
		        	return RC_FAIL;
		    		}

		    		switch(ilRC)
						{
							case NOTFOUND:
								dbg(TRACE, "<%s> The search for next allocation dep flight fails", pclFunc);
								return RC_FAIL;

								break;
							default:/*
											@fya 20140304
											Found
											URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA,AIRB
											*/
				    		get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
								get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
								get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
								get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
								get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
								get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
								get_fld(pclSqlData,FIELD_7,STR,20,pclEtai); TrimSpace(pclEtai);
								get_fld(pclSqlData,FIELD_8,STR,20,pclEtdi); TrimSpace(pclEtdi);
								get_fld(pclSqlData,FIELD_9,STR,20,pclTifa); TrimSpace(pclTifa);
								get_fld(pclSqlData,FIELD_10,STR,20,pclTifd); TrimSpace(pclTifd);
								get_fld(pclSqlData,FIELD_11,STR,20,pclOnbl); TrimSpace(pclOnbl);
								get_fld(pclSqlData,FIELD_12,STR,20,pclOfbl); TrimSpace(pclOfbl);
								get_fld(pclSqlData,FIELD_13,STR,20,pclPsta); TrimSpace(pclPsta);
								get_fld(pclSqlData,FIELD_14,STR,20,pclPstd); TrimSpace(pclPstd);
								get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);
								get_fld(pclSqlData,FIELD_16,STR,20,pclAirb); TrimSpace(pclAirb);

								dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
								dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
								dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
								dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
								dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
								dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
								dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
								dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
								dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
								dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
								dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
								dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
								dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
								dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
								dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
								dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);
								dbg(DEBUG,"<%s>pclAirb<%s>",pclFunc,pclAirb);

								if( atoi(pclAirb) != 0 )
								{
									dbg(TRACE,"<%s> Based on the query, AIRB should be null",pclFunc);

									return RC_FAIL;
								}
								else
								{
									/*
									At this stage, the departure part is ready, filling it.
									*/
									memset(&rlSentMsg,0,sizeof(rlSentMsg));
									BuildDepPart(&rlSentMsg, pclPstd, pclStod, pclEtdi, pclOfbl);
									/*
									Then search the corresponding arrival flight
									*/
									memset(pclWhere,0,sizeof(pclWhere));
					    		memset(pclSqlBuf,0,sizeof(pclSqlBuf));
					    		memset(pclSqlData,0,sizeof(pclSqlData));

					    		/*
					    		URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA
					    		*/
					    		FindNextAllocArrBuildWhereClause(pclWhere,pclPstd,pclTifaNewData,pclTifa);
					    		FindNextAllocArrBuildFullQuery(pclSqlBuf,pclWhere);

					    		memset(pclUrno,0,sizeof(pclUrno));
									memset(pclAdid,0,sizeof(pclAdid));
									memset(pclRkey,0,sizeof(pclRkey));
									memset(pclRegn,0,sizeof(pclRegn));
									memset(pclStoa,0,sizeof(pclStoa));
									memset(pclStod,0,sizeof(pclStod));
									memset(pclEtai,0,sizeof(pclEtai));
									memset(pclEtdi,0,sizeof(pclEtdi));
									memset(pclTifa,0,sizeof(pclTifa));
									memset(pclTifd,0,sizeof(pclTifd));
									memset(pclOnbl,0,sizeof(pclOnbl));
									memset(pclOfbl,0,sizeof(pclOfbl));
									memset(pclPsta,0,sizeof(pclPsta));
									memset(pclPstd,0,sizeof(pclPstd));
									memset(pclPstd,0,sizeof(pclTmoa));

					    		ilRC = RunSQL(pclSqlBuf, pclSqlData);
					    		if (ilRC != DB_SUCCESS)
					    		{
					        	dbg(TRACE, "<%s>: Next Allocation is not found in AFTTAB, line<%d>", pclFunc,__LINE__);
					        	/*return RC_FAIL;*/
					    		}

					    		switch(ilRC)
									{
										case NOTFOUND:
											dbg(TRACE, "<%s> The search for next allocation arr flight not found", pclFunc);
											/*This means the arrival part is not found, so keep the default value for the arrival part*/
											break;
										default:
											/*This means the arrival part is found, so filling values for the arrival part*/

											/*
					    				URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA
					    				*/
											get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
											get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
											get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
											get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
											get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
											get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
											get_fld(pclSqlData,FIELD_7,STR,20,pclEtai); TrimSpace(pclEtai);
											get_fld(pclSqlData,FIELD_8,STR,20,pclEtdi); TrimSpace(pclEtdi);
											get_fld(pclSqlData,FIELD_9,STR,20,pclTifa); TrimSpace(pclTifa);
											get_fld(pclSqlData,FIELD_10,STR,20,pclTifd); TrimSpace(pclTifd);
											get_fld(pclSqlData,FIELD_11,STR,20,pclOnbl); TrimSpace(pclOnbl);
											get_fld(pclSqlData,FIELD_12,STR,20,pclOfbl); TrimSpace(pclOfbl);
											get_fld(pclSqlData,FIELD_13,STR,20,pclPsta); TrimSpace(pclPsta);
											get_fld(pclSqlData,FIELD_14,STR,20,pclPstd); TrimSpace(pclPstd);
											get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

											dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
											dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
											dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
											dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
											dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
											dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
											dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
											dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
											dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
											dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
											dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
											dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
											dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
											dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
											dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
											dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);

											strcpy(rlSentMsg.pclStoa, pclStoa);
											strcpy(rlSentMsg.pclEtai, pclEtai);
											strcpy(rlSentMsg.pclTmoa, pclTmoa);
											strcpy(rlSentMsg.pclOnbl, pclOnbl);
											break;
									}
									ShowMsgStruct(rlSentMsg);

									memset(pclDataSent,0,sizeof(pclDataSent));
									BuildSentData(pclDataSent,rlSentMsg);
									dbg(TRACE,"<%s> pclDataSent<%s>",pclFunc,pclDataSent);
									StoreSentData(pclDataSent,pclUrnoNewData,"Towing",pclRecordURNO);
									return RC_SUCCESS;
								}
								break;
						}
					}
					else
					{
						dbg(TRACE,"<%s> Although OFBL is not null, the ADID is <%s> != B",pclFunc, pclAdidNewData);
					}
				}
				else
				{
					dbg(TRACE,"<%s> OFBL is null",pclFunc);
				}
			}
			/*
			For arrival, departure flight without ATD=AIRB time
			For towing flight without off-block=OFBL time
			*/
			if ( strncmp(pclAdidNewData,"A",1) == 0 || strncmp(pclAdidNewData,"B",1) == 0)
			{
				/*#ifndef TEST*/
				dbg(DEBUG,"<%s>ADID = A|B",pclFunc);
				/* 3) Getting the PSTA for building the message*/
				ilItemNo = get_item_no(pclFields, "PSTA", 5) + 1;
				if( ilItemNo <= 0)
    		{
    			dbg(TRACE,"<%s> PSTA is not included in the field list, can not proceed",pclFunc);
					return RC_FAIL;
    		}
    		get_real_item(pclPstaNewData, pclNewData, ilItemNo);
    		get_real_item(pclPstaOldData, pclOldData, ilItemNo);
    		dbg(DEBUG,"<%s> New PSTA<%s>",pclFunc,pclPstaNewData);
    		dbg(DEBUG,"<%s> Old PSTA<%s>",pclFunc,pclPstaOldData);

    		/*4) Getting the STOA for building the message*/
		  	ilItemNo =  get_item_no(pclFields, "STOA", 5) + 1;
		  	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> STOA is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclStoaNewData, pclNewData, ilItemNo);
				get_real_item(pclStoaOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New STOA is <%s>", pclFunc, pclStoaNewData);
				dbg(DEBUG,"<%s>The Old STOA is <%s>", pclFunc, pclStoaOldData);

				/*5)
				Getting the ETAI/ETDI for building the message
				*/

	    	ilItemNo =  get_item_no(pclFields, "ETAI", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> ETAI is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclEtaiNewData, pclNewData, ilItemNo);
				get_real_item(pclEtaiOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New ETAI is <%s>", pclFunc, pclEtaiNewData);
				dbg(DEBUG,"<%s>The Old ETAI is <%s>", pclFunc, pclEtaiOldData);

				ilItemNo =  get_item_no(pclFields, "ETDI", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> ETDI is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclEtdiNewData, pclNewData, ilItemNo);
				get_real_item(pclEtdiOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New ETDI is <%s>", pclFunc, pclEtdiNewData);
				dbg(DEBUG,"<%s>The Old ETDI is <%s>", pclFunc, pclEtdiOldData);

				/*6) Getting the TMOA for building the message*/
	    	ilItemNo =  get_item_no(pclFields, "TMOA", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> TMOA is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclTmoaNewData, pclNewData, ilItemNo);
				get_real_item(pclTmoaOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New TMOA is <%s>", pclFunc, pclTmoaNewData);
				dbg(DEBUG,"<%s>The Old TMOA is <%s>", pclFunc, pclTmoaOldData);

				/*7) Getting the TMOA for building the message*/
	    	ilItemNo =  get_item_no(pclFields, "ONBL", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> ONBL is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclOnblNewData, pclNewData, ilItemNo);
				get_real_item(pclOnblOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New ONBL is <%s>", pclFunc, pclOnblNewData);
				dbg(DEBUG,"<%s>The Old ONBL is <%s>", pclFunc, pclOnblOldData);
				/*#endif*/
				/*psta, stoa, etai, tmoa, onbl*/

				BuildArrPart(&rlSentMsg, pclPstaNewData, pclStoaNewData, pclEtaiNewData, pclTmoaNewData, pclOnblNewData);

				dbg(DEBUG,"<%s> After BuildArrPart, Show msg struct",pclFunc);
				/*ShowMsgStruct(rlSentMsg);*/

				/*Next Step is to complete the Departure part in Msg
				1)Based on RKEY and REGN, finding the nearest corresponding followed departure/towing flight from afttab
				1.1 Getting the REGN & RKEY from afttab
				1.2 Getting sequential flight
				*/
				memset(pclSqlBuf,0,sizeof(pclSqlBuf));
				memset(pclSqlData,0,sizeof(pclSqlData));
				sprintf(pclSelection, "WHERE URNO = %s", pclUrnoNewData);
    		sprintf(pclSqlBuf, "SELECT RKEY,REGN,TIFA,TIFD FROM AFTTAB %s", pclSelection);

    		ilRC = RunSQL(pclSqlBuf, pclSqlData);
    		if (ilRC != DB_SUCCESS)
    		{
        	dbg(DEBUG, "%s: URNO <%s> not found in AFTTAB", pclFunc, pclUrnoNewData);
        	return RC_FAIL;
    		}
    		get_fld(pclSqlData,FIELD_1,STR,20,pclRkey);
  			get_fld(pclSqlData,FIELD_2,STR,20,pclRegn);
  			get_fld(pclSqlData,FIELD_3,STR,20,pclTifa);
  			get_fld(pclSqlData,FIELD_4,STR,20,pclTifd);
  			dbg(TRACE, "<%s> RKEY <%s> REGN <%s> TIFA <%s> TIFD <%s>", pclFunc, pclRkey, pclRegn, pclTifa, pclTifd);

  			TrimSpace(pclRkey);
  			/*
				@fya 201440303
				keep regn as it is
  			TrimSpace(pclRegn);
  			*/
  			TrimSpace(pclTifa);
  			TrimSpace(pclTifd);

  			strcpy(rlSentMsg.pclRkey,pclRkey);
				strcpy(rlSentMsg.pclRegn,pclRegn);
				strcpy(rlSentMsg.pclTifa,pclTifa);
				strcpy(rlSentMsg.pclTifd,pclTifd);

				dbg(DEBUG,"+++++++++++");

				ShowMsgStruct(rlSentMsg);

				if (strncmp(pclAdidNewData,"A",1) == 0)
				{
					ilRC = GetSeqFlight(&rlSentMsg,"D");
					ilFlag = 1;
				}
				else if (strncmp(pclAdidNewData,"B",1) == 0)
				{
					/*Search all related towing flights and store them into LIGTAB
					Towing Flights are triggered by NTICH command in every minutes*/

					memset(pclWhere,0,sizeof(pclWhere));
					memset(pclSqlBuf,0,sizeof(pclSqlBuf));
					SearchTowingFlightBuildWhereClause(pclRkey,pclRegn,pclWhere);
					SearchTowingFlightBuildFullQuery(pclSqlBuf,pclWhere);

					memset(pclUrno,0,sizeof(pclUrno));
					memset(pclAdid,0,sizeof(pclAdid));
					memset(pclRkey,0,sizeof(pclRkey));
					memset(pclRegn,0,sizeof(pclRegn));
					memset(pclStoa,0,sizeof(pclStoa));
					memset(pclStod,0,sizeof(pclStod));
					memset(pclEtai,0,sizeof(pclEtai));
					memset(pclEtdi,0,sizeof(pclEtdi));
					memset(pclTifa,0,sizeof(pclTifa));
					memset(pclTifd,0,sizeof(pclTifd));
					memset(pclOnbl,0,sizeof(pclOnbl));
					memset(pclOfbl,0,sizeof(pclOfbl));
					memset(pclPsta,0,sizeof(pclPsta));
					memset(pclPstd,0,sizeof(pclPstd));
					memset(pclPstd,0,sizeof(pclTmoa));

					ilCountTowing = 0;
					slLocalCursor = 0;
					slFuncCode = START;
					memset(pclSqlData,0,sizeof(pclSqlData));
					memset(rlTowing,0,sizeof(rlTowing));
					while(sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS)
					{
					  slFuncCode = NEXT;

					  /*URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD*/
					  /*URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA*/
					 	get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
						get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
						get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
						get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);

						get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
						get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
						get_fld(pclSqlData,FIELD_7,STR,20,pclEtai); TrimSpace(pclEtai);
						get_fld(pclSqlData,FIELD_8,STR,20,pclEtdi); TrimSpace(pclEtdi);

						get_fld(pclSqlData,FIELD_9,STR,20,pclTifa); TrimSpace(pclTifa);
						get_fld(pclSqlData,FIELD_10,STR,20,pclTifd); TrimSpace(pclTifd);
						get_fld(pclSqlData,FIELD_11,STR,20,pclOnbl); TrimSpace(pclOnbl);
						get_fld(pclSqlData,FIELD_12,STR,20,pclOfbl); TrimSpace(pclOfbl);
						get_fld(pclSqlData,FIELD_13,STR,20,pclPsta); TrimSpace(pclPsta);
						get_fld(pclSqlData,FIELD_14,STR,20,pclPstd); TrimSpace(pclPstd);
						get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

						strcpy(rlTowing[ilCountTowing].pclUrno,pclUrno);
						strcpy(rlTowing[ilCountTowing].pclAdid,pclAdid);
						strcpy(rlTowing[ilCountTowing].pclRkey,pclRkey);
						strcpy(rlTowing[ilCountTowing].pclRegn,pclRegn);

						strcpy(rlTowing[ilCountTowing].pclStoa,pclStoa);
						strcpy(rlTowing[ilCountTowing].pclStod,pclStod);
						strcpy(rlTowing[ilCountTowing].pclEtai,pclEtai);
						strcpy(rlTowing[ilCountTowing].pclEtdi,pclEtdi);
						strcpy(rlTowing[ilCountTowing].pclTifa,pclTifa);
						strcpy(rlTowing[ilCountTowing].pclTifd,pclTifd);

						/*
						@fya 20140311
						*/
						if( atoi(pclUrno) == atoi(pclUrnoNewData) && strncmp(pclAdid,"B",1) == 0 )
						{
							if( ilNewTowingFlight == TRUE )
							{
								memset(pclUrnoP,0,sizeof(pclUrnoP));
								memset(pclRurnP,0,sizeof(pclRurnP));
								memset(pclTypeP,0,sizeof(pclTypeP));
								memset(pclTimeP,0,sizeof(pclTimeP));
								memset(pclStatP,0,sizeof(pclStatP));
								memset(pclRtabP,0,sizeof(pclRtabP));
								memset(pclRfldP,0,sizeof(pclRfldP));

								memset(pclFval_OFB,0,sizeof(pclFval_OFB));
								memset(pclFval_ONB,0,sizeof(pclFval_ONB));

								/* For retrive of OFBL from PDETAB*/
								memset(pclSqlBuf,0,sizeof(pclSqlBuf));
								memset(pclSqlData,0,sizeof(pclSqlData));

								sprintf(pclSelectionT, "WHERE RURN = '%s' AND TYPE = 'OFB' order by TIME desc", pclUrno);
                                sprintf(pclSqlBuf, "SELECT URNO,RURN,TYPE,TIME,STAT,RTAB,RFLD,FVAL FROM PDETAB %s", pclSelectionT);

                                dbg(TRACE,"<%s> 1-PDETAB pclSqlBuf<%s>",pclFunc,pclSqlBuf);

                                ilRC = RunSQL(pclSqlBuf, pclSqlData);
                                if (ilRC != DB_SUCCESS)
                                {
                                    dbg(DEBUG, "%s: RURN <%s> not found in PDETAB for OFB", pclFunc, pclUrnoNewData);
                                    /*return RC_FAIL;*/

                                    memset(pclSqlBuf,0,sizeof(pclSqlBuf));
                                    memset(pclSqlData,0,sizeof(pclSqlData));
                                    memset(pclSelectionT,0,sizeof(pclSelectionT));

                                    sprintf(pclSelectionT, "WHERE URNO = '%s'", pclUrno);
                                    sprintf(pclSqlBuf, "SELECT URNO,OFBL FROM AFTTAB %s", pclSelectionT);

                                    dbg(TRACE,"<%s> 1-AFTTAB-OFBL pclSqlBuf<%s>",pclFunc,pclSqlBuf);

                                    ilRC = RunSQL(pclSqlBuf, pclSqlData);
                                    if (ilRC != DB_SUCCESS)
                                    {
                                        dbg(DEBUG, "%s: URNO <%s> not found in AFTTAB for OFB", pclFunc, pclUrnoNewData);
                                    }
                                    else
                                    {
                                        get_fld(pclSqlData,FIELD_1,STR,20,pclUrnoP);
                                        get_fld(pclSqlData,FIELD_2,STR,512,pclFval_OFB);
                                        dbg(TRACE,"<%s> URNO<%s> OFBL<%s>",pclFunc,pclFval_OFB);
                                    }
                                }
                                else
                                {
                                    dbg(TRACE, "<%s> 1-PDETAB Record <%s>", pclFunc, pclSqlData);

                                    get_fld(pclSqlData,FIELD_1,STR,20,pclUrnoP);
                                    get_fld(pclSqlData,FIELD_2,STR,20,pclRurnP);
                                    get_fld(pclSqlData,FIELD_3,STR,20,pclTypeP);
                                    get_fld(pclSqlData,FIELD_4,STR,20,pclTimeP);
                                    get_fld(pclSqlData,FIELD_5,STR,20,pclStatP);
                                    get_fld(pclSqlData,FIELD_6,STR,20,pclRtabP);
                                    get_fld(pclSqlData,FIELD_7,STR,20,pclRfldP);
                                    get_fld(pclSqlData,FIELD_8,STR,512,pclFval_OFB);

                                    dbg(TRACE, "<%s> URNO <%s> RURN <%s> TYPE <%s> TIME <%s> STAT <%s> RTAB <%s> RFLD <%s> FVAL_OFBL <%s> ", pclFunc, pclUrnoP, pclRurnP, pclTypeP, pclTimeP, pclStatP, pclRtabP, pclRfldP, pclFval_OFB);
                                }

                                /************************************************************/

                                memset(pclUrnoP,0,sizeof(pclUrnoP));
                                memset(pclRurnP,0,sizeof(pclRurnP));
								memset(pclTypeP,0,sizeof(pclTypeP));
								memset(pclTimeP,0,sizeof(pclTimeP));
								memset(pclStatP,0,sizeof(pclStatP));
								memset(pclRtabP,0,sizeof(pclRtabP));
								memset(pclRfldP,0,sizeof(pclRfldP));

								/* For retrive of ONBL from PDETAB*/
								memset(pclSqlBuf,0,sizeof(pclSqlBuf));
								memset(pclSqlData,0,sizeof(pclSqlData));
								memset(pclSelectionT,0,sizeof(pclSelectionT));

								sprintf(pclSelectionT, "WHERE RURN = '%s' AND TYPE = 'ONB' order by TIME desc", pclUrnoNewData);
                                sprintf(pclSqlBuf, "SELECT URNO,RURN,TYPE,TIME,STAT,RTAB,RFLD,FVAL FROM PDETAB %s", pclSelectionT);

                                dbg(TRACE,"<%s> 2-PDETAB pclSqlBuf<%s>",pclFunc,pclSqlBuf);

                                ilRC = RunSQL(pclSqlBuf, pclSqlData);
                                if (ilRC != DB_SUCCESS)
                                {
                                    dbg(DEBUG, "%s: RURN <%s> not found in PDETAB", pclFunc, pclUrnoNewData);
                                    /*return RC_FAIL;*/

                                    memset(pclSqlBuf,0,sizeof(pclSqlBuf));
                                    memset(pclSqlData,0,sizeof(pclSqlData));
                                    memset(pclSelectionT,0,sizeof(pclSelectionT));

                                    sprintf(pclSelectionT, "WHERE URNO = '%s'", pclUrno);
                                    sprintf(pclSqlBuf, "SELECT URNO,ONBL FROM AFTTAB %s", pclSelectionT);

                                    dbg(TRACE,"<%s> 1-AFTTAB-ONBL pclSqlBuf<%s>",pclFunc,pclSqlBuf);

                                    ilRC = RunSQL(pclSqlBuf, pclSqlData);
                                    if (ilRC != DB_SUCCESS)
                                    {
                                        dbg(DEBUG, "%s: URNO <%s> not found in AFTTAB for ONB", pclFunc, pclUrnoNewData);
                                    }
                                    else
                                    {
                                        get_fld(pclSqlData,FIELD_1,STR,20,pclUrnoP);
                                        get_fld(pclSqlData,FIELD_2,STR,512,pclFval_ONB);
                                        dbg(TRACE,"<%s> URNO<%s> ONBL<%s>",pclFunc,pclFval_ONB);
                                    }
                                }
                                else
                                {
                                    dbg(TRACE, "<%s> 2-PDETAB Record <%s>", pclFunc, pclSqlData);

                                    get_fld(pclSqlData,FIELD_1,STR,20,pclUrnoP);
                                    get_fld(pclSqlData,FIELD_2,STR,20,pclRurnP);
                                    get_fld(pclSqlData,FIELD_3,STR,20,pclTypeP);
                                    get_fld(pclSqlData,FIELD_4,STR,20,pclTimeP);
                                    get_fld(pclSqlData,FIELD_5,STR,20,pclStatP);
                                    get_fld(pclSqlData,FIELD_6,STR,20,pclRtabP);
                                    get_fld(pclSqlData,FIELD_7,STR,20,pclRfldP);
                                    get_fld(pclSqlData,FIELD_8,STR,512,pclFval_ONB);

                                    dbg(TRACE, "<%s> URNO <%s> RURN <%s> TYPE <%s> TIME <%s> STAT <%s> RTAB <%s> RFLD <%s> FVAL_ONBL <%s> ", pclFunc, pclUrnoP, pclRurnP, pclTypeP, pclTimeP, pclStatP, pclRtabP, pclRfldP, pclFval_ONB);
                                }

                                strcpy(rlTowing[ilCountTowing].pclOnbl,pclFval_ONB);
								strcpy(rlTowing[ilCountTowing].pclOfbl,pclFval_OFB);
							}
							ilNewTowingFlight = FALSE;
						}
						else
						{
							strcpy(rlTowing[ilCountTowing].pclOnbl,pclOnbl);
							strcpy(rlTowing[ilCountTowing].pclOfbl,pclOfbl);
						}

						strcpy(rlTowing[ilCountTowing].pclPsta,pclPsta);
						strcpy(rlTowing[ilCountTowing].pclPstd,pclPstd);
						strcpy(rlTowing[ilCountTowing].pclTmoa,pclTmoa);

						ilCountTowing++;
					}
					close_my_cursor(&slLocalCursor);

					for(ilCountT=0; ilCountT<ilCountTowing; ilCountT++)
					{
						dbg(DEBUG,"+++++++++++++++++++++++++++<%d>+++++++++++++++++++++++++++++++++++++",ilCountT);
						dbg(DEBUG,"<%s> rlTowing[%d].pclUrno<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclUrno);
						dbg(DEBUG,"<%s> rlTowing[%d].pclAdid<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclAdid);
						dbg(DEBUG,"<%s> rlTowing[%d].pclRkey<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclRkey);
						dbg(DEBUG,"<%s> rlTowing[%d].pclRegn<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclRegn);

						dbg(DEBUG,"<%s> rlTowing[%d].pclStoa<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclStoa);
						dbg(DEBUG,"<%s> rlTowing[%d].pclStod<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclStod);
						dbg(DEBUG,"<%s> rlTowing[%d].pclEtai<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclEtai);
						dbg(DEBUG,"<%s> rlTowing[%d].pclEtdi<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclEtdi);

						dbg(DEBUG,"<%s> rlTowing[%d].pclTifa<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclTifa);
						dbg(DEBUG,"<%s> rlTowing[%d].pclTifd<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclTifd);
						dbg(DEBUG,"<%s> rlTowing[%d].pclOnbl<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclOnbl);
						dbg(DEBUG,"<%s> rlTowing[%d].pclOfbl<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclOfbl);

						dbg(DEBUG,"<%s> rlTowing[%d].pclPsta<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclPsta);
						dbg(DEBUG,"<%s> rlTowing[%d].pclPstd<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclPstd);
						dbg(DEBUG,"<%s> rlTowing[%d].pclTmoa<%s>",pclFunc,ilCountT,rlTowing[ilCountT].pclTmoa);
					}

					memset(rlSentMsgTowing,0,sizeof(rlSentMsgTowing));
					/*Build the send msg based on the towing records*/
					for(ilCountT=0; ilCountT+1<ilCountTowing; ilCountT++)
					{
						strcpy(rlSentMsgTowing[ilCountT].pclPosi, rlTowing[ilCountT].pclPsta);
						strcpy(rlSentMsgTowing[ilCountT].pclStoa, rlTowing[ilCountT].pclStoa);
						strcpy(rlSentMsgTowing[ilCountT].pclEtai, rlTowing[ilCountT].pclEtai);
						strcpy(rlSentMsgTowing[ilCountT].pclTmoa, rlTowing[ilCountT].pclTmoa);
						strcpy(rlSentMsgTowing[ilCountT].pclOnbl, rlTowing[ilCountT].pclOnbl);

						strcpy(rlSentMsgTowing[ilCountT].pclStod, rlTowing[ilCountT+1].pclStod);
						strcpy(rlSentMsgTowing[ilCountT].pclEtdi, rlTowing[ilCountT+1].pclEtdi);
						strcpy(rlSentMsgTowing[ilCountT].pclOfbl, rlTowing[ilCountT+1].pclOfbl);

						dbg(DEBUG,"******************<%d>*************",ilCountT);
						ShowMsgStruct(rlSentMsgTowing[ilCountT]);

						memset(pclDataSentTowing,0,sizeof(pclDataSentTowing));
						BuildSentData(pclDataSentTowing,rlSentMsgTowing[ilCountT]);
						dbg(TRACE,"<%s>Number<%d> pclDataSentTowing<%s>",pclFunc,ilCountT,pclDataSentTowing);

						StoreSentData(pclDataSentTowing,pclUrnoNewData,"Normal",pclRecordURNO);

						/*
						@fya 20140304
						X-Towing flights msgs have been inserted into LIGTAB, waitting for NTI command
						For normal towing flight which has no off-block, directly sending out the update
						*/

						/*@fya 20140304*/
				    strcat(pclDataSentTowing,"\n");
						/*At this stage, the message struct is completed*/
						strcpy(pcgCurSendData,pclDataSentTowing);
				    /*strcpy(pcgSendMsgId,pclSelection);*/
				    /*strcpy(pcgSendMsgId,pclUrnoSelection);*/
				    strcpy(pcgSendMsgId,pclRecordURNO);

				    for (ilCount = 0; ilCount < igReSendMax; ilCount++)
				    {
				  		if (igSock > 0)
				      {
				      	/*
				      	@fya 20140304
				      	strcat(pclDataSent,"\n");
				      	*/
				    		ilRC = Send_data(igSock,pclDataSentTowing);
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
					        /*SendRST_Command();*/
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
				        	/*SendRST_Command();*/
				        }
				        else
				        	  ilRC = RC_FAIL;
				      }
				    }

				    /* fya 20140227 sending ack right after normal message*/
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
					        /*SendRST_Command();*/
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
				        	/*SendRST_Command();*/
				        }
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

					/*
					@fya 20140227
					strcpy(rlSentMsgTowing[ilCountT].pclPosi, rlTowing[ilCountT].pclPstd);
					strcpy(rlSentMsgTowing[ilCountT].pclStoa, rlTowing[ilCountT].pclStoa);
					strcpy(rlSentMsgTowing[ilCountT].pclEtai, rlTowing[ilCountT].pclEtai);
					strcpy(rlSentMsgTowing[ilCountT].pclTmoa, rlTowing[ilCountT].pclTmoa);
					strcpy(rlSentMsgTowing[ilCountT].pclOnbl, rlTowing[ilCountT].pclOnbl);

					memset(rlSentMsgTowing[ilCountT].pclStod,0,sizeof(rlSentMsgTowing[ilCountT].pclStod));
					memset(rlSentMsgTowing[ilCountT].pclEtdi,0,sizeof(rlSentMsgTowing[ilCountT].pclEtdi));
					memset(rlSentMsgTowing[ilCountT].pclOfbl,0,sizeof(rlSentMsgTowing[ilCountT].pclOfbl));

					ShowMsgStruct(rlSentMsgTowing[ilCountT]);

					BuildSentData(pclDataSentTowing,rlSentMsgTowing[ilCountT]);
					dbg(TRACE,"<%s>1 pclDataSentTowing<%s>",pclFunc,pclDataSentTowing);

					StoreSentData(pclDataSentTowing,pclUrnoNewData,"Towing",pclRecordURNO);
					*/
					return RC_SUCCESS;
				}
			}
			else if ( strncmp(pclAdidNewData,"D",1) == 0)
			{
				dbg(DEBUG,"<%s>ADID = D",pclFunc);
				/*3) Getting the PSTD for building the message*/
				ilItemNo = get_item_no(pclFields, "PSTD", 5) + 1;
				if( ilItemNo <= 0)
    		{
    			dbg(TRACE,"<%s> PSTD is not included in the field list, can not proceed",pclFunc);
					return RC_FAIL;
    		}
    		get_real_item(pclPstdNewData, pclNewData, ilItemNo);
    		get_real_item(pclPstdOldData, pclOldData, ilItemNo);
    		dbg(DEBUG,"<%s> New PSTD<%s>",pclFunc,pclPstdNewData);
    		dbg(DEBUG,"<%s> Old PSTD<%s>",pclFunc,pclPstdOldData);

    		/*4) Getting the STOD for building the message*/
		  	ilItemNo =  get_item_no(pclFields, "STOD", 5) + 1;
		  	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> STOD is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclStodNewData, pclNewData, ilItemNo);
				get_real_item(pclStodOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New STOD is <%s>", pclFunc, pclStodNewData);
				dbg(DEBUG,"<%s>The Old STOD is <%s>", pclFunc, pclStodOldData);

				/*5) Getting the ETDI for building the message*/
	    	ilItemNo =  get_item_no(pclFields, "ETDI", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> ETDI is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclEtdiNewData, pclNewData, ilItemNo);
				get_real_item(pclEtdiOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New ETDI is <%s>", pclFunc, pclEtdiNewData);
				dbg(DEBUG,"<%s>The Old ETDI is <%s>", pclFunc, pclEtdiOldData);

				/*6) Getting the OFBL for building the message*/
	    	ilItemNo =  get_item_no(pclFields, "OFBL", 5) + 1;
	    	if (ilItemNo <= 0)
				{
					dbg(TRACE,"<%s> OFBL is not included in the field list, can not proceed",pclFunc);
		    	return RC_FAIL;
				}
				get_real_item(pclOfblNewData, pclNewData, ilItemNo);
				get_real_item(pclOfblOldData, pclOldData, ilItemNo);
				dbg(DEBUG,"<%s>The New OFBL is <%s>", pclFunc, pclOfblNewData);
				dbg(DEBUG,"<%s>The Old OFBL is <%s>", pclFunc, pclOfblOldData);

				/*pstd, stod, etdi, ofbl*/
				BuildDepPart(&rlSentMsg, pclPstdNewData, pclStodNewData, pclEtdiNewData, pclOfblNewData);

				dbg(DEBUG,"<%s> After BuildDepPart, Show msg struct",pclFunc);
				ShowMsgStruct(rlSentMsg);

				memset(pclSqlBuf,0,sizeof(pclSqlBuf));
				memset(pclSqlData,0,sizeof(pclSqlData));
				sprintf(pclSelection, "WHERE URNO = %s", pclUrnoNewData);
    		sprintf(pclSqlBuf, "SELECT RKEY,REGN,TIFA,TIFD FROM AFTTAB %s", pclSelection);

    		ilRC = RunSQL(pclSqlBuf, pclSqlData);
    		if (ilRC != DB_SUCCESS)
    		{
        	dbg(DEBUG, "%s: URNO <%s> not found in AFTTAB", pclFunc, pclUrnoNewData);
        	return RC_FAIL;
    		}
    		get_fld(pclSqlData,FIELD_1,STR,20,pclRkey);
  			get_fld(pclSqlData,FIELD_2,STR,20,pclRegn);
  			get_fld(pclSqlData,FIELD_3,STR,20,pclTifa);
  			get_fld(pclSqlData,FIELD_4,STR,20,pclTifd);
  			dbg(TRACE, "<%s> RKEY <%s> REGN <%s> TIFA <%s> TIFD <%s>", pclFunc, pclRkey, pclRegn, pclTifa, pclTifd);

  			TrimSpace(pclRkey);
				/*
				@fya 201440303
				keep regn as it is
  			TrimSpace(pclRegn);
  			*/
  			TrimSpace(pclTifa);
  			TrimSpace(pclTifd);

  			strncpy(rlSentMsg.pclRkey,pclRkey,strlen(pclRkey));
				strncpy(rlSentMsg.pclRegn,pclRegn,strlen(pclRegn));
				strncpy(rlSentMsg.pclTifa,pclTifa,strlen(pclTifa));
				strncpy(rlSentMsg.pclTifd,pclTifd,strlen(pclTifd));

				ilRC = GetSeqFlight(&rlSentMsg,"A");
				ilFlag = 2;

			}
			else
			{
				dbg(TRACE,"<%s> Unknown ADID<%s>",pclFunc,pclAdidNewData);
			}

			dbg(DEBUG,"Comes here");


			memset(pclDataSent,0,sizeof(pclDataSent));
			if(ilFlag == 1 && ilRC == RC_FAIL)
			{
				BuildSentDataArr(pclDataSent,rlSentMsg);
			}
			else if(ilFlag == 2 && ilRC == RC_FAIL)
			{
				BuildSentDataDep(pclDataSent,rlSentMsg);
			}
			else
			{
				BuildSentData(pclDataSent,rlSentMsg);
			}
			dbg(TRACE,"<%s> pclDataSent<%s>",pclFunc,pclDataSent);

			StoreSentData(pclDataSent,pclUrnoNewData,"Normal",pclRecordURNO);

			/*@fya 20140304*/
	    strcat(pclDataSent,"\n");
			/*At this stage, the message struct is completed*/
			strcpy(pcgCurSendData,pclDataSent);
	    /*strcpy(pcgSendMsgId,pclSelection);*/
	    /*strcpy(pcgSendMsgId,pclUrnoSelection);*/
	    strcpy(pcgSendMsgId,pclRecordURNO);

	    for (ilCount = 0; ilCount < igReSendMax; ilCount++)
	    {
	  		if (igSock > 0)
	      {
	      	/*
	      	@fya 20140304
	      	strcat(pclDataSent,"\n");
	      	*/
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
		        /*SendRST_Command();*/
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
	        	/*SendRST_Command();*/
	        }
	        else
	        	  ilRC = RC_FAIL;
	      }
	    }

	    /* fya 20140227 sending ack right after normal message*/
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
		        /*SendRST_Command();*/
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
	        	/*SendRST_Command();*/
	        }
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
		else
		{
				dbg(DEBUG,"<%s> The number of element in field and data list does not match each other",pclFunc);
				return RC_FAIL;
		}
	}
	else if (strcmp(cmdblk->command,"NTI") == 0)
	{
		slLocalCursor = 0;
		slFuncCode = START;

		memset(pclWhere,0,sizeof(pclWhere));
		memset(pclSqlBuf,0,sizeof(pclSqlBuf));
		NTI_FlightBuildWhereClause(pclWhere);
		NTI_FlightBuildFullQuery(pclSqlBuf,pclWhere);

		memset(pclUrno,0,sizeof(pclUrno));
		memset(pclUaft,0,sizeof(pclUaft));
		memset(pclType,0,sizeof(pclType));
		memset(pclData,0,sizeof(pclData));
		memset(pclStat,0,sizeof(pclStat));
		memset(pclTrit,0,sizeof(pclTrit));
		memset(pclCdat,0,sizeof(pclCdat));

		ilTowingLIGTAB = 0;
		memset(pclSqlData,0,sizeof(pclSqlData));
		while(sql_if(slFuncCode, &slLocalCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS)
		{
		  slFuncCode = NEXT;

			/*URNO,UAFT,TYPE,DATA,STAT,TRIT,CDAT*/
			/*URNO,UAFT,TYPE,DATA,STAT,TRIT,CDAT*/
		 	get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
			get_fld(pclSqlData,FIELD_2,STR,20,pclUaft); TrimSpace(pclUaft);
			get_fld(pclSqlData,FIELD_3,STR,20,pclType); TrimSpace(pclType);
			get_fld(pclSqlData,FIELD_4,STR,256,pclDataT); TrimSpace(pclDataT);
			get_fld(pclSqlData,FIELD_5,STR,20,pclStat); TrimSpace(pclStat);
			get_fld(pclSqlData,FIELD_6,STR,20,pclTrit); TrimSpace(pclTrit);
			get_fld(pclSqlData,FIELD_7,STR,20,pclCdat); TrimSpace(pclCdat);

			dbg(DEBUG,"<%s> Sending Towing<%d> Records in LIGTAB",pclFunc, ilTowingLIGTAB++);
			dbg(DEBUG,"<%s> pclUrno<%s>",pclFunc,pclUrno);
			dbg(DEBUG,"<%s> pclUaft<%s>",pclFunc,pclUaft);
			dbg(DEBUG,"<%s> pclType<%s>",pclFunc,pclType);
			dbg(DEBUG,"<%s> pclDataT<%s>",pclFunc,pclDataT);
			dbg(DEBUG,"<%s> pclStat<%s>",pclFunc,pclStat);
			dbg(DEBUG,"<%s> pclTrit<%s>",pclFunc,pclTrit);
			dbg(DEBUG,"<%s> pclCdat<%s>",pclFunc,pclCdat);


			/*@fya 20140304*/
	    strcat(pclDataT,"\n");
			/*At this stage, the message struct is completed*/
			strcpy(pcgCurSendData,pclDataT);
	   	/*strcpy(pcgSendMsgId,pclSelection);*/
	   	/*strcpy(pcgSendMsgId, pclUrnoSelection);*/

	   	strcpy(pcgSendMsgId,pclUrno);
	    /*strcpy(pcgSendMsgId,pclDataT);*/


	    for (ilCount = 0; ilCount < igReSendMax; ilCount++)
	    {
	  		if (igSock > 0)
	      {
	      	/*
	      	@fya 20140304
	      	strcat(pclDataSent,"\n");
	      	*/
	    		ilRC = Send_data(igSock,pclDataT);
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
		        /*SendRST_Command();*/
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
	        	/*SendRST_Command();*/
	        }
	        else
	        	  ilRC = RC_FAIL;
	      }
	    }

	    /* fya 20140227 sending ack right after normal message*/
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
		        /*SendRST_Command();*/
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
	        	/*SendRST_Command();*/
	        }
	        else
	        	  ilRC = RC_FAIL;
	      }
	    }

			if( ilCount >= igReSendMax)
			{
				dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);
				return RC_FAIL;
			}

			/*
			Update the STAT from 'R' to 'S'
			*/
			slLocalCursorT = 0;
			slFuncCodeT = START;

			memset(pclSqlBufT,0,sizeof(pclSqlBufT));
			memset(pclSqlDataT,0,sizeof(pclSqlDataT));

			Upd_NTI_FlightBuildWhereClause(pclWhere,pclUrno);
			Upd_NTI_FlightBuildFullQuery(pclSqlBufT,pclWhere);

			ilRC = sql_if(slFuncCodeT, &slLocalCursorT, pclSqlBufT, pclSqlDataT);
			if( ilRC != DB_SUCCESS )
			{
				ilRC = RC_FAIL;
			}
			close_my_cursor(&slLocalCursorT);

		}
		dbg(TRACE,"<%s>: Total Sent Towing Record From LIGTAB is <%d>",pclFunc,ilTowingLIGTAB);

		close_my_cursor(&slLocalCursor);
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

	/*
	dbg(TRACE,"Now read parameters for interface");
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","MODE",CFG_STRING,&prgCfg.mode,CFG_ALPHA,"REAL"))
                        != RC_SUCCESS)
		return RC_FAIL;

	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","TYPE",CFG_STRING,&prgCfg.type,CFG_PRINT,"SERVER"))
                        != RC_SUCCESS)
		{ }
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HOST1",CFG_STRING,&prgCfg.host1,CFG_PRINT,""))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HOST2",CFG_STRING,&prgCfg.host2,CFG_PRINT,""))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","SERVICE_PORT",CFG_STRING,&prgCfg.service_port,CFG_PRINT,"EXCO_DGS"))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","WAIT_FOR_ACK",CFG_STRING,&prgCfg.wait_for_ack,CFG_NUM,"2"))
                        != RC_SUCCESS)
		return RC_FAIL;
	else
		igWaitForAck = atoi(prgCfg.wait_for_ack);
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","RECV_TIMEOUT",CFG_STRING,&prgCfg.recv_timeout,CFG_NUM,"60"))
                        != RC_SUCCESS)
		return RC_FAIL;
	else
		igRecvTimeout = atoi(prgCfg.recv_timeout);
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","MAX_SENDS",CFG_STRING,&prgCfg.max_sends,CFG_NUM,"2"))
                        != RC_SUCCESS)
		return RC_FAIL;
	else
		igMaxSends = atoi(prgCfg.max_sends);
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","RECV_LOG",CFG_STRING,&prgCfg.recv_log,CFG_PRINT,""))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","SEND_LOG",CFG_STRING,&prgCfg.send_log,CFG_PRINT,""))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","TRY_RECONNECT",CFG_STRING,&prgCfg.try_reconnect,CFG_NUM,"60"))
                        != RC_SUCCESS)
		return RC_FAIL;
	*/

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
  {
  	igHeartBeatIntv = atoi(pclTmpBuf);
  }
  else
  {
  	igHeartBeatIntv = 5;
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

  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","TIME_RANGE",CFG_STRING,pclTmpBuf);
  if ((ilRC == RC_SUCCESS) && (strcmp(pclTmpBuf, " ") != 0))
      igTimeRange = atoi(pclTmpBuf);
  else
      igTimeRange = 30;

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CONFIG_TIME",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igConfigTime = atoi(pclTmpBuf);
  else
      igConfigTime = 1;

  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","NEXT_ALLOC_DEP_UPPER",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igNextAllocDepUpperRange = atoi(pclTmpBuf);
  else
      igNextAllocDepUpperRange = 1;

  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","BATCH",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igBatch = atoi(pclTmpBuf);
  else
      igBatch = 0;

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CONFIG_SHIFT",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igConfigShift = atoi(pclTmpBuf);
  else
      igConfigShift = 1;

  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","CONFIG_END_SHIFT",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igConfigEndShiftTime = atoi(pclTmpBuf);
  else
      igConfigEndShiftTime = 1;

  ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","ACTUAL_USE_TIME",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
      igActualUseTime = atoi(pclTmpBuf);
  else
      igActualUseTime = 1;

	/*
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HEADER_PREFIX",CFG_STRING,&prgCfg.header_prefix,CFG_PRINT,"#"))
                        != RC_SUCCESS)
		return RC_FAIL;
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","HEADER_SIZE",CFG_STRING,&prgCfg.header_size,CFG_NUM,"6"))
                        != RC_SUCCESS)
		return RC_FAIL;
    else
    	igHeaderSize = atoi(prgCfg.header_size);
  */
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

	ilRC = iGetConfigEntry(pcgConfigFile,"MAIN","PREFIX",CFG_STRING,pcgPrefix);
  if (ilRC != RC_SUCCESS)
  {
  	memset(pcgPrefix,0,sizeof(pcgPrefix));
  	dbg(TRACE,"PREFIX is not set");
  }

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

	/*
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","MSGNO_KEYWORD",CFG_STRING,&prgCfg.msgno_keyword,CFG_PRINT,"msgno="))
                        != RC_SUCCESS)
		return RC_FAIL;

	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","FTYP_KEYWORD",CFG_STRING,&prgCfg.ftyp_keyword,CFG_PRINT,""))
                        != RC_SUCCESS)
		return RC_FAIL;
	*/

	/* Special Position name handling. Remove trailing characters and leading zero */
	/* A01R becomes A1. Safegate is not able to handle the complex names           */

	/*
	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","REMOVE_SUFFIX",CFG_STRING,&prgCfg.remove_suffix,CFG_PRINT,"NO"))
                        != RC_SUCCESS)
		return RC_FAIL;

	if (strcmp(prgCfg.remove_suffix,"YES") == 0)
	{
		bgRemoveSuffix = TRUE;
	}

	// This is the list of stand positions that should not have the suffix removed

	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","KEEP_SUFFIX",CFG_STRING,&prgCfg.keep_suffix,CFG_PRINT,","))
                        != RC_SUCCESS)
		return RC_FAIL;



	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","REMOVE_ZERO",CFG_STRING,&prgCfg.remove_zero,CFG_PRINT,"NO"))
                        != RC_SUCCESS)
		return RC_FAIL;

	if (strcmp(prgCfg.remove_zero,"YES") == 0)
	{
		bgRemoveZero = TRUE;
	}


	if ((ilRC=GetCfgEntry(pcgConfigFile,"MAIN","STAND_TAG",CFG_STRING,&prgCfg.stand_tag,CFG_PRINT,"stand>"))
                        != RC_SUCCESS)
		return RC_FAIL;
	igStandTagLen = strlen(prgCfg.stand_tag);


	// Load the KeepAlive and Ack formats

	if(igKeepAlive)
	{
  		if ((fplFp = (FILE *)fopen(clKeepAliveFormatF,"r")) == (FILE *)NULL)
		{
			dbg(TRACE,"KeepAlive Format File <%s> does not exist",clKeepAliveFormatF);
			return RC_FAIL;
		}
		else
		{
			ilLen = fread(pcgKeepAliveFormat,1,1023,fplFp);
			pcgKeepAliveFormat[ilLen] = '\0';
			fclose(fplFp);
		}
	}
  */
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

			dbg(DEBUG,"pcgSendAckFormat<%s>",pcgSendAckFormat);

			fclose(fplFp);
		}
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
			ilLen = fread(pcgKeepAliveFormat,1,1023,fplFp);
			pcgKeepAliveFormat[ilLen] = '\0';

			dbg(DEBUG,"pcgKeepAliveFormat<%s>",pcgKeepAliveFormat);
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
	struct timeval rlTimeout;

	static char	pclRecvBuffer[BUFF] = "\0";
  char pclCurrentTime[64] = "\0";
	char pclTmpUrno[16] = "\0";
  char *pclMsgIdEnd = NULL;
  char *pclMsgIdBgn = NULL;
  char pclTmpBuf[10] = "\0";
  char *pclTmpStart = NULL;
	char pclSqlBuf[2048] = "\0",pclSqlData[2048] = "\0",pclWhere[2048] = "\0";
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

    	    if( ilNo > 0 )
          {
      	  	dbg(TRACE,"<%s> Len<%d>Complete message<%s>",pclFunc,ilNo,pclRecvBuffer);

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
      	    }

      	   	if (strstr(pclRecvBuffer,"ACK_") != 0 && strstr(pclRecvBuffer,"ETX_") != 0 && strstr(pclRecvBuffer,"sps_") != 0)
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
	            strncpy(pclTmpUrno,pclTmpStart+36,14);

	            TrimZero(pclTmpUrno);

	        		/*dbg(DEBUG,"<%s> Received the ack message<%s> The ack message id is <%s>",pclFunc,pclRecvBuffer,pclTmpUrno);*/
							dbg(DEBUG,"<%s> Received the ack message<%s> Msgid<%s>",pclFunc,pclRecvBuffer,pclTmpUrno);

	            if (atoi(pclTmpUrno) == 0)
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

				    		if (strcmp(pclTmpUrno, pcgSendMsgId) == 0)
	              {
	              	igSckWaitACK = FALSE;
	              	igSckTryACKCnt = 0;

	              	Upd_ACK_FlightBuildWhereClause(pclWhere,pclTmpUrno);
	              	Upd_ACK_FlightBuildFullQuery(pclSqlBuf,pclWhere);

	              	ilRC = RunSQL(pclSqlBuf,pclSqlData);
									if (ilRC != DB_SUCCESS)
									{
								  	dbg(DEBUG, "<%s> Updating LIGTAB for ack fails", pclFunc);
								  	ilRC = RC_FAIL;
									}
									else
									{
										dbg(DEBUG, "<%s> Updating LIGTAB for ack succeeds", pclFunc);
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
	              	dbg(TRACE,"<%s> Received ack message id <%s> does not equal sent one<%s>", pclFunc, pclTmpUrno, pcgSendMsgId);
	              	ilRC = RC_FAIL;
	              }
	            }
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
	 	      dbg(DEBUG,"%s %d send data",pclFunc,__LINE__);
	 	      /*
	 	      @fya 20140304
	 	      strcat(pcgCurSendData,"\n");
	 	      */
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
			/*SendRST_Command();*/
			GetServerTimeStamp( "UTC", 1, 0, pcgSckWaitACKExpTime);
			AddSecondsToCEDATime(pcgSckWaitACKExpTime, igSckACKCWait, 1);
		}
  }

  GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime);
  if (strcmp(pclCurrentTime, pcgRcvHeartBeatExpTime) >= 0)
  {
     dbg(DEBUG,"Current Time<%s>, Heartbeat Exp Rec Time<%s>", pclCurrentTime, pcgRcvHeartBeatExpTime);
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

	/*Frank 20130107

	if( ilAttempts >= ilMaxAttempt )
  {
  	dbg( TRACE,"<%s>Still cannot connect! EXIT!!",pclFunc );
  	return RC_FAIL;
  	//exit( -5 );
  }
  else
  {
  */

  /*
	if(ilRc == RC_SUCCESS)
	{
  	dbg( TRACE,"<%s>Connection is established successfully ilTcpfd<%d>*ilAcceptSocket<%d>", pclFunc,ilTcpfd,*ilAcceptSocket );
  	dbg(TRACE,"%s Sending Batch file to hwepde",pclFunc);

    ilProcessId = tool_get_q_id ("hwepde");
    if ((ilProcessId == RC_FAIL) || (ilProcessId == RC_NOT_FOUND))
	  {
	    dbg(TRACE,"%s: ====== ERROR ====== hwepde Mod Id not found",pclFunc);
	  }
    else
    {
    	if(ilProcessId != 0)
			{
    		SendCedaEvent(ilProcessId,0,mod_name,mod_name," "," ","BAT","","",
              "","","",3,NETOUT_NO_ACK);

        dbg(TRACE,"%s SendCedaEvent1 has executed,send to<%d>",pclFunc,ilProcessId);
			}
			else
			{
				dbg(TRACE,"%s ilProcessId<%d>==0",pclFunc,ilProcessId);
			}
  	}
  	return RC_SUCCESS;
  }
  else
  {
  	return RC_FAIL;
  }

   Frank 20130107
  }*/
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
      SendRST_Command();
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

	return ilRc;
}

/*
static int Sockt_Reconnect(void)
{
	int ilRc = RC_SUCCESS;
	int ilQueRc = RC_SUCCESS;
    int ilCount = 0;
    igOldCnnt = igConnected;
    CloseTCP();
    igSock = 0;
	dbg(TRACE,"Keep trying to connect <%s><%s>",pcgIP,pcgPort);
    for (ilCount = 0; ilCount < igReconMax; ilCount++)
	{
	 	 ilRc = OpenServerSocket();
		 dbg(DEBUG,"ilRc<%d>",ilRc);
		 ilCount++;
	 	 if(ilRc == RC_SUCCESS)
             break;
         else
             CloseTCP();
         sleep(1);
	}

	if(ilRc == RC_SUCCESS)
    {
        dbg(TRACE,"ilCount[%d]igSock<%d>, connection success",ilCount,igSock);
        igConnected = TRUE;
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
    AddSecondsToCEDATime(pcgReconExpTime, igReconIntv, 1);
	return ilRc;
}
*/

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
	int ilSoltNumber = 0;
	char * pclFunc = "SendBatchFlights";
	int  ilRC = RC_SUCCESS;
	short slLocalCursor = 0, slFuncCode = 0;
	char pclUrno[16] = "\0";
	char pclRecordURNO[16] = "\0";
	char pclParkstand[16] = "\0";
	char pclDataSent[1024] = "\0";
	char pclDataSentP[1024] = "\0";
	char pclSqlBuf[2048] = "\0",pclDataArea[2048] = "\0",pclWhere[2048] = "\0";
	static char pclAllValidSlots[4096] = "\0";

	SENT_MSG rlSentMsg;
	/* fya 20140221
	Getting all valid parking stand from basic data
	*/
	GetAllValidSlots(pclAllValidSlots,&ilSoltNumber);

	/*for each parking stand, searching the uasge*/

	for (ilCountP = 1; ilCountP <= ilSoltNumber; ilCountP++)
	/*for (ilCount = 1; ilCount <= 2; ilCount++)*/
	{
		get_real_item(pclParkstand,pclAllValidSlots,ilCountP);
		dbg(DEBUG,"<%s> ilCountP<%d> pclParkstand<%s>",pclFunc,ilCountP,pclParkstand);

		PutDefaultValue(&rlSentMsg);

		/* Get the latest actual use for parking stand before NOW_TIME*/
		ilRC = function_PST_USAGE(&rlSentMsg, pclParkstand, igConfigTime, pclUrno);
		/*dbg(DEBUG,"<%s> Return From function_PST_USAGE",pclFunc);*/

		if(strlen(rlSentMsg.pclPosi) == 0)
		{
			dbg(DEBUG,"<%s> rlSentMsg.pclPosi<%s> is null", pclFunc, rlSentMsg.pclPosi);
			continue;
		}

		/* If the return value is NO_END, then this means the SendMsg is null */
		memset(pclDataSent,0,sizeof(pclDataSent));
		switch(ilRC)
		{
			case NO_END:
				dbg(DEBUG,"<%s> The return value is NO_END",pclFunc);
				BuildEmptyData(pclDataSent,rlSentMsg.pclPosi);
				break;
			case START_FOUND:
				dbg(DEBUG,"<%s> The return value is START_FOUND",pclFunc);
				BuildSentData(pclDataSent,rlSentMsg);
				break;
			default:
				BuildSentData(pclDataSent,rlSentMsg);
				break;
		}

		/*
		@fya
		Adding the sending code
		Send Batch Data
		*/
		dbg(TRACE,"<%s> pclDataSent<%s>",pclFunc,pclDataSent);

		StoreSentData(pclDataSent,pclUrno,"Batch",pclRecordURNO);

		 /*@fya 20140304*/
    strcat(pclDataSent,"\n");
		/*At this stage, the message struct is completed*/
		strcpy(pcgCurSendData,pclDataSent);
    /*strcpy(pcgSendMsgId,pclSelection);*/
    /*strcpy(pcgSendMsgId,pclUrnoSelection);*/
    strcpy(pcgSendMsgId,pclRecordURNO);

    for (ilCount = 0; ilCount < igReSendMax; ilCount++)
    {
  		if (igSock > 0)
      {
      	/*
      	@fya 20140304
      	strcat(pclDataSent,"\n");
      	*/
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
	        /*SendRST_Command();*/
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
        	/*SendRST_Command();*/
        }
        else
        	  ilRC = RC_FAIL;
      }
    }

    /* fya 20140227 sending ack right after normal message*/
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
	        /*SendRST_Command();*/
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
        	/*SendRST_Command();*/
        }
        else
        	  ilRC = RC_FAIL;
      }
    }

		if( ilCount >= igReSendMax)
		{
			dbg(TRACE,"<%s>Send_data <%d>Times failed, drop msg",pclFunc, ilCount);
			return RC_FAIL;
		}

		memset(pclUrno,0,sizeof(pclUrno));
	}
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

	memset(rpSentMsg->pclPosi,0,sizeof(rpSentMsg->pclPosi));
	memset(rpSentMsg->pclStoa,0,sizeof(rpSentMsg->pclStoa));
	memset(rpSentMsg->pclEtai,0,sizeof(rpSentMsg->pclEtai));
	memset(rpSentMsg->pclTmoa,0,sizeof(rpSentMsg->pclTmoa));
	memset(rpSentMsg->pclOnbl,0,sizeof(rpSentMsg->pclOnbl));
	memset(rpSentMsg->pclStod,0,sizeof(rpSentMsg->pclStod));
	memset(rpSentMsg->pclEtdi,0,sizeof(rpSentMsg->pclEtdi));
	memset(rpSentMsg->pclOfbl,0,sizeof(rpSentMsg->pclOfbl));
	memset(rpSentMsg->pclRegn,0,sizeof(rpSentMsg->pclRegn));
	memset(rpSentMsg->pclRkey,0,sizeof(rpSentMsg->pclRkey));

	strncpy(rpSentMsg->pclStoa,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclEtai,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclTmoa,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOnbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclStod,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclEtdi,pclDefauleValueTime,strlen(pclDefauleValueTime));
	strncpy(rpSentMsg->pclOfbl,pclDefauleValueTime,strlen(pclDefauleValueTime));
}

void BuildArrPart(SENT_MSG *rpSentMsg, char *pcpPstaNewData, char *pcpStoaNewData, char *pcpEtaiNewData, char* pcpTmoaNewData, char *pcpOnblNewData)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	char *pclFunc = "BuildArrPart";

	TrimSpace(pcpPstaNewData);
	TrimSpace(pcpStoaNewData);
	TrimSpace(pcpEtaiNewData);
	TrimSpace(pcpTmoaNewData);
	TrimSpace(pcpOnblNewData);

	if (strlen(pcpPstaNewData) != 0 )
	{
		if (strlen(pcpPstaNewData) < POS_LEN)
		{
			ilNumOfZero = POS_LEN - strlen(pcpPstaNewData);
			for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
			{
				strcat(rpSentMsg->pclPosi,"0");
			}
		}
		strcat(rpSentMsg->pclPosi,pcpPstaNewData);
	}

	if (strlen(pcpStoaNewData) != 0 )
	{
		strncpy(rpSentMsg->pclStoa,pcpStoaNewData,strlen(pcpStoaNewData));
	}

	if (strlen(pcpEtaiNewData) != 0 )
	{
		strncpy(rpSentMsg->pclEtai,pcpEtaiNewData,strlen(pcpEtaiNewData));
	}

	if (strlen(pcpTmoaNewData) != 0 )
	{
		strncpy(rpSentMsg->pclTmoa,pcpTmoaNewData,strlen(pcpTmoaNewData));
	}

	if (strlen(pcpOnblNewData) != 0 )
	{
		strncpy(rpSentMsg->pclOnbl,pcpOnblNewData,strlen(pcpOnblNewData));
	}
}

void BuildDepPart(SENT_MSG *rpSentMsg, char *pcpPstdNewData, char *pcpStodaNewData, char *pcpEtdiNewData, char *pcpOfblNewData)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	char *pclFunc = "BuildDepPart";

	TrimSpace(pcpPstdNewData);
	TrimSpace(pcpStodaNewData);
	TrimSpace(pcpEtdiNewData);
	TrimSpace(pcpOfblNewData);

	if (strlen(pcpPstdNewData) != 0 )
	{
		if (strlen(pcpPstdNewData) < POS_LEN)
		{
			ilNumOfZero = POS_LEN - strlen(pcpPstdNewData);
			for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
			{
				strcat(rpSentMsg->pclPosi,"0");
			}
		}
		strcat(rpSentMsg->pclPosi,pcpPstdNewData);
	}

	if (strlen(pcpStodaNewData) != 0 )
	{
		strncpy(rpSentMsg->pclStod,pcpStodaNewData,strlen(pcpStodaNewData));
	}

	if (strlen(pcpEtdiNewData) != 0 )
	{
		strncpy(rpSentMsg->pclEtdi,pcpEtdiNewData,strlen(pcpEtdiNewData));
	}

	if (strlen(pcpOfblNewData) != 0 )
	{
		strncpy(rpSentMsg->pclOfbl,pcpOfblNewData,strlen(pcpOfblNewData));
	}
}

static void ShowMsgStruct(SENT_MSG rpSentMsg)
{
	char *pclFunc = "ShowMsgStruct";

	dbg(DEBUG,"<%s> POS<%s>",pclFunc,rpSentMsg.pclPosi);
  dbg(DEBUG,"<%s> STA<%s>",pclFunc,rpSentMsg.pclStoa);
	dbg(DEBUG,"<%s> ETA<%s>",pclFunc,rpSentMsg.pclEtai);
  dbg(DEBUG,"<%s> TMO<%s>",pclFunc,rpSentMsg.pclTmoa);
  dbg(DEBUG,"<%s> ONB<%s>",pclFunc,rpSentMsg.pclOnbl);
  dbg(DEBUG,"<%s> STD<%s>",pclFunc,rpSentMsg.pclStod);
  dbg(DEBUG,"<%s> ETD<%s>",pclFunc,rpSentMsg.pclEtdi);
  dbg(DEBUG,"<%s> OFB<%s>",pclFunc,rpSentMsg.pclOfbl);
}

static int GetSeqFlight(SENT_MSG *rpSentMsg,char *pcpADFlag)
{
	char *pclFunc = "GetSeqFlight";
	int  ilRC = RC_SUCCESS;
	short slLocalCursor = 0, slFuncCode = 0;

	char pclSqlBuf[2048] = "\0",pclSqlData[2048] = "\0",pclWhere[2048] = "\0";

	char pclStoa[16] = "\0";
	char pclStod[16] = "\0";
	char pclOfbl[16] = "\0";
	char pclOnbl[16] = "\0";
	char pclEtai[16] = "\0";
	char pclEtdi[16] = "\0";
	char pclTmoa[16] = "\0";

  if (strlen(pcpADFlag) == 0)
  {
  	dbg(TRACE,"<%s> pcpADFlag is null<%s>",pclFunc,pcpADFlag);
  	return RC_FAIL;
  }
  dbg(TRACE,"<%s> pcpADFlag is <%s>",pclFunc,pcpADFlag);

  /* Build Where Clause */
  UpdBuildWhereClause(rpSentMsg,pclWhere,pcpADFlag);

  /* Build Full Sql query */
  UpdBuildFullQuery(pclSqlBuf,pclWhere,pcpADFlag);

  ilRC = RunSQL(pclSqlBuf, pclSqlData);
	if (ilRC != DB_SUCCESS)
	{
  	dbg(DEBUG, "<%s> Not found in AFTTAB", pclFunc);
  	return RC_FAIL;
	}
	else
	{
		dbg(DEBUG, "<%s> Found in AFTTAB", pclFunc);
	}

	if (strncmp(pcpADFlag,"D",1) == 0)
	{
		get_fld(pclSqlData,FIELD_1,STR,20,pclStod);
		get_fld(pclSqlData,FIELD_2,STR,20,pclEtdi);
		get_fld(pclSqlData,FIELD_3,STR,20,pclOfbl);

		TrimSpace(pclStod);
		if(strlen(pclStod) != 0)
		{
			strncpy(rpSentMsg->pclStod,pclStod,strlen(pclStod));
		}

		TrimSpace(pclEtdi);
		if(strlen(pclEtdi) != 0)
		{
			strncpy(rpSentMsg->pclEtdi,pclEtdi,strlen(pclEtdi));
		}

		TrimSpace(pclOfbl);
		if(strlen(pclOfbl) != 0)
		{
			strncpy(rpSentMsg->pclOfbl,pclOfbl,strlen(pclOfbl));
		}

		dbg(DEBUG,"<%s> pclStod<%s>",pclFunc,pclStod);
		dbg(DEBUG,"<%s> pclEtdi<%s>",pclFunc,pclEtdi);
		dbg(DEBUG,"<%s> pclOfbl<%s>",pclFunc,pclOfbl);
	}
	else if (strncmp(pcpADFlag,"A",1) == 0)
	{
		get_fld(pclSqlData,FIELD_1,STR,20,pclStoa);
		get_fld(pclSqlData,FIELD_2,STR,20,pclEtai);
		get_fld(pclSqlData,FIELD_3,STR,20,pclTmoa);
		get_fld(pclSqlData,FIELD_4,STR,20,pclOnbl);

		TrimSpace(pclStoa);
		if(strlen(pclStoa) != 0)
		{
			strncpy(rpSentMsg->pclStoa,pclStoa,strlen(pclStoa));
		}

		TrimSpace(pclEtai);
		if(strlen(pclEtai) != 0)
		{
			strncpy(rpSentMsg->pclEtai,pclEtai,strlen(pclEtai));
		}

		TrimSpace(pclTmoa);
		if(strlen(pclTmoa) != 0)
		{
			strncpy(rpSentMsg->pclTmoa,pclTmoa,strlen(pclTmoa));
		}

		TrimSpace(pclOnbl);
		if(strlen(pclOnbl) != 0)
		{
			strncpy(rpSentMsg->pclOnbl,pclOnbl,strlen(pclOnbl));
		}

		dbg(DEBUG,"<%s> pclStoa<%s>",pclFunc,pclStoa);
		dbg(DEBUG,"<%s> pclEtai<%s>",pclFunc,pclEtai);
		dbg(DEBUG,"<%s> pclTmoa<%s>",pclFunc,pclTmoa);
		dbg(DEBUG,"<%s> pclOnbl<%s>",pclFunc,pclOnbl);
	}

	return ilRC;
}

static int UpdBuildWhereClause(SENT_MSG *rpSentMsg, char *pcpWhere, char *pcpADFlag)
{
	char *pclFunc = "UpdBuildWhereClause";

	char pclWhere[2048] = "\0";

	if (strlen(pcpADFlag) == 0)
  {
  	dbg(TRACE,"<%s> pcpADFlag is null<%s>",pclFunc,pcpADFlag);
  	return RC_FAIL;
  }
  dbg(TRACE,"<%s> pcpADFlag is <%s>",pclFunc,pcpADFlag);

	if (strncmp(pcpADFlag,"D",1) == 0)
  {
  	/*Seeking for related sequential departure flight*/
  	sprintf(pclWhere,"(REGN = '%s' and RKEY = '%s') AND (TIFD > %s) AND FTYP NOT IN ('X','N') ORDER BY TIFD desc", rpSentMsg->pclRegn, rpSentMsg->pclRkey,rpSentMsg->pclTifa);
  }
  else if (strncmp(pcpADFlag,"A",1) == 0)
  {
  	/*Seeking for related sequential arrival flight*/
  	sprintf(pclWhere,"(REGN = '%s' and RKEY = '%s') AND (TIFA < %s) AND FTYP NOT IN ('X','N') ORDER BY TIFA", rpSentMsg->pclRegn, rpSentMsg->pclRkey,rpSentMsg->pclTifd);
  }

  strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"Where Clause<%s>",pcpWhere);
}

/* it is defined in urno_fn.h
static int RunSQL(char *pcpSelection, char *pcpData )
{
    int ilRc = DB_SUCCESS;
    short slSqlFunc = 0;
    short slSqlCursor = 0;
    char pclErrBuff[128];
    char *pclFunc = "RunSQL";

    ilRc = sql_if ( START, &slSqlCursor, pcpSelection, pcpData );
    close_my_cursor ( &slSqlCursor );

    if( ilRc == DB_ERROR )
    {
        get_ora_err( ilRc, &pclErrBuff[0] );
        dbg( TRACE, "<%s> Error getting Oracle data error <%s>", pclFunc, &pclErrBuff[0] );
    }
    return ilRc;
}
*/

static int UpdBuildFullQuery(char *pcpSqlBuf, char *pcpWhere, char *pcpADFlag)
{
	char *pclFunc = "UpdBuildFullQuery";

	char pclSqlBuf[2048] = "\0";

	if (strlen(pcpADFlag) == 0)
  {
  	dbg(TRACE,"<%s> pcpADFlag is null<%s>",pclFunc,pcpADFlag);
  	return RC_FAIL;
  }
  dbg(DEBUG,"<%s> pcpADFlag is <%s>",pclFunc,pcpADFlag);

	if (strncmp(pcpADFlag,"D",1) == 0)
  {
  	sprintf(pclSqlBuf, "SELECT STOD,ETDI,OFBL FROM AFTTAB WHERE %s", pcpWhere);
  }
  else if (strncmp(pcpADFlag,"A",1) == 0)
  {
  	sprintf(pclSqlBuf, "SELECT STOA,ETAI,TMOA,ONBL FROM AFTTAB WHERE %s", pcpWhere);
  }

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void BuildEmptyData(char *pcpDataSent,char *pcpPosi)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	char *pclFunc = "BuildEmptyData";
	char pclDefauleValueTime[16] = "..............";

	memset(pcpDataSent,0,sizeof(pcpDataSent));

	strcat(pcpDataSent,pcgPrefix);

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

static void BuildSentDataDep(char *pcpDataSent,SENT_MSG rpSentMsg)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	int ilRc = DB_SUCCESS;
	char *pclFunc = "BuildSentDataDep";
	char pclDefauleValueTime[32] = "..............";

	char pclDataSentWithSeparator[1024] = "\0";

	strcat(pcpDataSent,pcgPrefix);

	if (strlen(rpSentMsg.pclPosi) < POS_LEN)
  {
	  ilNumOfZero = POS_LEN - strlen(rpSentMsg.pclPosi);
	  for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
	  {
	  	strcat(pcpDataSent,"0");
	  }
  }
  strcat(pcpDataSent,rpSentMsg.pclPosi);

  strcat(pcpDataSent,pclDefauleValueTime);
  strcat(pcpDataSent,pclDefauleValueTime);
  strcat(pcpDataSent,pclDefauleValueTime);
  strcat(pcpDataSent,pclDefauleValueTime);

  strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
  strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
  strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
  strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);

  if (strlen(rpSentMsg.pclStod) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclStod);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclStod);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

	if (strlen(rpSentMsg.pclEtdi) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclEtdi);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclEtdi);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

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
	/* Build suffix*/
	strcat(pcpDataSent,pcgSuffix);

	dbg(TRACE,"<%s> Data Sent<%s> strlen(pcpDataSent)<%d>",pclFunc,pcpDataSent,strlen(pcpDataSent));
	dbg(TRACE,"<%s> DataForCheck<%s> strlen(pclDataSentWithSeparator)<%d>",pclFunc,pclDataSentWithSeparator,strlen(pcpDataSent));
}

static void BuildSentDataArr(char *pcpDataSent,SENT_MSG rpSentMsg)
{
	int ilCount = 0;
	int ilNumOfZero = 0;
	int ilRc = DB_SUCCESS;
	char *pclFunc = "BuildSentDataArr";
	char pclDefauleValueTime[32] = "..............";

	char pclDataSentWithSeparator[1024] = "\0";

	strcat(pcpDataSent,pcgPrefix);

	if (strlen(rpSentMsg.pclPosi) < POS_LEN)
  {
	  ilNumOfZero = POS_LEN - strlen(rpSentMsg.pclPosi);
	  for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
	  {
	          strcat(pcpDataSent,"0");
	  }
  }
	strcat(pcpDataSent,rpSentMsg.pclPosi);

	if (strlen(rpSentMsg.pclStoa) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclStoa);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

	if (strlen(rpSentMsg.pclEtai) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclEtai);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

	if (strlen(rpSentMsg.pclTmoa) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclTmoa);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

	if (strlen(rpSentMsg.pclOnbl) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclOnbl);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}

	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);
	strcat(pcpDataSent,pclDefauleValueTime);

	strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);

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
	strcat(pcpDataSent,pcgPrefix);
	strcat(pclDataSentWithSeparator,pcgPrefix);

	if (strlen(rpSentMsg.pclPosi) < POS_LEN)
  {
	  ilNumOfZero = POS_LEN - strlen(rpSentMsg.pclPosi);
	  for(ilCount = 0; ilCount < ilNumOfZero; ilCount++)
	  {
	  	strcat(pcpDataSent,"0");
	  	strcat(pclDataSentWithSeparator,"0");
	  }
  }
	strcat(pcpDataSent,rpSentMsg.pclPosi);
	strcat(pclDataSentWithSeparator,rpSentMsg.pclPosi);
	strcat(pclDataSentWithSeparator,pcgSeparator);
	/*
	dbg(DEBUG,"<%s>1 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>1 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/


	if (strlen(rpSentMsg.pclStoa) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclStoa);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclStoa);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>2 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>2 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclEtai) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclEtai);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclEtai);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>3 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>3 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclTmoa) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclTmoa);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclTmoa);
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

	if (strlen(rpSentMsg.pclStod) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclStod);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclStod);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>6 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>6 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

	if (strlen(rpSentMsg.pclEtdi) > 0)
	{
		strcat(pcpDataSent,rpSentMsg.pclEtdi);
		strcat(pclDataSentWithSeparator,rpSentMsg.pclEtdi);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	else
	{
		strcat(pcpDataSent,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pclDefauleValueTime);
		strcat(pclDataSentWithSeparator,pcgSeparator);
	}
	/*
	dbg(DEBUG,"<%s>7 pcpDataSent<%s>",pclFunc,pcpDataSent);
	dbg(DEBUG,"<%s>7 pclDataSentWithSeparator<%s>",pclFunc,pclDataSentWithSeparator);*/

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

static int StoreSentData(char *pcpDataSent,char *pcpUaft,char *pcpFlagTowing, char *pcpRecordURNO)
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
	char pclTriggerTime[64] = "\0";
	char pclSqlBuf[2048] = "\0";
	char pclDataBuf[2048] = "\0";
	char pclFieldList[2048] = "\0";
	char pclDataList[2048] = "\0";

	memset(pclCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	strcpy(pclTriggerTime,pclCurrentTime);
	AddSecondsToCEDATime(pclTriggerTime, igTimeRange*60, 1);

	/*Get New Urno*/
	ilNextUrno = NewUrnos( "LIGTAB", 1 );
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

  dbg(DEBUG,"the URNO for LIGTAB is <%s>",pclUrno);

  /*Insert the normal flight which is not towing ones into LIGTAB
  And STAT is null
  */
  if( strcmp(pcpFlagTowing,"Towing") != 0 )
  {
  	strcpy(pclFieldList,"URNO,UAFT,TYPE,DATA,STAT,TRIT,CDAT");
		sprintf(pclDataList,"%s,'%s','%s','%s','%s','%s','%s'",pclUrno,pcpUaft,"NormalFlight",pcpDataSent,"R","",pclCurrentTime);
	}
	else
	{
		strcpy(pclFieldList,"URNO,UAFT,TYPE,DATA,STAT,TRIT,CDAT");
		sprintf(pclDataList,"%s,'%s','%s','%s','%s','%s','%s'",pclUrno,pcpUaft,"TowingFlight",pcpDataSent,"R",pclTriggerTime,pclCurrentTime);
  }

  sprintf(pclSqlBuf,"INSERT INTO LIGTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
  slCursor = 0;
	slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  if (ilRCdb == DB_SUCCESS)
     commit_work();
  else
  {
     dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
     ilRC = RC_FAIL;
  }
  close_my_cursor(&slCursor);

  memset(pcpRecordURNO,0,sizeof(pcpRecordURNO));
  strcpy(pcpRecordURNO,pclUrno);

  return ilRC;
}

static void SearchTowingFlightBuildWhereClause(char *pcpRkey, char *pcpRegn, char *pcpWhere)
{
	char *pclFunc = "SearchTowingFlightBuildWhereClause";
	char pclWhere[2048] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));

	if (strlen(pcpRegn) > 0)
	{
 		sprintf(pclWhere,"RKEY = '%s' and REGN = '%s' order by TIFA", pcpRkey,pcpRegn);
	}
	else
	{
		sprintf(pclWhere,"RKEY = '%s' order by TIFA", pcpRkey,pcpRegn);
	}
	strcpy(pcpWhere,pclWhere);
	dbg(DEBUG,"Where Clause<%s>",pcpWhere);
}

static void SearchTowingFlightBuildFullQuery(char *pcpSqlBuf, char *pcpWhere)
{
	char *pclFunc = "SearchTowingFlightBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	/*URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA*/
	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA FROM AFTTAB WHERE %s", pcpWhere);

	strcpy(pcpSqlBuf,pclSqlBuf);
	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void NTI_FlightBuildWhereClause(char *pcpWhere)
{
	char *pclFunc = "NTI_FlightBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));

  GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

 	sprintf(pclWhere,"TYPE = 'TowingFlight' and STAT = 'R' and TRIT <= '%s' order by URNO", pclCurrentTime);

	strcpy(pcpWhere,pclWhere);
	dbg(DEBUG,"Where Clause<%s>",pcpWhere);
}

static void NTI_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "NTI_FlightBuildFullQuery";

	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,UAFT,TYPE,DATA,STAT,TRIT,CDAT FROM LIGTAB WHERE %s", pcpWhere);

	strcpy(pcpSqlBuf,pclSqlBuf);
	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void Upd_NTI_FlightBuildWhereClause(char *pcpWhere,char *pcpUrno)
{
	char *pclFunc = "Upd_NTI_FlightBuildWhereClause";
	char pclWhere[2048] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));


 	sprintf(pclWhere,"TYPE = 'TowingFlight' and STAT = 'R' and URNO = '%s'",pcpUrno);

	strcpy(pcpWhere,pclWhere);
	dbg(DEBUG,"Where Clause<%s>",pcpWhere);
}

static void Upd_NTI_FlightBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "Upd_NTI_FlightBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "UPDATE LIGTAB SET STAT = 'S' WHERE %s", pcpWhere);

	strcpy(pcpSqlBuf,pclSqlBuf);
	dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static int function_PST_USAGE(SENT_MSG *rpSentMsg, char *pcpParkstand, int ipConfigTime, char *pcpUrno)
{
	char * pclFunc = "function_PST_USAGE";
	int  ilRC = RC_SUCCESS;

	static int ilFlagUseEST_SCHStartUse = FALSE;
	static int ilFlagFindNetSeqFlight = FALSE;
	static int ilFlagStart_use_found = FALSE;
	static int ilFlagFind_Next_Seq_flight = FALSE;
	static int ilFlagEnd_Use_found = FALSE;


	char pclSCH_UseStartTime[TIMEFORMAT] = "\0";

	char pclUrno[16] = "\0";
	char pclAdid[16] = "\0";
	char pclRkey[16] = "\0";
	char pclRegn[16] = "\0";
	char pclStoa[16] = "\0";
	char pclStod[16] = "\0";
	char pclTifa[16] = "\0";
	char pclTifd[16] = "\0";
	char pclPsta[16] = "\0";
	char pclPstd[16] = "\0";
	char pclEtai[16] = "\0";
	char pclEtdi[16] = "\0";
	char pclTmoa[16] = "\0";
	char pclOnbl[16] = "\0";
	char pclOfbl[16] = "\0";
	char pclPosa[16] = "\0";
	char pclSqlBuf[2048] = "\0",pclSqlData[2048] = "\0",pclWhere[2048] = "\0";

	TOWING rlIntermediateLatestUse;
	/*INTERMEDIATE rlIntermediateEndUse;*/
	/*
	1 Build the where clause
	2 Build the full query
	3 Run query
	4 Check the return value
	*/
	FindActualUseBuildWhereClause(pcpParkstand,pclWhere,ipConfigTime);
	FindActualUseBuildFullQuery(pclSqlBuf,pclWhere);

	ilRC = RunSQL(pclSqlBuf,pclSqlData);
	if (ilRC != DB_SUCCESS)
	{
  	dbg(TRACE, "<%s> Not found in AFTTAB, line<%d>", pclFunc,__LINE__);
  	/*return RC_FAIL;*/
	}

	switch(ilRC)
	{
		case NOTFOUND:
			/*The parking stand currently is not occupied by any flight, so let us choose the next scheduled flight*/
			dbg(TRACE, "<%s> Then search for scheduled flight, ilFlagUseEST_SCHStartUse = TRUE", pclFunc);
			ilFlagUseEST_SCHStartUse = TRUE;

			memset(pcgCurrentTime,0x00,TIMEFORMAT);
			GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

			strcpy(pclSCH_UseStartTime,pcgCurrentTime);
			AddSecondsToCEDATime(pclSCH_UseStartTime, -igConfigShift * 60 * 60 * 24, 1);
			dbg(TRACE, "<%s> pclSCH_UseStartTime<%s>", pclFunc,pclSCH_UseStartTime);
			break;
		default:
			/*FOUND*/
			/* The parking stand currently is occupied by one flight, so let us find out its actual end use time
			   To find the actual end use for the parking stand */

			/*
			URNO,ADID,RKEY,REGN,
			STOA,STOD,TIFA,TIFD,
			PSTA,PSTD,ETAI,ETDI,
			ONBL,OFBL,TMOA
			*/
			dbg(TRACE, "<%s> Then search for actual end use flight", pclFunc);

			get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
			get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
			get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
			get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); /*@fya 20140303 TrimSpace(pclRegn);*/
			get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
			get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
			get_fld(pclSqlData,FIELD_7,STR,20,pclTifa); TrimSpace(pclTifa);
			get_fld(pclSqlData,FIELD_8,STR,20,pclTifd); TrimSpace(pclTifd);
			get_fld(pclSqlData,FIELD_9,STR,20,pclPsta); TrimSpace(pclPsta);
			get_fld(pclSqlData,FIELD_10,STR,20,pclPstd); TrimSpace(pclPstd);
			get_fld(pclSqlData,FIELD_11,STR,20,pclEtai); TrimSpace(pclEtai);
			get_fld(pclSqlData,FIELD_12,STR,20,pclEtdi); TrimSpace(pclEtdi);
			get_fld(pclSqlData,FIELD_13,STR,20,pclOnbl); TrimSpace(pclOnbl);
			get_fld(pclSqlData,FIELD_14,STR,20,pclOfbl); TrimSpace(pclOfbl);
			get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

			strcpy(rlIntermediateLatestUse.pclUrno,pclUrno);
			strcpy(rlIntermediateLatestUse.pclAdid,pclAdid);
			strcpy(rlIntermediateLatestUse.pclRkey,pclRkey);
			strcpy(rlIntermediateLatestUse.pclRegn,pclRegn);
			strcpy(rlIntermediateLatestUse.pclStoa,pclStoa);
			strcpy(rlIntermediateLatestUse.pclStod,pclStod);
			strcpy(rlIntermediateLatestUse.pclTifa,pclTifa);
			strcpy(rlIntermediateLatestUse.pclTifd,pclTifd);
			strcpy(rlIntermediateLatestUse.pclPsta,pclPsta);
			strcpy(rlIntermediateLatestUse.pclPstd,pclPstd);
			strcpy(rlIntermediateLatestUse.pclEtai,pclEtai);
			strcpy(rlIntermediateLatestUse.pclEtdi,pclEtdi);
			strcpy(rlIntermediateLatestUse.pclOnbl,pclOnbl);
			strcpy(rlIntermediateLatestUse.pclOfbl,pclOfbl);
			strcpy(rlIntermediateLatestUse.pclTmoa,pclTmoa);

			dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
			dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
			dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
			dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
			dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
			dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
			dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
			dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
			dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
			dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
			dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
			dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
			dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
			dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
			dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
			dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);

			strcpy(rpSentMsg->pclPosi,pcpParkstand);
			strcpy(rpSentMsg->pclStoa,pclStoa);
			strcpy(rpSentMsg->pclEtai,pclEtai);
			strcpy(rpSentMsg->pclTmoa,pclTmoa);
			strcpy(rpSentMsg->pclOnbl,pclOnbl);

			strcpy(pcpUrno,pclUrno);

			/* To find the actual end use for the parking stand */
			FindEndUseBuildWhereClause(rlIntermediateLatestUse,pclWhere,pcpParkstand,igActualUseTime);
			FindEndUseBuildFullQuery(pclSqlBuf,pclWhere);

			memset(pclSqlData,0,sizeof(pclSqlData));
			memset(pclUrno,0,sizeof(pclUrno));
			memset(pclAdid,0,sizeof(pclAdid));
			memset(pclRkey,0,sizeof(pclRkey));
			memset(pclRkey,0,sizeof(pclRegn));
			memset(pclStoa,0,sizeof(pclStoa));
			memset(pclStod,0,sizeof(pclStod));
			memset(pclTifa,0,sizeof(pclTifa));
			memset(pclTifd,0,sizeof(pclTifd));
			memset(pclPsta,0,sizeof(pclPsta));
			memset(pclPstd,0,sizeof(pclPstd));
			memset(pclEtai,0,sizeof(pclEtai));
			memset(pclEtdi,0,sizeof(pclEtdi));
			memset(pclOnbl,0,sizeof(pclOnbl));
			memset(pclOfbl,0,sizeof(pclOfbl));
			memset(pclTmoa,0,sizeof(pclTmoa));

			ilRC = RunSQL(pclSqlBuf,pclSqlData);
			if (ilRC != DB_SUCCESS)
			{
		  	dbg(DEBUG, "<%s> Not found in AFTTAB, line<%d>", pclFunc,__LINE__);
		  	/*return RC_FAIL;*/
			}

			switch(ilRC)
			{
				case NOTFOUND:
					dbg(DEBUG,"<%s> End use is not found",pclFunc);
					/* 1 Latest actual start use found
						 2 Actual end use not found
						 3 Which means the actual use flight has no actual end use time in this parking stand, so finding out this flight's next nearest end use time be below query
					*/
					/* To find finish useage in this Park stand */
					FindFinishUsageBuildWhereClause(rlIntermediateLatestUse,pclWhere,pcpParkstand,igActualUseTime,igConfigEndShiftTime);
					FindFinishUsageBuildFullQuery(pclSqlBuf,pclWhere);

					memset(pclSqlData,0,sizeof(pclSqlData));

					memset(pclUrno,0,sizeof(pclUrno));
					memset(pclAdid,0,sizeof(pclAdid));
					memset(pclRkey,0,sizeof(pclRkey));
					memset(pclRkey,0,sizeof(pclRegn));
					memset(pclStoa,0,sizeof(pclStoa));
					memset(pclStod,0,sizeof(pclStod));
					memset(pclTifa,0,sizeof(pclTifa));
					memset(pclTifd,0,sizeof(pclTifd));
					memset(pclPsta,0,sizeof(pclPsta));
					memset(pclPstd,0,sizeof(pclPstd));
					memset(pclEtai,0,sizeof(pclEtai));
					memset(pclEtdi,0,sizeof(pclEtdi));
					memset(pclOnbl,0,sizeof(pclOnbl));
					memset(pclOfbl,0,sizeof(pclOfbl));
					memset(pclTmoa,0,sizeof(pclTmoa));

					ilRC = RunSQL(pclSqlBuf,pclSqlData);
					if (ilRC != DB_SUCCESS)
					{
				  	dbg(DEBUG, "<%s> Not found in AFTTAB, line<%d>", pclFunc,__LINE__);
				  	return RC_FAIL;
					}
					/*
					URNO,ADID,RKEY,REGN,
					STOA,STOD,TIFA,TIFD,
					PSTA,PSTD,ETAI,ETDI,
					ONBL,OFBL,TMOA
					*/
					get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
					get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
					get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
					get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
					get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
					get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
					get_fld(pclSqlData,FIELD_7,STR,20,pclTifa); TrimSpace(pclTifa);
					get_fld(pclSqlData,FIELD_8,STR,20,pclTifd); TrimSpace(pclTifd);
					get_fld(pclSqlData,FIELD_9,STR,20,pclPsta); TrimSpace(pclPsta);
					get_fld(pclSqlData,FIELD_10,STR,20,pclPstd); TrimSpace(pclPstd);
					get_fld(pclSqlData,FIELD_11,STR,20,pclEtai); TrimSpace(pclEtai);
					get_fld(pclSqlData,FIELD_12,STR,20,pclEtdi); TrimSpace(pclEtdi);
					get_fld(pclSqlData,FIELD_13,STR,20,pclOnbl); TrimSpace(pclOnbl);
					get_fld(pclSqlData,FIELD_14,STR,20,pclOfbl); TrimSpace(pclOfbl);
					get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

					dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
					dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
					dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
					dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
					dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
					dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
					dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
					dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
					dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
					dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
					dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
					dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
					dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
					dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
					dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
					dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);

					/*The next nearest end use time has found, so copy them into SentMsg departure parts and return*/
					strcpy(rpSentMsg->pclPosi,pcpParkstand);
					strcpy(rpSentMsg->pclStod,pclStod);
					strcpy(rpSentMsg->pclEtdi,pclEtdi);
					strcpy(rpSentMsg->pclOfbl,pclOfbl);

					return 3;

					break;
				default:
					/*FOUND*/
					/* In this parking stand, the actual start and end use time is found, so let us choose the next schedule start and end use time*/
					ilFlagUseEST_SCHStartUse = TRUE;

					memset(pcgCurrentTime,0x00,TIMEFORMAT);
					GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

					strcpy(pclSCH_UseStartTime,pcgCurrentTime);
					AddSecondsToCEDATime(pclSCH_UseStartTime, -igConfigShift*60*60*24, 1);
					dbg(DEBUG,"<%s> End use is found, pclSCH_UseStartTime<%s>",pclFunc,pclSCH_UseStartTime);
					break;
			}
			break;
	}

	if (ilFlagUseEST_SCHStartUse == TRUE)
	{
		/* Since in the current parking stand, the actual start and end use time is found, then the SentMsg should be filled by following schedule start and end use time*/

		memset(pcpUrno,0,sizeof(pcpUrno));
		memset(rpSentMsg,0,sizeof(rpSentMsg));

		strcpy(rpSentMsg->pclPosi,pcpParkstand);

		FindEST_SCHBuildWhereClause(pclWhere,pcpParkstand,pclSCH_UseStartTime);
		FindEST_SCHBuildFullQuery(pclSqlBuf,pclWhere);

		/*
		URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA
		*/
		memset(pclSqlData,0,sizeof(pclSqlData));

		memset(pclUrno,0,sizeof(pclUrno));
		memset(pclAdid,0,sizeof(pclAdid));
		memset(pclRkey,0,sizeof(pclRkey));
		memset(pclRkey,0,sizeof(pclRegn));
		memset(pclStoa,0,sizeof(pclStoa));
		memset(pclStod,0,sizeof(pclStod));
		memset(pclTifa,0,sizeof(pclTifa));
		memset(pclTifd,0,sizeof(pclTifd));
		memset(pclPsta,0,sizeof(pclPsta));
		memset(pclPstd,0,sizeof(pclPstd));
		memset(pclEtai,0,sizeof(pclEtai));
		memset(pclEtdi,0,sizeof(pclEtdi));
		memset(pclOnbl,0,sizeof(pclOnbl));
		memset(pclOfbl,0,sizeof(pclOfbl));
		memset(pclTmoa,0,sizeof(pclTmoa));

		ilRC = RunSQL(pclSqlBuf,pclSqlData);
		if (ilRC != DB_SUCCESS)
		{
	  	dbg(DEBUG, "<%s> Not found in AFTTAB, line<%d>", pclFunc,__LINE__);
	  	/*return RC_FAIL;*/
		}

		switch(ilRC)
		{
			case NOTFOUND:
				dbg(TRACE, "<%s> Then search for earliest start / leave flight, ilFlagUseEST_SCHStartUse = TRUE", pclFunc);
				FindEarliest_Start_LeaveBuildWhereClause(pcpParkstand,pclWhere,igConfigEndShiftTime);
				FindEarliest_Start_LeaveBuildFullQuery(pclSqlBuf,pclWhere);

				memset(pclUrno,0,sizeof(pclUrno));
				memset(pclAdid,0,sizeof(pclAdid));
				memset(pclRkey,0,sizeof(pclRkey));
				memset(pclRkey,0,sizeof(pclRegn));
				memset(pclStoa,0,sizeof(pclStoa));
				memset(pclStod,0,sizeof(pclStod));
				memset(pclTifa,0,sizeof(pclTifa));
				memset(pclTifd,0,sizeof(pclTifd));
				memset(pclPsta,0,sizeof(pclPsta));
				memset(pclPstd,0,sizeof(pclPstd));
				memset(pclEtai,0,sizeof(pclEtai));
				memset(pclEtdi,0,sizeof(pclEtdi));
				memset(pclOnbl,0,sizeof(pclOnbl));
				memset(pclOfbl,0,sizeof(pclOfbl));
				memset(pclTmoa,0,sizeof(pclTmoa));

				ilRC = RunSQL(pclSqlBuf,pclSqlData);
				if (ilRC != DB_SUCCESS)
				{
			  	dbg(DEBUG, "<%s> Not found in AFTTAB, line<%d>", pclFunc,__LINE__);
			  	/*return RC_FAIL;*/
				}

				/*get_real_fld(pclAdid);
				get_real_fld(pclPosi);
				get_real_fld(pclPosa);
				get_real_fld(pclPosd);*/
				switch(ilRC)
				{
					case NOTFOUND:

						dbg(DEBUG,"NOTFOUND++++++++++");

						/* There is no schedule start use flight, so return with NO_END which means the SendMsg is null */

						strcpy(rpSentMsg->pclPosi,pcpParkstand);

						ilFlagEnd_Use_found = NO_END;
						return ilFlagEnd_Use_found;

						break;
					default:
						/* FOUND

						dbg(DEBUG,"FOUND++++++++++");*/

						get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
						get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
						get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
						get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); /*@fya 20140303 TrimSpace(pclRegn);*/
						get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
						get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
						get_fld(pclSqlData,FIELD_7,STR,20,pclTifa); TrimSpace(pclTifa);
						get_fld(pclSqlData,FIELD_8,STR,20,pclTifd); TrimSpace(pclTifd);
						get_fld(pclSqlData,FIELD_9,STR,20,pclPsta); TrimSpace(pclPsta);
						get_fld(pclSqlData,FIELD_10,STR,20,pclPstd); TrimSpace(pclPstd);
						get_fld(pclSqlData,FIELD_11,STR,20,pclEtai); TrimSpace(pclEtai);
						get_fld(pclSqlData,FIELD_12,STR,20,pclEtdi); TrimSpace(pclEtdi);
						get_fld(pclSqlData,FIELD_13,STR,20,pclOnbl); TrimSpace(pclOnbl);
						get_fld(pclSqlData,FIELD_14,STR,20,pclOfbl); TrimSpace(pclOfbl);
						get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

						dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
						dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
						dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
						dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
						dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
						dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
						dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
						dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
						dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
						dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
						dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
						dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
						dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
						dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
						dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
						dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);

						/* The latest schedule start use time is found
						if ((strcmp(pclAdid,"A") == 0 || strcmp(pclAdid,"B") == 0) || strcmp(pclPosa,pcpParkstand)==0)*/
						if (strcmp(pclAdid,"A") == 0 || strcmp(pclAdid,"B") == 0)
						{
							/* The found latest schedule start use flight is arrival or towing, then fill the SentMsg with arrival part*/

							strcpy(rpSentMsg->pclPosi,pcpParkstand);
							strcpy(rpSentMsg->pclStoa,pclStoa);
							strcpy(rpSentMsg->pclEtai,pclEtai);
							strcpy(rpSentMsg->pclTmoa,pclTmoa);
							strcpy(rpSentMsg->pclOnbl,pclOnbl);
							strcpy(rpSentMsg->pclTifa,pclTifa);
							strcpy(rpSentMsg->pclRegn,pclRegn);
							strcpy(rpSentMsg->pclRkey,pclRkey);

              ilFlagStart_use_found = START_FOUND;
            	ilFlagFind_Next_Seq_flight = TRUE;
	          }
	          else if (strcmp(pclPstd,pcpParkstand) == 0)
				   	{
				   		/* The found latest schedule start use flight is departure, then fill the SentMsg with departure part*/

				   		strcpy(rpSentMsg->pclPosi,pcpParkstand);
				   		strcpy(rpSentMsg->pclStod,pclStod);
							strcpy(rpSentMsg->pclEtdi,pclEtdi);
							strcpy(rpSentMsg->pclOfbl,pclOfbl);

		   				ilFlagStart_use_found = NO_START;
		   				ilFlagEnd_Use_found = TRUE;
		   				return ilFlagStart_use_found;
						}
						break;
				}
				break;
			default:
				/*FOUND*/
				/* The latest shedule start use time is found, then fill the SentMsg with arrival parts */
				dbg(TRACE, "<%s> Then search for earliest start / leave flight", pclFunc);

				get_fld(pclSqlData,FIELD_1,STR,20,pclUrno); TrimSpace(pclUrno);
				get_fld(pclSqlData,FIELD_2,STR,20,pclAdid); TrimSpace(pclAdid);
				get_fld(pclSqlData,FIELD_3,STR,20,pclRkey); TrimSpace(pclRkey);
				get_fld(pclSqlData,FIELD_4,STR,20,pclRegn); TrimSpace(pclRegn);
				get_fld(pclSqlData,FIELD_5,STR,20,pclStoa); TrimSpace(pclStoa);
				get_fld(pclSqlData,FIELD_6,STR,20,pclStod); TrimSpace(pclStod);
				get_fld(pclSqlData,FIELD_7,STR,20,pclTifa); TrimSpace(pclTifa);
				get_fld(pclSqlData,FIELD_8,STR,20,pclTifd); TrimSpace(pclTifd);
				get_fld(pclSqlData,FIELD_9,STR,20,pclPsta); TrimSpace(pclPsta);
				get_fld(pclSqlData,FIELD_10,STR,20,pclPstd); TrimSpace(pclPstd);
				get_fld(pclSqlData,FIELD_11,STR,20,pclEtai); TrimSpace(pclEtai);
				get_fld(pclSqlData,FIELD_12,STR,20,pclEtdi); TrimSpace(pclEtdi);
				get_fld(pclSqlData,FIELD_13,STR,20,pclOnbl); TrimSpace(pclOnbl);
				get_fld(pclSqlData,FIELD_14,STR,20,pclOfbl); TrimSpace(pclOfbl);
				get_fld(pclSqlData,FIELD_15,STR,20,pclTmoa); TrimSpace(pclTmoa);

				dbg(DEBUG,"<%s>line <%d>",pclFunc,__LINE__);
				dbg(DEBUG,"<%s>pclUrno<%s>",pclFunc,pclUrno);
				dbg(DEBUG,"<%s>pclAdid<%s>",pclFunc,pclAdid);
				dbg(DEBUG,"<%s>pclRkey<%s>",pclFunc,pclRkey);
				dbg(DEBUG,"<%s>pclRegn<%s>",pclFunc,pclRegn);
				dbg(DEBUG,"<%s>pclStoa<%s>",pclFunc,pclStoa);
				dbg(DEBUG,"<%s>pclStod<%s>",pclFunc,pclStod);
				dbg(DEBUG,"<%s>pclTifa<%s>",pclFunc,pclTifa);
				dbg(DEBUG,"<%s>pclTifd<%s>",pclFunc,pclTifd);
				dbg(DEBUG,"<%s>pclPsta<%s>",pclFunc,pclPsta);
				dbg(DEBUG,"<%s>pclPstd<%s>",pclFunc,pclPstd);
				dbg(DEBUG,"<%s>pclEtai<%s>",pclFunc,pclEtai);
				dbg(DEBUG,"<%s>pclEtdi<%s>",pclFunc,pclEtdi);
				dbg(DEBUG,"<%s>pclOnbl<%s>",pclFunc,pclOnbl);
				dbg(DEBUG,"<%s>pclOfbl<%s>",pclFunc,pclOfbl);
				dbg(DEBUG,"<%s>pclTmoa<%s>",pclFunc,pclTmoa);

				/* Copy the latest schedule use flight arrival part to sentMsg */

				strcpy(rpSentMsg->pclPosi,pcpParkstand);
				strcpy(rpSentMsg->pclStoa,pclStoa);
				strcpy(rpSentMsg->pclEtai,pclEtai);
				strcpy(rpSentMsg->pclTmoa,pclTmoa);
				strcpy(rpSentMsg->pclOnbl,pclOnbl);

				ilFlagFindNetSeqFlight = TRUE;
				break;
		}

		if (ilFlagFind_Next_Seq_flight == TRUE)
		{
			/* At this stage, the SentMsg is filled completely*/
			GetSeqFlight(rpSentMsg,"D");
			return ilFlagStart_use_found;
		}
	}
}

static void FindActualUseBuildWhereClause(char *pcpParkstand, char *pcpWhere, int ipConfigTime)
{
	char *pclFunc = "FindActualUseBuildWhereClause";
	char pclWhere[2048] = "\0";
	char pclTmpTimeBottom[TIMEFORMAT] = "\0";
	char pclTmpTimeUpper[TIMEFORMAT] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));

	memset(pcgCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

	strcpy(pclTmpTimeBottom,pcgCurrentTime);
	AddSecondsToCEDATime(pclTmpTimeBottom, -ipConfigTime * 60 * 60 * 24, 1);

	strcpy(pclTmpTimeUpper,pcgCurrentTime);
	AddSecondsToCEDATime(pclTmpTimeUpper, ipConfigTime * 60 * 60 * 24, 1);

	/*sprintf(pclWhere,"PSTA = '%s' and FTYP NOT IN ('X','N') and ADID in ('A','B') and (ONBL between '%s' and '%s') order by ONBL desc",pcpParkstand,pclTmpTime,pcgCurrentTime);*/

	sprintf(pclWhere,"PSTA = '%s' and FTYP NOT IN ('X','N') and ADID in ('A','B') and (TIFA between '%s' and '%s') order by TIFA desc",pcpParkstand,pclTmpTimeBottom,pclTmpTimeUpper);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindActualUseBuildFullQuery(char *pcpSqlBuf, char *pcpWhere)
{
	char *pclFunc = "FindActualUseBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));
	/*sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,ONBL,OFBL,LAND,AIRB,PSTA,PSTD,ETAI,ETDI,TMOA FROM AFTTAB WHERE %s", pcpWhere);*/

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA FROM AFTTAB WHERE %s", pcpWhere);

	/*pclTmoa*/

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void FindEndUseBuildWhereClause(TOWING rpIntermediateLatestUse,char *pcpWhere,char *pcpParkstand,int ipActualUseTime)
{
	char *pclFunc = "FindEndUseBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";
	char pclTmpTimeBottom[TIMEFORMAT] = "\0";
	char pclTmpTimeUpper[TIMEFORMAT] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	strcpy(pclTmpTimeBottom,pclCurrentTime);
	AddSecondsToCEDATime(pclTmpTimeBottom, - ipActualUseTime * 60 * 60 * 24, 1);

	strcpy(pclTmpTimeUpper,pclCurrentTime);
	AddSecondsToCEDATime(pclTmpTimeUpper, ipActualUseTime * 60 * 60 * 24, 1);

	sprintf(pclWhere,"PSTD = '%s' and REGN = '%s' and ((ADID = 'D' and AIRB <> ' ') or (ADID = 'B' and ONBL <> ' ') ) and TIFD between '%s' and  '%s'",pcpParkstand,rpIntermediateLatestUse.pclRegn,pclTmpTimeBottom,pclTmpTimeUpper);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindEndUseBuildFullQuery(char *pcpSqlBuf, char *pcpWhere)
{
	char *pclFunc = "FindEndUseBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void FindFinishUsageBuildWhereClause(TOWING rpIntermediateLatestUse,char *pcpWhere,char *pcpParkstand,int ipActualUseTime,int ipConfigEndShiftTime)
{
	char *pclFunc = "FindFinishUsageBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";
	char pclTmpActualTime[TIMEFORMAT] = "\0";
	char pclTmpConfigEndShiftTime[TIMEFORMAT] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	strcpy(pclTmpActualTime,pclCurrentTime);
	strcpy(pclTmpConfigEndShiftTime,pclCurrentTime);

	AddSecondsToCEDATime(pclTmpActualTime, -ipActualUseTime * 60 * 60 * 24, 1);
	AddSecondsToCEDATime(pclTmpConfigEndShiftTime, ipConfigEndShiftTime * 60 * 60 * 24, 1);

	/*
	@fya 20140311
	*/
	/*
	sprintf(pclWhere,"PSTD = '%s' and REGN = '%s' and TIFD between '%s' and  '%s' order by TIFD",pcpParkstand,rpIntermediateLatestUse.pclRegn,pclTmpActualTime,pclTmpConfigEndShiftTime);
	*/
	/* Since it is another flight, REGN is not in use*/
	sprintf(pclWhere,"PSTD = '%s' and TIFD between '%s' and  '%s' order by TIFD asc",pcpParkstand,pclTmpActualTime,pclTmpConfigEndShiftTime);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindFinishUsageBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "FindFinishUsageBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void FindEST_SCHBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpSCHUseStartTime)
{
	char *pclFunc = "FindEST_SCHBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";
	/*char pclTmpConfigEndShiftTime[TIMEFORMAT] = "\0";*/

	memset(pcpWhere,0,sizeof(pcpWhere));
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	/*strcpy(pclTmpConfigEndShiftTime,pclCurrentTime);
	AddSecondsToCEDATime(pclTmpConfigEndShiftTime, ipConfigEndShiftTime*60*60*24, 1);*/
	/*
	@fya 20140311
	Since it is another flight, REGN is not in use.
	*/
	sprintf(pclWhere,"PSTA = '%s' and FTYP NOT IN ('X','N') and ADID in ('A','B') and (TIFA between '%s' and '%s') order by TIFA asc",pcpParkstand,pcpSCHUseStartTime,pclCurrentTime);


	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}
static void FindEST_SCHBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "FindEST_SCHBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void FindEarliest_Start_LeaveBuildWhereClause(char *pcpParkstand,char *pcpWhere,int ipConfigEndShiftTime)
{
	char *pclFunc = "FindEarliest_Start_LeaveBuildWhereClause";
	char pclCurrentTime[64] = "\0";
	char pclWhere[2048] = "\0";
	char pclTmpConfigEndShiftTime[TIMEFORMAT] = "\0";

	memset(pcpWhere,0,sizeof(pcpWhere));
	GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

	strcpy(pclTmpConfigEndShiftTime,pclCurrentTime);

	AddSecondsToCEDATime(pclTmpConfigEndShiftTime, ipConfigEndShiftTime * 60 * 60 * 24, 1);

	sprintf(pclWhere,"((PSTA = '%s' and TIFA between '%s' and '%s') or (PSTD = '%s' and TIFD between '%s' and '%s')) and FTYP NOT IN ('X','N') ORDER BY DECODE(ADID,'A',TIFA,'D',TIFD,'B',TIFA) asc ",pcpParkstand,pclCurrentTime,pclTmpConfigEndShiftTime,pcpParkstand,pclCurrentTime,pclTmpConfigEndShiftTime);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindEarliest_Start_LeaveBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "FindEarliest_Start_LeaveBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,TIFA,TIFD,PSTA,PSTD,ETAI,ETDI,ONBL,OFBL,TMOA FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
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

	sprintf(pclSqlBuf, "UPDATE LIGTAB SET STAT = 'A' WHERE %s", pcpWhere);

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

static void BuildCurrentTime(void)
{
	char *pclFunc = "BuildBatchFlightRng";

	memset(pcgCurrentTime,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pcgCurrentTime );

	memset(pcgTimeUpperLimit,0x00,TIMEFORMAT);
	GetServerTimeStamp( "UTC", 1, 0, pcgTimeUpperLimit );
  AddSecondsToCEDATime(pcgTimeUpperLimit, igTimeRange*60*60*24, 1);
	dbg(TRACE,"<%s>: Currnt Date is <%s>, Time Upper Limit is <%s>",pclFunc,pcgCurrentTime,pcgTimeUpperLimit);
}

static void FindNextAllocDepBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpTifdNewData)
{
	char *pclFunc = "FindNextAllocDepBuildWhereClause";
	char pclWhere[2048] = "\0";
	char pclTmpTime[TIMEFORMAT] = "\0";

	strcpy(pclTmpTime,pcpTifdNewData);

	/*
	@fya 20140310
	The time upper ramge is configurable
	*/
	/*AddSecondsToCEDATime(pclTmpTime, 2 * 60 * 60 * 24, 1);*/
	AddSecondsToCEDATime(pclTmpTime, igNextAllocDepUpperRange * 60 * 60 * 24, 1);

	/*sprintf(pclWhere,"PSTD = '%s' and FTYP NOT IN ('X','N') and ADID = 'D' and TIFD between '%s' and '%s' and AIRB=' ' order by TIFA desc",pcpParkstand,pcpTifdNewData,pclTmpTime);*/

	sprintf(pclWhere,"PSTD = '%s' and FTYP NOT IN ('X','N') and ADID = 'D' and TIFD between '%s' and '%s' and AIRB=' ' order by TIFD asc",pcpParkstand,pcpTifdNewData,pclTmpTime);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindNextAllocDepBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "FindNextAllocDepBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));
	/*URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA,AIRB*/
	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA,AIRB FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}

static void FindNextAllocArrBuildWhereClause(char *pcpWhere,char *pcpParkstand,char *pcpFormalTifd, char *pcpNextTifd)
{
	char *pclFunc = "FindNextAllocArrBuildWhereClause";
	char pclWhere[2048] = "\0";
	/*char pclTmpConfigEndShiftTime[TIMEFORMAT] = "\0";*/

	memset(pcpWhere,0,sizeof(pcpWhere));

	/*strcpy(pclTmpConfigEndShiftTime,pclCurrentTime);

	AddSecondsToCEDATime(pclTmpConfigEndShiftTime, ipConfigEndShiftTime*60*60*24, 1);*/

	sprintf(pclWhere,"PSTA = '%s' and FTYP NOT IN ('X','N') and ADID = 'A' and TIFA between '%s' and '%s' order by TIFA asc",pcpParkstand,pcpFormalTifd,pcpNextTifd);

	strcpy(pcpWhere,pclWhere);
  dbg(DEBUG,"<%s>Where Clause<%s>",pclFunc,pcpWhere);
}

static void FindNextAllocArrBuildFullQuery(char *pcpSqlBuf,char *pcpWhere)
{
	char *pclFunc = "FindNextAllocArrBuildFullQuery";
	char pclSqlBuf[2048] = "\0";

	memset(pcpSqlBuf,0,sizeof(pcpSqlBuf));

	sprintf(pclSqlBuf, "SELECT URNO,ADID,RKEY,REGN,STOA,STOD,ETAI,ETDI,TIFA,TIFD,ONBL,OFBL,PSTA,PSTD,TMOA FROM AFTTAB WHERE %s", pcpWhere);

  strcpy(pcpSqlBuf,pclSqlBuf);
  dbg(DEBUG,"\n*******************<%s>*******************",pclFunc);
	dbg(DEBUG,"<%s>Full Query<%s>",pclFunc,pcpSqlBuf);
}
