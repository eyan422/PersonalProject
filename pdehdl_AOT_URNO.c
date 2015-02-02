#ifndef _DEF_mks_version
#define _DEF_mks_version
#include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/pdehdl.c 1.26d 2015/01/26 9:52:56SGT fya Exp  $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* ABB ACE/FC Program Pdehdl                                                  */
/*                                                                            */
/* Author         :  hag                                                      */
/* Date           :                                                           */
/* Description    :  Pending event handler                                    */
/*                   Used to udpate database for special purposes             */
/* Update history :                                                           */
/* 20030630 JIM: may core!: dbg(TRACE,"Command: <%s>,prlCmdblk->command")     */
/* 20120220 CST: ATC for BLR (RPT command), TMO for AUH (TMO based on TIFA)   */
/* 20120322 CST: Merged bugfixes for TMO and In/Out bound fields (CST/MEI)    */
/* 20120421 MEI: FTYP config is not read. Remove hardcoding  (v1.16)          */
/* 20120521 MEI: Decided to develop new BATCH command for EFPS (v1.17)        */
/* 20120607 MEI: Bug in Batch sending function (v1.18)                        */
/* 20120622 MEI: EFPS required customized flight sending criteria (v1.20)     */
/* 20120824 FYA: Fix a dbg parameter mismatching bug imported by me in 		  */
/* InitForPortal()     */
/* 20130410 PDI: Made changes for BLR BHS interface  					      */
/* 20130704 PDI: Made changes for BLR BHS interface v1.24 					  */
/* 20131009 PDI: Made changes for BLR VDGS interface v1.25 					  */
/* 20131009 PDI: Made changes for BLR VDGS interface v1.25 					  */
/* 20140307 YYA: Merge the pdehdl change for UFIS-2146 CPRF change            */
/* 20140911 FYA: v1.26a UFIS-7275 clear buffer for VDGS*/
/* 20140911 FYA: v1.26b UFIS-7306 change the query in SendBatch*/
/* 20150113 FYA: v1.26c UFIS-8297 HOPO implementation*/
/* 20150202 FYA: v1.26d UFIS-8325 URNO expansion*/
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */

/* static char sccs_version[] ="@(#) UFIS44 (c) ABB AAT/I pdehdl.c 4.5.1.1 / 2013/04/10 HAG";

 be carefule with strftime or similar functions !!!

 ******************************************************************************/
/* This program is a MIKE main program */

#define TEST
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
#include <AATArray.h>

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
static ITEM *prgItem = NULL; /* The queue item pointer  */
static EVENT *prgEvent = NULL; /* The event pointer       */
static int igItemLen = 0; /* length of incoming item */
static EVENT *prgOutEvent = NULL;

static char cgProcessName[20] = "\0";
static char cgConfigFile[512] = "\0";

/*FYA v1.26c UFIS-8297*/
/*static char cgHopo[8] = "\0";*/ /* default home airport    */
static char cgHopo[256] = "\0"; /* default home airport    */
static char cgHopo_sgstab[256] = "\0"; /* default home airport    */
static char cgTabEnd[8] = "\0"; /* default table extension */

static long lgEvtCnt = 0;

#define ARR_NAME_LEN   10
#define ARR_MAX_FIELDS 10

/*FYA v1.26d UFIS-8325*/
#define URNO_LEN 64

struct _arrayinfo {
    HANDLE rrArrayHdl;
    char crArrayName[ARR_NAME_LEN + 1];
    char crArrayFldList[ARR_MAX_FIELDS * 5];
    long lrArrayFldCnt;
    long plrArrayFldOfs[ARR_MAX_FIELDS];
    long plrArrayFldLen[ARR_MAX_FIELDS];
    HANDLE rrIdx01Hdl;
    char crIdx01Name[ARR_NAME_LEN + 1];
};
typedef struct _arrayinfo ARRAYINFO;

BOOL IS_EMPTY(char *pcpBuf) {
    return (!pcpBuf || *pcpBuf == ' ' || *pcpBuf == '\0') ? TRUE : FALSE;
}
/** fuer VDG **/

#ifdef ARR_NAME_LEN
#undef ARR_NAME_LEN
#endif
#define ARR_NAME_LEN   (18)
#define ARR_FLDLST_LEN (256)

struct _VDGarrayinfo {
    HANDLE rrArrayHandle;
    char crArrayName[ARR_NAME_LEN + 1];
    char crArrayFieldList[ARR_FLDLST_LEN + 1];
    long lrArrayFieldCnt;
    char *pcrArrayRowBuf;
    long lrArrayRowLen;
    long *plrArrayFieldOfs;
    long *plrArrayFieldLen;
    HANDLE rrIdx01Handle;
    char crIdx01Name[ARR_NAME_LEN + 1];
    char crIdx01FieldList[ARR_FLDLST_LEN + 1];
    long lrIdx01FieldCnt;
    char *pcrIdx01RowBuf;
    long lrIdx01RowLen;
    long *plrIdx01FieldPos;
    long *plrIdx01FieldOrd;

    /*****************/
    HANDLE rrIdx02Handle;
    char crIdx02Name[ARR_NAME_LEN + 1];
    HANDLE rrIdx03Handle;
    char crIdx03Name[ARR_NAME_LEN + 1];
    HANDLE rrIdx04Handle;
    char crIdx04Name[ARR_NAME_LEN + 1];
    /****************************/
};

typedef struct _VDGarrayinfo VDGARRAYINFO;

static char cgCfgBuffer[1024];

static VDGARRAYINFO rgAftArrayInbound;
static VDGARRAYINFO rgAftArrayOutbound;

static char *pcgUrnoListIn = NULL;
static long lgUrnoListLengthIn = 50000;
static long lgUrnoListActualLengthIn = 0;

static char *pcgUrnoListOut = NULL;
static long lgUrnoListLengthOut = 50000;
static long lgUrnoListActualLengthOut = 0;

#define AFTFIELD(pcpBuf,ipFieldNo) (&pcpBuf[rgAftArrayInbound.plrArrayFieldOfs[ipFieldNo]])
#define FIELDVAL(rrArray,pcpBuf,ipFieldNo) (&pcpBuf[rrArray.plrArrayFieldOfs[ipFieldNo]])
#define PFIELDVAL(prArray,pcpBuf,ipFieldNo) (&pcpBuf[prArray->plrArrayFieldOfs[ipFieldNo]])
extern int get_no_of_items(char *s);

#define AFTFIELDS "URNO,ADID,TIFA,TIFD,STOA,STOD,FLNO"

static int igAftURNO;
static int igAftADID;
static int igAftSTOA;
static int igAftSTOD;
static int igAftTIFA;
static int igAftTIFD;
static int igAftFLNO;
static int igAftSENT;
static int igTMOFld;

static int igStartUpMode = TRACE;
static int igRuntimeMode = TRACE;

static char cgAftFields[2048];

static int igRangeFromInbound = -30;
static int igRangeFromOutbound = -30;
static int igRangeToInbound = 30;
static int igRangeToOutbound = 30;
static int igTimeInbound = -10;
static int igTimeOutbound = -10;

/*Frank v1.19 20120620*/
static int igFrom_In = 0;
static int igTo_In = 120;
static int igFrom_Out = 0;
static int igTo_Out = 120;
static int igSendToModIdForPortal = 7922;
static int igSendToModIdForVDGS = 7922;
/*Frank v1.19 20120620*/

static int igSetOnblAfterLand = 10;
static char pcgOnblField[8] = "ONBS";
static int igModIdFlight = 7800;

static int igSendToModId = 7922;
static char pcgResend[8] = "NO";

static char cgFieldListIn[1024] = "URNO,ADID,STOA,FLNO";
static char cgFieldListOut[1024] = "URNO,ADID,STOD,FLNO";
static char cgVdgSendCommandInbound[124] = "VDE";
static char cgVdgSendCommandOutbound[124] = "VDE";
static char cgTriggerFieldListIn[515] = "";
static char cgTriggerFieldListOut[515] = "";

/* MEI v1.16 */
static char pcgFtypInbound[128];
static char pcgFtypOutbound[128];
/* MEI v1.17 */
static int igInUaftIdx;
static int igOutUaftIdx;
static int igInNumFields;
static int igOutNumFields;
/* MEI v1.18 */
static char cgIncludeReturn;
/* MEI v1.19 */
static int igSpecialRangeFromInbound = -30;
static int igSpecialRangeFromOutbound = -30;
static int igSpecialRangeToInbound = 30;
static int igSpecialRangeToOutbound = 30;

/* Frank v1.16 Debug*/
static char pcgMode[128];
static int igMode;

static int igUaftIdx;
static char pcgTableName[128];

/*Frank v1.19 20120620*/
static char pcgFtyp_In[128] = "\0";
static char pcgFtyp_Out[128] = "\0";
static int igURNO_In = 0;
static int igURNO_Out = 0;
static int igNumFields_In = 0;
static int igNumFields_Out = 0;
static char cgField_In[1024] = "URNO,ADID,STOA,FLNO";
static char cgField_Out[1024] = "URNO,ADID,STOD,FLNO";
static char cgCommand_In[124] = "UXML";
static char cgCommand_Out[124] = "UXML";
/*Frank v1.19 20120620*/

#define CMD_IFR     (0)
#define CMD_UFR     (1)
#define CMD_DFR     (2)
#define CMD_VDG     (3)
#define CMD_VDR     (4)
#define CMD_SETOAL  (5)
#define CMD_REPEAT  (6)
#define CMD_TMO     (7)
#define CMD_BATCH   (8)
#define CMD_PORTAL  (9)
#define CMD_BATCH_EFPS   (10)
#define CMD_REQ	    (11)
/*BHS*/
#define CMD_RESEND   (12)
/*static char *prgCmdTxt[] = { "IFR",   "UFR",   "DFR",  "VDG", "VDR", "SETOAL", "RPT", "TMO", "BAT", "POR", "BATE", NULL }; */
/*static int    rgCmdDef[] = { CMD_IFR, CMD_UFR,CMD_DFR,CMD_VDG, CMD_VDR, CMD_SETOAL, CMD_REPEAT, CMD_TMO, CMD_BATCH, CMD_PORTAL, CMD_BATCH_EFPS, 0 }; */
static char *prgCmdTxt[] = {"IFR", "UFR", "DFR", "VDG", "VDR", "SETOAL", "RPT", "TMO", "BAT", "POR", "BATE", "BATR", NULL};
static int rgCmdDef[] = {CMD_IFR, CMD_UFR, CMD_DFR, CMD_VDG, CMD_VDR, CMD_SETOAL, CMD_REPEAT, CMD_TMO, CMD_BATCH, CMD_PORTAL, CMD_BATCH_EFPS, CMD_RESEND, 0};

/*BHS*/

/* Global Variables for PRFL Handling */
static char pcgTwStart[128];
static char pcgTwEnd[128];
static char pcgCurrentTime[16];
static char pcgLastDay[16];
static char pcgWaitTill[16];
static int igTriggerAction = FALSE;
static int igTriggerBchdl = FALSE;
static int igBcOutMode = 0;
/* End Global Variables for PRFL Handling */

static int igForATC = 0;
static int igSetTMO = 0;
static char cgTMOField[24] = "\0";
static int igTMOIsATCBased = 0;

/*FYA v1.26c UFIS-8297*/
static int igMultiHopo = FALSE;

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
static int Init_Pdehdl();
static int ReadConfigEntry(char *pcpSection, char *pcpKeyword,
        char *pcpCfgBuffer);


static int Reset(void); /* Reset program          */
static void Terminate(int ipSleep); /* Terminate program      */
static void HandleSignal(int); /* Handles signals        */
static void HandleErr(int); /* Handles general errors */
static void HandleQueErr(int); /* Handles queuing errors */
static void HandleQueues(void); /* Waiting for Sts.-switch*/


static int HandleData(EVENT *prpEvent); /* Handles event data     */
static int GetCommand(char *pcpCommand, int *pipCmd);
static int UpdDemUtpl();
static int IniDemRedundance();
static int SetArraySimple(ARRAYINFO *prpArr, char *pcpTable, char *pcpFields,
        char *pcpIdxField, BOOL bpFill, char *pcpSelection);
static int GetResourceName(ARRAYINFO *prpArray, ARRAYINFO *prpSgrArr,
        int ipGtabIdx, int ipUresIdx, int ipCodeIdx,
        char *pcpUrud, char *pcpName);
static int GetNameFromBasicData(char *pcpReft, char *pcpUrno, char *pcpName);
static int GetQualifications(ARRAYINFO *prpRpqArr, ARRAYINFO *prpSgrArr,
        char *pcpUrud, char *pcpQuco);
static int IniJobRedundance();
static int GetAlidAloc(ARRAYINFO *prpAloArray, ARRAYINFO *prpAlidResults,
        char *pcpUalo, char *pcpUaid, char *pcpAloc, char *pcpAlid);
static void DebugPrintArrayInfo(int ipDebugLevel, ARRAYINFO *prpArrayInfo);
static int GetStartEnd(BOOL bpStart, char *pcpTime);
static int SetArrayInfo(char *pcpArrayName, char *pcpTableName,
        char *pcpArrayFieldList, char *pcpAddFields, long *pcpAddFieldLens,
        VDGARRAYINFO *prpArrayInfo, char *pcpSelection, BOOL bpFill);

static int SendToVdgs();
static int HandleSETOAL();

/* Function Prototypes for PRFL Handling */
extern int AddSecondsToCEDATime(char *, long, int);
static int TimeToStr(char *pcpTime, time_t lpTime);
static int GetPrflConfig();
static int TriggerBchdlAction(char *pcpCmd, char *pcpTable, char *pcpSelection,
        char *pcpFields, char *pcpData, int ipBchdl, int ipAction);
static int HandleCPRF();
/* End Function Prototypes for PRFL Handling */

/* MEI v1.17 - own batch sending program. Not using SendToVdgs */
/*BHS static int SendBatch();*/
static int SendBatch(int igCmd, char *pcpData);
static int GetAndSendRecords(char *pcpSqlBuf, int ipUaftIdx, int ipNumFields, char *pcpFlightInOut);

static int SendPortal(void);
static int InitForPortal(void);
static int GetAndSendRecordsForPortal(char *pcpSqlBuf, int ipUaftIdx, int ipNumFields, char *pcpFlightInOut);

static int SendSingle(char *pcpData);

/*FYA v1.26c UFIS-8297*/
static void setHomeAirport(char *pcpProcessName, char *pcpHopo);
static void readDefaultHopo(char *pcpHopo);
static void setupHopo(char *pcpHopo);
static int checkAndsetHopo(char *pcpTwEnd, char *pcpHopo);
static char *to_upper(char *pcgIfName);
/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN{
    int ilRc = RC_SUCCESS; /* Return code          */
    int ilCnt = 0;
    int ilOldDebugLevel = 0;


    INITIALIZE; /* General initialization   */



    debug_level = TRACE;

    strcpy(cgProcessName, argv[0]);

    dbg(TRACE, "MAIN: version <%s>", mks_version);

    /* Attach to the MIKE queues */
    do {
        ilRc = init_que();
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "MAIN: init_que() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        }/* end of if */
    } while ((ilCnt < 10) && (ilRc != RC_SUCCESS));

    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "MAIN: init_que() failed! waiting 60 sec ...");
        sleep(60);
        exit(1);
    } else {
        dbg(TRACE, "MAIN: init_que() OK! mod_id <%d>", mod_id);
    }/* end of if */

    dbg(TRACE, "MAIN: Connecting to database");

    do {
        dbg(TRACE, "MAIN: Calling init_db()");
        ilRc = init_db();
        dbg(TRACE, "MAIN: Back from init_db()");
        if (ilRc != RC_SUCCESS) {
            check_ret(ilRc);
            dbg(TRACE, "MAIN: init_db() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        } /* end of if */
    } while ((ilCnt < 10) && (ilRc != RC_SUCCESS));

    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "MAIN: init_db() failed! waiting 60 sec ...");
        sleep(60);
        exit(2);
    } else {
        dbg(TRACE, "MAIN: init_db() OK!");
    } /* end of if */

    /* logon to DB is ok, but do NOT use DB while ctrl_sta == HSB_COMING_UP !!! */

    sprintf(cgConfigFile, "%s/%s", getenv("BIN_PATH"), mod_name);
    dbg(TRACE, "ConfigFile <%s>", cgConfigFile);
    ilRc = TransferFile(cgConfigFile);
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "MAIN: TransferFile(%s) failed!", cgConfigFile);
    } /* end of if */

    sprintf(cgConfigFile, "%s/%s.cfg", getenv("CFG_PATH"), mod_name);
    dbg(TRACE, "ConfigFile <%s>", cgConfigFile);
    ilRc = TransferFile(cgConfigFile);
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "MAIN: TransferFile(%s) failed!", cgConfigFile);
    } /* end of if */

    ilRc = SendRemoteShutdown(mod_id);
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "MAIN: SendRemoteShutdown(%d) failed!", mod_id);
    } /* end of if */

    if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY)) {
        dbg(DEBUG, "MAIN: waiting for status switch ...");
        HandleQueues();
    }/* end of if */

    if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY)) {
        dbg(TRACE, "MAIN: initializing ...");
        ilRc = Init_Pdehdl();
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "Init_Pdehdl: init failed!");
        } /* end of if */

    } else {
        Terminate(30);
    }/* end of if */



    dbg(TRACE, "=====================");
    dbg(TRACE, "MAIN: initializing OK");
    dbg(TRACE, "=====================");


    for (;;) {
        ilRc = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, igItemLen, (char *) &prgItem);
        /* Acknowledge the item */
        ilRc = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
        if (ilRc != RC_SUCCESS) {
            /* handle que_ack error */
            HandleQueErr(ilRc);
        } /* fi */

        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */

        prgEvent = (EVENT *) prgItem->text;

        if (ilRc == RC_SUCCESS) {

            lgEvtCnt++;

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
                    Terminate(1);
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
                    Terminate(1);
                    break;

                case RESET:
                    ilRc = Reset();
                    break;

                case EVENT_DATA:
                    if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY)) {

                        if (igMode == TRUE) {
                            dbg(DEBUG, "line<%d>---------before HandleData----------------", __LINE__);
                            dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                            dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
                            dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                            dbg(DEBUG, "line<%d>---------before HandleData----------------", __LINE__);
                        }

                        ilRc = HandleData(prgEvent);
                        if (ilRc != RC_SUCCESS) {
                            HandleErr(ilRc);
                        }/* end of if */
                    } else {
                        dbg(TRACE, "MAIN: wrong hsb status <%d>", ctrl_sta);
                        DebugPrintItem(TRACE, prgItem);
                        DebugPrintEvent(TRACE, prgEvent);
                    }/* end of if */

                    if (igMode == TRUE) {
                        dbg(DEBUG, "line<%d>---------after HandleData----------------", __LINE__);
                        dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                        dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
                        dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                        dbg(DEBUG, "line<%d>---------after HandleData----------------", __LINE__);
                    }
                    break;


                case TRACE_ON:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case TRACE_OFF:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case 300:
                    UpdDemUtpl();
                    break;
                case 301:
                    IniDemRedundance();
                    break;
                case 302:
                    IniJobRedundance();
                    break;
                default:
                    dbg(TRACE, "MAIN: unknown event");
                    DebugPrintItem(TRACE, prgItem);
                    DebugPrintEvent(TRACE, prgEvent);
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



static int ReadConfigEntry(char *pcpSection, char *pcpKeyword, char *pcpCfgBuffer) {
    int ilRc = RC_SUCCESS;

    char clSection[124] = "\0";
    char clKeyword[124] = "\0";

    strcpy(clSection, pcpSection);
    strcpy(clKeyword, pcpKeyword);

    ilRc = iGetConfigEntry(cgConfigFile, clSection, clKeyword,
            CFG_STRING, pcpCfgBuffer);
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "Not found in %s: <%s> <%s>", cgConfigFile, clSection, clKeyword);
    }
    else {
        dbg(DEBUG, "Config Entry <%s>,<%s>:<%s> found in %s",
                clSection, clKeyword, pcpCfgBuffer, cgConfigFile);
    }/* end of if */
    return ilRc;
}

static int FindItemInArrayList(char *pcpInput, char *pcpPattern, char cpDel, int *ipLine, int *ipCol, int *ipPos) {
    int ilRc = RC_SUCCESS;

    ilRc = FindItemInList(pcpInput, pcpPattern, cpDel, ipLine, ipCol, ipPos);
    *ipLine = (*ipLine) - 1;

    return ilRc;
}

static int InitVdgFieldIndex() {
    int ilRc = RC_SUCCESS;
    int ilCol, ilPos;

    FindItemInArrayList(cgAftFields, "URNO", ',', &igAftURNO, &ilCol, &ilPos);
    FindItemInArrayList(cgAftFields, "ADID", ',', &igAftADID, &ilCol, &ilPos);
    FindItemInArrayList(cgAftFields, "TIFA", ',', &igAftTIFA, &ilCol, &ilPos);
    FindItemInArrayList(cgAftFields, "TIFD", ',', &igAftTIFD, &ilCol, &ilPos); /* Added by OTR */
    FindItemInArrayList(cgAftFields, "STOA", ',', &igAftSTOA, &ilCol, &ilPos);
    FindItemInArrayList(cgAftFields, "STOD", ',', &igAftSTOD, &ilCol, &ilPos);
    FindItemInArrayList(cgAftFields, "FLNO", ',', &igAftFLNO, &ilCol, &ilPos);

    if (cgTMOField[0] != '\0') {
        FindItemInArrayList(cgAftFields, cgTMOField, ',', &igTMOFld, &ilCol, &ilPos);
    }

    if (igMode == TRUE) {
        dbg(DEBUG, "line<%d>----------------------", __LINE__);
        dbg(DEBUG, "line<%d>igAftFLNO=<%d>", __LINE__, igAftFLNO);
        dbg(DEBUG, "line<%d>----------------------", __LINE__);
    }

    return ilRc;
}

static int GetDebugLevel(char *pcpMode, int *pipMode) {
    int ilRc = RC_SUCCESS;
    char clCfgValue[64];

    ilRc = iGetConfigRow(cgConfigFile, "SYSTEM", pcpMode, CFG_STRING,
            clCfgValue);

    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "SYSTEM %s not found in <%s>", pcpMode, cgConfigFile);
    } else {
        dbg(TRACE, "SYSTEM %s <%s>", pcpMode, clCfgValue);
        if (!strcmp(clCfgValue, "DEBUG"))
            *pipMode = DEBUG;
        else if (!strcmp(clCfgValue, "TRACE"))
            *pipMode = TRACE;
        else
            *pipMode = 0;
    }

    return ilRc;
}

