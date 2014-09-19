#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_versmks[] = "@(#) "UFIS_VERSION" $Id: Standard/Kernel/aftnlsr.c MKS: 4.5.0.05 2013/10/18 18:57:19CET otr $";
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Base/Server/Interface/aftnlsr.c 1.8 2014/04/21 17:00:00SGT zdo $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* ABB ACE/FC Program Skeleton                                                */
/*                                                                            */
/* Author         :   OTR                                                        */
/* Date           :                                                           */
/* Description    :   Read AFTN from TCP/UDP Port listener and send to fdihdl */
/*                                                                            */
/* Update history :                                                           */
/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command")     */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */
static char sccs_version[] ="@(#) UFIS51 (c) UFIS skeleton.c 51.1.0 / 25.01.2013 OTR";
/* be carefule with strftime or similar functions !!!                         */
/*                                                                            */
/******************************************************************************/
/* This program is a MIKE main program 										*/
/* v01   First release  													*/
/* v02   Able to handle part of Telex and merge  							*/
/* v03   Support persistent connection 										*/
/* v04   Parameter START_UDP_LISTENER = FALSE/TRUE  (Default=FALSE)			*/
/* v05   Implement CH REPLY MESSAGE											*/
/* ****************************************************************************

20140401 v1.6 ZDO UFIS-5497.
                  Change child process to multi-thread. Child check into Telex msg for sequence num.
                   - Sending CH message to AFTN-PC every 20 minutes. Triggered by ntisch.
                   - If channel sequence number in Telex message from AFTN-PC skip back, send SVC LR EXP msg
                   - If channel sequence number in Telex message from AFTN-PC skip forward, send SVC QTA MIS msg
                   - When sequence number from AFTN-PC changes to 1, check if it's around midnight.
                   - All Telex messages received from or sent to AFTN-PC in recent 30 days(configurable) are saved
                     to file /ceda/debug/aftnlsr_MsgLog.MMDD on a daily basis.
20140407 v1.7 ZDO UFIS-5497.
                   - Check CH msg from AFTN-PC. If CH missed, send SVC MIS CH msg
20140408 v1.8 ZDO UFIS-5497.
                   - Send Ack msg for SS priority msg.
20140410 v1.8 ZDO Some code changes for last 2 requirements(v1.7, v1.8).
20140421 v1.8 ZDO Some code changes for v1.6


* *****************************************************************************/


#define U_MAIN
#define UGCCS_PRG
#define STH_USE
#include <stdio.h>
#include <stdarg.h>
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
#include <pthread.h>
#include "aftnlsr.h"
#include "AATArray.h"

#if defined(_HPUX) || defined(_SOLARIS) || defined(_AIX)
#include <fcntl.h>
#include <sys/socket.h>
#endif

/*
#define CHAR_SOH	(char)'\001'
#define CHAR_STX	(char)'\002'
#define CHAR_ETX	(char)'\003'
*/

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
static char  cgHopo[8] = "\0";                      /* default home airport    */
static char  cgTabEnd[8] ="\0";                     /* default table extension */
static char  pcgTwEnd[XS_BUFF];          			/* buffer for TABEND */
static long lgEvtCnt = 0;

extern int GetDataItem(char *pcpResult, char *pcpInput, int ipNum, char cpDel,char *pcpDef,char *pcpTrim);
extern int get_item_no(char *s, char *f, short elem_len);
extern  int SearchStringAndReplace(char *pcpString, char *pcpSearch, char *pcpReplace);
extern int GetNoOfElements(char *, char );
/*extern int AddSecondsToCEDATime(char *,long,int);

static int AddSecondsToCEDATime12(char *,long,int);
*/
static int TimeToStr(char *pcpTime,time_t lpTime);
static void TrimRight(char *pcpBuffer);

/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
static int  Init_aftnlsr();
static int ReadConfigEntry(char *pcpSection,char *pcpKeyword,
			   char *pcpCfgBuffer);


static int	Reset(void);                       /* Reset program          */
static void	Terminate(int ipSleep);            /* Terminate program      */
static void	HandleErr(int);                    /* Handles general errors */
static void   HandleQueErr(int);                 /* Handles queuing errors */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/


static int	HandleData(EVENT *prpEvent);       /* Handles event data     */
static int SendEvent(char *pcpCmd,int ipModID,int ipPriority,char *pcpTable,
             char *pcpTwStart, char *pcpTwEnd,
             char *pcpSelection,char *pcpFields,char *pcpData,
             char *pcpAddStruct,int ipAddStructSize);

/*************************/
/*GET CONFIG AND DB-DATA */
/*************************/
static int ReadCfg(void);
static void GetConfig(char* pcpFile,char* pcpSection,char* pcpTag,char* pcpTarget,char* pcpDefault);
static void GetConfigFields(char* pcpFile,char* pcpSection,char* pcpTag,char* pcpTarget,char* pcpDefault);
static void GetConfigSwitch(char* pcpFile,char* pcpSection,char* pcpTag,int* piTarget,int piDefault);
static void GetConfigValue(char* pcpFile,char* pcpSection,char* pcpTag,int* piTarget,int piDefault);

/* Belong to aftnlsr */
static int InitAFTNListener();
static void CreateAFTNtcpListener(int piPort);
static void CreateAFTNudpListener(int piPort);
static void SendUDPMessage(char *pIP, int pPort, char *pMessage);
static int SendTCPMessage(char *pIP, int pPort, char *pMessage);
static void ProcessAFTNRequest(char * pIP, char *pcpData);
static void UpdateRecord(char *pcpData);
static void UpdateAFTNRecord(char *pcpData);
static void ReadRecord(char *pIP, char *pcpData);
static void GetRecordFromDB(char *pTable, char *pFields, char *pSelection, int pFldCnt, char *pData);
static int ChangeCharFromTo(char *pcpData,char *pcpFrom,char *pcpTo);
static void SendSocket(char *lsTmpIP, char *pData);
static void ChildCollectTELEX(char *pcpData, char *pcpIp);
static void ParentCollectTELEX(char *pcpData);
static void child_start();
static void *childReader(void *);
static void *childWriter(void *);
static void *childSig(void *);
static int putWriteMsg(char * pcpData);
static void TerminateChild();
static void ChildHandleTelex(char * pcpData, char *pcpIp);
static BOOL CheckCHMsg(TELEXMSG * pspTelex);
static BOOL CheckSVCMsg(TELEXMSG * pspTelex);
static int HandleSVC(TELEXMSG * pspTelex);
static int CheckPrioId(TELEXMSG *pspTelex);
static int ParseTelex(char* pcpData, TELEXMSG *pspTelex);
static int CheckSequenceNum(TELEXMSG * pspTelex);
static int GetSequenceNum(char* pcpData);
static int SendSVCMIS(int ipLastSeqNum, int ipSeqNum, TELEXMSG * pspTelex);
static int SendSSAck(TELEXMSG *pspTelex);
static int GenerateHeading(char* pcpRes, char* pcpRecTransId);
static int SendSVCEXP(int ipLastSeqNum, int ipSeqNum, TELEXMSG * pspTelex);
static BOOL isNowMidNight();
static int SendCHMsg();
static int SendCHMisMsg();
static int  MtMsgLogSwitch(struct tm * pspTm);
static void MtMsgLog(char * pcpData, int who);
static int  MtMsgLogRelease();
static int  MtMsgLogInit();
static int  MtMsgLogDelete();
static void get_time(char *s);

/* Programe difinition */
int  igshow_config = TRUE;
static int giListenerPort;
static int giMonitorPort;
static char pcgMainProtocol[12];
pid_t childlsrTcpSock=0, childlsrUdpSock=0;
/* pthread_t childlsrTcpSock=0, childlsrUdpSock=0; */
static int igTCPTimeout=0;
int igMaxReqFields = 10;
static char pagReqFields[10][10];

static char pcgClientChars[20];
static char pcgServerChars[20];
static char pcgAFTNBuffer[XXXL_BUFF];
static int giFDI_Que_Id=8450;

static char gcStartOfTelex = (char)'\001';
static char gcEndOfTelex = (char)'\003';
static char CHAR_CR = (char)'\x0D';
static char CHAR_LF = (char)'\x0A';
static char CHAR_SOH = (char)'\001';
static char CHAR_STX = (char)'\002';
static char CHAR_ETX = (char)'\003';
static char CHAR_BEL = (char)'\007';
static int  igTRC_SEQ = 0;
static char gcCURRENT_DT[7];

static int igEnable_CH_REPLY_MSG = 0;
static int igMSG_DATETIME_UTC = 1;
static int igLastSeqNum = -1;
static int igMidNight_Min_Buf = 0;
static int igRecCH_Min_Buf = 0;
static int igDays_Msg_Log = 0;
static char pcgTRC_DESIGNATED_TEXT[VXS_BUFF];
static char pcgCH_MESSAGE_TEXT[VXS_BUFF];
static char pcgAFTNLSR_ORIG_ID[VXS_BUFF];
static char pcgSVC_ADDR_ID[VXS_BUFF];
static char pcgDEFAULT_ADDR_PRIO_ID[VXS_BUFF];
static char pcgCH_EXP_TIME[VXS_BUFF];

int  igChildCheckEndOfTelex = FALSE;
int  igParentCheckEndOfTelex = FALSE;
int  igkeepAliveMode = FALSE;
int  igStartUDPListener = FALSE;

time_t tgLastRecCHTime = 0;

static int             igListenSocket = -1;
static int             igClientSocket = -1;
static pthread_t       ThreadID_sig = NULL;
static pthread_t       ThreadID_reader = NULL;
static pthread_t       ThreadID_writer = NULL;
static pthread_attr_t  ThreadAttribute;
static pthread_cond_t  WriteWait;
static pthread_mutex_t WriteLock;
static MSGBUF *        WriteBuf = NULL;

static MT_MSG_LOG      sgMtMsgLog;
static pthread_mutex_t LastTelexMsgLock;
static char *          pcgLastTelexMsg = NULL;

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
			ilRc = Init_aftnlsr();
			if(ilRc != RC_SUCCESS)
			{
				dbg(TRACE,"Init_Process: init failed!");
			} /* end of if */

	} else {
		Terminate(30);
	}/* end of if */



	dbg(TRACE,"=====================");
	dbg(TRACE,"MAIN: initializing OK");
	/* dbg(TRACE,"Vorsicht, ich bin nur ein Skeleton und tue nichts.......... "); */
	dbg(TRACE,"=====================");
	
	if ((ilRc = InitAFTNListener()) != RC_SUCCESS)
    {
		Terminate(2);
	}
	

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


static int Init_aftnlsr()
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

	if (ilRc != RC_SUCCESS)
	{
	    dbg(TRACE,"<Init_aftnlsr> failed");
	}
	
	memset(pcgTwEnd,0x00,XS_BUFF);
    sprintf(pcgTwEnd,"%s,%s,%s",cgHopo,cgTabEnd,mod_name);
    dbg(TRACE,"%05d: Init_fldhdl : TW_END = <%s>",__LINE__,pcgTwEnd);

	if ((ilRc = ReadCfg()) != RC_SUCCESS)
    {
      /*  dbg(TRACE,"Init_dsphdl: ReadCfg() failed! Can't continue!"); */
      dbg(TRACE,"<Init_aftnlsr> Default settings in use !!");
      ilRc = RC_SUCCESS;
    }
	
	if (ilRc != RC_SUCCESS)
	{
	    dbg(TRACE,"Init_Process failed");
	}

		return(ilRc);
	
} /* end of initialize */

