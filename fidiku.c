#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/AUH/AUH_Server/Base/Server/Interface/fidiku.c 1.9 2013/11/18 11:00:01 fya Exp  $";
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
/* 20120606 FYA v1.2c: adding code share XML data
   20120724 FYA v1.3 : fixing bug of ALC2, ALC3 in codeshare part. Current logic
   	is if ALC3 exists, then ALC2 is abandoned, otherwise, searching ALC3 by given
   	ALC2. If search fails, then use ALC2
   20120803 FYA v1.4: if ALC3 and FLNO is null, then use YY9 to fill ALC3, and 
   	last 4 letters of CSGN to fill FLNO, which is configrable
   20120918 FYA v1.5: Use ICAO for Port and Vial instead of original three letters
   20121011 FYA v1.6: Support return taxi and return flight
   20121105 FYA v1.7: Check the time window every time before flight XML message generated.
   20130228 FYA v1.8: Make ACT3 & ACT5 configurable
   20131115 FYA v1.9: Print the return value of syslibSearchDbData and returned ALC3 
   				*/
/******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */
static char sccs_version[] ="@(#) UFIS (c) ABB AAT/I fidblk.c 45.1.0 / "__DATE__" "__TIME__" / VBL";
/* be carefule with strftime or similar functions !!!                         */
/*                                                                            */
/******************************************************************************/
/* This program is a MIKE main program */

#define FYA

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
#include "debugrec.h"
#include "hsbsub.h"
#include "db_if.h"

#include "/usr/include/iconv.h"

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE *outp       = NULL;*/
int  debug_level = DEBUG;
/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */
static int   igItemLen     = 0;                   /* length of incoming item */
static int   igInitOK      = FALSE;
static char  cgConfigFile[512];
static long lgEvtCnt = 0;

static int 	 glTimeWindowStart = -24;
static int   glTimeWindowEnd =48;
static int   glbBulkTimeHour = 12;
static char  glbLastServerUTCTime[20] = "\0";
static int   glbWMFIDF = 9831;
static int   glbWMFIDB = 9832;
static int   glbWMFIDA = 9833;
static int	 igFlight = 7800;
static int	 igSqlhdl = 506;

//FYA v1.9 20131115
static int RunSQL( char *pcpSelection, char *pcpData );

//Frank v1.4 20120803
static char pcgCodeForALC3[126] = "\0";
static int igLastPartOfCSGN = 0;
static char pcgApc3DefaultValueEnable[126] = "\0";
static char pcgApc3DefaultValue[126] = "\0";
static char pcgPortICAO[126] = "\0";
static char pcgVialICAO[126] = "\0";

//Frank v1.8 20130228
static char pcgActICAO[126] = "\0";

//Frank v1.1
static int ilMode;
static char  cgHopo[8] = "\0";	/* default home airport    */
static pid_t igMyPid = 0;
static pid_t igModID_Fidbas = 0;
#define DAILY_MASK_UPDATE 1
#define NON_DAILY_MASK_UPDATE 0
//Frank v1.1

#define FIELDS_LEN 1024
/*
char pcgAftFields[FIELDS_LEN] = "URNO,ALC3,FLTN,FLNO,STOD,STOA,CKIF,CKIT,GA1F,GA1B,GA1E,ACT3,ONBL,OFBL,ONBE,STYP,TTYP,ADID,ETOD,ETOA,CTOT,REGN,PAXT,VIA3,CSGN,FTYP,NXTI";
*/
char pcgAftFields[FIELDS_LEN] = "URNO,ALC3,FLTN,FLNO,STOD,STOA,CKIF,CKIT,GA1F,GA1B,GA1E,ACT3,ONBL,OFBL,ONBE,STYP,TTYP,ADID,PSTA,PSTD,GTA1,GTD1,WRO1,WRO2,ETAI,ETDI,ETOD,ETOA,LAND,AIRB,CTOT,REGN,TMOA,PAXT,BLT1,BLT2,VIA3,CSGN,FTYP,VIAN,VIAL,NXTI,ALC2,AURN,RTOW,TGA1,SLOU,JFNO,JCNT,GD1B,GD1E,ORG3,DES3,STEV,TGD1,TIFA,TIFD,ORG4,DES4,FLNS,ACT5";
char pcgCcaFields[FIELDS_LEN] = "URNO,CKIC,CKIT,CKBS,CKES,FLNO,FLNU,CTYP";
char pcgAltFields[FIELDS_LEN] = "URNO,ALC2,ALC3,ALFN,ADD4";
char pcgAptFields[FIELDS_LEN] = "URNO,APC3,APC4,APFN,APSN";
char pcgGatFields[FIELDS_LEN] = "URNO,GNAM,TERM";
char pcgCicFields[FIELDS_LEN] = "URNO,CNAM,TERM";
char pcgBltFields[FIELDS_LEN] = "URNO,BNAM,TERM";
char pcgPstFields[FIELDS_LEN] = "URNO,PNAM";
//char FIDTAB
//Frank v1.1
char pcgFidFields[FIELDS_LEN] = "URNO,CODE,BEME,BEMD";
char pcgBagFields[FIELDS_LEN] = "URNO,ALC3,FLTN,STOA,STOD,ADID,B1BA,B1EA,CSGN";
//char pcgBagFields[FIELDS_LEN] = "URNO,ALC3,FLTN,STOA,STOD,ADID,B1BA,B2BA,CSGN";

//afttab Bag
static int igBagUrno = 0;
static int igBagAlc3 = 0;
static int igBagFltn = 0;
static int igBagStoa = 0;
static int igBagStod = 0;
static int igBagAdid = 0;
static int igBagB1ba = 0;
static int igBagB1ea = 0;
//static int igBagB2ba = 0;
static int igBagCsgn = 0;

//afttab
static int igAftUrno = 0;
static int igAftAlc3 = 0;
static int igAftFltn = 0;
static int igAftFlno = 0;
static int igAftStod = 0;
static int igAftStoa = 0;
static int igAftCkif = 0;
static int igAftCkit = 0;
static int igAftGa1f = 0;
static int igAftGa1b = 0;
static int igAftGa1e = 0;
static int igAftAct3 = 0;

//Frank v1.8 20130228
static int igAftAct5 = 0;
//Frank v1.8 20130228

static int igAftOnbl = 0;
static int igAftOfbl = 0;
static int igAftOnbe = 0;
static int igAftStyp = 0;
static int igAftTtyp = 0;
static int igAftAdid = 0;

//Frank v1.1
static int igAftPsta = 0;
static int igAftPstd = 0;
static int igAftGta1 = 0;
static int igAftGtd1 = 0;
static int igAftEtai = 0;
static int igAftEtdi = 0;
//Frank v1.1

static int igAftEtod = 0;
static int igAftEtoa = 0;

//Frank v1.1
static int igAftLand = 0;
static int igAftAirb = 0;
//Frank v1.1

static int igAftCtot = 0;
static int igAftRegn = 0;

//Frank v1.1
static int igAftTmoa = 0;
//Frank v1.1

static int igAftPaxt = 0;

//Frank v1.1
static int igAftBlt1 = 0;
static int igAftBlt2 = 0;
//Frank v1.1

//static int igAftVia3 = 0;
static int igAftCsgn = 0;
static int igAftFtyp = 0;
static int igAftNxti = 0;

static int igAftWro1 = 0;
static int igAftWro2 = 0;

static int igAftVian = 0;
static int igAftVial = 0;
static int igAftAurn = 0;
static int igAftRtow = 0;
static int igAftTga1 = 0;
static int igAftTgd1 = 0;
static int igAftSlou = 0;
static int igAftJfno = 0;
static int igAftJcnt = 0;
static int igAftGd1b = 0;
static int igAftGd1e = 0;
static int igAftOrg3 = 0;
static int igAftDes3 = 0;
static int igAftStev = 0;
static int igAftTifa = 0;
static int igAftTifd = 0;
static int igAftOrg4 = 0;
static int igAftDes4 = 0;
static int igAftFlns = 0;
//ccatab
static int igCcaUrno = 0;
static int igCcaCkic = 0;
static int igCcaCkit = 0;
static int igCcaCkbs = 0;
static int igCcaCkes = 0;
static int igCcaFlno = 0;
static int igCcaFlnu = 0;
static int igCcaCtyp = 0;

//alttab
static int igAltUrno = 0;
static int igAltAlc2 = 0;
static int igAltAlc3 = 0;
static int igAltAlfn = 0;
static int igAltAdd4 = 0;

//apttab
static int igAptUrno = 0;
static int igAptApc3 = 0;
static int igAptApc4 = 0;
static int igAptApfn = 0;
static int igAptApsn = 0;

//gattab
static int igGatUrno = 0;
static int igGatGnam = 0;
static int igGatTerm = 0;

//cictab
static int igCicUrno = 0;
static int igCicCnam = 0;
static int igCicTerm = 0;

//blttab
static int igBltUrno = 0;
static int igBltBnam = 0;
static int igBltTerm = 0;

//psttab
static int igPstUrno = 0;
static int igPstPnam = 0;

//fidtab
//Frank v1.1
static int igFidUrno = 0;
static int igFidCode = 0;
static int igFidBeme = 0;
static int igFidBemd = 0;

#define NORMAL_COLUMN_LEN  32
#define BIG_COLUMN_LEN     256
#define REM1LEN            256
#define REM1LEN            256
#define VIALLEN						 8192
typedef struct
{
    char URNO[NORMAL_COLUMN_LEN];
    char ALC3[NORMAL_COLUMN_LEN];
    char FLTN[NORMAL_COLUMN_LEN];
    char FLNO[NORMAL_COLUMN_LEN];
    char STOD[NORMAL_COLUMN_LEN];
    char STOA[NORMAL_COLUMN_LEN];
    char CKIF[NORMAL_COLUMN_LEN];
    char CKIT[NORMAL_COLUMN_LEN];
    char GA1F[NORMAL_COLUMN_LEN];
    char GA1B[NORMAL_COLUMN_LEN];
    char GA1E[NORMAL_COLUMN_LEN];
    char ACT3[NORMAL_COLUMN_LEN];
    
    //Frank v1.8 20130228
    char ACT5[NORMAL_COLUMN_LEN];
    //Frank v1.8 20130228
    
    char ONBL[NORMAL_COLUMN_LEN];
    char OFBL[NORMAL_COLUMN_LEN];
    char ONBE[NORMAL_COLUMN_LEN];
    char STYP[NORMAL_COLUMN_LEN];
    char TTYP[NORMAL_COLUMN_LEN];
    char ADID[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char PSTA[NORMAL_COLUMN_LEN];
    char PSTD[NORMAL_COLUMN_LEN];
    char GTA1[NORMAL_COLUMN_LEN];
    char GTD1[NORMAL_COLUMN_LEN];
    char ETAI[NORMAL_COLUMN_LEN];
    char ETDI[NORMAL_COLUMN_LEN];
    char WRO1[NORMAL_COLUMN_LEN];
    char WRO2[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char ETOD[NORMAL_COLUMN_LEN];
    char ETOA[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char LAND[NORMAL_COLUMN_LEN];
    char AIRB[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char CTOT[NORMAL_COLUMN_LEN];
    char REGN[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char TMOA[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char PAXT[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char BLT1[NORMAL_COLUMN_LEN];
    char BLT2[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char VIA3[NORMAL_COLUMN_LEN];
    char CSGN[NORMAL_COLUMN_LEN];
    char FTYP[NORMAL_COLUMN_LEN];
    char NXTI[NORMAL_COLUMN_LEN];
    
    char VIAN[NORMAL_COLUMN_LEN];
    char VIAL[VIALLEN];
    char AURN[NORMAL_COLUMN_LEN];
    char RTOW[NORMAL_COLUMN_LEN];
    char TGA1[NORMAL_COLUMN_LEN];
    char TGD1[NORMAL_COLUMN_LEN];
    char SLOU[NORMAL_COLUMN_LEN];
    char JFNO[VIALLEN];
    char JCNT[NORMAL_COLUMN_LEN];

		char GD1B[NORMAL_COLUMN_LEN];
    char GD1E[NORMAL_COLUMN_LEN]; 

		char ORG3[NORMAL_COLUMN_LEN];
    char DES3[NORMAL_COLUMN_LEN]; 
    
    char STEV[NORMAL_COLUMN_LEN]; 
    
    char TIFA[NORMAL_COLUMN_LEN];
    char TIFD[NORMAL_COLUMN_LEN];
    
    char ORG4[NORMAL_COLUMN_LEN];
    char DES4[NORMAL_COLUMN_LEN]; 
    char FLNS[NORMAL_COLUMN_LEN]; 
} AFTREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char CKIC[NORMAL_COLUMN_LEN];
	  char CKIT[NORMAL_COLUMN_LEN];
	  char CKBS[NORMAL_COLUMN_LEN];
	  char CKES[NORMAL_COLUMN_LEN];
	  char FLNO[NORMAL_COLUMN_LEN];
	  char FLNU[NORMAL_COLUMN_LEN];
	  char CTYP[NORMAL_COLUMN_LEN];
}CCAREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char ALC2[NORMAL_COLUMN_LEN];
	  char ALC3[NORMAL_COLUMN_LEN];
	  char ALFN[BIG_COLUMN_LEN];
	  char ADD4[BIG_COLUMN_LEN];
}ALTREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char APC3[NORMAL_COLUMN_LEN];
	  char APC4[NORMAL_COLUMN_LEN];
	  char APFN[BIG_COLUMN_LEN];
	  char APSN[BIG_COLUMN_LEN];
}APTREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char GNAM[NORMAL_COLUMN_LEN];
	  char TERM[NORMAL_COLUMN_LEN];
}GATREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char CNAM[NORMAL_COLUMN_LEN];
	  char TERM[NORMAL_COLUMN_LEN];
}CICREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char BNAM[NORMAL_COLUMN_LEN];
	  char TERM[NORMAL_COLUMN_LEN];
}BLTREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char PNAM[NORMAL_COLUMN_LEN];
}PSTREC;

//fid
// Frank v1.1
typedef struct
{
		char URNO[NORMAL_COLUMN_LEN];
	  char CODE[NORMAL_COLUMN_LEN];
	  char BEME[NORMAL_COLUMN_LEN];
	  char BEMD[NORMAL_COLUMN_LEN];
	  char REMP[NORMAL_COLUMN_LEN];
	  char REME[NORMAL_COLUMN_LEN];
	  char REMA[NORMAL_COLUMN_LEN];
}FIDREC;

typedef struct
{
	  char URNO[NORMAL_COLUMN_LEN];
	  char ALC3[NORMAL_COLUMN_LEN];
	  char FLTN[NORMAL_COLUMN_LEN];
	  char STOA[NORMAL_COLUMN_LEN];
	  char STOD[NORMAL_COLUMN_LEN];
	  char ADID[NORMAL_COLUMN_LEN];
	  char B1BA[NORMAL_COLUMN_LEN];
	  char B1EA[NORMAL_COLUMN_LEN];
	  //char B2BA[NORMAL_COLUMN_LEN];
	  char CSGN[NORMAL_COLUMN_LEN];
}BAGREC;

/******************************************************************************/
/* Function prototypes                                                          */
/******************************************************************************/
static int Init_Process();
static int ReadConfigEntry(char *pcpFname, char *pcpSection,char *pcpKeyword, char *pcpCfgBuffer);
static int    Reset(void);                       /* Reset program          */
static void    Terminate(void);                   /* Terminate program      */
static void    HandleSignal(int);                 /* Handles signals        */
static void    HandleErr(int);                    /* Handles general errors */
static void    HandleQueErr(int);                 /* Handles queuing errors */
static int  HandleData(EVENT *prpEvent);       /* Handles event data     */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

static void HandleBulkData();
//static void sendAftBulkData(char *mySelectTime);
//Frank v1.1
static void sendAftBulkData(char *mySelectTime, char * pclNowUTCTime);
static int InitFieldIndex();
void get_fld_value(char *buf,short fld_no,char *dest);
int get_flds_count(char *buf);

static int IsFlightInTimeWindow(char * pcpAdid, char *pcpTifa, char *pcpTifd);

int ConvertEncoding( char *encFrom, char *encTo, const char * in, char *out);
//Frank v1.1
//int PackFlightXml( AFTREC rpAftrec );
//int PackFlightXml( AFTREC rpAftrec, CCAREC rpCcarec, CICREC rpCicrec);
int PackFlightXml( AFTREC rpAftrec, CCAREC rpCcarecCKBS, CCAREC rpCcarecCKES, CICREC rpCicrec);
int PackRemarkXml(FIDREC rlFidrec,char *updateOrInsert);
int PackAirlineXml(ALTREC rlAltrec,char *updateOrInsert);
int HandleFidsRemark(char *cpAlc3, char *cpFlno, char *cpStad, char *cpRem);
int HandleFidsGate(char *cpAlc3, char *cpFlno, char *cpStad, char *cpGTID, char* cpTERM, char *cpGD1X, char *cpGD1Y);
int HandleFidsCheckIN(char *cpAlc3, char *cpFlno, char *cpStad, char *clCKID,char *clTERM,char *clCKBA,char *clCKEA);
int TrimSpace( char *pcpInStr );
//Frank v1.1

//Frank v1.4 20120803
int IsAlc3AndFlnoNullForAdHocFlight(AFTREC rpAftrec, char *pcpALC3, char *pcpFLNO);
int IsAlc3AndFlnoNullForAdHocFlightBAG(BAGREC rpBagrec, char *pcpALC3, char *pcpFLNO);
int PackCodeShare(AFTREC rpAftrec, char * pclXML);

int PackTowingXml( AFTREC rpAftrec );
int PackCheckinCounterXml(CCAREC rlCcarec);
int PackAirportXml(APTREC rlAptrec,char *updateOrInsert);
int PackGateXml(GATREC rlGatrec,char *updateOrInsert);
int PackCicXml(CICREC rlCicrec,char *updateOrInsert);
int PackBltXml(BLTREC rlBltrec,char *updateOrInsert);
int PackPstXml(PSTREC rlPstrec,char *updateOrInsert);
static void TrimRight(char *s);
int IS_EMPTY (char *pcpBuf);
static char *GetKeyItem(char *pcpResultBuff, long *plpResultSize,char *pcpTextBuff, char *pcpKeyWord, char *pcpItemEnd, int bpCopyData);
static void HandleFlightUpdate(char *fld,char *data);
static void HandleCheckinCounterUpdate(char *fld,char *data);
static void HandleFlightInsert(char *fld,char *data);
static void HandleCheckinCounterInsert(char *fld,char *data);
static void HandleAirlineUpdateInsert(char *fld,char *data,char *updateOrInsert);
static void HandleAirportUpdateInsert(char *fld,char *data,char *updateOrInsert);

//Frank v1.1
static void HandleRemarkMessageUpdateInsert(char *fld,char *data,char *updateOrInsert);
//Frank v1.1

//Frank v1.7c 20121107
//static void HandleDeleteFlight(char *fld,char *data);

static void HandleGateUpdateInsert(char *fld,char *data,char *updateOrInsert);
static void HandleCICUpdateInsert(char *fld,char *data,char *updateOrInsert);
static void HandleBltUpdateInsert(char *fld,char *data,char *updateOrInsert);
static void HandlePstUpdateInsert(char *fld,char *data,char *updateOrInsert);
static void HandleMsgFromFIDS(char *data);
static void HandleBeat(char *data);
//static void sendAftData(char *mySqlBuf);
static void sendAftData(char *mySqlBuf, CCAREC rpCcarec,int ilFlag);
static void sendCcaData(char *mySqlBuf);
static void sendAltData(char *mySqlBuf,char *updateOrInsert);

//Frank v1.1
static void HandleBag(char *fld,char *data,char *Selection);
static void sendFidData(char *mySqlBuf,char *updateOrInsert);
static void sendBagData(char *mySqlBuf);
//Frank v1.1

static void sendAptData(char *mySqlBuf,char *updateOrInsert);
static void sendGateData(char *mySqlBuf,char *updateOrInsert);
static void sendCicData(char *mySqlBuf,char *updateOrInsert);
static void sendBltData(char *mySqlBuf,char *updateOrInsert);
static void sendPstData(char *mySqlBuf,char *updateOrInsert);

/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/

MAIN
{
    int    ilRc = RC_SUCCESS;            /* Return code            */
    int    ilCnt = 0;
    
    INITIALIZE;            /* General initialization    */
    
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
        if(igInitOK == FALSE)
        {
            ilRc = Init_Process();
            if(ilRc != RC_SUCCESS)
            {
                dbg(TRACE,"Init_Process: init failed!");
            } /* end of if */
        }/* end of if */
    } else {
        Terminate();
    }/* end of if */
    
    dbg(TRACE,"=====================");
    dbg(TRACE,"MAIN: initializing OK");
    dbg(TRACE,"=====================");
    
    InitFieldIndex();
    
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
                HandleQueues();
                break;    
            case    HSB_COMING_UP    :
                ctrl_sta = prgEvent->command;
                HandleQueues();
                break;    
            case    HSB_ACTIVE    :
                ctrl_sta = prgEvent->command;
                break;    
            case    HSB_ACT_TO_SBY    :
                ctrl_sta = prgEvent->command;
                /* CloseConnection(); */
                HandleQueues();
                break;    
            case    HSB_DOWN    :
                /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                ctrl_sta = prgEvent->command;
                Terminate();
                break;    
            case    HSB_STANDALONE    :
                ctrl_sta = prgEvent->command;

                break;    

            case    SHUTDOWN    :
                /* process shutdown - maybe from uutil */
                Terminate();
                break;
                    
            case    RESET        :
                ilRc = Reset();
                break;
                    
            case    EVENT_DATA    :
                if((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
                {		
                		dbg(TRACE,"process name       <%s>",argv[0]);
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
                    
            case    TRACE_ON :
                dbg_handle_debug(prgEvent->command);
                break;
            case    TRACE_OFF :
                dbg_handle_debug(prgEvent->command);
                break;
            default            :
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
    
    exit(0);
    
} /* end of MAIN */

/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int Init_Process()
{
    int    ilRc = RC_SUCCESS;            /* Return code */
    char pclTmp[16] = "\0";
    char pclTmp1[16] = "\0";
    char pclSelection[1024] = "\0";
    
    /* now reading from configfile or from database */
    SetSignals(HandleSignal);
    igInitOK = TRUE;
    
    // Frank v1.1
    if(ilRc == RC_SUCCESS)
		{
			/* read HomeAirPort from SGS.TAB */
			ilRc = tool_search_exco_data("SYS", "HOMEAP", cgHopo);
			if (ilRc != RC_SUCCESS)
			{
				dbg(TRACE,"<Init_Process> EXTAB,SYS,HOMEAP not found in SGS.TAB");
				Terminate();
			} else {
				dbg(TRACE,"<Init_Process> home airport <%s>",cgHopo);
			}
		}
    
    //mode
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","MODE",pclSelection);
   	if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> MODE not found ,ilMode use default <%d>", ilMode);
    }
    else
    {
    	 if(strncmp(pclSelection,"TEST",4) == 0)
    	 {
    	 		ilMode = 1;
    	 		dbg(TRACE,"<Init_Process> MODE <TEST> ilMode <%d>", pclSelection,ilMode);
    	 }
    	 else if(strncmp(pclSelection,"REAL",4) == 0)
    	 {
    	 		ilMode = 0;
    	 		dbg(TRACE,"<Init_Process> MODE <REAL> ilMode <%d>", pclSelection,ilMode);
    	 }
    }
    
    //Frank v1.4 20120803	
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","CODE_FOR_CSGN",pclSelection);
    if( ilRc != RC_SUCCESS )
    {    	
    	memset(pcgCodeForALC3,0,sizeof(pcgCodeForALC3));
    }    
    else
    {    	
			strcpy(pcgCodeForALC3,pclSelection);
			dbg(TRACE,"pcgCodeForALC3<%s>",pcgCodeForALC3);   
    }
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","LAST_PART_OF_CSGN",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
			igLastPartOfCSGN = 0;
    }
    else
    {
			igLastPartOfCSGN = atoi(pclSelection);
			dbg(TRACE,"igLastPartOfCSGN is <%d>",igLastPartOfCSGN);
    }
    
    //Frank v1.8 20130228
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","ACT_ICAO",pclSelection);
    if( ilRc != RC_SUCCESS )
    {    	
    	memset(pcgActICAO,0,sizeof(pcgActICAO));
    	strcpy(pcgActICAO,"NO");
    }    
    else
    {    	
			strcpy(pcgActICAO,pclSelection);
			dbg(TRACE,"pcgActICAO<%s>",pcgActICAO);   
    }
    //Frank v1.8 20130228
    
    //Frank v1.5 20120918	
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","PORT_ICAO",pclSelection);
    if( ilRc != RC_SUCCESS )
    {    	
    	memset(pcgPortICAO,0,sizeof(pcgPortICAO));
    	strcpy(pcgPortICAO,"NO");
    }    
    else
    {    	
			strcpy(pcgPortICAO,pclSelection);
			dbg(TRACE,"pcgPortICAO<%s>",pcgPortICAO);   
    }
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","VIAL_ICAO",pclSelection);
    if( ilRc != RC_SUCCESS )
    {    	
    	memset(pcgVialICAO,0,sizeof(pcgVialICAO));
    	strcpy(pcgVialICAO,"NO");
    }    
    else
    {    	
			strcpy(pcgVialICAO,pclSelection);
			dbg(TRACE,"pcgVialICAO<%s>",pcgVialICAO);   
    }
    //Frank v1.4 20120808
    /*	
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","ENABLE_APC3_DEFAULT_VALUE",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
	strcpy(pcgApc3DefaultValueEnable,"NO");
    }
    else
    {
	strcpy(pcgApc3DefaultValueEnable,pclSelection);
	dbg(TRACE,"pcgApc3DefaultValueEnable is <%s>",pcgApc3DefaultValueEnable);
    }
    
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","APC3_DEFAULT_VALUE",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
	memset(pcgApc3DefaultValue,0,sizeof(pcgApc3DefaultValue));
    }
    else
    {
	strcpy(pcgApc3DefaultValue,pclSelection);
	dbg(TRACE,"pcgApc3DefaultValue is <%s>",pcgApc3DefaultValue);
    }
    */
    // debug level
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","DEBUG_LEVEL",pclSelection);
    debug_level = TRACE;
    if( !strncmp( pclSelection, "DEBUG", 5 ) )
       debug_level = DEBUG;
    dbg(TRACE,"<Init_Process> debug_level <%s><%d>", pclSelection, debug_level);
    
    // bulk time
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","BULK_TIME_HOUR",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> BULK_TIME_HOUR not found , use default <%d>", glbBulkTimeHour);
    }
    else
    {
    	
    	 glbBulkTimeHour = atoi(pclSelection);
    	 dbg(TRACE,"<Init_Process> BULK_TIME_HOUR <%s> glbBulkTimeHour <%d>", pclSelection,glbBulkTimeHour);
    	 
    }
    
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","TIME_WINDOW",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> TIME_WINDOW not found , use default <%d~%d>", glTimeWindowStart,glTimeWindowEnd);
    }
    else
    {
    	 dbg(TRACE,"<TIME_WINDOW>pclSelection<%s>",pclSelection);
    	 
    	 get_item(1,pclSelection,pclTmp,0,",","\0","\0");
    	 get_item(2,pclSelection,pclTmp1,0,",","\0","\0");
    	
    	 dbg(TRACE,"---pclTmp<%s>",pclTmp);
    	 dbg(TRACE,"---pclTmp1<%s>",pclTmp1);
    	 
    	 glTimeWindowStart = atoi(pclTmp);
    	 glTimeWindowEnd = atoi(pclTmp1);
    	 
    	 dbg(TRACE,"---glTimeWindowStart<%d>",glTimeWindowStart);
    	 dbg(TRACE,"---glTimeWindowEnd<%d>",glTimeWindowEnd);
    }
    
    // send to modid fid glbWMFIDF
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","WMFIDF_MODID",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> WMFIDF_MODID not found ,glbWMFIDF use default <%d>", glbWMFIDF);
    }
    else
    {
    	 glbWMFIDF = atoi(pclSelection);
    	 dbg(TRACE,"<Init_Process> WMFIDF_MODID <%s> glbWMFIDF <%d>", pclSelection,glbWMFIDF);
    }
    // send to modid fid glbWMFIDB
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","WMFIDB_MODID",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> WMFIDB_MODID not found ,glbWMFIDB use default <%d>", glbWMFIDB);
    }
    else
    {
    	 glbWMFIDB = atoi(pclSelection);
    	 dbg(TRACE,"<Init_Process> WMFIDB_MODID <%s> glbWMFIDB <%d>", pclSelection,glbWMFIDB);
    }
    // send to modid fid glbWMFIDA
    ilRc = ReadConfigEntry( cgConfigFile, "MAIN","WMFIDA_MODID",pclSelection);
    if( ilRc != RC_SUCCESS )
    {
    	 dbg(TRACE,"<Init_Process> WMFIDA_MODID not found ,glbWMFIDA use default <%d>", glbWMFIDA);
    }
    else
    {
    	 glbWMFIDA = atoi(pclSelection);
    	 dbg(TRACE,"<Init_Process> WMFIDA_MODID <%s> glbWMFIDA <%d>", pclSelection,glbWMFIDA);
    }    
    return(ilRc);
    
} /* end of initialize */
/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int ReadConfigEntry(char *pcpFname, char *pcpSection,char *pcpKeyword, char *pcpCfgBuffer)
{
    int ilRc = RC_SUCCESS;

    char clSection[124] = "\0";
    char clKeyword[124] = "\0";

    strcpy(clSection,pcpSection);
    strcpy(clKeyword,pcpKeyword);

    ilRc = iGetConfigEntry(pcpFname,clSection,clKeyword,
               CFG_STRING,pcpCfgBuffer);
    if(ilRc != RC_SUCCESS){
        dbg(TRACE,"Not found in %s: <%s> <%s>",cgConfigFile,clSection,clKeyword);
    } 
    else{
        dbg(DEBUG,"Config Entry <%s>,<%s>:<%s> found in %s",
        clSection, clKeyword ,pcpCfgBuffer, cgConfigFile);
    }/* end of if */
    return ilRc;
}
/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{
    int    ilRc = RC_SUCCESS;                /* Return code */
    
    dbg(TRACE,"Reset: now resetting");
    
    return ilRc;
    
} /* end of Reset */