static int InitForPortal(void) {
    int ilRc = RC_SUCCESS;
    char *pclFunc = "InitForPortal";

    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    if (ReadConfigEntry("BLR_PORTAL", "SENDTO", cgCfgBuffer) == RC_SUCCESS) {
        igSendToModIdForPortal = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: SENDTO set to %d <%s>", pclFunc, igSendToModIdForPortal, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <SENDTO> not found set to default %d", pclFunc, igSendToModIdForPortal);
    }

    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    if (ReadConfigEntry("VDG", "SENDTO", cgCfgBuffer) == RC_SUCCESS) {
        igSendToModIdForVDGS = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: SENDTO set to %d <%s>", pclFunc, igSendToModIdForVDGS, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <SENDTO> not found set to default %d", pclFunc, igSendToModIdForVDGS);
    }

    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    /*---------------In-Arrival------------------- */
    if (ReadConfigEntry("BLR_PORTAL", "FROM_IN", cgCfgBuffer) == RC_SUCCESS) {
        igFrom_In = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: FROM_IN set to %d <%s>", pclFunc, igFrom_In, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <FROM_IN> not found set to default %d", pclFunc, igFrom_In);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    if (ReadConfigEntry("BLR_PORTAL", "TO_IN", cgCfgBuffer) == RC_SUCCESS) {
        igTo_In = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: TO_IN set to %d <%s>", pclFunc, igTo_In, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <TO_IN> not found set to default %d", pclFunc, igTo_In);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));

    memset(pcgFtyp_In, 0, sizeof (pcgFtyp_In));
    ilRc = ReadConfigEntry("BLR_PORTAL", "FTYP_IN", pcgFtyp_In);
    if (ilRc != RC_SUCCESS || strlen(pcgFtyp_In) <= 0)
        strcpy(pcgFtyp_In, "'T','G'");
    dbg(TRACE, "%s: FTYP_IN = <%s>", pclFunc, pcgFtyp_In);

    if (ReadConfigEntry("BLR_PORTAL", "FIELD_IN", cgField_In) == RC_SUCCESS) {
        dbg(TRACE, "%s: FIELD_IN set to <%s>", pclFunc, cgField_In);
    } else {
        strcpy(cgField_In, AFTFIELDS);
        /*dbg ( TRACE, "%s: Entry <FIELD_IN> not found set to default %s",cgField_In);   */
        dbg(TRACE, "%s: Entry <FIELD_IN> not found set to default %s", pclFunc, cgField_In);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    ilRc = ReadConfigEntry("BLR_PORTAL", "COMMAND_IN", cgCfgBuffer);
    if (ilRc == RC_SUCCESS) {
        strcpy(cgCommand_In, cgCfgBuffer);
        dbg(TRACE, "%s: COMMAND_IN set to <%s>", pclFunc, cgCommand_In);
    } else {
        dbg(TRACE, "%s: COMMAND_IN not found set to default <%s>", pclFunc, cgCommand_In);
    }

    /*---------------Out-Departure------------------- */
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    if (ReadConfigEntry("BLR_PORTAL", "FROM_OUT", cgCfgBuffer) == RC_SUCCESS) {
        igFrom_Out = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: FROM_OUT set to %d <%s>", pclFunc, igFrom_Out, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <FROM_OUT> not found set to default %d", pclFunc, igFrom_Out);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    if (ReadConfigEntry("BLR_PORTAL", "TO_OUT", cgCfgBuffer) == RC_SUCCESS) {
        igTo_Out = atoi(cgCfgBuffer);
        dbg(TRACE, "%s: TO_OUT set to %d <%s>", pclFunc, igTo_Out, cgCfgBuffer);
    } else {
        dbg(TRACE, "%s: Entry <TO_OUT> not found set to default %d", pclFunc, igTo_Out);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));

    memset(pcgFtyp_Out, 0, sizeof (pcgFtyp_Out));
    ilRc = ReadConfigEntry("BLR_PORTAL", "FTYP_OUT", pcgFtyp_Out);
    if (ilRc != RC_SUCCESS || strlen(pcgFtyp_Out) <= 0)
        strcpy(pcgFtyp_Out, "'T','G'");
    dbg(TRACE, "%s: FTYP_OUT = <%s>", pclFunc, pcgFtyp_Out);

    if (ReadConfigEntry("BLR_PORTAL", "FIELD_OUT", cgField_Out) == RC_SUCCESS) {
        dbg(TRACE, "%s: FIELD_IN set to <%s>", pclFunc, cgField_Out);
    } else {
        strcpy(cgField_Out, AFTFIELDS);
        dbg(TRACE, "%s: Entry <FIELD_IN> not found set to default %s", pclFunc, cgField_Out);
    }
    memset(cgCfgBuffer, 0, sizeof (cgCfgBuffer));
    ilRc = ReadConfigEntry("BLR_PORTAL", "COMMAND_OUT", cgCfgBuffer);
    if (ilRc == RC_SUCCESS) {
        strcpy(cgCommand_Out, cgCfgBuffer);
        dbg(TRACE, "%s: COMMAND_OUT set to <%s>", pclFunc, cgCommand_Out);
    } else {
        dbg(TRACE, "%s: COMMAND_OUT not found set to default <%s>", pclFunc, cgCommand_Out);
    }

    igURNO_In = get_item_no(cgField_In, "URNO", 5);
    igURNO_Out = get_item_no(cgField_Out, "URNO", 5);
    dbg(TRACE, "%s: igURNO_In <%d> igURNO_Out <%d> ", pclFunc, igURNO_In, igURNO_Out);
    igNumFields_In = get_no_of_items(cgField_In);
    igNumFields_Out = get_no_of_items(cgField_Out);
    dbg(TRACE, "%s: igInNumFields <%d> igOutNumFields <%d> ", pclFunc, igInNumFields, igOutNumFields);

    return ilRc;
}

static int Init_Pdehdl() {
    int ilRc = RC_SUCCESS; /* Return code */
    int ilRc2;
    char clSection[64] = "\0";
    char clKeyword[64] = "\0";
    int ilOldDebugLevel = 0;
    char clBreak[24] = "\0";
    char clNow[64];
    /*time_t tlNow;*/
    int ilRc1;
    long pclAddFieldLens[12];
    char pclAddFields[256];
    char pclSqlBuf[2560];
    char pclSelection[1024];
    short slCursor;
    short slSqlFunc;
    char pclTmpBuf[128];
    char *pclTmpPtr;

    char *pclFunc = "Init_Pdehdl";

	#if 0
    if (ilRc == RC_SUCCESS) {
        /* read HomeAirPort from SGS.TAB */
        ilRc = tool_search_exco_data("SYS", "HOMEAP", cgHopo);
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "<Init_Pdehdl> EXTAB,SYS,HOMEAP not found in SGS.TAB");
            Terminate(30);
        } else {
            dbg(TRACE, "<Init_Pdehdl> home airport <%s>", cgHopo);
        }
    }
	#endif

	setHomeAirport(cgProcessName, cgHopo_sgstab);
	dbg(TRACE,"%s : HOMEAP = <%s>",pclFunc,cgHopo_sgstab);

    if (ilRc == RC_SUCCESS) {

        ilRc = tool_search_exco_data("ALL", "TABEND", cgTabEnd);
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "<Init_Pdehdl> EXTAB,ALL,TABEND not found in SGS.TAB");
            Terminate(30);
        } else {
            dbg(TRACE, "<Init_Pdehdl> table extension <%s>", cgTabEnd);
        }
    }

    SetSignals(HandleSignal);

    if (ilRc == RC_SUCCESS) {
        GetDebugLevel("STARTUP_MODE", &igStartUpMode);
        GetDebugLevel("RUNTIME_MODE", &igRuntimeMode);
    }
    debug_level = igStartUpMode;

    GetServerTimeStamp("UTC", 1, 0, clNow);

    dbg(DEBUG, "Now = <%s>", clNow);

    /* now reading from configfile or from database */


#ifndef _LINUX
#if defined(_HPUX_SOURCE)  || defined(_AIX)
    dbg(TRACE, "Initial values daylight <%d> timezone <%ld>, zone0 <%s> zone1 <%s>",
            daylight, timezone, tzname[0], tzname[1]);
    daylight = 0;
#else
    dbg(TRACE, "Initial values _daylight <%d> _timezone <%ld>, zone0 <%s> zone1 <%s>",
            _daylight, _timezone, _tzname[0], _tzname[1]);
    _daylight = 0;
#endif
#endif

    pcgUrnoListIn = malloc(lgUrnoListLengthIn);
    if (pcgUrnoListIn == NULL) {
        dbg(TRACE, "unable to allocate %ld Bytes for UrnoListIn", lgUrnoListLengthIn);
        ilRc = RC_FAIL;
    } else {
        memset(pcgUrnoListIn, 0, lgUrnoListLengthIn);
    }
    pcgUrnoListOut = malloc(lgUrnoListLengthOut);
    if (pcgUrnoListOut == NULL) {
        dbg(TRACE, "unable to allocate %ld Bytes for UrnoListOut", lgUrnoListLengthOut);
        ilRc = RC_FAIL;
    } else {
        memset(pcgUrnoListOut, 0, lgUrnoListLengthOut);
    }

    if (ilRc == RC_SUCCESS) {
        ilRc = InitVdgFieldIndex();
    }
    if (ilRc == RC_SUCCESS) {
        if (ReadConfigEntry("VDG", "RANGEFROMINBOUND", cgCfgBuffer) == RC_SUCCESS) {
            igRangeFromInbound = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: RANGEFROMINBOUND set to %d <%s>", igRangeFromInbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <RANGEFROMINBOUND> not found set to default %d",
                    igRangeFromInbound);
        }
        if (ReadConfigEntry("VDG", "RANGEFROMOUTBOUND", cgCfgBuffer) == RC_SUCCESS) {
            igRangeFromOutbound = atoi(cgCfgBuffer);
            dbg(TRACE, "IinitVdghdl: RANGEFROMOUTBOUND set to %d <%s>", igRangeFromOutbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <RANGEFROMOUTBOUND> not found set to default %d",
                    igRangeFromOutbound);
        }
        if (ReadConfigEntry("VDG", "RANGETOINOUND", cgCfgBuffer) == RC_SUCCESS) {
            igRangeToInbound = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: RANGETOINOUND set to %d <%s>", igRangeToInbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <RANGETOINOUND> not found set to default %d",
                    igRangeToInbound);
        }
        if (ReadConfigEntry("VDG", "RANGETOOUTBOUND", cgCfgBuffer) == RC_SUCCESS) {
            igRangeToOutbound = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: RANGETOOUTBOUND set to %d <%s>", igRangeToOutbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <RANGETOOUTBOUND> not found set to default %d",
                    igRangeToOutbound);
        }
        if (ReadConfigEntry("VDG", "TIMEINBOUND", cgCfgBuffer) == RC_SUCCESS) {
            igTimeInbound = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: TIMEINBOUND set to %d <%s>", igTimeInbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <TIMEINBOUND> not found set to default %d",
                    igTimeInbound);
        }
        if (ReadConfigEntry("VDG", "TIMEOUTBOUND", cgCfgBuffer) == RC_SUCCESS) {
            igTimeOutbound = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: TIMEOUTBOUND set to %d <%s>", igTimeOutbound, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <TIMEOUTBOUND> not found set to default %d",
                    igTimeOutbound);
        }

        ilRc = ReadConfigEntry("VDG", "FIELDSINBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgFieldListIn, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: FIELDSINBOUND set to <%s>", cgFieldListIn);
        } else {
            dbg(TRACE, "InitVdghdl: FIELDSINBOUND not found set to default <%s>",
                    cgFieldListIn);
        }

        ilRc = ReadConfigEntry("VDG", "FIELDSOUTBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgFieldListOut, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: FIELDSOUTBOUND set to <%s>", cgFieldListOut);
        } else {
            dbg(TRACE, "InitVdghdl: FIELDSOUTBOUND not found set to default <%s>",
                    cgFieldListOut);
        }

        /* MEI v1.17 - capture URNO position */
        igInUaftIdx = get_item_no(cgFieldListIn, "URNO", 5);
        igOutUaftIdx = get_item_no(cgFieldListOut, "URNO", 5);
        dbg(TRACE, "Init_Pdehdl: igInUaftIdx <%d> igOutUaftIdx <%d> ", igInUaftIdx, igOutUaftIdx);
        igInNumFields = get_no_of_items(cgFieldListIn);
        igOutNumFields = get_no_of_items(cgFieldListOut);
        dbg(TRACE, "Init_Pdehdl: igInNumFields <%d> igOutNumFields <%d> ", igInNumFields, igOutNumFields);

        igUaftIdx = get_item_no(cgAftFields, "URNO", 5);
        dbg(TRACE, "Init_Pdehdl: igUaftIdx <%d>", igUaftIdx);

        ilRc = ReadConfigEntry("VDG", "TRIGGERFIELDSINBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgTriggerFieldListIn, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: TRIGGERFIELDSINBOUND set to <%s>", cgTriggerFieldListIn);
        } else {
            dbg(TRACE, "InitVdghdl: TRIGGERFIELDSINBOUND not found set to default <%s>",
                    cgTriggerFieldListIn);
        }

        ilRc = ReadConfigEntry("VDG", "TRIGGERFIELDSOUTBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgTriggerFieldListOut, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: TRIGGERFIELDSOUTBOUND set to <%s>", cgTriggerFieldListOut);
        } else {
            dbg(TRACE, "InitVdghdl: TRIGGERFIELDSOUTBOUND not found set to default <%s>",
                    cgTriggerFieldListOut);
        }

        if (ReadConfigEntry("VDG", "SENDTO", cgCfgBuffer) == RC_SUCCESS) {
            igSendToModId = atoi(cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: SENDTO set to %d <%s>", igSendToModId, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <SENDTO> not found set to default %d",
                    igSendToModId);
        }

        if (ReadConfigEntry("VDG", "RESEND_IF_TIFF_GT_NOW", cgCfgBuffer) == RC_SUCCESS) {
            strcpy(pcgResend, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: RESEND_IF_TIFF_GT_NOW set to <%s>", pcgResend);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <RESEND_IF_TIFF_GT_NOW> not found set to default <%s>",
                    pcgResend);
        }

        ilRc = ReadConfigEntry("VDG", "SENDCOMMANDINBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgVdgSendCommandInbound, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: SENDCOMMANDINBOUND set to <%s>", cgVdgSendCommandInbound);
        } else {
            dbg(TRACE, "InitVdghdl: SENDCOMMANDINBOUND not found set to default <%s>",
                    cgVdgSendCommandInbound);
        }
        ilRc = ReadConfigEntry("VDG", "SENDCOMMANDOUTBOUND", cgCfgBuffer);
        if (ilRc == RC_SUCCESS) {
            strcpy(cgVdgSendCommandOutbound, cgCfgBuffer);
            dbg(TRACE, "InitVdghdl: SENDCOMMANDOUTBOUND set to <%s>", cgVdgSendCommandOutbound);
        } else {
            dbg(TRACE, "InitVdghdl: SENDCOMMANDOUTBOUND not found set to default <%s>",
                    cgVdgSendCommandOutbound);
        }


        if (ReadConfigEntry("VDG", "AFTFIELDS", cgAftFields) == RC_SUCCESS) {
            dbg(TRACE, "InitVdghdl: AFTFIELDS set to <%s>", cgAftFields);
        } else {
            strcpy(cgAftFields, AFTFIELDS);
            dbg(TRACE, "InitVdghdl: Entry <AFTFIELDS> not found set to default %s",
                    cgAftFields);
        }

        if (ReadConfigEntry("VDG", "TMOBasedOn", cgTMOField) == RC_SUCCESS) {
            dbg(TRACE, "InitTMOhdl: TMOBasedOn set to <%s>", cgTMOField);
            strcat(cgAftFields, ",");
            strcat(cgAftFields, cgTMOField);
            strcat(cgFieldListIn, ",");
            strcat(cgFieldListIn, cgTMOField);
            igTMOIsATCBased = 1;
        } else {
            dbg(TRACE, "InitVdghdl: Entry <TMOBasedOn> not found, ignored");
        }

        if (ReadConfigEntry("VDG", "SetONBLAfterLAND", pclTmpBuf) == RC_SUCCESS) {
            igSetOnblAfterLand = atoi(pclTmpBuf);
            pclTmpPtr = strstr(pclTmpBuf, ",");
            if (pclTmpPtr != NULL) {
                pclTmpPtr++;
                strcpy(pcgOnblField, pclTmpPtr);
            }
            dbg(TRACE, "InitVdghdl: SetONBLAfterLAND set to <%d>/<%s>",
                    igSetOnblAfterLand, pcgOnblField);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <SetONBLAfterLAND> not found set to default <%d>/<%s>",
                    igSetOnblAfterLand, pcgOnblField);
        }
        if (ReadConfigEntry("VDG", "ModIdFlight", pclTmpBuf) == RC_SUCCESS) {
            igModIdFlight = atoi(pclTmpBuf);
            dbg(TRACE, "InitVdghdl: ModIdFlight set to <%d>", igModIdFlight);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <ModIdFlight> not found set to default <%d>",
                    igModIdFlight);
        }

        /* MEI v1.16 */
        pcgFtypInbound[0] = '\0';
        ilRc2 = ReadConfigEntry("VDG", "FTYPOFINBOUND", pcgFtypInbound);
        if (ilRc2 != RC_SUCCESS || strlen(pcgFtypInbound) <= 0)
            strcpy(pcgFtypInbound, "'T','G'");
        dbg(TRACE, "Init_Pdehdl: FTYPOFINBOUND = <%s>", pcgFtypInbound);

        pcgFtypOutbound[0] = '\0';
        ilRc2 = ReadConfigEntry("VDG", "FTYPOFOUTBOUND", pcgFtypOutbound);
        if (ilRc2 != RC_SUCCESS || strlen(pcgFtypOutbound) <= 0)
            strcpy(pcgFtypOutbound, "'T','G'");
        dbg(TRACE, "Init_Pdehdl: FTYPOFOUTBOUND = <%s>", pcgFtypOutbound);

        /* MEI v1.18 */
        cgIncludeReturn = FALSE;
        ilRc2 = ReadConfigEntry("VDG", "BATCH_INCLUDE_RETURN", pclTmpBuf);
        if (ilRc2 == RC_SUCCESS && !strcmp(pclTmpBuf, "YES"))
            cgIncludeReturn = TRUE;
        dbg(TRACE, "Init_Pdehdl: BATCH_INCLUDE_RETURN = <%d>", cgIncludeReturn);

        /* MEI v1.19 */
        igSpecialRangeFromInbound = 0;
        igSpecialRangeFromOutbound = 0;
        igSpecialRangeToInbound = 0;
        igSpecialRangeToOutbound = 0;
        ilRc2 = ReadConfigEntry("SPECIAL_BATCH", "RANGEFROMINBOUND", pclTmpBuf);
        if (ilRc2 == RC_SUCCESS)
            igSpecialRangeFromInbound = atoi(pclTmpBuf);
        dbg(TRACE, "Init_Pdehdl: [SPECIAL_BATCH] RANGEFROMINBOUND = <%d>", igSpecialRangeFromInbound);

        ilRc2 = ReadConfigEntry("SPECIAL_BATCH", "RANGEFROMOUTBOUND", pclTmpBuf);
        if (ilRc2 == RC_SUCCESS)
            igSpecialRangeFromOutbound = atoi(pclTmpBuf);
        dbg(TRACE, "Init_Pdehdl: [SPECIAL_BATCH] RANGEFROMOUTBOUND = <%d>", igSpecialRangeFromOutbound);

        ilRc2 = ReadConfigEntry("SPECIAL_BATCH", "RANGETOINBOUND", pclTmpBuf);
        if (ilRc2 == RC_SUCCESS)
            igSpecialRangeToInbound = atoi(pclTmpBuf);
        dbg(TRACE, "Init_Pdehdl: [SPECIAL_BATCH] RANGETOINBOUND = <%d>", igSpecialRangeToInbound);

        ilRc2 = ReadConfigEntry("SPECIAL_BATCH", "RANGETOOUTBOUND", pclTmpBuf);
        if (ilRc2 == RC_SUCCESS)
            igSpecialRangeToOutbound = atoi(pclTmpBuf);
        dbg(TRACE, "Init_Pdehdl: [SPECIAL_BATCH] RANGETOOUTBOUND = <%d>", igSpecialRangeToOutbound);


        /* Frank v1.16 Debug*/
        ilRc2 = ReadConfigEntry("SYSTEM", "MODE", pcgMode);
        if (ilRc2 != RC_SUCCESS || strlen(pcgMode) <= 0) {
            strcpy(pcgMode, "REAL");
            igMode = FALSE;
            dbg(TRACE, "Init_Pdehdl: igMode = <FALSE>");
        } else if (strcmp(pcgMode, "REAL") != 0) {
            igMode = TRUE;
            dbg(TRACE, "Init_Pdehdl: igMode = <TRUE>");
        } else {
            igMode = FALSE;
            dbg(TRACE, "Init_Pdehdl: igMode = <FALSE>");
        }
        dbg(TRACE, "Init_Pdehdl: MODE = <%s>", pcgMode);

        ilRc2 = ReadConfigEntry("SYSTEM", "TABLE_NAME", pcgTableName);
        if (ilRc2 != RC_SUCCESS || strlen(pcgTableName) <= 0) {
            strcpy(pcgTableName, "AFTTAB");
        }

        ilRc = CEDAArrayInitialize(20, 20);
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "InitVdghdl: CEDAArrayInitialize failed <%d>", ilRc);
        }
        if (ReadConfigEntry("VDG", "ATCINTERFACE", cgCfgBuffer) == RC_SUCCESS) {
            igForATC = atoi(cgCfgBuffer);
            dbg(TRACE, "IinitVdghdl: igForATC set to %d <%s>", igForATC, cgCfgBuffer);
        } else {
            dbg(TRACE, "InitVdghdl: Entry <ATCINTERFACE> not found set to default %d",
                    igForATC);
        }

        /* Check fieldlist for inbound flights */

        if (ilRc == RC_SUCCESS) {
            char clNow[24];
            char clFrom[24];
            char clTo[24];

            strcpy(pclAddFields, "SENT");
            pclAddFieldLens[0] = 2;
            pclAddFieldLens[1] = 0;

            GetServerTimeStamp("UTC", 1, 0, clNow);
            strcpy(clTo, clNow);
            strcpy(clFrom, clNow);
            AddSecondsToCEDATime(clFrom, igRangeFromInbound * 60, 1);
            AddSecondsToCEDATime(clTo, igRangeToInbound * 60, 1);

            /* MEI v1.16 */
            /*sprintf(pclSelection,
                      "WHERE TIFA BETWEEN '%s' AND '%s' AND (ADID = 'A' OR (ADID = 'B' AND FTYP IN ('T','G')))",
                      clFrom,clTo);*/

            /*FYA v1.26c UFIS-8297*/
            if (igMultiHopo == TRUE)
            {
                sprintf(pclSelection, "WHERE TIFA BETWEEN '%s' AND '%s' AND (ADID = 'A' OR "
                    "(ADID = 'B' AND FTYP IN (%s))) AND HOPO IN ('***','%s')", clFrom, clTo, pcgFtypInbound, cgHopo);
            }
            else/*FYA v1.26c UFIS-8297*/
            {
                sprintf(pclSelection, "WHERE TIFA BETWEEN '%s' AND '%s' AND (ADID = 'A' OR "
                    "(ADID = 'B' AND FTYP IN (%s)))", clFrom, clTo, pcgFtypInbound);
            }

            dbg(TRACE, "InitVdghdl: SetArrayInfo AFTTAB array");
            ilRc = SetArrayInfo("AFTTAB", "AFTTAB", cgAftFields,
                    pclAddFields, pclAddFieldLens, &rgAftArrayInbound, pclSelection, TRUE);
            if (ilRc != RC_SUCCESS) {
                dbg(TRACE, "InitVdghdl: SetArrayInfo AFTTAB failed <%d>", ilRc);
            } else {
                strcpy(rgAftArrayInbound.crIdx01Name, "TIFA");
                ilRc = CEDAArrayCreateSimpleIndexUp(&rgAftArrayInbound.rrArrayHandle,
                        rgAftArrayInbound.crArrayName,
                        &rgAftArrayInbound.rrIdx01Handle,
                        rgAftArrayInbound.crIdx01Name, "TIFA");
                if (ilRc != RC_SUCCESS) {
                    dbg(TRACE, "Index <%s> not created RC=%d", rgAftArrayInbound.crIdx01Name, ilRc);
                } else {
                    strcpy(rgAftArrayInbound.crIdx02Name, "URNO");
                    ilRc = CEDAArrayCreateSimpleIndexUp(&rgAftArrayInbound.rrArrayHandle,
                            rgAftArrayInbound.crArrayName,
                            &rgAftArrayInbound.rrIdx02Handle,
                            rgAftArrayInbound.crIdx02Name, "URNO");
                    if (ilRc != RC_SUCCESS) {
                        dbg(TRACE, "Index <%s> not created RC=%d", rgAftArrayInbound.crIdx02Name, ilRc);
                    }
                }
                if (ilRc == RC_SUCCESS) {
                    dbg(DEBUG, "InitVdghdl: SetArrayInfo AFTTAB OK");
                    dbg(DEBUG, "<%s> %d", rgAftArrayInbound.crArrayName, rgAftArrayInbound.rrArrayHandle);
                }
            }
        }
        if (ilRc == RC_SUCCESS) {
            char clNow[24];
            char clFrom[24];
            char clTo[24];

            strcpy(pclAddFields, "SENT");
            pclAddFieldLens[0] = 2;
            pclAddFieldLens[1] = 0;

            GetServerTimeStamp("UTC", 1, 0, clNow);
            strcpy(clTo, clNow);
            strcpy(clFrom, clNow);
            AddSecondsToCEDATime(clFrom, igRangeFromOutbound * 60, 1);
            AddSecondsToCEDATime(clTo, igRangeToOutbound * 60, 1);

            /* MEI v1.16 */
            /*sprintf(pclSelection,
                "WHERE TIFD BETWEEN '%s' AND '%s' AND (ADID = 'D' OR (ADID = 'B' AND FTYP IN ('T','G')))",
                        clFrom,clTo);*/

            /*FYA v1.26c UFIS-8297*/
            if (igMultiHopo == TRUE)
            {
                sprintf(pclSelection, "WHERE TIFD BETWEEN '%s' AND '%s' AND (ADID = 'D' OR "
                    "(ADID = 'B' AND FTYP IN (%s))) AND HOPO IN ('***','%s')", clFrom, clTo, pcgFtypOutbound, cgHopo);
            }
            else
            {
                sprintf(pclSelection, "WHERE TIFD BETWEEN '%s' AND '%s' AND (ADID = 'D' OR "
                    "(ADID = 'B' AND FTYP IN (%s)))", clFrom, clTo, pcgFtypOutbound);
            }


            dbg(TRACE, "InitVdghdl: SetArrayInfo AFTTAB array");
            ilRc = SetArrayInfo("AFTOUT", "AFTTAB", cgAftFields,
                    pclAddFields, pclAddFieldLens, &rgAftArrayOutbound, pclSelection, TRUE);
            if (ilRc != RC_SUCCESS) {
                dbg(TRACE, "InitVdghdl: SetArrayInfo AFTTAB failed <%d>", ilRc);
            } else {
                strcpy(rgAftArrayOutbound.crIdx01Name, "TIFD");
                ilRc = CEDAArrayCreateSimpleIndexUp(&rgAftArrayOutbound.rrArrayHandle,
                        rgAftArrayOutbound.crArrayName,
                        &rgAftArrayOutbound.rrIdx01Handle,
                        rgAftArrayOutbound.crIdx01Name, "TIFD");
                if (ilRc != RC_SUCCESS) {
                    dbg(TRACE, "Index <%s> not created RC=%d", rgAftArrayOutbound.crIdx01Name, ilRc);
                } else {
                    strcpy(rgAftArrayOutbound.crIdx02Name, "URNO");
                    ilRc = CEDAArrayCreateSimpleIndexUp(&rgAftArrayOutbound.rrArrayHandle,
                            rgAftArrayOutbound.crArrayName,
                            &rgAftArrayOutbound.rrIdx02Handle,
                            rgAftArrayOutbound.crIdx02Name, "URNO");
                    if (ilRc != RC_SUCCESS) {
                        dbg(TRACE, "Index <%s> not created RC=%d", rgAftArrayOutbound.crIdx02Name, ilRc);
                    }
                }
                if (ilRc == RC_SUCCESS) {
                    dbg(DEBUG, "InitVdghdl: SetArrayInfo AFTTAB OK");
                    dbg(DEBUG, "<%s> %d", rgAftArrayOutbound.crArrayName, rgAftArrayOutbound.rrArrayHandle);
                }
            }
        }
    }

    if (ilRc == RC_SUCCESS) {
        ilRc = CEDAArrayReadAllHeader(outp);
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "InitVdghdl: CEDAArrayReadAllHeader failed <%d>", ilRc);
        } else {
            dbg(TRACE, "InitVdghdl: CEDAArrayReadAllHeader OK");
        }/* end of if */
    }/* end of if */


    if (ilRc == RC_SUCCESS) {
        ilRc = InitVdgFieldIndex();
    }
    if (ilRc == RC_SUCCESS) {
        dbg(TRACE, "InitVdghdl: InitVdgFieldIndex OK");
    }/* end of if */
    else {
        dbg(TRACE, "InitVdghdl: InitVdgFieldIndex failed");
    }

    /* AKL Dynamic Action Configuration not necessary
        ilRc = ConfigureAction("AFTTAB", cgAftFields, "IFR,UFR,DFR");
        if(ilRc != RC_SUCCESS)
        {
            dbg(TRACE,"InitVdghdl: ConfigureAction <AFTTAB> failed <%d>",ilRc);
        }
     */

    ilRc = GetPrflConfig();

    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "Init_Pdehdl failed");
    }

    ilRc = InitForPortal();
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "Init_Pdehdl failed");
    }

    return (ilRc);

} /* end of initialize */




