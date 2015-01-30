#ifndef _DEF_mks_version
#define _DEF_mks_version
#include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/relhdl.c 1.14c 2012/03/03 15:03:03SGT fya Exp  $";
#endif /* _DEF_mks_version */
/* source-code-control-system version string                                  */
/* be carefule with strftime or similar functions !!!                         */
static char sccs_version[] = "@(#)UFIS 4.3 (c) ABB ACE/FC relhdl.c 1.20 / " __DATE__ "  " __TIME__ "  / BST/MCU";
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
/* 20020327 MCU   : "NO" added to QUICK or LATE to prevent ReleaseToAction    */
/*                  QuickHack should be replaceda asap                        */
/* 20020327 MCU   : changed the NO in QUICK and LATE to NOACTION,NOBC,NOLOG   */
/* 20030722 JIM   : added evaluation of ACTION_INFO from sqlhdl.cfg to read   */
/*                  and send old data                                         */
/* 20081014 BLE   : allow the forwarding of SQL statements to external process*/
/* 20120711 GFO: UFISRD-75 Add ifdef CCS_DBMODE_EMBEDDED                      */
/* 20150129 FYA: v1.14c UFIS-8295 HOPO implementation                         */
/*                                                                            */
/******************************************************************************/
/*                                                                            */

/*                                                                            */
/******************************************************************************/
/* This program is a CEDA main program */

#define U_MAIN
#define UGCCS_PRG
#define STH_USE
#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>
#include "ugccsma.h"
#include "msgno.h"
#include "glbdef.h"
#include "quedef.h"
#include "uevent.h"
#include "sthdef.h"
#include "db_if.h"
#include "libccstr.h"
#include "loghdl.h"
#include "action_tools.h"
#include "ct.h"

#define FOR_INSERT 1
#define FOR_UPDATE 2
#define FOR_DELETE 3
#define MAX_CT_DESC 5

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE *outp       = NULL;*/
int debug_level = TRACE;

/***************
internal structures
 ****************/

/* Outsending Field Lists */
typedef struct {
    int Count;
    char Table[16];
    char Fields[512];
} OUT_FLD_LIST;


/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static OUT_FLD_LIST prgOutFldLst[150];
static ITEM *prgItem = NULL; /* The queue item pointer  */
static EVENT *prgEvent = NULL; /* The event pointer       */
static int igInitOK = FALSE;
static char cgConfigFile[512];

char pcgProcName[100];
char *pcgCfgPath;
char pcgCfgFile[512];
char pcgBinFile[512];

static int igQueOut = 0;
static int igAnswerSent = FALSE;
static int igGlNoAck = FALSE;

static int igAddHopo = FALSE;
static int igChkHopoItem = -5;
static int igUrnoField = 0;
static int igRcvFldCnt = 0;
static int igOutFldCnt = 0;

static int igDebugDetails = TRUE;
static int igStartupMode = TRACE;
static int igRuntimeMode = 0;

static char pcgOutRoute[64];
static int igToAction = 0;
static int igToLogHdl = 0;
static int igToBcHdl = 1900;
static int igSendToAction = TRUE;
static int igSendToLogHdl = FALSE;
static int igSendToBcHdl = TRUE;
static int igSendOldData = FALSE;
static int igSilentAction = FALSE;
static int igIsCommitted = FALSE;
static int igLineSql = FALSE;
static int igCmdKeyLen = 0;

/* Mei 13-OCT-2008 */
static int igNumTableForward = 0;

static char pcgDefH3LC[64];
static char pcgH3LC[64];
static char pcgDefTblExt[8];
static char pcgTblExt[8];
static char pcgUrnoPrefix[4];

static char pcgRecvName[64];
static char pcgDestName[64];
static char pcgTwStart[64];
static char pcgTwEnd[64];
static char pcgRelCmdKey[16];

static char *pcgLineBegin = NULL;
static char *pcgLineEnd = NULL;

static char pcgTmpBuf1[4096 * 3];
static char pcgTmpBuf2[4096 * 3];
static char pcgMissingNewFlds[4096];

static char pcgActCmdCod[8];
static char pcgActTblNam[16];
static char pcgActFldLst[4096];
static char pcgRcvFldLst[4096];
static char pcgActDatLst[4096 * 3];
static char pcgActSelKey[4096 * 3];

static char pcgSqlBuf[4096 * 3];
static char pcgSqlBuf2[4096 * 3];
static char pcgSqlDat[4096 * 3];
static char pcgDataArea[4096 * 3];
static char pcgSqlRead[4096 * 6];
static char pcgOldData[4096 * 3];
static char pcgMovDelTab[4096];

static char pcgCtNewData[] = "NewData";
static char pcgCtOldData[] = "OldData";

/* Mei 13-OCT-2008 */
char (*pcgTableForward)[128];
char pcgDeletedData[4096];

static REC_DESC rgDbRecDesc;
static REC_DESC rgDbUrnoList;
static REC_DESC rgDbOldData;

static STR_DESC argCtData[MAX_CT_DESC + 1];

static LIST_DESC *prgTestList = NULL;

static int igMultiHopo = FALSE;
static char cgCfgBuffer[1024];

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
static int Init_relhdl();
static int Reset(void); /* Reset program          */
static void Terminate(void); /* Terminate program      */
static void HandleSignal(int); /* Handles signals        */
static void HandleErr(int); /* Handles general errors */
static void HandleQueErr(int); /* Handles queuing errors */
static int HandleData(void); /* Handles event data     */
static void HandleQueues(void); /* Waiting for Sts.-switch*/

static int HandleRelease(char *, char *, char *, char *);
static int HandleSyncCommand(void);
static int HandleSqlCommand(int, int *, int *);
static int BuildSqlString(char *, int);
static int BuildDataLine(int ipNbr, char *pcpDest, char *pcpSource);
static void CheckNullValue(char *pcpData);

static void CheckHomePort(void);
static int CheckTableExtension(char *);
static void CheckWhereClause(int, char *, int, int, char *);

static int ReadCfg(char *, char *, char *, char *, char *);
static int GetOutModId(char *pcpProcName);
static int GetLogFileMode(int *ipModeLevel, char *pcpLine, char *pcpDefault);
static void ReadOldData(char *pcpUrno);
static int InitSqlIfCall(void);
static void AddOutFieldsToSelFields(char *pcpTbl, char *pcpFld);
static void AddMissingFields(char *pcpRcvFld, char *pcpOutFld);
static void AddMissingEmptyData(char *pcpNewDat);
static void AddMissingData(char *pcpNewDat, char *pcpOldData);
static void InitOutFieldList(void);

extern void BuildItemBuffer(char *, char *, int, char *);
extern int StrgPutStrg(char *, int *, char *, int, int, char *);

extern short delton(char *);

static void SetHopoDefault(char *pcpTable, char *pcpValue);
static int InsertDataBuffer(int ipCmdTyp, char *pcpBuffer);

static void MergeBuffers(int ipForWhat);
static char *GetItemPointer(char *pcpList, int ipItemNo, char *pcpSepa, int *pipLen);
static void SetUrnoPrefix(char *pcpTable, char *pcpPrefix);

static void TestLists(void);

/* Mei 13-OCT-2008 */
static int Init_table_forward(char *);

static void setHomeAirport(char *pcpProcessName, char *pcpHopo);
static void readDefaultHopo(char *pcpHopo);
static void setupHopo(char *pcpHopo);
static int checkHopo(char *pcpTwEnd, char *pcpHopo);
static char *to_upper(char *pcgIfName);
static int checkAndsetHopo(char *pcpTwEnd, char *pcpHopo_sgstab);

/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN{
    int ilRC = RC_SUCCESS;
    int ilCnt = 0;
    debug_level = igStartupMode;

    /* copy my process name - used to form the config file name */
    strcpy(pcgProcName, argv[0]);

    (void) SetSignals(HandleSignal);
    (void) UnsetSignals();

    INITIALIZE; /* General initialization   */
    dbg(TRACE, "MAIN: MKS Version\n<%s>", mks_version);
    dbg(TRACE, "MAIN: old Version\n<%s>", sccs_version);
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
        exit(1);
    } else {
        dbg(DEBUG, "MAIN: init_que() OK! mod_id <%d>", mod_id);
    }/* end of if */
    do {
        ilRC = init_db();
        if (ilRC != RC_SUCCESS) {
            check_ret(ilRC);
            dbg(TRACE, "MAIN: init_db() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        } /* end of if */
    } while ((ilCnt < 10) && (ilRC != RC_SUCCESS));
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: init_db() failed! waiting 60 sec ...");
        sleep(60);
        exit(2);
    } else {
        dbg(DEBUG, "MAIN: init_db() OK!");
    } /* end of if */
    /* logon to DB is ok, but do NOT use DB while ctrl_sta == HSB_COMING_UP !!! */
    sprintf(cgConfigFile, "%s/relhdl", getenv("BIN_PATH"));
    ilRC = TransferFile(cgConfigFile);
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: TransferFile(%s) failed!", cgConfigFile);
    } /* end of if */
    /* uncomment if necessary */
    sprintf(cgConfigFile, "%s/relhdl.cfg", getenv("CFG_PATH"));
    ilRC = TransferFile(cgConfigFile);
    strcpy(pcgCfgFile, cgConfigFile);
    /* if(ilRC != RC_SUCCESS) */
    /* { */
    /*  dbg(TRACE,"MAIN: TransferFile(%s) failed!",cgConfigFile); */
    /* } */ /* end of if */
    ilRC = SendRemoteShutdown(mod_id);
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "MAIN: SendRemoteShutdown(%d) failed!", mod_id);
    } /* end of if */
    if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY)) {
        dbg(DEBUG, "MAIN: waiting for status switch ...");
        HandleQueues();
    }/* end of if */
    GetLogFileMode(&igStartupMode, "STARTUP_MODE", "OFF");
    GetLogFileMode(&igRuntimeMode, "RUNTIME_MODE", "OFF");
    debug_level = igStartupMode;
    if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY)) {
        dbg(DEBUG, "MAIN: initializing ...");
        if (igInitOK == FALSE) {
            ilRC = Init_relhdl();
            if (ilRC != RC_SUCCESS) {
                dbg(TRACE, "Init_relhdl: init failed!");
            } /* end of if */
        }/* end of if */
    } else {
        Terminate();
    }/* end of if */
    dbg(DEBUG, "MAIN: initializing OK");

    /* dbg_handle_debug(TRACE_OFF); */

    debug_level = igRuntimeMode;

    for (;;) {
        dbg(TRACE, "============== START/END =================");
        ilRC = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, 0, (char *) &prgItem);
        prgEvent = (EVENT *) prgItem->text;

        if (ilRC == RC_SUCCESS) {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
            if (ilRC != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            } /* fi */

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
                    /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    Terminate();
                    break;
                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    ResetDBCounter();
                    break;
                case REMOTE_DB:
                    /* ctrl_sta is checked inside */
                    HandleRemoteDB(prgEvent);
                    break;
                case SHUTDOWN:
                    /* process shutdown - maybe from uutil */
                    Terminate();
                    break;

                case RESET:
                    ilRC = Reset();
                    break;

                case EVENT_DATA:
                    if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY)) {
                        ilRC = HandleData();
                        if (ilRC != RC_SUCCESS) {
                            HandleErr(ilRC);
                        }/* end of if */
                    } else {
                        dbg(TRACE, "MAIN: wrong hsb status <%d>", ctrl_sta);
                        DebugPrintItem(TRACE, prgItem);
                        DebugPrintEvent(TRACE, prgEvent);
                    }/* end of if */
                    break;

                case TRACE_ON:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case TRACE_OFF:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case 777:
                    TestLists();
                    ilRC = RC_SUCCESS;
                    break;

                default:
                    dbg(TRACE, "MAIN: unknown event");
                    DebugPrintItem(TRACE, prgItem);
                    DebugPrintEvent(TRACE, prgEvent);
                    break;
            } /* end switch */
        } else {
            /* Handle queuing errors */
            HandleQueErr(ilRC);
        } /* end else */

    } /* end for */

    exit(0);

} /* end of MAIN */

/******************************************************************************/
/* The initialization routine                                                 */

/******************************************************************************/
static int Init_relhdl() {
    int ilRC = RC_SUCCESS;
    int i = 0;

    /* ilRC = InitSqlIfCall(); */

    strcpy(pcgRelCmdKey, "*CMD*");
    igCmdKeyLen = strlen(pcgRelCmdKey);

    pcgDefTblExt[0] = 0x00;
    pcgDefH3LC[0] = 0x00;

    ilRC = tool_search_exco_data("ALL", "TABEND", pcgDefTblExt);
    strcpy(pcgTblExt, pcgDefTblExt);

    /*ilRC = tool_search_exco_data("SYS", "HOMEAP", pcgDefH3LC);
    strcpy(pcgH3LC, pcgDefH3LC);*/

    setHomeAirport(pcgProcName, pcgDefH3LC);

    dbg(TRACE, "DEFAULTS: HOME <%s> EXT <%s>", pcgDefH3LC, pcgDefTblExt);
    (void) CheckTableExtension("SYS");

    /* read config file */
    if ((pcgCfgPath = getenv("CFG_PATH")) == NULL) {
        dbg(TRACE, "init Error reading CFG_PATH");
    }

    /* compose path/filename */
    /* sprintf(pcgCfgFile, "%s/%s.cfg", pcgCfgPath, pcgProcName); */
    sprintf(pcgCfgFile, "%s/relhdl.cfg", pcgCfgPath);
    dbg(DEBUG, "init config file <%s>", pcgCfgFile);

    /* igToLogHdl = tool_get_q_id("loghdl"); */
    igToLogHdl = GetOutModId("loghdl");

    /* igToAction = tool_get_q_id("action"); */
    igToAction = GetOutModId("action");

    /* igToBcHdl = tool_get_q_id("bchdl"); */
    igToBcHdl = GetOutModId("bchdl");

    dbg(TRACE, "SPECIAL OUT QUEUES:");
    dbg(TRACE, "LOGHDL=%d ACTION=%d BCHDL=%d",
            igToLogHdl, igToAction, igToBcHdl);

    /*sprintf(pcgOutRoute,"%d,%d,%d",igToAction,igToLogHdl,igToBcHdl);      */

    InitOutFieldList();

    InitRecordDescriptor(&rgDbRecDesc, 4096, 0, TRUE);
    InitRecordDescriptor(&rgDbUrnoList, (1024 * 11), 0, TRUE);
    InitRecordDescriptor(&rgDbOldData, 4096, 0, TRUE);

    CT_CreateArray(pcgCtNewData);
    CT_SetDataPoolAllocSize(pcgCtNewData, 1000);
    CT_ResetContent(pcgCtNewData);

    CT_CreateArray(pcgCtOldData);
    CT_SetDataPoolAllocSize(pcgCtOldData, 1000);
    CT_ResetContent(pcgCtOldData);

    for (i = 0; i <= MAX_CT_DESC; i++) {
        CT_InitStringDescriptor(&argCtData[i]);
        argCtData[i].Value = malloc(4096);
        argCtData[i].AllocatedSize = 4096;
        argCtData[i].Value[0] = 0x00;
    }

    ToolsListCreate(&prgTestList);

    /** Mei **/
    /** Initialize table forward info **/
    Init_table_forward(pcgCfgFile);

    ReadCfg(pcgCfgFile, pcgMovDelTab, "MOVE_DELETION", "MOVE_TABLES", "------");
    dbg(TRACE, "MOVE DELETION TABLES <%s>", pcgMovDelTab);

    igInitOK = TRUE;
    return (ilRC);

} /* end of initialize */