/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate()
{

    dbg(TRACE,"Terminate: now leaving ...");
    
    exit(0);
    
} /* end of Terminate */

/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/
static void HandleSignal(int pipSig)
{
    int    ilRc = RC_SUCCESS;            /* Return code */
    dbg(TRACE,"HandleSignal: signal <%d> received",pipSig);
    switch(pipSig)
    {
    default    :
        Terminate();
        break;
    } /* end of switch */
    exit(0);
    
} /* end of HandleSignal */

/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int pipErr)
{
    int    ilRc = RC_SUCCESS;
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
        dbg(TRACE,"<%d> : msgrcv failed",pipErr);
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
        dbg(TRACE,"<%d> :  unknown queue status ",pipErr);
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
        dbg(TRACE,"<%d> : no messages on queue",pipErr);
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
    int    ilRc = RC_SUCCESS;            /* Return code */
    int    ilBreakOut = FALSE;
    
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
                Terminate();
                break;    
    
            case    HSB_STANDALONE    :
                ctrl_sta = prgEvent->command;

                ilBreakOut = TRUE;
                break;    

            case    SHUTDOWN    :
                Terminate();
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
        } else {
            /* Handle queuing errors */
            HandleQueErr(ilRc);
        } /* end else */
    } while (ilBreakOut == FALSE);
    if(igInitOK == FALSE)
    {
            ilRc = Init_Process();
            if(ilRc != RC_SUCCESS)
            {
                dbg(TRACE,"Init_Process: init failed!");
            } /* end of if */
    }/* end of if */
    /* OpenConnection(); */
} /* end of HandleQueues */
    

/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData(EVENT *prpEvent)
{ 
    int ilRc = RC_SUCCESS;         
    int ilCmd = 0;
    int ilUpdPoolJob = TRUE;
    int ilNumdays;

    BC_HEAD *prlBchead       = NULL;
    CMDBLK  *prlCmdblk       = NULL;
    char    *pclSelection    = NULL;
    char    *pclFields       = NULL;
    char    *pclData         = NULL;
    char    *pclRow          = NULL;
    char    clUrnoList[2400];
    char    clTable[34];

    prlBchead    = (BC_HEAD *) ((char *)prpEvent + sizeof(EVENT));
    prlCmdblk    = (CMDBLK *)  ((char *)prlBchead->data);
    pclSelection = prlCmdblk->data;
    pclFields    = pclSelection + strlen(pclSelection) + 1;
    pclData      = pclFields + strlen(pclFields) + 1;


    strcpy(clTable,prlCmdblk->obj_name);

    dbg(TRACE,"========== START <%10.10d> ==========",lgEvtCnt);
    
     /****************************************/
    if(ilMode == 1)
    {
    	DebugPrintBchead(TRACE,prlBchead);
    	DebugPrintCmdblk(TRACE,prlCmdblk);
    }
    dbg(TRACE,"Command: <%s>",prlCmdblk->command);
    dbg(TRACE,"Obj_name: <%s>",prlCmdblk->obj_name);
    dbg(TRACE,"Tw_start: <%s>",prlCmdblk->tw_start);
    dbg(TRACE,"originator follows event = %p ",prpEvent);

    dbg(TRACE,"originator<%d>",prpEvent->originator);
    dbg(TRACE,"selection follows Selection = %p ",pclSelection);
    dbg(TRACE,"selection <%s>",pclSelection);
    dbg(TRACE,"fields    <%s>",pclFields);
    dbg(TRACE,"data      <%s>",pclData);
    dbg(TRACE,"pid       <%d>",igMyPid = getpid());
    
    /****************************************/
    
    /* From NTISCH */
    //fidblk
    if( !strcmp(prlCmdblk->command,"TIM") )
    {
    	HandleBulkData();
    }//fidflt
    else if ( strcmp(prlCmdblk->command,"UFR") == 0 )
    {
  		/* Handle flight updates */
    	if (strcmp(prlCmdblk->obj_name,"AFTTAB") == 0)
    	{
    		dbg(TRACE,"<HandleData> : handle afttab update ");
    		HandleFlightUpdate(pclFields,pclData);
		
		/*if( strstr(pclFields,"BLT1") != 0 || strstr(pclFields,"BLT2"))
		{
    			HandleBag(pclFields,pclData);
    		}*/
			}
    }
    else if ( strcmp(prlCmdblk->command,"IFR") == 0)
    {
    	/* Handle flight insert */
      if (strcmp(prlCmdblk->obj_name,"AFTTAB") == 0)
      {
      	dbg(TRACE,"<HandleData> : handle afttab insert ");
      	HandleFlightInsert(pclFields,pclData);
      }
    }
    else if ( strcmp(prlCmdblk->command,"URT") == 0)
    {
      if (strcmp(prlCmdblk->obj_name,"CCATAB") == 0)
      {
      	/* Handle check in updates */
      	dbg(TRACE,"<HandleData> : handle ccatab update ");
      	HandleCheckinCounterUpdate(pclFields,pclData);
      }
      //Frank v1.1
      if (strcmp(prlCmdblk->obj_name,"FIDTAB") == 0)
      {
      	/* Handle Remark Meaasge updates */
      	dbg(TRACE,"<HandleData> : handle fidtab update ");
      	HandleRemarkMessageUpdateInsert(pclFields,pclData,"U");
      }
      //Frank v1.1
      if (strcmp(prlCmdblk->obj_name,"ALTTAB") == 0)
      {
      	/* Handle airline update */
      	dbg(TRACE,"<HandleData> : handle ALTTAB update ");
      	HandleAirlineUpdateInsert(pclFields,pclData,"U");
      }
      if (strcmp(prlCmdblk->obj_name,"APTTAB") == 0)
      {
      	/* Handle airport update */
      	dbg(TRACE,"<HandleData> : handle APTTAB update ");
      	HandleAirportUpdateInsert(pclFields,pclData,"U");
      }
      if (strcmp(prlCmdblk->obj_name,"GATTAB") == 0)
      {
      	/* Handle gate update */
      	dbg(TRACE,"<HandleData> : handle GATTAB update ");
      	HandleGateUpdateInsert(pclFields,pclData,"U");
      }
      if (strcmp(prlCmdblk->obj_name,"CICTAB") == 0)
      {
      	/* Handle basic data check in counter update */
      	dbg(TRACE,"<HandleData> : handle CICTAB update ");
      	HandleCICUpdateInsert(pclFields,pclData,"U");
      }
      if (strcmp(prlCmdblk->obj_name,"BLTTAB") == 0)
      {
      	/* Handle belt update */
      	dbg(TRACE,"<HandleData> : handle BLTTAB update ");
      	HandleBltUpdateInsert(pclFields,pclData,"U");
      }
      if (strcmp(prlCmdblk->obj_name,"PSTTAB") == 0)
      {
      	/* Handle parking stand update */
      	dbg(TRACE,"<HandleData> : handle PSTTAB update ");
      	HandlePstUpdateInsert(pclFields,pclData,"U");
      }
    }
    else if ( strcmp(prlCmdblk->command,"IRT") == 0)
    {
    	//Frank v1.1
      if (strcmp(prlCmdblk->obj_name,"FIDTAB") == 0)
      {
      	/* Handle Remark Meaasge updates */
      	dbg(TRACE,"<HandleData> : handle fidtab update ");
      	HandleRemarkMessageUpdateInsert(pclFields,pclData,"I");
      }
      //Frank v1.1
      if (strcmp(prlCmdblk->obj_name,"CCATAB") == 0)
      {
      	/* Handle check in insert */
      	dbg(TRACE,"<HandleData> : handle ccatab insert ");
      	HandleCheckinCounterInsert(pclFields,pclData);
      }
      if (strcmp(prlCmdblk->obj_name,"ALTTAB") == 0)
      {
      	/* Handle airline insert */
      	dbg(TRACE,"<HandleData> : handle ALTTAB insert ");
      	HandleAirlineUpdateInsert(pclFields,pclData,"I");
      }
      if (strcmp(prlCmdblk->obj_name,"APTTAB") == 0)
      {
      	/* Handle airport insert */
      	dbg(TRACE,"<HandleData> : handle APTTAB insert ");
      	HandleAirportUpdateInsert(pclFields,pclData,"I");
      }
      if (strcmp(prlCmdblk->obj_name,"GATTAB") == 0)
      {
      	/* Handle gate insert */
      	dbg(TRACE,"<HandleData> : handle GATTAB insert ");
      	HandleGateUpdateInsert(pclFields,pclData,"I");
      }
      if (strcmp(prlCmdblk->obj_name,"CICTAB") == 0)
      {
      	/* Handle basic data check in counter insert */
      	dbg(TRACE,"<HandleData> : handle CICTAB insert ");
      	HandleCICUpdateInsert(pclFields,pclData,"I");
      }
      if (strcmp(prlCmdblk->obj_name,"BLTTAB") == 0)
      {
      	/* Handle belt insert */
      	dbg(TRACE,"<HandleData> : handle BLTTAB insert ");
      	HandleBltUpdateInsert(pclFields,pclData,"I");
      }
      if (strcmp(prlCmdblk->obj_name,"PSTTAB") == 0)
      {
      	/* Handle parking stand insert */
      	dbg(TRACE,"<HandleData> : handle PSTTAB insert ");
      	HandlePstUpdateInsert(pclFields,pclData,"I");
      }
    }
    else if ( strcmp(prlCmdblk->command,"BEAT") == 0)
    {
    	/* Handle heart beat */
    	HandleBeat(pclData);
    }
    else if ( strcmp(prlCmdblk->command,"FID") == 0)
    {
    	/* Handle message from fids: flight request,checkin c,remark */
    	HandleMsgFromFIDS(pclData);
    }
    else if ( strcmp(prlCmdblk->command,"BAG") == 0)
    {
    	HandleBag(pclFields,pclData,pclSelection);
    }
    /*
    else if ( strcmp(prlCmdblk->command,"DFR") == 0)
    {//Frank 20121107 v1.7c
    	if (strcmp(prlCmdblk->obj_name,"AFTTAB") == 0)
      {
      	// Handle delete flight
      	dbg(TRACE,"<HandleData> : handle afttab delete flight ");
      	HandleDeleteFlight(pclFields,pclData);
      }
    }
    */
    dbg(TRACE,"==========  END  <%10.10d> ==========",lgEvtCnt);

    /****************************************/
    return ilRc;
    
} /* end of HandleData */