/******************************************************************************/
/* The Reset routine                                                          */

/******************************************************************************/
static int Reset() {
    int ilRc = RC_SUCCESS; /* Return code */

    dbg(TRACE, "Reset: now resetting");

    return ilRc;

} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */

/******************************************************************************/
static void Terminate(int ipSleep) {
    /* unset SIGCHLD ! DB-Child will terminate ! */

	if (pcgUrnoListIn != NULL)
    {
        free(pcgUrnoListIn);
    }
    if (pcgUrnoListOut != NULL)
    {
        free(pcgUrnoListOut);
    }

    dbg(TRACE, "Terminate: now DB logoff ...");

    signal(SIGCHLD, SIG_IGN);

    logoff();

    dbg(TRACE, "Terminate: now sleep(%d) ...", ipSleep);


    dbg(TRACE, "Terminate: now leaving ...");

    fclose(outp);

    sleep(ipSleep < 1 ? 1 : ipSleep);

    exit(0);

} /* end of Terminate */

/******************************************************************************/
/* The handle signals routine                                                 */

/******************************************************************************/

static void HandleSignal(int pipSig) {
    dbg(TRACE, "HandleSignal: signal <%d> received", pipSig);

    switch (pipSig) {
        default:
            Terminate(1);
            break;
    } /* end of switch */

    exit(1);

} /* end of HandleSignal */

/******************************************************************************/
/* The handle general error routine                                           */

/******************************************************************************/
static void HandleErr(int pipErr) {
    dbg(TRACE, "HandleErr: <%d> : unknown ERROR", pipErr);

    return;
} /* end of HandleErr */

/******************************************************************************/
/* The handle queuing error routine                                           */

/******************************************************************************/
static void HandleQueErr(int pipErr) {
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
    int ilRc = RC_SUCCESS; /* Return code */
    int ilBreakOut = FALSE;

    do {
        ilRc = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, igItemLen, (char *) &prgItem);
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = (EVENT *) prgItem->text;

        if (ilRc == RC_SUCCESS) {
            /* Acknowledge the item */
            ilRc = que(QUE_ACK, 0, mod_id, 0, 0, NULL);
            if (ilRc != RC_SUCCESS) {
                /* handle que_ack error */
                HandleQueErr(ilRc);
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
                    Terminate(1);
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
                    Terminate(1);
                    break;

                case RESET:
                    ilRc = Reset();
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
            HandleQueErr(ilRc);
        } /* end else */
    } while (ilBreakOut == FALSE);

    ilRc = Init_Pdehdl();
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "InitDemhdl: init failed!");
    }


} /* end of HandleQueues */

static int GetFieldLength(char *pcpHopo, char *pcpTana, char *pcpFina,
        long *plpLen) {
    int ilRc = RC_SUCCESS; /* Return code */
    int ilCnt = 0;
    char clTaFi[32];
    char clFele[16];
    char clFldLst[16];

    ilCnt = 1;
    sprintf(&clTaFi[0], "%s,%s", pcpTana, pcpFina);
    sprintf(&clFldLst[0], "TANA,FINA"); /* wird von syslib zerst��rt */

    /*dbg(TRACE,"GetFieldLength Tana <%s> Fina <%s>",pcpTana,pcpFina);*/

    ilRc = syslibSearchSystabData(&cgTabEnd[0], &clFldLst[0], &clTaFi[0], "FELE", &clFele[0], &ilCnt, "");
    switch (ilRc) {
        case RC_SUCCESS:
            *plpLen = atoi(&clFele[0]);
            /* dbg(DEBUG,"GetFieldLength: <%s,%s,%s> <%d>",pcpHopo,pcpTana,pcpFina,*plpLen);*/
            break;

        case RC_NOT_FOUND:
            dbg(TRACE, "GetFieldLength: syslibSSD returned <%d> RC_NOT_FOUND <%s>", ilRc, &clTaFi[0]);
            break;

        case RC_FAIL:
            dbg(TRACE, "GetFieldLength: syslibSSD returned <%d> RC_FAIL", ilRc);
            break;

        default:
            dbg(TRACE, "GetFieldLength: syslibSSD returned <%d> unexpected", ilRc);
            break;
    }/* end of switch */

    return (ilRc);

}

static int GetRowLength(char *pcpHopo, char *pcpTana, char *pcpFieldList,
        long *plpLen) {
    int ilRc = RC_SUCCESS; /* Return code */
    int ilNoOfItems = 0;
    int ilLoop = 0;
    long llFldLen = 0;
    long llRowLen = 0;
    char clFina[8];

    ilNoOfItems = get_no_of_items(pcpFieldList);

    ilLoop = 1;
    do {
        ilRc = get_real_item(&clFina[0], pcpFieldList, ilLoop);
        if (ilRc > 0) {
            ilRc = GetFieldLength(pcpHopo, pcpTana, &clFina[0], &llFldLen);
            if (ilRc == RC_SUCCESS) {
                llRowLen++;
                llRowLen += llFldLen;
            }/* end of if */
        }/* end of if */
        ilLoop++;
    } while ((ilRc == RC_SUCCESS) && (ilLoop <= ilNoOfItems));

    if (ilRc == RC_SUCCESS) {
        *plpLen = llRowLen;
    }/* end of if */

    return (ilRc);

}

static int SetArrayInfo(char *pcpArrayName, char *pcpTableName, char *pcpArrayFieldList, char *pcpAddFields, long *pcpAddFieldLens, VDGARRAYINFO *prpArrayInfo,
        char *pcpSelection, BOOL bpFill) {
    int ilRc = RC_SUCCESS; /* Return code */

    if (prpArrayInfo == NULL) {
        dbg(TRACE, "SetArrayInfo: invalid last parameter : null pointer not valid");
        ilRc = RC_FAIL;
    } else {
        memset(prpArrayInfo, 0x00, sizeof (prpArrayInfo));

        prpArrayInfo->rrArrayHandle = -1;
        prpArrayInfo->rrIdx01Handle = -1;

        if (pcpArrayName != NULL) {
            if (strlen(pcpArrayName) <= ARR_NAME_LEN) {
                strcpy(prpArrayInfo->crArrayName, pcpArrayName);
            }/* end of if */
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        prpArrayInfo->lrArrayFieldCnt = get_no_of_items(pcpArrayFieldList);
        if (pcpAddFields != NULL) {
            prpArrayInfo->lrArrayFieldCnt += get_no_of_items(pcpAddFields);
        }

        prpArrayInfo->plrArrayFieldOfs = (long *) calloc(prpArrayInfo->lrArrayFieldCnt, sizeof (long));
        if (prpArrayInfo->plrArrayFieldOfs == NULL) {
            dbg(TRACE, "SetArrayInfo: plrArrayFieldOfs calloc(%d,%d) failed", prpArrayInfo->lrArrayFieldCnt, sizeof (long));
            ilRc = RC_FAIL;
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        prpArrayInfo->plrArrayFieldLen = (long *) calloc(prpArrayInfo->lrArrayFieldCnt, sizeof (long));
        if (prpArrayInfo->plrArrayFieldLen == NULL) {
            dbg(TRACE, "SetArrayInfo: plrArrayFieldLen calloc(%d,%d) failed", prpArrayInfo->lrArrayFieldCnt, sizeof (long));
            ilRc = RC_FAIL;
        } else {
            *(prpArrayInfo->plrArrayFieldLen) = -1;
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        ilRc = GetRowLength(&cgHopo[0], pcpTableName, pcpArrayFieldList,
                &(prpArrayInfo->lrArrayRowLen));
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "SetArrayInfo: GetRowLength failed <%s> <%s>", pcpArrayName, pcpArrayFieldList);
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        prpArrayInfo->lrArrayRowLen += 100;
        prpArrayInfo->pcrArrayRowBuf = (char *) calloc(1, (prpArrayInfo->lrArrayRowLen + 10));
        if (prpArrayInfo->pcrArrayRowBuf == NULL) {
            ilRc = RC_FAIL;
            dbg(TRACE, "SetArrayInfo: calloc failed");
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        prpArrayInfo->pcrIdx01RowBuf = (char *) calloc(1, (prpArrayInfo->lrIdx01RowLen + 1));
        if (prpArrayInfo->pcrIdx01RowBuf == NULL) {
            ilRc = RC_FAIL;
            dbg(TRACE, "SetArrayInfo: calloc failed");
        }/* end of if */
    }/* end of if */

    if (ilRc == RC_SUCCESS) {
        dbg(TRACE, "CEDAArrayCreate follows Array <%s> Table <%s>", pcpArrayName, pcpTableName);
        ilRc = CEDAArrayCreate(&(prpArrayInfo->rrArrayHandle),
                pcpArrayName, pcpTableName, pcpSelection, pcpAddFields,
                pcpAddFieldLens, pcpArrayFieldList,
                &(prpArrayInfo->plrArrayFieldLen[0]),
                &(prpArrayInfo->plrArrayFieldOfs[0]));

        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "SetArrayInfo: CEDAArrayInitialize failed <%d>", ilRc);
        }/* end of if */
    }/* end of if */

    if (pcpArrayFieldList != NULL) {
        if (strlen(pcpArrayFieldList) <= ARR_FLDLST_LEN) {
            strcpy(prpArrayInfo->crArrayFieldList, pcpArrayFieldList);
        }/* end of if */
    }/* end of if */
    if (pcpAddFields != NULL) {
        if (strlen(pcpAddFields) +
                strlen(prpArrayInfo->crArrayFieldList) <= (size_t) ARR_FLDLST_LEN) {
            strcat(prpArrayInfo->crArrayFieldList, ",");
            strcat(prpArrayInfo->crArrayFieldList, pcpAddFields);
            dbg(TRACE, "<%s>", prpArrayInfo->crArrayFieldList);
        }/* end of if */
    }/* end of if */


    if (ilRc == RC_SUCCESS && bpFill) {
        ilRc = CEDAArrayFill(&(prpArrayInfo->rrArrayHandle), pcpArrayName, NULL);
        if (ilRc != RC_SUCCESS) {
            dbg(TRACE, "SetArrayInfo: CEDAArrayFill failed <%d>", ilRc);
        }/* end of if */
    }/* end of if */

    return (ilRc);
}

static int ReloadArrays() {
    int ilRc = RC_SUCCESS;
    char clNow[24];
    char clTo[24];
    char clFrom[24];
    char clSelection[512];

    GetServerTimeStamp("UTC", 1, 0, clNow);
    /*** Arrival flights **/
    strcpy(clTo, clNow);
    strcpy(clFrom, clNow);
    AddSecondsToCEDATime(clFrom, igRangeFromInbound * 60, 1);
    AddSecondsToCEDATime(clTo, igRangeToInbound * 60, 1);

    /* MEI v1.16 */
    /*sprintf(clSelection,
            "WHERE TIFA BETWEEN '%s' AND '%s' AND (ADID = 'A' OR (ADID = 'B' AND FTYP IN ('T','G')))",
                    clFrom,clTo);*/

    /*FYA v1.26c UFIS-8297*/
    if (igMultiHopo == TRUE)
    {
        sprintf(clSelection, "WHERE TIFA BETWEEN '%s' AND '%s' AND "
            "(ADID = 'A' OR (ADID = 'B' AND FTYP IN (%s))) AND HOPO IN ('***','%s')", clFrom, clTo, pcgFtypInbound, cgHopo);
    }
    else
    {
        sprintf(clSelection, "WHERE TIFA BETWEEN '%s' AND '%s' AND "
            "(ADID = 'A' OR (ADID = 'B' AND FTYP IN (%s)))", clFrom, clTo, pcgFtypInbound);
    }

    CEDAArrayRefillHint(&rgAftArrayInbound.rrArrayHandle,
            rgAftArrayInbound.crArrayName, clSelection, "0", ARR_FIRST, "");

    /*** departure flights **/
    strcpy(clTo, clNow);
    strcpy(clFrom, clNow);
    AddSecondsToCEDATime(clFrom, igRangeFromOutbound * 60, 1);
    AddSecondsToCEDATime(clTo, igRangeToOutbound * 60, 1);
    /* MEI v1.16 */
    /*sprintf(clSelection,
            "WHERE TIFD BETWEEN '%s' AND '%s' AND (ADID = 'D' OR (ADID = 'B' AND FTYP IN ('T','G')))",
                    clFrom,clTo);*/

    /*FYA v1.26c UFIS-8297*/
    if (igMultiHopo == TRUE)
    {
        sprintf(clSelection, "WHERE TIFD BETWEEN '%s' AND '%s' AND "
            "(ADID = 'D' OR (ADID = 'B' AND FTYP IN (%s))) AND HOPO IN ('***','%s')", clFrom, clTo, pcgFtypOutbound, cgHopo);
    }
    else
    {
        sprintf(clSelection, "WHERE TIFD BETWEEN '%s' AND '%s' AND "
            "(ADID = 'D' OR (ADID = 'B' AND FTYP IN (%s)))", clFrom, clTo, pcgFtypOutbound);
    }

    CEDAArrayRefillHint(&rgAftArrayOutbound.rrArrayHandle,
            rgAftArrayOutbound.crArrayName, clSelection, "0", ARR_FIRST, "");

    return ilRc;
}

static int AddToUrnoList(char *pcpUrno, char *pcpList, long lpListLength, long *plpActualLength) {
    int ilRc = RC_SUCCESS;

    int ilNewLen;

    ilNewLen = strlen(pcpUrno) + 3;

    dbg(DEBUG, "line<%d> the begin of AddToUrnoList()", __LINE__);

    if (((*plpActualLength) + ilNewLen) > (lpListLength - 50)) {
        char *pclTmp;
        pclTmp = realloc(pcpList, lpListLength + 10000);
        if (pclTmp != NULL) {
            pcpList = pclTmp;
            lpListLength += 10000;
            if (*plpActualLength > 100000) {
                memmove(pcpList, pcpList + 10000, 10000);
                plpActualLength -= 10000;
            }
        } else {
            dbg(TRACE, "unable to reallocate %ld bytes for Urnolist",
                    lpListLength + 50000);
            Terminate(30);
        }
    }

    if (*pcpList != '\0') {
        sprintf(&pcpList[*plpActualLength], ",#%s#", pcpUrno);
        *plpActualLength += ilNewLen;
    } else {
        sprintf(&pcpList[*plpActualLength], "#%s#", pcpUrno);
        *plpActualLength += ilNewLen - 1;
    }

    dbg(DEBUG, "line<%d> the end of AddToUrnoList()", __LINE__);

    return ilRc;
}

static void StringTrimRight(char *pcpBuffer) {
    if (*pcpBuffer != '\0') {
        char *pclBlank = &pcpBuffer[strlen(pcpBuffer) - 1];
        while (isspace(*pclBlank) && pclBlank != pcpBuffer) {
            *pclBlank = '\0';
            pclBlank--;
        }
    }
}

static int FindTriggerField(char *pcpFields, char *pcpData) {
    int ilRc = RC_NOTFOUND;
    int ilLc, ilFieldCount;
    char clFina[12];
    char *pclTriggerFieldsIn = NULL;
    char *pclTriggerFieldsOut = NULL;
    int ilAdid;
    char clAdid[12];

    ilAdid = get_item_no(pcpFields, "ADID", 5) + 1;
    if (ilAdid < 1) {
        dbg(TRACE, "No ADID in fieldlist ");
        ilRc = RC_NOTFOUND;
    } else {
        get_real_item(clAdid, pcpData, ilAdid);
        switch (*clAdid) {
            case 'A':
                pclTriggerFieldsIn = cgTriggerFieldListIn;
                break;
            case 'D':
                pclTriggerFieldsOut = cgTriggerFieldListOut;
                break;
            case 'B':
                pclTriggerFieldsIn = cgTriggerFieldListIn;
                pclTriggerFieldsOut = cgTriggerFieldListOut;
                break;
            default:
                dbg(TRACE, "Invalid ADID <%s>", clAdid);
                break;
        }

        if (pclTriggerFieldsIn != NULL) {
            ilFieldCount = get_no_of_items(pclTriggerFieldsIn);
            for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                ilRc = get_real_item(clFina, pclTriggerFieldsIn, ilLc);
                if (strstr(pcpFields, clFina) != NULL) {
                    ilRc = RC_SUCCESS;
                    break;
                }
            }
        }
        if (pclTriggerFieldsOut != NULL) {
            ilFieldCount = get_no_of_items(pclTriggerFieldsOut);
            for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                ilRc = get_real_item(clFina, pclTriggerFieldsOut, ilLc);
                if (strstr(pcpFields, clFina) != NULL) {
                    ilRc = RC_SUCCESS;
                    break;
                }
            }
        }
    }
    return ilRc;
}

