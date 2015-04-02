#ifndef _DEF_mks_version
#define _DEF_mks_version
#include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/cdrhdl.c 1.4k 2015/02/10 17:46:20SGT fya Exp  $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* ABB ACE/FC Program Skeleton                                                */
/*                                                                            */
/* Author         : Eric Theessen eth                                         */
/* Date           : 12.02.2001                                                */
/* Description    : Interaktives Voice System Interface                       */
/*                                                                            */
/* Update history : eth 12.01.2001 start creation                             */
/*                  eth 19.04.2001 changed behavior in child function         */
/*                                 added config entry retry Init_cdrhdl       */
/*                                 added igRetry for Event timeout            */
/*                                                                            */
/*                  eth 29.05.2001 changed child behavior                     */
/*                                 always fork and die fast                   */
/*                                 no communication between parent and child  */
/*                                                                            */
/*                  eth 22.11.2001 copied version from SWR to POPS            */
/*                                 checked with diff ... OK                   */
/*                                                                            */
/*                  sfr 01.09.2002 added ReadBcData and ReadBcNum             */
/*                  JWE 13.01.2003 changed signal handling to ignore SIGCLD   */
/*                                 signal because this avoids having zombies  */
/*                                 being left after a lot of CDRHDL-children  */
/*                                 nearly die at the same time if the load for*/
/*                                 CDRHDL is high.                            */
/*  v.1.23      BST 07.Apr.08  Added the reload broadcasts command for        */
/*                             OCM.                                           */
/*  v.1.24      BST 23.Nov.08  Fix for looping problem - patched by DKA       */
/*              at site using email from BST.                                 */
/*  v.1.29      MEI 27.07.09  -New MI template to read LOATAB                 */
/*                            -Auto upd template values based on new template */
/*                            -2 new DB columns added in FMLTAB-TMTP and TXTP */
/*  v1.30       MEI 13.10.09  -Allow Booked Load info editable in EDIT LOAD   */
/*                             and reflect edited info in FLIGHT MOVEMENT LOG */
/*                             This involved upgrade of backend - FDIHDL ver  */
/*  v1.31       MEI 22.12.09  -New config added(CONFIG_LOAD), have new table  */
/*                             DORSIN to host these configurable items        */
/*  v1.32       MEI 22.01.10  -User requested DOR report for more TTYP        */
/*  v1.33       MEI 14.10.10  -to make it configurable to accept hint from client or not */
/*  v1.34       MEI 04.07.11  -Change in DOR data, display iTrek status timings */
/*  v1.35       MEI 14.10.11  -New config item on type C for combo box-AUH project */
/*  v1.36       MEI 17.11.11  -Include STYP check for DOR */
/*  v1.37       GFO 29.11.13  -Adjustments for AIX , add tcputil.h, netin.h remove double code*/
/*  v1.40       FYA 04.02.15  UFIS-8386 -Implement HandleData with FML command*/
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */
static char sccs_version[] = "%Z% UFIS44 (c) ABB AAT/I %M% %I% / %E% %U% / VBL";
/* be carefule with strftime or similar functions !!!                         */
/*                                                                            */
/******************************************************************************/
/* This program is a MIKE main program */

#if defined(_CDR) || defined(_BCQ)

#if defined(_CDR)
#define PROC_TYPE "PROC_TYPE: CDRHDL"
#endif
#if defined(_BCQ)
#define PROC_TYPE "PROC_TYPE: CDRCOM"
#endif

#else

#define _CDR
#define _BCQ
#define PROC_TYPE "PROC_TYPE: CDRBCQ"

#endif

#define U_MAIN
#define UGCCS_PRG
#define STH_USE
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h> /* by eth */
#ifndef _HPUX_SOURCE
#include <sys/select.h> /* by eth */
#endif
#include <sys/types.h>  /* by eth */
#include <sys/wait.h>   /* by eth */
#include <sys/time.h>   /* by eth */
#include <sys/stat.h>
#include <unistd.h>     /* by eth */
#if defined(_SOLARIS)
#include <sys/procfs.h>
#include <sys/fcntl.h>
#endif
#include "ugccsma.h"
#include "msgno.h"
#include "glbdef.h"
#include "quedef.h"
#include "uevent.h"
#include "sthdef.h"
#include "debugrec.h"
#include "hsbsub.h"
#include "db_if.h"
#include  "netin.h"
#include  "tcputil.h"
#include "cdrhdl.h"
#include "send.h"
#include "ct.h"

#define GRP_REC_SIZE 1024
#define GRP_DAT_SIZE 128

#ifdef PACKET_LEN
#undef PACKET_LEN
#endif
#define PACKET_LEN (1024)

#define MAX_BC_NUM 30000
/*
#define ccs_htons(s) ( (((s) & 0x00ff) << 8) | (((s) & 0xff00) >> 8) )
*/
#define MAX_FML_TAB  3
/* MEI 20-JUL-2011 */
#define STAT_NOT_CHECK 0
#define STAT_FALSE -1
#define STAT_TRUE 1

/* ******************************************************************** */
/* Internal used structures                                             */

/* ******************************************************************** */
typedef struct {
#if defined(_LINUX)
    unsigned short ActBcNum;
    unsigned short MaxBcNum;
#else
    short ActBcNum;
    short MaxBcNum;
#endif
} STACK_HEAD;

/*
typedef struct {
    short command;
    int length;
    char data[1];
} COMMIF;
*/
typedef struct {
    BC_HEAD BcHead;
    CMDBLK CmdBlk;
    char *DataBuffer;
    int DataBufferSize;
} REC_BC;

/*** MEI ***/
typedef struct {
    char fldname[8];
    char flddata[1028];
} FML_FLD_NODE;

typedef struct {
    short fldcnt;
    short valid;
    char tabname[8];
    char condition[8];
    FML_FLD_NODE *fld;
} FML_TAB_NODE;

typedef struct {
    char fcco[12];
    char fcco_list[128];
} FCCO_NODE;

typedef struct {
    char urno[12];
    char flno[12];
    char adid[2];
    char org3[4];
    char des3[4];
    char vial[1024];
    char vian[2];
    char flda[12];
    char tifa[12];
    char tifd[12];
    char vvia[40]; /* virtual field of slash separated via */
} AFTREC;

typedef struct {
    char sapc[4]; /* source airport code */
    char dapc[4]; /* destination airport code */
    char paxF[8]; /* first class */
    char paxB[8]; /* business class */
    char paxE[8]; /* economic class */
    char paxE2[8]; /* economic 2 class */
    char paxA[8]; /* adult */
    char paxC[8]; /* child */
    char paxI[8]; /* infant */
    char numbag[8]; /* number of bag */
    char bagwt[8]; /* weight of bag */
    char cargowt[8]; /* weight of cargo */
} LOADREC;
/************/

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE *outp       = NULL;*/
int debug_level = TRACE;
extern int errno;

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static FML_TAB_NODE FTN[MAX_FML_TAB];
static FCCO_NODE *rgFccoNode;
static LOADREC *prgLoadRec;
static AFTREC rgOAFT, rgJAFT;
static ITEM *prgItem = NULL; /* The queue item pointer     */
static EVENT *prgEvent = NULL; /* The event pointer          */
static EVENT *prgOutEvent = NULL; /* The Output Event for child */
static int igItemLen = 0; /* length of incoming item    */
static int igInitOK = FALSE;
static char cgConfigFile[512];
static char pcgAnyFile[512];
static FILE *pfgCdrGrp = NULL;
static char pcgGrpRec0[GRP_REC_SIZE + 1] = "";
static char pcgGrpRec1[GRP_REC_SIZE + 1] = "";
static char pcgGrpRec2[GRP_REC_SIZE + 1] = "";
static char pcgGrpDat[GRP_DAT_SIZE + 1] = "";
static char pcgOwnDat[GRP_DAT_SIZE + 1] = "";
static char pcgTmpDat[GRP_DAT_SIZE + 1] = "";
static char pcgOwnCmd[4] = "";
static char pcgTmpCmd[4] = "";
static char pcgTimeStamp[16] = "";
static char pcgBgnTime[16] = "";
static char pcgNowTime[16] = "";

static int igMyGrpNbr = 0;
static int igQueToRouter = 0;
static int igQueToNetin = 0;
static int igToBcHdl = 1900;
static int igRcvBcNum = 0;
static int igRcvBcCnt = 0;
static int igStopQueGet = FALSE;
static int igStopBcHdl = FALSE;

static int igAlarmStep = 10;
static int lgMainCheck = 60;
static int igGroupShutDown = FALSE;
static int igShutDownChild = FALSE;
static int igHsbDown = FALSE;
static long lgChkTime = 0;
static long lgChkDiff = 0;
static long lgBgnTime = 0;
static long lgNowTime = 0;
static long lgNowDiff = 0;
static long lgEvtBgnTime = 0;
static long lgEvtEndTime = 0;
static long lgEvtDifTime = 0;
static int igTimeOut = 0;
static int igGotTimeOut = FALSE;
static int igHrgSent = FALSE;
static int igUseOraHint = TRUE; /* MEI */

static char pcgService[16]; /* the client service socket from config-file*/

static int listenfd = 0; /* the listen-filedescriptor */
static struct sockaddr_in my_client; /* IP addr of client */

static int igMyPid = 0;
static int igMyParentPid = 0;
static int lgMyInitSize = -1;
static int igSizeCheck = TRUE;
static int lgChildMaxSize = -1;
static int igChildSizeCheck = TRUE;
static int igMaxFileSize = 1024 * 1024;
static int igMaxFiles = 10;
static int igResetChildLevel = 9;
static int igCleanChildLevel = 2;

static char pcgMyIpAddr[64] = "";
static char pcgMyHexAddr[64] = "........";
static char pcgMyBcQueId[64] = "";
static char pcgMyProcName[64] = "CDR";
static char pcgMyShortName[64] = "CDR";
static char pcgQueueName[16] = "QUE";
static char pcgBcKeyFlag[4] = "A";

static int igOraLoginOk = FALSE;
static int igProcIsChild = FALSE;
static int igProcIsBcCom = FALSE;
static int igMainIsBcCom = FALSE;

static int igAnswerQueue = 0;
static int igWaitForAck = FALSE;
static int igGotAlarm = FALSE;

static STR_DESC rgKeyItemListOut;
static STR_DESC rgRcvBuff;
static int igTmpAlive = FALSE;
static int igHrgAlive = TRUE;
static int igStatusAck = FALSE;
static int igAckCntSet = 0;

static time_t tgBeginStamp = 0;
static time_t tgEndStamp = 0;
static long lgConnCount = 0;
static long lgConnPeak = 0;
static long lgSizeLimit = 45200;
static long lgTotalRecs = 0;
static long lgTotalByte = 0;

static char pcgBcqPath[128];
static char pcgBcqFile[128];
static char pcgBcqFullPath[128];
static FILE *pfgBcqPtr = NULL;
static int igCheckFile = FALSE;
static int igClearFile = FALSE;
static int igBcqDataSize = 0;
static char pcgStackFileBcHdl[] = "BcHdlStack.Dat";

/* MEI 22-JAN-2010 */
static char pcgDORTtyp[128];
/* MEI 17-NOV-2011 */
static char pcgDORStyp[128];
static char bgIncludeTtyp;
static char bgIncludeStyp;
/* MEI 20-JUL-2011 */
static char bgCircularFlight = STAT_NOT_CHECK;
static int igFccoSettings = 0;
static int igCircularOffset = 0;
static int igNumLDMLoad = 0;

//FYA v1.40 UFIS-8386
static long lgEvtCnt = 0;
static char pcgTwStart[128];
static char pcgTwEnd[128];
static char cgHopo[256] = "\0"; /* default home airport    */
static char cgTabEnd[8] = "\0"; /* default table extension */
#define CMD_FML     (0)
static char *prgCmdTxt[] = {"FML"   , NULL};
static int    rgCmdDef[] = {CMD_FML , 0};

/* MEI 17-NOV-2011 */
/********************************************************************/
/* External functions                                               */
/********************************************************************/
extern int init_db();

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
static int Init_cdrhdl(); /* Initialize the program */
static int Reset(void); /* Reset program          */
static void Terminate(void); /* Terminate program      */
static void HandleSignal(int); /* Handles signals        */
static void HandleErr(int); /* Handles general errors */
static void HandleQueErr(int); /* Handles queuing errors */

//FYA v1.40 UFIS-8386
//static int HandleData(void); /* Handles event data     */
static int HandleData(EVENT *prpEvent); /* Handles event data     */
static int GetCommand(char *pcpCommand, int *pipCmd);
static int HandleFML_Command(int ipQue, char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpField, char *pcpData, BC_HEAD *prpBchead, CMDBLK *prpCmdblk);

static void HandleQueues(void); /* Waiting for Sts.-switch*/

static int CreateChild(int listenfd, int connfd);
static int StartChild(int listenfd, int ipGrpMbr);

static int CheckQueue(int ipModId, ITEM **prpItem, int ipNoWait);
static int WaitAndCheckQueue(int ipTimeout, int ipModId, ITEM **prpItem);
static int WaitAndCheckBcQueue(char *, int, int ipModId, ITEM **prpItem);

static char *GetKeyItem(char *pcpResultBuff, long *plpResultSize,
        char *pcpTextBuff, char *pcpKeyWord, char *pcpItemEnd, int bpCopyData);

static int StrgPutStrg(char *dest, int *destPos, char *src, int srcPos, int srcTo, char *deli);

static void TrimAndFilterCr(char *pclTextBuff, char *pcpFldSep, char *pcpRecSep);

static int ReadCedaArray(char *pcpMyChild, char *pcpKeyTbl, char *pcpKeyFld, char *pcpKeyWhe,
        REC_DESC *prpRecDesc, short spPackLines, int ipKeepOpen, int connfd,
        char *pcpFldSep, char *pcpRecSep, char *pcpOraHint);

static int HandleBcOut(char *pcpMyChild, int ipCpid, char *pcpHexAddr, int connfd);
static int HandleBcOff(char *pcpMyChild, char *pcpKeyQue, char *pcpHexAddr, int connfd);
static int HandleBcGet(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd);
static int HandleBcKey(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd);
static int SendDataToClient(char *pcpMyChild, char *pcpOutData, int ipOutLen, int connfd, int ipWaitForAck);
static int TransLateEvent(STR_DESC* prpString, int ipTrimData);
static long GetMyCurSize(int ipUsePid, char *pcpWho, int);
static int ReadMsgFromSock(int connfd, STR_DESC *rpMsg, int ipGetAck);
static void CheckChildStatus(void);
static int InitCdrGrpFile(int ipReSync);
static void SetGrpMemberInfo(int ipWho, int ipForWhat, char *pcpInfo, int ipOff);
static int GetNewMemberNbr(void);
static void GetGrpMemberInfo(int ipWho, int ipForWhat, char *pcpCmd, char *pcpInfo);
static int SwitchToNewLogFile(char *pcpOld, char *pcpNew, char *pcpWhy);
static void MainCheckLogSize(int ipSwitch);
static void SetMainCmd(char *pcpCmd, int ipWho);
static void SetChildCmd(char *pcpCmd, int ipWhere);
static void CheckMainTask(int ipFullCheck);
static void CheckChildTask(int ipFullCheck);
static void GetDbgLevel(char *pcpBuf);
static void GetChildAck(void);
static int HandleQueCreate(int ipCreate, char *pcpWho, int ipId);
static int HandleHrgCmd(int ipSend, char *pcpWho);
static int HandleOraLogin(int ipLogin, char *pcpWho);
static void PrintGrpData(char *pcpGrp, char *pcpPid, char *pcpQue, char *pcpTist, char *pcpHrg, char *pcpWks);
static void CheckBcqFile(void);
static int CheckFmlTemplate(char *pcpMyChild, char *pcpKeyTbl, char *pcpKeyWhe);
static int GetBcHistory(char *pcpMyChild, char *pcpKeyTbl,
        char *pcpKeyWhe, char *pcpKeyFld, char *pcpKeyDat,
        REC_DESC *prpRecDesc, short spPackLines, int connfd);
static int GetBcPacket(char *pcpMyChild, FILE *rpStackFile, int ipBcNum, COMMIF *prpReadBcPkt, int *pipBcCount);
static void SetShortValue(short *pspDest, short spValue);
static int TrimSpace(char *pcpInStr);
static int RunSQL(char *pcpMyChild, char *pcpSelection, char *pcpData);
static int GetFMLTabData(char *pcpMyChild, char *pcpLine, char *pcpUaft, int ipTabCnt);
static int GetFMLFieldData(char *pcpMyChild, char *pcpConfigName, char *pcpAftUrno, char *pcpFieldData);
static int TranslateText(char *pcpMyChild, char *pcpFmlText, char *pcpAftUrno);
static int TranslateTime(char *pcpMyChild, char *pcpFmlTime, char *pcpAftUrno);
static int GetStaffInfo(char *pcpMyChild, char *pclFieldName, char *pcpAftUrno, char *pcpFieldData);
static int strip_special_char(char *pcpProcessStr);
static int GetLDMLoad(char *pcpMyChild, char *pcpAftUrno);
static int CalcLoad(char *pcpMyChild, char *pcpFieldName, char *pcpAftUrno, char *pcpFieldData);


/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN{
    int ilRC = RC_SUCCESS; /* Return code */
    int ilCnt = 0; /* counter */
    int ilCount = 0; /* counter */
    int ilSig = 0;
    int ilLen = 0;
    int ilNewChild = TRUE;
    int ilStep = 0;

    struct in_addr* prlInAddr = NULL;

    int maxfdp1; /* max count of file-descriptors plus one */
    fd_set rset; /* the read set of descriptors */
    struct timeval connection; /* timeout structure for select */

    int connfd = 0;

    INITIALIZE; /* General initialization */

    /* all Signals */
    (void) SetSignals(HandleSignal);

    errno = 0;
    ilSig = sigignore(SIGCHLD);
    dbg(TRACE, "MAIN: sigingore(SIGCHLD) returns <%d>; ERRNO=<%s>", ilSig, strerror(errno));

    dbg(TRACE, "MAIN: version <%s>", mks_version);
    igMyPid = getpid();
    igMyParentPid = getppid();
    sprintf(pcgMyProcName, "MAIN: %05d>>", igMyPid);
    InitCdrGrpFile(FALSE);
    SetGrpMemberInfo(igMyGrpNbr, 2, "", 0);
    /* Attach to the MIKE queues */
    do {
        ilRC = init_que();
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "MAIN: init_que() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        }/* end of if */
    } while ((ilCnt < 10) && (ilRC != RC_SUCCESS));

    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: init_que() failed! waiting 60 sec ...");
        sleep(60);
        exit(0);
    } else {
        dbg(TRACE, "MAIN: init_que() OK! mod_id <%d>", mod_id);
    }/* end of if */

    sprintf(cgConfigFile, "%s/%s", getenv("BIN_PATH"), mod_name);
    ilRC = TransferFile(cgConfigFile);
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: TransferFile(%s) failed!", cgConfigFile);
    } /* end of if */

    /* uncomment if necessary */
    /* sprintf(cgConfigFile,"%s/%s.cfg",getenv("CFG_PATH"),mod_name); */
    /* ilRC = TransferFile(cgConfigFile); */
    /* if(ilRC != RC_SUCCESS) */
    /* { */
    /*   dbg(TRACE,"MAIN: TransferFile(%s) failed!",cgConfigFile); */
    /* } */ /* end of if */

    ilRC = SendRemoteShutdown(mod_id);
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: SendRemoteShutdown(%d) failed!", mod_id);
    } /* end of if */

    if ((ctrl_sta != HSB_STANDALONE) &&
            (ctrl_sta != HSB_ACTIVE) &&
            (ctrl_sta != HSB_ACT_TO_SBY)) {
        dbg(DEBUG, "MAIN: waiting for status switch ...");
        dbg(DEBUG, "MAIN: calling HandleQueues ...");
        HandleQueues();
    }/* end of if */

    if ((ctrl_sta == HSB_STANDALONE) ||
            (ctrl_sta == HSB_ACTIVE) ||
            (ctrl_sta == HSB_ACT_TO_SBY)) {
        dbg(TRACE, "MAIN: initializing ...");
        if (igInitOK == FALSE) {
            ilRC = Init_cdrhdl();
            if (ilRC != RC_SUCCESS) {
                dbg(TRACE, "Init_%s: init failed!", mod_name);
            } /* end of if */
        }/* end of if */
    } else {
        Terminate();
    }/* end of if */


    /* den listenfd erzeugen */
    dbg(TRACE, "MAIN: calling tcp_create_socket");
    listenfd = tcp_create_socket(SOCK_STREAM, &pcgService);
    if (listenfd != RC_FAIL) {
        ilRC = listen(listenfd, 5); /* maximal 5 zusaetzliche Anfrage */
        if (ilRC != 0) {
            /* ERROR */
            dbg(TRACE, "MAIN: problem with listen to listenfd");
        } else {
            /* alles klar */
            dbg(DEBUG, "MAIN: listen to listenfd OK");
        }
    } else {
        dbg(TRACE, "MAIN: can't get listenfd ... terminating");
        Terminate();
    }

    dbg(TRACE, "===== %s IS RUNNING NOW =====", pcgMyProcName);
    dbg(TRACE, "MAIN: initializing OK");
    dbg(TRACE, "=====================");

    tgBeginStamp = time(0L);
    lgConnCount = 0;
    lgConnPeak = 0;

    lgMyInitSize = GetMyCurSize(igMyPid, "INIT", TRUE);

    ilStep = 0;
    lgBgnTime = time(0L);
    lgChkTime = time(0L);
    lgNowTime = time(0L);

    ilNewChild = TRUE;
    for (;;) {
        FD_ZERO(&rset);
        FD_SET(listenfd, &rset);
        maxfdp1 = 0;
        maxfdp1 = listenfd + 1;

        connection.tv_sec = 2;
        connection.tv_usec = 0;

        /* hier kommt der select auf listenfd */
        if (ilNewChild == TRUE) {
            dbg(TRACE, "%s ===== LISTENING ON PORT =====", pcgMyProcName);
            SetGrpMemberInfo(igMyGrpNbr, 1, "L", 0);
        }

        ilNewChild = FALSE;
        ilRC = select(maxfdp1, &rset, NULL, NULL, &connection);
        if ((ilRC > 0) && (FD_ISSET(listenfd, &rset))) {
            if (lgMyInitSize <= 0) {
                lgMyInitSize = GetMyCurSize(igMyPid, "INIT", TRUE);
            }

            dbg(DEBUG, "%s GOT CONNECTION REQUEST =====", pcgMyProcName);

            ilLen = sizeof (my_client);
            #if defined(_HPUX_SOURCE) || defined(_SOLARIS)
                connfd = accept(listenfd, (struct sockaddr*) &my_client, &ilLen); ;
            #else
                connfd = accept(listenfd, (struct sockaddr*) &my_client, (size_t*)&ilLen);
            #endif

            if (connfd < 0) {
                dbg(TRACE, "MAIN: ERROR accept failed ... try again");
            } else {
                prlInAddr = (struct in_addr *) &my_client.sin_addr.s_addr;
                sprintf(pcgMyIpAddr, "%s", inet_ntoa(*prlInAddr));
                sprintf(pcgMyHexAddr, "%08x", ntohl(my_client.sin_addr.s_addr));
                (void) iStrup(pcgMyHexAddr);
                ilRC = CreateChild(listenfd, connfd);
                if (ilRC != RC_SUCCESS) {
                    dbg(TRACE, "MAIN: ERROR can't create new child");
                    /* Terminate(); */
                }
                ilNewChild = TRUE;
            }
        } else {
            /* dbg(DEBUG,"MAIN: something wrong with the socket connection");*/
            ;
        }

        ilRC = que(QUE_GETBIGNW, 0, mod_id, PRIORITY_3, igItemLen, (char *) &prgItem);
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = (EVENT *) prgItem->text;

        if (ilRC == RC_SUCCESS) {

            lgEvtCnt++;

            /* Acknowledge the item */
            ilRC = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
            if (ilRC != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            }

            switch (prgEvent->command) {
                case HSB_STANDBY:
                    ctrl_sta = prgEvent->command;
                    HandleQueues();
                    break;
                case HSB_COMING_UP:
                    ctrl_sta = prgEvent->command;
                    HandleQueues();
                    break;
                case HSB_ACTIVE:
                    ctrl_sta = prgEvent->command;
                    break;
                case HSB_ACT_TO_SBY:
                    ctrl_sta = prgEvent->command;
                    /* CloseConnection(); */
                    HandleQueues();
                    break;
                case HSB_DOWN:
                    /* whole system shutdown - */
                    /* do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    dbg(TRACE, "%s RECEIVED <HSB_DOWN> FROM SYSMON", pcgMyProcName);
                    SetChildCmd("K", -1);
                    Terminate();
                    break;
                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;

                    break;

                case SHUTDOWN:
                    /* process shutdown - maybe from uutil */
                    dbg(TRACE, "%s RECEIVED <SHUTDOWN>", pcgMyProcName);
                    SetChildCmd("D", -1);
                    Terminate();
                    break;

                case RESET:
                    ilRC = Reset();
                    break;

                case EVENT_DATA:
                    if ((ctrl_sta == HSB_STANDALONE) ||
                            (ctrl_sta == HSB_ACTIVE) ||
                            (ctrl_sta == HSB_ACT_TO_SBY)) {
                        //FYA v1.40 UFIS-8386
                        ilRC = HandleData(prgEvent);
                        if (ilRC != RC_SUCCESS) {
                            HandleErr(ilRC);
                        }/* end of if */
                    } else {
                        dbg(TRACE, "MAIN: wrong hsb status <%d>", ctrl_sta);
                        DebugPrintItem(TRACE, prgItem);
                        DebugPrintEvent(TRACE, prgEvent);
                    }
                    break;

                case TRACE_ON:
                    dbg_handle_debug(prgEvent->command);
                    if (debug_level == TRACE) {
                        SetChildCmd("T", -1);
                    } else {
                        SetChildCmd("F", -1);
                    }
                    break;
                case TRACE_OFF:
                    dbg_handle_debug(prgEvent->command);
                    SetChildCmd("O", -1);
                    break;
                case 100:
                    InitCdrGrpFile(TRUE);
                    break;
                case 101:
                    MainCheckLogSize(TRUE);
                    break;
                default:
                    dbg(TRACE, "MAIN: unknown event");
                    DebugPrintItem(TRACE, prgItem);
                    DebugPrintEvent(TRACE, prgEvent);
                    break;
            } /* end switch */

        }/* que returns RC_SUCCESS */
        else if (ilRC == QUE_E_NOMSG) {
            /* because QUE_GETBIGNW ... nowait */
            /* dbg(DEBUG,"MAIN: no message on queue\n"); */
        } else {
            /* Handle queuing errors */
            HandleQueErr(ilRC);
        } /* end else */

        lgNowTime = time(0L);

        lgChkDiff = lgNowTime - lgChkTime;
        if (lgChkDiff >= 10) {
            CheckMainTask(TRUE);
            lgChkTime = time(0L);
        }

        lgNowDiff = lgNowTime - lgBgnTime;
        if (lgNowDiff >= lgMainCheck) {
            ilStep++;
            if (ilStep == 1) {
                dbg(TRACE, "%s ===== CHILD STATUS CHECK ====", pcgMyProcName);
                SetChildCmd("A", -1);
            } else {
                dbg(DEBUG, "%s ===== CHILD ALIVE: CHECK ====", pcgMyProcName);
                GetChildAck();
                ilStep = 0;
            }
            lgBgnTime = time(0L);
            SetGrpMemberInfo(0, 1, "", 0);
        }

    } /* end for-ever */

    /* never reached */
    /* exit(0); */

} /* end of MAIN */



/******************************************************************************/
/* The initialization routine                                                 */

/******************************************************************************/
static int Init_cdrhdl() {
    int ilRC = RC_SUCCESS; /* Return code */
    int ilCount = 0; /* counter */
    int ilRetry = 0; /* retry counter */
    int llTmpValue = 0;
    int ili;
    char *pclCfgPath = NULL;
    char pclCfgFile[iMAX_BUF_SIZE];
    char pclTmpBuf[iMAX_BUF_SIZE];

    /* now reading from configfile or from database */
    igProcIsChild = FALSE;
    igProcIsBcCom = FALSE;
    igMyParentPid = getppid();
    igMyPid = getpid();
    sprintf(pcgMyProcName, "MAIN: %05d>>", igMyPid);
    dbg(TRACE, "INIT: <%s> COMPILED AS <%s>", pcgMyProcName, PROC_TYPE);

    if ((pclCfgPath = getenv("CFG_PATH")) == NULL) {
        dbg(TRACE, "<Init_%s> ERROR: missing environment CFG_PATH...", mod_name);
        dbg(DEBUG, "<Init_%s> ---- END ----", mod_name);
        return RC_FAIL;
    }

    strcpy(pclCfgFile, pclCfgPath);

    if (pclCfgFile[strlen(pclCfgFile) - 1] != '/')
        strcat(pclCfgFile, "/");

    strcat(pclCfgFile, mod_name);
    strcat(pclCfgFile, ".cfg");
    dbg(DEBUG, "<Init_%s> CFG-File: <%s>", mod_name, pclCfgFile);

    /* debug_level -------------------------------------------------- */
    pclTmpBuf[0] = 0x00;
    if ((ilRC = iGetConfigEntry(pclCfgFile,
            "MAIN",
            "debug_level",
            CFG_STRING,
            pclTmpBuf))
            != RC_SUCCESS) {
        /* default */
        dbg(DEBUG, "<Init_%s> no debug_level in section <MAIN>", mod_name);
        dbg(DEBUG, "<Init_%s> setting debug_level to TRACE", mod_name);
        strcpy(pclTmpBuf, "TRACE");
    }

    /* which debug_level is set in the Configfile ? */
    StringUPR((UCHAR*) pclTmpBuf);
    if (!strcmp(pclTmpBuf, "DEBUG")) {
        debug_level = DEBUG;
        dbg(TRACE, "<Init_%s> debug_level set to DEBUG", mod_name);
    } else if (!strcmp(pclTmpBuf, "TRACE")) {
        debug_level = TRACE;
        dbg(TRACE, "<Init_%s> debug_level set to TRACE", mod_name);
    } else if (!strcmp(pclTmpBuf, "OFF")) {
        debug_level = 0;
        dbg(TRACE, "<Init_%s> debug_level set to OFF", mod_name);
    } else {
        debug_level = 0;
        dbg(TRACE, "<Init_%s> unknown debug_level set to default OFF", mod_name);
    }


    /* client service socket ------------------------------------------- */
    pclTmpBuf[0] = 0x00;
    if ((ilRC = iGetConfigEntry(pclCfgFile,
            "MAIN",
            "service",
            CFG_STRING,
            pclTmpBuf))
            != RC_SUCCESS) {
        /* default */
        dbg(TRACE, "<Init_%s> no service in section <MAIN>", mod_name);
        if (strcmp(mod_name, "cdrcom") == 0) {
            strcpy(pclTmpBuf, "UFIS_BCQ");
        } else {
            strcpy(pclTmpBuf, "UFIS_BCR");
        }
    }
    sprintf(pcgService, "%s", pclTmpBuf);
    dbg(TRACE, "<Init_%s> SERVICE SET TO <%s>", mod_name, pcgService);
    if (strstr(pcgService, "_BCQ") != NULL) {
        igMainIsBcCom = TRUE;
        strcpy(pcgMyShortName, "BCQ");
    } else {
        igMainIsBcCom = FALSE;
        strcpy(pcgMyShortName, "CDR");
    }
    dbg(TRACE, "<Init_%s> STARTING AS <%s>", mod_name, pcgMyShortName);

    /* send to ------------------------------------------------------ */
    pclTmpBuf[0] = 0x00;
    if ((ilRC = iGetConfigEntry(pclCfgFile,
            "MAIN",
            "sendto",
            CFG_STRING,
            pclTmpBuf))
            != RC_SUCCESS) {
        /* default */
        dbg(TRACE, "<Init_%s> no sendto in section <MAIN>", mod_name);
        dbg(TRACE, "<Init_%s> setting sendto to router", mod_name);
        igQueToRouter = tool_get_q_id("router");
    }
    else
    {
        dbg(TRACE, "<Init_%s> sendto set to %s", mod_name, pclTmpBuf);
        igQueToRouter = tool_get_q_id(pclTmpBuf);
    }

    igQueToNetin = tool_get_q_id("netin");

    dbg(TRACE, "<Init_%s> Queue ID = %d", mod_name, igQueToRouter);


    /* CHILD SIZE CHECK ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "CHILD_MAX_SIZE", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        llTmpValue = 1000000;
    } else {
        llTmpValue = atol(pclTmpBuf);
    }
    if (llTmpValue > 0) {
        lgChildMaxSize = llTmpValue;
        igChildSizeCheck = TRUE;
        dbg(TRACE, "<Init_%s> EXIT CHILD ON SIZE CHECK ENABLED", mod_name);
        dbg(TRACE, "<Init_%s> CHILD_MAX_SIZE = %d K", mod_name, llTmpValue);
    } else {
        dbg(TRACE, "<Init_%s> EXIT CHILD ON SIZE CHECK DISABLED", mod_name);
        lgChildMaxSize = -1;
        igChildSizeCheck = FALSE;
    }


    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "MAX_LOG_SIZE", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        igMaxFileSize = 1024 * 1024;
    } else {
        igMaxFileSize = atoi(pclTmpBuf);
    }
    dbg(TRACE, "<Init_%s> MAX_LOG_SIZE = %d", mod_name, igMaxFileSize);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "CHILD_CHECK_STATUS", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        igAlarmStep = 10;
    } else {
        igAlarmStep = atoi(pclTmpBuf);
    }
    dbg(TRACE, "<Init_%s> CHILD_CHECK_STATUS = %d", mod_name, igAlarmStep);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "MAIN_CHECK_STATUS", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        lgMainCheck = 60;
    } else {
        lgMainCheck = atol(pclTmpBuf);
    }
    dbg(TRACE, "<Init_%s> MAIN_CHECK_STATUS = %d", mod_name, lgMainCheck);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "RESET_CHILD_LEVEL", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        igResetChildLevel = 9;
    } else {
        igResetChildLevel = atoi(pclTmpBuf);
    }
    dbg(TRACE, "<Init_%s> RESET_CHILD_LEVEL = %d", mod_name, igResetChildLevel);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "CLEAN_CHILD_LEVEL", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        igCleanChildLevel = 2;
    } else {
        igCleanChildLevel = atoi(pclTmpBuf);
    }
    dbg(TRACE, "<Init_%s> CLEAN_CHILD_LEVEL = %d", mod_name, igCleanChildLevel);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "SHUTDOWN_GROUP", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        igGroupShutDown = FALSE;
    } else {
        igGroupShutDown = FALSE;
        if (strcmp(pclTmpBuf, "YES") == 0) {
            igGroupShutDown = TRUE;
        }
    }
    if (igGroupShutDown == TRUE) {
        dbg(TRACE, "<Init_%s> SHUTDOWN_GROUP SET TO 'YES'", mod_name);
    } else {
        dbg(TRACE, "<Init_%s> SHUTDOWN_GROUP SET TO 'NO'", mod_name);
    }


    /* MEI 22-JAN-2010 */
    bgIncludeTtyp = FALSE;
    ilRC = iGetConfigEntry(pclCfgFile, "DOR_CONFIG", "INCLUDE_TTYP", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS && strlen(pclTmpBuf) > 0) {
        strcpy(pcgDORTtyp, pclTmpBuf);
        bgIncludeTtyp = TRUE;
        dbg(TRACE, "<Init_%s> DOR INCLUDE TTYP = <%s>", mod_name, pcgDORTtyp);
    }

    /* MEI 17-NOV-2011 */
    bgIncludeStyp = FALSE;
    ilRC = iGetConfigEntry(pclCfgFile, "DOR_CONFIG", "INCLUDE_STYP", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS && strlen(pclTmpBuf) > 0) {
        strcpy(pcgDORStyp, pclTmpBuf);
        bgIncludeStyp = TRUE;
        dbg(TRACE, "<Init_%s> DOR INCLUDE STYP = <%s>", mod_name, pcgDORStyp);
    }

    /*******************/

    /* MEI 21-JUL-2011 */
    if ((ilRC = iGetConfigEntry(pclCfgFile, "DOR_CONFIG", "FCCO_SETTINGS", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
        igFccoSettings = 0;
    else {
        igFccoSettings = get_no_of_items(pclTmpBuf);
        dbg(TRACE, "<Init_%s> DOR NUMBER OF FCCO SETTINGS = <%d>", mod_name, igFccoSettings);
    }
    if (igFccoSettings > 0) {
        rgFccoNode = (FCCO_NODE *) malloc(sizeof (FCCO_NODE) * igFccoSettings);
        memset(rgFccoNode, 0, sizeof (FCCO_NODE) * igFccoSettings);
        for (ili = 0; ili < igFccoSettings; ili++) {
            get_real_item(rgFccoNode[ili].fcco, pclTmpBuf, ili + 1);
            iGetConfigEntry(pclCfgFile, "DOR_CONFIG", rgFccoNode[ili].fcco, CFG_STRING, rgFccoNode[ili].fcco_list);
            dbg(TRACE, "<Init_%s> Item <%d> FCCO <%s> LIST <%s>", mod_name, ili, rgFccoNode[ili].fcco, rgFccoNode[ili].fcco_list);
        }
    }

    igCircularOffset = 0;
    if ((ilRC = iGetConfigEntry(pclCfgFile, "DOR_CONFIG", "CIRCULAR_SEARCH_OFFSET", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
        igCircularOffset = atoi(pclTmpBuf);
    dbg(TRACE, "<Init_%s> CIRCULAR_SEARCH_OFFSET <%d>", mod_name, igCircularOffset);
    /*******************/

    /* MEI 14-OCT-2010 */
    igUseOraHint = TRUE;
    if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "USE_ORA_HINT", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
        if (!strncmp(pclTmpBuf, "NO", 2))
            igUseOraHint = FALSE;
    dbg(TRACE, "<Init_%s> USE_ORA_HINT = <%d>", mod_name, igUseOraHint);
    /*******************/

    strcpy(cgConfigFile, pclCfgFile);

    /* ---------------------------------------------- */
    if ((ilRC = iGetConfigEntry("/ceda/conf/bchdl.cfg", "SYSTEM", "BCQ_FILE_PATH", CFG_STRING, pclTmpBuf))
            != RC_SUCCESS) {
        strcpy(pclTmpBuf, "/ceda/debug");
    }
    strcpy(pcgBcqPath, pclTmpBuf);
    dbg(TRACE, "<Init_%s> BCQ_FILE_PATH = %s", mod_name, pcgBcqPath);

    //FYA v1.40 UFIS-8386
    ilRC = tool_search_exco_data("SYS", "HOMEAP", cgHopo);
    if (ilRC != RC_SUCCESS)
    {
        dbg(TRACE, "<Init_%s> EXTAB,SYS,HOMEAP not found in SGS.TAB",mod_name);
        Terminate();
    }
    else
    {
        dbg(TRACE, "<Init_%s> home airport <%s>", mod_name,cgHopo);
    }
    ilRC = tool_search_exco_data("ALL", "TABEND", cgTabEnd);
    if (ilRC != RC_SUCCESS)
    {
        dbg(TRACE, "<Init_%s> EXTAB,ALL,TABEND not found in SGS.TAB",mod_name);
        Terminate();
    }
    else
    {
        dbg(TRACE, "<Init_%s> table extension <%s>", mod_name,cgTabEnd);
    }

    /*SetDbgLimits (0,igMaxFiles);*/

    CT_InitStringDescriptor(&rgKeyItemListOut);

    igInitOK = TRUE;

    return (RC_SUCCESS);

} /* end of initialize */



/******************************************************************************/
/* The Reset routine                                                          */

/******************************************************************************/
static int Reset() {
    int ilRC = RC_SUCCESS; /* Return code */

    dbg(DEBUG, "Reset: calling Init_%s", mod_name);

    ilRC = Init_cdrhdl();
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "Reset: Init_%s failed!", mod_name);
    }

    return ilRC;

} /* end of Reset */



/******************************************************************************/
/* The termination routine                                                    */

/******************************************************************************/
static void Terminate() {
    int ilRC = RC_SUCCESS;
    int ilCount = 0; /* counter */
    char pclKeyQue[16];

    dbg(DEBUG, "%s READY TO TERMINATE ...", pcgMyProcName);

    if (igHsbDown == TRUE) {
        igAnswerQueue = 0;
    }

    if ((igAnswerQueue > 0) && (igProcIsBcCom == TRUE)) {
        sprintf(pclKeyQue, "%d", igAnswerQueue);
        dbg(DEBUG, "%s DISCONNECTING BCQ%5d FROM BCHDL", pcgMyProcName, igAnswerQueue);
        ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "", /* BC_HEAD.dest_name           */
                "", /* BC_HEAD.recv_name           */
                "", /* CMDBLK.tw_start             */
                "", /* CMDBLK.tw_end               */
                "BCOFF", /* CMDBLK.command              */
                "BCQCOM", /* CMDBLK.obj_name             */
                pcgMyHexAddr, /* Selection Block(HexAddress) */
                pclKeyQue, /* Field Block                 */
                "", /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

    }

    /* =================================== */
    /* Close open Sessions and Queues      */
    /* =================================== */
    ilRC = HandleHrgCmd(FALSE, "");
    ilRC = HandleQueCreate(FALSE, "", 1);
    ilRC = HandleOraLogin(FALSE, "");

    SetGrpMemberInfo(igMyGrpNbr, 3, "D", 2);
    SetGrpMemberInfo(igMyGrpNbr, 1, ".", 0);
    SetGrpMemberInfo(igMyGrpNbr, 0, ".", 0);
    SetGrpMemberInfo(igMyGrpNbr, 0, ".", 1);

    if (pfgCdrGrp != NULL) {
        dbg(DEBUG, "%s DISCONNECT (MEMBER %d) FROM %s GROUP", pcgMyProcName, igMyGrpNbr, pcgMyShortName);
        fclose(pfgCdrGrp);
    } /* end if */

#ifdef WMQ
    ilRC = WmqDisconnectFromQM(1, 0, FALSE);
    dbg(DEBUG, "%05d:RC from WmqDisconnectFromMQ = %d", __LINE__, ilRC);
#endif

    dbg(TRACE, "%s NOW EXIT =============>>", pcgMyProcName);
    exit(0);

} /* end of Terminate */



/******************************************************************************/
/* The handle signals routine                                                 */

/******************************************************************************/
static void HandleSignal(int pipSig) {
    int ilRC = RC_SUCCESS; /* Return code */
    int ilCount = 0; /* counter */
    int ilCpid; /* the child pid */

    /* dbg(TRACE,"HandleSignal: signal <%d> received", pipSig); */

    /* see: /usr/include/sys/signal.h */

    switch (pipSig) {
        case SIGCLD:
            /* #define SIGCLD  18  ** child status change */
            dbg(DEBUG, "%s HandleSignal: reveived SIGCLD ...", pcgMyProcName);
            /****************************************************************************/
            /* the following two lines are obsolete because we now ignore SIGCLD signal */
            /****************************************************************************/
            /*ilCpid = wait(0);*/
            /*dbg(DEBUG,"HandleSignal: child %05d is dead now",ilCpid);*/
            break;

        case SIGPIPE:
            /* #define SIGPIPE 13     **  write on a pipe with no one to read it */
            dbg(TRACE, "%s RECEIVED SIGNAL <SIGPIPE> ...", pcgMyProcName);
            dbg(TRACE, "%s CLIENT APPLICATION TERMINATED", pcgMyProcName);
            Terminate();
            break;

        case SIGALRM:
            /* #define SIGALRM 14  ** alarm clock */
            igGotAlarm = TRUE;
            if (igWaitForAck == TRUE) {
                dbg(TRACE, "%s RECEIVED SIGNAL <SIGALARM> ...", pcgMyProcName);
                dbg(TRACE, "%s (timeout while waiting for message from client)", pcgMyProcName);
                Terminate();
            }
            break;

        default:
            dbg(TRACE, "%s HandleSignal: received SIGNAL (%d) ... terminate", pcgMyProcName, pipSig);
            Terminate();
            break;

    } /* end of switch */

} /* end of HandleSignal */



/******************************************************************************/
/* The handle general error routine                                           */

/******************************************************************************/
static void HandleErr(int pipErr) {
    int ilRC = RC_SUCCESS;
    return;
} /* end of HandleErr */



/******************************************************************************/
/* The handle queuing error routine                                           */

/******************************************************************************/
static void HandleQueErr(int pipErr) {
    int ilRC = RC_SUCCESS;

    switch (pipErr) {
        case QUE_E_FUNC: /* Unknown function */
            dbg(TRACE, "<%d> : unknown function", pipErr);
            break;
        case QUE_E_MEMORY: /* Malloc reports no memory */
            dbg(TRACE, "<%d> : malloc failed", pipErr);
            break;
        case QUE_E_SEND: /* Error using msgsnd */
            dbg(TRACE, "<%d> : msgsnd failed", pipErr);
            break;
        case QUE_E_GET: /* Error using msgrcv */
            dbg(TRACE, "<%d> : msgrcv failed", pipErr);
            break;
        case QUE_E_EXISTS:
            dbg(TRACE, "<%d> : route/queue already exists ", pipErr);
            break;
        case QUE_E_NOFIND:
            dbg(TRACE, "<%d> : route not found ", pipErr);
            break;
        case QUE_E_ACKUNEX:
            dbg(TRACE, "<%d> : unexpected ack received ", pipErr);
            break;
        case QUE_E_STATUS:
            dbg(TRACE, "<%d> :  unknown queue status ", pipErr);
            break;
        case QUE_E_INACTIVE:
            dbg(TRACE, "<%d> : queue is inaktive ", pipErr);
            break;
        case QUE_E_MISACK:
            dbg(TRACE, "<%d> : missing ack ", pipErr);
            break;
        case QUE_E_NOQUEUES:
            dbg(TRACE, "<%d> : queue does not exist", pipErr);
            break;
        case QUE_E_RESP: /* No response on CREATE */
            dbg(TRACE, "<%d> : no response on create", pipErr);
            break;
        case QUE_E_FULL:
            dbg(TRACE, "<%d> : too many route destinations", pipErr);
            break;
        case QUE_E_NOMSG: /* No message on queue */
            dbg(TRACE, "<%d> : no messages on queue", pipErr);
            break;
        case QUE_E_INVORG: /* Mod id by que call is 0 */
            dbg(TRACE, "<%d> : invalid originator=0", pipErr);
            break;
        case QUE_E_NOINIT: /* Queues is not initialized*/
            dbg(TRACE, "<%d> : queues are not initialized", pipErr);
            break;
        case QUE_E_ITOBIG:
            dbg(TRACE, "<%d> : requestet itemsize to big ", pipErr);
            break;
        case QUE_E_BUFSIZ:
            dbg(TRACE, "<%d> : receive buffer to small ", pipErr);
            break;
        default: /* Unknown queue error */
            dbg(TRACE, "<%d> : unknown error", pipErr);
            break;
    } /* end switch */

    return;
} /* end of HandleQueErr */



/******************************************************************************/
/* The handle queues routine                                                  */

/******************************************************************************/
static void HandleQueues() {
    int ilRC = RC_SUCCESS; /* Return code */
    int ilBreakOut = FALSE;

    do {
        ilRC = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, igItemLen, (char *) &prgItem);
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = (EVENT *) prgItem->text;

        if (ilRC == RC_SUCCESS) {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
            if (ilRC != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            }

            switch (prgEvent->command) {
                case HSB_STANDBY:
                    ctrl_sta = prgEvent->command;
                    break;
                case HSB_COMING_UP:
                    ctrl_sta = prgEvent->command;
                    break;
                case HSB_ACTIVE:
                    ctrl_sta = prgEvent->command;
                    ilBreakOut = TRUE;
                    break;
                case HSB_ACT_TO_SBY:
                    ctrl_sta = prgEvent->command;
                    break;
                case HSB_DOWN:
                    /* whole system shutdown - */
                    /* do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    dbg(TRACE, "%s RECEIVED <HSB_DOWN> FROM SYSMON", pcgMyProcName);
                    Terminate();
                    break;
                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    ilBreakOut = TRUE;
                    break;
                case SHUTDOWN:
                    dbg(TRACE, "%s RECEIVED <SHUTDOWN>", pcgMyProcName);
                    Terminate();
                    break;
                case RESET:
                    ilRC = Reset();
                    break;
                case EVENT_DATA:
                    dbg(TRACE, "HandleQueues: wrong hsb status <%d>", ctrl_sta);
                    DebugPrintItem(TRACE, prgItem);
                    DebugPrintEvent(TRACE, prgEvent);
                    break;
                case TRACE_ON:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case TRACE_OFF:
                    dbg_handle_debug(prgEvent->command);
                    break;
                default:
                    dbg(TRACE, "HandleQueues: unknown event");
                    DebugPrintItem(TRACE, prgItem);
                    DebugPrintEvent(TRACE, prgEvent);
                    break;
            } /* end switch */
        } else {
            /* Handle queuing errors */
            HandleQueErr(ilRC);
        } /* end else */

    } while (ilBreakOut == FALSE);

    if (igInitOK == FALSE) {
        ilRC = Init_cdrhdl();
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "HandleQueues: init failed!");
        } /* end of if */
    }/* end of if */

} /* end of HandleQueues */



/******************************************************************************/
/* The handle data routine                                                    */

/******************************************************************************/
//FYA v1.40 UFIS-8386
#if 0
static int HandleData() {
    int ilRC = RC_SUCCESS; /* Return code */

    return ilRC;

}/* end of HandleData */
#endif
static int HandleData(EVENT *prpEvent)
{
    char * pclFunc = "HandleData";
    int ilRc = RC_SUCCESS; /* Return code */
    int ilQue_out = 0;
    int i, ilCmd = 0;
    BC_HEAD *prlBchead = NULL;
    CMDBLK *prlCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;
    char clTable[34] = "\0";

    prlBchead = (BC_HEAD *) ((char *) prpEvent + sizeof (EVENT));
    prlCmdblk = (CMDBLK *) ((char *) prlBchead->data);
    pclSelection = prlCmdblk->data;
    pclFields = pclSelection + strlen(pclSelection) + 1;
    pclData = pclFields + strlen(pclFields) + 1;

    strcpy(clTable, prlCmdblk->obj_name);
    strcpy(pcgTwStart, prlCmdblk->tw_start);
    strcpy(pcgTwEnd, prlCmdblk->tw_end);

    dbg(TRACE, "========== START <%10.10d> ==========", lgEvtCnt);

    dbg(TRACE,"%s: TABEND = <%s>",pclFunc,cgTabEnd);
    memset(pcgTwEnd,0x00,sizeof(pcgTwEnd));
    sprintf(pcgTwEnd,"%s,%s,%s",cgHopo,cgTabEnd,mod_name);
    dbg(TRACE,"%s : TW_END = <%s>",pclFunc,pcgTwEnd);

    ilQue_out = prpEvent->originator;

    dbg(TRACE, "Originator = <%d>", prpEvent->originator);
    dbg(TRACE, "Command    = <%s>", prlCmdblk->command);
    dbg(TRACE, "Table      = <%s>", clTable);
    dbg(TRACE, "Selection  = <%s>", pclSelection);
    dbg(TRACE, "Fields     = <%s>", pclFields);
    dbg(TRACE, "Data       = <%s>", pclData);

    ilRc = GetCommand(&prlCmdblk->command[0], &ilCmd);
    if (ilRc == RC_SUCCESS)
    {
        switch (ilCmd)
        {
           case CMD_FML:
                dbg(DEBUG, "%s: %s Cmdblk->command = <%s>", pclFunc, prgCmdTxt[ilCmd], prlCmdblk->command);
                if(clTable == NULL)
                {
                    HandleFML_Command(ilQue_out,prlCmdblk->command,"FMLTAB",pclSelection,pclFields,pclData,prlBchead,prlCmdblk);
                }
                else
                {
                    HandleFML_Command(ilQue_out,prlCmdblk->command,clTable,pclSelection,pclFields,pclData,prlBchead,prlCmdblk);
                }

                break;
           default:
                dbg(DEBUG, "%s: %s Cmdblk->command = <%s> -> Invalid Command Received", pclFunc, prgCmdTxt[ilCmd], prlCmdblk->command);
                break;
        }
    }

    dbg(TRACE, "==========  END  <%10.10d> ==========", lgEvtCnt);
    return (RC_SUCCESS);
}

static int GetCommand(char *pcpCommand, int *pipCmd)
{
    int ilRc = RC_SUCCESS; /* Return code */
    int ilLoop = 0;
    char clCommand[48] = "\0";

    memset(&clCommand[0], 0x00, 8);

    ilRc = get_real_item(&clCommand[0], pcpCommand, 1);
    if (ilRc > 0) {
        ilRc = RC_FAIL;
    }/* end of if */

    while ((ilRc != 0) && (prgCmdTxt[ilLoop] != NULL))
    {
        ilRc = strcmp(&clCommand[0], prgCmdTxt[ilLoop]);
        ilLoop++;
    }/* end of while */

    if (ilRc == 0)
    {
        ilLoop--;
        dbg(TRACE, "GetCommand: <%s> <%d>", prgCmdTxt[ilLoop], rgCmdDef[ilLoop]);
        *pipCmd = rgCmdDef[ilLoop];
    }
    else
    {
        dbg(TRACE, "GetCommand: <%s> is not valid", &clCommand[0]);
        ilRc = RC_FAIL;
    }/* end of if */

    return (ilRc);
}

/******************************************************************************/
/* CreateChild */
/* by eth 12.02.2001                                                          */

/******************************************************************************/
static int CreateChild(int listenfd, int connfd) {
    char *pclFct = "CreateChild";
    int ilRC = RC_SUCCESS;
    pid_t childpid;
    int ilCPid = 0;
    int ilNewNbr = 0;
    long llStampDiff = 0;
    struct stat rlLfBuff;

    ilRC = RC_SUCCESS;

    dbg(DEBUG, "%s ---------------------------", pcgMyProcName);
    dbg(DEBUG, "%s PREPARING NEW CHILD PROCESS", pcgMyProcName);
    lgConnCount++;
    if (lgConnCount > lgConnPeak) {
        lgConnPeak = lgConnCount;
    }
    tgEndStamp = time(0L);
    llStampDiff = tgEndStamp - tgBeginStamp + 1;
    if (llStampDiff >= 2) {
        if (lgConnCount > 1) {
            dbg(TRACE, "%s CONNECTIONS: %d IN LAST %d SECONDS (PEAK=%d)", pcgMyProcName, lgConnCount, llStampDiff, lgConnPeak);
        }
        lgConnCount = 0;
        tgBeginStamp = tgEndStamp;
    }

    MainCheckLogSize(FALSE);

    /* neues child erzeugen ... fork */
    dbg(DEBUG, "%s NOW FORKING ...", pcgMyProcName);
    SetGrpMemberInfo(igMyGrpNbr, 1, "F", 0);
    ilNewNbr = GetNewMemberNbr();
    dbg(DEBUG, "%s ---------------------------", pcgMyProcName);
#ifdef WMQ
    ilRC = WmqDisconnectFromQM(1, 0, FALSE);
    dbg(DEBUG, "%05d:RC from WmqDisconnectFromMQ = %d", __LINE__, ilRC);
#endif
    if ((childpid = fork()) == 0) {
        ilCPid = getpid();
        close(listenfd);
        ilRC = StartChild(connfd, ilNewNbr);
        exit(0); /* never come here, but end child */
    }

#ifdef WMQ
    while (ilRC = init_que()) {
        dbg(DEBUG, "CDRHDL:Waiting for que creation %d", ilRC);
        sleep(1); /* Wait for QCP to create queues */
    }
#endif

    /* ############ parent process ############ */

    close(connfd);

    if (childpid < 0) {
        dbg(TRACE, "%s ERROR: UNABLE TO CREATE CHILD !!", pcgMyProcName);
        SetGrpMemberInfo(ilNewNbr, 1, ".", 0);
        ilRC = RC_FAIL;
    }

    return ilRC; /* wichtig fuer Fehlererkennung */

} /* end of CreateChild */



/******************************************************************************/
/* The child routine */

/******************************************************************************/
static int StartChild(int connfd, int ipGrpMbr) {
    char *pclFct = "CHILD"; /* the function name */
    char pclMyChild[64];
    int ilRC = RC_SUCCESS; /* Return code */
    int ilCpid; /* the child pid */
    int ilGotError = FALSE;
    int ilCmdAlive = FALSE;
    int ilEndAlive = FALSE;
    char *pclAliveAnsw = "ALIVE{=ALIVE=}ALIVE";
    char *pclTimeOutMsg = "ERROR: TIMEOUT WHILE WAITING FOR ANSWER";
    char *pclAnyErrMsg = "ERROR: UNEXPECTED MESSAGE FROM SYSQCP";

    EVENT *prlEvent = NULL;
    BC_HEAD *prlBchead = NULL;
    CMDBLK *prlCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;
    char *pclError = NULL;
    char pclNoError[] = {'\0'}; /* Change pointer to quoted string to an array (strup error) */
    char pclEmpty[] = {'\0'}; /* Change pointer to quoted string to an array (strup error) */

    long llActSize = 0;
    int ilSendToRouter = TRUE;

    char pclDummy[32];

    char pclChkCmd[64];
    char pclCurKey[64];
    char pclKeyEvt[64];
    char pclKeyTot[64];
    char pclKeyCmd[64];
    char pclKeyIdn[64];
    char pclKeyTbl[64];
    char pclKeyExt[64];
    char pclKeyHop[64];
    /*
      char pclKeyFldX[4096*2];
      char pclKeyWheX[4096*6];
      char pclKeyDatX[4096*6];
     */
    char *pclKeyWhe = NULL;
    char *pclKeyFld = NULL;
    char *pclKeyDat = NULL;
    char pclKeyTws[64];
    char pclKeyTwe[64];
    char pclKeyUsr[64];
    char pclKeyWks[64];
    char pclKeySep[64];
    char pclFldSep[64];
    char pclRecSep[64];
    char pclKeyApp[64];
    char pclKeyQue[64];
    char pclKeyHex[64];
    char pclKeyPak[64];
    char pclKeyAli[64];
    char pclKeyTim[64];
    char pclKeyHnt[128];
    char *pclKeyPtr = NULL;
    char *pclOutPtr = NULL;
    int ilEvtCnt = 0;
    int ilCurEvt = 0;
    int ilTist = 0;
    int ilMultEvt = FALSE;
    int ilKeepOpen = FALSE;
    long llDatLen = 0L;
    short slMaxPak = 0L;
    int ilBytesSent = 0;
    int ilSentThisTime = 0;
    int ilBytesToSend = 0;
    char pclTmpBuf[1024];

    int ilTotDatLen = 0;
    int ilTotRcvLen = 0;
    int ilRemaining = 0;
    int ilBreakOut = FALSE;

    int ilRcvDatLen = 0;
    int ilRcvDatPos = 0;
    char *pclRcvData = NULL;
    char *pclEvtData = NULL;

    int ilCurBufLen = 0;
    int ilCurStrLen = 0;
    int ilOutDatLen = 0;
    int ilOutDatPos = 0;
    long llMySize = 0;
    char *pclOutData = NULL;
    REC_DESC rlRecDesc;
    KEY_DESC rlKeyDat;
    KEY_DESC rlKeyWhe;
    KEY_DESC rlKeyFld;

    igProcIsChild = TRUE;
    igAnswerQueue = 0;

    ilCpid = getpid();
    igMyPid = ilCpid;
    igMyParentPid = getppid();
    sprintf(pclMyChild, "CHILD %05d>>", ilCpid);
    strcpy(pcgMyProcName, pclMyChild);
    dbg(TRACE, "===== %s IS RUNNING NOW =====", pclMyChild);
    dbg(TRACE, "%s MY CONFIG FILE <%s>", pcgMyProcName, cgConfigFile);
    dbg(TRACE, "%s TCP/IP <%s> HEX <%s>", pcgMyProcName, pcgMyIpAddr, pcgMyHexAddr);
    InitCdrGrpFile(FALSE);
    igMyGrpNbr = ipGrpMbr;
    SetGrpMemberInfo(igMyGrpNbr, 2, "", 0);
    /*dbg(DEBUG,"%s DISABLE DBG LOGFILE FEATURES",pcgMyProcName);
    SetDbgLimits (0,1);*/

    CT_InitStringDescriptor(&rgKeyItemListOut);
    CT_InitStringDescriptor(&rgRcvBuff);
    InitRecordDescriptor(&rlRecDesc, (1024 * 1024), 0, FALSE);
    igOraLoginOk = FALSE;

    igShutDownChild = FALSE;

    lgEvtBgnTime = time(0L);

    igHrgAlive = TRUE;
    ilEndAlive = FALSE;
    while (ilEndAlive == FALSE) {
        ilEndAlive = TRUE;

        igWaitForAck = FALSE;
        CheckChildTask(FALSE);
        if (igShutDownChild == TRUE) {
            close(connfd);
            Terminate();
        }
        if ((igTmpAlive == TRUE) || (ilCmdAlive == TRUE)) {
            dbg(TRACE, "%s WAITING FOR NEXT COMMAND FROM CLIENT", pclMyChild);
        } else {
            dbg(TRACE, "%s WAITING FOR COMMAND FROM CLIENT", pclMyChild);
        }
        ilRC = ReadMsgFromSock(connfd, &rgRcvBuff, FALSE);

        pclRcvData = rgRcvBuff.Value;
        pclData = NULL;
        lgTotalRecs = 0;
        lgTotalByte = 0;
        lgEvtBgnTime = time(0L);

        /* dbg(TRACE,"%s RECEIVED <%s>", pclMyChild,pclRcvData);  */

#ifdef WMQ
        /* reset all open queues inherited from the parent cdrhdl */
        WmqResetQuetable();
#endif
        pclKeyPtr = GetKeyItem(pclKeyEvt, &llDatLen, pclRcvData, "{=EVT=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=EVT=}", pclKeyEvt, llDatLen);
        ilEvtCnt = atoi(pclKeyEvt);
        if (ilEvtCnt > 0) {
            dbg(TRACE, "%s MULTI KEY EVENTS RECEIVED (%d)", pclMyChild, ilEvtCnt);
            ilMultEvt = TRUE;
            ilKeepOpen = TRUE;
        }/* end if */
        else {
            dbg(TRACE, "%s SINGLE KEY EVENT RECEIVED", pclMyChild);
            ilMultEvt = FALSE;
            ilKeepOpen = FALSE;
            ilEvtCnt = 1;
        } /* end else */

        pclKeyPtr = GetKeyItem(pclKeyTot, &llDatLen, pclRcvData, "{=TOT=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TOT=}", pclKeyTot, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyIdn, &llDatLen, pclRcvData, "{=IDN=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=IDN=}", pclKeyIdn, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyExt, &llDatLen, pclRcvData, "{=EXT=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=EXT=}", pclKeyExt, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyHop, &llDatLen, pclRcvData, "{=HOPO=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=HOPO=}", pclKeyHop, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyUsr, &llDatLen, pclRcvData, "{=USR=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=USR=}", pclKeyUsr, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyWks, &llDatLen, pclRcvData, "{=WKS=}", "{=", TRUE);
        if (llDatLen > 0) {
            dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=WKS=}", pclKeyWks, llDatLen);
            SetGrpMemberInfo(igMyGrpNbr, 8, pclKeyWks, 0);
        }

        pclKeyPtr = GetKeyItem(pclKeyTws, &llDatLen, pclRcvData, "{=TWS=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TWS=}", pclKeyTws, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyTwe, &llDatLen, pclRcvData, "{=TWE=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TWE=}", pclKeyTwe, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyTim, &llDatLen, pclRcvData, "{=TIM=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TIM=}", pclKeyTim, llDatLen);
        igTimeOut = atoi(pclKeyTim);

        pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=RECSEPA=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=RECSEPA=}", llDatLen);
        if (pclKeyPtr != NULL) {
            strcpy(pclRecSep, pclKeySep);
        } else {
            pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=SEPA=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=SEPA=}", llDatLen);
            if (pclKeyPtr != NULL) {
                strcpy(pclRecSep, pclKeySep);
            } else {
                strcpy(pclRecSep, "\n");
            }
        }

        pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=FLDSEPA=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=FLDSEPA=}", llDatLen);
        if (pclKeyPtr != NULL) {
            strcpy(pclFldSep, pclKeySep);
        } else {
            strcpy(pclFldSep, ",");
        }

        pclKeyPtr = GetKeyItem(pclKeyApp, &llDatLen, pclRcvData, "{=APP=}", "{=", TRUE);
        if (llDatLen > 0) {
            dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=APP=}", pclKeyApp, llDatLen);
            SetGrpMemberInfo(igMyGrpNbr, 9, pclKeyApp, 0);
        }

        pclKeyPtr = GetKeyItem(pclKeyCmd, &llDatLen, pclRcvData, "{=CMD=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=CMD=}", pclKeyCmd, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyTbl, &llDatLen, pclRcvData, "{=TBL=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TBL=}", pclKeyTbl, llDatLen);

        rlKeyDat.ItemPtr = GetKeyItem(rlKeyDat.Value, &rlKeyDat.ItemLen, pclRcvData, "{=DAT=}", "{=", FALSE);

        pclKeyPtr = GetKeyItem(pclKeyQue, &llDatLen, pclRcvData, "{=QUE=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=QUE=}", pclKeyQue, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyHex, &llDatLen, pclRcvData, "{=HEXIP=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=HEXIP=}", pclKeyHex, llDatLen);

        pclKeyPtr = GetKeyItem(pclKeyPak, &llDatLen, pclRcvData, "{=PACK=}", "{=", TRUE);
        slMaxPak = 0;
        if (llDatLen > 0) {
            dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=PACK=}", pclKeyPak, llDatLen);
            slMaxPak = atoi(pclKeyPak);
        }
        sprintf(pclKeyPak, "%d", slMaxPak);

        igTmpAlive = FALSE;
        pclKeyPtr = GetKeyItem(pclKeyAli, &llDatLen, pclRcvData, "{=ALIVE=}", "{=", TRUE);
        if (llDatLen > 0) {
            dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=ALIVE=}", pclKeyAli, llDatLen);
            if (strcmp(pclKeyAli, "CLOSE") != 0) {
                igHrgAlive = TRUE;
                igTmpAlive = TRUE;
                HandleHrgCmd(TRUE, "");
            }
        }
        rlRecDesc.UsedSize = 0;
        rlRecDesc.LineCount = 0;
        rlRecDesc.AppendBuf = FALSE;

        ilRC = RC_SUCCESS;
        for (ilCurEvt = 1; ((ilCurEvt <= ilEvtCnt)&&(ilRC == RC_SUCCESS)); ilCurEvt++) {
            if (ilMultEvt == TRUE) {
                sprintf(pclCurKey, "{=EVT_%d=}", ilCurEvt);
                pclEvtData = GetKeyItem(pclKeyFld, &llDatLen, pclRcvData, pclCurKey, "{=EVT_", FALSE);
                if (llDatLen > 0) dbg(TRACE, "%s EVENT SECTION %s LEN=%d", pclMyChild, pclCurKey, llDatLen);
                if (pclEvtData == NULL) {
                    dbg(TRACE, "%s KEY ITEM EVENT NOT FOUND", pclMyChild);
                    pclEvtData = pclRcvData;
                } /* end if */
            }/* end if */
            else {
                pclEvtData = pclRcvData;
            } /* end else */

            pclKeyPtr = GetKeyItem(pclKeyHnt, &llDatLen, pclEvtData, "{=ORAHINT=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=ORAHINT=}", pclKeyHnt, llDatLen);

            rlKeyWhe.ItemPtr = GetKeyItem(rlKeyWhe.Value, &rlKeyWhe.ItemLen, pclEvtData, "{=WHE=}", "{=", FALSE);
            rlKeyFld.ItemPtr = GetKeyItem(rlKeyFld.Value, &rlKeyFld.ItemLen, pclEvtData, "{=FLD=}", "{=", FALSE);

            pclKeyWhe = pclEmpty;
            pclKeyFld = pclEmpty;
            pclKeyDat = pclEmpty;
            if (rlKeyWhe.ItemPtr != NULL) {
                rlKeyWhe.Help[0] = rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen];
                rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen] = 0x00;
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=WHE=}", rlKeyWhe.ItemPtr, rlKeyWhe.ItemLen);
                pclKeyWhe = rlKeyWhe.ItemPtr;
            }
            if (rlKeyFld.ItemPtr != NULL) {
                rlKeyFld.Help[0] = rlKeyFld.ItemPtr[rlKeyFld.ItemLen];
                rlKeyFld.ItemPtr[rlKeyFld.ItemLen] = 0x00;
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=FLD=}", rlKeyFld.ItemPtr, rlKeyFld.ItemLen);
                pclKeyFld = rlKeyFld.ItemPtr;
            }
            if (rlKeyDat.ItemPtr != NULL) {
                rlKeyDat.Help[0] = rlKeyDat.ItemPtr[rlKeyDat.ItemLen];
                rlKeyDat.ItemPtr[rlKeyDat.ItemLen] = 0x00;
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=DAT=}", rlKeyDat.ItemPtr, rlKeyDat.ItemLen);
                pclKeyDat = rlKeyDat.ItemPtr;
            }

            sprintf(pclChkCmd, ",%s,", pclKeyCmd);
            ilSendToRouter = FALSE;

            if (strstr(",GFR,GFRC,GFRS,", pclChkCmd) != NULL) {
                if ((strstr(pclKeyWhe, "[CONFIG]") != NULL) ||
                        (strstr(pclKeyWhe, "[ROTATIONS]") != NULL) ||
                        (strstr(pclKeyWhe, "[USEFOGTAB,") != NULL)) {
                    ilSendToRouter = TRUE;
                } /* end if */
            } /* end if */

            if ((strstr(",RRA,", pclChkCmd) != NULL) && (ilSendToRouter == FALSE)) {
                dbg(TRACE, "%s RRA TRANSACTION RECEIVED (OLD STYLE)", pclMyChild);
                pclError = pclNoError;
                pclData = pclNoError;
            } else if ((strstr(",RT,RTA,GFR,GFRC,GFRS,", pclChkCmd) != NULL) &&
                    (ilSendToRouter == FALSE)) {
                ilSendToRouter = FALSE;
                rlRecDesc.FldSep = pclFldSep[0];
                rlRecDesc.RecSep = pclRecSep[0];
                if (ilCurEvt > 1) {
                    rlRecDesc.AppendBuf = TRUE;
                }
                if (ilCurEvt == ilEvtCnt) {
                    ilKeepOpen = FALSE;
                } /* end if */
                if (strcmp(pclKeyTbl, "FMLTAB") == 0) {
                    CheckFmlTemplate(pclMyChild, pclKeyTbl, pclKeyWhe);
                }
                ilRC = ReadCedaArray(pclMyChild, pclKeyTbl, pclKeyFld, pclKeyWhe, &rlRecDesc,
                        slMaxPak, ilKeepOpen, connfd, pclFldSep, pclRecSep, pclKeyHnt);
                if (ilRC == RC_SUCCESS) {
                    dbg(TRACE, "%s ReadCedaArray RETURN OK", pclMyChild);
                    pclError = pclNoError;
                    pclData = rlRecDesc.Value;
                    sprintf(pclKeyPak, "%d", rlRecDesc.LineCount);
                }/* end if */
                else {
                    dbg(TRACE, "%s ReadCedaArray RETURN ERROR", pclMyChild);
                    pclError = rlRecDesc.Value;
                    pclData = pclNoError;
                    strcpy(pclKeyPak, "0");
                } /* end else */
            } else if (strstr(",KEEP,ALIVE,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s KEEP ALIVE REQUEST RECEIVED", pclMyChild);
                ilSendToRouter = FALSE;
                pclError = pclNoError;
                pclData = pclAliveAnsw;
                HandleHrgCmd(TRUE, "");
                ilCmdAlive = TRUE;
                SetGrpMemberInfo(igMyGrpNbr, 3, "A", 2);
            } else if (strstr(",CLOSE,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s CLOSE ALIVE COMMAND RECEIVED", pclMyChild);
                ilSendToRouter = FALSE;
                pclError = pclNoError;
                pclData = pclNoError;
                ilCmdAlive = FALSE;
                close(connfd);
                Terminate();
            } else if (strstr(",GBCH,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s BROADCAST HISTORY REQUEST RECEIVED (NEW STYLE)", pclMyChild);
                ilSendToRouter = FALSE;
                rlRecDesc.FldSep = pclFldSep[0];
                rlRecDesc.RecSep = pclRecSep[0];
                ilRC = GetBcHistory(pclMyChild, pclKeyTbl, pclKeyWhe, pclKeyFld, pclKeyDat,
                        &rlRecDesc, slMaxPak, connfd);
                if (ilRC == RC_SUCCESS) {
                    pclError = pclNoError;
                    pclData = rlRecDesc.Value;
                    sprintf(pclKeyPak, "%d", rlRecDesc.LineCount);
                }/* end if */
                else {
                    pclError = rlRecDesc.Value;
                    pclData = pclNoError;
                    strcpy(pclKeyPak, "0");
                } /* end else */
            } else if (strstr(",BCOUT,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s KEEP ALIVE AS BC_OUT_CONNECTOR", pclMyChild);
                ilSendToRouter = FALSE;
                pclError = pclNoError;
                ilRC = HandleBcOut(pclMyChild, ilCpid, pclKeyHex, connfd);
                pclData = pclNoError;
            } else if (strstr(",BCKEY,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s BC_FILTER FOR BC_OUT_CONNECTOR", pclMyChild);
                ilSendToRouter = FALSE;
                ilRC = HandleBcKey(pclMyChild, pclKeyQue, pclKeyDat, connfd);
                pclError = pclNoError;
                pclData = pclNoError;
            } else if (strstr(",BCGET,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s RESEND BC TO BC_OUT_CONNECTOR", pclMyChild);
                ilSendToRouter = FALSE;
                ilRC = HandleBcGet(pclMyChild, pclKeyQue, pclKeyDat, connfd);
                pclError = pclNoError;
                pclData = pclNoError;
            } else if (strstr(",BCOFF,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s CLOSE BC_OUT_CONNECTOR", pclMyChild);
                ilSendToRouter = FALSE;
                pclError = pclNoError;
                ilRC = HandleBcOff(pclMyChild, pclKeyQue, pclKeyHex, connfd);
                pclData = pclNoError;
            } else if (strstr(",GBCN,", pclChkCmd) != NULL) {
                dbg(TRACE, "%s BCNUM REQUEST RECEIVED (NEW STYLE)", pclMyChild);
                ilSendToRouter = FALSE;
                pclError = pclNoError;
                pclData = pclNoError;
            }/* end else if */
            else {
                ilSendToRouter = TRUE;
            } /* end else */

            if (ilEvtCnt > 1) {
                if (rlKeyWhe.ItemPtr != NULL) rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen] = rlKeyWhe.Help[0];
                if (rlKeyFld.ItemPtr != NULL) rlKeyFld.ItemPtr[rlKeyFld.ItemLen] = rlKeyFld.Help[0];
                if (rlKeyDat.ItemPtr != NULL) rlKeyDat.ItemPtr[rlKeyDat.ItemLen] = rlKeyDat.Help[0];
            }

        } /* end for */
        dbg(DEBUG, "%s AFTER IF ", pclMyChild);
        ilRC = RC_SUCCESS;
        ilGotError = FALSE;
        pclOutData = NULL;
        rgKeyItemListOut.UsedLen = 0;

        if (ilSendToRouter == TRUE) {
            ilRC = HandleQueCreate(TRUE, pcgMyShortName, 1);
            if (ilRC != RC_SUCCESS) {
                pclData = pclNoError;
                pclError = pclAnyErrMsg;
                ilGotError = TRUE;
            }

            if (ilRC == RC_SUCCESS) {
                if (pclKeyTws[0] == '\0') {
                    strcpy(pclKeyTws, pclKeyIdn);
                }
                if (pclKeyTwe[0] == '\0') {
                    sprintf(pclKeyTwe, "%s,%s,%s", pclKeyHop, pclKeyExt, pclKeyApp);
                }

                dbg(DEBUG, "%s NOW SENDING SendCedaEvent ...", pclMyChild);
                if (igTimeOut <= 0) {
                    igTimeOut = 120;
                }
                ilTist = time(0L) % 10000;
                sprintf(pclKeyTim, "{-TIM-}%d,%d", ilTist, igTimeOut);
                ilRC = SendCedaEvent(igQueToRouter, /* adress to send the answer   */
                        igAnswerQueue, /* Set to temp queue ID        */
                        pclKeyUsr, /* BC_HEAD.dest_name           */
                        pclKeyWks, /* BC_HEAD.recv_name           */
                        pclKeyTws, /* CMDBLK.tw_start             */
                        pclKeyTwe, /* CMDBLK.tw_end               */
                        pclKeyCmd, /* CMDBLK.command              */
                        pclKeyTbl, /* CMDBLK.obj_name             */
                        pclKeyWhe, /* Selection Block             */
                        pclKeyFld, /* Field Block                 */
                        pclKeyDat, /* Data Block                  */
                        pclKeyTim, /* Error description           */
                        2, /* 0 or Priority               */
                        RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS      */

                /* warten auf Antwort vom Empfaenger */
                SetGrpMemberInfo(igMyGrpNbr, 1, "C", 0);
                ilRC = WaitAndCheckQueue(igTimeOut, igAnswerQueue, &prgItem);
                if (ilRC != RC_SUCCESS) {
                    pclData = pclNoError;
                    if (igGotTimeOut == TRUE) {
                        dbg(TRACE, "%s ERROR: GOT SERVER TIMEOUT <%s>", pcgMyProcName, pclKeyTim);
                        pclError = pclTimeOutMsg;
                    } else {
                        dbg(TRACE, "%s ERROR: QUE_GET FAILED", pcgMyProcName);
                        pclError = pclAnyErrMsg;
                    }
                    ilRC = HandleQueCreate(FALSE, pcgMyShortName, 1);
                    igGotTimeOut = FALSE;
                    ilRC = RC_SUCCESS;
                    if (igHsbDown == TRUE) {
                        close(connfd);
                        Terminate();
                    }
                } else {
                    pclData = pclNoError;
                    ilRC = TransLateEvent(&rgKeyItemListOut, TRUE);
                    if (rgKeyItemListOut.UsedLen > 0) {
                        if (rgKeyItemListOut.Value != NULL) {
                            /* dbg(TRACE,"TRANSLATED <%s>",rgKeyItemListOut.Value); */
                            pclOutData = rgKeyItemListOut.Value;
                        } else {
                            dbg(TRACE, "OUT LIST IS NULL !!");
                        }
                    }/* end if */
                    else if (rgKeyItemListOut.UsedLen == 0) {
                        dbg(TRACE, "EVENT WITHOUT DATA");
                    }/* end else if */
                    else {
                        switch (rgKeyItemListOut.UsedLen) {
                            case -1:
                                dbg(TRACE, "UNEXPECTED <BCOFF> RECEIVED");
                                break;
                            case -2:
                                dbg(TRACE, "UNEXPECTED <BCKEY> RECEIVED");
                                break;
                            default:
                                break;
                        }
                    } /* end else */
                } /* end else */
            } /* end else */
        } /* end if */

        /* TODO GFO 20130521 Add it
         * was causing some sig 11 from TrimAndFilterCr , maybe because of pclData or pclError was NULL
         */
        if (pclData == NULL) {
            pclData = pclNoError;
        }
        if (pclError == NULL) {
            pclError = pclNoError;
        }
        dbg(DEBUG, "%s Before  TrimAndFilterCr %d", pclMyChild, ilGotError);
        if ((ilRC == RC_SUCCESS) || (ilGotError == TRUE)) {
            if (pclOutData == NULL) {
                dbg(DEBUG, "%s Before  TrimAndFilterCr ", pclMyChild);
                TrimAndFilterCr(pclData, pclFldSep, pclRecSep);
                TrimAndFilterCr(pclError, pclFldSep, pclRecSep);

                ilOutDatLen = strlen(pclData);
                lgTotalByte += ilOutDatLen;
                ilOutDatLen += strlen(pclError) + 4096;
                if (ilOutDatLen > rgKeyItemListOut.AllocatedSize) {
                    rgKeyItemListOut.Value = (char *) realloc(rgKeyItemListOut.Value, ilOutDatLen);
                    rgKeyItemListOut.AllocatedSize = ilOutDatLen;
                }
                pclOutData = rgKeyItemListOut.Value;
                if (pclOutData == NULL) {
                    dbg(TRACE, "CAN'T ALLOC %d BYTES!!!", ilOutDatLen);
                    close(connfd);
                    Terminate();
                } /* end if */
                /* dbg(TRACE,"%s BUILDING MY OWN ANSWER",pclMyChild); */

                ilOutDatPos = 16;
                if (slMaxPak > 0) {
                    StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, pclKeyPak);
                } else {
                    StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, "0");
                }
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=ERR=}", 0, -1, pclError);
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=DAT=}", 0, -1, pclData);

                pclOutData[ilOutDatPos] = 0x00;
                ilBytesToSend = ilOutDatPos;
                dbg(TRACE, "%s TOTAL RECORDS=%d (BYTES=%d)", pclMyChild, lgTotalRecs, lgTotalByte);
                sprintf(pclDummy, "%09d", ilOutDatPos);
                ilOutDatPos = 0;
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=TOT=}", 0, -1, pclDummy);

                dbg(TRACE, "%s SENDING MY OWN ANSWER", pclMyChild);
                ilRC = SendDataToClient(pclMyChild, pclOutData, ilBytesToSend, connfd, TRUE);

            }/* end if */
            else {
                /* dbg(TRACE,"%s SENDING CEDA ANSWER",pclMyChild); */
                ilRC = SendDataToClient(pclMyChild, pclOutData, -1, connfd, TRUE);
            } /* end else */


        } /* end if */

        if ((igTmpAlive == TRUE) || (ilCmdAlive == TRUE)) {
            dbg(DEBUG, "%s GRP MEMBER (%d) STATUS IS: ALIVE", pclMyChild, igMyGrpNbr);
            HandleHrgCmd(TRUE, "");
            if (ilCmdAlive != TRUE) {
                SetGrpMemberInfo(igMyGrpNbr, 3, "T", 2);
            }
            ilEndAlive = FALSE;
        }

        lgEvtEndTime = time(0L);
        lgEvtDifTime = lgEvtEndTime - lgEvtBgnTime;
        if (lgEvtDifTime > 10) {
            dbg(TRACE, "%s CHECK TOTAL EVENT TIME (%d)", pclMyChild, lgEvtDifTime);
        }
        lgEvtBgnTime = time(0L);
    } /* end while not EndAlive */

    /* close connection to client */
    close(connfd); /* bleibt erst mal so ... kein shutdown */
    dbg(TRACE, "%s CONNECTION CLOSED", pclMyChild);

    ilRC = HandleQueCreate(FALSE, pcgMyShortName, 1);

    FreeRecordDescriptor(&rlRecDesc);

    close(connfd);
    dbg(TRACE, "===== %s TERMINATING ========", pclMyChild);
    Terminate();

    /* Will never come here hin */
    return RC_SUCCESS;

} /* end of child */


/* ======================================================================= */
/* ========================= CDR KERNEL ================================== */
/* THIS PART OF TOOL FUNCTIONS IS USED BY BOTH CDRHDL AND CDRCOM           */
/* ======================================================================= */

/******************************************************************************/
/* CheckQueue *****************************************************************/

/******************************************************************************/
static int CheckQueue(int ipModId, ITEM **prpItem, int ipNoWait) {
    int ilRC = RC_SUCCESS; /* Return code */
    int ilBreakOut = FALSE;
    int ilItemSize = 0;
    EVENT *prlEvent = NULL;

    ilItemSize = I_SIZE;

    while (ilBreakOut == FALSE) {
        if (ipNoWait == TRUE) {
            ilRC = que(QUE_GETBIGNW, ipModId, ipModId, PRIORITY_3, ilItemSize, (char *) prpItem);
            ilBreakOut = TRUE;
        } else {
            /* dbg(TRACE,"NOW READING FROM QUE %d",ipModId); */
            ilRC = que(QUE_GETBIG, ipModId, ipModId, PRIORITY_3, ilItemSize, (char *) prpItem);
            /* dbg(TRACE,"GOT EVENT FROM QUE %d",ipModId); */
        }
        if (ilRC == RC_SUCCESS) {
            prlEvent = (EVENT*) ((*prpItem)->text);

            switch (prlEvent->command) {
                case HSB_STANDBY:
                    dbg(TRACE, "CheckQueue: HSB_STANDBY");
                    HandleQueues();
                    ctrl_sta = prlEvent->command;
                    break;

                case HSB_COMING_UP:
                    dbg(TRACE, "CheckQueue: HSB_COMING_UP");
                    ctrl_sta = prlEvent->command;
                    break;

                case HSB_ACTIVE:
                    dbg(TRACE, "CheckQueue: HSB_ACTIVE");
                    ctrl_sta = prlEvent->command;
                    break;

                case HSB_ACT_TO_SBY:
                    dbg(TRACE, "CheckQueue: HSB_ACT_TO_SBY");
                    HandleQueues();
                    ctrl_sta = prlEvent->command;
                    break;

                case HSB_DOWN:
                    dbg(TRACE, "%s RECEIVED <HSB_DOWN> (PREPARE EXIT)", pcgMyProcName);
                    ilRC = que(QUE_ACK, 0, ipModId, 0, 0, NULL);
                    igHsbDown = TRUE;
                    ilRC = RC_FAIL;
                    ilBreakOut = TRUE;
                    break;

                case HSB_STANDALONE:
                    dbg(TRACE, "CheckQueue: HSB_STANDALONE");
                    ctrl_sta = prlEvent->command;
                    break;

                case SHUTDOWN:
                    dbg(TRACE, "%s RECEIVED <SHUTDOWN>", pcgMyProcName);
                    /* Acknowledge the item */
                    ilRC = que(QUE_ACK, 0, ipModId, 0, 0, NULL);
                    if (ilRC != RC_SUCCESS) {
                        /* handle que_ack error */
                        HandleQueErr(ilRC);
                    }
                    Terminate();
                    break;

                case RESET:
                    ilRC = Reset();
                    break;

                case EVENT_DATA:
                    /* DebugPrintItem(DEBUG,prgItem); */
                    /* DebugPrintEvent(DEBUG,prlEvent); */
                    ilBreakOut = TRUE;
                    break;

                case TRACE_ON:
                    dbg_handle_debug(prlEvent->command);
                    break;

                case TRACE_OFF:
                    dbg_handle_debug(prlEvent->command);
                    break;

                default:
                    dbg(TRACE, "CheckQueue: unknown event");
                    DebugPrintItem(TRACE, *prpItem);
                    DebugPrintEvent(TRACE, prlEvent);
                    break;

            } /* end switch */

            /* Acknowledge the item */
            ilRC = que(QUE_ACK, 0, ipModId, 0, 0, NULL);
            if (ilRC != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            }
        } else {
            if (ilRC != QUE_E_NOMSG) {
                dbg(TRACE, "CheckQueue: que(QUE_GETBIGNW,%d,%d,...) failed <%d>", ipModId, ipModId, ilRC);
                HandleQueErr(ilRC);
            }
            ilBreakOut = TRUE;
        }

    } /* end while */

    return ilRC;

} /* end of CheckQueue */





/*
 =================================================================================================
 Function GetKeyItem
 -------------------------------------------------------------------------------------------------
 Strips data from a textbuffer using a keyword and an indicator of the key's 'end of data'.

 Purpose and mainly use: find a text anchor by any pattern and determine the end of data
 by another pattern. This function will be used by the Full Document Interpreter (fdihdl)
 and by the Ceda Data Request Handler (cdrhdl) as well. It will be incorporated into the
 new 'accelerated communication' layer of the (roostering) client applications also.
 -------------------------------------------------------------------------------------------------
 There are 4 IN parameters:
 pcpTextBuff: The text stream that has to be analysed
 pcpKeyCode:  The keyword (pattern of anchor) of the text that is wanted
 pcpItemEnd:  The pattern that defines the end of the wanted text
 bpCopyData:  A flag that indicates if the identified data will be copied into the resultbuffer.
              This feature enables the caller to get the pointer to the data and the size of data
              regardless of the size of the given resultbuffer.
 -------------------------------------------------------------------------------------------------
 And 2 OUT parameters:
 pcpResultBuff: will contain the extracted data stream (if bpCopyData == true)
 pcpResultSize: always contains the size of the identified data.
 -------------------------------------------------------------------------------------------------
 Return Value:  The (char) pointer to the identified item data in the text or NULL.
 =================================================================================================

 Normal usage:
 -------------------------------------------------------------------------------------------------
 Let's assume that we got a text stream (pclTextBuff) like:

 "{=CMD=}RTA{=HOP=}ATH{=MODUL=}{=DATA=}Here is a lot of data\nwith several rows ....";

 and we want to extract the 'HOPO' section: We use the keyword {=HOP=} and as indicator for
 EndOfData we use "{=" from the beginning of the next keyword. Finally we decide to get the
 data back into the 'pclResult' buffer and set the flag CopyData to true.

 pclDataBegin = GetKeyItem(pclResult, &llSize, pclTextBuff, "{=HOP=}", "{=", true);
 if (pclDataBegin != NULL)
 {
   printf("{=HOP=} :\t<%s>\n",pclResult);
 }

 Now we receive the data in pclResult and llSize contains the length.
 Additionally the returned value in pclDataBegin points to the original position of the
 data inside the text stream of pclTextBuff for any purposes.
 -------------------------------------------------------------------------------------------------
 We have different possibilities to determine what we found:
 Case 1:
        CopyData set to true ...
        ... and pclDataBegin != NULL:   The keyword exists
        ... and llSize > 0 :            pclResult contains valid data
        ... (or llSize == 0:            The keyword section is empty, pclResult is set to '\0')
        CopyData set to true ...
        ... and pclDataBegin == NULL:   The keyword does not exist
        ... then llSize = 0 :           No data available
        ... and                         pclResult is set to '\0'
 Case 2:
        CopyData set to false ...       The result buffer remains unchanged
        ... and pclDataBegin != NULL:   The keyword exists
        ... and llSize > 0 :            pclDataBegin points to valid data
        ... (or llSize == 0:            The keyword section is empty)
        CopyData set to false ...
        ... and pclDataBegin == NULL:   The keyword does not exist
        ... then llSize == 0 and it makes no sense to proceed.
 -------------------------------------------------------------------------------------------------
 Note: (Special Usage)
 When you expect a very big data stream of several Mb you surely won't copy it into your local
 pclResult buffer. In such cases it is always the best way to set the CopyFlag to 'false': You
 will get the DataPointer and DataLength and you proceed working directly on the received data.
 =================================================================================================
 */

/* ======================================================================================== */
static char *GetKeyItem(char *pcpResultBuff, long *plpResultSize,
        char *pcpTextBuff, char *pcpKeyWord, char *pcpItemEnd, int bpCopyData) {
    long llDataSize = 0L;
    char *pclDataBegin = NULL;
    char *pclDataEnd = NULL;
    pclDataBegin = strstr(pcpTextBuff, pcpKeyWord); /* Search the keyword       */
    if (pclDataBegin != NULL) { /* Did we find it? Yes.     */
        pclDataBegin += strlen(pcpKeyWord); /* Skip behind the keyword  */
        pclDataEnd = strstr(pclDataBegin, pcpItemEnd); /* Search end of data       */
        if (pclDataEnd == NULL) { /* End not found?           */
            pclDataEnd = pclDataBegin + strlen(pclDataBegin); /* Take the whole string    */
        } /* end if */
        llDataSize = pclDataEnd - pclDataBegin; /* Now calculate the length */
        if (bpCopyData == TRUE) { /* Shall we copy?           */
            strncpy(pcpResultBuff, pclDataBegin, llDataSize); /* Yes, strip out the data  */
        } /* end if */
    } /* end if */
    if (bpCopyData == TRUE) { /* Allowed to set EOS?      */
        pcpResultBuff[llDataSize] = 0x00; /* Yes, terminate string    */
    } /* end if */
    *plpResultSize = llDataSize; /* Pass the length back     */
    return pclDataBegin; /* Return the data's begin  */
} /* end GetKeyItem */

/* ======================================================================================== */




static int StrgPutStrg(char *dest, int *destPos, char *src, int srcPos, int srcTo, char *deli) {
    int rc = RC_SUCCESS;
    if (srcTo < 0) {
        srcTo = strlen(src) - 1;
    } /* end if */
    while (srcPos <= srcTo) {
        dest[*destPos] = src[srcPos];
        (*destPos)++;
        srcPos++;
    } /* end while */
    srcPos = 0;
    srcTo = strlen(deli) - 1;
    while (srcPos <= srcTo) {
        dest[*destPos] = deli[srcPos];
        (*destPos)++;
        srcPos++;
    } /* end while */
    return rc;
} /* end of StrgPutStrg */

static void TrimAndFilterCr(char *pclTextBuff, char *pcpFldSep, char *pcpRecSep) {
    char *pclDest = NULL;
    char *pclSrc = NULL;
    char *pclLast = NULL; /* last non blank byte position */
    pclDest = pclTextBuff;
    pclSrc = pclTextBuff;
    pclLast = pclTextBuff - 1;
    dbg(DEBUG, "%s TRIMMING DATA ", pcgMyProcName);
    if (pclTextBuff != NULL) {
        dbg(DEBUG, "%s  DATA %s", pcgMyProcName, pclTextBuff);
    }

    while (*pclSrc != '\0') {
        if (*pclSrc != '\r') {
            if ((*pclSrc == pcpFldSep[0]) || (*pclSrc == pcpRecSep[0])) {
                pclDest = pclLast + 1;
            } /* end if */
            *pclDest = *pclSrc;
            if (*pclDest != ' ') {
                pclLast = pclDest;
            } /* end if */
            pclDest++;
        } /* end if */
        pclSrc++;
    } /* end while */
    pclDest = pclLast + 1;
    *pclDest = '\0';
    return;
} /* end TrimAndFilterCr */




/* =============================================================== */

/* =============================================================== */
static int SendDataToClient(char *pcpMyChild, char *pcpOutData, int ipOutLen, int connfd, int ipWaitForAck) {
    int ilRC = RC_SUCCESS;
    int ilBreakOut = FALSE;
    int ilBytesToSend = 0;
    int ilBytesSent = 0;
    int ilSentThisTime = 0;
    int pclRecvAck[64];
    char *pclOutPtr = NULL;

    pclOutPtr = pcpOutData;
    ilBytesToSend = ipOutLen;
    if (ilBytesToSend <= 0) {
        ilBytesToSend = strlen(pcpOutData);
    }
    ilBytesSent = 0;
    ilBreakOut = FALSE;
    dbg(DEBUG, "%s NOW SENDING %d BYTES (BC=%d)", pcpMyChild, ilBytesToSend, igRcvBcNum);
    /* dbg(TRACE,"%s DATA STREAM \n%s>", pcpMyChild,pcpOutData);  */
    do {
        ilSentThisTime = write(connfd, pclOutPtr, ilBytesToSend);
        dbg(DEBUG, "%s SENT %d/%d BYTES", pcpMyChild, ilSentThisTime, ilBytesToSend);
        pclOutPtr += ilSentThisTime;
        ilBytesSent += ilSentThisTime;
        ilBytesToSend -= ilSentThisTime;
        if (ilSentThisTime < 0) {
            dbg(TRACE, "%s ***** GOT SOCKET ERROR (%d BYTES) *****", pcpMyChild, ilSentThisTime);
            dbg(TRACE, "%s TOTAL SENT BYTES= %d", pcpMyChild, ilBytesSent);
            dbg(TRACE, "%s REMAINING BYTES = %d", pcpMyChild, ilBytesToSend);
            dbg(TRACE, "%s ********************************", pcpMyChild);
            ilBreakOut = TRUE;
        } /* end if */
    } while ((ilBytesToSend > 0) && (ilBreakOut == FALSE));

    if (ilBreakOut == TRUE) {
        close(connfd);
        Terminate();
    }

    if (ipWaitForAck == TRUE) {
        lgEvtEndTime = time(0L);
        lgEvtDifTime = lgEvtEndTime - lgEvtBgnTime;
        if (lgEvtDifTime > 10) {
            dbg(TRACE, "%s CHECK TOTAL EVENT TIME (%d)", pcpMyChild, lgEvtDifTime);
        } else {
            dbg(TRACE, "%s TOTAL TRANSACTION TIME (%d)", pcpMyChild, lgEvtDifTime);
        }
        lgEvtBgnTime = time(0L);
        ilRC = ReadMsgFromSock(connfd, &rgRcvBuff, TRUE);
    } /* end if */

    ilRC = RC_SUCCESS;
    return ilRC;
} /* end SendDataToClient */

/* =============================================================== */

/* =============================================================== */
static int TransLateEvent(STR_DESC* prpString, int ipTrimData) {
    int ilRC = RC_SUCCESS;
    int ilOutDatLen = 0;
    int ilOutDatPos = 0;
    char pclKeyTot[64];
    char pclKeyCmd[64];
    char pclKeyTbl[64];
    char pclKeyTws[64];
    char pclKeyTwe[64];
    char pclKeyUsr[64];
    char pclKeyWks[64];
    char pclKeyApp[64];
    char pclKeyQue[64];
    char pclKeyHex[64];
    char pclKeyBcn[64];
    char pclTmpBuf[64];
    char pclDummy[32];
    char pclChr[2];
    int ilFileSize = 0;
    int ilReadNow = 0;
    int ilNum = 0;
    int ilGrpPos = 0;
    int ilChrPos = 0;
    long llDatLen = 0;
    char *pclKeyPtr = NULL;
    char *pclEmpty = "";

    EVENT *prlEvent = NULL;
    BC_HEAD *prlBchead = NULL;
    CMDBLK *prlCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;
    char *pclError = NULL;
    char *pclNoError = "\0";
    char *pclOutData = NULL;

    dbg(DEBUG, "%s GOT CEDA EVENT", pcgMyProcName);
    igStopQueGet = FALSE;
    igStopBcHdl = FALSE;

    prpString->UsedLen = 0;

    prlEvent = (EVENT *) ((char *) prgItem->text);
    if (prlEvent->command == EVENT_DATA) {
        prlBchead = (BC_HEAD *) ((char *) prlEvent + sizeof (EVENT));
        if (prlBchead->rc != RC_FAIL) {
            pclError = pclNoError;
            prlCmdblk = (CMDBLK *) ((char *) prlBchead->data);
            pclSelection = (char *) prlCmdblk->data;
            pclFields = (char *) pclSelection + strlen(pclSelection) + 1;
            pclData = (char *) pclFields + strlen(pclFields) + 1;
            TrimAndFilterCr(pclData, ",", "\n");

        }/* end if */
        else {
            pclError = prlBchead->data;
            prlCmdblk = (CMDBLK *) ((char *) pclError + strlen(pclError) + 1);
            pclSelection = (char *) prlCmdblk->data;
            pclFields = (char *) pclSelection + strlen(pclSelection) + 1;
            pclData = (char *) pclFields + strlen(pclFields) + 1;
            pclData = pclNoError;
        } /* end else */

        pclOutData = prpString->Value;

        ilOutDatLen = prlEvent->data_length;
        if (strcmp(prlCmdblk->command, "BCQF") == 0) {
            igBcqDataSize = atoi(pclData);
            ilFileSize = igBcqDataSize;
            ilOutDatLen = ilFileSize + 1024;
        }
        if (ilOutDatLen > prpString->AllocatedSize) {
            if (prpString->AllocatedSize <= 0) {
                pclOutData = (char *) malloc(ilOutDatLen);
                if (pclOutData == NULL) {
                    dbg(TRACE, "%s CAN'T ALLOC %d BYTES!!!", pcgMyProcName, ilOutDatLen);
                    Terminate();
                } /* end if */
                prpString->AllocatedSize = ilOutDatLen;
                prpString->Value = pclOutData;
            }/* end if */
            else {
                pclOutData = (char*) realloc((char*) pclOutData, ilOutDatLen);
                if (pclOutData == NULL) {
                    dbg(TRACE, "%s CAN'T RE-ALLOC %d BYTES!!!", pcgMyProcName, ilOutDatLen);
                    Terminate();
                } /* end if */
                prpString->AllocatedSize = ilOutDatLen;
                prpString->Value = pclOutData;
            } /* end else */
        } /* end if */

        prpString->UsedLen = 0;

        igCheckFile = FALSE;
        igClearFile = FALSE;
        if (strcmp(prlCmdblk->command, "BCQF") == 0) {
            igCheckFile = TRUE;
            sprintf(pcgBcqFullPath, "%s/%s", pcgBcqPath, pclSelection);
            dbg(TRACE, "%s OPENING FILE <%s>", pcgMyProcName, pcgBcqFullPath);
            memset(pclOutData, 0x00, ilFileSize + 4);
            pfgBcqPtr = fopen(pcgBcqFullPath, "r+");
            if (pfgBcqPtr != NULL) {
                dbg(TRACE, "%s READING %d BYTES", pcgMyProcName, ilFileSize);
                ilReadNow = fread((void *) pclOutData, 1, ilFileSize, pfgBcqPtr);
                if (ilReadNow != ilFileSize) {
                    dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, ilFileSize, ilReadNow);
                    dbg(TRACE, "%s GOT FROM FILE <%s>", pcgMyProcName, pclOutData);
                }
                dbg(TRACE, "%s GOT FILE AS MEMBER <%s>", pcgMyProcName, pclFields);
                ilGrpPos = atoi(pclFields);
                ilChrPos = ilFileSize + ilGrpPos + 1;
                fseek(pfgBcqPtr, ilChrPos, SEEK_SET);
                fwrite((void *) ".", 1, 1, pfgBcqPtr);
                fflush(pfgBcqPtr);
                fclose(pfgBcqPtr);
                prpString->UsedLen = ilFileSize;
            }
            pclSelection = GetKeyItem(pclKeyCmd, &llDatLen, pclOutData, "{=WHE=}", "{=", FALSE);
            if (pclSelection == NULL) {
                pclSelection = pclEmpty;
            }
            pclKeyPtr = GetKeyItem(pclKeyCmd, &llDatLen, pclOutData, "{=CMD=}", "{=", TRUE);
            pclKeyPtr = GetKeyItem(pclKeyBcn, &llDatLen, pclOutData, "{=BCNUM=}", "{=", TRUE);
            strcpy(pclTmpBuf, "0");
            ilNum = atoi(pclKeyBcn);
            if (ilNum > 0) {
                igRcvBcNum = atoi(pclKeyBcn);
                igRcvBcCnt++;
                sprintf(pclTmpBuf, "%d", igRcvBcCnt);
            }
            StrgPutStrg(pclOutData, &ilFileSize, "{=PACK=}", 0, -1, pclTmpBuf);
            pclOutData[ilFileSize] = 0x00;
            prpString->UsedLen = ilFileSize;
            sprintf(pclKeyTot, "%09d", ilFileSize);
            ilOutDatPos = 0;
            StrgPutStrg(pclOutData, &ilOutDatPos, "{=TOT=}", 0, -1, pclKeyTot);
        } else {
            memset(pclKeyUsr, '\0', sizeof (prlBchead->dest_name) + 1);
            strncpy(pclKeyUsr, prlBchead->dest_name, sizeof (prlBchead->dest_name));
            memset(pclKeyWks, '\0', sizeof (prlBchead->recv_name) + 1);
            strncpy(pclKeyWks, prlBchead->recv_name, sizeof (prlBchead->recv_name));
            memset(pclKeyCmd, '\0', sizeof (prlCmdblk->command) + 1);
            strncpy(pclKeyCmd, prlCmdblk->command, sizeof (prlCmdblk->command));
            memset(pclKeyTws, '\0', sizeof (prlCmdblk->tw_start) + 1);
            strncpy(pclKeyTws, prlCmdblk->tw_start, sizeof (prlCmdblk->tw_start));
            memset(pclKeyTwe, '\0', sizeof (prlCmdblk->tw_end) + 1);
            strncpy(pclKeyTwe, prlCmdblk->tw_end, sizeof (prlCmdblk->tw_end));
            memset(pclKeyTbl, '\0', sizeof (prlCmdblk->obj_name) + 1);
            strncpy(pclKeyTbl, prlCmdblk->obj_name, sizeof (prlCmdblk->obj_name));
            sprintf(pclKeyBcn, "%d", prlBchead->bc_num);
            strcpy(pclTmpBuf, "0");
            if (prlBchead->bc_num > 0) {
                igRcvBcNum = atoi(pclKeyBcn);
                igRcvBcCnt++;
                sprintf(pclTmpBuf, "%d", igRcvBcCnt);
            }

            ilOutDatPos = 16;

            if (igProcIsBcCom == TRUE)
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, pclTmpBuf);

            if (igProcIsBcCom == TRUE)
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=BCNUM=}", 0, -1, pclKeyBcn);
            if (pclKeyCmd[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=CMD=}", 0, -1, pclKeyCmd);
            if (pclKeyTbl[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=TBL=}", 0, -1, pclKeyTbl);
            if (pclKeyUsr[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=USR=}", 0, -1, pclKeyUsr);
            if (pclKeyWks[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=WKS=}", 0, -1, pclKeyWks);
            if (pclKeyTws[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=TWS=}", 0, -1, pclKeyTws);
            if (pclKeyTwe[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=TWE=}", 0, -1, pclKeyTwe);
            if (prlBchead->rc == RC_FAIL)
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=ERR=}", 0, -1, pclError);
            if (pclSelection[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=WHE=}", 0, -1, pclSelection);
            if (pclFields[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=FLD=}", 0, -1, pclFields);
            if (pclData[0] != '\0')
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=DAT=}", 0, -1, pclData);

            pclOutData[ilOutDatPos] = 0x00;
            prpString->UsedLen = ilOutDatPos;
            sprintf(pclKeyTot, "%09d", ilOutDatPos);
            ilOutDatPos = 0;
            StrgPutStrg(pclOutData, &ilOutDatPos, "{=TOT=}", 0, -1, pclKeyTot);
        }

        if (strcmp(pclKeyCmd, "BCOFF") == 0) {
            prpString->UsedLen = -1;
        }/* end if */
        else if (strcmp(pclKeyCmd, "BCKEY") == 0) {
            prpString->UsedLen = -2;
            if (pclSelection[0] != '\0') {
                strcpy(pcgBcKeyFlag, "F");
            } else {
                strcpy(pcgBcKeyFlag, "E");
            }
        }/* end if */
        else if (strcmp(pclKeyCmd, "BCGET") == 0) {
            prpString->UsedLen = -3;
        }/* end if */
        else if (strcmp(pclKeyCmd, "CLO") == 0) {
            dbg(TRACE, "%s GOT <CLO> COMMAND (BCHDL CLOSED)", pcgMyProcName);
            igStopBcHdl = TRUE;
        }/* end if */
        else if (strcmp(pclKeyCmd, "QCPC") == 0) {
            dbg(TRACE, "%s GOT <QCPC> COMMAND (RESET FROM MAIN)", pcgMyProcName);
            igStopQueGet = TRUE;
        } /* end if */


        ilRC = RC_SUCCESS;
    }/* end if */
    else {
        dbg(TRACE, "TRANSLATE: EVENT WITHOUT DATA (TYPE=%d)", prlEvent->command);
        ilRC = RC_FAIL;
    } /* end else */
    return ilRC;
} /* end TransLateEvent */

/* =============================================================== */

/* =============================================================== */
static void CheckBcqFile(void) {
    struct stat rlLfBuff;
    int ilReadNow = 0;
    int ilMaxPos = 0;
    int ilChrPos = 0;
    char pclChr[4] = " ";
    pfgBcqPtr = fopen(pcgBcqFullPath, "r+");
    if (pfgBcqPtr != NULL) {
        ilChrPos = igBcqDataSize;
        fseek(pfgBcqPtr, ilChrPos, SEEK_SET);
        ilReadNow = fread((void *) pclChr, 1, 1, pfgBcqPtr);
        if (pclChr[0] == '!') {
            dbg(TRACE, "%s FILE IS READY TO CLEAR", pcgMyProcName);
            igCheckFile = TRUE;
        } else {
            dbg(TRACE, "%s BCQF <%s> (%s) BCHDL STILL BUSY (NO CHECK)", pcgMyProcName, pcgBcqFullPath, pclChr);
            igCheckFile = FALSE;
        }
        if (igCheckFile == TRUE) {
            if (fstat(fileno(pfgBcqPtr), &rlLfBuff) != RC_FAIL) {
                /* ilMaxPos = rlLfBuff.st_size - igBcqDataSize; */
                ilMaxPos = GRP_REC_SIZE;
                memset(pcgGrpRec0, 0x00, ilMaxPos + 1);
                fseek(pfgBcqPtr, ilChrPos, SEEK_SET);
                ilReadNow = fread((void *) pcgGrpRec0, 1, ilMaxPos, pfgBcqPtr);
                dbg(TRACE, "%s BCQ FILE STATUS <%s>", pcgMyProcName, pcgGrpRec0);
                if (ilReadNow == ilMaxPos) {
                    ilChrPos = 1;
                    while ((ilChrPos < ilMaxPos) &&
                            (pcgGrpRec0[ilChrPos] == '.') &&
                            (pcgGrpRec0[ilChrPos] != '*')) {
                        ilChrPos++;
                    }
                    if (pcgGrpRec0[ilChrPos] == '*') {
                        dbg(TRACE, "%s ALL MEMBERS GOT THE FILE", pcgMyProcName);
                        igClearFile = TRUE;
                    }
                }
            }
        }
        fclose(pfgBcqPtr);
        if (igClearFile == TRUE) {
            remove(pcgBcqFullPath);
            dbg(TRACE, "%s  FILE <%s> REMOVED", pcgMyProcName, pcgBcqFullPath);
        }
    }
    return;
}


/* =============================================================== */

/* =============================================================== */
static long GetMyCurSize(int ipUsePid, char *pcpWho, int ipFlag) {
    long llMySize = 1;
#if defined(_SOLARIS)
    int ilMyPid = 0;
    int ilFd = -1;
    char pclFile[64];
    struct prpsinfo p;
    if (ipFlag == TRUE) {
        dbg(DEBUG, "%s %s SIZE CHECK", pcgMyProcName, pcpWho);
    }
    sprintf(pclFile, "/proc/%d", ipUsePid);
    if ((ilFd = open(pclFile, O_RDONLY)) >= 0) {
        errno = 0;
        if (ioctl(ilFd, PIOCPSINFO, (void *) &p) >= 0) {
            llMySize = p.pr_size * 8;
            /*
            printf("Size of this process is %ld KB, resident %ld KB\n\n",
            p.pr_size*8,p.pr_rssize*8);
             */
        } else {
            dbg(TRACE, "Can't read process information from file <%s>", pclFile);
            dbg(TRACE, "Errno %d %s", errno, strerror(errno));
        }
        close(ilFd);
    } else {
        dbg(TRACE, "Can't open process information from file <%s>", pclFile);
    }
    if (ipFlag == TRUE) {
        dbg(TRACE, "%s %s SIZE = %d K", pcgMyProcName, pcpWho, llMySize);
    }
#endif
    return llMySize;
}

/* *************************************************************** */

/* *************************************************************** */
static int ReadMsgFromSock(int connfd, STR_DESC *rpMsg, int ipGetAck) {
    int ilRC = RC_SUCCESS;
    int ilStep = 0;
    int ilRcvNow = 0;
    int ilGotNow = 0;
    int ilRcvLen = 0;
    int ilBreakOut = FALSE;
    long llDatLen = 0;
    char pclKeyVal[32];
    char *pclPtr = NULL;

    if (ipGetAck == FALSE) {
        SetGrpMemberInfo(igMyGrpNbr, 1, "R", 0);
    } else {
        dbg(TRACE, "%s WAITING FOR ACK FROM CLIENT", pcgMyProcName);
        SetGrpMemberInfo(igMyGrpNbr, 1, "A", 0);
    }
    igWaitForAck = ipGetAck;
    ilBreakOut = FALSE;
    igGotAlarm = FALSE;
    ilRcvNow = 16;
    rpMsg->UsedLen = 0;
    while ((ilStep < 2) && (ilBreakOut == FALSE)) {
        ilStep++;
        ilRcvLen = ilRcvNow + 16 + 1;
        if (rpMsg->AllocatedSize < ilRcvLen) {
            rpMsg->Value = realloc(rpMsg->Value, ilRcvLen);
            rpMsg->AllocatedSize = ilRcvLen;
        }
        pclPtr = rpMsg->Value + rpMsg->UsedLen;
        *pclPtr = 0x00;
        dbg(DEBUG, "%s READING %d BYTES FROM SOCKET", pcgMyProcName, ilRcvNow);
        ilRcvLen = 0;
        while ((ilRcvLen < ilRcvNow) && (ilBreakOut == FALSE)) {
            alarm(igAlarmStep);
            ilGotNow = read(connfd, pclPtr, ilRcvNow);
            alarm(0);
            if (igGotAlarm == TRUE) {
                dbg(DEBUG, "%s GOT (%d) NOTHING FROM CLIENT (WAITED %d SEC)",
                        pcgMyProcName, ilGotNow, igAlarmStep);
                if (ilGotNow > 0) {
                    ilRcvLen += ilGotNow;
                    pclPtr += ilGotNow;
                    rpMsg->UsedLen += ilGotNow;
                }
                if (rpMsg->UsedLen > 0) {
                    dbg(TRACE, "%s INPUT STOPPED AFTER %d BYTES", pcgMyProcName, rpMsg->UsedLen);
                    *pclPtr = 0x00;
                    dbg(TRACE, "%s INPUT DATA STREAM <%s>", pcgMyProcName, rpMsg->Value);
                    ilBreakOut = TRUE;
                }
                if (ilBreakOut == FALSE) {
                    CheckChildStatus();
                    if (igShutDownChild == TRUE) {
                        ilBreakOut = TRUE;
                    }
                }
                igGotAlarm = FALSE;
            } else {
                if (ilGotNow > 0) {
                    ilRcvLen += ilGotNow;
                    pclPtr += ilGotNow;
                    rpMsg->UsedLen += ilGotNow;
                    if (ilRcvLen < ilRcvNow) {
                        dbg(TRACE, "%s RECEIVED THIS TIME %d BYTES", pcgMyProcName, ilGotNow);
                        *pclPtr = 0x00;
                        if (strcmp(rpMsg->Value, "{=ACK=}") == 0) {
                            dbg(TRACE, "%s RECEIVED <%s> FROM CLIENT", pcgMyProcName, rpMsg->Value);
                            ilBreakOut = TRUE;
                        }
                    }
                }
                if (ilGotNow < 0) {
                    dbg(TRACE, "%s SOCKET ERROR (%d) DETECTED", pcgMyProcName, ilGotNow);
                    ilBreakOut = TRUE;
                }
                if (ilGotNow == 0) {
                    dbg(TRACE, "%s CONNECTION CLOSED BY CLIENT", pcgMyProcName);
                    ilBreakOut = TRUE;
                }
            }
        } /* end while read  */
        *pclPtr = 0x00;

        if (ilBreakOut == FALSE) {
            if (ilStep == 1) {
                GetKeyItem(pclKeyVal, &llDatLen, rpMsg->Value, "{=TOT=}", "{=", TRUE);
                /* dbg(TRACE,"%s %s <%s> LEN=%d", pcgMyProcName,"{=TOT=}", pclKeyVal, llDatLen); */
                ilRcvNow = atoi(pclKeyVal) - 16;
                if (ilRcvNow <= 0) {
                    dbg(TRACE, "%s RECEIVED <%s> FROM CLIENT", pcgMyProcName, rpMsg->Value);
                    dbg(DEBUG, "%s NO FURTHER DATA EXPECTED", pcgMyProcName);
                    ilBreakOut = TRUE;
                }
            }
        }
    } /* end while ilStep < 2*/
    *pclPtr = 0x00;

    if ((ilBreakOut == FALSE) && (ipGetAck == TRUE)) {
        GetKeyItem(pclKeyVal, &llDatLen, rpMsg->Value, "{=ACK=}", "{=", TRUE);
        dbg(TRACE, "%s RECEIVED %s <%s> LEN=%d", pcgMyProcName, "{=ACK=}", pclKeyVal, llDatLen);
        if (llDatLen > 0) {
            if (strcmp(pclKeyVal, "CLOSE") == 0) {
                ilBreakOut = TRUE;
            }
            if (strcmp(pclKeyVal, "NEXT") == 0) {
                igHrgAlive = FALSE;
                igTmpAlive = TRUE;
            }
            if (strcmp(pclKeyVal, "KEEP") == 0) {
                igHrgAlive = TRUE;
                igTmpAlive = TRUE;
            }
            if (strcmp(pclKeyVal, "ALIVE") == 0) {
                igHrgAlive = TRUE;
                igTmpAlive = TRUE;
            }
        } else {
            if (strstr(rpMsg->Value, "{=ACK=}") != NULL) {
                dbg(TRACE, "%s FOUND {=ACK=} IN UNEXPECTED MESSAGE FORMAT <%s>", pcgMyProcName, rpMsg->Value);
            } else {
                dbg(TRACE, "%s EXPECTED ACK NOT FOUND IN <%s>", pcgMyProcName, rpMsg->Value);
            }
            ilBreakOut = TRUE;
        }
    }

    if (ilBreakOut == TRUE) {
        close(connfd);
        Terminate();
    }

    igWaitForAck = FALSE;

    return ilRC;
}

/* *************************************************************** */

/* *************************************************************** */
static void CheckChildStatus(void) {
    int ilRC = RC_SUCCESS;
    int ilMyParentPid = -1;
    char pclTmpBuf[32];
    struct stat rlLfBuff;

    /* =================================== */
    /* Checking any Task Set by Main (Cmd) */
    /* =================================== */
    CheckChildTask(TRUE);

    /* =================================== */
    /* Check Lost Parent or Logfile Switch */
    /* =================================== */
    ilMyParentPid = getppid();
    GetGrpMemberInfo(igMyGrpNbr, 3, pcgOwnCmd, pcgOwnDat);
    if ((ilMyParentPid != igMyParentPid) || (pcgOwnDat[0] == 'S')) {
        if (ilMyParentPid != igMyParentPid) {
            dbg(TRACE, "%s LOST MY PARENT (OLD=%d) (NEW=%d)", pcgMyProcName, igMyParentPid, ilMyParentPid);
            SetGrpMemberInfo(igMyGrpNbr, 3, "?", 0);
        }
        GetGrpMemberInfo(igMyGrpNbr, 6, pcgOwnCmd, pcgOwnDat);
        GetGrpMemberInfo(0, 6, pcgTmpCmd, pcgTmpDat);
        if (strcmp(pcgOwnDat, pcgTmpDat) != 0) {
            ilRC = SwitchToNewLogFile(pcgOwnDat, pcgTmpDat, "NEW PARENT LOG");
            SetGrpMemberInfo(igMyGrpNbr, 3, "S", 0);
            SetGrpMemberInfo(igMyGrpNbr, 6, pcgTmpDat, 0);
            igMyParentPid = ilMyParentPid;
        }
    }

    /* =================================== */
    /* Tell Main to switch too big logfile */
    /* =================================== */
    if (fstat(fileno(outp), &rlLfBuff) != RC_FAIL) {
        if (rlLfBuff.st_size > igMaxFileSize) {
            dbg(DEBUG, "%s LOGFILE SIZE=%d EXCEEDS LIMIT (%d)", pcgMyProcName, rlLfBuff.st_size, igMaxFileSize);
            SetMainCmd("S", igMyGrpNbr);
        }
    }

    /* =================================== */
    /* Close open Sessions and Queues      */
    /* =================================== */
    ilRC = HandleOraLogin(FALSE, "");
    if (igProcIsBcCom == FALSE) {
        ilRC = HandleQueCreate(FALSE, "", 1);
    }

    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void CheckMainTask(int ipFullCheck) {
    int ilRecPos = 0;
    int i = 0;
    int j = 0;
    int ilOldDbg = 0;
    int ilReadNow = 0;

    GetGrpMemberInfo(0, 3, pcgOwnCmd, pcgOwnDat);

    if (pcgOwnCmd[0] != '.') {
        SetGrpMemberInfo(0, 0, ".", 0);
        ilRecPos = GRP_REC_SIZE;
        fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
        ilReadNow = fread((void *) pcgGrpRec1, 1, GRP_REC_SIZE, pfgCdrGrp);
        if (ilReadNow != GRP_REC_SIZE) {
            dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
            return;
        }
        j = GRP_REC_SIZE - 1;
        for (i = 1; i < j; i++) {
            if (pcgGrpRec1[i] != '.') {
                switch (pcgGrpRec1[i]) {
                    case 'S':
                        MainCheckLogSize(FALSE);
                        SetGrpMemberInfo(i, 0, ".", 1);
                        break;
                    default:
                        /* Keep all other entries ! */
                        break;
                }
            }
        }
    } else {
        SetGrpMemberInfo(0, 1, "", 0);
    }
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void CheckChildTask(int ipFullCheck) {
    int ilOldDbg = 0;
    static char pclOwnDat[GRP_DAT_SIZE + 1] = "";
    static char pclTmpDat[GRP_DAT_SIZE + 1] = "";
    static char pclOwnCmd[4] = "";
    static char pclTmpCmd[4] = "";

    GetGrpMemberInfo(igMyGrpNbr, 3, pcgOwnCmd, pcgOwnDat);
    igStatusAck = FALSE;

    if (pcgOwnCmd[0] != '.') {
        switch (pcgOwnCmd[0]) {
            case 'T':
                debug_level = TRACE;
                dbg(TRACE, "%s (DBG) LOGFILE SET TO TRACE MODE", pcgMyProcName);
                SetGrpMemberInfo(igMyGrpNbr, 7, "", 0);
                break;
            case 'F':
                debug_level = DEBUG;
                dbg(TRACE, "%s (DBG) LOGFILE SET TO FULL MODE", pcgMyProcName);
                SetGrpMemberInfo(igMyGrpNbr, 7, "", 0);
                break;
            case 'O':
                dbg(TRACE, "%s (DBG) LOGFILE SET TO OFF MODE", pcgMyProcName);
                SetGrpMemberInfo(igMyGrpNbr, 7, "", 0);
                debug_level = 0;
                break;
            case 'D':
                debug_level = TRACE;
                dbg(TRACE, "%s RECEIVED SHUTDOWN FROM PARENT (MAIN)", pcgMyProcName);
                if (igGroupShutDown == TRUE) {
                    dbg(TRACE, "%s CONFIG: SHUTDOWN ALL GRP MEMBERS", pcgMyProcName);
                    igShutDownChild = TRUE;
                } else {
                    dbg(TRACE, "%s CONFIG: IGNORE GROUP SHUTDOWN", pcgMyProcName);
                    igShutDownChild = FALSE;
                }
                break;
            case 'K':
                debug_level = TRACE;
                dbg(TRACE, "%s RECEIVED HSB_DOWN FROM PARENT (SYSMON)", pcgMyProcName);
                igShutDownChild = TRUE;
                break;
            case 'S':
                ilOldDbg = debug_level;
                debug_level = TRACE;
                GetGrpMemberInfo(igMyGrpNbr, 6, pclOwnCmd, pclOwnDat);
                GetGrpMemberInfo(0, 6, pclTmpCmd, pclTmpDat);
                SwitchToNewLogFile(pclOwnDat, pclTmpDat, "LOG SIZE SWITCH");
                SetGrpMemberInfo(igMyGrpNbr, 0, ".", 0);
                debug_level = ilOldDbg;
                break;
            case 'A':
                dbg(DEBUG, "%s ===== CHILD STATUS: %s MEMBER (%d) ALIVE",
                        pcgMyProcName, pcgMyShortName, igMyGrpNbr);
                igStatusAck = TRUE;
                break;
            default:
                break;
        }
        SetGrpMemberInfo(igMyGrpNbr, 0, ".", 0);
    } else {
        SetGrpMemberInfo(igMyGrpNbr, 1, "", 0);
    }
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static int InitCdrGrpFile(int ipReSync) {
    int ilRC = RC_SUCCESS;
    int ilFileSize = 0;
    int i = 0;
    char *Ptmp = NULL;
    char pclFile[128];
    char pclTmp[128];
    struct stat rlStat;

    if (pfgCdrGrp != NULL) {
        fclose(pfgCdrGrp);
        pfgCdrGrp = NULL;
        dbg(DEBUG, "%s DETACHED FROM MAIN GROUP FILE ", pcgMyProcName);
        dbg(DEBUG, "%s NOW ATTACHED TO MY GROUP FILE ", pcgMyProcName);
    }

    if ((Ptmp = getenv("DBG_PATH")) == NULL) {
        dbg(TRACE, "%s ERROR Reading DBG_PATH", pcgMyProcName);
        ilRC = RC_FAIL;
    } else {
        sprintf(pclFile, "%s/%s_members.ctl", Ptmp, mod_name);
        dbg(DEBUG, "%s GROUP FILE <%s>", pcgMyProcName, pclFile);
    } /* fi */

    if (ilRC == RC_SUCCESS) {
        pfgCdrGrp = fopen(pclFile, "r+");
        if (pfgCdrGrp == NULL) {
            pfgCdrGrp = fopen(pclFile, "w+");
            if (pfgCdrGrp != NULL) {
                fclose(pfgCdrGrp);
            } /* end if */
            pfgCdrGrp = fopen(pclFile, "r+");
        } /* end if */
    } /* end if */

    if (ilRC == RC_SUCCESS) {
        if (pfgCdrGrp == NULL) {
            dbg(TRACE, "%s UNABLE TO OPEN CONTROL FILE <%s>", pcgMyProcName, pclFile);
            ilRC = RC_FAIL;
        } /* end if */
    } /* end if */

    memset(pcgGrpRec0, '.', GRP_REC_SIZE);
    pcgGrpRec0[GRP_REC_SIZE] = '\0';
    pcgGrpRec0[GRP_REC_SIZE - 1] = '\n';
    memset(pcgGrpDat, '.', GRP_DAT_SIZE);
    pcgGrpDat[GRP_DAT_SIZE] = '\0';
    pcgGrpDat[GRP_DAT_SIZE - 1] = '\n';

    if (ilRC == RC_SUCCESS) {
        dbg(DEBUG, "%s OPENED GROUP CONTROL FILE", pcgMyProcName);
        ilRC = fstat(fileno(pfgCdrGrp), &rlStat);
        dbg(DEBUG, "%s CURRENT FILE SIZE: %d", pcgMyProcName, rlStat.st_size);
        ilFileSize = (GRP_REC_SIZE * 3)+(GRP_DAT_SIZE * GRP_REC_SIZE);
        if (rlStat.st_size < ilFileSize) {
            dbg(DEBUG, "%s MUST CREATE %s GROUP FILE", pcgMyProcName, pcgMyShortName);
            dbg(DEBUG, "%s CURRENT FILE SIZE: %d", pcgMyProcName, rlStat.st_size);
            pcgGrpRec0[0] = '.';
            fseek(pfgCdrGrp, 0, SEEK_SET);
            fwrite((void *) pcgGrpRec0, GRP_REC_SIZE, 1, pfgCdrGrp);
            fflush(pfgCdrGrp);
            pcgGrpRec0[0] = '.';
            fwrite((void *) pcgGrpRec0, GRP_REC_SIZE, 1, pfgCdrGrp);
            fflush(pfgCdrGrp);
            fwrite((void *) pcgGrpRec0, GRP_REC_SIZE, 1, pfgCdrGrp);
            fflush(pfgCdrGrp);
            for (i = 0; i < GRP_REC_SIZE; i++) {
                sprintf(pclTmp, "%04d", i);
                strncpy(pcgGrpDat, pclTmp, 4);
                fwrite((void *) pcgGrpDat, GRP_DAT_SIZE, 1, pfgCdrGrp);
                fflush(pfgCdrGrp);
            }
        } /* end else */
    } /* end if */

    return ilRC;
}

/* *************************************************************** */

/* *************************************************************** */
static int GetNewMemberNbr(void) {
    int ilRecPos = 0;
    int ilNbr = -1;
    int i = 0;
    int ilReadNow = 0;
    ilRecPos = GRP_REC_SIZE * 1;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec1, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }
    ilRecPos = GRP_REC_SIZE * 2;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec2, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }

    if (ilNbr <= 0) {
        for (i = 1; ((i < GRP_REC_SIZE)&&(ilNbr < 1)); i++) {
            if ((pcgGrpRec2[i] == '.') && (pcgGrpRec1[i] == '.')) {
                ilNbr = i;
            }
        }
    }

    if (ilNbr <= 0) {
        for (i = 1; ((i < GRP_REC_SIZE)&&(ilNbr < 1)); i++) {
            if ((pcgGrpRec2[i] == '.') ||
                    ((pcgGrpRec1[i] >= '0') && (pcgGrpRec1[i] <= '9'))) {
                ilNbr = i;
            }
        }
    }

    if (ilNbr > 0) {
        SetGrpMemberInfo(ilNbr, 1, "-", 0);
        SetGrpMemberInfo(ilNbr, 0, ".", 0);
        SetGrpMemberInfo(ilNbr, 0, ".", 1);
    }
    return ilNbr;
}

/* *************************************************************** */

/* *************************************************************** */
static void SetGrpMemberInfo(int ipWho, int ipForWhat, char *pcpInfo, int ipOff) {
    int ilRecPos = -1;
    int ilLen = 0;
    int ilMainPid = 0;
    char pclDat[GRP_DAT_SIZE + 1];
    char pclTyp[2] = "";
    char pclDbg[2] = "";
    int ilReadNow = 0;
    if (pfgCdrGrp != NULL) {
        fseek(pfgCdrGrp, 0, SEEK_SET);
        ilReadNow = fread((void *) pclTyp, 1, 1, pfgCdrGrp);
    }
    if ((ipWho >= 0) && (pfgCdrGrp != NULL) && (pclTyp[0] != '!')) {
        GetServerTimeStamp("UTC", 1, 0, pcgTimeStamp);
        strcpy(pclDat, pcpInfo);
        if (ipWho > 0) {
            dbg(DEBUG, "%s SET MEMBER STATUS WHO=%d TYP=%d INF <%s>",
                    pcgMyProcName, ipWho, ipForWhat, pclDat);
        }
        switch (ipForWhat) {
            case 0:
                /* Set Member Action */
                ilRecPos = ipWho + (GRP_REC_SIZE * ipOff);
                break;
            case 1:
                /* Set Member Status */
                ilRecPos = (GRP_REC_SIZE * 2) + ipWho;
                break;
            case 2:
                /* Init Member Entry */
                GetDbgLevel(pclDbg);
                ilMainPid = igMyParentPid;
                strcpy(pclTyp, "C");
                if (ipWho == 0) {
                    ilMainPid = igMyPid;
                    strcpy(pclTyp, "M");
                }
                sprintf(pclDat, "%04d.%05d.%05d.%s.U.%05d.%05d.....%s.............%s.%s.%s.%s%05d .",
                        ipWho, igMyPid, igMyParentPid, pclTyp, mod_id, igAnswerQueue,
                        pcgMyHexAddr, pcgTimeStamp, pcgTimeStamp, pclDbg, mod_name, ilMainPid);
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho);
                break;
            case 3:
                /* Set Child Type or Alive Status */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 17 + ipOff;
                break;
            case 4:
                /* Set Child Queue */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 27;
                sprintf(pclDat, "%05d", igAnswerQueue);
                break;
            case 5:
                /* Set Child HRG/HUR/OraLogin */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 33 + ipOff;
                break;
            case 6:
                /* Set Child LogFile */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 90;
                break;
            case 7:
                /* Set LogFile Dbg Level */
                GetDbgLevel(pclDat);
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 88;
                break;
            case 8:
                /* Set Wks Name */
                ilLen = strlen(pclDat);
                memset(pclDat, '.', GRP_DAT_SIZE - 1);
                strncpy(pclDat, pcpInfo, ilLen);
                pclDat[10] = 0x00;
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 46;
                break;
            case 9:
                /* Set Module Name */
                ilLen = strlen(pclDat);
                memset(pclDat, '.', GRP_DAT_SIZE - 1);
                strncpy(pclDat, pcpInfo, ilLen);
                pclDat[23] = 0x00;
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 103;
                break;
            default:
                break;
        }
        if (ilRecPos >= 0) {
            ilLen = strlen(pclDat);
            if (ilLen > 0) {
                fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
                fwrite((void *) pclDat, ilLen, 1, pfgCdrGrp);
                fflush(pfgCdrGrp);
            }
            if (pcpInfo[0] == '\0') {
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 73;
                fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
                fwrite((void *) pcgTimeStamp, 14, 1, pfgCdrGrp);
                fflush(pfgCdrGrp);
            }
        }
    }
    return;
}


/* *************************************************************** */

/* *************************************************************** */
static void GetGrpMemberInfo(int ipWho, int ipForWhat, char *pcpCmd, char *pcpInfo) {
    int ilRecPos = -1;
    int ilLen = -1;
    char pclDat[GRP_DAT_SIZE + 1];
    char pclTyp[2] = "";
    int ilReadNow = 0;
    if (pfgCdrGrp != NULL) {
        fseek(pfgCdrGrp, 0, SEEK_SET);
        ilReadNow = fread((void *) pclTyp, 1, 1, pfgCdrGrp);
    }
    strcpy(pcpInfo, "");
    strcpy(pcpCmd, ".");
    if ((ipWho >= 0) && (pfgCdrGrp != NULL) && (pclTyp[0] != '!')) {
        ilRecPos = ipWho;
        fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
        ilReadNow = fread((void *) pcpCmd, 1, 1, pfgCdrGrp);
        switch (ipForWhat) {
            case 0:
                /* Get Member Action */
                ilRecPos = ipWho;
                ilLen = 1;
                break;
            case 1:
                /* Get Member Status */
                ilRecPos = (GRP_REC_SIZE * 2) + ipWho;
                ilLen = 1;
                break;
            case 2:
                /* Get Member Entry */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho);
                ilLen = GRP_DAT_SIZE - 1;
                break;
            case 3:
                /* Get Child Type */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 17;
                ilLen = 1;
                break;
            case 4:
                /* Get Child Queue */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 27;
                ilLen = 5;
                break;
            case 5:
                /* Get Child OraLogin */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 33;
                ilLen = 3;
                break;
            case 6:
                /* Get Child LogFile */
                ilRecPos = (GRP_REC_SIZE * 3) + (GRP_DAT_SIZE * ipWho) + 90;
                ilLen = 12;
                break;
            default:
                break;
        }
        if ((ilRecPos >= 0) && (ilLen > 0)) {
            memset(pcpInfo, 0x00, (ilLen + 1));
            fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
            ilReadNow = fread((void *) pcpInfo, 1, ilLen, pfgCdrGrp);
            if (ilReadNow != ilLen) {
                dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, ilLen, ilReadNow);
                return;
            }
            ilLen--;
            while ((ilLen >= 0) && (pcpInfo[ilLen] == ' ')) {
                pcpInfo[ilLen] = '\0';
                ilLen--;
            }
        }
    }
    return;
}


/* *************************************************************** */

/* *************************************************************** */
static int SwitchToNewLogFile(char *pcpOld, char *pcpNew, char *pcpWhy) {
    int ilRC = RC_SUCCESS;
    FILE *outp2 = NULL;
    dbg(DEBUG, "%s LOGFILE SWITCH BECAUSE <%s>", pcgMyProcName, pcpWhy);
    dbg(DEBUG, "%s NOW CONNECTING TO <%s.log>", pcgMyProcName, pcpNew);
    sprintf(pcgAnyFile, "%s/%s.log", getenv("DBG_PATH"), pcpNew);
    outp2 = fopen(pcgAnyFile, "a");
    if (outp2 != NULL) {
        dbg(DEBUG, "%s NOW CLOSING <%s.log>", pcgMyProcName, pcpOld);
        fclose(outp);
        outp = outp2;
        dbg(DEBUG, "%s ============ LOGSWITCH ============", pcgMyProcName);
        dbg(DEBUG, "%s SWITCHED FROM <%s.log> BECAUSE <%s>", pcgMyProcName, pcpOld, pcpWhy);
    } else {
        dbg(DEBUG, "%s ============ LOGSWITCH ============", pcgMyProcName);
        dbg(DEBUG, "%s COULD NOT CONNECT TO <%s.log>", pcgMyProcName, pcpNew);
        ilRC = RC_FAIL;
    }
    return ilRC;
}


/* *************************************************************** */

/* *************************************************************** */
static void MainCheckLogSize(int ipSwitch) {
    struct stat rlLfBuff;
    if (fstat(fileno(outp), &rlLfBuff) != RC_FAIL) {
        if ((rlLfBuff.st_size > igMaxFileSize) || (ipSwitch == TRUE)) {
            dbg(TRACE, "%s SWITCH LOGFILE (SIZE=%d LIMIT=%d)",
                    pcgMyProcName, rlLfBuff.st_size, igMaxFileSize);
            SwitchMyLogFile();
            SetChildCmd("S", -1);
        }
    }
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void SetMainCmd(char *pcpCmd, int ipWho) {
    SetGrpMemberInfo(ipWho, 0, pcpCmd, 1);
    SetGrpMemberInfo(0, 0, "C", 0);
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void SetChildCmd(char *pcpCmd, int ipWhere) {
    int ilRecPos = 0;
    int i = 0;
    int j = 0;
    int ilReadNow = 0;
    ilRecPos = GRP_REC_SIZE * 2;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec0, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }
    if (*pcpCmd == 'A') {
        igAckCntSet = 0;
    }
    j = GRP_REC_SIZE - 1;
    for (i = 1; i < j; i++) {
        if (pcgGrpRec0[i] != '.') {
            SetGrpMemberInfo(i, 0, pcpCmd, 0);
            if (*pcpCmd == 'A') {
                igAckCntSet++;
            }
        }
    }
    lgBgnTime = time(0L);
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void GetChildAck(void) {
    int ilRC = RC_SUCCESS;
    int ilRecPos = 0;
    int ilCnt = 0;
    int ilChkCnt = 0;
    int i = 0;
    int j = 0;
    char pclTmpHexAddr[16];
    char pclTmpIpAddr[32];
    char pclChkCnt[4] = "0";
    char pclPid[8] = "";
    char pclQue[8] = "";
    char pclHrg[8] = "";
    char pclTist[16] = "";
    char pclWks[16] = "";
    int ilReadNow = 0;
    ilRecPos = 0;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec0, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }
    ilRecPos = GRP_REC_SIZE;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec1, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }
    ilRecPos = GRP_REC_SIZE * 2;
    fseek(pfgCdrGrp, ilRecPos, SEEK_SET);
    ilReadNow = fread((void *) pcgGrpRec2, 1, GRP_REC_SIZE, pfgCdrGrp);
    if (ilReadNow != GRP_REC_SIZE) {
        dbg(TRACE, "%s READ GOT MORE OR LESS: SIZE=%d READ=%d", pcgMyProcName, GRP_REC_SIZE, ilReadNow);
        return;
    }
    j = GRP_REC_SIZE - 1;
    for (i = 1; i < j; i++) {
        if (pcgGrpRec0[i] == 'A') {
            GetGrpMemberInfo(i, 2, pcgTmpCmd, pcgTmpDat);
            if ((pcgTmpDat[19] != 'D') && (pcgTmpDat[19] != 'R')) {
                ilCnt++;
                if (ilCnt == 1) {
                    dbg(TRACE, "%s ===== FOUND MISSING ACK FROM CHILD", pcgMyProcName);
                }
                pclChkCnt[0] = pcgGrpRec1[i];
                ilChkCnt = atoi(pclChkCnt);
                ilChkCnt++;
                if (ilChkCnt > 9) {
                    ilChkCnt = 9;
                }
                PrintGrpData(pcgTmpDat, pclPid, pclQue, pclTist, pclHrg, pclWks);
                dbg(DEBUG, "    SYS INFO : <%s> PID <%s> QUE <%s> TIME <%s>",
                        pclChkCnt, pclPid, pclQue, pclTist);
                /* =========================================== */
                /* TO DO:                                      */
                /* We got PID, QUE and Last Activity (TIST)    */
                /* Now we can check if ...                     */
                /* ... the child is incative since xx minutes  */
                /* ... PID exists and is the same cdrhdl child */
                /* ... QUE exists and belongs to the child     */
                /* and then ...                                */
                /* ... remove the queue and kill the child     */
                /* =========================================== */

                if (ilChkCnt == igCleanChildLevel) {
                    igMyGrpNbr = i;
                    igAnswerQueue = atoi(pclQue);
                    if (igAnswerQueue > 20000) {
                        dbg(TRACE, "%s ===== DELETE STACKED QUEUE %d", pcgMyProcName, igAnswerQueue);
                        sprintf(pclQue, "DEL.%05d", igAnswerQueue);
                        SetGrpMemberInfo(igMyGrpNbr, 8, pclQue, 0);
                        ilRC = HandleQueCreate(FALSE, pcgMyShortName, 1);
                    }
                    igAnswerQueue = 0;
                    if (igMainIsBcCom == FALSE) {
                        if (pclHrg[0] == 'R') {
                            dbg(TRACE, "%s ===== UNREGISTER CHILD FROM BCHDL", pcgMyProcName);
                            strcpy(pclTmpHexAddr, pcgMyHexAddr);
                            strcpy(pclTmpIpAddr, pcgMyIpAddr);
                            strcpy(pcgMyHexAddr, pclWks);
                            strcpy(pcgMyIpAddr, "RESET BY MAIN CDRHDL");
                            igHrgSent = TRUE;
                            HandleHrgCmd(FALSE, "");
                            strcpy(pcgMyHexAddr, pclTmpHexAddr);
                            strcpy(pcgMyIpAddr, pclTmpIpAddr);
                        }
                    }
                    igMyGrpNbr = 0;
                }

                SetGrpMemberInfo(i, 0, ".", 0);
                sprintf(pclChkCnt, "%d", ilChkCnt);
                SetGrpMemberInfo(i, 0, pclChkCnt, 1);
                if (ilChkCnt == igResetChildLevel) {
                    SetGrpMemberInfo(i, 3, "R", 2);
                }
            } else if (pcgTmpDat[19] == 'D') {
                SetGrpMemberInfo(i, 0, ".", 0);
                SetGrpMemberInfo(i, 0, ".", 1);
                SetGrpMemberInfo(i, 1, ".", 0);
                igAckCntSet--;
            }
        }
    }
    if (ilCnt > 0) {
        dbg(TRACE, "%s ===== %d MISSING ACK DETECTED", pcgMyProcName, ilCnt);
    }

    if (igAckCntSet > 0) {
        dbg(TRACE, "%s ----- %d CONNECTIONS ACKNOWLEDGED", pcgMyProcName, igAckCntSet);
    } else {
        dbg(TRACE, "%s ----- ALL CONNECTIONS ARE CLOSED", pcgMyProcName);
    }
    lgBgnTime = time(0L);
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void GetDbgLevel(char *pcpBuf) {
    switch (debug_level) {
        case 0:
            strcpy(pcpBuf, "O");
            break;
        case TRACE:
            strcpy(pcpBuf, "T");
            break;
        case DEBUG:
            strcpy(pcpBuf, "F");
            break;
        default:
            strcpy(pcpBuf, "?");
            break;
    }
    return;
}

/* *************************************************************** */

/* *************************************************************** */
static void PrintGrpData(char *pcpGrp, char *pcpPid, char *pcpQue,
        char *pcpTist, char *pcpHrg, char *pcpWks) {
    char *pclBgn = NULL;
    char *pclEnd = NULL;
    char pclChr[2] = "";
    pclBgn = pcpGrp;
    pclEnd = pcpGrp;

    pclEnd = pclBgn + 26;
    *pclChr = *pclEnd;
    *pclEnd = '\0';
    dbg(DEBUG, "GRP MEMBER ID: <%s>", pclBgn);
    *pclEnd = *pclChr;
    pclBgn += 5;
    strncpy(pcpPid, pclBgn, 5);
    pcpPid[5] = 0x00;

    pclBgn = pclEnd + 1;
    pclEnd = pclBgn + 9;
    *pclChr = *pclEnd;
    *pclEnd = '\0';
    dbg(DEBUG, "    QUE/LOGIN: <%s>", pclBgn);
    *pclEnd = *pclChr;
    strncpy(pcpQue, pclBgn, 5);
    pcpQue[5] = 0x00;
    pcpHrg[0] = pclBgn[6];
    pcpHrg[1] = 0x00;

    pclBgn = pclEnd + 1;
    pclEnd = pclBgn + 20;
    *pclChr = *pclEnd;
    *pclEnd = '\0';
    dbg(DEBUG, "    WKS/NAME : <%s>", pclBgn);
    *pclEnd = *pclChr;
    strncpy(pcpWks, pclBgn, 8);
    pcpWks[8] = 0x00;

    pclBgn = pclEnd + 1;
    pclEnd = pclBgn + 29;
    *pclChr = *pclEnd;
    *pclEnd = '\0';
    dbg(DEBUG, "    ACTIVITY : <%s>", pclBgn);
    *pclEnd = *pclChr;
    pclBgn += 15;
    strncpy(pcpTist, pclBgn, 14);
    pcpTist[14] = 0x00;

    pclBgn = pclEnd + 1;
    pclEnd = pclBgn + 14;
    *pclChr = *pclEnd;
    *pclEnd = '\0';
    dbg(DEBUG, "    LOGFILE  : <%s>", pclBgn);
    *pclEnd = *pclChr;

    pclBgn = pclEnd + 1;
    dbg(DEBUG, "    MODULE   : <%s>", pclBgn);

    return;
}



/* *************************************************************** */

/* *************************************************************** */
static int HandleQueCreate(int ipCreate, char *pcpWho, int ipId) {
    int ilRC = RC_SUCCESS;
    int ilAck = 0;
    int ilQue = 0;
    char pclOldQue[16];
    ilAck = igWaitForAck;
    igWaitForAck = FALSE;
    igGotAlarm = FALSE;
    ilQue = igAnswerQueue;
    if (ipCreate == TRUE)
    {
        if (igAnswerQueue <= 0)
        {
            SetGrpMemberInfo(igMyGrpNbr, 1, "Q", 0);
            sprintf(pcgQueueName, "%s%04d", pcpWho, igMyGrpNbr);
            alarm(10);
            ilRC = GetDynamicQueue(&igAnswerQueue, pcgQueueName);
            alarm(0);
            if (igGotAlarm == TRUE) {
                dbg(TRACE, "%s ERROR: TIMEOUT (CREATE QUEUE)", pcgMyProcName);
                ilRC = RC_FAIL;
                igGotAlarm = FALSE;
                igAnswerQueue = -1;
            }
            if (ilRC != RC_SUCCESS) {
                dbg(TRACE, "%s ERROR: GetDynamicQueue failed (%d)", pcgMyProcName, ilRC);
                igAnswerQueue = 0;
            }
            else
            {
                dbg(DEBUG, "%s QUEUE <%s> (%d) ESTABLISHED", pcgMyProcName, pcgQueueName, igAnswerQueue);
            }
            SetGrpMemberInfo(igMyGrpNbr, 4, "", 0);
        }
    }
    else
    {
        if (igAnswerQueue > 0)
        {
            ilRC = RC_SUCCESS;
            if (ilRC == RC_SUCCESS)
            {
                if (ipId == 0)
                {
                    sprintf(pclOldQue, "%s%04d", pcpWho, igMyGrpNbr);
                    DeleteOldQueues(pclOldQue);
                }
                else
                {
                    ilRC = que(QUE_DELETE, igAnswerQueue, igAnswerQueue, 0, 0, 0);
                    if (ilRC != RC_SUCCESS)
                    {
                        dbg(TRACE, "%s QUE_DELETE <%s> (%d) FAILED (%d)", pcgMyProcName, pcgQueueName, ilQue, ilRC);
                        igAnswerQueue = -1;
                    }
                    else
                    {
                        dbg(DEBUG, "%s CEDA QUEUE (%d) <%s> REMOVED", pcgMyProcName, igAnswerQueue, pcgQueueName);
                        igAnswerQueue = 0;
                    }
                }
            }
            else
            {
                dbg(TRACE, "%s ERROR: UNABLE TO ATTACH QUEUE", pcgMyProcName);
                ilRC = RC_FAIL;
                igAnswerQueue = -2;
            }
            SetGrpMemberInfo(igMyGrpNbr, 4, "", 0);
        }
    }
    igGotAlarm = FALSE;
    igWaitForAck = ilAck;
    return ilRC;
}

/* ======================================================================= */
/* ========================= END KERNEL ================================== */
/* ======================================================================= */


/* ======================================================================= */
/* ========================= CDRHDL ONLY ================================= */
/* THIS PART OF TOOL FUNCTIONS IS USED BY CDRHDL ONLY                      */
/* ======================================================================= */

#if defined(_CDR)

/* *************************************************************** */

/* *************************************************************** */
static int HandleHrgCmd(int ipSend, char *pcpWho) {
    int ilRC = RC_SUCCESS;
    if (ipSend == TRUE) {
        if ((igHrgSent == FALSE) && (igHsbDown == FALSE) && (igHrgAlive == TRUE)) {
            dbg(TRACE, "%s REGISTER BCHDL <%s> (CMD HRG)", pcgMyProcName, pcgMyHexAddr);
            ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                    0, /* Set to mod_id if < 1        */
                    "", /* BC_HEAD.dest_name           */
                    "", /* BC_HEAD.recv_name           */
                    "", /* CMDBLK.tw_start             */
                    "", /* CMDBLK.tw_end               */
                    "HRG", /* CMDBLK.command              */
                    "", /* CMDBLK.obj_name             */
                    pcgMyHexAddr, /* Selection Block             */
                    pcgMyIpAddr, /* Field Block                 */
                    "", /* Data Block                  */
                    "", /* Error description           */
                    1, /* 0 or Priority               */
                    RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS      */
            igHrgSent = TRUE;
            SetGrpMemberInfo(igMyGrpNbr, 5, "R", 0);
        }
    } else {
        if ((igHrgSent == TRUE) && (igHsbDown == FALSE)) {
            dbg(TRACE, "%s UNREGISTER BCHDL <%s> (CMD HUR)", pcgMyProcName, pcgMyHexAddr);
            ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                    0, /* Set to mod_id if < 1        */
                    "", /* BC_HEAD.dest_name           */
                    "", /* BC_HEAD.recv_name           */
                    "", /* CMDBLK.tw_start             */
                    "", /* CMDBLK.tw_end               */
                    "HUR", /* CMDBLK.command              */
                    "", /* CMDBLK.obj_name             */
                    pcgMyHexAddr, /* Selection Block             */
                    pcgMyIpAddr, /* Field Block                 */
                    "", /* Data Block                  */
                    "", /* Error description           */
                    1, /* 0 or Priority               */
                    RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS      */
            igHrgSent = FALSE;
        }
        SetGrpMemberInfo(igMyGrpNbr, 5, "U", 0);
    }
    return ilRC;
}

/* *************************************************************** */

/* *************************************************************** */
static int HandleOraLogin(int ipLogin, char *pcpWho) {
    int ilRC = RC_SUCCESS;
    int ilTryAgain = 10;
    int ilCnt = 0;
    int ilWaitSec = 1;
    int ilWaitTtl = 0;

    if (ipLogin == TRUE) {
        if (igOraLoginOk == FALSE) {
            ilCnt = ilTryAgain;
            ilWaitSec = 1;
            ilWaitTtl = 0;
            do {
                ilRC = init_db();
                if (ilRC != RC_SUCCESS) {
                    dbg(DEBUG, "%s WAITING FOR LOGIN DATABASE", pcgMyProcName);
                    sleep(ilWaitSec);
                    ilCnt--;
                    ilWaitTtl += ilWaitSec;
                } /* end if */
            } while ((ilCnt > 0) && (ilRC != RC_SUCCESS));

            if ((ilRC != RC_SUCCESS) || (ilWaitTtl > 0)) {
                dbg(DEBUG, "%s WAITED %d SECONDS FOR LOGIN DATABASE", pcgMyProcName, ilWaitTtl);
            } /* end if */
            if (ilRC == RC_SUCCESS) {
                dbg(TRACE, "%s LOGIN DATABASE SUCCESSFUL", pcgMyProcName);
                igOraLoginOk = TRUE;
                SetGrpMemberInfo(igMyGrpNbr, 5, "L", 2);
            } else {
                dbg(TRACE, "%s LOGIN DATABASE FAILED", pcgMyProcName);
            }
        }
    } else {
        if (igOraLoginOk == TRUE) {
            logoff();
            dbg(DEBUG, "%s LOGOFF DATABASE SUCCESSFUL", pcgMyProcName);
            igOraLoginOk = FALSE;
            SetGrpMemberInfo(igMyGrpNbr, 5, "O", 2);
        }
    }

    return ilRC;
}

/* *************************************************************** */

/* *************************************************************** */
static int ReadCedaArray(char *pcpMyChild, char *pcpKeyTbl, char *pcpKeyFld, char *pcpKeyWhe,
        REC_DESC *prpRecDesc, short spPackLines, int ipKeepOpen, int connfd,
        char *pcpFldSep, char *pcpRecSep, char *pcpOraHint)
{
    static STR_DESC rlSqlStrg;
    static int ilFirstCall = TRUE;
    int ilRC = RC_SUCCESS;
    int ilGetRc = DB_SUCCESS;
    int ilExitLoop = FALSE;
    int ilTryAgain = 10;
    int ilCnt = 0;
    int ilWaitSec;
    int ilWaitTtl;
    int ilFldCnt = 0;
    int ilLinCnt = 0;
    int ilCpid = 0;
    int ilPtrMov = 0;
    long llPtrSize = 0;
    long llMovSize = 0;
    long llMySize = 1;
    long llLastSize = 1;
    int ilTotalLen = 0;
    short slMaxPak = 0;
    short ilFkt = 0;
    short ilLocalCursor = 0;
    char *pclPtr = NULL;

    char *pclFunc = "ReadCedaArray";

    /*
      char pclSqlBufX[32 * 1024];
     */
    char *pclSqlBuf = NULL;
    char pclDataArea[128] = "\0";

    ilCpid = getpid();

    if (ilFirstCall == TRUE)
    {
        /* dbg(DEBUG,"%s INIT SQL STRING BUFFER",pcpMyChild); */
        CT_InitStringDescriptor(&rlSqlStrg);
        ilFirstCall = FALSE;
    }

    ilRC = HandleOraLogin(TRUE, "");

    if (ilRC == RC_SUCCESS)
    {
        if (spPackLines > 0)
        {
            slMaxPak = spPackLines;
            SetGrpMemberInfo(igMyGrpNbr, 1, "P", 0);
        }
        else
        {
            SetGrpMemberInfo(igMyGrpNbr, 1, "F", 0);
            slMaxPak = 500;
        }
        rlSqlStrg.UsedLen = strlen(pcpKeyFld) + strlen(pcpKeyTbl) + strlen(pcpKeyWhe) + strlen(pcpOraHint) + 64;
        if (rlSqlStrg.AllocatedSize < rlSqlStrg.UsedLen)
        {
            rlSqlStrg.Value = realloc(rlSqlStrg.Value, rlSqlStrg.UsedLen + iMAXIMUM_BUF_SIZE);        /* tvo: extend from 4096 to iMAXIMUM_BUF_SIZE */
            rlSqlStrg.AllocatedSize = rlSqlStrg.UsedLen + iMAXIMUM_BUF_SIZE;
            dbg(DEBUG, "%s SQL STRING REALLOC (%d)", pcpMyChild, rlSqlStrg.AllocatedSize);
        }
        pclSqlBuf = rlSqlStrg.Value;

        if (strlen(pcpKeyFld) < 1) {
            sprintf(pcpKeyFld, "%s", "*");
        }

        sprintf(pclSqlBuf, "SELECT  %s %s FROM %s %s", pcpOraHint, pcpKeyFld, pcpKeyTbl, pcpKeyWhe);
        if (igUseOraHint == FALSE)
        {
           sprintf(pclSqlBuf, "SELECT  %s FROM %s %s", pcpKeyFld, pcpKeyTbl, pcpKeyWhe);
        }

        if (pcpOraHint[0] != '\0')
        {
            if (igUseOraHint == FALSE)
            {
                dbg(TRACE, "%s NOT USING HINT. TABLE <%s> HINT <%s>", pcpMyChild, pcpKeyTbl, pcpOraHint);
            }
            else
            {
                dbg(TRACE, "%s USING <%s> WITH HINT <%s>", pcpMyChild, pcpKeyTbl, pcpOraHint);
            }
        }
        /* dbg(TRACE,"%s <%s>",pcpMyChild,pclSqlBuf); */

        dbg(TRACE, "%s NOW READING FROM DATABASE %s ", pcpMyChild, pclSqlBuf);

        /* Note:                                      */
        /* Here we must always set the AppendBuf flag */
        /* because we might have got a data buffer    */
        /* in case of multiple EVT KeyItems !!        */
        prpRecDesc->AppendBuf = TRUE;

        ilLocalCursor = 0;
        ilFkt = START;
        ilGetRc = DB_SUCCESS;
        while ((ilGetRc == DB_SUCCESS) && (ilExitLoop == FALSE))
        {
            pclDataArea[0] = 0x00;
            dbg(DEBUG, "%s READ DBIF: BEFORE SqlIfArray 1 ", pcpMyChild);
            sprintf(pclSqlBuf, "SELECT  %s %s FROM %s %s", pcpOraHint, pcpKeyFld, pcpKeyTbl, pcpKeyWhe);
            ilGetRc = SqlIfArray(ilFkt, &ilLocalCursor, pclSqlBuf, pclDataArea, slMaxPak, prpRecDesc);
            dbg(DEBUG, "READ DBIF: AFTER SqlIfArray 1 ");
            dbg(DEBUG, "%s READ DBIF: RECS=%d DATA=%d BUFF=%d SIZE=%dK PTR=%x", pcpMyChild,
                    prpRecDesc->LineCount, prpRecDesc->UsedSize,
                    prpRecDesc->CurrSize, llMySize, &(prpRecDesc->Value[0]));

            if ((prpRecDesc->AppendBuf == TRUE) && (spPackLines <= 0))
            {
                if (pclPtr != prpRecDesc->Value)
                {
                    ilPtrMov++;
                    if (llLastSize < llMySize)
                    {
                        llMovSize += llPtrSize;
                        llLastSize = llMySize;
                    }
                    pclPtr = prpRecDesc->Value;
                    llPtrSize = prpRecDesc->CurrSize;
                    if ((lgChildMaxSize > 0) && (llMySize > lgChildMaxSize))
                    {
                        dbg(TRACE, "%s EXCEEDING MAX SIZE OF %d K", pcpMyChild, lgChildMaxSize);
                        ilExitLoop = TRUE;
                    }
                }
            }
            else
            {
                if (ilGetRc == DB_SUCCESS)
                {
                    lgTotalRecs += prpRecDesc->LineCount;
                    pclPtr = prpRecDesc->Value;
                    TrimAndFilterCr(pclPtr, pcpFldSep, pcpRecSep);
                    prpRecDesc->UsedSize = strlen(pclPtr);
                    lgTotalByte += prpRecDesc->UsedSize;
                    ilTotalLen = prpRecDesc->UsedSize + 35;
                    sprintf(pclDataArea, "{=TOT=}%09d{=PACK=}%04d{=DAT=}",
                            ilTotalLen, prpRecDesc->LineCount);
                    dbg(DEBUG, "%s PACKAGE <%s>", pcpMyChild, pclDataArea);

                    dbg(DEBUG, "%s connfd <%d> line <%d>", pclFunc,connfd,__LINE__);

                    if(connfd > 0)
                    {
                        SetGrpMemberInfo(igMyGrpNbr, 1, "W", 0);
                        SendDataToClient(pcpMyChild, pclDataArea, 35, connfd, FALSE);
                        SendDataToClient(pcpMyChild, pclPtr, prpRecDesc->UsedSize, connfd, FALSE);
                        SetGrpMemberInfo(igMyGrpNbr, 1, "P", 0);
                    }
                    else
                    {
                        ilRC = SendCedaEvent(igQueToNetin, /* adress to send the answer   */
                            mod_id, /* Set to temp queue ID        */
                            "", /* BC_HEAD.dest_name           */
                            "", /* BC_HEAD.recv_name           */
                            "", /* CMDBLK.tw_start             */
                            "", /* CMDBLK.tw_end               */
                            "", /* CMDBLK.command              */
                            pcpKeyTbl, /* CMDBLK.obj_name             */
                            pcpKeyWhe, /* Selection Block             */
                            pcpKeyFld, /* Field Block                 */
                            pclDataArea, /* Data Block                  */
                            "", /* Error description           */
                            2, /* 0 or Priority               */
                            NETOUT_NO_ACK); /* BC_HEAD.rc: RC_SUCCESS      */

                        ilRC = SendCedaEvent(igQueToNetin, /* adress to send the answer   */
                            mod_id, /* Set to temp queue ID        */
                            "", /* BC_HEAD.dest_name           */
                            "", /* BC_HEAD.recv_name           */
                            "", /* CMDBLK.tw_start             */
                            "", /* CMDBLK.tw_end               */
                            "", /* CMDBLK.command              */
                            pcpKeyTbl, /* CMDBLK.obj_name             */
                            pcpKeyWhe, /* Selection Block             */
                            pcpKeyFld, /* Field Block                 */
                            pclPtr, /* Data Block                  */
                            "", /* Error description           */
                            2, /* 0 or Priority               */
                            NETOUT_NO_ACK); /* BC_HEAD.rc: RC_SUCCESS      */
                    }

                    prpRecDesc->AppendBuf = FALSE;
                    prpRecDesc->UsedSize = 0;
                }
            }
            ilFkt = NEXT;
        } /* end while */
        /* commit remote query !! */
        commit_work();
        close_my_cursor(&ilLocalCursor);

        if (ilPtrMov > 1) {
            dbg(DEBUG, "%s FINISH DBIF: RECS=%d DATA=%d BUFF=%d SIZE=%dK MOVED=%d WASTE=%d", pcpMyChild,
                    prpRecDesc->LineCount, prpRecDesc->UsedSize,
                    prpRecDesc->CurrSize, llMySize, ilPtrMov, llMovSize);
        }

        if (ilGetRc == ORA_NOT_FOUND)
        {
            ilRC = RC_SUCCESS;
        }
        if (ilGetRc == DB_ERROR)
        {
            if (prpRecDesc->CurrSize < 1024)
            {
                prpRecDesc->Value = realloc(prpRecDesc->Value, 1024);
                prpRecDesc->CurrSize = 1024;
            }
            get_ora_err(ilGetRc, prpRecDesc->Value);
            prpRecDesc->UsedSize = strlen(prpRecDesc->Value);
            ilRC = RC_FAIL;
        } /* end if */

        if (ilExitLoop == TRUE)
        {
            if (prpRecDesc->CurrSize < 1024)
            {
                prpRecDesc->Value = realloc(prpRecDesc->Value, 1024);
                prpRecDesc->CurrSize = 1024;
            }
            sprintf(prpRecDesc->Value,
                    "Sorry. Access denied.\nUnable to fetch %d records with\n%d byte total processed size!",
                    prpRecDesc->LineCount, llMySize);
            ilRC = RC_FAIL;
        }

        if (ilRC == RC_SUCCESS)
        {
            lgTotalRecs += prpRecDesc->LineCount;
        } /* end if */
    }/* end if */
    else
    {
        if (prpRecDesc->CurrSize < 1024)
        {
            prpRecDesc->Value = realloc(prpRecDesc->Value, 1024);
            prpRecDesc->CurrSize = 1024;
        }
        strcpy(prpRecDesc->Value, "LOGIN DATABASE FAILED");
    } /* end else */

    /* ORA session will be closed in  */
    /* CheckChildStatus or Terminate. */
    /* Thus we won't close it here.   */
    /* ----
    if ((ilRC != RC_SUCCESS)||(ipKeepOpen != TRUE))
    {
      ilRC = HandleOraLogin(FALSE, "");
    }
    ------- */

    return ilRC;
} /* end ReadCedaArray */

/* *************************************************************** */
/* -- UNDER CONSTRUCTION -- */

/* *************************************************************** */
static int CheckFmlTemplate(char *pcpMyChild, char *pcpKeyTbl, char *pcpKeyWhe) {
    int ilRC = RC_SUCCESS;
    int ilDbRc = DB_SUCCESS;
    int ilTabCnt = 0;
    int ilFmlCnt = 0;
    int ilNumUrno = 0;
    int ilFmlLen = 0;
    int ilLen = 0;
    int ili, ilj, ilk;
    int ilCurUrno = 0;
    int ilSynUrno = 0;
    int ilTestSync = FALSE;
    long llNowTime = 0;
    long llTstTime = 0;
    long llOutTime = 0;
    long llTmpLen = 0;
    short slFmlCursor = 0;
    short slFmlFkt = 0;
    short slSynCursor = 0;
    short slSynFkt = 0;
    short slChkCursor = 0;
    short slChkFkt = 0;
    short slTplCursor = 0;
    short slTplFkt = 0;
    int ilCount = 0;
    FILE *fp = NULL;
    char blIsCombo;
    char pclAdid[2] = "";
    char pclSynUrno[64] = "";
    char pclFmlSqlBuf[512] = "";
    char pclFmlSqlDat[1024 * 4] = "";
    char pclSynSqlBuf[256] = "";
    char pclSynSqlDat[1024] = "";
    char pclChkSqlBuf[256] = "";
    char pclChkSqlDat[1024] = "";
    char pclTplSqlBuf[256] = "";
    char pclTplSqlDat[1024] = "";
    char pclFmlFile[256] = "";
    char pclFmlLine[1024] = "";
    char pclAftUrno[16] = "";
    char pclAftOrg3[16] = "";
    char pclAftStod[16] = "";
    char pclFmlTemplate[32] = "";
    char pclFmlUrno[64] = "";
    char pclFmlTime[32] = "";
    char pclFmlTimeTemplate[32] = "";
    char pclFmlText[2048] = "";
    char pclFmlTextTemplate[2048] = "";
    char pclFmlData[1028] = "";
    char pclFmlLino[32] = "";
    char pclFmlUsec[32] = "";
    char pclFmlCdat[32] = "";
    char pclFmlLstu[32] = "";
    char pclFmlCode[32] = "";
    char pclFmlType[4] = "";
    char pclAftAlc2[16] = "";
    char pclAftTtyp[32] = "";
    char pclAftStyp[32] = "";
    char pclTmpStr[1024] = "\0";
    char pclTabName[12];
    char pclFieldName[12];
    char *pclToken;

    char *pclPointer = pcpKeyWhe;

    ilRC = HandleOraLogin(TRUE, "");
    if (ilRC != RC_SUCCESS)
        return ilRC;

    /*         First we need the AFT.URNO, */
    /*         which is FLNU in the received whereclause */
    /*         FLNU IN ([FLNU]) AND AREA='A'  */
    (void) CedaGetKeyItem(pclAftUrno, &llTmpLen, pcpKeyWhe, "FLNU IN (", ")", TRUE);
    if(pclAftUrno == NULL || strlen(pclAftUrno) <=0)
    {
        while ( '\0' != *pclPointer )
        {
            if( !isdigit(*pclPointer) )
            {
                //dbg(DEBUG,"*pclPointer<%c> is invalid number\n",*pclPointer);
                pclPointer++;
                continue;
            }
            else
            {
                //dbg(DEBUG,"*pclPointer<%c> is valid number\n",*pclPointer);
                pclAftUrno[ilCount++] = *pclPointer++;
            }
        }
    }
    dbg(TRACE, "%s RELATED FLIGHT AFT.URNO = <%s> FOR DOR TEMPLATE CHECK", pcpMyChild, pclAftUrno);

    /* ------------------------------------------------------------------------------ */
    /* Logic change. When template has been created still need to upd auto-pop fields */
    /* ------------------------------------------------------------------------------ */
    sprintf(pclFmlSqlBuf, "SELECT ORG3,ALC2,TTYP,ADID,STYP FROM AFTTAB WHERE URNO=%s", pclAftUrno);
    ilRC = RunSQL(pcpMyChild, pclFmlSqlBuf, pclFmlSqlDat);
    if (ilRC != DB_SUCCESS) {
        dbg(DEBUG, "%s Flight (%s) NOT FOUND in AFTTAB", pcpMyChild, pclAftUrno);
        return ilRC;
    }
    BuildItemBuffer(pclFmlSqlDat, "", 5, ",");
    dbg(TRACE, "%s FLIGHT READ <%s>", pcpMyChild, pclFmlSqlBuf);
    dbg(TRACE, "%s FLIGHT DATA <%s>", pcpMyChild, pclFmlSqlDat);
    (void) get_real_item(pclAftOrg3, pclFmlSqlDat, 1);
    (void) get_real_item(pclAftAlc2, pclFmlSqlDat, 2);
    (void) get_real_item(pclAftTtyp, pclFmlSqlDat, 3);
    (void) get_real_item(pclAdid, pclFmlSqlDat, 4);
    (void) get_real_item(pclAftStyp, pclFmlSqlDat, 5);

    if (bgIncludeTtyp == TRUE) {
        sprintf(pclTmpStr, ",%s,", pclAftTtyp);
        if (strstr(pcgDORTtyp, pclTmpStr) == NULL) {
            dbg(TRACE, "%s IGNORE! INCORRECT TTYP (%s)", pcpMyChild, pclAftTtyp);
            return RC_SUCCESS;
        }
    }

    if (bgIncludeStyp == TRUE) {
        sprintf(pclTmpStr, ",%s,", pclAftStyp);
        if (strstr(pcgDORStyp, pclTmpStr) == NULL) {
            dbg(TRACE, "%s IGNORE! INCORRECT STYP (%s)", pcpMyChild, pclAftStyp);
            return RC_SUCCESS;
        }
    }

    if (pclAdid[0] != 'A')
        strcpy(pclAdid, "D");
    sprintf(pclTmpStr, "%c_%s_TPL", pclAdid[0], pclAftAlc2);

    if ((ilRC = iGetConfigEntry(cgConfigFile, "DOR_TEMPLATES", pclTmpStr, CFG_STRING, pclFmlTemplate)) != RC_SUCCESS) {
        dbg(TRACE, "%s USING DEFAULT DOR TEMPLATE", pcpMyChild);
        sprintf(pclFmlTemplate, "%c_fmld.tpl", pclAdid[0]);
    }
    sprintf(pclFmlFile, "%s/%s", getenv("CFG_PATH"), pclFmlTemplate);
    dbg(TRACE, "%s DOR TEMPLATE LOCATION/NAME <%s>", pcpMyChild, pclFmlFile);

    dbg(TRACE, "%s READING TEMPLATE TABLE CONFIG FIRST", pcpMyChild);
    fp = fopen(pclFmlFile, "r");
    if (fp == NULL) {
        dbg(TRACE, "%s DOR TEMPLATE ERROR: CANNOT OPEN FILE <%s>", pcpMyChild, pclFmlFile);
        return RC_FAIL;
    }

    ilTabCnt = 0;
    ilFmlCnt = 0;
    bgCircularFlight = STAT_NOT_CHECK;
    memset(FTN, 0, sizeof (FML_TAB_NODE) * MAX_FML_TAB);
    while (fgets(pclFmlLine, sizeof (pclFmlLine), fp) != NULL) {
        if (pclFmlLine[0] == '#')
            continue;
        if (pclFmlLine[0] != 'D') {
            ilFmlLen = strlen(pclFmlLine) - 1;
            while ((ilFmlLen >= 0) &&
                    ((pclFmlLine[ilFmlLen] == '\n') || (pclFmlLine[ilFmlLen] == '\r') || (pclFmlLine[ilFmlLen] == ' '))) {
                pclFmlLine[ilFmlLen] = '\0';
                ilFmlLen--;
            }
            ilLen = get_real_item(pclFmlText, pclFmlLine, 4);
            if (ilLen > 0)
                ilFmlCnt++;
            continue;
        }
        if (ilTabCnt >= MAX_FML_TAB)
            break;
        ilRC = GetFMLTabData(pcpMyChild, pclFmlLine, pclAftUrno, ilTabCnt);
        memset(pclFmlLine, 0, sizeof (pclFmlLine));
        if (ilRC != RC_SUCCESS)
            continue;
        ilTabCnt++;
    }
    fclose(fp);
    dbg(TRACE, "%s TEMPLATE <%s> HAS %d TABLE CONFIG", pcpMyChild, pclFmlTemplate, ilTabCnt);
    dbg(TRACE, "%s TEMPLATE <%s> HAS %d VALID LINES", pcpMyChild, pclFmlTemplate, ilFmlCnt);
    ilNumUrno = ilFmlCnt + 2;

    if (ilFmlCnt <= 0) {
        dbg(TRACE, "%s TEMPLATE <%s> HAS NO VALID LINES", pcpMyChild, pclFmlTemplate);
        return RC_SUCCESS;
    }

    /* -------------------------------------------- */
    /* Step 1A: Check if a template already exists  */
    /*          This can be identified by AREA/TYPE */
    /* -------------------------------------------- */
    sprintf(pclFmlSqlBuf, "SELECT COUNT(*) FROM %s WHERE FLNU=%s AND AREA='T' AND TYPE='OK'", pcpKeyTbl, pclAftUrno);
    dbg(TRACE, "%s <%s>", pcpMyChild, pclFmlSqlBuf);

    ilDbRc = RunSQL(pcpMyChild, pclFmlSqlBuf, pclFmlSqlDat);
    if (ilDbRc != DB_SUCCESS)
        return ilDbRc;
    ilFmlCnt = atoi(pclFmlSqlDat);

    if (ilFmlCnt == 0) {
#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclFmlSqlBuf, "INSERT INTO %s (URNO,FLNU,TIME,TEXT,LINO,USEC,CDAT,AREA,TYPE,TMTP,TXTP) "
                "VALUES (:VURNO,:VFLNU,:VTIME,:VTEXT,:VLINO,:VUSEC,:VCDAT,:VAREA,:VTYPE,:VTMTP,:VTXTP)",
                pcpKeyTbl);
#else
        sprintf(pclFmlSqlBuf, "INSERT INTO %s (URNO,FLNU,TIME,TEXT,LINO,USEC,CDAT,AREA,TYPE,TMTP,TXTP) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                pcpKeyTbl);

#endif

        /* --------------------------------------------- */
        /* Step 1B: No template created yet ...          */
        /* ----------------------------------------------------- */
        /* Here we know that a new template must be created, but */
        /* we don't know if the creation is already in progress. */
        /* Thus we stop all other cdrhdl reading FMLTAB records. */
        /* Unfortunatly there is a technical restriction:        */
        /* The lock will be released in any case of commit_work, */
        /* which is part of the get_urno function as well.       */
        /* Thus we must first count the lines of the template,   */
        /* request the amount URNOs and then lock other cdrhdls. */
        /* And Yes, I know, this might waste a few URNOs ...     */
        /* ----------------------------------------------------- */
        ilTestSync = FALSE;
        if ((ilRC = iGetConfigEntry(cgConfigFile, "DOR_TEMPLATES", "TEST_SYNC",
                CFG_STRING, pclTmpStr)) == RC_SUCCESS) {
            ilTestSync = FALSE;
            if (pclTmpStr[0] == 'Y') {
                ilTestSync = TRUE;
                dbg(TRACE, "%s SYNCHRONIZING TEST ACTIVATED", pcpMyChild);
            }
        }

        /* ----------------------------------------------------- */
        /* Now we got everything that we need to synchronize the */
        /* creation of the DOR template for a particular flight. */
        /* So we will retrieve the URNOs and stop all cdrhdl now */
        /* ----------------------------------------------------- */
        dbg(TRACE, "%s RETRIEVING %d URNOs FOR TEMPLATE CREATION", pcpMyChild, ilNumUrno);
        (void) GetNextValues(pclFmlUrno, ilNumUrno);
        ilCurUrno = atoi(pclFmlUrno);

        dbg(TRACE, "%s SYNCHRONIZING OTHER CDRHDLs USING SYNTAB ...", pcpMyChild);
        if (ilTestSync == TRUE) {
            llNowTime = time(0L);
            llTstTime = (((llNowTime - 1) / 10) + 2) * 10;
            dbg(TRACE, "%s SYNCHRONIZING FROM TIME NOW %d TO %d", pcpMyChild, llNowTime, llTstTime);
            while (llNowTime < llTstTime) {
                sleep(1);
                llNowTime = time(0L);
                llOutTime = llTstTime - llNowTime;
                dbg(TRACE, "%s SYNCHRONIZING %d SECONDS LEFT", pcpMyChild, llOutTime);
            }
            dbg(TRACE, "%s ===========>> AND GO !!!!!!!!!!!!!!!", pcpMyChild);
        }
        sprintf(pclSynSqlBuf, "SELECT TKEY FROM SYNTAB WHERE TKEY='DOR_TEMPL' FOR UPDATE");
        dbg(TRACE, "%s <%s>", pcpMyChild, pclSynSqlBuf);

        slSynCursor = 0;
        slSynFkt = START;
        pclSynSqlDat[0] = 0x00;
        ilDbRc = sql_if(slSynFkt, &slSynCursor, pclSynSqlBuf, pclSynSqlDat);
        if (ilDbRc != DB_SUCCESS) {
            /* ------------------------------------------------------------ */
            /* The locking record does not yet exist, so we must create it. */
            /* This is a weak point: In the worst case the record might be  */
            /* inserted twice, but we don't care. (It'll only happen once.) */
            /* ------------------------------------------------------------ */
            close_my_cursor(&slSynCursor);
            (void) GetNextValues(pclSynUrno, 1);
            ilSynUrno = atoi(pclSynUrno);
            dbg(TRACE, "%s MUST CREATE SYNCHRONIZING RECORD IN SYNTAB", pcpMyChild);
#ifdef CCS_DBMODE_EMBEDDED
            sprintf(pclSynSqlBuf, "INSERT INTO SYNTAB (URNO,TKEY,STAT,TIFB,TIFE,USRN,WKST) "
                    "VALUES (:VURNO,:VTKEY,:VSTAT,:VTIFB,:VTIFE,:VUSRN,:VWKST)");
#else
            sprintf(pclSynSqlBuf, "INSERT INTO SYNTAB (URNO,TKEY,STAT,TIFB,TIFE,USRN,WKST) "
                    "VALUES (?,?,?,?,?,?,?)");
#endif


            sprintf(pclSynSqlDat, "%d,DOR_TEMPL,T, , , , ", ilSynUrno);
            dbg(TRACE, "%s <%s>", pcpMyChild, pclSynSqlBuf);
            dbg(TRACE, "%s <%s>", pcpMyChild, pclSynSqlDat);
            delton(pclSynSqlDat);
            slSynCursor = 0;
            slSynFkt = IGNORE;
            ilDbRc = sql_if(slSynFkt, &slSynCursor, pclSynSqlBuf, pclSynSqlDat);
            commit_work();
            close_my_cursor(&slSynCursor);
            /* ---------------------------------------- */
            /* Now we must repeat the locking selection */
            /* ---------------------------------------- */
            dbg(TRACE, "%s SYNCHRONIZING OTHER CDRHDLs (AGAIN) ...", pcpMyChild);
            sprintf(pclSynSqlBuf, "SELECT TKEY FROM SYNTAB WHERE TKEY='DOR_TEMPL' FOR UPDATE");
            dbg(TRACE, "%s <%s>", pcpMyChild, pclSynSqlBuf);
            slSynCursor = 0;
            slSynFkt = START;
            pclSynSqlDat[0] = 0x00;
            ilDbRc = sql_if(slSynFkt, &slSynCursor, pclSynSqlBuf, pclSynSqlDat);
        }
        /* ----------------------------------------------------- */
        /* OTHER CDRHDLs LOCKED ON SYNTAB                        */
        /* ===================================================== */
        dbg(TRACE, "%s GOT THE LOCK OR JUST BEEN RELEASED ...", pcpMyChild);
        dbg(TRACE, "%s ALL CDRHDLs ON HOLD (SYNTAB LOCK) NOW EXCEPT 'ME'", pcpMyChild);
        /* ----------------------------------------------------- */
        /* At this point there will be only one cdrhdl, that got */
        /* the selected SYN record while all others are on hold. */
        /* ----------------------------------------------------- */
        /* But when the lock will be released, then all cdrhdl   */
        /* must check if the work has already been done or not.  */
        /* For fast synchronizing purposes we first insert a new */
        /* trigger record related to the flight in FMLTAB.       */
        /* ----------------------------------------------------- */
        /* First create some global FMLTAB key values ...        */
        /* ----------------------------------------------------- */
        strcpy(pclFmlUsec, "SERVER_U");
        /*strncpy(pclFmlUsec,pcpMyChild,11); */
        pclFmlUsec[11] = '\0';
        GetServerTimeStamp("UTC", 1, 0, pclFmlCdat);
        strcpy(pclFmlText, pclFmlTemplate);
        strcpy(pclFmlLino, "0");
        strcpy(pclFmlTime, " ");
        /* ----------------------------------------------------- */
        /* Now check if the template Sync Record already exists. */
        /* ----------------------------------------------------- */
        sprintf(pclChkSqlBuf, "SELECT USEC FROM %s WHERE FLNU=%s AND AREA='T' AND TYPE='CR'", pcpKeyTbl, pclAftUrno);
        dbg(TRACE, "%s <%s>", pcpMyChild, pclChkSqlBuf);
        slChkCursor = 0;
        slChkFkt = START;
        pclChkSqlDat[0] = 0x00;
        ilDbRc = sql_if(slChkFkt, &slChkCursor, pclChkSqlBuf, pclChkSqlDat);
        if (ilDbRc != DB_SUCCESS) {
            dbg(TRACE, "%s MUST INSERT FML DOR TEMPLATE SYNC RECORD FOR THIS FLIGHT", pcpMyChild);
            sprintf(pclFmlUrno, "%d", ilCurUrno);
            ilCurUrno++;
            sprintf(pclFmlSqlDat, "%s,%s,%s,%s,%s,%s,%s,T,CR, , ",
                    pclFmlUrno,
                    pclAftUrno,
                    pclFmlTime,
                    pclFmlText,
                    pclFmlLino,
                    pclFmlUsec,
                    pclFmlCdat);
            dbg(TRACE, "%s <%s>", pcpMyChild, pclFmlSqlBuf);
            dbg(TRACE, "%s <%s>", pcpMyChild, pclFmlSqlDat);
            delton(pclFmlSqlDat);
            slFmlCursor = 0;
            slFmlFkt = IGNORE;
            ilDbRc = sql_if(slFmlFkt, &slFmlCursor, pclFmlSqlBuf, pclFmlSqlDat);
            commit_work();
            close_my_cursor(&slFmlCursor);
        } else {
            dbg(TRACE, "%s DOR TEMPLATE CREATION TRIGGER ALREADY EXISTS", pcpMyChild);
        }
        close_my_cursor(&slChkCursor);
        /* ----------------------------------------------------- */
        /* ALL CDRHDLs UNLOCKED                                  */
        /* ===================================================== */
        dbg(TRACE, "%s ALL CDRHDLs RELEASED NOW INCLUDING 'ME'", pcpMyChild);
        /* ----------------------------------------------------- */
        /* At this point all cdrhdl have been released and must  */
        /* be synchronized again, but this time with a relation  */
        /* to the required flight. Thus the creation of the DOR  */
        /* template can run in parallel for different flights.   */
        /* ----------------------------------------------------- */
        /* Lock all cdrhdls reading the same flight's template.  */
        /* ----------------------------------------------------- */
        dbg(TRACE, "%s SYNCHRONIZING OTHER CDRHDLs NOW USING FMLTAB ...", pcpMyChild);
        sprintf(pclSynSqlBuf, "SELECT USEC FROM %s WHERE FLNU=%s AND AREA='T' AND TYPE='CR' FOR UPDATE",
                pcpKeyTbl, pclAftUrno);
        dbg(TRACE, "%s <%s>", pcpMyChild, pclSynSqlBuf);
        slSynCursor = 0;
        slSynFkt = START;
        pclSynSqlDat[0] = 0x00;
        ilDbRc = sql_if(slSynFkt, &slSynCursor, pclSynSqlBuf, pclSynSqlDat);
        /* ----------------------------------------------------- */
        /* CDRHDLs LOCKED ON FMLTAB AND FLNU                     */
        /* ===================================================== */
        dbg(TRACE, "%s GOT THE LOCK OR JUST BEEN RELEASED ...", pcpMyChild);
        dbg(TRACE, "%s ALL CDRHDLs ON HOLD AGAIN (FMLTAB LOCK) NOW EXCEPT 'ME'", pcpMyChild);
        /* ----------------------------------------------------- */
        /* Once again only one cdrhdl is busy with this template */
        /* but we must check first if the creation has been done */
        /* already. This is as well necessary in order to handle */
        /* the already existing templates without creating them  */
        /* again and again. (For downward compatibility too)     */
        /* ----------------------------------------------------- */
        /* Note: From this point onwards everything has to be    */
        /* performed without committing the transactions or      */
        /* closing a cursor because it would release the lock.   */
        /* ----------------------------------------------------- */

        dbg(TRACE, "%s CHECK IF THE TEMPLATE HAS ALREADY BEEN CREATED", pcpMyChild);
        //sprintf(pclChkSqlBuf, "SELECT COUNT(*) FROM %s %s AND USEC LIKE 'SYSTEM_%%'", pcpKeyTbl, pcpKeyWhe);
        sprintf(pclChkSqlBuf, "SELECT COUNT(*) FROM %s WHERE USEC LIKE 'SYSTEM_%%' AND %s", pcpKeyTbl, pcpKeyWhe+strlen("WHERE "));

        dbg(TRACE, "%s <%s>", pcpMyChild, pclChkSqlBuf);
        slChkCursor = 0;
        slChkFkt = START;
        pclChkSqlDat[0] = 0x00;
        ilDbRc = sql_if(slChkFkt, &slChkCursor, pclChkSqlBuf, pclChkSqlDat);
        ilFmlCnt = atoi(pclChkSqlDat);
        slFmlCursor = 0;
        slFmlFkt = IGNORE;
        fp = fopen(pclFmlFile, "r");
        if (ilFmlCnt == 0) {
            dbg(TRACE, "%s READ FROM FILE AND INSERT THE TEMPLATE RECORDS", pcpMyChild);
            ilFmlCnt = 0;
            memset(pclFmlLine, 0, sizeof (pclFmlLine));
            while (fgets(pclFmlLine, sizeof (pclFmlLine), fp) != NULL) {
                if (pclFmlLine[0] == '#' || pclFmlLine[0] == 'D')
                    continue;
                ilFmlLen = strlen(pclFmlLine) - 1;
                while ((ilFmlLen >= 0) &&
                        ((pclFmlLine[ilFmlLen] == '\n') || (pclFmlLine[ilFmlLen] == '\r') || (pclFmlLine[ilFmlLen] == ' '))) {
                    pclFmlLine[ilFmlLen] = '\0';
                    ilFmlLen--;
                }
                dbg(TRACE, "%s FML LINE <%s>", pcpMyChild, pclFmlLine);

                blIsCombo = FALSE;
                if (!strncmp(pclFmlLine, "CC", 2))
                    blIsCombo = TRUE;

                /********* FML Text *********/
                ilLen = get_real_item(pclFmlText, pclFmlLine, 4);
                if (ilLen <= 0)
                    continue;
                strcpy(pclFmlTextTemplate, pclFmlText);
                if (blIsCombo == FALSE)
                    TranslateText(pcpMyChild, pclFmlText, pclAftUrno);
                else
                    strcpy(pclFmlText, " ");
                dbg(DEBUG, "%s TRANSLATED FML TEXT <%s>", pcpMyChild, pclFmlText);
                /********* FML Text *********/

                /********* Time *********/
                ilLen = get_real_item(pclFmlTime, pclFmlLine, 3);
                strcpy(pclFmlTimeTemplate, pclFmlTime);
                if (blIsCombo == FALSE) {
                    ilRC = TranslateTime(pcpMyChild, pclFmlTime, pclAftUrno);
                    if (ilRC != RC_SUCCESS)
                        strcpy(pclFmlTime, " ");
                } else
                    strcpy(pclFmlTime, pclFmlTimeTemplate);
                dbg(DEBUG, "%s TRANSLATED FML TIME <%s>", pcpMyChild, pclFmlTime);
                /********* End of TIME *********/

                /********* FML Usec *********/
                ilLen = get_real_item(pclFmlCode, pclFmlLine, 2);
                if (ilLen != 2)
                    strcpy(pclFmlCode, "SU");
                strcpy(pclFmlType, &pclFmlCode[1]);
                switch (pclFmlCode[0]) {
                    case 'S':
                        sprintf(pclFmlUsec, "SYSTEM_%s", pclFmlType);
                        break;
                    case 'T':
                        sprintf(pclFmlUsec, "SERVER_%s", pclFmlType);
                        break;
                    default:
                        sprintf(pclFmlUsec, "SYSTEM_%s", pclFmlType);
                        break;
                }
                /********* FML Usec *********/

                /********* FML Type AUTO/MANUAL *********/
                ilLen = get_real_item(pclFmlType, pclFmlLine, 1);
                if (ilLen <= 0)
                    strcpy(pclFmlType, "MM");

                ilFmlCnt++;
                sprintf(pclFmlLino, "%c000.%0.3d", pclAdid[0], ilFmlCnt);
                sprintf(pclFmlUrno, "%d", ilCurUrno);
                ilCurUrno++;
                sprintf(pclFmlSqlDat, "%s,%s,%s,%s,%s,%s,%s,A,%2.2s,%s,%s",
                        pclFmlUrno,
                        pclAftUrno,
                        pclFmlTime,
                        pclFmlText,
                        pclFmlLino,
                        pclFmlUsec,
                        pclFmlCdat,
                        pclFmlType,
                        pclFmlTimeTemplate,
                        pclFmlTextTemplate);
                delton(pclFmlSqlDat);
                ilDbRc = sql_if(slFmlFkt, &slFmlCursor, pclFmlSqlBuf, pclFmlSqlDat);
                if (ilTestSync == TRUE) {
                    dbg(TRACE, "%s SYNCHRONIZING TEST: WAIT A SECOND ...", pcpMyChild);
                    sleep(1);
                }
            } /* end while */
            fclose(fp);
            fp = NULL;
        }
        dbg(TRACE, "%s AT LEAST %d DOR TEMPLATE RECORDS HAVE BEEN CREATED", pcpMyChild, ilFmlCnt);
        /* ----------------------------------------------------- */
        /* CDRHDLs STILL LOCKED ON FMLTAB AND FLNU               */
        /* ----------------------------------------------------- */
        /* Just in order to be on the safe side we check OK too. */
        /* ----------------------------------------------------- */
        dbg(TRACE, "%s CHECK IF THE 'DOR OK' RECORD ALREADY EXISTS", pcpMyChild);
        sprintf(pclTplSqlBuf, "SELECT USEC FROM %s WHERE FLNU=%s AND AREA='T' AND TYPE='OK'", pcpKeyTbl, pclAftUrno);
        dbg(TRACE, "%s <%s>", pcpMyChild, pclTplSqlBuf);
        slTplCursor = 0;
        slTplFkt = START;
        pclTplSqlDat[0] = 0x00;
        ilDbRc = sql_if(slTplFkt, &slTplCursor, pclTplSqlBuf, pclTplSqlDat);
        if (ilDbRc != DB_SUCCESS) {
            dbg(TRACE, "%s INSERTING THE 'DOR OK' RECORD (FINISH TRIGGER)", pcpMyChild);
            sprintf(pclFmlUrno, "%d", ilCurUrno);
            ilCurUrno++;
            strcpy(pclFmlLino, "1");
            strcpy(pclFmlTime, " ");
            sprintf(pclFmlSqlDat, "%s,%s,%s,%s,%s,%s,%s,T,OK, , ",
                    pclFmlUrno,
                    pclAftUrno,
                    pclFmlTime,
                    pclFmlText,
                    pclFmlLino,
                    pclFmlUsec,
                    pclFmlCdat);
            delton(pclFmlSqlDat);
            ilDbRc = sql_if(slFmlFkt, &slFmlCursor, pclFmlSqlBuf, pclFmlSqlDat);
            dbg(TRACE, "%s COMMIT DOR TEMPLATE CREATION NOW ", pcpMyChild);
            commit_work();
        }
        close_my_cursor(&slFmlCursor);
        close_my_cursor(&slChkCursor);
        close_my_cursor(&slSynCursor);
        close_my_cursor(&slTplCursor);
        /* ----------------------------------------------------- */
        /* CDRHDLs UNLOCKED                                      */
        /* ===================================================== */
        dbg(TRACE, "%s ALL CDRHDLs RELEASED NOW", pcpMyChild);

        if (igNumLDMLoad > 0) {
            igNumLDMLoad = 0;
            free(prgLoadRec);
        }
        return RC_SUCCESS;
    }

    /**** TEMPLATE HAS PREVIOUSLY BEEN CREATED ****/
    /**** UPDATE AUTO FIELD WILL DO ****/
    GetServerTimeStamp("UTC", 1, 0, pclFmlLstu);

    sprintf(pclFmlSqlBuf, "SELECT URNO,TYPE,TRIM(TMTP),TRIM(TXTP) FROM FMLTAB WHERE FLNU = '%s' AND TYPE LIKE '%%A%%'", pclAftUrno);
    dbg(TRACE, "%s <%s>", pcpMyChild, pclFmlSqlBuf);
    slFmlCursor = 0;
    slFmlFkt = START;
    while ((ilDbRc = sql_if(slFmlFkt, &slFmlCursor, pclFmlSqlBuf, pclFmlSqlDat)) == DB_SUCCESS) {
        slFmlFkt = NEXT;
        get_fld(pclFmlSqlDat, FIELD_1, STR, 10, pclFmlUrno);
        get_fld(pclFmlSqlDat, FIELD_2, STR, 2, pclFmlType);
        get_fld(pclFmlSqlDat, FIELD_3, STR, 16, pclFmlTimeTemplate);
        get_fld(pclFmlSqlDat, FIELD_4, STR, 1024, pclFmlTextTemplate);

        dbg(TRACE, "%s URNO <%s> TYPE <%s>", pcpMyChild, pclFmlUrno, pclFmlType);
        dbg(TRACE, "%s TIME TEMPLATE <%s>", pcpMyChild, pclFmlTimeTemplate);
        dbg(TRACE, "%s TEXT TEMPLATE <%s>", pcpMyChild, pclFmlTextTemplate);

        sprintf(pclTplSqlBuf, "UPDATE %s SET ", pcpKeyTbl);
        if (pclFmlType[0] == 'A') /* Auto-upd for TIME */ {
            ilRC = TranslateTime(pcpMyChild, pclFmlTimeTemplate, pclAftUrno);
            if (ilRC == RC_SUCCESS)
                sprintf(pclTmpStr, "TIME = '%s',", pclFmlTimeTemplate);
            else
                strcpy(pclTmpStr, "TIME = ' ',");
            strcat(pclTplSqlBuf, pclTmpStr);
        }
        if (pclFmlType[1] == 'A') /* Auto-upd for TEXT */ {
            ilRC = TranslateText(pcpMyChild, pclFmlTextTemplate, pclAftUrno);
            if (ilRC == RC_SUCCESS) {
                sprintf(pclTmpStr, "TEXT = '%s',", pclFmlTextTemplate);
                strcat(pclTplSqlBuf, pclTmpStr);
            }
        }
        sprintf(pclTmpStr, "LSTU = '%s' WHERE URNO = '%s' AND FLNU = '%s'",
                pclFmlLstu, pclFmlUrno, pclAftUrno);
        strcat(pclTplSqlBuf, pclTmpStr);
        dbg(TRACE, "%s UPDATE FML SQL <%s>", pcpMyChild, pclTplSqlBuf);
        RunSQL(pcpMyChild, pclTplSqlBuf, pclTplSqlDat);

    } /* while loop */
    close_my_cursor(&slFmlCursor);
    if (igNumLDMLoad > 0) {
        igNumLDMLoad = 0;
        free(prgLoadRec);
    }
}

/* **********************************************************************/
/* This function is called by CMD=GBCH and reads Broadcasts from bchdl's */
/* stackfile and sends them per tcp back to requester                   */

/* **********************************************************************/
static int GetBcHistory(char *pcpMyChild, char *pcpKeyTbl,
        char *pcpKeyWhe, char *pcpKeyFld, char *pcpKeyDat,
        REC_DESC *prpRecDesc, short spPackLines, int connfd) {
    static STACK_HEAD *prlStHd = NULL; /* Must be allocated (Reads BC Stack Header) */
    static COMMIF *prlBcPacket = NULL; /* Must be allocated (Reads One BC Record) */
    static REC_BC rlBcOutRec; /* DataBuffer Must be allocated (Collects BC Parts) */

    BC_HEAD *prlBcBcHead = NULL; /* Pointer To Broadcast Header */
    CMDBLK *prlBcCmdBlk = NULL; /* Pointer To Command Block */
    char *pclBcRecDat = NULL; /* Pointer To BC Data Area */

    BC_HEAD *prlBcOutBcHead = NULL; /* Pointer To Broadcast Header */
    CMDBLK *prlBcOutCmdBlk = NULL; /* Pointer To Command Block */
    char *pclBcOutRecDat = NULL; /* Pointer To BC Data Area */

    int ilRC = RC_SUCCESS;
    char *Ptmp = NULL;
    char *pclPtr = NULL;
    char pclStackFile[128] = "";
    FILE *rlStackFile = NULL;
    char pclTmpBuf[128] = "";

    char *pclBcSel = NULL;
    char *pclBcFld = NULL;
    char *pclBcDat = NULL;

    int ilGotTbl = FALSE;
    int ilGotSel = FALSE;
    int ilGotFld = FALSE;
    int ilGotDat = FALSE;

    int ilBcRec = 0;
    short ilActBcNum = 0;
    short ilOldActBcNum = 0;
    short ilNewActBcNum = 0;
    int ilBcHeadDiff = 0;
    int ilCurBcNum = 0;
    int ilLoopFirstRec = 0;
    int ilLoopLastRec = 0;
    int ilCurLoopDist = 0;
    int ilMaxLoopDist = 0;
    int ilBreakOut = FALSE;
    int ilGetNextFullBc = FALSE;
    int ilBcRecSwapped = FALSE;
    int ilBcRecCount = 0;
    int ilBcOutCount = 0;
    int ilRequestCount = 0;
    int ilDirection = 0;
    int ilTotalLen = 0;
    int ilOutPos = 0;
    int ilOffset = 0;
    int ilBcFound = FALSE;
    int ilNum = 0;
    int ilItmCnt = 0;
    int ilLen = 0;
    long llRecPos = 0;
    long llDatLen = 0;

    dbg(TRACE, "%s ENTERED GET BC HISTORY COMMAND", pcgMyProcName);

    dbg(TRACE, "%s GOT TBL <%s>", pcpMyChild, pcpKeyTbl);
    dbg(TRACE, "%s GOT SEL <%s>", pcpMyChild, pcpKeyWhe);
    dbg(TRACE, "%s GOT FLD <%s>", pcpMyChild, pcpKeyFld);
    dbg(TRACE, "%s GOT DAT <%s>", pcpMyChild, pcpKeyDat);

    if (strlen(pcpKeyTbl) > 0) ilGotTbl = TRUE;
    if (strlen(pcpKeyWhe) > 0) ilGotSel = TRUE;
    if (strlen(pcpKeyFld) > 0) ilGotFld = TRUE;
    if (strlen(pcpKeyDat) > 0) ilGotDat = TRUE;

    pclPtr = GetKeyItem(pclTmpBuf, &llDatLen, pcpKeyWhe, "[CMD=", "]", TRUE);
    if (llDatLen > 0) {
        dbg(DEBUG, "%s CMD <%s> LEN=%d", pcpMyChild, pclTmpBuf, llDatLen);
        if (strcmp(pclTmpBuf, "GET_HIST") == 0) {
            ilDirection = -1;
            pclPtr = GetKeyItem(pclTmpBuf, &llDatLen, pcpKeyWhe, "[CNT=", "]", TRUE);
            ilRequestCount = MAX_BC_NUM;
            if (llDatLen > 0) {
                ilRequestCount = atoi(pclTmpBuf);
            }
        } else if (strcmp(pclTmpBuf, "GET_FROM") == 0) {
            ilDirection = 1;
            dbg(TRACE, "%s FUNCTION COMMAND NOT YET SUPPORTED ...", pcpMyChild);
            ilRC = RC_FAIL;
        } else if (strcmp(pclTmpBuf, "GET_LIST") == 0) {
            ilDirection = 0;
            dbg(TRACE, "%s FUNCTION COMMAND NOT YET SUPPORTED ...", pcpMyChild);
            ilRC = RC_FAIL;
        } else {
            dbg(TRACE, "%s UNKNOWN FUNCTION COMMAND. DON'T KNOW WHAT TO DO ...", pcpMyChild);
            ilRC = RC_FAIL;
        }
    } else {
        dbg(TRACE, "%s MISSING FUNCTION COMMAND. DON'T KNOW WHAT TO DO ...", pcpMyChild);
        ilRC = RC_FAIL;
    }

    /* XBERNI */

    if (prpRecDesc->CurrSize <= 0) {
        dbg(TRACE, "%s GetBcHistory: ALLOCATING REC_DESC SIZE=%d", pcpMyChild, prpRecDesc->InitSize);
        prpRecDesc->Value = malloc(prpRecDesc->InitSize);
        prpRecDesc->CurrSize = prpRecDesc->InitSize;
        if (prpRecDesc->Value != NULL) {
            prpRecDesc->Value[0] = 0x00;
        } else {
            dbg(TRACE, "%s GetBcHistory: INIT REC_DESC FAILED", pcpMyChild);
            prpRecDesc->CurrSize = 0;
            ilRC = RC_FAIL;
        }
    }
    if ((ilRC == RC_SUCCESS) && (prlStHd == NULL)) {
        dbg(TRACE, "%s GetBcHistory: ALLOCATING STACK_HEAD SIZE=%d", pcpMyChild, PACKET_LEN);
        prlStHd = (STACK_HEAD *) malloc(PACKET_LEN);
        if (prlStHd == NULL) {
            dbg(TRACE, "%s GetBcHistory: MALLOC STACK_HEAD FAILED", pcpMyChild);
            ilRC = RC_FAIL;
        }
    }
    if ((ilRC == RC_SUCCESS) && (prlBcPacket == NULL)) {
        dbg(TRACE, "%s GetBcHistory: ALLOCATING READ PACKET (COMMIF) SIZE=%d", pcpMyChild, PACKET_LEN);
        prlBcPacket = (COMMIF *) malloc(PACKET_LEN);
        if (prlBcPacket == NULL) {
            dbg(TRACE, "%s GetBcHistory: MALLOC READ PACKET (COMMIF) FAILED", pcpMyChild);
            ilRC = RC_FAIL;
        } /* end if */
    }
    if ((ilRC == RC_SUCCESS) && (rlBcOutRec.DataBuffer == NULL)) {
        dbg(TRACE, "%s GetBcHistory: ALLOCATING BC_OUT DATABUFFER SIZE=%d", pcpMyChild, (32 * PACKET_LEN));
        rlBcOutRec.DataBuffer = (char *) malloc((32 * PACKET_LEN));
        rlBcOutRec.DataBufferSize = ((32 * PACKET_LEN));
        if (rlBcOutRec.DataBuffer == NULL) {
            dbg(TRACE, "%s GetBcHistory: MALLOC BC_OUT DATABUFFER FAILED", pcpMyChild);
            rlBcOutRec.DataBufferSize = 0;
            ilRC = RC_FAIL;
        } /* end if */
    }
    if (ilRC == RC_SUCCESS) {
        if ((Ptmp = getenv("CFG_PATH")) == NULL) {
            dbg(TRACE, "%s Error Reading CFG_PATH", pcpMyChild);
            ilRC = RC_FAIL;
        } else {
            sprintf(pclStackFile, "%s/%s", Ptmp, pcgStackFileBcHdl);
            rlStackFile = fopen(pclStackFile, "r");
            if (rlStackFile == NULL) {
                dbg(TRACE, "%s UNABLE TO OPEN <%s>", pcpMyChild, pclStackFile);
                ilRC = RC_FAIL;
            }
        }
    }
    if (ilRC == RC_SUCCESS) {
        dbg(TRACE, "%s OPENED BC STACK FILE", pcpMyChild);
        ilRC = fseek(rlStackFile, 0, SEEK_SET);
        fread((void *) prlStHd, PACKET_LEN, 1, rlStackFile);
        dbg(TRACE, "%s CURRENT ACTUAL BCNUM=%u MAX=%u", pcpMyChild,
                prlStHd->ActBcNum, prlStHd->MaxBcNum);
        ilActBcNum = prlStHd->ActBcNum;
        ilOldActBcNum = prlStHd->ActBcNum;

        /* Difficulty:                                    */
        /* We don't know if the record of the last BcNum  */
        /* has already been flushed into the stack file   */
        /* and if all parts are already available.        */
        /* But:                                           */
        /* We know that the record data of the previous   */
        /* BC_REC must be complete and flushed to disk.   */
        /* So we check the previous record and decide how */
        /* to proceed ... */

        /* Get the previous BC record number */
        ilBcRec = ilActBcNum - 1;
        if (ilBcRec < 1) {
            ilBcRec = MAX_BC_NUM;
        }
        ilBcRecCount = 1; /* Don't jump to first part */
        dbg(TRACE, "%s Before GetBcPacket1",pcpMyChild);
        ilBcRec = GetBcPacket(pcpMyChild, rlStackFile, ilBcRec, prlBcPacket, &ilBcRecCount);
        ilCurBcNum = ilBcRec;
        prlBcBcHead = (BC_HEAD *) prlBcPacket->data;
        dbg(TRACE, "%s LAST ACTUAL RECORD (%u) IS PART %u OF %u", pcpMyChild,
                prlBcBcHead->bc_num, prlBcBcHead->act_buf, prlBcBcHead->tot_buf);

        /* If it is the last record of multiple parts or  */
        /* the first of a single rec everything is fine.  */

        ilBcRecCount = prlBcBcHead->tot_buf - prlBcBcHead->act_buf;
        dbg(TRACE, "%s ilBcRecCount:%d , tot_buf:%u , act_buf:%u",ilBcRecCount, prlBcBcHead->tot_buf, prlBcBcHead->act_buf);

        /* A diff of 0 means that we got the last part.   */
        /* A diff of 1 means that the last part of the BC */
        /* most probably will be flushed when we read it. */

        if (ilBcRecCount > 1) {
            dbg(TRACE, "%s PARTS OF BC NOT YET COMPLETE. SKIP BC %u", pcpMyChild, prlBcBcHead->bc_num);
            /* Calculate the last valid (complete) BC_NUM */
            ilCurBcNum = ilBcRec - prlBcBcHead->act_buf;
            if (ilCurBcNum < 1) {
                ilCurBcNum = MAX_BC_NUM + ilCurBcNum;
            }
        }

        /* We now determined that we can safely read the  */
        /* data of record ilCurBcNum while Bchdl is still */
        /* extending the spooler's head.                  */

        if (ilDirection > 0) {
        } else if (ilDirection < 0) {
            /* We are searching backwards. */
            /* So we must start with the last number */
            /* which is already calculated in ilCurBcNum. */
            /* While looping backwards we must avoid a */
            /* collision with bchdl which is moving the */
            /* stack head in forward direction.         */
            /*                                        */
            /* During our loop we'll always check the */
            /* actual distance between the stack head */
            /* and our current position in the file. */
            ilCurLoopDist = ilOldActBcNum - ilCurBcNum;
            if (ilCurLoopDist < 0) {
                ilCurLoopDist = MAX_BC_NUM - ilOldActBcNum + ilCurBcNum;
            }
            ilMaxLoopDist = MAX_BC_NUM - ilCurLoopDist;

            /* ilMaxLoopDist now contains the maximum range */
            /* of records in the file, which can be fetched */
            /* without reading the last recent records again. */
            /* We'll reduce the range when bchdl inserts a */
            /* record and moves the stack head forwards. */
            /* Keep a bit away from bchdl stack head. */
            ilMaxLoopDist -= 10;
            dbg(TRACE, "%s SCANNING BACKWARDS MAX %d ENTRIES", pcpMyChild, ilMaxLoopDist);
        } else if (ilDirection == 0) {
        }

        ilBreakOut = FALSE;
        ilBcRecSwapped = FALSE;
        /* Prepare Loop Entry */
        ilGetNextFullBc = FALSE;
        ilBcOutCount = 0;
        ilBcRec = ilCurBcNum;
        ilLoopFirstRec = ilBcRec;
        ilLoopLastRec = ilBcRec;

        while ((ilCurBcNum > 0) && (ilBreakOut == FALSE)) {
            if (ilGetNextFullBc == TRUE) {
                ilRC = fseek(rlStackFile, 0, SEEK_SET);
                fread((void *) prlStHd, PACKET_LEN, 1, rlStackFile);
                dbg(DEBUG, "%s BCNUM: CURR_ACTUAL=%u END_LOOP=%u", pcpMyChild, prlStHd->ActBcNum, ilLoopFirstRec);
                ilNewActBcNum = prlStHd->ActBcNum;
                ilBcHeadDiff = ilNewActBcNum - ilOldActBcNum;
                if (ilBcHeadDiff != 0) {
                    if (ilBcHeadDiff < 0) {
                        ilBcHeadDiff = MAX_BC_NUM - ilOldActBcNum + ilNewActBcNum;
                    }
                    dbg(TRACE, "%s BCHDL SHIFTED SPOOLER HEAD BY %d ENTRIES", pcpMyChild, ilBcHeadDiff);
                    if (ilDirection > 0) {
                    } else if (ilDirection < 0) {
                        ilMaxLoopDist -= ilBcHeadDiff;
                        ilCurLoopDist = ilLoopFirstRec - ilCurBcNum;
                        if (ilCurLoopDist < 0) {
                            ilCurLoopDist = MAX_BC_NUM - ilCurBcNum + ilLoopFirstRec;
                        }
                        dbg(TRACE, "%s NEW SCANNING RANGE MAX=%d (CUR=%d) RECS", pcpMyChild, ilMaxLoopDist, ilCurLoopDist);
                    } else if (ilDirection == 0) {
                    }
                    ilOldActBcNum = ilNewActBcNum;
                }
                if (ilDirection > 0) {
                    ilCurBcNum++;
                    if ((ilBcRecSwapped == TRUE) && (ilCurBcNum >= ilLoopFirstRec)) {
                        dbg(TRACE, "%s BC_STACK LOOP REACHED FIRST BC_REC %d", pcpMyChild, ilLoopFirstRec);
                        ilBreakOut = TRUE;
                    }
                    if (ilCurBcNum >= MAX_BC_NUM) {
                        ilCurBcNum = 1;
                        ilBcRecSwapped = TRUE;
                        dbg(TRACE, "%s BC_STACK LOOP SWAPPED TO BC_REC %d", pcpMyChild, ilCurBcNum);
                    }
                } else if (ilDirection < 0) {
                    ilCurBcNum--;
                    if (ilCurBcNum <= 0) {
                        if (ilBcRecSwapped == FALSE) {
                            ilCurBcNum = MAX_BC_NUM;
                            ilBcRecSwapped = TRUE;
                            dbg(TRACE, "%s BC_STACK LOOP SWAPPED TO BC_REC %d", pcpMyChild, ilCurBcNum);
                        } else {
                            ilCurBcNum = MAX_BC_NUM;
                            dbg(TRACE, "%s BC_STACK LOOP ALREADY SWAPPED AT BC_REC %d", pcpMyChild, ilCurBcNum);
                            dbg(TRACE, "%s SEARCH LOOP STOPPED NOW", pcpMyChild);
                            ilBreakOut = TRUE;
                        }
                    }
                    ilCurLoopDist = ilLoopFirstRec - ilCurBcNum;
                    if (ilCurLoopDist < 0) {
                        ilCurLoopDist = MAX_BC_NUM - ilCurBcNum + ilLoopFirstRec;
                    }
                    if (ilCurLoopDist >= ilMaxLoopDist) {
                        dbg(TRACE, "%s HIT RANGE MAX=%d (CUR=%d) RECS", pcpMyChild, ilMaxLoopDist, ilCurLoopDist);
                        ilBreakOut = TRUE;
                    }
                } else if (ilDirection == 0) {
                }
                ilBcRec = ilCurBcNum;
                ilBcRecCount = 0;
            }
            if (ilBreakOut == FALSE) {
                dbg(TRACE, "%s Before GetBcPacket1",pcpMyChild);
                ilBcRec = GetBcPacket(pcpMyChild, rlStackFile, ilBcRec, prlBcPacket, &ilBcRecCount);
                if (ilBcRec > 0) {
                    prlBcBcHead = (BC_HEAD *) prlBcPacket->data;
                    prlBcCmdBlk = (CMDBLK *) prlBcBcHead->data;

                    if (prlBcBcHead->act_buf == 1) {
                        /*
                        dbg(TRACE,"%s FIRST PACK BC_REC %d (%d/%d) CMD <%s> TBL <%s> ", pcpMyChild,
                                   prlBcBcHead->bc_num,prlBcBcHead->act_buf,prlBcBcHead->tot_buf,
                                   prlBcCmdBlk->command,prlBcCmdBlk->obj_name);
                         */

                        ilCurBcNum = ilBcRec; /* Save Current BCNUM */
                        ilGetNextFullBc = TRUE;

                        /* Checking the table filter */
                        if ((ilGotTbl == FALSE) || (strstr(pcpKeyTbl, prlBcCmdBlk->obj_name) != NULL)) {
                            memcpy(&rlBcOutRec.BcHead, prlBcBcHead, sizeof (BC_HEAD));
                            memcpy(&rlBcOutRec.CmdBlk, prlBcBcHead->data, sizeof (CMDBLK));
                            if (rlBcOutRec.DataBufferSize < prlBcBcHead->tot_size) {
                                rlBcOutRec.DataBuffer = (char *) realloc(rlBcOutRec.DataBuffer, prlBcBcHead->tot_size + 40);
                                if (rlBcOutRec.DataBuffer != NULL) {
                                    rlBcOutRec.DataBufferSize = prlBcBcHead->tot_size;
                                }
                            }
                            ilOffset = (prlBcBcHead->act_buf - 1) * rlBcOutRec.BcHead.cmd_size;
                            if ((ilOffset + prlBcBcHead->cmd_size) <= rlBcOutRec.DataBufferSize) {
                                memcpy(rlBcOutRec.DataBuffer + ilOffset, prlBcCmdBlk->data, prlBcBcHead->cmd_size);
                                rlBcOutRec.DataBuffer[ilOffset + prlBcBcHead->cmd_size] = '\0';
                            }
                            ilGetNextFullBc = FALSE;
                        }
                    } else {
                        if (rlBcOutRec.DataBufferSize < prlBcBcHead->tot_size) {
                            rlBcOutRec.DataBuffer = (char *) realloc(rlBcOutRec.DataBuffer, prlBcBcHead->tot_size + 40);
                            if (rlBcOutRec.DataBuffer != NULL) {
                                rlBcOutRec.DataBufferSize = prlBcBcHead->tot_size;
                            }
                        }
                        ilOffset = (prlBcBcHead->act_buf - 1) * rlBcOutRec.BcHead.cmd_size;
                        if ((ilOffset + prlBcBcHead->cmd_size) <= rlBcOutRec.DataBufferSize) {
                            memcpy(rlBcOutRec.DataBuffer + ilOffset, prlBcCmdBlk->data, prlBcBcHead->cmd_size);
                            rlBcOutRec.DataBuffer[ilOffset + prlBcBcHead->cmd_size] = '\0';
                        }
                        ilGetNextFullBc = FALSE;
                    }

                    if ((ilGetNextFullBc == FALSE) && (prlBcBcHead->act_buf == prlBcBcHead->tot_buf)) {
                        /*
                        dbg(TRACE,"%s LAST: PACK BC_REC %d (%d/%d) CMD <%s> TBL <%s> ", pcpMyChild,
                                   prlBcBcHead->bc_num,prlBcBcHead->act_buf,prlBcBcHead->tot_buf,
                                   prlBcCmdBlk->command,prlBcCmdBlk->obj_name);
                         */

                        prlBcBcHead = (BC_HEAD *) & rlBcOutRec.BcHead;
                        prlBcCmdBlk = (CMDBLK *) & rlBcOutRec.CmdBlk;

                        pclBcSel = (char *) rlBcOutRec.DataBuffer;
                        pclBcFld = (char *) pclBcSel + strlen(pclBcSel) + 1;
                        pclBcDat = (char *) pclBcFld + strlen(pclBcFld) + 1;

                        /*
                        dbg(TRACE,"%s SEND: PACK BC_REC %d (%d/%d) CMD <%s> TBL <%s> ", pcpMyChild,
                                   prlBcBcHead->bc_num,prlBcBcHead->act_buf,prlBcBcHead->tot_buf,
                                   prlBcCmdBlk->command,prlBcCmdBlk->obj_name);
                        dbg(DEBUG,"%s BC_FLD <%s>",pcpMyChild,pclBcFld);
                        dbg(DEBUG,"%s BC_DAT <%s>",pcpMyChild,pclBcDat);
                        dbg(DEBUG,"%s BC_SEL <%s>",pcpMyChild,pclBcSel);
                         */

                        /* Checking the client's field list filter */
                        /* A data filter might be necessary too. */
                        ilBcFound = TRUE;
                        if (ilGotFld == TRUE) {
                            ilBcFound = FALSE;
                            ilItmCnt = field_count(pcpKeyFld);
                            for (ilNum = 1; ((ilBcFound == FALSE) && ilNum <= ilItmCnt); ilNum++) {
                                ilLen = get_real_item(pclTmpBuf, pcpKeyFld, ilNum);
                                if (ilLen > 0) {
                                    if (strstr(pclBcFld, pclTmpBuf) != NULL) {
                                        ilBcFound = TRUE;
                                    }
                                }
                            }
                        }
                        if (ilBcFound == TRUE) {
                            ilBcOutCount++;
                            pclPtr = prpRecDesc->Value;

                            ilOutPos = 16;
                            StrgPutStrg(pclPtr, &ilOutPos, "{=CMD=}", 0, -1, "GBCH");
                            StrgPutStrg(pclPtr, &ilOutPos, "{=QUE=}", 0, -1, "12345");
                            sprintf(pclTmpBuf, "%05d", ilBcOutCount);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=COUNT=}", 0, -1, pclTmpBuf);
                            sprintf(pclTmpBuf, "%05d", ilRequestCount);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=FETCH=}", 0, -1, pclTmpBuf);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=PACK=}", 0, -1, "1");

                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCREC=}", 0, -1, "");
                            strncpy(pclTmpBuf, prlBcBcHead->seq_id, 10);
                            pclTmpBuf[10] = '\0';
                            /* Simulate a BC SEQ_ID */
                            sprintf(pclTmpBuf, "%010d", ilRequestCount);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCSEQ=}", 0, -1, pclTmpBuf);
                            sprintf(pclTmpBuf, "%d", prlBcBcHead->bc_num);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCNUM=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcBcHead->ref_seq_id, 10);
                            pclTmpBuf[10] = '\0';
                            /* Simulate a BC REF_SEQ_ID */
                            sprintf(pclTmpBuf, "%010d", ilRequestCount);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCRSQ=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcCmdBlk->command, 6);
                            pclTmpBuf[6] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCCMD=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcCmdBlk->obj_name, 33);
                            pclTmpBuf[33] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCTBL=}", 0, -1, pclTmpBuf);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCSEL=}", 0, -1, pclBcSel);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCFLD=}", 0, -1, pclBcFld);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCDAT=}", 0, -1, pclBcDat);
                            strncpy(pclTmpBuf, prlBcCmdBlk->tw_start, 33);
                            pclTmpBuf[33] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCTWS=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcCmdBlk->tw_end, 33);
                            pclTmpBuf[33] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCTWE=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcBcHead->dest_name, 10);
                            pclTmpBuf[10] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCUSR=}", 0, -1, pclTmpBuf);
                            strncpy(pclTmpBuf, prlBcBcHead->recv_name, 10);
                            pclTmpBuf[10] = '\0';
                            StrgPutStrg(pclPtr, &ilOutPos, "{=BCWKS=}", 0, -1, pclTmpBuf);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=/BCREC=}", 0, -1, "");

                            pclPtr[ilOutPos] = 0x00;
                            ilTotalLen = ilOutPos;
                            prpRecDesc->UsedSize = ilTotalLen;
                            ilOutPos = 0;
                            sprintf(pclTmpBuf, "%09d", ilTotalLen);
                            StrgPutStrg(pclPtr, &ilOutPos, "{=TOT=}", 0, -1, pclTmpBuf);
                            /* dbg(TRACE,"%s <%s>",pcpMyChild,pclOutData); */


                            /* TrimAndFilterCr(pclPtr,pcpFldSep,pcpRecSep); */
                            SetGrpMemberInfo(igMyGrpNbr, 1, "W", 0);
                            ilRC = SendDataToClient(pcpMyChild, pclPtr, ilTotalLen, connfd, FALSE);
                            SetGrpMemberInfo(igMyGrpNbr, 1, "P", 0);
                            prpRecDesc->UsedSize = 0;

                            ilRequestCount--;
                        }
                        ilGetNextFullBc = TRUE;
                    }

                    if (ilGetNextFullBc == FALSE) {
                        ilBcRec++;
                        if (ilBcRec >= MAX_BC_NUM) {
                            ilBcRec = 1;
                        }
                    }
                } else {
                    dbg(TRACE, "%s ERROR WHILE READING BC_STACK FILE", pcpMyChild);
                    ilBreakOut = TRUE;
                }
            }
            if (ilRequestCount <= 0) {
                ilBreakOut = TRUE;
                dbg(TRACE, "%s STOP BC_STACK LOOP: REQUESTED AMOUNT TRANSMITTED", pcpMyChild);
            }
        } /* end while */


        ilLoopLastRec = ilBcRec;
        ilRC = fseek(rlStackFile, 0, SEEK_SET);
        fread((void *) prlStHd, PACKET_LEN, 1, rlStackFile);
        dbg(DEBUG, "%s BCNUM: CURR_ACTUAL=%d END_LOOP=%d", pcpMyChild, prlStHd->ActBcNum, ilLoopFirstRec);
        ilNewActBcNum = prlStHd->ActBcNum;
        dbg(TRACE, "%s BC_STACK LOOP STOPPED AT BC_REC %d", pcpMyChild, ilBcRec);
        dbg(TRACE, "%s BCNUM: FIRST=%d LAST=%d ACTUAL=%d", pcpMyChild, ilLoopFirstRec, ilLoopLastRec, ilNewActBcNum);

        pclPtr = prpRecDesc->Value;
        ilOutPos = 0;
        StrgPutStrg(pclPtr, &ilOutPos, "{=CMD=}", 0, -1, "GBCH");
        StrgPutStrg(pclPtr, &ilOutPos, "{=QUE=}", 0, -1, "12345");
        sprintf(pclTmpBuf, "%05d", ilBcOutCount);
        StrgPutStrg(pclPtr, &ilOutPos, "{=SENT=}", 0, -1, pclTmpBuf);
        sprintf(pclTmpBuf, "%05d", ilLoopFirstRec);
        StrgPutStrg(pclPtr, &ilOutPos, "{=FIRST=}", 0, -1, pclTmpBuf);
        sprintf(pclTmpBuf, "%05d", ilLoopLastRec);
        StrgPutStrg(pclPtr, &ilOutPos, "{=LAST=}", 0, -1, pclTmpBuf);
        sprintf(pclTmpBuf, "%05d", ilNewActBcNum);
        StrgPutStrg(pclPtr, &ilOutPos, "{=ACTUAL=}", 0, -1, pclTmpBuf);
        pclPtr[ilOutPos] = 0x00;
        ilTotalLen = ilOutPos;
        prpRecDesc->UsedSize = ilTotalLen;
        ilOutPos = 0;

        fclose(rlStackFile);
        dbg(TRACE, "%s CLOSED BC STACK FILE", pcpMyChild);
    }
    return ilRC;
} /* end GetBcHistory */

/* ******************************************************************** */

/* ******************************************************************** */
static int GetBcPacket(char *pcpMyChild, FILE *rpStackFile, int ipBcNum, COMMIF *prpReadBcPkt, int *pipBcCount) {
    int ilRC = RC_SUCCESS;
    BC_HEAD *prlBcBcHead = NULL;
    int ilBcNum = 0;
    int ilCnt = 0;
    int ilReadAgain = TRUE;
    long llRecPos = 0;
    ilBcNum = -1;
    if ((ipBcNum > 0) && (ipBcNum <= MAX_BC_NUM)) {
        ilCnt = 0;
        ilBcNum = ipBcNum;
        while (ilReadAgain == TRUE) {
            ilCnt++;
            ilReadAgain = FALSE;
            llRecPos = (ilBcNum * PACKET_LEN);
            fseek(rpStackFile, llRecPos, SEEK_SET);
            fread((void *) prpReadBcPkt, PACKET_LEN, 1, rpStackFile);
            prlBcBcHead = (BC_HEAD *) prpReadBcPkt->data;
            SetShortValue(&prlBcBcHead->bc_num, prlBcBcHead->bc_num);
            SetShortValue(&prlBcBcHead->tot_buf, prlBcBcHead->tot_buf);
            SetShortValue(&prlBcBcHead->act_buf, prlBcBcHead->act_buf);
            SetShortValue(&prlBcBcHead->rc, prlBcBcHead->rc);
            SetShortValue(&prlBcBcHead->tot_size, prlBcBcHead->tot_size);
            SetShortValue(&prlBcBcHead->cmd_size, prlBcBcHead->cmd_size);
            SetShortValue(&prlBcBcHead->data_size, prlBcBcHead->data_size);
            dbg(DEBUG, "%s CURRENT BC RECORD: BCNUM=%d ACT_BUF=%d TOT_BUF=%d",
                    pcpMyChild, prlBcBcHead->bc_num, prlBcBcHead->act_buf, prlBcBcHead->tot_buf);
            if (*pipBcCount == 0) {
                if (prlBcBcHead->act_buf > 1) {
                    dbg(DEBUG, "%s MUST JUMP TO FIRST PART OF BC_NUM %d", pcpMyChild, prlBcBcHead->bc_num);
                    if (ilCnt == 1) {
                        ilBcNum = ilBcNum - prlBcBcHead->act_buf + 1;
                        if (ilBcNum < 1) {
                            ilBcNum = MAX_BC_NUM + ilBcNum;
                        }
                        ilReadAgain = TRUE;
                    } else {
                        dbg(TRACE, "%s SOMETHING WENT WRONG WITH BC_NUM %d", pcpMyChild, prlBcBcHead->bc_num);
                    }
                }
            }
        } /* end while */
        *pipBcCount += 1;
    }
    return ilBcNum;
} /* end GetBcPacket */

/* ******************************************************************** */

/* ******************************************************************** */
static void SetShortValue(short *pspDest, short spValue) {
#if defined(_HPUX_SOURCE) || defined(_SNI) || defined(_SOLARIS) || defined(_AIX)
    *pspDest = ccs_htons(spValue);
#else
    *pspDest = spValue;
#endif
    return;
} /* end SetShortValue */

/******************************************************************************/
/* WaitAndCheckQueue **********************************************************/

/******************************************************************************/
static int WaitAndCheckQueue(int ipTimeout, int ipModId, ITEM **prpItem) {
    int ilRC = RC_SUCCESS;
    int ilBreakOut = FALSE;
    long llBgnTime = 0;
    long llNowTime = 0;
    long llNowDiff = 0;
    long llMaxDiff = 0;

    igGotTimeOut = FALSE;
    llMaxDiff = ipTimeout - 5;
    llBgnTime = time(0L);
    dbg(DEBUG, "%s WAITING %d SEC FOR CEDA ANSWER", pcgMyProcName, llMaxDiff);
    do {
        ilRC = CheckQueue(ipModId, prpItem, TRUE);
        if (ilRC == QUE_E_NOMSG) {
            llNowTime = time(0L);
            llNowDiff = llNowTime - llBgnTime;
            if (llNowDiff >= llMaxDiff) {
                dbg(TRACE, "%s WAITED TOO LONG (%d SEC)", pcgMyProcName, llNowDiff);
                igGotTimeOut = TRUE;
                ilBreakOut = TRUE;
                ilRC = RC_FAIL;
            }
            CheckChildTask(FALSE);
            if (igShutDownChild == TRUE) {
                ilBreakOut = TRUE;
                ilRC = RC_FAIL;
            }
            napms(100); /* nap 100 ms */
        }
    } while ((ilRC == QUE_E_NOMSG) && (ilBreakOut == FALSE));

    if (igHsbDown == TRUE) {
        igAnswerQueue = 0;
        ilBreakOut = TRUE;
    }

    if (ilBreakOut == FALSE) {
        llNowTime = time(0L);
        llNowDiff = llNowTime - llBgnTime;
        dbg(DEBUG, "%s GOT CEDA ANSWER AFTER %d SEC", pcgMyProcName, llNowDiff);
    }

    return ilRC;
} /* end of WaitAndCheckQueue */

#else

static int HandleHrgCmd(int ipSend, char *pcpWho) {
    int ilRC = RC_SUCCESS;
    return ilRC;
}

static int HandleOraLogin(int ipLogin, char *pcpWho) {
    int ilRC = RC_SUCCESS;
    return ilRC;
}

static int ReadCedaArray(char *pcpMyChild, char *pcpKeyTbl, char *pcpKeyFld, char *pcpKeyWhe,
        REC_DESC *prpRecDesc, short spPackLines, int ipKeepOpen, int connfd,
        char *pcpFldSep, char *pcpRecSep, char *pcpOraHint) {
    int ilRC = RC_SUCCESS;
    dbg(TRACE, "%s RECEIVED UNEXPECTED COMMAND", pcgMyProcName);
    return ilRC;
} /* end ReadCedaArray */

static int WaitAndCheckQueue(int ipTimeout, int ipModId, ITEM **prpItem) {
    int ilRC = RC_SUCCESS;
    return ilRC;
} /* end of WaitAndCheckQueue */

#endif

/* ======================================================================= */
/* ========================= END CDRHDL ================================== */
/* ======================================================================= */

/* ======================================================================= */
/* ========================= CDRCOM ONLY ================================= */
/* THIS PART OF TOOL FUNCTIONS IS USED BY CDRCOM ONLY                      */
/* ======================================================================= */

#if defined(_BCQ)

/* =============================================================== */

/* =============================================================== */
static int HandleBcOut(char *pcpMyChild, int ipCpid, char *pcpHexAddr, int connfd) {
    int ilRC = RC_SUCCESS;
    int ilOutDatPos = 0;
    int ilBytesToSend = 0;
    int ilBcQueue = 0;
    char pclKeyQue[16];
    char pclOutData[4096];
    char pclDummy[32];

    igProcIsBcCom = TRUE;
    ilRC = HandleQueCreate(TRUE, "BCQ", 1);
    if (ilRC != RC_SUCCESS) {
        return (RC_FAIL);
    }

    sprintf(pclKeyQue, "%d", igAnswerQueue);

    ilOutDatPos = 16;
    StrgPutStrg(pclOutData, &ilOutDatPos, "{=CMD=}", 0, -1, "BCOUT");
    StrgPutStrg(pclOutData, &ilOutDatPos, "{=QUE=}", 0, -1, pclKeyQue);
    StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, "0");
    pclOutData[ilOutDatPos] = 0x00;
    ilBytesToSend = ilOutDatPos;
    sprintf(pclDummy, "%09d", ilOutDatPos);
    ilOutDatPos = 0;
    StrgPutStrg(pclOutData, &ilOutDatPos, "{=TOT=}", 0, -1, pclDummy);
    /* dbg(TRACE,"%s <%s>",pcpMyChild,pclOutData); */

    ilRC = SendDataToClient(pcpMyChild, pclOutData, ilBytesToSend, connfd, FALSE);

    dbg(TRACE, "===== %s RUNNING AS BC_OUT NOW =====", pcpMyChild);
    dbg(TRACE, "%s BC_OUT_CONNECTOR <%s> ON QUEUE %d", pcpMyChild, pcgQueueName, igAnswerQueue);
    dbg(TRACE, "%s TCP/IP <%s> HEX <%s>", pcgMyProcName, pcgMyIpAddr, pcgMyHexAddr);

    ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
            0, /* Set to mod_id if < 1        */
            "", /* BC_HEAD.dest_name           */
            "", /* BC_HEAD.recv_name           */
            "", /* CMDBLK.tw_start             */
            "", /* CMDBLK.tw_end               */
            "BCOUT", /* CMDBLK.command              */
            "BCQCOM", /* CMDBLK.obj_name             */
            pcgMyHexAddr, /* Selection Block             */
            pclKeyQue, /* Field Block                 */
            pcgQueueName, /* Data Block                  */
            "", /* Error description           */
            1, /* 0 or Priority               */
            RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */


    dbg(TRACE, "%s ATTACHED TO BCHDL AS %s MEMBER (%d)", pcpMyChild, pcgMyShortName, igMyGrpNbr);

    ilRC = WaitAndCheckBcQueue(pcpMyChild, connfd, igAnswerQueue, &prgItem);

    dbg(TRACE, "%s STOPPED READING FROM QUEUE %d", pcpMyChild, igAnswerQueue);

    if (igHsbDown == TRUE) {
        igAnswerQueue = 0;
    }
    if (igStopQueGet == TRUE) {
        igAnswerQueue = 0;
    }

    HandleQueCreate(FALSE, "BCQ", 1);

    close(connfd);
    Terminate();

    return RC_SUCCESS;
} /* end HandleBcOut */

/* =============================================================== */

/* =============================================================== */
static int HandleBcKey(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd) {
    int ilRC = RC_SUCCESS;
    int ilBcQueue = 0;
    char pclKeyQue[32];

    ilBcQueue = atoi(pcpKeyQue);
    if (ilBcQueue > 0) {
        ilRC = SendCedaEvent(ilBcQueue, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "DestName", /* BC_HEAD.dest_name           */
                "RecvName", /* BC_HEAD.recv_name           */
                "TwStart", /* CMDBLK.tw_start             */
                "TwEnd", /* CMDBLK.tw_end               */
                "BCKEY", /* CMDBLK.command              */
                "TBL", /* CMDBLK.obj_name             */
                pcpData, /* Selection Block             */
                "FLD", /* Field Block                 */
                "DAT", /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

        ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "", /* BC_HEAD.dest_name           */
                "", /* BC_HEAD.recv_name           */
                "", /* CMDBLK.tw_start             */
                "", /* CMDBLK.tw_end               */
                "BCKEY", /* CMDBLK.command              */
                "BCQCOM", /* CMDBLK.obj_name             */
                pcgMyHexAddr, /* Selection Block             */
                pcpKeyQue, /* Field Block                 */
                pcpData, /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

    } /* end if */
    Terminate();
    return RC_SUCCESS;
} /* end HandleBcKey */

/* =============================================================== */

/* =============================================================== */
static int HandleBcGet(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd) {
    int ilRC = RC_SUCCESS;
    int ilBcQueue = 0;

    ilBcQueue = atoi(pcpKeyQue);
    if (ilBcQueue > 0) {
        ilRC = SendCedaEvent(ilBcQueue, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "DestName", /* BC_HEAD.dest_name           */
                "RecvName", /* BC_HEAD.recv_name           */
                "TwStart", /* CMDBLK.tw_start             */
                "TwEnd", /* CMDBLK.tw_end               */
                "BCGET", /* CMDBLK.command              */
                "TBL", /* CMDBLK.obj_name             */
                pcpData, /* Selection Block             */
                "FLD", /* Field Block                 */
                "DAT", /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

        ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "", /* BC_HEAD.dest_name           */
                "", /* BC_HEAD.recv_name           */
                "", /* CMDBLK.tw_start             */
                "", /* CMDBLK.tw_end               */
                "BCGET", /* CMDBLK.command              */
                "BCQCOM", /* CMDBLK.obj_name             */
                pcgMyHexAddr, /* Selection Block             */
                pcpKeyQue, /* Field Block                 */
                pcpData, /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

    } /* end if */
    Terminate();
    return RC_SUCCESS;
} /* end HandleBcGet */

/* =============================================================== */

/* =============================================================== */
static int HandleBcOff(char *pcpMyChild, char *pcpKeyQue, char *pcpHexAddr, int connfd) {
    int ilRC = RC_SUCCESS;
    int ilBcQueue = 0;

    ilBcQueue = atoi(pcpKeyQue);
    if (ilBcQueue > 0) {
        ilRC = SendCedaEvent(igToBcHdl, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "", /* BC_HEAD.dest_name           */
                "", /* BC_HEAD.recv_name           */
                "", /* CMDBLK.tw_start             */
                "", /* CMDBLK.tw_end               */
                "BCOFF", /* CMDBLK.command              */
                "BCQCOM", /* CMDBLK.obj_name             */
                pcgMyHexAddr, /* Selection Block             */
                pcpKeyQue, /* Field Block                 */
                "", /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

        ilRC = SendCedaEvent(ilBcQueue, /* adress to send the answer   */
                0, /* Set to mod_id if < 1        */
                "DestName", /* BC_HEAD.dest_name           */
                "RecvName", /* BC_HEAD.recv_name           */
                "TwStart", /* CMDBLK.tw_start             */
                "TwEnd", /* CMDBLK.tw_end               */
                "BCOFF", /* CMDBLK.command              */
                "TBL", /* CMDBLK.obj_name             */
                pcpHexAddr, /* Selection Block             */
                "FLD", /* Field Block                 */
                pcpKeyQue, /* Data Block                  */
                "", /* Error description           */
                1, /* 0 or Priority               */
                RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS or   */

    } /* end if */
    Terminate();
    return RC_SUCCESS;
} /* end HandleBcOff */

/******************************************************************************/
/* WaitAndCheckBcQueue **********************************************************/

/******************************************************************************/
static int WaitAndCheckBcQueue(char *pcpMyChild, int connfd, int ipModId, ITEM **prpItem) {
    int ilRC = RC_SUCCESS;
    int ilCntAll = 0;
    int ilCntNow = 0;
    int ilBreakOut = FALSE;
    int ilGetAck = FALSE;
    int ilGotBcKey = FALSE;
    int llAckTime = 0;
    char pclTmpBuf[64];
    char *pclCloCmd = "{=TOT=}000000058{=BCNUM=}0{=CMD=}CLO{=TBL=}CLOTAB{=PACK=}0";
    char *pclOutData = NULL;
    lgChkTime = time(0L);
    llAckTime = time(0L);

    SetGrpMemberInfo(igMyGrpNbr, 3, "A", 2);
    SetGrpMemberInfo(igMyGrpNbr, 5, "R", 0);
    do {
        ilRC = CheckQueue(ipModId, prpItem, FALSE);
        if (ilRC == RC_SUCCESS) {
            ilRC = TransLateEvent(&rgKeyItemListOut, FALSE);
            if (rgKeyItemListOut.UsedLen > 0) {
                if (rgKeyItemListOut.Value != NULL) {
                    ilCntNow++;
                    ilCntAll++;
                    pclOutData = rgKeyItemListOut.Value;
                    if (igStopBcHdl == TRUE) {
                        ilGetAck = TRUE;
                    }
                    ilRC = SendDataToClient(pcpMyChild, pclOutData, -1, connfd, ilGetAck);
                    if (ilCntAll == 1) {
                        SetGrpMemberInfo(igMyGrpNbr, 5, "B", 2);
                    }
                    if (igCheckFile == TRUE) {
                        CheckBcqFile();
                    }
                } else {
                    dbg(TRACE, "OUT LIST IS NULL !!");
                }
            }/* end if */
            else if (rgKeyItemListOut.UsedLen == 0) {
                dbg(TRACE, "%s DON'T SEND EVENT WITHOUT DATA", pcpMyChild);
            }/* end else if */
            else {
                switch (rgKeyItemListOut.UsedLen) {
                    case -1:
                        dbg(TRACE, "%s RECEIVED <BCOFF> DISCONNECT FROM QUEUE", pcpMyChild);
                        ilBreakOut = TRUE;
                        break;
                    case -2:
                        dbg(TRACE, "%s RECEIVED <BCKEY>: STORE BC FILTER ", pcpMyChild);
                        dbg(TRACE, "%s EVENT DATA <%s>", pcpMyChild, rgKeyItemListOut.Value);
                        ilGotBcKey = TRUE;
                        break;
                    case -3:
                        dbg(TRACE, "%s RECEIVED <BCGET>: WAIT BC RESEND ", pcpMyChild);
                        dbg(TRACE, "%s EVENT DATA <%s>", pcpMyChild, rgKeyItemListOut.Value);
                        break;
                    default:
                        break;
                } /* end switch */
            } /* end else */
        }

        if (igHsbDown == TRUE) {
            SendDataToClient(pcpMyChild, pclCloCmd, -1, connfd, TRUE);
            ilBreakOut = TRUE;
        }

        lgNowTime = time(0L);
        lgChkDiff = lgNowTime - lgChkTime;
        if ((lgChkDiff >= igAlarmStep) || (ilGotBcKey == TRUE)) {
            sprintf(pclTmpBuf, "T%06d.N%05d.B%05d.%s.", ilCntAll, ilCntNow, igRcvBcNum, pcgBcKeyFlag);
            SetGrpMemberInfo(igMyGrpNbr, 9, pclTmpBuf, 0);
            CheckChildStatus();
            if (igStatusAck == TRUE) {
                lgChkDiff = lgNowTime - llAckTime;
                dbg(TRACE, "%s       SBC SINCE %d SEC: <%s>", pcpMyChild, lgChkDiff, pclTmpBuf);
                ilCntNow = 0;
                llAckTime = time(0L);
            }
            if (igShutDownChild == TRUE) {
                ilBreakOut = TRUE;
            }
            ilGotBcKey = FALSE;
            lgChkTime = time(0L);
        }
        if (igStopQueGet == TRUE) {
            ilBreakOut = TRUE;
        }

    } while ((ilRC == RC_SUCCESS) && (ilBreakOut != TRUE));

    sprintf(pclTmpBuf, "T%06d.N%05d.B%05d.%s.", ilCntAll, ilCntNow, igRcvBcNum, pcgBcKeyFlag);
    SetGrpMemberInfo(igMyGrpNbr, 9, pclTmpBuf, 0);

    return ilRC;



} /* end of WaitAndCheckBcQueue */

#else

static int HandleBcOut(char *pcpMyChild, int ipCpid, char *pcpHexAddr, int connfd) {
    int ilRC = RC_SUCCESS;
    dbg(TRACE, "%s RECEIVED UNEXPECTED COMMAND BCOUT", pcgMyProcName);
    return RC_SUCCESS;
} /* end HandleBcOut */

static int HandleBcKey(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd) {
    int ilRC = RC_SUCCESS;
    dbg(TRACE, "%s RECEIVED UNEXPECTED COMMAND BCKEY", pcgMyProcName);
    return RC_SUCCESS;
} /* end HandleBcKey */

static int HandleBcGet(char *pcpMyChild, char *pcpKeyQue, char *pcpData, int connfd) {
    int ilRC = RC_SUCCESS;
    dbg(TRACE, "%s RECEIVED UNEXPECTED COMMAND BCGET", pcgMyProcName);
    return RC_SUCCESS;
} /* end HandleBcGet */

static int HandleBcOff(char *pcpMyChild, char *pcpKeyQue, char *pcpHexAddr, int connfd) {
    int ilRC = RC_SUCCESS;
    dbg(TRACE, "%s RECEIVED UNEXPECTED COMMAND BCOFF", pcgMyProcName);
    return RC_SUCCESS;
} /* end HandleBcOff */

#endif

static int RunSQL(char *pcpMyChild, char *pcpSelection, char *pcpData) {
    int ilRc = DB_SUCCESS;
    short slSqlFunc = 0;
    short slSqlCursor = 0;
    char pclErrBuff[128];
    char *pclFunc = "RunSQL";

    ilRc = sql_if(START, &slSqlCursor, pcpSelection, pcpData);
    dbg(DEBUG, "%s Query <%s> ilRc <%d>", pcpMyChild, pcpSelection, ilRc);
    close_my_cursor(&slSqlCursor);

    if (ilRc == DB_ERROR) {
        get_ora_err(ilRc, &pclErrBuff[0]);
        dbg(TRACE, "%s <%s> Oracle Error <%s>", pcpMyChild, pclFunc, &pclErrBuff[0]);
    }
    return ilRc;
}

static int GetFMLTabData(char *pcpMyChild, char *pcpLine, char *pcpUaft, int ipTabCnt) {
    int ilRc;
    int ili, ilf, ilField;
    int ilLen;
    char pclLine[1024];
    char pclTmpStr[2048];
    char pclSqlData[2048];
    char pclSqlBuf[1024];

    strcpy(pclLine, pcpLine);
    TrimSpace(pclLine);

    /*** keyword D - 1st field ***/
    get_real_item(pclTmpStr, pclLine, 1);
    if (pclTmpStr[0] != 'D')
        return RC_FAIL;

    /*** get table name - 2nd field ***/
    memset(FTN[ipTabCnt].tabname, 0, sizeof (FTN[ipTabCnt].tabname));
    ilRc = get_real_item(pclTmpStr, pclLine, 2);
    if (ilRc == RC_FAIL || strlen(pclTmpStr) <= 0)
        return RC_FAIL;
    memcpy(FTN[ipTabCnt].tabname, pclTmpStr, sizeof (FTN[ipTabCnt].tabname));
    dbg(TRACE, "%s TABLE NAME <%s>", pcpMyChild, FTN[ipTabCnt].tabname);

    /*** get where clause - 3rd field ***/
    memset(FTN[ipTabCnt].condition, 0, sizeof (FTN[ipTabCnt].condition));
    ilRc = get_real_item(pclTmpStr, pclLine, 3);
    if (ilRc == RC_FAIL || strlen(pclTmpStr) <= 0)
        return RC_FAIL;
    memcpy(FTN[ipTabCnt].condition, pclTmpStr, sizeof (FTN[ipTabCnt].condition));
    dbg(TRACE, "%s CONDITION <%s>", pcpMyChild, FTN[ipTabCnt].condition);

    /*** get field count - 4th field ***/
    ilRc = get_real_item(pclTmpStr, pclLine, 4);
    FTN[ipTabCnt].fldcnt = atoi(pclTmpStr);
    if (ilRc == RC_FAIL || strlen(pclTmpStr) <= 0 || FTN[ipTabCnt].fldcnt <= 0)
        return RC_FAIL;
    dbg(TRACE, "%s FIELD COUNT <%d>", pcpMyChild, FTN[ipTabCnt].fldcnt);

    /*** DB field list ***/
    FTN[ipTabCnt].fld = (FML_FLD_NODE *) malloc(sizeof (FML_FLD_NODE) * FTN[ipTabCnt].fldcnt);
    if (FTN[ipTabCnt].fld == NULL)
        return RC_FAIL;
    dbg(TRACE, "%s ALLOCATED <%d> FIELDS BUFFER ", pcpMyChild, FTN[ipTabCnt].fldcnt);

    /* Cut DB fields */
    strcpy(pclSqlBuf, "SELECT ");

    ilField = 0;
    ili = 5;
    while (ilField < FTN[ipTabCnt].fldcnt) {
        memset(pclTmpStr, 0, sizeof (pclTmpStr));
        get_real_item(pclTmpStr, pclLine, ili++);
        ilLen = strlen(pclTmpStr);
        for (ilf = 0; ilf < ilLen; ilf++) {
            if (pclTmpStr[ilf] == '\n' || pclTmpStr[ilf] == '\r' || pclTmpStr[ilf] == ' ') {
                pclTmpStr[ilf] = '\0';
                break;
            }
        }

        strcpy(FTN[ipTabCnt].fld[ilField].fldname, pclTmpStr);
        strcat(pclSqlBuf, FTN[ipTabCnt].fld[ilField].fldname);
        strcat(pclSqlBuf, ",");
        ilField++;

        if (ilField >= FTN[ipTabCnt].fldcnt)
            break;
    } /* while loop */
    pclSqlBuf[strlen(pclSqlBuf) - 1] = '\0'; /* this is a comma */
    dbg(TRACE, "%s FIELDS <%s>", pcpMyChild, pclSqlBuf);
    sprintf(pclTmpStr, " FROM %s WHERE %s = '%s'", FTN[ipTabCnt].tabname, FTN[ipTabCnt].condition, pcpUaft);
    strcat(pclSqlBuf, pclTmpStr);

    /** Only expect 1 record **/
    ilRc = RunSQL(pcpMyChild, pclSqlBuf, pclSqlData);
    dbg(TRACE, "%s <%s> ilRc <%d>", pcpMyChild, pclSqlBuf, ilRc);
    if (ilRc != DB_SUCCESS) {
        free(FTN[ipTabCnt].fld);
        return RC_FAIL;
    }

    /* Cut DB data */
    for (ilf = 0; ilf < FTN[ipTabCnt].fldcnt; ilf++) {
        get_fld(pclSqlData, ilf, STR, 1028, FTN[ipTabCnt].fld[ilf].flddata);
        TrimSpace(FTN[ipTabCnt].fld[ilf].flddata);
        dbg(TRACE, "%s %d<%s> FIELD[%d] <%s> DATA <%s>", pcpMyChild, ipTabCnt, FTN[ipTabCnt].tabname, ilf,
                FTN[ipTabCnt].fld[ilf].fldname, FTN[ipTabCnt].fld[ilf].flddata);
    }
    FTN[ipTabCnt].valid = 1;
    return RC_SUCCESS;
}

static int GetFMLFieldData(char *pcpMyChild, char *pcpConfigName, char *pcpAftUrno, char *pcpFieldData) {
    short slSqlCursor;
    short slSqlFunc;
    int ilRc, ilDbRc;
    int ili, ilj;
    int ilLen;
    char pclTabName[32];
    char pclFieldName[32];
    char pclConfigName[128];
    char pclSqlData[2048];
    char pclSqlBuf[1024];
    char pclTmpStr[512];
    char *pclP;

    ilLen = strlen(pcpConfigName);
    if (pcpConfigName == NULL || ilLen <= 0)
        return RC_FAIL;

    strcpy(pclConfigName, pcpConfigName);
    ilj = 0;
    for (ili = 0; ili < ilLen; ili++) {
        if (pclConfigName[ili] == '.')
            break;
        pclTabName[ilj++] = pclConfigName[ili];
    }
    pclTabName[ilj] = '\0';
    if (ilj <= 0 || ili >= ilLen)
        return RC_FAIL;

    memcpy(pclFieldName, &pclConfigName[ili + 1], ilLen - ili - 1);
    pclFieldName[ilLen - ili - 1] = '\0';
    if (strlen(pclFieldName) <= 0)
        return RC_FAIL;

    TrimSpace(pclTabName);
    TrimSpace(pclFieldName);

    dbg(DEBUG, "%s TABLE <%s> FIELD <%s>", pcpMyChild, pclTabName, pclFieldName);

    ilRc = RC_SUCCESS; /* to skip more checking */
    ilDbRc = RC_FAIL;

    memset(pclSqlData, 0, sizeof (pclSqlData));
    if (!strcmp(pclTabName, "LDM"))
    {
        ilDbRc = CalcLoad(pcpMyChild, pclFieldName, pcpAftUrno, pclSqlData);
    }
    else if (!strcmp(pclTabName, "STAFF"))
    {
        ilDbRc = GetStaffInfo(pcpMyChild, pclFieldName, pcpAftUrno, pclSqlData);
    }
    else if (!strcmp(pclTabName, "STIME"))
    {
        ilLen = strlen(pclFieldName);
        for (ili = 0; ili < ilLen; ili++)
        {
            if (pclFieldName[ili] == '_')
                pclFieldName[ili] = '-';
        }
        dbg(DEBUG, "%s Retrieving iTrek status timing from ITATAB", pcpMyChild);
        sprintf(pclSqlBuf, "SELECT TRIM(CONA) FROM ITATAB WHERE FLNU = '%s' AND ISNM = '%s' ORDER BY CDAT DESC",
                pcpAftUrno, pclFieldName);
        ilDbRc = RunSQL(pcpMyChild, pclSqlBuf, pclSqlData);
    } else
        ilRc = RC_FAIL;

    if (ilRc == RC_SUCCESS) {
        if (ilDbRc == DB_SUCCESS || ilDbRc == RC_SUCCESS) {
            strcpy(pcpFieldData, pclSqlData);
            dbg(DEBUG, "%s [Custom] DATA <%s>", pcpMyChild, pcpFieldData);
            return RC_SUCCESS;
        } else {
            dbg(DEBUG, "%s [Custom] NO DATA", pcpMyChild);
            return RC_FAIL;
        }
    }

    ilRc = RC_SUCCESS; /* intiailize flag */
    for (ili = 0; ili < MAX_FML_TAB; ili++) {
        if ((FTN[ili].valid != 1) || !strcmp(FTN[ili].tabname, pclTabName))
            break;
    }
    if (ili >= MAX_FML_TAB || FTN[ili].valid != 1)
        ilRc = RC_FAIL;

    if (ilRc == RC_SUCCESS) {
        for (ilj = 0; ilj < FTN[ili].fldcnt; ilj++) {
            if (!strcmp(FTN[ili].fld[ilj].fldname, pclFieldName))
                break;
        }
        if (ilj >= FTN[ili].fldcnt)
            ilRc = RC_FAIL;
    }
    if (ilRc == RC_SUCCESS) {
        strcpy(pcpFieldData, FTN[ili].fld[ilj].flddata);
        dbg(DEBUG, "%s [Config] DATA <%s>", pcpMyChild, pcpFieldData);
        return ilRc;
    }

    /* Search in DORXXX */
    sprintf(pclSqlBuf, "SELECT SQLS FROM DORSIN WHERE KEYS = '%s' ORDER BY SEQN", pclConfigName);
    dbg(TRACE, "%s SelDOR <%s>", pcpMyChild, pclSqlBuf);
    slSqlCursor = 0;
    slSqlFunc = START;
    ilRc = sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData);
    if (ilRc != DB_SUCCESS) {
        dbg("%s: Invalid config <%s>", pcpMyChild, pclConfigName);
        close_my_cursor(&slSqlCursor);
        return RC_FAIL;
    }
    while (ilRc == DB_SUCCESS) {
        if (strncmp(pclSqlData, "SELECT ", 7))
            dbg(TRACE, "%s: Not SELECT Query <%s>", pcpMyChild, pclSqlData);
        else if ((pclP = strstr(pclSqlData, "WHERE")) == NULL)
            dbg(TRACE, "%s: Missing WHERE clause <%s>", pcpMyChild, pclSqlData);
        else {
            sprintf(pclTmpStr, pclSqlData, pcpAftUrno);
            strcpy(pclSqlData, pclTmpStr);
            memset(pclTmpStr, 0, sizeof (pclTmpStr));
            dbg(TRACE, "%s: DOR Query <%s>", pcpMyChild, pclSqlData);
            ilRc = RunSQL(pcpMyChild, pclSqlData, pclTmpStr);
            if (ilRc == DB_SUCCESS && pclTmpStr[0] != '\0') {
                strcpy(pcpFieldData, pclTmpStr);
                break;
            }
        }
        slSqlFunc = NEXT;
        ilRc = sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData);
        if (ilRc != DB_SUCCESS)
            break;
    }
    close_my_cursor(&slSqlCursor);
    dbg(DEBUG, "%s [DORSIN] DATA <%s>", pcpMyChild, pcpFieldData);
    return RC_SUCCESS;
}

/* trim front and back space only */
int TrimSpace(char *pcpInStr) {
    int ils = 0, ile = 0;
    int ilLen;
    char *pclTmpStr;
    char *pclFunc = "TrimSpace";

    if (pcpInStr == NULL || (ilLen = strlen(pcpInStr)) <= 0)
        return;
    for (ils = 0; ils < ilLen; ils++)
        if (pcpInStr[ils] != ' ')
            break;
    if (ils == ilLen) {
        pcpInStr[0] = '\0';
        return;
    }
    for (ile = ilLen - 1; ile >= 0; ile--)
        if (pcpInStr[ile] != ' ')
            break;
    if (ils == 0 && ile >= ils) {
        pcpInStr[ile + 1] = '\0';
        return;
    }
    pclTmpStr = (char *) malloc(ilLen + 10);
    memcpy(pclTmpStr, &pcpInStr[ils], ile - ils + 1);
    memset(pcpInStr, 0, ilLen);
    strcpy(pcpInStr, pclTmpStr);
    free(pclTmpStr);
}

static int TranslateTime(char *pcpMyChild, char *pcpFmlTime, char *pcpAftUrno) {
    int ilRc;
    int ili;
    int ilMinutes = 0;
    int ilFmlMinutes = 0;
    int ilFmlDiff = 0;
    int ilDummy = 0;
    int ilLen = 0;
    char pclFmlTime[32] = "";
    char pclFmlConfig[32] = "\0";
    char pclTmpStr[512] = "\0";
    char pclFmlData[128] = "\0";
    char pclFmlTist[32] = "";

    strcpy(pclFmlTime, pcpFmlTime);
    TrimSpace(pclFmlTime);

    ilLen = strlen(pclFmlTime);
    if (ilLen <= 0)
        return RC_FAIL;

    memset(pclFmlConfig, 0, sizeof (pclFmlConfig));
    memset(pclTmpStr, 0, sizeof (pclTmpStr));
    if (strncmp(pclFmlTime, "....", 4) && ilLen > 0) {
        for (ili = 0; ili < ilLen; ili++) {
            if (pclFmlTime[ili] == '-' || pclFmlTime[ili] == '+')
                break;
        }
        memcpy(pclFmlConfig, pclFmlTime, ili);
        ilRc = GetFMLFieldData(pcpMyChild, pclFmlConfig, pcpAftUrno, pclFmlData);
        if (ilRc != RC_SUCCESS || strlen(pclFmlData) <= 0)
            return RC_FAIL;
        dbg(DEBUG, "%s TIME <%s> DATA <%s>", pcpMyChild, pclFmlConfig, pclFmlData);
        ilFmlDiff = 0;
        if (ili < ilLen) {
            ilFmlDiff = atoi(&pclFmlTime[ili]);
            dbg(DEBUG, "%s TIME DIFF <%s> DATA <%d>", pcpMyChild, &pclFmlTime[ili], ilFmlDiff);
        }

        /* not STIME, then is UTC timing */
        if (strncmp(pclFmlConfig, "STIME.", 6)) {
            (void) GetFullDay(pclFmlData, &ilDummy, &ilMinutes, &ilDummy);
            ilFmlMinutes = ilMinutes + 480 + ilFmlDiff;
            (void) FullTimeString(pclFmlTist, ilFmlMinutes, "00");
            strncpy(pclFmlTime, &pclFmlTist[8], 4);
            pclFmlTime[4] = '\0';
        } else {
            strcpy(pclFmlTime, " ");
            if (strlen(pclFmlData) == 14) {
                memcpy(pclFmlTime, &pclFmlData[8], 4);
                pclFmlTime[4] = '\0';
            }
        }
        strcpy(pcpFmlTime, pclFmlTime);
    }
    return RC_SUCCESS;
} /* TranslateTime */

static int TranslateText(char *pcpMyChild, char *pcpFmlText, char *pcpAftUrno) {
    int ilRc;
    int ili, ilj, ilk;
    int ilTextLen;
    int ilLineLen;
    int ilNumVia = 0;
    short slSqlCursor;
    short slSqlFunc;
    char pclSqlBuf[1024];
    char pclSqlData[2048];
    char pclFmlData[1028] = "";
    char pclFmlConfig[128] = "\0";
    char pclFmlVar[1024] = "\0";
    char pclFmlLine[1024] = "\0";
    char pclFmlText[2048] = "\0";
    char pclViaStr[56] = "";
    char pclAftVial[1028] = "";
    char pclTmpStr[1028] = "\0";
    char *pclP;

    strcpy(pclFmlText, pcpFmlText);
    TrimSpace(pclFmlText);
    ilTextLen = strlen(pclFmlText);

    if (ilTextLen <= 0)
        return RC_FAIL;

    ili = ilj = 0;
    memset(pclFmlLine, 0, sizeof (pclFmlLine));
    while (ili < ilTextLen && pclFmlText[ili] != '|')
        pclFmlLine[ilj++] = pclFmlText[ili++];
    pclFmlLine[ilj] = '\0';
    dbg(TRACE, "%s FML TEXT <%s>", pcpMyChild, pclFmlLine);

    ili++;
    ilj = 0;
    memset(pclFmlData, 0, sizeof (pclFmlData));
    for (ili; ili < ilTextLen; ili++) {
        if (pclFmlText[ili] != '|') {
            pclFmlConfig[ilj++] = pclFmlText[ili];
            continue;
        }
        pclFmlConfig[ilj] = '\0';
        dbg(DEBUG, "%s CONFIG <%s>", pcpMyChild, pclFmlConfig);

        ilRc = GetFMLFieldData(pcpMyChild, pclFmlConfig, pcpAftUrno, pclFmlData);
        if (ilRc == RC_SUCCESS) {
            if (!strcmp(pclFmlConfig, "AFTTAB.VIAL")) {
                if (strlen(rgOAFT.vvia) <= 0) {
                    memset(pclViaStr, 0, sizeof (pclViaStr));
                    strcpy(pclAftVial, pclFmlData);
                    ilRc = GetFMLFieldData(pcpMyChild, "AFTTAB.VIAN", pcpAftUrno, pclTmpStr);
                    if (ilRc == RC_SUCCESS) {
                        ilNumVia = atoi(pclTmpStr);
                        if (ilNumVia < 0 || ilNumVia > 9)
                            ilNumVia = 0;
                        for (ilk = 0; ilk < ilNumVia; ilk++) {
                            sprintf(pclTmpStr, "%3.3s", &pclAftVial[ilk * 120]);
                            strcat(pclViaStr, pclTmpStr);
                            if (ilk < ilNumVia)
                                strcat(pclViaStr, "/");
                        }
                    }
                    strcpy(pclFmlData, pclViaStr);
                } else
                    strcpy(pclFmlData, rgOAFT.vvia);
            }
        }
        dbg(DEBUG, "%s DATA <%s>", pcpMyChild, pclFmlData);

        memset(pclFmlVar, 0, sizeof (pclFmlVar));
        ilLineLen = strlen(pclFmlLine);
        for (ilk = 0; ilk < ilLineLen; ilk++) {
            pclFmlVar[ilk] = pclFmlLine[ilk];
            if (pclFmlLine[ilk] == '%') {
                ilk++;
                while (ilk < ilLineLen && pclFmlLine[ilk] != '%')
                    pclFmlVar[ilk] = pclFmlLine[ilk++];
                break;
            }
        } /* for loop */
        dbg(DEBUG, "%s FML VAR <%s>", pcpMyChild, pclFmlVar);
        sprintf(pclTmpStr, pclFmlVar, pclFmlData);
        strncat(pclTmpStr, &pclFmlLine[ilk], ilLineLen);
        strcpy(pclFmlLine, pclTmpStr);
        dbg(DEBUG, "%s FML LINE <%s>", pcpMyChild, pclFmlLine);

        memset(pclFmlConfig, 0, sizeof (pclFmlConfig));
        ilj = 0;
    }

    strcpy(pcpFmlText, pclFmlLine);
    return RC_SUCCESS;
} /* TranslateText */

static int CalcLoad(char *pcpMyChild, char *pcpFieldName, char *pcpAftUrno, char *pcpFieldData) {
    short slSqlCursor;
    short slSqlFunc;
    int ilRc;
    int ili;
    int ilNumVia;
    char pclSqlBuf[1024];
    char pclSqlData[2048];
    char pclTmpBuf[512];
    char pclTmpBuf2[512];
    char pclDate[20];

    if (strcmp(pcpFieldName, "FINAL_LOAD") && strcmp(pcpFieldName, "CARGO_LOAD") &&
            strcmp(pcpFieldName, "PAX_LOAD") && strcmp(pcpFieldName, "BAG_LOAD"))
        return RC_FAIL;

    /* Check whether flight is circular */
    if (bgCircularFlight == STAT_NOT_CHECK) {
        memset(&rgOAFT, 0, sizeof (AFTREC));
        memset(&rgJAFT, 0, sizeof (AFTREC));

        /* Get flight info */
        GetFMLFieldData(pcpMyChild, "AFTTAB.FLNO", pcpAftUrno, rgOAFT.flno);
        GetFMLFieldData(pcpMyChild, "AFTTAB.FLDA", pcpAftUrno, rgOAFT.flda);
        GetFMLFieldData(pcpMyChild, "AFTTAB.ADID", pcpAftUrno, rgOAFT.adid);
        GetFMLFieldData(pcpMyChild, "AFTTAB.VIAL", pcpAftUrno, rgOAFT.vial);
        GetFMLFieldData(pcpMyChild, "AFTTAB.VIAN", pcpAftUrno, rgOAFT.vian);
        GetFMLFieldData(pcpMyChild, "AFTTAB.ORG3", pcpAftUrno, rgOAFT.org3);
        GetFMLFieldData(pcpMyChild, "AFTTAB.DES3", pcpAftUrno, rgOAFT.des3);
        GetFMLFieldData(pcpMyChild, "AFTTAB.TIFA", pcpAftUrno, rgOAFT.tifa);
        GetFMLFieldData(pcpMyChild, "AFTTAB.TIFD", pcpAftUrno, rgOAFT.tifd);

        ilNumVia = atoi(rgOAFT.vian);
        if (ilNumVia < 0 || ilNumVia > 9)
            ilNumVia = 0;
        rgOAFT.vvia[0] = '\0';
        for (ili = 0; ili < ilNumVia; ili++) {
            sprintf(pclTmpBuf, "%3.3s", &rgOAFT.vial[ili * 120]);
            strcat(rgOAFT.vvia, pclTmpBuf);
            if ((ili + 1) < ilNumVia)
                strcat(rgOAFT.vvia, "/");
        }
        dbg(DEBUG, "%s OAFT VVIA <%s>", pcpMyChild, rgOAFT.vvia);

        sprintf(pclTmpBuf, "%s000000", rgOAFT.flda);
        AddSecondsToCEDATime(pclTmpBuf, igCircularOffset * SECONDS_PER_DAY, 1);
        memcpy(pclDate, pclTmpBuf, 8);

        sprintf(pclSqlBuf, "SELECT URNO,TRIM(ORG3),TRIM(DES3),TRIM(VIAL),TRIM(VIAN) FROM AFTTAB WHERE FLNO = '%s' "
                "AND FLDA BETWEEN '%s' AND '%s' AND ", rgOAFT.flno, rgOAFT.flda, pclDate);
        sprintf(pclTmpBuf, " ADID = 'A' AND TIFA > '%s' ORDER BY TIFA", rgOAFT.tifd);
        if (rgOAFT.adid[0] == 'A')
            sprintf(pclTmpBuf, " ADID = 'D' AND TIFD > '%s' ORDER BY TIFD", rgOAFT.tifa);
        strcat(pclSqlBuf, pclTmpBuf);

        ilRc = RunSQL(pcpMyChild, pclSqlBuf, pclSqlData);
        bgCircularFlight = STAT_FALSE;
        if (ilRc == DB_SUCCESS) {
            get_real_item(rgJAFT.urno, pclSqlData, 1);
            get_real_item(rgJAFT.org3, pclSqlData, 2);
            get_real_item(rgJAFT.des3, pclSqlData, 3);
            get_real_item(rgJAFT.vial, pclSqlData, 4);
            get_real_item(rgJAFT.vian, pclSqlData, 5);
            dbg(DEBUG, "%s FOUND FLIGHT <%s> WITH SAME FLIGHT NUMBER", pcpMyChild, rgJAFT.urno);

            /* Check mirroring path of flight */
            if (strcmp(rgOAFT.org3, rgJAFT.des3) || strcmp(rgOAFT.des3, rgJAFT.org3))
                bgCircularFlight = STAT_TRUE;
            else {
                /* Same number of vias, not sure if different number of vias what to do */
                if (!strcmp(rgOAFT.vian, rgJAFT.vian) && ilNumVia > 0) {
                    for (ili = 0; ili < ilNumVia; ili++) {
                        sprintf(pclTmpBuf, "%3.3s", &rgJAFT.vial[ili * 120]);
                        strcat(rgJAFT.vvia, pclTmpBuf);
                        if ((ili + 1) < ilNumVia)
                            strcat(rgJAFT.vvia, "/");
                    }
                    dbg(DEBUG, "%s JAFT VVIA <%s>", pcpMyChild, rgJAFT.vvia);

                    /* check mirror via */
                    for (ili = 0; ili < ilNumVia; ili++) {
                        memcpy(pclTmpBuf, &rgOAFT.vvia[ili * 4], 3);
                        memcpy(pclTmpBuf2, &rgJAFT.vvia[(ilNumVia - ili - 1)*4], 3);
                        if (strcmp(pclTmpBuf, pclTmpBuf2)) {
                            bgCircularFlight = STAT_TRUE;
                            break;
                        }
                    }
                } else
                    bgCircularFlight = STAT_TRUE;
            }
        }
        dbg(TRACE, "%s CIRCULAR FLIGHT <%s>", pcpMyChild, bgCircularFlight == STAT_TRUE ? "YES" : "NO");

        /* Get load data */
        ilNumVia = atoi(rgOAFT.vian);
        igNumLDMLoad = 1;
        if (bgCircularFlight == STAT_TRUE)
            igNumLDMLoad = ilNumVia + 1;
        dbg(DEBUG, "%s Number of LDM Load <%d>", pcpMyChild, igNumLDMLoad);

        /* For PAX,CARGO and BAG load */
        prgLoadRec = (LOADREC *) malloc(sizeof (LOADREC) * igNumLDMLoad);
        memset(prgLoadRec, 0, sizeof (LOADREC) * igNumLDMLoad);

        /* LoadRec = A->C and B->C */
        strcpy(prgLoadRec[0].sapc, rgOAFT.org3);
        strcpy(prgLoadRec[0].dapc, rgOAFT.des3);

        for (ili = 0; ili < ilNumVia; ili++) {
            if (rgOAFT.adid[0] == 'A') {
                if (igNumLDMLoad <= 1) /* should be = 1, just to play safe */
                    memcpy(prgLoadRec[ili].sapc, &rgOAFT.vvia[(ilNumVia - 1)*4], 3);
                else {
                    memcpy(prgLoadRec[ili + 1].sapc, &rgOAFT.vvia[ili * 4], 3);
                    strcpy(prgLoadRec[ili + 1].dapc, rgOAFT.des3);
                }
            } else {
                if (igNumLDMLoad <= 1)
                    memcpy(prgLoadRec[ili].dapc, rgOAFT.vvia, 3);
                else {
                    strcpy(prgLoadRec[ili + 1].sapc, rgOAFT.org3);
                    strcpy(prgLoadRec[ili + 1].dapc, rgOAFT.des3);
                    memcpy(prgLoadRec[ili].dapc, &rgOAFT.vvia[ili * 4], 3);
                }
            }
            dbg(DEBUG, "%s HOP <%d> SRC <%s> --> DEST <%s>", pcpMyChild, ili,
                    prgLoadRec[ili].sapc, prgLoadRec[ili].dapc);
        }
        if (igNumLDMLoad > 1)
            dbg(DEBUG, "%s HOP <%d> SRC <%s> -> DEST <%s>", pcpMyChild, ili,
                prgLoadRec[ili].sapc, prgLoadRec[ili].dapc);

        GetLDMLoad(pcpMyChild, pcpAftUrno);

        dbg(DEBUG, "%s COMPLETED CIRCULAR CHECK AND LOAD INFO RETRIEVE", pcpMyChild);
    } /* Finish Circular Check */

    /* Final load dun have to loop, just take the last station  */
    if (!strcmp(pcpFieldName, "FINAL_LOAD")) {
        if (rgOAFT.adid[0] == 'D') {
            dbg(DEBUG, "%s ERROR! FINAL_LOAD is not meant for DEP flight", pcpMyChild);
            return RC_FAIL;
        }
        ili = igNumLDMLoad - 1;
        if (ili < 0)
            return RC_FAIL;
        sprintf(pclTmpBuf, "%s/%s", prgLoadRec[ili].paxB, prgLoadRec[ili].paxE);
        if (strlen(prgLoadRec[ili].paxI) > 0) {
            sprintf(pclSqlBuf, "+%d INF TTL-", atoi(prgLoadRec[ili].paxI));
            strcat(pclTmpBuf, pclSqlBuf);
        }
        if (strlen(prgLoadRec[ili].paxB) <= 0 && strlen(prgLoadRec[ili].paxE) <= 0 && strlen(prgLoadRec[ili].paxI) <= 0)
            strcat(pclTmpBuf, " ");
        else {
            sprintf(pclSqlBuf, "%d", atoi(prgLoadRec[ili].paxB) + atoi(prgLoadRec[ili].paxE) + atoi(prgLoadRec[ili].paxI));
            strcat(pclTmpBuf, pclSqlBuf);
        }
        strcpy(pcpFieldData, pclTmpBuf);
        dbg(DEBUG, "%s 1. <%s>-DATA <%s>", pcpMyChild, pcpFieldName, pcpFieldData);
        return RC_SUCCESS;
    }

    for (ili = 0; ili < igNumLDMLoad; ili++) {
        dbg(DEBUG, "......<%d>........<%s> - <%s> ..........", ili, prgLoadRec[ili].sapc, prgLoadRec[ili].dapc);
        if (!strcmp(pcpFieldName, "PAX_LOAD")) {
            if (ili == 0 || rgOAFT.adid[0] == 'D')
                sprintf(pclTmpBuf, "%3.3s- %s/%s/%s",
                    rgOAFT.adid[0] == 'A' ? prgLoadRec[ili].sapc : prgLoadRec[ili].dapc,
                    prgLoadRec[ili].paxA, prgLoadRec[ili].paxC, prgLoadRec[ili].paxI);
            else {
                sprintf(pclTmpBuf, "%3.3s- ", prgLoadRec[ili].sapc);
                if (strlen(prgLoadRec[ili].paxA) > 0 || strlen(prgLoadRec[ili - 1].paxA) > 0) {
                    sprintf(pclSqlBuf, "%d", atoi(prgLoadRec[ili].paxA) - atoi(prgLoadRec[ili - 1].paxA));
                    strcat(pclTmpBuf, pclSqlBuf);
                }
                strcat(pclTmpBuf, "/");

                if (strlen(prgLoadRec[ili].paxC) > 0 || strlen(prgLoadRec[ili - 1].paxC) > 0) {
                    sprintf(pclSqlBuf, "%d", atoi(prgLoadRec[ili].paxC) - atoi(prgLoadRec[ili - 1].paxC));
                    strcat(pclTmpBuf, pclSqlBuf);
                }
                strcat(pclTmpBuf, "/");

                if (strlen(prgLoadRec[ili].paxI) > 0 || strlen(prgLoadRec[ili - 1].paxI) > 0) {
                    sprintf(pclSqlBuf, "%d", atoi(prgLoadRec[ili].paxI) - atoi(prgLoadRec[ili - 1].paxI));
                    strcat(pclTmpBuf, pclSqlBuf);
                }
            }
        } else if (!strcmp(pcpFieldName, "BAG_LOAD")) {
            sprintf(pclTmpBuf, "%3.3s- %s/%s",
                    (rgOAFT.adid[0] == 'A') ? prgLoadRec[ili].sapc : prgLoadRec[ili].dapc,
                    prgLoadRec[ili].numbag, prgLoadRec[ili].bagwt);

        } else if (!strcmp(pcpFieldName, "CARGO_LOAD")) {
            sprintf(pclTmpBuf, "%3.3s- %s",
                    (rgOAFT.adid[0] == 'A') ? prgLoadRec[ili].sapc : prgLoadRec[ili].dapc,
                    prgLoadRec[ili].cargowt);
        }


        sprintf(pclSqlData, "%-18.18s", pclTmpBuf);
        strcat(pcpFieldData, pclSqlData);
        dbg(DEBUG, "%s HOP <%d> <%s>-<%s>", pcpMyChild, ili, pcpFieldName, pclSqlData);
    }
    dbg(DEBUG, "%s 2. <%s>-DATA <%s>", pcpMyChild, pcpFieldName, pcpFieldData);
    return RC_SUCCESS;

} /* CalcLoad */

static int GetStaffInfo(char *pcpMyChild, char *pclFieldName, char *pcpAftUrno, char *pcpFieldData) {
    short slSqlCursor;
    short slSqlFunc;
    int ilRc;
    int ili;
    char *pclFunc = "GetStaffInfo";
    char pclSqlBuf[1024] = "\0";
    char pclSqlBuf2[1024] = "\0";
    char pclSqlData[2048] = "\0";
    char pclTmpBuf[1024] = "\0";

    for (ili = 0; ili < igFccoSettings; ili++)
    {
        sprintf(pclTmpBuf, "FCCO_%s", pclFieldName);
        if (!strcmp(rgFccoNode[ili].fcco, pclTmpBuf))
            break;
    }

    dbg(DEBUG, "%s line<%d> pclTmpBuf<%s>", pclFunc, __LINE__,pclTmpBuf);


    if (pclTmpBuf == NULL || strcmp(rgFccoNode[ili].fcco, pclTmpBuf))
    {
        dbg(DEBUG, "%s CANNOT FIND FCCO SETTING <%s>", pcpMyChild, pclFieldName);
        return RC_FAIL;
    }

    dbg(DEBUG, "%s line<%d> pcpAftUrno<%s>", pclFunc, __LINE__, pcpAftUrno);

    sprintf(pclSqlBuf, "SELECT TRIM(USTF) FROM JOBTAB WHERE UAFT = '%s' AND FCCO IN (%s)",
            pcpAftUrno, rgFccoNode[ili].fcco_list);

    dbg(DEBUG, "%s STAFF SELECT <%s>", pcpMyChild, pclSqlBuf);

    ilRc = RC_SUCCESS;
    slSqlCursor = 0;
    slSqlFunc = START;
    pcpFieldData[0] = '\0';
    while (ilRc == RC_SUCCESS) {
        ilRc = sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData);
        if (ilRc != DB_SUCCESS)
            break;
        slSqlFunc = NEXT;
        sprintf(pclSqlBuf2, "SELECT TRIM(FINM) FROM STFTAB WHERE URNO = %s", pclSqlData);
        memset(pclTmpBuf, 0, sizeof (pclTmpBuf));
        ilRc = RunSQL(pcpMyChild, pclSqlBuf2, pclTmpBuf);
        if (ilRc != DB_SUCCESS) {
            dbg(DEBUG, "%s STAFF NOT FOUND", pcpMyChild);
            ilRc = RC_SUCCESS;
            break;
        }
        strip_special_char(pclTmpBuf);
        ConvertDbStringToClient(pclTmpBuf);
        strcat(pcpFieldData, pclTmpBuf);
        strcat(pcpFieldData, ",");
        dbg(DEBUG, "%s STAFF <%s> FOUND", pcpMyChild, pclTmpBuf);
    }
    close_my_cursor(&slSqlCursor);
    ili = strlen(pcpFieldData);
    if (ili > 1) {
        pcpFieldData[ili - 2] = '\0'; /* to remove the comma */
        dbg(DEBUG, "%s STAFF LIST <%s> FOUND", pcpMyChild, pcpFieldData);
    } else {
        dbg(DEBUG, "%s NO JOB FOUND FOR FCCO <%s>", pcpMyChild, pclFieldName);
        return RC_FAIL;
    }

    return RC_SUCCESS;
}

int strip_special_char(char *pcpProcessStr) {
    int blStripped = FALSE;
    int ilLen;
    int ili, ilj;
    unsigned char clChar;
    char *pclStr;
    char *pclFunc = "strip_special_char";

    ilLen = strlen(pcpProcessStr);
    /*dbg( DEBUG, "%s: Orig len <%d>", pclFunc, ilLen );*/
    if (ilLen <= 0)
        return blStripped;
    pclStr = (char *) malloc(ilLen + 10);

    ilj = 0;
    for (ili = 0; ili < ilLen; ili++) {
        clChar = pcpProcessStr[ili] & 0xFF;
        if ((clChar >= 0x20 && clChar <= 0x21) ||
                (clChar >= 0x23 && clChar <= 0x25) ||
                (clChar >= 0x28 && clChar <= 0x3B) ||
                clChar == 0x3D ||
                (clChar >= 0x3F && clChar <= 0x5F) ||
                (clChar >= 0x61 && clChar <= 0x7E)) {
            pclStr[ilj++] = pcpProcessStr[ili];
            continue;
        }
        blStripped = TRUE;
        pclStr[ilj++] = ' ';
    }

    pclStr[ilj] = '\0';
    strcpy(pcpProcessStr, pclStr);
    /*dbg( DEBUG, "%s: New len <%d>", pclFunc, strlen(pcpProcessStr) );*/
    if (blStripped == TRUE)
        dbg(TRACE, "%s: str <%s>", pclFunc, pcpProcessStr);
    if (pclStr != NULL)
        free(pclStr);
    return blStripped;
}

static int HandleFML_Command(int ipQue, char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpField, char *pcpData, BC_HEAD *prpBchd, CMDBLK *prpCmdblk)
{
    char *pclFunc = "HandleFML_Command"; /* the function name */
    char *pclMyChild = pclFunc;

    int ilRC = RC_SUCCESS; /* Return code */
    int ilCpid; /* the child pid */
    int ilGotError = FALSE;
    int ilCmdAlive = FALSE;
    int ilEndAlive = FALSE;

    #if 0
        char *pclAliveAnsw = "ALIVE{=ALIVE=}ALIVE";
    #endif // 0

    char *pclTimeOutMsg = "ERROR: TIMEOUT WHILE WAITING FOR ANSWER";
    char *pclAnyErrMsg = "ERROR: UNEXPECTED MESSAGE FROM SYSQCP";

    EVENT *prlEvent = NULL;
    BC_HEAD *prlBchead = NULL;
    CMDBLK *prlCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;
    char *pclError = NULL;
    char pclNoError[] = {'\0'}; /* Change pointer to quoted string to an array (strup error) */
    char pclEmpty[] = {'\0'}; /* Change pointer to quoted string to an array (strup error) */

    long llActSize = 0;
    int ilSendToRouter = TRUE;

    int connfd = -1;

    char pclDummy[32] = "\0";

    char pclChkCmd[64] = "\0";
    char pclCurKey[64] = "\0";
    char pclKeyEvt[64] = "\0";
    char pclKeyTot[64] = "\0";
    char pclKeyCmd[64] = "\0";
    char pclKeyIdn[64] = "\0";
    char pclKeyTbl[64] = "\0";
    char pclKeyExt[64] = "\0";
    char pclKeyHop[64] = "\0";
    /*
      char pclKeyFldX[4096*2];
      char pclKeyWheX[4096*6];
      char pclKeyDatX[4096*6];
     */
    char *pclKeyWhe = NULL;
    char *pclKeyFld = NULL;
    char *pclKeyDat = NULL;
    char pclKeyTws[64] = "\0";
    char pclKeyTwe[64] = "\0";
    char pclKeyUsr[64] = "\0";
    char pclKeyWks[64] = "\0";
    char pclKeySep[64] = "\0";
    char pclFldSep[64] = "\0";
    char pclRecSep[64] = "\0";
    char pclKeyApp[64] = "\0";
    char pclKeyQue[64] = "\0";
    char pclKeyHex[64] = "\0";
    char pclKeyPak[64] = "\0";
    char pclKeyAli[64] = "\0";
    char pclKeyTim[64] = "\0";
    char pclKeyHnt[128] = "\0";
    char *pclKeyPtr = NULL;
    char *pclOutPtr = NULL;
    int ilEvtCnt = 0;
    int ilCurEvt = 0;
    int ilTist = 0;
    int ilMultEvt = FALSE;
    int ilKeepOpen = FALSE;
    long llDatLen = 0L;
    short slMaxPak = 0L;
    int ilBytesSent = 0;
    int ilSentThisTime = 0;
    int ilBytesToSend = 0;
    char pclTmpBuf[1024] = "\0";

    int ilTotDatLen = 0;
    int ilTotRcvLen = 0;
    int ilRemaining = 0;
    int ilBreakOut = FALSE;

    int ilRcvDatLen = 0;
    int ilRcvDatPos = 0;
    char *pclRcvData = NULL;
    char *pclEvtData = NULL;

    int ilCurBufLen = 0;
    int ilCurStrLen = 0;
    int ilOutDatLen = 0;
    int ilOutDatPos = 0;
    long llMySize = 0;
    char *pclOutData = NULL;
    REC_DESC rlRecDesc;
    KEY_DESC rlKeyDat;
    KEY_DESC rlKeyWhe;
    KEY_DESC rlKeyFld;

    igProcIsChild = TRUE;
    igAnswerQueue = 0;


    strcpy(pcgMyProcName, pclMyChild);
    #if 0
        ilCpid = getpid();
        igMyPid = ilCpid;
        igMyParentPid = getppid();
        sprintf(pclMyChild, "CHILD %05d>>", ilCpid);
        strcpy(pcgMyProcName, pclMyChild);
        dbg(TRACE, "===== %s IS RUNNING NOW =====", pclMyChild);
        dbg(TRACE, "%s MY CONFIG FILE <%s>", pcgMyProcName, cgConfigFile);
        dbg(TRACE, "%s TCP/IP <%s> HEX <%s>", pcgMyProcName, pcgMyIpAddr, pcgMyHexAddr);
        InitCdrGrpFile(FALSE);
        igMyGrpNbr = ipGrpMbr;
        SetGrpMemberInfo(igMyGrpNbr, 2, "", 0);
        /*dbg(DEBUG,"%s DISABLE DBG LOGFILE FEATURES",pcgMyProcName);
        SetDbgLimits (0,1);*/
    #endif // 0

    //dbg(DEBUG, "%s Inside-1", pclFunc);

    CT_InitStringDescriptor(&rgKeyItemListOut);
    CT_InitStringDescriptor(&rgRcvBuff);
    InitRecordDescriptor(&rlRecDesc, (1024 * 1024), 0, FALSE);
    igOraLoginOk = FALSE;

    #if 0
        igShutDownChild = FALSE;
    #endif // 0

    lgEvtBgnTime = time(0L);

    igHrgAlive = TRUE;
    //ilEndAlive = FALSE;
    //while (ilEndAlive == FALSE)
    {
        //ilEndAlive = TRUE;

        igWaitForAck = FALSE;

        #if 0
            CheckChildTask(FALSE);
        #endif // 0

        #if 0
            if (igShutDownChild == TRUE) {
                close(connfd);
                Terminate();
            }

            if ((igTmpAlive == TRUE) || (ilCmdAlive == TRUE)) {
                dbg(TRACE, "%s WAITING FOR NEXT COMMAND FROM CLIENT", pclMyChild);
            } else {
                dbg(TRACE, "%s WAITING FOR COMMAND FROM CLIENT", pclMyChild);
            }
            ilRC = ReadMsgFromSock(connfd, &rgRcvBuff, FALSE);

            pclRcvData = rgRcvBuff.Value;
        #endif

        //dbg(DEBUG, "%s Inside-2", pclFunc);
        dbg(TRACE,"%s Received Data <%s>", pclMyChild,pcpData);

        //rgRcvBuff.Value = pcpData;
        //dbg(DEBUG, "%s Inside-2.1", pclFunc);


        if (pcpData == NULL || strlen(pcpData) <= 0)
        {
            pclRcvData = pclFunc;
        }
        else
        {
            pclRcvData = pcpData;
        }

        //dbg(DEBUG, "%s Inside-2.2", pclFunc);

        pclData = NULL;
        lgTotalRecs = 0;
        lgTotalByte = 0;
        lgEvtBgnTime = time(0L);
#ifdef WMQ
        /* reset all open queues inherited from the parent cdrhdl */
        WmqResetQuetable();
#endif
        //dbg(DEBUG, "%s Inside-3", pclFunc);
        pclKeyPtr = GetKeyItem(pclKeyEvt, &llDatLen, pclRcvData, "{=EVT=}", "{=", TRUE);
        if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=EVT=}", pclKeyEvt, llDatLen);
        {
            ilEvtCnt = atoi(pclKeyEvt);
        }

        if (ilEvtCnt > 0)
        {
            dbg(TRACE, "%s MULTI KEY EVENTS RECEIVED (%d)", pclMyChild, ilEvtCnt);
            ilMultEvt = TRUE;
            ilKeepOpen = TRUE;
        }/* end if */
        else
        {
            dbg(TRACE, "%s SINGLE KEY EVENT RECEIVED", pclMyChild);
            ilMultEvt = FALSE;
            ilKeepOpen = FALSE;
            ilEvtCnt = 1;
        } /* end else */

        #if 0
            pclKeyPtr = GetKeyItem(pclKeyTot, &llDatLen, pclRcvData, "{=TOT=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TOT=}", pclKeyTot, llDatLen);

            pclKeyPtr = GetKeyItem(pclKeyIdn, &llDatLen, pclRcvData, "{=IDN=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=IDN=}", pclKeyIdn, llDatLen);


            pclKeyPtr = GetKeyItem(pclKeyExt, &llDatLen, pclRcvData, "{=EXT=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=EXT=}", pclKeyExt, llDatLen);

            pclKeyPtr = GetKeyItem(pclKeyHop, &llDatLen, pclRcvData, "{=HOPO=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=HOPO=}", pclKeyHop, llDatLen);

            pclKeyPtr = GetKeyItem(pclKeyUsr, &llDatLen, pclRcvData, "{=USR=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=USR=}", pclKeyUsr, llDatLen);


            pclKeyPtr = GetKeyItem(pclKeyWks, &llDatLen, pclRcvData, "{=WKS=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=WKS=}", pclKeyWks, llDatLen);
                //SetGrpMemberInfo(igMyGrpNbr, 8, pclKeyWks, 0);
            }

            pclKeyPtr = GetKeyItem(pclKeyTws, &llDatLen, pclRcvData, "{=TWS=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TWS=}", pclKeyTws, llDatLen);
            }

            pclKeyPtr = GetKeyItem(pclKeyTwe, &llDatLen, pclRcvData, "{=TWE=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TWE=}", pclKeyTwe, llDatLen);
            }

            pclKeyPtr = GetKeyItem(pclKeyTim, &llDatLen, pclRcvData, "{=TIM=}", "{=", TRUE);
            if (llDatLen > 0) dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TIM=}", pclKeyTim, llDatLen);
            {
                igTimeOut = atoi(pclKeyTim);
            }
        #endif // 0

        pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=RECSEPA=}", "{=", TRUE);
        if (llDatLen > 0)
        {
            dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=RECSEPA=}", llDatLen);
        }

        if (pclKeyPtr != NULL)
        {
            strcpy(pclRecSep, pclKeySep);
        }
        else
        {
            pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=SEPA=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=SEPA=}", llDatLen);
            }

            if (pclKeyPtr != NULL)
            {
                strcpy(pclRecSep, pclKeySep);
            }
            else
            {
                strcpy(pclRecSep, "\n");
            }
        }

        pclKeyPtr = GetKeyItem(pclKeySep, &llDatLen, pclRcvData, "{=FLDSEPA=}", "{=", TRUE);
        if (llDatLen > 0)
        {
            dbg(TRACE, "%s %s <...> LEN=%d", pclMyChild, "{=FLDSEPA=}", llDatLen);
        }


        if (pclKeyPtr != NULL)
        {
            strcpy(pclFldSep, pclKeySep);
        }
        else
        {
            strcpy(pclFldSep, ",");
        }

        #if 0
            pclKeyPtr = GetKeyItem(pclKeyApp, &llDatLen, pclRcvData, "{=APP=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=APP=}", pclKeyApp, llDatLen);
                //SetGrpMemberInfo(igMyGrpNbr, 9, pclKeyApp, 0);
            }

            pclKeyPtr = GetKeyItem(pclKeyCmd, &llDatLen, pclRcvData, "{=CMD=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=CMD=}", pclKeyCmd, llDatLen);
            }


            pclKeyPtr = GetKeyItem(pclKeyTbl, &llDatLen, pclRcvData, "{=TBL=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=TBL=}", pclKeyTbl, llDatLen);
            }
        #endif // 0

            //rlKeyDat.ItemPtr = GetKeyItem(rlKeyDat.Value, &rlKeyDat.ItemLen, pclRcvData, "{=DAT=}", "{=", FALSE);
            //pclKeyCmd = pcpCommand;
            //pclKeyTbl = pcpTable;
            strcpy(pclKeyCmd,pcpCommand);
            strcpy(pclKeyTbl,pcpTable);
            rlKeyDat.Value = pclRcvData;

            //dbg(DEBUG, "%s pclKeyCmd<%s> pclKeyTbl<%s> rlKeyDat.Value<%s>", pclFunc, pclKeyCmd, pclKeyTbl, rlKeyDat.Value);

        #if 0
            pclKeyPtr = GetKeyItem(pclKeyQue, &llDatLen, pclRcvData, "{=QUE=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=QUE=}", pclKeyQue, llDatLen);
            }

            pclKeyPtr = GetKeyItem(pclKeyHex, &llDatLen, pclRcvData, "{=HEXIP=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=HEXIP=}", pclKeyHex, llDatLen);
            }
        #endif // 0

        pclKeyPtr = GetKeyItem(pclKeyPak, &llDatLen, pclRcvData, "{=PACK=}", "{=", TRUE);
        slMaxPak = 0;
        if (llDatLen > 0)
        {
            dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=PACK=}", pclKeyPak, llDatLen);
            slMaxPak = atoi(pclKeyPak);
        }
        sprintf(pclKeyPak, "%d", slMaxPak);


        igTmpAlive = FALSE;
        #if 0
            pclKeyPtr = GetKeyItem(pclKeyAli, &llDatLen, pclRcvData, "{=ALIVE=}", "{=", TRUE);
            if (llDatLen > 0)
            {
                dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=ALIVE=}", pclKeyAli, llDatLen);
                if (strcmp(pclKeyAli, "CLOSE") != 0)
                {
                    igHrgAlive = TRUE;
                    igTmpAlive = TRUE;
                    HandleHrgCmd(TRUE, "");
                }
            }
        #endif

        rlRecDesc.UsedSize = 0;
        rlRecDesc.LineCount = 0;
        rlRecDesc.AppendBuf = FALSE;

        ilRC = RC_SUCCESS;
        for (ilCurEvt = 1; ((ilCurEvt <= ilEvtCnt)&&(ilRC == RC_SUCCESS)); ilCurEvt++)
        {

            dbg(TRACE,"%s In the Loop",pclFunc);

            if (ilMultEvt == TRUE)
            {
                sprintf(pclCurKey, "{=EVT_%d=}", ilCurEvt);
                pclEvtData = GetKeyItem(pclKeyFld, &llDatLen, pclRcvData, pclCurKey, "{=EVT_", FALSE);
                if (llDatLen > 0)
                {
                    dbg(TRACE, "%s EVENT SECTION %s LEN=%d", pclMyChild, pclCurKey, llDatLen);
                }

                if (pclEvtData == NULL)
                {
                    dbg(TRACE, "%s KEY ITEM EVENT NOT FOUND", pclMyChild);
                    pclEvtData = pclRcvData;
                } /* end if */
            }/* end if */
            else
            {
                pclEvtData = pclRcvData;
            } /* end else */

            #if 0
                pclKeyPtr = GetKeyItem(pclKeyHnt, &llDatLen, pclEvtData, "{=ORAHINT=}", "{=", TRUE);
                if (llDatLen > 0)
                {
                    dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=ORAHINT=}", pclKeyHnt, llDatLen);
                }

                rlKeyWhe.ItemPtr = GetKeyItem(rlKeyWhe.Value, &rlKeyWhe.ItemLen, pclEvtData, "{=WHE=}", "{=", FALSE);
                rlKeyFld.ItemPtr = GetKeyItem(rlKeyFld.Value, &rlKeyFld.ItemLen, pclEvtData, "{=FLD=}", "{=", FALSE);
            #endif
            rlKeyWhe.Value = pcpSelection;
            rlKeyFld.Value = pcpField;

            pclKeyWhe = pclEmpty;
            pclKeyFld = pclEmpty;
            pclKeyDat = pclEmpty;

            //dbg(DEBUG, "%s rlKeyWhe.Value<%s> rlKeyFld.Value<%s> rlKeyDat.Value<%s>", pclFunc, rlKeyWhe.Value, rlKeyFld.Value, rlKeyDat.Value);

            pclKeyWhe = rlKeyWhe.Value;
            pclKeyFld = rlKeyFld.Value;
            pclKeyDat = rlKeyDat.Value;
            dbg(DEBUG, "%s pclKeyWhe<%s> pclKeyFld<%s> pclKeyDat<%s>", pclFunc, pclKeyWhe, pclKeyFld, pclKeyDat);

            #if 0
                if (rlKeyWhe.ItemPtr != NULL)
                {
                    rlKeyWhe.Help[0] = rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen];
                    rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen] = 0x00;
                    dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=WHE=}", rlKeyWhe.ItemPtr, rlKeyWhe.ItemLen);
                    pclKeyWhe = rlKeyWhe.ItemPtr;
                }

                if (rlKeyFld.ItemPtr != NULL)
                {
                    rlKeyFld.Help[0] = rlKeyFld.ItemPtr[rlKeyFld.ItemLen];
                    rlKeyFld.ItemPtr[rlKeyFld.ItemLen] = 0x00;
                    dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=FLD=}", rlKeyFld.ItemPtr, rlKeyFld.ItemLen);
                    pclKeyFld = rlKeyFld.ItemPtr;
                }

                if (rlKeyDat.ItemPtr != NULL)
                {
                    rlKeyDat.Help[0] = rlKeyDat.ItemPtr[rlKeyDat.ItemLen];
                    rlKeyDat.ItemPtr[rlKeyDat.ItemLen] = 0x00;
                    dbg(TRACE, "%s %s <%s> LEN=%d", pclMyChild, "{=DAT=}", rlKeyDat.ItemPtr, rlKeyDat.ItemLen);
                    pclKeyDat = rlKeyDat.ItemPtr;
                }
            #endif

            sprintf(pclChkCmd, ",%s,", pclKeyCmd);
            ilSendToRouter = FALSE;

            #if 0
                if (strstr(",GFR,GFRC,GFRS,", pclChkCmd) != NULL)
                {
                    if ((strstr(pclKeyWhe, "[CONFIG]") != NULL) ||
                            (strstr(pclKeyWhe, "[ROTATIONS]") != NULL) ||
                            (strstr(pclKeyWhe, "[USEFOGTAB,") != NULL)) {
                        ilSendToRouter = TRUE;
                    } /* end if */
                } /* end if */

                if ((strstr(",RRA,", pclChkCmd) != NULL) && (ilSendToRouter == FALSE))
                {
                    dbg(TRACE, "%s RRA TRANSACTION RECEIVED (OLD STYLE)", pclMyChild);
                    pclError = pclNoError;
                    pclData = pclNoError;
                } else if ((strstr(",RT,RTA,GFR,GFRC,GFRS,", pclChkCmd) != NULL) &&
                        (ilSendToRouter == FALSE))
            #endif
            if ((strstr(",FML,", pclChkCmd) != NULL) && (ilSendToRouter == FALSE))
            {
                ilSendToRouter = FALSE;
                rlRecDesc.FldSep = pclFldSep[0];
                rlRecDesc.RecSep = pclRecSep[0];

                if (ilCurEvt > 1)
                {
                    rlRecDesc.AppendBuf = TRUE;
                }

                if (ilCurEvt == ilEvtCnt)
                {
                    ilKeepOpen = FALSE;
                } /* end if */

                if (strcmp(pclKeyTbl, "FMLTAB") == 0)
                {
                    CheckFmlTemplate(pclMyChild, pclKeyTbl, pclKeyWhe);
                }

                ilRC = ReadCedaArray(pclMyChild, pclKeyTbl, pclKeyFld, pclKeyWhe, &rlRecDesc,
                        slMaxPak, ilKeepOpen, connfd, pclFldSep, pclRecSep, pclKeyHnt);

                if (ilRC == RC_SUCCESS)
                {
                    dbg(TRACE, "%s ReadCedaArray RETURN OK", pclMyChild);
                    pclError = pclNoError;
                    pclData = rlRecDesc.Value;
                    sprintf(pclKeyPak, "%d", rlRecDesc.LineCount);
                }/* end if */
                else
                {
                    dbg(TRACE, "%s ReadCedaArray RETURN ERROR", pclMyChild);
                    pclError = rlRecDesc.Value;
                    pclData = pclNoError;
                    strcpy(pclKeyPak, "0");
                } /* end else */
            }
            #if 0
                else if (strstr(",KEEP,ALIVE,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s KEEP ALIVE REQUEST RECEIVED", pclMyChild);
                    ilSendToRouter = FALSE;
                    pclError = pclNoError;
                    pclData = pclAliveAnsw;
                    HandleHrgCmd(TRUE, "");
                    ilCmdAlive = TRUE;
                    SetGrpMemberInfo(igMyGrpNbr, 3, "A", 2);
                }
                else if (strstr(",CLOSE,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s CLOSE ALIVE COMMAND RECEIVED", pclMyChild);
                    ilSendToRouter = FALSE;
                    pclError = pclNoError;
                    pclData = pclNoError;
                    ilCmdAlive = FALSE;
                    close(connfd);
                    Terminate();
                }
                else if (strstr(",GBCH,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s BROADCAST HISTORY REQUEST RECEIVED (NEW STYLE)", pclMyChild);
                    ilSendToRouter = FALSE;
                    rlRecDesc.FldSep = pclFldSep[0];
                    rlRecDesc.RecSep = pclRecSep[0];
                    ilRC = GetBcHistory(pclMyChild, pclKeyTbl, pclKeyWhe, pclKeyFld, pclKeyDat,
                            &rlRecDesc, slMaxPak, connfd);
                    if (ilRC == RC_SUCCESS)
                    {
                        pclError = pclNoError;
                        pclData = rlRecDesc.Value;
                        sprintf(pclKeyPak, "%d", rlRecDesc.LineCount);
                    }/* end if */
                    else
                    {
                        pclError = rlRecDesc.Value;
                        pclData = pclNoError;
                        strcpy(pclKeyPak, "0");
                    } /* end else */
                }
                else if (strstr(",BCOUT,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s KEEP ALIVE AS BC_OUT_CONNECTOR", pclMyChild);
                    ilSendToRouter = FALSE;
                    pclError = pclNoError;
                    ilRC = HandleBcOut(pclMyChild, ilCpid, pclKeyHex, connfd);
                    pclData = pclNoError;
                }
                else if (strstr(",BCKEY,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s BC_FILTER FOR BC_OUT_CONNECTOR", pclMyChild);
                    ilSendToRouter = FALSE;
                    ilRC = HandleBcKey(pclMyChild, pclKeyQue, pclKeyDat, connfd);
                    pclError = pclNoError;
                    pclData = pclNoError;
                }
                else if (strstr(",BCGET,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s RESEND BC TO BC_OUT_CONNECTOR", pclMyChild);
                    ilSendToRouter = FALSE;
                    ilRC = HandleBcGet(pclMyChild, pclKeyQue, pclKeyDat, connfd);
                    pclError = pclNoError;
                    pclData = pclNoError;
                }
                else if (strstr(",BCOFF,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s CLOSE BC_OUT_CONNECTOR", pclMyChild);
                    ilSendToRouter = FALSE;
                    pclError = pclNoError;
                    ilRC = HandleBcOff(pclMyChild, pclKeyQue, pclKeyHex, connfd);
                    pclData = pclNoError;
                }
                else if (strstr(",GBCN,", pclChkCmd) != NULL)
                {
                    dbg(TRACE, "%s BCNUM REQUEST RECEIVED (NEW STYLE)", pclMyChild);
                    ilSendToRouter = FALSE;
                    pclError = pclNoError;
                    pclData = pclNoError;
                }/* end else if */
                else
                {
                    ilSendToRouter = TRUE;
                } /* end else */
            #endif

            if (ilEvtCnt > 1)
            {
                if (rlKeyWhe.ItemPtr != NULL) rlKeyWhe.ItemPtr[rlKeyWhe.ItemLen] = rlKeyWhe.Help[0];
                if (rlKeyFld.ItemPtr != NULL) rlKeyFld.ItemPtr[rlKeyFld.ItemLen] = rlKeyFld.Help[0];
                if (rlKeyDat.ItemPtr != NULL) rlKeyDat.ItemPtr[rlKeyDat.ItemLen] = rlKeyDat.Help[0];
            }

        } /* end for */

        dbg(DEBUG, "%s AFTER IF ", pclMyChild);
        ilRC = RC_SUCCESS;
        ilGotError = FALSE;
        pclOutData = NULL;
        rgKeyItemListOut.UsedLen = 0;

        if (ilSendToRouter == TRUE)
        {
            ilRC = HandleQueCreate(TRUE, pcgMyShortName, 1);
            if (ilRC != RC_SUCCESS)
            {
                pclData = pclNoError;
                pclError = pclAnyErrMsg;
                ilGotError = TRUE;
            }

            if (ilRC == RC_SUCCESS)
            {
                if (pclKeyTws[0] == '\0')
                {
                    strcpy(pclKeyTws, pclKeyIdn);
                }

                if (pclKeyTwe[0] == '\0')
                {
                    //sprintf(pclKeyTwe, "%s,%s,%s", pclKeyHop, pclKeyExt, pclKeyApp);
                    sprintf(pclKeyTwe, "%s,%s,cdrhdl", cgHopo, cgTabEnd);
                }

                dbg(DEBUG, "%s NOW SENDING SendCedaEvent ...", pclMyChild);
                if (igTimeOut <= 0)
                {
                    igTimeOut = 120;
                }

                ilTist = time(0L) % 10000;
                sprintf(pclKeyTim, "{-TIM-}%d,%d", ilTist, igTimeOut);
                ilRC = SendCedaEvent(igQueToRouter, /* adress to send the answer   */
                        igAnswerQueue, /* Set to temp queue ID        */
                        pclKeyUsr, /* BC_HEAD.dest_name           */
                        pclKeyWks, /* BC_HEAD.recv_name           */
                        pclKeyTws, /* CMDBLK.tw_start             */
                        pclKeyTwe, /* CMDBLK.tw_end               */
                        pclKeyCmd, /* CMDBLK.command              */
                        pclKeyTbl, /* CMDBLK.obj_name             */
                        pclKeyWhe, /* Selection Block             */
                        pclKeyFld, /* Field Block                 */
                        pclKeyDat, /* Data Block                  */
                        pclKeyTim, /* Error description           */
                        2, /* 0 or Priority               */
                        RC_SUCCESS); /* BC_HEAD.rc: RC_SUCCESS      */

                /* warten auf Antwort vom Empfaenger */
                //SetGrpMemberInfo(igMyGrpNbr, 1, "C", 0);
                ilRC = WaitAndCheckQueue(igTimeOut, igAnswerQueue, &prgItem);

                if (ilRC != RC_SUCCESS)
                {
                    pclData = pclNoError;
                    if (igGotTimeOut == TRUE)
                    {
                        dbg(TRACE, "%s ERROR: GOT SERVER TIMEOUT <%s>", pcgMyProcName, pclKeyTim);
                        pclError = pclTimeOutMsg;
                    }
                    else
                    {
                        dbg(TRACE, "%s ERROR: QUE_GET FAILED", pcgMyProcName);
                        pclError = pclAnyErrMsg;
                    }
                    ilRC = HandleQueCreate(FALSE, pcgMyShortName, 1);
                    igGotTimeOut = FALSE;
                    ilRC = RC_SUCCESS;
                    if (igHsbDown == TRUE)
                    {
                        close(connfd);
                        Terminate();
                    }
                }
                else
                {
                    pclData = pclNoError;
                    ilRC = TransLateEvent(&rgKeyItemListOut, TRUE);
                    if (rgKeyItemListOut.UsedLen > 0)
                    {
                        if (rgKeyItemListOut.Value != NULL)
                        {
                            /* dbg(TRACE,"TRANSLATED <%s>",rgKeyItemListOut.Value); */
                            pclOutData = rgKeyItemListOut.Value;
                        }
                        else
                        {
                            dbg(TRACE, "OUT LIST IS NULL !!");
                        }
                    }/* end if */
                    else if (rgKeyItemListOut.UsedLen == 0)
                    {
                        dbg(TRACE, "EVENT WITHOUT DATA");
                    }/* end else if */
                    else
                    {
                        switch (rgKeyItemListOut.UsedLen)
                        {
                            case -1:
                                dbg(TRACE, "UNEXPECTED <BCOFF> RECEIVED");
                                break;
                            case -2:
                                dbg(TRACE, "UNEXPECTED <BCKEY> RECEIVED");
                                break;
                            default:
                                break;
                        }
                    } /* end else */
                } /* end else */
            } /* end else */
        } /* end if */

        /* TODO GFO 20130521 Add it
         * was causing some sig 11 from TrimAndFilterCr , maybe because of pclData or pclError was NULL
         */
        if (pclData == NULL)
        {
            pclData = pclNoError;
        }

        if (pclError == NULL)
        {
            pclError = pclNoError;
        }

        dbg(DEBUG, "%s Before  TrimAndFilterCr %d", pclMyChild, ilGotError);
        if ((ilRC == RC_SUCCESS) || (ilGotError == TRUE))
        {
            if (pclOutData == NULL)
            {
                dbg(DEBUG, "%s Before  TrimAndFilterCr ", pclMyChild);
                TrimAndFilterCr(pclData, pclFldSep, pclRecSep);
                TrimAndFilterCr(pclError, pclFldSep, pclRecSep);

                ilOutDatLen = strlen(pclData);
                lgTotalByte += ilOutDatLen;
                ilOutDatLen += strlen(pclError) + 4096;

                if (ilOutDatLen > rgKeyItemListOut.AllocatedSize)
                {
                    rgKeyItemListOut.Value = (char *) realloc(rgKeyItemListOut.Value, ilOutDatLen);
                    rgKeyItemListOut.AllocatedSize = ilOutDatLen;
                }

                pclOutData = rgKeyItemListOut.Value;

                if (pclOutData == NULL)
                {
                    dbg(TRACE, "CAN'T ALLOC %d BYTES!!!", ilOutDatLen);
                    close(connfd);
                    Terminate();
                } /* end if */
                /* dbg(TRACE,"%s BUILDING MY OWN ANSWER",pclMyChild); */

                ilOutDatPos = 16;
                if (slMaxPak > 0)
                {
                    StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, pclKeyPak);
                }
                else
                {
                    StrgPutStrg(pclOutData, &ilOutDatPos, "{=PACK=}", 0, -1, "0");
                }
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=ERR=}", 0, -1, pclError);
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=DAT=}", 0, -1, pclData);

                pclOutData[ilOutDatPos] = 0x00;
                ilBytesToSend = ilOutDatPos;
                dbg(TRACE, "%s TOTAL RECORDS=%d (BYTES=%d)", pclMyChild, lgTotalRecs, lgTotalByte);
                sprintf(pclDummy, "%09d", ilOutDatPos);
                ilOutDatPos = 0;
                StrgPutStrg(pclOutData, &ilOutDatPos, "{=TOT=}", 0, -1, pclDummy);

                dbg(TRACE, "%s SENDING MY OWN ANSWER, line<%d>", pclMyChild,__LINE__);

                #if 0
                    ilRC = SendDataToClient(pclMyChild, pclOutData, ilBytesToSend, connfd, TRUE);
                #endif
            }/* end if */
            else
            {
                #if 0
                    /* dbg(TRACE,"%s SENDING CEDA ANSWER",pclMyChild); */
                    ilRC = SendDataToClient(pclMyChild, pclOutData, -1, connfd, TRUE);
                #endif // 0
            } /* end else */

            #if 0
                dbg(TRACE,"%s Route<%d>",pclMyChild,igQueToNetin);
                dbg(TRACE,"%s mod_id<%d>",pclMyChild,mod_id);
                dbg(TRACE,"%s pclKeyUsr<%s>",pclMyChild,pclKeyUsr);
                dbg(TRACE,"%s pclKeyWks<%s>",pclMyChild,pclKeyWks);
                dbg(TRACE,"%s pclKeyTws<%s>",pclMyChild,pclKeyTws);
                dbg(TRACE,"%s pclKeyTwe<%s>",pclMyChild,pclKeyTwe);
                dbg(TRACE,"%s pclKeyCmd<%s>",pclMyChild,pclKeyCmd);
                dbg(TRACE,"%s pclKeyTbl<%s>",pclMyChild,pclKeyTbl);
                dbg(TRACE,"%s pclKeyWhe<%s>",pclMyChild,pclKeyWhe);
                dbg(TRACE,"%s pclKeyFld<%s>",pclMyChild,pclKeyFld);
                dbg(TRACE,"%s pclOutData<%s>",pclMyChild,pclOutData);
                dbg(TRACE,"%s pclKeyTim<%s>",pclMyChild,pclKeyTim);

                ilRC = SendCedaEvent(igQueToNetin, /* adress to send the answer   */
                            mod_id, /* Set to temp queue ID        */
                            pclKeyUsr, /* BC_HEAD.dest_name           */
                            pclKeyWks, /* BC_HEAD.recv_name           */
                            pclKeyTws, /* CMDBLK.tw_start             */
                            pclKeyTwe, /* CMDBLK.tw_end               */
                            pclKeyCmd, /* CMDBLK.command              */
                            pclKeyTbl, /* CMDBLK.obj_name             */
                            pclKeyWhe, /* Selection Block             */
                            pclKeyFld, /* Field Block                 */
                            pclOutData, /* Data Block                  */
                            pclKeyTim, /* Error description           */
                            2, /* 0 or Priority               */
                            NETOUT_NO_ACK); /* BC_HEAD.rc: RC_SUCCESS      */
            #endif // 0
            /*
            ilRC = new_tools_send_sql(igQueToNetin,prpBchd,prpCmdblk,pclKeyFld,pclOutData);
            dbg(TRACE,"%s Route<%d>",pclMyChild,igQueToNetin);
            dbg(TRACE,"%s pclKeyFld<%s>",pclMyChild,pclKeyFld);
            dbg(TRACE,"SENT DATA:\n%s",pclOutData);
            */

            dbg(DEBUG,"===============tools_send_sql_rc ipQue <%d>==================", ipQue);
            dbg(TRACE,"cmd = <%s>", pcpCommand);
            dbg(TRACE,"sel = <%s>", pcpSelection );
            dbg(TRACE,"fld = <%s>", pclKeyFld );
            dbg(TRACE,"dat = <%s>", pclOutData );

            ilRC = RC_SUCCESS;
            tools_send_sql_rc(ipQue,pcpCommand,"TAB", pcpSelection,pclKeyFld,pclOutData,ilRC);
        } /* end if */

        #if 0
            if ((igTmpAlive == TRUE) || (ilCmdAlive == TRUE))
            {
                dbg(DEBUG, "%s GRP MEMBER (%d) STATUS IS: ALIVE", pclMyChild, igMyGrpNbr);
                HandleHrgCmd(TRUE, "");
                if (ilCmdAlive != TRUE)
                {
                    SetGrpMemberInfo(igMyGrpNbr, 3, "T", 2);
                }
                ilEndAlive = FALSE;
            }
        #endif // 0

        lgEvtEndTime = time(0L);
        lgEvtDifTime = lgEvtEndTime - lgEvtBgnTime;
        if (lgEvtDifTime > 10)
        {
            dbg(TRACE, "%s CHECK TOTAL EVENT TIME (%d)", pclMyChild, lgEvtDifTime);
        }
        lgEvtBgnTime = time(0L);
    } /* end while not EndAlive */

    #if 0
        /* close connection to client */
        close(connfd); /* bleibt erst mal so ... kein shutdown */
        dbg(TRACE, "%s CONNECTION CLOSED", pclMyChild);
        ilRC = HandleQueCreate(FALSE, pcgMyShortName, 1);
    #endif

    FreeRecordDescriptor(&rlRecDesc);

    #if 0
        close(connfd);
        dbg(TRACE, "===== %s TERMINATING ========", pclMyChild);
        Terminate();
    #endif

    dbg(TRACE, "===== %s TERMINATING ========", pclMyChild);

    /* Will never come here hin */
    return RC_SUCCESS;

} /* end of child */

static int GetLDMLoad(char *pcpMyChild, char *pcpAftUrno) {
    short slSqlCursor;
    short slSqlFunc;
    int ilRc;
    int ili;
    char pclSqlBuf[1024];
    char pclSqlData[2048];
    char pclTmpBuf[1024];
    char pclLatestTelexUrno[12];
    char pclTelexUrno[12];
    char pclValue[8];
    char pclType[4];
    char pclStyp[4];
    char pclSstp[12];
    char pclSsst[4];


    for (ili = 0; ili < igNumLDMLoad; ili++) {
        sprintf(pclSqlBuf, "SELECT TRIM(VALU),TRIM(RURN),TRIM(TYPE),TRIM(STYP),TRIM(SSTP),TRIM(SSST) FROM LDMTAB WHERE FLNU = '%s' AND "
                "TYPE IN ( 'PAX','LOA' ) AND SAPC = '%s' AND APC3 = '%s' ORDER BY CDAT",
                pcpAftUrno, prgLoadRec[ili].sapc, prgLoadRec[ili].dapc);
        dbg(DEBUG, "%s HOP <%d>..... LDM QUERY <%s>", pcpMyChild, ili, pclSqlBuf);

        slSqlCursor = 0;
        slSqlFunc = START;
        ilRc = DB_SUCCESS;
        while (ilRc == DB_SUCCESS) {
            ilRc = sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData);
            if (ilRc != DB_SUCCESS)
                break;

            get_fld(pclSqlData, FIELD_1, STR, 6, pclValue);
            get_fld(pclSqlData, FIELD_2, STR, 10, pclTelexUrno);
            if (slSqlFunc == START) {
                strcpy(pclLatestTelexUrno, pclTelexUrno);
                dbg(DEBUG, "%s GETTING LDM LOAD FROM TELEX URNO <%s>", pcpMyChild, pclLatestTelexUrno);
            }
            if (strcmp(pclLatestTelexUrno, pclTelexUrno))
                break;
            slSqlFunc = NEXT;

            get_fld(pclSqlData, FIELD_3, STR, 4, pclType);
            get_fld(pclSqlData, FIELD_4, STR, 4, pclStyp);
            get_fld(pclSqlData, FIELD_5, STR, 10, pclSstp);
            get_fld(pclSqlData, FIELD_6, STR, 4, pclSsst);
            TrimSpace(pclValue);
            dbg(DEBUG, "%s TelexUrno <%s> Type <%s> Styp <%s> Sstp <%s> Ssst <%s> Value <%s>",
                    pcpMyChild, pclLatestTelexUrno, pclType, pclStyp, pclSstp, pclSsst, pclValue);
            if (strlen(pclValue) <= 0)
                continue;
            if (!strcmp(pclType, "PAX")) {
                if (pclStyp[0] == 'F')
                    strcpy(prgLoadRec[ili].paxF, pclValue);
                else if (pclStyp[0] == 'B')
                    strcpy(prgLoadRec[ili].paxB, pclValue);
                else if (pclStyp[0] == 'E')
                    strcpy(prgLoadRec[ili].paxE, pclValue);
                else if (pclStyp[0] == '2')
                    strcpy(prgLoadRec[ili].paxE2, pclValue);
                else if (pclSsst[0] == 'A')
                    strcpy(prgLoadRec[ili].paxA, pclValue);
                else if (pclSsst[0] == 'C')
                    strcpy(prgLoadRec[ili].paxC, pclValue);
                else if (pclSsst[0] == 'I')
                    strcpy(prgLoadRec[ili].paxI, pclValue);
                continue;
            }
            if (!strcmp(pclType, "LOA")) {
                if (pclStyp[0] == 'B' && pclSstp[0] == 'N')
                    strcpy(prgLoadRec[ili].numbag, pclValue);
                else if (pclStyp[0] == 'B')
                    strcpy(prgLoadRec[ili].bagwt, pclValue);
                else if (pclStyp[0] == 'C')
                    strcpy(prgLoadRec[ili].cargowt, pclValue);
                continue;
            }
        } /* while, LDM query */
        close_my_cursor(&slSqlCursor);
    } /* for loop, for all LDM Records */
    return RC_SUCCESS;
}

/* ======================================================================= */
/* ========================= END CDRCOM ================================== */
/* ======================================================================= */