//static void HandleBag(char *fld,char *data)
static void HandleBag(char *fld,char *data,char *Selection)
{
	int ilRc = 0;
	char clUrno[24] = "\0";
	char clBagSelection[1024] = "\0";
	
	char *pclTmp=NULL;
	char *pclTmp1=NULL;
	
	//ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	
	if(strstr(Selection,"WHERE")!=0)
	{
		dbg(TRACE,"<HandleBag>Selection<%s>",Selection);
		pclTmp = strstr(Selection,"=");
		strcpy(clUrno,pclTmp+1);
		dbg(TRACE,"<HandleBag>pclTmp<%s>",pclTmp+1);
		pclTmp1=strstr(clUrno,"\n");
		dbg(TRACE,"<HandleBag>pclTmp1<%s>",pclTmp1+1);
		strcpy(clUrno,pclTmp1+1);
		
		if (!IS_EMPTY (clUrno))
		{
			dbg(TRACE,"<HandleBag> : get bag URNO <%s>",clUrno);
		
		  /*** SQL Queries ***/
		  sprintf( clBagSelection, "SELECT %s FROM AFTTAB WHERE URNO = '%s' ",
	                                   pcgBagFields, clUrno);
	    dbg(TRACE,"<HandleBag> : clBagSelection <%s> ",clBagSelection);
	    sendBagData(clBagSelection);
		}
		//else
			//dbg(TRACE,"<HandleBag> : no URNO found pclFields <%s> pclData <%s>",fld,data);
		}
	else
	{
		dbg(TRACE,"<HandleBag> : no URNO found Selection <%s>",Selection);
	}
}
static void sendBagData(char *mySqlBuf)
{
	BAGREC rlBagrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendBagData> : sql buf <%s>",mySqlBuf);
	//1.get Gate data
  flds_count = get_flds_count(pcgBagFields);
  dbg(TRACE,"<sendBagData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendBagData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlBagrec, 0, sizeof(rlBagrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igBagUrno,rlBagrec.URNO); TrimRight(rlBagrec.URNO);
    get_fld_value(pclDataArea,igBagAlc3,rlBagrec.ALC3); TrimRight(rlBagrec.ALC3);
    get_fld_value(pclDataArea,igBagFltn,rlBagrec.FLTN); TrimRight(rlBagrec.FLTN);
    get_fld_value(pclDataArea,igBagStoa,rlBagrec.STOA); TrimRight(rlBagrec.STOA);
    get_fld_value(pclDataArea,igBagStod,rlBagrec.STOD); TrimRight(rlBagrec.STOD);
    get_fld_value(pclDataArea,igBagAdid,rlBagrec.ADID); TrimRight(rlBagrec.ADID);
    get_fld_value(pclDataArea,igBagB1ba,rlBagrec.B1BA); TrimRight(rlBagrec.B1BA);
    get_fld_value(pclDataArea,igBagB1ea,rlBagrec.B1EA); TrimRight(rlBagrec.B1EA);
    //get_fld_value(pclDataArea,igBagB2ba,rlBagrec.B2BA); TrimRight(rlBagrec.B2BA);
    get_fld_value(pclDataArea,igBagCsgn,rlBagrec.CSGN); TrimRight(rlBagrec.CSGN);
    
    dbg(DEBUG,"<sendBagData> : URNO <%s>", rlBagrec.URNO);
    dbg(DEBUG,"<sendBagData> : ALC3 <%s>", rlBagrec.ALC3);
    dbg(DEBUG,"<sendBagData> : FLTN <%s>", rlBagrec.FLTN);
    dbg(DEBUG,"<sendBagData> : STOA <%s>", rlBagrec.STOA);
    dbg(DEBUG,"<sendBagData> : STOD <%s>", rlBagrec.STOD);
    dbg(DEBUG,"<sendBagData> : ADID <%s>", rlBagrec.ADID);
    dbg(DEBUG,"<sendBagData> : B1BA <%s>", rlBagrec.B1BA);
    dbg(DEBUG,"<sendBagData> : B1EA <%s>", rlBagrec.B1EA);
    //dbg(DEBUG,"<sendBagData> : B2BA <%s>", rlBagrec.B2BA);
    dbg(DEBUG,"<sendBagData> : CSGN <%s>", rlBagrec.CSGN);
    
    PackBagXml(rlBagrec);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendBagData> : records count <%d>",ilRecordCount);
}

static void HandleMsgFromFIDS(char *data)
{
	char myTimeFrom[8192] = "\0";
	char myTimeTo[32] = "\0";
	long llResultLen;
	char clHFMSelection[1024] = "\0";
	char mySelectTime_[20] = "\0";
	char tmp[20] = "201205021740";
	int  myBulkTimeSecond = 0;
	
	char clALC3[8192] = "\0";
	char clFLNO[8192] = "\0";
	char clSTAD[8192] = "\0";
	
	char clREM[8192]  = "\0";
	
	char clGTID[8192] = "\0";
	char clTERM[8192] = "\0";
	char clGD1X[8192] = "\0";
	char clGD1Y[8192]  = "\0";
	
	char clCKID[8192] = "\0";
	char clCKBA[8192] = "\0";
	char clCKEA[8192] = "\0";
	
	CCAREC rlCcarec;

	dbg(TRACE,"<HandleMsgFromFIDS> : Request XML Message \n<\n%s>",data);
	
	if (GetKeyItem(clALC3,&llResultLen,data,"<ALC3>","</",TRUE) != NULL)
	{
		dbg(DEBUG,"ALC3 <%s>llResultLen<%d>",clALC3,llResultLen);
	}
	if (GetKeyItem(clFLNO,&llResultLen,data,"<FLNO>","</",TRUE) != NULL)
	{
		dbg(DEBUG,"FLNO <%s>llResultLen<%d>",clFLNO,llResultLen);
	}
	if (GetKeyItem(clSTAD,&llResultLen,data,"<STAD>","</",TRUE) != NULL)
	{
		dbg(DEBUG,"STAD <%s>llResultLen<%d>",clSTAD,llResultLen);
	}
		
	if (GetKeyItem(myTimeFrom,&llResultLen,data,"<timefrom>","</timefrom>",TRUE) != NULL)
	{
		if (llResultLen == 14)
	  {
	    /*** Actual SQL Queries ***/
	    strncpy(myTimeTo,myTimeFrom,14);
	    myBulkTimeSecond = glbBulkTimeHour * SECONDS_PER_HOUR;
      AddSecondsToCEDATime(myTimeTo,myBulkTimeSecond,1);
	    //dbg(TRACE,"<HandleMsgFromFIDS> : Select Time From <%s> To <%s>",myTimeFrom,myTimeTo);
	    //1.get Flight data
	    /*
	    sprintf( clHFMSelection, "SELECT %s FROM AFTTAB WHERE ((TIFD BETWEEN '%s' AND '%s') OR (TIFA BETWEEN '%s' AND '%s')) ",
                                   pcgAftFields,myTimeFrom,myTimeTo,myTimeFrom,myTimeTo );
      */
      
      sprintf( clHFMSelection, "SELECT %s FROM AFTTAB WHERE (FTYP<>'T' AND (((ADID='A' OR ADID='B') AND TIFA BETWEEN '%s' and '%s') OR (ADID='D' AND TIFD BETWEEN '%s' AND '%s'))) OR (FTYP='T' AND ((TIFA BETWEEN '%s' AND '%s') OR (TIFD BETWEEN '%s' AND '%s')))", pcgAftFields,myTimeFrom,myTimeTo,myTimeFrom,myTimeTo,myTimeFrom,myTimeTo,myTimeFrom,myTimeTo);
      
      dbg(TRACE,"<HandleMsgFromFIDS> : clHFMSelection <%s> ",clHFMSelection);
    
      //2.send
      memset(&rlCcarec,0,sizeof(rlCcarec));
      sendAftData(clHFMSelection,rlCcarec,FALSE);
    }
    else
  	  dbg(TRACE,"<HandleMsgFromFIDS> : wrong time format request failed <%s> ",myTimeFrom);
	}//fidupd
	else if (GetKeyItem(myTimeFrom,&llResultLen,data,"<BD_FREM","</BD_FREM>",TRUE) != NULL)
	{
		// handle fids remark message
		if (GetKeyItem(clREM,&llResultLen,data,"<REM>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"REM <%s>llResultLen<%d>",clREM,llResultLen);
		}
		HandleFidsRemark(clALC3,clFLNO,clSTAD,clREM);
	}
	else if (GetKeyItem(myTimeFrom,&llResultLen,data,"<GATE_Flight","</GATE_Flight>",TRUE) != NULL)
	{
		// handle fids Gate Message
		if (GetKeyItem(clGTID,&llResultLen,data,"<GTID>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"GTID <%s>llResultLen<%d>",clGTID,llResultLen);
		}
		if (GetKeyItem(clTERM,&llResultLen,data,"<TERM>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"TERM <%s>llResultLen<%d>",clTERM,llResultLen);
		}
		if (GetKeyItem(clGD1X,&llResultLen,data,"<GD1X>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"GD1X <%s>llResultLen<%d>",clGD1X,llResultLen);
		}
		if (GetKeyItem(clGD1Y,&llResultLen,data,"<GD1Y>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"GD1Y <%s>llResultLen<%d>",clGD1Y,llResultLen);
		}
		
		HandleFidsGate(clALC3,clFLNO,clSTAD,clGTID,clTERM,clGD1X,clGD1Y);
	}
	else if (GetKeyItem(myTimeFrom,&llResultLen,data,"<CHKIN_Flight","</CHKIN_Flight>",TRUE) != NULL)
	{
		// handle Check-in Message
		if (GetKeyItem(clCKID,&llResultLen,data,"<CKID>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"CKID <%s>llResultLen<%d>",clCKID,llResultLen);
		}
		if (GetKeyItem(clTERM,&llResultLen,data,"<TERM>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"TERM <%s>llResultLen<%d>",clTERM,llResultLen);
		}
		if (GetKeyItem(clCKBA,&llResultLen,data,"<CKBA>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"CKBA <%s>llResultLen<%d>",clCKBA,llResultLen);
		}
		if (GetKeyItem(clCKEA,&llResultLen,data,"<CKEA>","</",TRUE) != NULL)
		{
			dbg(DEBUG,"CKEA <%s>llResultLen<%d>",clCKEA,llResultLen);
		}
		HandleFidsCheckIN(clALC3,clFLNO,clSTAD,clCKID,clTERM,clCKBA,clCKEA);
	}	
	
	dbg(DEBUG,"exit HandleMsgFromFIDS");
}//HandleMsgFromFIDS

int HandleFidsGate(char *cpAlc3, char *cpFlno, char *cpStad, char *cpGTID, char* cpTERM, char *cpGD1X, char *cpGD1Y)
{
	char clALC3[NORMAL_COLUMN_LEN] = "\0";
	char clFLNO[NORMAL_COLUMN_LEN] = "\0";
	char clSTAD[NORMAL_COLUMN_LEN] = "\0";
	char clREM[NORMAL_COLUMN_LEN]  = "\0";
	char clFidSelection[1024] = "\0";
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int ilRc = 0;
  int flds_count = 0;
  AFTREC rlAftrec;
  int ilRecordCount = 0;
	char pclCmd[8] = "UFR";
	char pclTab[8] = "AFTTAB";
	char pclSelection[1024] = "\0";
	char pclField[1024] = "\0";
	char pclData[1024] = "\0";
	long llResultLen = 0;

	memset(&rlAftrec,0,sizeof(rlAftrec));
	
	//modification
	/*
	SELECT %s FROM AFTTAB
	WHERE ALC3='%s' AND 
	FTYP IN ('O','S') AND 
	((ADID ='D' AND STOD LIKE '%12.12%%') OR (ADID='A' AND STOA LIKE '%12.12%%'))
	*/
	
	sprintf(clFidSelection,"SELECT %s FROM AFTTAB "
													"WHERE ALC3='%s' AND "
													"FLTN='%s' AND "
													"(ADID='D' OR ADID='B') AND "
													"FTYP NOT IN ('T','G') AND "
													"STOD like '%12.12s%%'",
													pcgAftFields,
													cpAlc3,
													cpFlno,
													cpStad);
	
	 dbg(TRACE,"clFidSelection<%s>",clFidSelection);
	 
	 flds_count = get_flds_count(pcgAftFields);
   dbg(TRACE,"<HandleFidsGate> : flds count <%d>",flds_count);
	 slSqlFunc = START;
	 slCursor = 0;
	 if((ilRc = sql_if(slSqlFunc,&slCursor,clFidSelection,pclDataArea)) == DB_SUCCESS)
	 {	
	 		memset(rlAftrec.URNO,0,sizeof(rlAftrec.URNO));
	 		memset(rlAftrec.ADID,0,sizeof(rlAftrec.ADID));
	 		BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  		dbg(TRACE,"<HandleFidsGate> : pclDataArea <%s>",pclDataArea);
  		ilRecordCount++;
    	slSqlFunc = NEXT;
  		get_fld_value(pclDataArea,igAftUrno,rlAftrec.URNO); TrimRight(rlAftrec.URNO);
  		get_fld_value(pclDataArea,igAftAdid,rlAftrec.ADID); TrimRight(rlAftrec.ADID);
  		
  		dbg(TRACE,"<HandleFidsGate> : URNO <%s>", rlAftrec.URNO);
  		dbg(TRACE,"ADID<%s>",rlAftrec.ADID);
  		
  		if( strlen(rlAftrec.URNO) > 0 && atoi(rlAftrec.URNO) > 0 )
  		{
  			//Found, update the record in AFTTAB
  		
				memset(pclSelection,0,sizeof(pclSelection));
				memset(pclField,0,sizeof(pclField));
				memset(pclData,0,sizeof(pclData));
				sprintf(pclSelection,"WHERE URNO=%s",rlAftrec.URNO);
				
				if(strncmp(rlAftrec.ADID,"B",1)==0)
				{
					dbg(TRACE,"This is a B flight -> GTA1,TGA1,GD1X,GD1Y");
					sprintf(pclField,"GTA1,TGA1,GD1X,GD1Y");
				}
				
				//sprintf(pclField,"GTA1,TGA1,GD1X,GD1Y");
				
				if(strncmp(rlAftrec.ADID,"A",1)==0)
				{
					dbg(TRACE,"This is a arrivial flight -> GTA1,TGA1,GD1X,GD1Y");
					sprintf(pclField,"GTA1,TGA1,GD1X,GD1Y");
				}
				else if(strncmp(rlAftrec.ADID,"D",1)==0)
				{
					dbg(TRACE,"This is a departure flight -> GTD1,TGD1,GD1X,GD1Y");
					sprintf(pclField,"GTD1,TGD1,GD1X,GD1Y");
				}
				
				sprintf(pclData,"%s,%s,%s,%s",cpGTID,cpTERM,cpGD1X,cpGD1Y);
				
				dbg(TRACE,"SendCedaEvent CMD<%s> to MOD<%d> TABLE<%s>",pclCmd,igFlight,pclTab);
				dbg(TRACE,"SendCedaEvent SELECTION<%s>",pclSelection);
				dbg(TRACE,"SendCedaEvent FIELD<%s>",pclField);
				dbg(TRACE,"SendCedaEvent DATA<%s>",pclData);
				
  			//ilRc = SendCedaEvent(igFlight, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection, pclField, pclData, "", 3, 0);
  			ilRc = SendCedaEvent(igFlight, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection, pclField, pclData, "", 3, NETOUT_NO_ACK);
				
  		}
	 }
	 close_my_cursor(&slCursor);
	 dbg(TRACE,"<HandleFidsGate> : records count <%d>",ilRecordCount);
	 
	 dbg(TRACE,"<HandleFidsGate> end");
	 return ilRc;					
}

int HandleFidsCheckIN(char *cpAlc3, char *cpFlno, char *cpStad, char *clCKID,char *clTERM,char *clCKBA,char *clCKEA)
{
	char clALC3[NORMAL_COLUMN_LEN] = "\0";
	char clFLNO[NORMAL_COLUMN_LEN] = "\0";
	char clSTAD[NORMAL_COLUMN_LEN] = "\0";
	char clREM[NORMAL_COLUMN_LEN]  = "\0";
	char clFidSelection[1024] = "\0";
	char clFidSelection1[1024] = "\0";
	short slSqlFunc = 0;
	short slSqlFunc1 = 0;
  short slCursor = 0;
  short slCursor1 = 0;
  char pclDataArea[4096] = "\0";
  //char pclDataArea1[4096] = "\0";
  int ilRc = 0;
  int ilRc1 = 0;
  int flds_count = 0;
  AFTREC rlAftrec;
  char CCATABUrno[20] = "\0";
  int ilRecordCount = 0;
	char pclCmd[8] = "URT";
	char pclTab[8] = "CCATAB";
	char pclSelection[1024] = "\0";
	char pclSelection1[1024] = "\0";
	char pclField[1024] = "\0";
	char pclData[1024] = "\0";
	long llResultLen = 0;

	memset(&rlAftrec,0,sizeof(rlAftrec));
	
	sprintf(clFidSelection,"SELECT %s FROM AFTTAB "
													"WHERE ALC3='%s' AND "
													"FLTN='%s' AND "
													"(ADID ='D' OR ADID='B') AND "
													//"FTYP in ('O','S') AND "
													"FTYP NOT IN ('T','G') AND "
													"STOD LIKE '%12.12s%%'",
													pcgAftFields,
													cpAlc3,
													cpFlno,
													cpStad);
	
	 dbg(TRACE,"clFidSelection<%s>",clFidSelection);
	 
	 flds_count = get_flds_count(pcgAftFields);
   dbg(TRACE,"<HandleFidsCheckIN> : flds count <%d>",flds_count);
	 slSqlFunc = START;
	 slCursor = 0;
	 if((ilRc = sql_if(slSqlFunc,&slCursor,clFidSelection,pclDataArea)) == DB_SUCCESS)
	 {	
	 		memset(rlAftrec.URNO,0,sizeof(rlAftrec.URNO));
	 		BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  		dbg(TRACE,"<HandleFidsCheckIN> : pclDataArea <%s>",pclDataArea);
  		ilRecordCount++;
    	slSqlFunc = NEXT;
  		get_fld_value(pclDataArea,igAftUrno,rlAftrec.URNO); TrimRight(rlAftrec.URNO);
  		
  		dbg(TRACE,"<HandleFidsCheckIN> : URNO <%s>", rlAftrec.URNO);
  		
  		if( strlen(rlAftrec.URNO) > 0 && atoi(rlAftrec.URNO) > 0 )
  		{
  			//Found, update the record in AFTTAB
  		
				memset(pclSelection,0,sizeof(pclSelection));
				memset(pclSelection1,0,sizeof(pclSelection1));
				memset(pclField,0,sizeof(pclField));
				memset(pclData,0,sizeof(pclData));
				//sprintf(pclSelection,"SELECT URNO FROM CCATAB WHERE FLNU='%s' AND CKIC='%s' AND CKIT='%s'",rlAftrec.URNO,clCKID,clTERM);
				
				sprintf(pclSelection,"SELECT URNO FROM CCATAB WHERE FLNU='%s' AND CKIC='%s'",rlAftrec.URNO,clCKID);
				
				slSqlFunc1 = START;
	 			slCursor1 = 0;
	 			if((ilRc = sql_if(slSqlFunc1,&slCursor1,pclSelection,CCATABUrno)) == DB_SUCCESS)
				{
						sprintf(pclSelection1,"WHERE URNO='%s'",CCATABUrno);
						
						sprintf(pclField,"CKBA,CKEA");
						sprintf(pclData,"%s,%s",clCKBA,clCKEA);
						
						dbg(TRACE,"SendCedaEvent CMD<%s> to MOD<%d> TABLE<%s>",pclCmd,igSqlhdl,pclTab);
						dbg(TRACE,"SendCedaEvent SELECTION<%s>",pclSelection1);
						dbg(TRACE,"SendCedaEvent FIELD<%s>",pclField);
						dbg(TRACE,"SendCedaEvent DATA<%s>",pclData);
						
		  			//ilRc = SendCedaEvent(igFlight, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection, pclField, pclData, "", 3, 0);
		  			ilRc = SendCedaEvent(igSqlhdl, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection1, pclField, pclData, "", 3, NETOUT_NO_ACK);
						
				}
				close_my_cursor(&slCursor1);
  		}
	 }
	 close_my_cursor(&slCursor);
	 dbg(TRACE,"<HandleFidsCheckIN> : records count <%d>",ilRecordCount);
	 
	 dbg(TRACE,"<HandleFidsCheckIN> end");
	 return ilRc;					
}

//Frank v1.1
int HandleFidsRemark(char *cpAlc3, char *cpFlno, char *cpStad, char *cpRem)
{
	char clFidSelection[1024] = "\0";
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int ilRc = 0;
  int flds_count = 0;
  AFTREC rlAftrec;
  int ilRecordCount = 0;
	char pclCmd[8] = "UFR";
	char pclTab[8] = "AFTTAB";
	char pclSelection[1024] = "\0";
	char pclField[1024] = "\0";
	char pclData[1024] = "\0";
	long llResultLen = 0;

	memset(&rlAftrec,0,sizeof(rlAftrec));
	
	sprintf(clFidSelection,"SELECT %s FROM AFTTAB "
													"WHERE ALC3='%s' AND "
													"FLTN='%s' AND "
													//"FTYP IN ('O','S') AND "
													"FTYP NOT IN ('T','G') AND "
													"(((ADID='A' OR ADID='B') AND STOA like '%12.12s%%') OR ((ADID='D' OR ADID='B') AND STOD like '%12.12s%%'))",
													pcgAftFields,
													cpAlc3,
													cpFlno,
													cpStad,
													cpStad);
	
	 dbg(TRACE,"clFidSelection<%s>",clFidSelection);
	 
	 flds_count = get_flds_count(pcgAftFields);
   dbg(TRACE,"<HandleFidsRemark> : flds count <%d>",flds_count);
	 slSqlFunc = START;
	 slCursor = 0;
	 while((ilRc = sql_if(slSqlFunc,&slCursor,clFidSelection,pclDataArea)) == DB_SUCCESS)
	 {	
	 		memset(rlAftrec.URNO,0,sizeof(rlAftrec.URNO));
	 		BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  		dbg(TRACE,"<HandleFidsRemark> : pclDataArea <%s>",pclDataArea);
  		ilRecordCount++;
    	slSqlFunc = NEXT;
  		get_fld_value(pclDataArea,igAftUrno,rlAftrec.URNO); TrimRight(rlAftrec.URNO);
  		
  		dbg(TRACE,"<HandleFidsRemark> : URNO <%s>", rlAftrec.URNO);
  		
  		if( strlen(rlAftrec.URNO) > 0 && atoi(rlAftrec.URNO) > 0 )
  		{
  			//Found, update the record in AFTTAB
  			if (ilRc == RC_SUCCESS)
				{
					memset(pclSelection,0,sizeof(pclSelection));
					memset(pclField,0,sizeof(pclField));
					memset(pclData,0,sizeof(pclData));
					sprintf(pclSelection,"WHERE URNO=%s",rlAftrec.URNO);
					sprintf(pclField,"REMP");
					sprintf(pclData,"%s",cpRem);
					
					dbg(TRACE,"SendCedaEvent CMD<%s> to MOD<%d> TABLE<%s>",pclCmd,igFlight,pclTab);
					dbg(TRACE,"SendCedaEvent SELECTION<%s>",pclSelection);
					dbg(TRACE,"SendCedaEvent FIELD<%s>",pclField);
					dbg(TRACE,"SendCedaEvent DATA<%s>",pclData);
					
    			//ilRc = SendCedaEvent(igFlight, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection, pclField, pclData, "", 3, 0);
    			ilRc = SendCedaEvent(igFlight, 0, mod_name, "", "", "",pclCmd, pclTab, pclSelection, pclField, pclData, "", 3, NETOUT_NO_ACK);
    			dbg(TRACE,"ilRc = SendCedaEvent()<%d>",ilRc);
				}
  		}
	 }
	 close_my_cursor(&slCursor);
	 dbg(TRACE,"<HandleFidsRemark> : records count <%d>",ilRecordCount);
	 
	 dbg(TRACE,"<HandleFidsRemark> end");
	 return ilRc;					
}
//Frank v1.1

static void HandleBeat(char *data)
{
	int ilRc = 0;
	char cmd[5] = "XMLO";
	dbg(TRACE,"<HandleBeat> : begin");
	dbg(TRACE,"<HandleBeat> : Beat XML Message \n<\n%s>",data);
	ilRc = SendCedaEvent(glbWMFIDA,0, " ", " ", " ", " ",cmd, " ", " "," ", data, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<HandleBeat> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<HandleBeat> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<HandleBeat> : end");
}

static void HandlePstUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clPstSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandlePstUpdateInsert> : get gate URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clPstSelection, "SELECT %s FROM PSTTAB WHERE URNO = '%s' ",
                                   pcgPstFields, clUrno);
    dbg(TRACE,"<HandlePstUpdateInsert> : clPstSelection <%s> ",clPstSelection);
    sendPstData(clPstSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandlePstUpdateInsert> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}
static void sendPstData(char *mySqlBuf,char *updateOrInsert)
{
	PSTREC rlPstrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendPstData> : sql buf <%s>",mySqlBuf);
	//1.get Gate data
  flds_count = get_flds_count(pcgPstFields);
  dbg(TRACE,"<sendPstData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendPstData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlPstrec, 0, sizeof(rlPstrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igPstUrno,rlPstrec.URNO); TrimRight(rlPstrec.URNO);
    get_fld_value(pclDataArea,igPstPnam,rlPstrec.PNAM); TrimRight(rlPstrec.PNAM);
    
    dbg(DEBUG,"<sendPstData> : URNO <%s>", rlPstrec.URNO);
    dbg(DEBUG,"<sendPstData> : PNAM <%s>", rlPstrec.PNAM);
    
    PackPstXml(rlPstrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendPstData> : records count <%d>",ilRecordCount);
}

int PackBagXml(BAGREC rlBagrec)
{
	int ilRc = 0;
	static char myBagXml[2048] = "\0";
	char cmd[5] = "XMLO";
	 char pclALC3[16] = "\0";
        char pclFLNO[16] = "\0";	

	if( strlen(pcgCodeForALC3) != 0 && igLastPartOfCSGN != 0)
	{
		dbg(TRACE,"Check the ALC3 & FLNO null or not");
	
		ilRc = IsAlc3AndFlnoNullForAdHocFlightBAG(rlBagrec,pclALC3,pclFLNO);
                if( ilRc == RC_SUCCESS)
                {    
                        strcpy(rlBagrec.ALC3,pclALC3);
                        strcpy(rlBagrec.FLTN,pclFLNO);
                } 
	}
	else
	{
		dbg(TRACE,"pcgCodeForALC3 is null and igLastPartOfCSGN is zero, so do not check the ALC3 & FLNO null or not");
	}

	if((strncmp(rlBagrec.ADID,"A",1)) == 0 || (strncmp(rlBagrec.ADID,"B",1)) == 0)
	{
		sprintf( myBagXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
			                    "<BAG_Flight version=\"1.0\">\n"
			                    "<flightIdentification>\n"
			                    "		<ALC3>%s</ALC3>\n"
			                    "		<FLNO>%s</FLNO>\n"
			                    "		<STAD>%s</STAD>\n"
			                    "</flightIdentification>\n"
			                    "<FBAG>%s</FBAG>\n"
			                    "<LBAG>%s</LBAG>\n"
			                    "</BAG_Flight>\n"
			                    "</FIDS_Data>\n",
			                        rlBagrec.ALC3,
			                        rlBagrec.FLTN,
			                        rlBagrec.STOA,
			                        rlBagrec.B1BA,
			                        rlBagrec.B1EA
			                        //rlBagrec.B2BA
			                        );
	}
	/*
	else if((strncmp(rlBagrec.ADID,"D",1)) == 0)
	{
		sprintf( myBagXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
			                    "<BAG_Flight version=\"1.0\">\n"
			                    "<flightIdentification>\n"
			                    "		<ALC3>%s</ALC3>\n"
			                    "		<FLNO>%s</FLNO>\n"
			                    "		<STAD>%s</STAD>\n"
			                    "</flightIdentification>"
			                    "<FBAG>%s</FBAG>\n"
			                    "<LBAG>%s</LBAG>\n"
			                    "</BAG_Flight>\n"
			                    "</FIDS_Data>\n",
			                        rlBagrec.ALC3,
			                        rlBagrec.FLTN,
			                        rlBagrec.STOD,
			                        rlBagrec.B1BA,
			                        rlBagrec.B1EA
			                        );
	} 
	*/         
	
	dbg(TRACE,"<PackBagXml> : Parking Stand XML Message <%s>",myBagXml);
  //ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myBagXml, "", 3, 0) ;
  ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myBagXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackBagXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDF); 
	else
		dbg(DEBUG,"<PackBagXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDF);
  dbg(TRACE,"<PackBagXml> : end");
  
  return ilRc;
}

int PackPstXml(PSTREC rlPstrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myPstXml[2048] = "\0";
	char cmd[5] = "XMLO";

  dbg(TRACE,"<PackPstXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myPstXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<BD_PARK version=\"1.0\">\n"
		                    "   <PRKS>%s</PRKS>\n"
		                    "</BD_PARK>\n"
		                    "</FIDS_Data>\n",
		                        rlPstrec.PNAM
		                        );
	dbg(TRACE,"<PackPstXml> : Parking Stand XML Message <%s>",myPstXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myPstXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackPstXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackPstXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackPstXml> : end");
  
  return ilRc;
}
static void HandleBltUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clBltSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleBltUpdateInsert> : get gate URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clBltSelection, "SELECT %s FROM BLTTAB WHERE URNO = '%s' ",
                                   pcgBltFields, clUrno);
    dbg(TRACE,"<HandleBltUpdateInsert> : clBltSelection <%s> ",clBltSelection);
    sendBltData(clBltSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandleBltUpdateInsert> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}
static void sendBltData(char *mySqlBuf,char *updateOrInsert)
{
	BLTREC rlBltrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendBltData> : sql buf <%s>",mySqlBuf);
	//1.get Gate data
  flds_count = get_flds_count(pcgBltFields);
  dbg(TRACE,"<sendBltData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendBltData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlBltrec, 0, sizeof(rlBltrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igBltUrno,rlBltrec.URNO); TrimRight(rlBltrec.URNO);
    get_fld_value(pclDataArea,igBltBnam,rlBltrec.BNAM); TrimRight(rlBltrec.BNAM);
    get_fld_value(pclDataArea,igBltTerm,rlBltrec.TERM); TrimRight(rlBltrec.TERM);
    
    dbg(DEBUG,"<sendBltData> : URNO <%s>", rlBltrec.URNO);
    dbg(DEBUG,"<sendBltData> : BNAM <%s>", rlBltrec.BNAM);
    dbg(DEBUG,"<sendBltData> : TERM <%s>", rlBltrec.TERM);
    
    PackBltXml(rlBltrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendBltData> : records count <%d>",ilRecordCount);
}
int PackBltXml(BLTREC rlBltrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myBltXml[2048] = "\0";
	char cmd[5] = "XMLO";

  dbg(TRACE,"<PackBltXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myBltXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<BD_BELT version=\"1.0\">\n"
		                    "   <BELT>%s</BELT>\n"
		                    "   <TERM>%s</TERM>\n"
		                    "</BD_BELT>\n"
		                    "</FIDS_Data>\n",
		                        rlBltrec.BNAM,
		                        rlBltrec.TERM
		                        );
	dbg(TRACE,"<PackBltXml> : Belt XML Message <%s>",myBltXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myBltXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackBltXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackBltXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackBltXml> : end");
  
  return ilRc;
}
static void HandleCICUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clCicSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleCICUpdateInsert> : get gate URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clCicSelection, "SELECT %s FROM CICTAB WHERE URNO = '%s' ",
                                   pcgCicFields, clUrno);
    dbg(TRACE,"<HandleCICUpdateInsert> : clCicSelection <%s> ",clCicSelection);
    sendCicData(clCicSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandleAirlineUpdate> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}
static void sendCicData(char *mySqlBuf,char *updateOrInsert)
{
	CICREC rlCicrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendCicData> : sql buf <%s>",mySqlBuf);
	//1.get Gate data
  flds_count = get_flds_count(pcgCicFields);
  dbg(TRACE,"<sendCicData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendCicData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlCicrec, 0, sizeof(rlCicrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igCicUrno,rlCicrec.URNO); TrimRight(rlCicrec.URNO);
    get_fld_value(pclDataArea,igCicCnam,rlCicrec.CNAM); TrimRight(rlCicrec.CNAM);
    get_fld_value(pclDataArea,igCicTerm,rlCicrec.TERM); TrimRight(rlCicrec.TERM);
    
    dbg(DEBUG,"<sendCicData> : URNO <%s>", rlCicrec.URNO);
    dbg(DEBUG,"<sendCicData> : CNAM <%s>", rlCicrec.CNAM);
    dbg(DEBUG,"<sendCicData> : TERM <%s>", rlCicrec.TERM);
    
    PackCicXml(rlCicrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendCicData> : records count <%d>",ilRecordCount);
}
int PackCicXml(CICREC rlCicrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myCicXml[2048] = "\0";
	char cmd[5] = "XMLO";

  dbg(TRACE,"<PackCicXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myCicXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<BD_CHKN version=\"1.0\">\n"
		                    "   <CNAM>%s</CNAM>\n"
		                    "   <TERM>%s</TERM>\n"
		                    "</BD_CHKN>\n"
		                    "</FIDS_Data>\n",
		                        rlCicrec.CNAM,
		                        rlCicrec.TERM
		                        );
	dbg(TRACE,"<PackCicXml> : Cic XML Message <%s>",myCicXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myCicXml, "", 3, 0) ;
  //ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myCicXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackCicXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackCicXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackCicXml> : end");
  
  return ilRc;
}
static void HandleGateUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clGateSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleGateUpdateInsert> : get gate URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clGateSelection, "SELECT %s FROM GATTAB WHERE URNO = '%s' ",
                                   pcgGatFields, clUrno);
    dbg(TRACE,"<HandleGateUpdateInsert> : clGateSelection <%s> ",clGateSelection);
    sendGateData(clGateSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandleAirlineUpdate> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}
static void sendGateData(char *mySqlBuf,char *updateOrInsert)
{
	GATREC rlGatrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendGateData> : sql buf <%s>",mySqlBuf);
	//1.get Gate data
  flds_count = get_flds_count(pcgAptFields);
  dbg(TRACE,"<sendGateData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendGateData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlGatrec, 0, sizeof(rlGatrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igGatUrno,rlGatrec.URNO); TrimRight(rlGatrec.URNO);
    get_fld_value(pclDataArea,igGatGnam,rlGatrec.GNAM); TrimRight(rlGatrec.GNAM);
    get_fld_value(pclDataArea,igGatTerm,rlGatrec.TERM); TrimRight(rlGatrec.TERM);
    
    dbg(DEBUG,"<sendGateData> : URNO <%s>", rlGatrec.URNO);
    dbg(DEBUG,"<sendGateData> : GNAM <%s>", rlGatrec.GNAM);
    dbg(DEBUG,"<sendGateData> : TERM <%s>", rlGatrec.TERM);
    
    PackGateXml(rlGatrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendGateData> : records count <%d>",ilRecordCount);
}
int PackGateXml(GATREC rlGatrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myGateXml[2048] = "\0";
	char cmd[5] = "XMLO";

  dbg(TRACE,"<PackGateXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myGateXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<AODB_BD version=\"1.0\">\n"
		                    "   <GATE>%s</GATE>\n"
		                    "   <TERM>%s</TERM>\n"
		                    "</AODB_BD>\n"
		                    "</FIDS_Data>\n",
		                        rlGatrec.GNAM,
		                        rlGatrec.TERM
		                        );
	dbg(TRACE,"<PackGateXml> : Gate XML Message <%s>",myGateXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myGateXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackGateXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackGateXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackGateXml> : end");
  
  return ilRc;
}
static void HandleAirportUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clAptSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleAirportUpdateInsert> : get airport URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clAptSelection, "SELECT %s FROM APTTAB WHERE URNO = '%s' ",
                                   pcgAptFields, clUrno);
    dbg(TRACE,"<HandleAirportUpdateInsert> : clAptSelection <%s> ",clAptSelection);
    sendAptData(clAptSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandleAirlineUpdate> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}

static void sendAptData(char *mySqlBuf,char *updateOrInsert)
{
	APTREC rlAptrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int ilRc2 = 0;
  short slSqlFunc2 = 0;
  short slCursor2 = 0;
  char pclDataArea2[32] = "\0";
  char  mySqlBuf2[256] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendAptData> : sql buf <%s>",mySqlBuf);
	//1.get Airport data
  flds_count = get_flds_count(pcgAptFields);
  dbg(TRACE,"<sendAptData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendAptData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlAptrec, 0, sizeof(rlAptrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igAltUrno,rlAptrec.URNO); TrimRight(rlAptrec.URNO);
    get_fld_value(pclDataArea,igAltAlc2,rlAptrec.APC3); TrimRight(rlAptrec.APC3);
    get_fld_value(pclDataArea,igAltAlc3,rlAptrec.APC4); TrimRight(rlAptrec.APC4);
    get_fld_value(pclDataArea,igAltAlfn,rlAptrec.APFN); TrimRight(rlAptrec.APFN);
    get_fld_value(pclDataArea,igAltAdd4,rlAptrec.APSN); TrimRight(rlAptrec.APSN);
    
    dbg(DEBUG,"<sendAptData> : URNO <%s>", rlAptrec.URNO);
    dbg(DEBUG,"<sendAptData> : APC3 <%s>", rlAptrec.APC3);
    dbg(DEBUG,"<sendAptData> : APC4 <%s>", rlAptrec.APC4);
    dbg(DEBUG,"<sendAptData> : APFN <%s>", rlAptrec.APFN);
    dbg(DEBUG,"<sendAptData> : APSN <%s>", rlAptrec.APSN);
    
    PackAirportXml(rlAptrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendAptData> : records count <%d>",ilRecordCount);
}

int PackAirportXml(APTREC rlAptrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myAirportXml[2048] = "\0";
	char cmd[5] = "XMLO";
        char	bufout[1024] = "\0";
        //char	xmlout[8192] = "\0";

	ConvertEncoding("CP1256", "UTF-8", rlAptrec.APSN, bufout);
	/*
	if( strncmp(pcgApc3DefaultValueEnable,"YES",3) ==0 )
	{
		dbg(DEBUG,"APC3_DEFAULT_VALUE_ENABLE is set as YES in fidbas.cfg");
		if( strlen(rlAptrec.APC3) == 0 )
		{
			dbg(DEBUG,"APC3 is null, replace it using APC3_DEFAULT_VALUE<%s> in fidbas.cfg",pcgApc3DefaultValue);
			memset(rlAptrec.APC3,0,sizeof(rlAptrec.APC3));
			strcpy(rlAptrec.APC3,pcgApc3DefaultValue);
			
		}
		else
		{
			dbg(DEBUG,"APC3 is not null");
		}	
	}
	else
	{
		dbg(DEBUG,"APC3_DEFAULT_VALUE_ENABLE is not set as YES in fidbas.cfg");
	}
  	*/
	dbg(TRACE,"<PackAirportXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myAirportXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<AODB_BD version=\"1.0\">\n"
		                    "  <AIRP>\n"
		                    "   <TRANS>%s</TRANS>\n"
		                    "   <APC3>%s</APC3>\n"
		                    "   <APC4>%s</APC4>\n"
		                    "   <APFN>%s</APFN>\n"
		                    "   <APN2>%s</APN2>\n"
		                    "  </AIRP>\n"
		                    "</AODB_BD>\n"
		                    "</FIDS_Data>\n",
		                        updateOrInsert,
		                        strlen(rlAptrec.APC3) == 0 ? rlAptrec.APC4 : rlAptrec.APC3,
		                        rlAptrec.APC4,
		                        rlAptrec.APFN,
		                        //rlAptrec.APSN
					bufout
		                        );
		
	//ConvertEncoding("CP1256", "UTF-8", myAirportXml, xmlout);

	dbg(TRACE,"<PackAirportXml> : Airport XML Message <%s>",myAirportXml);
	//dbg(TRACE,"<PackAirportXml> : Airport XML Message <%s>",xmlout);
	
	
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myAirportXml, "", 3, 0) ;
  //ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", xmlout, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackAirportXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackAirportXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackAirportXml> : end");
  
  return ilRc;
}


int ConvertEncoding( char *encFrom, char *encTo, const char *in, char *out)
{
        char	bufout[1024];
	char	*sin;
	char 	*sout;
        int 	lenin;
	int	lenout;
	int	ret;
        iconv_t c_pt;
	
	memset(out,0,sizeof(out));

        if ((c_pt = iconv_open(encTo, encFrom)) == (iconv_t)-1)
        {
                dbg(TRACE,"iconv_open false: %s ==> %s", encFrom, encTo);
                return RC_FAIL;
        }

        iconv(c_pt, NULL, NULL, NULL, NULL);
        lenin  = strlen(in) + 1;
        lenout = 1024;
        sin    = (char *)in;
        sout   = bufout;
        ret = iconv(c_pt, &sin, (size_t *)&lenin, &sout, (size_t *)&lenout);
        if (ret == -1)
        {
                return RC_FAIL;
        }
        iconv_close(c_pt);
	
	strcpy(out,bufout);
        return RC_SUCCESS;
}

//Frank v1.1
static void HandleRemarkMessageUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
     int ilRc = 0;
     char clUrno[21] = "\0";
     char clFidSelection[1024] = "\0";
     ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
     if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
     {
	      dbg(TRACE,"<HandleRemarkMessageUpdateInsert> : get fidtab URNO <%s>",clUrno);
	     
	      /*** SQL Queries ***/
	      sprintf( clFidSelection, "SELECT %s FROM FIDTAB WHERE URNO = '%s' ",
	                                   pcgFidFields, clUrno);
	    	dbg(TRACE,"<HandleRemarkMessageUpdateInsert> : clFidSelection <%s> ",clFidSelection);
	    	sendFidData(clFidSelection,updateOrInsert);
     }
     else
     		dbg(TRACE,"<HandleRemarkMessageUpdateInsert> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}
//Frank v1.1

static void HandleAirlineUpdateInsert(char *fld,char *data,char *updateOrInsert)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clAltSelection[1024] = "\0";
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleAirlineUpdateInsert> : get airline URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clAltSelection, "SELECT %s FROM ALTTAB WHERE URNO = '%s' ",
                                   pcgAltFields, clUrno);
    dbg(TRACE,"<HandleAirlineUpdateInsert> : clAltSelection <%s> ",clAltSelection);
    sendAltData(clAltSelection,updateOrInsert);
	}
	else
		dbg(TRACE,"<HandleAirlineUpdateInsert> : no URNO found pclFields <%s> pclData <%s>",fld,data);
}

//Frank v1.1
static void sendFidData(char *mySqlBuf,char *updateOrInsert)
{
	  FIDREC rlFidrec;
	  int ilRc = 0;
	  int flds_count = 0;
	  int ilRecordCount = 0;
	  short slSqlFunc = 0;
	  short slCursor = 0;
	  char pclDataArea[4096] = "\0";
	   
	   /*** Actual SQL Queries ***/
	   dbg(TRACE,"<sendFidData> : sql buf <%s>",mySqlBuf);
	   //1.get Remark data
	   
	  flds_count = get_flds_count(pcgFidFields);
	  dbg(TRACE,"<sendFidData> : flds count <%d>",flds_count);
	  slSqlFunc = START;
	  slCursor = 0;
	  
	  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
	  {
	      BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
	      dbg(TRACE,"<sendFidData> : pclDataArea <%s>",pclDataArea);
	      memset( &rlFidrec, 0, sizeof(rlFidrec) );
	      ilRecordCount++;
	    	slSqlFunc = NEXT;
	    
	    	get_fld_value(pclDataArea,igFidUrno,rlFidrec.URNO); TrimRight(rlFidrec.URNO);
		    get_fld_value(pclDataArea,igFidCode,rlFidrec.CODE); TrimRight(rlFidrec.CODE);
		    get_fld_value(pclDataArea,igFidBeme,rlFidrec.BEME); TrimRight(rlFidrec.BEME);
		    get_fld_value(pclDataArea,igFidBemd,rlFidrec.BEMD); TrimRight(rlFidrec.BEMD);
	    
		    dbg(DEBUG,"<sendFidData> : URNO <%s>", rlFidrec.URNO);
		    dbg(DEBUG,"<sendFidData> : CODE <%s>", rlFidrec.CODE);
		    dbg(DEBUG,"<sendFidData> : BEME <%s>", rlFidrec.BEME);
		    dbg(DEBUG,"<sendFidData> : BEMD <%s>", rlFidrec.BEMD);
		    
		    PackRemarkXml(rlFidrec,updateOrInsert);
	  }
	  close_my_cursor(&slCursor);
	  dbg(TRACE,"<sendFidData> : records count <%d>",ilRecordCount);
}
//Frank v1.1

static void sendAltData(char *mySqlBuf,char *updateOrInsert)
{
	ALTREC rlAltrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int ilRc2 = 0;
  short slSqlFunc2 = 0;
  short slCursor2 = 0;
  char pclDataArea2[32] = "\0";
  char  mySqlBuf2[256] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendAltData> : sql buf <%s>",mySqlBuf);
	//1.get Airline data
  flds_count = get_flds_count(pcgAltFields);
  dbg(TRACE,"<sendAltData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendAltData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlAltrec, 0, sizeof(rlAltrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igAltUrno,rlAltrec.URNO); TrimRight(rlAltrec.URNO);
    get_fld_value(pclDataArea,igAltAlc2,rlAltrec.ALC2); TrimRight(rlAltrec.ALC2);
    get_fld_value(pclDataArea,igAltAlc3,rlAltrec.ALC3); TrimRight(rlAltrec.ALC3);
    get_fld_value(pclDataArea,igAltAlfn,rlAltrec.ALFN); TrimRight(rlAltrec.ALFN);
    get_fld_value(pclDataArea,igAltAdd4,rlAltrec.ADD4); TrimRight(rlAltrec.ADD4);
    
    dbg(DEBUG,"<sendAltData> : URNO <%s>", rlAltrec.URNO);
    dbg(DEBUG,"<sendAltData> : ALC2 <%s>", rlAltrec.ALC2);
    dbg(DEBUG,"<sendAltData> : ALC3 <%s>", rlAltrec.ALC3);
    dbg(DEBUG,"<sendAltData> : ALFN <%s>", rlAltrec.ALFN);
    dbg(DEBUG,"<sendAltData> : ADD4 <%s>", rlAltrec.ADD4);
    
    PackAirlineXml(rlAltrec,updateOrInsert);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendAltData> : records count <%d>",ilRecordCount);
}

//Frank v1.1
int PackRemarkXml(FIDREC rlFidrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myRemarkXml[2048] = "\0";
	char cmd[5] = "XMLO";

        char	bufout[1024] = "\0";

	ConvertEncoding("CP1256", "UTF-8", rlFidrec.BEMD, bufout);
  
	dbg(TRACE,"<PackRemarkXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myRemarkXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<BD_REM version=\"1.0\">\n"
		                    "   <REMP>%s</REMP>\n"
		                    "   <REME>%s</REME>\n"
		                    "   <REMA>%s</REMA>\n"
		                    "</BD_REM>\n"
		                    "</FIDS_Data>\n",
		                        rlFidrec.CODE,
		                        rlFidrec.BEME,
		                        //rlFidrec.BEMD
					bufout
		           							);
	dbg(TRACE,"<PackRemarkXml> : Remark XML Message <%s>",myRemarkXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myRemarkXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackRemarkXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackRemarkXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackRemarkXml> : end");
  
  return ilRc;
}
//Frank v1.1


int PackAirlineXml(ALTREC rlAltrec,char *updateOrInsert)
{
	int ilRc = 0;
	static char myAirlineXml[2048] = "\0";
	char cmd[5] = "XMLO";

        char bufout[1024] = "\0";

	ConvertEncoding("CP1256", "UTF-8", rlAltrec.ADD4, bufout);
  
	dbg(TRACE,"<PackAirlineXml> : begin updateOrInsert <%s>",updateOrInsert);
	sprintf( myAirlineXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<AODB_BD version=\"1.0\">\n"
		                    "  <AIRL>\n"
		                    "   <TRANS>%s</TRANS>\n"
		                    "   <ALC2>%s</ALC2>\n"
		                    "   <ALC3>%s</ALC3>\n"
		                    "   <AIR_NE>%s</AIR_NE>\n"
		                    "   <AIR_NA>%s</AIR_NA>\n"
		                    "  </AIRL>\n"
		                    "</AODB_BD>\n"
		                    "</FIDS_Data>\n",
		                        updateOrInsert,
		                        //rlAltrec.ALC2,
		                        strlen(rlAltrec.ALC2) == 0 ? rlAltrec.ALC3 : rlAltrec.ALC2,
		                        rlAltrec.ALC3,
		                        rlAltrec.ALFN,
		                        //rlAltrec.ADD4
					bufout
		                        );
	dbg(TRACE,"<PackAirlineXml> : Airline XML Message <%s>",myAirlineXml);
  ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myAirlineXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackAirlineXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDB); 
	else
		dbg(DEBUG,"<PackAirlineXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDB);
  dbg(TRACE,"<PackAirlineXml> : end");
  
  return ilRc;
}
static void HandleCheckinCounterInsert(char *fld,char *data)
{
	HandleCheckinCounterUpdate(fld,data);
}
static void HandleCheckinCounterUpdate(char *fld,char *data)
{
	int ilRc = 0;
	char clCCABTABUrno[21] = "\0";
	char clCcaSelection[1024] = "\0";
	char clAftSelection[1024] = "\0";
	
	CCAREC rlCcarec;
	AFTREC rlAftrec;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int flds_count = 0;
  
  memset(pclDataArea,0,sizeof(pclDataArea));
	
	ilRc = tool_get_field_data (fld, data, "URNO", clCCABTABUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clCCABTABUrno) )
	{
		dbg(TRACE,"<HandleCheckinCounterUpdate> : get counter URNO <%s>",clCCABTABUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clCcaSelection, "SELECT %s FROM CCATAB WHERE URNO = '%s' ",
                                   pcgCcaFields, clCCABTABUrno);
    
    dbg(TRACE,"<HandleCheckinCounterUpdate> : sql buf <%s>",clCcaSelection);
    flds_count = get_flds_count(pcgCcaFields);
    dbg(TRACE,"<HandleCheckinCounterUpdate> : flds count <%d>",flds_count);
    
    slSqlFunc = START;
  	slCursor = 0;
  	
  	if((ilRc = sql_if(slSqlFunc,&slCursor,clCcaSelection,pclDataArea)) == DB_SUCCESS)
  	{
  		BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  		dbg(TRACE,"<HandleCheckinCounterUpdate> : pclDataArea <%s>",pclDataArea);
  		memset( &rlCcarec, 0, sizeof(rlCcarec) );
  		ilRecordCount++;
    	slSqlFunc = NEXT;
    	
    	get_fld_value(pclDataArea,igCcaUrno,rlCcarec.URNO); TrimRight(rlCcarec.URNO);
	    get_fld_value(pclDataArea,igCcaCkic,rlCcarec.CKIC); TrimRight(rlCcarec.CKIC);
	    get_fld_value(pclDataArea,igCcaCkit,rlCcarec.CKIT); TrimRight(rlCcarec.CKIT);
	    get_fld_value(pclDataArea,igCcaCkbs,rlCcarec.CKBS); TrimRight(rlCcarec.CKBS);
	    get_fld_value(pclDataArea,igCcaCkes,rlCcarec.CKES); TrimRight(rlCcarec.CKES);
	    get_fld_value(pclDataArea,igCcaFlno,rlCcarec.FLNO); TrimRight(rlCcarec.FLNO);
	    get_fld_value(pclDataArea,igCcaFlnu,rlCcarec.FLNU); TrimRight(rlCcarec.FLNU);
	    get_fld_value(pclDataArea,igCcaCtyp,rlCcarec.CTYP); TrimRight(rlCcarec.CTYP);
	    
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : URNO <%s>", rlCcarec.URNO);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKIC <%s>", rlCcarec.CKIC);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKIT <%s>", rlCcarec.CKIT);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKBS <%s>", rlCcarec.CKBS);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKES <%s>", rlCcarec.CKES);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : FLNO <%s>", rlCcarec.FLNO);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : FLNU <%s>", rlCcarec.FLNU);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CTYP <%s>", rlCcarec.CTYP);
  	}
  	close_my_cursor(&slCursor);
  	dbg(TRACE,"<HandleCheckinCounterUpdate> : records count <%d>",ilRecordCount);
  	
  	//
  	memset(pclDataArea,0,sizeof(pclDataArea));
  	sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE URNO = '%s' ",pcgAftFields,rlCcarec.FLNU);
  	dbg(TRACE,"<HandleCheckinCounterUpdate> : sql buf <%s>",clAftSelection);
  	flds_count = get_flds_count(pcgAftFields);
    dbg(TRACE,"<HandleCheckinCounterUpdate> : flds count <%d>",flds_count);
    
    slSqlFunc = START;
  	slCursor = 0;
  	
  	if((ilRc = sql_if(slSqlFunc,&slCursor,clAftSelection,pclDataArea)) == DB_SUCCESS)
  	{
  		BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  		dbg(TRACE,"<HandleCheckinCounterUpdate> : pclDataArea <%s>",pclDataArea);
  		memset( &rlAftrec, 0, sizeof(rlAftrec) );
  		ilRecordCount++;
    	slSqlFunc = NEXT;
    	
    	get_fld_value(pclDataArea,igAftUrno,rlAftrec.URNO); TrimRight(rlAftrec.URNO);
	    get_fld_value(pclDataArea,igAftCkif,rlAftrec.CKIF); TrimRight(rlAftrec.CKIF);
	    get_fld_value(pclDataArea,igAftCkit,rlAftrec.CKIT); TrimRight(rlAftrec.CKIT);
	    
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : URNO <%s>", rlAftrec.URNO);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKIF <%s>", rlAftrec.CKIF);
	    dbg(DEBUG,"<HandleCheckinCounterUpdate> : CKIT <%s>", rlAftrec.CKIT);
  	}
  	else
  	{
  		dbg(TRACE,"no record,set rlAftrec.CKIF and rlAftrec.CKIT null");
  		memset(rlAftrec.CKIF,0,sizeof(rlAftrec.CKIF));
  		memset(rlAftrec.CKIT,0,sizeof(rlAftrec.CKIT));
  	}
  	close_my_cursor(&slCursor);

		if(strncmp(rlCcarec.CTYP,"C",1) == 0)
    {
    	//common
    	dbg(TRACE,"This is for common check-in counter update");
    	PackCheckinCounterXml(rlCcarec);
    }
    else
    {
    	dbg(TRACE,"<HandleCheckinCounterUpdate> : To check if the common check-in counter equals the ones assigned in daily mask section ");
    	if( strcmp(rlCcarec.CKIC,rlAftrec.CKIF) == 0 || strcmp(rlCcarec.CKIC,rlAftrec.CKIT) == 0 )
    	{
    		dbg(TRACE,"<HandleCheckinCounterUpdate> : <rlCcarec.CKIC(%s) == rlAftrec.CKIF(%s) || rlCcarec.CKIC(%s) == rlAftrec.CKIT(%s)>",rlCcarec.CKIC,rlAftrec.CKIF,rlCcarec.CKIC,rlAftrec.CKIT);
	    	dbg(TRACE,"<HandleCheckinCounterUpdate> : clAftSelection <%s> ",clAftSelection);
	    
	    	sendAftData(clAftSelection,rlCcarec,TRUE);
	  	}
	  	else
	  	{
	  		dbg(TRACE,"<HandleCheckinCounterUpdate> : <rlCcarec.CKIC(%s) != rlAftrec.CKIF(%s) && rlCcarec.CKIC(%s) != rlAftrec.CKIT(%s)>",
	  		rlCcarec.CKIC,rlAftrec.CKIF,rlCcarec.CKIC,rlAftrec.CKIT);
	  	}
    }
	}
}

static void sendCcaData(char *mySqlBuf)
{
	CCAREC rlCcarec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  int ilRc2 = 0;
  short slSqlFunc2 = 0;
  short slCursor2 = 0;
  char pclDataArea2[32] = "\0";
  char  mySqlBuf2[256] = "\0";
	
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendCcaData> : sql buf <%s>",mySqlBuf);
	//1.get Flight data
  flds_count = get_flds_count(pcgCcaFields);
  dbg(TRACE,"<sendCcaData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  memset(pclDataArea,0,sizeof(pclDataArea));
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendCcaData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlCcarec, 0, sizeof(rlCcarec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igCcaUrno,rlCcarec.URNO); TrimRight(rlCcarec.URNO);
    get_fld_value(pclDataArea,igCcaCkic,rlCcarec.CKIC); TrimRight(rlCcarec.CKIC);
    get_fld_value(pclDataArea,igCcaCkit,rlCcarec.CKIT); TrimRight(rlCcarec.CKIT);
    get_fld_value(pclDataArea,igCcaCkbs,rlCcarec.CKBS); TrimRight(rlCcarec.CKBS);
    get_fld_value(pclDataArea,igCcaCkes,rlCcarec.CKES); TrimRight(rlCcarec.CKES);
    get_fld_value(pclDataArea,igCcaFlno,rlCcarec.FLNO); TrimRight(rlCcarec.FLNO);
    get_fld_value(pclDataArea,igCcaFlnu,rlCcarec.FLNU); TrimRight(rlCcarec.FLNU);
    get_fld_value(pclDataArea,igCcaCtyp,rlCcarec.CTYP); TrimRight(rlCcarec.CTYP);
    
    dbg(DEBUG,"<sendCcaData> : URNO <%s>", rlCcarec.URNO);
    dbg(DEBUG,"<sendCcaData> : CKIC <%s>", rlCcarec.CKIC);
    dbg(DEBUG,"<sendCcaData> : CKIT <%s>", rlCcarec.CKIT);
    dbg(DEBUG,"<sendCcaData> : CKBS <%s>", rlCcarec.CKBS);
    dbg(DEBUG,"<sendCcaData> : CKES <%s>", rlCcarec.CKES);
    dbg(DEBUG,"<sendCcaData> : FLNO <%s>", rlCcarec.FLNO);
    dbg(DEBUG,"<sendCcaData> : FLNU <%s>", rlCcarec.FLNU);
    dbg(DEBUG,"<sendCcaData> : CTYP <%s>", rlCcarec.CTYP);
    
    //get flgiht type
    slSqlFunc2 = START;
    slCursor2 = 0;
    memset(pclDataArea2,0x00,32);
    
    sprintf(mySqlBuf2,"SELECT FTYP FROM AFTTAB WHERE URNO = '%s'",rlCcarec.FLNU);
    //sprintf(mySqlBuf2,"SELECT FTYP FROM AFTTAB WHERE URNO = '23107433'");
    dbg(TRACE,"mySqlBuf2<%s>",mySqlBuf2);
    if ((ilRc2 = sql_if(slSqlFunc2,&slCursor2,mySqlBuf2,pclDataArea2)) == DB_SUCCESS)
    {
    	TrimRight(pclDataArea2);
    	dbg(TRACE,"<sendCcaData> : flight type <%s>",pclDataArea2);
    	//if (strncmp(pclDataArea2,"S",1) == 0 || strncmp(pclDataArea2,"O",1) == 0)
    	if (strncmp(pclDataArea2,"T",1) != 0)
        PackCheckinCounterXml(rlCcarec);
      else
    	  dbg(TRACE,"<sendCcaData> : flight type <%s> checkin counter info not send out",pclDataArea2);
    }
    else
    	dbg(TRACE,"<sendCcaData> : no flight found for checkin counter AFTTAB.URNO = FLNU <%s>",rlCcarec.FLNU);
    close_my_cursor(&slCursor2);
    memset(pclDataArea,0x00,4096);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendCcaData> : records count <%d>",ilRecordCount);
}

int PackCheckinCounterXml(CCAREC rlCcarec)
{
	int ilRc = 0;
	static char myCheckinCounterXml[2048] = "\0";
	char cmd[5] = "XMLO";
	
	memset(myCheckinCounterXml,0x00,2048);

  dbg(TRACE,"<PackCheckinCounterXml> : begin");
	sprintf( myCheckinCounterXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<COM_CK version=\"1.0\">\n"
		                    "   <CKIC>%s</CKIC>\n"
		                    //"   <ALCD>%.2s</ALCD>\n"
		                    "   <ALCD>%s</ALCD>\n"
		                    "   <CKIT>%s</CKIT>\n"
		                    "   <FLBS>%s</FLBS>\n"
		                    "   <FLES>%s</FLES>\n"
		                    "</COM_CK>\n"
		                    "</FIDS_Data>\n",
		                        rlCcarec.CKIC,
		                        rlCcarec.FLNO,
		                        rlCcarec.CKIT,
		                        rlCcarec.CKBS,
		                        rlCcarec.CKES
		                        );
	dbg(TRACE,"<PackCheckinCounterXml> : Checkin Counter XML Message <%s>",myCheckinCounterXml);
  //ilRc = SendCedaEvent(glbWMFIDB,0, " ", " ", " ", " ",cmd, " ", " "," ", myCheckinCounterXml, "", 3, 0) ;
  ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myCheckinCounterXml, "", 3, 0) ;
  if(ilRc==RC_SUCCESS)
		dbg(DEBUG, "<PackCheckinCounterXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDF); 
	else
		dbg(DEBUG,"<PackCheckinCounterXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDF);
  dbg(TRACE,"<PackCheckinCounterXml> : end");
  
  return ilRc;

}
static void HandleFlightUpdate(char *fld,char *data)
{
	int ilRc = 0;
	char clUrno[21] = "\0";
	char clAftSelection[1024] = "\0";
	CCAREC rlCcarec;
	
	ilRc = tool_get_field_data (fld, data, "URNO", clUrno );
	if ( (ilRc == RC_SUCCESS) && !IS_EMPTY (clUrno) )
	{
		dbg(TRACE,"<HandleFlightUpdate> : get flight URNO <%s>",clUrno);
	
	  /*** SQL Queries ***/
	  sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE URNO = '%s' ",
                                   pcgAftFields, clUrno);
    dbg(TRACE,"<HandleFlightUpdate> : clAftSelection <%s> ",clAftSelection);
    
    memset(&rlCcarec,0,sizeof(rlCcarec));
    sendAftData(clAftSelection,rlCcarec,TRUE);
	}
}
static void HandleFlightInsert(char *fld,char *data)
{
   HandleFlightUpdate(fld,data);
}

//static void sendAftData(char *mySqlBuf, CCAREC rpCcarec)
static void sendAftData(char *mySqlBuf, CCAREC rpCcarec,int ilFlag)
{
	//need use aftUrno find out the checkin counter if departure flight
	AFTREC rlAftrec;
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[4096] = "\0";
  
  //Frank v1.1
  CCAREC rlCcarecCKBS;
  CCAREC rlCcarecCKES;
  int flds_count1 = 0;
  short slSqlFunc1 = 0;
  short slCursor1 = 0;
  char pclDataArea1[4096] = "\0";
  char pclCCASqlbuf[4096] = "\0";
  
  CICREC rlCicrec;
  int flds_count2 = 0;
  short slSqlFunc2 = 0;
  short slCursor2 = 0;
  char pclDataArea2[4096] = "\0";
  char pclCICSqlbuf[4096] = "\0";
  
  CCAREC rlCcarec;
  int flds_count3 = 0;
  short slSqlFunc3 = 0;
  short slCursor3 = 0;
  char pclDataArea3[4096] = "\0";
  char pclCCASqlbuf1[4096] = "\0";
  
  //int ilFoundCKBS = FALSE;
  //int ilFoundCKES = FALSE;
  memset( &rlCcarecCKBS, 0, sizeof(rlCcarecCKBS) );
  memset( &rlCcarecCKES, 0, sizeof(rlCcarecCKES) );
  //Frank v1.1
  
  memset( &rlCcarec, 0, sizeof(rlCcarec) );
 
	/*** Actual SQL Queries ***/
	dbg(TRACE,"<sendAftData> : sql buf <%s>",mySqlBuf);
	//1.get Flight data
  flds_count = get_flds_count(pcgAftFields);
  dbg(TRACE,"<sendAftData> : flds count <%d>",flds_count);
  slSqlFunc = START;
  slCursor = 0;
  
  while((ilRc = sql_if(slSqlFunc,&slCursor,mySqlBuf,pclDataArea)) == DB_SUCCESS)
  {
  	BuildItemBuffer(pclDataArea, NULL, flds_count, ",");
  	dbg(TRACE,"<sendAftData> : pclDataArea <%s>",pclDataArea);
  	memset( &rlAftrec, 0, sizeof(rlAftrec) );
  	ilRecordCount++;
    slSqlFunc = NEXT;
    
    get_fld_value(pclDataArea,igAftUrno,rlAftrec.URNO); TrimRight(rlAftrec.URNO);
    get_fld_value(pclDataArea,igAftAlc3,rlAftrec.ALC3); TrimRight(rlAftrec.ALC3);
    get_fld_value(pclDataArea,igAftFltn,rlAftrec.FLTN); TrimRight(rlAftrec.FLTN);
    get_fld_value(pclDataArea,igAftFlno,rlAftrec.FLNO); TrimRight(rlAftrec.FLNO);
    get_fld_value(pclDataArea,igAftStod,rlAftrec.STOD); TrimRight(rlAftrec.STOD);
//    dbg(TRACE,"<sendAftBulkData> : 1");
    get_fld_value(pclDataArea,igAftStoa,rlAftrec.STOA); TrimRight(rlAftrec.STOA);
    get_fld_value(pclDataArea,igAftCkif,rlAftrec.CKIF); TrimRight(rlAftrec.CKIF);
    get_fld_value(pclDataArea,igAftCkit,rlAftrec.CKIT); TrimRight(rlAftrec.CKIT);
    get_fld_value(pclDataArea,igAftGa1f,rlAftrec.GA1F); TrimRight(rlAftrec.GA1F);
    get_fld_value(pclDataArea,igAftGa1b,rlAftrec.GA1B); TrimRight(rlAftrec.GA1B);
    get_fld_value(pclDataArea,igAftGa1e,rlAftrec.GA1E); TrimRight(rlAftrec.GA1E);
    get_fld_value(pclDataArea,igAftAct3,rlAftrec.ACT3); TrimRight(rlAftrec.ACT3);
    
    //Frank v1.8 20130228
    get_fld_value(pclDataArea,igAftAct5,rlAftrec.ACT5); TrimRight(rlAftrec.ACT5);
    //Frank v1.8 20130228
    
    get_fld_value(pclDataArea,igAftOnbl,rlAftrec.ONBL); TrimRight(rlAftrec.ONBL);
//    dbg(TRACE,"<sendAftBulkData> : 2");
    get_fld_value(pclDataArea,igAftOfbl,rlAftrec.OFBL); TrimRight(rlAftrec.OFBL);
    get_fld_value(pclDataArea,igAftOnbe,rlAftrec.ONBE); TrimRight(rlAftrec.ONBE);
    get_fld_value(pclDataArea,igAftStyp,rlAftrec.STYP); TrimRight(rlAftrec.STYP);
    get_fld_value(pclDataArea,igAftTtyp,rlAftrec.TTYP); TrimRight(rlAftrec.TTYP);
    get_fld_value(pclDataArea,igAftAdid,rlAftrec.ADID); TrimRight(rlAftrec.ADID);
    
    //Frank v1.1
    get_fld_value(pclDataArea,igAftPsta,rlAftrec.PSTA); TrimRight(rlAftrec.PSTA);
    get_fld_value(pclDataArea,igAftPstd,rlAftrec.PSTD); TrimRight(rlAftrec.PSTD);
    get_fld_value(pclDataArea,igAftGta1,rlAftrec.GTA1); TrimRight(rlAftrec.GTA1);
    get_fld_value(pclDataArea,igAftGtd1,rlAftrec.GTD1); TrimRight(rlAftrec.GTD1);
    
    get_fld_value(pclDataArea,igAftWro1,rlAftrec.WRO1); TrimRight(rlAftrec.WRO1);
    get_fld_value(pclDataArea,igAftWro2,rlAftrec.WRO2); TrimRight(rlAftrec.WRO2);
    
    get_fld_value(pclDataArea,igAftEtai,rlAftrec.ETAI); TrimRight(rlAftrec.ETAI);
    get_fld_value(pclDataArea,igAftEtdi,rlAftrec.ETDI); TrimRight(rlAftrec.ETDI);
    //Frank v1.1
    
    
//    dbg(TRACE,"<sendAftBulkData> : 3");
    get_fld_value(pclDataArea,igAftEtod,rlAftrec.ETOD); TrimRight(rlAftrec.ETOD);
    get_fld_value(pclDataArea,igAftEtoa,rlAftrec.ETOA); TrimRight(rlAftrec.ETOA);
    
    //Frank v1.1
		get_fld_value(pclDataArea,igAftLand,rlAftrec.LAND); TrimRight(rlAftrec.LAND);
		get_fld_value(pclDataArea,igAftAirb,rlAftrec.AIRB); TrimRight(rlAftrec.AIRB);
		//Frank v1.1
    
    get_fld_value(pclDataArea,igAftCtot,rlAftrec.CTOT); TrimRight(rlAftrec.CTOT);
    get_fld_value(pclDataArea,igAftRegn,rlAftrec.REGN); TrimRight(rlAftrec.REGN);
    
    //Frank v1.1
    get_fld_value(pclDataArea,igAftTmoa,rlAftrec.TMOA); TrimRight(rlAftrec.TMOA);
    //Frank v1.1
    
    get_fld_value(pclDataArea,igAftPaxt,rlAftrec.PAXT); TrimRight(rlAftrec.PAXT);
//    dbg(TRACE,"<sendAftBulkData> : 4");

		//Frank v1.1
    get_fld_value(pclDataArea,igAftBlt1,rlAftrec.BLT1); TrimRight(rlAftrec.BLT1);
    get_fld_value(pclDataArea,igAftBlt2,rlAftrec.BLT2); TrimRight(rlAftrec.BLT2);
    //Frank v1.1
    
   	//get_fld_value(pclDataArea,igAftVia3,rlAftrec.VIA3); TrimRight(rlAftrec.VIA3);
    
//    dbg(TRACE,"<sendAftBulkData> : 5");
//    dbg(TRACE,"<sendAftBulkData> : pclDataArea <%s>",pclDataArea);
    get_fld_value(pclDataArea,igAftCsgn,rlAftrec.CSGN); TrimRight(rlAftrec.CSGN);
//    dbg(TRACE,"<sendAftBulkData> : 6");
    get_fld_value(pclDataArea,igAftFtyp,rlAftrec.FTYP); TrimRight(rlAftrec.FTYP);
//    dbg(TRACE,"<sendAftBulkData> : 22");
    get_fld_value(pclDataArea,igAftNxti,rlAftrec.NXTI); TrimRight(rlAftrec.NXTI);
    
    get_fld_value(pclDataArea,igAftVian,rlAftrec.VIAN); TrimRight(rlAftrec.VIAN);
    get_fld_value(pclDataArea,igAftVial,rlAftrec.VIAL); TrimSpace(rlAftrec.VIAL);//TrimRight(rlAftrec.VIAL);
    
    get_fld_value(pclDataArea,igAftAurn,rlAftrec.AURN); TrimRight(rlAftrec.AURN);
    get_fld_value(pclDataArea,igAftRtow,rlAftrec.RTOW); TrimRight(rlAftrec.RTOW);
    get_fld_value(pclDataArea,igAftTga1,rlAftrec.TGA1); TrimRight(rlAftrec.TGA1);
    get_fld_value(pclDataArea,igAftTgd1,rlAftrec.TGD1); TrimRight(rlAftrec.TGD1);
    get_fld_value(pclDataArea,igAftSlou,rlAftrec.SLOU); TrimRight(rlAftrec.SLOU);
    get_fld_value(pclDataArea,igAftJfno,rlAftrec.JFNO); TrimRight(rlAftrec.JFNO);
    get_fld_value(pclDataArea,igAftJcnt,rlAftrec.JCNT); TrimRight(rlAftrec.JCNT);
		get_fld_value(pclDataArea,igAftGd1b,rlAftrec.GD1B); TrimRight(rlAftrec.GD1B);
    get_fld_value(pclDataArea,igAftGd1e,rlAftrec.GD1E); TrimRight(rlAftrec.GD1E);
    get_fld_value(pclDataArea,igAftOrg3,rlAftrec.ORG3); TrimRight(rlAftrec.ORG3);
    get_fld_value(pclDataArea,igAftDes3,rlAftrec.DES3); TrimRight(rlAftrec.DES3);
    get_fld_value(pclDataArea,igAftStev,rlAftrec.STEV); TrimRight(rlAftrec.STEV);
    
    get_fld_value(pclDataArea,igAftTifa,rlAftrec.TIFA); TrimRight(rlAftrec.TIFA);
    get_fld_value(pclDataArea,igAftTifd,rlAftrec.TIFD); TrimRight(rlAftrec.TIFD);
    
    //Frank v1.5 20120918
    get_fld_value(pclDataArea,igAftOrg4,rlAftrec.ORG4); TrimRight(rlAftrec.ORG4);
    get_fld_value(pclDataArea,igAftDes4,rlAftrec.DES4); TrimRight(rlAftrec.DES4);
    get_fld_value(pclDataArea,igAftFlns,rlAftrec.FLNS); TrimRight(rlAftrec.FLNS);
    
    //dbg(TRACE,"<sendAftBulkData> : pclDataArea 2 <%s>",pclDataArea);
    dbg(DEBUG,"<sendAftData> : URNO <%s>", rlAftrec.URNO);
    dbg(DEBUG,"<sendAftData> : ALC3 <%s>", rlAftrec.ALC3);
    dbg(DEBUG,"<sendAftData> : FLTN <%s>", rlAftrec.FLTN);
    dbg(DEBUG,"<sendAftData> : FLNO <%s>", rlAftrec.FLNO);
    dbg(DEBUG,"<sendAftData> : STOD <%s>", rlAftrec.STOD);
    dbg(DEBUG,"<sendAftData> : STOA <%s>", rlAftrec.STOA);
    dbg(DEBUG,"<sendAftData> : CKIF <%s>", rlAftrec.CKIF);
    dbg(DEBUG,"<sendAftData> : CKIT <%s>", rlAftrec.CKIT);
    dbg(DEBUG,"<sendAftData> : GA1F <%s>", rlAftrec.GA1F);
    dbg(DEBUG,"<sendAftData> : GA1B <%s>", rlAftrec.GA1B);
    dbg(DEBUG,"<sendAftData> : GA1E <%s>", rlAftrec.GA1E);
    dbg(DEBUG,"<sendAftData> : ACT3 <%s>", rlAftrec.ACT3);
    
    //Frank v1.8 20130228
    dbg(DEBUG,"<sendAftData> : ACT5 <%s>", rlAftrec.ACT5);
    
    dbg(DEBUG,"<sendAftData> : ONBL <%s>", rlAftrec.ONBL);
    dbg(DEBUG,"<sendAftData> : OFBL <%s>", rlAftrec.OFBL);
    dbg(DEBUG,"<sendAftData> : ONBE <%s>", rlAftrec.ONBE);
    dbg(DEBUG,"<sendAftData> : STYP <%s>", rlAftrec.STYP);
    dbg(DEBUG,"<sendAftData> : TTYP <%s>", rlAftrec.TTYP);
    dbg(DEBUG,"<sendAftData> : ADID <%s>", rlAftrec.ADID);
    
    //Frank v1.1
    dbg(DEBUG,"<sendAftData> : PSTA <%s>", rlAftrec.PSTA);
    dbg(DEBUG,"<sendAftData> : PSTD <%s>", rlAftrec.PSTD);
    dbg(DEBUG,"<sendAftData> : GTA1 <%s>", rlAftrec.GTA1);
    dbg(DEBUG,"<sendAftData> : GTD1 <%s>", rlAftrec.GTD1);
    dbg(DEBUG,"<sendAftData> : WRO1 <%s>", rlAftrec.WRO1);
    dbg(DEBUG,"<sendAftData> : WRO2 <%s>", rlAftrec.WRO2);
    dbg(DEBUG,"<sendAftData> : ETAI <%s>", rlAftrec.ETAI);
    dbg(DEBUG,"<sendAftData> : ETDI <%s>", rlAftrec.ETDI);
    //Frank v1.1
    
    dbg(DEBUG,"<sendAftData> : ETOD <%s>", rlAftrec.ETOD);
    dbg(DEBUG,"<sendAftData> : ETOA <%s>", rlAftrec.ETOA);
    
    //Frank v1.1
    dbg(DEBUG,"<sendAftData> : LAND <%s>", rlAftrec.LAND);
    dbg(DEBUG,"<sendAftData> : AIRB <%s>", rlAftrec.AIRB);
    //Frank v1.1
    
    dbg(DEBUG,"<sendAftData> : CTOT <%s>", rlAftrec.CTOT);
    dbg(DEBUG,"<sendAftData> : REGN <%s>", rlAftrec.REGN);
    
    //Frank v1.1
    dbg(DEBUG,"<sendAftData> : TMOA <%s>", rlAftrec.TMOA);
    //Frank v1.1
    
    dbg(DEBUG,"<sendAftData> : PAXT <%s>", rlAftrec.PAXT);
    
    //Frank v1.1
    dbg(DEBUG,"<sendAftData> : BLT1 <%s>", rlAftrec.BLT1);
    dbg(DEBUG,"<sendAftData> : BLT2 <%s>", rlAftrec.BLT2);
    //Frank v1.1
    
    //dbg(DEBUG,"<sendAftData> : VIA3 <%s>", rlAftrec.VIA3);
    dbg(DEBUG,"<sendAftData> : CSGN <%s>", rlAftrec.CSGN);
    dbg(DEBUG,"<sendAftData> : FTYP <%s>", rlAftrec.FTYP);
    dbg(DEBUG,"<sendAftData> : NXTI <%s>", rlAftrec.NXTI);
    
    dbg(DEBUG,"<sendAftData> : VIAN <%s>", rlAftrec.VIAN);
    dbg(DEBUG,"<sendAftData> : VIAL <%s>", rlAftrec.VIAL);
    dbg(DEBUG,"<sendAftData> : AURN <%s>", rlAftrec.AURN);
    dbg(DEBUG,"<sendAftData> : RTOW <%s>", rlAftrec.RTOW);
    dbg(DEBUG,"<sendAftData> : TGA1 <%s>", rlAftrec.TGA1);
    dbg(DEBUG,"<sendAftData> : TGD1 <%s>", rlAftrec.TGD1);
    dbg(DEBUG,"<sendAftData> : SLOU <%s>", rlAftrec.SLOU);
    dbg(DEBUG,"<sendAftData> : JFNO <%s>", rlAftrec.JFNO);
    dbg(DEBUG,"<sendAftData> : JCNT <%s>", rlAftrec.JCNT);
		dbg(DEBUG,"<sendAftData> : GD1B <%s>", rlAftrec.GD1B);
    dbg(DEBUG,"<sendAftData> : GD1E <%s>", rlAftrec.GD1E);
    dbg(DEBUG,"<sendAftData> : ORG3 <%s>", rlAftrec.ORG3);
    dbg(DEBUG,"<sendAftData> : DES3 <%s>", rlAftrec.DES3);
    dbg(DEBUG,"<sendAftData> : STEV <%s>", rlAftrec.STEV);
    
    dbg(DEBUG,"<sendAftData> : TIFA <%s>", rlAftrec.TIFA);
    dbg(DEBUG,"<sendAftData> : TIFD <%s>", rlAftrec.TIFD);
    
    //Frank v1.5 20120918
    dbg(DEBUG,"<sendAftData> : ORG4 <%s>", rlAftrec.ORG4);
    dbg(DEBUG,"<sendAftData> : DES4 <%s>", rlAftrec.DES4);
    dbg(DEBUG,"<sendAftData> : FLNS <%s>", rlAftrec.FLNS);
    //cictab
    
    memset( &rlCicrec, 0, sizeof(rlCicrec) );
    flds_count2 = get_flds_count(pcgCicFields);
    dbg(TRACE,"<sendAftData> : flds count2 <%d>",flds_count2);
    slSqlFunc2 = START;
	  slCursor2 = 0;
    sprintf( pclCICSqlbuf, "SELECT %s FROM CICTAB WHERE CNAM IN ('%s','%s')",pcgCicFields, rlAftrec.CKIF,rlAftrec.CKIT);
    dbg(TRACE,"<sendAftData> : sql buf <%s> ",pclCICSqlbuf);
    if((ilRc = sql_if(slSqlFunc2,&slCursor2,pclCICSqlbuf,pclDataArea2)) == DB_SUCCESS)
    {
    	BuildItemBuffer(pclDataArea2, NULL, flds_count2, ",");
    	dbg(TRACE,"<sendAftData> : pclDataArea2 <%s>",pclDataArea2);
    	memset( &rlCicrec, 0, sizeof(rlCicrec) );
    	
    	get_fld_value(pclDataArea2,igCicUrno,rlCicrec.URNO); TrimRight(rlCicrec.URNO);
		  get_fld_value(pclDataArea2,igCicCnam,rlCicrec.CNAM); TrimRight(rlCicrec.CNAM);
		  get_fld_value(pclDataArea2,igCicTerm,rlCicrec.TERM); TrimRight(rlCicrec.TERM);
		  dbg(TRACE,"<sendAftData> : URNO <%s>", rlCicrec.URNO);
		  dbg(TRACE,"<sendAftData> : CNAM <%s>", rlCicrec.CNAM);
		  dbg(TRACE,"<sendAftData> : TERM <%s>", rlCicrec.TERM);		  
    }
    close_my_cursor(&slCursor2);
    
    //test towing
    //memset(rlAftrec.FTYP,0,sizeof(rlAftrec.FTYP));
    //strncpy(rlAftrec.FTYP,"T",1);
    
    //ccatab
    if (strncmp(rlAftrec.FTYP,"T",1) != 0)
    {
    	flds_count3 = get_flds_count(pcgCcaFields);
    	dbg(TRACE,"<sendAftData> : flds_count3 <%d>",flds_count3);
    	slSqlFunc3 = START;
    	slCursor3 = 0;
    	sprintf(pclCCASqlbuf1,"SELECT %s FROM CCATAB WHERE FLNU='%s' AND CTYP<>'C' AND CKIC<>' '",pcgCcaFields,rlAftrec.URNO);
    	dbg(TRACE,"<sendAftData> : sql buf <%s>",pclCCASqlbuf1);
    	memset(pclDataArea3,0,sizeof(pclDataArea3));
    	
    	if((ilRc = sql_if(slSqlFunc3,&slCursor3,pclCCASqlbuf1,pclDataArea3))==DB_SUCCESS)
    	{
    		BuildItemBuffer(pclDataArea3, NULL, flds_count3, ",");
    		dbg(TRACE,"<sendAftData><ilRc-%d><line-%d><pclDataArea3-%s>",ilRc,__LINE__,pclDataArea3);
    		memset( &rlCcarec, 0, sizeof(rlCcarec) );
    		get_fld_value(pclDataArea3,igCcaUrno,rlCcarec.URNO); TrimRight(rlCcarec.URNO);
			  //get_fld_value(pclDataArea3,igCcaCkit,rlCcarec.CKIT); TrimRight(rlCcarec.CKIT);
			  //dbg(TRACE,"<sendAftData> : CKIC <%s>", rlCcarec.CKIC);
			  dbg(TRACE,"<sendAftData> : URNO <%s>", rlCcarec.URNO);
			  
    		//if( strlen(rlCcarec.CKIC)==0 && strlen(rlCcarec.CKIT)==0 )
    		if(strlen(rlCcarec.URNO)==0)
    		{
    			dbg(TRACE,"<sendAftData><pclDataArea3-%s><line-%d> FOUND - Zero result",pclDataArea3,__LINE__);
    			memset(&rlCcarecCKBS,0,sizeof(rlCcarecCKBS));
					memset(&rlCcarecCKES,0,sizeof(rlCcarecCKES));
					memset(&rlCicrec,0,sizeof(rlCicrec));
					memset(rlAftrec.CKIF,0,sizeof(rlAftrec.CKIF));
					memset(rlAftrec.CKIT,0,sizeof(rlAftrec.CKIT));
					
					if(ilFlag==TRUE)
					{
						if(IsFlightInTimeWindow( rlAftrec.ADID, rlAftrec.TIFA, rlAftrec.TIFD )==RC_SUCCESS)
				  	{
							PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
    				}
    			}
    			else if(ilFlag==FALSE)
    			{
    				PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
    			}
    		}
    		else if(strlen(pclDataArea3)!=0 && strncmp(pclDataArea3," ",1)!=0)
    		{
	    		dbg(TRACE,"<sendAftData><pclDataArea3-%s><line-%d> FOUND - Non Zero result",pclDataArea3,__LINE__);
	    		
			    // get ccatab data URNO,CKBS,CKES
			    flds_count1 = get_flds_count(pcgCcaFields);
			  	dbg(TRACE,"<sendAftData> : flds_count1 <%d>",flds_count1);
			    slSqlFunc1 = START;
			  	slCursor1 = 0;
			    sprintf(pclCCASqlbuf,"SELECT %s FROM CCATAB WHERE FLNU='%s' AND CKIC='%s'",pcgCcaFields,rlAftrec.URNO,rlAftrec.CKIF);
			    
			    if((ilRc = sql_if(slSqlFunc1,&slCursor1,pclCCASqlbuf,pclDataArea1)) == DB_SUCCESS)
			  	{
			  		BuildItemBuffer(pclDataArea1, NULL, flds_count1, ",");
			  		dbg(TRACE,"<sendAftData> : pclDataArea1 <%s>",pclDataArea1);
			  		memset( &rlCcarecCKBS, 0, sizeof(rlCcarecCKBS) );
			    
			    	get_fld_value(pclDataArea1,igCcaUrno,rlCcarecCKBS.URNO); TrimRight(rlCcarecCKBS.URNO);
			    	get_fld_value(pclDataArea1,igCcaCkic,rlCcarecCKBS.CKIC); TrimRight(rlCcarecCKBS.CKIC);
			    	get_fld_value(pclDataArea1,igCcaCkit,rlCcarecCKBS.CKIT); TrimRight(rlCcarecCKBS.CKIT);
			    	get_fld_value(pclDataArea1,igCcaCkbs,rlCcarecCKBS.CKBS); TrimRight(rlCcarecCKBS.CKBS);
			    	get_fld_value(pclDataArea1,igCcaCkes,rlCcarecCKBS.CKES); TrimRight(rlCcarecCKBS.CKES);
			    	get_fld_value(pclDataArea1,igCcaFlno,rlCcarecCKBS.FLNO); TrimRight(rlCcarecCKBS.FLNO);
			    	get_fld_value(pclDataArea1,igCcaFlnu,rlCcarecCKBS.FLNU); TrimRight(rlCcarecCKBS.FLNU);
			    	
			    	dbg(DEBUG,"<sendAftData> : line <%d> CKBS", __LINE__);
			    	dbg(TRACE,"<sendAftData> : URNO <%s>", rlCcarecCKBS.URNO);
			    	dbg(TRACE,"<sendAftData> : CKIC <%s>", rlCcarecCKBS.CKIC);
			    	dbg(TRACE,"<sendAftData> : CKIT <%s>", rlCcarecCKBS.CKIT);
			    	dbg(TRACE,"<sendAftData> : CKBS <%s>", rlCcarecCKBS.CKBS);
			    	dbg(TRACE,"<sendAftData> : CKES <%s>", rlCcarecCKBS.CKES);
			    	dbg(TRACE,"<sendAftData> : FLNO <%s>", rlCcarecCKBS.FLNO);
			    	dbg(TRACE,"<sendAftData> : FLNU <%s>", rlCcarecCKBS.FLNU);
				  }
				  close_my_cursor(&slCursor1);
				  
				  strcpy(rlCcarecCKES.CKES,rlCcarecCKBS.CKES);
				  memset(pclCCASqlbuf,0,sizeof(pclCCASqlbuf));
				  memset(pclDataArea1,0,sizeof(pclDataArea1));
				  flds_count1 = get_flds_count(pcgCcaFields);
				  dbg(TRACE,"<sendAftData> : flds_count1 <%d>",flds_count1);
		    	slSqlFunc1 = START;
		  		slCursor1 = 0;
		    	sprintf(pclCCASqlbuf,"SELECT %s FROM CCATAB WHERE FLNU='%s' AND CKIC='%s'",pcgCcaFields,rlAftrec.URNO,rlAftrec.CKIT);
		    	dbg(TRACE,"<sendAftData> : sql buf <%s>",pclCCASqlbuf);
				  
				  if((ilRc = sql_if(slSqlFunc1,&slCursor1,pclCCASqlbuf,pclDataArea1)) == DB_SUCCESS)
			  	{
			  		BuildItemBuffer(pclDataArea1, NULL, flds_count1, ",");
			  		dbg(TRACE,"<sendAftData> : pclDataArea1 <%s>",pclDataArea1);
			  		memset( &rlCcarecCKES, 0, sizeof(rlCcarecCKES) );
			    
			    	get_fld_value(pclDataArea1,igCcaUrno,rlCcarecCKES.URNO); TrimRight(rlCcarecCKES.URNO);
			    	get_fld_value(pclDataArea1,igCcaCkic,rlCcarecCKES.CKIC); TrimRight(rlCcarecCKES.CKIC);
			    	get_fld_value(pclDataArea1,igCcaCkit,rlCcarecCKES.CKIT); TrimRight(rlCcarecCKES.CKIT);
			    	get_fld_value(pclDataArea1,igCcaCkbs,rlCcarecCKES.CKBS); TrimRight(rlCcarecCKES.CKBS);
			    	get_fld_value(pclDataArea1,igCcaCkes,rlCcarecCKES.CKES); TrimRight(rlCcarecCKES.CKES);
			    	get_fld_value(pclDataArea1,igCcaFlno,rlCcarecCKES.FLNO); TrimRight(rlCcarecCKES.FLNO);
			    	get_fld_value(pclDataArea1,igCcaFlnu,rlCcarecCKES.FLNU); TrimRight(rlCcarecCKES.FLNU);
			    	
			    	dbg(DEBUG,"<sendAftData> : line <%d> CKES", __LINE__);
			    	dbg(TRACE,"<sendAftData> : URNO <%s>", rlCcarecCKES.URNO);
			    	dbg(TRACE,"<sendAftData> : CKIC <%s>", rlCcarecCKES.CKIC);
			    	dbg(TRACE,"<sendAftData> : CKIT <%s>", rlCcarecCKES.CKIT);
			    	dbg(TRACE,"<sendAftData> : CKBS <%s>", rlCcarecCKES.CKBS);
			    	dbg(TRACE,"<sendAftData> : CKES <%s>", rlCcarecCKES.CKES);
			    	dbg(TRACE,"<sendAftData> : FLNO <%s>", rlCcarecCKES.FLNO);
			    	dbg(TRACE,"<sendAftData> : FLNU <%s>", rlCcarecCKES.FLNU);
				  }
				  close_my_cursor(&slCursor1);
				  
				  //Frank 20121105 v1.7
				  if(ilFlag==TRUE)
					{
					  if( IsFlightInTimeWindow( rlAftrec.ADID, rlAftrec.TIFA, rlAftrec.TIFD ) == RC_SUCCESS )
					  {
					  	PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
						}
					}
					else if(ilFlag==FALSE)
					{
						PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
					}
				}
			}
			else
			{
				dbg(TRACE,"<sendAftData><line-%d>All CTYP='C',CKIF,CKIT and TERM are filled into blank",__LINE__);
				
				memset(&rlCcarecCKBS,0,sizeof(rlCcarecCKBS));
				memset(&rlCcarecCKES,0,sizeof(rlCcarecCKES));
				memset(&rlCicrec,0,sizeof(rlCicrec));
				memset(rlAftrec.CKIF,0,sizeof(rlAftrec.CKIF));
				memset(rlAftrec.CKIT,0,sizeof(rlAftrec.CKIT));
				
				if(ilFlag==TRUE)
				{
					if( IsFlightInTimeWindow( rlAftrec.ADID, rlAftrec.TIFA, rlAftrec.TIFD ) == RC_SUCCESS )
					{
						PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
					}
				}
				else if(ilFlag==FALSE)
				{
					PackFlightXml(rlAftrec,rlCcarecCKBS,rlCcarecCKES,rlCicrec);
				}
			}
			close_my_cursor(&slCursor3);
		}
		else if (strncmp(rlAftrec.FTYP,"T",1) == 0)
		{
		   //make towing xml
		   PackTowingXml(rlAftrec);
		}
    
    memset(pclDataArea,0x00,4096);
  }
  close_my_cursor(&slCursor);
  dbg(TRACE,"<sendAftData> : records count <%d>",ilRecordCount);
}

static int IsFlightInTimeWindow(char * pcpAdid, char *pcpTifa, char *pcpTifd)
{
	char pclNowUTCTime[20] = "\0";
	//char pclRangeUTCTime[20] = "\0";
	char pclRangeUTCTimeStart[20] = "\0";
	char pclRangeUTCTimeEnd[20] = "\0";
	int  myBulkTimeSecond = 0;
	
	memset(pclNowUTCTime,0,sizeof(pclNowUTCTime));
	//memset(pclRangeUTCTime,0,sizeof(pclRangeUTCTime));
	memset(pclRangeUTCTimeStart,0,sizeof(pclRangeUTCTimeStart));
	memset(pclRangeUTCTimeEnd,0,sizeof(pclRangeUTCTimeEnd));
	
	GetServerTimeStamp("UTC",1,0,pclNowUTCTime);
	dbg(TRACE,"<HandleBulkData> : Current UTC Time <%s>",pclNowUTCTime);
	
	//strncpy(pclRangeUTCTime,pclNowUTCTime,14);
	strncpy(pclRangeUTCTimeStart,pclNowUTCTime,14);
	strncpy(pclRangeUTCTimeEnd,pclNowUTCTime,14);

	dbg(TRACE,"---glTimeWindowStart<%d>",glTimeWindowStart);
  dbg(TRACE,"---glTimeWindowEnd<%d>",glTimeWindowEnd);
    	 
	//myBulkTimeSecond = glbBulkTimeHour * SECONDS_PER_HOUR;
	myBulkTimeSecond = glTimeWindowStart * SECONDS_PER_HOUR;
	AddSecondsToCEDATime(pclRangeUTCTimeStart,myBulkTimeSecond,1);
	myBulkTimeSecond = 0;
	myBulkTimeSecond = glTimeWindowEnd * SECONDS_PER_HOUR;
	AddSecondsToCEDATime(pclRangeUTCTimeEnd,myBulkTimeSecond,1);
	//AddSecondsToCEDATime(pclRangeUTCTime,myBulkTimeSecond,1);
	
	//dbg(TRACE,"<HandleBulkData> : Bulk UTC Time <%s>",pclRangeUTCTime);
	dbg(TRACE,"<HandleBulkData> : pclRangeUTCTimeStart<%s>",pclRangeUTCTimeStart);
	dbg(TRACE,"<HandleBulkData> : pclRangeUTCTimeEnd<%s>",pclRangeUTCTimeEnd);
	
	if( strlen(pcpAdid) == 0 )
	{
		dbg(DEBUG,"ADID is NULL, return and does not send out counter XML meaasge");
		return RC_FAIL;
	}
	
	if( strncmp(pcpAdid," ",1) == 0 || isalpha(*pcpAdid) == 0 )
	{
		dbg(DEBUG,"ADID is invalid(space or not alphabetic letter),return and does not send out counter XML meaasge");
		return RC_FAIL;
	}
	
	if( strncmp(pcpAdid,"A",1) == 0)
	{
		dbg(DEBUG,"ADID is %s",pcpAdid);
		
		if( strlen(pcpTifa) == 0 || strncmp(pcpTifa," ",1) == 0)
		{
			dbg(DEBUG,"TIFA is invalid(NULL or space),return and does not send out counter XML meaasge");
			
			return RC_FAIL;
		}
		else
		{
			//compare TIFA with the time window
			//if( strcmp(pcpTifa, pclNowUTCTime) > 0 && strcmp(pclRangeUTCTime,pcpTifa) > 0 )
			if( strcmp(pcpTifa, pclRangeUTCTimeStart) > 0 && strcmp(pclRangeUTCTimeEnd,pcpTifa) > 0 )
			{
				dbg(DEBUG,"TIFA<%s> is in the time window<%s~%s>, so send out the message",pcpTifa, pclRangeUTCTimeStart,pclRangeUTCTimeEnd);
				return RC_SUCCESS;
			}
			else
			{
				dbg(DEBUG,"TIFA<%s> is not in the time window<%s~%s>, so does not send out the message",pcpTifa, pclRangeUTCTimeStart,pclRangeUTCTimeEnd);
				return RC_FAIL;
			}
		}
	}
	else
	{
		dbg(DEBUG,"ADID is %s",pcpAdid);
		
		if( strlen(pcpTifd) == 0 || strncmp(pcpTifd," ",1) == 0)
		{
			dbg(DEBUG,"TIFD is invalid(NULL or space),return and does not send out counter XML meaasge");
			
			return RC_FAIL;
		}
		else
		{
			//compare TIFD with the time window
			//if( strcmp(pcpTifd, pclNowUTCTime) > 0 && strcmp(pclRangeUTCTime,pcpTifd) > 0 )
			if( strcmp(pcpTifd, pclRangeUTCTimeStart) > 0 && strcmp(pclRangeUTCTimeEnd,pcpTifd) > 0 )
			{
				dbg(DEBUG,"TIFD<%s> is in the time window<%s~%s>, so send out the message",pcpTifd, pclRangeUTCTimeStart,pclRangeUTCTimeEnd);
				return RC_SUCCESS;
			}
			else
			{
				dbg(DEBUG,"TIFD<%s> is not in the time window<%s~%s>, so does not send out the message",pcpTifd, pclRangeUTCTimeStart,pclRangeUTCTimeEnd);
				return RC_FAIL;
			}
		}
	}
}

static void HandleBulkData()
{
	char pclNowUTCTime[20] = "\0";
	char pclRangeUTCTime[20] = "\0";
	int  myBulkTimeSecond = 0;
	int  myDiffMin = 0;
	
	memset(pclRangeUTCTime,0,sizeof(pclRangeUTCTime));
	
	dbg( TRACE,"<HandleBulkData> ------------------ START ------------------" );
	
	GetServerTimeStamp("UTC",1,0,pclNowUTCTime);
	dbg(TRACE,"<HandleBulkData> : Current UTC Time <%s>",pclNowUTCTime);
	
	//compare current time and last TIM time , if > 1 ,then need do sth
	if (strlen(glbLastServerUTCTime) != 0)
  {
  	  dbg(TRACE,"<HandleBulkData> : start to compare time glbLastServerUTCTime <%s>",glbLastServerUTCTime);
      DateDiffToMin(pclNowUTCTime,glbLastServerUTCTime, &myDiffMin);
  }
  dbg(TRACE,"<HandleBulkData> : myDiffMin <%d>",myDiffMin);
	strcpy(glbLastServerUTCTime,pclNowUTCTime);
	
	
	/* Arrival Flight */
	myBulkTimeSecond = glbBulkTimeHour * SECONDS_PER_HOUR;
	
	strncpy(pclRangeUTCTime,pclNowUTCTime,14);
  AddSecondsToCEDATime(pclNowUTCTime,myBulkTimeSecond,1);
  dbg(TRACE,"<HandleBulkData> : Bulk UTC Time <%s>",pclNowUTCTime);
  sendAftBulkData(pclRangeUTCTime,pclNowUTCTime);
	dbg( TRACE,"<HandleBulkData> ------------------   END ------------------" );
}
//fidblk
static void sendAftBulkData(char *mySelectTime, char *pclNowUTCTime)
{
	int ilRc = 0;
	int flds_count = 0;
	int ilRecordCount = 0;
	short slSqlFunc = 0;
  short slCursor = 0;
	char clAftSelection[4096] = "\0";
	CCAREC rlCcarec;
	
	memset(clAftSelection,0,sizeof(clAftSelection));
	//char mySelectTime_[20] = "\0";
	//char tmp[20] = "201205021740";
	//getCurrentTime
	
	/*** Actual SQL Queries ***/
	//strncpy(mySelectTime_,mySelectTime,12);
	//strncpy(mySelectTime_,tmp,12);
	
	// Frank v1.1
	//dbg(TRACE,"<sendAftBulkData> : Select Time <%s> <%s>",mySelectTime,mySelectTime_);
	dbg(DEBUG,"<sendAftBulkData> : Select Time <%s> <%s>",mySelectTime,pclNowUTCTime);
	
	//1.get Flight data
	// Frank v1.1
	/*sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE (TIFD LIKE '%s%') OR (TIFA LIKE '%s%') ",
                                   pcgAftFields,mySelectTime_,mySelectTime_ );
  */
  dbg(DEBUG,"line<%d>++++++++++++++++++++++",__LINE__);
  //sprintf(clAftSelection,"Hello,world");
  //dbg(DEBUG,"clAftSelection<%s>",clAftSelection);
  
  /*
  sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE (TIFD BETWEEN '%s' AND '%s') OR (TIFA BETWEEN '%s' and '%s') ",
                                   pcgAftFields,mySelectTime,pclNowUTCTime,mySelectTime,pclNowUTCTime);
  */
  
  //strcpy(pclNowUTCTime,"201204020310");
  
  /*
  sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE (FTYP<>'T' AND ((ADID='A' AND TIFA LIKE '%12.12s%%') OR (ADID='D' AND TIFD LIKE '%12.12s%%'))) OR (FTYP='T' AND (TIFA LIKE '%12.12s%%' OR TIFD LIKE '%12.12s%%'))", pcgAftFields,pclNowUTCTime,pclNowUTCTime,pclNowUTCTime,pclNowUTCTime);
  */
  
  /* Frank v1.1g */
  sprintf( clAftSelection, "SELECT %s FROM AFTTAB WHERE (FTYP<>'T' AND (((ADID='A' OR ADID='B') AND TIFA LIKE '%12.12s%%') OR ((ADID<>'A' AND ADID<>'B') AND TIFD LIKE '%12.12s%%'))) OR (FTYP='T' AND (TIFA LIKE '%12.12s%%' OR TIFD LIKE '%12.12s%%'))", pcgAftFields,pclNowUTCTime,pclNowUTCTime,pclNowUTCTime,pclNowUTCTime);
  
  dbg(DEBUG,"<sendAftBulkData> : clAftSelection <%s> ",clAftSelection);
  
  memset(&rlCcarec,0,sizeof(rlCcarec));
  sendAftData(clAftSelection,rlCcarec,FALSE);
}

int PackTowingXml( AFTREC rpAftrec )
{
	int ilRc = 0;
	static char myTowingXml[2048] = "\0";
	char cmd[5] = "XMLO";
	
	short slSqlFunc = 0;
  short slCursor = 0;
  char pclDataArea[1024] = "\0";
  
  char TowSqlBuf[1024] = "\0";
  
  sprintf(TowSqlBuf,"SELECT FLNO FROM AFTTAB WHERE RKEY='%s' AND ADID='D'",rpAftrec.AURN);
  
  dbg(TRACE,"TowSqlBuf<%s>",TowSqlBuf);
  
  slSqlFunc= START;
  ilRc = sql_if(slSqlFunc,&slCursor,TowSqlBuf,pclDataArea);
 	dbg(TRACE,"pclDataArea<%s>ilRc<%d>",pclDataArea,ilRc);
 	close_my_cursor(&slCursor);
 	
 	if(ilRc!=DB_SUCCESS)
 	{
 		memset(pclDataArea,0,sizeof(pclDataArea));
 	}
 	else
 	{
 		TrimRight(pclDataArea);
 	}
	
	memset(myTowingXml,0x00,2048);
	
	dbg(TRACE,"<PackTowingXml> : arrival flight");
	sprintf( myTowingXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	                    "<TOW_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
	                    "<FIS_Flight version=\"1.0\">\n"
	                    "   <flight_Identification>\n"
                      "      <REGN>%s</REGN>\n"
                      /*"      <ACT3>%s</ACT3>\n"*/
                      //Frank v1.8 20130228
                      "      <ACT%d>%s</ACT%d>\n"
                      //Frank v1.8 20130228
                      "      <STD>%s</STD>\n"
                      "      <STA>%s</STA>\n"
                      "   </flight_Identification>\n"
                      "   <AFLNO>%s</AFLNO>\n"
                      "   <DFLNO>%s</DFLNO>\n"
                      "   <POSD>%s</POSD>\n"
                      "   <POSA>%s</POSA>\n"
                      "   <ARFT>%s</ARFT>\n"
                      "   <SRFT></SRFT>\n"
                      "   <OFBL>%s</OFBL>\n"
                      "   <ONBL>%s</ONBL>\n"
                      "</FIS_Flight>\n"
                      "</TOW_Data>",
                                   rpAftrec.REGN,
                                   /*rpAftrec.ACT3,*/
                                   //Frank v1.8 20130228
                                   strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                   strncmp(pcgActICAO,"YES",3) == 0 ? rpAftrec.ACT3 : rpAftrec.ACT5,
                                   strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                   //Frank v1.8 20130228
                                   rpAftrec.STOD,//STD
                                   rpAftrec.STOA,//STA
                                   rpAftrec.FLNO,//AFLNO
                                   pclDataArea,//DFLNO
                                   rpAftrec.PSTD,//POSD
                                   rpAftrec.PSTA,//POSA
                                   (rpAftrec.RTOW[0]=='R'?"1":"0"),//RTOW
                                   rpAftrec.OFBL,//OFBL
                                   rpAftrec.ONBL
                                    );
   dbg(TRACE,"<PackTowingXml> : Towing XML Message <%s>",myTowingXml);
   ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myTowingXml, "", 3, 0) ;
   if(ilRc==RC_SUCCESS)
	   dbg(DEBUG, "<PackTowingXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDF); 
   else
	   dbg(DEBUG,"<PackTowingXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDF);
	
	return ilRc;
}

int PackFlightXml( AFTREC rpAftrec, CCAREC rpCcarecCKBS, CCAREC rpCcarecCKES,CICREC rpCicrec)
{
	int ilRc = 0;
	static char myFlightXml[2048] = "\0";
	char cmd[5] = "XMLO";
	
	//
	char cli = '0';
	int ili = 0;
	char pclTmp[16] = "\0";
  char pclTmp1[16] = "\0";
  char pclXML[8192]="\0";
  char pclXML1[8192]="\0";
  char *pclPointer = "\0";
  int ilVIAN = 0;
	
	char pclALC3[16] = "\0";
	char pclFLNO[16] = "\0";
	
	ilVIAN = atoi(rpAftrec.VIAN);
	
	if(strlen(rpCcarecCKBS.CKBS) == 0 && strlen(rpCcarecCKES.CKES) == 0 && strlen(rpCicrec.TERM) == 0
		&& strlen(rpCcarecCKBS.CKIT) == 0)
	{
		dbg(DEBUG,"This is new \"abnormal\" procedure");
		
		//For terminal : STEV
		strcpy(rpCcarecCKBS.CKIT,rpAftrec.STEV);
	}
	  
//FYA 20120606 v1.2c
 PackCodeShare(rpAftrec,pclXML1);
	
	// For test
	/*
	strncpy(rpAftrec.VIAN,"4",1);
	ilVIAN = atoi(rpAftrec.VIAN);
	memset(rpAftrec.VIAL,0,sizeof(rpAftrec.VIAL));
	strcpy(rpAftrec.VIAL,"LADFNLU20110512134500                                          20110512154500                                           NBOYNBO20110512193500                                          20110512230500                                           AMSEHAM20110513074000                                          20110513103500                                           SHJOMSJ20110513170500                                          20110513180500");
	*/	
	// end For test
	
	pclPointer = rpAftrec.VIAL;
	dbg(DEBUG,"<\npclPointer<%s>\n>",pclPointer);
	
	for(ili=0;ili < ilVIAN;ili++)
  {
          memset(pclTmp,0,sizeof(pclTmp));
          memset(pclTmp1,0,sizeof(pclTmp1));
          
          if(strncmp(pcgVialICAO,"YES",3)!=0)
          {
	          strncpy(pclTmp,pclPointer,3);
		  			if(ili == 0)
	          {
	          		pclPointer += 121;
	          }
	          else
	          {
	          		pclPointer += 120;
	          }	
	          //dbg(DEBUG,"three letter is <%s>\n",pclTmp);
					}
					else
					{
						strncpy(pclTmp,pclPointer+3,4);
		  			if(ili == 0)
	          {
	          		pclPointer += 121;
	          }
	          else
	          {
	          		pclPointer += 120;
	          }	
	          //dbg(DEBUG,"four letter is <%s>\n",pclTmp);
					}
					
          cli++;
          sprintf(pclTmp1,"\t\t<VIA%c>%s</VIA%c>\n",cli,pclTmp,cli);
          strcat(pclXML,pclTmp1);
  }
  //printf("+++<\n%s\n>+++",pclXML);
  //dbg(DEBUG,"+++<\n%s\n>+++",pclXML);

	//Frank 20120803  
	//rpAftrec.ALC3 & rpAftrec.FLTN,//FLNO have been TrimRight() in sendAftData  
	if( strlen(pcgCodeForALC3) != 0 && igLastPartOfCSGN != 0)
	{	
		dbg(TRACE,"Check the ALC3 & FLNO null or not");

		ilRc = IsAlc3AndFlnoNullForAdHocFlight(rpAftrec,pclALC3,pclFLNO);
		if( ilRc == RC_SUCCESS)
		{
			strcpy(rpAftrec.ALC3,pclALC3);
			strcpy(rpAftrec.FLTN,pclFLNO);
		}
	}
	else
	{	
		
		dbg(TRACE,"pcgCodeForALC3 is null and igLastPartOfCSGN is zero, so do not check the ALC3 & FLNO null or not");
	}

	memset(myFlightXml,0x00,2048);
	// 1. Arrival Flight
	if (strncmp(rpAftrec.ADID,"A",1) == 0 || strncmp(rpAftrec.FTYP,"B",1)==0 ||strncmp(rpAftrec.FTYP,"Z",1)==0)
	{
		dbg(TRACE,"<PackFlightXml> : arrival flight");
		sprintf( myFlightXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
		                    "<FIS_Flight version=\"1.0\">\n"
		                    "   <flight_Identification>\n"
                        "      <ALC3>%s</ALC3>\n"
                        "      <FLNO>%s%s</FLNO>\n"
                        "      <STAD>%s</STAD>\n"
                        "   </flight_Identification>\n"
                        "   <FTYP>%s</FTYP>\n"
                        "   <STYP>%s</STYP>\n"
                        "   <TTYP>%s</TTYP>\n"
                        "   <CSGN>%s</CSGN>\n"
                        "   <ARRDEP>%s</ARRDEP>\n"
                        "   <STAND>%s</STAND>\n"
                        "   <CKIF></CKIF>\n"
                        "   <CKIT></CKIT>\n"
                        "   <CKBS></CKBS>\n"
                        "   <CKES></CKES>\n"
                        "   <PAX>%s</PAX>\n"
                        "   <TERM>%s</TERM>\n"
                        "   <GATT>%s</GATT>\n"
                        /*"   <LOUNGE>%s</LOUNGE>\n"*/
                        "   <GATE>%s</GATE>\n"
                        "   <GD1B>%s</GD1B>\n"
                        "   <GD1E>%s</GD1E>\n"
                        /*"   <ACT3>%s</ACT3>\n"*/
                        //Frank v1.8 20130228
                      	"   <ACT%d>%s</ACT%d>\n"
                      	//Frank v1.8 20130228
                        "   <TMO>%s</TMO>\n"
                        "   <ETAD>%s</ETAD>\n"
                        "   <ETADF>%s</ETADF>\n"
                        "   <ATAD>%s</ATAD>\n"
                        "   <ONBL>%s</ONBL>\n"
                        "   <OFBL>%s</OFBL>\n"
                        "   <EONBL>%s</EONBL>\n"
                        "   <CTOT>%s</CTOT>\n"
                        "   <REGN>%s</REGN>\n"
                        "   <BELT>%s</BELT>\n"
                        "   <PORTS>\n"
                        "        <P>%s</P>\n"
                        /*"        <VIA1>%s</VIA1>\n"*/
                        /*"        <VIA2>%s</VIA2>\n"*/
                        /*"   </PORTS>\n"
                        "   <codeshare>\n"
                        "      <flightNo1>\n"
                        "        <ALC3>unknown</ALC3>\n"
                        "        <FLNO>unknown</FLNO>\n"
                        "      </flightNo1>\n"
                        "   </codeshare>\n"
                        "   <NXTI>%s</NXTI>\n"
                        "</FIS_Flight>\n"
                        "</FIDS_Data>"*/,
                                     rpAftrec.ALC3,
                                     rpAftrec.FLTN,rpAftrec.FLNS,//FLNO
                                     rpAftrec.STOA,//STAD
                                     rpAftrec.FTYP,
                                     rpAftrec.STYP,
                                     rpAftrec.TTYP,
                                     rpAftrec.CSGN,
                                     "A",//rpAftrec.ADID,//ARRDEP
                                     rpAftrec.PSTA,//STAND
                                     /*
                                     rpAftrec.CKIF,//CKIF
                                     rpAftrec.CKIT,//CKIT
                                     rpCcarecCKBS.CKBS,//CKBS
                                     rpCcarecCKES.CKES,//CKES
                                     */
                                     rpAftrec.PAXT,//PAX
                                     rpAftrec.TGA1,//TERM
                                     rpAftrec.TGA1,//GATT
                                     /*rpAftrec.WRO1,//LOUNGE*/
                                     /*rpAftrec.GA1F,//GATE*/
                                     rpAftrec.GTA1,//GATE
                                     rpAftrec.GA1B,//GD1B
                                     rpAftrec.GA1E,//GD1E
                                     //rpAftrec.ACT3,//ACT3
                                     //Frank v1.8 20130228
                                   	 strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                   	 strncmp(pcgActICAO,"YES",3) == 0 ? rpAftrec.ACT3 : rpAftrec.ACT5,
                                   	 strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                     rpAftrec.TMOA,//TMO
                                     rpAftrec.ETAI,//ETAD
                                     rpAftrec.ETOA,//ETADF
                                     rpAftrec.LAND,//ATAD
                                     rpAftrec.ONBL,
                                     rpAftrec.OFBL,
                                     rpAftrec.ONBE,//EONBL
                                     rpAftrec.SLOU,
                                     rpAftrec.REGN,
                                     rpAftrec.BLT1,//BELT
                                     strncmp(pcgPortICAO,"YES",3)==0?rpAftrec.ORG4:rpAftrec.ORG3);//P //Frank v1.5 20120918
                                     //rpAftrec.VIAN,
                                  	 //rpAftrec.VIAL+1,
                                     //rpAftrec.NXTI);
     strcat(myFlightXml,pclXML);
     strcat(myFlightXml,"   </PORTS>\n");
     
     if(strlen(pclXML1) != 0)
     {
     	strcat(myFlightXml,pclXML1);
     }
     else
     {
     	strcat(myFlightXml,"   <codeshare>\n   </codeshare>\n");
     }
     
     memset(pclTmp1,0,sizeof(pclTmp1));
     sprintf(pclTmp1,"   <NXTI>%s</NXTI>\n",rpAftrec.NXTI);
     strcat(myFlightXml,pclTmp1);
     strcat(myFlightXml,"</FIS_Flight>\n"
                        "</FIDS_Data>");
     
     dbg(TRACE,"<PackFlightXml> : Flight XML Message <%s>",myFlightXml);
     ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myFlightXml, "", 3, 0) ;
     if(ilRc==RC_SUCCESS)
		   dbg(DEBUG, "<PackFlightXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDF); 
	   else
		   dbg(DEBUG,"<PackFlightXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDF);
	}//Frank v1.1g
	
	if (strncmp(rpAftrec.ADID,"A",1) != 0 || strncmp(rpAftrec.FTYP,"B",1)==0 ||strncmp(rpAftrec.FTYP,"Z",1)==0) /*else if (strncmp(rpAftrec.ADID,"D",1) == 0)*/
 	{
	//	2. Departure Flight
	// add CKBS,CKES
		dbg(TRACE,"<PackFlightXml> : departure flight");
     //
   		sprintf( myFlightXml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	                    "<FIDS_Data xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
	                    "<FIS_Flight version=\"1.0\">\n"
	                    "   <flight_Identification>\n"
                      "      <ALC3>%s</ALC3>\n"
                      "      <FLNO>%s%s</FLNO>\n"
                      "      <STAD>%s</STAD>\n"
                      "   </flight_Identification>\n"
                      "   <FTYP>%s</FTYP>\n"
                      "   <STYP>%s</STYP>\n"
                      "   <TTYP>%s</TTYP>\n"
                      "   <CSGN>%s</CSGN>\n"
                      "   <ARRDEP>%s</ARRDEP>\n"
                      "   <STAND>%s</STAND>\n"
                      "   <CKIF>%s</CKIF>\n"
                      "   <CKIT>%s</CKIT>\n"
                      "   <CKBS>%s</CKBS>\n"
                      "   <CKES>%s</CKES>\n"
                      "   <PAX>%s</PAX>\n"
                      "   <TERM>%s</TERM>\n"
                      "   <GATT>%s</GATT>\n"
                      "   <LOUNGE>%s</LOUNGE>\n"
                      "   <GATE>%s</GATE>\n"
                      "   <GD1B>%s</GD1B>\n"
                      "   <GD1E>%s</GD1E>\n"
                      /*"   <ACT3>%s</ACT3>\n"*/
                      //Frank v1.8 20130228
                      "   <ACT%d>%s</ACT%d>\n"
                      //Frank v1.8 20130228
                      /*"   <TMO>%s</TMO>\n"*/
                      "   <ETAD>%s</ETAD>\n"
                      "   <ETADF>%s</ETADF>\n"
                      "   <ATAD>%s</ATAD>\n"
                      "   <ONBL>%s</ONBL>\n"
                      "   <OFBL>%s</OFBL>\n"
                      "   <EONBL>%s</EONBL>\n"
                      "   <CTOT>%s</CTOT>\n"
                      "   <REGN>%s</REGN>\n"
                      "   <BELT>%s</BELT>\n"
                      "   <PORTS>\n"
                      "        <P>%s</P>\n"
                      /*"        <VIA1>%s</VIA1>\n"*/
                      /*"        <VIA2>%s</VIA2>\n"*/
                      /*"   </PORTS>\n"
                      "   <codeshare>\n"
                      "      <flightNo1>\n"
                      "        <ALC3>unknown</ALC3>\n"
                      "        <FLNO>unknown</FLNO>\n"
                      "      </flightNo1>\n"
                      "   </codeshare>\n"
                      "   <NXTI>%s</NXTI>\n"
                      "</FIS_Flight>\n"
                      "</FIDS_Data>"*/,
                                   rpAftrec.ALC3,
                                   rpAftrec.FLTN,rpAftrec.FLNS,//FLNO
                                   rpAftrec.STOD,//STAD
                                   rpAftrec.FTYP,
                                   rpAftrec.STYP,
                                   rpAftrec.TTYP,
                                   rpAftrec.CSGN,
                                   "D",//rpAftrec.ADID,//ARRDEP
                                   rpAftrec.PSTD,//STAND
                                   rpAftrec.CKIF,//CKIF
                                   rpAftrec.CKIT,//CKIT
                                   rpCcarecCKBS.CKBS,//CKBS
                                   rpCcarecCKES.CKES,//CKES
                                   rpAftrec.PAXT,//PAX
                                   /*rpCcarec.CKIT,//TERM*/
                                   strlen(rpCicrec.TERM) != 0?rpCicrec.TERM:rpCcarecCKBS.CKIT,
                                   rpAftrec.TGD1,//GATT
                                   rpAftrec.WRO1,//LOUNGE
                                   /*rpAftrec.GA1F,//GATE*/
                                   rpAftrec.GTD1,//GATE
                                   rpAftrec.GD1B,//GD1B
                                   rpAftrec.GD1E,//GD1E
                                   /*rpAftrec.ACT3,*/
                                   //Frank v1.8 20130228
                                   strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                   strncmp(pcgActICAO,"YES",3) == 0 ? rpAftrec.ACT3 : rpAftrec.ACT5,
                                   strncmp(pcgActICAO,"YES",3) == 0 ? 3 : 5,
                                   //Frank v1.8 20130228
                                   /*rpAftrec.TMOA,//TMO*/
                                   rpAftrec.ETDI,//ETAD
                                   rpAftrec.ETOD,//ETADF
                                   rpAftrec.AIRB,//ATAD
                                   rpAftrec.ONBL,
                                   rpAftrec.OFBL,
                                   rpAftrec.ONBE,//EONBL
                                   rpAftrec.SLOU,
                                   rpAftrec.REGN,
                                   rpAftrec.BLT2,//BELT
                                   strncmp(pcgPortICAO,"YES",3)==0?rpAftrec.DES4:rpAftrec.DES3);//P
                                   /*
                                   rpAftrec.VIAN,
                                   rpAftrec.VIAL,
                                   rpAftrec.NXTI);*/
     //
     strcat(myFlightXml,pclXML);
     strcat(myFlightXml,"   </PORTS>\n");
     
     if(strlen(pclXML1) != 0)
     {
     	strcat(myFlightXml,pclXML1);
     }
     else
     {
     	strcat(myFlightXml,"   <codeshare>\n   </codeshare>\n");
     }

     memset(pclTmp1,0,sizeof(pclTmp1));
     sprintf(pclTmp1,"   <NXTI>%s</NXTI>\n",rpAftrec.NXTI);
     strcat(myFlightXml,pclTmp1);
     strcat(myFlightXml,"</FIS_Flight>\n"
                        "</FIDS_Data>");
     
     dbg(TRACE,"<PackFlightXml> : Flight XML Message <%s>",myFlightXml);
     ilRc = SendCedaEvent(glbWMFIDF,0, " ", " ", " ", " ",cmd, " ", " "," ", myFlightXml, "", 3, 0) ;
     if(ilRc==RC_SUCCESS)
		   dbg(DEBUG, "<PackFlightXml> \n ****** Send command report ****** \nSending command: <%s> to <%d>\n ****** End send command report ****** ", cmd, glbWMFIDF); 
	   else
		   dbg(DEBUG,"<PackFlightXml> unable send command:<%s> to <%d> ",cmd,glbWMFIDF);
	}
	/*
	else
	{
		dbg(TRACE,"<PackFlightXml> : should never come here");
		dbg(TRACE,"<PackFlightXml> : unknow flight type ADID <%s> URNO <%s> FLTN <%s>",rpAftrec.ADID,rpAftrec.URNO,rpAftrec.FLTN);
	}
	*/
	return ilRc;
}

int IsAlc3AndFlnoNullForAdHocFlightBAG(BAGREC rpBagrec,char *pcpALC3,char *pcpFLNO)
{
        char *pclOrg;
        char *pclFunc = "IsAlc3AndFlnoNullForAdHocFlight";
        int ilLengthOfCsgn = 0;

        //Frank 20120803  
        //rpBagrec.ALC3 & rpBagrec.FLTN,//FLNO have been TrimRight() in sendAftData();  

        //test
        //memset(rpBagrec.ALC3,0,sizeof(rpBagrec.ALC3));
        //memset(rpBagrec.FLTN,0,sizeof(rpBagrec.FLTN));
        //test
        dbg(DEBUG,"%s: Checking ALC3 and FLNO null or not",pclFunc);
        if( strlen(rpBagrec.ALC3) == 0 )
        {
                dbg(DEBUG,"%s: ALC3 is null",pclFunc);
                //if( strlen(rpBagrec.FLTN) == 0 )              
                //{                     
                        dbg(DEBUG,"%s: FLNO is null too, then filling ALC3 by %s, and FLNO by the last %d alphanumeric characters of the call-sign ",pclFunc,pcgCodeForALC3,igLastPartOfCSGN);


                        dbg(DEBUG,"%s The original rpBagrec.ALC3 is <%s>,pcgCodeForALC3<%s>",pclFunc,rpBagrec.ALC3,pcgCodeForALC3);
                        memset(pcpALC3,0,sizeof(pcpALC3));
                        strcpy(pcpALC3,pcgCodeForALC3);
                        dbg(DEBUG,"%s The modified pcpALC3 is <%s>",pclFunc,pcpALC3);

                        dbg(DEBUG,"%s The original rpBagrec.FLTN is <%s>",pclFunc,rpBagrec.FLTN);
                        memset(pcpFLNO,0,sizeof(pcpFLNO));

                        ilLengthOfCsgn = strlen(rpBagrec.CSGN);
                        if( ilLengthOfCsgn >= igLastPartOfCSGN )
                        {
                                pclOrg = rpBagrec.CSGN + ilLengthOfCsgn - igLastPartOfCSGN;                                         

                                strcpy(pcpFLNO,pclOrg);
                                dbg(DEBUG,"%s The modified rpBagrec.FLTN is <%s> which is last <%d> of rpBagrec.CSGN<%s>",pclFunc,pcpFLNO,igLastPartOfCSGN,rpBagrec.CSGN);
                                //return RC_SUCCESS;            
                        }
                        else
                        {
                                strcpy(pcpFLNO,rpBagrec.CSGN);
                                dbg(DEBUG,"%s The ilLengthOfCsgn is <%d>pcpFLNO<%s>",pclFunc,ilLengthOfCsgn,pcpFLNO);
                                //return RC_FAIL;               
                        }
                        return RC_SUCCESS;
                //}             
                //else
                //{                     
                        //dbg(DEBUG,"%s: FLTN is not null",pclFunc);
                        //return RC_FAIL;               
                //}
        }
        else
        {
                dbg(DEBUG,"%s: ALC3 is not null",pclFunc);
                return RC_FAIL;
        }
}

int IsAlc3AndFlnoNullForAdHocFlight(AFTREC rpAftrec,char *pcpALC3,char *pcpFLNO)
{	
	char *pclOrg;	
	char *pclFunc = "IsAlc3AndFlnoNullForAdHocFlight";	
	int ilLengthOfCsgn = 0;    
	
	//Frank 20120803  
	//rpAftrec.ALC3 & rpAftrec.FLTN,//FLNO have been TrimRight() in sendAftData();  
	
	//test
	//memset(rpAftrec.ALC3,0,sizeof(rpAftrec.ALC3));
	//memset(rpAftrec.FLTN,0,sizeof(rpAftrec.FLTN));
	//test
	dbg(DEBUG,"%s: Checking ALC3 and FLNO null or not",pclFunc);	
	if( strlen(rpAftrec.ALC3) == 0 )	
	{		
		dbg(DEBUG,"%s: ALC3 is null",pclFunc);		
		//if( strlen(rpAftrec.FLTN) == 0 )		
		//{			
			dbg(DEBUG,"%s: FLNO is null too, then filling ALC3 by %s, and FLNO by the last %d alphanumeric characters of the call-sign ",pclFunc,pcgCodeForALC3,igLastPartOfCSGN);
			
			
			dbg(DEBUG,"%s The original rpAftrec.ALC3 is <%s>,pcgCodeForALC3<%s>",pclFunc,rpAftrec.ALC3,pcgCodeForALC3);
			memset(pcpALC3,0,sizeof(pcpALC3));
			strcpy(pcpALC3,pcgCodeForALC3);
			dbg(DEBUG,"%s The modified pcpALC3 is <%s>",pclFunc,pcpALC3);
			
			dbg(DEBUG,"%s The original rpAftrec.FLTN is <%s>",pclFunc,rpAftrec.FLTN);
			memset(pcpFLNO,0,sizeof(pcpFLNO));
			
			ilLengthOfCsgn = strlen(rpAftrec.CSGN);
			if( ilLengthOfCsgn >= igLastPartOfCSGN )
			{
				pclOrg = rpAftrec.CSGN + ilLengthOfCsgn - igLastPartOfCSGN;								
				strcpy(pcpFLNO,pclOrg);
				dbg(DEBUG,"%s The modified rpAftrec.FLTN is <%s> which is last <%d> of rpAftrec.CSGN<%s>",pclFunc,pcpFLNO,igLastPartOfCSGN,rpAftrec.CSGN);			
				//return RC_SUCCESS;		
			}			
			else
			{	
				strcpy(pcpFLNO,rpAftrec.CSGN);
				dbg(DEBUG,"%s The ilLengthOfCsgn is <%d>pcpFLNO<%s>",pclFunc,ilLengthOfCsgn,pcpFLNO);
				//return RC_FAIL;		
			}
			return RC_SUCCESS;
		//}		
		//else
		//{			
			//dbg(DEBUG,"%s: FLTN is not null",pclFunc);
			//return RC_FAIL;		
		//}
	}	
	else	
	{		
		dbg(DEBUG,"%s: ALC3 is not null",pclFunc);		
		return RC_FAIL;
	}
}

void get_fld_value(char *buf,short fld_no,char *dest)
{
	int i=0;
	char *ret;
	char myDataArea[4096] = "\0";
	strncpy(myDataArea,buf,strlen(buf));
	
	ret = strtok(myDataArea, ","); 
	for(i=1;i<fld_no;i++)
	{
	   ret = strtok(NULL, ","); 
  }
  strncpy(dest,ret,strlen(ret));
}
int get_flds_count(char *buf)
{
	int i=0;
	char *ret;
	char myDataArea[4096] = "\0";
	strncpy(myDataArea,buf,strlen(buf));
	
	ret = strtok(myDataArea, ","); 
	while(ret != NULL)
	{
		i++;
		ret = strtok(NULL, ","); 
	}
	
	return i;
}
static int InitFieldIndex()
{
	int ilRc = RC_SUCCESS;
  int ilCol,ilPos;

  FindItemInList(pcgAftFields,"URNO",',',&igAftUrno,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ALC3",',',&igAftAlc3,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"FLTN",',',&igAftFltn,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"FLNO",',',&igAftFlno,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"STOD",',',&igAftStod,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"STOA",',',&igAftStoa,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"CKIF",',',&igAftCkif,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"CKIT",',',&igAftCkit,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GA1F",',',&igAftGa1f,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GA1B",',',&igAftGa1b,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GA1E",',',&igAftGa1e,&ilCol,&ilPos);

  FindItemInList(pcgAftFields,"GD1B",',',&igAftGd1b,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GD1E",',',&igAftGd1e,&ilCol,&ilPos);

  FindItemInList(pcgAftFields,"ACT3",',',&igAftAct3,&ilCol,&ilPos);
  
  //Frank v1.8 20130228
  FindItemInList(pcgAftFields,"ACT5",',',&igAftAct5,&ilCol,&ilPos);
  
  FindItemInList(pcgAftFields,"ONBL",',',&igAftOnbl,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"OFBL",',',&igAftOfbl,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ONBE",',',&igAftOnbe,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"STYP",',',&igAftStyp,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"TTYP",',',&igAftTtyp,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ADID",',',&igAftAdid,&ilCol,&ilPos);
  
  //Frank v1.1
  FindItemInList(pcgAftFields,"PSTA",',',&igAftPsta,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"PSTD",',',&igAftPstd,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GTA1",',',&igAftGta1,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"GTD1",',',&igAftGtd1,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"WRO1",',',&igAftWro1,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"WRO2",',',&igAftWro2,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ETAI",',',&igAftEtai,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ETDI",',',&igAftEtdi,&ilCol,&ilPos);
  //Frank v1.1
  
  FindItemInList(pcgAftFields,"ETOD",',',&igAftEtod,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ETOA",',',&igAftEtoa,&ilCol,&ilPos);
  
  //Frank v1.1
  FindItemInList(pcgAftFields,"LAND",',',&igAftLand,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"AIRB",',',&igAftAirb,&ilCol,&ilPos);
  //Frank v1.1
  
  FindItemInList(pcgAftFields,"CTOT",',',&igAftCtot,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"REGN",',',&igAftRegn,&ilCol,&ilPos);
  
  //Frank v1.1
  FindItemInList(pcgAftFields,"TMOA",',',&igAftTmoa,&ilCol,&ilPos);
  //Frank v1.1
  
  FindItemInList(pcgAftFields,"PAXT",',',&igAftPaxt,&ilCol,&ilPos);
  
  //Frank v1.1
  FindItemInList(pcgAftFields,"BLT1",',',&igAftBlt1,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"BLT2",',',&igAftBlt2,&ilCol,&ilPos);
  //Frank v1.1
  
  
  //FindItemInList(pcgAftFields,"VIA3",',',&igAftVia3,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"CSGN",',',&igAftCsgn,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"FTYP",',',&igAftFtyp,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"NXTI",',',&igAftNxti,&ilCol,&ilPos);
  
  FindItemInList(pcgAftFields,"VIAN",',',&igAftVian,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"VIAL",',',&igAftVial,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"AURN",',',&igAftAurn,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"RTOW",',',&igAftRtow,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"TGA1",',',&igAftTga1,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"SLOU",',',&igAftSlou,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"JFNO",',',&igAftJfno,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"JCNT",',',&igAftJcnt,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"ORG3",',',&igAftOrg3,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"DES3",',',&igAftDes3,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"STEV",',',&igAftStev,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"TGD1",',',&igAftTgd1,&ilCol,&ilPos);
  
  FindItemInList(pcgAftFields,"TIFA",',',&igAftTifa,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"TIFD",',',&igAftTifd,&ilCol,&ilPos);
  
  //Frank v1.5 20120918
  FindItemInList(pcgAftFields,"ORG4",',',&igAftOrg4,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"DES4",',',&igAftDes4,&ilCol,&ilPos);
  FindItemInList(pcgAftFields,"FLNS",',',&igAftFlns,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
	  dbg(TRACE,"<InitFieldIndex> : igAftUrno<%d>",igAftUrno);
	  dbg(TRACE,"<InitFieldIndex> : igAftAlc3<%d>",igAftAlc3);
	  dbg(TRACE,"<InitFieldIndex> : igAftFltn<%d>",igAftFltn);
	  dbg(TRACE,"<InitFieldIndex> : igAftFlno<%d>",igAftFlno);
	  dbg(TRACE,"<InitFieldIndex> : igAftStod<%d>",igAftStod);
	  dbg(TRACE,"<InitFieldIndex> : igAftStoa<%d>",igAftStoa);
	  dbg(TRACE,"<InitFieldIndex> : igAftCkif<%d>",igAftCkif);
	  dbg(TRACE,"<InitFieldIndex> : igAftCkit<%d>",igAftCkit);
	  dbg(TRACE,"<InitFieldIndex> : igAftGa1f<%d>",igAftGa1f);
	  dbg(TRACE,"<InitFieldIndex> : igAftGa1b<%d>",igAftGa1b);
	  dbg(TRACE,"<InitFieldIndex> : igAftGa1e<%d>",igAftGa1e);
	  dbg(TRACE,"<InitFieldIndex> : igAftAct3<%d>",igAftAct3);
	  dbg(TRACE,"<InitFieldIndex> : igAftOnbl<%d>",igAftOnbl);
	  dbg(TRACE,"<InitFieldIndex> : igAftOfbl<%d>",igAftOfbl);
	  dbg(TRACE,"<InitFieldIndex> : igAftOnbe<%d>",igAftOnbe);
	  dbg(TRACE,"<InitFieldIndex> : igAftStyp<%d>",igAftStyp);
	  dbg(TRACE,"<InitFieldIndex> : igAftTtyp<%d>",igAftTtyp);
	  dbg(TRACE,"<InitFieldIndex> : igAftAdid<%d>",igAftAdid);

	  
	  //Frank v1.1
	  dbg(TRACE,"<InitFieldIndex> : igAftPsta<%d>",igAftPsta);
	  dbg(TRACE,"<InitFieldIndex> : igAftPstd<%d>",igAftPstd);
	  dbg(TRACE,"<InitFieldIndex> : igAftGta1<%d>",igAftGta1);
	  dbg(TRACE,"<InitFieldIndex> : igAftGtd1<%d>",igAftGtd1);
	  dbg(TRACE,"<InitFieldIndex> : igAftWro1<%d>",igAftWro1);
	  dbg(TRACE,"<InitFieldIndex> : igAftWro2<%d>",igAftWro2);
	  
	  dbg(TRACE,"<InitFieldIndex> : igAftEtai<%d>",igAftEtai);
	  dbg(TRACE,"<InitFieldIndex> : igAftEtdi<%d>",igAftEtdi);
	  //Frank v1.1
	  
	  dbg(TRACE,"<InitFieldIndex> : igAftEtod<%d>",igAftEtod);
	  dbg(TRACE,"<InitFieldIndex> : igAftEtoa<%d>",igAftEtoa);
	  
	  //Frank v1.1
	  dbg(TRACE,"<InitFieldIndex> : igAftLand<%d>",igAftLand);
	  dbg(TRACE,"<InitFieldIndex> : igAftAirb<%d>",igAftAirb);
	  //Frank v1.1
	  
	  dbg(TRACE,"<InitFieldIndex> : igAftCtot<%d>",igAftCtot);
	  dbg(TRACE,"<InitFieldIndex> : igAftRegn<%d>",igAftRegn);
	  
	  //Frank v1.1
	  dbg(TRACE,"<InitFieldIndex> : igAftTmoa<%d>",igAftTmoa);
	  //Frank v1.1
	  
	  dbg(TRACE,"<InitFieldIndex> : igAftPaxt<%d>",igAftPaxt);
	  
	  //Frank v1.1
	  dbg(TRACE,"<InitFieldIndex> : igAftBlt1<%d>",igAftBlt1);
	  dbg(TRACE,"<InitFieldIndex> : igAftBlt2<%d>",igAftBlt2);
	  //Frank v1.1
	  
	  //dbg(TRACE,"<InitFieldIndex> : igAftVia3<%d>",igAftVia3);
	  dbg(TRACE,"<InitFieldIndex> : igAftCsgn<%d>",igAftCsgn);
	  dbg(TRACE,"<InitFieldIndex> : igAftFtyp<%d>",igAftFtyp);
	  dbg(TRACE,"<InitFieldIndex> : igAftNxti<%d>",igAftNxti);
	  
	  dbg(TRACE,"<InitFieldIndex> : igAftAurn<%d>",igAftAurn);
	}
	
  FindItemInList(pcgCcaFields,"URNO",',',&igCcaUrno,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"CKIC",',',&igCcaCkic,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"CKIT",',',&igCcaCkit,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"CKBS",',',&igCcaCkbs,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"CKES",',',&igCcaCkes,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"FLNO",',',&igCcaFlno,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"FLNU",',',&igCcaFlnu,&ilCol,&ilPos);
  FindItemInList(pcgCcaFields,"CTYP",',',&igCcaCtyp,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
	  dbg(DEBUG,"<InitFieldIndex> : igCcaUrno<%d>",igCcaUrno);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaCkic<%d>",igCcaCkic);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaCkit<%d>",igCcaCkit);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaCkbs<%d>",igCcaCkbs);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaCkes<%d>",igCcaCkes);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaFlno<%d>",igCcaFlno);
	  dbg(DEBUG,"<InitFieldIndex> : igCcaFlnu<%d>",igCcaFlnu);
	}
  FindItemInList(pcgAltFields,"URNO",',',&igAltUrno,&ilCol,&ilPos);
  FindItemInList(pcgAltFields,"ALC2",',',&igAltAlc2,&ilCol,&ilPos);
  FindItemInList(pcgAltFields,"ALC3",',',&igAltAlc3,&ilCol,&ilPos);
  FindItemInList(pcgAltFields,"ALFN",',',&igAltAlfn,&ilCol,&ilPos);
  FindItemInList(pcgAltFields,"ADD4",',',&igAltAdd4,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igAltUrno<%d>",igAltUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igAltAlc2<%d>",igAltAlc2);
  	dbg(DEBUG,"<InitFieldIndex> : igAltAlc3<%d>",igAltAlc3);
  	dbg(DEBUG,"<InitFieldIndex> : igAltAlfn<%d>",igAltAlfn);
  	dbg(DEBUG,"<InitFieldIndex> : igAltAdd4<%d>",igAltAdd4);
  }
  
  FindItemInList(pcgAptFields,"URNO",',',&igAptUrno,&ilCol,&ilPos);
  FindItemInList(pcgAptFields,"APC3",',',&igAptApc3,&ilCol,&ilPos);
  FindItemInList(pcgAptFields,"APC4",',',&igAptApc4,&ilCol,&ilPos);
  FindItemInList(pcgAptFields,"APFN",',',&igAptApfn,&ilCol,&ilPos);
  FindItemInList(pcgAptFields,"APSN",',',&igAptApsn,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igAptUrno<%d>",igAptUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igAptApc3<%d>",igAptApc3);
  	dbg(DEBUG,"<InitFieldIndex> : igAptApc4<%d>",igAptApc4);
  	dbg(DEBUG,"<InitFieldIndex> : igAptApfn<%d>",igAptApfn);
  	dbg(DEBUG,"<InitFieldIndex> : igAptApsn<%d>",igAptApsn);
  }
  FindItemInList(pcgGatFields,"URNO",',',&igGatUrno,&ilCol,&ilPos);
  FindItemInList(pcgGatFields,"GNAM",',',&igGatGnam,&ilCol,&ilPos);
  FindItemInList(pcgGatFields,"TERM",',',&igGatTerm,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igGatUrno<%d>",igGatUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igGatGnam<%d>",igGatGnam);
  	dbg(DEBUG,"<InitFieldIndex> : igGatTerm<%d>",igGatTerm);
  }
  
  FindItemInList(pcgCicFields,"URNO",',',&igCicUrno,&ilCol,&ilPos);
  FindItemInList(pcgCicFields,"CNAM",',',&igCicCnam,&ilCol,&ilPos);
  FindItemInList(pcgCicFields,"TERM",',',&igCicTerm,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igCicUrno<%d>",igCicUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igCicCnam<%d>",igCicCnam);
  	dbg(DEBUG,"<InitFieldIndex> : igCicTerm<%d>",igCicTerm);
  }
  
  FindItemInList(pcgBltFields,"URNO",',',&igBltUrno,&ilCol,&ilPos);
  FindItemInList(pcgBltFields,"BNAM",',',&igBltBnam,&ilCol,&ilPos);
  FindItemInList(pcgBltFields,"TERM",',',&igBltTerm,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igBltUrno<%d>",igBltUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igBltBnam<%d>",igBltBnam);
  	dbg(DEBUG,"<InitFieldIndex> : igBltTerm<%d>",igBltTerm);
  }
  
  FindItemInList(pcgPstFields,"URNO",',',&igPstUrno,&ilCol,&ilPos);
  FindItemInList(pcgPstFields,"PNAM",',',&igPstPnam,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igPstUrno<%d>",igPstUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igPstPnam<%d>",igPstPnam);
  }  
  
  FindItemInList(pcgFidFields,"URNO",',',&igFidUrno,&ilCol,&ilPos);
  FindItemInList(pcgFidFields,"CODE",',',&igFidCode,&ilCol,&ilPos);
  FindItemInList(pcgFidFields,"BEME",',',&igFidBeme,&ilCol,&ilPos);
  FindItemInList(pcgFidFields,"BEMD",',',&igFidBemd,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igFidUrno<%d>",igFidUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igFidCode<%d>",igFidCode);
  	dbg(DEBUG,"<InitFieldIndex> : igFidBeme<%d>",igFidBeme);
  	dbg(DEBUG,"<InitFieldIndex> : igFidBemd<%d>",igFidBemd);
  }  
  
  FindItemInList(pcgBagFields,"URNO",',',&igBagUrno,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"ALC3",',',&igBagAlc3,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"FLTN",',',&igBagFltn,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"STOA",',',&igBagStoa,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"STOD",',',&igBagStod,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"ADID",',',&igBagAdid,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"B1BA",',',&igBagB1ba,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"B1EA",',',&igBagB1ea,&ilCol,&ilPos);
  //FindItemInList(pcgBagFields,"B2BA",',',&igBagB2ba,&ilCol,&ilPos);
  FindItemInList(pcgBagFields,"CSGN",',',&igBagCsgn,&ilCol,&ilPos);
  
  if(ilMode == 1)
  {
  	dbg(DEBUG,"<InitFieldIndex> : igBagUrno<%d>",igBagUrno);
  	dbg(DEBUG,"<InitFieldIndex> : igBagAlc3<%d>",igBagAlc3);
  	dbg(DEBUG,"<InitFieldIndex> : igBagFltn<%d>",igBagFltn);
  	dbg(DEBUG,"<InitFieldIndex> : igBagStoa<%d>",igBagStoa);
  	dbg(DEBUG,"<InitFieldIndex> : igBagStod<%d>",igBagStod);
  	dbg(DEBUG,"<InitFieldIndex> : igBagAdid<%d>",igBagAdid);
  	dbg(DEBUG,"<InitFieldIndex> : igBagB1ba<%d>",igBagB1ba);
  	dbg(DEBUG,"<InitFieldIndex> : igBagB1ea<%d>",igBagB1ea);
  	//dbg(DEBUG,"<InitFieldIndex> : igBagB2ba<%d>",igBagB2ba);
  }
  
  return ilRc;
}

/*
int PackCodeShare(AFTREC rpAftrec, char *pclXML)
{
		int ili = 0;
		int ilj = 0;
		int ilJCNT = 0;
		char pclTmpALC2[256] = "\0";
		char pclTmpALC3[256] = "\0";
		char pclTmpALC2Pack[256] = "\0";
		char pclTmpFLNO[1024] = "\0";
		char pclTmpFLNOPack[1024] = "\0";
		char pclTmpflightNo[1024] = "\0";
		char *pclPointer = "\0";
		int ilCount =0;
		int ilRC =0;
pclXML[0] = '\0';
		
		
		if(strlen(rpAftrec.JFNO) == 0 || strncmp(rpAftrec.JFNO," ",1) == 0 || strncmp(rpAftrec.JCNT," ",1) == 0 || atoi(rpAftrec.JCNT) == 0)
		{
			return 1;
		}
		
		ilJCNT = atoi(rpAftrec.JCNT);
		pclPointer = rpAftrec.JFNO;
		
		//test
		//
		//ilJCNT = 7;
		//strcpy(rpAftrec.JFNO,"AB 4071  AZ 3925  ME 6637  NZ 4255  OA 8055           VA 7455");
		//pclPointer = rpAftrec.JFNO;
		//
		
		dbg(DEBUG,"\n<%s> JCNT <%d>\n",pclPointer, ilJCNT);
		//printf("<%s>",pclPointer);
		
		strcat(pclXML,"\t<codeshare>\n");
    for(ili=0; ili < ilJCNT;ili++)
  	{
		dbg(DEBUG,"\n<ili><%d> XML <%s>\n",ili, pclXML);
	    memset(pclTmpALC2,0,sizeof(pclTmpALC2));
	    memset(pclTmpFLNO,0,sizeof(pclTmpFLNO));
	    memset(pclTmpflightNo,0,sizeof(pclTmpflightNo));
	    memcpy( pclTmpflightNo, &pclPointer[ili*9], 9 );
	
            if( pclPointer[2] == ' ' )
            {
	        strncpy(pclTmpALC2,pclPointer,2);
      	        syslibSearchDbData("ALTTAB","ALC3",pclTmpALC3,"ALC2",pclTmpALC2,&ilCount,"\n");
	    }
            else
            {
	        strncpy(pclTmpALC3,pclPointer,3);
      	        syslibSearchDbData("ALTTAB","ALC2",pclTmpALC2,"ALC3",pclTmpALC3,&ilCount,"\n");
            }
	    //pclPointer += 4;
	    strncpy(pclTmpFLNO,&pclTmpflightNo[3],6 );
	
	    //if(strncmp(pclTmpFLNO," ",1) == 0) continue;
	    //pclPointer += 9;//
	    
	    //  sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",(strlen(pclTmpALC3)==0 || pclTmpALC3[0] == ' ')? pclTmpALC2 : pclTmpALC3);//
	      sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);
	
	      //dbg(DEBUG,"three letter is <%s>\n",pclTmp);
	      sprintf(pclTmpflightNo,"\t<flightNo%d>%s%s\t</flightNo%d>\n",ilj+1,pclTmpALC2Pack,pclTmpFLNOPack,ilj+1);
	      strcat(pclXML,pclTmpflightNo);
	      
	      ilj++;
		dbg(DEBUG,"\n<ili><%d> XML <%s>\n",ili, pclXML);
  		}
  	strcat(pclXML,"\t</codeshare>\n");
  	dbg(DEBUG,"\n+++<%s>+++",pclXML);
  	return 0;    
}
*/
int PackCodeShare(AFTREC rpAftrec, char *pclXML)
{
		int ili = 0;
		int ilj = 0;
		int ilJCNT = 0;
		char pclTmpALC2[256] = "\0";
		char pclTmp[256] = "\0";
		char pclTmpALC3[256] = "\0";
		char pclTmpALC2Pack[256] = "\0";
		char pclTmpFLNO[1024] = "\0";
		char pclTmpFLNOPack[1024] = "\0";
		char pclTmpflightNo[1024] = "\0";
		char *pclPointer = "\0";
		int ilCount =0;
		int ilRC =0;
			
		//FYA v1.9 20131115
		#ifdef FYA
		int ilReturnValue = 0;
		short slSqlCursor = 0;
 		short slSqlFunc = START;
		char pclFunc[] = "PackCodeShare";
		char pclAlc2[20] = "";
		char pclAlc3[20] = "";
                char pclVpfr[20] = "";
                char pclVpto[20] = "";
		char pclNowUTCTime[20] = "\0";
		char pclSqlBuf[1024] = "";
    		char pclSqlData[1024] = "";
		char pclSqlSelection[1024] = "";
		#endif
		//FYA v1.9 20131115	
		
		if(strlen(rpAftrec.JFNO) == 0 || strncmp(rpAftrec.JFNO," ",1) == 0 || strncmp(rpAftrec.JCNT," ",1) == 0 || atoi(rpAftrec.JCNT) == 0)
		{	
			dbg(DEBUG,"JFNO or JCNT is NULL or space or zero");
			return 1;
		}
		
		ilJCNT = atoi(rpAftrec.JCNT);
		pclPointer = rpAftrec.JFNO;
		
		//test
		/*
		ilJCNT = 7;
		strcpy(rpAftrec.JFNO,"AMV4071  AZ 3925  ME 6637  NZ 4255  OA 8055           VA 7455");
		pclPointer = rpAftrec.JFNO;
		*/
		
		dbg(DEBUG,"pclPointer<%s>",pclPointer);
		//printf("<%s>",pclPointer);
		
		strcat(pclXML,"\t<codeshare>\n");
    for(ili=0; ili < ilJCNT;ili++)
  	{
	    memset(pclTmpALC2,0,sizeof(pclTmpALC2));
	    memset(pclTmpFLNO,0,sizeof(pclTmpFLNO));
	    memset(pclTmpflightNo,0,sizeof(pclTmpflightNo));
	
	    strncpy(pclTmpALC2,pclPointer,2);
	    strncpy(pclTmp,pclPointer,3);
	    dbg(DEBUG,"++++++++ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmp);
            
	    //pclPointer += 4;
	    strncpy(pclTmpFLNO,pclPointer+3,6);
	
		dbg(DEBUG,"calling TrimRight(pclTmpFLNO)");
		TrimRight(pclTmpFLNO);
	
	    //if(strncmp(pclTmpFLNO," ",1) == 0) continue;
	    pclPointer += 9;
	    
	    if(strncmp(pclTmpALC2," ",1) != 0 || strncmp(pclTmpFLNO," ",1) != 0 ||
	       strncmp(pclTmp," ",1) != 0)
      { 
	dbg(DEBUG,"pclTmp last letter<%c>",pclTmp[strlen(pclTmp)-1]);
      	//ALC2->ALC3
	if( pclTmp[strlen(pclTmp)-1] == ' ' )
	{
				//FYA 2013/11/15 v1.9
				memset(pclTmpALC3,0,sizeof(pclTmpALC3));
				//FYA 2013/11/15 v1.9
      	ilRC = syslibSearchDbData("ALTTAB","ALC2",pclTmpALC2,"ALC3",pclTmpALC3,&ilCount,"\n");

	//FYA 2013/11/15 v1.9
	dbg(TRACE,"**************The return value of syslibSearchDbData ilRC<%d> ilCount<%d>",ilRC,ilCount);
	dbg(TRACE,"**************Getting ALC3 from basic data <%s>",pclTmpALC3);
      	
	if (ilCount > 1)
	{

	ilRC = RC_FAIL;
	#ifdef FYA
	memset(pclSqlBuf,0,sizeof(pclSqlBuf));
	memset(pclSqlData,0,sizeof(pclSqlData));
	memset(pclSqlSelection,0,sizeof(pclSqlSelection));
	memset(pclNowUTCTime,0,sizeof(pclNowUTCTime));

	GetServerTimeStamp("UTC",1,0,pclNowUTCTime);
	//dbg(TRACE,"<%s> : Current UTC Time <%s>",pclFunc,pclNowUTCTime);

	sprintf(pclSqlSelection, "WHERE ALC2='%s'",pclTmpALC2);
	sprintf( pclSqlBuf, "SELECT ALC2,ALC3,VAFR,VATO FROM ALTTAB %s", pclSqlSelection );
	
	dbg(TRACE,"^^^^^^^^^^^^pclSqlBuf<%s>",pclSqlBuf);

	//////////////////
	slSqlCursor = 0;
  	slSqlFunc = START;
	while((ilReturnValue = sql_if(slSqlFunc,&slSqlCursor,pclSqlBuf,pclSqlData)) == DB_SUCCESS)
	{ 
      		slSqlFunc = NEXT;
      		//TrimRight(pclSqlData);
		get_fld(pclSqlData,FIELD_1,STR,20,pclAlc2);
		get_fld(pclSqlData,FIELD_2,STR,20,pclAlc3);
                get_fld(pclSqlData,FIELD_3,STR,20,pclVpfr);
                get_fld(pclSqlData,FIELD_4,STR,20,pclVpto);
	
		TrimRight(pclAlc2);
		TrimRight(pclAlc3);
		TrimRight(pclVpfr);
		TrimRight(pclVpto);		
     		
		//dbg( TRACE, "<%s> SqlData <%s>", pclFunc, pclSqlData );
  		dbg( TRACE, "<%s> ALC2<%s> ALC3<%s>,VPFR <%s> VPTO <%s> Date<%s>", pclFunc, pclAlc2, pclAlc3, pclVpfr, pclVpto , pclNowUTCTime);
  	
		if (strlen(pclVpfr) != 0 && strlen(pclVpto) != 0)
		{
			dbg(TRACE,"<%s> VPFR<%s> & VPTO<%s> are not null",pclFunc, pclVpfr, pclVpto);
			if(strcmp(pclVpfr,pclVpto) >= 0)
			{	
				dbg(TRACE,"<%s> pclVpfr[%s] > pclVpto[%s] -> Invalid", pclFunc, pclVpfr, pclVpto);
				continue;
			}
		}

		if( strlen(pclVpfr) != 0 &&  strcmp(pclVpfr,pclNowUTCTime) <= 0)
		{
			 dbg(TRACE,"<%s> pclVpfr[%s] < pclNowUTCTime[%s]", pclFunc, pclVpfr, pclNowUTCTime);
			 if( strlen(pclVpto) != 0)
			 {
				if(strcmp(pclNowUTCTime,pclVpto) <= 0)
				{
					dbg(TRACE,"<%s> Today[%s] < VPTO[%s] -> Valid", pclFunc, pclNowUTCTime, pclVpto);	
					ilRC = RC_SUCCESS;
					memset(pclTmpALC3,0,sizeof(pclTmpALC3));
					strncpy(pclTmpALC3,pclAlc3,strlen(pclAlc3));
					/////////////
					break;
				}
				else
				{
					dbg(TRACE,"<%s> Today[%s] > VPTO[%s] -> Invalid", pclFunc, pclNowUTCTime, pclVpto);
					continue;
				}
	       		 }
			 else 
			 {
				dbg(TRACE,"<%s> VPTO is null -> Valid", pclFunc, pclVpto);
				ilRC = RC_SUCCESS;
				memset(pclTmpALC3,0,sizeof(pclTmpALC3));
				strncpy(pclTmpALC3,pclAlc3,strlen(pclAlc3));
				////////////
				break;
			 }
		}
	}
  	close_my_cursor(&slSqlCursor);
	/////////////////	

	/*ilReturnValue = RunSQL( pclSqlBuf, pclSqlData );
	if( ilReturnValue != DB_SUCCESS )
	{
		
		dbg(TRACE,"@@@@@@@Not Found\n");
	}
	else
	{
		char pclAlc3[20] = "";
		char pclVpfr[20] = "";
		char pclVpto[20] = "";
		
		get_fld(pclSqlData,FIELD_1,STR,20,pclAlc3);
		get_fld(pclSqlData,FIELD_2,STR,20,pclVpfr);
  		get_fld(pclSqlData,FIELD_3,STR,20,pclVpto);
  		dbg( TRACE, "ALC3<%s>,VPFR <%s> VPTO <%s>", pclAlc3, pclVpfr, pclVpto );
		//dbg(TRACE,"@@@@@@@pclSqlData<%s>\n",pclSqlData);
	}
	*/
	}
	#endif

	if( ilRC == 0 && strlen(pclTmpALC3) == 3 && pclTmpALC3[strlen(pclTmpALC3) - 1] != ' ' )
	{	
	      sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmpALC3);
	    	dbg(DEBUG,"syslibSearchDbData succed, use ALC3-----ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmpALC3);
	}	
      	else
      	{
      		dbg(TRACE,"syslibSearchDbData fails or return ALC3 length is not three or ALC3 last leter is space, use ALC2");

				//FYA 2013/11/15 v1.9
				memset(pclTmpALC3,0,sizeof(pclTmpALC3));
				//FYA 2013/11/15 v1.9		
				strncpy(pclTmpALC3,pclTmpALC2,strlen(pclTmpALC2));
	      
				sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmpALC3);
	    	
				dbg(DEBUG,"######ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmpALC3);
      		//return 2;
      	}
	  
	
              sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);
	      //dbg(DEBUG,"three letter is <%s>\n",pclTmp);
	      sprintf(pclTmpflightNo,"\t<flightNo%d>%s%s\t</flightNo%d>\n",ilj+1,pclTmpALC2Pack,pclTmpFLNOPack,ilj+1);
	      strcat(pclXML,pclTmpflightNo);
	      
	      ilj++;

	}
	else
	{//ALC3
	      sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmp);
	      sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);
	
	      //dbg(DEBUG,"three letter is <%s>\n",pclTmp);
	      sprintf(pclTmpflightNo,"\t<flightNo%d>%s%s\t</flightNo%d>\n",ilj+1,pclTmpALC2Pack,pclTmpFLNOPack,ilj+1);
	      strcat(pclXML,pclTmpflightNo);
	      
	      ilj++;
	}
    }
    }   
  	strcat(pclXML,"\t</codeshare>\n");
  	//dbg(DEBUG,"\n+++<%s>+++",pclXML);
  	return 0;    
}

int TrimSpace( char *pcpInStr )
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

static void TrimRight(char *s)
{
    int i = 0;    /* search for last non-space character */
    for (i = strlen(s) - 1; i >= 0 && isspace(s[i]); i--); /* trim off right spaces */    
    s[++i] = '\0';
}
int IS_EMPTY (char *pcpBuf) 
{
    return  (!pcpBuf || *pcpBuf==' ' || *pcpBuf=='\0') ? TRUE : FALSE;
}

static int RunSQL( char *pcpSelection, char *pcpData )
{
    int ilRc = DB_SUCCESS;
    short slSqlFunc = 0;
    short slSqlCursor = 0;
    char pclErrBuff[128];
    char *pclFunc = "RunSQL";

    dbg( DEBUG, "%s: SQL <%s>", pclFunc, pcpSelection );
    ilRc = sql_if ( START, &slSqlCursor, pcpSelection, pcpData );
    close_my_cursor ( &slSqlCursor );

    if( ilRc == DB_ERROR )
    {
        get_ora_err( ilRc, &pclErrBuff[0] );
        dbg( TRACE, "%s OraError! <%s>", pclFunc, &pclErrBuff[0] );
    }
    return ilRc; 
}

static char *GetKeyItem(char *pcpResultBuff, long *plpResultSize, 
char *pcpTextBuff, char *pcpKeyWord, char *pcpItemEnd, int bpCopyData)
{
    long llDataSize    = 0L;
    char *pclDataBegin = NULL;
    char *pclDataEnd   = NULL;
    pclDataBegin = strstr(pcpTextBuff, pcpKeyWord);/* Search the keyword*/
    
    if (pclDataBegin != NULL)
    {
    	/* Did we find it? Yes.*/
			pclDataBegin += strlen(pcpKeyWord);/* Skip behind the keyword*/
			pclDataEnd = strstr(pclDataBegin, pcpItemEnd);/* Search end of data*/
			if (pclDataEnd == NULL)
			{
				/* End not found?*/
    		pclDataEnd = pclDataBegin + strlen(pclDataBegin);/* Take the whole string*/
			} /* end if */
			llDataSize = pclDataEnd - pclDataBegin;/* Now calculate the length*/
			if (bpCopyData == TRUE)
			{/* Shall we copy?*/
			    strncpy(pcpResultBuff, pclDataBegin, llDataSize);/* Yes, strip out the data*/
			} /* end if */
    } /* end if */
    if (bpCopyData == TRUE)
    {/* Allowed to set EOS?*/
			pcpResultBuff[llDataSize] = 0x00;/* Yes, terminate string*/
    } /* end if */
    *plpResultSize = llDataSize;/* Pass the length back*/
    return pclDataBegin;/* Return the data's begin*/
}
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