static int MtMsgLogInit()
{
    int         ilRc = RC_SUCCESS;
    struct tm * _tm;

    time_t t = time(NULL);

    if (igMSG_DATETIME_UTC)
    {
        _tm = (struct tm *)gmtime(&t);
    } else
    {
        _tm = (struct tm *)localtime(&t);
    }

    sgMtMsgLog.msgcount = 0;
    sgMtMsgLog.month = _tm->tm_mon + 1;    /* 1~12 */
    sgMtMsgLog.day = _tm->tm_mday;
    sprintf(sgMtMsgLog.filename, "%s/%s_MsgLog.%02d%02d", getenv("DBG_PATH"), mod_name, sgMtMsgLog.month, sgMtMsgLog.day);

    sgMtMsgLog.fp = fopen(sgMtMsgLog.filename, "a");

    pthread_mutex_init(&(sgMtMsgLog.mutex),NULL);

    MtMsgLogDelete();

    return ilRc;
}

static int MtMsgLogRelease()
{
    int        ilRc = RC_SUCCESS;

    pthread_mutex_destroy(&(sgMtMsgLog.mutex));

    if(sgMtMsgLog.fp != NULL)
    {
        fclose(sgMtMsgLog.fp);
        sgMtMsgLog.fp = NULL;
    }

    return ilRc;
}

/*
 * who: 0 - reader, 1 - writer
 */
static void MtMsgLog(char * pcpData, int who)
{
    struct tm * _tm;
    char        sbuf[S_BUFF];

    memset(sbuf, 0x0, sizeof(sbuf));

    time_t t = time(NULL);

    if (igMSG_DATETIME_UTC)
    {
        _tm = (struct tm *)gmtime(&t);
    } else
    {
        _tm = (struct tm *)localtime(&t);
    }

    get_time(sbuf);

    pthread_mutex_lock(&(sgMtMsgLog.mutex));

    if((sgMtMsgLog.month != (_tm->tm_mon+1)) || (sgMtMsgLog.day != _tm->tm_mday))
    {
        MtMsgLogSwitch(_tm);
    }

    fseek(sgMtMsgLog.fp, 0, SEEK_END);
    if(who > -1)
    {
        sgMtMsgLog.msgcount++;
        if(sgMtMsgLog.msgcount < 0)
            sgMtMsgLog.msgcount = 1;

        fprintf(sgMtMsgLog.fp, "%s              %s <%d>\n",
                sbuf, (who == READER)? "<<<<----------" : "---------->>>>", sgMtMsgLog.msgcount);
    }
    else
        fprintf(sgMtMsgLog.fp, "%s\n", sbuf);

    fprintf(sgMtMsgLog.fp, "%s\n", pcpData);
    fflush(sgMtMsgLog.fp);

    pthread_mutex_unlock(&(sgMtMsgLog.mutex));
}

static int MtMsgLogSwitch(struct tm * pspTm)
{
    int       ilRc = RC_SUCCESS;
    char        sbuf[S_BUFF];

    memset(sbuf, 0x0, sizeof(sbuf));
    get_time(sbuf);

    fprintf(sgMtMsgLog.fp, "<<<<<<<<   switch on %s\n", sbuf);
    fflush(sgMtMsgLog.fp);
    fclose(sgMtMsgLog.fp);
    sgMtMsgLog.fp = NULL;

    sgMtMsgLog.msgcount = 0;
    sgMtMsgLog.month = pspTm->tm_mon + 1;   /* 1~12  */
    sgMtMsgLog.day = pspTm->tm_mday;
    sprintf(sgMtMsgLog.filename, "%s/%s_MsgLog.%02d%02d", getenv("DBG_PATH"), mod_name, sgMtMsgLog.month, sgMtMsgLog.day);

    sgMtMsgLog.fp = fopen(sgMtMsgLog.filename, "a");

    MtMsgLogDelete();

    return ilRc;
}

/* Delete MsgLog igDays_Msg_Log days ago */
static int MtMsgLogDelete()
{
    int       ilRc = RC_SUCCESS;
    int       ili;
    int       ilt;
    char      pcpFile[VS_BUFF];
    struct tm * _tm;
    time_t    t, t1;

    t1 = time(NULL);

    for(ili = 0; ili < 5; ili++)
    {
        t = t1 - (ili + igDays_Msg_Log)*24*60*60;

        if (igMSG_DATETIME_UTC)
        {
            _tm = (struct tm *)gmtime(&t);
        } else
        {
            _tm = (struct tm *)localtime(&t);
        }

        sprintf(pcpFile, "%s/%s_MsgLog.%02d%02d", getenv("DBG_PATH"), mod_name, _tm->tm_mon + 1, _tm->tm_mday);

        remove(pcpFile);
    }

    return ilRc;
}

static void get_time(char *s)
{
    struct timeb tp;
    time_t _CurTime;
    struct tm *CurTime;
    struct tm rlTm;
    char tmp[10];

    if (s != NULL) {
        _CurTime = time(0L);
        CurTime = (struct tm *) localtime(&_CurTime);
        rlTm = *CurTime;

        strftime(s, 20, "%" "Y%" "m%" "d%" "H%" "M%" "S", &rlTm);

        ftime(&tp);
        sprintf(tmp, ":%3.3d:", tp.millitm);
        strcat(s, tmp);
    }
}

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

	if (childlsrTcpSock > 0)
	{
		dbg(TRACE,"Terminate: TCP Listener ...");
		kill(childlsrTcpSock,SIGUSR2);
	}
	if (childlsrUdpSock > 0)
	{
		dbg(TRACE,"Terminate: UDP Listener ...");
		kill(childlsrUdpSock,SIGKILL);
	}
	
	dbg(TRACE,"Terminate: now DB logoff ...");

	signal(SIGCHLD,SIG_IGN);

	logoff();

	dbg(TRACE,"Terminate: now sleep(%d) ...",ipSleep);


	dbg(TRACE,"Terminate: now leaving ...");

	fclose(outp);

	sleep(ipSleep < 5 ? 5 : ipSleep);
	
	exit(0);
	
} /* end of Terminate */


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

			ilRc = Init_aftnlsr();
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
	/* DebugPrintBchead(TRACE,prlBchead);   */
	/* DebugPrintCmdblk(TRACE,prlCmdblk);    */
	/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command"); */
	dbg(TRACE,"Command: <%s>",prlCmdblk->command);
	/* dbg(TRACE,"originator follows event = %p ",prpEvent); */

	/* dbg(TRACE,"originator<%d>",prpEvent->originator); */
	/* dbg(TRACE,"selection follows Selection = %p ",pclSelection); */
	dbg(TRACE,"selection <%s>",pclSelection);
	dbg(TRACE,"fields    <%s>",pclFields);
	/* dbg(TRACE,"data      <%s>",pclData);  */
	/****************************************/

	if (0 == strcmp(prlCmdblk->command, "AFTN"))	/* handle command and request from TCP/UDP socket */
	{
		if ((strncmp(pclSelection, "TCP", 3) == 0) || (strncmp(pclSelection, "UDP", 3) == 0))
		{
			ProcessAFTNRequest(pclFields, pclData);
		}
	} 
	else if (0 == strcmp(prlCmdblk->command, "AFTR"))	/* handle command and request from TCP/UDP socket */
	{
		kill(childlsrTcpSock, SIGUSR1);
	} 

	dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);

	return(RC_SUCCESS);
	
} /* end of HandleData */

/* ********************************************************************/
/* The ReadCfg() routine                                              */
/* ********************************************************************/
static int ReadCfg(void)
{
	int ilRc = RC_SUCCESS;
	char pclTmp[S_BUFF];
	int ilI;
	char *pclTmpPtr;
	char pclTmpNum[16];
	
	strcpy(gcCURRENT_DT, "       ");
	memset(pcgAFTNBuffer, 0x00, sizeof(pcgAFTNBuffer));
	
	memset(pcgTRC_DESIGNATED_TEXT, 0x00, sizeof(pcgTRC_DESIGNATED_TEXT));
	memset(pcgCH_MESSAGE_TEXT, 0x00, sizeof(pcgCH_MESSAGE_TEXT));
	memset(pcgAFTNLSR_ORIG_ID, 0x00, sizeof(pcgAFTNLSR_ORIG_ID));
	memset(pcgSVC_ADDR_ID, 0x00, sizeof(pcgSVC_ADDR_ID));
	memset(pcgDEFAULT_ADDR_PRIO_ID, 0x00, sizeof(pcgDEFAULT_ADDR_PRIO_ID));
	
	GetConfigValue(cgConfigFile,"MAIN","LISTENER_PORT",&giListenerPort,3801);
	GetConfigValue(cgConfigFile,"MAIN","MONITOR_PORT",&giMonitorPort,3802);
	GetConfig(cgConfigFile,"MAIN","MAIN_PROTOCOL",pcgMainProtocol,"UDP");
	GetConfigValue(cgConfigFile,"MAIN","TCP_TIMEOUT",&igTCPTimeout,0);
	GetConfigValue(cgConfigFile,"MAIN","FDI_QUE_ID",&giFDI_Que_Id,8450);
	GetConfigSwitch(cgConfigFile,"MAIN","CHILD_CHECK_ENDOFTELEX",&igChildCheckEndOfTelex,FALSE);
	GetConfigSwitch(cgConfigFile,"MAIN","PARENT_CHECK_ENDOFTELEX",&igParentCheckEndOfTelex,FALSE);
	GetConfigSwitch(cgConfigFile,"MAIN","PERSISTENT_CONNECTION",&igkeepAliveMode,FALSE);
	GetConfigSwitch(cgConfigFile,"MAIN","START_UDP_LISTENER",&igStartUDPListener,FALSE);
	GetConfigSwitch(cgConfigFile,"MAIN","ENABLE_CH_REPLY_MESSAGE",&igEnable_CH_REPLY_MSG,FALSE);
	GetConfigSwitch(cgConfigFile,"MAIN","MESSAGE_DATETIME_UTC",&igMSG_DATETIME_UTC,TRUE);
	GetConfigValue(cgConfigFile,"MAIN","MIDNIGHT_MIN_BUF",&igMidNight_Min_Buf,5);
	GetConfigValue(cgConfigFile,"MAIN","RECCH_MIN_BUF",&igRecCH_Min_Buf,2);
	GetConfigValue(cgConfigFile,"MAIN","DAYS_MSG_LOG",&igDays_Msg_Log,30);
	
		/* Start Server and Client Character replacement */
	GetConfig(cgConfigFile,"MAIN","CLIENTCHARS",pclTmp,"");
	if (strlen(pclTmp) == 0)
	 strcpy(pcgClientChars,"\042\047\054\012\015\072\173\175\077");
	else
	{
	 ilI = 0;
	 pclTmpPtr = pclTmp;
	 while (pclTmpPtr != NULL)
	 {
		pclTmpPtr++;
		strncpy(pclTmpNum,pclTmpPtr,3);
		pclTmpNum[3] = '\0';
		pcgClientChars[ilI] = atoi(pclTmpNum);
		ilI++;
		pclTmpPtr = strstr(pclTmpPtr,",");
	 }
	 pcgClientChars[ilI] = '\0';
	}
	if (igshow_config == TRUE)
	 dbg(TRACE,"ReadCfg: Client Chars = <%s>",pcgClientChars);
	GetConfig(cgConfigFile,"MAIN","SERVERCHARS",pclTmp,"");
	if (strlen(pclTmp) == 0)
	 strcpy(pcgServerChars,"\260\261\262\264\263\277\223\224\231");
	else
	{
	 ilI = 0;
	 pclTmpPtr = pclTmp;
	 while (pclTmpPtr != NULL)
	 {
		pclTmpPtr++;
		strncpy(pclTmpNum,pclTmpPtr,3);
		pclTmpNum[3] = '\0';
		pcgServerChars[ilI] = atoi(pclTmpNum);
		ilI++;
		pclTmpPtr = strstr(pclTmpPtr,",");
	 }
	 pcgServerChars[ilI] = '\0';
	}
	if (igshow_config == TRUE)
	 dbg(TRACE,"ReadCfg: Server Chars = <%s>",pcgServerChars);
	
	/* End of Telex char */
	memset(pclTmp, 0x00, sizeof(pclTmp));
	GetConfig(cgConfigFile,"MAIN","END_OF_TELEX",pclTmp,"");
	if (strlen(pclTmp) == 0)
	{
		dbg(TRACE,"ReadCfg: Default of END_OF_TELEX = CHAR <%d>", gcEndOfTelex);
	} else
	{
		gcEndOfTelex = atoi(pclTmp);
		dbg(TRACE,"ReadCfg: Set END_OF_TELEX = CHAR <%d>", gcEndOfTelex);
	}

	/* Start of Telex char */
	memset(pclTmp, 0x00, sizeof(pclTmp));
	GetConfig(cgConfigFile,"MAIN","START_OF_TELEX",pclTmp,"");
	if (strlen(pclTmp) == 0)
	{
		dbg(TRACE,"ReadCfg: Default of START_OF_TELEX = CHAR <%d>", gcStartOfTelex);
	} else
	{
		gcStartOfTelex = atoi(pclTmp);
		dbg(TRACE,"ReadCfg: Set START_OF_TELEX = CHAR <%d>", gcStartOfTelex);
	}
	
	GetConfig(cgConfigFile,"MAIN","TRC_DESIGNATED_TEXT", pcgTRC_DESIGNATED_TEXT, "FYA");
	GetConfig(cgConfigFile,"MAIN","CH_MESSAGE_TEXT", pcgCH_MESSAGE_TEXT, "CH");
	GetConfig(cgConfigFile,"MAIN","AFTNLSR_ORIG_ID", pcgAFTNLSR_ORIG_ID, "EPWAYDYA");
	GetConfig(cgConfigFile,"MAIN","SVC_ADDR_ID", pcgSVC_ADDR_ID, "EPWWYFYC");
	GetConfig(cgConfigFile,"MAIN","DEFAULT_ADDR_PRIO_ID", pcgDEFAULT_ADDR_PRIO_ID, "FF");

	return RC_SUCCESS;
}