/******************************************************************************/
/* The Reset routine                                                          */

/******************************************************************************/
static int Reset() {
    int ilRC = RC_SUCCESS; /* Return code */

    dbg(TRACE, "Reset: now resetting");

    return ilRC;

} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */

/******************************************************************************/
static void Terminate() {
    /* unset SIGCHLD ! DB-Child will terminate ! */
    logoff();
    dbg(TRACE, "Terminate: now leaving ...");

    exit(0);

} /* end of Terminate */

/******************************************************************************/
/* The handle signals routine                                                 */

/******************************************************************************/
static void HandleSignal(int pipSig) {
    int ilRC = RC_SUCCESS; /* Return code */
    dbg(TRACE, "HandleSignal: signal <%d> received", pipSig);
    switch (pipSig) {
        default:
            Terminate();
            break;
    } /* end of switch */
    exit(pipSig);

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
        ilRC = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, 0, (char *) &prgItem);
        prgEvent = (EVENT *) prgItem->text;
        if (ilRC == RC_SUCCESS) {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
            if (ilRC != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            } /* fi */

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
                    /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    Terminate();
                    break;

                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    ResetDBCounter();
                    ilBreakOut = TRUE;
                    break;
                case REMOTE_DB:
                    /* ctrl_sta is checked inside */
                    HandleRemoteDB(prgEvent);
                    break;
                case SHUTDOWN:
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
        ilRC = Init_relhdl();
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "Init_relhdl: init failed!");
        } /* end of if */
    }/* end of if */
    /* OpenConnection(); */
} /* end of HandleQueues */


/******************************************************************************/
/* The handle data routine                                                    */

/******************************************************************************/
static int HandleData() {
    int ilRC = RC_SUCCESS;
    char pclActCmd[16];
    char pclTable[64];
    BC_HEAD *prlBchd;
    CMDBLK *prlCmdblk;
    char *pclSelKey;
    char *pclFields;
    char *pclData;

    igQueOut = prgEvent->originator;
    igAnswerSent = FALSE;
    prlBchd = (BC_HEAD *) ((char *) prgEvent + sizeof (EVENT));
    prlCmdblk = (CMDBLK *) ((char *) prlBchd->data);
    pclSelKey = (char *) prlCmdblk->data;
    pclFields = (char *) pclSelKey + strlen(pclSelKey) + 1;
    pclData = (char *) pclFields + strlen(pclFields) + 1;

    char *pclFunc = "HandleData";

    /* Save Global Elements of BC_HEAD */
    /* strcpy(pcgRecvName,prlBchd->recv_name); */
    /* strcpy(pcgDestName,prlBchd->dest_name); */

    /* WorkStation */
    memset(pcgRecvName, '\0', (sizeof (prlBchd->recv_name) + 1));
    strncpy(pcgRecvName, prlBchd->recv_name, sizeof (prlBchd->recv_name));

    /* User */
    memset(pcgDestName, '\0', (sizeof (prlBchd->dest_name) + 1));
    strncpy(pcgDestName, prlBchd->dest_name, sizeof (prlBchd->dest_name));


    /* Save Global Elements of CMDBLK */
    strcpy(pcgTwStart, prlCmdblk->tw_start);
    strcpy(pcgTwEnd, prlCmdblk->tw_end);

    if(igMultiHopo == TRUE)
    {
      dbg(DEBUG,"%s igMultiHopo == TRUE",pclFunc);
    }

    if(checkAndsetHopo(pcgTwEnd, pcgDefH3LC) == RC_FAIL)
        return RC_FAIL;

    CheckHomePort();

    strcpy(pclActCmd, prlCmdblk->command);
    str_trm_rgt(pclActCmd, " ", TRUE);
    strcpy(pclTable, prlCmdblk->obj_name);

    dbg(TRACE, "CMD <%s> QUE (%d) WKS <%s> USR <%s>",
            pclActCmd, igQueOut, pcgRecvName, pcgDestName);
    dbg(TRACE, "SPECIAL INFO TWS <%s> TWE <%s>",
            pcgTwStart, pcgTwEnd);

    if (prlBchd->rc == NETOUT_NO_ACK) {
        igGlNoAck = TRUE;
        dbg(DEBUG, "NO ANSWER EXPECTED (NO_ACK IS TRUE)");
    }/* end if */
    else {
        igGlNoAck = FALSE;
        dbg(DEBUG, "SENDER EXPECTS ANSWER (NO_ACK IS FALSE)");
    } /* end else */

    if (strcmp(pclActCmd, "REL") == 0) {
        ilRC = HandleRelease(pclTable, pclFields, pclData, pclSelKey);
    } else if (strcmp(pclActCmd, "XXX") == 0) {
        /* Dummy Line */;
    } else {
        dbg(TRACE, "UNKNOWN COMMAND");
    } /* end if else */


    return ilRC;
} /* end of HandleData */

/******************************************************************************/

/******************************************************************************/
static int HandleRelease(char *pcpTable, char *pcpFields,
        char *pcpData, char *pcpSelKey) {
    int ili;
    int ilRC = RC_SUCCESS;
    int ilGetRc = RC_SUCCESS;
    int ilBreakOut = FALSE;
    int ilLinCnt = 0;
    int ilLen; /* MEI 20081013 */
    char pclActCmd[16];
    char pclAnswType[16];
    char clTmp[124];

    dbg(DEBUG, "============ DATA FROM CLIENT ============");
    dbg(DEBUG, "TABLE  <%s>", pcpTable);
    dbg(DEBUG, "FIELDS <%s>", pcpFields);
    dbg(DEBUG, "SELECT <%s>", pcpSelKey);
    dbg(DEBUG, "DATA BUFFER:\n<%s>", pcpData);

    /* MCU 20020327 reset pcgOutRoute then add igToAction,igToLogHdl,igToBcHdl
           depending on the Selection ************/
    if ((strstr(pcpSelKey, "NOACTION") != NULL) ||
            (strstr(pcpSelKey, "NOLOG") != NULL) || (strstr(pcpSelKey, "NOBC") != NULL)) {
        /* dbg(TRACE,"New Version: Send to ACTION or LOG or BC disabled");  */
        igSendToAction = FALSE;
        igSendToLogHdl = FALSE;
        igSendToBcHdl = FALSE;
        igSendOldData = FALSE;
        *pcgOutRoute = '\0';
        if (strstr(pcpSelKey, "NOACTION") == NULL) {
            if (igToAction > 0) {
                sprintf(pcgOutRoute, "%d,", igToAction);
                igSendToAction = TRUE;
                igSendOldData = TRUE;
            }
        }
        if (strstr(pcpSelKey, "NOLOG") == NULL) {
            if (igToLogHdl > 0) {
                sprintf(clTmp, "%d,", igToLogHdl);
                strcat(pcgOutRoute, clTmp);
                igSendToLogHdl = TRUE;
            }
        }
        if (strstr(pcpSelKey, "NOBC") == NULL) {
            if (igToBcHdl > 0) {
                sprintf(clTmp, "%d", igToBcHdl);
                strcat(pcgOutRoute, clTmp);
                igSendToBcHdl = TRUE;
            }
        }
    } else {
        /* dbg(TRACE,"Old Call: Send to ACTION and LOG and BC enabled");  */
        sprintf(pcgOutRoute, "%d,%d,%d", igToAction, igToLogHdl, igToBcHdl);
        igSendToAction = TRUE;
        igSendToLogHdl = TRUE;
        igSendToBcHdl = TRUE;
        igSendOldData = TRUE;
    }

    /***** MEI: 13-OCT-2008 *****/
    sprintf(clTmp, "%s=", pcpTable);
    for (ili = 0; ili < igNumTableForward; ili++) {
        if (strstr(pcgTableForward[ili], clTmp) != NULL) {
            ilLen = strlen(pcgOutRoute);
            if (ilLen > 0 && pcgOutRoute[ilLen - 1] != ',')
                strcat(pcgOutRoute, ",");
            strcat(pcgOutRoute, &pcgTableForward[ili][strlen(clTmp)]);
            igSendOldData = TRUE;
            break;
        }
    }
    ilLen = strlen(pcgOutRoute);
    if (ilLen > 0 && pcgOutRoute[ilLen - 1] == ',')
        pcgOutRoute[ilLen - 1] = '\0';
    /*******************************/


    if (pcgOutRoute[0] != '\0') {
        dbg(TRACE, "DISTRIBUTE OUTPUT TO <%s>", pcgOutRoute);
        igSilentAction = FALSE;
        ToolsSetSpoolerInfo(pcgDestName, pcgRecvName, pcgTwStart, pcgTwEnd);
    } else {
        dbg(TRACE, "SILENT TRANSACTION (NO OUTPUT)");
        igSilentAction = TRUE;
        ToolsSetSpoolerInfo("RelHdl", "RelHdl", "RelHdl", "RelHdl");
    }

    (void) get_real_item(pclAnswType, pcpSelKey, 1);
    /*if (strcmp(pclAnswType,"QUICK") == 0)*/
    /* MCU 20020327 */if (strncmp(pclAnswType, "QUICK", 5) == 0) {
        dbg(TRACE, "QUICK ANSWER SENT TO %d", igQueOut);
        (void) tools_send_info_flag(igQueOut, 0, pcgDestName, "", pcgRecvName,
                "", "", pcgTwStart, pcgTwEnd,
                "REL", pcpTable, "OK", "BOT,RM", "OK", 0);
        igAnswerSent = TRUE;
    } /* end if */


    /* BroadCast Begin Of Transaction */
    /*
      (void) tools_send_info_flag(igToBcHdl,0, pcgDestName, "", pcgRecvName,
                                    "", "", pcgTwStart, pcgTwEnd,
                                    "REL",pcpTable,pcpSelKey,"BOT,RM","OK",0);
     */

    /* Reset InfoData for ActionScheduler */

    pcgLineBegin = pcpData;
    ilLinCnt = 0;
    ilBreakOut = FALSE;
    while ((ilRC == RC_SUCCESS) && (ilBreakOut == FALSE)) {
        ilLinCnt++;
        pcgLineEnd = strstr(pcgLineBegin, "\n");
        if (pcgLineEnd != NULL) {
            *pcgLineEnd = 0x00;
        } /* end if */
        if (strstr(pcgLineBegin, pcgRelCmdKey) == pcgLineBegin) {
            (void) get_real_item(pclActCmd, pcgLineBegin, 3);
            strcpy(pcgActCmdCod, pclActCmd);

            if (strcmp(pclActCmd, "IRT") == 0) {
                ilRC = HandleSqlCommand(FOR_INSERT, &ilLinCnt, &ilBreakOut);
            } else if (strcmp(pclActCmd, "URT") == 0) {
                ilRC = HandleSqlCommand(FOR_UPDATE, &ilLinCnt, &ilBreakOut);
            } else if (strcmp(pclActCmd, "DRT") == 0) {
                ilRC = HandleSqlCommand(FOR_DELETE, &ilLinCnt, &ilBreakOut);
            } else if (strcmp(pclActCmd, "SYNC") == 0) {
                ilRC = HandleSyncCommand();
            } else {
                dbg(TRACE, "UNKNOWN COMMAND LINE %d: <%s>", ilLinCnt, pcgLineBegin);
                if (pcgLineEnd != NULL) {
                    pcgLineBegin = pcgLineEnd + 1;
                } /* end if */
            } /* end if else */
        }/* end if */
        else {
            dbg(TRACE, "ERROR LINE %d: <%s>", ilLinCnt, pcgLineBegin);
            if (pcgLineEnd != NULL) {
                pcgLineBegin = pcgLineEnd + 1;
            } /* end if */
        } /* end else */
        if (pcgLineEnd == NULL) {
            ilBreakOut = TRUE;
        } /* end if */
    } /* end while */

    if (ilRC == RC_SUCCESS) {
        if (igIsCommitted == FALSE) {
            commit_work();
            igIsCommitted = TRUE;
            dbg(TRACE, "FINALLY COMMITTED");
        }
        ToolsReleaseEventSpooler(TRUE, PRIORITY_3);

        /*
        (void) tools_send_info_flag(igToBcHdl,0, pcgDestName, "", pcgRecvName,
                                    "", "", pcgTwStart, pcgTwEnd,
                                    "REL",pcpTable,pcpSelKey,"EOT,RM","OK",0);
         */

    }/* end if */
    else {
        rollback();
        dbg(TRACE, "ROLLED BACK");
        ToolsReleaseEventSpooler(FALSE, 0);

        /*
        (void) tools_send_info_flag(igToBcHdl,0, pcgDestName, "", pcgRecvName,
                                    "", "", pcgTwStart, pcgTwEnd,
                                    "REL",pcpTable,pcpSelKey,"EOT,RM","ERR",0);
         */
    } /* end else */

    if ((strncmp(pclAnswType, "QUICK", 5) != 0) &&
            (igAnswerSent != TRUE)) {
        /* Late Answer */
        (void) tools_send_info_flag(igQueOut, 0, pcgDestName, "", pcgRecvName,
                "", "", pcgTwStart, pcgTwEnd,
                "REL", pcpTable, "OK", "EOT,RM", "OK", 0);
        dbg(TRACE, "LATE ANSWER SENT TO %d", igQueOut);
        igAnswerSent = TRUE;
    } /* end if */
    ilRC = RC_SUCCESS;
    return ilRC;
} /* end of HandleRelease */

/******************************************************************************/

/******************************************************************************/
static int SendCmdCode(void) {
    int ilRC = RC_SUCCESS;
    (void) tools_send_info_flag(igQueOut, 0, pcgDestName, "", pcgRecvName,
            "", "", pcgTwStart, pcgTwEnd,
            "REL", " ", " ", " ", pcgRelCmdKey, 0);
    return ilRC;
} /* end SendCmdCode */


/******************************************************************************/