static int SendSingleFlight(char *pcpUrnoList) {
    int ilRc = RC_SUCCESS;
    long llRowNum;
    static long llSent = -1;
    char clNow[24] = "\0";
    char *pclRowBuf;
    int ilLc, ilFieldCount;
    char clTmpFieldBuf[1024];

    /*FYA v1.26d UFIS-8325
    char clFlightUrno[24];*/
    char clFlightUrno[URNO_LEN];

    int ilUrnoCount;
    int ilUrnoLc;
    GetServerTimeStamp("UTC", 1, 0, clNow);
    ilUrnoCount = get_no_of_items(pcpUrnoList);
    for (ilUrnoLc = 1; ilUrnoLc <= ilUrnoCount; ilUrnoLc++) {
        long llUrnoNo = -1;
        llRowNum = ARR_FIRST;
        ilRc = get_real_item(clFlightUrno, pcpUrnoList, ilUrnoLc);
        if (CEDAArrayFindRowPointer(&rgAftArrayInbound.rrArrayHandle,
                rgAftArrayInbound.crArrayName,
                &rgAftArrayInbound.rrIdx02Handle,
                rgAftArrayInbound.crIdx02Name, clFlightUrno,
                &llRowNum, (void*) &pclRowBuf) == RC_SUCCESS) {
            long llFieldNo = -1;

            /*FYA v1.26d UFIS-8325
            char clUrno[24] = "\0";*/
            char clUrno[URNO_LEN] = "\0";

            static long llUrno = -1;
            char clAftData[2048] = "";
            char clFina[12] = "\0" ;
            char clSelection[248] = "\0";


            CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                    &llUrno, "URNO", 12, ARR_CURRENT, clUrno);
            StringTrimRight(clUrno);
            dbg(TRACE, "URNO is <%s>", clUrno);

            AddToUrnoList(clUrno, pcgUrnoListIn, lgUrnoListLengthIn, &lgUrnoListActualLengthIn);
            CEDAArrayPutField(&rgAftArrayInbound.rrArrayHandle,
                    rgAftArrayInbound.crArrayName, &llSent, "SENT", ARR_CURRENT, "1");
            dbg(DEBUG, "flight <%s> <%s> will be send to vdgs at %s",
                    AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), clNow);
            ilFieldCount = get_no_of_items(cgFieldListIn);
            for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                llFieldNo = -1;
                ilRc = get_real_item(&clFina[0], cgFieldListIn, ilLc);
                CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                 &llFieldNo , clFina ,1026, ARR_CURRENT,clTmpFieldBuf); /* v1.25 increasing "1023" to "1026" */
				StringTrimRight(clTmpFieldBuf); /* v1.25 */
                dbg(DEBUG, "Field: <%s> data <%s>", clFina, clTmpFieldBuf);
                if (*clAftData != 0) {
                    strcat(clAftData, ",");
                }
                strcat(clAftData, clTmpFieldBuf);
            }
            /** build where clause **/
            sprintf(clSelection, "WHERE URNO=%s\n%s", clUrno,clUrno);

            dbg(TRACE, "%05d: Selection=<%s>", __LINE__, clSelection);

			/*FYA v1.26c UFIS-8297*/
            /*tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "",
                    "", "", "", "",
                    cgVdgSendCommandInbound, "AFTTAB", clSelection, cgFieldListIn, clAftData, 0);*/
			tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "",
                    "", "", "", pcgTwEnd,
                    cgVdgSendCommandInbound, "AFTTAB", clSelection, cgFieldListIn, clAftData, 0);
        } else {
            long llUrnoNo = -1;
            llRowNum = ARR_FIRST;
            ilRc = get_real_item(clFlightUrno, pcpUrnoList, ilUrnoLc);
            if (CEDAArrayFindRowPointer(&rgAftArrayOutbound.rrArrayHandle,
                    rgAftArrayOutbound.crArrayName,
                    &rgAftArrayOutbound.rrIdx02Handle,
                    rgAftArrayOutbound.crIdx02Name, clFlightUrno,
                    &llRowNum, (void*) &pclRowBuf) == RC_SUCCESS) {
                long llFieldNo = -1;

                /*FYA v1.26d UFIS-8325
                char clUrno[24];*/
                char clUrno[URNO_LEN] = "\0";

                static long llUrno = -1;
                char clAftData[2048] = "\0";
                char clFina[12] = "\0";
                char clSelection[248] = "\0";


                CEDAArrayGetField(&rgAftArrayOutbound.rrArrayHandle, rgAftArrayOutbound.crArrayName,
                        &llUrno, "URNO", 12, ARR_CURRENT, clUrno);
                StringTrimRight(clUrno);
                dbg(TRACE, "URNO is <%s>", clUrno);

                AddToUrnoList(clUrno, pcgUrnoListOut, lgUrnoListLengthOut, &lgUrnoListActualLengthOut);
                CEDAArrayPutField(&rgAftArrayOutbound.rrArrayHandle,
                        rgAftArrayOutbound.crArrayName, &llSent, "SENT", ARR_CURRENT, "1");
                dbg(DEBUG, "flight <%s> <%s> will be send to vdgs at %s",
                        AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), clNow);
                ilFieldCount = get_no_of_items(cgFieldListIn);
                for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                    llFieldNo = -1;
                    ilRc = get_real_item(&clFina[0], cgFieldListIn, ilLc);
                    CEDAArrayGetField(&rgAftArrayOutbound.rrArrayHandle, rgAftArrayOutbound.crArrayName,
                 &llFieldNo , clFina ,1026, ARR_CURRENT,clTmpFieldBuf); /* v1.25 increasing "1023" to "1026" */
				StringTrimRight(clTmpFieldBuf); /* v1.25  */
                    dbg(DEBUG, "Field: <%s> data <%s>", clFina, clTmpFieldBuf);
                    if (*clAftData != 0) {
                        strcat(clAftData, ",");
                    }
                    strcat(clAftData, clTmpFieldBuf);
                }
                /** build where clause **/
                sprintf(clSelection, "WHERE URNO=%s\n%s", clUrno,clUrno);
                dbg(TRACE, "%05d: Selection=<%s>", __LINE__, clSelection);

				/*FYA v1.26c UFIS-8297*/
                /*tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "",
                        "", "", "", "",
                        cgVdgSendCommandOutbound, "AFTTAB", clSelection, cgFieldListOut, clAftData, 0);*/
				tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "",
                        "", "", "", pcgTwEnd,
                        cgVdgSendCommandOutbound, "AFTTAB", clSelection, cgFieldListOut, clAftData, 0);
            }
        }
    }
    return ilRc;
}

static int SendToVdgs() {
    int ilRc = RC_SUCCESS;
    long llRowNum = 0;
    static long llSent = -1;
    char clNow[24] = "\0";
    char clNowTMO[24] = "\0";
    /* char *pclRowBuf;   */
    char *pclRowBuf = "\0";
    int ilLc = 0, ilFieldCount = 0;
    char clTmpFieldBuf[1024] = "\0";
    long llFieldNo = -1;
    char *pclTmpPtr = "\0";

    GetServerTimeStamp("UTC", 1, 0, clNow);
    strcpy(clNowTMO, clNow);
    AddSecondsToCEDATime(clNow, igTimeInbound * 60, 1);
    llRowNum = ARR_FIRST;

    if (igMode == TRUE) {
        dbg(DEBUG, "line<%d>-----------SendToVdgs1--------------", __LINE__);
        dbg(DEBUG, "line<%d> the begin of SendToVdgs()_before the first while loop", __LINE__);
        dbg(DEBUG, "line<%d> pclRowBuf=<%s>", __LINE__, pclRowBuf);
        dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
        dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
        dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
        dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
        dbg(DEBUG, "line<%d>-----------SendToVdgs1--------------", __LINE__);
    }

    while (CEDAArrayGetRowPointer(&rgAftArrayInbound.rrArrayHandle,
            rgAftArrayInbound.crArrayName, llRowNum, (void *) &pclRowBuf) == RC_SUCCESS) {

        /*FYA v1.26d UFIS-8325
        char clUrno[24];
        char pclUrnoBuf[24];*/
        char clUrno[URNO_LEN] = "\0";
        char pclUrnoBuf[URNO_LEN * 2];

        char pclTMOField[24];
        static long llUrno = -1;

        if (igMode == TRUE) {
            dbg(DEBUG, "line<%d>------------Arrival1-------------", __LINE__);
            dbg(DEBUG, "line<%d> in the 1st while loop_calling CEDAArrayGetField()", __LINE__);
            /* dbg(DEBUG,"line<%s> pclRowBuf=<%s>",__LINE__,pclRowBuf); */
            dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
            dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
            dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
            dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
            dbg(DEBUG, "line<%d>------------Arrival1-------------", __LINE__);
        }

        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llUrno, "URNO", 12, ARR_CURRENT, clUrno);
        StringTrimRight(clUrno);
        sprintf(pclUrnoBuf, "#%s#", clUrno);

        if (igMode == TRUE) {
            dbg(DEBUG, "line<%d> in the 1st while loop_after calling CEDAArrayGetField()", __LINE__);
        }

        if (strstr(pcgUrnoListIn, pclUrnoBuf) == NULL) {
            if (strcmp(clNow, AFTFIELD(pclRowBuf, igAftTIFA)) >= 0) {
                char clAftData[2048] = "";
                char clAftDataOld[2048] = "";
                char clAftDataToSend[4096] = "";
                char clFina[12];
                char clSelection[124];

                if (igSetTMO == FALSE) {
                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> calling AddToUrnoList1()", __LINE__);

                    AddToUrnoList(clUrno, pcgUrnoListIn, lgUrnoListLengthIn, &lgUrnoListActualLengthIn);
                    CEDAArrayPutField(&rgAftArrayInbound.rrArrayHandle,
                            rgAftArrayInbound.crArrayName, &llSent, "SENT", ARR_CURRENT, "1");

                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> after calling AddToUrnoList1()", __LINE__);
                }

                if (igSetTMO == TRUE) {
                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> igSetTMO == TRUE", __LINE__);

                    if (igTMOIsATCBased && (strcmp(AFTFIELD(pclRowBuf, igTMOFld), AFTFIELD(pclRowBuf, igAftTIFA)) != 0)) {
                        dbg(TRACE, "%05d: Not setting TMO for <%s>. TIFA = <%s>, %s = <%s>",
                                __LINE__, AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), cgTMOField, AFTFIELD(pclRowBuf, igTMOFld));
                        llRowNum = ARR_NEXT;
                        continue;
                    }

                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> calling AddToUrnoList2()", __LINE__);

                    AddToUrnoList(clUrno, pcgUrnoListIn, lgUrnoListLengthIn, &lgUrnoListActualLengthIn);
                    CEDAArrayPutField(&rgAftArrayInbound.rrArrayHandle,
                            rgAftArrayInbound.crArrayName, &llSent, "SENT", ARR_CURRENT, "1");

                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> after calling AddToUrnoList2()", __LINE__);

                    dbg(TRACE, "Setting TMO to NOW with an UFR to flight for <%s>", AFTFIELD(pclRowBuf, igAftFLNO));

                    sprintf(clSelection, "WHERE URNO=%s", clUrno);
                    dbg(DEBUG, "%05d: Selection=<%s>", __LINE__, clSelection);

					/*FYA v1.26c UFIS-8297*/
                    /*tools_send_info_flag(igSendToModId, 0, "TMOHDL", "", "", "", "", "", "",
                            "UFR", "AFTTAB", clSelection, "TMAA", clNowTMO, NETOUT_NO_ACK);*/
					tools_send_info_flag(igSendToModId, 0, "TMOHDL", "", "", "", "", "", pcgTwEnd,
                            "UFR", "AFTTAB", clSelection, "TMAA", clNowTMO, NETOUT_NO_ACK);

                } else /* igSetTMO == FALSE */
                {
                    if (igMode == TRUE) {
                        dbg(DEBUG, "line<%d>-----------Arrival2--------------", __LINE__);
                        dbg(DEBUG, "line<%d> igSetTMO == False", __LINE__);
                        /* dbg(DEBUG,"line<%d> pclRowBuf=<%s>",__LINE__,pclRowBuf);  */
                        dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                        dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
                        dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                        dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
                        dbg(DEBUG, "line<%d>-----------Arrival2--------------", __LINE__);
                    }
                    /* #define AFTFIELD(pcpBuf,ipFieldNo) (&pcpBuf[rgAftArrayInbound.plrArrayFieldOfs[ipFieldNo]])  */
                    /* long   *plrArrayFieldOfs; */

                    if (igAftFLNO >= 0) {
                        dbg(DEBUG, "flight <%s> <%s> will be sent at %s",
                                AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), clNow);
                    } else if (igAftFLNO < 0) {
                        dbg(DEBUG, "line<%d>++++++++++++++++++", __LINE__);
                        dbg(DEBUG, "line<%d>hello, bug here", __LINE__);
                        dbg(DEBUG, "line<%d>++++++++++++++++++", __LINE__);
                    }

                    if (igMode == TRUE) {
                        dbg(DEBUG, "line<%d>*******************", __LINE__);
                        /* dbg(DEBUG,"\n\nline<%s>pclRowBuf<%s>\n\n",__LINE__,pclRowBuf);  */
                        /* dbg(DEBUG,"line<%s>rgAftArrayInbound.plrArrayFieldOfs<%s>",__LINE__,rgAftArrayInbound.plrArrayFieldOfs); */
                        dbg(DEBUG, "line<%d>*******************", __LINE__);
                    }


                    if (igMode == TRUE)
                        dbg(DEBUG, "line<%d> after calling AFTFIELD", __LINE__);

                    ilFieldCount = get_no_of_items(cgFieldListIn);
                    for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                        llFieldNo = -1;
                        ilRc = get_real_item(&clFina[0], cgFieldListIn, ilLc);
                        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                                   &llFieldNo , clFina ,1026, ARR_CURRENT,clTmpFieldBuf); /* v1.25 increasing "1023" to "1026" */
				StringTrimRight(clTmpFieldBuf); /* v1.25  */
                        dbg(DEBUG, "ARR Field : <%s> data <%s>", clFina, clTmpFieldBuf);
                        if (*clAftData != 0) {
                            strcat(clAftData, ",");
                            strcat(clAftDataOld, ",");
                        }

                        strcat(clAftData, clTmpFieldBuf);

                        if (strstr(clFina, "PST") != NULL) {
                            strcat(clAftDataOld, "     ");
                        } else {
                            strcat(clAftDataOld, clTmpFieldBuf);
                        }
                    }
                    /** build where clause **/

                    sprintf(clSelection, "WHERE URNO=%s\n%s", clUrno, cgHopo, clUrno);
                    dbg(TRACE, "%05d: Selection=<%s>", __LINE__, clSelection);

                    /* Check if for ATC (Delete/Insert events) */

                    if (igForATC) {
                        strcpy(clAftDataToSend, clAftDataOld);
                        strcat(clAftDataToSend, "\n");
                        strcat(clAftDataToSend, clAftData);

						/*FYA v1.26c UFIS-8297*/
                        /*tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", "",
                                "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);*/
						tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", pcgTwEnd,
                                "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);

                        strcpy(clAftDataToSend, clAftData);
                        strcat(clAftDataToSend, "\n");
                        strcat(clAftDataToSend, clAftDataOld);

						/*FYA v1.26c UFIS-8297*/
                        /*tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", "",
                                "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);*/
						tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", pcgTwEnd,
                                "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);
                    } else {
					dbg(DEBUG,"cgFieldListIn <%s> clAftData <%s>",cgFieldListIn,clAftData); /* test v1.25 */

						/*FYA v1.26c UFIS-8297*/
                        /*tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "", "", "", "", "",
                                cgVdgSendCommandInbound, "AFTTAB", clSelection, cgFieldListIn, clAftData, 0);*/
						tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "", "", "", "", pcgTwEnd,
                                cgVdgSendCommandInbound, "AFTTAB", clSelection, cgFieldListIn, clAftData, 0);
                    }
                } /* end else (TMO setting) */
            } else {
                dbg(DEBUG, "flight <%s> will be send later TIFA <%s> now <%s>",
                        AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), clNow);
            }
        } else {
            dbg(DEBUG, "flight <%s> <%s> already sent",
                    AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA));
            if (strcmp(clNow, AFTFIELD(pclRowBuf, igAftTIFA)) < 0 && strcmp(pcgResend, "YES") == 0) {
                dbg(DEBUG, "... but will be sent again because TIFA has changed");
                pclTmpPtr = strstr(pcgUrnoListIn, pclUrnoBuf);
                if (pclTmpPtr != NULL) {
                    while (*pclTmpPtr != ',' && *pclTmpPtr != '\0') {
                        *pclTmpPtr = 'x';
                        pclTmpPtr++;
                    }
                }

                if (igSetTMO) {
                    char clSel[124];

                    if (igTMOIsATCBased && (strcmp(AFTFIELD(pclRowBuf, igTMOFld), AFTFIELD(pclRowBuf, igAftTIFA)) != 0)) {
                        dbg(TRACE, "%05d: Not clearing TMO for <%s>. TIFA = <%s>, %s = <%s>",
                                __LINE__, AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFA), cgTMOField, AFTFIELD(pclRowBuf, igTMOFld));
                        return ilRc;
                    }

                    dbg(TRACE, "Clearing TMO due to TIFA change for <%s>", AFTFIELD(pclRowBuf, igAftFLNO));

                    sprintf(clSel, "WHERE URNO=%s", clUrno);
                    dbg(DEBUG, "%05d: Selection=<%s>", __LINE__, clSel);

					/*FYA v1.26c UFIS-8297*/
                    /*tools_send_info_flag(igSendToModId, 0, "TMOHDL", "", "", "", "", "", "",
                            "UFR", "AFTTAB", clSel, "TMAA", " ", NETOUT_NO_ACK);*/

					tools_send_info_flag(igSendToModId, 0, "TMOHDL", "", "", "", "", "", pcgTwEnd,
                            "UFR", "AFTTAB", clSel, "TMAA", " ", NETOUT_NO_ACK);
                }

            }
        }
        llRowNum = ARR_NEXT;
    }

    if (igMode == TRUE) {
        dbg(DEBUG, "line<%d>----------SendToVdgs2---------------", __LINE__);
        dbg(DEBUG, "line<%d> inside SendToVdgs: in between", __LINE__);
        dbg(DEBUG, "line<%d> pclRowBuf=<%s>", __LINE__, pclRowBuf);
        dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
        dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
        dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
        dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
        dbg(DEBUG, "line<%d>----------SendToVdgs2---------------", __LINE__);
    }

    if (igSetTMO) {
        return ilRc;
    }

    if (igMode == TRUE) {
        dbg(DEBUG, "line<%d>----------SendToVdgs3---------------", __LINE__);
        dbg(DEBUG, "line<%d> inside SendToVdgs: before the second while loop", __LINE__);
        dbg(DEBUG, "line<%d> pclRowBuf=<%s>", __LINE__, pclRowBuf);
        dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
        dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
        dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
        dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
        dbg(DEBUG, "line<%d>----------SendToVdgs3---------------", __LINE__);
    }

    GetServerTimeStamp("UTC", 1, 0, clNow);
    AddSecondsToCEDATime(clNow, igTimeOutbound * 60, 1);
    llRowNum = ARR_FIRST;
    while (CEDAArrayGetRowPointer(&rgAftArrayOutbound.rrArrayHandle,
            rgAftArrayOutbound.crArrayName, llRowNum, (void *) &pclRowBuf) == RC_SUCCESS) {

        /*FYA v1.26d UFIS-8325
        char clUrno[24];
        char pclUrnoBuf[24];*/
        char clUrno[URNO_LEN] = "\0";
        char pclUrnoBuf[URNO_LEN * 2] = "\0";

        static long llUrno = -1;

        if (igMode == TRUE) {
            dbg(DEBUG, "line<%d>------------Depature1-------------", __LINE__);
            dbg(DEBUG, "line<%d> pclRowBuf=<%s>", __LINE__, pclRowBuf);
            dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
            dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
            dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
            dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
            dbg(DEBUG, "line<%d>------------Depature1-------------", __LINE__);
        }

        CEDAArrayGetField(&rgAftArrayOutbound.rrArrayHandle, rgAftArrayOutbound.crArrayName,
                &llUrno, "URNO", 12, ARR_CURRENT, clUrno);
        StringTrimRight(clUrno);
        sprintf(pclUrnoBuf, "#%s#", clUrno);
        if (strstr(pcgUrnoListOut, pclUrnoBuf) == NULL) {
            if (strcmp(clNow, AFTFIELD(pclRowBuf, igAftTIFD)) >= 0) {
                char clAftData[2048] = "";
                char clAftDataOld[2048] = "";
                char clAftDataToSend[4096] = "";
                char clFina[12];
                char clSelection[124];

                if (igMode == TRUE)
                    dbg(DEBUG, "line<%d> calling AddToUrnoList3()", __LINE__);

                AddToUrnoList(clUrno, pcgUrnoListOut, lgUrnoListLengthOut, &lgUrnoListActualLengthOut);

                if (igMode == TRUE)
                    dbg(DEBUG, "line<%d> after calling AddToUrnoList3()", __LINE__);

                if (igMode == TRUE) {
                    dbg(DEBUG, "line<%d>----------Depature2---------------", __LINE__);
                    dbg(DEBUG, "line<%d> pclRowBuf=<%s>", __LINE__, pclRowBuf);
                    dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                    dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                    dbg(DEBUG, "line<%d> clNow=<%s>", __LINE__, clNow);
                    dbg(DEBUG, "line<%d>----------Depature2---------------", __LINE__);
                }

                dbg(DEBUG, "flight <%s> <%s> will be sent at %s",
                        AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFD), clNow);

                if (igMode == TRUE)
                    dbg(DEBUG, "line<%d> after calling AFTFIELD", __LINE__);

                ilFieldCount = get_no_of_items(cgFieldListOut);
                for (ilLc = 1; ilLc <= ilFieldCount; ilLc++) {
                    llFieldNo = -1;
                    ilRc = get_real_item(&clFina[0], cgFieldListOut, ilLc);
                    CEDAArrayGetField(&rgAftArrayOutbound.rrArrayHandle, rgAftArrayOutbound.crArrayName,
                                &llFieldNo , clFina ,1026, ARR_CURRENT,clTmpFieldBuf); /* v1.25 increasing "1023" to "1026" */
				StringTrimRight(clTmpFieldBuf); /* v1.25 */
                    dbg(DEBUG, "DEP Field: <%s> data <%s>", clFina, clTmpFieldBuf);
                    if (*clAftData != 0) {
                        strcat(clAftData, ",");
                        strcat(clAftDataOld, ",");
                    }
                    strcat(clAftData, clTmpFieldBuf);

                    if (strstr(clFina, "PST") != NULL) {
                        strcat(clAftDataOld, "     ");
                    } else {
                        strcat(clAftDataOld, clTmpFieldBuf);
                    }
                }
                /** build where clause **/

                sprintf(clSelection, "WHERE URNO=%s\n%s", clUrno, clUrno);
                dbg(TRACE, "%05d: Selection=<%s>", __LINE__, clSelection);

                /* Check if for ATC (Delete/Insert events) */

                if (igForATC) {
                    strcpy(clAftDataToSend, clAftDataOld);
                    strcat(clAftDataToSend, "\n");
                    strcat(clAftDataToSend, clAftData);

					/*FYA v1.26c UFIS-8297*/
                    /*tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", "",
                            "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);*/
					tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", pcgTwEnd,
                            "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);

                    strcpy(clAftDataToSend, clAftData);
                    strcat(clAftDataToSend, "\n");
                    strcat(clAftDataToSend, clAftDataOld);

					/*FYA v1.26c UFIS-8297*/
                    /*tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", "",
                            "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);*/
					tools_send_info_flag(igSendToModId, mod_id, "ATCHDL", "", "", "", "", "UFR,-1,-1", pcgTwEnd,
                            "UFR", "AFTTAB", clSelection, cgFieldListOut, clAftDataToSend, 0);
                } else {
					dbg(DEBUG,"cgFieldListIn <%s> clAftData <%s>",cgFieldListIn,clAftData); /* test v1.25 */
					/*FYA v1.26c UFIS-8297*/
                    /*tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "", "", "", "", "",
                            cgVdgSendCommandOutbound, "AFTTAB", clSelection, cgFieldListOut, clAftData, 0);*/
					tools_send_info_flag(igSendToModId, mod_id, "VGDHDL", "", "", "", "", "", pcgTwEnd,
                            cgVdgSendCommandOutbound, "AFTTAB", clSelection, cgFieldListOut, clAftData, 0);
                }
            } else {
                dbg(DEBUG, "flight <%s> will be send later TIFD <%s> now <%s>",
                        AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFD), clNow);
            }
        } else {
            dbg(DEBUG, "flight <%s> <%s> already sent",
                    AFTFIELD(pclRowBuf, igAftFLNO), AFTFIELD(pclRowBuf, igAftTIFD));
            if (strcmp(clNow, AFTFIELD(pclRowBuf, igAftTIFD)) < 0 && strcmp(pcgResend, "YES") == 0) {
                dbg(DEBUG, "... but will be sent again because TIFD has changed");
                pclTmpPtr = strstr(pcgUrnoListOut, pclUrnoBuf);
                if (pclTmpPtr != NULL) {
                    while (*pclTmpPtr != ',' && *pclTmpPtr != '\0') {
                        *pclTmpPtr = 'x';
                        pclTmpPtr++;
                    }
                }
            }
        }
        llRowNum = ARR_NEXT;
    }

    if (igMode == TRUE)
        dbg(DEBUG, "line<%d> inside SendToVdgs_after the second while loop", __LINE__);

    return ilRc;
}