static int InitAFTNListener()
{
	int ilRc = RC_SUCCESS;
	
	childlsrTcpSock =fork();
	switch(childlsrTcpSock)
	{
		case -1:
			dbg(TRACE,"Error during creating child tcp listener process!. <%s> can not receive any call", mod_name);
			ilRc = -1;
			break;
		case 0:
		  /* Child's code goes here */
		  if (igkeepAliveMode)
		  {
			  /* CreateAFTNtcpPersistent(giListenerPort); */
		      child_start();
		  }
		  else
		  {
			  CreateAFTNtcpListener(giListenerPort);
		  } 

		  break;
		default:
		  /* Parent's code goes here */
			dbg(TRACE, "TCP Listener Executed from <%s>", mod_name);
			if (igStartUDPListener)
			{
				childlsrUdpSock =fork();
				switch(childlsrUdpSock)
				{
					case -1:
						dbg(TRACE,"Error during creating child udp listener process!. <%s> can not receive any call", mod_name);
						ilRc = -1;
						break;
					case 0:
					  /* Child's code goes here */
					  CreateAFTNudpListener(giListenerPort);
					  break;
					default:
					  /* Parent's code goes here */
					  dbg(TRACE, "UDP Listener Executed from <%s>", mod_name);
					break;
				}
			}
		break;
	}
	
		
	return RC_SUCCESS;
}

static void child_start()
{
    int      ilRc;
    sigset_t set;
    int      sig;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_mutex_init(&WriteLock, NULL);
    pthread_mutex_init(&LastTelexMsgLock, NULL);
    pthread_cond_init(&WriteWait, NULL);

    pthread_attr_init(&ThreadAttribute);
    pthread_attr_setdetachstate(&ThreadAttribute,PTHREAD_CREATE_DETACHED);

    MtMsgLogInit();

    ilRc = pthread_create(&ThreadID_sig, &ThreadAttribute, childSig, NULL);
    dbg(TRACE,"MAIN.: Create childSig. ilRc = %d,%s", ilRc,strerror(errno));

    ilRc = pthread_create(&ThreadID_reader, &ThreadAttribute, childReader, NULL);
    dbg(TRACE,"MAIN.: Create childReader. ilRc = %d,%s", ilRc,strerror(errno));

    ilRc = pthread_create(&ThreadID_writer, &ThreadAttribute, childWriter, NULL);
    dbg(TRACE,"MAIN.: Create childWriter. ilRc = %d,%s", ilRc,strerror(errno));

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    while(1)
    {
        sigwait(&set, &sig);
        dbg(TRACE, "child_start: Main thread receive sig: %d", sig);

        /*child is asked to exit*/
        if(sig == SIGUSR2)
        {
            TerminateChild();
        }
    }
}

static void *childReader(void *arg)
{
    int sockfd, newsockfd, clilen;
    int oldState, oldType;
    char buffer[XXL_BUFF];
    char    clCH_MSG[M_BUFF];
    struct sockaddr_in serv_addr, cli_addr;
    int  nbytes, ilRC;
    int count;
    int optval;
    BOOL    isComplTelex;
    char * pclData = NULL;

#if defined(_HPUX)
    int optlen = sizeof(optval);
#else
    socklen_t optlen = sizeof(optval);
#endif

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        dbg(TRACE,"Create tcp socket failed! error: %d", errno);
        TerminateChild();
    }

    igListenSocket = sockfd;

    if (igkeepAliveMode)
    {
       if(getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen) < 0)
       {
          dbg(TRACE,"getsockopt SO_KEEPALIVE failed!. errno:%d:%s", errno, strerror(errno));
          close(sockfd);
          TerminateChild();
       }
       if (optval == 1)
       {
            dbg(TRACE,"SO_KEEPALIVE is ON.");
       } else
       {
           optval = 1;
           optlen = sizeof(optval);
           if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
           {
              dbg(TRACE,"setsockopt SO_KEEPALIVE failed!. errno:%d:%s", errno, strerror(errno));
              close(sockfd);
              TerminateChild();
           } else
           {
                dbg(TRACE,"Set SO_KEEPALIVE succeed");
           }
        }
    }

    optval = 1;
    optlen = sizeof(optval);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        dbg(TRACE,"setsockopt SO_REUSEADDR failed!. errno:%d:%s", errno, strerror(errno));
        close(sockfd);
        TerminateChild();
    }

    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(giListenerPort);

    /* Now bind the host address using bind() call.*/
    count = 5;
    while (((ilRC=bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) && (count-- > 0))
    {
         dbg(TRACE,"childReader: bind tcp socket failed!. errno: %d. count: %d. sleep 2 seconds.", errno, count);
         sleep(2);
    }

    if(ilRC < 0)
    {
        close(sockfd);
        TerminateChild();
    }

    dbg(TRACE, "Child of <%s> TCP listening on port <%d>", mod_name, giListenerPort);
    /* Now start listening for the clients, here
     * process will go in sleep mode and will wait
     * for the incoming connection
     */
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    while (1)
    {
        pthread_testcancel();

        igClientSocket = -1;

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0)
        {
            dbg(TRACE,"accept socket failed!. errno:%d", errno);
            continue;
        }

        igClientSocket = newsockfd;

        while(1)
        {
            pthread_testcancel();

            memset(buffer, 0x00, XXL_BUFF);
            nbytes = read(newsockfd, buffer, XXL_BUFF);

            if (nbytes > 0)
            {
                buffer[nbytes] = 0;

                if(igChildCheckEndOfTelex)
                {
                    /* Child collect complete Telex and check into the message */
                    ChildCollectTELEX(buffer, (char *)inet_ntoa(cli_addr.sin_addr));
                }
                else
                {
                    /* Send to parent anyway */
                    ilRC = SendEvent("AFTN", mod_id, PRIORITY_3, "TCP","TCP", cgTabEnd,"TCP", (char *)inet_ntoa(cli_addr.sin_addr), buffer, NULL, 0);

                    if (ilRC != RC_SUCCESS)
                    {
                        dbg(TRACE, "AFTN TCP Listener failed to escalate data to <%s>.", mod_name);
                    }
                }
            }
            else
            {
                dbg(TRACE, "childReader: read() return:%d, errno: %d", nbytes, errno);
                break;
            }
        }

        close(newsockfd);
    } /* end of while */

    close(sockfd);

    dbg(TRACE, "AFTN thread TCP listening completed!");

    return (void *)0;
}

static void TerminateChild()
{
    close(igListenSocket);
    close(igClientSocket);

    pthread_cancel(ThreadID_sig);
    pthread_cancel(ThreadID_reader);
    pthread_cancel(ThreadID_writer);

    pthread_mutex_destroy(&WriteLock);
    pthread_mutex_destroy(&LastTelexMsgLock);
    pthread_cond_destroy(&WriteWait);
    pthread_attr_destroy(&ThreadAttribute);

    MtMsgLogRelease();

    sleep(1);

    exit(1);
}

static void *childWriter(void *arg)
{
    int    ilRc;
    MSGBUF * buffer;
    MSGBUF * next;
    int    oldState, oldType;
    char   sbuf[S_BUFF];

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

    while(1)
    {
        buffer = NULL;

        pthread_mutex_lock(&WriteLock);
        pthread_cond_wait(&WriteWait, &WriteLock);

        /* dbg(TRACE, "childWriter: waken");  */
        if((igClientSocket > 0) && (WriteBuf != NULL))
        {
            buffer = WriteBuf;
            WriteBuf = NULL;
        }
        pthread_mutex_unlock(&WriteLock);

repeat:
        while( buffer != NULL )
        {
            if((igClientSocket > 0) && (buffer->data))
            {
                /* dbg(TRACE, "childWriter: sending: %s", buffer->data); */
                ilRc = send(igClientSocket, buffer->data, strlen(buffer->data), 0);
                if(ilRc < 0)
                {
                    memset(sbuf, 0x0, sizeof(sbuf));
                    sprintf(sbuf, "ERROR: childWriter: send failure. errno: %d", errno);
                    MtMsgLog(sbuf, -1);
                }
                else
                {
                    MtMsgLog(buffer->data, WRITER);
                }
            }

            next = buffer->next;

            /* free MSGBUF */
            if(buffer->data)
                free(buffer->data);
            free(buffer);

            buffer = next;
        }
        /* Here buffer is NULL */

        pthread_mutex_lock(&WriteLock);
        if(WriteBuf != NULL)
        {
            buffer = WriteBuf;
            WriteBuf = NULL;
        }
        pthread_mutex_unlock(&WriteLock);

        if(buffer != NULL)
            goto repeat;
    }

    return (void *)0;
}