/******************************************************************************/
static int HandleSyncCommand(void) {
    int ilRC = RC_SUCCESS;
    int ilGetRc = DB_SUCCESS;
    char pclTable[16];
    char pclNewUrno[64];
    char pclRcvTkey[34];
    char pclRcvStat[4];
    char pclRcvTifb[16];
    char pclRcvTife[16];
    char pclRcvText[512];
    char pclRcvSelKey[512];

    char pclDbsWkst[34];
    char pclDbsStat[4];
    char pclDbsTife[16];

    char pclOutSel[16];

    char pclSyncFields[] = "URNO,TKEY,STAT,TIFB,TIFE,WKST,USRN,TEXT";
    char pclSyncValues[] = ":URNO,:TKEY,:STAT,:TIFB,:TIFE,:WKST,:USRN,:TEXT";
    short slCursor = 0;
    short slUpdCursor = 0;

    dbg(DEBUG, "------------------------------------------");
    dbg(TRACE, "COMMAND SYNC\n<%s>", pcgLineBegin);
    dbg(DEBUG, "------------------------------------------");

    (void) get_real_item(pclTable, pcgLineBegin, 2);
    if ((strcmp(pcgDefTblExt, "TAB") == 0) ||
            (strlen(pclTable) < 6)) {
        strcpy(&pclTable[3], pcgTblExt);
    } /* end if */

    (void) get_real_item(pclRcvTkey, pcgLineBegin, 4);
    (void) get_real_item(pclRcvStat, pcgLineBegin, 5);
    (void) get_real_item(pclRcvTifb, pcgLineBegin, 6);
    (void) get_real_item(pclRcvTife, pcgLineBegin, 7);
    (void) get_real_item(pclRcvText, pcgLineBegin, 8);
    CheckNullValue(pclRcvTkey);
    CheckNullValue(pclRcvStat);
    CheckNullValue(pclRcvTifb);
    CheckNullValue(pclRcvTife);
    CheckNullValue(pclRcvText);

    dbg(DEBUG, "TABLE    <%s>", pclTable);
    dbg(DEBUG, "WKST RCV <%s>", pcgRecvName);
    dbg(DEBUG, "USRN RCV <%s>", pcgDestName);
    dbg(DEBUG, "TKEY RCV <%s>", pclRcvTkey);
    dbg(DEBUG, "STAT RCV <%s>", pclRcvStat);
    dbg(DEBUG, "TIFB RCV <%s>", pclRcvTifb);
    dbg(DEBUG, "TIFE RCV <%s>", pclRcvTife);
    dbg(DEBUG, "TEXT RCV <%s>", pclRcvText);
    dbg(DEBUG, "------------------------------------------");

    if(igMultiHopo == TRUE)
    {
        sprintf(pclRcvSelKey, "WHERE TKEY='%s' AND HOPO IN ('***','%s')", pclRcvTkey,pcgH3LC);
    }
    else
    {
        sprintf(pclRcvSelKey, "WHERE TKEY='%s'", pclRcvTkey);
    }

    /* CheckWhereClause(TRUE,pclRcvSelKey,FALSE,TRUE,"\0"); */
    sprintf(pcgSqlBuf, "SELECT %s FROM %s %s FOR UPDATE", pclSyncFields, pclTable, pclRcvSelKey);
    dbg(TRACE, "<%s>", pcgSqlBuf);

    slCursor = 0;
    ilGetRc = sql_if(START, &slCursor, pcgSqlBuf, pcgSqlDat);
    if (ilGetRc == DB_SUCCESS) {
        (void) BuildItemBuffer(pcgSqlDat, pclSyncFields, 0, ",");
        dbg(TRACE, "FOUND DATA OF TKEY\n<%s>\n<%s>", pclSyncFields, pcgSqlDat);
        /* "URNO,TKEY,STAT,TIFB,TIFE,WKST,USRN,TEXT" */
        (void) get_real_item(pclDbsWkst, pcgSqlDat, 6);
        (void) get_real_item(pclDbsStat, pcgSqlDat, 3);
        (void) get_real_item(pclDbsTife, pcgSqlDat, 5);
        CheckNullValue(pclDbsTife);
        CheckNullValue(pclDbsStat);
        if ((pclDbsStat[0] == ' ') ||
                (strcmp(pclDbsWkst, pcgRecvName) == 0) ||
                (strcmp(pclDbsTife, pclRcvTifb) < 0)) {
            dbg(TRACE, "===>> ALLOW STATUS SWITCH");
            sprintf(pcgSqlBuf, "UPDATE %s SET "
                    "WKST='%s',STAT='%s',TIFB='%s',TIFE='%s',TEXT='%s' %s",
                    pclTable,
                    pcgRecvName,
                    pclRcvStat,
                    pclRcvTifb,
                    pclRcvTife,
                    pclRcvText,
                    pclRcvSelKey);
            dbg(TRACE, "<%s>", pcgSqlBuf);

            slUpdCursor = 0;
            ilGetRc = sql_if(START, &slUpdCursor, pcgSqlBuf, pcgSqlDat);
            close_my_cursor(&slUpdCursor);
            if (ilGetRc == DB_SUCCESS) {
                strcpy(pclOutSel, "OK");
                strcpy(pcgSqlBuf, pclSyncFields);
            }/* end if */
            else {
                strcpy(pcgSqlBuf, "ORACLE ERROR");
                strcpy(pcgSqlDat, "COULD NOT UPDATE THAT RECORD");
                strcpy(pclOutSel, "NOT OK");
            } /* end else */
        }/* end if */
        else {
            dbg(TRACE, "===>> REJECT STATUS SWITCH");
            strcpy(pcgSqlBuf, pclSyncFields);
            strcpy(pclOutSel, "NOT OK");
        } /* end else */
        close_my_cursor(&slCursor);
    }/* end if */
    else {
        close_my_cursor(&slCursor);
        dbg(DEBUG, "TRANSACTION KEY NOT YET INSTALLED");
        ilGetRc = GetNextValues(pclNewUrno, 1);
        if (ilGetRc != RC_SUCCESS) {
            dbg(DEBUG, "NO NEXT URNO RECEIVED !!");
            strcpy(pclNewUrno, "0");
        } /* end else */
        dbg(DEBUG, "USED NEW URNO <%s>", pclNewUrno);
        /* "URNO,TKEY,STAT,TIFB,TIFE,WKST,USRN,TEXT" */
        if (strcmp(pcgDefTblExt, "TAB") == 0) {
#ifdef CCS_DBMODE_EMBEDDED
            sprintf(pcgSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,:VHOPO)",
                    pclTable, pclSyncFields, pclSyncValues);
#else
            sprintf(pcgSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,?)",
                    pclTable, pclSyncFields, pclSyncValues);

#endif


            sprintf(pcgSqlDat, "%s,%s,%s,%s,%s,%s,%s,%s,%s",
                    pclNewUrno, pclRcvTkey, pclRcvStat, pclRcvTifb, pclRcvTife,
                    pcgRecvName, pcgDestName, pclRcvText, pcgH3LC);
        }/* end if */
        else {
            sprintf(pcgSqlBuf, "INSERT INTO %s (%s) VALUES (%s)",
                    pclTable, pclSyncFields, pclSyncValues);
            sprintf(pcgSqlDat, "%s,%s,%s,%s,%s,%s,%s,%s",
                    pclNewUrno, pclRcvTkey, pclRcvStat, pclRcvTifb, pclRcvTife,
                    pcgRecvName, pcgDestName, pclRcvText);
        } /* end else */
        dbg(DEBUG, "<%s>", pcgSqlBuf);
        dbg(DEBUG, "<%s>", pcgSqlDat);
        delton(pcgSqlDat);
        slCursor = 0;
        ilGetRc = sql_if(START, &slCursor, pcgSqlBuf, pcgSqlDat);
        close_my_cursor(&slCursor);
        if (ilGetRc == DB_SUCCESS) {
            strcpy(pcgSqlBuf, pclSyncFields);
            BuildItemBuffer(pcgSqlDat, pclSyncFields, 0, ",");
            strcpy(pclOutSel, "OK");
        }/* end if */
        else {
            strcpy(pcgSqlBuf, "ORACLE ERROR");
            strcpy(pcgSqlDat, "COULD NOT INSERT THAT RECORD");
            strcpy(pclOutSel, "NOT OK");
        } /* end else */
    } /* end else */

    dbg(TRACE, "SYNC ANSWER SEL <%s>", pclOutSel);
    dbg(TRACE, "SYNC ANSWER FLD <%s>", pcgSqlBuf);
    dbg(TRACE, "SYNC ANSWER DAT <%s>", pcgSqlDat);

    (void) tools_send_info_flag(igQueOut, 0, pcgDestName, "", pcgRecvName,
            "", "", pcgTwStart, pcgTwEnd,
            "REL", pclTable, pclOutSel, pcgSqlBuf, pcgSqlDat, 0);
    igAnswerSent = TRUE;

    if (pcgLineEnd != NULL) {
        pcgLineBegin = pcgLineEnd + 1;
    } /* end if */

    return ilRC;
} /* end of HandleSyncCommand */

/******************************************************************************/

/******************************************************************************/
static int HandleSqlCommand(int ipCmdTyp, int *pipLinCnt, int *pipBreakOut) {
    int ilRC = RC_SUCCESS;
    int ilGetRc = DB_SUCCESS;
    int ilLoopEnd = FALSE;
    int ilCallSql = TRUE;
    int ilDatLinCnt = 0;
    int ilFldCnt = 0;
    int ilItmCnt = 0;
    short slCursor = 0;
    char pclUrnoValue[32];

    dbg(DEBUG, "------------------------------------------");
    dbg(DEBUG, "COMMAND LINE\n<%s>", pcgLineBegin);
    rgDbRecDesc.FieldCount = 0;

    ilFldCnt = BuildSqlString(pcgLineBegin, ipCmdTyp);
    dbg(DEBUG, "after Build SqlString");

    if (pcgLineEnd != NULL) {
        pcgLineBegin = pcgLineEnd + 1;
    } /* end if */

    igLineSql = FALSE;
    switch (ipCmdTyp) {
        case FOR_INSERT:
            igLineSql = FALSE;
            ilGetRc = InsertDataBuffer(ipCmdTyp, pcgLineBegin);
            dbg(DEBUG, "InsertDataBuffer");
            if (ilGetRc != RC_SUCCESS) {
                dbg(TRACE, "SWITCHING TO SINGLE LINE SQL MODE");
                igLineSql = TRUE;
            }
            break;
        case FOR_UPDATE:
            igLineSql = FALSE;
            ilGetRc = InsertDataBuffer(ipCmdTyp, pcgLineBegin);
            if (ilGetRc != RC_SUCCESS) {
                dbg(TRACE, "SWITCHING TO SINGLE LINE SQL MODE");
                igLineSql = TRUE;
            }
            break;
        case FOR_DELETE:
            igLineSql = FALSE;
            ilGetRc = InsertDataBuffer(ipCmdTyp, pcgLineBegin);
            if (ilGetRc != RC_SUCCESS) {
                dbg(TRACE, "SWITCHING TO SINGLE LINE SQL MODE");
                igLineSql = TRUE;
            }
            break;
        default:
            dbg(TRACE, "FOR WHAT? USING SINGLE LINE SQL MODE");
            igLineSql = TRUE;
            break;
    }

    ilLoopEnd = FALSE;
    ilCallSql = igLineSql;
    ilDatLinCnt = 0;
    ilItmCnt = 0;
    slCursor = 0;
    while ((ilLoopEnd == FALSE) && (*pipBreakOut == FALSE)) {
        *pipLinCnt++;

        pcgLineEnd = strstr(pcgLineBegin, "\n");
        if (pcgLineEnd != NULL) {
            *pcgLineEnd = 0x00;
            /* dbg(DEBUG,"FOUND END OF LINE"); */
        }/* end if */
        else {
            *pipBreakOut = TRUE;
            /* dbg(DEBUG,"FOUND END OF BUFFER"); */
        } /* end else */

        if ((pcgLineBegin[0] == pcgRelCmdKey[0]) &&
                (strstr(pcgLineBegin, pcgRelCmdKey) == pcgLineBegin)) {
            /* dbg(DEBUG,"FOUND NEW COMMAND LINE"); */
            if (pcgLineEnd != NULL) {
                *pcgLineEnd = '\n';
            } /* end if */
            ilLoopEnd = TRUE;
            ilCallSql = FALSE;
            if (ilDatLinCnt == 0) {
                /* dbg(TRACE,"NO DATA LINE FOUND"); */
                /* Check Conditions For DirectStatement */
                if (ipCmdTyp == FOR_DELETE) {
                    ilCallSql = TRUE;
                } /* end if */
            } /* end if */
        }/* end if */
        else {
            ilDatLinCnt++;
            /* dbg(DEBUG,"%d. DATA LINE\n<%s>",ilDatLinCnt,pcgLineBegin); */
        } /* end else */

        if (ilCallSql == TRUE) {
            pcgSqlDat[0] = 0x00;
            if (ilDatLinCnt > 0) {
                if ((igLineSql == TRUE) || (igSilentAction == FALSE)) {
                    ilItmCnt = BuildDataLine(ilDatLinCnt, pcgSqlDat, pcgLineBegin);
                }
            } /* end if */

            if (ilItmCnt >= ilFldCnt) {
                strcpy(pcgOldData, "\0");
                if (igUrnoField > 0) {
                    get_real_item(pclUrnoValue, pcgActDatLst, igUrnoField);
#ifdef CCS_DBMODE_EMBEDDED
                    sprintf(pcgActSelKey, "WHERE URNO=:VURNO ");
#else
                    sprintf(pcgActSelKey, "WHERE URNO=? ");
#endif

                    if ((igSendOldData == TRUE) && (ipCmdTyp != FOR_INSERT)) {
                        ReadOldData(pclUrnoValue);
                        AddMissingData(pcgActDatLst, pcgOldData);
                    } else {
                        AddMissingEmptyData(pcgActDatLst);
                    } /* end if */
                } /* end if */
                ilGetRc = DB_SUCCESS;
                if (igLineSql == TRUE) {
                    dbg(DEBUG, "CALLING SQL_IF");
                    igIsCommitted = FALSE;
                    ilGetRc = sql_if(IGNORE, &slCursor, pcgSqlBuf, pcgSqlDat);
                }
                if (ilGetRc == DB_SUCCESS) {
                    if ((igSilentAction == FALSE) && (igUrnoField > 0)) {
                        get_real_item(pclUrnoValue, pcgActDatLst, igUrnoField);
                        sprintf(pcgActSelKey, "WHERE URNO=%s ", pclUrnoValue);
                        (void) ToolsSpoolEventData(pcgOutRoute, pcgActTblNam, pcgActCmdCod,
                                pclUrnoValue, pcgActSelKey, pcgActFldLst,
                                pcgActDatLst, pcgOldData);
                    } /* end if */
                    ilRC = RC_SUCCESS;
                }/* end if */
                else {
                    dbg(DEBUG, "SQL LINE %d WITH WARNING OR ERROR", ilDatLinCnt);

                    /* IN CASE OF ERROR:                         */
                    /* We decided to accept all valid lines.     */
                    /* It's better than loosing the whole packet */

                    /* ------> DISABLED:
                    if (ipCmdTyp != FOR_DELETE)
                    {
                      dbg(DEBUG,"WITH ERROR");
                      ilRC = RC_FAIL;
                      ilLoopEnd = TRUE;
                     *pipBreakOut = TRUE;
                    }
                    ---------> DISABLED END */

                    ilRC = RC_SUCCESS;
                } /* end else */
            }/* end if */
            else {
                if ((igLineSql == TRUE) || (igSilentAction == FALSE)) {
                    dbg(DEBUG, "INCOMPLETE %d. LINE IGNORED <%s>", ilDatLinCnt, pcgLineBegin);
                    /* dbg(DEBUG,"(FIELDS: %d  DATA ITEMS: %d)",ilFldCnt,ilItmCnt); */
                }
            } /* end else */

        } /* end if */

        if ((ilLoopEnd == FALSE) && (pcgLineEnd != NULL)) {
            pcgLineBegin = pcgLineEnd + 1;
        } /* end if */

    } /* end while */

    if (igLineSql == TRUE) {
        dbg(DEBUG, "CLOSE CURSOR");
        close_my_cursor(&slCursor);
    }

    return ilRC;
} /* end of HandleSqlCommand */