static int GetCommand(char *pcpCommand, int *pipCmd) {
    int ilRc = RC_SUCCESS; /* Return code */
    int ilLoop = 0;
    char clCommand[48];

    memset(&clCommand[0], 0x00, 8);

    ilRc = get_real_item(&clCommand[0], pcpCommand, 1);
    if (ilRc > 0) {
        ilRc = RC_FAIL;
    }/* end of if */

    while ((ilRc != 0) && (prgCmdTxt[ilLoop] != NULL)) {
        ilRc = strcmp(&clCommand[0], prgCmdTxt[ilLoop]);
        ilLoop++;
    }/* end of while */

    if (ilRc == 0) {
        ilLoop--;
        dbg(TRACE, "GetCommand: <%s> <%d>", prgCmdTxt[ilLoop], rgCmdDef[ilLoop]);
        *pipCmd = rgCmdDef[ilLoop];
    } else {
        dbg(TRACE, "GetCommand: <%s> is not valid", &clCommand[0]);
        ilRc = RC_FAIL;
    }/* end of if */

    return (ilRc);

}


/******************************************************************************/
/* The handle data routine                                                    */

/******************************************************************************/
static int HandleData(EVENT *prpEvent) {

    char * pclFunc = "HandleData";
    int ilRc = RC_SUCCESS; /* Return code */
    int i, ilCmd = 0;
    BC_HEAD *prlBchead = NULL;
    CMDBLK *prlCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;

    /*FYA v1.26d UFIS-8325
    char clUrnoList[2400];
    char clUrno[124];*/
    char clUrnoList[URNO_LEN * 200] = "\0";
    char clUrno[URNO_LEN] = "\0";

    char clTable[34];
    char *pclNewline = NULL;


    prlBchead = (BC_HEAD *) ((char *) prpEvent + sizeof (EVENT));
    prlCmdblk = (CMDBLK *) ((char *) prlBchead->data);
    pclSelection = prlCmdblk->data;
    pclFields = pclSelection + strlen(pclSelection) + 1;
    pclData = pclFields + strlen(pclFields) + 1;

    strcpy(clTable, prlCmdblk->obj_name);
    strcpy(pcgTwStart, prlCmdblk->tw_start);
    strcpy(pcgTwEnd, prlCmdblk->tw_end);

	/*if(checkHopo(pcgTwEnd, cgHopo) == RC_FAIL)
		return RC_FAIL;*/

    dbg(TRACE, "========== START <%10.10d> ==========", lgEvtCnt);

    if(checkAndsetHopo(pcgTwEnd, cgHopo_sgstab) == RC_FAIL)
        return RC_FAIL;

    dbg(TRACE,"%s: TABEND = <%s>",pclFunc,cgTabEnd);
    memset(pcgTwEnd,0x00,sizeof(pcgTwEnd));
    sprintf(pcgTwEnd,"%s,%s,%s",cgHopo,cgTabEnd,mod_name);
    dbg(TRACE,"%s : TW_END = <%s>",pclFunc,pcgTwEnd);

    /****************************************/
    /* DebugPrintBchead(DEBUG,prlBchead); */
    /* DebugPrintCmdblk(TRACE,prlCmdblk); */

    dbg(TRACE, "Originator = <%d>", prpEvent->originator);
    dbg(TRACE, "Command    = <%s>", prlCmdblk->command);
    dbg(TRACE, "Table      = <%s>", clTable);
    dbg(TRACE, "Selection  = <%s>", pclSelection);
    dbg(TRACE, "Fields     = <%s>", pclFields);
    dbg(TRACE, "Data       = <%s>", pclData);
    /****************************************/

    /* delete old data from pclData */
    if ((pclNewline = strchr(pclData, '\n')) != NULL) {
        *pclNewline = '\0';
    }

    ilRc = GetCommand(&prlCmdblk->command[0], &ilCmd);
    if (ilRc == RC_SUCCESS) {
        igSetTMO = FALSE;
        switch (ilCmd) {
            case CMD_IFR:
            case CMD_UFR:
                dbg(DEBUG, "HandleData: %s Cmdblk->command = <%s>", prgCmdTxt[ilCmd], prlCmdblk->command);
                if (strcmp(prlCmdblk->command, "IFR") == 0) {
                    strcpy(prlCmdblk->command, "IRT");
                } else {
                    strcpy(prlCmdblk->command, "URT");
                }
                ilRc = CEDAArrayEventUpdate2(prgEvent, clUrnoList, clTable);
                if (ilRc != RC_SUCCESS) {
                    dbg(TRACE, "HandleData: CEDAArrayEventUpdate failed <%d>", ilRc);
                } else {
                    if (FindTriggerField(pclFields, pclData) == RC_SUCCESS) {
                        ilRc = SendSingleFlight(clUrnoList);
                    }
                }
                break;
            case CMD_DFR:
                strcpy(prlCmdblk->command, "DRT");
                ilRc = CEDAArrayEventUpdate2(prgEvent, clUrnoList, clTable);
                if (ilRc != RC_SUCCESS) {
                    dbg(TRACE, "HandleData: CEDAArrayEventUpdate failed <%d>", ilRc);
                }
                break;
            case CMD_VDG:

                /*UFIS-7275*/

				//*pcgUrnoListIn = 0x00;
                if (pcgUrnoListIn != NULL)
                {
                    free(pcgUrnoListIn);
                    pcgUrnoListIn = malloc(lgUrnoListLengthIn);
                    if (pcgUrnoListIn == NULL)
                    {
                        dbg(TRACE, "unable to allocate %ld Bytes for UrnoListIn", lgUrnoListLengthIn);
                        return RC_FAIL;
                    } else
                    {
                        memset(pcgUrnoListIn, 0, lgUrnoListLengthIn);
                        lgUrnoListActualLengthIn = 0;
                    }
                }

                //*pcgUrnoListOut = 0x00;
                if (pcgUrnoListOut != NULL)
                {
                    free(pcgUrnoListOut);
                    pcgUrnoListOut = malloc(lgUrnoListLengthIn);
                    if (pcgUrnoListOut == NULL)
                    {
                        dbg(TRACE, "unable to allocate %ld Bytes for UrnoListIn", lgUrnoListLengthIn);
                        return RC_FAIL;
                    } else
                    {
                        memset(pcgUrnoListOut, 0, lgUrnoListLengthIn);
                        lgUrnoListActualLengthOut = 0;
                    }
                }



                ilRc = SendToVdgs();
                break;
            case CMD_VDR:
                ilRc = ReloadArrays();
                break;
            case CMD_SETOAL:
                ilRc = HandleSETOAL();
                break;
            case CMD_REPEAT:

                if (igMode == TRUE) {
                    dbg(DEBUG, "line<%d>----------before ReloadArrays---------------", __LINE__);
                    dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                    dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
                    dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                    dbg(DEBUG, "line<%d>----------before ReloadArrays---------------", __LINE__);
                }

                ilRc = ReloadArrays();
                /* Clear the URNO lists in order to resend all */

                *pcgUrnoListIn = 0x00;

                lgUrnoListActualLengthIn = 0;

                *pcgUrnoListOut = 0x00;

                lgUrnoListActualLengthOut = 0;

                if (igMode == TRUE) {
                    dbg(DEBUG, "line<%d>-----------before SendToVdgs--------------", __LINE__);
                    dbg(DEBUG, "line<%d> igAftFLNO=<%d>", __LINE__, igAftFLNO);
                    dbg(DEBUG, "line<%d> igAftTIFA=<%d>", __LINE__, igAftTIFA);
                    dbg(DEBUG, "line<%d> igAftTIFD=<%d>", __LINE__, igAftTIFD);
                    dbg(DEBUG, "line<%d>-----------before SendToVdgs--------------", __LINE__);
                }
                ilRc = SendToVdgs();
                break;
            case CMD_TMO:
                igSetTMO = 1;
                ilRc = ReloadArrays();
                ilRc = SendToVdgs();
            case CMD_BATCH:
            case CMD_BATCH_EFPS:
                /***********BHS*****************/
            case CMD_RESEND:
                ilRc = SendBatch(ilCmd, pclData); /*BHS*/
                break;
                /***********BHS*****************/
            case CMD_PORTAL:
                ilRc = SendPortal();
                break;
            case CMD_REQ:
                ilRc = SendSingle(pclData);
                break;
            default:;
        } /* end of switch */
    }

    ilRc = TimeToStr(pcgCurrentTime, time(NULL));
    if (strcmp(prlCmdblk->command, "CPRF") == 0) {
        if (strcmp(pcgCurrentTime, pcgWaitTill) < 0)
            dbg(DEBUG, "HandleData: CPRF Command not yet allowed");
        else
            ilRc = HandleCPRF();
    } else if (strcmp(prlCmdblk->command, "SPRF") == 0) {
        ilRc = TimeToStr(pcgWaitTill, time(NULL));
        dbg(DEBUG, "HandleData: Now Handle CPRF Command");
    }

    dbg(TRACE, "==========  END  <%10.10d> ==========", lgEvtCnt);
    return (RC_SUCCESS);
}

static int UpdDemUtpl() {
    int ilRc = RC_INIT_FAIL;
    long llRowNum = ARR_FIRST, llRowCount = 0;
    char clUrud[21], clUtpl[21], *pclRow;
    char clDataArea[2512];
    char clSqlBuf[256];
    short slCursor = 0;

    char clTaFi[21], clFele[11], clModId[11], clFldLst[16];
    int ilCnt = 1, ilRc1, ilMod;
    HANDLE slArrHdl = -1;
    char clArrName[11] = "RUDARR";
    char clRudFields[21] = "URNO,UTPL";
    long llFeles[5];
    long llOffs[5];
    int ilDone = 0, ilLen;

	/*FYA v1.26c UFIS-8297*/
    /*char clTwEnd[33];*/
	char clTwEnd[256] = "\0";

    dbg(TRACE, "========== UpdDemUtpl started <%10.10d> ==========", lgEvtCnt);

    sprintf(clTaFi, "DEM%s,UTPL", cgTabEnd);
    ilRc = syslibSearchSystabData(cgTabEnd, "TANA,FINA", clTaFi, "FELE", clFele, &ilCnt, "");
    if (ilRc != RC_SUCCESS)
        dbg(TRACE, "UpdDemUtpl: DEM%s.UTPL not in DB configuration, RC <%d>", cgTabEnd, ilRc);
    else {
        ilCnt = 1;
        sprintf(clTaFi, "RUD%s,UTPL", cgTabEnd);
        strcpy(clFldLst, "TANA,FINA");
        ilRc = syslibSearchSystabData(cgTabEnd, clFldLst, &clTaFi[0], "FELE", clFele, &ilCnt, "");
        if (ilRc != RC_SUCCESS)
            dbg(TRACE, "UpdDemUtpl: RUD%s.UTPL not in DB configuration, RC <%d>", cgTabEnd, ilRc);
    }
    if (ilRc == RC_SUCCESS) {
        sprintf(clTaFi, "RUD%s", cgTabEnd);
        ilRc = CEDAArrayCreate(&slArrHdl, clArrName, clTaFi, "", 0, 0,
                clRudFields, llFeles, llOffs);
        dbg(TRACE, "UpdDemUtpl: CEDAArrayCreate done, RC <%d>", ilRc);
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = CEDAArrayFill(&slArrHdl, clArrName, "");
    }
    if (ilRc == RC_SUCCESS) {
        CEDAArrayGetRowCount(&slArrHdl, clArrName, &llRowCount);
        while (CEDAArrayGetRowPointer(&slArrHdl, clArrName, llRowNum, (void *) &pclRow) == RC_SUCCESS) {
            slCursor = 0;
            strcpy(clUrud, pclRow + llOffs[0]);
            strcpy(clUtpl, pclRow + llOffs[1]);
            CT_TrimRight(clUrud, ' ', -1);
            CT_TrimRight(clUtpl, ' ', -1);

			/*FYA v1.26c UFIS-8297*/
            if (igMultiHopo == TRUE)
            {
               sprintf(clSqlBuf, "UPDATE DEMTAB SET UTPL='%s' WHERE URUD='%s' AND (UTPL is null or UTPL=' ') AND HOPO IN ('***','%s')", clUtpl, clUrud, cgHopo);
            }
			else
			{
               sprintf(clSqlBuf, "UPDATE DEMTAB SET UTPL='%s' WHERE URUD='%s' AND (UTPL is null or UTPL=' ')", clUtpl, clUrud);
			}

            clDataArea[0] = '\0';
            ilRc1 = sql_if(START, &slCursor, clSqlBuf, clDataArea);
            if ((ilRc1 != DB_SUCCESS) && (ilRc1 != RC_NOTFOUND)) {
                get_ora_err(ilRc1, clDataArea);
                dbg(TRACE, "UpdDemUtpl: ORA-ERR <%d> <%s>\nSqlBuf <%s>", ilRc1, clDataArea, clSqlBuf);
            } else
                dbg(TRACE, "UpdDemUtpl: Update <%s> done (%d/%ld) RC <%d> ", clSqlBuf, ilDone, llRowCount, ilRc1);

            commit_work();
            ilDone++;
            close_my_cursor(&slCursor);
            llRowNum = ARR_NEXT;
        }
        dbg(TRACE, "UpdDemUtpl: All %d RUD records processed", ilDone);
    }
    if (slArrHdl > -1)
        CEDAArrayDestroy(&slArrHdl, clArrName);
    if (ilRc == RC_SUCCESS) { /* Activate loading of demands with DEMTAB.UTPL */
        sprintf(clDataArea, "%s/demhdl.cfg", getenv("CFG_PATH"));
        ilRc = iWriteConfigEntry(clDataArea, "MAIN", "LoadFormat", CFG_STRING, "DEM");
        if (ilRc != RC_SUCCESS)
            dbg(TRACE, "UpdDemUtpl: iWriteConfigEntry in <%s> failed RC <%s>", clDataArea, ilRc);
        clDataArea[0] = '\0';
        if (ReadConfigEntry("UPDDEMUTPL", "Notify", clDataArea) != RC_SUCCESS)
            strcpy(clDataArea, "9000,9002,9004");
        sprintf(clTwEnd, "%s,%s,pdehdl", cgHopo, cgTabEnd);

        prgEvent->type = USR_EVENT;
        prgEvent->command = RESET;
        prgEvent->originator = mod_id;
        strcpy(prgEvent->sys_ID, mod_name);
        ilLen = sizeof (EVENT);

        ilCnt = 1;
        while (GetDataItem(clModId, clDataArea, ilCnt, ',', "", " \0") > 0) {
            ilMod = atoi(clModId);
            dbg(TRACE, "UpdDemUtpl: Going to send event of length <%d> to <%d>:", ilLen, ilMod);
            DebugPrintEvent(TRACE, prgEvent);
            ilRc1 = que(QUE_PUT, ilMod, mod_id, PRIORITY_5, ilLen, (char *) prgEvent);
            dbg(TRACE, "UpdDemUtpl: que returned <%d>", ilRc1);
            ilCnt++;
        }
    }
    dbg(TRACE, "========== UpdDemUtpl finished RC <%d> ==========", ilRc);
    return ilRc;
}

static int IniDemRedundance() {
    /* update demtab set lstu='20041118144900', useu='hag' where urno='398725522'; */
    int ilRc = RC_INIT_FAIL, ilCnt = 1, ilRc1;
    long llRowNum = ARR_FIRST, llRowCount = 0, llResRow;
    char clUrud[21], clUghs[21], clReco[34], clQuco[211];
    char *pclRow, *pclRety, *pclResource = 0, *pclRloc, *pclReft, *pclGtab;
    char clDataArea[2512], clSqlBuf[256];
    short slCursor = 0;
    char clTaFi[21], clFele[11], clFldLst[16];
    int ilDone = 0;
    ARRAYINFO rlRudArr, rlRpfArr, rlRpqArr;
    ARRAYINFO rlReqArr, rlRloArr, rlSgrArr;

    dbg(TRACE, "========== IniDemRedundance started <%10.10d> ==========", lgEvtCnt);
    rlRudArr.rrArrayHdl = rlRpfArr.rrArrayHdl = -1;
    rlRpqArr.rrArrayHdl = rlReqArr.rrArrayHdl = -1;
    rlRloArr.rrArrayHdl = rlSgrArr.rrArrayHdl = -1;

    sprintf(clTaFi, "DEM%s,UGHS", cgTabEnd);
    strcpy(clFldLst, "TANA,FINA");
    ilRc = syslibSearchSystabData(cgTabEnd, clFldLst, clTaFi, "FELE", clFele, &ilCnt, "");
    if (ilRc != RC_SUCCESS)
        dbg(TRACE, "IniDemRedundance: DEM%s.UGHS not in DB configuration, RC <%d>", cgTabEnd, ilRc);
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlRudArr, "RUD", "URNO,RETY,UGHS", 0, TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlRpfArr, "RPF", "URUD,GTAB,UPFC,FCCO", "URUD", TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlRpqArr, "RPQ", "URUD,GTAB,UPER,QUCO", "URUD", TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlReqArr, "REQ", "URUD,GTAB,UEQU,EQCO", "URUD", TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlRloArr, "RLO", "URUD,GTAB,RLOC,REFT", "URUD", TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlSgrArr, "SGR", "URNO,GRPN", "URNO", TRUE, "WHERE APPL='RULE_AFT'");
    }

    if (ilRc == RC_SUCCESS) {
        CEDAArrayGetRowCount(&rlRudArr.rrArrayHdl, rlRudArr.crArrayName, &llRowCount);
        while (CEDAArrayGetRowPointer(&rlRudArr.rrArrayHdl, rlRudArr.crArrayName,
                llRowNum, (void *) &pclRow) == RC_SUCCESS) {
            clReco[0] = clQuco[0] = '\0';

            slCursor = 0;
            strcpy(clUrud, pclRow + rlRudArr.plrArrayFldOfs[0]);
            strcpy(clUghs, pclRow + rlRudArr.plrArrayFldOfs[2]);
            pclRety = pclRow + rlRudArr.plrArrayFldOfs[1];
            CT_TrimRight(clUrud, ' ', -1);
            CT_TrimRight(clUghs, ' ', -1);

            if (strstr(pclRety, "100")) {
                ilRc1 = GetResourceName(&rlRpfArr, &rlSgrArr, 1, 2, 3, clUrud, clReco);
                ilRc1 |= GetQualifications(&rlRpqArr, &rlSgrArr, clUrud, clQuco);
            } else if (strstr(pclRety, "010")) {
                ilRc1 = GetResourceName(&rlReqArr, &rlSgrArr, 1, 2, 3, clUrud, clReco);
            } else {
                llResRow = ARR_FIRST;
                if (CEDAArrayFindRowPointer(&rlRloArr.rrArrayHdl,
                        rlRloArr.crArrayName,
                        &rlRloArr.rrIdx01Hdl,
                        rlRloArr.crIdx01Name, clUrud,
                        &llResRow, (void**) &pclResource) == RC_SUCCESS) {
                    pclRloc = pclResource + rlRloArr.plrArrayFldOfs[2];
                    pclGtab = pclResource + rlRloArr.plrArrayFldOfs[1];
                    pclReft = pclResource + rlRloArr.plrArrayFldOfs[3];
                    if (*pclGtab == 'S') {
                        llResRow = ARR_FIRST;
                        if (CEDAArrayFindRowPointer(&rlSgrArr.rrArrayHdl,
                                rlSgrArr.crArrayName,
                                &rlSgrArr.rrIdx01Hdl,
                                rlSgrArr.crIdx01Name, pclRloc,
                                &llResRow, (void**) &pclResource) == RC_SUCCESS) {
                            clReco[0] = '*';
                            strcpy(&(clReco[1]), pclResource + rlSgrArr.plrArrayFldOfs[1]);
                        }
                    } else {
                        ilRc1 = GetNameFromBasicData(pclReft, pclRloc, clReco);
                    }
                }

            }
            if (ilRc1 != RC_SUCCESS)
                dbg(TRACE, "IniDemRedundance: No data found for URUD <%s> -> RECO=' ' RC <%d>", clUrud, ilRc1);
            if (!clUghs[0])
                strcpy(clUghs, " ");
            if (!clQuco[0])
                strcpy(clQuco, " ");
            if (!clReco[0])
                strcpy(clReco, " ");

			/*FYA v1.26c UFIS-8297*/
            if (igMultiHopo == TRUE)
            {
               sprintf(clSqlBuf, "UPDATE DEMTAB SET UGHS='%s',RECO='%s',QUCO='%s' WHERE URUD='%s' AND (UGHS is null or UGHS=' ') AND HOPO IN ('***', '%s')",clUghs, clReco, clQuco, clUrud, cgHopo);
            }
            else
            {
               sprintf(clSqlBuf, "UPDATE DEMTAB SET UGHS='%s',RECO='%s',QUCO='%s' WHERE URUD='%s' AND (UGHS is null or UGHS=' ')",
                    clUghs, clReco, clQuco, clUrud);
            }


            clDataArea[0] = '\0';
            ilRc1 = sql_if(START, &slCursor, clSqlBuf, clDataArea);
            if ((ilRc1 != DB_SUCCESS) && (ilRc1 != RC_NOTFOUND)) {
                get_ora_err(ilRc1, clDataArea);
                dbg(TRACE, "IniDemRedundance: ORA-ERR <%d> <%s>\nSqlBuf <%s>", ilRc1, clDataArea, clSqlBuf);
            } else
                dbg(TRACE, "IniDemRedundance: Update <%s> done (%d/%ld) RC <%d> ", clSqlBuf, ilDone, llRowCount, ilRc1);
            if (ilRc1 == DB_SUCCESS)
                commit_work();
            close_my_cursor(&slCursor);
            ilDone++;

            llRowNum = ARR_NEXT;
        }
        dbg(TRACE, "IniDemRedundance: All %d RUD records processed", ilDone);
    }
    if (rlRudArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlRudArr.rrArrayHdl), rlRudArr.crArrayName);
    if (rlRpfArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlRpfArr.rrArrayHdl), rlRpfArr.crArrayName);
    if (rlRpqArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlRpqArr.rrArrayHdl), rlRpqArr.crArrayName);
    if (rlReqArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlReqArr.rrArrayHdl), rlReqArr.crArrayName);
    if (rlRloArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlRloArr.rrArrayHdl), rlRloArr.crArrayName);
    if (rlSgrArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlSgrArr.rrArrayHdl), rlSgrArr.crArrayName);
    dbg(TRACE, "========== IniDemRedundance finished RC <%d> ==========", ilRc);
    return ilRc;
}