/*
 * put Msg to write buffer
 */
static int putWriteMsg(char * pcpData)
{
    int        ilRc = RC_SUCCESS;
    char *     msg = NULL;
    MSGBUF *   buf = NULL;

    msg = calloc(1, strlen(pcpData));
    if(NULL == msg)
    {
        MtMsgLog("ERROR: putWriteMsg: calloc msg fail!", -1);
        ilRc = RC_FAIL;
    }
    else
    {
        strcpy(msg, pcpData);
    }

    if(ilRc == RC_SUCCESS)
    {
        buf = calloc( 1, sizeof(MSGBUF));
        if(NULL == buf)
        {
            MtMsgLog("ERROR: putWriteMsg: calloc buf fail!", -1);
            ilRc = RC_FAIL;
            free(msg);
        }
    }

    if(ilRc == RC_SUCCESS)
    {
        pthread_mutex_lock(&WriteLock);

        buf->data = msg;
        buf->next = WriteBuf;
        WriteBuf = buf;

        pthread_mutex_unlock(&WriteLock);

        pthread_cond_signal(&WriteWait);
    }

    return ilRc;
}

static void *childSig(void *arg)
{
    sigset_t    set;
    int         sig;
    int         oldState;
    time_t      cur;
    struct tm  *_tm;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGALRM);

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);

    while(1)
    {
        sigwait(&set, &sig);
        /* dbg(TRACE, "childSig: Signal thread receive sig: %d", sig); */

        cur = time(NULL);

        if((sig == SIGUSR1) && (igEnable_CH_REPLY_MSG))
        {
            SendCHMsg();

            if((cur - tgLastRecCHTime) > igRecCH_Min_Buf*60)
            {
                if(tgLastRecCHTime == 0)
                {
                    /* Have not gotten first CH yet */
                    tgLastRecCHTime = cur - igRecCH_Min_Buf*60 - 1;
                }

                /* Save the expected CH time string */
                memset(pcgCH_EXP_TIME, 0x0, sizeof(pcgCH_EXP_TIME));
                if (igMSG_DATETIME_UTC)
                {
                    _tm = (struct tm *)gmtime(&cur);
                } else
                {
                    _tm = (struct tm *)localtime(&cur);
                }
                sprintf(pcgCH_EXP_TIME, "%02d%02d", _tm->tm_hour,_tm->tm_min);

                /* We haven't got incoming CH, set timer*/
                alarm( igRecCH_Min_Buf*60 );
            }

        }
        else if(sig == SIGALRM)
        {
            if((cur - tgLastRecCHTime) > igRecCH_Min_Buf*60 + 1)
            {
                /* Time out for incoming CH msg */
                SendCHMisMsg();
            }
        }
    }

    return (void *)0;
}

static int SendCHMsg()
{
    int         ilRc = RC_SUCCESS;
    char        msg[M_BUFF];
    char        sbuf[S_BUFF];

    memset(msg, 0x0, sizeof(msg));
    memset(sbuf, 0x0, sizeof(sbuf));

    /* Heading Line */
    ilRc = GenerateHeading(sbuf, NULL);
    sprintf(msg, "%c%s\x0d\x0a", CHAR_SOH, sbuf);

    /* Text */
    sprintf(msg, "%s%cCH\x0d\x0a", msg, CHAR_STX);

    /* End */
    sprintf(msg, "%s%c", msg, CHAR_ETX);

    putWriteMsg(msg);

    return ilRc;
}

static int SendCHMisMsg()
{
    int         ilRc = RC_SUCCESS;
    char        msg[M_BUFF];
    char        sbuf[S_BUFF];
    char*       cur;
    TELEXMSG    slTelex;

    memset(msg, 0x0, sizeof(msg));
    memset(sbuf, 0x0, sizeof(sbuf));

    /* parse last Telex msg */
    pthread_mutex_lock(&LastTelexMsgLock);

    memset(&slTelex, 0, sizeof(TELEXMSG));
    ilRc = ParseTelex(pcgLastTelexMsg, &slTelex);

    pthread_mutex_unlock(&LastTelexMsgLock);

    if(ilRc == RC_SUCCESS)
    {
        cur = msg;

        /* SOH */
        *cur = CHAR_SOH;
        cur++;

        /* Heading Line */
        ilRc = GenerateHeading(cur, slTelex.transId);
        while (*cur != '\0')
            cur++;

        sprintf(cur, "\x0d\x0a");
        cur += 2;

        /* Address */
        if(strlen(slTelex.prioId) > 0)
            sprintf(cur, "%s %s\x0d\x0a", slTelex.prioId, pcgSVC_ADDR_ID);
        else
            sprintf(cur, "%s %s\x0d\x0a", pcgDEFAULT_ADDR_PRIO_ID, pcgSVC_ADDR_ID);

        while (*cur != '\0')
            cur++;

        /* Origin */
        sprintf(cur, "%s %s\x0d\x0a", gcCURRENT_DT, pcgAFTNLSR_ORIG_ID);
        while (*cur != '\0')
            cur++;

        /* Text */
        sprintf(cur, "%cSVC MIS CH %s LR %s", CHAR_STX, pcgCH_EXP_TIME, slTelex.transId);
        while (*cur != '\0')
            cur++;

        /* End */
        sprintf(cur, "%c", CHAR_ETX);

        /* Send */
        putWriteMsg(msg);
    }

    if(slTelex.addresseeId)
        free(slTelex.addresseeId);

    return ilRc;

}

static void CreateAFTNtcpListener(int piPort)
{	
	int sockfd, newsockfd, clilen;
    char buffer[XL_BUFF];
	char bigBuff[XXXL_BUFF];
    struct sockaddr_in serv_addr, cli_addr;
    int  nbytes, ilRC;
	int optval;
#if defined(_HPUX)
	int optlen = sizeof(optval);
#else
    socklen_t optlen = sizeof(optval);
#endif
    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) 
    {
        dbg(TRACE,"Create tcp socket failed!.");
        exit(1);
    }

    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(piPort);
 
    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
         dbg(TRACE,"bind tcp socket failed!.");
		 close(sockfd);
         exit(1);
    }
	dbg(TRACE, "Child of <%s> TCP listening on port <%d>", mod_name, piPort);
    /* Now start listening for the clients, here 
     * process will go in sleep mode and will wait 
     * for the incoming connection
     */
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    while (1) 
    {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
        {
            dbg(TRACE,"accept socket failed!.");
			close(sockfd);
            exit(1);
        }
		memset(buffer, 0x00, XL_BUFF);
        /* nbytes = read(newsockfd, buffer, XXL_BUFF); */
		memset(bigBuff, 0x00, XXXL_BUFF);
		do
		{
			nbytes = read(newsockfd, buffer, XL_BUFF);
			buffer[nbytes] = 0;
			strcat(bigBuff, buffer);
		} while (nbytes > 0);	
		/* buffer[nbytes] = 0; */
		close(newsockfd);
		dbg(TRACE, "TCP Recieved from <%s> : <%s>", inet_ntoa(cli_addr.sin_addr), bigBuff);
		/* ilRC = SendCedaEvent(mod_id, 0, mod_name, "CEDA", "twStart", "twEnd", "OTR", "FIDTAB", "Selxxx", "CODE,BEMD,BEME", buffer, "abc", 5, RC_SUCCESS); */
		ilRC = SendEvent("AFTN", mod_id, PRIORITY_3, "TCP","TCP", cgTabEnd,"TCP", (char *)inet_ntoa(cli_addr.sin_addr), bigBuff, NULL, 0);
		if (ilRC != RC_SUCCESS)
		{
			dbg(TRACE, "AFTN TCP Listener failed to escalate data to <%s>.", mod_name);
		}
    } /* end of while */

	close(sockfd);
	
	dbg(TRACE, "AFTN Child TCP listening completed!");
	exit(0);
}