/********************************************************/

/********************************************************/
static int BuildSqlString(char *pcpCmdStrg, int ipTyp) {
    int ilRC = RC_SUCCESS;
    int ilFldCnt = 0;
    int ilFld = 0;
    int ilFldPos = 0;
    int ilStrLen1 = 0;
    int ilStrLen2 = 0;
    char pclTblNam[16];
    char pclFldNam[16];
    char pclFldNbr[16];
    char pclFldVal[16];
    char *pclWhereBegin = NULL;
    char *pclWhereEnd = NULL;

    pcgTmpBuf1[0] = 0x00;
    pcgTmpBuf2[0] = 0x00;
    if (ipTyp != FOR_DELETE)
        pcgActFldLst[0] = 0x00;
    pcgActSelKey[0] = 0x00;
    igUrnoField = 0;

    igAddHopo = FALSE;

    (void) get_real_item(pclTblNam, pcpCmdStrg, 2);
    if ((strcmp(pcgDefTblExt, "TAB") == 0) ||
            (strlen(pclTblNam) < 6)) {
        strcpy(&pclTblNam[3], pcgTblExt);
    } /* end if */
    strcpy(pcgActTblNam, pclTblNam);

    (void) get_real_item(pclFldNbr, pcpCmdStrg, 4);
    ilFldCnt = atoi(pclFldNbr);
    ilRC = ilFldCnt;
    ilFldCnt += 4;

    igChkHopoItem = -5;

    switch (ipTyp) {
        case FOR_UPDATE:
            ilStrLen1 = 0;
            for (ilFld = 5; ilFld <= ilFldCnt; ilFld++) {
                (void) get_real_item(pclFldNam, pcpCmdStrg, ilFld);
                if (strcmp(pclFldNam, "HOPO") == 0) {
                    /* HOPO ItemPosition in Data */
                    /* First Item = 1 */
                    dbg(TRACE, "RCVD HOPO IN LIST !!");
                    igChkHopoItem = ilFld - 4;
                } /* end if */
                if (strcmp(pclFldNam, "URNO") == 0) {
                    igUrnoField = ilFld - 4;
                } /* end if */
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(pclFldVal, "%s=:V%s", pclFldNam, pclFldNam);
#else
                sprintf(pclFldVal, "%s=?", pclFldNam);
#endif

                StrgPutStrg(pcgTmpBuf1, &ilStrLen1, pclFldVal, 0, -1, ",");
                StrgPutStrg(pcgActFldLst, &ilFldPos, pclFldNam, 0, -1, ",");
            } /* end for */
            if (ilStrLen1 > 0) {
                ilStrLen1--;
                pcgTmpBuf1[ilStrLen1] = 0x00;
                ilFldPos--;
                pcgActFldLst[ilFldPos] = 0x00;
                pclWhereBegin = strstr(pcpCmdStrg, "[");
                if (pclWhereBegin != NULL) {
                    pclWhereBegin++;
                    pclWhereEnd = strstr(pclWhereBegin, "]");
                    if (pclWhereEnd != NULL) {
                        *pclWhereEnd = 0x00;
                    } /* end if */

#ifdef CCS_DBMODE_EMBEDDED

#else
                    SearchStringAndReplace(pclWhereBegin, ":VURNO", "?");
#endif

                    if (igMultiHopo == TRUE)
                    {
                        sprintf(pcgTmpBuf2, "WHERE %s AND HOPO IN ('***','%s')", pclWhereBegin, pcgH3LC);
                    }
                    else
                    {
                        sprintf(pcgTmpBuf2, "WHERE %s", pclWhereBegin);
                    }

                } /* end if */
                /* CheckWhereClause(TRUE,pcgTmpBuf2,FALSE,TRUE,"\0"); */
                sprintf(pcgSqlBuf, "UPDATE %s SET %s %s",
                        pclTblNam, pcgTmpBuf1, pcgTmpBuf2);
                strcpy(pcgActSelKey, pcgTmpBuf2);
            } /* end if */
            break;

        case FOR_INSERT:
            ilStrLen1 = 0;
            ilStrLen2 = 0;
            for (ilFld = 5; ilFld <= ilFldCnt; ilFld++) {
                (void) get_real_item(pclFldNam, pcpCmdStrg, ilFld);
                /* QuickHack */
                if (strcmp(pclFldNam, "HOPO") == 0) {
                    /* HOPO ItemPosition in Data */
                    /* First Item = 1 */
                    dbg(TRACE, "RCVD HOPO IN LIST !!");
                    igChkHopoItem = ilFld - 4;
                } /* end if */
                if (strcmp(pclFldNam, "URNO") == 0) {
                    igUrnoField = ilFld - 4;
                } /* end if */
                StrgPutStrg(pcgTmpBuf1, &ilStrLen1, pclFldNam, 0, -1, ",");
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(pclFldVal, ":V%s", pclFldNam);
#else
                sprintf(pclFldVal, "?");
#endif

                StrgPutStrg(pcgTmpBuf2, &ilStrLen2, pclFldVal, 0, -1, ",");
            } /* end for */
            if (ilStrLen1 > 0) {
                ilStrLen1--;
                pcgTmpBuf1[ilStrLen1] = 0x00;
                strcpy(pcgActFldLst, pcgTmpBuf1);
                ilStrLen2--;
                pcgTmpBuf2[ilStrLen2] = 0x00;

                sprintf(pcgSqlBuf2, "INSERT INTO %s (%s) VALUES (%s)",
                        pclTblNam, pcgTmpBuf1, pcgTmpBuf2);

                if ((strcmp(pcgDefTblExt, "TAB11111111111") == 0) &&
                        (strstr(pcgTmpBuf1, "HOPO") == NULL)) {
#ifdef CCS_DBMODE_EMBEDDED
                    sprintf(pcgSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,:VHOPO)",
                            pclTblNam, pcgTmpBuf1, pcgTmpBuf2);
#else
                    sprintf(pcgSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,?)",
                            pclTblNam, pcgTmpBuf1, pcgTmpBuf2);

#endif

                    igAddHopo = TRUE;
                    strcat(pcgActFldLst, ",HOPO");
                }/* end if */
                else {
                    sprintf(pcgSqlBuf, "INSERT INTO %s (%s) VALUES (%s)",
                            pclTblNam, pcgTmpBuf1, pcgTmpBuf2);
                } /* end else */
            } /* end if */
            break;

        case FOR_DELETE:
            pclWhereBegin = strstr(pcpCmdStrg, "[");
            if (pclWhereBegin != NULL) {
                pclWhereBegin++;
                pclWhereEnd = strstr(pclWhereBegin, "]");
                if (pclWhereEnd != NULL) {
                    *pclWhereEnd = 0x00;
                } /* end if */

                if (igMultiHopo == TRUE)
                {
                    sprintf(pcgTmpBuf2, "WHERE %s AND HOPO IN ('***','%s')", pclWhereBegin, pcgH3LC);
                }
                else
                {
                    sprintf(pcgTmpBuf2, "WHERE %s", pclWhereBegin);
                }
            } /* end if */
            /* CheckWhereClause(TRUE,pcgTmpBuf2,FALSE,TRUE,"\0"); */
            sprintf(pcgSqlBuf, "DELETE FROM %s %s", pclTblNam, pcgTmpBuf2);
            strcpy(pcgActSelKey, pcgTmpBuf2);
            strcpy(pcgActFldLst, "URNO");
            igUrnoField = 1;
            break;

        default:
            break;
    } /* end switch */
    if (igUrnoField == 0) {
        if ((strstr(pcgActSelKey, ":VURNO") != NULL) ||  (strstr(pcgActSelKey, "URNO=?") != NULL) )     /* tvo_fix for UFIS-5897 */
        {
            igUrnoField = ilRC + 1;
        } /* end if */
    } /* end if */
    dbg(DEBUG, "SQL STRING\n<%s>", pcgSqlBuf);

    strcpy(pcgRcvFldLst, pcgActFldLst);
    if (igSilentAction == FALSE) {
        dbg(DEBUG, "COMAND <%s>", pcgActCmdCod);
        dbg(DEBUG, "TBLNAM <%s>", pcgActTblNam);
        dbg(DEBUG, "FLDLST <%s>", pcgRcvFldLst);

        AddOutFieldsToSelFields(pcgActTblNam, pcgActFldLst);

        dbg(DEBUG, "OUTLST <%s>", pcgActFldLst);
        dbg(DEBUG, "SQLKEY <%s>", pcgActSelKey);
    }

    return ilRC;
} /* end BuildSqlString */

/******************************************************************************/

/******************************************************************************/
static int BuildDataLine(int ipNbr, char *pcpDest, char *pcpSource) {
    int ilItmCnt = 1;
    int ilDstPos = 0;
    int ilSrcPos = 0;
    int ilDelFnd = TRUE;
    if (pcpSource[ilSrcPos] == '\0') {
        ilItmCnt = -9999; /* To avoid Client Shit */
    } /* end if */
    while (pcpSource[ilSrcPos] != '\0') {
        if (pcpSource[ilSrcPos] == ',') {
            ilItmCnt++;
            if (ilDelFnd == TRUE) {
                pcpDest[ilDstPos] = ' ';
                ilDstPos++;
            } /* end if */
            pcpDest[ilDstPos] = ',';
            ilDelFnd = TRUE;
        }/* end if */
        else {
            pcpDest[ilDstPos] = pcpSource[ilSrcPos];
            ilDelFnd = FALSE;
        } /* end else */
        ilSrcPos++;
        ilDstPos++;
    } /* end while */
    if (ilDelFnd == TRUE) {
        pcpDest[ilDstPos] = ' ';
        ilDstPos++;
    } /* end if */
    pcpDest[ilDstPos] = ',';
    if (igAddHopo == TRUE) {
        ilDstPos++;
        StrgPutStrg(pcpDest, &ilDstPos, pcgH3LC, 0, -1, "\0");
    } /* end if */
    pcpDest[ilDstPos] = '\0';
    dbg(DEBUG, "%d. <%s>", ipNbr, pcpDest);
    strcpy(pcgActDatLst, pcpDest);
    if (igLineSql == TRUE) {
        delton(pcpDest);
    }
    return ilItmCnt;
} /* end BuildDataLine */

/******************************************************************************/

/******************************************************************************/
static int OLD_BuildDataLine(char *pcpDest, char *pcpSource) {
    int ilItmCnt = 1;
    int ilDstPos = 0;
    int ilSrcPos = 0;
    int ilDelFnd = TRUE;
    dbg(DEBUG, "PREPARING DATA LINE");
    if (pcpSource[ilSrcPos] == '\0') {
        ilItmCnt = -9999; /* To avoid Client Shit */
    } /* end if */
    while (pcpSource[ilSrcPos] != '\0') {
        if (pcpSource[ilSrcPos] == ',') {
            ilItmCnt++;
            if (ilDelFnd == TRUE) {
                pcpDest[ilDstPos] = ' ';
                ilDstPos++;
            } /* end if */
            pcpDest[ilDstPos] = '\0';
            ilDelFnd = TRUE;
        }/* end if */
        else {
            pcpDest[ilDstPos] = pcpSource[ilSrcPos];
            ilDelFnd = FALSE;
        } /* end else */
        ilSrcPos++;
        ilDstPos++;
    } /* end while */
    if (ilDelFnd == TRUE) {
        pcpDest[ilDstPos] = ' ';
        ilDstPos++;
    } /* end if */
    pcpDest[ilDstPos] = '\0';
    if (igAddHopo == TRUE) {
        ilDstPos++;
        StrgPutStrg(pcpDest, &ilDstPos, pcgH3LC, 0, -1, "\0");
        pcpDest[ilDstPos] = '\0';
    } /* end if */
    dbg(DEBUG, "NEW DATA LINE\n<%s>", pcpDest);
    return ilItmCnt;
} /* end BuildDataLine */


/******************************************************************************/

/******************************************************************************/
static void CheckNullValue(char *pcpData) {
    if (pcpData[0] == '\0') {
        pcpData[0] = ' ';
        pcpData[1] = '\0';
    } /* end if */
    return;
} /* end CheckNullValue */

/********************************************************/

/********************************************************/
static void CheckHomePort(void) {
    int ilGetRc = RC_SUCCESS;
    int ilHomLen = 0;
    int ilExtLen = 0;
    char pclHome[8];
    char pclExts[8];

    if (strcmp(pcgDefTblExt, "TAB") == 0) {
        /* First Check Table Extension */
        /* Because We Need It Later */
        ilExtLen = get_real_item(pclExts, pcgTwEnd, 2);
        if (ilExtLen != 3) {
            strcpy(pclExts, pcgDefTblExt);
        } /* end if */
        /* QuickHack */
        strcpy(pclExts, pcgDefTblExt);
        if (strcmp(pclExts, pcgTblExt) != 0) {
            strcpy(pcgTblExt, pclExts);
            dbg(TRACE, "EXTENSIONS SET TO <%s>", pcgTblExt);
        } /* end if */

        #if 0
            /* Now Check HomePort */
            ilHomLen = get_real_item(pclHome, pcgTwEnd, 1);
            if (ilHomLen != 3) {
                strcpy(pclHome, pcgDefH3LC);
            } /* end if */
            if (strcmp(pclHome, pcgH3LC) != 0) {
                strcpy(pcgH3LC, pclHome);
                dbg(TRACE, "HOMEPORT SET TO <%s>", pcgH3LC);
            } /* end if */
        #endif
    } /* end if */

    return;
} /* end CheckHomePort */



