#ifndef _DEF_mks_version
#define _DEF_mks_version
#include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/authdl.c 1.40 2014/04/11 13:00:00SGT zdo $";
#endif /* _DEF_mks_version */
/* ******************************************************************** *

 CEDA Program Skeleton

 Author       :Brian Chandler.

 Date         :17/11/97

 Description  :Authority handler

 20040715 JIM added:  SetHopoDefaults: get rid of unset HOPO values
 20070111 MEI added:  Chk for password expiry (v1.21)
 20080122 MEI added:  Activate/Deactivate user(v1.22)
 20091028 MEI: When APPL string is >= 20 chars, the global var contents are flushed. Resulting in nobody can log in to UFIS
               Remove the global var since it is not in use
20120412 FYA added: UAM function including UAL & UAA tables and SSO
command which is similar to GPR command but skips the password check
20120425 FYA modifying the URNO generating method which is NewUrnos
instead of original GetNextUrno
20120427 FYA providing the profile urno to client
20120628 FYA Capturing the login info for BLR - UFIS-2044
20120918 MEI Bug in profile urno
20130716 YYA Support New View Configuration for Client UFIS-3793
20130829 MEI New column DEPT in SECTAB
20131004 ZDO (v1.36)FOR GPR/SSO command only return PROU that enable the APPL
             Support SSO cmd with USID case insensitive. New authdl.cfg flag to control: SSO_USID_CASE_INSENSITIVE
             Fix last login time setting.
20131018 ZDO (v1.37)Support client encrypted password, 64 chars. Read GCFTAB to control. UFIS-3388
20140211 ZDO v1.38 UFIS-5581 The last login time displayed for the user is incorrect.(DXBEK-147).
             Set LOGD of SECTAB to UTC time.
20140411 ZDO v1.39 UFIS-5816 BIAL Login page hangs after sign in (a specific combination)
20140411 FYA v1.40 UFIS-7203 Enable UAC for non UFIS$ADMIN superuser check 
* ******************************************************************** */


static char sccs_version[] = "@(#)UFIS 4.3 (c) ABB ACE/FC authdl.c 1.38 / 03/10/29 12:00:00 / BCH";
/* ======================= */
#define LOGGING
/* ======================= */

/* This program is a CEDA main program */

#define U_MAIN

#define CEDA_PRG

/* Frank v1.32 UFIS2044 */
#define FYA

/* The master header file */
#include "authdl.h"
#include "glbdef.h"
#include "quedef.h"
#include "uevent.h"
#include "msgno.h"
#include "hsbsub.h"
#include "db_if.h"
#include "tools.h"
#include "send_tools.h"
#include  "loghdl.h"
#include  "calutl.h"    /* MEI v1.21 */
/* #include "urno_fn.h"  */      /* Frank v.1.29 */

#define FOR_INIT 1
#define FOR_CHECK 2

/* field lengths */
#define PASSLEN 64
#define ENCRYPTEDLEN 9
#define URNOLEN 10
#define NAMELEN 32
#define REMALEN 256
#define USIDLEN 32
#define FREQLEN 7
#define DATELEN 14
#define STATLEN 1
#define APPLLEN 9
#define TYPELEN 1
#define FUNCLEN 80
#define PAGELEN 20
#define FKTPLEN 1
#define MENULEN 32
#define MENALEN 32
#define FLAGLEN 1
#define MODULEN 1
#define INITLEN 1
#define LANGLEN 4
#define SUBDLEN 80
#define SDALLEN 80
#define FUALLEN 80

/* Frank@UAL_UAA Feb 21 2012 start*/
#define EVCTLEN 5
#define EVDELEN 32
#define CDATLEN 14
#define USECLEN 32
#define RMRKLEN 128
#define DPT1LEN 8

#define ACTIVE      1
#define DEACTIVE    0
#define RESETPASSWORD       3
/* Frank@UAL_UAA Feb 21 2012 end*/

#define MAX_FIELD_LEN 0x0800
#define TABNAMELEN 7
#define BIGBUF 250

#define NO_PROFILE 0
#define STANDARD 1
#define DISABLE_ALL 2
#define ENABLE_ALL 3

#define RC_NO_WORKSTATION 100

#define INSERT_ACTION 0
#define UPDATE_ACTION 1
#define NO_DATA_AREA 2

#define LOGINERROR "LOGINERROR"

#define SYSTEM_INI 101 /* com sent by cutil to create UFIS$ADMIN and BDPS-SEC */
#define BDPSSEC "BDPS-SEC"
#define UFISADMIN "UFIS$ADMIN"

#define SMI_COMMAND "SMI"
#define NEWSEC_COMMAND "NEWSEC"
#define UPDSEC_COMMAND "UPDSEC"
#define DELSEC_COMMAND "DELSEC"
#define NEWCAL_COMMAND "NEWCAL"
#define UPDCAL_COMMAND "UPDCAL"
#define DELCAL_COMMAND "DELCAL"
#define NEWPWD_COMMAND "NEWPWD"
#define SETPWD_COMMAND "SETPWD"
#define REMAPP_COMMAND "REMAPP"
#define NEWAPP_COMMAND "NEWAPP"
#define SETPRV_COMMAND "SETPRV"
#define SETFKT_COMMAND "SETFKT"
#define ADDAPP_COMMAND "ADDAPP"
#define DELAPP_COMMAND "DELAPP"
#define UPDAPP_COMMAND "UPDAPP"
#define NEWREL_COMMAND "NEWREL"
#define DELREL_COMMAND "DELREL"
#define SBC_COMMAND "SEC"

#define MAX_BC_LEN 13880

typedef unsigned char BOOL;

/* status definitions : HIGHEST_STAT -> given 2 statuses the one
   with the highest priority is returned. */
#define STAT_HIDDEN "-"
#define STAT_DISABLED "0"
#define STAT_ENABLED "1"
#define GET_PRIO(S) ( (!strcmp(S,STAT_ENABLED)) ? 2 : ((!strcmp(S,STAT_DISABLED)) ? 1 : 0))
#define HIGHEST_STAT(S1,S2) (GET_PRIO(S1)>GET_PRIO(S2) ? (S1) : (S2) )
#define WKS_STAT(S1,S2) (GET_PRIO(S1)>GET_PRIO(S2) ? (S2) : (S1) )

/* variable length buffer - grows in size as required */
/* buf can be of any type and is accessed by casting */
#define VARBUFBLOCK 1024

typedef struct
{
    int len;    /* total current length of buf in bytes */
    int used;   /* num bytes in buf used so far */
    int numObjs;    /* count of objs in buf - updated and used by user */
    void *buf;  /* pointer to variable length memory */
} VARBUF;

/* list to contain one or more errors */
#define MAXERRLEN 250
#define MAXERRORS 20

typedef struct
{
    int curPos; /* pointer used to pop the top error in the list */
    char line[MAXERRORS][MAXERRLEN];    /* error line(s) */
} ERRBUF;

/* list containing URNOs read using Get Many Urnos */
typedef struct
{
    int num;    /* number of URNOs read */
    int curr;   /* pointer to next URNO returned in GetNextUrno() */
    /* london implementation
        VARBUF varbuf;  /x* varible length list of URNOs - grows as required *x/
    end london implementation */
    /* dallas implementation */
    char urno[50];
    /* end dallas implentation */
} URNOLIST;

/* struct to hold lines for multiple array insert
   format eg. for URNO (len=10+NULL):
   FieldLen = 11, DataList = "URNO1+NULL ... URNOn+NULL" */
typedef struct
{
    short FieldLen;
    VARBUF DataList;
} ARRAYLINE;


#define COMMANDLEN      10
#define TABLENAMELEN    20
#define FIELDLISTLEN    2048
#define DATALISTLEN     2048

typedef struct
{
    char Command[COMMANDLEN + 1];
    char TableName[TABLENAMELEN + 1];
    char Urno[URNOLEN + 1];
    char FieldList[FIELDLISTLEN + 1];
    char DataList[DATALISTLEN + 1];

} BROADCASTLINE;

/* record for insert/update of SECTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char USID[USIDLEN + 1];
    char NAME[NAMELEN + 1];
    char TYPE[TYPELEN + 1];
    char LANG[LANGLEN + 1];
    char PASS[PASSLEN + 1];
    char STAT[STATLEN + 1];
    char REMA[REMALEN + 1];
    char DEPT[20];
} SECREC;

/* record for insert/update of FKTTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char FPRV[URNOLEN + 1]; /* set to PRV URNO of STAT updated */
    char FAPP[URNOLEN + 1];
    char SUBD[SUBDLEN + 1];
    char SDAL[SDALLEN + 1];
    char FUNC[FUNCLEN + 1];
    char FUAL[FUALLEN + 1];
    char TYPE[TYPELEN + 1];
    char STAT[STATLEN + 1];
    BOOL found;
} FKTREC;

/* record for insert/update of CALTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char FSEC[URNOLEN + 1];
    char VAFR[DATELEN + 1];
    char VATO[DATELEN + 1];
    char FREQ[FREQLEN + 1];
} CALREC;

/* record for insert/update of PRVTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char FSEC[URNOLEN + 1];
    char FFKT[URNOLEN + 1];
    char FAPP[URNOLEN + 1];
    char STAT[STATLEN + 1];
} PRVREC;

/* record for insert/update of GRPTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char FREL[URNOLEN + 1];
    char FSEC[URNOLEN + 1];
    char TYPE[TYPELEN + 1];
    char STAT[STATLEN + 1];
} GRPREC;

/* record for insert of PWDTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char FSEC[URNOLEN + 1];
    char PASS[PASSLEN + 1];
    char CDAT[DATELEN + 1];
} PWDREC;

typedef struct
{
    char URNO[URNOLEN + 1];
} URNOREC;

/* Frank@UAL_UAA Feb 21 2012 start*/

/* record for insert/update of UALTAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char EVCT[EVCTLEN + 1];
    char USID[USIDLEN + 1];
    char EVDE[EVDELEN + 1];
    char CDAT[CDATLEN + 1];
    char USEC[USECLEN + 1];
    char RMRK[RMRKLEN + 1];
} UALREC;

/* record for insert/update of UAATAB */
typedef struct
{
    char URNO[URNOLEN + 1];
    char DPT1[DPT1LEN + 1];
    char USID[USIDLEN + 1];
    char CDAT[CDATLEN + 1];
    char USEC[USECLEN + 1];
} UAAREC;

/* Frank@UAL_UAA Feb 21 2012 end*/

typedef struct
{
    int  Count;
    char VersStrg[64];
} MOD_VERS;

#define PAIDLEN      17
#define VALULEN       6
#define NUM_PARAM     4
#define STAT_VALID   'V'
#define STAT_INVALID 'I'

#define IDX_PWD_VALIDITY  0
#define IDX_USER_DEACT    1
#define IDX_SYSADM_PWD_EX 2
#define IDX_PWD_CHANGE    3

typedef struct
{
    time_t VAFR;
    time_t VATO;
    int iValue;
    char PAID[PAIDLEN];
    char VALU[VALULEN];
    char isValid;
} PARAM_DATA;

BOOL bgPwdtabExists = FALSE;
BOOL bgDbConfig = TRUE;         /* Mei 1.22 */

#define VALULEN1    32

typedef enum
{
    E_ENCBY_INVALID,
    E_ENCBY_SERVER = 0,         /* 0   */
    E_ENCBY_CLIENT
} ENC_BY;

typedef enum
{
    E_ENCMETHOD_INVALID,
    E_ENCMETHOD_NONE = 0,       /* 0    */
    E_ENCMETHOD_UFIS,
    E_ENCMETHOD_SHA256
} ENC_METHOD;

/* ******************************************************************** */
/* External variables                                                   */
/* ******************************************************************** */
/*extern char sql_buf[];*/
/*extern char data_area[];*/


/* ******************************************************************** */
/* Global variables                                                     */
/* ******************************************************************** */

static ITEM *prgItem = NULL;        /* The queue item pointer   */
static EVENT *prgEvent;             /* The event pointer        */
static int igQueOut;
static int igInitOK = FALSE;
static int igToBcHdl = 1900;
static int igBcHdlPrio = 4;
BOOL bgSendSmiBcOnly = FALSE;
/* Frank@UAL_UAA */
BOOL bgUpdateUALtab = FALSE;
BOOL bgUpdateUAAtab = FALSE;
BOOL bgLogAllAccountEvent = FALSE;
/* Frank@UAL_UAA */
BOOL bgSendProfileUrnoToClient = FALSE; /* Frank v1.31*/
BOOL bgCaptureLoginInfo = FALSE; /* Frank v1.32 UFIS2044 */
BOOL bgSmiCommandActive = FALSE;
BOOL bgSSOUsidCaseInsensitive = FALSE; /* v1.36 UFIS-4318 */

int debug_level=DEBUG;
static int igDebugDetails = TRUE;

/* Frank v1.29 */
char  cgHopo[8] = "\0"; /* default home airport    */

static char pcgOutRoute[64];
static int igToAction = 0;
static int igToLogHdl = 0;

static int igPasswordUsedDays = -1; /* Mei */

char *pcgResultBuf = NULL;      /* used to hold result of GPR command */
char *pcgUrnoList = NULL;       /* used to hold a list of urnos */
URNOLIST rgUrnoList;            /* list of new URNOs */
VARBUF rgResultBuf;             /* holds data returned to the client */
ERRBUF rgErrList;               /* holds a list of error messages */
VARBUF rgFktBuf;                /* list of records read from FKTTAB */
VARBUF rgPrvBuf;                /* multi-insert PRV recs */
VARBUF rgProfileList;
VARBUF rgAlreadyCheckedList;
VARBUF rgBroadcastLines;

PARAM_DATA rgParam[NUM_PARAM];          /* Mei */
char *pcgParamList[NUM_PARAM] = { "ID_PWD_VALIDITY", "ID_USER_DEACT", "ID_SYSADM_PWD_EX", "ID_PWD_CHANGE" };

char *pcgCfgPath;
char pcgCfgFile[BIGBUF];
char pcgCedaCfgFile[BIGBUF];
char pcgBinFile[BIGBUF];

char pcgCurrDate[15], pcgDayOfWeek[3];

char pcgProcName[100];

char pcgSectab[TABNAMELEN];     /* SECTAB table name */
char pcgCaltab[TABNAMELEN];     /* CALTAB table name */
char pcgGrptab[TABNAMELEN];     /* GRPTAB table name */
char pcgPrvtab[TABNAMELEN];     /* PRVTAB table name */
char pcgFkttab[TABNAMELEN];     /* FKTTAB table name */
char pcgPwdtab[TABNAMELEN];     /* PWDTAB table name */
/*Frank@UAL_UAA*/
char pcgUaltab[TABNAMELEN];     /* UALTAB table name */
char pcgUaatab[TABNAMELEN];     /* UAATAB table name */
/*Frank@UAL_UAA*/
char pcgGcftab[TABNAMELEN];     /* GCFTAB table name */

char pcgDefFunc[FUNCLEN + 1];
char pcgDefSubd[SUBDLEN + 1];
char pcgDefUsid[USIDLEN + 1];
char pcgDefPass[PASSLEN + 1];
char pcgDefModu[USIDLEN + 1];
char pcgDefStat[STATLEN + 1];
char pcgDefFreq[FREQLEN + 1];
char pcgDefVafr[DATELEN + 1];
char pcgDefVato[DATELEN + 1];
char pcgDefSdal[SDALLEN + 1];
char pcgDefFual[FUALLEN + 1];

int igLastPasswordCount;
int igNumLoginAttemptsAllowed; /* num login attempts allowed before user is deactivated (-1 means not used)*/
int igPasswordExpiryDays; /*  (<1 means not used) */ /* MEI v1.21 */

static char pcgHopoList[128];

static char pcgDefTblExt[8]; /* Default TableExtension */
static char pcgDefH3LC[8];   /* Default HomePort 3LC */

static char pcgTblExt[8]; /* Current TableExtension */
static char pcgH3LC[8];   /* Current HomePort 3LC */

static char pcgTwStart[64]; /*  */
static char pcgTwEnd[64]; /* Hopo,Ext,Modul from Client */

static char pcgRecvName[64]; /*  */
static char pcgDestName[64]; /*  */
static char pcgUsername[64]; /*  */


static int igAddHopo = FALSE; /* May Be Used as Flag */
static char pcgUfisTables[2048];


static int igNewConfMod = FALSE; /* Used as Flag */

static ENC_BY egEncBy = E_ENCBY_INVALID;
static ENC_METHOD egEncMethod = E_ENCMETHOD_INVALID;


static char pcgGrantUsidSuperUser[1024] = "\0";

/* ******************************************************************** */
/* External functions                                                   */
/* ******************************************************************** */

extern int init_que(); /* Attach to CEDA queues */
extern int que( int, int, int, int, int, char* ); /* CEDA queuing routine */
/*extern    void SetSignals();*/
#ifdef UFIS43
extern int send_message( int, int, int, int, char* ); /*CEDA message handler if*/
#endif
extern char *sGet_item2( char *, int );
extern void addSecs( char * );
extern void removeNull( char * );
extern int itemCount( char * );
extern char *getItem( char *, char *, char * );
extern int snap( char *, long, FILE * );

#ifdef UFIS43
extern int TransferFile( char * );
#endif

#ifdef LOGGING
static int ReleaseActionInfo( char *, char *, char *, char *, char *, char *, char * );
static int GetOutModId( char *pcpProcName );
#endif


extern int get_real_item( char *, char *, int );
extern int StrgPutStrg( char *, int *, char *, int, int, char * );

/* ******************************************************************** */
/* Function prototypes                                                  */
/* ******************************************************************** */

static void init_authdl();
static int GetOutModIdWithPrio( char *pcpProcName, int *pipPrio );
static int reset();         /* Reset program        */
static void terminate();        /* Terminate program        */
static void handleSignal( int ipSignal ); /* Handles signals      */
static void handle_err( int );  /* Handles general errors   */
static void HandleQueErr( int pipErr );
static int handle_data();       /* Handles event data       */
static void HandleQueues();
void handleCommit();
void handleRollback();

static int ReadCfg( char *, char *, char *, char *, char * );
static int handleNewSec( char *, char *, char * );
static int handleUpdSec( char *, char *, char * );
static int handleNewCal( char *, char *, char * );
static int handleDelCal( char *, char *, char * );
static int handleUpdCal( char *, char *, char * );
static int handleSetPwd( char *, char *, char *, BOOL );
static int handleNewPwd( char *, char *, char * );
static int handleSetRgt( char *, char *, char *, char * );
static int handleNewApp( char *, char *, char * );
static int handleNewRel( char *, char *, char * );
static int handleDelRel( char *, char *, char * );
static int handleAddApp( char *, char *, char * );
static int handleUpdApp( char *, char *, char * );
static int handleDelSec( char *, char *, char * );
static int handleDelApp( char *, char *, char * );
static int handleRemApp( char *, char *, char * );
static int handleReadTab( char *, char *, char * );
static int handleModIni( char *, char *, char * );
void handleSysIni( void );
static int handleGetPrivileges( char *, char *, char *, char *, char * );
static int CreatePrivList( BOOL, char *, char *, VARBUF *, char *, char *, char *, char *, char * );
static int GetProfiles( char * );
static int SetPrivStat( VARBUF *, char *, char *, char *, BOOL );
static int SetWksPrivStat( VARBUF *, char *, char *, char *, BOOL );
static int CheckValidSecCal( char *, char *, char *, char *, char * );
static int CheckValidCal( char *, char * );
static int CheckValidLogin( char *, BOOL, BOOL, BOOL, char * );
static int CheckIsSuperUser( char *, char * );
static int MergeFktPrv( char *, char *, VARBUF * );
void UpdateStatInFkt( char *, char *, VARBUF * );
static int CreatePrvFromFkt( char *, char *, VARBUF *, VARBUF * );
static int DeactivateInitModuFlag( char *, char * );

static int UpdatePasswordTable( char *, char * );
static int CheckForUnusedPassword( char *, char * );
static int CheckPasswordNotAlreadyUsed( char *, char *, char * );
static int CheckForPasswordExpiry( char *pcpUsid, char *pcpFsec, char *pcpPass, char *pcpErrorMess ); /* MEI v1.21 */
static BOOL PwdtabExists( void );

static int InitVarBuf( VARBUF * );
void ClearVarBuf( VARBUF * );
void FreeVarBuf( VARBUF * );
static int SetSizeVarBuf( VARBUF *, int );
static int AddVarBuf( VARBUF *, void *, int );
int LenVarBuf( VARBUF * );

void ClearError( ERRBUF * );
void PushError( ERRBUF *, char * );
char *PopError( ERRBUF * );
void LogErrors( ERRBUF * );
int GetNumErrors( ERRBUF * );

static int LoadUrnoList( int );
static int GetNextUrno( char * );

static int handleCount( char *, char *, int * );
static int handleCollectUrnos( char *, char *, VARBUF * );
void FormatFields( int, VARBUF *, char *, char *, char * );
int GetItemIndex( char *, char * );
static int handleInsert( char *, char *, char *, char * );
static int handleUpdate( char *, char *, char *, char *, char *, VARBUF * );
static int handleDelete( char *, char *, char *, VARBUF * );
static int GetCurrDate( void );
BOOL AddToList( VARBUF *prpList, char *pcpUrno );
static char *GetCurrTime( void );

static int SendBroadcastLines();
static int CancelSendBroadcastLines();
static int AddBroadcastLine( char *, char *, char *, char *, char * );
static int AddBroadcastLines( char *, char *, char *, char *, char * );

static int InsertSecRec( SECREC * );
static int InsertCalRec( CALREC * );
static int InsertGrpRec( GRPREC * );
static int InsertPrvRec( PRVREC * );
static int InsertPrvRecs( VARBUF * );
static int InsertFktRec( FKTREC * );
static int InsertFktRecs( VARBUF * );
static int UpdateFktRec( FKTREC * );
/*Frank@UAL_UAA*/
static int InsertUalRec( UALREC * );
/*Frank@UAM*/
/*static int InsertUalTab(char *pcpUrno,char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived);   */
static int InsertUalTab( char *pcpUrno, char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived, char *pcpRema );
/*Frank@UAM*/
static int handleDeleteUal( char *cpUsid );
static int handleDeleteUaa( char *cpGrpid );
/*Frank@UAL_UAA*/

/* Frank v1.32 UFIS2044 */
static int CaptureLogInInfoToUaltab( char *pcpFieldList, char *pcpDataList, const char *pcpRecvName, const char *pcpDestName, char *pcpErrorMess );
static int ConvertIp( const char *pcpRecvName, char *pcpIpText );
static int UpdateUALTAB( UALREC rlUalRec, char *pcpErrorMess );

static int LoadFktTab( char *, VARBUF * );
FKTREC *GetFktRec( char *, char *, VARBUF * );
BOOL FktRecExists( char *, char *, char *, VARBUF * );
static int DeleteUnfoundFktRecs( VARBUF * );
static int DeleteOldUnreferencedFktRecs( char * );
static int LoadPrvTab( char *, VARBUF * );
static int handleCreateSecCal( char *, char *, char *, VARBUF * );
static int handleCreateCal( char *, char *, char *, VARBUF * );
static int handleCreateGrpCal( char *, char *, VARBUF *, VARBUF * );
static int handleCreatePrv( char *, char *, VARBUF * );
static int handleCopyPrv( char *, char *, VARBUF * );
static int handleCreateFkt( char *, char *, char *, VARBUF * );
static int handleDeleteProfile( char * );
static int handleDeleteRel( char *, char * );
static int handleDeleteSecCal( char * );
static int InsertPwdRec( PWDREC *prpPwdRec );

static int CheckHomePort( void );
static void CheckWhereClause( int, char *, int, int, char * );
static void GetHopoList( void );
static int CheckModulVersion( int ipForWhat );
static void SetHopoDefaults( void );

extern void str_chg_upc( char * );

/* Mei */
extern void LoadDBParam();
extern int ExecSqlQuery( char *pcpSqlQuery, char *pcpSqlData );
extern time_t TimeConvert_sti( char *pcpTimeStr );
extern void CurrentUTCTime( char *pcpTimeStr );
void SetLoginTime( char *pcpUsec );

/* User Setting Configuration Implementation JIRA-3793 */
static int NewConFormat( char *pcpResult, char *pcpUsid, char *pcpProfileUrnos, char *pcpCommandType );
static void ChgProfListForNewConFormat( char *pcpProu );
/* ******************************************************************** */
/* The MAIN program                                                     */
/* ******************************************************************** */
MAIN
{
    int rc;         /* Return code          */
    int ilRC = RC_SUCCESS;
    int ilCnt = 0;

    /* copy my process name - used to form the config file name */
    strcpy( pcgProcName, argv[0] );

    INITIALIZE;         /* General initialization   */
    dbg( TRACE,"<MAIN>\n%s",sccs_version );

    /* Attach to the UFIS43 queues */
    do
    {
        ilRC = init_que();

        if( ilRC != RC_SUCCESS )
        {
            dbg( TRACE, "MAIN: init_que() failed! waiting 6 sec ..." );
            sleep( 6 );
            ilCnt++;
        }/* end of if */
    }
    while( ( ilCnt < 10 ) && ( ilRC != RC_SUCCESS ) );

    if( ilRC != RC_SUCCESS )
    {
        dbg( TRACE, "MAIN: init_que() failed! waiting 60 sec ..." );
        sleep( 60 );
        exit( 1 );
    }
    else
    {
        dbg( DEBUG,"MAIN: init_que() OK! mod_id <%d>", mod_id );
    }/* end of if */

    /* logon to database */
    do
    {
        ilRC = init_db();

        if( ilRC != RC_SUCCESS )
        {
            check_ret( ilRC );
            dbg( TRACE, "MAIN: init_db() failed! waiting 6 sec ..." );
            sleep( 6 );
            ilCnt++;
        } /* end of if */
    }
    while( ( ilCnt < 10 ) && ( ilRC != RC_SUCCESS ) );

    if( ilRC != RC_SUCCESS )
    {
        dbg( TRACE, "MAIN: init_db() failed! waiting 60 sec ..." );
        sleep( 60 );
        exit( 2 );
    }
    else
    {
        dbg( DEBUG, "MAIN: init_db() OK!" );
    } /* end of if */
    /* logon to DB is ok, but do NOT use DB while ctrl_sta == HSB_COMING_UP !!! */

    ilRC = SendRemoteShutdown( mod_id );

    if( ilRC != RC_SUCCESS )
    {
        dbg( TRACE, "MAIN: SendRemoteShutdown(%d) failed!", mod_id );
    } /* end of if */

    SetSignals( handleSignal ); /* handles signal       */

    if( ( ctrl_sta != HSB_STANDALONE ) && ( ctrl_sta != HSB_ACTIVE ) && ( ctrl_sta != HSB_ACT_TO_SBY ) )
    {
        dbg( TRACE, "MAIN: waiting for status switch ..." );
        HandleQueues();
    }/* end of if */

    if( ( ctrl_sta == HSB_STANDALONE ) || ( ctrl_sta == HSB_ACTIVE ) || ( ctrl_sta == HSB_ACT_TO_SBY ) )
    {
        dbg( TRACE, "MAIN: initializing ..." );

        if( igInitOK == FALSE )
        {
            init_authdl();
            igInitOK = TRUE;
        }/* end of if */
    }
    else {
        terminate();
    }/* end of if */

    dbg( DEBUG, "MAIN: enter EventLoop" );

    while( TRUE )
    {
        dbg( TRACE, "=================== START/END ==================" );
        rc = que( QUE_GETBIG, 0, mod_id, PRIORITY_3, 0, ( char * ) &prgItem );

        prgEvent = ( EVENT * ) prgItem->text; /* set event pointer */

        if( rc == RC_SUCCESS )
        {
            rc = que( QUE_ACK, 0, mod_id, 0, 0, NULL );

            if( rc != RC_SUCCESS )
            {
                /* handle que_ack error */
                HandleQueErr( rc );
            } /* fi */

            switch( prgEvent->command )
            {

                case TRACE_ON: /* 7 */
                case TRACE_OFF: /* 8 */
                    dbg_handle_debug( prgEvent->command );
                    break;

                case SHUTDOWN:
                    terminate();
                    break;

                case RESET:
                    rc = reset();
                    break;

                case EVENT_DATA:
                    if( ctrl_sta == HSB_ACTIVE || ctrl_sta == HSB_ACT_TO_SBY || ctrl_sta == HSB_STANDALONE )
                    {
                        rc = handle_data();
                    } /* end if */

                    break;

                case REMOTE_DB:
                    /* ctrl_sta is checked inside  vbl */
                    HandleRemoteDB( prgEvent );
                    break;

                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ResetDBCounter();
                    break;

                case HSB_DOWN:
                    ctrl_sta = prgEvent->command;
                    terminate( RC_SHUTDOWN );
                    break;

                case HSB_STANDBY:
                    ctrl_sta = prgEvent->command;
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    HandleQueues();
                    break;

                case HSB_ACTIVE:
                    ctrl_sta = prgEvent->command;
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    break;

                case HSB_COMMING_UP:
                    ctrl_sta = prgEvent->command;
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    HandleQueues();
                    break;

                case HSB_ACT_TO_SBY:
                    ctrl_sta = prgEvent->command;
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    HandleQueues();
                    break;

                case SYSTEM_INI:
                    /* create UFIS$ADMIN and BDPS-SEC */
                    handleSysIni();
                    break;

                case 777:
                    break;

                default:
                    DebugPrintItem( TRACE, prgItem );
                    DebugPrintEvent( TRACE, prgEvent );
                    break;

            } /* end switch */

            /* Handle error conditions */

            if( rc != RC_SUCCESS )
            {
                handle_err( rc );
            } /* end if */

        } /* end if */
        else
        {
            /* Handle queuing errors */
            HandleQueErr( rc );
        } /* end else */

        if( prgItem != NULL )
            free( ( void* )prgItem );

        prgItem = NULL;

    } /* end while */

    /*exit(0);*/

} /* end of MAIN */

/* ******************************************************************** */
/* The initialization routine                                           */
/* ******************************************************************** */
static void init_authdl()
{
    char clLastPasswordCount[100];
    char clNumLoginAttemptsAllowed[100];
    int ilGetRc = RC_SUCCESS;

    int     ilFileTrans = RC_SUCCESS;
    int     ilRc = RC_SUCCESS;
    char    pclSqlBuf[2048];
    char    pclDataArea[2048];
    short   slLocalCursor;
    short   slFuncCode;
    char    clOraErr[250];
    char    clSmiBcs[100];
    char    clPasswordExpiryDays[100];     /* MEI v1.21 */
    /*Frank@UAL_UAA*/
    char clUALTabUpdate[100];
    char clUAATabUpdate[100];
    char clLogAllAccountEvent[100];
    /*Frank@UAL_UAA*/

    /* Frank v1.31*/
    char clSendProfileUrnoToClient[100] = "\0";

    /* Frank v1.32 UFIS-2044 */
    char pclCaptureLoginInfo[16] = "\0";

    /* v1.36 UFIS-4318 */
    char clSSOUsidCaseInsensitive[16];

    dbg( DEBUG, "Startup AUTHDL at %s", GetCurrTime() );

    /* read config file */
    if( ( pcgCfgPath = getenv( "CFG_PATH" ) ) == NULL )
    {
        dbg( TRACE, "init_authdl() Error reading CFG_PATH" );
    }

    /* compose path/filename */
    sprintf( pcgCfgFile, "%s/%s.cfg", pcgCfgPath, pcgProcName );
    sprintf( pcgCedaCfgFile, "%s/ufis_ceda.cfg", pcgCfgPath );
    dbg( TRACE, "init auth config file <%s>", pcgCfgFile );
    dbg( TRACE, "init ceda config file <%s>", pcgCedaCfgFile );

    pcgDefTblExt[0] = 0x00;
    pcgDefH3LC[0] = 0x00;

    ilGetRc = tool_search_exco_data( "ALL", "TABEND", pcgDefTblExt );
    strcpy( pcgTblExt, pcgDefTblExt );
    ilGetRc = tool_search_exco_data( "SYS", "HOMEAP", pcgDefH3LC );
    strcpy( pcgH3LC, pcgDefH3LC );
    dbg( TRACE, "10.01.2005 DEFAULTS: HOME <%s> EXT <%s>", pcgDefH3LC, pcgDefTblExt );


    /* init the table names for the default project */
    sprintf( pcgSectab, "SEC%s", pcgDefTblExt );
    sprintf( pcgCaltab, "CAL%s", pcgDefTblExt );
    sprintf( pcgGrptab, "GRP%s", pcgDefTblExt );
    sprintf( pcgPrvtab, "PRV%s", pcgDefTblExt );
    sprintf( pcgFkttab, "FKT%s", pcgDefTblExt );
    sprintf( pcgPwdtab, "PWD%s", pcgDefTblExt );
    /*Frank@UAL_UAA*/
    /* sprintf(pcgUaltab,"UAP%s",pcgDefTblExt);   */
    sprintf( pcgUaltab, "UAL%s", pcgDefTblExt );

    sprintf( pcgUaatab, "UAA%s", pcgDefTblExt );
    /* sprintf(pcgUaatab,"UAB%s",pcgDefTblExt);    */
    /*Frank@UAL_UAA*/
    sprintf(pcgGcftab, "GCF%s", pcgDefTblExt);

    /* Read FUNC entry */
    memset( pcgDefFunc, 0, sizeof( pcgDefFunc ) );
    ReadCfg( pcgCfgFile, pcgDefFunc, "DEFAULTS", "FUNC", "InitModu" );
    dbg( TRACE, "FUNC = %s", pcgDefFunc );

    /* Read SUBD entry */
    memset( pcgDefSubd, 0, sizeof( pcgDefSubd ) );
    ReadCfg( pcgCfgFile, pcgDefSubd, "DEFAULTS", "SUBD", "InitModu" );
    dbg( TRACE, "SUBD = %s", pcgDefSubd );

    /* Read USID entry */
    memset( pcgDefUsid, 0, sizeof( pcgDefUsid ) );
    ReadCfg( pcgCfgFile, pcgDefUsid, "DEFAULTS", "USID", UFISADMIN );
    dbg( TRACE, "USID = %s", pcgDefUsid );

    /* Read PASS entry */
    memset( pcgDefPass, 0, sizeof( pcgDefPass ) );
    ReadCfg( pcgCfgFile, pcgDefPass, "DEFAULTS", "PASS", "Passwort" );
    dbg( TRACE, "PASS = %s", pcgDefPass );

    /* Read MODU entry */
    memset( pcgDefModu, 0, sizeof( pcgDefModu ) );
    ReadCfg( pcgCfgFile, pcgDefModu, "DEFAULTS", "MODU", BDPSSEC );
    dbg( TRACE, "MODU = %s", pcgDefModu );

    /* Read STAT entry */
    memset( pcgDefStat, 0, sizeof( pcgDefStat ) );
    ReadCfg( pcgCfgFile, pcgDefStat, "DEFAULTS", "STAT", "1" );
    dbg( TRACE, "STAT = %s", pcgDefStat );

    /* Read FREQ entry */
    memset( pcgDefFreq, 0, sizeof( pcgDefFreq ) );
    ReadCfg( pcgCfgFile, pcgDefFreq, "DEFAULTS", "FREQ", "1234567" );
    dbg( TRACE, "FREQ = %s", pcgDefFreq );

    /* Read VAFR entry */
    memset( pcgDefVafr, 0, sizeof( pcgDefVafr ) );
    ReadCfg( pcgCfgFile, pcgDefVafr, "DEFAULTS", "VAFR", "19970101000000" );
    dbg( TRACE, "VAFR = %s", pcgDefVafr );

    /* Read VATO entry */
    memset( pcgDefVato, 0, sizeof( pcgDefVato ) );
    ReadCfg( pcgCfgFile, pcgDefVato, "DEFAULTS", "VATO", "20100101000000" );
    dbg( TRACE, "VATO = %s", pcgDefVato );

    /* Read SDAL entry */
    memset( pcgDefSdal, 0, sizeof( pcgDefSdal ) );
    ReadCfg( pcgCfgFile, pcgDefSdal, "DEFAULTS", "SDAL", " " );
    dbg( TRACE, "SDAL = %s", pcgDefSdal );

    /* Read FUAL entry */
    memset( pcgDefFual, 0, sizeof( pcgDefFual ) );
    ReadCfg( pcgCfgFile, pcgDefFual, "DEFAULTS", "FUAL", " " );
    dbg( TRACE, "FUAL = %s", pcgDefFual );

    memset( clLastPasswordCount, 0, sizeof( clLastPasswordCount ) );
    ReadCfg( pcgCfgFile, clLastPasswordCount, "DEFAULTS", "LAST_PASSWORD_COUNT", "3" );
    igLastPasswordCount = atoi( clLastPasswordCount );
    dbg( TRACE, "LAST_PASSWORD_COUNT = %d", igLastPasswordCount );

    memset( pcgGrantUsidSuperUser, 0, sizeof( pcgGrantUsidSuperUser ) );
    ReadCfg( pcgCfgFile, pcgGrantUsidSuperUser, "DEFAULTS", "GRANT_USID_SUPERUSER", "3" );
    dbg( TRACE, "GRANT_USID_SUPERUSER = %s", pcgGrantUsidSuperUser );

    /* MEI v1.21 */
    /* Read PASSWORD_EXPIRY_DAYS entry */
    memset( clPasswordExpiryDays, 0, sizeof( clPasswordExpiryDays ) );
    ReadCfg( pcgCfgFile, clPasswordExpiryDays, "DEFAULTS", "PASSWORD_EXPIRY_DAYS", "0" );
    igPasswordExpiryDays = atoi( clPasswordExpiryDays );
    dbg( TRACE, " PASSWORD_EXPIRY_DAYS = %d", igPasswordExpiryDays );
    /************/

    /* Read num valid attempts to login before user is deactivated */
    memset( clNumLoginAttemptsAllowed, 0, sizeof( clNumLoginAttemptsAllowed ) );
    ReadCfg( pcgCfgFile, clNumLoginAttemptsAllowed, "DEFAULTS", "NUM_LOGIN_ATTEMPTS_ALLOWED", "-1" );
    igNumLoginAttemptsAllowed = atoi( clNumLoginAttemptsAllowed );
    dbg( TRACE, "NUM_LOGIN_ATTEMPTS_ALLOWED = %d", igNumLoginAttemptsAllowed );

    /* Read SEND_REGISTER_BROADCAST_ONLY entry */
    /* if YES then send broadcasts only for SMI command (register application) */
    /* if NO the send all broadcasts unless     [QUEUE_DEFINES] QUE_TO_bchdl = 0,4   which switches off broadcasting*/
    memset( clSmiBcs, 0, sizeof( clSmiBcs ) );
    ReadCfg( pcgCfgFile, clSmiBcs, "DEFAULTS", "SEND_REGISTER_BROADCAST_ONLY", "YES" );
    dbg( TRACE, "SEND_REGISTER_BROADCAST_ONLY = %s", clSmiBcs );

    if( !strcmp( clSmiBcs, "YES" ) )
        bgSendSmiBcOnly = TRUE;
    else
        bgSendSmiBcOnly = FALSE;

    /*Frank@UAL_UAA*/
    /* Read UALTAB_UPDATE entry */
    memset( clUALTabUpdate, 0, sizeof( clUALTabUpdate ) );
    ReadCfg( pcgCfgFile, clUALTabUpdate, "DEFAULTS", "UALTAB_UPDATE", "NO" );
    dbg( TRACE, "UALTAB_UPDATE = %s", clUALTabUpdate );

    if( !strcmp( clUALTabUpdate, "YES" ) )
        bgUpdateUALtab = TRUE;
    else
        bgUpdateUALtab = FALSE;

    /* Read UAATAB_UPDATE entry */
    memset( clUAATabUpdate, 0, sizeof( clUAATabUpdate ) );
    ReadCfg( pcgCfgFile, clUAATabUpdate, "DEFAULTS", "UAATAB_UPDATE", "NO" );
    dbg( TRACE, "UAATAB_UPDATE = %s", clUAATabUpdate );

    if( !strcmp( clUAATabUpdate, "YES" ) )
        bgUpdateUAAtab = TRUE;
    else
        bgUpdateUAAtab = FALSE;

    /* Read LOG_ALL_ACCNT_EVENT entry */
    memset( clLogAllAccountEvent, 0, sizeof( clLogAllAccountEvent ) );
    ReadCfg( pcgCfgFile, clLogAllAccountEvent, "DEFAULTS", "LOG_ALL_ACCNT_EVENT", "NO" );
    dbg( TRACE, "LOG_ALL_ACCNT_EVENT = %s", clLogAllAccountEvent );

    if( !strcmp( clLogAllAccountEvent, "YES" ) )
        bgLogAllAccountEvent = TRUE;
    else
        bgLogAllAccountEvent = FALSE;

    /*Frank@UAL_UAA*/

    /* Frank v1.31 */
    memset( clSendProfileUrnoToClient, 0, sizeof( clSendProfileUrnoToClient ) );
    ReadCfg( pcgCfgFile, clSendProfileUrnoToClient, "DEFAULTS", "SEND_PROFILE_URNO_TO_CLIENT", "NO" );

    if( !strcmp( clSendProfileUrnoToClient, "YES" ) )
    {
        bgSendProfileUrnoToClient = TRUE;
    }
    else
    {
        bgSendProfileUrnoToClient = FALSE;
    }

    dbg( TRACE, "line<%d>bgSendProfileUrnoToClient<%d>", __LINE__, bgSendProfileUrnoToClient );

    /* Frank v1.32 UFIS-2044 */
    memset( pclCaptureLoginInfo, 0, sizeof( pclCaptureLoginInfo ) );
    ReadCfg( pcgCfgFile, pclCaptureLoginInfo, "DEFAULTS", "CAPTURE_LOGIN_INFO", "NO" );
    dbg( TRACE, "line<%d>pcgCaptureLoginInfo<%s>", __LINE__, pclCaptureLoginInfo );

    if( strncmp( pclCaptureLoginInfo, "YES", 3 ) == 0 )
    {
        bgCaptureLoginInfo = TRUE;
        dbg( TRACE, "line<%d>bgCaptureLoginInfo = TRUE", __LINE__ );
    }
    else
    {
        bgCaptureLoginInfo = FALSE;
        dbg( TRACE, "line<%d>bgCaptureLoginInfo = FALSE", __LINE__ );
    }

    /* Read SSO_USID_CASE_INSENSITIVE entry */
    memset( clSSOUsidCaseInsensitive,0,sizeof( clSSOUsidCaseInsensitive ) );
    ReadCfg( pcgCfgFile,clSSOUsidCaseInsensitive,"DEFAULTS","SSO_USID_CASE_INSENSITIVE","NO" );
    dbg( TRACE,"SSO_USID_CASE_INSENSITIVE = %s", clSSOUsidCaseInsensitive );

    if( !strcmp( clSSOUsidCaseInsensitive,"YES" ) )
        bgSSOUsidCaseInsensitive = TRUE;
    else
        bgSSOUsidCaseInsensitive = FALSE;

    /* in cfg file:
    [QUEUE_DEFINES]
     * To Overwrite QueId's of sgs.tab
     * Value <0 (or None) Uses Sgs.Tab.PNTAB
     * Value =0 will Stop OutPut to That Process
     * Value >0 Overwrites DestQue
     *QUE_TO_bchdl = -1,4     */
    igToBcHdl = GetOutModIdWithPrio( "bchdl", &igBcHdlPrio );
    dbg( TRACE, "[QUEUE_DEFINES] QUE_TO_bchdl = %d,%d", igToBcHdl, igBcHdlPrio );

    if( igToBcHdl == 0 )
    {
        dbg( TRACE, "Broadcasting has been disabled in the config file!\nTo enable broadcasting set [QUEUE_DEFINES] QUE_TO_bchdl = 0,4" );
    }
    else
    {
        dbg( TRACE, "Broadcasting is enabled and broadcasts will be sent with prio %d - %s", igBcHdlPrio, bgSendSmiBcOnly ? "only SMI broadcasting is enabled." : "all updates will be broadcasted" );
    }

    /* transfer cfg file, necessary for HSB */
    ilFileTrans = TransferFile( pcgCfgFile );

    if( ilFileTrans != RC_SUCCESS )
    {
        dbg( TRACE, "Init: FileTransfer cfgfile to remote host failed" );
    }

    sprintf( pcgBinFile, "%s/%s", getenv( "BIN_PATH" ), mod_name );
    ilFileTrans = TransferFile( pcgBinFile );

    if( ilFileTrans != RC_SUCCESS )
    {
        dbg( TRACE, "Init: FileTransfer binary to remote host failed" );
    }


    /* informix may hang without the following code */
#ifdef CCSDB_INFORMIX
    slFuncCode = START;
    slLocalCursor = 0;
    sprintf( pclSqlBuf, "SET ISOLATION TO COMMITTED READ" );
    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS )
    {
        get_ora_err( ilRc, clOraErr );
        dbg( TRACE, "init_authdl() Set Isolation failed:\n%s", clOraErr );
    }
    else
    {
        dbg( TRACE, "init_authdl() Set Isolation successful" );
    }

    close_my_cursor( &slLocalCursor );
    slLocalCursor = 0;
#endif


    /* initialize global variables */
    InitVarBuf( &rgResultBuf );
    InitVarBuf( &rgFktBuf );
    InitVarBuf( &rgPrvBuf );
    InitVarBuf( &rgProfileList );
    InitVarBuf( &rgAlreadyCheckedList );
    InitVarBuf( &rgBroadcastLines );
    rgBroadcastLines.numObjs = 0;


#ifdef LOGGING
    /* igToLogHdl = tool_get_q_id("loghdl"); */
    igToLogHdl = GetOutModId( "loghdl" );

    /* igToAction = tool_get_q_id("action"); */
    igToAction = GetOutModId( "action" );

    dbg( TRACE, "SPECIAL OUT QUEUES:" );
    dbg( TRACE, "LOGHDL=%d ACTION=%d", igToLogHdl, igToAction );

    sprintf( pcgOutRoute, "%d,%d", igToAction, igToLogHdl );
#endif

    SetHopoDefaults();

    bgPwdtabExists = PwdtabExists();
    GetHopoList();
    CheckModulVersion( FOR_INIT );

    /* Mei */
    LoadDBParam();

    return;

} /* end init_authdl() */

/* in cfg file:
[QUEUE_DEFINES]
 * To Overwrite QueId's of sgs.tab
 * Value <0 (or None) Uses Sgs.Tab.PNTAB
 * Value =0 will Stop OutPut to That Process
 * Value >0 Overwrites DestQue
 *QUE_TO_bchdl = -1,4     */
static int GetOutModIdWithPrio( char *pcpProcName, int *pipPrio )
{
    int ilQueId = -1;
    int ilPrio = -1;
    char pclCfgCode[32];
    char pclTmpData[32];
    char pclTmpBuf1[1000];

    memset( pclTmpBuf1, 0, sizeof( pclTmpBuf1 ) );
    sprintf( pclCfgCode, "QUE_TO_%s", pcpProcName );
    ReadCfg( pcgCfgFile, pclTmpBuf1, "QUE_DEFINES", pclCfgCode, "-1,4" );
    /*(void) GetCfg ("QUEUE_DEFINES", pclCfgCode, CFG_STRING, pclTmpBuf1, "-1,4");*/
    get_real_item( pclTmpData, pclTmpBuf1, 1 );
    ilQueId = atoi( pclTmpData );

    if( ilQueId < 0 )
    {
        ilQueId = tool_get_q_id( pcpProcName );
    } /* end if */

    get_real_item( pclTmpData, pclTmpBuf1, 2 );
    ilPrio = atoi( pclTmpData );

    if( ilPrio >= 0 )
    {
        if( ( ilPrio < 1 ) || ( ilPrio > 5 ) )
        {
            ilPrio = 4;
        } /* end if */
    } /* end if */

    if( ilQueId < 0 )
    {
        ilQueId = 0;
        ilPrio = 0;
    } /* end if */

    *pipPrio = ilPrio;
    return ilQueId;
} /* end of GetOutModId */

static int ReadCfg( char *pcpFile, char *pcpVar, char *pcpSection, char *pcpEntry, char *pcpDefVar )
{

    int ilRc = RC_SUCCESS;

    ilRc = iGetConfigEntry( pcpFile, pcpSection, pcpEntry, 1, pcpVar );

    if( ilRc != RC_SUCCESS || strlen( pcpVar ) <= 0 )
    {
        dbg( DEBUG,
             "ReadCfg() Error reading [%s] %s from the config file (%s)!"
             "--> Using the default value.",
             pcpSection, pcpEntry, pcpFile );
        strcpy( pcpVar, pcpDefVar ); /* copy the default value */
        ilRc = RC_FAIL;
    }

    dbg( DEBUG, "ReadCfg() %s = <%s>", pcpEntry,
         !strcmp( pcpEntry, "PASS" ) ? "********" : pcpVar );

    return ilRc;

} /* end ReadCfg() */

/* ******************************************************************** */
/* The reset routine                                                    */
/* ******************************************************************** */
static int reset()
{
    int rc = 0;             /* Return code */

    if( prgItem != NULL )
        free( ( void* )prgItem );

    prgItem = NULL;

    return rc;

} /* end of reset */

/* ******************************************************************** */
/* The termination routine                                              */
/* ******************************************************************** */
static void terminate()
{
    /* free memory for variable length buffers */
    /*london    FreeVarBuf(&rgUrnoList.varbuf); */
    FreeVarBuf( &rgResultBuf );
    FreeVarBuf( &rgFktBuf );
    FreeVarBuf( &rgPrvBuf );
    FreeVarBuf( &rgBroadcastLines );
    FreeVarBuf( &rgProfileList );
    FreeVarBuf( &rgAlreadyCheckedList );

    if( prgItem != NULL )
        free( ( void* )prgItem );

    prgItem = NULL;

    logoff(); /* disconnect from oracle */

    dbg( TRACE, "terminate: terminating authdl" );
    /*send_message(IPRIO_ADMIN,SHUTD_REQUEST,0,0,NULL);*/

    exit( 0 );

} /* end of terminate */

/* ******************************************************************** */
/* The handle signals routine                                           */
/* ******************************************************************** */
static void handleSignal( int ipSignal )
{
    /* write message if it is not alarm clock */
    if( ipSignal != 14 && ipSignal != 18 )
        dbg( TRACE, "HandleSignal: signal <%d> received", ipSignal );

    switch( ipSignal )
    {
        case SIGALRM:
        case SIGCLD:
            /*
            SetSignals(handleSignal);
             */
            break;

        default:
            exit( 0 );
            break;
    }

    return;

} /* end of handleSignal() */

/* ******************************************************************** */
/* The handle general error routine                                     */
/* ******************************************************************** */
static void handle_err( err )
int err; /* Error code */
{
    if( err )
    {
    }

    return;
} /* end of handle_err */

/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr( int pipErr )
{
    int ilRC = RC_SUCCESS;

    switch( pipErr )
    {
        case QUE_E_FUNC: /* Unknown function */
            dbg( TRACE, "<%d> : unknown function", pipErr );
            break;

        case QUE_E_MEMORY: /* Malloc reports no memory */
            dbg( TRACE, "<%d> : malloc failed", pipErr );
            break;

        case QUE_E_SEND: /* Error using msgsnd */
            dbg( TRACE, "<%d> : msgsnd failed", pipErr );
            break;

        case QUE_E_GET: /* Error using msgrcv */
            dbg( TRACE, "<%d> : msgrcv failed", pipErr );
            break;

        case QUE_E_EXISTS:
            dbg( TRACE, "<%d> : route/queue already exists ", pipErr );
            break;

        case QUE_E_NOFIND:
            dbg( TRACE, "<%d> : route not found ", pipErr );
            break;

        case QUE_E_ACKUNEX:
            dbg( TRACE, "<%d> : unexpected ack received ", pipErr );
            break;

        case QUE_E_STATUS:
            dbg( TRACE, "<%d> :  unknown queue status ", pipErr );
            break;

        case QUE_E_INACTIVE:
            dbg( TRACE, "<%d> : queue is inaktive ", pipErr );
            break;

        case QUE_E_MISACK:
            dbg( TRACE, "<%d> : missing ack ", pipErr );
            break;

        case QUE_E_NOQUEUES:
            dbg( TRACE, "<%d> : queue does not exist", pipErr );
            break;

        case QUE_E_RESP: /* No response on CREATE */
            dbg( TRACE, "<%d> : no response on create", pipErr );
            break;

        case QUE_E_FULL:
            dbg( TRACE, "<%d> : too many route destinations", pipErr );
            break;

        case QUE_E_NOMSG: /* No message on queue */
            dbg( TRACE, "<%d> : no messages on queue", pipErr );
            break;

        case QUE_E_INVORG: /* Mod id by que call is 0 */
            dbg( TRACE, "<%d> : invalid originator=0", pipErr );
            break;

        case QUE_E_NOINIT: /* Queues is not initialized*/
            dbg( TRACE, "<%d> : queues are not initialized", pipErr );
            break;

        case QUE_E_ITOBIG:
            dbg( TRACE, "<%d> : requestet itemsize to big ", pipErr );
            break;

        case QUE_E_BUFSIZ:
            dbg( TRACE, "<%d> : receive buffer to small ", pipErr );
            break;

        default: /* Unknown queue error */
            dbg( TRACE, "<%d> : unknown error", pipErr );
            break;
    } /* end switch */

    return;
} /* end of HandleQueErr */

/* ******************************************************************** */
/* The handle data routine                                              */
/* ******************************************************************** */
static int handle_data()
{
    int ilRc = 0;       /* Return code          */
    BC_HEAD *bchd;      /* Broadcast header     */
    CMDBLK  *cmdblk;    /* Command Block        */
    char *pclData;      /* Data to send back to client  */
    char *pclSelectList;
    char *pclFieldList, *pclDataList, *pclTabSuffix;
    char pclErrorMess[BIGBUF];
    char pclActCmd[16];
    /*Frank@UAL_UAA*/
    char *pclItem = NULL;
    char clUrno[URNOLEN+1] = "\0";     /* user URNO received in user data */
    char clStat[STATLEN+1];     /* STAT received in user data */
    int  ili;/*Frank@UAM*/
    char pclFieldTmp[128];
    char pclFieldTmp1[128];
    char pclDataTmp[128];
    char pclDataTmp1[128];
    char pclFieldListTmp[MAX_FIELD_LEN];
    char pclDataListTmp[4000];
    int ilNumFields;
    char clRmrk[RMRKLEN + 1];
    char clUsid[USIDLEN + 1] = "\0"; /* user USID received in user data */

    memset( clRmrk, 0, sizeof( clRmrk ) );
    /*Frank@UAL_UAA*/

    igQueOut = prgEvent->originator;
    bchd = ( BC_HEAD * )( ( char * ) prgEvent + sizeof( EVENT ) );

    cmdblk = ( CMDBLK * )( ( char * ) bchd->data );
    pclSelectList = cmdblk->data; /* The Selektion is not Used Here */

    pclFieldList = pclSelectList + strlen( pclSelectList ) + 1;
    pclDataList = pclFieldList + strlen( pclFieldList ) + 1;
    pclTabSuffix = cmdblk->obj_name;

    strcpy( pclActCmd, cmdblk->command );

    strncpy( pcgDestName, bchd->dest_name, sizeof( bchd->dest_name ) );
    pcgDestName[sizeof( bchd->dest_name )] = 0x00;
    strcpy( pcgUsername, pcgDestName );

    strncpy( pcgRecvName, bchd->recv_name, sizeof( bchd->recv_name ) );
    pcgRecvName[sizeof( bchd->recv_name )] = 0x00;

    strncpy( pcgTwStart, cmdblk->tw_start, sizeof( cmdblk->tw_start ) );
    pcgTwStart[sizeof( cmdblk->tw_start )] = 0x00;

    strncpy( pcgTwEnd, cmdblk->tw_end, sizeof( cmdblk->tw_end ) );
    pcgTwEnd[sizeof( cmdblk->tw_end )] = 0x00;

    if( !strcmp( pclActCmd, "SEC" ) )
    {
        dbg( TRACE, "CMD <%s/%s> QUE (%d) WKS <%s> USR <%s>", pclActCmd, pclSelectList, igQueOut, pcgRecvName, pcgDestName );
    }
    else
    {
        dbg( TRACE, "CMD <%s> QUE (%d) WKS <%s> USR <%s>", pclActCmd, igQueOut, pcgRecvName, pcgDestName );
    }

    dbg( TRACE, "SPECIAL INFO TWS <%s> TWE <%s>", pcgTwStart, pcgTwEnd );


    ilRc = CheckModulVersion( FOR_CHECK );

    if( ilRc != RC_SUCCESS )
    {
        dbg( TRACE, "ILLEGAL VERSION <%s> WKS <%s> USR <%s>", pcgTwEnd, pcgRecvName, pcgDestName );
        return RC_SUCCESS;
    } /* end if */

    ilRc = CheckHomePort();

    if( ilRc != RC_SUCCESS )
    {
        sprintf( pclErrorMess, "Illegal Home Airport %s\nValid Airports: %s", pcgH3LC, pcgHopoList );
        tools_send_sql_rc( igQueOut, cmdblk->command, pclTabSuffix, pclSelectList, pclFieldList, pclErrorMess, ilRc );
        return RC_SUCCESS;
    } /* end if */

    strcpy( pclTabSuffix, pcgTblExt );

    dbg( DEBUG, "Table Names = <%s>", pclTabSuffix );
    dbg( DEBUG, "Field List <%s>", pclFieldList );

    if( strstr( pclFieldList, "PASS" ) == NULL && strstr( pclFieldList, "OLDP" ) == NULL && strstr( pclFieldList, "NEWP" ) == NULL && strlen( pclDataList ) <= 500 )
        dbg( DEBUG, "Data List <%s>", pclDataList );
    else
        dbg( DEBUG, "Data List <Not printed because it contains PASS or is too long!>" );


    /* get the table names */
    sprintf( pcgSectab, "SEC%s", pclTabSuffix );
    sprintf( pcgCaltab, "CAL%s", pclTabSuffix );
    sprintf( pcgGrptab, "GRP%s", pclTabSuffix );
    sprintf( pcgPrvtab, "PRV%s", pclTabSuffix );
    sprintf( pcgFkttab, "FKT%s", pclTabSuffix );
    sprintf( pcgPwdtab, "PWD%s", pclTabSuffix );
    /*Frank@UAL_UAA*/
    /* sprintf(pcgUaltab,"UAP%s",pclTabSuffix);    */
    sprintf( pcgUaltab, "UAL%s", pclTabSuffix );
    sprintf( pcgUaatab, "UAA%s", pclTabSuffix );
    /* sprintf(pcgUaatab,"UAB%s",pclTabSuffix);     */
    /*Frank@UAL_UAA*/
    sprintf(pcgGcftab, "GCF%s", pclTabSuffix);

    memset( pclErrorMess, 0, sizeof( pclErrorMess ) );
    ilRc = RC_SUCCESS;

    if( strcmp( cmdblk->command, "SEC" ) == 0 )
    {
        if( strstr( pclSelectList, NEWSEC_COMMAND ) != NULL )
        {
            /* create a new user/profile/group etc */
            ilRc = handleNewSec( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, UPDSEC_COMMAND ) != NULL )
        {
            /*Frank@UAM*/
            if( strstr( pcgTwEnd, "UAM" ) != 0 )
            {
                dbg( DEBUG, "Frank@UAM:This app is UAM" );

                /* get RMRK from the userdata */
                pclItem = getItem( pclFieldList, pclDataList, "REMA" );
                dbg( DEBUG, "Frank@UAM RMRK pclItem is <%s>", pclItem );

                if( pclItem != NULL )
                {
                    strncpy( clRmrk, pclItem, 128 );
                }

                dbg( DEBUG, "Frank@UAM RMRK for UALTAB is <%s>", clRmrk );

                ilNumFields = itemCount( pclFieldList ); /*num flds in the field list*/
                dbg( DEBUG, "Frank@UAM the length pclFieldList is <%d>", ilNumFields );

                memset( pclFieldListTmp, 0, strlen( pclFieldListTmp ) );
                memset( pclDataListTmp, 0, strlen( pclDataListTmp ) );

                for( ili = 0; ili < ilNumFields; ili++ )
                {
                    memset( pclFieldTmp, 0, strlen( pclFieldTmp ) );
                    memset( pclFieldTmp1, 0, strlen( pclFieldTmp1 ) );
                    memset( pclDataTmp, 0, strlen( pclDataTmp ) );
                    memset( pclDataTmp1, 0, strlen( pclDataTmp1 ) );

                    get_real_item( pclFieldTmp, pclFieldList, ili + 1 );

                    if( strcmp( pclFieldTmp, "REMA" ) == 0 )
                    {
                        dbg( DEBUG, "found REMA, continue" );
                        continue;
                    }
                    else
                    {
                        get_real_item( pclDataTmp, pclDataList, ili + 1 );

                        sprintf( pclFieldTmp1, "%s,", pclFieldTmp );
                        strcat( pclFieldListTmp, pclFieldTmp1 );
                        sprintf( pclDataTmp1, "%s,", pclDataTmp );
                        strcat( pclDataListTmp, pclDataTmp1 );
                    }
                }

                pclFieldListTmp[strlen( pclFieldListTmp ) - 1] = '\0';
                pclDataListTmp[strlen( pclDataListTmp ) - 1] = '\0';

                dbg( DEBUG, "Frank@UAM pclFieldListTmp<%s>", pclFieldListTmp );
                dbg( DEBUG, "Frank@UAM pclDataListTmp<%s>", pclDataListTmp );

                strcpy( pclFieldList, pclFieldListTmp );
                strcpy( pclDataList, pclDataListTmp );
            }
            else
            {
                dbg( DEBUG, "Frank@UAM:This app is not UAM" );
            }

            /*Frank@UAM*/

            /* update records in SEC/CAL-TAB */
            ilRc = handleUpdSec( pclFieldList, pclDataList, pclErrorMess );

            /*Frank@UAL_UAA*/
            if( bgUpdateUALtab == TRUE )
            {
                /* get URNO,STAT from the userdata */
                pclItem = getItem( pclFieldList, pclDataList, "URNO" );

                if( pclItem != NULL )
                {
                    strcpy( clUrno, pclItem );
                }

                dbg( DEBUG, "URNO for UALTAB is <%s>", clUrno );

                pclItem = getItem( pclFieldList, pclDataList, "STAT" );

                if( pclItem != NULL )
                {
                    strcpy( clStat, pclItem );
                }

                dbg( DEBUG, "STAT for UALTAB is <%s>", clStat );

                if( atoi( clStat ) == ACTIVE )
                {
                    /*Frank@UAM*/
                    InsertUalTab( clUrno, pclErrorMess, FALSE, ACTIVE, clRmrk );
                    /* InsertUalTab(char *pcpUrno,char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived, char *pcpRema);  */
                }
                else if( atoi( clStat ) == DEACTIVE )
                {
                    /*Frank@UAM*/
                    InsertUalTab( clUrno, pclErrorMess, FALSE, DEACTIVE, clRmrk );
                }
            }/* end of if  */

            /*Frank@UAL_UAA*/
        }
        else if( strstr( pclSelectList, DELSEC_COMMAND ) != NULL )
        {
            /* delete a user,profile,group,wks,wksgrp */
            ilRc = handleDelSec( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, NEWCAL_COMMAND ) != NULL )
        {
            /* insert records into CALTAB */
            ilRc = handleNewCal( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, UPDCAL_COMMAND ) != NULL )
        {
            /* update records in CALTAB */
            ilRc = handleUpdCal( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, DELCAL_COMMAND ) != NULL )
        {
            /* delete records from CALTAB */
            ilRc = handleDelCal( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, SETPWD_COMMAND ) != NULL )
        {
            /* Set Password */
            ilRc = handleSetPwd( pclFieldList, pclDataList, pclErrorMess, TRUE );
        }
        else if( strstr( pclSelectList, NEWPWD_COMMAND ) != NULL )
        {
            /* New Password - called from BDPSPASS */
            ilRc = handleNewPwd( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, SETPRV_COMMAND ) != NULL )
        {
            /* Set Rights - update STAT field in PRVTAB   */
            ilRc = handleSetRgt( pcgPrvtab, pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, SETFKT_COMMAND ) != NULL )
        {
            /* Set Rights - update STAT field in FKTTAB   */
            ilRc = handleSetRgt( pcgFkttab, pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, NEWAPP_COMMAND ) != NULL )
        {
            /* create a new application */
            ilRc = handleNewApp( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, NEWREL_COMMAND ) != NULL )
        {
            /* create one or more new relations */
            ilRc = handleNewRel( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, DELREL_COMMAND ) != NULL )
        {
            /* delete one or more relations */
            ilRc = handleDelRel( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, ADDAPP_COMMAND ) != NULL )
        {
            /* install an application for a profile */
            ilRc = handleAddApp( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, DELAPP_COMMAND ) != NULL )
        {
            /* delete an application */
            ilRc = handleDelApp( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, UPDAPP_COMMAND ) != NULL )
        {
            /* reinstall an application for a profile */
            ilRc = handleUpdApp( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, REMAPP_COMMAND ) != NULL )
        {
            /* deinstall an application for a profile */
            ilRc = handleRemApp( pclFieldList, pclDataList, pclErrorMess );
        }
        else if( strstr( pclSelectList, "READTAB" ) != NULL )
        {
            /* read all records in the specified table */
            ilRc = handleReadTab( pclFieldList, pclTabSuffix, pclErrorMess );
        }
        else
        {
            sprintf( pclErrorMess, "Unknown where condition received (%s)", pclSelectList );
            ilRc = RC_FAIL;
        }
    }
    else if( strcmp( cmdblk->command, "GPR" ) == 0 || strcmp( cmdblk->command, "SSO" ) == 0 )
    {
        if( strncmp( cmdblk->command, "GPR", 3 ) == 0 )
        {
            dbg( DEBUG, "line<%d> GPR command received", __LINE__ );
            ilRc = handleGetPrivileges( pclFieldList, pclDataList, pclErrorMess, pcgRecvName, "GPR" );

            /* Frank v1.32 UFIS2044 */
            /* For Test  */
            if( ilRc == RC_SUCCESS && bgCaptureLoginInfo == TRUE )
            {
                ilRc = CaptureLogInInfoToUaltab( pclFieldList, pclDataList, pcgRecvName, pcgDestName, pclErrorMess );
            }
        }
        else if( strncmp( cmdblk->command, "SSO", 3 ) == 0 )
        {
            dbg( DEBUG, "line<%d> SSO command received", __LINE__ );
            ilRc = handleGetPrivileges( pclFieldList, pclDataList, pclErrorMess, pcgRecvName, "SSO" );
        }
    }
    else if( strcmp( cmdblk->command, "PARA" ) == 0 )
    {
        dbg( TRACE, "Reloading param..." );
        LoadDBParam();
        dbg( TRACE, "New param loaded" );
        return RC_SUCCESS;
    }
    else if( strcmp( cmdblk->command, SMI_COMMAND ) == 0 )
    {

        ilRc = handleModIni( pclFieldList, pclDataList, pclErrorMess );
    }
    else
    {
        dbg( TRACE, "Unknown command received (%s)", cmdblk->command );
        ilRc = RC_FAIL;
    } /* end else */

    if( ilRc != RC_SUCCESS )
    {

        dbg( DEBUG, "*** ERROR *** %s *** ERROR ***", GetCurrTime() );

        /* rollback SQL commands and the logging transaction */
        /* and pop and log all errors from the error stack   */
        handleRollback();

        /* log some info about the command received */
        dbg( DEBUG, "SenderID <%d> Command <%s> Where <%s>", igQueOut, cmdblk->command, pclSelectList );
        dbg( DEBUG, "Table Names = <%s>", pclTabSuffix );
        dbg( DEBUG, "Field List <%s>", pclFieldList );

        /* print the data area - but not for login errors */
        /* (ie. not for invalid password etc.)            */
        if( strstr( pclErrorMess, LOGINERROR ) == NULL )
            dbg( DEBUG, "Data List...\n%s\n...End Data List", pclDataList );

        /* copy error message to client data area */
        pclData = pclErrorMess;
        dbg( DEBUG, "pclErrorMess %s", pclErrorMess );
        ilRc = RC_FAIL; /* error occurred */
    }
    else
    {
        /* commit SQL commands and send commit to loghdl */
        handleCommit();

        pclData = ( char* ) rgResultBuf.buf;

        if( strcmp( cmdblk->command, "GPR" ) != 0 )
        {
            dbg( DEBUG, "Return to client %s:\n%.*s", ( strlen( pclData ) > 500 ) ? "(first 500 bytes)" : "", 500, pclData );
        }

        ilRc = RC_SUCCESS;
    }

    dbg( TRACE, "tools_send_sql_rc igQueOut %d   cmdblk->command %s ", igQueOut, cmdblk->command );
    dbg( TRACE, "cmd = <%s>", cmdblk->command );
    dbg( TRACE, "sel = <%s>", pclSelectList );
    dbg( TRACE, "fld = <%s>", pclFieldList );
    dbg( TRACE, "dat = <%s>", pclData );
    dbg( TRACE, "ilRc = <%d>", ilRc );
    tools_send_sql_rc( igQueOut, cmdblk->command, pclTabSuffix, pclSelectList, pclFieldList, pclData, ilRc );

    return ilRc;
} /* end of handle_data() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    handleCommit()                                          */
/*                                                                      */
/* Parameters:  None.                                                   */
/*                                                                      */
/* Returns: None.                                                       */
/*                                                                      */
/* Description: Commits sql commands and sends commit to loghdl.        */
/*                                                                      */
/* ******************************************************************** */
void handleCommit( void )
{
    if( SendBroadcastLines() == RC_SUCCESS )
    {
        dbg( DEBUG, "***** Success! Committing changes. *****" );

        /* commit any SQL commands */
        commit_work();
    }
    else
    {
        handleRollback();
    }

    return;

} /* end handleCommit() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    handleRollback()                                        */
/*                                                                      */
/* Parameters:  None.                                                   */
/*                                                                      */
/* Returns: None.                                                       */
/*                                                                      */
/* Description: Logs any errors added to the error stack, rollsback     */
/*              sql commands and sends rollback to the logging hdl.     */
/*                                                                      */
/* ******************************************************************** */
void
handleRollback( void )
{

    int ilRc = RC_SUCCESS;

    /* log any errors on the error stack */
    LogErrors( &rgErrList );

    dbg( DEBUG, "***** Rolling back due to errors. *****" );

    /* rollback any SQL commands */
    rollback();

    CancelSendBroadcastLines();

} /* end handleRollback() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleGetPrivileges()                                   */
/*                                                                      */
/* Parameters:  pcpFieldList.                                           */
/*              pcpDataList.                                            */
/*              pcpErrorMess - log in errors returned to the user.      */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: Command to check user privileges and return a profile.  */
/*              Fields: "USID,PASS,APPL,WKST"                           */
/*                                                                      */
/* ******************************************************************** */
static int
handleGetPrivileges( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess,
                     char *pcpWksHex, char *pcpCommandType )
{
    BOOL blIsAdmin = FALSE;
    BOOL blIncDay = FALSE;
    int ilRc = RC_SUCCESS;
    int ilRc2;
    char clFsec[URNOLEN + 1];
    char clUserUrno[URNOLEN + 1];
    char clFapp[URNOLEN + 1];
    char clFwks[URNOLEN + 1];
    char clUsid[USIDLEN + 1]; /* user ID received in user data */
    char clPass[PASSLEN + 1]; /* password received in user data */
    char clAppl[USIDLEN + 1]; /* application received in user data */
    char clWkst[USIDLEN + 1]; /* workstation received in user data */
    char pclTimeStr[30] = "\0";
    char *pclItem;
    char *pclWks = NULL;

    char pclProu[128] = "\0";

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );


    memset( clUsid, 0, USIDLEN + 1 );
    memset( clPass, 0, PASSLEN + 1 );
    memset( clAppl, 0, USIDLEN + 1 );
    memset( clWkst, 0, USIDLEN + 1 );
    memset( clUserUrno, 0, sizeof(clUserUrno));
    memset( clFsec, 0, sizeof(clFsec));
    memset( clFapp, 0, sizeof(clFapp));
    memset( clFwks, 0, sizeof(clFwks));

    /* get the current date and day of week */
    GetCurrDate();


    /* get USID,PASS from the userdata */
    pclItem = getItem( pcpFieldList, pcpDataList, "USID" );

    if( pclItem != NULL )
    {
        strcpy( clUsid,pclItem );

        if( ( bgSSOUsidCaseInsensitive == TRUE ) && ( strncmp( pcpCommandType,"SSO",3 )==0 ) )
        {
            /*  change Usid to upper case  */
            str_chg_upc( clUsid );
        }
    }

    pclItem = getItem( pcpFieldList,pcpDataList,"PASS" );

    if( pclItem != NULL )
        strcpy( clPass, pclItem );

    pclItem = getItem( pcpFieldList, pcpDataList, "APPL" );

    if( pclItem != NULL )
        strcpy( clAppl, pclItem );

    pclItem = getItem( pcpFieldList, pcpDataList, "WKST" );

    if( pclItem != NULL )
        strcpy( clWkst, pclItem );

    /* New User Setting Configuration Implementation JIRA-3793 */
    pclItem = getItem( pcpFieldList, pcpDataList, "#PROU" );

    if( pclItem != NULL )
    {
        strcpy( pclProu, pclItem );
        igNewConfMod = TRUE;
        str_trm_all( pclProu, " ", TRUE );
    }
    else
    {
        igNewConfMod = FALSE;
    }

    if( strlen( clUsid ) <= 0 || strlen( clAppl ) <= 0 )
    {
        sprintf( pcpErrorMess, "USID or APPL not found in the field list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    dbg( TRACE, "LOGIN: USID <%s> APPL <%s> WKST <%s> HEX <%s>",
         clUsid, clAppl, clWkst, pcpWksHex );

    blIsAdmin = FALSE;

    if( !strcmp( clUsid, UFISADMIN ) )
        blIsAdmin = TRUE;

    /* check if the user is enabled */
    dbg( DEBUG, "GetPriv() **** Check User Enabled ****" );

    if( strncmp( pcpCommandType, "GPR", 3 ) == 0 )
    {
        dbg( DEBUG, "line<%d>the command is GPR", __LINE__ );
        ilRc = CheckValidSecCal( clUsid, clPass, "U", clFsec, pcpErrorMess );
    }
    else if( strncmp( pcpCommandType, "SSO", 3 ) == 0 )
    {
        dbg( DEBUG, "line<%d>the command is SSO", __LINE__ );
        ilRc = CheckValidSecCal( clUsid, NULL, "U", clFsec, pcpErrorMess );
    }

    strcpy( clUserUrno, clFsec );

    /* check if the application is valid */
    if( ilRc == RC_SUCCESS )
    {
        dbg( DEBUG, "GetPriv() **** Check Application Enabled ****" );
        ilRc = CheckValidSecCal( clAppl, NULL, "A", clFapp, pcpErrorMess );
    }

    /* if the application is BDPS-SEC then check that */
    /* the user is a superuser (UFIS$ADMIN or SECTAB.LANG == '1') */
	
	/* fya UFIS-7203 */
    /*if( ilRc == RC_SUCCESS && !strcmp( clAppl, BDPSSEC ) && blIsAdmin == FALSE )*/
    if( ilRc == RC_SUCCESS && (!strcmp( clAppl, BDPSSEC ) || !strcmp( clAppl, pcgDefModu )) && blIsAdmin == FALSE )
    {
        dbg( DEBUG, "GetPriv() **** Check for superuser (BDPS-SEC or UAC only) ****" );
        ilRc = CheckIsSuperUser( clFsec, pcpErrorMess);
    }

    /* check if the workstation is valid */
    if( ilRc == RC_SUCCESS && strlen( clWkst ) > 0 )
    {

        dbg( DEBUG, "GetPriv() **** Check Workstation Enabled ****" );
        ilRc = CheckValidSecCal( clWkst, NULL, "W", clFwks, pcpErrorMess );

        /* it is not an error to have no workstation record */
        if( ilRc == RC_NO_WORKSTATION )
        {
            strcpy( pcpErrorMess, "" );
            ilRc = RC_SUCCESS;
        }
        else if( ilRc == RC_SUCCESS )
        {
            dbg( DEBUG, "GetPriv() Using Wks URNO instead of user." );
            /* if wrkstation record exists use it instead of user */
            /* strcpy(clFsec,clFwks); */
            pclWks = clFwks;
        }
    }

    /* the user must reset the password before it is used the first time */
    if( igPasswordExpiryDays > 0 && bgPwdtabExists && ilRc == RC_SUCCESS )
    {
        ilRc = CheckForUnusedPassword( clUserUrno, pcpErrorMess );
    }

    /* Mei */
    blIncDay = TRUE;

    if( blIsAdmin == TRUE )
        blIncDay = FALSE;

    dbg( DEBUG, "Admin <%d> Include Day <%d>", blIsAdmin, blIncDay );

    if( blIsAdmin == FALSE || blIncDay == TRUE ) /* should not deactivate the superuser */
    {
        if( ilRc == RC_SUCCESS )
        {
            /* sucessful login so set login count to zero */
            ilRc2 = CheckValidLogin( clFsec, TRUE, blIsAdmin, blIncDay, pcpErrorMess );
        }
        else
        {
            /* increment counter of unsucessful logins - if limit reached then deactivate the user */
            ilRc2 = CheckValidLogin( clFsec, FALSE, blIsAdmin, blIncDay, pcpErrorMess );
        }
    }

    if( ilRc2 == RC_FAIL )
        ilRc = ilRc2;

    /* create privilege list to be sent back to the client in rgResultBuf */
    if( ilRc == RC_SUCCESS )
    {
        dbg( DEBUG, "GetPriv() **** Creating the privilege list ****" );

        ilRc = CreatePrivList( blIncDay, clFsec, clFapp, &rgResultBuf, pcpErrorMess, pclWks, clUsid, pclProu, pcpCommandType );
    }

    rgProfileList.numObjs = 0;

    /* FYA 20130920 */
    dbg( TRACE,"ilRc<%d>RC_SUCCESS<%d>line<%d>bgDbConfig<%d>TRUE<%d>",ilRc,RC_SUCCESS,__LINE__,bgDbConfig,TRUE );

    if( ilRc != RC_SUCCESS )
        LogErrors( &rgErrList );
    else
    {
        /* Mei */
        dbg( TRACE, "GetPriv: Upd Login Time" );
        SetLoginTime( clFsec );
    }

    return ilRc;

} /* end handleGetPrivileges() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    CreatePrivList()                                        */
/*                                                                      */
/* Parameters:  pcpFsec - URNO of user/wks in SECTAB.                   */
/*              pcpFapp - URNO of application in SECTAB.                */
/*              prpResultBuf - buffer to receive a profile.             */
/*              pcpErrorMess - log in error messages.                   */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: Creates a profile for the user/appl from one or many    */
/*              profiles.                                               */
/*                                                                      */
/* ******************************************************************** */
static int CreatePrivList( BOOL bpIncDay, char *pcpFsec, char *pcpFapp, VARBUF *prpResultBuf,
                           char *pcpErrorMess, char *pcpWksUrno, char *pcpUsid, char *pcpProu, char *pcpCommandType )
{
    FKTREC *prlFktRec = NULL;
    int ilNumFktRecs, ilFktLoop;
    int ilDiff;
    int ilRc;
    int ilNumPrvRecs = 0;
    short slLocalCursor, slFuncCode;
    char clWhere[BIGBUF];
    char clFprv[URNOLEN + 1], clFfkt[URNOLEN + 1], clStat[STATLEN + 1];
    char pclProfileUrnos[BIGBUF];

    char pclNewFormat[BIGBUF] = "\0";

    /* for the application specified, load a list of FKT records */
    dbg( DEBUG, "CreatePrivList() **** Load FKTmap ****" );
    sprintf( clWhere, "FAPP=%s ORDER BY FUNC", pcpFapp );
    ilRc = LoadFktTab( clWhere, &rgFktBuf );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList, "CreatePrivList() Error loading FKTTAB." );
        ilRc = RC_FAIL;
    }

    /* loop through the records loaded and set the STAT to hidden
       (lowest priority status) */
    if( ilRc == RC_SUCCESS )
    {
        /* point to the list of FKT records */
        prlFktRec = ( FKTREC * ) rgFktBuf.buf;
        ilNumFktRecs = rgFktBuf.numObjs;

        if( ilNumFktRecs <= 0 )
        {
            PushError( &rgErrList, "CreatePrivList() 0 records found in FKTTAB." );
            ilRc = RC_FAIL;
        }

        for( ilFktLoop = 0; ilFktLoop < ilNumFktRecs; ilFktLoop++ )
            strcpy( prlFktRec[ilFktLoop].STAT, STAT_HIDDEN );
    }

    /* create a list of profile URNOs for this user or workstation */
    ClearVarBuf( &rgProfileList );
    rgProfileList.numObjs = 0;
    ClearVarBuf( &rgAlreadyCheckedList );
    rgAlreadyCheckedList.numObjs = 0;
    dbg( DEBUG, "CreatePrivList() **** Loading Profile URNOs ****" );
    ilRc = GetProfiles( pcpFsec );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList, "CreatePrivList() Error GetProfiles()." );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS && pcpWksUrno != NULL )
    {
        dbg( DEBUG, "CreatePrivList() **** Loading Workstation Profile URNOs ****" );
        ilRc = GetProfiles( pcpWksUrno );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "CreatePrivList() Error GetProfiles() for Workstation." );
            ilRc = RC_FAIL;
        }
    }


    /* loop through the profile URNOs, read the profile from the DB and update the FktMap */

    if( ilRc == RC_SUCCESS )
    {

        if( igNewConfMod == TRUE )
        {
            ChgProfListForNewConFormat( pcpProu );
        }

        URNOREC *prlUrnoList = ( URNOREC * ) rgProfileList.buf;
        int ilNumUrnos = rgProfileList.numObjs;
        int ilLoop;
        char pclSqlBuf[2048],pclDataArea[2048];
        BOOL blIsValidAppForProf = FALSE;
        int ilCountValidProf = 0;

        dbg( DEBUG, "CreatePrivList() **** Merging profs with FKTmap ****" );

#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "SELECT URNO,FFKT,STAT FROM %s WHERE FSEC=:Vfsec AND FAPP=:Vfapp", pcgPrvtab );
#else
        sprintf(pclSqlBuf, "SELECT URNO,FFKT,STAT FROM %s WHERE FSEC=? AND FAPP=?", pcgPrvtab);
#endif

        CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
        memset( pclProfileUrnos, 0, BIGBUF );

        for( ilLoop = 0; ilLoop < ilNumUrnos; ilLoop++ )
        {
            sprintf( pclDataArea, "%s\n%s\n", prlUrnoList[ilLoop].URNO, pcpFapp );
            dbg( TRACE, "VALUES USED : <%s>", pclDataArea );
            nlton( pclDataArea );

            slLocalCursor = 0;
            ilRc = DB_SUCCESS;
            slFuncCode = START;
            blIsValidAppForProf = FALSE;

            /* loop - read and process a line at a time */
            while( ilRc == DB_SUCCESS )
            {
                memset( clFprv, 0, sizeof( clFprv ) );
                memset( clFfkt, 0, sizeof( clFfkt ) );
                memset( clStat, 0, sizeof( clStat ) );

                /* read a line from the DB */
                ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

                if( ( FALSE == blIsValidAppForProf ) && ( ilRc == DB_SUCCESS ) )
                    blIsValidAppForProf = TRUE;

                if( ilRc == DB_SUCCESS )
                {
                    /* process the fields in the line just read */
                    get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clFprv );
                    get_fld( pclDataArea, FIELD_2, STR, URNOLEN, clFfkt );
                    get_fld( pclDataArea, FIELD_3, STR, STATLEN, clStat );

                    if( ilLoop < ( ilNumUrnos - 1 ) || pcpWksUrno == NULL )
                    {
                        /* set the status in the FKT map */
                        ilRc = SetPrivStat( &rgFktBuf, clFfkt, clFprv, clStat, FALSE );
                    }
                    else
                    {
                        /* set the status in the FKT map */
                        ilRc = SetWksPrivStat( &rgFktBuf, clFfkt, clFprv, clStat, FALSE );
                    }

                    /* count of profile records */
                    if( ilRc == RC_SUCCESS )
                        ilNumPrvRecs++;

                    ilRc = RC_SUCCESS;
                    slFuncCode = NEXT;

                } /* end if */

            } /* end while */

            /*v1.36 Only return client PROU when there is something in PRVTAB */
            /* Frank v1.31 */
            if( blIsValidAppForProf && ( bgSendProfileUrnoToClient == TRUE ) )
            {
                dbg( DEBUG, "COLLECTING PROFILE URNOS ... %d ... <%s>", ilLoop, prlUrnoList[ilLoop].URNO );

                if( ilCountValidProf > 0 )  /* Not the first PROU */
                    strcat( pclProfileUrnos, ";" );

                strcat( pclProfileUrnos, prlUrnoList[ilLoop].URNO );
                ilCountValidProf++;
                dbg( TRACE,"------ ALL PROFILE URNOS <%s> ------", pclProfileUrnos );
            }

            /* Frank v1.31 */

            close_my_cursor( &slLocalCursor );
            ilRc = RC_SUCCESS;
        } /* end for() */
    }

    /* check for undefined profile */
    if( ilNumPrvRecs <= 0 )
    {
        sprintf( pcpErrorMess, "%s UNDEFINED_PROFILE", LOGINERROR );
        ilRc = RC_FAIL;
    }

    /* loop through the FKT map creating the return buffer */
    if( ilRc == RC_SUCCESS )
    {
        char clTmp[BIGBUF];

        /* add "[PRV]" to the result buffer */
        strcpy( clTmp, "[PRV]" );
        ilRc = AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) );

        if( bgDbConfig == TRUE && bpIncDay == TRUE )
        {
            /** Mei, include the number of days of password used in [DAY] **/
            ilDiff = igPasswordExpiryDays - igPasswordUsedDays;

            if( ilDiff > 0 && ilDiff <= rgParam[IDX_PWD_CHANGE].iValue )
            {
                sprintf( clTmp, "\n[DAY]%d", ilDiff );
                ilRc = AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) );
            }
        }

        /* --------------------------------------- */
        /* Frank v1.31 MEI v1.33 */
        if( bgSendProfileUrnoToClient == TRUE )
        {
            if( igNewConfMod == TRUE )
            {
                memset( pclNewFormat, 0, BIGBUF );
                ilRc = NewConFormat( pclNewFormat, pcpUsid, pclProfileUrnos, pcpCommandType );
                sprintf( clTmp,"%s", pclNewFormat );
            }
            else
                sprintf( clTmp,"\n[PRO]%s", pclProfileUrnos );

            ilRc = AddVarBuf( prpResultBuf,clTmp,strlen( clTmp ) );
        }/* Frank v1.31  MEI v1.33*/

        /* --------------------------------------- */

        /* point to the list of FKT records */
        prlFktRec = ( FKTREC * ) rgFktBuf.buf;
        ilNumFktRecs = rgFktBuf.numObjs;

        /*
        for(ilFktLoop=0; ilFktLoop<ilNumFktRecs; ilFktLoop++)
            dbg(DEBUG,"%s,%s,%s", prlFktRec[ilFktLoop].FPRV, prlFktRec[ilFktLoop].FUNC, prlFktRec[ilFktLoop].STAT);
         */

        dbg( DEBUG, "======= Merge profiles returns to client =======" );
        dbg( DEBUG, "PRV.URNO (profile), FKT.FUNC (key), FKT.STAT (status)" );

        if( ilNumFktRecs == 1 )
        {
            sprintf( clTmp, "\n%s,%s,%s", prlFktRec[0].FPRV, prlFktRec[0].FUNC, prlFktRec[0].STAT );
            ilRc = AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) );
            dbg( DEBUG, "%s,%s,%s", prlFktRec[0].FPRV, prlFktRec[0].FUNC, prlFktRec[0].STAT );
        }
        else if( ilNumFktRecs > 1 )
        {
            char clFPRV[URNOLEN + 1];
            char clFUNC[FUNCLEN + 1];
            char clSTAT[STATLEN + 1];
            BOOL blUpdate = FALSE;

            strcpy( clFPRV, prlFktRec[0].FPRV );
            strcpy( clFUNC, prlFktRec[0].FUNC );
            strcpy( clSTAT, prlFktRec[0].STAT );

            for( ilFktLoop = 1; ilRc == RC_SUCCESS && ilFktLoop <= ilNumFktRecs; ilFktLoop++ )
            {
                blUpdate = FALSE;

                if( ilFktLoop < ilNumFktRecs )
                {
                    if( !strcmp( prlFktRec[ilFktLoop].FUNC, clFUNC ) )
                    {
                        /* two FKT recs with same FUNC have been found - this happens when an application has
                           been re-registered but not all profiles have been updated yet - chose the best one
                           ie. if one is referenced by the profile and the other not OR the status is higher */
                        if( strlen( clFPRV ) <= 0 && strlen( prlFktRec[ilFktLoop].FPRV ) > 0 )
                        {
                            dbg( DEBUG, "REJECT (NO PRV.URNO) %s,%s,%s", clFPRV, clFUNC, clSTAT );
                            strcpy( clFPRV, prlFktRec[ilFktLoop].FPRV );
                            strcpy( clFUNC, prlFktRec[ilFktLoop].FUNC );
                            strcpy( clSTAT, prlFktRec[ilFktLoop].STAT );
                        }
                        else
                        {
                            /* check if profile STAT > FKT map STAT */
                            char clNewStat[STATLEN + 1];
                            strcpy( clNewStat, HIGHEST_STAT( prlFktRec[ilFktLoop].STAT, clSTAT ) );

                            if( strcmp( clSTAT, clNewStat ) )
                            {
                                dbg( DEBUG, "REJECT (LOWER STAT) %s,%s,%s", clFPRV, clFUNC, clSTAT );
                                strcpy( clFPRV, prlFktRec[ilFktLoop].FPRV );
                                strcpy( clFUNC, prlFktRec[ilFktLoop].FUNC );
                                strcpy( clSTAT, prlFktRec[ilFktLoop].STAT );
                            }
                        }
                    }
                }

                if( ilFktLoop == ilNumFktRecs )
                {
                    blUpdate = TRUE;
                }
                else if( strcmp( prlFktRec[ilFktLoop].FUNC, clFUNC ) )
                {
                    blUpdate = TRUE;
                }

                if( blUpdate )
                {
                    if( strlen( clFPRV ) <= 0 )
                    {
                        dbg( DEBUG, "REJECT (NO PRV.URNO) %s,%s,%s", clFPRV, clFUNC, clSTAT );
                    }
                    else
                    {
                        /* is either the last record or a different key */
                        sprintf( clTmp, "\n%s,%s,%s", clFPRV, clFUNC, clSTAT );
                        ilRc = AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) );
                        dbg( DEBUG, "%s,%s,%s", clFPRV, clFUNC, clSTAT );
                    }

                    if( ilFktLoop < ilNumFktRecs )
                    {
                        strcpy( clFPRV, prlFktRec[ilFktLoop].FPRV );
                        strcpy( clFUNC, prlFktRec[ilFktLoop].FUNC );
                        strcpy( clSTAT, prlFktRec[ilFktLoop].STAT );
                    }
                }
            }
        }

        dbg( DEBUG, "============ END Merge profiles END ============" );
    }

    return ilRc;

} /* end CreatePrivList() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    GetProfiles()                                           */
/*                                                                      */
/* Parameters:  pcpFsec - URNO of user/wks in SECTAB. (FSEC in GRPTAB). */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: Reads all records from GRPTAB for FSEC.                 */
/*              If the type of the record read is 'P' (profile)         */
/*              then add the FREL (URNO of profile in SECTAB) to a      */
/*              global profile list.                                    */
/*              If the type of the record read is 'G' (group)           */
/*              then use FREL to search downwards until only 'P's       */
/*              are found, which are in turn added to the profile list. */
/*                                                                      */
/* ******************************************************************** */
static int GetProfiles( char *pcpFsec )
{
    int ilRc = RC_SUCCESS;
    char pclSqlBuf[2048], pclDataArea[2048];
    short slLocalCursor, slFuncCode;
    GRPREC rlGrpRec, *prlGrpRec;
    VARBUF rlGrpList;
    int ilGrp;
    char pclErrorMess[BIGBUF];

    dbg( DEBUG, "GetProfiles() **** FSEC <%s> ****", pcpFsec );

    /* init list to hold groups read */
    InitVarBuf( &rlGrpList );
    rlGrpList.numObjs = 0;

    /* select group tab records by FSEC , need to order by type so that recs with TYPE 'G' (groups)
       are returned first - and added to the alreadyCheckedList before profile checking starts */
    sprintf( pclSqlBuf, "SELECT URNO,FREL,TYPE FROM %s WHERE FSEC=%s ORDER BY TYPE", pcgGrptab, pcpFsec );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;


    /* read the GRP recs for FSEC, write URNOs into rlGrpList */
    while( ilRc == DB_SUCCESS )
    {
        memset( &rlGrpRec, 0, sizeof( rlGrpRec ) );

        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {

            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, rlGrpRec.URNO );
            get_fld( pclDataArea, FIELD_2, STR, URNOLEN, rlGrpRec.FREL );
            get_fld( pclDataArea, FIELD_3, STR, TYPELEN, rlGrpRec.TYPE );

            /* add to the list */
            AddVarBuf( &rlGrpList, &rlGrpRec, sizeof( rlGrpRec ) );
            rlGrpList.numObjs++;

            slFuncCode = NEXT;

        } /* end if */

    } /* end while */

    close_my_cursor( &slLocalCursor );
    ilRc = RC_SUCCESS;


    /* loop through the group records (rlGrpList) creating two lists */
    /* 1. a list of profiles and 2. a list of groups already checked */
    prlGrpRec = ( GRPREC* ) rlGrpList.buf;
    dbg( DEBUG, "GetProfiles() Num GRP recs = %d", rlGrpList.numObjs );

    for( ilGrp = 0; ilGrp < rlGrpList.numObjs; ilGrp++ )
    {
        /* check STAT/VAFR/VATO/FREQ of the group or profile in SECTAB*/
        ilRc = CheckValidSecCal( NULL, NULL, prlGrpRec[ilGrp].TYPE, prlGrpRec[ilGrp].FREL, pclErrorMess );

        /* check VAFR/VATO/FREQ of the group itself */
        if( ilRc == RC_SUCCESS )
        {
            ilRc = CheckValidCal( prlGrpRec[ilGrp].URNO, pclErrorMess );
        }

        if( ilRc == RC_SUCCESS )
        {
            /* check the grouping type (either P or G) */
            if( !strcmp( prlGrpRec[ilGrp].TYPE, "P" ) )
            {
                /* Profile so add URNO of profile to the list */
                if( AddToList( &rgProfileList, prlGrpRec[ilGrp].FREL ) )
                {
                    dbg( DEBUG, "GetProfiles() Profile <%s> added to list.", prlGrpRec[ilGrp].FREL );
                }
            }
            else
            {
                /* Group - go deeper until 'P' found */
                if( AddToList( &rgAlreadyCheckedList, prlGrpRec[ilGrp].FREL ) )
                {
                    dbg( DEBUG, "GetProfiles() Group <%s> going deeper.", prlGrpRec[ilGrp].FREL );
                    GetProfiles( prlGrpRec[ilGrp].FREL );
                }
            }
        }

        ilRc = RC_SUCCESS;
    }

    FreeVarBuf( &rlGrpList );
    return ilRc;


} /* end GetProfiles() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    AddToList()                                             */
/*                                                                      */
/* Parameters:  prpList - list of URNOs.                                */
/*              pcpUrno - URNO to be added to prpList.                  */
/*                                                                      */
/* Returns: TRUE if pcpUrno is added to prpList.                        */
/*                                                                      */
/* Description: Add pcpUrno to prpList if not found.                    */
/*                                                                      */
/* ******************************************************************** */
BOOL AddToList( VARBUF *prpList, char *pcpUrno )
{
    URNOREC *prlList = ( URNOREC * ) prpList->buf;
    int ilNumRecs = prpList->numObjs;
    BOOL blNotFound = TRUE;
    int ilRec;

    /* check if URNO already exists in the list */
    for( ilRec = 0; blNotFound && ilRec < ilNumRecs; ilRec++ )
        if( !strcmp( prlList[ilRec].URNO, pcpUrno ) )
            blNotFound = FALSE;

    /* if URNO not found in the list then add it */
    if( blNotFound )
    {
        URNOREC rlNewUrno;
        memset( &rlNewUrno, 0, sizeof( rlNewUrno ) );
        strcpy( rlNewUrno.URNO, pcpUrno );
        AddVarBuf( prpList, &rlNewUrno, sizeof( rlNewUrno ) );
        prpList->numObjs++;
    }

    return blNotFound; /* TRUE if added to the list */

} /* end AddToList() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    SetPrivStat()                                           */
/*                                                                      */
/* Parameters:  prpFktList - list of records read from FKTTAB.          */
/*              pcpFfkt - URNO in FKTTAB.                               */
/*              pcpFprv - URNO in PRVTAB.                               */
/*              pcpStat - STAT to be set in the list.                   */
/*              bpSetStat - if TRUE then copy pcpStat else              */
/*                          set the STAT only if higher priority        */
/*                          eg HIDDEN < DISABLED < ENABLED.             */
/*                                                                      */
/* Returns: RC_FAIL - URNO not found else RC_SUCCESS.                   */
/*                                                                      */
/* Description: Search in prpFktList if found then set STAT.            */
/*                                                                      */
/* ******************************************************************** */
static int SetPrivStat( VARBUF *prpFktList, char *pcpFfkt, char *pcpFprv, char *pcpStat, BOOL bpSetStat )
{
    BOOL blNotFound = TRUE;
    int ilLoop;
    char clNewStat[STATLEN + 1];
    FKTREC *prlFktRec = ( FKTREC * ) prpFktList->buf;
    int ilNumFktRecs = prpFktList->numObjs;

    for( ilLoop = 0; ilLoop < ilNumFktRecs; ilLoop++ )
    {
        if( !strcmp( prlFktRec[ilLoop].URNO, pcpFfkt ) )
        {
            if( bpSetStat )
            {
                /* copy the status and the profile URNO */
                strcpy( prlFktRec[ilLoop].STAT, pcpStat );
                strcpy( prlFktRec[ilLoop].FPRV, pcpFprv );
            }
            else
            {
                /* check if profile STAT > FKT map STAT */
                strcpy( clNewStat, HIGHEST_STAT( prlFktRec[ilLoop].STAT, pcpStat ) );

                /*dbg(DEBUG,"CurrStat %s NewStat %s Setting to: Stat %s FPRV %s",prlFktRec[ilLoop].STAT,pcpStat,clNewStat,pcpFprv);*/
                /* if STAT has changed or FPRV was blank, update */
                if( strcmp( prlFktRec[ilLoop].STAT, clNewStat ) || strlen( prlFktRec[ilLoop].FPRV ) <= 0 )
                {
                    strcpy( prlFktRec[ilLoop].STAT, clNewStat );
                    strcpy( prlFktRec[ilLoop].FPRV, pcpFprv );
                }
            }

            blNotFound = FALSE;
            break;
        }
    }

    return blNotFound ? RC_FAIL : RC_SUCCESS;

} /* end SetPrivStat() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    SetWksPrivStat()                                           */
/*                                                                      */
/* Parameters:  prpFktList - list of records read from FKTTAB.          */
/*              pcpFfkt - URNO in FKTTAB.                               */
/*              pcpFprv - URNO in PRVTAB.                               */
/*              pcpStat - STAT to be set in the list.                   */
/*              bpSetStat - if TRUE then copy pcpStat else              */
/*                          set the STAT only if higher priority        */
/*                          eg HIDDEN < DISABLED < ENABLED.             */
/*                                                                      */
/* Returns: RC_FAIL - URNO not found else RC_SUCCESS.                   */
/*                                                                      */
/* Description: Search in prpFktList if found then set STAT.            */
/*                                                                      */
/* ******************************************************************** */
static int SetWksPrivStat( VARBUF *prpFktList, char *pcpFfkt, char *pcpFprv, char *pcpStat, BOOL bpSetStat )
{
    BOOL   blNotFound = TRUE;
    int    ilLoop;
    char   clNewStat[STATLEN + 1];
    FKTREC *prlFktRec = ( FKTREC * ) prpFktList->buf;
    int ilNumFktRecs = prpFktList->numObjs;

    for( ilLoop = 0; ilLoop < ilNumFktRecs; ilLoop++ )
    {
        if( !strcmp( prlFktRec[ilLoop].URNO, pcpFfkt ) )
        {
            if( bpSetStat )
            {
                /* copy the status and the profile URNO */
                strcpy( prlFktRec[ilLoop].STAT, pcpStat );
                strcpy( prlFktRec[ilLoop].FPRV, pcpFprv );
            }
            else
            {
                /* check if profile STAT > FKT map STAT */
                strcpy( clNewStat, WKS_STAT( prlFktRec[ilLoop].STAT, pcpStat ) );

                /*dbg(DEBUG,"CurrStat %s NewStat %s Setting to: Stat %s FPRV %s",prlFktRec[ilLoop].STAT,pcpStat,clNewStat,pcpFprv);*/
                /* if STAT has changed or FPRV was blank, update */
                if( strcmp( prlFktRec[ilLoop].STAT, clNewStat ) || strlen( prlFktRec[ilLoop].FPRV ) <= 0 )
                {
                    strcpy( prlFktRec[ilLoop].STAT, clNewStat );
                    strcpy( prlFktRec[ilLoop].FPRV, pcpFprv );
                }
            }

            blNotFound = FALSE;
            break;
        }
    }

    return blNotFound ? RC_FAIL : RC_SUCCESS;

} /* end SetWksPrivStat() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckForPasswordExpiry()                                */
/*                                                                      */
/* Parameters:  pcpFsec - URno from SECTAB                              */
/*              pcpPass - encrypted password from SECTAB                */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*              Password for expiry every regular inteval stated in     */
/*              the authdl.cfg file                                     */
/*                                                                      */
/* ******************************************************************** */

static int CheckForPasswordExpiry( char *pcpUsid, char *pcpFsec, char *pcpPass, char *pcpErrorMess )
{
    BOOL blExpireCheck;
    int ilRc = RC_SUCCESS;
    int ilCurrDays = 0;
    int ilCdatDays = 0;
    int ilDummy = 0;
    short slLocalCursor;
    short slFuncCode;
    char clCdat[DATELEN + 1];
    char clPass[PASSLEN + 1];
    char clOraErr[250];
    char pclSqlBuf[2048];
    char pclDataArea[2048];
    char *pclFunc = "CheckForPasswordExpiry";

    if( ( igPasswordExpiryDays > 0 ) && ( bgPwdtabExists == TRUE ) )
    {
#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "SELECT CDAT,PASS FROM %s WHERE FSEC=:VUrno ORDER BY CDAT DESC", pcgPwdtab );
#else
        sprintf(pclSqlBuf, "SELECT CDAT,PASS FROM %s WHERE FSEC=? ORDER BY CDAT DESC", pcgPwdtab);
#endif
        CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
        sprintf( pclDataArea, "%s\n", pcpFsec );
        dbg( DEBUG, "<%s> Selecting last password for FSEC <%s>...\n<%s>", pclFunc, pcpFsec, pclSqlBuf );

        slLocalCursor = 0;
        nlton( pclDataArea );
        slFuncCode = START;
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc != DB_SUCCESS && ilRc != ORA_NOT_FOUND )
        {
            get_ora_err( ilRc, clOraErr );
            sprintf( pcpErrorMess, "<%s> Error selecting PWD record for SEC.URNO <%s>\n%s", pclFunc, pcpFsec, clOraErr );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        } /* end if */

        close_my_cursor( &slLocalCursor );

        if( ilRc == DB_SUCCESS )
        {
            ilRc = RC_SUCCESS;
            memset( clCdat, 0, DATELEN + 1 );
            memset( clPass, 0, PASSLEN + 1 );
            get_fld( pclDataArea, FIELD_1, STR, DATELEN, clCdat );
            get_fld( pclDataArea, FIELD_2, STR, PASSLEN, clPass );
            dbg( DEBUG, "<%s> Read PWDTAB FSEC <%s> CDAT <%s> PASS <%s>", pclFunc, pcpFsec, clCdat, clPass );

            if( strcmp( clPass, pcpPass ) == 0 )
            {
                /* Mei */
                blExpireCheck = TRUE;

                if( bgDbConfig == TRUE )
                {
                    if( !strcmp( pcpUsid, UFISADMIN ) && !strncmp( rgParam[IDX_SYSADM_PWD_EX].VALU, "NO", 2 ) )
                    {
                        dbg( TRACE, "<%s> ADMIN staff, password will not be expired!", pclFunc );
                        blExpireCheck = FALSE;
                    }
                }

                if( blExpireCheck == TRUE )
                {
                    ( void ) GetFullDay( pcgCurrDate, &ilCurrDays, &ilDummy, &ilDummy );
                    ( void ) GetFullDay( clCdat, &ilCdatDays, &ilDummy, &ilDummy );
                    igPasswordUsedDays = ilCurrDays - ilCdatDays;
                    dbg( TRACE, "<%s> Passwd has been created for %d days", pclFunc, igPasswordUsedDays );

                    if( igPasswordUsedDays >= igPasswordExpiryDays )
                    {
                        dbg( DEBUG, "<%s> Password has to be renewed.", pclFunc );
                        sprintf( pcpErrorMess, "%s MUST_CHANGE_PASSWORD_FIRST (PASSWORD EXPIRED)", LOGINERROR );
                        PushError( &rgErrList, pcpErrorMess );
                        ilRc = RC_FAIL;
                    }
                }
            }
            else
            {
                dbg( DEBUG, "<%s> Different passwords in SECTAB and PWDTAB.", pclFunc );
                sprintf( pcpErrorMess, "%s INCONSISTENT_PASSWORD", LOGINERROR );
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
            }
        }
        else
        {
            if( ilRc == ORA_NOT_FOUND )
                ilRc = RC_SUCCESS;
        }
    }

    return ilRc;

} /* end CheckForPasswordExpiry() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckValidSecCal()                                      */
/*                                                                      */
/* Parameters:  pcpUsid - user ID. If NULL then pcpUrno is used.        */
/*              pcpPass - password, required for TYPE="U" only.         */
/*              pcpType - TYPE in SECTAB.                               */
/*              pcpUrno - returns the URNO of the record in SECTAB.     */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              1. If USID and TYPE exist in SECTAB.                    */
/*              2. SECTAB.STAT                                          */
/*              3. CALTAB VAFR/VATO/FREQ are valid for currentDate.     */
/*              4. Returns the SECTAB.URNO in pcpUrno.                  */
/*                                                                      */
/*              Ceda errors are returned in pcpErrorMess.               */
/*              In addition the following errors can be returned:       */
/*              "INVALID_USER" --> user not found in SECTAB             */
/*              "INVALID_APPLICATION" -> ::::::::::::::::::             */
/*              "INVALID_WORKSTATION" -> ::::::::::::::::::             */
/*              "INVALID_PASSWORD" --> Wrong password for a user.       */
/*              "DISABLED_USER" --> SECTAB.STAT="0"                     */
/*              "DISABLED_APPLICATION" --> ::::::::::::::::             */
/*              "EXPIRED_USER" --> user valid dates invalid.            */
/*              "EXPIRED_APPLICATION" --> :::::::::::::::::             */
/*              "EXPIRED_WORKSTATION" --> :::::::::::::::::             */
/*                                                                      */
/* ******************************************************************** */
static int CheckValidSecCal( char *pcpUsid, char *pcpPass, char *pcpType, char *pcpUrno, char *pcpErrorMess )
{
    int ilRc = RC_SUCCESS;
    short slLocalCursor;
    short slFuncCode;
    char clStat[STATLEN + 1]; /* status of the user */
    char clOraErr[250];
    char clValidPass[PASSLEN + 1]; /* password read from DB */
    char clTypeText[BIGBUF];
    char pclSqlBuf[2048], pclDataArea[2048];
    char *pclFunc = "CheckValidSecCal";
    BOOL blIsSSOCmd = FALSE;

    if( strcmp( pcpType, "U" ) == 0 && pcpPass == NULL )
    {
        dbg( DEBUG, "line<%d> SSO command, skip password check", __LINE__ );
        dbg( DEBUG, "line<%d> pcpType=U && pcpPass=NULL", __LINE__ );
        blIsSSOCmd = TRUE;
    }

    if( !strcmp( pcpType, "U" ) )
        strcpy( clTypeText, "USER" );
    else if( !strcmp( pcpType, "A" ) )
        strcpy( clTypeText, "APPLICATION" );
    else if( !strcmp( pcpType, "W" ) )
        strcpy( clTypeText, "WORKSTATION" );
    else if( !strcmp( pcpType, "P" ) )
        strcpy( clTypeText, "PROFILE" );
    else if( !strcmp( pcpType, "G" ) )
        strcpy( clTypeText, "GROUP" );
    else if( !strcmp( pcpType, "S" ) )
        strcpy( clTypeText, "WKSGROUP" );
    else
    {
        PushError( &rgErrList, "CheckValidSecCal() Invalid pcpType." );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS )
    {
        /* Select password and status from SECTAB.*/

        if( pcpUsid == NULL )
        {
#ifdef CCS_DBMODE_EMBEDDED
            sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
            sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE URNO=?", pcgSectab);
#endif
            sprintf( pclDataArea, "%s\n", pcpUrno );
        }
        else
        {
            if( ( bgSSOUsidCaseInsensitive == TRUE ) && ( blIsSSOCmd == TRUE ) )
            {
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE UPPER(USID)=:VUser AND TYPE=:VType", pcgSectab );
#else
                sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE UPPER(USID)=? AND TYPE=?", pcgSectab);
#endif
            }
            else
            {
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE USID=:VUser AND TYPE=:VType", pcgSectab );
#else
                sprintf(pclSqlBuf, "SELECT PASS,STAT,URNO FROM %s WHERE USID=? AND TYPE=?", pcgSectab);
#endif
            }

            sprintf( pclDataArea, "%s\n%s\n", pcpUsid, pcpType );
        }

        dbg( DEBUG, "CheckValidSecCal() Selecting pcpUsid=<%s>,pcpType=<%s>\n", pcpUsid, pcpType );
        CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
        dbg( DEBUG, "CheckValidSecCal() Selecting %s data...\n<%s>", clTypeText, pclSqlBuf );

        slLocalCursor = 0;
        nlton( pclDataArea );
        slFuncCode = START;

        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc != DB_SUCCESS )
        {
            if( ilRc != ORA_NOT_FOUND )
            {
                get_ora_err( ilRc, clOraErr );
                sprintf( pcpErrorMess, "Error selecting %s record for <%s>\n%s", clTypeText, pcpUsid, clOraErr );
                ilRc = RC_FAIL;
            }
            else
            {
                if( strcmp( pcpType, "W" ) )
                {
                    sprintf( pcpErrorMess, "%s INVALID_%s", LOGINERROR, clTypeText );
                    ilRc = RC_FAIL;
                }
                else
                    ilRc = RC_NO_WORKSTATION;

            }

            if( ilRc == RC_FAIL )
                PushError( &rgErrList, pcpErrorMess );

        } /* end if */

    } /* end if( ilRc == RC_SUCCESS ) */

    if( ilRc == RC_SUCCESS )
    {
        memset( clValidPass, 0, PASSLEN + 1 );
        memset( clStat, 0, STATLEN + 1 );
        memset( pcpUrno, 0, URNOLEN + 1 );

        get_fld( pclDataArea, FIELD_1, STR, PASSLEN, clValidPass );
        get_fld( pclDataArea, FIELD_2, STR, STATLEN, clStat );
        get_fld( pclDataArea, FIELD_3, STR, URNOLEN, pcpUrno );

        CT_TrimRight(clValidPass, ' ', -1);

        /* for users check the password */
        if( pcpPass != NULL )
        {
            char clEncryptedPass[PASSLEN + 1];

            memset( clEncryptedPass, 0, sizeof( clEncryptedPass ) );

            if( strlen( pcpPass ) <= 0 )
                strcpy( pcpPass, " " );

            /* Check encryption method */
            if((egEncBy == E_ENCBY_CLIENT) || ((egEncBy != E_ENCBY_INVALID) && (egEncMethod == E_ENCMETHOD_NONE)))
            {
                /* Client already encrypted the password, just use it */
                strncpy(clEncryptedPass, pcpPass, PASSLEN);
            }
            else
            {
                /* Client sent plain text password */
                string_to_key( pcpPass, clEncryptedPass );
            }

            dbg( DEBUG, "SEC PASS <%s> PASS from user <%s>", clValidPass, clEncryptedPass );
            /*
            dbg(TRACE,"PASS FROM SECTAB");
            snap(clValidPass,PASSLEN+1,outp);
            dbg(TRACE," ");
            dbg(TRACE,"PASS FROM CLIENT <%s>",pcpPass);
            snap(clEncryptedPass,PASSLEN+1,outp);
             */

            /* MEI v1.21 */
            ilRc = CheckForPasswordExpiry( pcpUsid, pcpUrno, clValidPass, pcpErrorMess );

            if( ilRc == RC_SUCCESS )
            {
                if( strcmp( clValidPass, clEncryptedPass ) )
                {
                    sprintf( pcpErrorMess, "%s INVALID_PASSWORD", LOGINERROR );
                    PushError( &rgErrList, pcpErrorMess );
                    ilRc = RC_FAIL;
                }
            }
            else
                dbg( TRACE, "CheckValidUser() Password expired !" );
        }

    } /* end if( ilRc == RC_SUCCESS ) */


    if( ilRc == RC_SUCCESS )
    {
        /* check STAT: 1 = enabled  0 = disabled */
        dbg( DEBUG, "CheckValidUser() STAT=<%s>", clStat );

        if( strcmp( clStat, "1" ) )
        {
            sprintf( pcpErrorMess, "%s DISABLED_%s", LOGINERROR, clTypeText );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
            dbg( DEBUG, "CheckValidSecCal() The user STATUS flag = activated!" );

    } /* end if( ilRc == RC_SUCCESS ) */

    if( ilRc == RC_SUCCESS )
    {
        ilRc = CheckValidCal( pcpUrno, pcpErrorMess );

        if( ilRc == RC_SUCCESS )
            dbg( DEBUG, "CheckValidSecCal() VAFR/VATO are valid!" );
        else if( ilRc == ORA_NOT_FOUND )
        {
            /*  means that the dates have expired */
            sprintf( pcpErrorMess, "%s EXPIRED_%s", LOGINERROR, clTypeText );
            PushError( &rgErrList, pcpErrorMess );
        }

    } /* end if( ilRc == RC_SUCCESS ) */

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end CheckValidSecCal() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    UpdatePasswordTable()                                   */
/*                                                                      */
/* Parameters:  pcpFsec - URNO of user in SECTAB                        */
/*              pcpPass - the new password                              */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              Inserts a new password into PWDTAB - if there are       */
/*              already more than n passwords for the user then         */
/*              delete the excess passwords                             */
/*                                                                      */
/*              In addition the following errors can be returned:       */
/*              "CHANGE_PASSWORD"                                       */
/*                                                                      */
/* ******************************************************************** */
static int UpdatePasswordTable( char *pcpFsec, char *pcpPass )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor;
    char        clOraErr[250];
    char        pclSqlBuf[2048],pclDataArea[2048];
    PWDREC      rlPwdRec;
    char        clUrno[URNOLEN+1];
    int         ilCount = 0;
    VARBUF      rlUrnoList;
    short       slFuncCode;
    char        pclUrnosToDelete[BIGBUF];


    /* insert the new password in PWDTAB - record of previous passwords */
    memset( &rlPwdRec, 0, sizeof( PWDREC ) );

    if( LoadUrnoList( 1 ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "UpdatePasswordTable() Error loading URNOs." );
        ilRc = RC_FAIL;
    }

    if( GetNextUrno( rlPwdRec.URNO ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "UpdatePasswordTable() Error getting new URNO" );
        return RC_FAIL;
    }

    strcpy( rlPwdRec.FSEC, pcpFsec );
    strcpy( rlPwdRec.PASS, pcpPass );

    if( InsertPwdRec( &rlPwdRec ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "UpdatePasswordTable() Error inserting into PWDTAB" );
        return RC_FAIL;
    }

    dbg( DEBUG, "UpdatePasswordTable() URNO=<%s> FSEC=<%s> PASS=<********> CDAT=<%s>", rlPwdRec.URNO, rlPwdRec.FSEC, rlPwdRec.CDAT );


    /* delete all but the last n entries in PWDTAB for the user */
#ifdef CCS_DBMODE_EMBEDDED
    sprintf(pclSqlBuf, "SELECT URNO FROM %s WHERE FSEC=:VUrno ORDER BY CDAT DESC", pcgPwdtab );
#else
    sprintf(pclSqlBuf, "SELECT URNO FROM %s WHERE FSEC=? ORDER BY CDAT DESC", pcgPwdtab);
#endif

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    sprintf( pclDataArea, "%s\n", pcpFsec );
    dbg( DEBUG, "UpdatePasswordTable() Selecting pervious passwords...\n<%s>", pclSqlBuf );
    nlton( pclDataArea );


    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;
    InitVarBuf( &rlUrnoList );

    /* loop - read and process a line at a time */
    memset( pclUrnosToDelete, 0, sizeof( pclUrnosToDelete ) );

    while( ilRc == DB_SUCCESS )
    {
        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {
            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clUrno );
            ilCount++;

            if( ilCount > igLastPasswordCount )
            {
                int ilLen = strlen( pclUrnosToDelete );

                if( ilLen < ( BIGBUF - 12 ) )
                {
                    if( ilLen > 0 ) strcat( pclUrnosToDelete, "," );

                    strcat( pclUrnosToDelete, clUrno );
                }

            }
        }

        slFuncCode = NEXT;

    } /* while */

    if( strlen( pclUrnosToDelete ) > 0 )
    {
        char clWhere[BIGBUF];
        sprintf( clWhere, "URNO IN (%s)", pclUrnosToDelete );
        ilRc = handleDelete( pcgPwdtab, clWhere, NULL, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            char pclErrorMess[BIGBUF];
            sprintf( pclErrorMess, "Error deleting from %s.", pcgPwdtab );
            PushError( &rgErrList, pclErrorMess );
            ilRc = RC_FAIL;
        }
    }


    FreeVarBuf( &rlUrnoList );
    close_my_cursor( &slLocalCursor );
    ilRc = RC_SUCCESS;
    return ilRc;

} /* end UpdatePasswordTable() */



/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckForUnusedPassword()                                */
/*                                                                      */
/* Parameters:  pcpFsec - URNO of user in SECTAB                        */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              Checks that the user has changed the password before it */
/*              is used the first time. If no records are found in      */
/*                                                                      */
/*              In addition the following errors can be returned:       */
/*              "CHANGE_PASSWORD"                                       */
/*                                                                      */
/* ******************************************************************** */
static int CheckForUnusedPassword( char *pcpFsec, char *pcpErrorMess )
{

    int         ilRc = RC_SUCCESS;
    short       slLocalCursor;
    char        clOraErr[250];
    char        pclSqlBuf[2048],pclDataArea[2048];
    short       slFuncCode;

#ifdef CCS_DBMODE_EMBEDDED
    sprintf(pclSqlBuf, "SELECT PASS FROM %s WHERE FSEC=:VUrno", pcgPwdtab );
#else
    sprintf(pclSqlBuf, "SELECT PASS FROM %s WHERE FSEC=?", pcgPwdtab);
#endif

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    sprintf( pclDataArea, "%s\n", pcpFsec );
    dbg( DEBUG, "CheckForUnusedPassword() Selecting previous passwords for FSEC <%s>...\n<%s>", pcpFsec, pclSqlBuf );

    slLocalCursor = 0;
    nlton( pclDataArea );
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS && ilRc != ORA_NOT_FOUND )
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess,
                 "Error selecting record for SEC.URNO <%s>\n%s",
                 pcpFsec, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;

    } /* end if */

    if( ilRc == ORA_NOT_FOUND )
    {
        dbg( DEBUG, "CheckForUnusedPassword() Password needs to be changed before use." );
        sprintf( pcpErrorMess, "%s MUST_CHANGE_PASSWORD_FIRST", LOGINERROR );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end CheckForUnusedPassword() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckPasswordNotAlreadyUsed()                           */
/*                                                                      */
/* Parameters:  pcpFsec - URNO of user in SECTAB                        */
/*              pcpPass - New Password                                  */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              Password cannot be set if it was one of the employee's  */
/*              most recently used passwords (held in PWDTAB)           */
/*                                                                      */
/* ******************************************************************** */
static int CheckPasswordNotAlreadyUsed( char *pcpFsec, char *pcpPass, char *pcpErrorMess )
{

    int         ilRc = RC_SUCCESS;
    char        clLang[LANGLEN+1];
    short       slLocalCursor;
    short       slFuncCode;
    char        clOraErr[250];
    char        pclSqlBuf[2048],pclDataArea[2048];

#ifdef CCS_DBMODE_EMBEDDED
    sprintf(pclSqlBuf, "SELECT PASS FROM %s WHERE FSEC=:VUrno AND PASS=:VPass", pcgPwdtab );
#else
    sprintf(pclSqlBuf, "SELECT PASS FROM %s WHERE FSEC=? AND PASS=?", pcgPwdtab);
#endif
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    sprintf( pclDataArea, "%s\n%s\n", pcpFsec, pcpPass );
    dbg( DEBUG, "CheckPasswordNotAlreadyUsed() Selecting pervious passwords...\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    nlton( pclDataArea );
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS && ilRc != ORA_NOT_FOUND )
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess,
                 "Error selecting record from <%s> for FSEC <%s> and PASS <%s>\n%s",
                 pcgPwdtab, pcpFsec, pcpPass, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;

    } /* end if */

    if( ilRc == ORA_NOT_FOUND )
    {
        dbg( DEBUG, "CheckPasswordNotAlreadyUsed() Password has not been used before." );
        ilRc = RC_SUCCESS;
    }
    else
    {
        dbg( DEBUG, "CheckPasswordNotAlreadyUsed() Password needs to be changed before use." );
        sprintf( pcpErrorMess, "%s PASSWORD_ALREADY_USED", LOGINERROR );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end CheckPasswordNotAlreadyUsed() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckValidLogin()                                       */
/*                                                                      */
/* Parameters:  pcpUrno - URNO of user in SECTAB                        */
/*              bpIsValidLogin - TRUE if user has logged in sucessfully */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              if bpIsValidLogin is TRUE - sucessful login so set      */
/*              login count to zero.                                    */
/*              if bpIsValidLogin is FALSE - increment counter of       */
/*              unsucessful logins - if limit reached then deactivate   */
/*              the user and add "*" to pcpErrorMessage to indicate     */
/*              to calling environment that the user was deactivated.   */
/*                                                                      */
/* ******************************************************************** */
static int CheckValidLogin( char *pcpUrno, BOOL bpIsValidLogin, BOOL bpIsAdmin, BOOL bpIncDay, char *pcpErrorMess )
{
    time_t tlPLogin, tlCLogin;
    time_t tlDiff;
    int ilRc = RC_SUCCESS;
    short slLocalCursor;
    short slFuncCode;
    char clLang[LANGLEN + 1];
    char clStat[STATLEN + 1];
    char clLogd[DATELEN + 1]; /* Mei */
    char pclTimeStr[30];
    char clOraErr[250];
    char pclSqlBuf[2048], pclDataArea[2048];
    char *pclFunc = "CheckValidLogin";

    dbg( DEBUG, "<%s> bpIsValidLogin <%d> igNumLoginAttemptsAllowed <%d>", pclFunc, bpIsValidLogin, igNumLoginAttemptsAllowed );
    dbg( DEBUG, "<%s> pcpUrno <%s> pcpErrorMess <%s>", pclFunc, pcpUrno, pcpErrorMess );

    /* Mei: Check period of login */
    /* only deactivate users whose password was wrong */
    /* if igNumLoginAttemptsAllowed is -1 then checking is deactivated */
    /*if(strlen(pcpUrno) > 0 && igNumLoginAttemptsAllowed != -1 && (bpIsValidLogin || !strcmp(pcpErrorMess,"LOGINERROR INVALID_PASSWORD")))*/
    if( strlen( pcpUrno ) > 0 && ( bpIsValidLogin || !strcmp( pcpErrorMess, "LOGINERROR INVALID_PASSWORD" ) ) )
    {
        if( bgDbConfig == TRUE )
        {
#ifdef CCS_DBMODE_EMBEDDED
            sprintf(pclSqlBuf, "SELECT LANG,STAT,LOGD FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
            sprintf(pclSqlBuf, "SELECT LANG,STAT,LOGD FROM %s WHERE URNO=?", pcgSectab);
#endif
        }
        else
        {
#ifdef CCS_DBMODE_EMBEDDED
            sprintf(pclSqlBuf, "SELECT LANG,STAT FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
            sprintf(pclSqlBuf, "SELECT LANG,STAT FROM %s WHERE URNO=?", pcgSectab);
#endif

        }

        CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
        sprintf(pclDataArea, "%s\n", pcpUrno);

        slLocalCursor = 0;
        nlton( pclDataArea );
        slFuncCode = START;

        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );
        close_my_cursor( &slLocalCursor );

        if( ilRc != DB_SUCCESS )
        {
            get_ora_err( ilRc, clOraErr );
            sprintf( pcpErrorMess, "Error selecting record for SEC.URNO <%s>\n%s", pcpUrno, clOraErr );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
            return ilRc;
        }

        if( ilRc == RC_SUCCESS )
        {
            memset( clLang, 0, LANGLEN + 1 );
            memset( clStat, 0, STATLEN + 1 );
            memset( clLogd, 0, DATELEN + 1 );
            get_fld( pclDataArea, FIELD_1, STR, LANGLEN, clLang );
            get_fld( pclDataArea, FIELD_2, STR, STATLEN, clStat );

            if( bgDbConfig == TRUE )
                get_fld( pclDataArea, FIELD_3, STR, DATELEN, clLogd );

            dbg( DEBUG, "CheckValidLogin() Read SECTAB URNO <%s> LANG <%s> STAT <%s> LOGD <%s>", pcpUrno, clLang, clStat, clLogd );
        }
    }
    else
        return RC_SUCCESS;

    if( igNumLoginAttemptsAllowed != -1 )
    {
        if( !strcmp( clStat, "1" ) && bpIsAdmin != TRUE )
        {
            int ilNumLoginAttempts = 0;
            VARBUF rlUrnoList;
            char pclDataList[BIGBUF];

            if( bpIsValidLogin )
            {
                ilNumLoginAttempts = 0;
                dbg( DEBUG, "CheckValidLogin() valid login - setting counter back to zero" );
            }
            else
            {
                ilNumLoginAttempts = atoi( &clLang[1] ) + 1;
                dbg( DEBUG, "CheckValidLogin() Num login attempts so far %d. Max Allowed attempts %d", ilNumLoginAttempts, igNumLoginAttemptsAllowed );

                if( ilNumLoginAttempts >= igNumLoginAttemptsAllowed )
                {
                    /* deactivate the user */
                    strcpy( clStat, "0" );
                    strcpy( pcpErrorMess, "LOGINERROR INVALID_PASSWORD DEACTIVATE_USER" );
                    ilNumLoginAttempts = 0;
                    dbg( DEBUG, "CheckValidLogin() User exceeded max allowed login attempts and will be deactivated.", pcpUrno, clLang, clStat );

                    /*Frank@UAL_UAA*/
                    if( bgUpdateUALtab == TRUE )
                    {
                        InsertUalTab( pcpUrno, pcpErrorMess, TRUE, DEACTIVE, "" );
                    }

                    /*InsertUalTab(char *pcpUrno,char *pcpErrorMess, BOOL bpIsSystem, BOOL bpIsActived); */
                    /*Frank@UAL_UAA*/
                }
            }

            /* init the list of urnos returned from handleUpdate() */
            InitVarBuf( &rlUrnoList );
            sprintf( &clLang[1], "%d\0", ilNumLoginAttempts );
            sprintf( pclDataList, "%s,%s", clLang, clStat );
            dbg( DEBUG, "CheckValidLogin() Update SECTAB URNO <%s> LANG <%s> STAT <%s>", pcpUrno, clLang, clStat );

            if( handleUpdate( pcgSectab, "LANG,STAT", pclDataList, NULL, pcpUrno, &rlUrnoList ) != RC_SUCCESS )
            {
                strcpy( pcpErrorMess, "Error updating SECTAB." );
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
                return ilRc;
            }
            else
            {
                dbg( TRACE, "CheckValidLogin() Success, committing changes." );
                handleCommit();
            }

            FreeVarBuf(&rlUrnoList);
        }
    }

    /* Mei: Check how many days from previous login */
    if( !bpIsValidLogin || !strcmp( pcpErrorMess, "LOGINERROR INVALID_PASSWORD" ) )
        return ilRc;

    if( bpIncDay == TRUE && bgDbConfig == TRUE )
    {
        dbg( TRACE, "CheckValidLogin() Check for overdue login LOGD <%s>", clLogd );
        CurrentUTCTime( pclTimeStr );

        if( strlen( clLogd ) > 0 )
        {
            tlPLogin = TimeConvert_sti( clLogd );
            tlCLogin = TimeConvert_sti( pclTimeStr );
            tlDiff = ( tlCLogin - tlPLogin ) / SECONDS_PER_DAY;
            dbg( TRACE, "CheckValidLogin() #day of login = %d", tlDiff );

            if( tlDiff >= rgParam[IDX_USER_DEACT].iValue )
            {
                dbg( TRACE, "CheckValidLogin() password is OVERDUE" );
                strcpy( pcpErrorMess, "LOGINERROR OVERDUE LOGIN" );
                PushError( &rgErrList, pcpErrorMess );

                sprintf( pclSqlBuf, "SELECT URNO FROM %s WHERE FSEC = '%s'", pcgCaltab, pcpUrno );
                ilRc = ExecSqlQuery( pclSqlBuf, pclDataArea );

                if( ilRc == DB_SUCCESS )
                {
                    VARBUF rlUrnoList;

                    InitVarBuf( &rlUrnoList );
                    handleUpdate( pcgCaltab, "VATO", pclTimeStr, NULL, pclDataArea, &rlUrnoList );
                    FreeVarBuf(&rlUrnoList);
                }

                ilRc = RC_FAIL;
            }
        }
    }

    SetLoginTime( pcpUrno );

    return ilRc;
} /* end CheckValidLogin() */



/*Frank@UAL_UAA*/
/*Frank@UAM*/
/*static int InsertUalTab(char *pcpUrno,char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived)  */

static int InsertUalTab( char *pcpUrno, char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived, char *pcpRema )
{
    UALREC rlUalRec;
    int    ilRc = RC_SUCCESS;
    char   pclSqlBuf[2048], pclDataArea[2048];
    short  slLocalCursor;
    char   clUsid[USIDLEN + 1]; /* user ID received in user data */
    char   clEvct[EVCTLEN + 1];
    char   clUrno[URNOLEN + 1];
    char   clEvdeTemp[EVDELEN + 1];
    int    ilEvct = 0;
    short  slFuncCode;
    char   clWhere[BIGBUF];
    char   clOraErr[250];

    /* Frank v1.29 */
    int ilNextUrno = 0;
    char buffer[33] = "\0";
    /* Frank v1.29 */

    memset( &rlUalRec, 0, sizeof( rlUalRec ) );
    memset( clUsid, 0, USIDLEN + 1 );

    dbg( DEBUG, "UALTAB is initialized" );

    /*Frank v1.28*/
    /*
    if( LoadUrnoList(1) != RC_SUCCESS)
    {
        PushError(&rgErrList,"UpdateUserAccountLogTable() Error loading URNOs.");
        ilRc = RC_FAIL;
    }
    if( GetNextUrno(rlUalRec.URNO) != RC_SUCCESS )
    {
        PushError(&rgErrList,"UpdateUserAccountLogTable() Error getting new URNO");
        return RC_FAIL;
    }
    */

    /*Frank v1.29*/
    ilNextUrno = NewUrnos( "UALTAB", 1 );

    if( ilNextUrno > 0 )
    {
        if( itoa( ilNextUrno, buffer, 10 ) == NULL )
        {
            dbg( DEBUG, "line<%d>Error getting new URNO", __LINE__ );
            return RC_FAIL;
        }
        else
        {
            strncpy( rlUalRec.URNO, buffer, strlen( buffer ) );
        }
    }
    else
    {
        dbg( DEBUG, "line<%d>Error getting new URNO", __LINE__ );
        return RC_FAIL;
    }/*Frank v1.29*/

    dbg( DEBUG, "the URNO for UALTAB is <%s>", rlUalRec.URNO );

#ifdef CCS_DBMODE_EMBEDDED
    sprintf(pclSqlBuf, "SELECT USID FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
    sprintf(pclSqlBuf, "SELECT USID FROM %s WHERE URNO=?", pcgSectab);
#endif
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    sprintf( pclDataArea, "%s\n", pcpUrno );

    dbg( DEBUG, "UpdateUserAccountLogTable() Selecting the USID from SECTAB using given URNO<%s>", pclSqlBuf );
    nlton( pclDataArea );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc == DB_SUCCESS )
    {
        /* process the fields in the line just read */
        get_fld( pclDataArea, FIELD_1, STR, USIDLEN, clUsid );
        dbg( DEBUG, "the usid is <%s>", clUsid );
    }
    else
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess, "Error selecting record for SEC.URNO <%s>\n%s", pcpUrno, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
        return ilRc;
    }

    close_my_cursor( &slLocalCursor );

    strcpy( rlUalRec.EVCT, "1" );
    strcpy( rlUalRec.USID, clUsid );
    strcpy( rlUalRec.CDAT, pcgCurrDate );

    if( bpIsSystem == TRUE )
    {
        strcpy( rlUalRec.USEC, "authdl" );
        strcpy( rlUalRec.RMRK, "wrong password" );
        /*strcpy(rlUalRec.EVDE,"Deactivate"); */
        strcpy( rlUalRec.EVDE, "DEACT" );
    }
    else
    {
        /* strcpy(rlUalRec.USEC,pcpUrno); */
        strcpy( rlUalRec.USEC, pcgDestName );

        /*Frank@UAM*/
        if( strlen( pcpRema ) != 0 )
        {
            strcpy( rlUalRec.RMRK, pcpRema );
        }/*Frank@UAM*/
        else
        {
            strcpy( rlUalRec.RMRK, "administrator operation" );
        }

        if( ipIsActived == FALSE )
        {
            /* strcpy(rlUalRec.EVDE,"Deactivate"); */
            strcpy( rlUalRec.EVDE, "DEACT" );
        }
        else if( ipIsActived == TRUE )
        {
            /*strcpy(rlUalRec.EVDE,"Activate"); */
            strcpy( rlUalRec.EVDE, "ACT" );
        }
        else if( ipIsActived == RESETPASSWORD )
        {
            strcpy( rlUalRec.EVDE, "RESET" );
        }
    }

    dbg( DEBUG, "UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
         rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;

    /*Essential Part*/
    /*
    //--------------------------------------------------
    //  if(!strcmp(rlUalRec.USEC,"authdl"))
    //  {
    //  	dbg(DEBUG,"USEC == authdl,line<%5d>",__LINE__);
    //  	//Frank@home
    //		//sprintf(pclSqlBuf,"SELECT EVCT,URNO FROM %s WHERE URNO=(SELECT MAX(URNO) FROM %s) AND USID='%s' AND USEC='%s'"
    //										//,pcgUaltab, pcgUaltab, rlUalRec.USID, rlUalRec.USEC);
    //			sprintf(pclSqlBuf,"SELECT EVCT,URNO FROM %s WHERE URNO=(SELECT MAX(URNO) FROM %s WHERE USID='%s' AND USEC='%s')"
    //										,pcgUaltab, pcgUaltab, rlUalRec.USID, rlUalRec.USEC);
    //										//select * from SECTAB where URNO=(select max(URNO) from SECTAB);
    //		//Frank@home
    //	}
    //	else if(!strcmp(rlUalRec.USEC,pcgDestName))
    //	{
    //		dbg(DEBUG,"USEC == %s",pcgDestName);
    //		//if(!strcmp(rlUalRec.EVDE,"Activate") || !strcmp(rlUalRec.EVDE,"Deactivate"))
    //		{
    //				dbg(DEBUG,"EVDE == %s,line<%5d>",rlUalRec.EVDE, __LINE__);
    //
    //				sprintf(pclSqlBuf,"SELECT EVCT,URNO FROM %s WHERE URNO=(SELECT MAX(URNO) FROM %s WHERE USID='%s' AND USEC='%s' AND EVDE='%s') AND USID='%s' AND USEC='%s' AND EVDE='%s'"
    //										,pcgUaltab, pcgUaltab, rlUalRec.USID, rlUalRec.USEC, rlUalRec.EVDE, rlUalRec.USID, rlUalRec.USEC, rlUalRec.EVDE);
    //		}
    //		//else if(!strcmp(rlUalRec.EVDE,"Activate"))
    //	}
    */
    sprintf( pclSqlBuf, "SELECT MAX(EVCT) FROM %s WHERE USID='%s' AND EVDE='%s'"
             , pcgUaltab, rlUalRec.USID, rlUalRec.EVDE );
    /*-------------------------------------------------- */

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "UpdateUserAccountLogTable() Selecting the EVCT from UALTAB<%s>", pclSqlBuf );
    nlton( pclDataArea );

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc == DB_SUCCESS ) /*DB_SUCCESS = 0 */
    {
        /* process the fields in the line just read */
        get_fld( pclDataArea, FIELD_1, STR, EVCTLEN, clEvct );
        /*get_fld(pclDataArea,FIELD_2,STR,URNOLEN,clUrno); */
        dbg( DEBUG, "the EVCT is <%s>", clEvct );
        /*dbg(DEBUG,"the URNO is <%s>",clUrno); */

        ilEvct = atoi( clEvct );
        ilEvct++;
        itoa( ilEvct, rlUalRec.EVCT, 10 );
        dbg( DEBUG, "the evct++ is <%s>", rlUalRec.EVCT );

        dbg( DEBUG, "~~~UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
             rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );

        dbg( DEBUG, "UpdateUserAccountLogTable() insert a record based on existing one" );

        /*Frank@home*/
        if( bgLogAllAccountEvent == FALSE )
        {
            VARBUF rlUrnoList;

            InitVarBuf( &rlUrnoList );

            ilRc = RC_SUCCESS;

            dbg( DEBUG, "handleDeleteUal() Delete from %s where USID=%s AND EVDE='%s'", pcgUaltab, rlUalRec.USID, rlUalRec.EVDE );

            sprintf( clWhere, "USID='%s' AND EVDE='%s'", rlUalRec.USID, rlUalRec.EVDE );
            ilRc = handleDelete( pcgUaltab, clWhere, NULL, &rlUrnoList );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList, "handleDeleteUal() Error deleting record from UALTAB." );
                ilRc = RC_FAIL;
            }

            FreeVarBuf( &rlUrnoList );
        }

        if( InsertUalRec( &rlUalRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "UpdateUserAccountLogTable() Error inserting into UALTAB" );
            return RC_FAIL;
        }

        /*Frank@home*/
    }
    else if( ilRc == DB_ERROR ) /*DB_ERROR = -1 */
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess, "Error selecting record for SEC.URNO <%s>\n%s", pcpUrno, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
        return ilRc;
    }
    else if( ilRc == ORA_NOT_FOUND ) /*ORA_NOT_FOUND = 1 */
    {
        dbg( DEBUG, "!!!UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
             rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );
        dbg( DEBUG, "UpdateUserAccountLogTable() insert a new record" );

        if( InsertUalRec( &rlUalRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "UpdateUserAccountLogTable() Error inserting into UALTAB" );
            return RC_FAIL;
        }
    }

    dbg( DEBUG, "InsertUalTab finished" );

    close_my_cursor( &slLocalCursor );

    return ilRc;
}/*end of InsertUalTab */
/*Frank@UAM*/
/*Frank@UAL_UAA*/

/*Frank@UAL_UAA*/
static int handleDeleteUal( char *cpUsid )
{
    VARBUF rlUrnoList;

    InitVarBuf( &rlUrnoList );

    int ilRc = RC_SUCCESS;

    char clWhere[BIGBUF];

    dbg( DEBUG, "handleDeleteUal() Delete from %s where USID='%s'", pcgUaltab, cpUsid );

    sprintf( clWhere, "USID='%s'", cpUsid );
    ilRc = handleDelete( pcgUaltab, clWhere, NULL, &rlUrnoList );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList, "handleDeleteUal() Error deleting record from UALTAB." );
        ilRc = RC_FAIL;
    }

    FreeVarBuf( &rlUrnoList );
    return ilRc;
}/*end of handleDeleteUal */

static int handleDeleteUaa( char *cpGrpid )
{
    VARBUF rlUrnoList;

    InitVarBuf( &rlUrnoList );

    int ilRc = RC_SUCCESS;

    char clWhere[BIGBUF];

    dbg( DEBUG, "handleDeleteUaa() Delete from %s where cpGrpid ='%s' ", pcgUaatab, cpGrpid );

    sprintf( clWhere, "GRPU='%s'", cpGrpid );
    ilRc = handleDelete( pcgUaatab, clWhere, NULL, &rlUrnoList );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList, "handleDeleteUaa() Error deleting record from UAATAB." );
        ilRc = RC_FAIL;
    }

    FreeVarBuf( &rlUrnoList );
    return ilRc;
}/*end of handleDeleteUaa */

static int MakeSureThereIsOnlyOneRecordGivenSingleUsid( const char *pcpDestName, char *pcpErrorMess, int *pipRowNum )
{
    char *pclFunc = "MakeSureThereIsOnlyOneRecordGivenSingleUsid";
    short slLocalCursor = 0;
    short slFuncCode = 0;
    int   ilRc = RC_SUCCESS;
    char  pclSqlBuf[2048] = "\0";
    char  pclDataArea[2048] = "\0";
    char  clOraErr[250] = "\0";
    char  clWhere[BIGBUF] = "\0";

    memset( pclSqlBuf, 0, sizeof( pclSqlBuf ) );
    memset( pclDataArea, 0, sizeof( pclDataArea ) );
    memset( clWhere, 0, sizeof( clWhere ) );

    if( pcpDestName == NULL )
    {
        dbg( DEBUG, "pcpDestName is null, return" );
        return RC_FAIL;
    }

    sprintf( clWhere, "USID='%s'", pcpDestName );
    sprintf( pclSqlBuf, "SELECT ROWNUM FROM %s WHERE %s", pcgUaltab, clWhere );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "%s <select>pclSqlBuf\n%s", pclFunc, pclSqlBuf );

    slFuncCode = START;
    slLocalCursor = 0;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc == DB_SUCCESS )
    {
        *pipRowNum = atoi( pclDataArea );
        dbg( DEBUG, "%s SELECT execution successfully pipRowNum = %d", pclFunc, *pipRowNum ); /* successful execution  */
    }
    else
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess, "Error selecting record for UALTAB\n%s", clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        return RC_FAIL;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;
}

/* dbg(TRACE,"CMD <%s> QUE (%d) WKS <%s> USR <%s>", pclActCmd,igQueOut,pcgRecvName,pcgDestName);   */

static int CaptureLogInInfoToUaltab( char *pcpFieldList, char *pcpDataList, const char *pcpRecvName, const char *pcpDestName, char *pcpErrorMess )
{
    char *pclFunc = "CaptureLogInInfoToUaltab";
    char *pclItem;
    int  ilRc = RC_SUCCESS;
    char pclUtcTime[16] = "\0";
    char pclLocTime[16] = "\0";
    char clAppl[USIDLEN + 1]; /* application received in user data */
    char pclIpText[INET_ADDRSTRLEN] = "\0";
    UALREC rlUalRec;
    int  ilNextUrno = 0;
    char buffer[33] = "\0";

    memset( pclUtcTime, 0, sizeof( pclUtcTime ) );
    memset( clAppl, 0, USIDLEN + 1 );
    memset( &rlUalRec, 0, sizeof( rlUalRec ) );

    pclItem = getItem( pcpFieldList, pcpDataList, "APPL" );

    if( pclItem != NULL )
    {
        strcpy( clAppl, pclItem );
    }

    dbg( DEBUG, "%s APPL is <%s>", pclFunc, pclItem );

    /* get CDAT, UTC time  */
    GetServerTimeStamp( "LOC", 1, 0, pclLocTime );
    GetServerTimeStamp( "UTC", 1, 0, pclUtcTime );
    dbg( TRACE, "%s SYSTEM: LOCAL <%s> UTC <%s> ", pclFunc, pclLocTime, pclUtcTime );

    /* test */
    /*
    char pcpRecvNameTmp[128];
    memset(pcpRecvNameTmp,0,sizeof(pcpRecvNameTmp));
    //strcpy(pcpRecvNameTmp,"WIN7");//->0
      strcpy(pcpRecvNameTmp,"ORACLE");//->0
    dbg(DEBUG,"%s pcpRecvNameTmp = %s",pclFunc,pcpRecvNameTmp);
    ConvertIp(pcpRecvNameTmp,pclIpText);
     */
    ConvertIp( pcpRecvName, pclIpText );

    /* Getting the New URNO  */
    ilNextUrno = NewUrnos( "UALTAB", 1 );

    if( ilNextUrno > 0 )
    {
        if( itoa( ilNextUrno, buffer, 10 ) == NULL )
        {
            dbg( DEBUG, "line<%d>Error getting new URNO", __LINE__ );
            return RC_FAIL;
        }
        else
        {
            strncpy( rlUalRec.URNO, buffer, strlen( buffer ) );
        }
    }
    else
    {
        dbg( DEBUG, "line<%d>Error getting new URNO", __LINE__ );
        return RC_FAIL;
    }

    dbg( DEBUG, "the URNO for UALTAB is <%s>", rlUalRec.URNO );

    strcpy( rlUalRec.CDAT, pclUtcTime );
    strcpy( rlUalRec.EVDE, "LOGIN" );
    strcpy( rlUalRec.USID, pcpDestName );
    strcpy( rlUalRec.RMRK, pclIpText );
    strcpy( rlUalRec.USEC, clAppl );

    dbg( DEBUG, "UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
         rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );

    ilRc = UpdateUALTAB( rlUalRec, pcpErrorMess );
    return ilRc;
}

static int UpdateUALTAB( UALREC rlUalRec, char *pcpErrorMess )
{
    int ilRc = RC_SUCCESS;
    char clOraErr[250] = "\0";
    char *pclFunc = "UpdateUALTAB";
    char pclSqlBuf[2048] = "\0";
    char pclDataArea[2048] = "\0";
    char clWhere[BIGBUF] = "\0";
    short slLocalCursor = 0;
    short slFuncCode = 0;
    char clEvct[EVCTLEN + 1] = "\0";
    int ilEvct = 0;

    memset( clOraErr, 0, sizeof( clOraErr ) );
    memset( pclSqlBuf, 0, sizeof( pclSqlBuf ) );
    memset( pclDataArea, 0, sizeof( pclDataArea ) );
    memset( clWhere, 0, sizeof( clWhere ) );

    slLocalCursor = 0;
    slFuncCode = START;

    sprintf( pclSqlBuf, "SELECT MAX(EVCT) FROM %s WHERE USID='%s' AND EVDE='%s'"
             , pcgUaltab, rlUalRec.USID, rlUalRec.EVDE );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "UpdateUserAccountLogTable() Selecting the EVCT from UALTAB<%s>", pclSqlBuf );
    nlton( pclDataArea );
    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc == DB_SUCCESS )
    {
        /* process the fields in the line just read */
        get_fld( pclDataArea, FIELD_1, STR, EVCTLEN, clEvct );
        /* get_fld(pclDataArea,FIELD_2,STR,URNOLEN,clUrno);   */
        dbg( DEBUG, "%s the EVCT is <%s>", pclFunc, clEvct );
        /* dbg(DEBUG,"the URNO is <%s>",clUrno); */

        ilEvct = atoi( clEvct );
        ilEvct++;
        itoa( ilEvct, rlUalRec.EVCT, 10 );
        dbg( DEBUG, "%s the evct++ is <%s>", pclFunc, rlUalRec.EVCT );

        dbg( DEBUG, "%s <FOUND>UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
             pclFunc, rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );

        dbg( DEBUG, "%s UpdateUserAccountLogTable() insert a record based on existing one", pclFunc );

        if( bgLogAllAccountEvent == FALSE )
        {
            VARBUF rlUrnoList;

            InitVarBuf( &rlUrnoList );

            ilRc = RC_SUCCESS;

            dbg( DEBUG, "handleDeleteUal() Delete from %s where USID=%s AND EVDE='%s'", pcgUaltab, rlUalRec.USID, rlUalRec.EVDE );

            sprintf( clWhere, "USID='%s' AND EVDE='%s'", rlUalRec.USID, rlUalRec.EVDE );
            ilRc = handleDelete( pcgUaltab, clWhere, NULL, &rlUrnoList );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList, "handleDeleteUal() Error deleting record from UALTAB." );
                ilRc = RC_FAIL;
            }

            FreeVarBuf( &rlUrnoList );
        }

        if( InsertUalRec( &rlUalRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "Error inserting into UALTAB" );
            return RC_FAIL;
        }
    }
    else if( ilRc == DB_ERROR ) /* DB_ERROR = -1 */
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess, "Error selecting record for SEC.URNO <%s>\n%s", rlUalRec.URNO, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
        return ilRc;
    }
    else if( ilRc == ORA_NOT_FOUND )  /* ORA_NOT_FOUND = 1 */
    {
        dbg( DEBUG, "%s <NOTFOUND>UALTAB URNO<%s>,EVCT<%s>,USID<%s>,EVDE<%s>,CDAT<%s>,USEC<%s>,RMRK<%s>",
             pclFunc, rlUalRec.URNO, rlUalRec.EVCT, rlUalRec.USID, rlUalRec.EVDE, rlUalRec.CDAT, rlUalRec.USEC, rlUalRec.RMRK );
        dbg( DEBUG, "%s insert a new record", pclFunc );

        if( InsertUalRec( &rlUalRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "Error inserting into UALTAB" );
            return RC_FAIL;
        }
    }

    close_my_cursor( &slLocalCursor );

    return ilRc;
}

static int ConvertIp( const char *pcpRecvName, char *pcpIpText )
{
    char *pclFunc = "ConvertIp";
    unsigned long llIpDecimal = 0;
    struct sockaddr_in rlIpConverter;
    int ilRc = RC_SUCCESS;

    memset( &rlIpConverter, 0, sizeof( rlIpConverter ) );
    memset( pcpIpText, 0, sizeof( pcpIpText ) );

    if( pcpRecvName == NULL )
    {
        dbg( DEBUG, "pcpRecvName is null, return" );
        return RC_FAIL;
    }

    /* 1 It is hex string type */
    dbg( DEBUG, "%s pcpRecvName = %s", pclFunc, pcpRecvName );

    /* 2 Changing it to decimal long type */
    llIpDecimal = strtoul( pcpRecvName, NULL, 16 );
    dbg( DEBUG, "%s llIpDecimal = %lu", pclFunc, llIpDecimal );

    if( llIpDecimal == ERANGE )
    {
        dbg( DEBUG, "strtoul error -> ERANGE,error code is %d", errno );
        return RC_FAIL;
    }
    else if( llIpDecimal == 0 )
    {
        dbg( DEBUG, "This is not a valid IP adress,strcpy(pcpIpText,pcpRecvName)" );
        strcpy( pcpIpText, pcpRecvName );

        return RC_FAIL;
    }
    else
    {
        /* 3 Get the dot seperated format */
        rlIpConverter.sin_addr.s_addr = llIpDecimal;
        inet_ntop( AF_INET, &( rlIpConverter.sin_addr ), pcpIpText, INET_ADDRSTRLEN );

        if( pcpIpText == NULL )
        {
            dbg( DEBUG, "inet_ntop return NULL and pcpIpText is NULL, error code is %d", errno );
            pcpIpText = NULL;
            return RC_FAIL;
        }
        else
        {
            dbg( DEBUG, "%s pcpIpText=%s\n", pclFunc, pcpIpText );
            return ilRc;
        }
    }
}

/**************************************************************************/
/* InsertUalRec()                                                         */
/*                                                                        */
/* prpUalRec - UALREC record.                                             */
/*                                                                        */
/* Copies the contents of UALREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertUalRec( UALREC *prpUalRec )
{
    char pclFieldList[500], pclDataList[500];

    strcpy( pclFieldList, "URNO,EVCT,USID,EVDE,CDAT,USEC,RMRK" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s,%s,%s",
             prpUalRec->URNO, prpUalRec->EVCT, prpUalRec->USID,
             prpUalRec->EVDE, prpUalRec->CDAT, prpUalRec->USEC,
             prpUalRec->RMRK );

    dbg( DEBUG, "&&&pcgUaltab<%s>", pcgUaltab );
    return handleInsert( pcgUaltab, pclFieldList, pclDataList, prpUalRec->URNO );

} /* end InsertUalRec() */
/*Frank@UAL_UAA*/


/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckIsSuperUser()                                      */
/*                                                                      */
/* Parameters:  pcpUrno - URNO of user in SECTAB                        */
/*              pcpErrorMess - possible error mess (see below)          */
/*                                                                      */
/* Returns: RC_FAIL or RC_SUCCESS.                                      */
/*                                                                      */
/* Description: This command checks:                                    */
/*                                                                      */
/*              If the user is a superuser, ir can use BDPS-SEC         */
/*              the user is a superuser if SECTAB.LANG == '1'           */
/*              Ceda errors are returned in pcpErrorMess.               */
/*                                                                      */
/*              In addition the following errors can be returned:       */
/*              "INVALID_SUPERUSER" --> not superuser                   */
/*                                                                      */
/* ******************************************************************** */
static int CheckIsSuperUser( char *pcpUrno, char *pcpErrorMess)
{
    int         ilRc = RC_SUCCESS;
    char        clLang[LANGLEN+1];
    short       slLocalCursor;
    short       slFuncCode;
    char        clOraErr[250];
    char        pclSqlBuf[2048],pclDataArea[2048];

#ifdef CCS_DBMODE_EMBEDDED
    sprintf(pclSqlBuf, "SELECT LANG FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
    sprintf(pclSqlBuf, "SELECT LANG FROM %s WHERE URNO=?", pcgSectab);
#endif

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    sprintf( pclDataArea, "%s\n", pcpUrno );
    dbg( DEBUG, "CheckIsSuperUser() Selecting superuser data...\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    nlton( pclDataArea );
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS )
    {
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess,
                 "Error selecting record for SEC.URNO <%s>\n%s",
                 pcpUrno, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;

    } /* end if */


    if( ilRc == RC_SUCCESS )
    {
        memset( clLang, 0, LANGLEN + 1 );
        get_fld( pclDataArea, FIELD_1, STR, LANGLEN, clLang );
        dbg( DEBUG, "CheckIsSuperUser() SEC.URNO <%s> SEC.LANG <%s>",
             pcpUrno, clLang );

		if( strncmp( clLang, "1", 1 ))
		{
			sprintf( pcpErrorMess, "%s INVALID_SUPERUSER", LOGINERROR );
			PushError( &rgErrList, pcpErrorMess );
			ilRc = RC_FAIL;
		}
		else
		{
			dbg( DEBUG, "CheckIsSuperUser() Is SuperUser!" );
		}
    } /* end if( ilRc == RC_SUCCESS ) */

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end CheckIsSuperUser() */





/* ******************************************************************** */
/*                                                                      */
/* Function:    CheckValidCal()                                         */
/*                                                                      */
/* Parameters:  pcpFsec - URNO                                          */
/*              pcpErrorMess - Error message (see below).               */
/*                                                                      */
/* Returns: RC_SUCCESS,ORA_NOT_FOUND or RC_FAIL.                        */
/*                                                                      */
/* Description: Reads CALTAB using pcpFsec as the key. If the record    */
/*              exists then check if the currentDate is within VAFR,    */
/*              VATO and FREQ.  Returns ORA_NOT_FOUND if the dates      */
/*              are invalid.                                            */
/*                                                                      */
/* ******************************************************************** */
static int CheckValidCal( char *pcpFsec, char *pcpErrorMess )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor;
    short       slFuncCode;
    char        clOraErr[250];
    char        pclSqlBuf[2048],pclDataArea[2048];

    sprintf( pclSqlBuf, "SELECT URNO FROM %s WHERE FSEC=%s AND VAFR<='%s' AND VATO>='%s' AND FREQ LIKE '%%%s%%'", pcgCaltab, pcpFsec, pcgCurrDate, pcgCurrDate, pcgDayOfWeek );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "CheckValidCal() Selecting VAFR/VATO...\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    strcpy( pclDataArea, "" );
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS && ilRc != ORA_NOT_FOUND )
    {
        char clErr[BIGBUF];
        get_ora_err( ilRc, clOraErr );
        sprintf( pcpErrorMess, "Error CheckValidCal() for FSEC=<%s>\n%s",
                 pcpFsec, clOraErr );
        PushError( &rgErrList, pcpErrorMess );
        sprintf( clErr, "CheckValidCal() SqlBuf:\n%s", pclSqlBuf );
        PushError( &rgErrList, clErr );
        ilRc = RC_FAIL;
    } /* end if */

    if( ilRc == DB_SUCCESS )
    {
        dbg( DEBUG, "CheckValidCal() Dates Valid !" );
        ilRc = RC_SUCCESS;
    }

    if( ilRc == ORA_NOT_FOUND )
    {
        dbg( DEBUG, "CheckValidCal() Dates NOT Valid !" );
        dbg( DEBUG, "CheckValidCal() FSEC <%s> Expired Dates", pcpFsec );
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end CheckValidCal() */



/* ******************************************************************** */
/*                                                                      */
/* Function:    handleSysIni()                                          */
/*                                                                      */
/* Parameters:  None.                                                   */
/*                                                                      */
/* Returns: None.                                                       */
/*                                                                      */
/* Description: This is a command called when CEDA is installed via     */
/*              cutil. This function creates the BDPS-SEC application   */
/*              with a single button in FKTTAB "ModuInit".              */
/*              The user UFIS$ADMIN is created with the default         */
/*              password and a profile allowing the user to log into    */
/*              BDPS-SEC, press the ModuInit button and thus download   */
/*              the BDPS-SEC configuration.                             */
/*                                                                      */
/* ******************************************************************** */
void handleSysIni( void )
{
    char pclFieldList[BIGBUF], pclDataList[BIGBUF], pclErrorMess[BIGBUF];

    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;
    char clWhere[BIGBUF];

    dbg( DEBUG, "-------------- %s -------------", GetCurrTime() );
    strcpy( pcgRecvName, "" );
    strcpy( pcgDestName, "" );
    strcpy( pcgUsername, "" );

    if( strcmp( pcgDefTblExt, "TAB" ) == 0 )
    {
        /* GET HOPO FROM CONFIG FILE */
        /* (Read it into pcgH3LC) */
        /* (Don't change the Default in pcgDefH3LC) */
        ReadCfg( pcgCfgFile, pcgH3LC, "DEFAULTS", "HOME", pcgDefH3LC );

        if( strcmp( pcgH3LC, pcgDefH3LC ) == 0 )
        {
            dbg( TRACE, "USING DEFAULT HOMEPORT" );
        } /* end if */

        igAddHopo = TRUE;
    } /* end if */
    else
    {
        igAddHopo = FALSE;
    } /* end else */

    dbg( TRACE, "Initializing the system for <%s>", pcgH3LC );

    if( strstr( pcgHopoList, pcgH3LC ) == NULL )
    {
        if( strlen( pcgHopoList ) > 0 )
        {
            strcat( pcgHopoList, "," );
        } /* end if */

        strcat( pcgHopoList, pcgH3LC );
    } /* end if */

    /* init the list of urnos returned from handleCollectUrnos() */
    InitVarBuf( &rlUrnoList );

    /* initialise logging of this command */

    /* check if the application already exists, if so delete it */
    sprintf( clWhere, "USID='%s' AND TYPE='A'", pcgDefModu );
    ilRc = handleCollectUrnos( pcgSectab, clWhere, &rlUrnoList );

    if( ilRc == RC_SUCCESS && LenVarBuf( &rlUrnoList ) > 0 )
    {
        sprintf( pclFieldList, "URNO" );
        strcpy( pclDataList, ( char* ) rlUrnoList.buf );
        dbg( DEBUG, "handleSysIni() Deleting %s.", pcgDefModu );
        ilRc = handleDelApp( pclFieldList, pclDataList, pclErrorMess );
    }

    /* clear the URNO list */
    ClearVarBuf( &rlUrnoList );

    /* check if the user already exists, if so delete it */
    sprintf( clWhere, "USID='%s' AND TYPE='U'", pcgDefUsid );
    ilRc = handleCollectUrnos( pcgSectab, clWhere, &rlUrnoList );

    if( ilRc == RC_SUCCESS && LenVarBuf( &rlUrnoList ) > 0 )
    {
        sprintf( pclFieldList, "URNO" );
        strcpy( pclDataList, ( char* ) rlUrnoList.buf );
        dbg( DEBUG, "handleSysIni() Deleting %s.", pcgDefUsid );
        ilRc = handleDelSec( pclFieldList, pclDataList, pclErrorMess );
    }


    /* create the BDPS-SEC application with a single record "ModuInit"
       in FKTTAB */

    strcpy( pclFieldList, "USID,TYPE,STAT,VAFR,VATO" );
    sprintf( pclDataList, "%s,A,%s,%s,%s",
             pcgDefModu, pcgDefStat, pcgDefVafr, pcgDefVato );

    dbg( DEBUG, "handleSysIni() Inserting %s.", pcgDefModu );
    ilRc = handleNewApp( pclFieldList, pclDataList, pclErrorMess );


    /* create the user UFIS$ADMIN with a profile for BDPS-SEC */
    if( ilRc == RC_SUCCESS )
    {
        strcpy( pclFieldList, "USID,TYPE,PASS,STAT,MODU,VAFR,VATO" );
        sprintf( pclDataList, "%s,U,%s,%s,1,%s,%s",
                 pcgDefUsid, pcgDefPass,
                 pcgDefStat, pcgDefVafr, pcgDefVato );

        dbg( DEBUG, "handleSysIni() Inserting %s.", pcgDefUsid );
        ilRc = handleNewSec( pclFieldList, pclDataList, pclErrorMess );

    }

    if( ilRc == RC_SUCCESS )
    {
        dbg( TRACE, "handleSysIni() Success, committing changes." );
        handleCommit();
    }
    else
    {
        handleRollback();
    }

    FreeVarBuf(&rlUrnoList);

} /* end handleSysIni() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleReadTab()                                         */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpTableName - table to be read                         */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Reads and returns records in the table specified.       */
/*              This function is used by BDPS-SEC to load the           */
/*              security tables, sqlhdl is not used due to performance  */
/*              degradation and so that access to these tables can be   */
/*              prevented for applications other than BDPS-SEC.         */
/*                                                                      */
/* ******************************************************************** */
static int handleReadTab( char *pcpFieldList, char *pcpTableName, char *pcpErrorMess )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor, slFuncCode;
    char        pclSqlBuf[2048],pclDataArea[2048];
    int         ilNumRecs = 0;
    int         ilNumFields = itemCount( pcpFieldList );
    int         ilFld, ilFieldLen;
    char        pclField[MAX_FIELD_LEN];
    char        clErr[250], clOraErr[500];

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* prepare the SQL command to read the FKTTAB */
    sprintf( pclSqlBuf, "SELECT %s FROM %s", pcpFieldList, pcpTableName );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );

    dbg( DEBUG, "handleReadTab()..\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;

    /* loop - read and process a line at a time */
    while( ilRc == DB_SUCCESS )
    {
        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {
            for( ilFld = 0; ilFld < ilNumFields; ilFld++ )
            {
                memset( pclField, 0x00, MAX_FIELD_LEN );
                get_fld( pclDataArea, ilFld, STR, MAX_FIELD_LEN, pclField );

                if( ilFld < ( ilNumFields - 1 ) )
                {
                    /* add comma (field seperator) */
                    strcat( pclField, "," );
                    ilFieldLen = strlen( pclField );
                }
                else
                {
                    /* add CR LF (end of record) */
                    ilFieldLen = strlen( pclField ) + 2;
                    strcat( pclField, "\r\n" );
                }

                /* add the new field to the result buffer */
                ilRc = AddVarBuf( &rgResultBuf, pclField,
                                  ilFieldLen );
            }

            ilNumRecs++;
            slFuncCode = NEXT;
        }
        else if( ilRc == RC_FAIL )
        {
            get_ora_err( ilRc, clOraErr );
            ilRc = RC_FAIL;
        }

    } /* while */

    if( ilRc == RC_FAIL )
    {
        PushError( &rgErrList, clOraErr );
        sprintf( "handleReadTab() FieldList <%s>", pcpFieldList );
        sprintf( "handleReadTab() TableName <%s>", pcpTableName );
        AddVarBuf( &rgResultBuf, clOraErr, strlen( clOraErr ) );
        ilRc = RC_FAIL;
    }
    else if( ilNumRecs == 0 )
    {
        strcpy( clOraErr, "No Data Found" );
        AddVarBuf( &rgResultBuf, clOraErr, strlen( clOraErr ) );
        ilRc = RC_FAIL;
    }
    else
    {
        char *pclBuf = ( char * ) rgResultBuf.buf;
        pclBuf[strlen( pclBuf ) - 2] = '\0';
        dbg( DEBUG, "handleReadTab() %d records read from %s.",
             ilNumRecs, pcpTableName );

        ilRc = RC_SUCCESS;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end handleReadTab() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleNewSec()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Creates new SECTAB and CALTAB records for users,        */
/*              profiles,groups,wks and wks groups. If MODU is non-     */
/*              zero a profile record is created.                       */
/*              If MODU is the URNO of a profile in SECTAB, then the    */
/*              profile is copied.                                      */
/*              "USID,[NAME],TYPE,[LANG],[PASS],STAT,[REMA],[MODU],     */
/*               VAFR,VATO,[FREQ]"                                      */
/*                                                                      */
/* ******************************************************************** */
static int handleNewSec( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char *pclItem;
    int ilNumFields, ilNumData;
    int ilModu, ilCount, ilNumUrnos, ilNumCalFlds;
    char clSecUrno[URNOLEN + 1], clUrnoOfProfileToCopy[URNOLEN + 1];
    int ilRc = RC_SUCCESS;
    char clWhere[BIGBUF];

    dbg( DEBUG, "handleNewSec() pcpFieldList <%s> pcpDataList <%s>", pcpFieldList, pcpDataList );

    /* MODU field --> 0 = no profile, 1 = standard profile, 2 = profile disabled, 3 = profile enabled, URNO = copy profile with this URNO */
    strcpy( clUrnoOfProfileToCopy, "" );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "MODU" ) ) != NULL )
    {
        ilModu = atoi( pclItem );

        if( ilModu != STANDARD && ilModu != DISABLE_ALL && ilModu != ENABLE_ALL )
        {
            strcpy( clUrnoOfProfileToCopy, pclItem );
        }
    }
    else
    {
        ilModu = NO_PROFILE;
    }


    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* caluclate how many URNOs are required:
        SECTAB.URNO * 1
        CALTAB.URNO * num cal recs
        if( ilModu > 0 )
            GRPTAB.URNO * 2 (one for GRPTAB and one for CALTAB)
            PRVTAB.URNO * num recs in FKTTAB
        endif
        All URNOs are selected together at start because GetNextVal()
        does a commit meaning that rollbacks could not be used */

    ilNumUrnos = 1; /* SECTAB.URNO */

    /* work out how many cal values there are */
    ilNumFields = itemCount( pcpFieldList ); /*num fields in the field list */
    ilNumData = itemCount( pcpDataList ); /* num fields in the data list */

    /* check for FREQ field (optional field) */
    if( getItem( pcpFieldList, pcpDataList, "FREQ" ) != NULL )
    {
        ilNumCalFlds = 3;
    }
    else
    {
        ilNumCalFlds = 2;
    }

    ilNumUrnos += ( ilNumData - ( ilNumFields - ilNumCalFlds ) ) / ilNumCalFlds;


    /* if profile is to be added work out how many profile recs there are */
    if( ilModu != NO_PROFILE )
    {
        if( strlen( clUrnoOfProfileToCopy ) )
        {
            /* the URNO of a profile to be copied, so count how many recs in the profile */
            sprintf( clWhere, "FSEC=%s", clUrnoOfProfileToCopy );

            if( handleCount( pcgPrvtab, clWhere, &ilCount ) != RC_SUCCESS )
            {
                sprintf( pcpErrorMess, "Error count records in PRVTAB for Profile URNO <%s>.", clUrnoOfProfileToCopy );
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
            }
        }
        else
        {
            /* count the number of records in FKTTAB */
            if( handleCount( pcgFkttab, NULL, &ilCount ) != RC_SUCCESS )
            {
                sprintf( pcpErrorMess, "Error count records in FKTTAB." );
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
            }
        }

        if( ilRc == RC_SUCCESS )
        {
            ilNumUrnos += ( ilCount + 2 ); /* +2 for GRP/CAL record */
            dbg( DEBUG, "handleNewSec() Num URNOs required is <%d>", ilNumUrnos );
        }
    }


    /* read all the URNOs required */
    if( ilRc == RC_SUCCESS )
    {
        if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* create a record in SECTAB and get back the new SEC URNO */
    /* also creates 1 to n records in CALTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( handleCreateSecCal( pcpFieldList, pcpDataList, clSecUrno, &rgResultBuf ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a SECTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }


    /* create a personal profile if required */
    if( ilRc == RC_SUCCESS && ilModu != NO_PROFILE )
    {
        char pclFieldList[BIGBUF], pclDataList[BIGBUF];

        /* create a group rec for the personal prof, for personal profs
           FREL and FSEC are identical, also creates related CAL recs */
        strcpy( pclFieldList, "FREL,FSEC,TYPE" );
        sprintf( pclDataList, "%s,%s,P", clSecUrno, clSecUrno );

        if( handleCreateGrpCal( pclFieldList, pclDataList, &rgResultBuf, NULL ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a GRPTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }

        /* copy records from FKTTAB to PRVTAB */
        if( ilRc == RC_SUCCESS )
        {
            if( strlen( clUrnoOfProfileToCopy ) )
            {
                if( handleCopyPrv( clSecUrno, clUrnoOfProfileToCopy, &rgResultBuf ) != RC_SUCCESS )
                {
                    strcpy( pcpErrorMess, "Error copying PRVTAB." );
                    PushError( &rgErrList, pcpErrorMess );
                    ilRc = RC_FAIL;
                }
            }
            else
            {
                strcpy( pclFieldList, "FSEC,MODU" );
                sprintf( pclDataList, "%s,%d", clSecUrno, ilModu );

                if( handleCreatePrv( pclFieldList, pclDataList, &rgResultBuf ) != RC_SUCCESS )
                {
                    strcpy( pcpErrorMess, "Error creating a PRVTAB record." );
                    PushError( &rgErrList, pcpErrorMess );
                    ilRc = RC_FAIL;
                }
            }
        }


    } /* end if( ilRc == RC_SUCCESS && ilModu != NO_PROFILE ) */

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, NEWSEC_COMMAND, "", "FSEC", clSecUrno );
    }

    return ilRc;

} /* end handleNewSec() */



/* ******************************************************************** */
/*                                                                      */
/* Function:    handleUpdSec()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Update fields in SECTAB/CALTAB.                         */
/*              "URNO,[USID],[NAME],[LANG],[STAT],[REMA],[VAFR],        */
/*               [VATO],[FREQ]"                                         */
/*                                                                      */
/* ******************************************************************** */
static int handleUpdSec( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char        *pclItem;
    char        clSecUrno[URNOLEN+1];
    char        clWhere[BIGBUF];
    char        pclFieldList[BIGBUF], pclDataList[BIGBUF];
    int         ilRc = RC_SUCCESS;
    VARBUF      rlUrnoList;
    BOOL        blFieldListNotEmpty;


    /* init the list of urnos returned from handleUpdate() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* get the URNO of the record to be updated */
    memset( clSecUrno, 0, sizeof( clSecUrno ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "URNO" ) ) != NULL )
    {
        strcpy( clSecUrno, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error URNO not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* get the fields to be updated in SECTAB */
    if( ilRc == RC_SUCCESS )
    {
        memset( pclFieldList, 0, sizeof( pclFieldList ) );
        memset( pclDataList, 0, sizeof( pclDataList ) );

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "USID" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "USID" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "NAME" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "NAME" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "LANG" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "LANG" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "STAT" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "STAT" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "REMA" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "REMA" );
        }

        if( ( pclItem = getItem( pcpFieldList,pcpDataList,"DEPT" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "DEPT" );
        }

        blFieldListNotEmpty = strlen( pclFieldList ) > 0 ? TRUE : FALSE;

        /* remove the last comma */
        if( blFieldListNotEmpty )
        {
            pclDataList[strlen( pclDataList ) - 1] = '\0';
            pclFieldList[strlen( pclFieldList ) - 1] = '\0';
            dbg( DEBUG, "UpdSec \"%s\" --> \"%s\"", pclFieldList, pclDataList );
        }
    }


    /* update SECTAB */
    if( ilRc == RC_SUCCESS && blFieldListNotEmpty )
    {
        if( handleUpdate( pcgSectab, pclFieldList, pclDataList, NULL, clSecUrno, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error updating SECTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* get the fields to be updated in CALTAB */
    if( ilRc == RC_SUCCESS )
    {
        memset( pclFieldList, 0, sizeof( pclFieldList ) );
        memset( pclDataList, 0, sizeof( pclDataList ) );

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "VAFR" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "VAFR" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "VATO" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "VATO" );
        }

        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "FREQ" ) ) != NULL )
        {
            sprintf( pclDataList, "%s%s,", pclDataList, pclItem );
            sprintf( pclFieldList, "%s%s,", pclFieldList, "FREQ" );
        }

        blFieldListNotEmpty = strlen( pclFieldList ) > 0 ? TRUE : FALSE;

        /* remove the last comma */
        if( blFieldListNotEmpty )
        {
            pclDataList[strlen( pclDataList ) - 1] = '\0';
            pclFieldList[strlen( pclFieldList ) - 1] = '\0';
        }
    }


    /* update CALTAB */
    if( ilRc == RC_SUCCESS && blFieldListNotEmpty )
    {
        sprintf( clWhere, "FSEC=%s", clSecUrno );

        if( handleUpdate( pcgCaltab, pclFieldList, pclDataList, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error updating CALTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, UPDSEC_COMMAND, "", "FSEC", clSecUrno );
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleUpdSec() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleNewCal()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Creates new SECTAB and CALTAB records for users,        */
/*              profiles,groups,wks and wks groups. If MODU is non-     */
/*              zero a profile record is created.                       */
/*              "USID,[NAME],TYPE,[LANG],[PASS],STAT,[REMA],[MODU],     */
/*               VAFR,VATO,[FREQ]"                                      */
/*                                                                      */
/* ******************************************************************** */
static int handleNewCal( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumFields, ilNumData;
    int ilNumUrnos;
    int ilRc = RC_SUCCESS;

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* caluclate how many URNOs are required: */

    /* work out how many cal values there are */
    ilNumFields = itemCount( pcpFieldList ); /*num fields in the field list */
    ilNumData = itemCount( pcpDataList ); /* num fields in the data list */
    ilNumUrnos = ilNumData / ilNumFields;

    /* read all the URNOs required */
    if( ilRc == RC_SUCCESS )
    {
        if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* create a record in SECTAB and get back the new SEC URNO */
    /* also creates 1 to n records in CALTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( handleCreateCal( pcpFieldList, pcpDataList, NULL, &rgResultBuf ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a CALTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    return ilRc;

} /* end handleNewCal() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleDelCal()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Deletes records from CALTAB.                            */
/*              Fields: "(URNO)..." one to many.                        */
/*                                                                      */
/* ******************************************************************** */
static int handleDelCal( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumData, ilLoop, ilNumFields;
    char clUrno[URNOLEN + 1];
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;


    /* init the list of urnos returned from handleUpdate() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* get the number of records to be updated */
    ilNumData = itemCount( pcpDataList );
    ilNumFields = itemCount( pcpFieldList );

    for( ilLoop = 1; ilRc == RC_SUCCESS && ilLoop <= ilNumData; ilLoop += ilNumFields )
    {
        memset( clUrno, 0, sizeof( clUrno ) );

        strcpy( clUrno, sGet_item2( pcpDataList, ilLoop ) );


        ilRc = handleDelete( pcgCaltab, NULL, clUrno, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            sprintf( pcpErrorMess, "Error del from %s.", pcgCaltab );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }

    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleDelCal() */



/* ******************************************************************** */
/*                                                                      */
/* Function:    handleUpdCal()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Updates fields in CALTAB.                               */
/*              Fields: "(URNO,VAFR,VATO,FREQ)..." one to many.         */
/*                                                                      */
/* ******************************************************************** */
static int handleUpdCal( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumData, ilLoop, ilNumFields;
    char clUrno[URNOLEN + 1], clVafr[DATELEN + 1];
    char clVato[DATELEN + 1], clFreq[FREQLEN + 1];
    char pclDataList[BIGBUF], clWhere[BIGBUF];
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;


    /* init the list of urnos returned from handleUpdate() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* get the number of records to be updated */
    ilNumData = itemCount( pcpDataList );
    ilNumFields = itemCount( pcpFieldList );

    for( ilLoop = 1; ilRc == RC_SUCCESS && ilLoop <= ilNumData; ilLoop += ilNumFields )
    {
        memset( clUrno, 0, sizeof( clUrno ) );
        memset( pclDataList, 0, sizeof( pclDataList ) );
        memset( clVafr, 0, sizeof( clVafr ) );
        memset( clVato, 0, sizeof( clVato ) );
        memset( clFreq, 0, sizeof( clFreq ) );

        strcpy( clUrno, sGet_item2( pcpDataList, ilLoop ) );
        strcpy( clVafr, sGet_item2( pcpDataList, ilLoop + 1 ) );
        strcpy( clVato, sGet_item2( pcpDataList, ilLoop + 2 ) );
        strcpy( clFreq, sGet_item2( pcpDataList, ilLoop + 3 ) );

        if( strlen( clFreq ) <= 0 )
            strcpy( clFreq, " " );

        sprintf( pclDataList, "%s,%s,%s", clVafr, clVato, clFreq );
        sprintf( clWhere, "URNO=%s", clUrno );

        ilRc = handleUpdate( pcgCaltab, "VAFR,VATO,FREQ", pclDataList, clWhere, NULL, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            sprintf( pcpErrorMess, "Error updating %s.", pcgCaltab );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }

    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleUpdCal() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleNewPwd()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Sets the password for a user in SECTAB.                 */
/*              Checks if the user exists, checks the old password      */
/*              if all is valid sets the new password.                  */
/*              "USID,OLDP,NEWP"                                        */
/*                                                                      */
/* ******************************************************************** */
static int handleNewPwd( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char *pclItem;
    char clUsid[USIDLEN + 1];
    char clOldPass[PASSLEN + 1];
    char clNewPass[PASSLEN + 1];
    char clUrno[URNOLEN + 1];
    int ilRc = RC_SUCCESS;


    /* get the USID */
    memset( clUsid, 0, sizeof( clUsid ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "USID" ) ) != NULL )
    {
        strcpy( clUsid, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error USID not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* get the existing password */
    memset( clOldPass, 0, sizeof( clOldPass ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "OLDP" ) ) != NULL )
    {
        strcpy( clOldPass, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error OLDP not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* get the new password */
    memset( clNewPass, 0, sizeof( clNewPass ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "NEWP" ) ) != NULL )
    {
        strcpy( clNewPass, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error NEWP not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* check the USID and the old password */
    if( ilRc == RC_SUCCESS )
    {
        char clErrMess[BIGBUF];
        ilRc = CheckValidSecCal( clUsid, clOldPass, "U", clUrno, clErrMess );

        if( ilRc != RC_SUCCESS )
        {
            if( strstr( clErrMess, LOGINERROR ) != NULL )
            {
                if( strstr( clErrMess, "INVALID_USER" ) != NULL || strstr( clErrMess, "INVALID_PASSWORD" ) != NULL )
                {
                    /* either username of password error */
                    strcpy( pcpErrorMess, clErrMess );
                    ilRc = RC_FAIL;
                }
                else /* other irrelevant error */
                    ilRc = RC_SUCCESS;
            }
            else
            {
                /* database error */
                strcpy( pcpErrorMess, clErrMess );
                ilRc = RC_FAIL;
            }
        }
    }

    /* if everything is valid reset the password */
    if( ilRc == RC_SUCCESS )
    {
        if( LoadUrnoList( 1 ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            char clFieldList[BIGBUF];
            char clDataList[BIGBUF];
            sprintf( clFieldList, "URNO,PASS" );
            sprintf( clDataList, "%s,%s", clUrno, clNewPass );
            ilRc = handleSetPwd( clFieldList, clDataList, pcpErrorMess, FALSE );
        }
    }

    return ilRc;

} /* end handleNewPwd() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleSetPwd()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*              bpResetOldPasswords - delete old passwords in PWDTAB    */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Sets the password for a user in SECTAB.                 */
/*              "URNO,PASS"                                             */
/*                                                                      */
/* ******************************************************************** */
static int handleSetPwd( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess, BOOL bpResetOldPasswords )
{
    char *pclItem;
    char clSecUrno[URNOLEN + 1];
    char clPass[PASSLEN + 1];
    char clEncryptedPass[PASSLEN + 1];
    char clPwdUrno[URNOLEN + 1];
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;

    /*Frank@UAM*/
    char clRmrk[RMRKLEN + 1];
    /*Frank@UAM*/

    /* init the list of urnos returned from handleUpdate() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /*Frank@UAM*/
    /* get the RMRK of the record to be updated */
    memset( clRmrk, 0, sizeof( clRmrk ) );
    pclItem = getItem( pcpFieldList, pcpDataList, "REMA" );

    if( pclItem != NULL )
    {
        strcpy( clRmrk, pclItem );
    }

    dbg( DEBUG, "Frank@UAM RMRK for UALTAB is <%s>", clRmrk );
    /*Frank@UAM*/

    /* get the URNO of the record to be updated */
    memset( clSecUrno, 0, sizeof( clSecUrno ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "URNO" ) ) != NULL )
    {
        strcpy( clSecUrno, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error URNO not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* get the PASS of the record to be updated */
    memset( clPass, 0, sizeof( clPass ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "PASS" ) ) != NULL )
    {
        strcpy( clPass, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error PASS not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS && bpResetOldPasswords && bgPwdtabExists )
    {
        char clWhere[BIGBUF];
        sprintf( clWhere, "FSEC=%s", clSecUrno );
        ilRc = handleDelete( pcgPwdtab, clWhere, NULL, &rlUrnoList );

        if( ilRc != RC_SUCCESS && ilRc != ORA_NOT_FOUND )
        {
            sprintf( pcpErrorMess, "Error del from %s.", pcgPwdtab );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            ilRc = RC_SUCCESS;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        /* encrypt the password */
        memset( clEncryptedPass, 0, sizeof( clEncryptedPass ) );

        if( strlen( clPass ) <= 0 )
            strcpy( clPass, " " );

        /*  Check encryption method */
        if((egEncBy == E_ENCBY_CLIENT) || ((egEncBy != E_ENCBY_INVALID) && (egEncMethod == E_ENCMETHOD_NONE)))
        {
            /*  Client already encrypted the password, just use it */
            strncpy(clEncryptedPass, clPass, PASSLEN);
        }
        else
        {
            /* Client sent plain text password */
            string_to_key( clPass, clEncryptedPass );
        }

        if( !bpResetOldPasswords && bgPwdtabExists )
        {
            if( ( ilRc = CheckPasswordNotAlreadyUsed( clSecUrno, clEncryptedPass, pcpErrorMess ) ) != RC_SUCCESS )
            {
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
            }
        }
    }

    /* update SECTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( handleUpdate( pcgSectab, "PASS", clEncryptedPass, NULL, clSecUrno, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error updating SECTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else/*Frank@UAL_UAA*/
        {
            if( bgUpdateUALtab == TRUE )
            {
                /*Frank@UAM*/
                InsertUalTab( clSecUrno, pcpErrorMess, FALSE, RESETPASSWORD, clRmrk );
                /*Frank@UAM*/
                /*InsertUalTab(char *pcpUrno,char *pcpErrorMess, BOOL bpIsSystem, int ipIsActived, char *pcpRema);   */
            }
        }/*Frank@UAL_UAA*/
    }

    /* update PWDTAB */
    if( ilRc == RC_SUCCESS && !bpResetOldPasswords && bgPwdtabExists )
    {
        if( UpdatePasswordTable( clSecUrno, clEncryptedPass ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error updating PWDTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleSetPwd() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleSetRgt()                                          */
/*                                                                      */
/* Parameters:  pcpTable - either FKT or PRV table name.                */
/*              pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Sets the STAT field in either FKTTAB or PRVTAB.         */
/*              "URNO,STAT" one to many                                 */
/*                                                                      */
/* ******************************************************************** */
static int handleSetRgt( char *pcpTable, char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumData, ilRgtsLoop, ilNumFields;
    char clUrno[URNOLEN + 1], clStat[STATLEN + 1];
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;

    memset( clUrno, 0, sizeof( clUrno ) );

    /* init the list of urnos returned from handleUpdate() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* get the number of records to be updated */
    ilNumData = itemCount( pcpDataList );
    ilNumFields = itemCount( pcpFieldList );

    for( ilRgtsLoop = 1; ilRc == RC_SUCCESS && ilRgtsLoop <= ilNumData; ilRgtsLoop += ilNumFields )
    {
        memset( clUrno, 0, sizeof( clUrno ) );
        memset( clStat, 0, sizeof( clStat ) );

        strcpy( clUrno, sGet_item2( pcpDataList, ilRgtsLoop ) );
        strcpy( clStat, sGet_item2( pcpDataList, ilRgtsLoop + 1 ) );

        ilRc = handleUpdate( pcpTable, "STAT", clStat, NULL, clUrno, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            sprintf( pcpErrorMess, "Error updating %s.", pcpTable );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }

    }


    if( ilRc == RC_SUCCESS && strlen( clUrno ) > 0 )
    {
        if( !strcmp( pcpTable, pcgPrvtab ) )
        {
            char pclSqlBuf[2048], pclDataArea[2048];
            short slFuncCode = START;
            short slLocalCursor = 0;
            sprintf( pclSqlBuf, "SELECT FSEC FROM %s WHERE URNO='%s'", pcgPrvtab, clUrno );
            ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

            if( ilRc != DB_SUCCESS )
            {
                char clOraErr[MAXERRLEN];
                get_ora_err( ilRc, clOraErr );
                sprintf( pcpErrorMess, "Error: %s.\n%s", pclSqlBuf, clOraErr );
                PushError( &rgErrList, pcpErrorMess );
                ilRc = RC_FAIL;
            }
            else
            {
                char clFsec[URNOLEN + 1];
                get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clFsec );
                AddBroadcastLine( SBC_COMMAND, SETPRV_COMMAND, "", "FSEC", clFsec );
                ilRc = RC_SUCCESS;
            }

            close_my_cursor( &slLocalCursor );
            slLocalCursor = 0;
        }
        else
        {
            AddBroadcastLine( SBC_COMMAND, SETFKT_COMMAND, "", "", "" );
        }
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleSetRgt() */



/* ******************************************************************** */
/*                                                                      */
/* Function:    handleNewApp()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Create a new application --> inserts new application    */
/*              in SECTAB and CALTAB, creates a single record in        */
/*              FKTTAB for the button "ModuInit".                       */
/*              "USID,[NAME],TYPE,[LANG],[PASS],STAT,[REMA],VAFR,       */
/*               VATO,[FREQ]"                                           */
/*                                                                      */
/* ******************************************************************** */
static int handleNewApp( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumFields, ilNumData;
    int ilNumUrnos, ilNumCalFlds;
    char pclFieldList[BIGBUF], pclDataList[BIGBUF];
    char clSecUrno[URNOLEN + 1];
    int ilRc = RC_SUCCESS;


    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* caluclate how many URNOs are required:
        SECTAB.URNO * 1
        CALTAB.URNO * num cal recs
        FKTTAB.URNO * 1 ("ModuInit" button)
        All URNOs are selected together at the start coz GetNextVal()
        does a commit meaning that rollbacks could not be used */

    ilNumUrnos = 2; /* SECTAB.URNO + FKTTAB.URNO */

    /* work out how many cal values there are */
    ilNumFields = itemCount( pcpFieldList ); /* num flds in the field list */
    ilNumData = itemCount( pcpDataList ); /* num fields in the data list */

    /* check for FREQ field (optional field) */
    if( getItem( pcpFieldList, pcpDataList, "FREQ" ) != NULL )
        ilNumCalFlds = 3;
    else
        ilNumCalFlds = 2;

    ilNumUrnos += ( ilNumData - ( ilNumFields - ilNumCalFlds ) ) / ilNumCalFlds;


    dbg( DEBUG, "handleNewApp() Num URNOs required is <%d>", ilNumUrnos );

    /* read all the URNOs required */
    if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
    {
        strcpy( pcpErrorMess, "Error loading URNOs." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* create a record in SECTAB and get back the new SEC URNO */
    /* also creates 1 to n records in CALTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( handleCreateSecCal( pcpFieldList, pcpDataList, clSecUrno, &rgResultBuf ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a SECTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* create a minimal application in FKTTAB (Button "ModuInit") allowing
       the application configuration to be downloaded from the application
       itself when the "ModuInit" button is pressed */
    if( ilRc == RC_SUCCESS )
    {
        strcpy( pclFieldList, "SUBD,FUNC,TYPE,STAT" );
        sprintf( pclDataList, "%s,%s,B,1", pcgDefSubd, pcgDefFunc );

        if( handleCreateFkt( pclFieldList, pclDataList, clSecUrno, &rgResultBuf ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a FKTTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, NEWAPP_COMMAND, "", "FAPP", clSecUrno );
    }

    return ilRc;

} /* end handleNewApp() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    handleNewRel()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Creates new relations in GRPTAB and CALTAB.             */
/*              "FREL,FSEC,TYPE,STAT,VAFR,VATO,[FREQ]" one or many.     */
/*                                                                      */
/* ******************************************************************** */
static int handleNewRel( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    int ilNumFields, ilNumData;
    int ilNumUrnos;
    int ilRc = RC_SUCCESS;

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* caluclate how many URNOs are required:
        GRPTAB.URNO * num recs
        CALTAB.URNO * num recs
        All URNOs are selected together at start because GetNextVal()
        does a commit meaning that rollbacks could not be used */

    ilNumUrnos = 0;

    /* work out how many cal values there are */
    ilNumFields = itemCount( pcpFieldList ); /* num fields in the fld list */
    ilNumData = itemCount( pcpDataList ); /* num fields in the data list */
    ilNumUrnos += ( ilNumData / ilNumFields ) * 2;

    dbg( DEBUG, "handleNewRel() Num URNOs required is <%d>", ilNumUrnos );

    /* read all the URNOs required */
    if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
    {
        strcpy( pcpErrorMess, "Error loading URNOs." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* create a group and calendar records */
    if( ilRc == RC_SUCCESS )
    {
        VARBUF rlBcUrnoBuf;
        InitVarBuf( &rlBcUrnoBuf );
        ilRc = handleCreateGrpCal( pcpFieldList, pcpDataList, &rgResultBuf, &rlBcUrnoBuf );

        if( ilRc != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a GRPTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            /* send back a FSEC/FRELs for the GRPTAB records created */
            AddBroadcastLine( SBC_COMMAND, NEWREL_COMMAND, "", "FSEC", ( char* ) rlBcUrnoBuf.buf );
        }

        FreeVarBuf(&rlBcUrnoBuf);
    }


    return ilRc;

} /* end handleNewRel() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    handleDelRel()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Deletes relation records from GRPTAB and CALTAB.        */
/*              "URNO" one or many.                                     */
/*                                                                      */
/* ******************************************************************** */
static int handleDelRel( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    VARBUF rlUrnoList;
    char clUrno[URNOLEN + 1];
    int ilRc = RC_SUCCESS;
    int ilNumUrnos = itemCount( pcpDataList );
    int ilUrnoLoop;

    if( pcpFieldList != NULL )
    {
    }

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* initialize the urno list */
    InitVarBuf( &rlUrnoList );

    /* clear the error list */
    ClearError( &rgErrList );


    /* delete one or many relations */
    for( ilUrnoLoop = 1; ilRc == RC_SUCCESS && ilUrnoLoop <= ilNumUrnos; ilUrnoLoop++ )
    {
        memset( clUrno, 0, URNOLEN + 1 );
        strcpy( clUrno, sGet_item2( pcpDataList, ilUrnoLoop ) );
        dbg( DEBUG, "handleDelRel() Urno=<%s>", clUrno );

        /* delete any relations and associated cal records */
        if( handleDeleteRel( clUrno, NULL ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting relations." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, DELREL_COMMAND, "", "FGRP", pcpDataList );
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleDelRel() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleAddApp()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Adds an application to a profile.                       */
/*              "FSEC,FAPP"                                             */
/*                                                                      */
/* ******************************************************************** */
static int handleAddApp( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char *pclItem;
    BOOL blProfileExists;
    char clWhere[BIGBUF];
    char clFsec[URNOLEN + 1], clFapp[URNOLEN + 1];
    int ilCount, ilNumUrnos;
    int ilRc = RC_SUCCESS;


    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* caluclate how many URNOs are required:
        if profile doesn't already exist
            GRPTAB/CALTAB = 2
        One URNO for each record in FKTTAB for FAPP
        All URNOs are selected together at start because GetNextVal()
        does a commit meaning that rollbacks could not be used */


    /* check if the profile already exists in the GRPTAB --> */
    /* when a user or profile was created with the Profile-None */
    /* option then a record will not exist in GRPTAB */
    pclItem = getItem( pcpFieldList, pcpDataList, "FSEC" );

    if( pclItem != NULL )
    {
        strcpy( clFsec, pclItem );
        sprintf( clWhere, "FSEC=%s AND TYPE='P'", clFsec );
    }
    else
    {
        strcpy( pcpErrorMess, "Error FSEC not found in field list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    pclItem = getItem( pcpFieldList, pcpDataList, "FAPP" );

    if( pclItem != NULL )
    {
        strcpy( clFapp, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error FAPP not found in field list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS )
    {
        if( handleCount( pcgGrptab, clWhere, &ilCount ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error Check GRPTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            if( ilCount <= 0 )
            {
                blProfileExists = FALSE;
                ilNumUrnos = 2;
            }
            else
            {
                blProfileExists = TRUE;
                ilNumUrnos = 0;
            }
        }
    }

    /* count the number of recs in FKTTAB for the application specified */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s", clFapp );

        if( handleCount( pcgFkttab, NULL, &ilCount ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error count records in FKTTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            ilNumUrnos += ilCount;
            dbg( DEBUG, "handleAddApp() Num URNOs required is <%d>", ilNumUrnos );
        }
    }

    /* read all the URNOs required */
    if( ilRc == RC_SUCCESS )
    {
        if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* if the profile doesn't already exist then create a GRP/CAL */
    if( ilRc == RC_SUCCESS && blProfileExists == FALSE )
    {
        char pclFieldList[BIGBUF], pclDataList[BIGBUF];
        dbg( DEBUG, "Profile does not exists already, so creating it!" );

        /* create a group record for the personal profile, for
           personal profiles FREL and FSEC are identical, also
           creates related CAL records  */
        strcpy( pclFieldList, "FREL,FSEC,TYPE" );
        sprintf( pclDataList, "%s,%s,P", clFsec, clFsec );

        ilRc = handleCreateGrpCal( pclFieldList, pclDataList, &rgResultBuf, NULL );

        if( ilRc != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a GRPTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }


    /* copy records from FKTTAB to PRVTAB */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = handleCreatePrv( pcpFieldList, pcpDataList, &rgResultBuf );

        if( ilRc != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error creating a PRVTAB record." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        char pclData[50];
        sprintf( pclData, "%s,%s", clFsec, clFapp );
        AddBroadcastLine( SBC_COMMAND, ADDAPP_COMMAND, "", "FSEC,FAPP", pclData );
    }

    return ilRc;

} /* end handleAddApp() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleUpdApp()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Reinstalls an application for a profile.                */
/*              "FSEC,FAPP"                                             */
/*                                                                      */
/* ******************************************************************** */
static int handleUpdApp( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char *pclItem;
    char clWhere[BIGBUF];
    char clFsec[URNOLEN + 1], clFapp[URNOLEN + 1];
    int ilCount, ilNumUrnos;
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;


    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );

    /* initialize the urno list */
    InitVarBuf( &rlUrnoList );

    /* get FSEC and FAPP */
    pclItem = getItem( pcpFieldList, pcpDataList, "FSEC" );

    if( pclItem != NULL )
    {
        strcpy( clFsec, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error FSEC field not found in field list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    pclItem = getItem( pcpFieldList, pcpDataList, "FAPP" );

    if( pclItem != NULL )
    {
        strcpy( clFapp, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error FAPP field not found in field list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* caluclate how many URNOs are required: one for each rec for this
       application in FKTTAB */

    /* count number of records in FKTTAB for the application specified */
    ilNumUrnos = 0;

    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s AND SDAL<>'1'", clFapp );

        if( handleCount( pcgFkttab, clWhere, &ilCount ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error count records in FKTTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            ilNumUrnos += ilCount;
            dbg( DEBUG, "handleUpdApp() Num URNOs required is <%d>", ilNumUrnos );
        }
    }

    /* read all the URNOs required */
    if( ilRc == RC_SUCCESS )
    {
        if( LoadUrnoList( ilNumUrnos ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* load the FKTTAB into local memory */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s AND SDAL<>'1'", clFapp );
        ilRc = LoadFktTab( clWhere, &rgFktBuf );

        if( ilRc != RC_SUCCESS )
            PushError( &rgErrList, "handleUpdApp() Error loading FKTTAB." );
    }

    /* select FFKTs and STATs from PRVTAB for FSEC and FAPP
       and set STAT in the FKT map to the STAT in the profile */
    if( ilRc == RC_SUCCESS )
    {

        ilRc = MergeFktPrv( clFsec, clFapp, &rgFktBuf );

        if( ilRc != RC_SUCCESS )
            PushError( &rgErrList, "handleUpdApp() Error merging PRVTAB and FKTTAB." );
    }

    /* delete the profile for FSEC and FAPP */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FSEC=%s AND FAPP=%s", clFsec, clFapp );

        if( handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleUpdApp() Error deleting profile" );
            ilRc = RC_FAIL;
        }
    }

    /* loop through the FKT map inserting profile records */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = CreatePrvFromFkt( clFsec, clFapp, &rgFktBuf, &rgResultBuf );

        if( ilRc != RC_SUCCESS )
            PushError( &rgErrList, "handleUpdApp() Error inserting the new profile into PRVTAB." );
    }

    /* delete any FKTTAB recs that have SDAL='1' (old config) and are no longer referenced in PRVTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( ( ilRc = DeleteOldUnreferencedFktRecs( clFapp ) ) != RC_SUCCESS )
            PushError( &rgErrList, "handleUpdApp() Error Deleting Old Unreferenced FKT Recs." );
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, UPDAPP_COMMAND, "", "FSEC", clFsec );
    }

    FreeVarBuf(&rlUrnoList);
    return ilRc;

} /* end handleUpdApp() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    MergeFktPrv()                                           */
/*                                                                      */
/* Parameters:  pcpFsec - FSEC in PRVTAB.                               */
/*              pcpFapp - FAPP in PRVTAB.                               */
/*              prpFktBuf - contents of FKTTAB for FSEC and FAPP.       */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*                                                                      */
/* Description: prpFktBuf contains all records in FKTTAB for FSEC       */
/*              and FAPP. Read PRVTAB for FSEC and FAPP and set         */
/*              STAT in prpFktBuf so that status is that of the prof.   */
/*                                                                      */
/* ******************************************************************** */
static int MergeFktPrv( char *pcpFsec, char *pcpFapp, VARBUF *prpFktBuf )
{
    int ilRc = RC_SUCCESS;
    short slLocalCursor, slFuncCode;
    char clFfkt[URNOLEN + 1], clStat[STATLEN + 1];
    char pclSqlBuf[2048], pclDataArea[2048];

    /* read records from PRVTAB for FSEC and FAPP use the status
       field to update the status field in the loaded FKTTAB
       in this way the statuses of the original profile are not lost */

    /* prepare the SQL command to read the PRVTAB */
    sprintf( pclSqlBuf, "SELECT FFKT,STAT FROM %s WHERE FSEC=%s AND FAPP=%s", pcgPrvtab, pcpFsec, pcpFapp );
    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "MergeFktPrv()..\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;

    /* loop - read and process a line at a time */
    while( ilRc == DB_SUCCESS )
    {
        memset( clFfkt, 0, sizeof( clFfkt ) );
        memset( clStat, 0, sizeof( clStat ) );

        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {
            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clFfkt );
            get_fld( pclDataArea, FIELD_2, STR, STATLEN, clStat );
            UpdateStatInFkt( clFfkt, clStat, prpFktBuf );
        }

        slFuncCode = NEXT;

    } /* end while */

    close_my_cursor( &slLocalCursor );
    ilRc = RC_SUCCESS;

    return ilRc;

} /* end MergeFktPrv() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    UpdateStatInFkt()                                       */
/*                                                                      */
/* Parameters:  pcpFfkt - FFKT in PRVTAB (URNO in FKTTAB).              */
/*              pcpStat - STAT from PRVTAB.                             */
/*              prpFktBuf - contents of FKTTAB for FSEC and FAPP.       */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*                                                                      */
/* Description: prpFktBuf contains all records in FKTTAB for FSEC       */
/*              and FAPP. Search through this buffer for FFKT, if       */
/*              found, update STAT.                                     */
/*                                                                      */
/* ******************************************************************** */
void UpdateStatInFkt( char *pcpFfkt, char *pcpStat, VARBUF *prpFktBuf )
{

    BOOL blNotFound = TRUE;
    FKTREC *prlFktRec = ( FKTREC * ) prpFktBuf->buf;
    int ilNumFktRecs = prpFktBuf->numObjs;
    int ilFktLoop;

    for( ilFktLoop = 0; blNotFound && ilFktLoop < ilNumFktRecs; ilFktLoop++ )
    {

        if( !strcmp( prlFktRec[ilFktLoop].URNO, pcpFfkt ) )
        {

            /*dbg(DEBUG,"UpdteStatInFkt() FFKT %s found",pcpFfkt);*/
            strcpy( prlFktRec[ilFktLoop].STAT, pcpStat );
            blNotFound = FALSE;
        }
    }

} /* end UpdateStatInFkt() */

/* ******************************************************************** */
/*                                                                      */
/* Function:    CreatePrvFromFkt()                                      */
/*                                                                      */
/* Parameters:  pcpFsec - FSEC in PRVTAB.                               */
/*              pcpFapp - FAPP in PRVTAB.                               */
/*              prpFktBuf - contents of FKTTAB for FSEC and FAPP.       */
/*              prpResultBuf - records inserted to send back to client. */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*                                                                      */
/* Description: prpFktBuf contains all records in FKTTAB for FSEC       */
/*              and FAPP. Insert them into PRVTAB.                      */
/*                                                                      */
/* ******************************************************************** */
static int CreatePrvFromFkt( char *pcpFsec, char *pcpFapp, VARBUF *prpFktBuf, VARBUF *prpResultBuf )
{

    PRVREC rlPrvRec;
    FKTREC *prlFktRec = ( FKTREC * ) prpFktBuf->buf;
    int ilNumFktRecs = prpFktBuf->numObjs;
    int ilFktLoop;
    int ilRc = RC_SUCCESS;
    char clTmp[BIGBUF], clErr[BIGBUF];

    strcpy( rlPrvRec.FSEC, pcpFsec );
    strcpy( rlPrvRec.FAPP, pcpFapp );

    for( ilFktLoop = 0; ilRc == RC_SUCCESS && ilFktLoop < ilNumFktRecs; ilFktLoop++ )
    {
        strcpy( rlPrvRec.FFKT, prlFktRec[ilFktLoop].URNO );
        strcpy( rlPrvRec.STAT, prlFktRec[ilFktLoop].STAT );

        /* Get new URNO for the PRV rec */
        if( GetNextUrno( rlPrvRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList,
                       "CreatePrvFromFkt() Error getting new URNO" );
            ilRc = RC_FAIL;
        }

        /* insert the profile record */
        if( ilRc == RC_SUCCESS )
        {
            dbg( DEBUG, "URNO<%s> FSEC<%s> FFKT<%s> FAPP<%s> STAT<%s>", rlPrvRec.URNO, rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );
            ilRc = InsertPrvRec( &rlPrvRec );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList,
                           "CreatePrvFromFkt() Error insert PRV rec." );
            }
        }

        /* add the new PRV record to the res buff (sent to client) */
        if( ilRc == RC_SUCCESS )
        {
            sprintf( clTmp, "%s,%s,%s,%s,%s,%s", LenVarBuf( prpResultBuf ) > 0 ? "\nPRV" : "PRV",
                     rlPrvRec.URNO, rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );

            if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
            {
                sprintf( clErr, "CreatePrvFromFkt() Error cannot add the following string to the result buffer:\n%s", clTmp );
                PushError( &rgErrList, clErr );
                ilRc = RC_FAIL;
            }
        }
    }

    return ilRc;

} /* end CreatePrvFromFkt() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleDelSec()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Delete a record from SECTAB and any other records       */
/*              associated with it (CALTAB/GRPTAB/PRVTAB).              */
/*              "URNO"                                                  */
/*                                                                      */
/* ******************************************************************** */
static int handleDelSec( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char *pclItem;
    char clWhere[BIGBUF];
    VARBUF rlUrnoList;
    char clUrno[URNOLEN + 1];
    /*Frank@UAL_UAA*/
    char clUsid[USIDLEN + 1];
    /*Frank@UAL_UAA*/
    int ilRc = RC_SUCCESS;

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* initialize the urno list */
    InitVarBuf( &rlUrnoList );

    /* clear the error list */
    ClearError( &rgErrList );

    /* read the URNO */
    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "URNO" ) ) != NULL )
    {
        strcpy( clUrno, pclItem );
        dbg( DEBUG, "handleDelSec() URNO <%s>", clUrno );
    }
    else
    {
        strcpy( pcpErrorMess, "Error URNO field not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /*Frank@UAL_UAA*/
    if( bgUpdateUALtab || bgUpdateUAAtab )
    {
        /* read the USID */
        char pclSqlBuf[2048], pclDataArea[2048];
        short slLocalCursor;
        char clType[TYPELEN + 1];
        short slFuncCode;
        char clOraErr[250];

        memset( clUsid, 0, USIDLEN + 1 );
        memset( clType, 0, TYPELEN + 1 );
#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "SELECT USID,TYPE FROM %s WHERE URNO=:VUrno", pcgSectab );
#else
        sprintf(pclSqlBuf, "SELECT USID,TYPE FROM %s WHERE URNO=?", pcgSectab);
#endif

        CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
        sprintf( pclDataArea, "%s\n", clUrno );

        dbg( DEBUG, "UpdateUserAccountLogTable() <%s>", pclSqlBuf );
        nlton( pclDataArea );

        slLocalCursor = 0;
        ilRc = DB_SUCCESS;
        slFuncCode = START;

        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        /*sprintf(pclSqlBuf,"SELECT USID, TYPE FROM %s WHERE URNO=:VUrno",pcgSectab);
        CheckWhereClause(TRUE, pclSqlBuf, FALSE, TRUE, "\0");
        sprintf(pclDataArea,"%s\n",clUrno);

        sprintf(pclSqlBuf,"SELECT USID,TYPE FROM %s WHERE URNO=:VUrno",pcgSectab);
        CheckWhereClause(TRUE, pclSqlBuf, FALSE, TRUE, "\0");
        sprintf(pclDataArea,"%s\n",clUrno);

        dbg(DEBUG,"UpdateUserAccountLogTable() Selecting the USID,TYPE from SECTAB using given URNO<%s>",pclSqlBuf);
        nlton(pclDataArea);

        slLocalCursor = 0;
        ilRc = DB_SUCCESS;
        slFuncCode = START;

        ilRc = sql_if(slFuncCode,&slLocalCursor,pclSqlBuf,pclDataArea);*/

        if( ilRc == DB_SUCCESS )
        {
            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, USIDLEN, clUsid );
            get_fld( pclDataArea, FIELD_2, STR, TYPELEN, clType );
            dbg( DEBUG, "the usid is <%s>", clUsid );
            dbg( DEBUG, "the type is <%s>", clType );
        }
        else
        {
            get_ora_err( ilRc, clOraErr );
            sprintf( pcpErrorMess, "Error selecting record for SEC.URNO <%s>\n%s", clUrno, clOraErr );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
            return ilRc;
        }

        /*dbg(DEBUG,"1line<%5d>$$$$",__LINE__);  */

        close_my_cursor( &slLocalCursor );

        /*dbg(DEBUG,"2line<%5d>$$$$",__LINE__); */

        if( bgUpdateUALtab )
        {
            if( ilRc == RC_SUCCESS && ( !strcmp( clType, "U" ) ) )
            {
                /*delete the corresponding record in UALTAB using USID(SECTAB)*/
                if( handleDeleteUal( clUsid ) != RC_SUCCESS )
                {
                    strcpy( pcpErrorMess, "Error deleting from UALTAB." );
                    PushError( &rgErrList, pcpErrorMess );
                    ilRc = RC_FAIL;
                }
            }
        }

        /*dbg(DEBUG,"3line<%5d>$$$$",__LINE__); */

        if( bgUpdateUAAtab )
        {
            if( ilRc == RC_SUCCESS && ( !strcmp( clType, "G" ) ) )
            {
                dbg( DEBUG, "4line<%5d>$$$$", __LINE__ );

                /*delete the corresponding record in UAATAB using clUrno(SECTAB-URNO)*/
                if( handleDeleteUaa( clUrno ) != RC_SUCCESS )
                {
                    strcpy( pcpErrorMess, "Error deleting from UAATAB." );
                    PushError( &rgErrList, pcpErrorMess );
                    ilRc = RC_FAIL;
                }
            }
        }

        /*dbg(DEBUG,"5line<%5d>",__LINE__); */

    }

    /*Frank@UAL_UAA*/

    /* delete SEC/CAL combinations for the URNO specified */
    if( ilRc == RC_SUCCESS )
    {
        if( handleDeleteSecCal( clUrno ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting from SECTAB/CALTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any possible personal profile records */
    /* does not return an error if there are none   */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FSEC=%s", clUrno );

        if( handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting personal profile records." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any relations and associated cal records */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FREL=%s OR FSEC=%s", clUrno, clUrno );

        if( handleDeleteRel( NULL, clWhere ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting relations." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any save passwords from PWDTAB */
    if( ilRc == RC_SUCCESS && bgPwdtabExists )
    {
        sprintf( clWhere, "FSEC=%s", clUrno );

        if( handleDelete( pcgPwdtab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting old passwords." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any FKTTAB recs that have SDAL='1' (old config) and are no longer referenced in PRVTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( ( ilRc = DeleteOldUnreferencedFktRecs( "" ) ) != RC_SUCCESS )
            PushError( &rgErrList, "Error Deleting Old Unreferenced FKT Recs." );
    }

    FreeVarBuf( &rlUrnoList );


    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, DELSEC_COMMAND, "", "FSEC", clUrno );
    }

    return ilRc;

} /* end handleDelSec() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleDelApp()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Delete an application from SECTAB and any records       */
/*              associated with it (CALTAB/GRPTAB/PRVTAB/FKTTAB).       */
/*              "URNO"                                                  */
/*                                                                      */
/* ******************************************************************** */
static int handleDelApp( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char        *pclItem;
    char        clWhere[BIGBUF];
    VARBUF      rlUrnoList;
    char        clUrno[URNOLEN + 1];
    int         ilRc = RC_SUCCESS;

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* initialize the urno list */
    InitVarBuf( &rlUrnoList );

    /* clear the error list */
    ClearError( &rgErrList );

    /* read the URNO */
    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "URNO" ) ) != NULL )
    {
        strcpy( clUrno, pclItem );
        dbg( DEBUG, "handleDelApp() URNO <%s>", clUrno );
    }
    else
    {
        strcpy( pcpErrorMess, "Error URNO field not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* delete SEC/CAL combinations for the URNO specified */
    if( ilRc == RC_SUCCESS )
    {
        if( handleDeleteSecCal( clUrno ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting from SECTAB/CALTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any profile records defined for the application */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s", clUrno );

        if( handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting an application from PRVTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any function records defined for the application */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s", clUrno );

        if( handleDelete( pcgFkttab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting an application from FKTTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, DELAPP_COMMAND, "", "FAPP", clUrno );
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleDelApp() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleRemApp()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: Remove (deinstall) an application from a profile.       */
/*              "FSEC,FAPP"                                             */
/*                                                                      */
/* ******************************************************************** */
static int handleRemApp( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{
    char        *pclItem;
    char        clWhere[BIGBUF];
    VARBUF      rlUrnoList;
    char        clFsec[URNOLEN + 1], clFapp[URNOLEN + 1];
    int         ilRc = RC_SUCCESS;

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* initialize the urno list */
    InitVarBuf( &rlUrnoList );

    /* clear the error list */
    ClearError( &rgErrList );

    /* read the FSEC */
    if( ilRc == RC_SUCCESS )
    {
        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "FSEC" ) ) != NULL )
        {
            strcpy( clFsec, pclItem );
            dbg( DEBUG, "handleRemApp() FSEC <%s>", clFsec );
        }
        else
        {
            strcpy( pcpErrorMess, "Error FSEC field not found in the data list." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* read the FAPP */
    if( ilRc == RC_SUCCESS )
    {
        if( ( pclItem = getItem( pcpFieldList, pcpDataList, "FAPP" ) ) != NULL )
        {
            strcpy( clFapp, pclItem );
            dbg( DEBUG, "handleRemApp() FAPP <%s>", clFapp );
        }
        else
        {
            strcpy( pcpErrorMess, "Error FAPP field not found in the data list." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any profile records defined for the application */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FSEC=%s AND FAPP=%s", clFsec, clFapp );

        if( handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList ) != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error deleting an application from PRVTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* delete any FKTTAB recs that have SDAL='1' (old config) and are no longer referenced in PRVTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( ( ilRc = DeleteOldUnreferencedFktRecs( clFapp ) ) != RC_SUCCESS )
            PushError( &rgErrList, "Error Deleting Old Unreferenced FKT Recs." );
    }

    if( ilRc == RC_SUCCESS )
    {
        char pclData[50];
        sprintf( pclData, "%s,%s", clFsec, clFapp );
        AddBroadcastLine( SBC_COMMAND, REMAPP_COMMAND, "", "FSEC,FAPP", pclData );
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleRemApp() */


/* ******************************************************************** */
/*                                                                      */
/* Function:    handleModIni()                                          */
/*                                                                      */
/* Parameters:  pcpFieldList - list of field names.                     */
/*              pcpDataList - data for each field in pcpFieldList.      */
/*              pcpErrorMessage - string to receive error messages.     */
/*                                                                      */
/* Returns:     RC_SUCCESS or RC_FAIL:                                  */
/*              For RC_SUCCESS pcpErrorMess is NULL.                    */
/*              For RC_FAIL pcpErrorMess contains an error message.     */
/*                                                                      */
/* Description: This command is received when an application downloads  */
/*              its' configuration to be written to FKTTAB.             */
/*              "APPL,SUBD,FUNC,FUAL,TYPE,STAT" one to many.            */
/*                                                                      */
/* ******************************************************************** */
static int handleModIni( char *pcpFieldList, char *pcpDataList, char *pcpErrorMess )
{

    char *pclItem;
    char clWhere[BIGBUF];
    VARBUF rlUrnoList;
    char clAppl[USIDLEN + 1]; /* USID of the application */
    char clFapp[URNOLEN + 1];
    int ilDataCount, ilFieldCount, ilNumUrnos;
    int ilRc = RC_SUCCESS;


    /* enables broadcasting if SEND_REGISTER_BROADCAST_ONLY = YES */
    bgSmiCommandActive = TRUE;

    /* init the list of urnos returned from handleCollectUrnos() */
    InitVarBuf( &rlUrnoList );

    /* clear the result buffer - used to return data to the client */
    ClearVarBuf( &rgResultBuf );

    /* clear the error list */
    ClearError( &rgErrList );


    /* get the APPL (this is USID of the application in SECTAB) */
    memset( clAppl, 0, sizeof( clAppl ) );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "APPL" ) ) != NULL )
    {
        strcpy( clAppl, pclItem );
    }
    else
    {
        strcpy( pcpErrorMess, "Error APPL not found in the data list." );
        PushError( &rgErrList, pcpErrorMess );
        ilRc = RC_FAIL;
    }

    /* get the URNO (FAPP) of the application in SECTAB */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "USID='%s' AND TYPE='A'", clAppl );
        ilRc = handleCollectUrnos( pcgSectab, clWhere, &rlUrnoList );

        if( ilRc != RC_SUCCESS || rlUrnoList.numObjs != 1 )
        {
            sprintf( pcpErrorMess, "Error: the application (%s) not found in %s.",
                     clAppl, pcgSectab );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else
        {
            /* copy the URNO to FAPP */
            memset( clFapp, 0, sizeof( clFapp ) );
            strcpy( clFapp, ( char * ) rlUrnoList.buf );
        }
    }

    /* work how many urnos are needed for inserts */
    if( ilRc == RC_SUCCESS )
    {
        ilFieldCount = itemCount( pcpFieldList ) - 1; /* -1 for APPL field */
        ilDataCount = itemCount( pcpDataList ) - 1; /* -1 for APPL value */
        ilNumUrnos = ilDataCount / ilFieldCount;

        if( ilNumUrnos <= 0 )
        {
            strcpy( pcpErrorMess, "Count of data lines is zero." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
        else if( ( ilDataCount % ilFieldCount ) != 0 )
        {
            strcpy( pcpErrorMess, "An incomplete field list was received - probable extra comma in the field list." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {

        /* read all the URNOs required */
        ilRc = LoadUrnoList( ilNumUrnos );

        if( ilRc != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error loading URNOs." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* update FKTTAB -> insert new records, update existing records,
                        delete non-existant records */
    if( ilRc == RC_SUCCESS )
    {
        char *pclDataList, *pclFieldList;

        pclDataList = strchr( pcpDataList, ',' ) + 1; /*datalist without APPL*/
        pclFieldList = strchr( pcpFieldList, ',' ) + 1; /*fldlist withot APPL*/

        ilRc = handleCreateFkt( pclFieldList, pclDataList, clFapp, &rgResultBuf );

        if( ilRc != RC_SUCCESS )
        {
            strcpy( pcpErrorMess, "Error updating FKTTAB." );
            PushError( &rgErrList, pcpErrorMess );
            ilRc = RC_FAIL;
        }
    }

    /* deactivate the InitModu flag for the user that registered the application */
    if( ilRc == RC_SUCCESS )
    {
        if( ( ilRc = DeactivateInitModuFlag( clFapp, pcgUsername ) ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "Error deactivating the InitModu flag." );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {
        AddBroadcastLine( SBC_COMMAND, SMI_COMMAND, clAppl, "FAPP", clFapp );
    }

    FreeVarBuf(&rlUrnoList);
    bgSmiCommandActive = FALSE;
    return ilRc;

} /* end handleModIni() */


/*****************************************************************************/
/* DeleteOldUnreferencedFktRecs()                                            */
/*                                                                           */
/* pcpFapp -> Application URNO                                               */
/*                                                                           */
/* Delete any records in FKTTAB which are old (ie. SDAL='1' meaning that the */
/* record has been replaced when an application has been registered) and     */
/* are no longer reference by any FFKTs in PRVTAB.                           */
/*                                                                           */
/*****************************************************************************/
static int DeleteOldUnreferencedFktRecs( char *pcpFapp )
{
    FKTREC *prlFktRec;
    int ilFktLoop, ilNumFktRecs;
    int ilRc = RC_SUCCESS;
    VARBUF  rlUrnoList;
    char    clWhere[BIGBUF];

    InitVarBuf( &rlUrnoList );

    /* delete the unfound records from fkttab */
    if( strlen( pcpFapp ) > 0 )
    {
        /* for an application */
        sprintf( clWhere, "FAPP='%s' AND SDAL = '1' AND URNO NOT IN (SELECT DISTINCT(FFKT) FROM PRVTAB WHERE FAPP='%s')", pcpFapp, pcpFapp );
    }
    else
    {
        /* all old unrefenced records */
        sprintf( clWhere, "SDAL = '1' AND URNO NOT IN (SELECT DISTINCT(FFKT) FROM PRVTAB)", pcpFapp, pcpFapp );
    }

    dbg( DEBUG, "DeleteOldUnreferencedFktRecs DELETE FROM FKTTAB %", clWhere );

    if( ( ilRc = handleDelete( pcgFkttab, clWhere, NULL, &rlUrnoList ) ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "DeleteOldUnreferencedFktRecs() Error deleting from FKTTAB" );
        ilRc = RC_FAIL;
    }

    FreeVarBuf( &rlUrnoList );
    return ilRc;

} /* end DeleteOldUnreferencedFktRecs() */


/*-------------------------------------------------------------------------*/
/*  VARBUF ROUTINES                                                        */
/*-------------------------------------------------------------------------*/

/*********************************************************************/
/* InitVarBuf()                                                      */
/* Create the initial memory required by a buffer and clear it.      */
/* Call this after the VARBUF variable has been decalred and before  */
/* it is used.                                                       */
/* prpBuf - variable length buffer.                                  */
/* Returns RC_SUCCESS or RC_FAIL (memory alloc error).               */
/*********************************************************************/
static int InitVarBuf( VARBUF *prpBuf )
{

    int ilRc = RC_SUCCESS;

    prpBuf->buf = NULL;
    prpBuf->len = 0;
    prpBuf->used = 0;

    /* allocate initially a block of memory to the buffer */
    if( ( ilRc = SetSizeVarBuf( prpBuf, VARBUFBLOCK ) ) == RC_SUCCESS )

        /* clear the buffer and set prpBuf->used to zero */
        ClearVarBuf( prpBuf );

    return ilRc;

} /* end InitVarBuf() */

/*********************************************************************/
/* ClearVarBuf()                                                     */
/* Clear the contents of the buffer.                                 */
/* prpBuf - variable length buffer.                                  */
/* Returns: None.                                                    */
/*********************************************************************/
void ClearVarBuf( VARBUF *prpBuf )
{

    memset( prpBuf->buf, 0x00, prpBuf->len );
    prpBuf->used = 0;

} /* end ClearVarBuf() */

/*************************************************************************/
/* FreeVarBuf()                                                          */
/* Delete any memory allocated to the buffer, call this when your VARBUF */
/* variable goes out of scope or at program end for global VARBUFs.      */
/* prpBuf - variable length buffer.                                      */
/* Returns: None.                                                        */
/*************************************************************************/
void FreeVarBuf( VARBUF *prpBuf )
{
    if( prpBuf->buf != NULL )
        free( prpBuf->buf );

    prpBuf->used = 0;
    prpBuf->len = 0;

} /* end FreeVarBuf() */

/**************************************************************************/
/* SetSizeVarBuf()                                                        */
/* Given the size of buffer required make sure enough memory is allocated */
/* prpBuf - variable length buffer.                                       */
/* ipSize - required amount of memory in bytes.                           */
/* Returns RC_SUCCESS or RC_FAIL (memory alloc error).                    */
/**************************************************************************/
static int SetSizeVarBuf( VARBUF *prpBuf, int ipSize )
{
    int ilRc = RC_SUCCESS;
    int oldlen = prpBuf->len;   /* zdo */

    if( prpBuf->len < ipSize )
    {
        /* keep adding blocks of memory until there is enough */
        while( prpBuf->len < ipSize )
            prpBuf->len += VARBUFBLOCK;

        if( ( prpBuf->buf = realloc( prpBuf->buf, prpBuf->len ) ) == NULL )
        {
            dbg( TRACE,
                 "SetSizeVarBuf() Error realloc() to %d bytes.",
                 prpBuf->len );
            ilRc = RC_FAIL;
        }
        else
        {
            /* init newly added mem */
            memset(((char *)prpBuf->buf) + oldlen, 0, prpBuf->len - oldlen);
        }
    }

    return ilRc;

} /* end SetSizeVarBuf() */


/**************************************************************/
/* AddVarBuf()                                                */
/* Add pcpMem to the buffer, allocate more memory if required */
/* prpBuf - variable length buffer                            */
/* pcpMem - area of memory to concatenate with prpBuf->buf    */
/* ipMemLen - length in bytes of pcpMem                       */
/* Returns RC_SUCCESS or RC_FAIL (memory alloc error).        */
/**************************************************************/
static int AddVarBuf( VARBUF *prpBuf, void *pcpMem, int ipMemLen )
{

    int ilRc = RC_SUCCESS;
    int ilMemRequired = prpBuf->used + ipMemLen;

    /* allocate more memory if required */
    if( ilMemRequired > prpBuf->len )
        ilRc = SetSizeVarBuf( prpBuf, ilMemRequired );

    /* concatenate contents of buf with pcpMem */
    if( ilRc == RC_SUCCESS )
    {
        /* char* cast required for ptr arithmetic */
        memcpy( ( void * )( ( char * ) prpBuf->buf + prpBuf->used ),
                ( const void * ) pcpMem,
                ( size_t ) ipMemLen );

        /* update number of bytes used */
        prpBuf->used += ipMemLen;
    }

    return ilRc;

} /* end AddVarBuf() */

/**************************************************************/
/* LenVarBuf()                                                */
/* Returns the number of bytes used so far in the buffer.     */
/* prpBuf - variable length buffer                            */
/**************************************************************/
int LenVarBuf( VARBUF *prpVarBuf )
{
    if( prpVarBuf == NULL )
        return 0;

    return prpVarBuf->used;

} /* end LenVarBuf() */

/*-------------------------------------------------------------------------*/
/*  URNOLIST ROUTINES                                                      */
/*-------------------------------------------------------------------------*/

/**************************************************************/
/* LoadUrnoList()                                             */
/* Reads the required num of URNOs into the urno list.        */
/* ipNumUrnos - number of URNOs required.                     */
/**************************************************************/
static int LoadUrnoList( int ipNumUrnos )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor;

    rgUrnoList.curr = 0;
    rgUrnoList.num = ipNumUrnos;

    /* allocate enough memory to read the URNOs */
    /*london    ClearVarBuf(&rgUrnoList.varbuf);
        SetSizeVarBuf(&rgUrnoList.varbuf, (ipNumUrnos*(URNOLEN+1))); */


    /* get the URNOs from the DB */
    slLocalCursor = 0;

    /* london implementation
        if( GetNextVal((char *) rgUrnoList.varbuf.buf,ipNumUrnos) != RC_SUCCESS )
        {
            dbg(TRACE,"LoadUrnoList() Error GetNextVal()");
            rgUrnoList.num = 0;
            ilRc = RC_FAIL;
        }
    end london implementation */
    /* dallas implementation */
    if( GetNextValues( rgUrnoList.urno, ipNumUrnos ) != RC_SUCCESS )
    {
        dbg( TRACE, "LoadUrnoList() Error GetNextVal()" );
        rgUrnoList.num = 0;
        ilRc = RC_FAIL;
    }

    /* end dallas implementation */

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end LoadUrnoList() */

/**************************************************************/
/* GetNextUrno()                                              */
/* Returns the next URNO in the URNO list.                    */
/* pcpUrno - char buffer to receive the URNO.                 */
/**************************************************************/
static int GetNextUrno( char *pcpUrno )
{

    int ilRc = RC_SUCCESS;
    /* london implementation
        rgUrnoList.curr++;
        if( rgUrnoList.curr <= rgUrnoList.num )
            strcpy(pcpUrno,sGet_item2((char *) rgUrnoList.varbuf.buf,rgUrnoList.curr));
        else
            ilRc = RC_FAIL;
    end london implementation */

    /* dallas implementation */
    if( rgUrnoList.curr < rgUrnoList.num )
    {
        sprintf( pcpUrno, "%d", atoi( rgUrnoList.urno ) + rgUrnoList.curr );
        rgUrnoList.curr++;
    }
    else
    {
        dbg( TRACE, "GetNextUrno() Error exceeded num pre-loaded URNOs" );
        ilRc = RC_FAIL;
    }

    /*  dbg(DEBUG,"GetNextUrno() BaseURNO<%s> Cnt<%d> ReturnURNO<%s>",
                rgUrnoList.urno,rgUrnoList.curr,pcpUrno); */
    /* end dallas implentation */

    return ilRc;

} /* end GetNextUrno() */

/*-------------------------------------------------------------------------*/
/*  ERRBUF ROUTINES                                                        */
/*-------------------------------------------------------------------------*/

/**************************************************************/
/* ClearError()                                               */
/* Clear the error list - doen't actually clear the list,     */
/* but resets the current error position.                     */
/* prpErrBuf - error list.                                    */
/**************************************************************/
void ClearError( ERRBUF *prpErrBuf )
{

    prpErrBuf->curPos = -1;

} /* end ClearError() */

/**************************************************************/
/* PushError()                                                */
/* Adds an error to the error list.                           */
/* prpErrBuf - error list.                                    */
/* pcpNewError - error string to add to the error list.       */
/**************************************************************/
/* add an error to the error list */
void PushError( ERRBUF *prpErrBuf, char *pcpNewError )
{

    if( prpErrBuf->curPos < ( MAXERRORS - 1 ) )
        strcpy( prpErrBuf->line[++prpErrBuf->curPos], pcpNewError );

} /* end PushError() */

/**************************************************************/
/* PopError()                                                 */
/* Returns a pointer to the current error and moves the       */
/* pointer to the previous error in the list.                 */
/* prpErrorBuf - error list.                                  */
/**************************************************************/
/* return a pointer to last error and move current position to the prev error */
char *PopError( ERRBUF *prpErrBuf )
{

    char *pclThisErr;

    if( prpErrBuf->curPos >= 0 )
        pclThisErr = prpErrBuf->line[prpErrBuf->curPos--];
    else
        pclThisErr = NULL;

    return pclThisErr;

} /* end PopError() */

/**************************************************************/
/* LogErrors()                                                */
/* Loops through the error list, popping errors and writes    */
/* them to the error log.                                     */
/* prpErrBuf - error list.                                    */
/**************************************************************/
void LogErrors( ERRBUF *prpErrBuf )
{

    char *pclError;

    while( ( pclError = PopError( prpErrBuf ) ) != NULL )
        dbg( TRACE, pclError );

} /* end LogErrors() */

/**************************************************************/
/* GetNumErrors()                                             */
/* Returns the number of errors so far.                       */
/* prpErrBuf - error list.                                    */
/**************************************************************/
int GetNumErrors( ERRBUF *prpErrBuf )
{

    return prpErrBuf->curPos + 1;

} /* end GetNumErrors() */

/*-------------------------------------------------------------------------*/
/*  DATABASE ACCESS ROUTINES                                               */
/*-------------------------------------------------------------------------*/

/**************************************************************/
/* handleCount()                                              */
/* Counts the number of records in a table for the given      */
/* where conditions.                                          */
/* pcpTableName - DB table name.                              */
/* pcpWhere - where condition.                                */
/* pipCount - receives the record count.                      */
/**************************************************************/
static int
handleCount( char *pcpTableName, char *pcpWhere, int *pipCount )
{

    char        clOraErr[MAXERRLEN];
    short       slFuncCode, slLocalCursor;
    int         ilRc;
    char        pclSqlBuf[2048], pclDataArea[2048];

    *pipCount = 0;

    if( pcpWhere == NULL )
        sprintf( pclSqlBuf, "SELECT COUNT(*) FROM %s", pcpTableName );
    else
        sprintf( pclSqlBuf, "SELECT COUNT(*) FROM %s WHERE %s", pcpTableName, pcpWhere );

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "handleCount() ...\n<%s>", pclSqlBuf );
    slLocalCursor = 0;
    slFuncCode = START;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    switch( ilRc )
    {
        case DB_SUCCESS:
            *pipCount = atoi( pclDataArea );
            ilRc = RC_SUCCESS;
            break;

        case ORA_NOT_FOUND:
            *pipCount = 0;
            ilRc = RC_SUCCESS;
            break;

        default:
            get_ora_err( ilRc, clOraErr );
            PushError( &rgErrList, clOraErr );
            ilRc = RC_FAIL;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end handleCount() */

/**************************************************************/
/* handleCollectUrnos()                                       */
/* Creates a list of URNOs selected from a DB table for the   */
/* given where condition.                                     */
/* pcpTab - DB table name.                                    */
/* pcpWhere - where condition.                                */
/* prpUrnoList - Varibale length list to receive a comma      */
/*               seperated list of URNOs.                     */
/**************************************************************/
static int handleCollectUrnos( char *pcpTab, char *pcpWhere, VARBUF *prpUrnoList )
{

    int         ilRc;
    char        clTmp[BIGBUF];
    short       slLocalCursor, slFuncCode;
    char        clUrno[URNOLEN+1];
    char        pclSqlBuf[2048],pclDataArea[2048];


    prpUrnoList->numObjs = 0; /* zero the urno count */

    /* clear the buffer which receives records read */
    ClearVarBuf( prpUrnoList );

    /* prepare the SQL command */
    sprintf( pclSqlBuf, "SELECT URNO FROM %s WHERE %s", pcpTab, pcpWhere );

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "handleCollectUrnos() ..\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;


    /* loop - read and process a line at a time */
    while( ilRc == DB_SUCCESS )
    {

        memset( clUrno, 0, URNOLEN + 1 );

        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {

            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clUrno );

            if( prpUrnoList->numObjs <= 0 )
                strcpy( clTmp, clUrno );
            else
                sprintf( clTmp, ",%s", clUrno );

            ilRc = AddVarBuf( prpUrnoList, clTmp, strlen( clTmp ) );
            prpUrnoList->numObjs++; /* increment the record count */

            slFuncCode = NEXT;

        } /* end if */

    } /* while */

    dbg( DEBUG, "handleCollectUrnos() %d URNOs read urno list follows (first 500 bytes):\n<%.*s>", prpUrnoList->numObjs, 500, ( char * ) prpUrnoList->buf );

    close_my_cursor( &slLocalCursor );
    ilRc = RC_SUCCESS;

    return ilRc;

} /* end handleCollectUrnos() */

/**************************************************************************/
/* FormatFields()                                                         */
/*                                                                        */
/* ipAction: NO_DATA_AREA INSERT_ACTION or UPDATE_ACTION.                 */
/* prpFields: Variable length buffer to receive formatted field list.     */
/* pcpData: Buffer to receive formatted data.                             */
/* pcpFieldList: List of field names.                                     */
/* pcpDataList: List of data.                                             */
/*                                                                        */
/* Given the field list and data list format the field list in the        */
/* following way "USID,NAME,REMA" --> ":Va1,:Va2,:Va3"                    */
/* and write the data list to the DataArea.                               */
/*                                                                        */
/* Returns: None.                                                         */
/*                                                                        */
/**************************************************************************/
void FormatFields( int ipAction, VARBUF *prpFields, char *pcpDataArea, char *pcpFieldList, char *pcpDataList )
{

    char *pclItem;
    int ilFldLoop;
    int ilNumFlds = field_count( pcpFieldList );
    char clTmp[BIGBUF];

    if( ipAction != NO_DATA_AREA )
        pcpDataArea[0] = '\0';

    for( ilFldLoop = 0; ilFldLoop < ilNumFlds; ilFldLoop++ )
    {
        if( ipAction == INSERT_ACTION || ipAction == NO_DATA_AREA )
        {
            /* format for INSERT or SELECT */
            if( ilFldLoop > 0 )
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(clTmp, ",:Va%d", ilFldLoop + 1 );

#else
                sprintf(clTmp, ",?");
#endif

            else
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(clTmp, ":Va%d", ilFldLoop + 1 );

#else
                sprintf(clTmp, "?");
#endif

        }
        else
        {
            /* format for UPDATE */
            if( ilFldLoop > 0 )
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(clTmp, ",%s=:Va%d",
                         sGet_item2( pcpFieldList, ilFldLoop + 1 ),
                         ilFldLoop + 1 );

#else
                sprintf(clTmp, ",%s=?",
                        sGet_item2(pcpFieldList, ilFldLoop + 1));
#endif
            else
#ifdef CCS_DBMODE_EMBEDDED
                sprintf(clTmp, "%s=:Va%d",
                         sGet_item2( pcpFieldList, ilFldLoop + 1 ),
                         ilFldLoop + 1 );

#else
                sprintf(clTmp, "%s=?",
                        sGet_item2(pcpFieldList, ilFldLoop + 1));
#endif
        }

        AddVarBuf( prpFields, clTmp, strlen( clTmp ) );

        if( ipAction != NO_DATA_AREA )
        {
            pclItem = sGet_item2( pcpDataList, ilFldLoop + 1 );

            if( strlen( pclItem ) > 0 )
                sprintf( clTmp, "%s\n", pclItem );
            else
                strcpy( clTmp, " \n" );

            strcat( pcpDataArea, clTmp );
        }
    }


    /*
    dbg(DEBUG,"FormatFields() fieldList \"%s\" dataList \"%s\"",
        (char*) prpFields->buf,pcpDataArea);
     */

    if( ipAction != NO_DATA_AREA )
    {
        /* We Always can append the Value of HOPO */
        /* because it's only used when :VHOPO is set */
        sprintf( clTmp, "%s\n", pcgH3LC );
        strcat( pcpDataArea, clTmp );
        nlton( pcpDataArea );
    } /* end if */

} /* end FormatFields() */

static int AddBroadcastLine( char *pcpCommand, char *pcpTableName, char *pcpUrno, char *pcpFieldList, char *pcpDataList )
{
    int ilRc = RC_SUCCESS;

    if( !bgSendSmiBcOnly || bgSmiCommandActive )
    {
        if( strlen( pcpFieldList ) > FIELDLISTLEN || strlen( pcpDataList ) > DATALISTLEN )
        {
            dbg( TRACE, "AddBroadcastLine() parameter too long FieldList len %d max %d DataList len %d max %d", strlen( pcpFieldList ), strlen( pcpDataList ) );
            ilRc = RC_FAIL;
        }
        else
        {
            BROADCASTLINE rlBroadcastLine;
            strcpy( rlBroadcastLine.Command, pcpCommand );
            strcpy( rlBroadcastLine.TableName, pcpTableName );
            strcpy( rlBroadcastLine.Urno, pcpUrno );
            strcpy( rlBroadcastLine.FieldList, pcpFieldList );
            strcpy( rlBroadcastLine.DataList, pcpDataList );
            AddVarBuf( &rgBroadcastLines, &rlBroadcastLine, sizeof( BROADCASTLINE ) );
            rgBroadcastLines.numObjs++; /* count of records added */
            ilRc = RC_SUCCESS;
        }
    }

    return ilRc;
}

static int AddBroadcastLines( char *pcpCommand, char *pcpTableName, char *pcpUrnoList, char *pcpFieldList, char *pcpDataList )
{
    int ilRc = RC_FAIL;

    char clUrno[URNOLEN + 1];
    int i, ilNumUrnos = itemCount( pcpUrnoList );

    for( i = 0; i <= ilNumUrnos; i++ )
    {
        memset( clUrno, 0, sizeof( clUrno ) );
        strcpy( clUrno, sGet_item2( pcpUrnoList, i ) );
        ilRc = AddBroadcastLine( pcpCommand, pcpTableName, clUrno, pcpFieldList, pcpDataList );
    }

    return ilRc;
}

static int SendBroadcastLines()
{
    int ilRc = RC_SUCCESS;
    BROADCASTLINE *prlBcLine = ( BROADCASTLINE* ) rgBroadcastLines.buf;
    int ilNumBcLines = rgBroadcastLines.numObjs, ilBcLine;
    char clSelection[100], clTwEnd[100];
    char clWks[100], clUser[100];

    memset( clWks, 0, sizeof( clWks ) );

    if( strlen( pcgRecvName ) > 0 )
        strcpy( clWks, pcgRecvName );
    else
        strcpy( clWks, "authdl" );

    memset( clUser, 0, sizeof( clUser ) );

    if( strlen( pcgUsername ) > 0 )
        strcpy( clUser, pcgUsername );
    else
        strcpy( clUser, "SYSINI" );

    dbg( TRACE, "SendBroadcastLines() Send Broadcast ilNumBcLines = <%d>", ilNumBcLines );

    for( ilBcLine = 0; ilBcLine < ilNumBcLines; ilBcLine++ )
    {
        if( strlen( prlBcLine[ilBcLine].Command ) <= 0 || strlen( prlBcLine[ilBcLine].TableName ) <= 0 )
        {
            dbg( TRACE, "SendBroadcastLines() **ERROR** SendBroadcastLines() invalid data:\nCommand <%s> \nTableName <%s> \nSelection <%s> \nFieldList <%s> \nDataList <%s>",
                 prlBcLine[ilBcLine].Command, prlBcLine[ilBcLine].TableName, clSelection, prlBcLine[ilBcLine].FieldList, prlBcLine[ilBcLine].DataList );
        }
        else
        {
            sprintf( clSelection, "WHERE URNO='%s'", prlBcLine[ilBcLine].Urno );

            ilRc = tools_send_info_flag( igToBcHdl, 0, pcgDestName, "", pcgRecvName, "", "", pcgTwStart, pcgTwEnd,
                                         prlBcLine[ilBcLine].Command, prlBcLine[ilBcLine].TableName, clSelection,
                                         prlBcLine[ilBcLine].FieldList, prlBcLine[ilBcLine].DataList, igBcHdlPrio );

            /*
                sprintf(clTwEnd, "%s,%s,%s", pcgDefH3LC, pcgDefTblExt, (strlen(pcgSendingApplication) > 0) ? pcgSendingApplication : pcgProcName);
            ilRc = tools_send_info_flag(igToBcHdl, 0, clUser, "", clWks, "", "", "", clTwEnd,
                                         prlBcLine[ilBcLine].Command, prlBcLine[ilBcLine].TableName, clSelection,
                                         prlBcLine[ilBcLine].FieldList, prlBcLine[ilBcLine].DataList, igBcHdlPrio );
             */
            if( ilRc == RC_SUCCESS )
            {
                dbg( DEBUG, "SendBroadcastLines() Send Broadcast Success \npcgDestName <%s>\npcgRecvName <%s>\npcgTwStart <%s>\npcgTwEnd <%s>\nCommand <%s> \nTableName <%s> \nSelection <%s> \nFieldList <%s> \nDataList <%s>",
                     pcgDestName, pcgRecvName, pcgTwStart, pcgTwEnd, prlBcLine[ilBcLine].Command, prlBcLine[ilBcLine].TableName, clSelection, prlBcLine[ilBcLine].FieldList, prlBcLine[ilBcLine].DataList );
            }
            else
            {
                dbg( TRACE, "SendBroadcastLines() **ERROR** SendBroadcastLines() Send Broadcast Failed RC = <%d>", ilRc );
                dbg( TRACE, "SendBroadcastLines() \nCommand <%s> \nTableName <%s> \nSelection <%s> \nFieldList <%s> \nDataList <%s>",
                     prlBcLine[ilBcLine].Command, prlBcLine[ilBcLine].TableName, clSelection, prlBcLine[ilBcLine].FieldList, prlBcLine[ilBcLine].DataList );
                ilRc = RC_FAIL;
                break;
            }
        }
    }

    ClearVarBuf( &rgBroadcastLines );
    rgBroadcastLines.numObjs = 0;
    return ilRc;
}

static int CancelSendBroadcastLines()
{
    ClearVarBuf( &rgBroadcastLines );
    rgBroadcastLines.numObjs = 0;
}


/**************************************************************/
/* handleInsert()                                             */
/* Inserts a record into the database.                        */
/* pcpTableName - DB table name.                              */
/* pcpFieldList - list of comma seperated field names.        */
/* pcpDataList - list of comma seperated data.                */
/* pcpUrno     - URNO of new record, required for logging.    */
/**************************************************************/
static int handleInsert( char *pcpTableName, char *pcpFieldList, char *pcpDataList, char *pcpNewUrno )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor,slFuncCode;
    char        clErr[MAXERRLEN];
    VARBUF      rlFields;
    char        pclSqlBuf[2048],pclDataArea[2048];

    memset( pclSqlBuf, 0, sizeof( pclSqlBuf ) );
    memset( pclDataArea, 0, sizeof( pclDataArea ) );

    /* init a buffer to hold a list of fields format ":Val1,..,:Valn" */
    InitVarBuf( &rlFields );

    /* format field and data list for sql_if command */
    FormatFields( INSERT_ACTION, &rlFields, pclDataArea, pcpFieldList, pcpDataList );

    /* create the insert command */
    if( ( igAddHopo == TRUE ) && ( strstr( pcpFieldList, "HOPO" ) == NULL ) )
    {
#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,:VHOPO)", pcpTableName, pcpFieldList, ( char * ) rlFields.buf );
#else
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,'%s')", pcpTableName, pcpFieldList, (char *) rlFields.buf, pcgH3LC);
#endif

    }
    else
    {
        sprintf( pclSqlBuf, "INSERT INTO %s (%s) VALUES (%s)", pcpTableName, pcpFieldList, ( char * ) rlFields.buf );
    }

    dbg( DEBUG, "handleInsert()\n<%s>", pclSqlBuf );

    slFuncCode = START;
    slLocalCursor = 0;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS )
    {
        /* get the oracle error and add it to the error list */
        get_ora_err( ilRc, clErr );
        PushError( &rgErrList, clErr );
        sprintf( clErr, "handleInsert() SqlBuf:\n%s", pclSqlBuf );
        PushError( &rgErrList, clErr );
        sprintf( clErr, "handleInsert() DataArea:\n%s", pcpDataList );
        PushError( &rgErrList, clErr );
        ilRc = RC_FAIL;
    }
    else
    {
#ifdef LOGGING
        ( void ) ReleaseActionInfo( pcgOutRoute, pcpTableName, "IRT",
                                    pcpNewUrno, pcpNewUrno, pcpFieldList, pcpDataList );
#endif

        /* add lines to be broadcast */
        AddBroadcastLine( "IRT", pcpTableName, pcpNewUrno, pcpFieldList, pcpDataList );

        ilRc = RC_SUCCESS;
    }

    FreeVarBuf( &rlFields );
    close_my_cursor( &slLocalCursor );

    return ilRc;

} /* end handleInsert() */


/**************************************************************/
/* handleUpdate()                                             */
/* Updates a record into the database.                        */
/* pcpTableName - DB table name.                              */
/* pcpFieldList - list of comma seperated field names.        */
/* pcpDataList - list of comma seperated data.                */
/* pcpWhereList - where condition for the update.             */
/* pcpUrno - URNO for the update used instead of pcpWhereList.*/
/* prpUrnoList - list of URNOs that were updated - returned.  */
/**************************************************************/
static int handleUpdate( char *pcpTableName, char *pcpFieldList, char *pcpDataList, char *pcpWhereList, char *pcpUrno, VARBUF *prpUrnoList )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor,slFuncCode;
    char        clErr[MAXERRLEN];
    VARBUF      rlFields;
    char        pclSqlBuf[2048],pclDataArea[2048];

    memset( pclSqlBuf, 0, sizeof( pclSqlBuf ) );
    memset( pclDataArea, 0, sizeof( pclDataArea ) );

    /* init a buffer to hold a list of fields format ":Val1,..,:Valn" */
    InitVarBuf( &rlFields );

    /* buffer to hold urnos - only created if where is specified */
    ClearVarBuf( prpUrnoList );


    /* create the update command */
    if( pcpWhereList != NULL )
    {
        /* where condition was specified so get a list of urnos */
        /* of all records which will be updated - for history.  */
        handleCollectUrnos( pcpTableName, pcpWhereList, prpUrnoList );

        /* format field and data list for sql_if command */
        FormatFields( UPDATE_ACTION, &rlFields, pclDataArea, pcpFieldList, pcpDataList );

        sprintf( pclSqlBuf, "UPDATE %s SET %s WHERE %s", pcpTableName, ( char * ) rlFields.buf, pcpWhereList );

        /* add lines to be broadcast */
        AddBroadcastLines( "URT", pcpTableName, ( char * ) prpUrnoList->buf, pcpFieldList, pcpDataList );
    }
    else if( pcpUrno != NULL )
    {
        AddVarBuf( prpUrnoList, pcpUrno, strlen( pcpUrno ) );

        /* format field and data list for sql_if command */
        FormatFields( UPDATE_ACTION, &rlFields, pclDataArea, pcpFieldList, pcpDataList );

        sprintf( pclSqlBuf, "UPDATE %s SET %s WHERE URNO=%s", pcpTableName, ( char * ) rlFields.buf, pcpUrno );

        /* add lines to be broadcast */
        AddBroadcastLine( "URT", pcpTableName, pcpUrno, pcpFieldList, pcpDataList );
    }
    else
    {
        /* no where condition specified so get a list of urnos */
        /* of all records which will be updated - for history. */
        handleCollectUrnos( pcpTableName, pcpWhereList, prpUrnoList );

        /* format field and data list for sql_if command */
        FormatFields( UPDATE_ACTION, &rlFields, pclDataArea, pcpFieldList, pcpDataList );

        sprintf( pclSqlBuf, "UPDATE %s SET %s", pcpTableName, ( char * ) rlFields.buf );

        /* add lines to be broadcast */
        AddBroadcastLines( "URT", pcpTableName, ( char * ) prpUrnoList->buf, pcpFieldList, pcpDataList );
    }

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );

    dbg( DEBUG, "handleUpdate()\n%s", pclSqlBuf );

    slFuncCode = START;
    slLocalCursor = 0;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

    if( ilRc != DB_SUCCESS )
    {
        /* get the oracle error and add it to the error list */
        get_ora_err( ilRc, clErr );
        PushError( &rgErrList, clErr );
        sprintf( clErr, "handleUpdate() SqlBuf:\n%s", pclSqlBuf );
        PushError( &rgErrList, clErr );
        sprintf( clErr, "handleUpdate() DataArea:\n%s", pcpDataList );
        PushError( &rgErrList, clErr );
        ilRc = RC_FAIL;
    }
    else
    {
        char *pclUrnoList = ( char * ) prpUrnoList->buf;

#ifdef LOGGING
        ( void ) ReleaseActionInfo( pcgOutRoute, pcpTableName, "URT", pclUrnoList, pclUrnoList, pcpFieldList, pcpDataList );
#endif
        ilRc = RC_SUCCESS;
    }

    FreeVarBuf( &rlFields );
    close_my_cursor( &slLocalCursor );

    return ilRc;

} /* end handleUpdate() */

/**************************************************************/
/* handleDelete()                                             */
/* Deletes a record in the database.                          */
/* pcpTableName - DB table name.                              */
/* pcpWhereList - where condition for the update.             */
/* pcpUrno - URNO for the update used instead of pcpWhereList.*/
/* prpUrnoList - list of URNOs that were updated - returned.  */
/**************************************************************/
static int handleDelete( char *pcpTableName, char *pcpWhereList, char *pcpUrno, VARBUF *prpUrnoList )
{
    int         ilRc = RC_SUCCESS;
    short       slLocalCursor,slFuncCode;
    char        clErr[MAXERRLEN];
    char        pclSqlBuf[2048],pclDataArea[2048];
    BOOL        blManyUrnos = FALSE;

    memset( pclSqlBuf, 0, sizeof( pclSqlBuf ) );
    memset( pclDataArea, 0, sizeof( pclDataArea ) );


    /* buffer to hold urnos - only created if where is specified */
    ClearVarBuf( prpUrnoList );


    /* create the update command */
    if( pcpWhereList != NULL )
    {
        /* where condition was specified so get a list of urnos */
        /* of all records which will be deleted - for history.  */
        handleCollectUrnos( pcpTableName, pcpWhereList, prpUrnoList );
        sprintf( pclSqlBuf, "DELETE FROM %s WHERE %s", pcpTableName, pcpWhereList );
        blManyUrnos = TRUE;
    }
    else if( pcpUrno != NULL )
    {
        AddVarBuf( prpUrnoList, pcpUrno, strlen( pcpUrno ) );
        sprintf( pclSqlBuf, "DELETE FROM %s WHERE URNO=%s", pcpTableName, pcpUrno );
    }
    else
    {
        /* no where condition specified so get a list of urnos */
        /* of all records which will be deleted - for history. */
        handleCollectUrnos( pcpTableName, pcpWhereList, prpUrnoList );
        sprintf( pclSqlBuf, "DELETE FROM %s", pcpTableName );
        blManyUrnos = TRUE;
    }

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "handleDelete()\n%s", pclSqlBuf );

    slFuncCode = START;
    slLocalCursor = 0;

    ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );
    dbg( DEBUG, "handleDelete() rc = %d", ilRc );

    if( ilRc == ORA_NOT_FOUND )
    {
        dbg( DEBUG, "handleDelete() ORA_NOT_FOUND, continuing..." );
        ilRc = RC_SUCCESS;
    }
    else if( ilRc != DB_SUCCESS )
    {
        /* get the oracle error and add it to the error list */
        get_ora_err( ilRc, clErr );
        PushError( &rgErrList, clErr );
        sprintf( clErr, "handleDelete() SqlBuf:\n%s", pclSqlBuf );
        PushError( &rgErrList, clErr );
        ilRc = RC_FAIL;
    }
    else
    {
        char *pclUrnoList = ( char * ) prpUrnoList->buf;

#ifdef LOGGING
        ( void ) ReleaseActionInfo( pcgOutRoute, pcpTableName, "DRT", pclUrnoList, pclUrnoList, "", "" );
#endif

        if( blManyUrnos )
        {
            AddBroadcastLines( "DRT", pcpTableName, pclUrnoList, "", "" );
        }
        else
        {
            AddBroadcastLine( "DRT", pcpTableName, pcpUrno, "", "" );
        }

        ilRc = RC_SUCCESS;
    }

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end handleDelete() */


/**************************************************************************/
/* InsertSecRec()                                                         */
/*                                                                        */
/* prpSecRec - SECREC record.                                             */
/*                                                                        */
/* Copies the contents of SECREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertSecRec( SECREC *prpSecRec )
{
    char pclFieldList[500], pclDataList[500];
    char clEncryptedPass[PASSLEN + 1];

    /* encrypt the password */
    memset( clEncryptedPass, 0, sizeof( clEncryptedPass ) );

    if( strlen( prpSecRec->PASS ) <= 0 )
        strcpy( prpSecRec->PASS, " " );

    /* Check encryption method */
    if((egEncBy == E_ENCBY_CLIENT) || ((egEncBy != E_ENCBY_INVALID) && (egEncMethod == E_ENCMETHOD_NONE)))
    {
        /* Client already encrypted the password, just use it */
        strncpy(clEncryptedPass, prpSecRec->PASS, PASSLEN);
    }
    else
    {
        /* Client sent plain text password   */
        string_to_key( prpSecRec->PASS, clEncryptedPass );
    }

    strcpy( pclFieldList, "URNO,USID,TYPE,NAME,LANG,PASS,STAT,REMA,DEPT" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s,%s,%s,%s,%s",
             prpSecRec->URNO, prpSecRec->USID, prpSecRec->TYPE, prpSecRec->NAME,
             prpSecRec->LANG, clEncryptedPass, prpSecRec->STAT, prpSecRec->REMA,
             prpSecRec->DEPT );

    return handleInsert( pcgSectab, pclFieldList, pclDataList, prpSecRec->URNO );

} /* end InsertSecRec() */

/**************************************************************************/
/* InsertCalRec()                                                         */
/*                                                                        */
/* prpCalRec - CALREC record.                                             */
/*                                                                        */
/* Copies the contents of CALREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertCalRec( CALREC *prpCalRec )
{
    char pclFieldList[500], pclDataList[500];

    strcpy( pclFieldList, "URNO,FSEC,VAFR,VATO,FREQ" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s", prpCalRec->URNO, prpCalRec->FSEC,
             prpCalRec->VAFR, prpCalRec->VATO, prpCalRec->FREQ );

    return handleInsert( pcgCaltab, pclFieldList, pclDataList, prpCalRec->URNO );

} /* end InsertCalRec() */


/**************************************************************************/
/* InsertGrpRec()                                                         */
/*                                                                        */
/* prpGrpRec - GRPREC record.                                             */
/*                                                                        */
/* Copies the contents of GRPREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertGrpRec( GRPREC *prpGrpRec )
{
    char pclFieldList[500], pclDataList[500];

    strcpy( pclFieldList, "URNO,FREL,FSEC,TYPE,STAT" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s", prpGrpRec->URNO, prpGrpRec->FREL,
             prpGrpRec->FSEC, prpGrpRec->TYPE, prpGrpRec->STAT );

    return handleInsert( pcgGrptab, pclFieldList, pclDataList, prpGrpRec->URNO );

} /* end InsertGrpRec() */

/**************************************************************************/
/* InsertPrvRecs()                                                        */
/*                                                                        */
/* prpPrvRecs - VARBUF containing multiple PRV recs to be inserted.       */
/*                                                                        */
/* Carries out multiple insert of PRV recs with a transaction.            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertPrvRecs( VARBUF *prpPrvRecs )
{
    char pclFieldList[BIGBUF], pclDataList[BIGBUF];
    PRVREC *prlPrvRec = ( PRVREC* ) prpPrvRecs->buf;
    int ilNumPrvRecs = prpPrvRecs->numObjs;
    int ilRc = RC_SUCCESS;
    short slLocalCursor, slFuncCode;
    char clErr[MAXERRLEN];
    VARBUF rlFields;
    int ilRec;
    char pclSqlBuf[2048], pclDataArea[2048];

    strcpy( pclFieldList, "URNO,FSEC,FFKT,FAPP,STAT" );

    /* init a buffer to hold a list of fields format ":Val1,..,:Valn" */
    InitVarBuf( &rlFields );

    /* format field list for sql_if command */
    FormatFields( NO_DATA_AREA, &rlFields, NULL, pclFieldList, NULL );

    /* create the insert command */
    if( ( igAddHopo == TRUE ) &&
            ( strstr( pclFieldList, "HOPO" ) == NULL ) )
    {
#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,:VHOPO)", pcgPrvtab, pclFieldList, ( char * ) rlFields.buf );
#else
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,'%s')", pcgPrvtab, pclFieldList, (char *) rlFields.buf, pcgH3LC);
#endif
    } /* end if */
    else
    {
        sprintf( pclSqlBuf, "INSERT INTO %s (%s) VALUES (%s)", pcgPrvtab, pclFieldList, ( char * ) rlFields.buf );
    } /* end else */


    slFuncCode = START;
    slLocalCursor = 0;

    for( ilRec = 0; ilRc == RC_SUCCESS && ilRec < ilNumPrvRecs; ilRec++ )
    {
        pclDataArea[0] = 0x00;
        sprintf( pclDataArea, "%s\n%s\n%s\n%s\n%s\n%s\n",
                 prlPrvRec[ilRec].URNO, prlPrvRec[ilRec].FSEC,
                 prlPrvRec[ilRec].FFKT, prlPrvRec[ilRec].FAPP,
                 prlPrvRec[ilRec].STAT, pcgH3LC );
        nlton( pclDataArea );

        /* dataList for logging only */
        sprintf( pclDataList, "%s,%s,%s,%s,%s,%s",
                 prlPrvRec[ilRec].URNO, prlPrvRec[ilRec].FSEC,
                 prlPrvRec[ilRec].FFKT, prlPrvRec[ilRec].FAPP,
                 prlPrvRec[ilRec].STAT, pcgH3LC );

        /*      dbg(DEBUG,
                    "InsertPrvRecs() inserting: fieldList <%s>  dataList <%s>",
                    pclFieldList,pclDataList);
         */

        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc != DB_SUCCESS )
        {
            /* get the oracle error and add it to the error list */
            get_ora_err( ilRc, clErr );
            PushError( &rgErrList, clErr );
            sprintf( clErr, "InsertPrvRecs() pclSqlBuf:\n%s", pclSqlBuf );
            PushError( &rgErrList, clErr );
            sprintf( clErr, "InsertPrvRecs() pclDataArea:\n%s", pclFieldList );
            PushError( &rgErrList, clErr );
            ilRc = RC_FAIL;
        }
        else
        {
#ifdef LOGGING
            ( void ) ReleaseActionInfo( pcgOutRoute, pcgPrvtab, "IRT",
                                        prlPrvRec[ilRec].URNO,
                                        prlPrvRec[ilRec].URNO,
                                        pclFieldList,
                                        pclDataList );
#endif

            /* add lines to be broadcast */
            AddBroadcastLine( "IRT", pcgPrvtab, prlPrvRec[ilRec].URNO, pclFieldList, pclDataList );

            ilRc = RC_SUCCESS;
        }
    }

    FreeVarBuf( &rlFields );
    close_my_cursor( &slLocalCursor );

    return ilRc;

} /* end InsertPrvRecs() */

/**************************************************************************/
/* InsertPrvRec()                                                         */
/*                                                                        */
/* prpPrvRec - PRVREC record.                                             */
/*                                                                        */
/* Copies the contents of PRVREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertPrvRec( PRVREC *prpPrvRec )
{
    char pclFieldList[500], pclDataList[500];

    strcpy( pclFieldList, "URNO,FSEC,FFKT,FAPP,STAT" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s", prpPrvRec->URNO, prpPrvRec->FSEC,
             prpPrvRec->FFKT, prpPrvRec->FAPP, prpPrvRec->STAT );

    return handleInsert( pcgPrvtab, pclFieldList, pclDataList, prpPrvRec->URNO );

} /* end InsertPrvRec() */

/**************************************************************************/
/* InsertFktRecs()                                                        */
/*                                                                        */
/* prpFktRecs - VARBUF containing multiple PRV recs to be inserted.       */
/*                                                                        */
/* Carries out multiple insert of FKT recs with a transaction.            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertFktRecs( VARBUF *prpFktRecs )
{

    char pclFieldList[BIGBUF], pclDataList[BIGBUF];
    FKTREC *prlFktRec = ( FKTREC* ) prpFktRecs->buf;
    int ilNumFktRecs = prpFktRecs->numObjs;
    int ilRc = RC_SUCCESS;
    short slLocalCursor, slFuncCode;
    char clErr[MAXERRLEN];
    VARBUF rlFields;
    int ilRec;
    char pclSqlBuf[2048], pclDataArea[2048];

    strcpy( pclFieldList, "URNO,FAPP,SUBD,SDAL,FUNC,FUAL,TYPE,STAT" );

    /* init a buffer to hold a list of fields format ":Val1,..,:Valn" */
    InitVarBuf( &rlFields );

    /* format field list for sql_if command */
    FormatFields( NO_DATA_AREA, &rlFields, NULL, pclFieldList, NULL );

    /* create the insert command */
    if( ( igAddHopo == TRUE ) &&
            ( strstr( pclFieldList, "HOPO" ) == NULL ) )
    {

#ifdef CCS_DBMODE_EMBEDDED
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,:VHOPO)",
                pcgFkttab, pclFieldList, ( char * ) rlFields.buf );
#else
        sprintf(pclSqlBuf, "INSERT INTO %s (%s,HOPO) VALUES (%s,'%s')", pcgFkttab, pclFieldList, (char *) rlFields.buf, pcgH3LC);

#endif
    } /* end if */
    else
    {
        sprintf( pclSqlBuf, "INSERT INTO %s (%s) VALUES (%s)",
                 pcgFkttab, pclFieldList, ( char * ) rlFields.buf );
    } /* end else */


    slFuncCode = START;
    slLocalCursor = 0;

    for( ilRec = 0; ilRc == RC_SUCCESS && ilRec < ilNumFktRecs; ilRec++ )
    {
        pclDataArea[0] = 0x00;
        sprintf( pclDataArea, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
                 prlFktRec[ilRec].URNO, prlFktRec[ilRec].FAPP,
                 prlFktRec[ilRec].SUBD, prlFktRec[ilRec].SDAL,
                 prlFktRec[ilRec].FUNC, prlFktRec[ilRec].FUAL,
                 prlFktRec[ilRec].TYPE, prlFktRec[ilRec].STAT, pcgH3LC );
        nlton( pclDataArea );

        /* dataList - used for logging and error log */
        sprintf( pclDataList, "%s,%s,%s,%s,%s,%s,%s,%s,%s",
                 prlFktRec[ilRec].URNO, prlFktRec[ilRec].FAPP,
                 prlFktRec[ilRec].SUBD, prlFktRec[ilRec].SDAL,
                 prlFktRec[ilRec].FUNC, prlFktRec[ilRec].FUAL,
                 prlFktRec[ilRec].TYPE, prlFktRec[ilRec].STAT, pcgH3LC );

        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc != DB_SUCCESS )
        {
            /* get the oracle error and add it to the error list */
            get_ora_err( ilRc, clErr );
            PushError( &rgErrList, clErr );
            sprintf( clErr, "InsertFktRecs() pclSqlBuf:\n%s",
                     pclSqlBuf );
            PushError( &rgErrList, clErr );
            sprintf( clErr, "InsertFktRecs() pclDataArea:\n%s",
                     pclDataList );
            PushError( &rgErrList, clErr );
            ilRc = RC_FAIL;
        }
        else
        {
#ifdef LOGGING
            ( void ) ReleaseActionInfo( pcgOutRoute, pcgFkttab, "IRT",
                                        prlFktRec[ilRec].URNO,
                                        prlFktRec[ilRec].URNO,
                                        pclFieldList, pclDataList );
#endif

            /* add lines to be broadcast */
            AddBroadcastLine( "IRT", pcgFkttab, prlFktRec[ilRec].URNO, pclFieldList, pclDataList );

            ilRc = RC_SUCCESS;
        }
    }

    FreeVarBuf( &rlFields );
    close_my_cursor( &slLocalCursor );

    return ilRc;

} /* end InsertFktRecs() */


/**************************************************************************/
/* InsertFktRec()                                                         */
/*                                                                        */
/* prpFktRec - FKTREC record.                                             */
/*                                                                        */
/* Copies the contents of FKTREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertFktRec( FKTREC *prpFktRec )
{
    char pclFieldList[500], pclDataList[500];

    strcpy( pclFieldList, "URNO,FAPP,SUBD,SDAL,FUNC,FUAL,TYPE,STAT" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s,%s,%s,%s",
             prpFktRec->URNO, prpFktRec->FAPP, prpFktRec->SUBD,
             prpFktRec->SDAL, prpFktRec->FUNC, prpFktRec->FUAL,
             prpFktRec->TYPE, prpFktRec->STAT );

    return handleInsert( pcgFkttab, pclFieldList, pclDataList, prpFktRec->URNO );

} /* end InsertFktRec() */


/**************************************************************************/
/* InsertPwdRec()                                                         */
/*                                                                        */
/* prpPwdRec - PWDREC record.                                             */
/*                                                                        */
/* Copies the contents of PWDREC to field and data lists and calls        */
/* handleInsert().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int InsertPwdRec( PWDREC *prpPwdRec )
{

    char pclFieldList[500], pclDataList[500];

    if( strlen( prpPwdRec->CDAT ) <= 0 )
    {
        GetCurrDate();
        strcpy( prpPwdRec->CDAT, pcgCurrDate );
    }

    strcpy( pclFieldList, "URNO,FSEC,PASS,CDAT" );
    sprintf( pclDataList, "%s,%s,%s,%s", prpPwdRec->URNO, prpPwdRec->FSEC,
             prpPwdRec->PASS, prpPwdRec->CDAT );

    return handleInsert( pcgPwdtab, pclFieldList, pclDataList, prpPwdRec->URNO );

} /* end InsertPwdRec() */

/**************************************************************************/
/* UpdateFktRec()                                                         */
/*                                                                        */
/* prpFktRec - FKTREC record.                                             */
/*                                                                        */
/* Copies the contents of FKTREC to field and data lists and calls        */
/* handleUpdate().                                                        */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int UpdateFktRec( FKTREC *prpFktRec )
{

    char pclFieldList[500], pclDataList[500];
    int ilRc = RC_SUCCESS;
    VARBUF rlUrnoList;

    InitVarBuf( &rlUrnoList );

    strcpy( pclFieldList, "FAPP,SUBD,SDAL,FUNC,FUAL,TYPE,STAT" );
    sprintf( pclDataList, "%s,%s,%s,%s,%s,%s,%s",
             prpFktRec->FAPP, prpFktRec->SUBD, prpFktRec->SDAL, prpFktRec->FUNC,
             prpFktRec->FUAL, prpFktRec->TYPE, prpFktRec->STAT );

    ilRc = handleUpdate( pcgFkttab, pclFieldList, pclDataList, NULL, prpFktRec->URNO, &rlUrnoList );

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end UpdateFktRec() */


/**************************************************************************/
/* LoadFktTab()                                                           */
/*                                                                        */
/* pcpWhere  - load only the records specified (eg WHERE FAPP=URNO).      */
/* prpFktRecs - List containing FKTTAB records loaded by this function.   */
/*                                                                        */
/* Loads records from FKTTAB into prpFktRecs. If pcpWhere is NULL, the    */
/* whole table is loaded else the required records are loaded.            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int LoadFktTab( char *pcpWhere, VARBUF *prpFktRecs )
{

    int         ilRc;
    short       slLocalCursor, slFuncCode;
    FKTREC      rlFktRec;
    char        pclSqlBuf[2048],pclDataArea[2048];


    prpFktRecs->numObjs = 0; /* zero the record count */

    /* clear the buffer which receives records read */
    ClearVarBuf( prpFktRecs );

    /* prepare the SQL command to read the FKTTAB */
    if( pcpWhere == NULL )
        sprintf( pclSqlBuf, "SELECT URNO,FAPP,SUBD,SDAL,FUNC,FUAL,TYPE,STAT FROM %s", pcgFkttab );
    else
        sprintf( pclSqlBuf, "SELECT URNO,FAPP,SUBD,SDAL,FUNC,FUAL,TYPE,STAT FROM %s WHERE %s", pcgFkttab, pcpWhere );

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "LoadFktTab()..\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;


    /* loop - read and process a line at a time */
    while( ilRc == DB_SUCCESS )
    {
        memset( &rlFktRec, 0, sizeof( FKTREC ) );

        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {
            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, rlFktRec.URNO );
            get_fld( pclDataArea, FIELD_2, STR, URNOLEN, rlFktRec.FAPP );
            get_fld( pclDataArea, FIELD_3, STR, SUBDLEN, rlFktRec.SUBD );
            get_fld( pclDataArea, FIELD_4, STR, SDALLEN, rlFktRec.SDAL );
            get_fld( pclDataArea, FIELD_5, STR, FUNCLEN, rlFktRec.FUNC );
            get_fld( pclDataArea, FIELD_6, STR, FUALLEN, rlFktRec.FUAL );
            get_fld( pclDataArea, FIELD_7, STR, TYPELEN, rlFktRec.TYPE );
            get_fld( pclDataArea, FIELD_8, STR, STATLEN, rlFktRec.STAT );

            /* set flag - to check new FKT config against existing*/
            rlFktRec.found = FALSE;

            AddVarBuf( prpFktRecs, &rlFktRec, sizeof( FKTREC ) );
            slFuncCode = NEXT;

            prpFktRecs->numObjs++; /* increment the record count */

        } /* end if */

    } /* while */

    /*
        if( ilRc != ORA_NOT_FOUND ) {
            get_ora_err(ilRc,clOraErr);
            PushError(&rgErrList,clOraErr);
            ilRc = RC_FAIL;
        }
        else {
     */
    dbg( DEBUG, "LoadFktTab() %d records read from %s.",
         prpFktRecs->numObjs, pcgFkttab );
    ilRc = RC_SUCCESS;
    /*
        }
     */

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end LoadFktTab() */



/*****************************************************************************/
/* FktRecExists()                                                            */
/*                                                                           */
/* prpFktRecs -> list of records read from FKTTAB                            */
/* pcpFapp -> search for this field in prpFktRecs                            */
/* pcpFunc -> search for this field in prpFktRecs                            */
/* pcpFfkt -> URNO of the found FKT record                                   */
/*                                                                           */
/* This functions checks if FAPP and FUNC already exist in FKTTAB (loaded    */
/* into prpFktRecs), if found then TRUE is returned and the URNO of the      */
/* record found is copied to pcpFfkt, else FALSE is returned.                */
/*****************************************************************************/
BOOL FktRecExists( char *pcpFapp, char *pcpFunc, char *pcpFfkt, VARBUF *prpFktRecs )
{
    FKTREC *prlFktRec;
    int ilFktLoop, ilNumFktRecs;
    BOOL blFound = FALSE;

    /* point to the list of FKT records */
    prlFktRec = ( FKTREC * ) prpFktRecs->buf;
    ilNumFktRecs = prpFktRecs->numObjs;

    for( ilFktLoop = 0; blFound == FALSE && ilFktLoop < ilNumFktRecs; ilFktLoop++ )
    {
        if( strcmp( prlFktRec[ilFktLoop].FAPP, pcpFapp ) == 0 && strcmp( prlFktRec[ilFktLoop].FUNC, pcpFunc ) == 0 )
        {
            blFound = TRUE;
            prlFktRec[ilFktLoop].found = TRUE; /* recs not TRUE are deleted */
            strcpy( pcpFfkt, prlFktRec[ilFktLoop].URNO );
        }
    }

    return blFound;

} /* end FktRecExists() */

/*****************************************************************************/
/* GetFktRec()                                                               */
/*                                                                           */
/* prpFktRecs -> list of records read from FKTTAB                            */
/* pcpFapp -> search for this field in prpFktRecs                            */
/* pcpFunc -> search for this field in prpFktRecs                            */
/*                                                                           */
/* This functions checks if FAPP and FUNC already exist in FKTTAB (loaded    */
/* into prpFktRecs), if found then TRUE is returned and the URNO of the      */
/* record found is copied to pcpFfkt, else FALSE is returned.                */
/*****************************************************************************/
FKTREC *GetFktRec( char *pcpFapp, char *pcpFunc, VARBUF *prpFktRecs )
{
    FKTREC *prlFktRec, *prlRecFound = NULL;
    int ilFktLoop, ilNumFktRecs;

    /* point to the list of FKT records */
    prlFktRec = ( FKTREC * ) prpFktRecs->buf;
    ilNumFktRecs = prpFktRecs->numObjs;

    for( ilFktLoop = 0; ilFktLoop < ilNumFktRecs; ilFktLoop++ )
    {
        if( !strcmp( prlFktRec[ilFktLoop].FAPP, pcpFapp ) && !strcmp( prlFktRec[ilFktLoop].FUNC, pcpFunc ) )
        {
            prlRecFound = &prlFktRec[ilFktLoop];
            break;
        }
    }

    return prlRecFound;

} /* end GetFktRec() */


/*****************************************************************************/
/* DeleteUnfoundFktRecs()                                                    */
/*                                                                           */
/* prpFktRecs -> list of records read from FKTTAB                            */
/*                                                                           */
/* Loops through a list of FKT records if the flag 'found' is FALSE the      */
/* record is deleted from FKTTAB.                                            */
/*****************************************************************************/
static int DeleteUnfoundFktRecs( VARBUF *prpFktRecs )
{

    FKTREC *prlFktRec;
    int ilFktLoop, ilNumFktRecs;
    int ilRc = RC_SUCCESS;
    VARBUF  rlUrnoList;
    char    clWhere[BIGBUF];

    InitVarBuf( &rlUrnoList );

    /* point to the list of FKT records */
    prlFktRec = ( FKTREC * ) prpFktRecs->buf;
    ilNumFktRecs = prpFktRecs->numObjs;


    for( ilFktLoop = 0; ilRc == RC_SUCCESS && ilFktLoop < ilNumFktRecs; ilFktLoop++ )
    {
        if( prlFktRec[ilFktLoop].found == FALSE )
        {
            dbg( DEBUG, "DeleteUnfoundFktRecs() Delete SUBD %s FUNC %s", prlFktRec[ilFktLoop].SUBD, prlFktRec[ilFktLoop].FUNC );

            /* delete the unfound records from fkttab */
            ilRc = handleDelete( pcgFkttab, NULL, prlFktRec[ilFktLoop].URNO, &rlUrnoList );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList, "DeleteUnfoundFktRecs() Error deleting from FKTTAB" );
                ilRc = RC_FAIL;
            }

            /* delete any profile records with the above FFKT */
            sprintf( clWhere, "FFKT=%s", prlFktRec[ilFktLoop].URNO );
            ilRc = handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList, "DeleteUnfoundFktRecs() Error deleting from PRVTAB" );
                ilRc = RC_FAIL;
            }
        }
    }

    FreeVarBuf( &rlUrnoList );
    return ilRc;

} /* end DeleteUnfoundFktRecs() */



/**************************************************************************/
/* LoadPrvTab()                                                           */
/*                                                                        */
/* pcpWhere  - load only the records specified (eg WHERE FAPP=URNO).      */
/* prpPrvRecs - List containing PRVTAB records loaded by this function.   */
/*                                                                        */
/* Loads records from PRVTAB into prpPrvRecs. If pcpWhere is NULL, the    */
/* whole table is loaded else the required records are loaded.            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int LoadPrvTab( char *pcpWhere, VARBUF *prpPrvRecs )
{
    int         ilRc;
    short       slLocalCursor, slFuncCode;
    PRVREC      rlPrvRec;
    char        pclSqlBuf[2048],pclDataArea[2048];

    prpPrvRecs->numObjs = 0; /* zero the record count */

    /* clear the buffer which receives records read */
    ClearVarBuf( prpPrvRecs );

    /* prepare the SQL command to read the PRVTAB */
    if( pcpWhere == NULL )
    {
        sprintf( pclSqlBuf, "SELECT URNO,FSEC,FFKT,FAPP,STAT FROM %s", pcgPrvtab );
    }
    else
    {
        sprintf( pclSqlBuf, "SELECT URNO,FSEC,FFKT,FAPP,STAT FROM %s WHERE %s", pcgPrvtab, pcpWhere );
    }

    CheckWhereClause( TRUE, pclSqlBuf, FALSE, TRUE, "\0" );
    dbg( DEBUG, "LoadPrvTab()..\n<%s>", pclSqlBuf );

    slLocalCursor = 0;
    ilRc = DB_SUCCESS;
    slFuncCode = START;


    /* loop - read and process a line at a time */
    while( ilRc == DB_SUCCESS )
    {
        memset( &rlPrvRec, 0, sizeof( PRVREC ) );

        /* read a line from the DB */
        ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea );

        if( ilRc == DB_SUCCESS )
        {
            /* process the fields in the line just read */
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, rlPrvRec.URNO );
            get_fld( pclDataArea, FIELD_2, STR, URNOLEN, rlPrvRec.FSEC );
            get_fld( pclDataArea, FIELD_3, STR, URNOLEN, rlPrvRec.FFKT );
            get_fld( pclDataArea, FIELD_4, STR, URNOLEN, rlPrvRec.FAPP );
            get_fld( pclDataArea, FIELD_5, STR, STATLEN, rlPrvRec.STAT );

            AddVarBuf( prpPrvRecs, &rlPrvRec, sizeof( PRVREC ) );
            slFuncCode = NEXT;

            prpPrvRecs->numObjs++; /* increment the record count */

        } /* end if */

    } /* while */

    /*
        if( ilRc != ORA_NOT_FOUND ) {
            get_ora_err(ilRc,clOraErr);
            PushError(&rgErrList,clOraErr);
            ilRc = RC_FAIL;
        }
        else {
     */
    dbg( DEBUG, "LoadPrvTab() %d records read from %s.", prpPrvRecs->numObjs, pcgPrvtab );
    ilRc = RC_SUCCESS;
    /*
        }
     */

    close_my_cursor( &slLocalCursor );
    return ilRc;

} /* end LoadPrvTab() */


/**************************************************************************/
/* handleCreateSecCal()                                                   */
/*                                                                        */
/* pcpFieldList - list of field names:                                    */
/*   "USID,TYPE,[NAME],[LANG],[PASS],STAT,[REMA]"                         */
/*    where PASS is NOT optional when TYPE='U'.                           */
/*    The list also contains one or many calendar records:                */
/*    "VAFR,VATO,[FREQ]"                                                  */
/* pcpDataList - data corresponding to pcpFieldList.                      */
/* pcpSecUrno - URNO of the newly create SECTAB rec --> returned.         */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/*                                                                        */
/* Creates new profile/user/group etc records in SECTAB and one or more   */
/* calendar records in CALTAB.                                            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCreateSecCal( char *pcpFieldList, char *pcpDataList, char *pcpSecUrno, VARBUF *prpResultBuf )
{
    SECREC      rlSecRec;
    char        clTmp[BIGBUF];
    char        *pclItem = NULL;
    BOOL        blInvalidFieldList = FALSE;
    char        clErr[MAXERRLEN];

    memset( &rlSecRec, 0, sizeof( SECREC ) );

    /* read all fields */
    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "USID" ) ) != NULL )
        strcpy( rlSecRec.USID, pclItem );
    else
    {
        PushError( &rgErrList, "handleCreateSecCal() FSEC field not found." );
        blInvalidFieldList = TRUE; /* mandatory field */
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "TYPE" ) ) != NULL )
        strcpy( rlSecRec.TYPE, pclItem );
    else
    {
        PushError( &rgErrList, "handleCreateSecCal() TYPE field not found." );
        blInvalidFieldList = TRUE; /* mandatory field */
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "NAME" ) ) != NULL )
        strcpy( rlSecRec.NAME, pclItem );

    if( strlen( rlSecRec.NAME ) <= 0 )
        strcpy( rlSecRec.NAME, " " );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "LANG" ) ) != NULL )
        strcpy( rlSecRec.LANG, pclItem );

    if( strlen( rlSecRec.LANG ) <= 0 )
        strcpy( rlSecRec.LANG, " " );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "DEPT" ) ) != NULL )
        strcpy( rlSecRec.DEPT, pclItem );

    if( strlen( rlSecRec.DEPT ) <= 0 )
        strcpy( rlSecRec.DEPT, " " );

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "PASS" ) ) != NULL )
        strcpy( rlSecRec.PASS, pclItem );
    else if( strcmp( rlSecRec.TYPE, "U" ) )
        strcpy( rlSecRec.PASS, " " ); /* for TYPE<>'U' PASS can be blank */
    else
    {
        PushError( &rgErrList, "handleCreateSecCal() PASS field not found." );
        blInvalidFieldList = TRUE; /* mandatory field */
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "STAT" ) ) != NULL )
        strcpy( rlSecRec.STAT, pclItem );
    else
    {
        PushError( &rgErrList, "handleCreateSecCal() STAT field not found." );
        blInvalidFieldList = TRUE; /* mandatory field */
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "REMA" ) ) != NULL )
        strcpy( rlSecRec.REMA, pclItem );

    if( strlen( rlSecRec.REMA ) <= 0 )
        strcpy( rlSecRec.REMA, " " );

    if( blInvalidFieldList )
        return RC_FAIL;

    /* Get new URNO for the SEC rec */
    if( GetNextUrno( rlSecRec.URNO ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "handleCreateSecCal() Error getting new URNO" );
        return RC_FAIL;
    }


    /* copy the SEC URNO so that it can returned to the calling environment */
    strcpy( pcpSecUrno, rlSecRec.URNO );


    dbg( DEBUG,"handleCreateSecCal() URNO=<%s> USID=<%s> TYPE=<%s> NAME=<%s> LANG=<%s> DEPT=<%s> PASS=<********> STAT=<%s> REMA=<%s>",rlSecRec.URNO,rlSecRec.USID,rlSecRec.TYPE,rlSecRec.NAME,rlSecRec.LANG,rlSecRec.DEPT, rlSecRec.STAT,rlSecRec.REMA );


    /* insert the SEC rec into the DB */
    if( InsertSecRec( &rlSecRec ) != RC_SUCCESS )
    {
        PushError( &rgErrList, "handleCreateSecCal() Error inserting into SECTAB" );
        return RC_FAIL;
    }


    /* add the SEC URNO to the result list */
    sprintf( clTmp, "%s,%s", LenVarBuf( prpResultBuf ) > 0 ? "\nSEC" : "SEC",
             rlSecRec.URNO );

    if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
    {
        sprintf( clErr, "handleCreateSecCal() Error cannot add the following string to the result buffer:\n%s", clTmp );
        PushError( &rgErrList, clErr );
        return RC_FAIL;
    }

    /* create records in CALTAB */
    if( handleCreateCal( pcpFieldList, pcpDataList,
                         rlSecRec.URNO, prpResultBuf ) != RC_SUCCESS )
    {
        PushError( &rgErrList,
                   "handleCreateSecCal() Error creating a CALTAB record." );
        return RC_FAIL;
    }

    return RC_SUCCESS;

} /* end handleCreateSecCal() */

/**************************************************************************/
/* handleCreateCal()                                                      */
/*                                                                        */
/* pcpFieldList - list of field names:                                    */
/*    The list contains one or many calendar records:                     */
/*    "VAFR,VATO,[FREQ]" or if pcpSecUrno null: "FSEC,VAFR,VATO,[FREQ]".  */
/* pcpDataList - data corresponding to pcpFieldList.                      */
/* pcpSecUrno - FSEC URNO.                                                */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/*                                                                        */
/* Create one or more calendar records in CALTAB, setting FSEC=pcpSecUrno */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCreateCal( char *pcpFieldList, char *pcpDataList, char *pcpSecUrno, VARBUF *prpResultBuf )
{

    int ilNumFields = itemCount( pcpFieldList ); /*numflds in the fld list */
    int ilNumData = itemCount( pcpDataList ); /* num flds in the data list */
    int ilNumCalFlds, ilCalLoop, ilFirstCalFld;
    int ilVafrPos, ilVatoPos, ilFreqPos, ilFsecPos;
    BOOL blInvalidFieldList = FALSE;
    char clTmp[BIGBUF];
    char clErr[MAXERRLEN];
    CALREC rlCalRec;
    /* FSEC can be either in the fieldList or in pcpSecUrno */
    BOOL blFsecInList = ( pcpSecUrno != NULL ) ? FALSE : TRUE;

    /* work out how many cal values there are */

    /* work out positions and number of CAL fields */
    ilNumCalFlds = 0;

    if( ( ilVafrPos = GetItemIndex( pcpFieldList, "VAFR" ) ) > 0 )
        ilNumCalFlds++;
    else
    {
        PushError( &rgErrList, "handleCreateCal() VAFR field not found." );
        blInvalidFieldList = TRUE;
    }

    if( ( ilVatoPos = GetItemIndex( pcpFieldList, "VATO" ) ) > 0 )
        ilNumCalFlds++;
    else
    {
        PushError( &rgErrList, "handleCreateCal() VATO field not found." );
        blInvalidFieldList = TRUE;
    }

    if( blFsecInList && ( ilFsecPos = GetItemIndex( pcpFieldList, "FSEC" ) ) > 0 )
        ilNumCalFlds++;

    if( ( ilFreqPos = GetItemIndex( pcpFieldList, "FREQ" ) ) > 0 )
        ilNumCalFlds++;

    dbg( DEBUG, "Num cal fields is %d", ilNumCalFlds );

    if( blInvalidFieldList )
        return RC_FAIL;

    /* fld list contains: "USID,NAME...,VAFR,VATO", ignore non-CAL fields */
    ilFirstCalFld = ilNumFields - ilNumCalFlds;
    ilVafrPos -= ilFirstCalFld;
    ilVatoPos -= ilFirstCalFld;
    ilFsecPos -= ilFirstCalFld;
    ilFreqPos -= ilFirstCalFld;

    /* loop for 1 to n VAFR,VATO,FREQ combinations and insert them into CALTAB*/
    for( ilCalLoop = ilFirstCalFld; ilCalLoop < ilNumData; ilCalLoop += ilNumCalFlds )
    {
        memset( &rlCalRec, 0, sizeof( CALREC ) );

        /* get new URNO for the CAL rec */
        if( GetNextUrno( rlCalRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateCal() Error getting new URNO." );
            return RC_FAIL;
        }

        /* load the data into the CAL rec */
        if( blFsecInList )
            strcpy( rlCalRec.FSEC,
                    sGet_item2( pcpDataList, ilCalLoop + ilFsecPos ) );
        else
            strcpy( rlCalRec.FSEC, pcpSecUrno );

        strcpy( rlCalRec.VAFR, sGet_item2( pcpDataList, ilCalLoop + ilVafrPos ) );
        strcpy( rlCalRec.VATO, sGet_item2( pcpDataList, ilCalLoop + ilVatoPos ) );

        if( ilFreqPos > 0 ) /* FREQ is optional */
            strcpy( rlCalRec.FREQ,
                    sGet_item2( pcpDataList, ilCalLoop + ilFreqPos ) );
        else
            strcpy( rlCalRec.FREQ, pcgDefFreq );

        dbg( DEBUG, "handleCreateCal() URNO=<%s> FSEC=<%s> VAFR=<%s> VATO=<%s> FREQ=<%s>", rlCalRec.URNO, rlCalRec.FSEC, rlCalRec.VAFR, rlCalRec.VATO, rlCalRec.FREQ );

        /* insert the CAL rec into the DB */
        if( InsertCalRec( &rlCalRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateCal() Error inserting into CALTAB." );
            return RC_FAIL;
        }

        /* add new CAL rec to the result buffer (sent back to client) */
        sprintf( clTmp, "%s,%s,%s,%s,%s,%s",
                 LenVarBuf( prpResultBuf ) > 0 ? "\nCAL" : "CAL", rlCalRec.URNO,
                 rlCalRec.FSEC, rlCalRec.VAFR, rlCalRec.VATO, rlCalRec.FREQ );

        if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
        {
            sprintf( clErr, "handleCreateCal() Error cannot add the following string to the result buffer:\n%s", clTmp );
            PushError( &rgErrList, clErr );
            return RC_FAIL;
        }

    } /* end for() */

    return RC_SUCCESS;

} /* end handleCreateCal() */

/**************************************************************************/
/* handleCreateGrpCal()                                                   */
/*                                                                        */
/* pcpFieldList - list of field names:                                    */
/*    The list contains one or many group and calendar records:           */
/*    "FREL,FSEC,TYPE,[STAT],[VAFR],[VATO],[FREQ]"                        */
/* pcpDataList - data corresponding to pcpFieldList.                      */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/* prpBcUrnosBuf - buffer for FSEC URNOs to be broadcasted                */
/*                                                                        */
/* Create one or more group records in GRPTAB with their associated       */
/* calendar records.                                                      */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCreateGrpCal( char *pcpFieldList, char *pcpDataList, VARBUF *prpResultBuf, VARBUF *prpBcUrnosBuf )
{
    int ilNumFields = itemCount( pcpFieldList ); /* num flds in field list */
    int ilNumData = itemCount( pcpDataList ); /* num fields in data list */
    int ilGrpLoop;
    char clTmp[BIGBUF];
    char clErr[MAXERRLEN];
    char pclFieldList[BIGBUF], pclDataList[BIGBUF];
    char clVafr[DATELEN + 1], clVato[DATELEN + 1], clFreq[FREQLEN + 1];
    int ilFrelPos, ilFsecPos, ilTypePos, ilStatPos;
    int ilVafrPos, ilVatoPos, ilFreqPos;
    BOOL blInvalidFieldList = FALSE;
    GRPREC rlGrpRec;
    int ilBcBufLen = 0;

    /* work out positions and number of GRP fields */
    if( ( ilFrelPos = GetItemIndex( pcpFieldList, "FREL" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateGrpCal() FREL not found." );
        blInvalidFieldList = TRUE;
    }

    if( ( ilFsecPos = GetItemIndex( pcpFieldList, "FSEC" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateGrpCal() FSEC not found." );
        blInvalidFieldList = TRUE;
    }

    if( ( ilTypePos = GetItemIndex( pcpFieldList, "TYPE" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateGrpCal() TYPE not found." );
        blInvalidFieldList = TRUE;
    }

    ilStatPos = GetItemIndex( pcpFieldList, "STAT" );
    ilVafrPos = GetItemIndex( pcpFieldList, "VAFR" );
    ilVatoPos = GetItemIndex( pcpFieldList, "VATO" );
    ilFreqPos = GetItemIndex( pcpFieldList, "FREQ" );


    if( blInvalidFieldList )
        return RC_FAIL;


    for( ilGrpLoop = 0; ilGrpLoop < ilNumData; ilGrpLoop += ilNumFields )
    {
        memset( &rlGrpRec, 0, sizeof( GRPREC ) );

        /* get a URNO for a new group record */
        if( GetNextUrno( rlGrpRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateGrpCal() Error get new URNO." );
            return RC_FAIL;
        }

        /* load the data into the GRP rec */
        strcpy( rlGrpRec.FREL, sGet_item2( pcpDataList, ilGrpLoop + ilFrelPos ) );
        strcpy( rlGrpRec.FSEC, sGet_item2( pcpDataList, ilGrpLoop + ilFsecPos ) );
        strcpy( rlGrpRec.TYPE, sGet_item2( pcpDataList, ilGrpLoop + ilTypePos ) );

        if( ilStatPos > 0 )
            strcpy( rlGrpRec.STAT, sGet_item2( pcpDataList, ilGrpLoop + ilStatPos ) );
        else
            strcpy( rlGrpRec.STAT, pcgDefStat );

        if( ilVafrPos > 0 )
            strcpy( clVafr, sGet_item2( pcpDataList, ilGrpLoop + ilVafrPos ) );
        else
            strcpy( clVafr, pcgDefVafr );

        if( ilVatoPos > 0 )
            strcpy( clVato, sGet_item2( pcpDataList, ilGrpLoop + ilVatoPos ) );
        else
            strcpy( clVato, pcgDefVato );

        if( ilFreqPos > 0 )
            strcpy( clFreq, sGet_item2( pcpDataList, ilGrpLoop + ilFreqPos ) );
        else
            strcpy( clFreq, pcgDefFreq );


        dbg( DEBUG, "handleCreateGrpCal URNO=<%s> FREL=<%s> FSEC=<%s> TYPE=<%s> STAT=<%s>", rlGrpRec.URNO, rlGrpRec.FREL, rlGrpRec.FSEC, rlGrpRec.TYPE, rlGrpRec.STAT );

        /* the URNO referenced by FSEC cannot be allocated to other
           profiles if it already has a personal profile, so delete
           the personal prof */
        if( strcmp( rlGrpRec.TYPE, "P" ) == 0 )
        {
            if( handleDeleteProfile( rlGrpRec.FSEC ) != RC_SUCCESS )
            {
                PushError( &rgErrList,
                           "handleCreateGrpCal() Error deleting personal profile." );
                return RC_FAIL;
            }
        }



        /* insert a record into GRPTAB */
        if( InsertGrpRec( &rlGrpRec ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateGrpCal() Error inserting into GRPTAB." );
            return RC_FAIL;
        }

        /* add new GRP rec to the result buffer (sent back to client) */
        sprintf( clTmp, "%s,%s,%s,%s,%s,%s", LenVarBuf( prpResultBuf ) > 0 ? "\nGRP" : "GRP",
                 rlGrpRec.URNO, rlGrpRec.FREL, rlGrpRec.FSEC, rlGrpRec.TYPE, rlGrpRec.STAT );

        if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
        {
            sprintf( clErr, "handleCreateGrpCal() Error cannot add the following string to the result buffer:\n%s", clTmp );
            PushError( &rgErrList, clErr );
            return RC_FAIL;
        }


        /* create a CAL rec for the new GRP rec */
        strcpy( pclFieldList, "VAFR,VATO,FREQ" );
        sprintf( pclDataList, "%s,%s,%s", clVafr, clVato, clFreq );

        if( handleCreateCal( pclFieldList, pclDataList, rlGrpRec.URNO, &rgResultBuf ) != RC_SUCCESS )
        {
            PushError( &rgErrList,
                       "handleCreateGrpCal() Error creating a CALTAB record." );
            return RC_FAIL;
        }


        /* add new GRP rec to the result buffer (sent back to client) */
        if( prpBcUrnosBuf != NULL )
        {
            if( ( ilBcBufLen = LenVarBuf( prpBcUrnosBuf ) ) < MAX_BC_LEN )
            {
                if( ilBcBufLen <= 0 )
                    sprintf( clTmp, "%s,", rlGrpRec.FSEC, rlGrpRec.FREL );
                else
                    sprintf( clTmp, ",%s,", rlGrpRec.FSEC, rlGrpRec.FREL );

                if( AddVarBuf( prpBcUrnosBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
                {
                    sprintf( clErr, "handleCreateGrpCal() Error cannot add the following string to the broadcast buffer:\n%s", clTmp );
                    PushError( &rgErrList, clErr );
                    return RC_FAIL;
                }
            }
        }

    } /* end for() */

    return RC_SUCCESS;

} /* end handleCreateGrpCal() */


/**************************************************************************/
/* handleDeleteProfile()                                                  */
/*                                                                        */
/* pcpUrno - FSEC in PRVTAB,GRPTAB.                                       */
/*                                                                        */
/* Deletes a profile - profile records in PRVTAB and their related        */
/* GRPTAB/CALTAB records.                                                 */
/* This function receives GRPTAB.FSEC and so is the URNO of a user.       */
/* The function checks if any relations already exist for this user       */
/* and deletes them because a user can only have one profile at a time.   */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleDeleteProfile( char *pcpUrno )
{
    int ilRc;
    char clWhere[BIGBUF];
    VARBUF rlUrnoList;

    InitVarBuf( &rlUrnoList );

    sprintf( clWhere, "FSEC=%s", pcpUrno );
    ilRc = handleDelete( pcgPrvtab, clWhere, NULL, &rlUrnoList );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList,
                   "handleDeleteProfile() Error deleting record from PRVTAB." );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "TYPE='P' AND (FREL=%s OR FSEC=%s)", pcpUrno, pcpUrno );

        if( handleDeleteRel( NULL, clWhere ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleDeleteProfile() Error deleting a relation." );
            ilRc = RC_FAIL;
        }
    }

    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleDeleteProfile() */



/**************************************************************************/
/* handleDeleteRel()                                                      */
/*                                                                        */
/* pcpUrno - deletes a relation indexed by URNO.                          */
/* pcpWhere - deletes a relation using a where condition.                 */
/*                                                                        */
/* Delete a relation - delete the relation record from GRPTAB and its'    */
/* calendar records.                                                      */
/*                                                                        */
/* Where frel=urno or fsec=urno --> deletes a user/profile/group etc      */
/* Urno --> deletes a relation                                            */
/* Where frel=urno and type='P' --> deletes a personal profile            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleDeleteRel( char *pcpUrno, char *pcpWhere )
{
    int         ilRc;
    char        clWhere[BIGBUF];
    VARBUF      rlUrnoList;
    VARBUF      rlUrnoList2;

    InitVarBuf( &rlUrnoList );
    InitVarBuf( &rlUrnoList2 );

    /* delete the GRP rec and get its' URNO back in rlUrnoList */
    ilRc = handleDelete( pcgGrptab, pcpWhere, pcpUrno, &rlUrnoList );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList, "handleDeleteRel() Error deleting record from GRPTAB." );
        ilRc = RC_FAIL;
    }

    if( ilRc == RC_SUCCESS && LenVarBuf( &rlUrnoList ) > 0 )
    {
        char *pclUrnoList = ( char * ) rlUrnoList.buf;
        int ilUrnoCount = itemCount( pclUrnoList );
        int ilUrnoLoop;

        /* this relation could have been assigned to many other
           users, groups etc so loop through the urno list returned
           and delete all CAL recs where FSEC=URNO of deleted GRP rec */

        for( ilUrnoLoop = 1; ilUrnoLoop <= ilUrnoCount && ilRc == RC_SUCCESS; ilUrnoLoop++ )
        {
            sprintf( clWhere, "FSEC=%s", sGet_item2( pclUrnoList, ilUrnoLoop ) );
            ilRc = handleDelete( pcgCaltab, clWhere, NULL, &rlUrnoList2 );

            if( ilRc != RC_SUCCESS )
            {
                PushError( &rgErrList,
                           "handleDeleteRel() Error deleting record from CALTAB." );
                ilRc = RC_FAIL;
            }
        }
    }

    FreeVarBuf( &rlUrnoList );
    FreeVarBuf( &rlUrnoList2 );

    return ilRc;

} /* end handleDeleteRel() */


/**************************************************************************/
/* handleDeleteSecCal()                                                   */
/*                                                                        */
/* pcpUrno URNO in SECTAB and FSEC in CALTAB.                             */
/*                                                                        */
/* Deletes a record from SECTAB and its' associated CALTAB records.       */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleDeleteSecCal( char *pcpUrno )
{
    int         ilRc;
    char        clWhere[BIGBUF];
    VARBUF      rlUrnoList;

    InitVarBuf( &rlUrnoList );

    /* delete a record from SECTAB */
    dbg( DEBUG, "handleDeleteSecCal() Delete from %s where URNO=%s",
         pcgSectab, pcpUrno );

    ilRc = handleDelete( pcgSectab, NULL, pcpUrno, &rlUrnoList );

    if( ilRc != RC_SUCCESS )
    {
        PushError( &rgErrList,
                   "handleDeleteSecCal() Error deleting record from SECTAB." );
        ilRc = RC_FAIL;
    }

    /* delete associated CALTAB records */
    if( ilRc == RC_SUCCESS )
    {
        dbg( DEBUG, "handleDeleteSecCal() Delete from %s where URNO=%s",
             pcgCaltab, pcpUrno );
        sprintf( clWhere, "FSEC=%s", pcpUrno );
        ilRc = handleDelete( pcgCaltab, clWhere, NULL, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleDeleteSecCal() Error deleting record from CALTAB." );
            ilRc = RC_FAIL;
        }
    }


    FreeVarBuf( &rlUrnoList );

    return ilRc;

} /* end handleDeleteSecCal() */


/**************************************************************************/
/* handleCreatePrv()                                                      */
/*                                                                        */
/* pcpFieldList - list of field names:                                    */
/*    "FSEC,[FAPP],[MODU]"                                                */
/* pcpDataList - data corresponding to pcpFieldList.                      */
/* pcpSecUrno - FSEC URNO.                                                */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/*                                                                        */
/* Creates profile records. Initially loads the contents of FKTTAB, then  */
/* sets the FSEC field and writes the data to PRVTAB. If FAPP is specified*/
/* then the profile is created only for the application specified.        */
/* MODU allows the status of new profile records to be changed:           */
/*   MODU = 1 (default) standard copy.                                    */
/*          2 disable all functions.                                      */
/*          3 enable all functions.                                       */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCreatePrv( char *pcpFieldList, char *pcpDataList, VARBUF *prpResultBuf )
{

    int ilRc = RC_SUCCESS;
    short slNumFktRecs;
    char clTmp[BIGBUF], clWhere[BIGBUF];
    char clErr[MAXERRLEN];
    int ilFktLoop;
    char clFsec[URNOLEN + 1], clFapp[URNOLEN + 1];
    int ilModu;
    char *pclItem = NULL;
    FKTREC *prlFktRec; /*points to a list of FKT records read from the DB */
    PRVREC rlPrvRec;

    dbg( DEBUG, "handleCreatePrv() pcpFieldList <%s> pcpDataList <%s>",
         pcpFieldList, pcpDataList );


    /* get the data from the field list */
    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "FSEC" ) ) != NULL )
    {
        strcpy( clFsec, pclItem );
    }
    else
    {
        PushError( &rgErrList, "handleCreatePrv() FSEC field not found." );
        ilRc = RC_FAIL;
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "FAPP" ) ) != NULL )
    {
        strcpy( clFapp, pclItem );
        sprintf( clWhere, "FAPP=%s AND SDAL<>'1'", clFapp );
    }
    else
    {
        sprintf( clWhere, "SDAL<>'1'", clFapp );
    }

    if( ( pclItem = getItem( pcpFieldList, pcpDataList, "MODU" ) ) != NULL )
        ilModu = atoi( pclItem );
    else
        ilModu = STANDARD;

    dbg( DEBUG, "handleCreatePrv() MODU=%d.", ilModu );


    /* load the contents of FKTTAB --> NULL means load the whole table */
    /* otherwise load it for a single application only */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = LoadFktTab( clWhere, &rgFktBuf );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreatePrv() Error loading FKTTAB." );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {

        /* point to the list of FKT records */
        prlFktRec = ( FKTREC * ) rgFktBuf.buf;
        slNumFktRecs = ( short ) rgFktBuf.numObjs;

        dbg( DEBUG, "handleCreatePrv() slNumFktRecs %d", slNumFktRecs );

        /* clear mem for multi-insert */
        ClearVarBuf( &rgPrvBuf );
    }


    /* loop through the FKT records, creating and inserting PRV records */
    rgPrvBuf.numObjs = 0; /* count of records added */

    for( ilFktLoop = 0; ilRc == RC_SUCCESS && ilFktLoop < slNumFktRecs; ilFktLoop++ )
    {
        memset( &rlPrvRec, 0x00, sizeof( rlPrvRec ) );

        /* get a new URNO for insert into PRVTAB */
        if( GetNextUrno( rlPrvRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreatePrv() Error getting new URNO." );
            ilRc = RC_FAIL;
        }

        /* copy fields required from FKT to PRV */
        strcpy( rlPrvRec.FSEC, clFsec );
        strcpy( rlPrvRec.FFKT, prlFktRec[ilFktLoop].URNO );
        strcpy( rlPrvRec.FAPP, prlFktRec[ilFktLoop].FAPP );

        if( strcmp( prlFktRec[ilFktLoop].FUNC, pcgDefFunc ) )
        {
            switch( ilModu )
            {
                case DISABLE_ALL:
                    strcpy( rlPrvRec.STAT, "0" );
                    break;

                case ENABLE_ALL:
                    strcpy( rlPrvRec.STAT, "1" );
                    break;

                default: /* standard copy */
                    strcpy( rlPrvRec.STAT, prlFktRec[ilFktLoop].STAT );
            } /* end switch() */
        }
        else
        {
            /* when FUNC="InitModu" copy the default STAT */
            strcpy( rlPrvRec.STAT, prlFktRec[ilFktLoop].STAT );
        }

        dbg( DEBUG, "handleCreatePrv() URNO=<%s> FSEC=<%s> FFKT=<%s> FAPP=<%s> STAT=<%s>", rlPrvRec.URNO, rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );

        /* add fields to the multi-insert array */
        AddVarBuf( &rgPrvBuf, &rlPrvRec, sizeof( PRVREC ) );
        rgPrvBuf.numObjs++; /* count of records added */

        /* add new PRV rec to the result buffer (sent back to client) */
        sprintf( clTmp, "%s,%s,%s,%s,%s,%s",
                 LenVarBuf( prpResultBuf ) > 0 ? "\nPRV" : "PRV", rlPrvRec.URNO,
                 rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );

        if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
        {
            sprintf( clErr, "handleCreatePrv() Error cannot add the following string to the result buffer:\n%s", clTmp );
            PushError( &rgErrList, clErr );
            ilRc = RC_FAIL;
        }

    } /* end for() */

    /* insert all the new PRV records */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = InsertPrvRecs( &rgPrvBuf );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreatePrv() Error inserting into PRVTAB." );
            ilRc = RC_FAIL;
        }
    }

    return ilRc;

} /* end handleCreatePrv() */


/**************************************************************************/
/* handleCopyPrv()                                                        */
/*                                                                        */
/* pcpFsec - The URNO in SECTAB of the new profile being created:         */
/* pcpFprv - The URNO in SECTAB of the profile to be copied               */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/*                                                                        */
/* Copies a profile.                                                      */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCopyPrv( char *pcpFsec, char *pcpFprv, VARBUF *prpResultBuf )
{
    int ilRc = RC_SUCCESS;
    short slNumPrvRecs;
    char clTmp[BIGBUF], clWhere[BIGBUF];
    char clErr[MAXERRLEN];
    int ilPrvLoop;
    int ilModu;
    char *pclItem = NULL;
    PRVREC *prlOldPrvRec; /*points to a list of PRV records read from the DB */
    PRVREC rlPrvRec;

    dbg( DEBUG, "handleCopyPrv() pcpFsec <%s> pcpFprv <%s>", pcpFsec, pcpFprv );

    /* load the contents of PRVTAB --> NULL means load the whole table */
    /* otherwise load it for a single application only */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FSEC=%s", pcpFprv );
        ilRc = LoadPrvTab( clWhere, &rgFktBuf );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCopyPrv() Error loading PRVTAB." );
            ilRc = RC_FAIL;
        }
    }

    if( ilRc == RC_SUCCESS )
    {

        /* point to the list of PRV records */
        prlOldPrvRec = ( PRVREC * ) rgFktBuf.buf;
        slNumPrvRecs = ( short ) rgFktBuf.numObjs;

        dbg( DEBUG, "handleCopyPrv() slNumPrvRecs %d", slNumPrvRecs );

        /* clear mem for multi-insert */
        ClearVarBuf( &rgPrvBuf );
    }


    /* loop through the PRV records, creating and inserting PRV records */
    rgPrvBuf.numObjs = 0; /* count of records added */

    for( ilPrvLoop = 0; ilRc == RC_SUCCESS && ilPrvLoop < slNumPrvRecs; ilPrvLoop++ )
    {
        memset( &rlPrvRec, 0x00, sizeof( rlPrvRec ) );

        /* get a new URNO for insert into PRVTAB */
        if( GetNextUrno( rlPrvRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCopyPrv() Error getting new URNO." );
            ilRc = RC_FAIL;
        }

        /* copy fields required from PRV to PRV */
        strcpy( rlPrvRec.FSEC, pcpFsec );
        strcpy( rlPrvRec.FFKT, prlOldPrvRec[ilPrvLoop].FFKT );
        strcpy( rlPrvRec.FAPP, prlOldPrvRec[ilPrvLoop].FAPP );
        strcpy( rlPrvRec.STAT, prlOldPrvRec[ilPrvLoop].STAT );

        dbg( DEBUG, "handleCopyPrv() URNO=<%s> FSEC=<%s> FFKT=<%s> FAPP=<%s> STAT=<%s>", rlPrvRec.URNO, rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );

        /* add fields to the multi-insert array */
        AddVarBuf( &rgPrvBuf, &rlPrvRec, sizeof( PRVREC ) );
        rgPrvBuf.numObjs++; /* count of records added */

        /* add new PRV rec to the result buffer (sent back to client) */
        sprintf( clTmp, "%s,%s,%s,%s,%s,%s",
                 LenVarBuf( prpResultBuf ) > 0 ? "\nPRV" : "PRV", rlPrvRec.URNO,
                 rlPrvRec.FSEC, rlPrvRec.FFKT, rlPrvRec.FAPP, rlPrvRec.STAT );

        if( AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) ) != RC_SUCCESS )
        {
            sprintf( clErr, "handleCopyPrv() Error cannot add the following string to the result buffer:\n%s", clTmp );
            PushError( &rgErrList, clErr );
            ilRc = RC_FAIL;
        }

    } /* end for() */

    /* insert all the new PRV records */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = InsertPrvRecs( &rgPrvBuf );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCopyPrv() Error inserting into PRVTAB." );
            ilRc = RC_FAIL;
        }
    }

    return ilRc;

} /* end handleCopyPrv() */

/**************************************************************************/
/* handleCreateFkt()                                                      */
/*                                                                        */
/* pcpFieldList - list of field names:                                    */
/*    The list contains one or many FKTTAB   records:                     */
/*    "SUBD,[SDAL],FUNC,[FUAL],TYPE,STAT"                                 */
/* pcpDataList - data corresponding to pcpFieldList.                      */
/* pcpFapp - urno of the application to be updated.                       */
/* prpResultBuf - buffer to receive insert results to send back to client.*/
/*                                                                        */
/* Create records in FKTTAB for the application specified in FAPP.        */
/* Existing FKTTAB records - SDAL is set to '1' if the new record is not  */
/* identical to the old one or if the new record is not found.            */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int handleCreateFkt( char *pcpFieldList, char *pcpDataList, char *pcpFapp, VARBUF *prpResultBuf )
{
    int ilRc = RC_SUCCESS;
    char clTmp[BIGBUF];
    int ilNumFields = itemCount( pcpFieldList ); /*num flds in the field list*/
    int ilNumData = itemCount( pcpDataList ); /* num fields in the data list */
    int ilSubdPos, ilSdalPos, ilFuncPos, ilFualPos, ilTypePos, ilStatPos;
    BOOL blInvalidFieldList = FALSE;
    int ilFktLoop;
    char clFfkt[URNOLEN + 1];
    char clWhere[BIGBUF];
    FKTREC rlFktRec, *prlFktRec;
    VARBUF rlNewFktRecs;
    BOOL blInsertFktRec = TRUE;

    /* initialise list of records to be inserted into FKTTAB */
    InitVarBuf( &rlNewFktRecs );
    rlNewFktRecs.numObjs = 0;

    if( ( ilSubdPos = GetItemIndex( pcpFieldList, "SUBD" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateFkt(1) SUBD field not found." );
        blInvalidFieldList = TRUE;
    }

    ilSdalPos = GetItemIndex( pcpFieldList, "SDAL" ); /* SDAL optional */

    if( ( ilFuncPos = GetItemIndex( pcpFieldList, "FUNC" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateFkt(2) FUNC field not found." );
        blInvalidFieldList = TRUE;
    }

    ilFualPos = GetItemIndex( pcpFieldList, "FUAL" ); /* FUAL optional */

    if( ( ilTypePos = GetItemIndex( pcpFieldList, "TYPE" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateFkt(3) TYPE field not found." );
        blInvalidFieldList = TRUE;
    }

    if( ( ilStatPos = GetItemIndex( pcpFieldList, "STAT" ) ) <= 0 )
    {
        PushError( &rgErrList, "handleCreateFkt(4) STAT field not found." );
        blInvalidFieldList = TRUE;
    }

    if( blInvalidFieldList )
        ilRc = RC_FAIL;

    /* load the contents of FKTTAB for the application */
    /* this is used to check the new config against the existing one */
    if( ilRc == RC_SUCCESS )
    {
        sprintf( clWhere, "FAPP=%s AND SDAL<>'1'", pcpFapp );

        if( LoadFktTab( clWhere, &rgFktBuf ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateFkt(5) Error loading FKTTAB." );
            ilRc = RC_FAIL;
        }
    }

    for( ilFktLoop = 0; ilRc == RC_SUCCESS && ilFktLoop < ilNumData; ilFktLoop += ilNumFields )
    {
        memset( &rlFktRec, 0, sizeof( FKTREC ) );

        /* get a URNO for a new FKT record */
        if( GetNextUrno( rlFktRec.URNO ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateFkt(6) Error get new URNO." );
            ilRc = RC_FAIL;
        }

        /* load the data into the FKT rec */
        strcpy( rlFktRec.FAPP, pcpFapp );
        strcpy( rlFktRec.SUBD,
                sGet_item2( pcpDataList, ilFktLoop + ilSubdPos ) );

        if( ilSdalPos > 0 )
            strcpy( rlFktRec.SDAL, sGet_item2( pcpDataList, ilFktLoop + ilSdalPos ) );
        else
            strcpy( rlFktRec.SDAL, pcgDefSdal );

        strcpy( rlFktRec.FUNC, sGet_item2( pcpDataList, ilFktLoop + ilFuncPos ) );

        if( ilFualPos > 0 )
            strcpy( rlFktRec.FUAL, sGet_item2( pcpDataList, ilFktLoop + ilFualPos ) );
        else
            strcpy( rlFktRec.FUAL, pcgDefFual );

        strcpy( rlFktRec.TYPE,
                sGet_item2( pcpDataList, ilFktLoop + ilTypePos ) );
        strcpy( rlFktRec.STAT,
                sGet_item2( pcpDataList, ilFktLoop + ilStatPos ) );

        blInsertFktRec = TRUE;

        if( ( prlFktRec = GetFktRec( rlFktRec.FAPP, rlFktRec.FUNC, &rgFktBuf ) ) != NULL )
        {
            if( !strcmp( prlFktRec->SUBD, rlFktRec.SUBD ) && !strcmp( prlFktRec->FUAL, rlFktRec.FUAL ) &&
                    !strcmp( prlFktRec->TYPE, rlFktRec.TYPE ) && !strcmp( prlFktRec->STAT, rlFktRec.STAT ) )
            {
                /* identical record already exists so don't insert new record at end, loop through all unfound recs and set SDAL to 1 to indicate that the record is old */
                prlFktRec->found = TRUE;
                blInsertFktRec = FALSE;
            }
        }

        if( blInsertFktRec )
        {
            /* record does not exist in the FKTTAB so insert it */
            dbg( DEBUG, "handleCreateFkt(7) INSERT INTO FKTTAB URNO=<%s> FAPP=<%s> SUBD=<%s> SDAL=<%s> FUNC=<%s> FUAL=<%s> TYPE=<%s> STAT=<%s>", rlFktRec.URNO, rlFktRec.FAPP, rlFktRec.SUBD, rlFktRec.SDAL, rlFktRec.FUNC, rlFktRec.FUAL, rlFktRec.TYPE, rlFktRec.STAT );

            ilRc = AddVarBuf( &rlNewFktRecs, &rlFktRec, sizeof( rlFktRec ) );
            rlNewFktRecs.numObjs++;

            /* add new FKT rec to res buf (sent back to client) */
            sprintf( clTmp, "%s,%s,%s,%s,%s,%s,%s,%s,%s", LenVarBuf( prpResultBuf ) > 0 ? "\nFKT" : "FKT", rlFktRec.URNO,
                     rlFktRec.FAPP, rlFktRec.SUBD, rlFktRec.SDAL, rlFktRec.FUNC, rlFktRec.FUAL, rlFktRec.TYPE, rlFktRec.STAT );
            ilRc = AddVarBuf( prpResultBuf, clTmp, strlen( clTmp ) );
        }

    } /* end for() */

    /* insert new records into FKTTAB */
    if( ilRc == RC_SUCCESS )
    {
        ilRc = InsertFktRecs( &rlNewFktRecs );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "handleCreateFkt(8) Error inserting into FKTTAB." );
            ilRc = RC_FAIL;
        }
    }

    FreeVarBuf( &rlNewFktRecs );


    /* set SDAL='1' of any records that weren't updated */
    if( ilRc == RC_SUCCESS )
    {
        char clUrnos[BIGBUF];
        int ilNumFktRecs = 0;

        memset( clUrnos, 0, BIGBUF );
        prlFktRec = ( FKTREC * ) rgFktBuf.buf;
        ilNumFktRecs = rgFktBuf.numObjs;

        for( ilFktLoop = 0; ilFktLoop < ilNumFktRecs; ilFktLoop++ )
        {
            if( !prlFktRec[ilFktLoop].found )
            {
                dbg( DEBUG, "handleCreateFkt(9) SET SDAL='1' FKTTAB URNO=<%s> FAPP=<%s> SUBD=<%s> SDAL=<%s> FUNC=<%s> FUAL=<%s> TYPE=<%s> STAT=<%s>", prlFktRec[ilFktLoop].URNO, prlFktRec[ilFktLoop].FAPP, prlFktRec[ilFktLoop].SUBD, prlFktRec[ilFktLoop].SDAL, prlFktRec[ilFktLoop].FUNC, prlFktRec[ilFktLoop].FUAL, prlFktRec[ilFktLoop].TYPE, prlFktRec[ilFktLoop].STAT );

                if( strlen( clUrnos ) != 0 )
                    strcat( clUrnos, "," );

                strcat( clUrnos, prlFktRec[ilFktLoop].URNO );
            }

            if( strlen( clUrnos ) > 0 && ( ( ( strlen( clUrnos ) + 30 ) > BIGBUF || ilFktLoop == ( ilNumFktRecs - 1 ) ) ) )
            {
                VARBUF rlUrnoList;
                memset( clWhere, 0, BIGBUF );
                sprintf( clWhere, "URNO IN (%s)", clUrnos );
                dbg( DEBUG, "handleCreateFkt(10) %s", clWhere );
                InitVarBuf( &rlUrnoList );
                ilRc = handleUpdate( pcgFkttab, "SDAL", "1", clWhere, NULL, &rlUrnoList );

                if( ilRc != RC_SUCCESS )
                {
                    PushError( &rgErrList, "handleCreateFkt(11) Error updating FKTTAB." );
                    ilRc = RC_FAIL;
                    break;
                }

                FreeVarBuf( &rlUrnoList );
                memset( clUrnos, 0, BIGBUF );
            }
        }
    }


    /* Delete any records from FKTTAB if they are set to old (SDAL='1') and not referenced in PRVTAB */
    if( ilRc == RC_SUCCESS )
    {
        if( ( ilRc = DeleteOldUnreferencedFktRecs( pcpFapp ) ) != RC_SUCCESS )
            PushError( &rgErrList, "handleCreateFkt(13) Error Deleting Old Unreferenced FKT Recs." );
    }

    return ilRc;

} /* end handleCreateFkt() */


/**************************************************************************/
/* DeactivateInitModuFlag()                                               */
/*                                                                        */
/* pcpFapp - URNO of the application                                      */
/* pcpUsid - the username that registered the application                 */
/* pcpFfkt - FKTTAB.URNO of InitModu flag for the application             */
/*                                                                        */
/* After an application has been registered, deactivate the InitModu flag */
/* so that the application won't be re-registered the next time that it   */
/* is started                                                             */
/*                                                                        */
/* Returns: RC_SUCCESS or RC_FAIL.                                        */
/**************************************************************************/
static int DeactivateInitModuFlag( char *pcpFapp, char *pcpUsid )
{
    VARBUF rlUrnoList;
    int ilRc = RC_SUCCESS;
    char clFsec[URNOLEN + 1], clFfkt[URNOLEN + 1], clFfktUrnos[2048], clFsecUrnos[2048];

    dbg( DEBUG, "DeactivateInitModuFlag() START" );

    /* get the SECTAB.URNO of the user */
    if( ilRc == RC_SUCCESS )
    {
        char pclSqlBuf[2048], pclDataArea[2048];
        short slLocalCursor = 0;
        short slFuncCode = START;
        memset( pclDataArea, 0, 2048 );

        sprintf( pclSqlBuf, "SELECT URNO FROM %s WHERE USID='%s' AND TYPE='U'", pcgSectab, pcpUsid );
        dbg( DEBUG, "DeactivateInitModuFlag() get the SECTAB.URNO of the user %s", pclSqlBuf );

        if( ( ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea ) ) == DB_SUCCESS )
        {
            get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clFsec );
            dbg( DEBUG, "DeactivateInitModuFlag() SECTAB.URNO of the user = %s", clFsec );
            ilRc = RC_SUCCESS;
        }
        else
        {
            PushError( &rgErrList, "DeactivateInitModuFlag() SECTAB.URNO of the user not found." );
            ilRc = RC_FAIL;
        }

        close_my_cursor( &slLocalCursor );
    }

    /* get the FKTTAB.URNO of the InitModu flag for the application (could be more than one FFKT if SDAL flag is set) */
    if( ilRc == RC_SUCCESS )
    {
        char pclSqlBuf[2048], pclDataArea[2048];
        short slLocalCursor = 0;
        short slFuncCode = START;
        memset( pclDataArea, 0, 2048 );
        memset( clFfktUrnos, 0, 2048 );
        memset( clFsecUrnos, 0, 2048 );

        ilRc = DB_SUCCESS;

        while( ilRc == DB_SUCCESS )
        {
            memset( clFfkt, 0, sizeof( clFfkt ) );
            sprintf( pclSqlBuf, "SELECT URNO FROM %s WHERE FAPP='%s' AND FUNC='%s'", pcgFkttab, pcpFapp, pcgDefFunc );
            dbg( DEBUG, "DeactivateInitModuFlag() get the FKTTAB.URNO of the InitModu flag for the application %s", pclSqlBuf );

            if( ( ilRc = sql_if( slFuncCode, &slLocalCursor, pclSqlBuf, pclDataArea ) ) == DB_SUCCESS )
            {
                get_fld( pclDataArea, FIELD_1, STR, URNOLEN, clFfkt );

                if( strlen( clFfktUrnos ) > 0 )
                    strcat( clFfktUrnos, "," );

                strcat( clFfktUrnos, clFfkt );
                ilRc = RC_SUCCESS;
                slFuncCode = NEXT;

                if( strlen( clFfktUrnos ) > 4980 )
                    break; /* theoretically will never happen! */
            }

            dbg( DEBUG, "DeactivateInitModuFlag() FKTTAB.URNO of the InitModu flag = %s", clFfktUrnos );
        }

        if( strlen( clFfktUrnos ) > 0 )
        {
            ilRc = RC_SUCCESS;
        }
        else
        {
            PushError( &rgErrList, "DeactivateInitModuFlag() FKTTAB.URNO of the InitModu flag not found." );
            ilRc = RC_FAIL;
        }

        close_my_cursor( &slLocalCursor );
    }

    /* create a list of profile URNOs for this user */
    if( ilRc == RC_SUCCESS )
    {
        dbg( DEBUG, "DeactivateInitModuFlag() create a list of profile URNOs for this user" );
        ClearVarBuf( &rgProfileList );
        rgProfileList.numObjs = 0;
        ClearVarBuf( &rgAlreadyCheckedList );
        rgAlreadyCheckedList.numObjs = 0;
        dbg( DEBUG, "CreatePrivList() **** Loading Profile URNOs ****" );

        if( ( ilRc = GetProfiles( clFsec ) ) != RC_SUCCESS )
        {
            PushError( &rgErrList, "DeactivateInitModuFlag() Error GetProfiles()." );
            ilRc = RC_FAIL;
        }
        else
        {
            URNOREC *prlUrnoList = ( URNOREC * ) rgProfileList.buf;
            int ilNumUrnos = rgProfileList.numObjs;

            if( ilNumUrnos > 0 )
            {
                int ilLoop;

                for( ilLoop = 0; ilLoop < ilNumUrnos; ilLoop++ )
                {
                    if( ilLoop != 0 )
                        strcat( clFsecUrnos, "," );

                    strcat( clFsecUrnos, prlUrnoList[ilLoop].URNO );

                    if( strlen( clFsecUrnos ) > 4980 )
                        break; /* theoretically will never happen! */
                }
            }

            dbg( DEBUG, "DeactivateInitModuFlag() profile URNOs for this user: %s", clFsecUrnos );
        }
    }

    /* loop through the profile URNOs, read the profile from the DB and update the FktMap */
    if( ilRc == RC_SUCCESS )
    {
        char clWhere[10000];
        memset( clWhere, 0, 10000 );
        sprintf( clWhere, "FFKT IN (%s) AND FSEC IN (%s)", clFfktUrnos, clFsecUrnos );
        dbg( DEBUG, "DeactivateInitModuFlag() Set STAT=%s WHERE %s", STAT_DISABLED, clWhere );

        InitVarBuf( &rlUrnoList );
        ilRc = handleUpdate( pcgPrvtab, "STAT", STAT_DISABLED, clWhere, NULL, &rlUrnoList );

        if( ilRc != RC_SUCCESS )
        {
            PushError( &rgErrList, "DeactivateInitModuFlag() Error updating PRVTAB." );
            ilRc = RC_FAIL;
        }

        FreeVarBuf( &rlUrnoList );
    }

    return ilRc;
}


/* ******************************************************************** */
/*                                                                      */
/* Function:    GetCurrDate()                                           */
/*                                                                      */
/* Parameters:  pcgCurrDate - receives the current date.                */
/*              pcgDayOfWeek - receives the day of the week number.     */
/*                                                                      */
/* Returns: RC_SUCCESS or RC_FAIL.                                      */
/*                                                                      */
/* Description: Returns pcpCurrDate as "yyyymmddhhmmss" and             */
/*              pcpDayOfWeek where 1=Mon,2=Tues .... 7=Sun.             */
/*                                                                      */
/* ******************************************************************** */
static int GetCurrDate( void )
{
    int ilRc = RC_SUCCESS;

    time_t _lCurTime;
    struct tm *plCurTime;
    struct tm rlTm;

    _lCurTime = time( 0L );
    plCurTime = ( struct tm * ) localtime( &_lCurTime );
    rlTm = *plCurTime;

    memset( pcgCurrDate, 0, 15 );
    memset( pcgDayOfWeek, 0, 3 );

    strftime( pcgCurrDate, 15, "%""Y%""m%""d%""H%""M%""S", &rlTm );
    strftime( pcgDayOfWeek, 2, "%w", &rlTm );

    if( pcgDayOfWeek[0] == '0' )
        pcgDayOfWeek[0] = '7';

    dbg( DEBUG, "GetCurrDate() Date <%s> DayOfWeek <%s>",
         pcgCurrDate, pcgDayOfWeek );
    return ilRc;

} /* end GetCurrDate() */



/* return the item index of an item in an item list         */
/* eg. ItemList="XXXX,YYYY,ZZZZ" item "YYYY" has index of 2 */
int GetItemIndex( char *pcpItemList, char *pcpItem )
{

    char *pcpPtr = NULL;
    int ilCount = 0;

    if( ( pcpPtr = strstr( pcpItemList, pcpItem ) ) != NULL )
        for( ilCount = 1; pcpPtr >= pcpItemList; pcpPtr-- )
            if( *pcpPtr == ',' )
                ilCount++;

    return ilCount;

} /* end GetItemIndex() */

static char *GetCurrTime( void )
{

    static char clCurTime[20];
    time_t      _lCurTime;
    struct tm   *plCurTime;
    struct tm   rlTm;

    _lCurTime = time( 0L );
    plCurTime = ( struct tm * ) localtime( &_lCurTime );
    rlTm = *plCurTime;
    memset( clCurTime, 0, 20 );
    strftime( clCurTime, 20, "%""d/%""m/%""Y %""H:%""M:%""S", &rlTm );

    return clCurTime;

} /* end GetCurrTime() */


/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues()
{
    int ilRC = RC_SUCCESS; /* Return code */
    int ilBreakOut = FALSE;

    do
    {
        ilRC = que( QUE_GETBIG, 0, mod_id, PRIORITY_3, 0, ( char * ) &prgItem );
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = ( EVENT * ) prgItem->text;

        if( ilRC == RC_SUCCESS )
        {
            /* Acknowledge the item */
            ilRC = que( QUE_ACK, 0, mod_id, 0, 0, NULL );

            if( ilRC != RC_SUCCESS )
            {
                /* handle que_ack error */
                HandleQueErr( ilRC );
            } /* fi */

            switch( prgEvent->command )
            {
                case HSB_STANDBY:
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ctrl_sta = prgEvent->command;
                    break;

                case HSB_COMING_UP:
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ctrl_sta = prgEvent->command;
                    DebugPrintItem( TRACE, prgItem );
                    DebugPrintEvent( TRACE, prgEvent );
                    break;

                case HSB_ACTIVE:
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ctrl_sta = prgEvent->command;
                    ilBreakOut = TRUE;
                    break;

                case HSB_ACT_TO_SBY:
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ctrl_sta = prgEvent->command;
                    break;

                case HSB_DOWN:
                    /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    terminate();
                    break;

                case HSB_STANDALONE:
                    send_message( IPRIO_ADMIN, HSB_REQUEST, 0, 0, NULL );
                    ctrl_sta = prgEvent->command;
                    ResetDBCounter();
                    ilBreakOut = TRUE;
                    break;

                case REMOTE_DB:
                    /* ctrl_sta is checked inside */
                    HandleRemoteDB( prgEvent );
                    break;

                case SHUTDOWN:
                    terminate();
                    break;

                case RESET:
                    ilRC = reset();
                    break;

                case EVENT_DATA:
                    dbg( TRACE, "HandleQueues: wrong hsb status <%d>", ctrl_sta );
                    DebugPrintItem( TRACE, prgItem );
                    DebugPrintEvent( TRACE, prgEvent );
                    break;

                case TRACE_ON:
                    dbg_handle_debug( prgEvent->command );
                    break;

                case TRACE_OFF:
                    dbg_handle_debug( prgEvent->command );
                    break;

                default:
                    dbg( TRACE, "HandleQueues: unknown event" );
                    DebugPrintItem( TRACE, prgItem );
                    DebugPrintEvent( TRACE, prgEvent );
                    break;
            } /* end switch */
        }
        else
        {
            /* Handle queuing errors */
            HandleQueErr( ilRC );
        } /* end else */
    }
    while( ilBreakOut == FALSE );

    if( igInitOK == FALSE )
    {
        init_authdl();
        igInitOK = TRUE;
    }/* end of if */

    /* OpenConnection(); */
} /* end of HandleQueues */




/********************************************************/
/********************************************************/
static int CheckHomePort( void )
{
    int ilRC = RC_SUCCESS;
    int ilGetRc = RC_SUCCESS;
    int ilHomLen = 0;
    int ilExtLen = 0;
    char pclHome[8];
    char pclExts[8];

    if( strcmp( pcgDefTblExt, "TAB" ) == 0 )
    {
        /* First Check Table Extension */
        /* Because We Need It Later */
        ilExtLen = get_real_item( pclExts, pcgTwEnd, 2 );

        if( ilExtLen != 3 )
        {
            strcpy( pclExts, pcgDefTblExt );
        } /* end if */

        /* QuickHack */
        strcpy( pclExts, pcgDefTblExt );

        if( strcmp( pclExts, pcgTblExt ) != 0 )
        {
            strcpy( pcgTblExt, pclExts );
            dbg( DEBUG, "EXTENSIONS SET TO <%s>", pcgTblExt );
        } /* end if */

        /* Now Check HomePort */
        ilHomLen = get_real_item( pclHome, pcgTwEnd, 1 );

        if( ilHomLen != 3 )
        {
            strcpy( pclHome, pcgDefH3LC );
            dbg( TRACE, "HOMEPORT SET TO DEFAULT <%s>", pcgDefH3LC );
        } /* end if */

        if( strcmp( pclHome, pcgH3LC ) != 0 )
        {
            strcpy( pcgH3LC, pclHome );
            dbg( DEBUG, "HOMEPORT SWITCHED TO <%s>", pcgH3LC );
        } /* end if */

        if( strstr( pcgHopoList, pcgH3LC ) == NULL )
        {
            dbg( TRACE, "LOGIN REJECTED: INVALID HOME AIRPORT" );
            ilRC = RC_FAIL;
        } /* end if */

        igAddHopo = TRUE;
    } /* end if */
    else
    {
        igAddHopo = FALSE;
    } /* end else */

    dbg( DEBUG, "CURRENT: HOME <%s> EXT <%s>", pcgH3LC, pcgTblExt );

    return ilRC;
} /* end CheckHomePort */



/********************************************************/
/********************************************************/
static void CheckWhereClause( int ipUseOrder, char *pcpSelKey,
                              int ipChkStat, int ipChkHopo, char *pcpOrderKey )
{
    char pclTmpBuf[32];
    char pclAddKey[128];
    char pclRcvSqlBuf[12 * 1024];
    char *pclBufBgn = NULL;
    char *pclBufEnd = NULL;
    char *pclWhereBgn = NULL;
    char *pclWhereEnd = NULL;
    char *pclDummyWhere = "  ";
    char *pclOrderBgn = NULL;
    char *pclChrPtr = NULL;
    int ilKeyPos = 0;
    int ilOutPos = 0;
    int blFldDat = FALSE;
    int blWhereFound = FALSE;
    pclAddKey[0] = 0x00;

    /* First Copy Selection Text To WorkBuffer */
    strcpy( pclRcvSqlBuf, pcpSelKey );
    /* Set All Chars Of WhereClause To Capitals */
    str_chg_upc( pclRcvSqlBuf );


    pclWhereBgn = strstr( pclRcvSqlBuf, "WHERE " );

    if( pclWhereBgn == NULL )
    {
        pclWhereBgn = pclDummyWhere;
        pclWhereEnd = pclDummyWhere;
        pclBufBgn = pclRcvSqlBuf;
        pclBufEnd = pclRcvSqlBuf + strlen( pclRcvSqlBuf );
    } /* end if */
    else
    {
        pclBufBgn = pclRcvSqlBuf;
        pclBufEnd = pclRcvSqlBuf;

        if( pclWhereBgn > pclBufBgn )
        {
            pclBufEnd = pclWhereBgn - 1;
        } /* end if */

        /* Skip Over KeyWord */
        pclWhereBgn += 6;
        blWhereFound = TRUE;

        /* Search First Non Blank Character of WhereClause */
        while( ( *pclWhereBgn == ' ' ) && ( *pclWhereBgn != '\0' ) )
        {
            pclWhereBgn++;
        } /* end while */

        pclWhereEnd = pclWhereBgn + strlen( pclWhereBgn );

        /* Set All Data To Blank, So Fields And 'Order By' Are Remaining */
        pclChrPtr = pclWhereBgn;
        blFldDat = FALSE;

        while( *pclChrPtr != '\0' )
        {
            if( *pclChrPtr == '\'' )
            {
                if( blFldDat == FALSE )
                {
                    blFldDat = TRUE;
                } /* end if */
                else
                {
                    blFldDat = FALSE;
                } /* end else */
            } /* end if */
            else
            {
                if( blFldDat == TRUE )
                {
                    *pclChrPtr = ' ';
                } /* end if */
            } /* end else */

            pclChrPtr++;
        } /* end while */
    } /* end else */

    /* Search 'Order By' Statement */
    pclOrderBgn = strstr( pclRcvSqlBuf, "ORDER BY " );

    if( pclOrderBgn != NULL )
    {
        if( blWhereFound == TRUE )
        {
            pclWhereEnd = pclOrderBgn - 1;
        } /* end if */
        else
        {
            if( pclOrderBgn > pclBufBgn )
            {
                pclBufEnd = pclOrderBgn - 1;
            } /* end if */
        } /* end else */
    } /* end if */

    if( pclWhereEnd > pclWhereBgn )
    {
        /* Search Last Non Blank Character of WhereClause */
        pclWhereEnd--;

        while( ( pclWhereEnd >= pclWhereBgn ) && ( *pclWhereEnd == ' ' ) )
        {
            pclWhereEnd--;
        } /* end while */

        pclWhereEnd++;
        *pclWhereEnd = '\0';
    } /* end if */

    if( ipChkStat == TRUE )
    {
        if( strstr( pclWhereBgn, "STAT" ) == NULL )
        {
            strcpy( pclAddKey, "STAT<>'DEL'" );
        } /* end if */
    } /* end if */

    ilKeyPos = strlen( pclAddKey );

    if( ( igAddHopo == TRUE ) &&
            ( ipChkHopo == TRUE ) && ( strcmp( pcgDefTblExt, "TAB" ) == 0 ) )
    {
        if( strstr( pclWhereBgn, "HOPO" ) == NULL )
        {
            if( ilKeyPos > 0 )
            {
                StrgPutStrg( pclAddKey, &ilKeyPos, " AND ", 0, -1, "\0" );
            }

            sprintf( pclTmpBuf, "HOPO='%s'", pcgH3LC );
            StrgPutStrg( pclAddKey, &ilKeyPos, pclTmpBuf, 0, -1, "\0" );
        } /* end if */
    } /* end if */

    pclAddKey[ilKeyPos] = 0x00;

    if( ( ilKeyPos > 0 ) || ( ipUseOrder == FALSE ) )
    {
        dbg( DEBUG, "\n<%s>", pcpSelKey );
        /* Restore Original WhereClause */
        strcpy( pclRcvSqlBuf, pcpSelKey );
        ilOutPos = 0;

        if( pclBufEnd > pclBufBgn )
        {
            *pclBufEnd = '\0';
            strcpy( pcpSelKey, pclBufBgn );
            ilOutPos = strlen( pcpSelKey );
        } /* end if */

        if( blWhereFound == TRUE )
        {
            *pclWhereEnd = '\0';
        } /* end if */

        dbg( DEBUG, "MODIFY WHERECLAUSE:" );
        dbg( DEBUG, "KEY: <%s>", pclWhereBgn );
        dbg( DEBUG, "ADD: <%s>", pclAddKey );

        if( ilKeyPos > 0 )
        {
            if( pclWhereEnd > pclWhereBgn )
            {
                StrgPutStrg( pcpSelKey, &ilOutPos, " WHERE (", 0, -1, "\0" );
                StrgPutStrg( pcpSelKey, &ilOutPos, pclWhereBgn, 0, -1, ")" );
                StrgPutStrg( pcpSelKey, &ilOutPos, " AND ", 0, -1, "\0" );
            }/* end if */
            else
            {
                StrgPutStrg( pcpSelKey, &ilOutPos, " WHERE ", 0, -1, "\0" );
            } /* end else */

            StrgPutStrg( pcpSelKey, &ilOutPos, pclAddKey, 0, -1, "\0" );
        }/* end if */
        else
        {
            if( pclWhereEnd > pclWhereBgn )
            {
                StrgPutStrg( pcpSelKey, &ilOutPos, pclWhereBgn, 0, -1, "\0" );
            } /* end if */
        } /* end else */

        pcpSelKey[ilOutPos] = 0x00;

        if( ipUseOrder == FALSE )
        {
            pcpOrderKey[0] = 0x00;
        } /* end if */

        if( pclOrderBgn != NULL )
        {
            if( ipUseOrder == TRUE )
            {
                ilOutPos = strlen( pcpSelKey );
                StrgPutStrg( pcpSelKey, &ilOutPos, " ", 0, -1, pclOrderBgn );
                pcpSelKey[ilOutPos] = 0x00;
            } /* end if */
            else
            {
                strcpy( pcpOrderKey, pclOrderBgn );
            } /* end else */
        } /* end if */

        if( ipUseOrder == FALSE )
        {
            dbg( DEBUG, "ORD: <%s>", pcpOrderKey );
        } /* end if */

        dbg( DEBUG, "\n<%s>", pcpSelKey );
    } /* end if */

    return;
} /* end CheckWhereClause */


/********************************************************/
/********************************************************/
static void GetHopoList( void )
{
    int ilGetRc = DB_SUCCESS;
    int ilChrPos = 0;
    short slHopoCursor = 0;
    short slHopoFkt = START;
    char pclMySqlBuf[128];
    char pclMySqlDat[16];
    char pclSecTab[8];

    pcgHopoList[0] = 0x00;
    sprintf( pclMySqlBuf, "SELECT DISTINCT(HOPO) FROM SEC%s "
             "WHERE URNO>0 ORDER BY HOPO",
             pcgTblExt );
    slHopoCursor = 0;
    slHopoFkt = START;

    while( ( ilGetRc = sql_if( slHopoFkt, &slHopoCursor, pclMySqlBuf, pclMySqlDat ) ) ==
            DB_SUCCESS )
    {
        if( ilGetRc == DB_SUCCESS )
        {
            if( slHopoFkt == NEXT )
            {
                strcat( pcgHopoList, "," );
            } /* end if */

            strcat( pcgHopoList, pclMySqlDat );
        } /* end if */

        slHopoFkt = NEXT;
    } /* end while */

    close_my_cursor( &slHopoCursor );
    dbg( TRACE, "VALID HOPO LIST <%s>", pcgHopoList );
    return;
} /* end GetHopoList */

static BOOL PwdtabExists( void )
{
    BOOL blPwdtabExists = FALSE;
    int ilCount;

    if( handleCount( pcgPwdtab, NULL, &ilCount ) == RC_SUCCESS )
    {
        dbg( TRACE, "Found PWDTAB" );
        blPwdtabExists = TRUE;
    }
    else
    {
        dbg( TRACE, "PWDTAB NOT found - continuing without it!\n" );
    }

    return blPwdtabExists;
}

#ifdef LOGGING

/********************************************************/
/********************************************************/
static int ReleaseActionInfo( char *pcpRoute, char *pcpTbl, char *pcpCmd,
                              char *pcpUrnoList, char *pcpSel, char *pcpFld, char *pcpDat )
{
    int ilRC = RC_SUCCESS;
    int ilSelLen = 0;
    int ilOutPos = 0;
    int ilRouteItm = 0;
    int ilItmCnt = 0;
    int ilActRoute = 0;
    char pclRouteNbr[8];
    char *pclOutSel = NULL;

    if( igDebugDetails == TRUE )
    {
        dbg( TRACE, "ACTION TBL <%s>", pcpTbl );
        dbg( TRACE, "ACTION CMD <%s>", pcpCmd );
        dbg( TRACE, "ACTION SEL <%s>", pcpSel );
        dbg( TRACE, "ACTION URN <%s>", pcpUrnoList );
        dbg( TRACE, "ACTION USR <%s>", pcgDestName );
    } /* end if */

    dbg( TRACE, "ACTION FLD/DAT\n<%s>\n<%s>", pcpFld, pcpDat );
    ilSelLen = strlen( pcpUrnoList ) + strlen( pcpSel ) + 32;
    pclOutSel = ( char * ) malloc( ilSelLen );

    if( pclOutSel != NULL )
    {
        StrgPutStrg( pclOutSel, &ilOutPos, pcpSel, 0, -1, " \n" );
        StrgPutStrg( pclOutSel, &ilOutPos, pcpUrnoList, 0, -1, "\n" );

        if( ilOutPos > 0 )
        {
            ilOutPos--;
        } /* end if */

        pclOutSel[ilOutPos] = 0x00;
        ilItmCnt = field_count( pcpRoute );

        for( ilRouteItm = 1; ilRouteItm <= ilItmCnt; ilRouteItm++ )
        {
            ( void ) get_real_item( pclRouteNbr, pcpRoute, ilRouteItm );
            ilActRoute = atoi( pclRouteNbr );

            if( ilActRoute > 0 )
            {
                ( void ) tools_send_info_flag( ilActRoute, 0, pcgDestName, "", pcgRecvName,
                                               "", "", pcgTwStart, pcgTwEnd,
                                               pcpCmd, pcpTbl, pclOutSel, pcpFld, pcpDat, 0 );
            } /* end if */
        } /* end for */

        free( pclOutSel );
    } /* end if */
    else
    {
        debug_level = TRACE;
        dbg( TRACE, "ERROR: UNABLE TO ALLOC %d BYTES FOR ACTION INFO", ilSelLen );
        exit( 0 );
    } /* end else */

    pclOutSel = NULL;
    return ilRC;
} /* end ReleaseActionInfo() */

/**************************************************************/
/**************************************************************/
static int GetOutModId( char *pcpProcName )
{
    int ilQueId = -1;
    char pclCfgCode[32];
    char pclCfgValu[32];
    sprintf( pclCfgCode, "QUE_TO_%s", pcpProcName );
    ReadCfg( pcgCfgFile, pclCfgValu, "QUE_DEFINES", pclCfgCode, "-1" );
    ilQueId = atoi( pclCfgValu );

    if( ilQueId < 0 )
    {
        ilQueId = tool_get_q_id( pcpProcName );
    } /* end if */

    if( ilQueId < 0 )
    {
        ilQueId = 0;
    } /* end if */

    return ilQueId;
} /* end of GetOutModId */

#endif

/* ******************************************************************** */
/* ******************************************************************** */
static int CheckModulVersion( int ipForWhat )
{
    int ilRC = RC_SUCCESS;
    int ilGetRc = RC_SUCCESS;
    int ilCnt = 0;
    int ilPos = 0;
    static char pclModulNames[512];
    static MOD_VERS prlModulVers[64];
    char pclTmpBuf[512];
    char pclTmpKey[8];
    char pclActModul[32];
    char pclChkModul[32];
    char pclActVers[32];
    char pclChkVers[32];
    char pclModLst[512];
    char pclErrMsg[512];

    switch( ipForWhat )
    {
        case FOR_INIT:
            ReadCfg( pcgCfgFile, pclTmpBuf, "CHECK_VERSION", "CHECK_MODUL", "NONE" );
            dbg( TRACE, "MODULES TO CHECK: <%s>", pclTmpBuf );
            sprintf( pclModulNames, ",%s,", pclTmpBuf );

            do
            {
                ilCnt++;
                sprintf( pclTmpKey, "MODUL%d", ilCnt );
                ilGetRc = ReadCfg( pcgCedaCfgFile, pclTmpBuf,
                                   "CLIENT_VERSIONS", pclTmpKey, "NONE" );
                sprintf( prlModulVers[ilCnt].VersStrg, ",%s,", pclTmpBuf );
                dbg( TRACE, "MODUL %2d: <%s>", ilCnt, pclTmpBuf );
            }
            while( ilGetRc == RC_SUCCESS );

            ilCnt--;
            prlModulVers[0].Count = ilCnt;
            ilRC = RC_SUCCESS;
            break;

        case FOR_CHECK:
            get_real_item( pclActModul, pcgTwEnd, 3 );
            get_real_item( pclActVers, pcgTwEnd, 4 );
            sprintf( pclChkModul, ",%s,", pclActModul );

            if( strstr( pclModulNames, pclChkModul ) != NULL )
            {
                ilRC = RC_FAIL;
                sprintf( pclTmpBuf, ",%s,%s,", pclActModul, pclActVers );
                dbg( TRACE, "CHECK <%s>", pclTmpBuf );

                for( ilCnt = 1;
                        ( ( ilCnt <= prlModulVers[0].Count ) && ( ilRC != RC_SUCCESS ) );
                        ilCnt++ )
                {
                    if( strstr( prlModulVers[ilCnt].VersStrg, pclTmpBuf ) != NULL )
                    {
                        ilRC = RC_SUCCESS;
                    } /* end if */
                    else
                    {
                        if( strstr( prlModulVers[ilCnt].VersStrg, pclChkModul ) != NULL )
                        {
                            get_real_item( pclChkVers, prlModulVers[ilCnt].VersStrg, 3 );
                            StrgPutStrg( pclModLst, &ilPos, pclChkVers, 0, -1, "\n" );
                        } /* end if */
                    } /* end else */
                } /* end for */

                if( ilRC != RC_SUCCESS )
                {
                    if( ilPos > 0 )
                    {
                        ilPos--;
                    } /* end if */

                    pclModLst[ilPos] = 0x00;
                    ilPos = 0;
                    StrgPutStrg( pclErrMsg, &ilPos, "Unregistered Modul Version ", 0, -1, "\0" );
                    StrgPutStrg( pclErrMsg, &ilPos, pclActVers, 0, -1, " !!\n" );
                    StrgPutStrg( pclErrMsg, &ilPos, "Valid Version(s):", 0, -1, "\n" );
                    StrgPutStrg( pclErrMsg, &ilPos, pclModLst, 0, -1, "\n" );
                    pclErrMsg[ilPos] = 0x00;
                    tools_send_sql_rc( igQueOut, "GPR", "", "", "", pclErrMsg, ilRC );
                } /* end if */
            } /* end if */

            break;

        default:
            break;
    } /* end switch */

    return ilRC;
} /* end CheckModulVersion */

/* *******************************************************/
/* 20040715 JIM added:                                   */
/* SetHopoDefaults:     get rid of unset HOPO values     */
/* *******************************************************/
static void SetHopoDefaults( void )
{
    int ilRC = RC_SUCCESS;
    int ilChrPos = 0;
    int ilLen;
    int ilNr;
    short slCursor = 0;
    short slFkt = 0;
    char pclTana[32];
    char pclDefault[320];
    char pclHopo[8];
    char pclSqlBuf[2048];
    char pclDataArea[2048];

    sprintf( pclHopo, "'%s'", pcgH3LC ); /* DATA_DEFAULT includes '' ! */

    sprintf( pclSqlBuf, "SELECT TABLE_NAME,DATA_DEFAULT FROM USER_TAB_COLUMNS "
             "WHERE COLUMN_NAME='HOPO' AND TABLE_NAME IN "
             "(SELECT TABLE_NAME FROM USER_TABLES) "
             "ORDER BY TABLE_NAME" );
    dbg( DEBUG, "<%s>", pclSqlBuf );
    pcgUfisTables[0] = 0x00;
    ilChrPos = 0;
    slCursor = 0;
    slFkt = START;

    while( ( ilRC = sql_if( slFkt, &slCursor, pclSqlBuf, pclDataArea ) ) == RC_SUCCESS )
    {
        ( void ) BuildItemBuffer( pclDataArea, "", 2, "," );
        ilLen = get_real_item( pclTana, pclDataArea, 1 );
        ilLen = get_real_item( pclDefault, pclDataArea, 2 );

        if( ( strncmp( pclDefault, pclHopo, 5 ) != 0 ) )
        {
            StrgPutStrg( pcgUfisTables, &ilChrPos, pclTana, 0, -1, "," );
        }

        slFkt = NEXT;
    } /* end while */

    if( ilChrPos > 0 )
    {
        ilChrPos--;
    } /* end if */

    pcgUfisTables[ilChrPos] = 0x00;
    dbg( TRACE, "TABLES CONTAINING 'HOPO' without default '%s' \n<%s>",
         pcgH3LC, pcgUfisTables );
    ilNr = 1;
    close_my_cursor( &slCursor );

    while( ( ilLen = get_real_item( pclTana, pcgUfisTables, ilNr++ ) ) > 0 )
    {
#ifdef CCSDB_DB2
        sprintf(pclSqlBuf, "ALTER TABLE %s ALTER COLUMN HOPO SET DEFAULT '%s'",
                pclTana, pcgH3LC);
#else
        sprintf(pclSqlBuf, "ALTER TABLE %s MODIFY HOPO CHAR(3) DEFAULT '%s'",
                pclTana, pcgH3LC );
#endif
        dbg( TRACE, "<%s>", pclSqlBuf );
        slFkt = 0;
        pclDataArea[0] = 0;
        slCursor = 0;

        if( ( ilRC = sql_if( 0, &slCursor, pclSqlBuf, pclDataArea ) ) == RC_SUCCESS )
        {
            dbg( TRACE, "alter table succeeded <%s>", pclDataArea );
        }

        close_my_cursor( &slCursor );
    }

    return;
} /* end SetHopoDefaults */

/* ********************************************************************
   MEI: Functions added for v1.22
 * ********************************************************************/
void LoadDBParam()
{
    time_t tlNow;
    int ili, ilj;
    int ilLen;
    int ilRc;
    char pclSqlBuf[512] = "\0";
    char pclSqlData[512] = "\0";
    char pclTmpStr[512] = "\0";
    char *pclFunc = "LoadDBParam";

    memset( rgParam, 0, sizeof( rgParam ) );
    tlNow = time( 0 );

    bgDbConfig = TRUE;

    for( ili = 0; ili < NUM_PARAM; ili++ )
    {
        sprintf( pclSqlBuf, "SELECT TRIM(VALU),TRIM(URNO) FROM PARTAB WHERE PAID = '%s'", pcgParamList[ili] );
        ilRc = ExecSqlQuery( pclSqlBuf, pclSqlData );
        sprintf( rgParam[ili].PAID, "%s", pcgParamList[ili] );
        rgParam[ili].isValid = STAT_INVALID;

        if( ilRc == DB_SUCCESS )
        {
            get_fld( pclSqlData, FIELD_1, STR, VALULEN, rgParam[ili].VALU );

            rgParam[ili].iValue = atoi( rgParam[ili].VALU );

            get_fld( pclSqlData, FIELD_2, STR, URNOLEN, pclTmpStr );
            sprintf( pclSqlBuf, "SELECT TRIM(VATO), TRIM(VAFR) FROM VALTAB WHERE UVAL = '%s'", pclTmpStr );
            ilRc = ExecSqlQuery( pclSqlBuf, pclSqlData );

            if( ilRc == DB_SUCCESS )
            {
                get_fld( pclSqlData, FIELD_1, STR, DATELEN, pclTmpStr );
                dbg( TRACE, "<%s> VATO <%s>", pclFunc, pclTmpStr );
                rgParam[ili].VATO = TimeConvert_sti( pclTmpStr );
                get_fld( pclSqlData, FIELD_2, STR, DATELEN, pclTmpStr );
                dbg( TRACE, "<%s> VAFR <%s>", pclFunc, pclTmpStr );
                rgParam[ili].VAFR = TimeConvert_sti( pclTmpStr );

                if( rgParam[ili].VATO <= 0 )
                {
                    if( rgParam[ili].VAFR <= tlNow )
                        rgParam[ili].isValid = STAT_VALID;
                }
                else
                {
                    if( rgParam[ili].VATO >= tlNow && rgParam[ili].VAFR <= tlNow )
                        rgParam[ili].isValid = STAT_VALID;
                }

                dbg( TRACE, "<%s> Param [%s] - Valid <%c>", pclFunc, rgParam[ili].PAID, rgParam[ili].isValid );
                dbg( TRACE, "<%s>       - VATO <%d>", pclFunc, rgParam[ili].VATO );
                dbg( TRACE, "<%s>       - VAFR <%d>", pclFunc, rgParam[ili].VAFR );
                dbg( TRACE, "<%s>       - VALU <%s>", pclFunc, rgParam[ili].VALU );
                dbg( TRACE, "<%s>       - iValue <%d>", pclFunc, rgParam[ili].iValue );
            } /* VALTAB */
            else
            {
                dbg( TRACE, "<%s> Fail to read password validity period from VALTAB! Fallback old config", pclFunc );
                bgDbConfig = FALSE;
                break;
            }
        } /* PARTAB */
        else
        {
            dbg( TRACE, "<%s> Fail to read password config from PARTAB! Fallback old config", pclFunc );
            bgDbConfig = FALSE;
            break;
        }
    } /* for */

    if( bgDbConfig == TRUE )
    {
        igPasswordExpiryDays = rgParam[IDX_PWD_VALIDITY].iValue;
        dbg( TRACE, "<%s> Password Expiry Day follows the PARTAB val <%d>", pclFunc, igPasswordExpiryDays );
    }

    /* Check GCFTAB for encryption method   */
    sprintf(pclSqlBuf, "SELECT TRIM(VALU) FROM %s WHERE APNA='GLOBAL' AND SECT='GLOBAL' AND PARA ='ENC_BY'", pcgGcftab);
    ilRc = ExecSqlQuery(pclSqlBuf, pclSqlData);

    if(ilRc == DB_SUCCESS)
    {
        get_fld(pclSqlData, FIELD_1, STR, VALULEN1, pclTmpStr);
        dbg(TRACE, "<%s> ENC_BY <%s>", pclFunc, pclTmpStr);

        if(strncmp(pclTmpStr, "CLIENT", 6) == 0)
        {
            egEncBy = E_ENCBY_CLIENT;
        }
        else
        {
            egEncBy = E_ENCBY_SERVER;
        }
    }
    else
    {
        egEncBy = E_ENCBY_INVALID;
    }

    sprintf(pclSqlBuf, "SELECT TRIM(VALU) FROM %s WHERE APNA='GLOBAL' AND SECT='GLOBAL' AND PARA ='ENC_METHOD'", pcgGcftab);
    ilRc = ExecSqlQuery(pclSqlBuf, pclSqlData);

    if(ilRc == DB_SUCCESS)
    {
        get_fld(pclSqlData, FIELD_1, STR, VALULEN1, pclTmpStr);
        dbg(TRACE, "<%s> ENC_METHOD <%s>", pclFunc, pclTmpStr);

        if(strncmp(pclTmpStr, "UFIS", 4) == 0)
        {
            egEncMethod = E_ENCMETHOD_UFIS;
        }
        else if(strncmp(pclTmpStr, "SHA-256", 7) == 0)
        {
            egEncMethod = E_ENCMETHOD_SHA256;
        }
        else if(strncmp(pclTmpStr, "NONE", 4) == 0)
        {
            egEncMethod = E_ENCMETHOD_NONE;
        }
        else
        {
            egEncMethod = E_ENCMETHOD_NONE;
        }
    }
    else
    {
        egEncMethod = E_ENCMETHOD_INVALID;
    }

}

int ExecSqlQuery( char *pcpSqlQuery, char *pcpSqlData )
{
    int ilRc = DB_SUCCESS;
    short slSqlCursor = 0;
    char pclError[128];
    char *pclFunc = "ExecSqlQuery";

    dbg( TRACE, "<%s> Query <%s>", pclFunc, pcpSqlQuery );

    slSqlCursor = 0;
    ilRc = sql_if( START, &slSqlCursor, pcpSqlQuery, pcpSqlData );
    close_my_cursor( &slSqlCursor );

    if( ilRc == DB_ERROR )
    {
        get_ora_err( ilRc, &pclError[0] );
        dbg( TRACE, "<%s> ERROR! <%s>", pclFunc, &pclError[0] );
    }

    return ilRc;
}

/* pcpTimeStr should be in the format YYYYMMDDHHMISS */
/* Function to convert time from STRING to INTERGER  */
time_t TimeConvert_sti( char *pcpTimeStr )
{
    struct tm rlTm;
    time_t tlNow;
    int ilLen;
    char pclTmpStr[50] = "\0";
    char *pclFunc = "TimeConvert_sti";

    ilLen = strlen( pcpTimeStr );

    if( ilLen <= 0 || ilLen > DATELEN )
        return 0;

    tlNow = time( 0 );
    localtime_r( ( time_t * ) & tlNow, ( struct tm * ) &rlTm );

    sprintf( pclTmpStr, "%4.4s", pcpTimeStr );
    rlTm.tm_year = atoi( pclTmpStr ) - 1900;

    sprintf( pclTmpStr, "%2.2s", &pcpTimeStr[4] );
    rlTm.tm_mon = atoi( pclTmpStr ) - 1;

    sprintf( pclTmpStr, "%2.2s", &pcpTimeStr[6] );
    rlTm.tm_mday = atoi( pclTmpStr );

    sprintf( pclTmpStr, "%2.2s", &pcpTimeStr[8] );
    rlTm.tm_hour = atoi( pclTmpStr );

    sprintf( pclTmpStr, "%2.2s", &pcpTimeStr[10] );
    rlTm.tm_min = atoi( pclTmpStr );

    rlTm.tm_sec = 0;

    /*dbg( TRACE, "<%s> Year = %d", pclFunc, rlTm.tm_year );
    dbg( TRACE, "<%s> Mth = %d", pclFunc, rlTm.tm_mon );
    dbg( TRACE, "<%s> Day = %d", pclFunc, rlTm.tm_mday );
    dbg( TRACE, "<%s> Hour = %d", pclFunc, rlTm.tm_hour );
    dbg( TRACE, "<%s> Min = %d", pclFunc, rlTm.tm_min );*/

    tlNow = mktime( ( struct tm * ) &rlTm );
    return tlNow;
}

void CurrentUTCTime( char *pcpTimeStr )
{
    struct tm *prTime;
    time_t ilNow;
    char *pclFunc = "CurrentUTCTime";

    ilNow = time( 0 );
    prTime = ( struct tm * ) gmtime( ( time_t * ) &ilNow );
    sprintf( pcpTimeStr, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d", prTime->tm_year + 1900, prTime->tm_mon + 1,
             prTime->tm_mday, prTime->tm_hour, prTime->tm_min, prTime->tm_sec );
    dbg( TRACE, "<%s> Time <%s>", pclFunc, pcpTimeStr );
}

void SetLoginTime( char *pcpUsec )
{
    char pclTimeStr[30] = "\0";
    char pclSqlQuery[512] = "\0";
    char pclSqlData[128] = "\0";
    char *pclFunc = "setLoginTime";

    CurrentUTCTime( pclTimeStr );
    sprintf( pclSqlQuery, "UPDATE SECTAB SET LOGD = '%s' WHERE URNO = '%s'", pclTimeStr, pcpUsec );
    ExecSqlQuery( pclSqlQuery, pclSqlData );
}

/*********************************************************/
/* Create string of profile urnos and user urno. E.g.
 *   [PRO]4634541|ALL RIGHTS|Can use everything.;49875881|yyy|yyy FOR TEST
 *   [USRU]2509799
 *
 * pcpResult - hold the output string
 * pcpUsid - Usid. E.g. USIF$ADMIN
 * pcpProfileUrnos - semicolon delimited string. E.g. "123;456"
 */
static int NewConFormat( char *pcpResult, char *pcpUsid, char *pcpProfileUrnos, char *pcpCommandType )
{
    int ilRc = DB_SUCCESS;
    int ilCnt = 0;
    int ilFndCnt = 0;

    char pclSqlBuf[2048] = "\0";
    char pclSqlDat[4096*2] = "\0";
    char pclFldLst[] = "USID,URNO,NAME,TYPE,STAT";
    char pclTmp[2048] = "\0";
    char pclUsid[32] = "\0";
    char pclUrno[32] = "\0";
    char pclName[256] = "\0";
    char pclProfileUrnos[BIGBUF];
    char *pclCh;

    short slCursor = 0;
    short slFkt = 0;

    /* First change semicolon delimited ProfileUrnos to comma delimited   */
    memset( pclProfileUrnos, 0, BIGBUF );
    strncpy( pclProfileUrnos, pcpProfileUrnos, BIGBUF-1 );
    pclCh = pclProfileUrnos;

    while( *pclCh != '\0' )
    {
        if( *pclCh  == ';' )
            *pclCh = ',';

        pclCh++;
    }

    ilFndCnt = get_no_of_items( pclProfileUrnos );

    if( ilFndCnt > 0 )
    {
        sprintf( pclSqlBuf, "SELECT %s FROM SECTAB WHERE URNO IN (%s) ", pclFldLst, pclProfileUrnos );

        slCursor = 0;
        slFkt = START;
        ilCnt = 0;
        dbg( TRACE, "SQL<%s>", pclSqlBuf );

        while( ( ilRc = sql_if( slFkt, &slCursor, pclSqlBuf, pclSqlDat ) ) == DB_SUCCESS )
        {
            BuildItemBuffer( pclSqlDat, pclFldLst, 0,"," );
            dbg( TRACE, "SQL Result<%s>", pclSqlDat );
            get_real_item( pclUsid, pclSqlDat, 1 );
            get_real_item( pclUrno, pclSqlDat, 2 );
            get_real_item( pclName, pclSqlDat, 3 );

            ilCnt ++;

            if( ilCnt > 1 )
                sprintf( pclTmp, ";%s|%s|%s", pclUrno, pclUsid, pclName );
            else
                sprintf( pclTmp, "\n[PRO]%s|%s|%s", pclUrno, pclUsid, pclName );

            strcat( pcpResult,pclTmp );
            slFkt = NEXT;
        }

        close_my_cursor( &slCursor );

        if( ilCnt > 0 )
        {
            if( ( bgSSOUsidCaseInsensitive == TRUE ) && ( strncmp( pcpCommandType,"SSO",3 )==0 ) )
            {
                sprintf( pclSqlBuf, "SELECT URNO FROM SECTAB WHERE UPPER(USID) = '%s' ", pcpUsid );
            }
            else
            {
                sprintf( pclSqlBuf, "SELECT URNO FROM SECTAB WHERE USID = '%s' ", pcpUsid );
            }

            slCursor = 0;
            slFkt = START;
            dbg( TRACE, "SQL<%s>", pclSqlBuf );

            if( ( ilRc = sql_if( slFkt, &slCursor, pclSqlBuf, pclSqlDat ) ) == DB_SUCCESS )
            {
                BuildItemBuffer( pclSqlDat, "", 1,"," );
                get_real_item( pclUrno, pclSqlDat, 1 );
                sprintf( pclTmp,"\n[USRU]%s", pclUrno );
                strcat( pcpResult,pclTmp );
            }

            close_my_cursor( &slCursor );
        }
    }

    dbg( TRACE, "New Format<%s>", pcpResult );
    return ilRc;
} /* End of NewConFormat */

/*********************************************************/
/* Change rgProfileList considering client sent #PROU list
 * pcpProu - semicolon delimited profile URNO strings.
 */
static void ChgProfListForNewConFormat( char *pcpProu )
{
    URNOREC *prlUrnoList = ( URNOREC * ) rgProfileList.buf;
    int ilUrnCnt = rgProfileList.numObjs;
    int ilCnt = 0;
    int ilFndCnt = 0;
    char pclTmp[2048] = "\0";
    char pclFndList[2048] = "\0";
    char pclUsid[32] = "\0";
    char pclUrno[32] = "\0";
    char pclProu[1024] = "\0";

    if( ( strlen( pcpProu ) <= 0 ) || ( strcmp( pcpProu, " " ) == 0 ) )
        return;

    ilFndCnt = 0;
    sprintf( pclProu, ";%s;", pcpProu );

    for( ilCnt = 0; ilCnt < ilUrnCnt; ilCnt++ )
    {
        sprintf( pclTmp,";%s;", prlUrnoList[ilCnt].URNO );

        if( strstr( pclProu, pclTmp ) != NULL )
        {
            ilFndCnt++;

            if( ilFndCnt > 1 )
                strcat( pclFndList, "," );

            strcat( pclFndList, prlUrnoList[ilCnt].URNO );
        }
    }

    ClearVarBuf( &rgProfileList );
    rgProfileList.numObjs = 0;

    for( ilCnt = 0; ilCnt < ilFndCnt; ilCnt++ )
    {
        get_real_item( pclTmp, pclFndList, ilCnt+1 );
        AddToList( &rgProfileList,pclTmp );
    }
}


/* *******************************************************/


/* ******************************************************************** */
/* Space for Appendings (Source-Code Control)               */
/* ******************************************************************** */

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