static void CreateAFTNudpListener(int piPort)
{
	struct sockaddr_in si_me, si_other;
    int sockfd, i, slen=sizeof(si_other), ilRC;
    char buffer[XXL_BUFF];

	if ((sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	{
		dbg(TRACE,"Create ucp socket failed!.");
        exit(1);
	}
    
	memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(piPort);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
    {   
         dbg(TRACE,"bind udp socket failed!.");
		 close(sockfd);
         exit(1);
    }
	dbg(TRACE, "Child of <%s> UDP listening on port <%d>", mod_name, piPort);
	while (1) 
    {
		memset(buffer, 0x00, XXL_BUFF);
		if (recvfrom(sockfd, buffer, XXL_BUFF, 0, (struct sockaddr *)&si_other, &slen)==-1)
		{
			dbg(TRACE,"Receive UDP failed!.");
			close(sockfd);
			exit(1);
		}

		dbg(TRACE, "UDP Recieved from <%s> : <%s>", inet_ntoa(si_other.sin_addr), buffer);
		/*
		ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                             ilPriority,RC_SUCCESS); */
		/*ilRC = SendCedaEvent(mod_id, 0, mod_name, "CEDA", "twStart", "twEnd", "OTR", "FIDTAB", "Selxxx", "CODE,BEMD,BEME", buffer, "abc", 5, RC_SUCCESS); */
		ilRC = SendEvent("AFTN", mod_id, PRIORITY_3, "UDP","UDP",cgTabEnd, "UDP", (char *)inet_ntoa(si_other.sin_addr), buffer, NULL, 0);
		if (ilRC != RC_SUCCESS)
		{
			dbg(TRACE, "UDP Listener failed to escalate data to <%s>.", mod_name);
		}
	}
	dbg(TRACE, "AFTN Child UDP listening completed!");
	close(sockfd);
	exit(0);
}

static void GetConfig(char* pcpFile,char* pcpSection,char* pcpTag,char* pcpTarget,char* pcpDefault)
{
  int ilRc = RC_FAIL;
  char pclTmp[L_BUFF];
  memset(pclTmp,0x00,L_BUFF);
  if ((ilRc = iGetConfigEntry(pcpFile,pcpSection,pcpTag,CFG_STRING,pclTmp)) == RC_SUCCESS)
    {
      strcpy(pcpTarget,pclTmp);
      if(igshow_config==TRUE)
	dbg(TRACE,"GetCfgEntry %s \tis <%s>.",pcpTag,pclTmp);
    }else{
      strcpy(pcpTarget,pcpDefault);
      if(igshow_config==TRUE)
	dbg(TRACE,"GetCfgEntry set %s \tto default<%s>.",pcpTag,pcpTarget);
    }
}

static void GetConfigFields(char* pcpFile, char* pcpSection, char* pcpTag, char* pcpTarget, char* pcpDefault)
{
    int ilRc = RC_FAIL;
    char pclTmp[L_BUFF];
    memset(pclTmp, 0x00, L_BUFF);
    if((ilRc = iGetConfigEntry(pcpFile, pcpSection, pcpTag, CFG_STRING, pclTmp)) == RC_SUCCESS)
    {
        strcpy(pcpTarget, pcpDefault);
        if(pclTmp[0] == ',')
        {
            strcat(pcpTarget, pclTmp);
        }
        else
        {
            strcat(pcpTarget, ",");
            strcat(pcpTarget, pclTmp);
            /* sprintf(pcpTarget,"%s,%s",pcpTarget,pclTmp); */
        }
        if(igshow_config == TRUE)
            dbg(TRACE, "GetCfgEntry %s \tis <%s>.", pcpTag, pclTmp);
    }
    else
    {
        strcpy(pcpTarget, pcpDefault);
        if(igshow_config == TRUE)
            dbg(TRACE, "GetCfgEntry set %s \tto default<%s>.", pcpTag, pcpTarget);
    }
}

static void GetConfigSwitch(char* pcpFile,char* pcpSection,char* pcpTag,int* piTarget,int piDefault)
{
  int ilRc = RC_FAIL;
  char pclTmp[L_BUFF];

  memset(pclTmp,0x00,L_BUFF);
  if(strstr(pcpTag,"MODE")!=NULL)
    {
      if ((ilRc = iGetConfigEntry(pcpFile,pcpSection,pcpTag,CFG_STRING,pclTmp)) == RC_SUCCESS)
	{
	  if(!strcmp(pclTmp,"DEBUG"))
	    {
	      *piTarget = DEBUG;
	    }else if(!strcmp(pclTmp,"TRACE")){
	      *piTarget = TRACE;
	    }else if(NULL!=strstr(pclTmp,"OF")){
	      *piTarget = 0;
	    }
	  dbg(TRACE,"GetCfgEntry %s is <%d>.",pcpTag,*piTarget);
	}else{
	  *piTarget = piDefault;
	  if(igshow_config==TRUE)
	    dbg(TRACE,"GetCfgEntry set %s to %d.",pcpTag,*piTarget);
	}
    }else{
      if ((ilRc = iGetConfigEntry(pcpFile,pcpSection,pcpTag,CFG_STRING,pclTmp)) == RC_SUCCESS)
	{
	  if(!strcmp(pclTmp,"ON")||!strcmp(pclTmp,"TRUE"))
	    {
	      *piTarget = TRUE;
	    }else{
	      *piTarget = FALSE;
	    }
	  dbg(TRACE,"GetCfgEntry %s is <%d>.",pcpTag,*piTarget);
	}else{
	  *piTarget = piDefault;
	  if(igshow_config==TRUE)
	    dbg(TRACE,"GetCfgEntry set %s to %d.",pcpTag,*piTarget);
	}
    }
}

static void GetConfigValue(char* pcpFile,char* pcpSection,char* pcpTag,int* piTarget,int piDefault)
{
  int ilRc = RC_FAIL;
  char pclTmp[L_BUFF];

  memset(pclTmp,0x00,L_BUFF);
  if ((ilRc = iGetConfigEntry(cgConfigFile,pcpSection,pcpTag,CFG_STRING,pclTmp)) == RC_SUCCESS)
    {
		*piTarget = (time_t)atoi(pclTmp);
		if(igshow_config==TRUE)
		dbg(TRACE,"GetCfgEntry %s \t\tis <%d>",pcpTag,*piTarget);
    }else{
      *piTarget = piDefault;
      if(igshow_config==TRUE)
	{
	  if(strlen(pcpTag)>10)
	    {
	      dbg(TRACE,"GetCfgEntry set %s \tto default<%d>",pcpTag,*piTarget);
	    }else{
	      dbg(TRACE,"GetCfgEntry set %s \t\tto default<%d>",pcpTag,*piTarget);
	    }
	}
    }
}
/*********************************************************************
Function : SendEvent()
Paramter : IN: pcpCmd = command for cmdblk->command
           IN: ipModID = process-ID where the event is send to
       IN: ipPriority = priority for sending ( 1- 5, usuallay 3)
       IN: pcpTable = Name (3 letters) of cmdblk->obj_name (if
           necessary), will be expanded with "pcgTabEnd".
       IN: pcpTwStart = cmdblk->twstart
       IN: pcpTwEnd = cmdblk->twend (always HOMEAP,TABEND,processname)
       IN: pcpSelection = selection for event (cmdblk->data)
       IN: pcpFields = fieldlist (corresponding to pcpdata)
       IN: pcpData = datalist (comma separated, corresponding to 
                                   pcpFields)
       IN: pcpAddStruct = additional structure to be transmitted
       IN: ipAddStructSize = size of the additional structure
Return Code: RC_SUCCESS, RC_FAIL
Result:
Description: Sends an event to another CEDA-process using que(QUE_PUT).
             Sends the event in standard CEDA-format (BCHEAD,CMDBLK,
         selection,fieldlist,datalist) or sends a different
         data structure (special) at CMDBLK->data. !! Sends always
         only one type, standard OR special, with one event !!
*********************************************************************/
static int SendEvent(char *pcpCmd,int ipModID,int ipPriority,char *pcpTable,
             char *pcpTwStart, char *pcpTwEnd,
             char *pcpSelection,char *pcpFields,char *pcpData,
             char *pcpAddStruct,int ipAddStructSize)
{
  int     ilRc             = RC_FAIL;
  int     ilLen            = 0;
  EVENT   *prlOutEvent  = NULL;
  BC_HEAD *prlOutBCHead = NULL;
  CMDBLK  *prlOutCmdblk = NULL;

  if (pcpAddStruct == NULL)
    ipAddStructSize = 0;
  if (ipAddStructSize == 0)
    pcpAddStruct = NULL;

  /* size-calculation for prlOutEvent */
  ilLen = sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + 
    strlen(pcpSelection) + strlen(pcpFields) + strlen(pcpData) + 
    ipAddStructSize + 128;

  /* memory for prlOutEvent */
  if ((prlOutEvent = (EVENT*)malloc((size_t)ilLen)) == NULL)
    {
      dbg(TRACE,"SendEvent: cannot malloc <%d>-bytes for outgoing event!",ilLen);
      prlOutEvent = NULL;
    }else{
      /* clear whole outgoing event */
      memset((void*)prlOutEvent, 0x00, ilLen);

      /* set event structure... */
      prlOutEvent->type         = SYS_EVENT;
      prlOutEvent->command    = EVENT_DATA;
      prlOutEvent->originator   = (short)mod_id;
      prlOutEvent->retry_count  = 0;
      prlOutEvent->data_offset  = sizeof(EVENT);
      prlOutEvent->data_length  = ilLen - sizeof(EVENT); 

      /* BC_HEAD-Structure... */
      prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
      /* prlOutBCHead->rc = (short)RC_SUCCESS;*/
      prlOutBCHead->rc = (short)NETOUT_NO_ACK;/*spaeter nur bei disconnect*/
      strncpy(prlOutBCHead->dest_name,mod_name,10);
      strncpy(prlOutBCHead->recv_name, "FIDS",10);
 
      /* Cmdblk-Structure... */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,pcpCmd);
      if (pcpTable != NULL)
    {
      strcpy(prlOutCmdblk->obj_name,pcpTable);
      strcat(prlOutCmdblk->obj_name,cgTabEnd);
    }
        
      /* setting tw_x entries */
      strncpy(prlOutCmdblk->tw_start,pcpTwStart,32);
      strncpy(prlOutCmdblk->tw_end,pcpTwEnd,32);
        
      /* means that no additional structure is used */
      /* STANDARD CEDA-ipcs between CEDA-processes */
      if (pcpAddStruct == NULL)
    {
      /* setting selection inside event */
      strcpy(prlOutCmdblk->data,pcpSelection);
      /* setting field-list inside event */
      strcpy(prlOutCmdblk->data+strlen(pcpSelection)+1,pcpFields);
      /* setting data-list inside event */
      strcpy((prlOutCmdblk->data + (strlen(pcpSelection)+1) + (strlen(pcpFields)+1)),pcpData);
    }else{
      /*an additional structure is used and will be copied to */
      /*cmdblk + sizeof(CMDBLK).!!! No STANDARD CEDA-ipcs is used !!!! */
      memcpy(prlOutCmdblk->data,(char*)pcpAddStruct,ipAddStructSize);    
    }

      /*DebugPrintEvent(DEBUG,prlOutEvent);*/ 
      /*snapit((char*)prlOutEvent,ilLen,outp);*/
      dbg(DEBUG,"SendEvent: sending event to mod_id <%d>",ipModID);

      if (ipModID != 0)
    {
      if ((ilRc = que(QUE_PUT,ipModID,mod_id,ipPriority,ilLen,(char*)prlOutEvent))
          != RC_SUCCESS)
        {
          dbg(TRACE,"SendEvent: QUE_PUT returns: <%d>", ilRc);
          Terminate(1);
        }
    }else{
      dbg(TRACE,"SendEvent: mod_id = <%d>! Can't send!",ipModID);
    }
      /* free memory */
      free((void*)prlOutEvent); 
    }
  return ilRc;
} /* end of SendEvent*/

static void SendSocket(char *lsTmpIP, char *pData)
{
	dbg(TRACE,"Send by %s to <%s>", pcgMainProtocol, lsTmpIP);
	dbg(TRACE,"Data <%s>", pData);
    if (strncmp("TCP", pcgMainProtocol, 3) == 0)
	{
		SendTCPMessage(lsTmpIP, giMonitorPort, pData);		
	} else 
	{
		SendUDPMessage(lsTmpIP, giMonitorPort, pData);
	} 
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

static int SendTCPMessage(char *pIP, int pPort, char *pMessage)
{
	int sockfd;
	struct sockaddr_in their_addr;
	struct hostent *he;
	int numbytes;
	struct timeval timeout;

	timeout.tv_sec = igTCPTimeout;
	timeout.tv_usec = 200000;
	fd_set fdset;
	int so_error;
#if defined(_HPUX)
	int len = sizeof so_error;
#else
    socklen_t len = sizeof so_error;
#endif
	if ((he = gethostbyname(pIP)) == NULL)
	{
		dbg(TRACE,"SendTCPMessage: gethostbyname error"); 
		return RC_FAIL;
	}

	if((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		dbg(TRACE,"SendTCPMessage: create socket error");
		return RC_FAIL;
		return;
	}
	
	their_addr.sin_family = AF_INET;
	/* short, network byte order */
	their_addr.sin_port = htons(pPort);
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);

	/* zero the rest of the struct */
	memset(&(their_addr.sin_zero), '\0', 8);
	
#if defined(_HPUX) || defined(_SOLARIS) || defined(_AIX)
		/* Finding solution */
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
	{
		dbg(TRACE,"SendTCPMessage: set non blocking (fcntl) error <%s>", pIP);
		close(sockfd);
		return RC_FAIL;
	}

	connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
	/*
	if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) < 0)
	{
		dbg(TRACE,"SendTCPMessage: connect socket error <%s>", pIP);
		close(sockfd);
		return;
    } */
	FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
	if (select(sockfd + 1, NULL, &fdset, NULL, &timeout) == 1)
    {        
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1)
		{
			dbg(TRACE,"SendTCPMessage: getsockopt error");
			close(sockfd);
			return RC_FAIL;
		}

        if (so_error != 0) {
            dbg(TRACE,"SendTCPMessage: Timeout to %s", pIP);
			close(sockfd);
			return RC_FAIL;
        }
    } else
	{
		dbg(TRACE,"SendTCPMessage: Timeout to %s", pIP);
		close(sockfd);
		return RC_FAIL;
	}

#else 
	if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1)
	{
		dbg(TRACE,"SendTCPMessage: Set socket timeout error");
		close(sockfd);
		return RC_FAIL;
	}		
    if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) < 0)
	{
		dbg(TRACE,"SendTCPMessage: connect socket error <%s>", pIP);
		close(sockfd);
		return RC_FAIL;
    }