/*******************************************************/

/*******************************************************/
static int CheckTableExtension(char *pcpTblNam) {
    int ilRC = RC_SUCCESS;
    int ilGetRc = DB_SUCCESS;
    short slFkt = 0;
    short slCursor = 0;

    /* Disable this Function */
    return ilRC;


    sprintf(pcgSqlBuf, "SELECT URNO FROM %s%s WHERE URNO=0", pcpTblNam, pcgTblExt);
    slFkt = START;
    slCursor = 0;
    ilGetRc = sql_if(slFkt, &slCursor, pcgSqlBuf, pcgDataArea);
    if (ilGetRc == DB_SUCCESS) {
        ilRC = RC_SUCCESS;
    }/* end if */
    else {
        if (ilGetRc != NOTFOUND) {
            dbg(TRACE, "=====================================================");
            dbg(TRACE, "ERROR: WRONG SGS CONFIGURATION !!");
            dbg(TRACE, "TABLE EXT CORRECTED TO: <%s>", pcgH3LC);
            strcpy(pcgDefTblExt, pcgH3LC);
            strcpy(pcgTblExt, pcgH3LC);
            dbg(TRACE, "=====================================================");
            ilRC = RC_FAIL;
        } /* end if */
    } /* end else */
    close_my_cursor(&slCursor);
    return ilRC;
} /* end CheckTableExtension */



/********************************************************/

/********************************************************/
static void CheckWhereClause(int ipUseOrder, char *pcpSelKey,
        int ipChkStat, int ipChkHopo, char *pcpOrderKey) {
    char pclTmpBuf[32];
    char pclAddKey[128];
    char pclRcvSqlBuf[12 * 1024];
    char *pclWhereBgn = NULL;
    char *pclWhereEnd = NULL;
    char *pclOrderBgn = NULL;
    char *pclChrPtr = NULL;
    int ilKeyPos = 0;
    int blFldDat = FALSE;
    pclAddKey[0] = 0x00;

    /* First Copy Selection Text To WorkBuffer */
    strcpy(pclRcvSqlBuf, pcpSelKey);
    /* Set All Chars Of WhereClause To Capitals */
    str_chg_upc(pclRcvSqlBuf);

    pclWhereBgn = strstr(pclRcvSqlBuf, "WHERE");
    if (pclWhereBgn == NULL) {
        pclWhereBgn = pclRcvSqlBuf;
    }/* end if */
    else {
        /* Skip Over KeyWord */
        pclWhereBgn += 5;
    } /* end else */
    /* Search First Non Blank Character of WhereClause */
    while ((*pclWhereBgn == ' ') && (*pclWhereBgn != '\0')) {
        pclWhereBgn++;
    } /* end while */
    pclWhereEnd = pclWhereBgn + strlen(pclWhereBgn);

    /* Set All Data To Blank, So Fields And 'Order By' Are Remaining */
    pclChrPtr = pclWhereBgn;
    blFldDat = FALSE;
    while (*pclChrPtr != '\0') {
        if (*pclChrPtr == '\'') {
            if (blFldDat == FALSE) {
                blFldDat = TRUE;
            }/* end if */
            else {
                blFldDat = FALSE;
            } /* end else */
        }/* end if */
        else {
            if (blFldDat == TRUE) {
                *pclChrPtr = ' ';
            } /* end if */
        } /* end else */
        pclChrPtr++;
    } /* end while */

    /* Search 'Order By' Statement */
    pclOrderBgn = strstr(pclWhereBgn, "ORDER BY");
    if (pclOrderBgn != NULL) {
        pclWhereEnd = pclOrderBgn - 1;
    } /* end if */
    /* Search Last Non Blank Character of WhereClause */
    pclWhereEnd--;
    while ((pclWhereEnd >= pclWhereBgn) && (*pclWhereEnd == ' ')) {
        pclWhereEnd--;
    } /* end while */
    pclWhereEnd++;
    *pclWhereEnd = '\0';


    if (ipChkStat == TRUE) {
        if (strstr(pclWhereBgn, "STAT") == NULL) {
            strcpy(pclAddKey, "STAT<>'DEL'");
        } /* end if */
    } /* end if */
    ilKeyPos = strlen(pclAddKey);

    if ((ipChkHopo == TRUE) && (strcmp(pcgDefTblExt, "TAB") == 0)) {
        if (strstr(pclWhereBgn, "HOPO") == NULL) {
            if (ilKeyPos > 0) {
                StrgPutStrg(pclAddKey, &ilKeyPos, " AND ", 0, -1, "\0");
            }
            sprintf(pclTmpBuf, "HOPO='%s'", pcgH3LC);
            StrgPutStrg(pclAddKey, &ilKeyPos, pclTmpBuf, 0, -1, "\0");
        } /* end if */
    } /* end if */
    pclAddKey[ilKeyPos] = 0x00;

    if ((ilKeyPos > 0) || (ipUseOrder == FALSE)) {
        /* Restore Original WhereClause */
        strcpy(pclRcvSqlBuf, pcpSelKey);
        *pclWhereEnd = '\0';
        dbg(DEBUG, "MODIFY WHERECLAUSE:");
        dbg(DEBUG, "KEY: <%s>", pclWhereBgn);
        dbg(DEBUG, "ADD: <%s>", pclAddKey);
        if (ilKeyPos > 0) {
            if (pclWhereEnd > pclWhereBgn) {
                sprintf(pcpSelKey, "WHERE (%s)", pclWhereBgn);
                ilKeyPos = strlen(pcpSelKey);
                StrgPutStrg(pcpSelKey, &ilKeyPos, " AND ", 0, -1, "\0");
            }/* end if */
            else {
                sprintf(pcpSelKey, "WHERE ", pclWhereBgn);
                ilKeyPos = strlen(pcpSelKey);
            } /* end else */
            StrgPutStrg(pcpSelKey, &ilKeyPos, pclAddKey, 0, -1, "\0");
            pcpSelKey[ilKeyPos] = 0x00;
        }/* end if */
        else {
            if (pclWhereEnd > pclWhereBgn) {
                sprintf(pcpSelKey, "WHERE %s", pclWhereBgn);
            } /* end if */
        } /* end else */
        if (ipUseOrder == FALSE) {
            pcpOrderKey[0] = 0x00;
        } /* end if */
        if (pclOrderBgn != NULL) {
            if (ipUseOrder == TRUE) {
                ilKeyPos = strlen(pcpSelKey);
                StrgPutStrg(pcpSelKey, &ilKeyPos, " ", 0, -1, pclOrderBgn);
                pcpSelKey[ilKeyPos] = 0x00;
            }/* end if */
            else {
                strcpy(pcpOrderKey, pclOrderBgn);
            } /* end else */
        } /* end if */
        dbg(DEBUG, "KEY: <%s>", pcpSelKey);
        if (ipUseOrder == FALSE) {
            dbg(DEBUG, "ORD: <%s>", pcpOrderKey);
        } /* end if */
    } /* end if */

    return;
} /* end CheckWhereClause */

/**************************************************************/

/**************************************************************/
static int GetOutModId(char *pcpProcName) {
    int ilQueId = -1;
    char pclCfgCode[32];
    char pclCfgValu[32];
    sprintf(pclCfgCode, "QUE_TO_%s", pcpProcName);
    ReadCfg(pcgCfgFile, pclCfgValu, "QUE_DEFINES", pclCfgCode, "-1");
    ilQueId = atoi(pclCfgValu);
    if (ilQueId < 0) {
        ilQueId = tool_get_q_id(pcpProcName);
    } /* end if */
    if (ilQueId < 0) {
        ilQueId = 0;
    } /* end if */
    return ilQueId;
} /* end of GetOutModId */

static int
ReadCfg(char *pcpFile, char *pcpVar, char *pcpSection,
        char *pcpEntry, char *pcpDefVar) {

    int ilRc = RC_SUCCESS;
    pcpVar[0] = 0x00;
    ilRc = iGetConfigEntry(pcpFile, pcpSection, pcpEntry, 1, pcpVar);
    if (ilRc != RC_SUCCESS || strlen(pcpVar) <= 0) {
        strcpy(pcpVar, pcpDefVar); /* copy the default value */
        ilRc = RC_SUCCESS;
    }

    return ilRc;

} /* end ReadCfg() */

/********************************************************/
/* NO LONGER USED !!                                    */

/********************************************************/
static int ReleaseActionInfo(char *pcpRoute, char *pcpTbl, char *pcpCmd,
        char *pcpUrnoList, char *pcpSel, char *pcpFld, char *pcpDat, char *pcpOldDat) {
    int ilRC = RC_SUCCESS;
    int ilSelLen = 0;
    int ilSelPos = 0;
    int ilDatLen = 0;
    int ilDatPos = 0;
    int ilRouteItm = 0;
    int ilItmCnt = 0;
    int ilActRoute = 0;
    char pclRouteNbr[8];
    char *pclOutSel = NULL;
    char *pclOutDat = NULL;

    if (pcpRoute[0] != '\0') {
        if (igDebugDetails == TRUE) {
            dbg(TRACE, "ACTION TBL <%s>", pcpTbl);
            dbg(TRACE, "ACTION CMD <%s>", pcpCmd);
            dbg(TRACE, "ACTION SEL <%s>", pcpSel);
            dbg(TRACE, "ACTION URN <%s>", pcpUrnoList);
            dbg(TRACE, "ACTION USR <%s>", pcgDestName);
        } /* end if */
        dbg(TRACE, "ACTION FLD/DAT/OLD\n<%s>\n<%s>\n<%s>", pcpFld, pcpDat, pcpOldDat);
        ilSelLen = strlen(pcpUrnoList) + strlen(pcpSel) + 32;
        pclOutSel = (char *) malloc(ilSelLen);
        ilDatLen = strlen(pcpDat) + strlen(pcpOldDat) + 32;
        pclOutDat = (char *) malloc(ilDatLen);
        if ((pclOutSel != NULL) && (pclOutDat != NULL)) {
            StrgPutStrg(pclOutSel, &ilSelPos, pcpSel, 0, -1, " \n");
            StrgPutStrg(pclOutSel, &ilSelPos, pcpUrnoList, 0, -1, "\n");
            if (ilSelPos > 0) {
                ilSelPos--;
            } /* end if */
            pclOutSel[ilSelPos] = 0x00;
            StrgPutStrg(pclOutDat, &ilDatPos, pcpDat, 0, -1, " \n");
            StrgPutStrg(pclOutDat, &ilDatPos, pcpOldDat, 0, -1, "\n");
            if (ilDatPos > 0) {
                ilDatPos--;
            } /* end if */
            dbg(TRACE, "Sending Data to <%s>", pcpRoute);
            pclOutDat[ilDatPos] = 0x00;
            ilItmCnt = field_count(pcpRoute);
            for (ilRouteItm = 1; ilRouteItm <= ilItmCnt; ilRouteItm++) {
                (void) get_real_item(pclRouteNbr, pcpRoute, ilRouteItm);
                ilActRoute = atoi(pclRouteNbr);
                if (ilActRoute > 0) {
                    (void) tools_send_info_flag(ilActRoute, 0, pcgDestName, "", pcgRecvName,
                            "", "", pcgTwStart, pcgTwEnd,
                            pcpCmd, pcpTbl, pclOutSel, pcpFld, pclOutDat, 0);
                } /* end if */
            } /* end for */
            free(pclOutSel);
            free(pclOutDat);
        }/* end if */
        else {
            debug_level = TRACE;
            dbg(TRACE, "ERROR: ALLOC %d/%d BYTES FOR ACTION INFO", ilSelLen, ilDatLen);
            exit(0);
        } /* end else */
        pclOutSel = NULL;
        pclOutDat = NULL;
    }
    return ilRC;
} /* end ReleaseActionInfo() */






/*******************************************************/

/*******************************************************/
static int GetLogFileMode(int *ipModeLevel, char *pcpLine, char *pcpDefault) {
    char pclTmp[32];
    ReadCfg(pcgCfgFile, pclTmp, "SYSTEM", pcpLine, pcpDefault);
    if (strcmp(pclTmp, "TRACE") == 0) {
        *ipModeLevel = TRACE;
    } else if (strcmp(pclTmp, "DEBUG") == 0) {
        *ipModeLevel = DEBUG;
    }/* end else if */
    else {
        *ipModeLevel = 0;
    } /* end else */
    return RC_SUCCESS;
} /* end GetLogFileMode */

/*******************************************************/

/*******************************************************/
static void ReadOldData(char *pcpUrno) {
    int ilGetRc = DB_SUCCESS;
    short slCursor = 0;
    sprintf(pcgSqlRead, "SELECT %s FROM %s %s",
            pcgActFldLst, pcgActTblNam, pcgActSelKey);
    dbg(TRACE, "<%s> URNO:<%s>", pcgSqlRead, pcpUrno);
    strcpy(pcgOldData, pcpUrno);
    ilGetRc = sql_if(START, &slCursor, pcgSqlRead, pcgOldData);
    dbg(TRACE, "ReadOldData, RC from sql_if %d", ilGetRc);
    if (ilGetRc == DB_SUCCESS) {
        (void) BuildItemBuffer(pcgOldData, pcgActFldLst, 0, ",");
    }/* end if */
    else {
        strcpy(pcgOldData, "\0");
    } /* end else */
    close_my_cursor(&slCursor);

    return;
} /* end ReadOldData */



/* *******************************************************/

/* *******************************************************/
static int InitSqlIfCall(void) {
    int ilRc = RC_SUCCESS;
    char pclSqlBuf[30000];
    char pclFldLst[8192];
    char pclValLst[8192];
    char pclDatLst[8192];
    int i = 0;
    int ilFldPos = 0;
    int ilValPos = 0;
    int ilDatPos = 0;
    char pclTmp[32];
    short slCursor = 0;
    short slFkt = 0;

    ilFldPos = 0;
    ilValPos = 0;
    ilDatPos = 0;
    for (i = 1; i <= 256; i++) {
        sprintf(pclTmp, "F%03d", i);
        StrgPutStrg(pclFldLst, &ilFldPos, pclTmp, 0, -1, ",");
        sprintf(pclTmp, ":VF%03d", i);
        StrgPutStrg(pclValLst, &ilValPos, pclTmp, 0, -1, ",");
        sprintf(pclTmp, "%025d", i);
        dbg(DEBUG, "%s", pclTmp);
        StrgPutStrg(pclDatLst, &ilDatPos, pclTmp, 0, -1, ",");
    }
    ilFldPos--;
    ilValPos--;
    ilDatPos--;
    pclFldLst[ilFldPos] = 0x00;
    pclValLst[ilValPos] = 0x00;
    pclDatLst[ilDatPos] = 0x00;
    dbg(TRACE, "FIELDS:\n%s>", pclFldLst);

    sprintf(pclSqlBuf, "INSERT INTO SQSTAB (%s) VALUES (%s)", pclFldLst, pclValLst);

    delton(pclDatLst);
    slFkt = COMMIT | REL_CURSOR;
    slCursor = 0;
    ilRc = sql_if(slFkt, &slCursor, pclSqlBuf, pclDatLst);
    if (ilRc == DB_SUCCESS) {
        dbg(TRACE, "SQL IF INITIALIZED");
    } else {
        dbg(TRACE, "SQL IF INIT FAILED");
    }

    return ilRc;
} /* InitSqlIfCall */