static int SetArraySimple(ARRAYINFO *prpArr, char *pcpTable, char *pcpFields,
        char *pcpIdxField, BOOL bpFill, char *pcpSelection) {
    int ilRc;

    dbg(TRACE, "SetArraySimple: Table <%s> Start", pcpTable);

    prpArr->rrArrayHdl = prpArr->rrIdx01Hdl = -1;
    prpArr->lrArrayFldCnt = 0;
    prpArr->crArrayName[0] = prpArr->crIdx01Name[0] = '\0';
    prpArr->crArrayFldList[0] = '\0';
    memset(&(prpArr->plrArrayFldOfs), 0, sizeof (prpArr->plrArrayFldOfs));
    memset(&(prpArr->plrArrayFldLen), 0, sizeof (prpArr->plrArrayFldLen));

    sprintf(prpArr->crArrayName, "%s%s", pcpTable, cgTabEnd);
    strcpy(prpArr->crArrayFldList, pcpFields);
    prpArr->lrArrayFldCnt = get_no_of_items(prpArr->crArrayFldList);

    ilRc = CEDAArrayCreate(&(prpArr->rrArrayHdl), prpArr->crArrayName,
            prpArr->crArrayName, "", 0, 0, prpArr->crArrayFldList,
            prpArr->plrArrayFldLen, prpArr->plrArrayFldOfs);
    if (ilRc != RC_SUCCESS) {
        dbg(TRACE, "SetArraySimple: CEDAArrayCreate table <%s> fields <%s> failed <%d>",
                pcpTable, pcpFields, ilRc);
    } else {
        if (bpFill) {
            ilRc = CEDAArrayRefill(&(prpArr->rrArrayHdl), prpArr->crArrayName,
                    pcpSelection, NULL, ARR_FIRST);
            if (ilRc != RC_SUCCESS)
                dbg(TRACE, "SetArraySimple: CEDAArrayRefill table <%s> failed <%d>", pcpTable, ilRc);
        }
    }
    if (ilRc == RC_SUCCESS) {
        if (pcpIdxField && *pcpIdxField) {
            sprintf(prpArr->crIdx01Name, "%sIDX1", pcpTable);
            ilRc = CEDAArrayCreateSimpleIndexUp(&(prpArr->rrArrayHdl), prpArr->crArrayName,
                    &(prpArr->rrIdx01Hdl), prpArr->crIdx01Name,
                    pcpIdxField);
            if (ilRc != RC_SUCCESS)
                dbg(TRACE, "SetArraySimple: CEDAArrayCreateSimpleIndexUp table <%s> field <%s> failed <%d>",
                    pcpTable, pcpIdxField, ilRc);
        }
        DebugPrintArrayInfo(DEBUG, prpArr);
    }
    dbg(TRACE, "SetArraySimple: Table <%s> done, RC <%d>", pcpTable, ilRc);
    return ilRc;
}

static int GetResourceName(ARRAYINFO *prpArray, ARRAYINFO *prpSgrArr,
        int ipGtabIdx, int ipUresIdx, int ipCodeIdx,
        char *pcpUrud, char *pcpName) {
    long llRow = ARR_FIRST;
    char *pclResource = 0, *pclSgrRow, *pclUres, *pclGtab, *pclCode;
    int ilRc = RC_NOTFOUND;

    if (!pcpUrud || !pcpName)
        return RC_INVALID;

    /*dbg ( DEBUG, "GetResourceName: ARRAY <%s> URUD <%s> UresIdx <%d> Codeidx <%d> GtabIdx <%d>",
          prpArray->crArrayName, pcpUrud, ipUresIdx, ipCodeIdx, ipGtabIdx );*/
    pcpName[0] = '\0';
    if (CEDAArrayFindRowPointer(&(prpArray->rrArrayHdl),
            prpArray->crArrayName,
            &(prpArray->rrIdx01Hdl),
            prpArray->crIdx01Name, pcpUrud,
            &llRow, (void**) &pclResource) == RC_SUCCESS) {
        pclUres = pclResource + prpArray->plrArrayFldOfs[ipUresIdx];
        pclGtab = pclResource + prpArray->plrArrayFldOfs[ipGtabIdx];
        pclCode = pclResource + prpArray->plrArrayFldOfs[ipCodeIdx];

        if (*pclGtab == 'S') {
            llRow = ARR_FIRST;
            ilRc = CEDAArrayFindRowPointer(&(prpSgrArr->rrArrayHdl),
                    prpSgrArr->crArrayName,
                    &(prpSgrArr->rrIdx01Hdl),
                    prpSgrArr->crIdx01Name, pclUres,
                    &llRow, (void**) &pclSgrRow);
            if (ilRc == RC_SUCCESS) {
                pcpName[0] = '*';
                strcpy(&(pcpName[1]), pclSgrRow + prpSgrArr->plrArrayFldOfs[1]);
            } else
                dbg(TRACE, "GetResourceName: Didn't find group <%s>, RC <%d>", pclUres, ilRc);
        } else {
            strcpy(pcpName, pclCode);
            ilRc = RC_SUCCESS;
        }
    }
    if (ilRc != RC_SUCCESS)
        dbg(TRACE, "GetResourceName failed: URUD <%s> ilRc <%d>", pcpUrud, ilRc);
    /* else
        dbg ( DEBUG, "GetResourceName: URUD <%s> NAME <%s> ilRc <%d>", pcpUrud, pcpName, ilRc ); */

    return ilRc;
}

static int GetNameFromBasicData(char *pcpReft, char *pcpUrno, char *pcpName) {
    int ilRc, dbg_lvl = DEBUG;
    char *pclDot;
    char clSqlBuf[128], clDataArea[128] = "";
    short slCursor = 0;

    if (!pcpReft || !pcpUrno || !pcpName)
        return RC_INVALID;

    pclDot = strchr(pcpReft, '.');
    if (!pclDot) {
        dbg(TRACE, "GetNameFromBasicData: No dot found in REFT <%s>", pcpReft);
        return RC_INVALID;
    }
    *pclDot = '\0';
    pclDot++;

	/*FYA v1.26c UFIS-8297*/
    if (igMultiHopo == TRUE)
    {
        sprintf(clSqlBuf, "SELECT %s FROM %sTAB WHERE URNO = '%s AND HOPO IN ('***','%s')", pclDot, pcpReft, pcpUrno, cgHopo);
    }
    else
    {
        sprintf(clSqlBuf, "SELECT %s FROM %sTAB WHERE URNO = '%s'", pclDot, pcpReft, pcpUrno);
    }

    ilRc = sql_if(START, &slCursor, clSqlBuf, clDataArea);
    if (ilRc != DB_SUCCESS) {
        check_ret(ilRc);
        dbg_lvl = TRACE;
    } else {
        strncpy(pcpName, clDataArea, 32);
        pcpName[32] = '\0';
    }
    close_my_cursor(&slCursor);
    dbg(dbg_lvl, "GetNameFromBasicData: <%s> Data <%s> ilRc <%d>", clSqlBuf, clDataArea, ilRc);
    return ilRc;
}

static int GetQualifications(ARRAYINFO *prpRpqArr, ARRAYINFO *prpSgrArr,
        char *pcpUrud, char *pcpQuco) {
    int ilRc = RC_SUCCESS, ilRc1;
    long llRow = ARR_FIRST, llSgrRow;
    char *pclQuali = 0, *pclSgrRow, *pclGtab, *pclUper, *pclQuco;
    char clCode[40];

    if (!pcpUrud || !pcpQuco) {
        dbg(DEBUG, "GetQualifications: pcpUrud or pcpQuco invalid");
        return RC_INVALID;
    }
    pcpQuco[0] = '\0';
    while (CEDAArrayFindRowPointer(&(prpRpqArr->rrArrayHdl),
            prpRpqArr->crArrayName,
            &(prpRpqArr->rrIdx01Hdl),
            prpRpqArr->crIdx01Name, pcpUrud,
            &llRow, (void**) &pclQuali) == RC_SUCCESS) {
        pclUper = pclQuali + prpRpqArr->plrArrayFldOfs[2];
        pclGtab = pclQuali + prpRpqArr->plrArrayFldOfs[1];
        pclQuco = pclQuali + prpRpqArr->plrArrayFldOfs[3];

        ilRc1 = RC_NOTFOUND;
        clCode[0] = '\0';
        if (*pclGtab == 'S') {
            llSgrRow = ARR_FIRST;
            if (CEDAArrayFindRowPointer(&(prpSgrArr->rrArrayHdl),
                    prpSgrArr->crArrayName,
                    &(prpSgrArr->rrIdx01Hdl),
                    prpSgrArr->crIdx01Name, pclUper,
                    &llSgrRow, (void**) &pclSgrRow) == RC_SUCCESS) {
                clCode[0] = '*';
                strcpy(&(clCode[1]), pclSgrRow + prpSgrArr->plrArrayFldOfs[1]);
                ilRc1 = RC_SUCCESS;
            }
        } else {
            strcpy(clCode, pclQuco);
            ilRc1 = RC_SUCCESS;
        }
        if (ilRc1 == RC_SUCCESS) {
            CT_TrimRight(clCode, ' ', -1);
            if (strlen(pcpQuco) + strlen(clCode) + 2 > 210) {
                dbg(TRACE, "GetQualifications: QUCU too small (%210 bytes) for URUD <%s>", pcpUrud);
                ilRc = RC_NOMEM;
            } else {
                if (*pcpQuco)
                    strcat(pcpQuco, "|");
                strcat(pcpQuco, clCode);
            }
        } else {
            dbg(TRACE, "GetQualifications: Could not find resource URUD <%s>", pcpUrud);
            ilRc = ilRc1;
        }
        llRow = ARR_NEXT;
    }
    if (ilRc != RC_SUCCESS)
        dbg(DEBUG, "GetQualifications: URUD <%s> failed RC <%d>", pcpUrud, ilRc);
    return ilRc;
}

static int IniJobRedundance() {
    int ilRc = RC_INIT_FAIL, ilCnt = 1, ilRc1;
    ARRAYINFO rlDemArr, rlJobArr, rlJodArr;
    ARRAYINFO rlAloArr, rlAlidRes;
    int ilItemNo, ilCol, ilPos, i;
    char clTaFi[21], clBuffer[400], clFldLst[16];
    char clStart[21], clEnd[21], clActDate[15], clSelection[400];
    char *pclJob, *pclDemand, *pclRowJod, *pclUjob, *pclReco;
    char *pclUaid, *pclUalo, *pclUjty;
    long llJobRow, llDemRow, llJodRow;
    short slCursor = 0;
    long llRowCnt, llDone;
    char clUdem[11], clFcco[11], clUghs[11], clAlid[15], clAloc[11];

    dbg(TRACE, "========== IniJobRedundance started <%10.10d> ==========", lgEvtCnt);
    rlJodArr.rrArrayHdl = rlDemArr.rrArrayHdl = -1;
    rlJobArr.rrArrayHdl = rlAloArr.rrArrayHdl = -1;
    rlAlidRes.rrArrayHdl = -1;

    sprintf(clTaFi, "DEM%s,UGHS", cgTabEnd);
    strcpy(clFldLst, "TANA,FINA");
    ilRc = syslibSearchSystabData(cgTabEnd, clFldLst, clTaFi, "FELE", clBuffer, &ilCnt, "");
    if (ilRc != RC_SUCCESS)
        dbg(TRACE, "IniJobRedundance: DEM%s.UGHS not in DB configuration, RC <%d>", cgTabEnd, ilRc);
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlDemArr, "DEM", "URNO,RECO,UGHS", "URNO", FALSE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlJobArr, "JOB", "URNO,FCCO,UGHS,UALO,UAID,ALOC,ALID,UDEM,UJTY", 0, FALSE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlJodArr, "JOD", "URNO,UDEM,UJOB", "UJOB", FALSE, "");
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = SetArraySimple(&rlAloArr, "ALO", "URNO,ALOC,REFT", "URNO", TRUE, "");
    }
    if (ilRc == RC_SUCCESS) {
        strcpy(rlAlidRes.crArrayName, "ALIDRES");
        strcpy(rlAlidRes.crArrayFldList, "UALO,UAID,ALOC,ALID");
        rlAlidRes.lrArrayFldCnt = 4;
        rlAlidRes.plrArrayFldOfs[0] = 0;
        for (i = 0; i < rlAlidRes.lrArrayFldCnt; i++) {
            get_real_item(clFldLst, rlAlidRes.crArrayFldList, i + 1);
            FindItemInList(rlJobArr.crArrayFldList, clFldLst, ',', &ilItemNo, &ilCol, &ilPos);
            if (ilItemNo > 0) {
                rlAlidRes.plrArrayFldLen[i] = rlJobArr.plrArrayFldLen[ilItemNo - 1];
                rlAlidRes.plrArrayFldOfs[i + 1] = rlAlidRes.plrArrayFldOfs[i] +
                        rlAlidRes.plrArrayFldLen[i] + 1;
            } else {
                dbg(TRACE, "Error initialising rlAlidRes: Field <%s> not in JOBTAB fieldlist", clFldLst);
                ilRc = RC_NOTINITIALIZED;
                break;
            }
        }
        ilRc = AATArrayCreate(&(rlAlidRes.rrArrayHdl), rlAlidRes.crArrayName, TRUE,
                1000, rlAlidRes.lrArrayFldCnt, rlAlidRes.plrArrayFldLen,
                rlAlidRes.crArrayFldList, NULL);
        if (ilRc != RC_SUCCESS)
            dbg(TRACE, "Error initialising rlAlidRes: AATArrayCreate failed <%d>", ilRc);
        else {
            strcpy(rlAlidRes.crIdx01Name, "ALIDRES1");
            ilRc = CEDAArrayCreateSimpleIndexUp(&(rlAlidRes.rrArrayHdl),
                    rlAlidRes.crArrayName,
                    &(rlAlidRes.rrIdx01Hdl),
                    rlAlidRes.crIdx01Name, "UALO,UAID");
            if (ilRc != RC_SUCCESS)
                dbg(TRACE, "Error initialising rlAlidRes: CEDAArrayCreateSimpleIndexUp failed <%d>", ilRc);
            DebugPrintArrayInfo(DEBUG, &rlAlidRes);
        }
    }
    if (ilRc == RC_SUCCESS) {
        ilRc = GetStartEnd(TRUE, clStart);
        ilRc |= GetStartEnd(FALSE, clEnd);
    }
    if (ilRc == RC_SUCCESS) {
        if (strcmp(clStart, clEnd) > 0) {
            ilRc = RC_INVALID;
            dbg(TRACE, "IniJobRedundance: Interval [%s,%s] not valid", clStart, clEnd);
        }
    }
    if (ilRc == RC_SUCCESS) {
        strcpy(clActDate, clStart);
        do {
            slCursor = 0;

            ilRc = AddSecondsToCEDATime(clActDate, 86400, 1);
            if (ilRc != RC_SUCCESS)
                break;

            dbg(TRACE, "IniJobRedundance: Loading from <%s> to <%s>", clStart, clActDate);

			/*FYA v1.26c UFIS-8297*/
            /*sprintf(clBuffer, "WHERE ACTO BETWEEN '%s' AND '%s'", clStart, clActDate);*/
            if (igMultiHopo == TRUE)
            {
                sprintf(clBuffer, "WHERE ACTO BETWEEN '%s' AND '%s' AND HOPO IN ('***','%s')", clStart, clActDate, cgHopo);
            }
            else
            {
                sprintf(clBuffer, "WHERE ACTO BETWEEN '%s' AND '%s'", clStart, clActDate);
            }

            ilRc = CEDAArrayRefill(&(rlJobArr.rrArrayHdl), rlJobArr.crArrayName,
                    clBuffer, NULL, ARR_FIRST);

            /*FYA v1.26c UFIS-8297*/
            if (igMultiHopo == TRUE)
            {
                sprintf(clSelection, "WHERE UJOB IN (SELECT URNO FROM JOBTAB %s) AND HOPO IN ('***','%s')", clBuffer, cgHopo);
            }
            else
            {
                sprintf(clSelection, "WHERE UJOB IN (SELECT URNO FROM JOBTAB %s)", clBuffer);
            }

            ilRc = CEDAArrayRefill(&(rlJodArr.rrArrayHdl), rlJodArr.crArrayName,
                    clSelection, NULL, ARR_FIRST);

            /*FYA v1.26c UFIS-8297*/
            if(igMultiHopo == TRUE)
            {
                sprintf(clBuffer, "WHERE URNO IN (SELECT UDEM FROM JODTAB %s) AND HOPO IN ('***','%s')", clSelection, cgHopo);
            }
            else
            {
                sprintf(clBuffer, "WHERE URNO IN (SELECT UDEM FROM JODTAB %s)", clSelection);
            }

            ilRc = CEDAArrayRefill(&(rlDemArr.rrArrayHdl), rlDemArr.crArrayName,
                    clBuffer, NULL, ARR_FIRST);

            CEDAArrayGetRowCount(&rlJobArr.rrArrayHdl, rlJobArr.crArrayName, &llRowCnt);
            llDone = 0;
            llJobRow = ARR_FIRST;
#ifdef CCS_DBMODE_EMBEDDED
			/*FYA v1.26c UFIS-8297*/
			if(igMultiHopo == TRUE)
			{
                strcpy(clSelection, "UPDATE JOBTAB SET UGHS=:VUGHS,UDEM=:VUDEM,FCCO=:VFCCO,ALOC=:VALOC,ALID=:VALID WHERE URNO=:VURNO AND HOPO IN ('***',:VHOPO)");
			}
			else
			{
                strcpy(clSelection, "UPDATE JOBTAB SET UGHS=:VUGHS,UDEM=:VUDEM,FCCO=:VFCCO,ALOC=:VALOC,ALID=:VALID WHERE URNO=:VURNO");
			}
#else
			/*FYA v1.26c UFIS-8297*/
			if(igMultiHopo == TRUE)
			{
                strcpy(clSelection, "UPDATE JOBTAB SET UGHS=?,UDEM=?,FCCO=?,ALOC=?,ALID=? WHERE URNO=? AND HOPO IN ('***',':VHOPO')");
			}
			else
			{
                strcpy(clSelection, "UPDATE JOBTAB SET UGHS=?,UDEM=?,FCCO=?,ALOC=?,ALID=? WHERE URNO=?");
			}
#endif

            while (CEDAArrayGetRowPointer(&(rlJobArr.rrArrayHdl), rlJobArr.crArrayName,
                    llJobRow, (void *) &pclJob) == RC_SUCCESS) {
                clUdem[0] = clFcco[0] = clUghs[0] = clAlid[0] = clAloc[0] = '\0';
                pclUjob = pclJob + rlJobArr.plrArrayFldOfs[0];
                llJodRow = ARR_FIRST;
                if (CEDAArrayFindRowPointer(&(rlJodArr.rrArrayHdl), rlJodArr.crArrayName,
                        &(rlJodArr.rrIdx01Hdl), rlJodArr.crIdx01Name,
                        pclUjob, &llJodRow, (void *) &pclRowJod) == RC_SUCCESS) {
                    strncpy(clUdem, pclRowJod + rlJodArr.plrArrayFldOfs[1], 10);
                    clUdem[10] = '\0';
                }
                if (!IS_EMPTY(clUdem)) {
                    llDemRow = ARR_FIRST;
                    ilRc1 = CEDAArrayFindRowPointer(&(rlDemArr.rrArrayHdl), rlDemArr.crArrayName,
                            &(rlDemArr.rrIdx01Hdl), rlDemArr.crIdx01Name,
                            clUdem, &llDemRow, (void *) &pclDemand);
                    if (ilRc1 == RC_SUCCESS) {
                        strncpy(clUghs, pclDemand + rlDemArr.plrArrayFldOfs[2], 10);
                        clUghs[10] = '\0';
                        pclReco = pclDemand + rlDemArr.plrArrayFldOfs[1];
                        if (!IS_EMPTY(pclReco) && (*pclReco != '*')) {
                            strncpy(clFcco, pclReco, 10);
                            clFcco[10] = '\0';
                        }
                    } else
                        dbg(TRACE, "IniJobRedundance: Demand <%s> not found, RC <%d>", clUdem, ilRc1);
                }
                pclUaid = pclJob + rlJobArr.plrArrayFldOfs[4];
                pclUalo = pclJob + rlJobArr.plrArrayFldOfs[3];
                pclUjty = pclJob + rlJobArr.plrArrayFldOfs[8];
                ilRc1 = GetAlidAloc(&rlAloArr, &rlAlidRes, pclUalo, pclUaid, clAloc, clAlid);

                if (!clUdem[0])
                    strcpy(clUdem, " ");
                if (!clUghs[0])
                    strcpy(clUghs, " ");
                if (!clFcco[0])
                    strcpy(clFcco, " ");
                if (!clAloc[0])
                    strcpy(clAloc, " ");
                if (!clAlid[0])
                    strcpy(clAlid, " ");

				/*FYA v1.26c UFIS-8297*/
				if(igMultiHopo == TRUE)
				{
                    dbg(DEBUG, "UJOB <%s> UJTY <%s> UDEM <%s> FCCO <%s> UGHS <%s> ALOC <%s> ALID <%s> HOPO<%s> RC <%d>",
                        pclUjob, pclUjty, clUdem, clFcco, clUghs, clAloc, clAlid, cgHopo, ilRc1);
				}
				else
				{
                    dbg(DEBUG, "UJOB <%s> UJTY <%s> UDEM <%s> FCCO <%s> UGHS <%s> ALOC <%s> ALID <%s> RC <%d>",
                        pclUjob, pclUjty, clUdem, clFcco, clUghs, clAloc, clAlid, ilRc1);
				}

                /*FYA v1.26c UFIS-8297*/
				if(igMultiHopo == TRUE)
				{
                    sprintf(clBuffer, "%s,%s,%s,%s,%s,%s,%s", clUghs, clUdem, clFcco, clAloc, clAlid, pclUjob, cgHopo);
				}
				else
				{
                    sprintf(clBuffer, "%s,%s,%s,%s,%s,%s", clUghs, clUdem, clFcco, clAloc, clAlid, pclUjob);
				}

                delton(clBuffer);
                llDone++;

                ilRc1 = sql_if(IGNORE, &slCursor, clSelection, clBuffer);
                if (ilRc1 != DB_SUCCESS) {
                    get_ora_err(ilRc1, clBuffer);
                    dbg(TRACE, "IniJobRedundance: ORA-ERR <%d> <%s>\nSqlBuf <%s>", ilRc1, clBuffer, clSelection);
                } else
                    dbg(TRACE, "IniJobRedundance: Update <%s> done (%d/%ld) RC <%d> ", pclUjob, llDone, llRowCnt, ilRc1);
                llJobRow = ARR_NEXT;
            }
            strcpy(clStart, clActDate);
            commit_work();
            close_my_cursor(&slCursor);
        } while (strcmp(clActDate, clEnd) < 0);

    }
    if (rlDemArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlDemArr.rrArrayHdl), rlDemArr.crArrayName);
    if (rlJobArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlJobArr.rrArrayHdl), rlJobArr.crArrayName);
    if (rlJodArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlJodArr.rrArrayHdl), rlJodArr.crArrayName);
    if (rlAloArr.rrArrayHdl > -1)
        CEDAArrayDestroy(&(rlAloArr.rrArrayHdl), rlAloArr.crArrayName);
    if (rlAlidRes.rrArrayHdl > -1)
        AATArrayDestroy(&(rlAlidRes.rrArrayHdl), rlAlidRes.crArrayName);

    dbg(TRACE, "========== IniJobRedundance finished RC <%d> ==========", ilRc);
    return ilRc;
}