#endif
	/* if((numbytes = sendto(sockfd, pMessage, strlen(pMessage), 0, (struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) */
	if ((numbytes = send(sockfd, pMessage, strlen(pMessage), 0)) == -1)
	{
		dbg(TRACE,"SendTCPMessage: Send Message error"); 
		close(sockfd);
		return RC_FAIL;
	}
	close(sockfd);
	return RC_SUCCESS;
}

/*
 * Child process collect a complete Telex msg and handle it
 */
static void ChildCollectTELEX(char *pcpData, char *pcpIp)
{
	char               *s;
	int                posETX;
	char               buffer[XXL_BUFF];

	s = strchr (pcpData, gcEndOfTelex);

	if (s == NULL)
	{
		strcat(pcgAFTNBuffer, pcpData);
	} else
	{
		memset(buffer, 0x00, sizeof(buffer));

		posETX = s - pcpData + 1;
		strncpy(buffer, pcpData, posETX);
		strcat(pcgAFTNBuffer, buffer);	

        /* sprintf(mbuff, "From: <%s>, size: %d", pcpIp, strlen(pcgAFTNBuffer));   */
        /* MtMsgLog(mbuff, -1);  */
        MtMsgLog(pcgAFTNBuffer, READER);

		ChildHandleTelex(pcgAFTNBuffer, pcpIp);

		memset(pcgAFTNBuffer, 0x00, sizeof(pcgAFTNBuffer));
		if (posETX < strlen(pcpData))
		{
		    ChildCollectTELEX(&(pcpData[posETX]), pcpIp);
		}
	}		
}

/*
 * Handle a complete Telex msg
 */
static void ChildHandleTelex(char * pcpData, char * pcpIp)
{
    int               ilRc;
    BOOL              isCHMsg;
    BOOL              isSVCMsg;
    TELEXMSG          slTelex;

    memset(&slTelex, 0, sizeof(TELEXMSG));

    /* save TelexMsg */
    pthread_mutex_lock(&LastTelexMsgLock);
    if(pcgLastTelexMsg)
    {
        free(pcgLastTelexMsg);
        pcgLastTelexMsg = NULL;
    }

    if((pcgLastTelexMsg = calloc(1, strlen(pcpData)+1)) == NULL)
    {
        MtMsgLog("ERROR: ChildHandleTelex: calloc fail!", -1);
        ilRc = RC_FAIL;
    }
    else
    {
        strncpy(pcgLastTelexMsg, pcpData, strlen(pcpData));
    }
    pthread_mutex_unlock(&LastTelexMsgLock);


    if(ilRc == RC_SUCCESS)
    {
        ilRc = ParseTelex(pcpData, &slTelex);
    }

    /* Check channel sequence num */
    if(ilRc == RC_SUCCESS)
    {
        CheckSequenceNum(&slTelex);
    }

    /* Check if this's CH msg */
    if(ilRc == RC_SUCCESS)
    {
        isCHMsg = CheckCHMsg(&slTelex);
    }

    /* Check if this's SVC msg */
    if(ilRc == RC_SUCCESS)
    {
        isSVCMsg = CheckSVCMsg(&slTelex);
    }

    /* Check Prio Id */
    if(ilRc == RC_SUCCESS)
    {
        ilRc = CheckPrioId(&slTelex);
    }

    if(slTelex.addresseeId)
    {
        free(slTelex.addresseeId);
    }

    /* Send to parent process except CH/SVC msg */
    if( ! isCHMsg && ! isSVCMsg)
    {
        ilRc = SendEvent("AFTN", mod_id, PRIORITY_3, "TCP", "TCP", cgTabEnd, "TCP", pcpIp, pcpData, NULL, 0);

        if(ilRc != RC_SUCCESS)
        {
            MtMsgLog("ERROR: ChildHandleTelex: failed to escalate data to", -1);
        }
    }
}

static BOOL CheckCHMsg(TELEXMSG * pspTelex)
{
    BOOL          isCHMsg = FALSE;
    char          sbuf[S_BUFF];

    memset(sbuf, 0x0, sizeof(sbuf));
    strncpy(sbuf, pspTelex->text, 4);

    if( strncmp(sbuf, "CH\x0d\x0a", 4) == 0)
    {
        isCHMsg = TRUE;
        tgLastRecCHTime = time(NULL);
    }

    dbg(TRACE, "CheckCHMsg: isCHMsg <%d>", isCHMsg);

    return isCHMsg;
}

static BOOL CheckSVCMsg(TELEXMSG * pspTelex)
{
    BOOL          isSVCMsg = FALSE;
    char          sbuf[S_BUFF];

    memset(sbuf, 0x0, sizeof(sbuf));
    strncpy(sbuf, pspTelex->text, 4);

    if( strncmp(sbuf, "SVC ", 4) == 0)
    {
        isSVCMsg = TRUE;
        /* HandleSVC(pspTelex);   */
    }

    dbg(TRACE, "CheckCHMsg: isSVCMsg <%d>", isSVCMsg);

    return isSVCMsg;
}

/*
 * Child handle SVC msg logic
 */
static int HandleSVC(TELEXMSG * pspTelex)
{
    int           ilRc = RC_SUCCESS;
    int           ili;
    char          msgbuf[S_BUFF];
    char          numbuf[VXS_BUFF];
    char         *cur;
    char         *end;

    memset(msgbuf, 0x0, sizeof(msgbuf));
    if( (cur = strstr(pspTelex->text, "\x0d\x0a")) == NULL)
    {
        dbg(TRACE, "ERROR: HandleSVC: can not find CR/LF");
        return RC_FAIL;
    }

    strncpy(msgbuf, pspTelex->text, (size_t)(cur - pspTelex->text));

    /* EXP */
    if( (cur = strstr(msgbuf, "EXP")) )
    {
        while( ! isdigit(*cur)) cur++;

        end = cur;
        while( isdigit(*end) ) end++;

        memset(numbuf, 0x0, sizeof(numbuf));
        strncpy(numbuf, cur, (size_t)(end - cur));
        igTRC_SEQ = atoi(numbuf) - 1;
        dbg(TRACE, "HandleSVC: EXP: igTRC_SEQ changed to %d", igTRC_SEQ);
    }

    return ilRc;
}

static int CheckPrioId(TELEXMSG *pspTelex)
{
    int        ilRc = RC_SUCCESS;

    if(strncmp(pspTelex->prioId, "SS", 2) == 0)
    {
        SendSSAck( pspTelex );
    }

    return ilRc;
}

/*
 * Parse Telex msg parts before text and fill TELEXMSG structure
 */
static int ParseTelex(char* pcpData, TELEXMSG *pspTelex)
{
    int               ilRc = RC_SUCCESS;
    char*             pclCur;
    char*             pclTmp;

    if(ilRc == RC_SUCCESS)
    {
        if((pclCur = strchr(pcpData, CHAR_SOH)) == NULL)
            ilRc = RC_FAIL;
        else
            pspTelex->heading = ++pclCur;
    }

    /* Transmission Id */
    if(ilRc == RC_SUCCESS)
    {
        pclTmp = pclCur;
        while( isalnum(*pclTmp))
            pclTmp++;
        strncpy(pspTelex->transId, pclCur, (size_t)(pclTmp-pclCur));
        pclCur = pclTmp;
    }

    /* Additional Service Id */
    if((ilRc == RC_SUCCESS) && isspace(*pclCur))
    {
        pclCur++;
        pclTmp = pclCur;
        while( isalnum(*pclTmp))
            pclTmp++;
        strncpy(pspTelex->addservId, pclCur, (size_t)(pclTmp-pclCur));
        pclCur = pclTmp;
    }

    /* Skip CR/LF */
    if(ilRc == RC_SUCCESS)
    {
        pclCur = strstr(pclCur, "\x0d\x0a");

        if(pclCur == NULL)
            ilRc = RC_FAIL;
        else
            pclCur += 2;
    }

    /* Address */
    if((ilRc == RC_SUCCESS) && isalpha(*pclCur))
    {
        /* Priority Id */
        pclTmp = pclCur + 2;
        if( ! isspace(*pclTmp) )
            ilRc = RC_FAIL;           /* Not two letter priority Id  */
        else
        {
            pspTelex->address = pclCur;
            strncpy(pspTelex->prioId, pclCur, 2);
            pclCur = pclTmp + 1;

            /* Addressee Id*/
            pclTmp = strstr(pclCur, "\x0d\x0a");
            if(pclTmp == NULL)
                ilRc = RC_FAIL;
            else
            {
                pspTelex->addresseeId = calloc(1, (size_t)(pclTmp-pclCur + 1));
                if(pspTelex->addresseeId == NULL)
                {
                    MtMsgLog("ERROR: ParseTelex: calloc fail!", -1);
                    ilRc = RC_FAIL;
                }
                else
                {
                    strncpy(pspTelex->addresseeId, pclCur, (size_t)(pclTmp-pclCur));
                    pclCur = pclTmp + 2;    /* skip CR/LF */
                }
            }
        }
    }

    /* Origin */
    if((ilRc == RC_SUCCESS) && isdigit(*pclCur))
    {
        pclTmp = pclCur;
        while( isdigit(*pclTmp) )
            pclTmp++;

        if( ! isspace(*pclTmp) )
            ilRc = RC_FAIL;
        else
        {
            pspTelex->origin = pclCur;
            strncpy(pspTelex->filingtime, pclCur, (size_t)(pclTmp-pclCur));
            pclTmp++;
            pclCur = pclTmp;
            while( isalpha(*pclTmp) )
                pclTmp++;

            /* Originator Id */
            strncpy(pspTelex->origId, pclCur, (size_t)(pclTmp-pclCur));

            pclCur = strstr(pclTmp, "\x0d\x0a");
            pclCur += 2;             /* skip CR/LF */
        }
    }

    if(ilRc == RC_SUCCESS)
    {
        pclCur = strchr(pclCur, CHAR_STX);

        if(pclCur == NULL)
            ilRc = RC_FAIL;
        else
        {
            pspTelex->text = pclCur + 1;
        }
    }

    return ilRc;
}