/* *******************************************************/

/* *******************************************************/
static void InitOutFieldList(void) {
    int ilCnt = 0;
    int ilItmCnt = 0;
    int ilItm = 0;
    int ilLen = 0;
    char pclTmpName[64];
    char pclKeyName[64];
    char pclTmpLine[512];
    char pclKeyLine[512];
    char pclFileName[128];

    pclFileName[0] = 0x00;
    sprintf(pclFileName, "%s/sqlhdl.cfg", getenv("CFG_PATH"));

    /* at first check own config file: */
    ReadCfg(pcgCfgFile, pclTmpLine, "SYSTEM", "USE_SQL_ACTION_INFO", "YES");
    if (strcmp(pclTmpLine, "NO") == 0) {
        dbg(TRACE, "USE_SQL_ACTION_INFO in file <%s> set to <NO> !)", pcgCfgFile);
        dbg(TRACE, "Don't read ACTION_INFO configuration from file <%s> (!!!)", pclFileName);
        prgOutFldLst[0].Count = 0; /* old behaviour, no old data for DRT */
    } else { /* default: read sqlhdl config for ACTION_INFO
        this allows to send old data for DRT
      */
        dbg(TRACE, "[SYSTEM] USE_SQL_ACTION_INFO not set to <NO>: ", pcgCfgFile);
        dbg(TRACE, "Reading ACTION_INFO configuration from file %s (!!!)", pclFileName);

        ilCnt = 0;
        ReadCfg(pclFileName, pclTmpLine, "ACTION_INFO", "TABLE_CFG", "\0");
        ilItmCnt = field_count(pclTmpLine);
        for (ilItm = 1; ilItm <= ilItmCnt; ilItm++) {
            ilLen = get_real_item(pclTmpName, pclTmpLine, ilItm);
            if (ilLen > 0) {
                ilCnt++;
                sprintf(prgOutFldLst[ilCnt].Table, "%s%s", pclTmpName, pcgTblExt);
                sprintf(pclKeyName, "%s_OUT", pclTmpName);
                ReadCfg(pclFileName, prgOutFldLst[ilCnt].Fields, "ACTION_INFO",
                        pclKeyName, "\0");
                dbg(TRACE, "<%s> OUT <%s>", prgOutFldLst[ilCnt].Table,
                        prgOutFldLst[ilCnt].Fields);
            } /* end if */
        } /* end for */
        prgOutFldLst[0].Count = ilCnt;
        dbg(TRACE, "OUT TABLE CFG'S = %d", ilCnt);
    }

    return;
} /* end InitOutFieldList */


/*******************************************************/

/*******************************************************/
static void AddMissingEmptyData(char *pcpNewDat) {
    int ilFldCnt = 0;
    int ilFldCntRcv = 0;
    int ilFld = 0;

    if (prgOutFldLst[0].Count > 0) /* using ACTION_INFO of sqlhdl */ {
        if (pcgMissingNewFlds[0] != 0) {
            ilFldCnt = field_count(pcgMissingNewFlds);
            if (ilFldCnt > 0) {
                dbg(DEBUG, "AddMissingEmptyData: adding <%d> empty fields.", ilFldCnt);
            }
            ilFldCntRcv = field_count(pcgActFldLst) - ilFldCnt;
            for (ilFld = 1; ilFld <= ilFldCnt; ilFld++) {/* send empty data for fields, that where not received
          but have to be sent
       */
                strcat(pcpNewDat, ",");
            } /* end for */
        } /* end have to add empty fields */
    }

    return;
} /* end AddMissingEmptyData */

/*******************************************************/

/*******************************************************/
static void AddMissingData(char *pcpNewDat, char *pcpOldData) {
    int ilFldCnt = 0;
    int ilFldCntRcv = 0;
    int ilFld = 0;

    if (prgOutFldLst[0].Count > 0) /* using ACTION_INFO of sqlhdl */ {
        dbg(DEBUG, "AddMissingData BEGIN");
        dbg(DEBUG, "RCV FIELDS <%s>", pcgActFldLst);
        dbg(DEBUG, "ADD FIELDS <%s>", pcgMissingNewFlds);
        ilFldCnt = field_count(pcgMissingNewFlds);
        ilFldCntRcv = field_count(pcgActFldLst) - ilFldCnt;
        for (ilFld = 1; ilFld <= ilFldCnt; ilFld++) {/* data for this fields, that where not received
        but have to be sent, must be copied from old
     */
            get_real_item(pcgTmpBuf1, pcpOldData, ilFld + ilFldCntRcv);
            strcat(pcpNewDat, ",");
            strcat(pcpNewDat, pcgTmpBuf1);
            dbg(DEBUG, "AddMissingData adding <%s>", pcgTmpBuf1);
        } /* end for */
    }

    return;
} /* end AddMissingData */

/*******************************************************/

/*******************************************************/
static void AddMissingFields(char *pcpRcvFld, char *pcpOutFld) {
    int ilFldCnt = 0;
    int ilFld = 0;
    char pclFldNam[8];

    pcgMissingNewFlds[0] = 0; /* data for this fields, that where not received
                               but have to be sent, must be copied from old
                            */
    ilFldCnt = field_count(pcpOutFld);
    for (ilFld = 1; ilFld <= ilFldCnt; ilFld++) {
        get_real_item(pclFldNam, pcpOutFld, ilFld);
        if (strstr(pcpRcvFld, pclFldNam) == NULL) {
            strcat(pcpRcvFld, ",");
            strcat(pcpRcvFld, pclFldNam);
            strcat(pcgMissingNewFlds, pclFldNam);
            strcat(pcgMissingNewFlds, ",");
        } /* end if */
    } /* end for */
    if (strlen(pcgMissingNewFlds) > 0) {
        pcgMissingNewFlds[strlen(pcgMissingNewFlds) - 1] = 0; /* remove last ',' */
    }
    dbg(TRACE, "ADDED <%s> TO FIELD LIST.", pcgMissingNewFlds);

    return;
} /* end AddMissingFields */

/*******************************************************/

/*******************************************************/
static void AddOutFieldsToSelFields(char *pcpTbl, char *pcpFld) {
    int ilGetRc = RC_SUCCESS;
    int ilUrnoPos = 0;
    int ilTbl = 0;
    int ilFound = FALSE;

    pcgMissingNewFlds[0] = 0; /* data for this fields, that where not received
                               but have to be sent, must be copied from old
                            */
    for (ilTbl = 1; ((ilTbl <= prgOutFldLst[0].Count) && (ilFound == FALSE));
            ilTbl++) {
        if (strcmp(prgOutFldLst[ilTbl].Table, pcpTbl) == 0) {
            ilFound = TRUE;
            dbg(TRACE, "SEND ALWAYS <%s> FIELDS <%s>", prgOutFldLst[ilTbl].Table,
                    prgOutFldLst[ilTbl].Fields);
            AddMissingFields(pcpFld, prgOutFldLst[ilTbl].Fields);
        } /* end if */
    } /* end for */
} /* end AddOutFieldsToSelFields */

/*******************************************************/

/*******************************************************/
static void SetHopoDefault(char *pcpTable, char *pcpValue) {
    int ilGetRc = DB_SUCCESS;
    short slCursor = 0;
    char pclSqlBuf[128];
    char pclData[32];
    sprintf(pclSqlBuf, "ALTER TABLE %s MODIFY HOPO CHAR(3) DEFAULT '%s'",
            pcpTable, pcpValue);
    dbg(TRACE, "<%s>", pclSqlBuf);
    pclData[0] = 0x00;
    slCursor = 0;
    ilGetRc = sql_if(0, &slCursor, pclSqlBuf, pclData);
    close_my_cursor(&slCursor);
    dbg(TRACE, "READY");
    return;
}

/*******************************************************/