static int GetAlidAloc(ARRAYINFO *prpAloArray, ARRAYINFO *prpAlidResults,
        char *pcpUalo, char *pcpUaid, char *pcpAloc, char *pcpAlid) {
    char clKey[60], clReft[11];
    long llRowNum = ARR_FIRST;
    char *pclAlidRes, *pclAloRes;
    BOOL blAlocFound = FALSE;
    int ilRc = RC_SUCCESS, ilRc1;

    if (!pcpUalo || !pcpUaid || !pcpAloc || !pcpAlid)
        return RC_INVALID;

    pcpAloc[0] = pcpAlid[0] = '\0';

    if (IS_EMPTY(pcpUalo))
        return RC_NODATA;

    dbg(DEBUG, "GetAlidAloc: Start UALO <%s> UAID <%s>", pcpUalo, pcpUaid);

    if (!IS_EMPTY(pcpUaid)) {
        sprintf(clKey, "%s,%s", pcpUalo, pcpUaid);
        if (AATArrayFindRowPointer(&(prpAlidResults->rrArrayHdl), prpAlidResults->crArrayName,
                &(prpAlidResults->rrIdx01Hdl), prpAlidResults->crIdx01Name,
                clKey, &llRowNum, (void *) &pclAlidRes) == RC_SUCCESS) {
            strcpy(pcpAloc, pclAlidRes + prpAlidResults->plrArrayFldOfs[2]);
            strcpy(pcpAlid, pclAlidRes + prpAlidResults->plrArrayFldOfs[3]);
            blAlocFound = TRUE;
            dbg(DEBUG, "GetAlidAloc: found ALOC <%s> ALID <%s> with key <%s> in Results",
                    pcpAloc, pcpAlid, clKey);
        }
    }
    if (!blAlocFound) {
        llRowNum = ARR_FIRST;
        ilRc = CEDAArrayFindRowPointer(&(prpAloArray->rrArrayHdl), prpAloArray->crArrayName,
                &(prpAloArray->rrIdx01Hdl), prpAloArray->crIdx01Name,
                pcpUalo, &llRowNum, (void *) &pclAloRes);
        if (ilRc == RC_SUCCESS) {
            strcpy(clReft, pclAloRes + prpAloArray->plrArrayFldOfs[2]);
            strcpy(pcpAloc, pclAloRes + prpAloArray->plrArrayFldOfs[1]);
            dbg(DEBUG, "GetAlidAloc: found ALOC <%s> REFT <%s> with key <%s> in ALOTAB",
                    pcpAloc, clReft, pcpUalo);
            if (!IS_EMPTY(pcpUaid) && (atol(pcpUaid) > 0)) {
                ilRc1 = GetNameFromBasicData(clReft, pcpUaid, pcpAlid);
                if ((ilRc1 == RC_SUCCESS) && !IS_EMPTY(pcpAlid)) {
                    memset(clKey, ' ', sizeof (clKey));
                    sprintf(clKey, "%s,%s,%s,%s", pcpUalo, pcpUaid, pcpAloc, pcpAlid);
                    dbg(DEBUG, "GetAlidAloc: Going to add row <%s> to AlidResults", clKey);
                    delton(clKey);
                    llRowNum = ARR_NEXT;
                    ilRc = AATArrayAddRow(&(prpAlidResults->rrArrayHdl),
                            prpAlidResults->crArrayName, &llRowNum, (void*) clKey);
                    if (ilRc != RC_SUCCESS)
                        dbg(TRACE, "GetAlidAloc: AATArrayAddRow failed RC <%d>", ilRc);
                } else {
                    dbg(DEBUG, "GetAlidAloc: No ALOC found for REFT <%s> UAID <%s> in DB, RC <%d>",
                            clReft, pcpUaid, ilRc1);
                    ilRc = RC_NOTFOUND;
                }
            }
        } else
            dbg(DEBUG, "GetAlidAloc: UALO <%s> not found in ALOTAB, RC <%d>", pcpUalo, ilRc);

    }
    return ilRc;
}

static void DebugPrintArrayInfo(int ipDebugLevel, ARRAYINFO *prpArrayInfo) {
    int ilLoop = 0;

    if (prpArrayInfo == NULL) {
        return;
    }

    dbg(ipDebugLevel, "ArrayInfo: Name <%s> Handle <%d>", prpArrayInfo->crArrayName, prpArrayInfo->rrArrayHdl);
    dbg(ipDebugLevel, "ArrayInfo: FieldCnt <%d> FieldList <%s>", prpArrayInfo->lrArrayFldCnt, prpArrayInfo->crArrayFldList);

    for (ilLoop = 0; ilLoop < prpArrayInfo->lrArrayFldCnt; ilLoop++) {
        dbg(ipDebugLevel, "ArrayInfo: Fld <%d> Len[%02ld] Ofs [%02ld]",
                ilLoop, prpArrayInfo->plrArrayFldLen[ilLoop], prpArrayInfo->plrArrayFldOfs[ilLoop]);
    }

    dbg(ipDebugLevel, "ArrayInfo: Idx01Name <%s> Idx01Handle <%d>",
            prpArrayInfo->crIdx01Name, prpArrayInfo->rrIdx01Hdl);

} /* end of DebugPrintArrayInfo */

static int GetStartEnd(BOOL bpStart, char *pcpTime) {
    int ilRc = RC_SUCCESS;
    char clSqlBuf[128], clDataArea[128] = "";
    short slCursor = 0;
    char clText[11];

    strcpy(clText, bpStart ? "Start" : "End");
    if (!pcpTime)
        return RC_INVALID;

    if (ReadConfigEntry("JOBTAB_RED", clText, clDataArea) != RC_SUCCESS) {
        slCursor = 0;
        clDataArea[0] = '\0';

		/*FYA v1.26c UFIS-8297*/
        if(igMultiHopo == TRUE)
        {
            sprintf(clSqlBuf, "SELECT %s(ACTO) FROM JOBTAB WHERE ACTO > '0' AND HOPO IN ('***','%s')", bpStart ? "MIN" : "MAX", cgHopo);
        }
        else
        {
            sprintf(clSqlBuf, "SELECT %s(ACTO) FROM JOBTAB WHERE ACTO > '0'", bpStart ? "MIN" : "MAX");
        }

        ilRc = sql_if(START, &slCursor, clSqlBuf, clDataArea);
        if (ilRc != DB_SUCCESS) {
            dbg(TRACE, "GetStartEnd: SQL-statement <%s> failed, RC <%d>", clSqlBuf, ilRc);
        } else
            dbg(DEBUG, "GetStartEnd: SQL-statement <%s> Data <%s> OK", clSqlBuf, clDataArea);
        close_my_cursor(&slCursor);
    }
    if (ilRc == RC_SUCCESS) {
        clDataArea[14] = '\0';
        strcpy(pcpTime, clDataArea);
        if (strlen(pcpTime) < 8) {
            dbg(TRACE, "GetStartEnd: Invalid %s date <%s>", clText, pcpTime);
            ilRc = RC_INVALID;
        } else if (strlen(pcpTime) < 14) {
            strcat(pcpTime, "000000");
            pcpTime[14] = '\0';
        }
    }
    dbg(DEBUG, "GetStartEnd: [%s] Time <%s> RC <%d>", clText, pcpTime, ilRc);
    return ilRc;
}

static int TimeToStr(char *pcpTime, time_t lpTime) {
    struct tm *_tm;

    _tm = (struct tm *) gmtime(&lpTime);
    sprintf(pcpTime, "%4d%02d%02d%02d%02d%02d",
            _tm->tm_year + 1900, _tm->tm_mon + 1, _tm->tm_mday, _tm->tm_hour, _tm->tm_min, _tm->tm_sec);

    return RC_SUCCESS;
} /* End of TimeToStr */

static int GetPrflConfig() {
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "GetPrflConfig:";
    char pclTmpBuf[128];

    ilRC = TimeToStr(pcgWaitTill, time(NULL));
    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "WAIT_AFTER_RESTART", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf, "-1") != 0)
        ilRC = AddSecondsToCEDATime(pcgWaitTill, atoi(pclTmpBuf)*60, 1);

    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "LAST_DAY", CFG_STRING, pcgLastDay);
    if (ilRC != RC_SUCCESS)
        strcpy(pcgLastDay, "20050101");

    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "TRIGGER_ACTION", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf, "YES") == 0)
        igTriggerAction = TRUE;
    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "TRIGGER_BCHDL", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf, "YES") == 0)
        igTriggerBchdl = TRUE;

    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "BC_OUT_MODE", CFG_STRING, pclTmpBuf);
    if (ilRC == RC_SUCCESS)
        igBcOutMode = atoi(pclTmpBuf);

    dbg(TRACE, "%s Wait after restart for CPRF Command till %s UTC", pclFunc, pcgWaitTill);
    dbg(TRACE, "%s Last Day in History = %s", pclFunc, pcgLastDay);
    dbg(TRACE, "%s TRIGGER_ACTION = %d", pclFunc, igTriggerAction);
    dbg(TRACE, "%s TRIGGER_BCHDL = %d", pclFunc, igTriggerBchdl);
    dbg(TRACE, "%s BC_OUT_MODE = %d", pclFunc, igBcOutMode);

    return RC_SUCCESS;
} /* End of GetPrflConfig */

static int TriggerBchdlAction(char *pcpCmd, char *pcpTable, char *pcpSelection,
        char *pcpFields, char *pcpData, int ipBchdl, int ipAction) {
    int ilRC = RC_SUCCESS;

    if (ipBchdl == TRUE && igTriggerBchdl == TRUE) {
        (void) tools_send_info_flag(1900, 0, "pedhdl", "", "CEDA", "", "", pcgTwStart, pcgTwEnd,
                pcpCmd, pcpTable, pcpSelection, pcpFields, pcpData, 0);
        dbg(DEBUG, "Send Broadcast: <%s><%s><%s><%s><%s>",
                pcpCmd, pcpTable, pcpSelection, pcpFields, pcpData);
    }
    if (ipAction == TRUE && igTriggerAction == TRUE) {
        (void) tools_send_info_flag(7400, 0, "pedhdl", "", "CEDA", "", "", pcgTwStart, pcgTwEnd,
                pcpCmd, pcpTable, pcpSelection, pcpFields, pcpData, 0);
        dbg(DEBUG, "Send to ACTION: <%s><%s><%s><%s><%s>",
                pcpCmd, pcpTable, pcpSelection, pcpFields, pcpData);
    }

    return ilRC;
} /* End of TriggerBchdlAction */

static int HandleCPRF() {
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "HandleCPRF:";
    int ilRCdb = DB_SUCCESS;
    short slFkt;
    short slCursor;
    char pclSqlBuf[2048];
    char pclDataBuf[8000];
    char pclSelection[1024];
    char pclFieldList[1024];
    int ilRCdbWr = DB_SUCCESS;
    short slFktWr;
    short slCursorWr;
    char pclSqlBufWr[2048];
    char pclDataBufWr[8000];
    char pclSelectionWr[1024];
    int ilI;
    int ilNoEle;
    char pclParamList[1024];
    char pclCurPar[64];
    char pclAdid[16];
    char pclFtyp[64];
    char pclField[16];
  	char pclTime[16] = "\0";
    int ilConfigError;
    char *pclTmpPtr;
    char pclTmpBuf[1024];
    char pclLastTime[16];
    int ilCount;

    /*FYA v1.26d UFIS-8325
    char pclUrno[16];*/
    char pclUrno[URNO_LEN] = "\0";


  short slVafrDay;
  short slVatoDay;
  short slBaseDay;
  char pclCurrentTime[20] = "\0";
  char pclVafrDay[20] = "\0";
  char pclVatoDay[20] = "\0";

  int ilVafrDef = TRUE;
  int ilVatoDef = TRUE;
  char pclTimStart[20] = "\0";
  char pclTimEnd[20] = "\0";


    ilRC = iGetConfigEntry(cgConfigFile, "PRFLHD", "PARAM_LIST", CFG_STRING, pclParamList);
    if (ilRC != RC_SUCCESS) {
        dbg(TRACE, "%s No Parameter List defined", pclFunc);
        return ilRC;
    }
    ilNoEle = GetNoOfElements(pclParamList, ',');
    for (ilI = 1; ilI <= ilNoEle; ilI++) {
        ilConfigError = FALSE;
        get_real_item(pclCurPar, pclParamList, ilI);
        ilRC = iGetConfigEntry(cgConfigFile, pclCurPar, "ADID", CFG_STRING, pclAdid);
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "%s Parameter <ADID> not defined in Section <%s>", pclFunc, pclCurPar);
            ilConfigError = TRUE;
        }
        ilRC = iGetConfigEntry(cgConfigFile, pclCurPar, "FTYP", CFG_STRING, pclFtyp);
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "%s Parameter <FTYP> not defined in Section <%s>", pclFunc, pclCurPar);
            ilConfigError = TRUE;
        }
        ilRC = iGetConfigEntry(cgConfigFile, pclCurPar, "FLD", CFG_STRING, pclField);
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "%s Parameter <FLD> not defined in Section <%s>", pclFunc, pclCurPar);
            ilConfigError = TRUE;
        }

     ilRC = iGetConfigEntry(cgConfigFile,pclCurPar,"BASE_DAY",CFG_STRING,pclTmpBuf);
     slBaseDay = atoi(pclTmpBuf);
     if (ilRC != RC_SUCCESS)
     {
       /* dbg(TRACE,"%s Parameter <BASE_DAY> not defined in Section <%s>",pclFunc,pclCurPar); */
       /* ilConfigError = TRUE; */
     }
     ilRC = iGetConfigEntry(cgConfigFile,pclCurPar,"VAFR_DAY",CFG_STRING,pclTmpBuf);
     slVafrDay = atoi(pclTmpBuf);
     if (ilRC != RC_SUCCESS)
     {
        dbg(DEBUG,"%s Parameter <VAFR_DAY> not defined in Section <%s>",pclFunc,pclCurPar);
        ilVafrDef = FALSE;
     }
     ilRC = iGetConfigEntry(cgConfigFile,pclCurPar,"VATO_DAY",CFG_STRING,pclTmpBuf);
     slVatoDay = atoi(pclTmpBuf);
     if (ilRC != RC_SUCCESS)
     {
        dbg(DEBUG,"%s Parameter <VATO_DAY> not defined in Section <%s>",pclFunc,pclCurPar);
        ilVatoDef = FALSE;
     }

        ilRC = iGetConfigEntry(cgConfigFile, pclCurPar, "TIME", CFG_STRING, pclTime);
        if (ilRC != RC_SUCCESS) {
            dbg(TRACE, "%s Parameter <TIME> not defined in Section <%s>", pclFunc, pclCurPar);
        if (ilVatoDef == FALSE)
            ilConfigError = TRUE;
        }
        if (ilConfigError == FALSE) {
            pclTmpPtr = strstr(pclFtyp, "P");
            if (pclTmpPtr != NULL)
                *pclTmpPtr = ' ';
            memset(pclTmpBuf, 0x00, 64);
            pclTmpPtr = pclFtyp;
            while (*pclTmpPtr != '\0') {
                strcat(pclTmpBuf, "'");
                strncat(pclTmpBuf, pclTmpPtr, 1);
                strcat(pclTmpBuf, "',");
                pclTmpPtr++;
                if (*pclTmpPtr != '\0')
                    pclTmpPtr++;
            }
            if (strlen(pclTmpBuf) > 0) {
                pclTmpBuf[strlen(pclTmpBuf) - 1] = '\0';
                strcpy(pclFtyp, pclTmpBuf);
            }
            ilCount = 0;

        GetServerTimeStamp("UTC", 1, 0, pclTmpBuf);
        sprintf(pclCurrentTime, "%8.8s%6.6d", pclTmpBuf, 0 );

        if (ilVafrDef == TRUE)
        {
            strcpy(pclVafrDay, pclCurrentTime );
            (void) AddSecondsToCEDATime(pclVafrDay,slVafrDay*24*60*60,1) ;
            strcpy(pclTimStart, pclVafrDay);
        }
        else
            sprintf(pclTimStart, "%s000000", pcgLastDay);

        if (ilVatoDef == TRUE)
        {
            strcpy(pclVatoDay, pclCurrentTime );
            (void) AddSecondsToCEDATime(pclVatoDay,slVatoDay*24*60*60,1) ;
            strcpy(pclTimEnd, pclVatoDay);
        }
        else
        {
            GetServerTimeStamp("UTC", 1, 0, pclTmpBuf);
            strcpy(pclLastTime, pclTmpBuf);
            (void) AddSecondsToCEDATime(pclLastTime,atoi(pclTime)*60*(-1),1);
            strcpy(pclTimEnd, pclLastTime);
        }

            strcpy(pclFieldList, "URNO");

			/*FYA v1.26c UFIS-8297*/
			if(igMultiHopo == TRUE)
			{
                sprintf(pclSelection,"WHERE %s BETWEEN '%s' AND '%s' AND ADID = '%s' AND FTYP IN (%s) AND HOPO IN ('***','%s')",
                    pclField,pclTimStart,pclTimEnd,pclAdid,pclFtyp,cgHopo);
			}
			else
			{
                sprintf(pclSelection,"WHERE %s BETWEEN '%s' AND '%s' AND ADID = '%s' AND FTYP IN (%s)",
                    pclField,pclTimStart,pclTimEnd,pclAdid,pclFtyp);
			}

			strcat(pclSelection, " AND PRFL NOT IN ('C','U')");
            sprintf(pclSqlBuf, "SELECT %s FROM AFTTAB %s", pclFieldList, pclSelection);
            slCursor = 0;
            slFkt = START;
            dbg(DEBUG, "%s SQL = <%s>", pclFunc, pclSqlBuf);
            ilRCdb = sql_if(slFkt, &slCursor, pclSqlBuf, pclUrno);
            while (ilRCdb == DB_SUCCESS) {
                ilCount++;

				/*FYA v1.26c UFIS-8297*/
                if(igMultiHopo == TRUE)
                {
                    sprintf(pclSqlBufWr, "UPDATE AFTTAB SET PRFL = 'C' WHERE URNO = %s AND HOPO IN ('***','%s')", pclUrno, cgHopo);
                }
                else
                {
                    sprintf(pclSqlBufWr, "UPDATE AFTTAB SET PRFL = 'C' WHERE URNO = %s", pclUrno);
                }


                slCursorWr = 0;
                slFktWr = START;
                ilRCdbWr = sql_if(slFktWr, &slCursorWr, pclSqlBufWr, pclDataBufWr);
                if (ilRCdbWr == DB_SUCCESS) {
                    commit_work();

					/*FYA v1.26c UFIS-8297*/
                    if(igMultiHopo == TRUE)
                    {
                        sprintf(pclSelectionWr, "WHERE URNO = %s AND HOPO IN('***','%s')", pclUrno, cgHopo);
                    }
                    else
                    {
                        sprintf(pclSelectionWr, "WHERE URNO = %s", pclUrno);
                    }


                    if (igBcOutMode == 0)
                        ilRC = TriggerBchdlAction("UFR", "AFTTAB", pclSelectionWr, "PRFL", "C", TRUE, TRUE);
                    else if (igBcOutMode == 1)
                        ilRC = TriggerBchdlAction("UFR", "AFTTAB", pclSelectionWr, "PRFL,#PRFL", "C, ", TRUE, TRUE);
                    else if (igBcOutMode == 2)
                        ilRC = TriggerBchdlAction("UFR", "AFTTAB", pclSelectionWr, "PRFL,[CF],PRFL", "C,[CF],C", TRUE, TRUE);
                }
                close_my_cursor(&slCursorWr);
                slFkt = NEXT;
                ilRCdb = sql_if(slFkt, &slCursor, pclSqlBuf, pclUrno);
            }
            close_my_cursor(&slCursor);
            dbg(DEBUG, "%s Section: %s , %d Records updated", pclFunc, pclCurPar, ilCount);
        }
    }

    return ilRC;
} /* End of HandleCPRF */

static int HandleSETOAL() {
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "HandleSETOAL:";
    char pclCurrentTime[16];
    char pclSetTime[16];
    long llRowNum;
    char *pclRowBuf;
    long llFieldNo = -1;

    /*FYA v1.26d UFIS-8325
    char pclUrno[16];*/
    char pclUrno[URNO_LEN] = "\0";

    char pclFlno[16];
    char pclStoa[16];
    char pclLand[16];
    char pclOnbl[16];
    char pclSelection[128];

    GetServerTimeStamp("UTC", 1, 0, pclCurrentTime);
    strcpy(pclSetTime, pclCurrentTime);
    AddSecondsToCEDATime(pclSetTime, igSetOnblAfterLand * 60 * (-1), 1);
    llRowNum = ARR_FIRST;
    while (CEDAArrayGetRowPointer(&rgAftArrayInbound.rrArrayHandle,
            rgAftArrayInbound.crArrayName, llRowNum, (void *) &pclRowBuf) == RC_SUCCESS) {

        llFieldNo = -1;
        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llFieldNo, "URNO", 12, ARR_CURRENT, pclUrno);
        StringTrimRight(pclUrno);
        llFieldNo = -1;
        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llFieldNo, "FLNO", 10, ARR_CURRENT, pclFlno);
        StringTrimRight(pclFlno);
        llFieldNo = -1;
        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llFieldNo, "STOA", 15, ARR_CURRENT, pclStoa);
        StringTrimRight(pclStoa);
        llFieldNo = -1;
        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llFieldNo, "LAND", 15, ARR_CURRENT, pclLand);
        StringTrimRight(pclLand);
        llFieldNo = -1;
        CEDAArrayGetField(&rgAftArrayInbound.rrArrayHandle, rgAftArrayInbound.crArrayName,
                &llFieldNo, "ONBL", 15, ARR_CURRENT, pclOnbl);
        StringTrimRight(pclOnbl);
        if (*pclLand != ' ' && *pclOnbl == ' ') {
            dbg(DEBUG, "%s FLIGHT = <%s> <%s> <%s> <%s> <%s>", pclFunc, pclUrno, pclFlno, pclStoa, pclLand, pclOnbl);
            if (strcmp(pclLand, pclSetTime) <= 0) {
                dbg(DEBUG, "%s Set ONBL now for Flight <%s>, LAND = <%s>, NOW = <%s>",
                        pclFunc, pclFlno, pclLand, pclCurrentTime);

				/*FYA v1.26c UFIS-8297*/
				if(igMultiHopo == TRUE)
				{
                    sprintf(pclSelection, "WHERE URNO = %s AND HOPO IN ('***','%s')", pclUrno, cgHopo);
				}
				else
				{
                    sprintf(pclSelection, "WHERE URNO = %s", pclUrno);
				}

				/*FYA v1.26c UFIS-8297*/
                /*tools_send_info_flag(igModIdFlight, mod_id, "PDEHDL", "", "", "", "", "", "",
                        "UFR", "AFTTAB", pclSelection, pcgOnblField, pclCurrentTime, 0);*/
				tools_send_info_flag(igModIdFlight, mod_id, "PDEHDL", "", "", "", "", "", pcgTwEnd,
                        "UFR", "AFTTAB", pclSelection, pcgOnblField, pclCurrentTime, 0);
            }
        }
        llRowNum = ARR_NEXT;
    }

    return ilRC;
} /* End of HandleSETOAL */