static int CheckSequenceNum(TELEXMSG * pspTelex)
{
    int            ilSeqNum;
    int            ili;
    int            ilRc = RC_SUCCESS;
    char           sbuf[S_BUFF];
    char*          pclBegin;
    char*          pclEnd;

    ilSeqNum = GetSequenceNum(pspTelex->transId);
    /* dbg(TRACE, "CheckSequencNum: Last:<%d>, Current:<%d>", igLastSeqNum, ilSeqNum);   */

    if(igLastSeqNum < 0)
        igLastSeqNum = ilSeqNum;
    else
    {
        if( ilSeqNum != (igLastSeqNum + 1))
        {
            /* dbg(TRACE, "CheckSequencNum: Found Sequence number issue."); */

            if((ilSeqNum == 1) && isNowMidNight())
            {
                igLastSeqNum = ilSeqNum;
            }
            else if(ilSeqNum > (igLastSeqNum + 1))
            {
                ilRc = RC_FAIL;
                SendSVCMIS(igLastSeqNum, ilSeqNum, pspTelex);
                igLastSeqNum = ilSeqNum;
            }
            else if(ilSeqNum < (igLastSeqNum + 1))
            {
                ilRc = RC_FAIL;
                SendSVCEXP(igLastSeqNum, ilSeqNum, pspTelex);
                igLastSeqNum = ilSeqNum;
            }
        }
        else
        {
            igLastSeqNum = ilSeqNum;
        }
    }

    return ilRc;
}

/*
 * Check if current time is around midnight
 */
static BOOL isNowMidNight()
{
    BOOL         isNMN = FALSE;
    struct tm * _tm;

    time_t t = time(NULL);
    if (igMSG_DATETIME_UTC)
    {
        _tm = (struct tm *)gmtime(&t);
    } else
    {
        _tm = (struct tm *)localtime(&t);
    }

    if(
            ((_tm->tm_hour == 23) && ((59 - _tm->tm_min) < igMidNight_Min_Buf)) ||
            ((_tm->tm_hour == 0) && (_tm->tm_min < igMidNight_Min_Buf))
      )
    {
        isNMN = TRUE;
    }

    return isNMN;
}


/* input: E.g. YFA0001 */
static int GetSequenceNum(char* pcpData)
{
    int        num = -1;
    char       sbuf[S_BUFF];
    char*      pclTmp;

    pclTmp = pcpData;
    memset(sbuf, 0x0, sizeof(sbuf));

    while( ! isdigit(*pclTmp) )
        pclTmp++;

    strcpy(sbuf, pclTmp);
    num = atoi(sbuf);

    return num;
}

static int SendSSAck(TELEXMSG *pspTelex)
{
    int         ilRc = RC_SUCCESS;
    char        msg[M_BUFF];
    char        sbuf[S_BUFF];
    char*       cur;

    memset(msg, 0x0, sizeof(msg));
    memset(sbuf, 0x0, sizeof(sbuf));
    cur = msg;

    /* SOH */
    *cur = CHAR_SOH;
    cur++;

    /* Heading Line */
    ilRc = GenerateHeading(cur, pspTelex->transId);
    while(*cur != '\0') cur++;

    sprintf(cur, "\x0d\x0a");
    cur += 2;

    /* Address */
    sprintf(cur, "%s %s\x0d\x0a", "SS", pspTelex->origId);

    while(*cur != '\0') cur++;

    /* Origin */
    sprintf(cur, "%s %s%c%c%c%c%c\x0d\x0a", gcCURRENT_DT, pcgAFTNLSR_ORIG_ID, CHAR_BEL, CHAR_BEL, CHAR_BEL, CHAR_BEL, CHAR_BEL);
    while(*cur != '\0') cur++;

    /* Text */
    sprintf(cur, "%cR %s %s\x0d\x0a", CHAR_STX, pspTelex->addservId, pspTelex->origId);
    while(*cur != '\0') cur++;

    /* End */
    sprintf(cur, "%c", CHAR_ETX);

    /* Send */
    putWriteMsg(msg);

    return ilRc;
}

/*
 * For the case (ipLastSeqNum+1) < ipSeqNum
 */
static int SendSVCMIS(int ipLastSeqNum, int ipSeqNum, TELEXMSG * pspTelex)
{
    int         ilRc = RC_SUCCESS;
    char        msg[M_BUFF];
    char        sbuf[S_BUFF];
    char*       cur;

    memset(msg, 0x0, sizeof(msg));
    memset(sbuf, 0x0, sizeof(sbuf));
    cur = msg;

    /* SOH */
    *cur = CHAR_SOH;
    cur++;

    /* Heading Line */
    ilRc = GenerateHeading(cur, pspTelex->transId);
    while(*cur != '\0') cur++;

    sprintf(cur, "\x0d\x0a");
    cur += 2;

    /* Address */
    if(strlen(pspTelex->prioId) > 0)
        sprintf(cur, "%s %s\x0d\x0a", pspTelex->prioId, pcgSVC_ADDR_ID);
    else
        sprintf(cur, "%s %s\x0d\x0a", pcgDEFAULT_ADDR_PRIO_ID, pcgSVC_ADDR_ID);

    while(*cur != '\0') cur++;

    /* Origin */
    sprintf(cur, "%s %s\x0d\x0a", gcCURRENT_DT, pcgAFTNLSR_ORIG_ID);
    while(*cur != '\0') cur++;

    /* Text */
    sprintf(cur, "%cSVC QTA MIS ", CHAR_STX);
    while(*cur != '\0') cur++;

    strncpy(cur, pspTelex->transId, 3);
    while(*cur != '\0') cur++;

    if( ipSeqNum > (ipLastSeqNum + 2) )
        sprintf(cur, "%04d-%04d\x0d\x0a", ipLastSeqNum+1, ipSeqNum-1);
    else
        sprintf(cur, "%04d\x0d\x0a", ipLastSeqNum+1);

    while(*cur != '\0') cur++;

    /* End */
    sprintf(cur, "%c", CHAR_ETX);

    /* Send */
    putWriteMsg(msg);

    return ilRc;
}

/*
 * Output: pcpRes. E.g. 'YFA0001 270628'
 */
static int GenerateHeading(char* pcpRes, char* pcpRecTransId)
{
    int         ilRc = RC_SUCCESS;
    struct tm * _tm;
    char        currDD[8];
    char        lastDD[8];

    time_t t = time(NULL);
    if (igMSG_DATETIME_UTC)
    {
        _tm = (struct tm *)gmtime(&t);
    } else
    {
        _tm = (struct tm *)localtime(&t);
    }

    memset(currDD, 0x0, sizeof(currDD));
    memset(lastDD, 0x0, sizeof(lastDD));
    sprintf(currDD, "%02d", _tm->tm_mday);
    strncpy(lastDD, gcCURRENT_DT, 2);

    if(strncmp(currDD, lastDD, 2) == 0)
    {
        igTRC_SEQ++;
    }
    else    /*  Day change. Reset counter  */
    {
        igTRC_SEQ = 1;
    }

    sprintf(gcCURRENT_DT,"%02d%02d%02d", _tm->tm_mday, _tm->tm_hour,_tm->tm_min);

    sprintf(pcpRes, "%s%04d %s", pcgTRC_DESIGNATED_TEXT, igTRC_SEQ, gcCURRENT_DT);

    return ilRc;
}

/*
 * For the case ipLastSeqNum+1 > ipSeqNum
 */
static int SendSVCEXP(int ipLastSeqNum, int ipSeqNum, TELEXMSG * pspTelex)
{
    int         ilRc = RC_SUCCESS;
    char        msg[M_BUFF];
    char        sbuf[S_BUFF];
    char*       cur;

    memset(msg, 0x0, sizeof(msg));
    memset(sbuf, 0x0, sizeof(sbuf));

    cur = msg;

    /* SON */
    *cur = CHAR_SOH;
    cur++;

    /* Heading Line */
    ilRc = GenerateHeading(cur, pspTelex->transId);
    while(*cur != '\0') cur++;

    sprintf(cur, "\x0d\x0a");
    while(*cur != '\0') cur++;

    /* Address */
    if(strlen(pspTelex->prioId) > 0)
        sprintf(cur, "%s %s\x0d\x0a", pspTelex->prioId, pcgSVC_ADDR_ID);
    else
        sprintf(cur, "%s %s\x0d\x0a", pcgDEFAULT_ADDR_PRIO_ID, pcgSVC_ADDR_ID);

    while(*cur != '\0') cur++;


    /* Origin */
    sprintf(cur, "%s %s\x0d\x0a", gcCURRENT_DT, pcgAFTNLSR_ORIG_ID);
    while(*cur != '\0') cur++;

    /* Text */
    memset(sbuf, 0x0, sizeof(sbuf));
    strncpy(sbuf, pspTelex->transId, 3);
    sprintf(cur, "%cSVC LR %s%04d EXP %s%04d\x0d\x0a", CHAR_STX, sbuf, ipSeqNum, sbuf, ipLastSeqNum+1);
    while(*cur != '\0') cur++;

    /* End */
    sprintf(cur, "%c", CHAR_ETX);

    /* Send */
    putWriteMsg(msg);

    return ilRc;

}

/*
 * Called by parent process
 */
static void ProcessAFTNRequest(char * pIP, char *pcpData)
{
	dbg(DEBUG, "ProcessAFTNRequest : <%s> from <%s>", pcpData, pIP);
	if (igParentCheckEndOfTelex)
	{
		dbg(TRACE," Parent collecting and merge part of Telex <%s>", pcpData);
		ParentCollectTELEX(pcpData);
	} else
	{
		UpdateAFTNRecord(pcpData);
	}

	/*
	if (strncmp("DBU", pcpData, 3) == 0)    // DB Update
	{
	    UpdateRecord(pcpData);
	} else if (strncmp("DBR", pcpData, 3) == 0) // DB Read
	{
	    ReadRecord(pIP, pcpData);
	} else
	{
	    dbg(TRACE, "Unkhown command. Do nothing.");
	} */

}

/*
 * Parent collect a complete Telex and send to FDIHDL
 */
static void ParentCollectTELEX(char *pcpData)
{
    char *s;
    int posETX;
    char buffer[XXL_BUFF];

    s = strchr (pcpData, gcEndOfTelex);

    if (s == NULL)
    {
        strcat(pcgAFTNBuffer, pcpData);
    } else
    {
        memset(buffer, 0x00, sizeof(buffer));

        posETX = s - pcpData + 1;
        strncpy(buffer, pcpData, posETX);
        strcat(pcgAFTNBuffer, buffer);
        dbg(TRACE,"Found END OF TELEX <%s>", pcgAFTNBuffer);

        UpdateAFTNRecord(pcgAFTNBuffer);

        memset(pcgAFTNBuffer, 0x00, sizeof(pcgAFTNBuffer));
        if (posETX < strlen(pcpData))
        {
            ParentCollectTELEX(&(pcpData[posETX]));
        }
    }
}