/*******************************************************/
static int InsertDataBuffer(int ipCmdTyp, char *pcpBuffer) {
    int ilRc = RC_SUCCESS;
    int ilGetRc = DB_SUCCESS;
    int ilGetOldData = FALSE;
    int ilProceed = TRUE;
    int ilLoopEnd = FALSE;
    int ilCallSql = TRUE;
    int ilDatLinCnt = 0;
    int ilFldCnt = 0;
    int ilItmCnt = 0;
    int ilUrnoLen = 0;
    int ilLstPos = 0;
    int ilMovDelRec = FALSE;
    long llBufferLines = 0;
    short slCursor = 0;
    char pclForWhat[16];
    char pclUrno[16];
    char pclTist[16];
    char pclChr[2];
    char pclDelTabNam[32];
    char *pclPtr = NULL;
    char *pclLineBegin = NULL;
    char *pclLineEnd = NULL;
    char *pclBuffEnd = NULL;

    dbg(DEBUG, "------------------------------------------");
    switch (ipCmdTyp) {
        case FOR_INSERT:
            strcpy(pclForWhat, "INSERT");
            break;
        case FOR_UPDATE:
            strcpy(pclForWhat, "UPDATE");
            if (igSendOldData == TRUE) {
                ilGetOldData = TRUE;
            }
            break;
        case FOR_DELETE:
            strcpy(pclForWhat, "DELETE");
            if (igSendOldData == TRUE) {
                ilGetOldData = TRUE;
            }
            if (strstr(pcgMovDelTab, pcgActTblNam) != NULL) {
                strcpy(pclDelTabNam, "XXXT4D");
                strncpy(pclDelTabNam, pcgActTblNam, 3);
                ilMovDelRec = TRUE;
            }
            break;
        default:
            strcpy(pclForWhat, "FOR WHAT?");
            break;
    }
    ilProceed = TRUE;

    if (ilGetOldData == TRUE) {
        dbg(TRACE, "MUST READ OLD DATA");
        dbg(TRACE, "FIELDS <%s>", pcgActFldLst);
        dbg(TRACE, "URNO ITEM=%d", igUrnoField);
        if (igUrnoField < 1) {
            dbg(TRACE, "URNO MISSING. UNABLE TO SEND OLD DATA");
            dbg(TRACE, "LEAVING ARRAY BUFFER HANDLING ...");
            ilGetOldData = FALSE;
            ilProceed = FALSE;
        }
    }
    ilRc = RC_FAIL;
    dbg(DEBUG, "ilProceed %d", ilProceed);
    if (ilProceed == TRUE) {
        rgDbRecDesc.FieldCount = 0;
        if (igAddHopo == TRUE) {
            if (rgDbRecDesc.CurrSize >= 4) {
                rgDbRecDesc.FieldCount = 1;
                strcpy(rgDbRecDesc.Value, pcgH3LC);
            }
        }
        SetUrnoPrefix(pcgActTblNam, pcgUrnoPrefix);
        dbg(DEBUG, "After SetUrnoPrefix");

        ilLstPos = 0;
        StrgPutStrg(rgDbUrnoList.Value, &ilLstPos, pcgUrnoPrefix, 0, -1, "\0");
        rgDbUrnoList.LineCount = 0;
        ilDatLinCnt = 0;
        ilLoopEnd = FALSE;
        pclLineBegin = pcpBuffer;
        dbg(DEBUG, "Before while");
        while (ilLoopEnd == FALSE) {
            if (strncmp(pclLineBegin, pcgRelCmdKey, igCmdKeyLen) == 0) {
                pclBuffEnd = pclLineBegin - 2;
                ilLoopEnd = TRUE;
            }/* end if */
            else {
                ilDatLinCnt++;
                pclPtr = strstr(pclLineBegin, "\n");
                if (pclPtr != NULL) {
                    *pclPtr = '\0';
                }
                if ((ilGetOldData == TRUE) || (ilMovDelRec == TRUE)) {
                    ilUrnoLen = get_real_item(pclUrno, pclLineBegin, igUrnoField) - 1;
                    if (ilUrnoLen >= 0) {
                        StrgPutStrg(rgDbUrnoList.Value, &ilLstPos, pclUrno, 0, ilUrnoLen, pcgUrnoPrefix);
                        StrgPutStrg(rgDbUrnoList.Value, &ilLstPos, ",", 0, -1, pcgUrnoPrefix);
                        rgDbUrnoList.LineCount++;
                    }
                }
                if (pclPtr != NULL) {
                    *pclPtr = '\n';
                }
                pclLineEnd = pclPtr;
                if (pclLineEnd != NULL) {
                    pclLineBegin = pclLineEnd + 1;
                } else {
                    pclBuffEnd = pclLineBegin + strlen(pclLineBegin) - 1;
                    ilLoopEnd = TRUE;
                } /* end if */
            }
        } /* end while */

        dbg(DEBUG, "After while");
        if (ilLstPos > 1) {
            if (pcgUrnoPrefix[0] != '\0') {
                ilLstPos--;
            }
            ilLstPos--;
        }
        rgDbUrnoList.Value[ilLstPos] = 0x00;
        dbg(DEBUG, "Before  ilMovDelRec If");
        if (ilMovDelRec == TRUE) {
            /* BERNI */
            sprintf(pcgSqlRead, "SELECT TABLE_NAME FROM USER_TABLES WHERE TABLE_NAME='%s'", pclDelTabNam);
            dbg(DEBUG, "<%s>", pcgSqlRead);
            slCursor = 0;
            pcgSqlDat[0] = 0x00;
            ilGetRc = sql_if(START, &slCursor, pcgSqlRead, pcgSqlDat);
            close_my_cursor(&slCursor);
            if (ilGetRc != DB_SUCCESS) {
                sprintf(pcgSqlRead, "SELECT TABLESPACE_NAME FROM USER_TABLES WHERE TABLE_NAME='%s'", pcgActTblNam);
                dbg(DEBUG, "<%s>", pcgSqlRead);
                slCursor = 0;
                pcgSqlDat[0] = 0x00;
                ilGetRc = sql_if(START, &slCursor, pcgSqlRead, pcgSqlDat);
                close_my_cursor(&slCursor);
                if (ilGetRc == DB_SUCCESS) {
                    sprintf(pcgSqlRead, "CREATE TABLE %s TABLESPACE %s AS (SELECT * FROM %s WHERE URNO IN (%s))",
                            pclDelTabNam, pcgSqlDat, pcgActTblNam, rgDbUrnoList.Value);
                } else {
                    sprintf(pcgSqlRead, "CREATE TABLE %s AS (SELECT * FROM %s WHERE URNO IN (%s))",
                            pclDelTabNam, pcgActTblNam, rgDbUrnoList.Value);
                }
                dbg(TRACE, "<%s>", pcgSqlRead);
                slCursor = 0;
                pcgSqlDat[0] = 0x00;
                ilGetRc = sql_if(IGNORE, &slCursor, pcgSqlRead, pcgSqlDat);
                close_my_cursor(&slCursor);
                /* We'll take the first tablespace name of indexes */
                /* otherwise we must find out which index is on URNO */
                sprintf(pcgSqlRead, "SELECT TABLESPACE_NAME FROM USER_INDEXES WHERE TABLE_NAME='%s'", pcgActTblNam);
                dbg(DEBUG, "<%s>", pcgSqlRead);
                slCursor = 0;
                pcgSqlDat[0] = 0x00;
                ilGetRc = sql_if(START, &slCursor, pcgSqlRead, pcgSqlDat);
                close_my_cursor(&slCursor);
                if (ilGetRc == DB_SUCCESS) {
                    sprintf(pcgSqlRead, "CREATE INDEX %s_URNO ON %s(URNO) TABLESPACE %s", pclDelTabNam, pclDelTabNam, pcgSqlDat);
                } else {
                    sprintf(pcgSqlRead, "CREATE INDEX %s_URNO ON %s(URNO)", pclDelTabNam, pclDelTabNam);
                }
                dbg(TRACE, "<%s>", pcgSqlRead);
                slCursor = 0;
                pcgSqlDat[0] = 0x00;
                ilGetRc = sql_if(IGNORE, &slCursor, pcgSqlRead, pcgSqlDat);
                close_my_cursor(&slCursor);
            } else {
                sprintf(pcgSqlRead, "INSERT INTO %s (SELECT * FROM %s WHERE URNO IN (%s))",
                        pclDelTabNam, pcgActTblNam, rgDbUrnoList.Value);
                dbg(TRACE, "<%s>", pcgSqlRead);
                slCursor = 0;
                pcgSqlDat[0] = 0x00;
                ilGetRc = sql_if(IGNORE, &slCursor, pcgSqlRead, pcgSqlDat);
                close_my_cursor(&slCursor);
            }
            GetServerTimeStamp("UTC", 1, 0, pclTist);
#ifdef CCS_DBMODE_EMBEDDED
            if(igMultiHopo == TRUE)
            {
                sprintf(pcgSqlRead, "UPDATE %s SET USEU=:VUSEU,LSTU=:VLSTU WHERE URNO IN (%s) AND HOPO IN ('***','%s')",
                    pclDelTabNam, rgDbUrnoList.Value, pcgH3LC);
            }
            else
            {
                sprintf(pcgSqlRead, "UPDATE %s SET USEU=:VUSEU,LSTU=:VLSTU WHERE URNO IN (%s)",
                    pclDelTabNam, rgDbUrnoList.Value);
            }
#else
            if(igMultiHopo == TRUE)
            {
                sprintf(pcgSqlRead, "UPDATE %s SET USEU=?,LSTU=? WHERE URNO IN (%s) AND HOPO IN ('***','%s')",
                    pclDelTabNam, rgDbUrnoList.Value, pcgH3LC);
            }
            else
            {
                sprintf(pcgSqlRead, "UPDATE %s SET USEU=?,LSTU=? WHERE URNO IN (%s)",
                    pclDelTabNam, rgDbUrnoList.Value);
            }
#endif
            dbg(TRACE, "<%s>", pcgSqlRead);
            dbg(TRACE, "USER <%s> TIST(UTC) <%s>", pcgDestName, pclTist);
            sprintf(pcgSqlDat, "%s,%s", pcgDestName, pclTist);
            slCursor = 0;
            delton(pcgSqlDat);
            ilGetRc = sql_if(IGNORE, &slCursor, pcgSqlRead, pcgSqlDat);
            close_my_cursor(&slCursor);
            commit_work();
        }
        dbg(DEBUG, "After  ilMovDelRec If");

        ilRc = RC_FAIL;
        if (ilDatLinCnt > 0) {
            while ((pclBuffEnd >= pcpBuffer) &&
                    ((*pclBuffEnd == ' ') || (*pclBuffEnd == '\n'))) {
                if (*pclBuffEnd == '\n') {
                    ilDatLinCnt--;
                }
                pclBuffEnd--;
            }
            pclBuffEnd++;
            *pclChr = *pclBuffEnd;
            *pclBuffEnd = '\0';
            dbg(DEBUG, "%s TRANSACTION BUFFER:\n%s>", pclForWhat, pcpBuffer);
            dbg(TRACE, "FOUND %d VALID DATA LINES", ilDatLinCnt);
            if (ilDatLinCnt > 0) {
                if (ilGetOldData == TRUE) {
                    dbg(TRACE, "NOW READING OLD DATA");
                    dbg(TRACE, "URNO COUNT=%d", rgDbUrnoList.LineCount);
                    dbg(TRACE, "URNO LIST <%s>", rgDbUrnoList.Value);

                    if(igMultiHopo == TRUE)
                    {
                        sprintf(pcgSqlRead, "SELECT %s FROM %s WHERE URNO IN (%s) AND HOPO IN ('***','%s')", pcgActFldLst, pcgActTblNam, rgDbUrnoList.Value,pcgH3LC);
                    }
                    else
                    {
                        sprintf(pcgSqlRead, "SELECT %s FROM %s WHERE URNO IN (%s)", pcgActFldLst, pcgActTblNam, rgDbUrnoList.Value);
                    }

                    rgDbUrnoList.Value[0] = 0x00;
                    rgDbOldData.LineCount = 0;
                    rgDbOldData.Value[0] = 0x00;
                    rgDbOldData.UsedSize = 0;
                    rgDbOldData.FldSep = ',';
                    rgDbOldData.RecSep = '\n';
                    dbg(TRACE, "<%s>", pcgSqlRead);
                    slCursor = 0;
                    ilGetRc = SqlIfArray(START, &slCursor, pcgSqlRead, rgDbUrnoList.Value, 550, &rgDbOldData);
                    close_my_cursor(&slCursor);
                    dbg(TRACE, "FOUND: %d RECORDS", rgDbOldData.LineCount);
                    dbg(TRACE, "DATA :\n%s", rgDbOldData.Value);
                }
                dbg(TRACE, "CALL %s ARRAY", pclForWhat);
                igIsCommitted = FALSE;
                slCursor = 0;
                ilGetRc = SqlIfArray(0, &slCursor, pcgSqlBuf, pcpBuffer, 0, &rgDbRecDesc);
                close_my_cursor(&slCursor);
                if (ilGetRc == DB_SUCCESS) {
                    commit_work();
                    igIsCommitted = TRUE;
                    dbg(TRACE, "%s BUFFER COMMITTED", pclForWhat);
                    if (igSilentAction == FALSE) {
                        CT_ResetContent(pcgCtNewData);
                        CT_ResetContent(pcgCtOldData);
                        CT_SetFieldList(pcgCtOldData, pcgActFldLst);
                        llBufferLines = CT_InsertBuffer(pcgCtNewData, pcpBuffer, "\n");
                        dbg(TRACE, "CT NEW DATA BUFFER LINES=%d", llBufferLines);
                        if (ilGetOldData == TRUE) {
                            llBufferLines = CT_InsertBuffer(pcgCtOldData, rgDbOldData.Value, "\n");
                            dbg(TRACE, "CT OLD DATA BUFFER LINES=%d", llBufferLines);
                        }
                        MergeBuffers(ipCmdTyp);
                    }
                    ilRc = RC_SUCCESS;
                } else {
                    dbg(TRACE, "DB %s TRANSACTION FAILED", pclForWhat);
                    rollback();
                    dbg(TRACE, "%s BUFFER ROLLED BACK", pclForWhat);
                    ilRc = RC_FAIL;
                }
            }
            *pclBuffEnd = *pclChr;
        }
    }
    dbg(DEBUG, "End ");
    return ilRc;
}

/******************************************************************************/

/******************************************************************************/
static void MergeBuffers(int ipForWhat) {
    int ilItmLen = 0;
    int ilMrgLen = 0;
    int ilNewLen = 0;
    int ilLastField = 0;
    long llUrnoCol = 0;
    long llNewCount = 0;
    long llOldCount = 0;
    long llHitCount = 0;
    long llCurLine = 0;
    long llHitLine = 0;
    char pclTmp[64];
    char *pclDmmPtr = NULL;
    char *pclNewPtr = NULL;
    char *pclMrgPtr = NULL;
    int ilDataCnt, ilFieldCnt, ilLen;

    dbg(TRACE, "============================");
    dbg(TRACE, "NOW MERGING NEW AND OLD DATA");
    dbg(TRACE, "============================");

    igRcvFldCnt = CT_CountPattern(pcgRcvFldLst, ",") + 1;
    igOutFldCnt = CT_CountPattern(pcgActFldLst, ",") + 1;
    dbg(TRACE, "FIELD COUNT: RCV=%d OUT=%d", igRcvFldCnt, igOutFldCnt);

    llUrnoCol = igUrnoField - 1;
    sprintf(pclTmp, "%d", llUrnoCol);

    /* ATTENTION: */
    /* DeleteIndex creates a segmentation fault */
    /* when an index has been used.             */
    /* Thus this feature is not used.           */
    /* See also below: GetLinesByIndexValue()  */
    /*
     */
    dbg(TRACE, "DELETING URNO INDEX ON COLNO <%s> (OLD DATA)", pclTmp);
    CT_DeleteIndex(pcgCtOldData, "URNO");
    dbg(TRACE, "CREATING URNO INDEX ON COLNO <%s> (OLD DATA)", pclTmp);
    CT_CreateIndex(pcgCtOldData, "URNO", pclTmp);

    dbg(TRACE, "============================");
    llNewCount = CT_GetLineCount(pcgCtNewData);
    llOldCount = CT_GetLineCount(pcgCtOldData);
    dbg(TRACE, "ARRAY COUNT: NEW=%d OLD=%d", llNewCount, llOldCount);

    /* argCtData[0] = New Data Line          */
    /* argCtData[1] = Old Data Line          */
    /* argCtData[2] = New URNO Value         */
    /* argCtData[3] = Old Data URNO HIT List */

    ilItmLen = 0;
    ilLastField = igRcvFldCnt - 1;

    dbg(TRACE, "============================");

    for (llCurLine = 0; llCurLine < llNewCount; llCurLine++) {
        CT_GetColumnValue(pcgCtNewData, llCurLine, llUrnoCol, &argCtData[2]);
        CT_GetLineValues(pcgCtNewData, llCurLine, &argCtData[0]);
        dbg(TRACE, "LINE NEW %d: URNO <%s> DATA <%s> LEN=%d", llCurLine,
                argCtData[2].Value, argCtData[0].Value, argCtData[0].UsedLen);

        llHitCount = CT_GetLinesByIndexValue(pcgCtOldData, "URNO", argCtData[2].Value, &argCtData[3], "0");
        /*
        llHitCount = CT_GetLinesByColumnValue(pcgCtOldData, "URNO", argCtData[2].Value, &argCtData[3], 0);
         */
        if (llHitCount > 0) {
            /* dbg(TRACE,"OLD DATA HIT LIST <%s>",argCtData[3].Value); */
            llHitLine = atol(argCtData[3].Value);
            CT_GetLineValues(pcgCtOldData, llHitLine, &argCtData[1]);
            dbg(TRACE, "LINE OLD %d: DATA <%s>", llHitLine, argCtData[1].Value);
        } else {
            /* dbg(TRACE,"MERGE: OLD DATA NOT FOUND"); */
            memset(argCtData[1].Value, ',', igOutFldCnt - 1);
            argCtData[1].Value[igOutFldCnt - 1] = 0x00;
        }

        /* Cut NewData behind last the field:    */
        /* In case of URT or DRT we must cut off */
        /* the URNO value of the WhereClause     */
        pclDmmPtr = GetItemPointer(argCtData[0].Value, ilLastField, ",", &ilItmLen);
        if (pclDmmPtr != NULL) {
            argCtData[0].UsedLen = pclDmmPtr - argCtData[0].Value + ilItmLen;
        }
        /* dbg(TRACE,"LINE NEW %d: CUT: <%s> LEN=%d", llCurLine, argCtData[0].Value,argCtData[0].UsedLen); */

        if (igOutFldCnt > igRcvFldCnt) {
            /* We must append SendAlways data to the new values */
            /* Get OldData behind last NEW Field, but don't cut */
            pclMrgPtr = GetItemPointer(argCtData[1].Value, igRcvFldCnt, ",", NULL);
            if (pclMrgPtr != NULL) {
                ilMrgLen = strlen(pclMrgPtr);
                /* dbg(TRACE,"LINE OLD %d: MERG <%s> LEN=%d", llHitLine, pclMrgPtr,ilMrgLen); */
                ilNewLen = argCtData[0].UsedLen + ilMrgLen + 4;
                if (ilNewLen > argCtData[0].AllocatedSize) {
                    argCtData[0].Value = realloc(argCtData[0].Value, ilNewLen);
                    argCtData[0].AllocatedSize = ilNewLen;
                }
                ilMrgLen--;
                ilNewLen = argCtData[0].UsedLen;
                StrgPutStrg(argCtData[0].Value, &ilNewLen, ",", 0, -1, "\0");
                StrgPutStrg(argCtData[0].Value, &ilNewLen, pclMrgPtr, 0, ilMrgLen, ",");
                ilNewLen--;
                argCtData[0].Value[ilNewLen] = 0x00;
                /* dbg(TRACE,"LINE NEW %d: OUT: <%s> LEN=%d", llCurLine, argCtData[0].Value,argCtData[0].UsedLen); */
            }
        }

        /* Note: */
        /* It would be correct to rease all New Data */
        /* but the old style format distributed the  */
        /* same information in New and Old.          */
        /* Thus this feature is commented out.       */
        /*
        if (ipForWhat == FOR_DELETE)
        {
          memset(argCtData[0].Value, ',', igOutFldCnt-1);
          argCtData[0].Value[igOutFldCnt-1] = 0x00;
        }
         */
        dbg(DEBUG, "MergeBuffers: fields <%s>\nNew <%s> Old <%s>", pcgActFldLst, argCtData[0].Value, argCtData[1].Value);

        /** Mei, bug in var assignment **/
        ilFieldCnt = get_no_of_items(pcgActFldLst);
        ilDataCnt = get_no_of_items(argCtData[0].Value);
        dbg(DEBUG, "MergeBuffers: #Data <%d> #Fields <%d>", ilDataCnt, ilFieldCnt);
        if ((ilDataCnt != ilFieldCnt) && (rgDbRecDesc.FieldCount == 1) && rgDbRecDesc.Value[0]) {
            dbg(DEBUG, "MergeBuffers: rgDbRecDesc.FieldCount <%d> rgDbRecDesc.Value <%s>", rgDbRecDesc.FieldCount, rgDbRecDesc.Value);
            ilLen = argCtData[0].UsedLen + 1 + strlen(rgDbRecDesc.Value);
            if (ilLen > argCtData[0].AllocatedSize) {
                argCtData[0].Value = realloc(argCtData[0].Value, ilLen);
                argCtData[0].AllocatedSize = ilLen;
            }

            ilLen = argCtData[0].UsedLen;
            argCtData[0].Value[ilLen] = ',';
            ilLen++;
            StrgPutStrg(argCtData[0].Value, &ilLen, rgDbRecDesc.Value, 0, -1, "\0");
            argCtData[0].Value[ilLen] = '\0';
            argCtData[0].UsedLen = ilLen;
            dbg(TRACE, "MergeBuffers: Added HOPO, now UsedLen <%d> Data <%s> ", argCtData[0].UsedLen, argCtData[0].Value);
        }

        sprintf(pclTmp, "WHERE URNO=%s%s%s", pcgUrnoPrefix, argCtData[2].Value, pcgUrnoPrefix);
        (void) ToolsSpoolEventData(pcgOutRoute, pcgActTblNam, pcgActCmdCod,
                argCtData[2].Value, pclTmp, pcgActFldLst,
                argCtData[0].Value, argCtData[1].Value);
    } /* end for all new lines */

    return;
}