static int SendPortal() {
    char pclSqlBuf[1024];
    char pclNow[20] = "\0";
    char pclFrom[20] = "\0";
    char pclTo[20] = "\0";
    char pclTmpStr[128] = "\0";
    char *pclFunc = "SendPortal";

    GetServerTimeStamp("UTC", 1, 0, pclNow);
    dbg(TRACE, "%s: Current UTC time <%s>", pclFunc, pclNow);

    /* Arrival part */
    strcpy(pclFrom, pclNow);
    strcpy(pclTo, pclNow);
    AddSecondsToCEDATime(pclFrom, igFrom_In * 60, 1);
    AddSecondsToCEDATime(pclTo, igTo_In * 60, 1);

	/*FYA v1.26c UFIS-8297*/
	if(igMultiHopo == TRUE)
	{
	    sprintf(pclSqlBuf, "SELECT %s FROM AFTTAB WHERE ADID = 'A' AND STOA BETWEEN '%s' AND '%s' AND FTYP in (%s) AND HOPO IN ('***','%s')",cgField_In, pclFrom, pclTo, pcgFtyp_In, cgHopo);

	}
	else
	{
        sprintf(pclSqlBuf, "SELECT %s FROM AFTTAB WHERE ADID = 'A' AND STOA BETWEEN '%s' AND '%s' AND FTYP in (%s)",
            cgField_In, pclFrom, pclTo, pcgFtyp_In);
	}

    /* For test */
    /*
    sprintf( pclSqlBuf, "SELECT %s FROM AFTTAB WHERE ADID = 'A' AND STOA LIKE '201203240105%%' AND FTYP in (%s)",
    cgField_In, pcgFtyp_In );
     */
    GetAndSendRecordsForPortal(pclSqlBuf, igURNO_In, igNumFields_In, "ARRIVAL");

    /* Departure part */
    memset(pclSqlBuf, 0, sizeof (pclSqlBuf));
    memset(pclFrom, 0, sizeof (pclFrom));
    memset(pclTo, 0, sizeof (pclTo));

    strcpy(pclFrom, pclNow);
    strcpy(pclTo, pclNow);
    AddSecondsToCEDATime(pclFrom, igFrom_Out * 60, 1);
    AddSecondsToCEDATime(pclTo, igTo_Out * 60, 1);

    strcpy(pclTmpStr, "ADID = 'D'");

    /* if( cgIncludeReturn == TRUE )    */
    /*    strcpy( pclTmpStr, "ADID <> 'A'" ); */

	/*FYA v1.26c UFIS-8297*/
    if(igMultiHopo == TRUE)
    {
        sprintf(pclSqlBuf, "SELECT %s FROM AFTTAB WHERE %s AND TIFD BETWEEN '%s' AND '%s' AND FTYP in (%s) AND HOPO IN ('***','%s')",cgField_Out, pclTmpStr, pclFrom, pclTo, pcgFtyp_Out, cgHopo);
    }
    else
    {
        sprintf(pclSqlBuf, "SELECT %s FROM AFTTAB WHERE %s AND TIFD BETWEEN '%s' AND '%s' AND FTYP in (%s)",
            cgField_Out, pclTmpStr, pclFrom, pclTo, pcgFtyp_Out);
    }

    /*
      For Test */
    /*
    sprintf( pclSqlBuf, "SELECT %s FROM AFTTAB WHERE ADID = 'D' AND STOD='20120324160500' AND FTYP in (%s)",
    cgField_Out, pcgFtyp_Out );
     */
    GetAndSendRecordsForPortal(pclSqlBuf, igURNO_Out, igNumFields_Out, "DEPARTURE");

    dbg(TRACE, "%s: Portal sending completed.", pclFunc);
}

static int GetAndSendRecordsForPortal(char *pcpSqlBuf, int ipUaftIdx, int ipNumFields, char *pcpFlightInOut) {
    int ilRc;
    int ilCount;
    int blArrival;
    short slSqlCursor;
    short slSqlFunc;
    char pclSqlBuf[1024];
    char pclSqlData[8192] = "\0";
    char pclSelection[1024] = "\0";

    /*FYA v1.26d UFIS-8325
    char pclUaft[20] = "\0";*/
    char pclUaft[URNO_LEN] = "\0";

    char *pclFunc = "GetAndSendRecordsForPortal";

    strcpy(pclSqlBuf, pcpSqlBuf);
    dbg(TRACE, "%s: Executing SQL <%s> #Fields <%d>", pclFunc, pclSqlBuf, ipNumFields);

    blArrival = FALSE;
    if (!strcmp(pcpFlightInOut, "ARRIVAL"))
        blArrival = TRUE;

    slSqlCursor = 0;
    slSqlFunc = START;
    ilCount = 0;

    dbg(DEBUG, "%s igSendToModIdForPortal<%d>", pclFunc, igSendToModIdForPortal);
    dbg(DEBUG, "%s mod_id<%d>", pclFunc, mod_id);
    dbg(DEBUG, "%s mod_name<%s>", pclFunc, mod_name);
    dbg(DEBUG, "%s cgCommand_In<%s>", pclFunc, cgCommand_In);
    dbg(DEBUG, "%s cgCommand_Out<%s>", pclFunc, cgCommand_Out);
    memset(pclSqlData, 0, sizeof (pclSqlData));

    while (sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS) {
        /* memset(pclSqlData,0,sizeof(pclSqlData));  */
        slSqlFunc = NEXT;
        ilCount++;
        if (ipUaftIdx >= 0) {
            memset(pclUaft, 0, sizeof (pclUaft));
            get_fld(pclSqlData, ipUaftIdx, STR, 20, pclUaft);
            sprintf(pclSelection, "WHERE URNO = %s", pclUaft);
        }
        dbg(DEBUG, "%s pclSelection<%s>", pclFunc, pclSelection);
        BuildItemBuffer(pclSqlData, "", ipNumFields, ",");

        dbg(DEBUG, "%s: Rec %2d :- DATA <%s>", pclFunc, ilCount, pclSqlData);
        if (blArrival == TRUE)
		{
			/*FYA v1.26c UFIS-8297*/
            /*tools_send_info_flag(igSendToModIdForPortal, mod_id, mod_name, "", "", "", "", "", "",
                cgCommand_In, "AFTTAB", pclSelection, cgField_In,
                pclSqlData, NETOUT_NO_ACK);*/
			tools_send_info_flag(igSendToModIdForPortal, mod_id, mod_name, "", "", "", "", "", pcgTwEnd,
                cgCommand_In, "AFTTAB", pclSelection, cgField_In,
                pclSqlData, NETOUT_NO_ACK);
		}
        else
		{
			/*FYA v1.26c UFIS-8297*/
            /*tools_send_info_flag(igSendToModIdForPortal, mod_id, mod_name, "", "", "", "", "", "",
                cgCommand_Out, "AFTTAB", pclSelection, cgField_Out,
                pclSqlData, NETOUT_NO_ACK);*/
			tools_send_info_flag(igSendToModIdForPortal, mod_id, mod_name, "", "", "", "", "", pcgTwEnd,
                cgCommand_In, "AFTTAB", pclSelection, cgField_In,
                pclSqlData, NETOUT_NO_ACK);
		}
        memset(pclSqlData, 0, sizeof (pclSqlData));
    }
    close_my_cursor(&slSqlCursor);
    dbg(TRACE, "%s: Records sent <%d>", pclFunc, ilCount);

} /*GetAndSendRecordsForPortal*/

static int SendBatch(int igCmd, char *pcpData) {
    char pclSqlBuf[1024];
    char pclNow[20] = "\0";
    char pclFrom[20] = "\0";
    char pclTo[20] = "\0";
    char pclTmpStr[128] = "\0";
    char pclSpecialSelect[1024] = "\0";
    char *pclFunc = "SendBatch";
	/********BHS 3279 v1.24*******/
	char pclVafr[16]="\0";
	/********BHS 3279 v1.24*******/


    GetServerTimeStamp("UTC", 1, 0, pclNow);
    dbg(TRACE, "%s: Current UTC time <%s>", pclFunc, pclNow);
    /*********BHS****************/
	if( igCmd == CMD_RESEND )
	{
		if(get_no_of_items(pcpData)==1)
		{
		/********BHS 3279 v1.24*******/
			/*strcpy(pclFrom,pcpData);
			strcpy(pclTo,pcpData);*/
		   if(strlen(pcpData)>=8)
			{
			strncpy(pclVafr,pcpData,8);
			pclVafr[8]='\0';
			sprintf(pclFrom,"%s000000",pclVafr);
			strcpy(pclTo,pclFrom);
			dbg( DEBUG, "%s: pclFrom <%s> pclVafr<%s>", pclFunc, pclFrom ,pclVafr);
			dbg( DEBUG, "%s: pclTo <%s>", pclFunc, pclTo );
			}
			else
			{
			dbg( TRACE, "%s: Date <%s> should be atleast of 8 characters", pclFunc, pcpData );
			}
		/********BHS 3279 v1.24*******/

		}
		else
		{
            strcpy(pclFrom, GetDataField(pcpData, 0, ','));
            strcpy(pclTo, GetDataField(pcpData, 1, ','));
        }
    } else {
        /* Arrival part */
        strcpy(pclFrom, pclNow);
        strcpy(pclTo, pclNow);
    }
    strcpy(pclSpecialSelect, " ");

    if (igCmd == CMD_BATCH_EFPS) {
        AddSecondsToCEDATime(pclFrom, igSpecialRangeFromInbound * 60, 1);
        AddSecondsToCEDATime(pclTo, igSpecialRangeToInbound * 60, 1);
        /*sprintf(pclSpecialSelect, "AND URNO NOT IN (SELECT URNO FROM EFPTAB WHERE (USEC BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s'))",
                pclFrom, pclTo, pclFrom, pclTo);*/

        /*v1.26b fya "NOT IN"->"IN"; USEC->CDAT*/
        /*sprintf(pclSpecialSelect, "AND URNO IN (SELECT URNO FROM EFPTAB WHERE (CDAT BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s'))",
                pclFrom, pclTo, pclFrom, pclTo);*/

		/*FYA v1.26c UFIS-8297*/
		if(igMultiHopo == TRUE)
		{
		    sprintf(pclSpecialSelect, "AND URNO IN (SELECT URNO FROM EFPTAB WHERE (CDAT BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s') AND HOPO IN ('***','%s'))",pclFrom, pclTo, pclFrom, pclTo, cgHopo);
		}
		else
		{
            sprintf(pclSpecialSelect, "AND URNO IN (SELECT URNO FROM EFPTAB WHERE (CDAT BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s'))",pclFrom, pclTo, pclFrom, pclTo);
		}

    } else {
        AddSecondsToCEDATime(pclFrom, igRangeFromInbound * 60, 1);
		if( igCmd == CMD_RESEND )/********BHS 3279 v1.24*******/
		{
        AddSecondsToCEDATime(pclTo,(igRangeToInbound*60)-1, 1);
		}
		else
		{
        AddSecondsToCEDATime(pclTo, igRangeToInbound * 60, 1);
    }
}
    /*
    sprintf( pclSqlBuf, "SELECT %s FROM HWEVIEW WHERE ADID = 'A' AND ((TIFA BETWEEN '%s' AND '%s') OR (STOA BETWEEN '%s' AND '%s')) "
                        "AND FTYP in (%s) %s",
                        cgFieldListIn, pclFrom, pclTo, pclFrom, pclTo, pcgFtypInbound, pclSpecialSelect );
     */

    /*FYA v1.26c UFIS-8297*/
    if(igMultiHopo == TRUE)
    {
       sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE ADID = 'A' AND ((TIFA BETWEEN '%s' AND '%s') OR (STOA BETWEEN '%s' AND '%s') )"
            "AND FTYP in (%s) %s AND HOPO IN ('***','%s')",
            cgFieldListIn, pcgTableName, pclFrom, pclTo, pclFrom, pclTo, pcgFtypInbound, pclSpecialSelect, cgHopo);
    }
    else
    {
        sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE ADID = 'A' AND ((TIFA BETWEEN '%s' AND '%s') OR (STOA BETWEEN '%s' AND '%s')) "
            "AND FTYP in (%s) %s",
            cgFieldListIn, pcgTableName, pclFrom, pclTo, pclFrom, pclTo, pcgFtypInbound, pclSpecialSelect);
    }

    GetAndSendRecords(pclSqlBuf, igInUaftIdx, igInNumFields, "ARRIVAL");
    /*********BHS****************/
		if( igCmd == CMD_RESEND )
	{
		if(get_no_of_items(pcpData)==1)
		{
   			/********BHS 3279 v1.24*******/
			/*strcpy(pclFrom,pcpData);
			strcpy(pclTo,pcpData);*/
		    if(strlen(pcpData)>=8)
			{
			strncpy(pclVafr,pcpData,8);
			pclVafr[8]='\0';
			sprintf(pclFrom,"%s000000",pclVafr);
			strcpy(pclTo,pclFrom);
			dbg( DEBUG, "%s: pclFrom <%s> pclVafr<%s>", pclFunc, pclFrom ,pclVafr);
			dbg( DEBUG, "%s: pclTo <%s>", pclFunc, pclTo );
			}
			else
			{
			dbg( TRACE, "%s: Date <%s> should be atleast of 8 characters", pclFunc, pcpData );
			}
		/********BHS 3279 v1.24*******/

		}
		else
		{
            strcpy(pclFrom, GetDataField(pcpData, 0, ','));
            strcpy(pclTo, GetDataField(pcpData, 1, ','));
        }
    } else {
        /* Departure part */
        strcpy(pclFrom, pclNow);
        strcpy(pclTo, pclNow);
    }
    strcpy(pclSpecialSelect, " ");
    /*********BHS****************/

    if (igCmd == CMD_BATCH_EFPS) {
        AddSecondsToCEDATime(pclFrom, igSpecialRangeFromOutbound * 60, 1);
        AddSecondsToCEDATime(pclTo, igSpecialRangeToOutbound * 60, 1);
        /*sprintf(pclSpecialSelect, "AND URNO NOT IN (SELECT URNO FROM EFPTAB WHERE (USEC BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s'))",
                pclFrom, pclTo, pclFrom, pclTo);*/

        /*v1.26b fya "NOT IN"->"IN"; USEC->CDAT*/
        if(igMultiHopo == TRUE)
        {
            sprintf(pclSpecialSelect, "AND URNO IN (SELECT URNO FROM EFPTAB WHERE (CDAT BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s') AND HOPO IN ('***','%s'))",pclFrom, pclTo, pclFrom, pclTo, cgHopo);
        }
        else
        {
            sprintf(pclSpecialSelect, "AND URNO IN (SELECT URNO FROM EFPTAB WHERE (CDAT BETWEEN '%s' AND '%s') OR (LSTU BETWEEN '%s' AND '%s'))",pclFrom, pclTo, pclFrom, pclTo);
        }

    } else {
        AddSecondsToCEDATime(pclFrom, igRangeFromOutbound * 60, 1);
		if(igCmd == CMD_RESEND) /********BHS 3279 v1.24*******/
		{
			AddSecondsToCEDATime(pclTo,(igRangeToOutbound*60)-1, 1);
		}
		else
		{
        AddSecondsToCEDATime(pclTo, igRangeToOutbound * 60, 1);
    	}
	}
    strcpy(pclTmpStr, "ADID = 'D'");
    if (cgIncludeReturn == TRUE)
        strcpy(pclTmpStr, "ADID <> 'A'");

    /*
    sprintf( pclSqlBuf, "SELECT %s FROM HWEVIEW WHERE %s AND ((TIFD BETWEEN '%s' AND '%s') OR (STOD BETWEEN '%s' AND '%s')) "
                        "AND FTYP in (%s) %s",
                        cgFieldListOut, pclTmpStr, pclFrom, pclTo, pclFrom, pclTo, pcgFtypOutbound, pclSpecialSelect );
     */

    if(igMultiHopo == TRUE)
    {
        sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE %s AND ((TIFD BETWEEN '%s' AND '%s') OR (STOD BETWEEN '%s' AND '%s')) "
            "AND FTYP in (%s) %s AND HOPO IN ('***','%s')",
            cgFieldListOut, pcgTableName, pclTmpStr, pclFrom, pclTo, pclFrom, pclTo, pcgFtypOutbound, pclSpecialSelect, cgHopo);
    }
    else
    {
        sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE %s AND ((TIFD BETWEEN '%s' AND '%s') OR (STOD BETWEEN '%s' AND '%s')) "
            "AND FTYP in (%s) %s",
            cgFieldListOut, pcgTableName, pclTmpStr, pclFrom, pclTo, pclFrom, pclTo, pcgFtypOutbound, pclSpecialSelect);
    }

    GetAndSendRecords(pclSqlBuf, igOutUaftIdx, igOutNumFields, "DEPARTURE");

    dbg(TRACE, "%s: Batch sending completed.", pclFunc);
} /*SendBatch*/

static int GetAndSendRecords(char *pcpSqlBuf, int ipUaftIdx, int ipNumFields, char *pcpFlightInOut) {
    int ilRc;
    int ilCount;
    int blArrival;
    short slSqlCursor;
    short slSqlFunc;
    char pclSqlBuf[1024];
    char pclSqlData[8192] = "\0";
    char pclSelection[1024] = "\0";

    /*FYA v1.26d UFIS-8325
    char pclUaft[20] = "\0";*/
    char pclUaft[URNO_LEN] = "\0";

    char *pclFunc = "GetAndSendRecords";

    strcpy(pclSqlBuf, pcpSqlBuf);
    dbg(TRACE, "%s: Executing SQL <%s> #Fields <%d>", pclFunc, pclSqlBuf, ipNumFields);

    blArrival = FALSE;
    if (!strcmp(pcpFlightInOut, "ARRIVAL"))
        blArrival = TRUE;

    slSqlCursor = 0;
    slSqlFunc = START;
    ilCount = 0;

    dbg(DEBUG, "%s igSendToModId<%d>", pclFunc, igSendToModId);
    dbg(DEBUG, "%s mod_id<%d>", pclFunc, mod_id);
    dbg(DEBUG, "%s mod_name<%s>", pclFunc, mod_name);
    dbg(DEBUG, "%s cgVdgSendCommandInbound<%s>", pclFunc, cgVdgSendCommandInbound);
    dbg(DEBUG, "%s cgVdgSendCommandOutbound<%s>", pclFunc, cgVdgSendCommandOutbound);
    memset(pclSqlData, 0, sizeof (pclSqlData));

    while (sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS) {
        slSqlFunc = NEXT;
        ilCount++;
        if (ipUaftIdx >= 0) {
            memset(pclUaft, 0, sizeof (pclUaft));
            get_fld(pclSqlData, ipUaftIdx, STR, 20, pclUaft);
            sprintf(pclSelection, "WHERE URNO = %s", pclUaft);
        }
        dbg(DEBUG, "%s pclSelection<%s>", pclFunc, pclSelection);
        BuildItemBuffer(pclSqlData, " ", ipNumFields, ",");

        dbg(DEBUG, "%s: Rec %2d :- DATA <%s>", pclFunc, ilCount, pclSqlData);
        if (blArrival == TRUE)
		{
			/*FYA v1.26c UFIS-8297*/
            /*tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", "",
                cgVdgSendCommandInbound, pcgTableName, pclSelection, cgFieldListIn,
                pclSqlData, NETOUT_NO_ACK);*/
			tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", pcgTwEnd,
                cgVdgSendCommandInbound, pcgTableName, pclSelection, cgFieldListIn,
                pclSqlData, NETOUT_NO_ACK);
		}
        else
		{
			/*FYA v1.26c UFIS-8297*/
            /*tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", "",
                cgVdgSendCommandOutbound, pcgTableName, pclSelection, cgFieldListOut,
                pclSqlData, NETOUT_NO_ACK);*/
			tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", pcgTwEnd,
                cgVdgSendCommandInbound, pcgTableName, pclSelection, cgFieldListIn,
                pclSqlData, NETOUT_NO_ACK);
		}
        memset(pclSqlData, 0, sizeof (pclSqlData));
    }
    close_my_cursor(&slSqlCursor);
    dbg(TRACE, "%s: Records sent <%d>", pclFunc, ilCount);

} /*GetAndSendRecords*/

static int SendSingle(char * pcpData) {
    char *pclFunc = "SendSingle";

    /*FYA v1.26d UFIS-8325
    char pclUaft[20] = "\0";*/
    char pclUaft[URNO_LEN] = "\0";

    char pclSqlBuf[1024] = "\0";
    short slSqlCursor;
    short slSqlFunc;
    int ilRc = RC_SUCCESS;
    char pclSqlData[8192] = "\0";
    int ilFieldNum = 0;
    char pclSelection[1024] = "\0";

    if (atoi(pcpData) != 0) {
        dbg(DEBUG, "pcpdata<%s>", pcpData);

        if(igMultiHopo == TRUE)
        {
            sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE URNO='%s' AND HOPO IN ('***','%s')", cgAftFields, pcgTableName, pcpData, cgHopo);
        }
        else
        {
            sprintf(pclSqlBuf, "SELECT %s FROM %s WHERE URNO='%s'", cgAftFields, pcgTableName, pcpData);
        }

    } else {
        dbg(DEBUG, "%s pcpData<%s> is invalid,return RC_FAIL", pclFunc, pcpData);
        return RC_FAIL;
    }

    ilFieldNum = get_no_of_items(cgAftFields);

    slSqlCursor = 0;
    slSqlFunc = START;
    dbg(TRACE, "%s: Executing SQL <%s>", pclFunc, pclSqlBuf);

    dbg(DEBUG, "%s igSendToModId<%d>", pclFunc, igSendToModId);
    dbg(DEBUG, "%s mod_id<%d>", pclFunc, mod_id);
    dbg(DEBUG, "%s mod_name<%s>", pclFunc, mod_name);
    dbg(DEBUG, "%s cgVdgSendCommandInbound<%s>", pclFunc, cgVdgSendCommandInbound);
    dbg(DEBUG, "%s cgVdgSendCommandOutbound<%s>", pclFunc, cgVdgSendCommandOutbound);
    memset(pclSqlData, 0, sizeof (pclSqlData));

    if (sql_if(slSqlFunc, &slSqlCursor, pclSqlBuf, pclSqlData) == DB_SUCCESS) {
        if (igUaftIdx >= 0) {
            memset(pclUaft, 0, sizeof (pclUaft));
            get_fld(pclSqlData, igUaftIdx, STR, 20, pclUaft);
            sprintf(pclSelection, "WHERE URNO = %s", pclUaft);
        }

        BuildItemBuffer(pclSqlData, "", ilFieldNum, ",");
        dbg(DEBUG, "%s DATA <%s>", pclFunc, pclSqlData);

		/*FYA v1.26c UFIS-8297*/
        /*tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", "",
                cgVdgSendCommandInbound, pcgTableName, pclSelection, cgAftFields,
                pclSqlData, NETOUT_NO_ACK);*/
		tools_send_info_flag(igSendToModId, mod_id, mod_name, "", "", "", "", "", pcgTwEnd,
                cgVdgSendCommandInbound, pcgTableName, pclSelection, cgAftFields,
                pclSqlData, NETOUT_NO_ACK);
    } else {
        dbg(DEBUG, "%s flight not found in %s", pclFunc, pcgTableName);
    }
    close_my_cursor(&slSqlCursor);

    return RC_SUCCESS;
}

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
		Terminate(30);
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
		ilLen = get_real_item(cgHopo, pcpTwEnd, 1);
		if (strlen(cgHopo) == 0 || ilLen < 3)
		{
		    dbg(TRACE, "%s: Extracting pcpTwEnd = <%s> for received hopo fails or length < 3",pclFunc, pcpTwEnd);
		    ilRc = RC_FAIL;
		}
		else
		{
		    strcat(cgHopo,",");
		    sprintf(pclTmp,"%s,",pcpHopo_sgstab);

            if  ( strstr(pclTmp, cgHopo) == 0 )
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is not in SGS.TAB HOPO<%s>",
                           pclFunc, cgHopo, pclTmp);
                ilRc = RC_FAIL;
            }
            else
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is in SGS.TAB HOPO<%s>",
                           pclFunc, cgHopo, pclTmp);
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