/* Send Telex to FDIHDL */
static void UpdateAFTNRecord(char *pcpData)
{
	char clCmd[VXS_BUFF];
	char clTable[VXS_BUFF];
	char clFields[XS_BUFF];
	char clSelection[XS_BUFF];
	int  ilRC; /* que_id = 8450;  */
		
	memset(clCmd, 0x00, VXS_BUFF);
	memset(clTable, 0x00, VXS_BUFF);	
	memset(clFields, 0x00, XS_BUFF);
	memset(clSelection, 0x00, XS_BUFF);
	
	strcpy(clCmd, "AFTN");
	strcpy(clTable, "TLX");
	strcpy(clFields, "");
	strcpy(clSelection, "");
	ilRC = SendEvent(clCmd, giFDI_Que_Id, PRIORITY_4,clTable,"", pcgTwEnd,clSelection,
                      clFields,pcpData,NULL,0);
					  if (ilRC != RC_SUCCESS)
	{
		dbg(TRACE, "UpdateAFTNRecord : failed <%s><%s><%s><%s>.", clTable, clFields, pcpData, clSelection);
	} else
	{
		dbg(TRACE, "Forwarded to FDIHDL on QUEUE <%d>.", giFDI_Que_Id);
	}
}

static void UpdateRecord(char *pcpData)
{
	char clTable[VXS_BUFF];
	char clCmd[VXS_BUFF];
	char clFields[XS_BUFF];
	char clData[XS_BUFF];
	char clSelection[XS_BUFF];
	char pclNowTime[16] = "\0";
	int  que_id =0, ilRC;
	
	dbg(TRACE, "UpdateRecord : Processing <%s>", pcpData);
	memset(clCmd, 0x00, VXS_BUFF);
	memset(clTable, 0x00, VXS_BUFF);	
	memset(clFields, 0x00, XS_BUFF);
	memset(clData, 0x00, XS_BUFF);
	memset(clSelection, 0x00, XS_BUFF);
	
	GetDataItem(clTable,pcpData,2,'|',"","\0 ");
	GetDataItem(clFields,pcpData,3,'|',"","\0 ");
	GetDataItem(clData,pcpData,4,'|'," ","\0");
	GetDataItem(clSelection,pcpData,5,'|',"", "\0 ");
	
	
	if (strncmp(clTable, "AFT", 3) == 0)
	{
		strcpy(clCmd, "UFR");
		clTable[3] = '\0';
		que_id = 7800;
	} else  /* For CCA,ALT,APT etc. */
	{
		strcpy(clCmd, "URT");
		clTable[3] = '\0';
		que_id = 506;
	} 

	if (strlen(clCmd) > 0)
	{
		if (strstr(clData, "{CEDA_DATETIME_UTC}") != NULL)
		{
			TimeToStr(pclNowTime,time(NULL));
			do
			{
				SearchStringAndReplace(clData, "{CEDA_DATETIME_UTC}", pclNowTime);
			} while (strstr(clData, "{CEDA_DATETIME_UTC}") != NULL);
		}
		ilRC = SendEvent(clCmd, que_id, PRIORITY_3,clTable,"", pcgTwEnd,clSelection,
                      clFields,clData,NULL,0);
					  if (ilRC != RC_SUCCESS)
		{
			dbg(TRACE, "UpdateRecord : failed <%s><%s><%s><%s>.", clTable, clFields, clData, clSelection);
		}
	} else
	{
		dbg(TRACE, "Invalid Table name. Do nothing.");
	}
}

static void ReadRecord(char *pIP, char *pcpData)
{
	/* char clType[VXS_BUFF];   */
	char clPage[VXS_BUFF];
	char clObject[VXS_BUFF];
	char clTable[VXS_BUFF];
	char clFields[XS_BUFF];	
	char clSelection[XS_BUFF];
	int ili, ilFldCount=0;
	char clchar='x';
	char clData[L_BUFF];
	char clReturnData[XXL_BUFF];
	/* char clCmd[VXS_BUFF]; */
	/* char clData[XS_BUFF];  */
	dbg(TRACE, "ReadRecord : Processing <%s> from <%s>", pcpData, pIP);
	
	/* memset(clType, 0x00, VXS_BUFF);  */
	memset(clPage, 0x00, VXS_BUFF);
	memset(clObject, 0x00, VXS_BUFF);
	memset(clTable, 0x00, VXS_BUFF);	
	memset(clFields, 0x00, XS_BUFF);
	memset(clSelection, 0x00, XS_BUFF);
	memset(clData, 0x00, L_BUFF);	

	/* 1  ;  2    ;   3     ;  4  ;  5    ;  6     ;   7    ;    */
	/* DBR;PAGE_ID;OBJECT_ID;TABLE;FIELDS;SELECTION;   */
	/* GetDataItem(clType,pcpData,2,';'," ","\0 ");     */
	GetDataItem(clPage,pcpData,2,';'," ","\0 ");
	GetDataItem(clObject,pcpData,3,';'," ","\0 ");
	GetDataItem(clTable,pcpData,4,';'," ","\0 ");
	GetDataItem(clFields,pcpData,5,';'," ","\0 ");
	GetDataItem(clSelection,pcpData,6,';'," ", "\0 ");
	
	/* dbg(TRACE, "ReadRecord : Page <%s> Object <%s> Table <%s> Fields <%s> Selection<%s>", clPage, clObject, clTable, clFields, clSelection); */
	
	/* Counting fields  */
	if (strlen(clFields) > 0 || clFields[0] != ' ')
	{
		ili = 0;
		while (clchar != '\0')
		{
			clchar = clFields[ili];
			ili++;
			if (clchar == ',')
			{
			 ilFldCount++;
			}
		}
		ilFldCount++;
	}
	
	if (igMaxReqFields < ilFldCount)
	{
		dbg(TRACE, "Request fields <%d> is exceed buffer limit <%d>.", ilFldCount, igMaxReqFields);
		return;
	}
	for (ili=0; ili < ilFldCount; ili++)
	{	
		memset(pagReqFields[ili], 0x00, 10);
		GetDataItem(pagReqFields[ili],clFields,ili+1,','," "," ");
	}
	/*
	dbg(TRACE, "Found %d fields.", ilFldCount);
	for (ili=0; ili < ilFldCount; ili++)
	{	
		dbg(TRACE, "<%d> <%s>", ili+1, pagReqFields[ili]);
	} */
	if ((strlen(clTable) > 2) && (strlen(clFields) > 3) && (strlen(clSelection)>5))
	{
		GetRecordFromDB(clTable, clFields, clSelection, ilFldCount, clData);
		/* dbg(TRACE, "Return Data <%s>", clData);   */
		memset(clReturnData, 0x00, XXL_BUFF);
		sprintf(clReturnData, "DATA;REQUEST_XML;%s\n", clPage);
		strcat(clReturnData, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>");
		strcat(clReturnData,"<XML_DATA>");
		strcat(clReturnData,"<OBJECT_ID>");
		strcat(clReturnData, clObject);	
		strcat(clReturnData,"</OBJECT_ID>");
		/*
		strcat(clReturnData,"<FIELDS>");
		strcat(clReturnData, clFields);	
		strcat(clReturnData,"</FIELDS>");
		*/
		strcat(clReturnData, clData);
		strcat(clReturnData,"</XML_DATA>");
		SendSocket(pIP, clReturnData);
	}
}

static void GetRecordFromDB(char *pTable, char *pFields, char *pSelection, int pFldCnt, char *pData)
{
	int ilRc = RC_SUCCESS;
	short slCursor = 0;
	short slFkt = 0;
	int rc=0, ili;

	char pclSqlBuf[M_BUFF];
	char pclTmpAnswer[L_BUFF];
	char pclDataBuff[VS_BUFF];


	memset(pclSqlBuf, 0x00, M_BUFF);
	memset(pclTmpAnswer, 0x00, L_BUFF);
	slFkt = START; 
	slCursor = 0;
	/* sprintf(pclSqlBuf,"SELECT %s FROM APT%s WHERE APC3='%s'",pFields,cgTabEnd,APC3); */
	sprintf(pclSqlBuf,"SELECT %s FROM %s WHERE %s",pFields,pTable,pSelection);
	/* dbg(TRACE,pclSqlBuf); */
	ilRc = sql_if(slFkt,&slCursor,pclSqlBuf,pclTmpAnswer);
	close_my_cursor(&slCursor);
	/* strcpy(pData, " "); */
	for (ili=0; ili < pFldCnt; ili++)
	{
		strcat(pData, "<");
		strcat(pData, pagReqFields[ili]);
		strcat(pData, ">");
		if (ilRc == DB_SUCCESS)
		{
			memset(pclDataBuff, 0x00, VS_BUFF);
			GetDataItem(pclDataBuff,pclTmpAnswer,ili+1,'\0',"","\0 ");
			ChangeCharFromTo(pclDataBuff,pcgServerChars,pcgClientChars);
			strcat(pData, pclDataBuff);
		} else
		{
			strcat(pData, " ");
		}
		strcat(pData, "</");
		strcat(pData, pagReqFields[ili]);
		strcat(pData, ">");
	}
}
static int ChangeCharFromTo(char *pcpData,char *pcpFrom,char *pcpTo)
{
  int ilRC = RC_SUCCESS;
  int i;
  char *pclData;

  if (pcpData == NULL)
     return -1;
  else if (pcpFrom == NULL)
     return -2;
  else if (pcpTo == NULL)
     return -3;
  else
  {
     pclData = pcpData;
     while (*pclData != 0x00)
     {
        for (i = 0; pcpFrom[i] != 0x00; i++)
        {
           if (pcpFrom[i] == *pclData)
           {		   	  
/*			  
              if (pcpFrom[i] == '\277')
              {
                 if (*(pclData-1) == 'H' && *(pclData+1) == 'M')
                    *pclData = pcpTo[i];
              }
              else */
                *pclData = pcpTo[i];
			
           }
        }
        pclData++;
     }
  }
}
/******************************************************************************/
/* The TimeToStr routine                                                      */
/******************************************************************************/
static int TimeToStr(char *pcpTime,time_t lpTime) 
{                                                                              
    struct tm *_tm;
    char   _tmpc[6];
    
      /*_tm = (struct tm *)localtime(&lpTime); */
    _tm = (struct tm *)gmtime(&lpTime);
    /*      strftime(pcpTime,15,"%" "Y%" "m%" "d%" "H%" "M%" "S",_tm); */
 

    sprintf(pcpTime,"%4d%02d%02d%02d%02d%02d",
            _tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday,_tm->tm_hour,
            _tm->tm_min,_tm->tm_sec);

    return RC_SUCCESS;  
                        
}     /* end of TimeToStr */
static int AddSecondsToCEDATime12(char *pcpTime, long llVal, int ilTyp)
{
  int ilRC = RC_SUCCESS;

  if (strlen(pcpTime) > 0 && *pcpTime != ' ')
     TrimRight(pcpTime);
  if (strlen(pcpTime) == 12)
     strcat(pcpTime,"00");
  ilRC = AddSecondsToCEDATime(pcpTime,llVal,ilTyp);

  return ilRC;
} /*end of AddSecondsToCEDATime12 */
static void TrimRight(char *pcpBuffer)
{
  char *pclBlank = &pcpBuffer[strlen(pcpBuffer)-1];
  if (strlen(pcpBuffer) == 0)
    {
      strcpy(pcpBuffer," ");
    }
  else
	{
	  while(isspace(*pclBlank) && pclBlank != pcpBuffer)
	    {
	      *pclBlank = '\0';
	      pclBlank--;
	    }
	}
}/* end of TrimRight*/