/******************************************************************************/

/******************************************************************************/
static char *GetItemPointer(char *pcpList, int ipItemNo, char *pcpSepa, int *pipLen) {
    int ilSepLen = 0;
    int ilItmLen = 0;
    char *pclBgn = NULL;
    char *pclEnd = NULL;
    ilSepLen = strlen(pcpSepa);
    pclBgn = pcpList;
    while ((ipItemNo > 0) && (pclBgn != NULL)) {
        pclBgn = strstr(pclBgn, pcpSepa);
        if (pclBgn != NULL) {
            pclBgn += ilSepLen;
            ipItemNo--;
        }
    }
    if ((pipLen != NULL) && (pclBgn != NULL)) {
        pclEnd = strstr(pclBgn, pcpSepa);
        if (pclEnd != NULL) {
            ilItmLen = pclEnd - pclBgn;
        } else {
            ilItmLen = strlen(pclBgn);
        }
        if (*pipLen >= 0) {
            pclBgn[ilItmLen] = 0x00;
        }
        *pipLen = ilItmLen;
    }
    return pclBgn;
}

/******************************************************************************/

/******************************************************************************/
static void SetUrnoPrefix(char *pcpTable, char *pcpPrefix) {
    int ilGetRc = DB_SUCCESS;
    char pclSqlBuf[32];

    strcpy(pcpPrefix, "'");
    sprintf(pclSqlBuf, "SELECT URNO FROM %s", pcpTable);
    ilGetRc = GetSqlCmdInfo(pclSqlBuf, &rgDbUrnoList);
    if (ilGetRc == DB_SUCCESS) {
        if (rgDbUrnoList.FieldInfo[0].DbFieldType == 2) {
            strcpy(pcpPrefix, "");
        }
    }
    return;
}

/******************************************************************************/

/******************************************************************************/
static void TestLists(void) {
    int i = 0;
    char pclBuff[4096];
    /*
    ToolsListSetInfo(prgTestList,  "TEST_LIST", "LIST_HEAD",
                    "{", "}", ";", ".\n.", 4096, TRUE);
     */
    for (i = 1; i <= 20; i++) {
        sprintf(pclBuff, "%d,123,123,123,123", i);
        ToolsListAddValue(prgTestList, pclBuff, -1);
    }
    dbg(TRACE, "LIST BUFFER:\n<%s>", prgTestList->ValueList);
    return;
}
/******************************************************************************/

/*** MEI 20081013 ***/
static int Init_table_forward(char *pcpFname) {
    FILE *rlFptr;
    int ilFound = FALSE;
    int ilStartFound = FALSE;
    int ili = 0;
    int ilLen;
    int ilID;
    char pclLine[50][2048];
    char pclStartSec[32];
    char pclEndSec[32];
    char pclTmpStr[2048];
    char pclIDStr[128];
    char *pclToken;
    char *pclFunc = "Init_table_forward";

    rlFptr = fopen(pcpFname, "r");
    if (rlFptr == NULL) {
        dbg(TRACE, "Error in reading Config Entry errno = %d<%s>", errno, strerror(errno));
        return RC_FAIL;
    }
    strcpy(pclStartSec, "[TABLE_FORWARDING]");
    strcpy(pclEndSec, "[TABLE_FORWARDING_END]");

    while (!feof(rlFptr)) {
        if (fgets(pclTmpStr, sizeof (pclTmpStr), rlFptr) == NULL)
            break;
        tool_filter_spaces(pclTmpStr);
        if (pclTmpStr[0] == '#')
            continue;
        if (!strncmp(pclTmpStr, pclEndSec, strlen(pclEndSec)))
            break;
        if (!strncmp(pclTmpStr, pclStartSec, strlen(pclStartSec))) {
            ilStartFound = TRUE;
            continue;
        }
        if (ilStartFound == FALSE)
            continue;
        if (pclTmpStr[0] == '[')
            break;
        if (strstr(pclTmpStr, "=") == NULL)
            continue;
        if (ili >= 50)
            break;
        ilLen = strlen(pclTmpStr) - 1;
        if (ilLen > 0 && !isalpha(pclTmpStr[ilLen]))
            pclTmpStr[ilLen] = '\0';
        strcpy(pclLine[ili], pclTmpStr);
        dbg(TRACE, "<%s> Line[%d] = <%s>", pclFunc, ili, pclLine[ili]);
        ili++;
    }
    fclose(rlFptr);

    igNumTableForward = ili;
    dbg(TRACE, "<%s> Number of tables to be forwarded = <%d>", pclFunc, igNumTableForward);

    if (igNumTableForward <= 0)
        return;

    pcgTableForward = malloc(igNumTableForward * sizeof (*pcgTableForward));
    if (pcgTableForward == NULL) {
        dbg(TRACE, "<%s> Malloc failed", pclFunc);
        return RC_FAIL;
    }

    for (ili = 0; ili < igNumTableForward; ili++) {
        pclToken = (char *) strtok(pclLine[ili], "=");
        if (pclToken == NULL)
            continue;
        strcpy(pcgTableForward[ili], pclToken);
        strcat(pcgTableForward[ili], "=");
        pclToken = (char *) strtok(NULL, "=");
        if (pclToken == NULL)
            continue;
        strcpy(pclTmpStr, pclToken);

        pclToken = (char *) strtok(pclTmpStr, ",");
        while (pclToken != NULL) {
            ilID = GetOutModId(pclToken);
            if (ilID > 0) {
                sprintf(pclIDStr, "%d,", ilID);
                strcat(pcgTableForward[ili], pclIDStr);
            }
            pclToken = (char *) strtok(NULL, ",");
        }
        ilLen = strlen(pcgTableForward[ili]);
        if (ilLen > 0 && pcgTableForward[ili][ilLen - 1] == ',');
        pcgTableForward[ili][ilLen - 1] = '\0';
        dbg(TRACE, "<%s> Table Forward [%d] = <%s>", pclFunc, ili, pcgTableForward[ili]);
    }
    return RC_SUCCESS;
} /* Init_table_forward */

static void setHomeAirport(char *pcpProcessName, char *pcpHopo)
{
    int ilRc = RC_SUCCESS;
	char *pclFunc = "setHomeAirport";

	memset(cgCfgBuffer,0x0,sizeof(cgCfgBuffer));

	/*
	1-	read HOME_AIRPORT_SOURCE
	2-	if HOME_AIRPORT_SOURCE is found in .cfg file
			if the value of HOME_AIRPORT_SOURCE is not null
				if HOME_AIRPORT_SOURCE == SGS.TAB
					use the value from sgs.tab
					if the value length >3, then use the appointed hopo
				else if HOME_AIRPORT_SOURCE == CONFIG then use goto [HOME_AIRPORT]
					compare the process name and collect the corresponding HOPO
			else if HOME_AIRPORT_SOURCE is null
				use the value from sgs.tab
				if the value length >3, then use the appointed hopo
		else if HOME_AIRPORT_SOURCE is not found in .cfg file
				use the value from sgs.tab
				if the value length >3, then use the appointed hopo
	*/
	ilRc = ReadConfigEntry("SYSTEM", "HOME_AIRPORT_SOURCE", cgCfgBuffer);
	if (ilRc == RC_SUCCESS)
	{
		if(strlen(cgCfgBuffer) == 0)
		{
			dbg(TRACE, "%s: HOME_AIRPORT_SOURCE is null, read the default value from sgs.tab", pclFunc);
			readDefaultHopo(pcpHopo);
			//setupHopo(pcpHopo);
		}
		else
		{
			if(strcmp(cgCfgBuffer,"SGS.TAB") == 0)
			{
				dbg(TRACE, "%s: HOME_AIRPORT_SOURCE is SGS.TAB, read the default value from sgs.tab", pclFunc);
				readDefaultHopo(pcpHopo);
				//setupHopo(pcpHopo);
			}
			else if(strcmp(cgCfgBuffer,"CONFIG") == 0)
			{
			    igMultiHopo = TRUE;

			    dbg(TRACE, "%s: igMultiHopo is set as TRUE-1", pclFunc);

				memset(cgCfgBuffer,0x0,sizeof(cgCfgBuffer));
				ilRc = ReadConfigEntry("HOME_AIRPORT", to_upper(pcpProcessName), cgCfgBuffer);
				if (ilRc == RC_SUCCESS)
				{
					if(strlen(cgCfgBuffer) > 0)
					{
						strcpy(pcpHopo,cgCfgBuffer);
						dbg(TRACE, "%s: HOME_AIRPORT_SOURCE set to <%s>", pclFunc, pcpHopo);
					}
					else
					{
						dbg(TRACE, "%s: <%s> is not found-1, read the default value from sgs.tab", pclFunc, to_upper(pcpProcessName));
						readDefaultHopo(pcpHopo);
						//setupHopo(pcpHopo);
					}
				}
				else
				{
					dbg(TRACE, "%s: <%s> is not found-2, read the default value from sgs.tab", pclFunc, to_upper(pcpProcessName));
					readDefaultHopo(pcpHopo);
					//setupHopo(pcpHopo);
				}
			}
		}

		dbg(TRACE, "%s: HOME_AIRPORT_SOURCE set to <%s>", pclFunc, pcpHopo);
	}
	else
	{
	    igMultiHopo = TRUE;
	    dbg(TRACE, "%s: igMultiHopo is set as TRUE-2", pclFunc);
		dbg(TRACE, "%s: HOME_AIRPORT_SOURCE is not found-3, read the default value from sgs.tab", pclFunc);
		readDefaultHopo(pcpHopo);
		//setupHopo(pcpHopo);
	}
}

static void readDefaultHopo(char *pcpHopo)
{
    int ilRc = RC_SUCCESS;
	char *pclFunc = "readDefaultHopo";

	/* read HomeAirPort from SGS.TAB */
	ilRc = tool_search_exco_data("SYS", "HOMEAP", pcpHopo);
	if (ilRc != RC_SUCCESS)
	{
		dbg(TRACE, "<%s> EXTAB,SYS,HOMEAP not found in SGS.TAB", pclFunc);
		Terminate();
	}
}

static void setupHopo(char *pcpHopo)
{
    char *pclFunc = "setupHopo";
	if(strlen(pcpHopo) > 3)
	{
		dbg(TRACE, "<%s> The length of homeairport<%s> > 3", pclFunc, pcpHopo);
		pcpHopo[3] = '0';
	}
	dbg(TRACE, "<%s> home airport <%s>", pclFunc, pcpHopo);
}

static int checkAndsetHopo(char *pcpTwEnd, char *pcpHopo_sgstab)
{
    int ilLen = 0;
    int ilRc = RC_FAIL;
	char *pclFunc = "checkAndsetHopo";

	char pclTmp[32] = "\0";

	/*char pclHopoEvent[512] = "\0";*/

	if ( (pcpTwEnd == NULL) || (strlen(pcpTwEnd) == 0) )
	{
	    dbg(TRACE, "%s: Received pcpTwEnd = <%s> is null",pclFunc, pcpTwEnd);
		ilRc = RC_FAIL;
	}
	else
	{
		ilLen = get_real_item(pcgH3LC, pcpTwEnd, 1);
		if (strlen(pcgH3LC) == 0 || ilLen != 3)
		{
		    dbg(TRACE, "%s: Extracting pcpTwEnd = <%s> for received hopo fails or length < 3",pclFunc, pcpTwEnd);
		    ilRc = RC_FAIL;
		}
		else
		{
		    strcat(pcgH3LC,",");
		    sprintf(pclTmp,"%s,",pcpHopo_sgstab);

            if  ( strstr(pclTmp, pcgH3LC) == 0 )
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is not in SGS.TAB HOPO<%s>",
                           pclFunc, pcgH3LC, pclTmp);
                ilRc = RC_FAIL;
            }
            else
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is in SGS.TAB HOPO<%s>",
                           pclFunc, pcgH3LC, pclTmp);
                ilRc = RC_SUCCESS;
            }
		}
	}

	return ilRc;
}

/*
static int checkHopo(char *pcpTwEnd, char *pcpHopo)
{
	char *pclFunc = "checkHopo";

	if( strncmp(pcpHopo,pcpHopo,3) != 0 )
	{
		dbg(TRACE, "<%s> Received HOPO<%3s> != initial HOPO<%s>", pclFunc, pcpTwEnd, pcpHopo);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE, "<%s> Received HOPO<%3s> == initial HOPO<%s>", pclFunc, pcpTwEnd, pcpHopo);
		return RC_SUCCESS;
	}
}
*/

static char *to_upper(char *pcgIfName)
{
  char *ptr;

  ptr = pcgIfName;
  while (*ptr != '\0') {
    *ptr = toupper(*ptr);
    ptr++;
  }
  return pcgIfName;
}

/******************************************************************************/
/******************************************************************************/
/* Space for Appendings (Source-Code Control)                                 */
/******************************************************************************/
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
