#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/exchdl.c 1.57 04/09/2014 16:42:20 AM FYA Exp  $";
#endif /* _DEF_mks_version */


/*
 UFIS AS EXCHDL.C

 Author         : Andreas Kloidt
 Date           : February 2005
 Description    : Process to receive and send XMS information

 Update history :
 20120409 MEI: Include EFPS changes for Dubai (v1.39)
 20120420 MEI: Send blank parking stand. Add new type "MD" and "MA"
               MD = mandatory for dep. MA = mandatory for arr (v1.40)
 20120426 FYA: modify the bug in HandleWEBIn. That is, PSA and PPV both have
 Arrival and Departure flight. When it is PSA data, then only send INSERT
 command for arrival and depature flight. Otherwise(PPV), send the command
 according to the XML RequestType for arrival and departure flight
 20120510 MEI: Missing WHERE for selection to 506 - EFPS - DXB (v1.43a)
 20120515 MEI: New config to trim 0 from CSGN (v1.43)
 20120516 MEI: CSGN may not be received from FLIGHT (v1.44)
 20120531 FYA: Add process_without_CCATAB config to get data from que
 instead of CCATAB(v1.45) UFIS-1901
 20120531 MEI: EFPS - give VIA/ORG for arrival (v1.46)
 20120619 FYA: adding BKK_CUSTOMIZATION config option, and change the head comment format
 20120713 MEI: Merge with EFPS changes - give VIA/DES for departure (v1.48)
 20120714 CST: Use A fields for EFPS estimates and enable EFPS PUSI -> ETDC (COB)
 20120727 FYA: Adding conditions(pcgInstallation == "BLR" && pcgBLRCustomization == "YES")
 							 for copying data from received data list.
 20121109 FYA: Fix bug in field copy
 20121224 FYA: HoneyWell interface modification
 20130131 MEI: Honeywell usage of URNO is wrong, this will cause issue in EFPS interface v1.53
 20130416 WON: Resolve & issue.
 20130514 WON: Resolve missing AURN issue that causes ORA Errors (UFIS-3349)
             : Resolve overlapping fields (Jira 3397).  No initialization of pclFldNam2 in GetXmlLineIdxFreeTag.
 ******************************************************************************/
/*                                                                            */
/* source-code-control-system version string                                  */
static char sccs_exchdl[]="%Z% UFIS 4.4 (c) ABB AAT/I %M% %I% / %E% %U% / AKL";
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
#include "db_if.h"
#include "tools.h"
#include "helpful.h"
#include "timdef.h"

#define XS_BUFF  128
#define S_BUFF   512
#define M_BUFF   1024
#define L_BUFF   2048
#define XL_BUFF  4096
#define XXL_BUFF 8192

//#define FRANK

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
FILE *outp       = NULL;
int  debug_level = TRACE;
/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int SetSignals(void (*HandleSignal)(int));
extern int DebugPrintItem(int,ITEM *);
extern int DebugPrintEvent(int,EVENT *);
extern int init_db(void);
extern int  ResetDBCounter(void);
extern void HandleRemoteDB(EVENT*);
extern int  sql_if(short ,short* ,char* ,char* );
extern int close_my_cursor(short *cursor);
extern void snap(char*,int,FILE*);
extern int GetDataItem(char *pcpResult, char *pcpInput, int ipNum, char cpDel,char *pcpDef,char *pcpTrim);
extern int BuildItemBuffer(char *pcpData, char *pcpFieldList, int ipNoOfFields, char *pcpSepChr);
extern int get_item_no(char *s, char *f, short elem_len);
extern void  GetServerTimeStamp(char*,int,long,char*);
extern int AddSecondsToCEDATime(char *,long,int);
extern int get_real_item(char *, char *, int);
extern int GetNoOfElements(char *s, char c);
extern long nap(long);
extern int get_item(int,char*,char*,int,char*,char*,char*);
extern int SendCedaEvent(int,int,char*,char*,char*,char*,char*,char*,char*,char*,char*,char*,int,int);
extern int GetFullDay(char*,int*,int*,int*);
extern int syslibSearchDbData(char*,char*,char*,char*,char*,int*,char*);
extern int MakeTokenList(char*,char*,char*,char);

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;        /* The queue item pointer  */
static EVENT *prgEvent     = NULL;        /* The event pointer       */
static int   igItemLen     = 0;           /* length of incoming item */
static int   igInitOK      = FALSE;      /* Flag for init */

/*   static CFG   *prgCfg; */                     /* structure of the cfg-file */
static char  pcgHomeAp[XS_BUFF];      /* buffer for home airport */
static char  pcgHomeAp4[XS_BUFF];      /* buffer for home airport 4-letters */
static char  pcgTabEnd[XS_BUFF];      /* buffer for TABEND */
static char  pcgTwStart[XS_BUFF] = "";
static char  pcgTwEnd[XS_BUFF];
static char  pcgConfFile[S_BUFF];      /* buffer for config-file name */
static int   igUseHopo = FALSE;          /* flag for use of HOPO-field */
static int   igModID_Router  = 1200;      /* MOD-ID of Router  */
static int   igModID_IfCtrl  = 7208;      /* MOD-ID of Interface Control Line  */

static char      pcgHostName[XS_BUFF];
static char      pcgHostIp[XS_BUFF];

static int    igQueCounter=0;
static int    igQueOut=0;

static int igMsgCount = 0;
static char pcgMsgIdOld[32] = "\0";
static char pcgMsgId[32] = "\0";

/*entry's from configfile*/
static char pcgConfigFile[512];
static char pcgCfgBuffer[512];
static EVENT *prgOutEvent = NULL;
static char pcgCurrentTime[32];
static int igDiffUtcToLocal;
static char pcgServerChars[110];
static char pcgClientChars[110];
static char pcgUfisConfigFile[512];

static char pcgCurCmd[64];
static char pcgStatusMsgTxt[1024];
static char *pcgStatusMsgBuf = NULL;
static int igStatusMsgBufLen = 0;
static int igStatusMsgLen = 0;
static int igStatusMsgIni = 0;
static int igSearchFlightHits = 0;
/* MEI v1.44 */
static int igDelZeroFromCsgn = 0;
/* MEI v1.45 */
static int igUseViaAsOrg = 0;
/* MEI v1.47 */
static int igUseViaAsDes = 0;
/* CST v1.49 */
static int igEFPSDestination = 7800;
static int igCOBHandling = FALSE;


static char pcgAfttabFields[1024];
static char pcgHermesFields[1024];

/* XML structure */
#define MAX_FILE_SIZE 512000
#define MAX_XML_LINES 2000
#define MAX_INTERFACES 50
typedef struct
{
  int ilLevel;
  char pclTag[32];
  char pclName[32];   /* MEI */
  //char pclType[8];
  char pclType[32]; //Frank v1.52 20121224
  char pclBasTab[32];
  char pclBasFld[32];
  char pclFlag[64];
  char pclMethod[MAX_INTERFACES][32];
  int ilNoMethod;
  char pclFieldA[MAX_INTERFACES][16];
  int ilNoFieldA;
  char pclFieldD[MAX_INTERFACES][16];
  int ilNoFieldD;
  int ilRcvFlag;
  int ilPrcFlag;
  char pclData[4000];
  char pclOldData[4000];
} _LINE;

typedef struct
{
  _LINE rlLine[MAX_XML_LINES];
} _XML;
_XML rgXml;
static int igCurXmlLines = 0;

typedef struct
{
  char pclIntfName[32];
  int ilIntfIdx;
} _INTERFACE;
_INTERFACE rgInterface[MAX_INTERFACES];
static int igCurInterface = 0;
/* End XML structure */
typedef struct
{
  char pclField[32];
  char pclType[8];
  char pclMethod[32];
  char pclData[1024];
} _XMLKEY;
_XMLKEY rgKeys[16];
static int igNoKeys;
typedef struct
{
  char pclField[32];
  char pclType[32];
  char pclMethod[32];
  char pclData[1024];
  char pclOldData[1024];
} _XMLDATA;
_XMLDATA rgData[1000];
static int igNoData;
static char pcgOutBuf[500000];
static char pcgAdid[4];
static char pcgUrno[16];
static char pcgIntfName[32];
static int igIntfIdx;
static int igVdgsTimeDiff;
static int igTimeWindowArrBegin;
static int igTimeWindowArrEnd;
static int igTimeWindowDepBegin;
static int igTimeWindowDepEnd;
static int igMaxTimeDiffForUpdate;
static int igActualPeriod;
static char pcgNewUrnos[1000*16];
static int igLastUrno = 0;
static int igLastUrnoIdx = -1;
static char pcgMode[16];
static char pcgSubMode[16];
static int igCompressTag = FALSE;
static int igSimulation = FALSE;
static char pcgSendOutput[16];
static char pcgRepeatKeyFieldsInBody[16];
static char pcgSectionsForKeyFieldsCheck[1024];
static char pcgVersion[1024];
static int igFtypChange;
static int igAutoActType;
static int igReturnFlight;
static char pcgSaveInserts[200][8000];
static int igSavInsCnt = 0;
static char pcgSaveDeletes[200][16];
static int igSavDelCnt = 0;
static int igUsingMultiTypeFlag = FALSE;
static int igMultipleSectionFound = FALSE;
static char pcgMultipleSection[64];
static char pcgListOfMultipleSections[1024];
static char pcgListOfMultipleMethodTriggers[1024];
static char pcgMultipleSectionSetTrigger[1024];
static char pcgMultipleSectionTriggered[1024];
static char pcgHandleReturnFlights[64];
static char pcgSaveReturnFlights[200][16];
static int igSavRetCnt = 0;
static char pcgCDStart[64];
static char pcgCDEnd[64];
static char pcgDesForTouchAndGo[64];
static char pcgInstallation[64];
static char pcgAUH_Hermes[64];
static char pcgCustomization[64];
static int igUseUrno = TRUE;
static char pcgViaFldLst[1024];
static char pcgViaDatLst[1024];
static char pcgViaApc[16];
static int igNBIATowing = TRUE;
static int igNBIAReturnFlight = TRUE;
static char pcgMessageId[64];
static char pcgMessageType[64];
static char pcgMessageOrigin[64];
static char pcgTimeId[64];
static char pcgTimeStamp[64];
static char pcgTimeStampDate[64];
static char pcgTimeStampTime[64];
static int  igPSAInsertOnly;
static char pcgActionType[64];
static char pcgActionTypeIns[16];
static char pcgActionTypeUpd[16];
static char pcgActionTypeDel[16];
static char pcgArrivalSections[128] = "";
static char pcgDepartureSections[128] = "";
//FYA v1.45 UFIS-1901
static char pcgProceedWithoutCCATAB[128] = "\0";
static char pcgBKKCustomization[128] = "\0";

static char pcgTowingSections[128] = "\0";
static int igMsgId = 0;
static unsigned igMsgIdHoneyWell = 1;

static char pcgBLRCustomization[128] = "\0";
static int igChangeTheMessageTypeForBlrPortalDailyBulkData = 0;

//Frank v1.52 20121224
static char pcgDXBCustomization[128] = "\0";
static char pcgDXBHoneywell[128] = "\0";
static char pcgTableName[128] = "\0";
static char pcgRemoveSeparator[8] = "\0";
static char pcgSeparator[8] = "\0";
static char cgSeparator = '\0';
static char pcgGetUrnoHead[128]="\0";
static char pcgGetUrnoTail[128]="\0";
static char pcgModifySGDTTimeFormat[128] = "\0";
static char pcgIgnoreKeySection[128] = "\0";
static char pcgChgAdid[16] = "\0";
//Frank v1.52 20121224

static char pcgCheckPosition[32];
static char pcgPosSel[128];

static char pcgTagAttributeName[10][128];
static char pcgTagAttributeValue[10][128];

static int igRsid;
static int igRsty;
static int igRaid;
static int igAlcd;
static int igHanm;
static int igActs;
static int igActe;
static int igSchs;
static int igSche;
static int igDisp;
static int igRmrk;
static int igUrno;
static int igFlno;
static int igCsgn;
static int igStdt;
static int igAdid;
static int igToid;
static int igPstd;
static int igPsta;
static int igTwtp;
static char pcgGMSResRsid[16];
static char pcgGMSResRsty[16];
static char pcgGMSResRaid[16];
static char pcgGMSResAlcd[16];
static char pcgGMSResHanm[32];
static char pcgGMSResActs[16];
static char pcgGMSResActe[16];
static char pcgGMSResSchs[16];
static char pcgGMSResSche[16];
static char pcgGMSResDisp[64];
static char pcgGMSResRmrk[64];
static char pcgGMSResUrno[16];
static char pcgGMSResFlno[16];
static char pcgGMSResCsgn[16];
static char pcgGMSResStdt[16];
static char pcgGMSResAdid[16];
static char pcgGMSResToid[16];
static char pcgGMSResPstd[16];
static char pcgGMSResPsta[16];
static char pcgGMSResTwtp[16];
static char pcgUrnoListArr[120000];
static char pcgUrnoListDep[120000];
static char pcgSitaText[512000];
static char pcgUrnoListPdeDep[10000];
static char pcgUrnoListPdeTowA[10000];
static char pcgUrnoListPdeTowD[10000];

typedef struct
{
  char pclFldNam[16];
  char pclFldNamUpd[16];
  int ilBgn;
  int ilLen;
  char pclFldDat[4096];
} _FILEDATA;
_FILEDATA rgFileData[256];
static int igNoFileData;

typedef struct
{
	char pclAlc3[16];
	char pclFltn[16];
	char pclFlns[16];
	char pclStoad[16];
	char pclAdid[16];
}FLIGHT_ID;

static int    Init_exchdl();
static int    Reset(void);                        /* Reset program          */
static void   Terminate(int);                     /* Terminate program      */
static void   HandleSignal(int);                  /* Handles signals        */
static void   HandleErr(int);                     /* Handles general errors */
static void   HandleQueErr(int);                  /* Handles queuing errors */
static int    HandleInternalData(void);           /* Handles event data     */
static void   HandleQueues(void);                 /* Waiting for Sts.-switch*/
/******************************************************************************/
/* Function prototypes by AKL                                                 */
/******************************************************************************/
/* Init-functions  */

static int GetQueues();
static int SendEvent(char *pcpCmd,int ipModID,int ipPriority,char *pcpTable,
                     char *pcpTwStart,char* pcpTwEnd,
             char *pcpSelection,char *pcpFields,char *pcpData,
                     char *pcpAddStruct,int ipAddstructSize);
static int GetConfig();
static int TimeToStr(char *pcpTime,time_t lpTime);
static int TriggerBchdlAction(char *pcpCmd, char *pcpTable, char *pcpSelection,
                              char *pcpFields, char *pcpData, int ipBchdl, int ipAction);
static long GetSecondsFromCEDATime(char *pcpDateTime);
static void TrimRight(char *pcpBuffer);
static void TrimLeftRight(char *pcpBuffer);
static void CheckCDATA(char *pcpBuffer);
static int MyAddSecondsToCEDATime(char *pcpTime, long lpValue, int ipP);
static char *GetLine(char *pcpLine, int ipLineNo);
static void CopyLine(char *pcpDest, char *pcpSource);
static int GetXMLSchema();
static int CheckXmlInput(char *pcpData);
static int HandleInput(char *pcpData, int ipBegin, int ipEnd);
static int HandleUpdate(char *pcpActionType);
static int HandleInsert();
static int HandleDelete();
static int HandleAck();
static int GetKeys(char *pcpFkt);
static int ShowKeys();
static int GetData(char *pcpMethod, int ipIdx, char *pcpName);
static int ShowData();
static int ShowAll();
static int SearchFlight(char *pcpMethod, char *pcpTable);
static int SearchTurnAroundFlight(char *pcpMethod);
static int CheckData(char *pcpData, int ipIdx);
static int CheckFlightNo(char *pcpFlight);
static int DataAddOns();
static int GetDataLineIdx(char *pcpField, int ipIdx);
static int ValidateAdid();
static int AdjustData();
static int HandleResources(char *pcpFkt);
static int HandleLocation(char *pcpFkt, char *pcpType, int ipNo);
static int GetLocaData(int ipRaid1, int ipRaid2,
                       char *pcpFld1L1, char *pcpFld2L1, char *pcpFld3L1, char *pcpFld4L1, char *pcpFld5L1,
                       char *pcpFld1L2, char *pcpFld2L2, char *pcpFld3L2, char *pcpFld4L2, char *pcpFld5L2,
                       char *pcpOut1L1, char *pcpOut2L1, char *pcpOut3L1, char *pcpOut4L1, char *pcpOut5L1, char *pcpOut6L1,
                       char *pcpOut1L2, char *pcpOut2L2, char *pcpOut3L2, char *pcpOut4L2, char *pcpOut5L2, char *pcpOut6L2);
static int GetNextUrno();
static int HandleCounter(char *pcpFkt, char *pcpType);
static int HandleBlkTab(char *pcpFkt);
static int HandleTowIns(char *pcpSection);
static int HandleTowUpd(char *pcpSection);
static int HandleTowDel(char *pcpSection);
static int HandleOutput(char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleFlightOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);

//frank 20120613 16:38
static int HandleRemarksOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleAirlineOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleAirportOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int IsAnyOfAlc2Alc3AlfnNull(char *Alc2,char *Alc3,char *Alfn);
static int IsAnyOfApc3Apc4ApfnNull(char *Apc3,char *Apc4,char *Apfn);

static int HandleBulkData(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);

static int MarkOutput();
static int BuildOutput(int ipIndex, int *ipCount, char *pcpCurSec, char *pcpType, char *pcpIntfNam,
                       char *pcpMode);

//Frank v1.52 20121224
static int BuildOutputDXB_HoneyWell(int ipIndex, int *ipCount, char *pcpCurSec, char *pcpType, char *pcpIntfNam,
                       char *pcpMode, char *pcpUrno,char *pcpFtyp,char *pcpActionType);

static int BuildHeader(char *pcpActType);
static int GetXmlLineIdx(char *pcpTag, char *pcpName, char *pcpSection);
static int GetXmlLineIdxFreeTag(char *pcpTag, char *pcpName, char *pcpSection);
static int GetKeyFieldList(char *pcpFieldList, int *ipNoEle);
static int StoreKeyData(char *pcpFieldList, char *pcpDataBuf, int ipNoEle, char *pcpOrgFields, char *pcpNewData, char *pcpOldData, char *pcpCommand, int ipTowing);
static int GetInterfaceIndex(char *pcpInterfaceName);
static int GetManFieldList(char *pcpSection, char *pcpFieldList, int *ipNoEle, char *pcpRcvFieldList, int ipLocSec);
static int GetFieldList(char *pcpSection, char *pcpFieldList, int *ipNoEle);
static int StoreData(char *pcpFieldList, char *pcpNewData, char *pcpOldData, int ipNoEle, char *pcpCurSec, int *ipDatCnt);
static int RemoveData(char *pcpFieldList, int ipNoEle, char *pcpCurSec, int *ipDatCnt, int ipStep);
static int ClearSection(char *pcpCurSec);
static int HandleRaid(char *pcpUrno, int ipRaid, char *pcpCurSec, char *pcpFields,
                      char *pcpNewData, char *pcpOldData, char *pcpFkt1, char *pcpCtyp, char *pcpRarUrno, char *pcpAddFldLst);
static int GetXmlLineIdxBas(char *pcpSection, int ipCount);
static int HandleCounterOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleBlktabOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleSitaOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleTableOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleAckOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData);
static int HandleFileRequest(char *pcpSection);
static int HandleFileReady(char *pcpSection);
static int HandleDataSync(char *pcpSection);
static int HandleRequest(char *pcpSection);
static int HandleFile(char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpFields, char *pcpNewData);
static int HandleGMSBelt(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSGate(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSPos(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSCounterDedi(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSCounterCom(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSTowing(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int HandleGMSNoop(char *pcpFile, char *pcpSeperator, char *pcpFields);
static int GetMinMaxTimes(char *pcpLine, char *pcpSeperator, char *pcpMinArr, char *pcpMaxArr,
                          char *pcpMinDep, char *pcpMaxDep, char *pcpType);
static int GetUrnoList(char *pcpMinArr, char *pcpMaxArr, char *pcpMinDep, char *pcpMaxDep, char *pcpTable, char *pcpType,
                       char *pcpExclCounterList);
static int CheckGMSResLine(char *pcpLine, char *pcpSeperator, char *pcpType);
static int CheckGMSTowLine(char *pcpLine, char *pcpSeperator);
static int CheckGMSNoopLine(char *pcpLine, char *pcpSeperator);
static int RemoveUrno(char *pcpUrno, char *pcpType);
static int HandleGMSResource(char *pcpUrno, char *pcpType, char *pcpLine);
static int HandleGMSResourceDcnt(char *pcpUrno, char *pcpType, char *pcpLine);
static int HandleGMSResourceCcnt(char *pcpUrno, char *pcpType, char *pcpLine);
static int HandleGMSResTow(char *pcpUrno, char *pcpLine);
static int HandleGMSResNoop(char *pcpUrno, char *pcpLine);
static int HandleGMSDelete(char *pcpAdid, char *pcpType);
static int HandleGMSDeleteCnt(char *pcpAdid, char *pcpType);
static int HandleGMSDeleteTow(char *pcpAdid);
static int HandleGMSDeleteNoop();
static int HandleGMSFileOut(char *pcpFile, char *pcpSeperator, char *pcpFieldList, char *pcpSelection);
static int ReplaceItem(char *pcpList, int ipItemNo, char *pcpNew);
static int RemoveItem(char *pcpFields, char *pcpData, int ipItemNo);
static int RemoveCharFromTag(char *pcpTag, char *pcpNewTag);
static int CheckForInsert();
static int CheckForInsertDel();
static int CheckForDelFromCache();
static int CheckForDelFromCacheDel();
static int HandleGMSLongTerm(char *pcpFile);
static int GetXMLDataItem(char *pcpResult, char *pcpLine, char *pcpTag);
static int HandleGMSShortTerm(char *pcpFile);
static int HandleSitaSections(char *pcpSection);
static int HandleTableData(char *pcpSection);
static int RemoveSection(char *pcpData);
static int CheckIfDepUpd(char *pcpFields, char *pcpNewData, char *pcpOldData);
static int ChangeCharFromTo(char *pcpData,char *pcpFrom,char *pcpTo);
static int HardCodedVDGS(char *pcpCommand, char *pcpFields, char *pcpNewData, char *pcpOldData, char *pcpSec);
static int HardCodedVDGSWAW(char *pcpCommand, char *pcpFields, char *pcpNewData,
                            char *pcpOldData, char *pcpSec);
static int HardCodedVDGSDXB(char *pcpCommand, char *pcpFields, char *pcpNewData,
                            char *pcpOldData, char *pcpSec);
static int CheckIfActualData(char *pcpType, char *pcpFields, char *pcpData, int ipNoEle);
static int GetTrailingBlanks(char *pcpBlanks, char *pcpData, int ipLen);
static int HandleEqiUsage(char *pcpFile);
static int HandleEqiUsageWAW();
static int GetTimeDiffUtc(char *pcpDate, int *ipTDIS, int *ipTDIW);
static int HandleArsData(char *pcpFile);
static int GetFieldConfig(char *pcpFieldList, char *pcpFieldListUpd, char *pcpFieldLenList, char *pcpSection);
static int HandleSeasonData(char *pcpFile);
static int HandleDailyData(char *pcpFile);
static int HandleDailyDataUpdate(char *pcpFile, char *pcpSection);
static int HandleCounterData(char *pcpFile);
static int HandleBasicData(char *pcpFile, char *pcpTableName);
static int HandlePlbData(char *pcpFile);
static int GetFileName(char *pcpFile);
static void SetStatusMessage(char *pcpTxt);

static int HandleFlightRelatedData(char *pcpFkt);
static int CheckReceivedTags(char *pcpTagLst);

static void ConvertVialToRout(char *pcpData);
static void AddViaInfoToRoute(char *pcpField, char *pcpData);

static int HandlePDAIn();
static int HandlePDAOut(char *pcpType, char *pcpParam, char *pcpFields, char *pcpNewData, char *pcpOldData,
                        int ipIndex, char *pcpCurSec, char *pcpCurIntf);
static int HandleWEBIn();
static int GetWebData(int ipPSA, char *pcpTag1, char *pcpTag2, char cpMode1, char cpMode2, int ipLen,
                      char *pcpFldLstA, char *pcpDatLstA, char *pcpFldLstD, char *pcpDatLstD);
static int HandleAftSync(char *pcpFields, char *pcpData);
static int RunSQL( char *pcpSelection, char *pcpData );

/* MEI v1.44 */
static int HardCodedEFPSDXB();
static int DeleteZeroFromCsgn( char *pcpCsgn );

//Frank v1.52 20121224
static void get_fld_value(char *buf,short fld_no,char *dest);
static int get_flds_count(char *buf);
static void FindNextStandId(char *pcpUrno,char *pcpNpst);
static void AddUrnoIntoXml(char *pcpData,char *pclTmpLine);
static int HardCodedHoneyWellDXB(char *pcpTagName);

static void CheckMsgId(char *pcpData, char * pcpSelection);
static void RemoveEndSeparator(char *pcpNewData);

static int AddAdid_EFPTAB_HWE(char *pcpTable,char *pcpCommand,char *pclFieldList,char *pclFields,char *pclNewData,char *pclDataBuf,char *pclOldData);

/* FYA v1.45 UFIS-1901 */
static void GetTheFieldAndOldDataForProceedWithoutCCATAB(char *pcpFields,char *pcpData,char *pcpCcaCkicTemp,
                                                char *pcpCcaCtypTemp,char *pcpCcaCkitTemp,char *pcpCcaCkbsTemp,char *pcpCcaCkesTemp,
                                                char *pcpCcaFlnuTemp,char *pcpCcaUrnoTemp);

static int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData);
static int getFltId(FLIGHT_ID *rpFltId, char *pcpFieldList, char *pcpDataList);
static int getFlightUrno(char *pcpUrno, FLIGHT_ID rlFltId);
static int getHermesFieldList(char *pcphermesDataList, char *pcpFlightDataList, char *pcpFlightFieldList, char *pcpFieldList, char *pcpDataList, char *pcpHermesFields);
static int handleHemers(char *pcpSecName, char *pcpFieldList, char *pcpDataList, char *pcpDestModId, int ipPrio, char *pcpCmd, char *pcpTable);
static void removeTag(char *pcpData, char *pcpTagName);
/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
  int    ilRc = RC_SUCCESS;            /* Return code            */
  int    ilCnt = 0;
  int   ilItemFlag=TRUE;
  time_t now = 0;
  INITIALIZE;            /* General initialization    */

  /* signal handling of SIGPIPE,SIGCHLD,SIGALRM,SIGTERM */
  SetSignals(HandleSignal);

  dbg(TRACE,"------------------------------------------");
  dbg(TRACE,"MAIN: version <%s>",sccs_exchdl);

  /* Attach to the MIKE queues */
  do{
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
      dbg(TRACE,"MAIN: init_que() OK!");
      dbg(TRACE,"MAIN: mod_id   <%d>",mod_id);
      dbg(TRACE,"MAIN: mod_name <%s>",mod_name);
    }/* end of if */
  do
    {
      ilRc = init_db();
      if (ilRc != RC_SUCCESS)
    {
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
      dbg(TRACE,"MAIN: init_db()  OK!");
    } /* end of if */

  /* logon to DB is ok, but do NOT use DB while ctrl_sta == HSB_COMING_UP !!! */
  *pcgConfFile = 0x00;
  sprintf(pcgConfFile,"%s/%s",getenv("BIN_PATH"),mod_name);
  ilRc = TransferFile(pcgConfFile);
  if(ilRc != RC_SUCCESS)
    {
      dbg(TRACE,"MAIN: TransferFile(%s) failed!",pcgConfFile);
    } /* end of if */
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
      ilRc = Init_exchdl();
      if(ilRc == RC_SUCCESS)
        {
          dbg(TRACE,"");
          dbg(TRACE,"------------------------------------------");
          dbg(TRACE,"MAIN: initializing OK");
          igInitOK = TRUE;
        }
    }
    }else{
      Terminate(1);
    }
  dbg(TRACE,"------------------------------------------");

  if (igInitOK == TRUE)
    {
      now = time(NULL);
      while(TRUE)
    {
      memset(prgItem,0x00,igItemLen);
      ilRc = que(QUE_GETBIG,0,mod_id,PRIORITY_3,igItemLen,(char *)&prgItem);
      dbg(DEBUG,"QUE Counter %d",++igQueCounter);
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
          Terminate(1);
          break;
        case    HSB_STANDALONE    :
          ctrl_sta = prgEvent->command;
          ResetDBCounter();
          break;
        case    REMOTE_DB :
          /* ctrl_sta is checked inside */
          HandleRemoteDB(prgEvent);
          break;
        case    SHUTDOWN    :
          /* process shutdown - maybe from uutil */
          Terminate(1);
          break;
        case    RESET        :
          ilRc = Reset();
          break;
        case    EVENT_DATA    :
          if((ctrl_sta == HSB_STANDALONE) ||
             (ctrl_sta == HSB_ACTIVE) ||
             (ctrl_sta == HSB_ACT_TO_SBY))
            {
              ilItemFlag=TRUE;
              ilRc = HandleInternalData();
              if(ilRc != RC_SUCCESS)
            {
              HandleErr(ilRc);
            }/* end of if */
            }
          else
            {
              dbg(TRACE,"MAIN: wrong HSB-status <%d>",ctrl_sta);
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
                case  111 :
                  break;
        default            :
          dbg(TRACE,"MAIN: unknown event");
          DebugPrintItem(TRACE,prgItem);
          DebugPrintEvent(TRACE,prgEvent);
          break;
        } /* end switch */
        }else{
          /* Handle queuing errors */
          HandleQueErr(ilRc);
        } /* end else */



      /**************************************************************/
      /* time parameter for cyclic actions                          */
      /**************************************************************/

      now = time(NULL);

    } /* end while */
    }else{
      dbg(TRACE,"MAIN: Init_exchdl() failed with <%d> Sleeping 30 sec.! Then terminating ...",ilRc);
      sleep(30);
    }
  exit(0);
  return 0;
} /* end of MAIN */

/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int Init_exchdl()
{
  int    ilRc = RC_SUCCESS;            /* Return code */
  int ilCount = 0;

  GetQueues();
  /* reading default home-airport from sgs.tab */
  memset(pcgHomeAp,0x00,sizeof(pcgHomeAp));
  ilRc = tool_search_exco_data("SYS","HOMEAP",pcgHomeAp);
  if (ilRc != RC_SUCCESS)
  {
      dbg(TRACE,"Init_exchdl : No HOMEAP entry in sgs.tab: EXTAB! Please add!");
      return RC_FAIL;
  }
  else
  {
      dbg(TRACE,"Init_exchdl : HOMEAP = <%s>",pcgHomeAp);
  }

  /* MEI v1.45 */
  memset(pcgHomeAp4,0x00,sizeof(pcgHomeAp4));
  syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pcgHomeAp4,&ilCount,"\n");
  dbg(TRACE,"Init_exchdl : HOMEAP4 = <%s>",pcgHomeAp4);


  /* reading default table-extension from sgs.tab */
  memset(pcgTabEnd,0x00,sizeof(pcgTabEnd));
  ilRc = tool_search_exco_data("ALL","TABEND",pcgTabEnd);
  if (ilRc != RC_SUCCESS)
    {
      dbg(TRACE,"Init_exchdl : No TABEND entry in sgs.tab: EXTAB! Please add!");
      return RC_FAIL;
    }
  else
    {
      dbg(TRACE,"Init_exchdl : TABEND = <%s>",pcgTabEnd);
      memset(pcgTwEnd,0x00,XS_BUFF);
      sprintf(pcgTwEnd,"%s,%s,%s",pcgHomeAp,pcgTabEnd,mod_name);
      dbg(TRACE,"Init_exchdl : TW_END = <%s>",pcgTwEnd);

      if (strcmp(pcgTabEnd,"TAB") == 0)
    {
      igUseHopo = TRUE;
      dbg(TRACE,"Init_exchdl: use HOPO-field!");
    }
    }

  ilRc = GetConfig();

  ilRc = TimeToStr(pcgCurrentTime,time(NULL));

  igSavInsCnt = 0;
  igSavDelCnt = 0;
  igSavRetCnt = 0;

  strcpy(pcgUrnoListPdeDep,"");
  strcpy(pcgUrnoListPdeTowA,"");
  strcpy(pcgUrnoListPdeTowD,"");

  return(ilRc);
} /* end of initialize */
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
  int ilRc = RC_FAIL;

  /* get mod-id of router */
  if ((igModID_Router = tool_get_q_id("router")) == RC_NOT_FOUND ||
      igModID_Router == RC_FAIL || igModID_Router == 0)
    {
      dbg(TRACE,"GetQueues   : tool_get_q_id(router) returns: <%d>",igModID_Router);
      ilRc = RC_FAIL;
    }else{
      dbg(TRACE,"GetQueues   : <router> mod_id <%d>",igModID_Router);
      ilRc = RC_SUCCESS;
    }
  return ilRc;
}

static int CheckCfg(void)
{
  int ilRc = RC_SUCCESS;
  return ilRc;
}
/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{

  int    ilRc = RC_SUCCESS;    /* Return code */

  dbg(TRACE,"Reset: now reseting ...");

  return ilRc;

} /* end of Reset */
/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(int ipSleep)
{
  dbg(TRACE,"Terminate: now leaving ...");

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
      break;
    case SIGPIPE:
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
static void HandleErr(int pipErr)
{
  /*    int    ilRc = RC_SUCCESS; */
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
  int    ilRc = RC_SUCCESS;            /* Return code */
  int    ilBreakOut = FALSE;

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
      }else{
    /* Handle queuing errors */
    HandleQueErr(ilRc);
      } /* end else */
  } while (ilBreakOut == FALSE);
  if(igInitOK == FALSE)
    {
      ilRc = Init_exchdl();
      if(ilRc == RC_SUCCESS)
    {
      dbg(TRACE,"HandleQueues: Init_exchdl() OK!");
      igInitOK = TRUE;
    }else{ /* end of if */
      dbg(TRACE,"HandleQueues: Init_exchdl() failed!");
      igInitOK = FALSE;
    } /* end of if */
    }/* end of if */
  /* OpenConnection(); */
} /* end of HandleQueues */


/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleInternalData()
{
  int  ilRC = RC_SUCCESS;      /* Return code */
  char *pclSelection = NULL;
  char *pclFields = NULL;
  char *pclData = NULL;
  char *pclLen = NULL;
  char pclNewData[512000] = "\0";
  char pclOldData[512000] = "\0";
  char pclUrno[50];
  char *pclTmpPtr=NULL;
  char pclTmpLine[1024] = "\0";
  BC_HEAD *bchd = NULL;          /* Broadcast header*/
  CMDBLK  *cmdblk = NULL;
  char pclTmpInitFog[32];
  char pclRegn[32];
  char pclStartTime[32];
  char pclTmpBuf[1024];
  int ilLen = 0;
  int ilToMod = 0;
  int ilToPrio = 0;
  char pclDefaultParam[32];
  char pclDefaultFile[256];
  int ilNoEle;
  char pclParam1[256];
  char pclParam2[256];
  char pclParam3[256];
  int ilI;
  int ilFound;
  char pclSection[256];
  char *pclFunc = "HandleInternalData";

  bchd  = (BC_HEAD *) ((char *)prgEvent + sizeof(EVENT));
  cmdblk= (CMDBLK  *) ((char *)bchd->data);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TWS",CFG_STRING,pcgTwStart);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgTwStart,cmdblk->tw_start);
  strcpy(pcgTwEnd,cmdblk->tw_end);

  /***********************************/
  /*    DebugPrintItem(DEBUG,prgItem);   */
  /*    DebugPrintEvent(DEBUG,prgEvent); */
  /***********************************/
  memset(pclOldData,0x00,L_BUFF);
  pclSelection = cmdblk->data;
  pclFields = (char *)pclSelection + strlen(pclSelection) + 1;
  pclData = (char *)pclFields + strlen(pclFields) + 1;

  strcpy(pclNewData,pclData);
  *pclUrno = '\0';
  pclTmpPtr = strstr(pclSelection,"\n");
  if (pclTmpPtr != NULL)
  {
     *pclTmpPtr = '\0';
     pclTmpPtr++;
     strcpy(pclUrno,pclTmpPtr);
  }

  igQueOut = prgEvent->originator;

  ilRC = TimeToStr(pcgCurrentTime,time(NULL));

  strcpy(pcgCurCmd,cmdblk->command);
  /* Reset StatusMessage Buffer */
  SetStatusMessage("?\n");

	if(strcmp(cmdblk->command,"RES") == 0)
	{
		if(strcmp(pcgDXBHoneywell,"YES") == 0)
		{
			igMsgIdHoneyWell = 1;
			dbg(DEBUG,"%s RES command is received,igMsgIdHoneyWell has been reset to one",pclFunc);
		}
	}
  else if (strcmp(cmdblk->command,"XMLI") == 0 && strcmp(pcgMode,"INPUT") == 0)
  {
/* MEI*/
/*strcpy( pclNewData, "<EfpsUfisMessage><ACID>UAE123</ACID><AIRB>2011-04-15T18:23:09</AIRB><AircraftType>A380</AircraftType><CircuitTraining>1</CircuitTraining><CJS>ABC</CJS><ClearanceIssued>2011-04-15T18:23:09</ClearanceIssued><ClearanceRequest>2011-04-15T18:23:09</ClearanceRequest><DepartureAirport>OMDB</DepartureAirport><DestinationAirport>KMIA</DestinationAirport><ETOA>2011-04-15T18:23:09</ETOA><ETOD>2011-04-15T18:23:09</ETOD><FlightPlannedAltitude>F390</FlightPlannedAltitude><FrequencyChange>2011-04-15T18:23:09</FrequencyChange><GroundRoute>J3 W X</GroundRoute><Intersection>K2M5</Intersection><LAND>2011-04-15T18:23:09</LAND><LandingClearanceIssued>2011-04-15T18:23:09</LandingClearanceIssued><MissedApproachTime>2011-04-15T18:23:09</MissedApproachTime><PSTA>C1</PSTA><PSTD>C2</PSTD><PushbackIssued>2011-04-15T18:23:09</PushbackIssued><PushbackRequest>2011-04-15T18:23:09</PushbackRequest><ReturnTaxiToParking>1</ReturnTaxiToParking><ReturnToAerodrome>1</ReturnToAerodrome><Route>OMDB DIR KMIA</Route><SID>GIDIS 2D</SID><STOA>2011-04-15T18:23:09</STOA><STOD>2011-04-15T18:23:09</STOD><TakeOffClearanceIssued>2011-04-15T18:23:09</TakeOffClearanceIssued><TaxiClearanceIssued>2011-04-15T18:23:09</TaxiClearanceIssued><TaxiClearanceRequest>2011-04-15T18:23:09</TaxiClearanceRequest><TrainingFlight>1</TrainingFlight><TransponderCode>3764</TransponderCode><TSAT>2011-04-15T18:23:09</TSAT><URNO>1621791182</URNO><Wake>H</Wake></EfpsUfisMessage>");*/
     dbg(TRACE,"RECEIVED XML MESSAGE FROM %d",prgEvent->originator);
     dbg(DEBUG,"Command:   <%s>",cmdblk->command);
     dbg(DEBUG,"Selection: <%s><%s>strlen(Selection)<%d>",pclSelection,pclUrno,strlen(pclSelection));
     dbg(DEBUG,"Fields:    <%s>",pclFields);
     dbg(DEBUG,"Data:      \n<%-.5120s>",pclNewData);
     dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
     dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);

    //Frank 20130116
    TrimRight(pclSelection);
    if( !strcmp(pcgDXBHoneywell,"YES"))
    {
        if( strlen(pclSelection) >= 1 && strncmp(pclSelection," ",1 )!= 0)
            CheckMsgId(pclNewData,pclSelection);
        else
            dbg(TRACE,"Selection is null,skip id check");
    }

    //Frank v1.52 20121224
    if(strcmp(pcgRemoveSeparator,"YES") == 0)
    {
        RemoveEndSeparator(pclNewData);
    }
    //Frank v1.52 20121224

    if(strcmp(pcgAUH_Hermes,"YES") == 0 && strcmp(pcgCustomization,"AUH") == 0 && strcmp(pcgInstallation
            ,"AUH") == 0)
    {
        removeTag(pclNewData, "flight_Identification");
        dbg(DEBUG,"pclNewData<%s>",pclNewData);
    }

    ilRC = CheckXmlInput(pclNewData);
    if (ilRC == RC_SUCCESS)
    {
        strcpy(pcgMultipleSection,"");
        strcpy(pcgMultipleSectionTriggered,"");
        strcpy(pcgMultipleSectionSetTrigger,"");
        igMultipleSectionFound = FALSE;
        ilRC = HandleInput(pclNewData,-1,-1);
        while (igMultipleSectionFound == TRUE)
        {
           ilRC = RemoveSection(pclNewData);
           strcpy(pcgMultipleSection,"");
           strcpy(pcgMultipleSectionSetTrigger,"");
           igMultipleSectionFound = FALSE;
           ilRC = HandleInput(pclNewData,-1,-1);
        }
    }
  }
  else if (strcmp(pcgMode,"OUTPUT") == 0)
  {
     pclTmpPtr = strstr(pclNewData,"\n");
     if (pclTmpPtr != NULL)
     {
        *pclTmpPtr = '\0';
        pclTmpPtr++;
        strcpy(pclOldData,pclTmpPtr);
     }
     dbg(TRACE,"Originator = <%d>",prgEvent->originator);
     dbg(TRACE,"From = <%s>",bchd->dest_name);
     dbg(DEBUG,"Command:   <%s>",cmdblk->command);
     dbg(DEBUG,"Table:     <%s>",cmdblk->obj_name);
     dbg(DEBUG,"Selection: <%s><%s>",pclSelection,pclUrno);
     //dbg(DEBUG,"______________________________________");
     dbg(DEBUG,"Fields:    <%s>",pclFields);
     dbg(DEBUG,"Data:      <%s>",pclNewData);
     //dbg(DEBUG,"______________________________________");
     dbg(DEBUG,"Old Data:  <%s>",pclOldData);
     dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
     dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);

     if( strncmp(pcgBLRCustomization,"YES",3) == 0 && strncmp(bchd->dest_name,"pdehdl",6) == 0
             && strncmp(cmdblk->command,"UXML",4) == 0)
     {
            igChangeTheMessageTypeForBlrPortalDailyBulkData = 1;
     }
     else
     {
            igChangeTheMessageTypeForBlrPortalDailyBulkData = 0;
     }
     dbg(DEBUG,"igChangeTheMessageTypeForBlrPortalDailyBulkData<%d>",igChangeTheMessageTypeForBlrPortalDailyBulkData);

     ilRC = HandleOutput(cmdblk->command,cmdblk->obj_name,pclSelection,pclFields,pclNewData,pclOldData);
  }
  else if (strcmp(cmdblk->command,"XMLF") == 0 && strcmp(pcgMode,"FILE") == 0)
  {
/*
     cput 0 exchdf XMLF y /ceda/scripts/akl/filename GMS_RESOURCE
     Command:   XMLF
     Selection: GMS_RESOURCE
     Fields:    Timefrom,TimeTo
     Data:      Full File Name
*/
     dbg(DEBUG,"Command:   <%s>",cmdblk->command);
     dbg(DEBUG,"Selection: <%s><%s>",pclSelection,pclUrno);
     dbg(DEBUG,"Fields:    <%s>",pclFields);
     dbg(DEBUG,"Data:      <%s>",pclNewData);
     dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
     dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);
     ilRC = HandleFile(cmdblk->command,cmdblk->obj_name,pclSelection,pclFields,pclNewData);
  }
  else if (strcmp(cmdblk->command,"BASW") == 0 && strcmp(pcgMode,"FILE") == 0)
  {
     dbg(DEBUG,"Command:   <%s>",cmdblk->command);
     dbg(DEBUG,"Selection: <%s><%s>",pclSelection,pclUrno);
     dbg(DEBUG,"Fields:    <%s>",pclFields);
     dbg(DEBUG,"Data:      <%s>",pclNewData);
     dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
     dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);
     ilRC = HandleEqiUsageWAW();
  }
  else if (strcmp(cmdblk->command,"FINAL") == 0)
  {
     dbg(TRACE,"RECEIVED FINAL TRIGGER EVENT OF MULTIPLE SECTION");
     dbg(TRACE,"Command:   <%s>",cmdblk->command);
     dbg(TRACE,"Selection: <%s><%s>",pclSelection,pclUrno);
     dbg(TRACE,"Fields:    <%s>",pclFields);
     dbg(TRACE,"Data:      <%s>",pclNewData);
     dbg(TRACE,"TwStart:   <%s>",pcgTwStart);
     dbg(TRACE,"TwEnd:     <%s>",pcgTwEnd);
     get_real_item(pclTmpBuf,pclFields,1);
     ilToMod = atoi(pclTmpBuf);
     get_real_item(pclTmpBuf,pclFields,2);
     ilToPrio = atoi(pclTmpBuf);
     ilRC = SendCedaEvent(ilToMod,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                cmdblk->command,cmdblk->obj_name,pclSelection,pclFields,pclData,"",ilToPrio,NETOUT_NO_ACK);
     dbg(TRACE,"MESSAGE FORWARDED TO <%d> ON PRIO <%d>",ilToMod,ilToPrio);
  }
  else if (strcmp(cmdblk->command,"SYNA") == 0 && strcmp(pcgMode,"FILE") == 0)
  {
     ilRC = HandleAftSync(pclFields,pclNewData);
  }
  else if (strcmp(pcgMode,"FILE") == 0)
  {
/*
     cput 0 exchdf FAFT
     or
     cput 0 exchdf D_DATA_UPD_01
     or
     cput 0 exchdf FALT
*/
     sprintf(pclDefaultParam,"DEFAULT_%s",cmdblk->command);
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL",pclDefaultParam,CFG_STRING,pclDefaultFile);
     if (ilRC == RC_SUCCESS)
     {
        dbg(DEBUG,"Command:   <%s>",cmdblk->command);
        dbg(DEBUG,"Selection: <%s><%s>",pclSelection,pclUrno);
        dbg(DEBUG,"Fields:    <%s>",pclFields);
        dbg(DEBUG,"Data:      <%s>",pclNewData);
        dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
        dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);
        ilNoEle = GetNoOfElements(pclDefaultFile,',');
        if (ilNoEle != 3)
           dbg(TRACE,"Invalid No. of Parameters received %d,<%s>.",ilNoEle,pclDefaultFile);
        else
        {
           get_real_item(pclParam1,pclDefaultFile,1);
           get_real_item(pclParam2,pclDefaultFile,2);
           get_real_item(pclParam3,pclDefaultFile,3);
           ilRC = HandleFile("XMLF","",pclParam3,pclParam1,pclParam2);
        }
     }
     else
     {
        ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DAILY_DATA_UPD",CFG_STRING,pclTmpBuf);
        if (ilRC == RC_SUCCESS)
        {
           ilFound = FALSE;
           ilNoEle = GetNoOfElements(pclTmpBuf,',');
           for (ilI = 1; ilI <= ilNoEle && ilFound == FALSE; ilI++)
           {
              get_real_item(pclSection,pclTmpBuf,ilI);
              if (strcmp(pclSection,cmdblk->command) == 0)
              {
                 ilFound = TRUE;
                 sprintf(pclDefaultParam,"DEFAULT_%s",cmdblk->command);
                 ilRC = iGetConfigEntry(pcgConfigFile,pclSection,"DEFAULT_FAFT_UPD",
                                        CFG_STRING,pclDefaultFile);
                 if (ilRC == RC_SUCCESS)
                 {
                    dbg(DEBUG,"Command:   <%s>",cmdblk->command);
                    dbg(DEBUG,"Selection: <%s><%s>",pclSelection,pclUrno);
                    dbg(DEBUG,"Fields:    <%s>",pclFields);
                    dbg(DEBUG,"Data:      <%s>",pclNewData);
                    dbg(DEBUG,"TwStart:   <%s>",pcgTwStart);
                    dbg(DEBUG,"TwEnd:     <%s>",pcgTwEnd);
                    ilNoEle = GetNoOfElements(pclDefaultFile,',');
                    if (ilNoEle != 3)
                       dbg(TRACE,"Invalid No. of Parameters received %d,<%s>.",ilNoEle,pclDefaultFile);
                    else
                    {
                       get_real_item(pclParam1,pclDefaultFile,1);
                       get_real_item(pclParam2,pclDefaultFile,2);
                       get_real_item(pclParam3,pclDefaultFile,3);
                       ilRC = HandleFile("XMLF","",pclParam3,pclParam1,pclParam2);
                    }
                 }
              }
           }
           if (ilFound == FALSE)
              dbg(TRACE,"Invalid Command <%s> received.",cmdblk->command);
        }
        else
           dbg(TRACE,"Invalid Command <%s> received.",cmdblk->command);
     }
  }
  else
     dbg(TRACE,"Invalid Command <%s> received.",cmdblk->command);

  if ((igStatusMsgLen > igStatusMsgIni) && (igModID_IfCtrl > 0))
  {
     if (ilRC != RC_SUCCESS)
     {
        SetStatusMessage("XML MESSAGE COULD NOT BE PROCESSED\n");
     }
     ilRC = SendCedaEvent(igModID_IfCtrl,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          "STAT","TAB","","",pcgStatusMsgBuf,"", 3,NETOUT_NO_ACK);
     dbg(TRACE,"STATUS MESSAGE FORWARDED TO <%d> ON PRIO <%d>",igModID_IfCtrl,3);
     dbg(TRACE,"%s",pcgStatusMsgBuf);
  }

  dbg(TRACE,"========================= START / END =========================");
  return ilRC;
} /* end of HandleInternalData */

static void RemoveEndSeparator(char *pcpNewData)
{
	char *pclFunc = "RemoveEndSeparator";
	dbg(DEBUG,"%s pcpNewData last charater<%c>",pclFunc,pcpNewData[strlen(pcpNewData)-1]);

	dbg(DEBUG,"%s The seperator is %s",pclFunc,pcgSeparator);

	if(pcpNewData[strlen(pcpNewData)-1] == cgSeparator)
	{
	    dbg(TRACE,"%s Remove %c from input message",pclFunc,cgSeparator);
	    pcpNewData[strlen(pcpNewData)-1] = '\0';

	    dbg(DEBUG,"%s Modified Data:      \n<%-.5120s>",pclFunc,pcpNewData);
	}
}

static void CheckMsgId(char *pcpData, char* pcpSelection)
{
	int ilI = 0;
	char pclMessageIdTemp[1024] = "\0";
	char *pclFunc = "CheckingMsgId";

	memset(pclMessageIdTemp,0,sizeof(pclMessageIdTemp));

	strcpy(pclMessageIdTemp,strstr(pcpData,"messageId=\""));

	memset(pcgMsgId,0,sizeof(pcgMsgId));

 	for(ilI = 0; ilI < (strlen(pclMessageIdTemp)-strlen("messageId=\"")); ilI++)
	{
		if( *(pclMessageIdTemp+strlen("messageId=\"")+ilI) != '\"' && *(pclMessageIdTemp+strlen("messageId=\"")+ilI) != ' ')
		{
			strncpy( (pcgMsgId+ilI),(pclMessageIdTemp+strlen("messageId=\"")+ilI),1 );
		}
		else
		{
			break;
		}
	}

	if( strlen(pcgMsgId)>0 && atoi(pcgMsgId)>=1 && atoi(pcgMsgId)<=65535)
	{
		dbg(DEBUG,"%s MessageId: <%d>",pclFunc,atoi(pcgMsgId));

		if( atoi(pcpSelection)>0 && (atoi(pcpSelection)==atoi(pcgMsgId)) )
		{
			dbg(DEBUG,"%s pcgMsgId<%s> equals pclSelection<%s>",pclFunc,pcgMsgId,pcpSelection);

			if( strlen(pcgMsgIdOld)>0 && atoi(pcgMsgIdOld)>=1 && atoi(pcgMsgId)<=65535)
			{
				if( ((atoi(pcgMsgId) - atoi(pcgMsgIdOld)) == 1) || ( (atoi(pcgMsgId)==1) && atoi(pcgMsgIdOld)==65535 ) )
				{
					dbg(TRACE,"%s Message Id is in sequence,current<%s>,last<%s>",pclFunc,pcgMsgId,pcgMsgIdOld);
				}
				else
				{
					dbg(TRACE,"%s Message Id is in not sequence,current<%s>,last<%s>",pclFunc,pcgMsgId,pcgMsgIdOld);
				}
			}
			else
			{
				if(igMsgCount == 0)
				{
					dbg(DEBUG,"%s the first time, no comparation",pclFunc);
				}
			 	else
			 	{
					dbg(TRACE,"%s Last message id<%s> is invalid,return RC_SUCCESS",pclFunc,pcgMsgIdOld);
					igMsgCount++;
				}
			}
			igMsgCount++;
			memset(pcgMsgIdOld,0,sizeof(pcgMsgIdOld));
			strcpy(pcgMsgIdOld,pcgMsgId);
		}
		else
		{
			dbg(TRACE,"%s pclSelection<%s> is invalid",pclFunc,pcpSelection);
		}
	}
 	else
 	{
 		dbg(TRACE,"%s pcgMsgId<%s> is invalid which shoud be 65535>= msgid >= 1",pclFunc,pcgMsgId);
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
      strncpy(prlOutBCHead->recv_name, "EXCO",10);

      /* Cmdblk-Structure... */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,pcpCmd);
      if (pcpTable != NULL)
    {
      strcpy(prlOutCmdblk->obj_name,pcpTable);
      strcat(prlOutCmdblk->obj_name,pcgTabEnd);
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
}

/*
    Get Config Entries
*/
static int GetConfig()
{
  int ilRC = RC_SUCCESS;
  int ilI;
  int ilJ;
  char pclTmpBuf[128];
  char pclDebugLevel[128];
  int ilLen;
  char pclBuffer[10];
  char pclClieBuffer[100];
  char pclServBuffer[100];
  char *pclFunc = "GetConfig";

  sprintf(pcgConfigFile,"%s/%s.cfg",getenv("CFG_PATH"),mod_name);
  dbg(TRACE,"Config File is <%s>",pcgConfigFile);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CDStart",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     strcpy(pcgCDStart,pclTmpBuf);
  else
     strcpy(pcgCDStart,"<![CDATA[");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CDEnd",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     strcpy(pcgCDEnd,pclTmpBuf);
  else
     strcpy(pcgCDEnd,"]]>");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_WINDOW_ARR_BEGIN",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igTimeWindowArrBegin = atoi(pclTmpBuf);
  else
     igTimeWindowArrBegin = 0;
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_WINDOW_ARR_END",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igTimeWindowArrEnd = atoi(pclTmpBuf);
  else
     igTimeWindowArrEnd = 0;
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_WINDOW_DEP_BEGIN",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igTimeWindowDepBegin = atoi(pclTmpBuf);
  else
     igTimeWindowDepBegin = 0;
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_WINDOW_DEP_END",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igTimeWindowDepEnd = atoi(pclTmpBuf);
  else
     igTimeWindowDepEnd = 0;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MAX_TIME_DIFF_FOR_UPDATE",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igMaxTimeDiffForUpdate = atoi(pclTmpBuf);
  else
     igMaxTimeDiffForUpdate = 0;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","VDGS_TIME_DIFFERENCE",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igVdgsTimeDiff = atoi(pclTmpBuf);
  else
     igVdgsTimeDiff = 10;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","ACTUAL_PERIOD",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igActualPeriod = atoi(pclTmpBuf);
  else
     igActualPeriod = 48;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","HANDLE_RETURN_FLIGHTS",CFG_STRING,pcgHandleReturnFlights);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgHandleReturnFlights,"AUTO");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DES_FOR_TOUCH_AND_GO",CFG_STRING,pcgDesForTouchAndGo);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgDesForTouchAndGo,"");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","NBIA_TOWING",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclTmpBuf,"YES") == 0)
        igNBIATowing = TRUE;
     else
        igNBIATowing = FALSE;
  }
  else
     igNBIATowing = TRUE;
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","NBIA_RETURN_FLIGHT",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclTmpBuf,"YES") == 0)
        igNBIAReturnFlight = TRUE;
     else
        igNBIAReturnFlight = FALSE;
  }
  else
     igNBIAReturnFlight = TRUE;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","INSTALLATION",CFG_STRING,pcgInstallation);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgInstallation,"BKK");

    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CUSTOMIZATION",CFG_STRING,pcgCustomization);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgCustomization,"AUH");
    dbg(DEBUG,"pcgCustomization<%s>",pcgCustomization);

        ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","AUH_HERMES",CFG_STRING,pcgAUH_Hermes);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgAUH_Hermes,"NO");
    dbg(DEBUG,"pcgAUH_Hermes<%s>",pcgAUH_Hermes);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MODE",CFG_STRING,pcgMode);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgMode,"INPUT");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SUB_MODE",CFG_STRING,pcgSubMode);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgSubMode,"");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","COMPRESS_TAG",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclTmpBuf,"YES") == 0)
        igCompressTag = TRUE;
     else
        igCompressTag = FALSE;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SIMULATION",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclTmpBuf,"YES") == 0)
        igSimulation = TRUE;
     else
        igSimulation = FALSE;
  }
  else
     igSimulation = FALSE;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","USE_URNO",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclTmpBuf,"FALSE") == 0)
        igUseUrno = FALSE;
     else
        igUseUrno = TRUE;
  }
  else
     igUseUrno = TRUE;

  //igPSAInsertOnly

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PSA_INSERT_ONLY",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strncmp(pclTmpBuf,"YES",3) != 0)
        igPSAInsertOnly = FALSE;
     else
        igPSAInsertOnly = TRUE;
  }
  else
     igPSAInsertOnly = TRUE;
  dbg(DEBUG,"line<%d>igPSAInsertOnly=<%d>",__LINE__,igPSAInsertOnly);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SEND_OUTPUT",CFG_STRING,pcgSendOutput);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgSendOutput,"YES");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","REPEAT_KEY_FIELDS_IN_BODY",CFG_STRING,
                         pcgRepeatKeyFieldsInBody);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgRepeatKeyFieldsInBody,"YES");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SECTIONS_FOR_KEY_FIELDS_CHECK",CFG_STRING,
                         pcgSectionsForKeyFieldsCheck);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgSectionsForKeyFieldsCheck,"");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","VERSION",CFG_STRING,pcgVersion);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgVersion,"");
  else
  {
     for (ilI = 0; ilI < strlen(pcgVersion); ilI++)
        if (pcgVersion[ilI] == '_')
           pcgVersion[ilI] = ' ';
  }

  /* MEI v1.44 */
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEL_ZERO_FROM_CSGN",CFG_STRING,pclTmpBuf);
  igDelZeroFromCsgn = FALSE;
  if (ilRC == RC_SUCCESS && !strcmp(pclTmpBuf,"YES") )
      igDelZeroFromCsgn = TRUE;
  dbg( TRACE, "%s: DEL_ZERO_FROM_CSGN = <%d>", pclFunc, igDelZeroFromCsgn );

  /* MEI v1.45 */
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","USE_VIA_AS_ORG",CFG_STRING,pclTmpBuf);
  igUseViaAsOrg = FALSE;
  if (ilRC == RC_SUCCESS && !strcmp(pclTmpBuf,"YES") )
      igUseViaAsOrg = TRUE;
  dbg( TRACE, "%s: USE_VIA_AS_ORG = <%d>", pclFunc, igUseViaAsOrg );
  /* MEI v1.47 */
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","USE_VIA_AS_DES",CFG_STRING,pclTmpBuf);
  igUseViaAsDes = FALSE;
  if (ilRC == RC_SUCCESS && !strcmp(pclTmpBuf,"YES") )
      igUseViaAsDes = TRUE;
  dbg( TRACE, "%s: USE_VIA_AS_DES = <%d>", pclFunc, igUseViaAsDes );
  /* CST v1.49 */
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DXB_EFPS_DESTINATION",CFG_STRING,pclTmpBuf);
  igEFPSDestination = 7800;
  if (ilRC == RC_SUCCESS)
      igEFPSDestination = atoi(pclTmpBuf);
  dbg( TRACE, "%s: DXB_EFPS_DESTINATION = <%d>", pclFunc, igEFPSDestination );

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DXB_COB_HANDLING",CFG_STRING,pclTmpBuf);
  igCOBHandling = FALSE;
  if (ilRC == RC_SUCCESS && !strcmp(pclTmpBuf,"YES") )
      igCOBHandling = TRUE;
  dbg( TRACE, "%s: COB HANDLING = <%d>", pclFunc, igCOBHandling );
  /************/

  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","MESSAGE_ID",CFG_STRING,pcgMessageId);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgMessageId,"MESSAGEID");
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","MESSAGE_TYPE",CFG_STRING,pcgMessageType);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgMessageType,"MESSAGETYPE");
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","MESSAGE_ORIGIN",CFG_STRING,pcgMessageOrigin);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgMessageOrigin,"MESSAGEORIGIN");
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","TIME_ID",CFG_STRING,pcgTimeId);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgTimeId,"TIMEID");
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","TIME_STAMP",CFG_STRING,pclTmpBuf);
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgTimeStamp,"TIMESTAMP");
     strcpy(pcgTimeStampDate,"");
     strcpy(pcgTimeStampTime,"");
  }
  else
  {
     if (strstr(pclTmpBuf,",") == NULL)
     {
        strcpy(pcgTimeStamp,pclTmpBuf);
        strcpy(pcgTimeStampDate,"");
        strcpy(pcgTimeStampTime,"");
     }
     else
     {
        strcpy(pcgTimeStamp,"");
        get_real_item(pcgTimeStampDate,pclTmpBuf,1);
        get_real_item(pcgTimeStampTime,pclTmpBuf,2);
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","ACTION_TYPE",CFG_STRING,pclTmpBuf);
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgActionType,"ACTIONTYPE");
     strcpy(pcgActionTypeIns,"I");
     strcpy(pcgActionTypeUpd,"U");
     strcpy(pcgActionTypeDel,"D");
  }
  else
  {
     get_real_item(pcgActionType,pclTmpBuf,1);
     get_real_item(pcgActionTypeIns,pclTmpBuf,2);
     get_real_item(pcgActionTypeUpd,pclTmpBuf,3);
     get_real_item(pcgActionTypeDel,pclTmpBuf,4);
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MODID_INTERFACE_CONTROL",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     igModID_IfCtrl = atoi(pclTmpBuf);
  else
     igModID_IfCtrl = 0;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","ARRIVAL_SECTIONS",CFG_STRING,pcgArrivalSections);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgArrivalSections,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEPARTURE_SECTIONS",CFG_STRING,pcgDepartureSections);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgDepartureSections,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING_SECTIONS",CFG_STRING,pcgTowingSections);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgTowingSections,"");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEBUG_LEVEL",CFG_STRING,pclDebugLevel);
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

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","AFTTAB_FIELD",CFG_STRING, pcgAfttabFields);
  if (ilRC != RC_SUCCESS)
  {
           strcpy(pcgAfttabFields,"");
  }
  dbg( TRACE, "%s: AFTTAB_FIELD = <%s>", pclFunc, pcgAfttabFields );

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","HERMES_FIELDS",CFG_STRING, pcgHermesFields);
  if (ilRC != RC_SUCCESS)
  {
           strcpy(pcgHermesFields,"");
  }
  dbg( TRACE, "%s: HERMES_FIELDS = <%s>", pclFunc, pcgHermesFields );

  //FYA v1.45 UFIS-1901
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PROCEED_WITHOUT_CCATAB",CFG_STRING,pcgProceedWithoutCCATAB);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgProceedWithoutCCATAB,"");
  //FYA v1.45 UFIS-1901
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BKK_CUSTOMIZATION",CFG_STRING,pcgBKKCustomization);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgBKKCustomization,"");

  //FYA v1.52 20121224
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","REMOVE_SEPERATOR",CFG_STRING,pcgRemoveSeparator);
  if (ilRC != RC_SUCCESS)
  {
  	   strcpy(pcgRemoveSeparator,"");
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SEPERATOR",CFG_STRING,pcgSeparator);
  if (ilRC != RC_SUCCESS)
  {
  	   strcpy(pcgSeparator,"");
  	   cgSeparator = '0';
  }
  else
  {
  	if( !strncmp(pcgSeparator,"0x04",4) )
  	{
  		cgSeparator = 0x04;
  	}
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GET_URNO_HEAD",CFG_STRING,pclTmpBuf);
  if (ilRC != RC_SUCCESS)
  {
  	   strcpy(pcgGetUrnoHead,"");
  }
  else
  {
  	memset(pcgGetUrnoHead,0,sizeof(pcgGetUrnoHead));
  	for(ilI=0;ilI<strlen(pclTmpBuf);ilI++)
  	{
  		if(pclTmpBuf[ilI] == '_')
  		{
  			pcgGetUrnoHead[ilI] = ' ';
  		}
  		else
  		{
  			pcgGetUrnoHead[ilI] = pclTmpBuf[ilI];
  		}
  	}

  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GET_URNO_TAIL",CFG_STRING,pcgGetUrnoTail);
  if (ilRC != RC_SUCCESS)
  {
  	   strcpy(pcgGetUrnoTail,"");
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DXB_HONEYWELL",CFG_STRING,pcgDXBHoneywell);
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgDXBHoneywell,"");
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DXB_CUSTOMIZATION",CFG_STRING,pcgDXBCustomization);
  if (ilRC != RC_SUCCESS)
     strcpy(pcgDXBCustomization,"");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TABLENAME",CFG_STRING,pcgTableName);
  if (ilRC != RC_SUCCESS)
  {
  	strcpy(pcgTableName,"");
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MODIFY_SGDT_TIME_FORMAT",CFG_STRING,pcgModifySGDTTimeFormat);
  if (ilRC != RC_SUCCESS)
  {
  	strcpy(pcgModifySGDTTimeFormat,"");
  }
  //FYA v1.52 20121224

  //FYA v1.53 20130103
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","IGNORE_KEY_SECTION",CFG_STRING,pcgIgnoreKeySection);
  if (ilRC != RC_SUCCESS)
  {
  	strcpy(pcgIgnoreKeySection,"");
  }
   //FYA v1.53 20130103

  dbg(DEBUG,"pcgBLRCustomization<%s>",pcgBLRCustomization);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CHANGE_ADID",CFG_STRING,pcgChgAdid);
  if (ilRC != RC_SUCCESS)
  {
  	strcpy(pcgChgAdid,"");
  }

  //FYA v1.52 20121224

  dbg(DEBUG,"pcgBLRCustomization<%s>",pcgBLRCustomization);


  dbg(TRACE,"CDStart = %s",pcgCDStart);
  dbg(TRACE,"CDEnd   = %s",pcgCDEnd);
  if (strcmp(pcgMode,"INPUT") == 0)
  {
     dbg(TRACE,"TIME_WINDOW_ARR_BEGIN = %d",igTimeWindowArrBegin);
     dbg(TRACE,"TIME_WINDOW_ARR_END   = %d",igTimeWindowArrEnd);
     dbg(TRACE,"TIME_WINDOW_DEP_BEGIN = %d",igTimeWindowDepBegin);
     dbg(TRACE,"TIME_WINDOW_DEP_END   = %d",igTimeWindowDepEnd);
     dbg(TRACE,"MAX_TIME_DIFF_FOR_UPDATE = %d",igMaxTimeDiffForUpdate);
     if (igUseUrno == TRUE)
        dbg(TRACE,"USE_URNO = TRUE");
     else
        dbg(TRACE,"USE_URNO = FALSE");
  }
  dbg(TRACE,"MODE = %s",pcgMode);
  if (strcmp(pcgMode,"INPUT") == 0)
  {
     dbg(TRACE,"SUB_MODE = %s",pcgSubMode);
     dbg(TRACE,"ARRIVAL_SECTIONS = %s",pcgArrivalSections);
     dbg(TRACE,"DEPARTURE_SECTIONS = %s",pcgDepartureSections);
     dbg(TRACE,"TOWING_SECTIONS = %s",pcgTowingSections);
  }
  if (strcmp(pcgMode,"OUTPUT") == 0)
  {
     dbg(TRACE,"SEND_OUTPUT = %s",pcgSendOutput);
     dbg(TRACE,"COMPRESS_TAG = %d",igCompressTag);
     dbg(TRACE,"REPEAT_KEY_FIELDS_IN_BODY = %s",pcgRepeatKeyFieldsInBody);
     dbg(TRACE,"SECTIONS_FOR_KEY_FIELDS_CHECK = %s",pcgSectionsForKeyFieldsCheck);
     dbg(TRACE,"VERSION = %s",pcgVersion);
     dbg(TRACE,"VDGS_TIME_DIFFERENCE = %d",igVdgsTimeDiff);
     dbg(TRACE,"ACTUAL_PERIOD = %d hours in future",igActualPeriod);
     dbg(TRACE,"HANDLE_RETURN_FLIGHTS = %s",pcgHandleReturnFlights);
     dbg(TRACE,"DES_FOR_TOUCH_AND_GO = %s",pcgDesForTouchAndGo);
     dbg(TRACE,"NBIA_TOWING = %d",igNBIATowing);
     dbg(TRACE,"NBIA_RETURN_FLIGHT = %d",igNBIAReturnFlight);
     dbg(TRACE,"INSTALLATION = %s",pcgInstallation);
  }
  dbg(TRACE,"MESSAGE_ID     = %s",pcgMessageId);
  dbg(TRACE,"MESSAGE_TYPE   = %s",pcgMessageType);
  dbg(TRACE,"MESSAGE_ORIGIN = %s",pcgMessageOrigin);
  dbg(TRACE,"TIME_ID        = %s",pcgTimeId);
  dbg(TRACE,"TIME_STAMP     = %s,%s,%s",pcgTimeStamp,pcgTimeStampDate,pcgTimeStampTime);
  dbg(TRACE,"ACTION_TYPE    = %s,%s,%s,%s",pcgActionType,pcgActionTypeIns,pcgActionTypeUpd,pcgActionTypeDel);
  dbg(TRACE,"MODID_INTERFACE_CONTROL = %d",igModID_IfCtrl);
  dbg(TRACE,"DEBUG_LEVEL = %s",pclDebugLevel);
  //FYA v1.45 UFIS-1901
  dbg(TRACE,"PROCEED_WITHOUT_CCATAB = %s",pcgProceedWithoutCCATAB);
  dbg(TRACE,"BKK_CUSTOMIZATION = %s",pcgBKKCustomization);

  //Frank v1.52 20121224
  dbg(TRACE,"%s REMOVE_SEPERATOR = %s",pclFunc,pcgRemoveSeparator);
  dbg(TRACE,"%s SEPERATOR(string) = %s",pclFunc,pcgSeparator);
  dbg(TRACE,"%s SEPERATOR(char) = %c",pclFunc,cgSeparator);
  dbg(TRACE,"%s GET_URNO_HEAD = [%s]",pclFunc,pcgGetUrnoHead);
  dbg(TRACE,"%s GET_URNO_TAIL = [%s]",pclFunc,pcgGetUrnoTail);
  dbg(TRACE,"%s DXB_HONEYWELL = %s",pclFunc,pcgDXBHoneywell);
  dbg(TRACE,"%s TABLENAME = %s",pclFunc,pcgTableName);

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
           }
           pcgServerChars[(ilJ-1)*2+1] = 0x00;
           for (ilJ=0; ilJ < ilI; ilJ++)
           {
              GetDataItem(pclBuffer,pclClieBuffer,ilJ+1,',',"","\0\0");
              pcgClientChars[ilJ*2] = atoi(pclBuffer);
              pcgClientChars[ilJ*2+1] = ',';
           }
           pcgClientChars[(ilJ-1)*2+1] = 0x00;
           dbg(DEBUG,"New Clientchars <%s> dec <%s>",pcgClientChars,pclClieBuffer);
           dbg(DEBUG,"New Serverchars <%s> dec <%s>",pcgServerChars,pclServBuffer);
           dbg(DEBUG,"Serverchars <%d> ",strlen(pcgServerChars));
           dbg(DEBUG,"%s  and count <%d> ",pclBuffer,strlen(pcgServerChars));
           dbg(DEBUG,"ilI <%d>",ilI);
        }
     }
     else
     {
        ilRC = RC_SUCCESS;
        dbg(DEBUG,"Use standard (old) serverchars");
     }
  }
  else
  {
     dbg(DEBUG,"Use standard (old) serverchars");
     ilRC = RC_SUCCESS;
  }
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgClientChars,"\042\047\054\012\015");
     strcpy(pcgServerChars,"\260\261\262\263\263");
     ilRC = RC_SUCCESS;
  }

  ilRC = GetXMLSchema();

  return RC_SUCCESS;
} /* Enf of GetConfig */

/******************************************************************************/
/* The TimeToStr routine                                                      */
/******************************************************************************/
static int TimeToStr(char *pcpTime,time_t lpTime)
{
  struct tm *_tm;

  _tm = (struct tm *)gmtime(&lpTime);

  sprintf(pcpTime,"%4d%02d%02d%02d%02d%02d",
          _tm->tm_year+1900,_tm->tm_mon+1,_tm->tm_mday,_tm->tm_hour,
          _tm->tm_min,_tm->tm_sec);

  return RC_SUCCESS;

}     /* end of TimeToStr */


static int TriggerBchdlAction(char *pcpCmd, char *pcpTable, char *pcpSelection,
                              char *pcpFields, char *pcpData, int ipBchdl, int ipAction)
{
  int ilRC = RC_SUCCESS;

  if (ipBchdl == TRUE)
  {
     (void) tools_send_info_flag(1900,0,"exchdl","","CEDA","","",pcgTwStart,pcgTwEnd,
                                 pcpCmd,pcpTable,pcpSelection,pcpFields,pcpData,0);
     dbg(DEBUG,"Send Broadcast: <%s><%s><%s><%s><%s>",
         pcpCmd,pcpTable,pcpSelection,pcpFields,pcpData);
  }
  if (ipAction == TRUE)
  {
     (void) tools_send_info_flag(7400,0,"exchdl","","CEDA","","",pcgTwStart,pcgTwEnd,
                                 pcpCmd,pcpTable,pcpSelection,pcpFields,pcpData,0);
     dbg(DEBUG,"Send to ACTION: <%s><%s><%s><%s><%s>",
         pcpCmd,pcpTable,pcpSelection,pcpFields,pcpData);
  }

  return ilRC;
} /* End of TriggerBchdlAction */


/*******************************************++++++++++*************************/
/*   GetSecondsFromCEDATime                                                   */
/*                                                                            */
/*   Serviceroutine, rechnet einen Datumseintrag im Cedaformat in Seconds um  */
/*   Input:  char *  Zeiger auf CEDA-Zeitpuffer                               */
/*   Output: long    Zeitwert                                                 */
/******************************************************************************/

static long GetSecondsFromCEDATime(char *pcpDateTime)
{
  long rc = 0;
  int year;
  char ch_help[5];
  struct tm *tstr1 = NULL, t1;
  struct tm *CurTime, t2;
  time_t llTime;
  time_t llUtcTime;
  time_t llLocalTime;

  CurTime = &t2;
  llTime = time(NULL);
  CurTime = (struct tm *)gmtime(&llTime);
  CurTime->tm_isdst = 0;
  llUtcTime = mktime(CurTime);
  CurTime = (struct tm *)localtime(&llTime);
  CurTime->tm_isdst = 0;
  llLocalTime = mktime(CurTime);
  igDiffUtcToLocal = llLocalTime - llUtcTime;
  /* dbg(TRACE,"UTC: <%ld> , Local: <%ld> , Diff: <%ld>",llUtcTime,llLocalTime,igDiffUtcToLocal); */

  memset(&t1,0x00,sizeof(struct tm));

  tstr1 = &t1;

  memset(ch_help,0x00,5);
  strncpy(ch_help,pcpDateTime,4);
  year = atoi(ch_help);
  tstr1->tm_year = year - 1900;

  memset(ch_help,0x00,5);
  strncpy(ch_help,&pcpDateTime[4],2);
  tstr1->tm_mon = atoi(ch_help) -1;

  memset(ch_help,0x00,5);
  strncpy(ch_help,&pcpDateTime[6],2);
  tstr1->tm_mday = atoi(ch_help);

  memset(ch_help,0x00,5);
  strncpy(ch_help,&pcpDateTime[8],2);
  tstr1->tm_hour = atoi(ch_help);

  memset(ch_help,0x00,5);
  strncpy(ch_help,&pcpDateTime[10],2);
  tstr1->tm_min = atoi(ch_help);

  memset(ch_help,0x00,5);
  strncpy(ch_help,&pcpDateTime[12],2);
  tstr1->tm_sec = atoi(ch_help);

  tstr1->tm_wday = 0;
  tstr1->tm_yday = 0;
  tstr1->tm_isdst = 0;
  rc = mktime(tstr1);

  rc = rc + igDiffUtcToLocal; /* add difference between local and utc */

  return(rc);
} /* end of GetSecondsFromCEDATime() */


static void TrimRight(char *pcpBuffer)
{
  char *pclBlank = &pcpBuffer[strlen(pcpBuffer)-1];

  if (strlen(pcpBuffer) == 0)
  {
     strcpy(pcpBuffer, " ");
  }
  else
  {
     while (isspace(*pclBlank) && pclBlank != pcpBuffer)
     {
        *pclBlank = '\0';
        pclBlank--;
     }
  }
} /* End of TrimRight */


static void TrimLeftRight(char *pcpBuffer)
{
  char *pclBlank = &pcpBuffer[0];

  if (strlen(pcpBuffer) == 0)
  {
     strcpy(pcpBuffer, " ");
  }
  else
  {
     while (isspace(*pclBlank) && *pclBlank != '\0')
     {
        pclBlank++;
     }
     strcpy(pcpBuffer,pclBlank);
     TrimRight(pcpBuffer);
  }
} /* End of TrimLeftRight */


static void CheckCDATA(char *pcpBuffer)
{
  char pclTmpBuf[512000];
  char *pclTmpPtr1;
  char *pclTmpPtr2;

  memset(pclTmpBuf,0x00,512000);
  pclTmpPtr1 = strstr(pcpBuffer,pcgCDStart);
  if (pclTmpPtr1 != NULL)
  {
     if (pclTmpPtr1 != pcpBuffer)
     {
        strncpy(pclTmpBuf,pcpBuffer,pclTmpPtr1 - pcpBuffer);
     }
     pclTmpPtr1 += strlen(pcgCDStart);
     pclTmpPtr2 = strstr(pclTmpPtr1,pcgCDEnd);
     if (pclTmpPtr2 == NULL)
        strcat(pclTmpBuf,pclTmpPtr1);
     else
     {
        strncat(pclTmpBuf,pclTmpPtr1,pclTmpPtr2 - pclTmpPtr1);
        pclTmpPtr2 += strlen(pcgCDEnd);
        if (*pclTmpPtr2 != '\0')
           strcat(pclTmpBuf,pclTmpPtr2);
     }
  strcpy(pcpBuffer,pclTmpBuf);
  }
} /* End of CheckCDATA */


static int MyAddSecondsToCEDATime(char *pcpTime, long lpValue, int ipP)
{
  int ilRC = RC_SUCCESS;

  if (strlen(pcpTime) == 12)
     strcat(pcpTime,"00");
  ilRC = AddSecondsToCEDATime(pcpTime,lpValue,ipP);
  return ilRC;
} /* End of MyAddSecondsToCEDATime */


static char *GetLine(char *pcpLine, int ipLineNo)
{
  char *pclResult = NULL;
  char *pclP = pcpLine;
  int ilCurLine;

  ilCurLine = 1;
  while (*pclP != '\0' && ilCurLine < ipLineNo)
  {
     if (*pclP == '\n')
        ilCurLine++;
     pclP++;
  }
  if (*pclP != '\0')
     pclResult = pclP;

  return pclResult;
} /* End of GetLine */


static void CopyLine(char *pcpDest, char *pcpSource)
{
  char *pclP1 = pcpDest;
  char *pclP2 = pcpSource;

  while (*pclP2 != '\n' && *pclP2 != '\0')
  {
     *pclP1 = *pclP2;
     pclP1++;
     pclP2++;
  }
  *pclP1 = '\0';
  TrimRight(pcpDest);
} /* End of CopyLine */


static int GetXMLSchema()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetXMLSchema:";
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

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","XML_SCHEMA_FILE",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     sprintf(pclCfgFile,"%s/%s",getenv("CFG_PATH"),pclTmpBuf);
  else
     sprintf(pclCfgFile,"%s/%s.csv",getenv("CFG_PATH"),mod_name);
  dbg(TRACE,"%s XML Config File is <%s>",pclFunc,pclCfgFile);
  if ((fp = (FILE *)fopen(pclCfgFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s XML Config File <%s> does not exist",pclFunc,pclCfgFile);
     return RC_FAIL;
  }

  igCurInterface = 0;
  igUsingMultiTypeFlag = FALSE;
  memset((char *)&rgXml.rlLine[0].ilLevel,0x00,sizeof(_LINE)*MAX_XML_LINES);
  strcpy(pclPrevTag,"");
  ilInfoFldFlag = FALSE;
  igCurXmlLines = 0;
  ilCurLevel = 0;
  ilCurLine = 1;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (pclLine[strlen(pclLine)-1] == '\012' || pclLine[strlen(pclLine)-1] == '\015')
        pclLine[strlen(pclLine)-1] = '\0' ;
     ilNoEle = GetNoOfElements(pclLine,';');
     dbg(DEBUG,"%s Current Line = (%d)<%s>",pclFunc,ilNoEle,pclLine);
     get_item(ilNoEle,pclLine,pclTag,0,";","\0","\0");
     get_item(1,pclLine,pclTag,0,";","\0","\0");
     TrimRight(pclTag);
     get_item(2,pclLine,pclName,0,";","\0","\0");
     TrimRight(pclName);

     //Frank v1.52 20121227
     //dbg(DEBUG,"%s %d pclName<%s>",pclFunc,__LINE__,pclName);

     get_item(3,pclLine,pclType,0,";","\0","\0");
     TrimRight(pclType);
     get_item(4,pclLine,pclBasTab,0,";","\0","\0");
     TrimRight(pclBasTab);
     get_item(5,pclLine,pclBasFld,0,";","\0","\0");
     TrimRight(pclBasFld);
     get_item(6,pclLine,pclFlag,0,";","\0","\0");
     TrimRight(pclFlag);
     if (strcmp(pclTag,"TITLE") != 0)
     {
        if (strcmp(pclTag,"STR_BGN") == 0)
        {
           if (strcmp(pclPrevTag,"STR_END") != 0)
              ilCurLevel++;
           if (ilInfoFldFlag == TRUE)
              ilCurLevel--;
           ilInfoFldFlag = FALSE;
           strcpy(&pclNameStack[ilCurLevel-1][0],pclName);
        }
        else
        {
           if (strcmp(pclTag,"STR_END") == 0)
           {
              ilCurLevel--;
              ilInfoFldFlag = FALSE;
              if (strcmp(pclName,&pclNameStack[ilCurLevel-1][0]) != 0)
              {
                 dbg(TRACE,"%s Wrong Name at STR_END <%s><%s> specified!!! Line = %d",
                     pclFunc,pclName,&pclNameStack[ilCurLevel-1][0],ilCurLine);
                 return ilRC;
              }
           }
           else
           {
              if (strcmp(pclTag,"HDR") == 0)
              {
                 if (ilInfoFldFlag == FALSE)
                 {
                    ilCurLevel++;
                    ilInfoFldFlag = TRUE;
                 }
              }
              else
              {
                 if (strcmp(pclTag,"KEY") == 0)
                 {
                    if (strcmp(pclPrevTag,"STR_END") == 0)
                       ilCurLevel--;
                    if (ilInfoFldFlag == FALSE)
                    {
                       ilCurLevel++;
                       ilInfoFldFlag = TRUE;
                    }
                 }
                 else
                 {
                    if (strcmp(pclTag,"DAT") == 0)
                    {
                       if (strcmp(pclPrevTag,"STR_END") == 0)
                          ilCurLevel--;
                       if (ilInfoFldFlag == FALSE)
                       {
                          ilCurLevel++;
                          ilInfoFldFlag = TRUE;
                       }
                    }
                    else
                    {
                       dbg(TRACE,"%s Wrong Tag <%s> specified!!!",pclFunc,pclTag);
                       return ilRC;
                    }
                 }
              }
           }
        }
        rgXml.rlLine[igCurXmlLines].ilLevel = ilCurLevel;
        strcpy(&rgXml.rlLine[igCurXmlLines].pclTag[0],pclTag);

        strcpy(&rgXml.rlLine[igCurXmlLines].pclName[0],pclName);
        //Frank v1.52 20121227
        //dbg(DEBUG,"%s %d pclName<%s>&rgXml.rlLine[igCurXmlLines].pclName[0]<%s>",pclFunc,__LINE__,pclName,&rgXml.rlLine[igCurXmlLines].pclName[0]);

        strcpy(&rgXml.rlLine[igCurXmlLines].pclType[0],pclType);
        if (strcmp(pclType,"MULTI") == 0)
        {
           igUsingMultiTypeFlag = TRUE;
        }
        if (strcmp(pcgMode,"OUTPUT") == 0 && strcmp(pclTag,"STR_BGN") != 0 && strcmp(pclTag,"STR_END") != 0)
        {
           if (*pclBasTab == ' ')
              strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0],pclName);
           else
              strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0],pclBasTab);
           if (*pclBasFld == ' ')
              strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0],pclName);
           else
              strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0],pclBasFld);
        }
        else
        {
           strcpy(&rgXml.rlLine[igCurXmlLines].pclBasTab[0],pclBasTab);
           strcpy(&rgXml.rlLine[igCurXmlLines].pclBasFld[0],pclBasFld);
        }
        strcpy(&rgXml.rlLine[igCurXmlLines].pclFlag[0],pclFlag);
        ilNoEle = GetNoOfElements(pclLine,';');
        for (ilI = 7; ilI <= ilNoEle; ilI += 3)
        {
           get_item(ilI,pclLine,pclItem1,0,";","\0","\0");
           TrimRight(pclItem1);
           get_item(ilI+1,pclLine,pclItem2,0,";","\0","\0");
           TrimRight(pclItem2);
           get_item(ilI+2,pclLine,pclItem3,0,";","\0","\0");
           TrimRight(pclItem3);
           if (strcmp(pclItem1,"{END}") != 0)
           {
              if (*pclItem1 != ' ')
              {
                 if (strcmp(&rgXml.rlLine[igCurXmlLines].pclTag[0],"HDR") == 0 &&
                     strcmp(&rgXml.rlLine[igCurXmlLines].pclName[0],pcgMessageOrigin) == 0)
                 {
                    strcpy(&rgInterface[igCurInterface].pclIntfName[0],pclItem1);
                    rgInterface[igCurInterface].ilIntfIdx = (ilI - 7) / 3;
                    igCurInterface++;
                 }
                 else
                 {
                    strcpy(&rgXml.rlLine[igCurXmlLines].pclMethod[(ilI-7)/3][0],pclItem1);
                    rgXml.rlLine[igCurXmlLines].ilNoMethod++;
                 }
              }
              if (*pclItem2 != ' ')
              {
                 strcpy(&rgXml.rlLine[igCurXmlLines].pclFieldA[(ilI-7)/3][0],pclItem2);
                 rgXml.rlLine[igCurXmlLines].ilNoFieldA++;
              }
              if (*pclItem3 != ' ')
              {
                 strcpy(&rgXml.rlLine[igCurXmlLines].pclFieldD[(ilI-7)/3][0],pclItem3);
                 rgXml.rlLine[igCurXmlLines].ilNoFieldD++;
              }
           }
        }
        igCurXmlLines++;
        ilCurLine++;
        strcpy(pclPrevTag,pclTag);
     }
  }
  fclose(fp);
  if (strcmp(pcgMode,"INPUT") == 0)
  {
     strcpy(pclItem1,"");
     ilNoMethod = 0;
     for (ilI = 0; ilI < MAX_INTERFACES; ilI++)
     {
        for (ilJ = 0; ilJ < igCurXmlLines; ilJ++)
        {
           if (strlen(&rgXml.rlLine[ilJ].pclMethod[ilI][0]) > 0)
           {
              if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"HDR") == 0 &&
                  strcmp(&rgXml.rlLine[ilJ].pclName[0],pcgActionType) == 0)
              {
              }
              else
              {
                 strcpy(pclItem1,&rgXml.rlLine[ilJ].pclMethod[ilI][0]);
                 ilNoMethod = rgXml.rlLine[ilJ].ilNoMethod;
              }
           }
           else
           {
              strcpy(&rgXml.rlLine[ilJ].pclMethod[ilI][0],pclItem1);
              rgXml.rlLine[ilJ].ilNoMethod = ilNoMethod;
           }
        }
     }
  }
  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
    dbg(DEBUG,"%s %d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d",pclFunc,
        rgXml.rlLine[ilI].ilLevel,
        &rgXml.rlLine[ilI].pclTag[0],
        &rgXml.rlLine[ilI].pclName[0],
        &rgXml.rlLine[ilI].pclType[0],
        &rgXml.rlLine[ilI].pclBasTab[0],
        &rgXml.rlLine[ilI].pclBasFld[0],
        &rgXml.rlLine[ilI].pclFlag[0],
        &rgXml.rlLine[ilI].pclMethod[0][0],
        &rgXml.rlLine[ilI].pclFieldA[0][0],
        &rgXml.rlLine[ilI].pclFieldD[0][0],
        &rgXml.rlLine[ilI].pclMethod[1][0],
        &rgXml.rlLine[ilI].pclFieldA[1][0],
        &rgXml.rlLine[ilI].pclFieldD[1][0],
        rgXml.rlLine[ilI].ilNoMethod,
        rgXml.rlLine[ilI].ilNoFieldA,
        rgXml.rlLine[ilI].ilNoFieldD);
  }
  dbg(TRACE,"---------------------------------------------------------");
  for (ilI = 0; ilI < igCurInterface; ilI++)
  {
    dbg(DEBUG,"%s Interface: %s,%d",pclFunc,
        &rgInterface[ilI].pclIntfName[0],
        rgInterface[ilI].ilIntfIdx);
  }
  if (igUsingMultiTypeFlag == TRUE)
  {
     dbg(TRACE,"---------------------------------------------------------");
     dbg(TRACE,"USING SECTION TYPE <MULTI> FOR MULTIPLE RECORD PROCESSING");
     dbg(TRACE,"---------------------------------------------------------");
  }

  return ilRC;
} /* End of GetXMLSchema */

static int HandleInput(char *pcpData, int ipBegin, int ipEnd)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleInput:";
  int ilI;
  int ilJ;
  int ilCurLine;
  int ilXmlBgnPos = 0;
  int ilXmlEndPos = 0;
  int ilTagNamLen = 0;
  char *pclTmpPtr;
  char *pclBgnPtr;
  char pclLine[8000];
  char *pclLinePtr;
  char pclNewLine[8000];
  int ilIdx;
  int ilCurXMLLine;
  int ilFound;
  int ilMultiSectionIsOpen = FALSE;
  int ilMultiSectionIsComplete = FALSE;
  int ilMultiSectionHasData = FALSE;
  int ilMultiSectionHasMore = FALSE;
  char pclName[1024];
  char *pclNamePtr;
  char pclData[4000];
  char pclAction[16];
  int ilWrongData;
  char pclOrigin[16];
  char pclSection[64] = "";
  char pclChkMethod[64];
  char pclFileRequest[1024];
  char pclHandleRequest[1024];
  char pclTmpBuf[1024];
  char pclTmpMsgBuf[1024];
  int ilNoEle;
  int ilBegin;
  int ilEnd;
  int ilReturn = RC_SUCCESS;
  int ilActTypeIdx;
  char pclSearchItem[8000];
  char pclTagBgn[8000];
  char pclDat[8000];
  char pclTagEnd[8000];
  char *pclTmpPtr2;
  char pclSitaSections[1024];
  char pclTableSections[1024];
  char pclNoDelFields[1024];
  char pclTmpMethod[32] = "";
  char pclTmpTable[16] = "";
  char pclMinTime[32];
  char pclMaxTime[32];
  char pclMinField[32];
  char pclMaxField[32];

  char pclTmpLine[4096] = "\0";

  //Frank v1.52 20121226
  strcpy(pclTmpLine,pcpData);
	AddUrnoIntoXml(pcpData,pclTmpLine);
  //Frank v1.52 20121226

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MULTIPLE_SECTIONS",CFG_STRING,pcgListOfMultipleSections);
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgListOfMultipleSections,"");
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MULTIPLE_METHODS",CFG_STRING,pcgListOfMultipleMethodTriggers);
  if (ilRC != RC_SUCCESS)
  {
     strcpy(pcgListOfMultipleMethodTriggers,"");
  }
  strcpy(pcgAdid,"");
  strcpy(pcgUrno,"");
  strcpy(pcgIntfName,"");
  igNoKeys = 0;
  igNoData = 0;
  igIntfIdx = 0;
  ilWrongData = FALSE;
  igAutoActType = FALSE;
  igSearchFlightHits = 0;
  strcpy(pcgViaFldLst,"");
  strcpy(pcgViaDatLst,"");
  strcpy(pcgViaApc,"");
  if (strcmp(pcgMode,"FILE") != 0)
  {
     for (ilI = 0; ilI < igCurXmlLines; ilI++)
     {
        rgXml.rlLine[ilI].ilRcvFlag = 0;
        rgXml.rlLine[ilI].ilPrcFlag = 0;
        strcpy(&rgXml.rlLine[ilI].pclData[0],"");
     }
  }
  strcpy(&pcgTagAttributeName[0][0],"");
  strcpy(&pcgTagAttributeValue[0][0],"");

  ilXmlBgnPos = -1;
  ilXmlEndPos = -1;
  ilCurXMLLine = 0;
  ilCurLine = 1;

  pclTmpPtr = GetLine(pcpData,ilCurLine);
  while (pclTmpPtr != NULL)
  {
     memset(pclNewLine,0x00,8000);
     memset(pclDat,0x00,8000);
     strcpy(pclTagBgn,"");
     strcpy(pclTagEnd,"");
     ilIdx = 0;
     CopyLine(pclLine,pclTmpPtr);
     TrimRight(pclLine);
     pclLinePtr = pclLine;
     while (*pclLinePtr == ' ' && *pclLinePtr != '\0')
        pclLinePtr++;
     if (strlen(pclLinePtr) > 0)
     {
        if (*pclLinePtr == '<')
        {
           pclBgnPtr = pclLinePtr;
           pclLinePtr++;
           while (*pclLinePtr != '>' && *pclLinePtr != '\0')
           {
              pclNewLine[ilIdx] = *pclLinePtr;
              pclLinePtr++;
              ilIdx++;
           }
           if (*pclLinePtr != '\0')
              pclLinePtr++;
           strcpy(pclTagBgn,pclNewLine);
           if (strlen(pclLinePtr) > 0)
           {
              sprintf(pclSearchItem,"</%s",pclNewLine);
              if (pclSearchItem[strlen(pclSearchItem)-1] == '/')
                 pclSearchItem[strlen(pclSearchItem)-1] = '\0';
              pclTmpPtr2 = strstr(pclLinePtr,pclSearchItem);
              if (pclTmpPtr2 != NULL)
              {
                 strncpy(pclDat,pclLinePtr,pclTmpPtr2-pclLinePtr);
                 pclTmpPtr2++;
                 strcpy(pclTagEnd,pclTmpPtr2);
                 if (pclTagEnd[strlen(pclTagEnd)-1] == '>')
                    pclTagEnd[strlen(pclTagEnd)-1] = '\0';
              }
              else
              {
                 strcpy(pclDat,pclLinePtr);
              }
/*
              pclNewLine[ilIdx] = ',';
              ilIdx++;
              while (*pclLinePtr != '<' && *pclLinePtr != '\0')
              {
                 pclNewLine[ilIdx] = *pclLinePtr;
                 pclLinePtr++;
                 ilIdx++;
              }
              if (*pclLinePtr != '\0')
                 pclLinePtr++;
              if (strlen(pclLinePtr) > 0)
              {
                 pclNewLine[ilIdx] = ',';
                 ilIdx++;
                 while (*pclLinePtr != '>' && *pclLinePtr != '\0')
                 {
                    pclNewLine[ilIdx] = *pclLinePtr;
                    pclLinePtr++;
                    ilIdx++;
                 }
              }
*/
           }
           if (strlen(pclTagEnd) > 0)
           {
              strcat(pclTagBgn,",");
              strcat(pclTagBgn,pclDat);
              strcat(pclTagBgn,",");
              strcat(pclTagBgn,pclTagEnd);
           }
           else if (strlen(pclDat) > 0)
           {
              strcat(pclTagBgn,",");
              strcat(pclTagBgn,pclDat);
           }
/*
           while (*pclLinePtr != '\0')
              pclLinePtr++;
*/
           strcpy(pclNewLine,pclTagBgn);
           get_item(1,pclNewLine,pclName,0,",","\0","\0");
           TrimRight(pclName);

           pclNamePtr = pclName;
           if (pclName[strlen(pclName)-1] != '/' && strstr(pclName,"!--") == NULL && strstr(pclName,"?xml") == NULL)
           { /* ignore line */
              if (*pclNamePtr == '/')
                 *pclNamePtr++;
              ilFound = FALSE;
              if (ipBegin >= 0)
              {
                 ilBegin = ipBegin;
                 ilEnd = ipEnd;
              }
              else
              {
                 ilBegin = ilCurXMLLine;
                 ilEnd = igCurXmlLines;
              }
              dbg(DEBUG,"SEARCH  <%s> IN SCHEMA FROM LINE %d TO LINE %d",pclNamePtr,ilBegin,ilEnd);
              for (ilI = ilBegin; ilI < ilEnd && ilFound == FALSE; ilI++)
              {
              		//Frank v1.52 20121228
              		/*
              		if(!strcmp(pclNamePtr,"ONFL2ASTT"))
              		{
              			dbg(DEBUG,"&rgXml.rlLine[ilI].pclName[0] %s",&rgXml.rlLine[ilI].pclName[0]);
              			dbg(DEBUG,"pclNamePtr %s",pclNamePtr);
              		}
              		*/

                 if (strcmp(&rgXml.rlLine[ilI].pclName[0],pclNamePtr) == 0)
                 {
                    dbg(DEBUG,"        <%s> FOUND IN LINE %d RCV=%d",pclNamePtr,ilI,rgXml.rlLine[ilI].ilRcvFlag);
                    if (rgXml.rlLine[ilI].ilRcvFlag == 0)
                    {
                       rgXml.rlLine[ilI].ilRcvFlag = 1;
                       if (GetNoOfElements(pclNewLine,',') > 1)
                       {
                          if (ilMultiSectionIsComplete == FALSE)
                          {
                             get_item(2,pclNewLine,pclData,0,",","\0","\0");
                             if (strcmp(pclName,"VIAL") != 0)
                                TrimLeftRight(pclData);
                             else
                                TrimRight(pclData);
                             CheckCDATA(pclData);
                             strcpy(&rgXml.rlLine[ilI].pclData[0],pclData);
                             ilRC = CheckData(pclData,ilI);
                             if (ilRC != RC_SUCCESS)
                                ilWrongData = TRUE;
                          }
                       }
                       else
                       {
                          if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0)
                          {
                             dbg(DEBUG,"STR_BGN <%s> IDENTIFIED AS BEGIN OF SECTION",pclNamePtr);
                             strcpy(pclSection,&rgXml.rlLine[ilI].pclName[0]);
                             ilMultiSectionIsOpen = FALSE;
                             ilMultiSectionIsComplete = FALSE;
                             ilMultiSectionHasData = FALSE;
                             ilMultiSectionHasMore = FALSE;
                             if (strcmp(&rgXml.rlLine[ilI].pclType[0],"MULTI") == 0)
                             {
                                ilMultiSectionIsOpen = TRUE;
                                strcpy(pclChkMethod,&rgXml.rlLine[ilI].pclMethod[0][0]);
                                dbg(TRACE,"SECTION <%s> IS DECLARED AS MULTIPLE <%s> DATA AREA",pclSection,pclChkMethod);
                                ilTagNamLen = strlen(pclSection);
                                ilXmlBgnPos = (pclTmpPtr - pcpData) + (pclBgnPtr - pclLine);
                                dbg(DEBUG,"MULTI   <%s> BYTE POSITON in XML MESSAGE = %d",pclNamePtr,ilXmlBgnPos);
                             }
                             ilCurXMLLine = ilI + 1;
                          }
                          if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
                          {
                             if (ilMultiSectionIsOpen == TRUE)
                             {
                                ilMultiSectionHasData = TRUE;
                                dbg(TRACE,"MULTI   <%s> SECTION HAS DATA",pclSection);
                             }
                          }
                          if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_END") == 0)
                          {
                             dbg(DEBUG,"STR_END <%s> IDENTIFIED AS END OF SECTION",pclNamePtr);
                             if (ilMultiSectionIsOpen == TRUE)
                             {
                                dbg(TRACE,"MULTI   <%s> SECTION IS COMPLETE",pclSection);
                                ilMultiSectionIsComplete = TRUE;
                                if (ilXmlBgnPos >= 0)
                                {
                                   /* We will keep the patched area of the message buffer  */
                                   /* until we find more data of this multiple section and */
                                   /*then we'll set the flag to remove the patched section */
                                   ilXmlEndPos = (pclTmpPtr - pcpData) + (pclBgnPtr - pclLine);
                                   dbg(DEBUG,"MULTI   <%s> BYTE POSITON in XML MESSAGE = %d",pclNamePtr,ilXmlEndPos);
                                   dbg(DEBUG,"PATCH   <%s> SECTION START/END TAGS IN MESSAGE",pclNamePtr);
                                   memset(&pcpData[ilXmlBgnPos],' ',ilTagNamLen+2);
                                   strncpy(&pcpData[ilXmlBgnPos],"<@>",3);
                                   if (ilXmlEndPos >= 0)
                                   {
                                      memset(&pcpData[ilXmlEndPos],' ',ilTagNamLen+3);
                                      ilXmlEndPos += ilTagNamLen - 1;
                                      strncpy(&pcpData[ilXmlEndPos],"</@>",4);
                                   }
                                   ilXmlBgnPos = -1;
                                   ilXmlEndPos = -1;
                                   dbg(TRACE,"SECTION TRIGGER NAME <%s>",pclChkMethod);
                                   dbg(DEBUG,"======= PATCHED XML MESSAGE BUFFER ==========");
                                   dbg(DEBUG,"\n%s",pcpData);
                                   dbg(DEBUG,"=============================================");
                                }
                             }
                          }
                       }
                       ilFound = TRUE;
                    }
                    else
                    {
                       /* Decision Point of processing Multiple Record Sections */
                       if (ilMultiSectionIsOpen == TRUE)
                       {
                          if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
                          {
                             dbg(DEBUG,"DATATAG <%s> IN LINE %d IS ALREADY HANDLED",pclNamePtr,ilI);
                             ilMultiSectionHasMore = TRUE;
                             /* Avoid searching the whole schema */
                             ilFound = TRUE;
                          }
                          if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_END") == 0)
                          {
                             dbg(DEBUG,"STR_END <%s> IN LINE %d IS ALREADY HANDLED",pclNamePtr,ilI);
                             ilMultiSectionHasMore = TRUE;
                             if (strstr(pcgMultipleSectionSetTrigger,pclChkMethod) == NULL)
                             {
                                strcat(pcgMultipleSectionSetTrigger,pclChkMethod);
                             }
                             /* Set the flag to remove the patched section */
                             igMultipleSectionFound = TRUE;
                             /* Avoid searching the whole schema */
                             ilFound = TRUE;
                          }
                       }
                       else
                       {
                          ilReturn = - 10;
                       }
                    }
                 }
              }
              if (ilFound == FALSE)
              {
                 dbg(TRACE,"MULTI SECTIONS <%s>",pcgListOfMultipleSections);
                 dbg(TRACE,"CURRENT SECTION <%s> CHK <%s>",pclSection,pclChkMethod);
                 if (strstr(pcgListOfMultipleSections,pclSection) == NULL)
                 {
                    if (strcmp(pcgMode,"FILE") == 0)
                    {
                       if (ilReturn != -10)
                          dbg(TRACE,"%s Can't find Definition for Line %s in XML Schema! Section = <%s>",
                              pclFunc,pclLine,pclSection);
                    }
                    else
                    {
                       dbg(TRACE,"%s Can't find Definition for Line %s in XML Schema! Section = <%s>",
                           pclFunc,pclLine,pclSection);
                       sprintf(pcgStatusMsgTxt,"UNDEFINED TAG IN SECTION <%s> : %s\n",pclSection,pclLine);
                       SetStatusMessage(pcgStatusMsgTxt);
                    }
                    ilWrongData = TRUE;
                 }
                 else
                 {
                    strcpy(pcgMultipleSection,pclSection);
                    igMultipleSectionFound = TRUE;
                 }
              }
           }
        }
     }
     ilCurLine++;
     pclTmpPtr = GetLine(pcpData,ilCurLine);
  }

  if (strcmp(pcgMode,"FILE") == 0)
  {
     if (ilWrongData == TRUE)
     {
        if (ilReturn == -10)
           return ilReturn;
        else
           return RC_FAIL;
     }
     else
        return RC_SUCCESS;
  }

  if (ilWrongData == TRUE)
  {
     return RC_FAIL;
  }

  dbg(DEBUG,"%s Found Input:",pclFunc);
  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
    if (rgXml.rlLine[ilI].ilRcvFlag == 1)
    {
       dbg(DEBUG,"%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s",
           rgXml.rlLine[ilI].ilLevel,
           &rgXml.rlLine[ilI].pclTag[0],
           &rgXml.rlLine[ilI].pclName[0],
           &rgXml.rlLine[ilI].pclType[0],
           &rgXml.rlLine[ilI].pclBasFld[0],
           &rgXml.rlLine[ilI].pclBasTab[0],
           &rgXml.rlLine[ilI].pclFlag[0],
           &rgXml.rlLine[ilI].pclMethod[0][0],
           &rgXml.rlLine[ilI].pclFieldA[0][0],
           &rgXml.rlLine[ilI].pclFieldD[0][0],
           &rgXml.rlLine[ilI].pclMethod[1][0],
           &rgXml.rlLine[ilI].pclFieldA[1][0],
           &rgXml.rlLine[ilI].pclFieldD[1][0],
           rgXml.rlLine[ilI].ilNoMethod,
           rgXml.rlLine[ilI].ilNoFieldA,
           rgXml.rlLine[ilI].ilNoFieldD,
           &rgXml.rlLine[ilI].pclData[0]);
     }
  }

  ilActTypeIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
  if (ilActTypeIdx >= 0)
  {
     if (rgXml.rlLine[ilActTypeIdx].ilRcvFlag == 0)
     {
        rgXml.rlLine[ilActTypeIdx].ilRcvFlag = 1;
        strcpy(&rgXml.rlLine[ilActTypeIdx].pclData[0],"U");
        igAutoActType = TRUE;
     }
  }
  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 0 &&
         rgXml.rlLine[ilI].pclFlag[0] == 'M')
     {
         ilFound = FALSE;
         ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FILE_REQUEST",CFG_STRING,pclFileRequest);
         if (ilRC == RC_SUCCESS)
         {
            ilNoEle = GetNoOfElements(pclFileRequest,',');
            for (ilJ = 1; ilJ <= ilNoEle; ilJ++)
            {
               get_real_item(pclTmpBuf,pclFileRequest,ilJ);
               ilIdx = GetXmlLineIdx("STR_BGN",pclTmpBuf,NULL);
               if (ilIdx >= 0)
               {
                  if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
                  {
                     ilFound = TRUE;
                     ilRC = HandleFileRequest(pclTmpBuf);
                  }
               }
            }
         }
         if (ilFound == FALSE)
         {
            dbg(TRACE,"%s Header Field <%s> missing",pclFunc,&rgXml.rlLine[ilI].pclName[0]);
            sprintf(pcgStatusMsgTxt,"HEADER FIELD <%s> MISSING\n",&rgXml.rlLine[ilI].pclName[0]);
            SetStatusMessage(pcgStatusMsgTxt);
            return RC_FAIL;
         }
         else
         {
            return ilRC;
         }
     }
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","HANDLE_REQUEST",CFG_STRING,pclHandleRequest);
  if (ilRC == RC_SUCCESS && strlen(pclHandleRequest) > 0)
  {
     ilIdx = GetXmlLineIdx("STR_BGN",pclHandleRequest,NULL);
     if (ilIdx >= 0)
     {
        if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
        {
           ilRC = HandleRequest(pclHandleRequest);
        }
     }
  }

  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0 && strcmp(&rgXml.rlLine[ilI].pclName[0],pcgMessageOrigin) == 0)
     {
        ilFound = TRUE;
        strcpy(pclOrigin,"STD");
        if (rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           strcpy(pclOrigin,&rgXml.rlLine[ilI].pclData[0]);
           strcpy(pcgIntfName,&rgXml.rlLine[ilI].pclData[0]);
        }
     }
  }
  dbg(TRACE,"CHECK INTERFACE NAME <%s>",pclOrigin);
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurInterface && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgInterface[ilI].pclIntfName[0],pclOrigin) == 0)
     {
        igIntfIdx = rgInterface[ilI].ilIntfIdx;
        ilFound = TRUE;
     }
  }
  if (ilFound == FALSE)
  {
     if (strcmp(pclOrigin,"STD") == 0)
     {
        igIntfIdx = 0;
     }
     else
     {
        strcpy(pclOrigin,"STD");
        ilFound = FALSE;
        for (ilI = 0; ilI < igCurInterface && ilFound == FALSE; ilI++)
        {
           if (strcmp(&rgInterface[ilI].pclIntfName[0],pclOrigin) == 0)
           {
              igIntfIdx = rgInterface[ilI].ilIntfIdx;
              ilFound = TRUE;
           }
        }
     }
  }
  if (ilFound == TRUE)
     dbg(DEBUG,"Use Interface Index = %d for Interface = %s",igIntfIdx,&rgInterface[igIntfIdx].pclIntfName[0]);
  else
     dbg(DEBUG,"No Special Interface handling defined!!!");

  if (strcmp(pcgMode,"INPUT") == 0)
  {
     if (strcmp(pcgSubMode,"ADR_PDA") == 0)
     {
        ilRC = HandlePDAIn();
        return ilRC;
     }
     else if (strcmp(pcgSubMode,"ADR_WEB") == 0)
     {
        ilRC = HandleWEBIn();
        return ilRC;
     }

     dbg(TRACE,"CHECK SITA_SECTIONS CONFIG");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SITA_SECTIONS",CFG_STRING,pclSitaSections);
     if (ilRC == RC_SUCCESS)
     {
        ilNoEle = GetNoOfElements(pclSitaSections,',');
        for (ilJ = 1; ilJ <= ilNoEle; ilJ++)
        {
           get_real_item(pclTmpBuf,pclSitaSections,ilJ);
           ilIdx = GetXmlLineIdx("STR_BGN",pclTmpBuf,NULL);
           if (ilIdx >= 0)
           {
              if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
              {
                 ilRC = HandleSitaSections(pclTmpBuf);
                 return ilRC;
              }
           }
        }
     }
     dbg(TRACE,"CHECK TABLE_SECTIONS CONFIG");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TABLE_SECTIONS",CFG_STRING,pclTableSections);
     if (ilRC == RC_SUCCESS)
     {
        ilNoEle = GetNoOfElements(pclTableSections,',');
        for (ilJ = 1; ilJ <= ilNoEle; ilJ++)
        {
           get_real_item(pclTmpBuf,pclTableSections,ilJ);
           ilIdx = GetXmlLineIdx("STR_BGN",pclTmpBuf,NULL);
           if (ilIdx >= 0)
           {
              if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
              {
                 ilRC = HandleTableData(pclTmpBuf);
                 return ilRC;
              }
           }
        }
     }
     dbg(TRACE,"CHECK AUTO_ACT_TYPE SETTING");
     if (igAutoActType == TRUE)
     {
        if (rgXml.rlLine[ilActTypeIdx].pclMethod[igIntfIdx][0] == 'M')
        {
           rgXml.rlLine[ilActTypeIdx].ilRcvFlag = 0;
           strcpy(&rgXml.rlLine[ilActTypeIdx].pclData[0],"");
           dbg(TRACE,"%s Header Field <%s> is mandatory for this Interface",pclFunc,pcgActionType);
           return RC_FAIL;
        }
     }
     dbg(TRACE,"CHECK MAX_TIME_DIFF_FOR_UPDATE SETTING");
     if (igMaxTimeDiffForUpdate > 0)
     {  /* Check for abnormal Time Differences */
        strcpy(pclMinTime,"");
        strcpy(pclMaxTime,"");
        strcpy(pclMinField,"");
        strcpy(pclMaxField,"");
        for (ilI = 0; ilI < igCurXmlLines; ilI++)
        {
           if ((strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0 || strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0) &&
               strcmp(&rgXml.rlLine[ilI].pclType[0],"DATI") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
           {
              strcpy(pclTmpBuf,&rgXml.rlLine[ilI].pclData[0]);
              if (strlen(pclTmpBuf) == 12)
                 strcat(pclTmpBuf,"00");
              if (strlen(pclTmpBuf) == 14)
              {
                 if (*pclMinTime == '\0')
                 {
                    strcpy(pclMinTime,pclTmpBuf);
                    strcpy(pclMaxTime,pclTmpBuf);
                    strcpy(pclMinField,&rgXml.rlLine[ilI].pclName[0]);
                    strcpy(pclMaxField,&rgXml.rlLine[ilI].pclName[0]);
                 }
                 else
                 {
                    if (strcmp(pclTmpBuf,pclMinTime) < 0)
                    {
                       strcpy(pclMinTime,pclTmpBuf);
                       strcpy(pclMinField,&rgXml.rlLine[ilI].pclName[0]);
                    }
                    else
                    {
                       if (strcmp(pclTmpBuf,pclMaxTime) > 0)
                       {
                          strcpy(pclMaxTime,pclTmpBuf);
                          strcpy(pclMaxField,&rgXml.rlLine[ilI].pclName[0]);
                       }
                    }
                 }
              }
           }
        }
        if (strlen(pclMinTime) > 0)
        {
           strcpy(pclTmpBuf,pclMinTime);
           ilRC = MyAddSecondsToCEDATime(pclTmpBuf,igMaxTimeDiffForUpdate*60*60,1);
           if (strcmp(pclTmpBuf,pclMaxTime) < 0)
           {
              dbg(TRACE,"%s Time Difference between Field <%s> and <%s> is more than %d hours",
                  pclFunc,pclMinField,pclMaxField,igMaxTimeDiffForUpdate);
              dbg(TRACE,"%s Message is rejected",pclFunc);
              return RC_FAIL;
           }
        }
     }
     dbg(TRACE,"CHECK MANDATORY HEADER TAG ACTIONTYPE");
     ilFound = FALSE;
     for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
     {

        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0 && strcmp(&rgXml.rlLine[ilI].pclName[0],pcgActionType) == 0)
        {
           ilFound = TRUE;
           if (rgXml.rlLine[ilI].ilRcvFlag == 1)
           {
              strcpy(pclAction,&rgXml.rlLine[ilI].pclData[0]);
              if (strcmp(pclAction,pcgActionTypeUpd) == 0)
              {
                 dbg(TRACE,"CALLING HANDLE_UPDATE");
                 ilRC = HandleUpdate(pclAction);
              }
              else
              {
                 if (strcmp(pclAction,pcgActionTypeIns) == 0)
                 {
                    dbg(TRACE,"ACTIONTYPE = INSERT: CHECK IF FLIGHT ALREADY EXISTS");
                    ilRC = GetKeys(pcgActionTypeUpd);
                    ilRC = SearchFlight(pclTmpMethod,pclTmpTable);
                    if (ilRC == RC_SUCCESS)
                    {
                       dbg(TRACE,"CALLING HANDLE_UPDATE");
                       ilRC = HandleUpdate(pclAction);
                    }
                    else
                    {
                       if (igSearchFlightHits == 0)
                       {
                          dbg(TRACE,"CALLING HANDLE_INSERT");
                          ilRC = HandleInsert();
                       }
                       else
                       {
                          dbg(TRACE,"NOT INSERTED: ALREADY %d SUCH FLIGHTS IN DB",igSearchFlightHits);
                          ilRC = RC_FAIL;
                       }
                    }
                 }
                 else
                 {
                    if (strcmp(pclAction,pcgActionTypeDel) == 0)
                    {
                       dbg(TRACE,"ACTIONTYPE = DELETE: CHECK IF FLIGHT CAN BE DELETED");
                       ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","NO_DEL_FIELDS",CFG_STRING,pclNoDelFields);
                       if (ilRC != RC_SUCCESS)
                       {
                          strcpy(pclNoDelFields,"");
                       }
                       ilRC = CheckReceivedTags(pclNoDelFields);
                       if (ilRC == TRUE)
                       {
                          dbg(TRACE,"CALLING HANDLE_UPDATE");
                          ilRC = HandleUpdate(pclAction);
                       }
                       else
                       {
                          dbg(TRACE,"CALLING HANDLE_DELETE");
                          ilRC = HandleDelete();
                       }
                    }
                    else
                    {
                       if (strcmp(pclAction,"A") == 0)
                       {
                          dbg(TRACE,"CALLING HANDLE_ACK");
                          ilRC = HandleAck();
                       }
                       else
                       {
                          ilRC = RC_FAIL;
                      dbg(TRACE,"%s Wrong Action Type <%s> received!!!",pclFunc,pclAction);
                       }
                    }
                 }
              }
           }
           else
           {
              ilRC = RC_FAIL;
              dbg(TRACE,"%s No Action Type received!!!",pclFunc);
           }
        }
     }
     if (ilFound == FALSE)
     {
        ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEFAULT_ACTIONTYPE",CFG_STRING,pclAction);
        if (ilRC == RC_SUCCESS)
           ilRC = HandleUpdate(pclAction);
        else
           dbg(TRACE,"%s No Action Type defined!!!",pclFunc);
     }
  }

  return ilRC;
} /* End of HandleInput */

static int CheckReceivedTags(char *pcpTagLst)
{
  int ilI = 0;
  int ilJ = 0;
  int ilNoEle = 0;
  int ilFound = FALSE;
  char pclTmpBuf[32];
  ilFound = FALSE;
  ilNoEle = GetNoOfElements(pcpTagLst,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pcpTagLst,ilI);
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) != NULL && rgXml.rlLine[ilJ].ilRcvFlag == 1)
        {
           dbg(TRACE,"CheckReceivedTags found <%s> data <%s>",pclTmpBuf,rgXml.rlLine[ilJ].pclData);
           ilFound = TRUE;
        }
     }
  }
  return ilFound;
}

static int HandleAck()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleAck:";
  char pclMethod[32];
  char pclTable[16];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;

  strcpy(pclMethod,"");
  strcpy(pclTable,"");
  ilRC = GetKeys("A");
  if (ilRC != RC_SUCCESS)
     return ilRC;
  ilRC = SearchFlight(pclMethod,pclTable);
  if (*pcgUrno != '\0')
  {
     strcpy(pclFieldList,"ADID");
     strcpy(pclDataList,pcgAdid);
     strcpy(pclSelection,pcgUrno);
     ilRC = iGetConfigEntry(pcgConfigFile,"INS-ACK","mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7922");
     ilRC = iGetConfigEntry(pcgConfigFile,"INS-ACK","snd_cmd",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"ACK");
     ilRC = iGetConfigEntry(pcgConfigFile,"INS-ACK","priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                          ilPriority,NETOUT_NO_ACK);
  }

  return ilRC;
} /* End of HandleAck */


static int HandleDelete()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleDelete:";
  char pclMethod[32];
  char pclTable[16];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;
  int ilFound;
  int ilI;
  int ilJ;
  char pclName[1024];
  char pclTmpBuf[1024];
  int ilNoEle;

  strcpy(pcgPosSel,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT",CFG_STRING,pclName);
  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pclName,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING",CFG_STRING,pclTmpBuf);
  ilFound = FALSE;
  if (ilRC == RC_SUCCESS)
  {
     for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilI].pclName[0]) != NULL && rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }
  if (ilFound == TRUE)
  {
     ilRC = HandleTowDel(pclTmpBuf);
     return ilRC;
  }
  ilRC = GetKeys("D");
  if (ilRC != RC_SUCCESS)
     return ilRC;
  ilFound = FALSE;
  ilNoEle = GetNoOfElements(pclName,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) != NULL && rgXml.rlLine[ilJ].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }
  if (ilFound == FALSE)
  {
     ilRC = HandleResources("D");
     return ilRC;
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_D",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Delete of Flight not allowed for this interface",pclFunc);
     ilRC = HandleResources("D");
     return ilRC;
  }
  ilRC = SearchFlight(pclMethod,pclTable);
  if (*pcgUrno != '\0')
  {
     ilRC = CheckForDelFromCache();
     ilRC = CheckForDelFromCacheDel();
     strcpy(&pcgSaveDeletes[igSavDelCnt][0],"URNO");
     igSavDelCnt++;
     strcpy(&pcgSaveDeletes[igSavDelCnt][0],pcgUrno);
     igSavDelCnt++;
     dbg(TRACE,"%s Current Number of deleted Records in Cache = %d",pclFunc,igSavDelCnt/2);
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7800");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
  }

  return ilRC;
} /* End of HandleDelete */


static int HandleInsert()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleInsert:";
  int ilI;
  int ilJ;
  char pclMethod[32];
  char pclName[1024];
  char pclTable[16];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;
  int ilFound;
  char pclFieldList[1024];
  char pclDataList[8000];
  char pclSelection[128];
  int ilSavNoData;
  int ilNoEle;
  char pclTmpBuf[1024];
  int ilIdx;
  char pclManFlds[1024];
  char pclFldNam[16];
  char pclAirlineCodes[1024];
  char pclAlc2[16];
  char pclAlc3[16];
  int ilItemNo;
  char pclAdid[16];
  char pclOrg3[16];
  char pclDes3[16];

  strcpy(pcgPosSel,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT",CFG_STRING,pclName);
  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pclName,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING",CFG_STRING,pclTmpBuf);
  ilFound = FALSE;
  if (ilRC == RC_SUCCESS)
  {
     for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilI].pclName[0]) != NULL && rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }
  if (ilFound == TRUE)
  {
     ilRC = GetKeys("U");
     if (ilRC != RC_SUCCESS)
        return ilRC;
     ilRC = HandleTowIns(pclTmpBuf);
     return ilRC;
  }
  ilRC = GetKeys("I");
  ilFound = FALSE;
  ilNoEle = GetNoOfElements(pclName,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) != NULL && rgXml.rlLine[ilJ].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }
  if (ilFound == FALSE)
  {
     ilRC = HandleResources("I");
     return ilRC;
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_I",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Insert of Flight not allowed for this interface",pclFunc);
     ilRC = HandleResources("I");
     return ilRC;
  }
  for (ilI = 0; ilI < igNoKeys; ilI++)
  {
     strcpy(&rgData[ilI].pclField[0],&rgKeys[ilI].pclField[0]);
     strcpy(&rgData[ilI].pclType[0],&rgKeys[ilI].pclType[0]);
     strcpy(&rgData[ilI].pclMethod[0],&rgKeys[ilI].pclMethod[0]);
     strcpy(&rgData[ilI].pclData[0],&rgKeys[ilI].pclData[0]);
     strcpy(&rgData[ilI].pclOldData[0]," ");
  }
  igNoData = igNoKeys;
  ilSavNoData = igNoData;
  ilNoEle = GetNoOfElements(pclName,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     ilRC = GetData(pclMethod,igNoData,pclTmpBuf);
  }
  if (igNoData > ilSavNoData)
  {
     ilRC = DataAddOns();
     ilRC = ValidateAdid();
     if (ilRC == RC_SUCCESS)
     {
        ilRC = AdjustData();
        ilIdx = GetDataLineIdx("ADID",0);
        if (ilIdx >= 0)
           strcpy(pclAdid,&rgData[ilIdx].pclData[0]);
        else
           strcpy(pclAdid,"");
        ilIdx = GetDataLineIdx("ORG3",0);
        if (ilIdx >= 0)
           strcpy(pclOrg3,&rgData[ilIdx].pclData[0]);
        else
           strcpy(pclOrg3,"");
        ilIdx = GetDataLineIdx("DES3",0);
        if (ilIdx >= 0)
           strcpy(pclDes3,&rgData[ilIdx].pclData[0]);
        else
           strcpy(pclDes3,"");
        if (*pclAdid == 'A' && strcmp(pclOrg3,pclDes3) == 0 && strcmp(pclOrg3,pcgHomeAp) == 0)
        {
           dbg(TRACE,"%s Insert ignored because this is an Insert of an Arrival Return or Turnaround Flight",
               pclFunc);
           ilRC = SearchTurnAroundFlight(pclMethod);
           return RC_FAIL;
        }
        if (ilRC == RC_SUCCESS)
        {
           ilRC = GetNextUrno();
           strcpy(pclFieldList,"URNO,");
           sprintf(pclDataList,"%d,",igLastUrno);
           for (ilI = 0; ilI < igNoData; ilI++)
           {
              if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0)
              {
                 strcat(pclFieldList,&rgData[ilI].pclField[0]);
                 strcat(pclFieldList,",");
                 strcat(pclDataList,&rgData[ilI].pclData[0]);
                 strcat(pclDataList,",");
              }
           }
           if (strlen(pclFieldList) > 0)
           {
              pclFieldList[strlen(pclFieldList)-1] = '\0';
              if (strlen(pclDataList) > 0)
                 pclDataList[strlen(pclDataList)-1] = '\0';
              strcpy(pclSelection,"");
              strcpy(pclAirlineCodes,"");
              sprintf(pclTmpBuf,"ALLOWED_AIRLINE_CODES_FOR_%s",pcgIntfName);
              ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL",pclTmpBuf,CFG_STRING,pclAirlineCodes);
              if (strlen(pclAirlineCodes) > 0)
              {
                 ilItemNo = get_item_no(pclFieldList,"ALC2",5) + 1;
                 if (ilItemNo > 0)
                    get_real_item(pclAlc2,pclDataList,ilItemNo);
                 ilItemNo = get_item_no(pclFieldList,"ALC3",5) + 1;
                 if (ilItemNo > 0)
                    get_real_item(pclAlc3,pclDataList,ilItemNo);
                 TrimRight(pclAlc2);
                 TrimRight(pclAlc3);
                 ilFound = FALSE;
                 ilNoEle = GetNoOfElements(pclAirlineCodes,',');
                 for (ilI = 1; ilI <= ilNoEle && ilFound == FALSE; ilI++)
                 {
                    GetDataItem(pclTmpBuf,pclAirlineCodes,ilI,',',""," ");
                    TrimRight(pclTmpBuf);
                    if (strcmp(pclTmpBuf,pclAlc2) == 0 || strcmp(pclTmpBuf,pclAlc3) == 0)
                       ilFound = TRUE;
                 }
                 if (ilFound == FALSE)
                 {
                    dbg(TRACE,"%s Airline <%s>/<%s> not allowed for this Interface <%s>",pclFunc,pclAlc2,pclAlc3,pcgIntfName);
                    *pcgUrno = '\0';
                    return RC_FAIL;
                 }
              }
              if (igAutoActType == TRUE)
              {
                 if (*pcgAdid == 'D')
                    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","INS_FLT_D",
                                           CFG_STRING,pclManFlds);
                 else
                    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","INS_FLT_A",
                                           CFG_STRING,pclManFlds);
                 if (ilRC == RC_SUCCESS)
                 {
                    ilNoEle = GetNoOfElements(pclManFlds,',');
                    for (ilJ = 1; ilJ <= ilNoEle; ilJ++)
                    {
                       get_real_item(pclFldNam,pclManFlds,ilJ);
                       if (strstr(pclFieldList,pclFldNam) == NULL)
                       {
                          dbg(TRACE,"%s Mandatory Field <%s> for Insert missing",
                              pclFunc,pclFldNam);
                          return RC_FAIL;
                       }
                    }
                 }
                 ilRC = CheckForDelFromCache();
                 ilRC = CheckForDelFromCacheDel();
                 strcpy(&pcgSaveInserts[igSavInsCnt][0],pclFieldList);
                 igSavInsCnt++;
                 strcpy(&pcgSaveInserts[igSavInsCnt][0],pclDataList);
                 igSavInsCnt++;
                 dbg(TRACE,"%s Current Number of inserted Records in Cache = %d",pclFunc,igSavInsCnt/2);
              }
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclTable,"AFTTAB");
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclModId,"7800");
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclPriority,"3");
              ilPriority = atoi(pclPriority);
              ilModId = atoi(pclModId);
              dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
              dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
              dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
              dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
              ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                   pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
           }
        }
     }
  }
  else
  {
     ilRC = HandleResources("I");
  }

  return ilRC;
} /* End of HandleInsert */


static int HandleUpdate(char *pcpActionType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleUpdate:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  int ilI;
  char pclFieldList[1024];
  char pclFieldListSave[1024];
  char pclDataList[8000];
  char pclSelection[1024];
  char pclTmpBuf[1024];
  char pclMethod[32];
  char pclName[1024];
  char pclTable[16];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;
  int ilNoItems;
  char pclItem[16];
  int ilItemNo;
  int ilFound;
  char pclDoNotClearFields[1024];
  int ilDoUpdate;
  int ilNoEle;
  int ilJ;
  int ilActTypeIdx;
  int ilDoCachedUpdate = FALSE;
  char pclFtyp[16];
  int ilIdx;
  char pclViaList[2048];
  char pclViaFldLst[2048];
  char pclViaDatLst[2048];
  int ilPosIdx;
  int ilDataRead = FALSE;

  strcpy(pcgPosSel,"");
  dbg(TRACE,"CHECK FLIGHT CONFIG");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT",CFG_STRING,pclName);
  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pclName,&rgXml.rlLine[ilI].pclName[0]) != NULL && rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  dbg(TRACE,"CHECK TOWING CONFIG");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING",CFG_STRING,pclTmpBuf);
  ilFound = FALSE;
  if (ilRC == RC_SUCCESS)
  {
     for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilI].pclName[0]) != NULL && rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }
  if (ilFound == TRUE)
  {
     if (*pcpActionType == 'I')
        ilRC = HandleTowIns(pclTmpBuf);
     else
        ilRC = HandleTowUpd(pclTmpBuf);
     return ilRC;
  }
  dbg(TRACE,"CHECK GET_KEYS SETTING");
  ilRC = GetKeys("U");
  if (ilRC != RC_SUCCESS)
     return ilRC;


  dbg(TRACE,"CHECK NO_OF_ELEMENTS <%s>",pclName);
  ilFound = FALSE;
  ilNoEle = GetNoOfElements(pclName,',');
  dbg(TRACE,"RESULT = %d ELEMENTS",ilNoEle);

  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strstr(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) != NULL && rgXml.rlLine[ilJ].ilRcvFlag == 1)
        {
           ilFound = TRUE;
        }
     }
  }

  if (ilFound == FALSE)
  {
     dbg(TRACE,"NO DATA RECEIVED FOR ELEMENTS <%s>",pclName);
     dbg(TRACE,"CALLING HANDLE_RESOURCES");
     ilRC = HandleResources("U");
     return ilRC;
  }

  dbg(TRACE,"LOOKUP CONFIG <%s> FOR <snd_cmd_U>",pclMethod);
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_U",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Update of Flight not allowed for this interface",pclFunc);
     dbg(TRACE,"CALLING HANDLE_RESOURCES");
     ilRC = HandleResources("U");
     return ilRC;
  }

  ilDoCachedUpdate = FALSE;
  if (strcmp(&rgInterface[igIntfIdx].pclIntfName[0],"ATC") == 0 || strcmp(&rgInterface[igIntfIdx].pclIntfName[0],"ATCA") == 0)
  {
     dbg(TRACE,"HANDLE ATC/ATCA INTERFACE");
     for (ilI = 0; ilI < igNoKeys; ilI++)
     {
        strcpy(&rgData[ilI].pclField[0],&rgKeys[ilI].pclField[0]);
        strcpy(&rgData[ilI].pclType[0],&rgKeys[ilI].pclType[0]);
        strcpy(&rgData[ilI].pclMethod[0],&rgKeys[ilI].pclMethod[0]);
        strcpy(&rgData[ilI].pclData[0],&rgKeys[ilI].pclData[0]);
        strcpy(&rgData[ilI].pclOldData[0]," ");
     }
     igNoData = igNoKeys;
     strcpy(pcgUrno,"ATC");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"AFTTAB");
  }
  else
  {
     dbg(TRACE,"CHECK CACHED_UPDATE SETTING");
     if (igNoKeys > 0)
     {
        dbg(TRACE,"CALLING SEARCH_FLIGHT");
        ilDataRead = FALSE;
        ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"CHECK_POSITION",CFG_STRING,pcgCheckPosition);
        if (ilRC != RC_SUCCESS)
           strcpy(pcgPosSel,"");
        else
        {
           igNoData = 0;
           ilNoEle = GetNoOfElements(pclName,',');
           for (ilI = 1; ilI <= ilNoEle; ilI++)
           {
              get_real_item(pclTmpBuf,pclName,ilI);
              ilRC = GetData(pclMethod,igNoData,pclTmpBuf);
           }
           ilDataRead = TRUE;
           ilPosIdx = GetDataLineIdx("PSTA",0);
           if (ilPosIdx >= 0)
              sprintf(pcgPosSel," AND PSTA = '%s'",&rgData[ilPosIdx].pclData[0]);
           else
           {
              ilPosIdx = GetDataLineIdx("PSTD",0);
              if (ilPosIdx >= 0)
                 sprintf(pcgPosSel," AND PSTD = '%s'",&rgData[ilPosIdx].pclData[0]);
              else
              {
                 ilPosIdx = GetDataLineIdx("STAND",0);
                 if (ilPosIdx >= 0)
                 {
                    if (*pcgAdid == 'A')
                       sprintf(pcgPosSel," AND PSTA = '%s'",&rgData[ilPosIdx].pclData[0]);
                    else
                       sprintf(pcgPosSel," AND PSTD = '%s'",&rgData[ilPosIdx].pclData[0]);
                 }
                 else
                    strcpy(pcgPosSel,"");
              }
           }
        }
        ilRC = SearchFlight(pclMethod,pclTable);
        if (ilRC != RC_SUCCESS && igAutoActType == TRUE)
        {
           dbg(TRACE,"CALLING CHECK_FOR_INSERT");
           ilRC = CheckForInsert();
           if (ilRC == RC_SUCCESS)
           {
              ilActTypeIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
              strcpy(&rgXml.rlLine[ilActTypeIdx].pclData[0],pcgActionTypeIns);
              dbg(TRACE,"CALLING HANDLE_INSERT");
              ilRC = HandleInsert();
              return ilRC;
           }
           else
           {
              dbg(TRACE,"SET CACHED_UPDATE FLAG");
              ilDoCachedUpdate = TRUE;
           }
        }
        else if (ilRC == RC_SUCCESS && igAutoActType == TRUE)
        {
           dbg(TRACE,"CALLING CHECK_FOR_INSERT_DEL");
           ilRC = CheckForInsertDel();
           if (ilRC == RC_SUCCESS)
           {
              ilActTypeIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
              strcpy(&rgXml.rlLine[ilActTypeIdx].pclData[0],pcgActionTypeIns);
              dbg(TRACE,"CALLING HANDLE_INSERT");
              ilRC = HandleInsert();
              return ilRC;
           }
        }
     }
  }
  dbg(TRACE,"STILL IN HANDLE_UPDATE");
  dbg(TRACE,"IDENTIFIED URNO <%s>",pcgUrno);
  if (*pcgUrno != '\0')
  {
     if (ilDataRead == FALSE)
     {
        ilNoEle = GetNoOfElements(pclName,',');
        for (ilI = 1; ilI <= ilNoEle; ilI++)
        {
           if (strcmp(pcgUrno,"ATC") != 0 && ilI == 1)
              igNoData = 0;
           get_real_item(pclTmpBuf,pclName,ilI);
           ilRC = GetData(pclMethod,igNoData,pclTmpBuf);
        }
     }
     dbg(TRACE,"CALLING DATA_ADD_ONS");
     ilRC = DataAddOns();
     dbg(TRACE,"RESULT = %d ADD_ON DATA",igNoData);
     if (igNoData > 0 || strlen(pcgViaFldLst) > 0)
     {
        if (strlen(pcgViaFldLst) > 0)
        {
           pcgViaFldLst[strlen(pcgViaFldLst)-1] = ']';
           pcgViaDatLst[strlen(pcgViaDatLst)-1] = '\0';
        }
        ilNoItems = 0;
        strcpy(pclFieldList,"");
        for (ilI = 0; ilI < igNoData; ilI++)
        {
           if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0 && strcmp(&rgData[ilI].pclField[0],"ROUT") != 0)
           {
              strcat(pclFieldList,&rgData[ilI].pclField[0]);
              strcat(pclFieldList,",");
              ilNoItems++;
           }
        }
        if (strlen(pclFieldList) > 0 || strlen(pcgViaFldLst) > 0)
        {
           if (strlen(pclFieldList) > 0)
              pclFieldList[strlen(pclFieldList)-1] = '\0';
           if (strcmp(pcgUrno,"ATC") == 0 || ilDoCachedUpdate == TRUE ||
               (strlen(pclFieldList) == 0 && strlen(pcgViaFldLst) > 0))
           {
              ilRCdb = DB_SUCCESS;
           }
           else
           {
              ilIdx = GetDataLineIdx("TOID",0);
              if (ilIdx >= 0)
              {
                 if (strcmp(&rgData[ilIdx].pclData[0],pcgUrno) != 0)
                    strcpy(pcgUrno,&rgData[ilIdx].pclData[0]);
              }
              sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
              sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pclTable,pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              close_my_cursor(&slCursor);
           }
           if (ilRCdb == DB_SUCCESS)
           {
              strcpy(pclFieldListSave,pclFieldList);
              strcpy(pclFieldList,"");
              strcpy(pclDataList,"");
              if (strcmp(pcgUrno,"ATC") != 0)
                 BuildItemBuffer(pclDataBuf,"",ilNoItems,",");
              for (ilI = 0; ilI < igNoData; ilI++)
              {
                 strcpy(pclItem,&rgData[ilI].pclField[0]);
                 if (strcmp(pcgUrno,"ATC") == 0 || ilDoCachedUpdate == TRUE)
                 {
                    strcpy(&rgData[ilI].pclOldData[0],&rgData[ilI].pclData[0]);
                    strcat(&rgData[ilI].pclOldData[0],"ATC");
                 }
                 else
                 {
                    ilItemNo = get_item_no(pclFieldListSave,pclItem,5) + 1;
                    if (ilItemNo > 0)
                    {
                       get_real_item(&rgData[ilI].pclOldData[0],pclDataBuf,ilItemNo);
                       TrimRight(&rgData[ilI].pclOldData[0]);
                    }
                 }
                 if (strcmp(pclItem,"XXXX") != 0 && strcmp(&rgData[ilI].pclData[0],&rgData[ilI].pclOldData[0]) != 0)
                 {
                    ilDoUpdate = TRUE;
                    if (rgData[ilI].pclData[0] == ' ')
                    {
                       ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"DO_NOT_CLEAR",CFG_STRING,pclDoNotClearFields);
                       if (ilRC == RC_SUCCESS)
                       {
                          if (strstr(pclDoNotClearFields,pclItem) != NULL)
                             ilDoUpdate = FALSE;
                       }
                    }
                    if (ilDoUpdate == TRUE)
                    {
                       strcat(pclFieldList,pclItem);
                       strcat(pclFieldList,",");
                       strcat(pclDataList,&rgData[ilI].pclData[0]);
                       strcat(pclDataList,",");
                    }
                 }
              }
              if (strlen(pcgViaFldLst) > 0)
              {
                 sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
                 sprintf(pclSqlBuf,"SELECT VIAL FROM %s %s",pclTable,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 sleep(1);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclViaList);
                 close_my_cursor(&slCursor);
                 TrimRight(pclViaList);
                 if (strlen(pclViaList) == 1)
                 {
                    strcat(pclFieldList,"ROUT,");
                    strcat(pclDataList,pcgViaFldLst);
                    strcat(pclDataList,pcgViaDatLst);
                    strcat(pclDataList,",");
                 }
                 else if (strstr(pclViaList,pcgViaApc) != NULL)
                 {
                    strcat(pclFieldList,"VSUP,");
                    strcat(pclDataList,pcgViaFldLst);
                    strcat(pclDataList,pcgViaDatLst);
                    strcat(pclDataList,",");
                 }
                 else
                 {
                    strcpy(pclViaFldLst,"[FIDS;APC3;APC4;STOA;ETOA;LAND;ONBL;STOD;ETOD;OFBL;AIRB]");
                    memset(pclViaDatLst,0x00,2000);
                    ilJ = 0;
                    while (strlen(pclViaList) > ilJ * 120)
                    {
                       if (pclViaList[ilJ*120+0] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+0],1);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+1] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+1],3);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+4] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+4],4);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+8] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+8],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+22] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+22],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+36] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+36],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+50] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+50],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+64] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+64],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+78] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+78],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+92] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+92],14);
                       strcat(pclViaDatLst,";");
                       if (pclViaList[ilJ*120+106] != ' ')
                          strncat(pclViaDatLst,&pclViaList[ilJ*120+106],14);
                       strcat(pclViaDatLst,"|");
                       ilJ++;
                    }
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"FIDS",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"APC3",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"APC4",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"STOA",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"ETOA",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"LAND",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"ONBL",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"STOD",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"ETOD",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"OFBL",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    ilItemNo = get_item_no(&pcgViaFldLst[1],"AIRB",5) + 1;
                    if (ilItemNo > 0)
                    {
                       GetDataItem(pclTmpBuf,pcgViaDatLst,ilItemNo,';',"","\0\0");
                       strcat(pclViaDatLst,pclTmpBuf);
                    }
                    strcat(pclViaDatLst,";");
                    strcat(pclFieldList,"ROUT,");
                    strcat(pclDataList,pclViaFldLst);
                    strcat(pclDataList,pclViaDatLst);
                 }
              }
              if (strlen(pclFieldList) > 0)
              {
                 pclFieldList[strlen(pclFieldList)-1] = '\0';
                 if (strlen(pclDataList) > 0)
                    pclDataList[strlen(pclDataList)-1] = '\0';
                 if (strcmp(pcgUrno,"ATC") == 0)
                 {
                    ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"sub_type_U",CFG_STRING,pclSelection);
                    strcat(pclDataList,"\n");
                    ilNoEle = GetNoOfElements(pclDataList,',');
                    for (ilJ = 0; ilJ < ilNoEle; ilJ++)
                       strcat(pclDataList," ,");
                    if (strlen(pclDataList) > 0)
                       pclDataList[strlen(pclDataList)-1] = '\0';
                 }
                 else
                    sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
                 ilItemNo = get_item_no(pclFieldList,"FTYP",5) + 1;
                 if (ilItemNo > 0)
                 {
                    get_real_item(pclFtyp,pclDataList,ilItemNo);
                    if (*pclFtyp == 'D')
                    {
                       ilItemNo = get_item_no(pclFieldList,"DES3",5) + 1;
                       if (ilItemNo > 0)
                          ilRC = ReplaceItem(pclFieldList,ilItemNo,"DIVR");
                       ilItemNo = get_item_no(pclFieldList,"DES4",5) + 1;
                       if (ilItemNo > 0)
                          ilRC = RemoveItem(pclFieldList,pclDataList,ilItemNo);
                    }
                    else if (*pclFtyp == 'O')
                    {
                       strcat(pclFieldList,",DIVR");
                       strcat(pclDataList,", ");
                    }
                 }
                 ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclModId,"7800");
                 ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclPriority,"3");
                 ilPriority = atoi(pclPriority);
                 ilModId = atoi(pclModId);
                 dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                 dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
                 dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
                 dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
                 ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                      pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
              }
              else
              {
                 dbg(TRACE,"%s No changed field values for Flight Update found",pclFunc);
              }
           }
        }
     }
     else
     {
        dbg(TRACE,"%s No Fields for Flight Update found",pclFunc);
     }
  }
  dbg(TRACE,"CALLING HANDLE_RESOURCES");
  ilRC = HandleResources("U");

  return ilRC;
} /* End of HandleUpdate */


static int GetKeys(char *pcpFkt)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetKeys:";
  int ilI, ilJ;
  int ilFound, ilFoundJ;
  int ilUrnoRcv;
  int ilStore;
  char pclSection[64];

  strcpy(pclSection,"");
  ilUrnoRcv = FALSE;
  ilFound = FALSE;
  *pcgAdid = '\0';
  igNoKeys = 0;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        ilStore = TRUE;
        if (strcmp(&rgXml.rlLine[ilI].pclName[0],"URNO") == 0 ||
            strcmp(&rgXml.rlLine[ilI].pclType[0],"URNO") == 0)
        {
           ilUrnoRcv = TRUE;
           if (igUseUrno == FALSE)
              ilStore = FALSE;
           ilFoundJ = FALSE;
           for (ilJ = ilI-1; ilJ >= 0 && ilFoundJ == FALSE; ilJ--)
           {
              if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_BGN") == 0)
              {
                 strcpy(pclSection,&rgXml.rlLine[ilJ].pclName[0]);
                 ilFoundJ = TRUE;
              }
           }
        }
        if (strcmp(&rgXml.rlLine[ilI].pclName[0],"ADID") == 0 ||
            strcmp(&rgXml.rlLine[ilI].pclType[0],"ADID") == 0)
        {
           strcpy(pcgAdid,&rgXml.rlLine[ilI].pclData[0]);
           ilFound = TRUE;
        }
        if (ilStore == TRUE)
        {
           strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclName[0]);
           strcpy(&rgKeys[igNoKeys].pclType[0],&rgXml.rlLine[ilI].pclType[0]);
           strcpy(&rgKeys[igNoKeys].pclMethod[0],&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
           strcpy(&rgKeys[igNoKeys].pclData[0],&rgXml.rlLine[ilI].pclData[0]);
           igNoKeys++;
           rgXml.rlLine[ilI].ilPrcFlag = 1;
        }
     }
  }

  if (ilFound == FALSE && strlen(pclSection) > 0)
  {
     if (strlen(pcgArrivalSections) > 0 && strstr(pcgArrivalSections,pclSection) != NULL)
        strcpy(pcgAdid,"A");
     else if (strlen(pcgDepartureSections) > 0 && strstr(pcgDepartureSections,pclSection) != NULL)
        strcpy(pcgAdid,"D");
     else if (strlen(pcgTowingSections) > 0 && strstr(pcgTowingSections,pclSection) != NULL)
        strcpy(pcgAdid,"B");
  }

  if (*pcgAdid != '\0')
  {
     if (*pcgAdid != 'A' && *pcgAdid != 'D' && *pcgAdid != 'B')
     {
        dbg(TRACE,"%s Invalid ADID <%s> received!!!",pclFunc,pcgAdid);
        ilRC = RC_FAIL;
     }
     igNoKeys = 0;
     for (ilI = 0; ilI < igCurXmlLines; ilI++)
     {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           ilStore = TRUE;
           if (strcmp(&rgXml.rlLine[ilI].pclName[0],"URNO") == 0 ||
               strcmp(&rgXml.rlLine[ilI].pclType[0],"URNO") == 0)
           {
              if (igUseUrno == FALSE)
                 ilStore = FALSE;
           }
           if ((*pcgAdid == 'A' || *pcgAdid == 'B') &&
               strcmp(&rgXml.rlLine[ilI].pclFieldA[igIntfIdx][0],"XXXX") == 0)
              ilStore = FALSE;
           if (*pcgAdid == 'D' && strcmp(&rgXml.rlLine[ilI].pclFieldD[igIntfIdx][0],"XXXX") == 0)
              ilStore = FALSE;
           if (ilStore == TRUE)
           {
              strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclName[0]);
              if (*pcgAdid == 'A' || *pcgAdid == 'B')
              {
                 if (strlen(&rgXml.rlLine[ilI].pclFieldA[igIntfIdx][0]) > 0)
                    strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclFieldA[igIntfIdx][0]);
                 else
                    if (strlen(&rgXml.rlLine[ilI].pclFieldA[0][0]) > 0)
                       strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclFieldA[0][0]);
              }
              if (*pcgAdid == 'D')
              {
                 if (strlen(&rgXml.rlLine[ilI].pclFieldD[igIntfIdx][0]) > 0)
                    strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclFieldD[igIntfIdx][0]);
                 else
                    if (strlen(&rgXml.rlLine[ilI].pclFieldD[0][0]) > 0)
                       strcpy(&rgKeys[igNoKeys].pclField[0],&rgXml.rlLine[ilI].pclFieldD[0][0]);
              }
              strcpy(&rgKeys[igNoKeys].pclType[0],&rgXml.rlLine[ilI].pclType[0]);
              strcpy(&rgKeys[igNoKeys].pclMethod[0],&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
              strcpy(&rgKeys[igNoKeys].pclData[0],&rgXml.rlLine[ilI].pclData[0]);
              igNoKeys++;
           }
        }
     }
  }
  else
  {
     if (ilUrnoRcv == FALSE)
     {
        if (strcmp(&rgInterface[igIntfIdx].pclIntfName[0],"ATC") != 0 && *pcpFkt != 'I')
        {
           dbg(TRACE,"%s No ADID and no URNO received!!!",pclFunc);
           /*ilRC = RC_FAIL;*/
        }
     }
  }
  for (ilI = 0; ilI < igNoKeys; ilI++)
  {
     dbg(DEBUG,"%s Key: <%s> <%s> <%s> <%s>",pclFunc,
         &rgKeys[ilI].pclField[0],&rgKeys[ilI].pclType[0],
         &rgKeys[ilI].pclMethod[0],&rgKeys[ilI].pclData[0]);
  }

  return ilRC;
} /* End of GetKeys */


static int ShowKeys()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ShowKeys:";
  int ilI;

  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        dbg(DEBUG,"%s Key: <%s> <%s> <%s> <%s>",pclFunc,
            &rgXml.rlLine[ilI].pclName[0],&rgXml.rlLine[ilI].pclType[0],
            &rgXml.rlLine[ilI].pclMethod[0],&rgXml.rlLine[ilI].pclData[0]);
     }
  }

  return ilRC;
} /* End of ShowKeys */


static int GetData(char *pcpMethod, int ipIdx, char *pcpName)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetData:";
  int ilI;
  int ilJ;
  int ilBegin;

  ilBegin = FALSE;
  igNoData = ipIdx;
  ilJ = 0;
  while (ilJ < igCurXmlLines && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpName) != 0)
     ilJ++;
  for (ilI = ilJ; ilI < igCurXmlLines; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1 &&
         rgXml.rlLine[ilI].ilPrcFlag == 0  && strcmp(&rgXml.rlLine[ilI].pclType[0],"IGNORE") != 0 &&
         strcmp(&rgXml.rlLine[ilI].pclType[0],"LOV") != 0)
     {
        strcpy(&rgData[igNoData].pclField[0],&rgXml.rlLine[ilI].pclName[0]);
        if (*pcgAdid == 'A' || *pcgAdid == 'B' ||*pcgAdid == '\0')
        {
           if (strlen(&rgXml.rlLine[ilI].pclFieldA[igIntfIdx][0]) > 0)
              strcpy(&rgData[igNoData].pclField[0],&rgXml.rlLine[ilI].pclFieldA[igIntfIdx][0]);
        }
        if (*pcgAdid == 'D')
        {
           if (strlen(&rgXml.rlLine[ilI].pclFieldD[igIntfIdx][0]) > 0)
              strcpy(&rgData[igNoData].pclField[0],&rgXml.rlLine[ilI].pclFieldD[igIntfIdx][0]);
        }
        strcpy(&rgData[igNoData].pclType[0],&rgXml.rlLine[ilI].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        if (strcmp(&rgData[igNoData].pclType[0],"TIME") == 0)
           strcat(&rgData[igNoData-1].pclData[0],&rgXml.rlLine[ilI].pclData[0]);
        else if (strcmp(&rgData[igNoData].pclType[0],"SGDT") == 0)
        {
                 memset(&rgData[igNoData].pclData[0],0x00,16);
                 strncpy(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[0],4);
                 strncat(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[5],2);
                 strncat(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[8],2);
                 strncat(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[11],2);
                 strncat(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[14],2);
                 strncat(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[17],2);
        }
        else
           strcpy(&rgData[igNoData].pclData[0],&rgXml.rlLine[ilI].pclData[0]);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        if (strlen(pcpMethod) > 0)
        {
           if (strcmp(&rgData[igNoData].pclMethod[0],pcpMethod) == 0)
           {
              rgXml.rlLine[ilI].ilPrcFlag = 1;
              if (strcmp(&rgData[igNoData].pclType[0],"TIME") != 0)
                 igNoData++;
           }
        }
        else
        {
           rgXml.rlLine[ilI].ilPrcFlag = 1;
           if (strcmp(&rgData[igNoData].pclType[0],"TIME") != 0)
              igNoData++;
        }
     }
     if (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpName) == 0)
     {
        if (ilBegin == FALSE)
           ilBegin = TRUE;
        else
        {
           ilBegin = FALSE;
           if (strcmp(&rgXml.rlLine[ilI+1].pclName[0],pcpName) != 0)
              ilI = igCurXmlLines;
        }
     }
  }
  if (igNoData > ipIdx)
  {
     for (ilI = 0; ilI < igNoData; ilI++)
     {
        if (strcmp(&rgData[ilI].pclType[0],"URNO") == 0)
           strcpy(&rgData[ilI].pclField[0],"URNO");
        if (strcmp(&rgData[ilI].pclType[0],"FLNU") == 0)
           strcpy(&rgData[ilI].pclField[0],"FLNU");
        if (strcmp(&rgData[ilI].pclType[0],"RAID") == 0)
           strcpy(&rgData[ilI].pclField[0],"RAID");
        if (strcmp(&rgData[ilI].pclType[0],"ALCD") == 0)
           strcpy(&rgData[ilI].pclField[0],"ALCD");
        dbg(DEBUG,"%s Data: <%s> <%s> <%s> <%s>",pclFunc,
            &rgData[ilI].pclField[0],&rgData[ilI].pclType[0],
            &rgData[ilI].pclMethod[0],&rgData[ilI].pclData[0]);

     }
  }

  return ilRC;
} /* End of GetData */


static int ShowData()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ShowData:";
  int ilI;

  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        dbg(DEBUG,"%s Data: <%s> <%s> <%s> <%s>",pclFunc,
            &rgXml.rlLine[ilI].pclName[0],&rgXml.rlLine[ilI].pclType[0],
            &rgXml.rlLine[ilI].pclMethod[0],&rgXml.rlLine[ilI].pclData[0]);
     }
  }

  return ilRC;
} /* End of ShowData */


static int ShowAll()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ShowAll:";
  int ilI;

  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     if (rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        dbg(DEBUG,"%s (%d) <%s> <%s> <%s>",pclFunc,
            rgXml.rlLine[ilI].ilLevel,&rgXml.rlLine[ilI].pclTag[0],
            &rgXml.rlLine[ilI].pclName[0],&rgXml.rlLine[ilI].pclData[0]);
     }
  }

  return ilRC;
} /* End of ShowAll */


static int SearchFlight(char *pcpMethod, char *pcpTable)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "SearchFlight:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  int ilI;
  int ilFound;
  char pclUrno[16];
  char pclField[16];
  char pclFieldList[1024];
  char pclSelection[1024];
  char pclTmpBuf[1024];
  int ilNoItems;
  int ilHit;
  int ilItemNo;
  char pclAdid[8];
  char pclFtyp[8];
  char pclOrg3[8];
  char pclDes3[8];
  int ilDone;
  char pclTimeBegin[16];
  char pclTimeEnd[16];
  char pclAirlineCodes[1024];
  char pclAlc2[16];
  char pclAlc3[16];
  int ilNoEle;

  *pclUrno = '\0';
  strcpy(pclField,"URNO");
  ilFound = FALSE;
  for (ilI = 0; ilI < igNoKeys && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgKeys[ilI].pclField[0],"URNO") == 0 || strcmp(&rgKeys[ilI].pclType[0],"URNO") == 0)
     {
        strcpy(pclUrno,&rgKeys[ilI].pclData[0]);
        strcpy(pclField,&rgKeys[ilI].pclField[0]);
        strcpy(pcpMethod,&rgKeys[ilI].pclMethod[0]);
        ilFound = TRUE;
     }
  }
  if (*pclUrno != '\0')
  {
     sprintf(pclSelection,"WHERE %s = %s",pclField,pclUrno);
  }
  else
  {
     for (ilI = 0; ilI < igCurXmlLines; ilI++)
     {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 0 &&
            (rgXml.rlLine[ilI].pclFlag[0] == 'M' || rgXml.rlLine[ilI].pclFlag[0] == 'C'))
        {
           if (strcmp(&rgXml.rlLine[ilI].pclName[0],"URNO") != 0 &&
               strcmp(&rgXml.rlLine[ilI].pclType[0],"URNO") != 0)
           {
              dbg(TRACE,"%s Key Field <%s> missing",pclFunc,&rgXml.rlLine[ilI].pclName[0]);
              sprintf(pcgStatusMsgTxt,"KEY FIELD <%s> MISSING\n",&rgXml.rlLine[ilI].pclName[0]);
              SetStatusMessage(pcgStatusMsgTxt);
              return RC_FAIL;
           }
        }
     }
     strcpy(pcpMethod,&rgKeys[0].pclMethod[0]);
     strcpy(pclSelection,"WHERE ");
     for (ilI = 0; ilI < igNoKeys; ilI++)
     {
        ilDone = FALSE;
        if (strcmp(&rgKeys[ilI].pclType[0],"DATE") == 0)
        {
           sprintf(pclTmpBuf,"%s LIKE '%s%%'",&rgKeys[ilI].pclField[0],&rgKeys[ilI].pclData[0]);
           ilDone = TRUE;
        }
        else
        {
           if (strcmp(&rgKeys[ilI].pclType[0],"DATI") == 0 || strcmp(&rgKeys[ilI].pclType[0],"SGDT") == 0)
           {
              if (strcmp(&rgKeys[ilI].pclType[0],"DATI") == 0)
                 strcpy(pclTimeBegin,&rgKeys[ilI].pclData[0]);
              else if (strcmp(&rgKeys[ilI].pclType[0],"SGDT") == 0)
              {
                 memset(pclTimeBegin,0x00,16);
                 strncpy(pclTimeBegin,&rgKeys[ilI].pclData[0],4);
                 strncat(pclTimeBegin,&rgKeys[ilI].pclData[5],2);
                 strncat(pclTimeBegin,&rgKeys[ilI].pclData[8],2);
                 strncat(pclTimeBegin,&rgKeys[ilI].pclData[11],2);
                 strncat(pclTimeBegin,&rgKeys[ilI].pclData[14],2);
                 strncat(pclTimeBegin,&rgKeys[ilI].pclData[17],2);
              }
              strcpy(pclTimeEnd,pclTimeBegin);
              if (*pcgAdid == 'A')
              {
                 if (igTimeWindowArrBegin != 0 || igTimeWindowArrEnd != 0)
                 {
                    ilRC = MyAddSecondsToCEDATime(pclTimeBegin,igTimeWindowArrBegin*60,1);
                    ilRC = MyAddSecondsToCEDATime(pclTimeEnd,igTimeWindowArrEnd*60,1);
                    sprintf(pclTmpBuf,"%s BETWEEN '%s' AND '%s'",&rgKeys[ilI].pclField[0],pclTimeBegin,pclTimeEnd);
                    ilDone = TRUE;
                 }
                 else if (strcmp(&rgKeys[ilI].pclType[0],"SGDT") == 0)
                 {
                    sprintf(pclTmpBuf,"%s = '%s'",&rgKeys[ilI].pclField[0],pclTimeBegin);
                    ilDone = TRUE;
                 }
              }
              else
              {
                 if (*pcgAdid == 'D')
                 {
                    if (igTimeWindowDepBegin != 0 || igTimeWindowDepEnd != 0)
                    {
                       ilRC = MyAddSecondsToCEDATime(pclTimeBegin,igTimeWindowDepBegin*60,1);
                       ilRC = MyAddSecondsToCEDATime(pclTimeEnd,igTimeWindowDepEnd*60,1);
                       sprintf(pclTmpBuf,"%s BETWEEN '%s' AND '%s'",&rgKeys[ilI].pclField[0],pclTimeBegin,pclTimeEnd);
                       ilDone = TRUE;
                    }
                    else if (strcmp(&rgKeys[ilI].pclType[0],"SGDT") == 0)
                    {
                       sprintf(pclTmpBuf,"%s = '%s'",&rgKeys[ilI].pclField[0],pclTimeBegin);
                       ilDone = TRUE;
                    }
                 }
              }
           }
           else
           {
              if (strcmp(&rgKeys[ilI].pclField[0],"ADID") == 0)
              {
                 if (*pcgAdid == 'D')
                 {
                    sprintf(pclTmpBuf,"ORG3 = '%s'",pcgHomeAp);
                    ilDone = TRUE;
                 }
                 else
                 {
                    sprintf(pclTmpBuf,"DES3 = '%s'",pcgHomeAp);
                    ilDone = TRUE;
                 }
              }
           }
        }
        if (ilDone == FALSE)
           sprintf(pclTmpBuf,"%s = '%s'",&rgKeys[ilI].pclField[0],&rgKeys[ilI].pclData[0]);
        if (strlen(pclSelection) > 6)
           strcat(pclSelection," AND ");
        strcat(pclSelection,pclTmpBuf);
     }
     if (strlen(pcgPosSel) > 0)
     {
        strcat(pclSelection,pcgPosSel);
        if (strstr(pcgPosSel,"PSTA") != NULL)
        {
           if (strstr(pcgPosSel,"STOA") == NULL)
           {
                if(strncmp(pcgBKKCustomization,"YES",3) != 0)
                {
              strcpy(pclTimeBegin,pcgCurrentTime);
              ilRC = MyAddSecondsToCEDATime(pclTimeBegin,30*60*(-1),1);
              strcpy(pclTimeEnd,pcgCurrentTime);
              ilRC = MyAddSecondsToCEDATime(pclTimeEnd,30*60,1);
              sprintf(pclTmpBuf," AND STOA BETWEEN '%s' AND '%s'",pclTimeBegin,pclTimeEnd);
              strcat(pclSelection,pclTmpBuf);
              strcat(pclSelection," AND ADID = 'B'");
           }
        }
        }
        else if (strstr(pcgPosSel,"PSTD") != NULL)
        {
           if (strstr(pcgPosSel,"STOD") == NULL)
           {
                if(strncmp(pcgBKKCustomization,"YES",3) != 0)
                {
              strcpy(pclTimeBegin,pcgCurrentTime);
              ilRC = MyAddSecondsToCEDATime(pclTimeBegin,30*60*(-1),1);
              strcpy(pclTimeEnd,pcgCurrentTime);
              ilRC = MyAddSecondsToCEDATime(pclTimeEnd,30*60,1);
              sprintf(pclTmpBuf," AND STOD BETWEEN '%s' AND '%s'",pclTimeBegin,pclTimeEnd);
              strcat(pclSelection,pclTmpBuf);
              strcat(pclSelection," AND ADID = 'B'");
           }
        }
     }
     }
     else
        strcat(pclSelection," AND FTYP NOT IN ('T','G')");
  }
  ilNoItems = 0;
  strcpy(pclTmpBuf,"");
  for (ilI = 0; ilI < igNoKeys; ilI++)
  {
     if (strcmp(&rgKeys[ilI].pclField[0],"STDT") != 0)
     {
        strcat(pclTmpBuf,&rgKeys[ilI].pclField[0]);
        strcat(pclTmpBuf,",");
        ilNoItems++;
     }
  }
  if (strlen(pclTmpBuf) > 0)
     pclTmpBuf[strlen(pclTmpBuf)-1] = '\0';
  if (strstr(pclTmpBuf,"URNO") == NULL)
  {
     sprintf(pclFieldList,"URNO,%s",pclTmpBuf);
     ilNoItems++;
  }
  else
     strcpy(pclFieldList,pclTmpBuf);
/*
  ilRC = iGetConfigEntry(pcgConfigFile,pcpMethod,"table",CFG_STRING,pcpTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pcpTable,"AFTTAB");
*/
  strcpy(pcpTable,"AFTTAB");
  if (strstr(pclTmpBuf,"ADID") == NULL && strcmp(pcpTable,"AFTTAB") == 0)
  {
     strcat(pclFieldList,",ADID");
     ilNoItems++;
  }
  strcpy(pclAirlineCodes,"");
  sprintf(pclTmpBuf,"ALLOWED_AIRLINE_CODES_FOR_%s",pcgIntfName);
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL",pclTmpBuf,CFG_STRING,pclAirlineCodes);
  if (strlen(pclAirlineCodes) > 0)
  {
     if (strstr(pclFieldList,"ALC2") == NULL)
     {
        strcat(pclFieldList,",ALC2");
        ilNoItems++;
     }
     if (strstr(pclFieldList,"ALC3") == NULL)
     {
        strcat(pclFieldList,",ALC3");
        ilNoItems++;
     }
  }
  if (strstr(pclFieldList,"FTYP") == NULL)
  {
     strcat(pclFieldList,",FTYP");
     ilNoItems++;
  }
  if (strstr(pclFieldList,"ORG3") == NULL)
  {
     strcat(pclFieldList,",ORG3");
     ilNoItems++;
  }
  if (strstr(pclFieldList,"DES3") == NULL)
  {
     strcat(pclFieldList,",DES3");
     ilNoItems++;
  }
  if (strlen(pclSelection) <= 6)
     return RC_FAIL;
  ilRC = RC_SUCCESS;
  ilHit = 0;
  strcpy(pcgUrno,"");
  sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pcpTable,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  while (ilRCdb == DB_SUCCESS)
  {
     ilHit++;
     slFkt = NEXT;
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  }
  close_my_cursor(&slCursor);
  igSearchFlightHits = ilHit;
  if (ilHit == 0)
  {
     dbg(TRACE,"%s No Record Found: <%s>",pclFunc,pclSqlBuf);
     ilRC = RC_FAIL;
  }
  else
  {
     if (ilHit > 1)
     {
        dbg(TRACE,"%s Too Many (%d) Records Found: <%s>",pclFunc,ilHit,pclSqlBuf);
        ilRC = RC_FAIL;
     }
     else
     {
        BuildItemBuffer(pclDataBuf,"",ilNoItems,",");
        ilItemNo = get_item_no(pclFieldList,"URNO",5) + 1;
        get_real_item(pcgUrno,pclDataBuf,ilItemNo);
        dbg(DEBUG,"%s Found Record with URNO <%s>",pclFunc,pcgUrno);
        ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
        get_real_item(pclAdid,pclDataBuf,ilItemNo);
        ilItemNo = get_item_no(pclFieldList,"FTYP",5) + 1;
        get_real_item(pclFtyp,pclDataBuf,ilItemNo);
        ilItemNo = get_item_no(pclFieldList,"ORG3",5) + 1;
        get_real_item(pclOrg3,pclDataBuf,ilItemNo);
        ilItemNo = get_item_no(pclFieldList,"DES3",5) + 1;
        get_real_item(pclDes3,pclDataBuf,ilItemNo);
        dbg(TRACE,"CHECK ADID PCG <%s> PCL <%s>",pcgAdid,pclAdid);
        if (*pcgAdid == '\0')
        {
           strcpy(pcgAdid,pclAdid);
           dbg(DEBUG,"%s ADID = <%s> is now used",pclFunc,pcgAdid);
        }
        else
        {
           if (*pcgAdid != *pclAdid)
           {
              if (*pclFtyp != 'Z' && *pclFtyp != 'B' && *pclFtyp != 'T' && *pclFtyp != 'G')
              {
                 if (strcmp(pclOrg3,pclDes3) != 0)
                 {
                    dbg(TRACE,"%s ADID mismatch, Key: <%s> Rec; <%s>",pclFunc,pcgAdid,pclAdid);
                    *pcgUrno = '\0';
                    ilRC = RC_FAIL;
                 }
              }
           }
        }
        dbg(TRACE,"CHECK AIRLINE CODES <%s>",pclAirlineCodes);
        if (strlen(pclAirlineCodes) > 0)
        {
           ilItemNo = get_item_no(pclFieldList,"ALC2",5) + 1;
           get_real_item(pclAlc2,pclDataBuf,ilItemNo);
           ilItemNo = get_item_no(pclFieldList,"ALC3",5) + 1;
           get_real_item(pclAlc3,pclDataBuf,ilItemNo);
           TrimRight(pclAlc2);
           TrimRight(pclAlc3);
           ilFound = FALSE;
           ilNoEle = GetNoOfElements(pclAirlineCodes,',');
           for (ilI = 1; ilI <= ilNoEle && ilFound == FALSE; ilI++)
           {
              GetDataItem(pclTmpBuf,pclAirlineCodes,ilI,',',""," ");
              TrimRight(pclTmpBuf);
              if (strcmp(pclTmpBuf,pclAlc2) == 0 || strcmp(pclTmpBuf,pclAlc3) == 0)
                 ilFound = TRUE;
           }
           if (ilFound == FALSE)
           {
              dbg(TRACE,"%s Airline <%s>/<%s> not allowed for this Interface <%s>",pclFunc,pclAlc2,pclAlc3,pcgIntfName);
              *pcgUrno = '\0';
              ilRC = RC_FAIL;
           }
        }
     }
  }

  if (ilRC == RC_SUCCESS)
  {
    dbg(TRACE,"FLIGHT RELATED MESSAGE ACCEPTED");
  }
  else
  {
    dbg(TRACE,"FLIGHT NOT FOUND OR MESSAGE REJECTED");
  }

  return ilRC;
} /* End of SearchFlight */


static int SearchTurnAroundFlight(char *pcpMethod)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "SearchTurnAroundFlight:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclSelectBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[2048];
  char pclFieldList[2048];
  char pclDataList[2048];
  char pclFlno[16];
  char pclRegn[16];
  char pclStoa[16];
  char pclTmpTime[16];
  int ilIdx;
  char pclMethod[32];
  char pclTable[16];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;

  ilIdx = GetDataLineIdx("FLNO",0);
  if (ilIdx >= 0)
     strcpy(pclFlno,&rgData[ilIdx].pclData[0]);
  else
     strcpy(pclFlno,"");
  ilIdx = GetDataLineIdx("STOA",0);
  if (ilIdx >= 0)
     strcpy(pclStoa,&rgData[ilIdx].pclData[0]);
  else
     strcpy(pclStoa,"");
  ilIdx = GetDataLineIdx("REGN",0);
  if (ilIdx >= 0)
     strcpy(pclRegn,&rgData[ilIdx].pclData[0]);
  else
     strcpy(pclRegn,"");
  strcpy(pclTmpTime,pclStoa);
  ilRC = MyAddSecondsToCEDATime(pclTmpTime,6*60*60*(-1),1);

  ilRC = RC_FAIL;
  strcpy(pcgUrno,"");
  if (strlen(pclRegn) == 0)
     sprintf(pclSelectBuf,"FLNO LIKE '%s%%' AND STOD BETWEEN '%s' AND '%s' AND ADID = 'B' AND FTYP NOT IN ('T','G')",pclFlno,pclTmpTime,pclStoa);
  else
     sprintf(pclSelectBuf,"FLNO LIKE '%s%%' AND STOD BETWEEN '%s' AND '%s' AND ADID = 'B' AND FTYP NOT IN ('T','G') AND REGN = '%s'",pclFlno,pclTmpTime,pclStoa,pclRegn);
  sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB WHERE %s",pclSelectBuf);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb == DB_SUCCESS)
  {
     strcpy(pcgUrno,pclDataBuf);
     sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
     strcpy(pclFieldList,"STOA");
     strcpy(pclDataList,pclStoa);
     ilRC = iGetConfigEntry(pcgConfigFile,pcpMethod,"snd_cmd_u",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"UFR");
     ilRC = iGetConfigEntry(pcgConfigFile,pcpMethod,"table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"AFTTAB");
     ilRC = iGetConfigEntry(pcgConfigFile,pcpMethod,"mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
       strcpy(pclModId,"7800");
     ilRC = iGetConfigEntry(pcgConfigFile,pcpMethod,"priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                          ilPriority,NETOUT_NO_ACK);
     ilRC = RC_SUCCESS;
  }

  return ilRC;
} /* End of SearchTurnAroundFlight */


static int CheckData(char *pcpData, int ipIdx)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckData:";
  int ilIdx2 = -1;
  int ilDay;
  int ilMin;
  int ilWkDay;
  char pclTmpBuf[1024];
  char pclTable[16];
  char pclField[16];
  int ilCount;
  int ilI;
  char pclApc[8];

  if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"DATI") == 0)
  {
     if (*pcpData != ' ')
     {
        if (strcmp(&rgXml.rlLine[ipIdx].pclBasTab[0],"GET") != 0)
        {
           if (strlen(pcpData) < 9)
              ilRC = RC_FAIL;
           else
              ilRC = GetFullDay(pcpData,&ilDay,&ilMin,&ilWkDay);
        }
        else
        {
           dbg(TRACE,"%s DATE PART Data <%s> for <%s>",pclFunc,pcpData,&rgXml.rlLine[ipIdx].pclName[0]);
           ilRC = RC_SUCCESS;
        }
     }
  }
  else
  {
     if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"DATE") == 0)
     {
        if (*pcpData != ' ')
        {
           sprintf(pclTmpBuf,"%s120000",pcpData);
           ilRC = GetFullDay(pclTmpBuf,&ilDay,&ilMin,&ilWkDay);
        }
     }
     else
     {
        if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"TIME") == 0 ||
            strcmp(&rgXml.rlLine[ipIdx].pclType[0],"TIMS") == 0)
        {
           if (strcmp(&rgXml.rlLine[ipIdx].pclBasTab[0],"PUT") != 0)
           {
              if (*pcpData != ' ')
              {
                 if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"TIMS") == 0 && strlen(pcpData) > 4)
                    ilRC = RC_FAIL;
                 else
                 {
                    sprintf(pclTmpBuf,"20050202%s",pcpData);
                    ilRC = GetFullDay(pclTmpBuf,&ilDay,&ilMin,&ilWkDay);
                 }
              }
           }
           else
           {
              dbg(TRACE,"%s TIME PART Data <%s> for <%s> PUT <%s>",
                     pclFunc,pcpData,&rgXml.rlLine[ipIdx].pclName[0],&rgXml.rlLine[ipIdx].pclBasFld[0]);
              ilIdx2 = ipIdx - 1;
              strcpy(pclTmpBuf,rgXml.rlLine[ilIdx2].pclData);
              strcat(pclTmpBuf,pcpData);
              if (*pclTmpBuf != ' ')
                 strcat(pclTmpBuf,"00");
              else
                 strcpy(pclTmpBuf," ");
              dbg(TRACE,"%s FULL DATE/TIME Data <%s> ",pclFunc,pclTmpBuf);
              strcpy(rgXml.rlLine[ilIdx2].pclData,pclTmpBuf);
              if (*pclTmpBuf != ' ')
                 ilRC = GetFullDay(pclTmpBuf,&ilDay,&ilMin,&ilWkDay);
              rgXml.rlLine[ipIdx].ilRcvFlag = 0;
           }
        }
        else
        {
           if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"FLNO") == 0)
           {
              ilRC = CheckFlightNo(pcpData);
              if (ilRC == RC_SUCCESS)
                 strcpy(&rgXml.rlLine[ipIdx].pclData[0],pcpData);
           }
           else
           {
              if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"BAS") == 0)
              {
                 if (*pcpData != ' ')
                 {
                    strcpy(pclTable,&rgXml.rlLine[ipIdx].pclBasTab[0]);
                    strcpy(pclField,&rgXml.rlLine[ipIdx].pclBasFld[0]);
                    ilCount = 1;
                    ilRC = syslibSearchDbData(pclTable,pclField,pcpData,pclField,pclTmpBuf,&ilCount,"\n");
                 }
              }
              else
              {
                 if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"VIAL") == 0)
                 {
                    if (strlen(pcpData) > 3)
                    {
                       ConvertVialToRout(pcpData);
                       strcpy(&rgXml.rlLine[ipIdx].pclData[0],pcpData);
                    }
                 }
                 else
                 {
                    if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"LOV") == 0)
                    {
                       AddViaInfoToRoute(&rgXml.rlLine[ipIdx].pclFieldA[0][0],pcpData);
                    }
                    else
                    {
                       if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"JFNO") == 0)
                       {
                       }
                       else
                       {
                          if (strcmp(&rgXml.rlLine[ipIdx].pclType[0],"ATYP") == 0)
                          {
                             dbg(TRACE,"FOUND ATYP %s",pcpData);
                             ilIdx2 = GetXmlLineIdx("HDR",pcgActionType,NULL);
                             if (ilIdx2 >= 0)
                             {
                                dbg(TRACE,"COPY ATYP %s TO ACTIONTYPE",pcpData);
                                strcpy(rgXml.rlLine[ilIdx2].pclData,pcpData);
                                rgXml.rlLine[ilIdx2].ilRcvFlag = 1;
                             }
                          }
                       }
                    }
                 }
              }
           }
        }
     }
  }
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Wrong Data <%s> for <%s>",pclFunc,pcpData,&rgXml.rlLine[ipIdx].pclName[0]);
     snap(pcpData,strlen(pcpData),outp);
     sprintf(pcgStatusMsgTxt,"INVALID DATA <%s> OF TAG <%s>\n",pcpData,&rgXml.rlLine[ipIdx].pclName[0]);
     SetStatusMessage(pcgStatusMsgTxt);
  }

  return ilRC;
} /* End of CheckData */


static int CheckFlightNo(char *pcpFlight)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckFlightNo:";
  char pclTmpBuf[100];
  char pclTmpNum[100];
  int ilTmpNum;
  char pclSuffix[4];
  int ilI;
  int ilCount;
  char pclAlc[16];
  char pclResult[16];

  if (*pcpFlight == '\0' || *pcpFlight == ' ')
     return RC_SUCCESS;

  memset(pclTmpBuf,0x00,100);
  *pclSuffix = '\0';
  strncpy(pclTmpBuf,pcpFlight,2);
  if (pcpFlight[2] == ' ')
  {
     pclTmpBuf[2] =  ' ';
     strcpy(pclTmpNum,&pcpFlight[3]);
  }
  else
  {
     if (!isdigit(pcpFlight[2]))
     {
        pclTmpBuf[2] = pcpFlight[2];
        strcpy(pclTmpNum,&pcpFlight[3]);
     }
     else
     {
        pclTmpBuf[2] =  ' ';
        strcpy(pclTmpNum,&pcpFlight[2]);
     }
  }
  if (!isdigit(pcpFlight[strlen(pcpFlight)-1]))
  {
     strcpy(pclSuffix,&pcpFlight[strlen(pcpFlight)-1]);
     /*pcpFlight[strlen(pcpFlight)-1] = '\0';*/
     pclTmpNum[strlen(pclTmpNum)-1] = '\0';
  }
  if (strlen(pclTmpNum) == 0)
  {
     dbg(TRACE,"%s Number Part is empty",pclFunc);
     return RC_FAIL;
  }
  TrimRight(pclTmpNum);
  for (ilI = 0; ilI < strlen(pclTmpNum); ilI++)
  {
     if (!isdigit(pclTmpNum[ilI]))
     {
        dbg(TRACE,"%s Number Part contains Char <%s>",pclFunc,pclTmpNum);
        return RC_FAIL;
     }
  }
  strcpy(pclAlc,pclTmpBuf);
  if (pclAlc[2] == ' ')
     pclAlc[2] = '\0';
  ilCount = 1;
  if (strlen(pclAlc) == 2)
     ilRC = syslibSearchDbData("ALTTAB","ALC2",pclAlc,"URNO",pclResult,&ilCount,"\n");
  else
     ilRC = syslibSearchDbData("ALTTAB","ALC3",pclAlc,"URNO",pclResult,&ilCount,"\n");
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Airline Code <%s> not found in ALTTAB",pclFunc,pclAlc);
     return ilRC;
  }
  ilTmpNum = atoi(pclTmpNum);
  if (ilTmpNum <= 9)
  {
     strcat(pclTmpBuf,"00");
  }
  else
  {
     if (ilTmpNum <= 99)
     {
        strcat(pclTmpBuf,"0");
     }
  }
  sprintf(pclTmpNum,"%d",ilTmpNum);
  strcat(pclTmpBuf,pclTmpNum);
  if (*pclSuffix != '\0')
  {
     if (strlen(pclTmpBuf) == 6)
        strcat(pclTmpBuf,"  ");
     else
        if (strlen(pclTmpBuf) == 7)
           strcat(pclTmpBuf," ");
     strcat(pclTmpBuf,pclSuffix);
  }
  strcpy(pcpFlight,pclTmpBuf);

  return ilRC;
} /* End of CheckFlightNo */


static int DataAddOns()
{
  int ilRC = RC_SUCCESS;
  int ilRCdb = RC_SUCCESS;
  char pclFunc[] = "DataAddOns:";
  int ilRCdbr = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[2048];
  char pclSelection[2048];
  int ilIdx;
  int ilIdx1;
  int ilIdx2;
  int ilIdx3;
  int ilIdx4;
  int ilIdx5;
  int ilCount;
  char pclTmpBuf[1024];
  char pclFlno[10];
  char pclAlc2[10];
  char pclAlc3[10];
  char pclFltn[10];
  char pclFlns[10];

  ilRCdb = RC_FAIL;
  ilIdx1 = GetDataLineIdx("ORG3",0);
  ilIdx2 = GetDataLineIdx("ORG4",0);
  if (ilIdx1 >= 0 || ilIdx2 >= 0)
  {
     ilCount = 1;
     if (ilIdx1 >= 0)
     {
        if (ilIdx2 < 0)
        {
           ilRCdb = syslibSearchDbData("APTTAB","APC3",&rgData[ilIdx1].pclData[0],"APC4",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ORG4");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        }
     }
     else
     {
        if (ilIdx1 < 0)
        {
           ilRCdb = syslibSearchDbData("APTTAB","APC4",&rgData[ilIdx2].pclData[0],"APC3",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ORG3");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx2].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx2].pclMethod[0]);
        }
     }
     if (ilRCdb == RC_SUCCESS)
     {
        strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }

  ilRCdb = RC_FAIL;
  ilIdx1 = GetDataLineIdx("DES3",0);
  ilIdx2 = GetDataLineIdx("DES4",0);
  if (ilIdx1 >= 0 || ilIdx2 >= 0)
  {
     ilCount = 1;
     if (ilIdx1 >= 0)
     {
        if (ilIdx2 < 0)
        {
           ilRCdb = syslibSearchDbData("APTTAB","APC3",&rgData[ilIdx1].pclData[0],"APC4",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"DES4");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        }
     }
     else
     {
        if (ilIdx1 < 0)
        {
           ilRCdb = syslibSearchDbData("APTTAB","APC4",&rgData[ilIdx2].pclData[0],"APC3",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"DES3");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx2].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx2].pclMethod[0]);
        }
     }
     if (ilRCdb == RC_SUCCESS)
     {
        strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }

/*
  ilRCdb = RC_FAIL;
  ilIdx1 = GetDataLineIdx("ACT3",0);
  ilIdx2 = GetDataLineIdx("ACT5",0);
  if (ilIdx1 >= 0 || ilIdx2 >= 0)
  {
     ilCount = 1;
     if (ilIdx1 >= 0)
     {
        if (ilIdx2 < 0)
        {
           ilRCdb = syslibSearchDbData("ACTTAB","ACT3",&rgData[ilIdx1].pclData[0],"ACT5",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ACT5");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        }
     }
     else
     {
        if (ilIdx1 < 0)
        {
           ilRCdb = syslibSearchDbData("ACTTAB","ACT5",&rgData[ilIdx2].pclData[0],"ACT3",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ACT3");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx2].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx2].pclMethod[0]);
        }
     }
     if (ilRCdb == RC_SUCCESS)
     {
        strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }
*/

  ilRCdb = RC_FAIL;
  ilIdx1 = GetDataLineIdx("ALC2",0);
  ilIdx2 = GetDataLineIdx("ALC3",0);
  if (ilIdx1 >= 0 || ilIdx2 >= 0)
  {
     ilCount = 1;
     if (ilIdx1 >= 0)
     {
        if (ilIdx2 < 0)
        {
           ilRCdb = syslibSearchDbData("ALTTAB","ALC2",&rgData[ilIdx1].pclData[0],"ALC3",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ALC3");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        }
     }
     else
     {
        if (ilIdx1 < 0)
        {
           ilRCdb = syslibSearchDbData("ALTTAB","ALC3",&rgData[ilIdx2].pclData[0],"ALC2",pclTmpBuf,&ilCount,"\n");
           strcpy(&rgData[igNoData].pclField[0],"ALC2");
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx2].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx2].pclMethod[0]);
        }
     }
     if (ilRCdb == RC_SUCCESS)
     {
        strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }

  ilRCdb = RC_FAIL;
  ilIdx1 = GetDataLineIdx("FLNO",0);
  ilIdx2 = GetDataLineIdx("ALC2",0);
  ilIdx3 = GetDataLineIdx("ALC3",0);
  ilIdx4 = GetDataLineIdx("FLTN",0);
  ilIdx5 = GetDataLineIdx("FLNS",0);
  if (ilIdx1 >= 0)
  {
     ilCount = 1;
     strncpy(pclAlc3,&rgData[ilIdx1].pclData[0],3);
     pclAlc3[3] = '\0';
     if (pclAlc3[2] == ' ')
     {
        strncpy(pclAlc2,pclAlc3,2);
        pclAlc2[2] = '\0';
        ilRCdb = syslibSearchDbData("ALTTAB","ALC2",pclAlc2,"ALC3",pclAlc3,&ilCount,"\n");
     }
     else
     {
        ilRCdb = syslibSearchDbData("ALTTAB","ALC3",pclAlc3,"ALC2",pclAlc2,&ilCount,"\n");
     }
     strcpy(pclFltn,&rgData[ilIdx1].pclData[3]);
     if (strlen(pclFltn) > 5)
     {
        strcpy(pclFlns,&pclFltn[5]);
        if (pclFltn[3] == ' ')
           pclFltn[3] = '\0';
        else if (pclFltn[4] == ' ')
           pclFltn[4] = '\0';
        else
           pclFltn[5] = '\0';
     }
     else
     {
        strcpy(pclFlns,"");
     }
     if (ilIdx2 < 0)
     {
        strcpy(&rgData[igNoData].pclField[0],"ALC2");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclAlc2);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
     if (ilIdx3 < 0)
     {
        strcpy(&rgData[igNoData].pclField[0],"ALC3");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclAlc3);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
     if (ilIdx4 < 0)
     {
        strcpy(&rgData[igNoData].pclField[0],"FLTN");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclFltn);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
     if (ilIdx5 < 0 && strlen(pclFlns) > 0)
     {
        strcpy(&rgData[igNoData].pclField[0],"FLNS");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclFlns);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }
  else
  {
     if (ilIdx2 >= 0 || ilIdx3 >= 0 || ilIdx4 >= 0 || ilIdx5 >= 0)
     {
        sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
        sprintf(pclSqlBuf,"SELECT FLNO FROM AFTTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdbr = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        TrimRight(pclDataBuf);
        strcpy(pclFlno,"");
        if (ilIdx2 >= 0)
        {
           if (strlen(&rgData[ilIdx2].pclData[0]) > 0)
           {
              strcpy(pclFlno,&rgData[ilIdx2].pclData[0]);
              strcat(pclFlno," ");
           }
        }
        else if (ilIdx3 >= 0)
        {
           strcpy(pclFlno,&rgData[ilIdx3].pclData[0]);
        }
        if (strlen(pclFlno) == 0)
        {
           strcpy(pclFlno,pclDataBuf);
           pclFlno[3] = '\0';
        }
        if (ilIdx4 >= 0)
           strcat(pclFlno,&rgData[ilIdx4].pclData[0]);
        else
           strcat(pclFlno,&pclDataBuf[3]);
        if (strlen(&rgData[ilIdx5].pclData[0]) > 0)
        {
           if (strlen(pclFlno) == 6)
              strcat(pclFlno,"  ");
           else if (strlen(pclFlno) == 7)
              strcat(pclFlno," ");
           if (strlen(pclFlno) == 9)
              pclFlno[8] = '\0';
           strcat(pclFlno,&rgData[ilIdx5].pclData[0]);
        }
        if (ilIdx2 >= 0)
           ilIdx = ilIdx2;
        else if (ilIdx3 >= 0)
           ilIdx = ilIdx3;
        else if (ilIdx4 >= 0)
           ilIdx = ilIdx4;
        else
           ilIdx = ilIdx5;
        strcpy(&rgData[igNoData].pclField[0],"FLNO");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclFlno);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
     }
  }

  return ilRC;
} /* End of DataAddOns */


static int GetDataLineIdx(char *pcpField, int ipIdx)
{
  int ilRC = -1;
  char pclFunc[] = "GetDataLineIdx:";
  int ilI;
  int ilFound;

  ilFound = FALSE;
  for (ilI = ipIdx; ilI < igNoData && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgData[ilI].pclField[0],pcpField) == 0 || strcmp(&rgData[ilI].pclType[0],pcpField) == 0)
     {
        ilRC = ilI;
        ilFound = TRUE;
     }
  }

  return ilRC;
} /* End of GetDataLineIdx */


static int ValidateAdid()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ValidateAdid:";
  int ilIdx1;
  int ilIdx2;
  char pclAdid[16];
  char pclTmpBuf[16];
  char pclFldNam[8];
  int ilCount;
  char pclOrg3[16];
  char pclDes3[16];

  ilIdx1 = GetDataLineIdx("ADID",0);
  if (ilIdx1 >= 0)
  {
     strcpy(pclAdid,&rgData[ilIdx1].pclData[0]);
     if (*pclAdid == 'A')
        strcpy(pclFldNam,"DES3");
     else if (*pclAdid == 'D')
        strcpy(pclFldNam,"ORG3");
     else
        ilRC = RC_FAIL;
     if (ilRC != RC_FAIL)
     {
        ilIdx2 = GetDataLineIdx(pclFldNam,0);
        if (ilIdx2 < 0)
        {
           strcpy(&rgData[igNoData].pclField[0],pclFldNam);
           strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
           strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
           strcpy(&rgData[igNoData].pclData[0],pcgHomeAp);
           strcpy(&rgData[igNoData].pclOldData[0],"");
           igNoData++;
           ilCount = 1;
           ilRC = syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pclTmpBuf,&ilCount,"\n");
           if (ilRC == RC_SUCCESS)
           {
              if (*pclAdid == 'A')
                 strcpy(&rgData[igNoData].pclField[0],"DES4");
              else
                 strcpy(&rgData[igNoData].pclField[0],"ORG4");
              strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
              strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
              strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
              strcpy(&rgData[igNoData].pclOldData[0],"");
              igNoData++;
           }
        }
        else
        {
           if (strcmp(&rgData[ilIdx2].pclData[0],pcgHomeAp) != 0)
           {
              dbg(TRACE,"%s ADID and Airport Data mismatch",pclFunc);
              ilRC = RC_FAIL;
           }
        }
     }
  }
  else
  {
     strcpy(pclAdid,"");
     ilIdx1 = GetDataLineIdx("ORG3",0);
     if (ilIdx1 >= 0)
     {
        strcpy(pclOrg3,&rgData[ilIdx1].pclData[0]);
        if (strcmp(pclOrg3,pcgHomeAp) == 0)
           strcpy(pclAdid,"D");
        else
        {
           ilIdx2 = GetDataLineIdx("DES3",0);
           if (ilIdx2 < 0)
           {
              strcpy(pclAdid,"A");
              strcpy(&rgData[igNoData].pclField[0],"DES3");
              strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
              strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
              strcpy(&rgData[igNoData].pclData[0],pcgHomeAp);
              strcpy(&rgData[igNoData].pclOldData[0],"");
              igNoData++;
              ilCount = 1;
              ilRC = syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pclTmpBuf,&ilCount,"\n");
              if (ilRC == RC_SUCCESS)
              {
                 strcpy(&rgData[igNoData].pclField[0],"DES4");
                 strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
                 strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
                 strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
                 strcpy(&rgData[igNoData].pclOldData[0],"");
                 igNoData++;
              }
           }
           else
           {
              if (strcmp(&rgData[ilIdx2].pclData[0],pcgHomeAp) == 0)
                 strcpy(pclAdid,"A");
           }
        }
     }
     else
     {
        ilIdx1 = GetDataLineIdx("DES3",0);
        if (ilIdx1 >= 0)
        {
           strcpy(pclDes3,&rgData[ilIdx1].pclData[0]);
           if (strcmp(pclDes3,pcgHomeAp) == 0)
              strcpy(pclAdid,"A");
           else
           {
              strcpy(pclAdid,"D");
              strcpy(&rgData[igNoData].pclField[0],"ORG3");
              strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
              strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
              strcpy(&rgData[igNoData].pclData[0],pcgHomeAp);
              strcpy(&rgData[igNoData].pclOldData[0],"");
              igNoData++;
              ilCount = 1;
              ilRC = syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pclTmpBuf,&ilCount,"\n");
              if (ilRC == RC_SUCCESS)
              {
                 strcpy(&rgData[igNoData].pclField[0],"ORG4");
                 strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
                 strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
                 strcpy(&rgData[igNoData].pclData[0],pclTmpBuf);
                 strcpy(&rgData[igNoData].pclOldData[0],"");
                 igNoData++;
              }
           }
        }
     }
     if (*pclAdid != '\0')
     {
        strcpy(&rgData[igNoData].pclField[0],"ADID");
        strcpy(&rgData[igNoData].pclType[0],&rgData[ilIdx1].pclType[0]);
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilIdx1].pclMethod[0]);
        strcpy(&rgData[igNoData].pclData[0],pclAdid);
        strcpy(&rgData[igNoData].pclOldData[0],"");
        igNoData++;
        strcpy(pcgAdid,pclAdid);
     }
     else
     {
        dbg(TRACE,"%s ADID is missing and can't be calculated",pclFunc);
        ilRC = RC_FAIL;
     }
  }

  return ilRC;
} /* End of ValidateAdid */


static int AdjustData()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "AdjustData:";
  int ilI;
  int ilJ;
  int ilK;
  int ilFound;
  int ilFound2;
  char pclField[16];

  for (ilI = 0; ilI < igNoData; ilI++)
  {
     strcpy(pclField,&rgData[ilI].pclField[0]);
     ilFound = FALSE;
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if ((strcmp(&rgXml.rlLine[ilJ].pclTag[0],"DAT") == 0 || strcmp(&rgXml.rlLine[ilJ].pclTag[0],"KEY") == 0) &&
            strcmp(&rgXml.rlLine[ilJ].pclName[0],pclField) == 0 &&
            rgXml.rlLine[ilJ].ilRcvFlag == 1 && rgXml.rlLine[ilJ].ilPrcFlag == 1)
        {
           ilFound = TRUE;
           strcpy(&rgData[ilI].pclField[0],&rgXml.rlLine[ilJ].pclName[0]);
           if (*pcgAdid == 'A')
           {
              if (strlen(&rgXml.rlLine[ilJ].pclFieldA[igIntfIdx][0]) > 0)
                 strcpy(&rgData[ilI].pclField[0],&rgXml.rlLine[ilJ].pclFieldA[igIntfIdx][0]);
              else
                 if (strlen(&rgXml.rlLine[ilJ].pclFieldA[0][0]) > 0)
                    strcpy(&rgData[ilI].pclField[0],&rgXml.rlLine[ilJ].pclFieldA[0][0]);
           }
           if (*pcgAdid == 'D')
           {
              if (strlen(&rgXml.rlLine[ilJ].pclFieldD[igIntfIdx][0]) > 0)
                 strcpy(&rgData[ilI].pclField[0],&rgXml.rlLine[ilJ].pclFieldD[igIntfIdx][0]);
              else
                 if (strlen(&rgXml.rlLine[ilJ].pclFieldD[0][0]) > 0)
                    strcpy(&rgData[ilI].pclField[0],&rgXml.rlLine[ilJ].pclFieldD[0][0]);
           }
           ilFound2 = FALSE;
           for (ilK = 0; ilK < ilI && ilFound2 == FALSE; ilK++)
           {
              if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0 && strcmp(&rgData[ilI].pclField[0],&rgData[ilK].pclField[0]) == 0)
              {
                 if (strcmp(&rgData[ilI].pclData[0],&rgData[ilK].pclData[0]) == 0)
                    strcpy(&rgData[ilI].pclField[0],"XXXX");
                 else
                 {
                    dbg(TRACE,"%s Duplicate Data mismatch, Field = <%s>",pclFunc,&rgData[ilI].pclField[0]);
                    ilRC = RC_FAIL;
                 }
                 ilFound2 = TRUE;
              }
           }
        }
     }
  }
  for (ilI = 0; ilI < igNoData; ilI++)
  {
     dbg(DEBUG,"%s Data: <%s> <%s> <%s> <%s>",pclFunc,
         &rgData[ilI].pclField[0],&rgData[ilI].pclType[0],
         &rgData[ilI].pclMethod[0],&rgData[ilI].pclData[0]);
  }

  return ilRC;
} /* End of AdjustData */


static int HandleResources(char *pcpFkt)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleResources:";
  char pclMethod[32];
  char pclTable[32];

  dbg(TRACE,"IDENTIFIED URNO <%s> NO_KEYS = %d",pcgUrno,igNoKeys);

  if (*pcgUrno == '\0' && igNoKeys > 0)
  {
     dbg(TRACE,"CALLING SEARCH_FLIGHT");
     ilRC = SearchFlight(pclMethod,pclTable);
  }

  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"FLIGHT NOT FOUND (EXIT FUNCTION)");
     return ilRC;
  }

  dbg(TRACE,"CALLING HANDLE_FLIGHT_RELATED_DATA");
  ilRC = HandleFlightRelatedData(pcpFkt);

  dbg(TRACE,"NOW CHECK PA/PD/BB/GA/GD/WR/EX/DC/CC AND BLKTAB");

  ilRC = HandleLocation(pcpFkt,"PA",1);
  ilRC = HandleLocation(pcpFkt,"PD",1);
  ilRC = HandleLocation(pcpFkt,"BB",2);
  ilRC = HandleLocation(pcpFkt,"GA",2);
  ilRC = HandleLocation(pcpFkt,"GD",2);
  ilRC = HandleLocation(pcpFkt,"WR",2);
  ilRC = HandleLocation(pcpFkt,"EX",2);
  ilRC = HandleCounter(pcpFkt,"DC");
  ilRC = HandleCounter(pcpFkt,"CC");
  ilRC = HandleBlkTab(pcpFkt);

  dbg(TRACE,"NOTHING MORE TO DO ?");
  return ilRC;
} /* End of HandleResources */


static int HandleLocation(char *pcpFkt, char *pcpType, int ipNo)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleLocation:";
  int ilRCdb1 = DB_SUCCESS;
  int ilRCdb2 = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf1[8000];
  char pclDataBuf2[8000];
  char pclTmpBuf[1024];
  char pclName[1024];
  char pclMethod[32];
  int ilI;
  int ilFound;
  int ilRaid1;
  int ilRaid2;
  char pclRaid1[16] = "";
  char pclLoca1[16] = "";
  char pclSbgn1[16] = "";
  char pclSend1[16] = "";
  char pclAbgn1[16] = "";
  char pclAend1[16] = "";
  char pclRaid2[16] = "";
  char pclLoca2[16] = "";
  char pclSbgn2[16] = "";
  char pclSend2[16] = "";
  char pclAbgn2[16] = "";
  char pclAend2[16] = "";
  char pclFld1L1[8] = "";
  char pclFld2L1[8] = "";
  char pclFld3L1[8] = "";
  char pclFld4L1[8] = "";
  char pclFld5L1[8] = "";
  char pclFld1L2[8] = "";
  char pclFld2L2[8] = "";
  char pclFld3L2[8] = "";
  char pclFld4L2[8] = "";
  char pclFld5L2[8] = "";
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclRarSelection[1024];
  char pclRarFieldList[1024];
  char pclLocaOld1[16] = "";
  char pclLocaOld2[16] = "";
  char pclRarUrno[16] = "";
  char pclAftLoca1[16] = "";
  char pclAftSbgn1[16] = "";
  char pclAftSend1[16] = "";
  char pclAftAbgn1[16] = "";
  char pclAftAend1[16] = "";
  char pclAftLoca2[16] = "";
  char pclAftSbgn2[16] = "";
  char pclAftSend2[16] = "";
  char pclAftAbgn2[16] = "";
  char pclAftAend2[16] = "";
  char pclTable[16] = "";
  char pclCommand[16] = "";
  char pclModId[16] = "";
  char pclPriority[16] = "";
  int ilPriority;
  int ilModId;
  int ilUseAft;
  int ilNoItems;
  int ilRaidNotFound;
  char pclStat[16];
  char pclStat1[16];
  char pclStat2[16];
  char pclFkt[16];
  char pclDoNotUpd[2048];
  int ilNoFlds;
  int ilFldI;
  char pclFldNam[8];

  if (strcmp(pcpType,"PA") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","POS_A_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"PSTA");
     strcpy(pclFld2L1,"PABS");
     strcpy(pclFld3L1,"PAES");
     strcpy(pclFld4L1,"PABA");
     strcpy(pclFld5L1,"PAEA");
     strcpy(pclFld1L2,"");
     strcpy(pclFld2L2,"");
     strcpy(pclFld3L2,"");
     strcpy(pclFld4L2,"");
     strcpy(pclFld5L2,"");
  }
  else if (strcmp(pcpType,"PD") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","POS_D_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"PSTD");
     strcpy(pclFld2L1,"PDBS");
     strcpy(pclFld3L1,"PDES");
     strcpy(pclFld4L1,"PDBA");
     strcpy(pclFld5L1,"PDEA");
     strcpy(pclFld1L2,"");
     strcpy(pclFld2L2,"");
     strcpy(pclFld3L2,"");
     strcpy(pclFld4L2,"");
     strcpy(pclFld5L2,"");
  }
  else if (strcmp(pcpType,"BB") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BELT_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"BLT1");
     strcpy(pclFld2L1,"B1BS");
     strcpy(pclFld3L1,"B1ES");
     strcpy(pclFld4L1,"B1BA");
     strcpy(pclFld5L1,"B1EA");
     strcpy(pclFld1L2,"BLT2");
     strcpy(pclFld2L2,"B2BS");
     strcpy(pclFld3L2,"B2ES");
     strcpy(pclFld4L2,"B2BA");
     strcpy(pclFld5L2,"B2EA");
  }
  else if (strcmp(pcpType,"GA") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GATE_A_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"GTA1");
     strcpy(pclFld2L1,"GA1B");
     strcpy(pclFld3L1,"GA1E");
     strcpy(pclFld4L1,"GA1X");
     strcpy(pclFld5L1,"GA1Y");
     strcpy(pclFld1L2,"GTA2");
     strcpy(pclFld2L2,"GA2B");
     strcpy(pclFld3L2,"GA2E");
     strcpy(pclFld4L2,"GA2X");
     strcpy(pclFld5L2,"GA2Y");
  }
  else if (strcmp(pcpType,"GD") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GATE_D_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"GTD1");
     strcpy(pclFld2L1,"GD1B");
     strcpy(pclFld3L1,"GD1E");
     strcpy(pclFld4L1,"GD1X");
     strcpy(pclFld5L1,"GD1Y");
     strcpy(pclFld1L2,"GTD2");
     strcpy(pclFld2L2,"GD2B");
     strcpy(pclFld3L2,"GD2E");
     strcpy(pclFld4L2,"GD2X");
     strcpy(pclFld5L2,"GD2Y");
  }
  else if (strcmp(pcpType,"WR") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","WRO_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"WRO1");
     strcpy(pclFld2L1,"GD1B");
     strcpy(pclFld3L1,"GD1E");
     strcpy(pclFld4L1,"GD1X");
     strcpy(pclFld5L1,"GD1Y");
     strcpy(pclFld1L2,"WRO2");
     strcpy(pclFld2L2,"");
     strcpy(pclFld3L2,"");
     strcpy(pclFld4L2,"");
     strcpy(pclFld5L2,"");
  }
  else if (strcmp(pcpType,"EX") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","EXIT_RES",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcpy(pclFld1L1,"EXT1");
     strcpy(pclFld2L1,"B1BS");
     strcpy(pclFld3L1,"B1ES");
     strcpy(pclFld4L1,"B1BA");
     strcpy(pclFld5L1,"B1EA");
     strcpy(pclFld1L2,"EXT2");
     strcpy(pclFld2L2,"");
     strcpy(pclFld3L2,"");
     strcpy(pclFld4L2,"");
     strcpy(pclFld5L2,"");
  }
  else
  {
     dbg(TRACE,"%s Invalid location <%s>",pclFunc,pcpType);
     ilRC = RC_FAIL;
  }
  if (ilRC != RC_SUCCESS)
     return ilRC;
  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0  && strstr(pclName,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  if (strlen(pclMethod) == 0)
     return ilRC;
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_U",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Update of Flight not allowed for this interface",pclFunc);
     return ilRC;
  }
  strcpy(pclFkt,pcpFkt);
  ilRC = GetData(pclMethod,0,pclName);
  if (igNoData > 0)
  {
     ilRaid1 = GetDataLineIdx("RAID",0);
     ilRaid2 = GetDataLineIdx("RAID",ilRaid1+1);
     ilRC = GetLocaData(ilRaid1,ilRaid2,
                        pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1,
                        pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2,
                        pclRaid1,pclLoca1,pclSbgn1,pclSend1,pclAbgn1,pclAend1,
                        pclRaid2,pclLoca2,pclSbgn2,pclSend2,pclAbgn2,pclAend2);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     if (*pclFkt == 'U')
        if ((*pclLoca1 == ' ' || *pclLoca1 == '\0') && (*pclLoca2 == ' ' || *pclLoca2 == '\0'))
           strcpy(pclFkt,"D");
     if (ilRaid1 < 0)
     {
        dbg(TRACE,"%s RAID not found",pclFunc);
        ilRaidNotFound = TRUE;
/* can't be used , if location data are always in first group
        if (*pclFkt == 'I')
           strcpy(pclFkt,"U");
*/
        if (*pclFkt == 'D' || *pclFkt == 'U')
        {
           if (*pclLoca1 != ' ')
              strcpy(pclLocaOld1,pclLoca1);
           if (*pclLoca2 != ' ')
              strcpy(pclLocaOld1,pclLoca2);
        }
        /*return RC_FAIL;*/
     }
     else
     {
        ilRaidNotFound = FALSE;
        sprintf(pclFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO,STAT");
        /*sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",pclRaid1,pcpType);*/
        sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s'",pclRaid1,pcpType);
        sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
        close_my_cursor(&slCursor);
        if (ilRaid2 > 0)
        {
           /*sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",pclRaid2,pcpType);*/
           sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s'",pclRaid2,pcpType);
           sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb2 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf2);
           close_my_cursor(&slCursor);
        }
        if (*pclFkt == 'I')
        {
           if (ilRCdb1 == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf1,"",8,",");
              get_real_item(pclStat,pclDataBuf1,8);
              TrimRight(pclStat);
              if (*pclStat == ' ')
              {
                 dbg(TRACE,"%s RAID = <%s> exists already",pclFunc,pclRaid1);
                 return ilRC;
              }
           }
           if (ilRaid2 > 0 && ilRCdb2 == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf2,"",8,",");
              get_real_item(pclStat,pclDataBuf2,8);
              TrimRight(pclStat);
              if (*pclStat == ' ')
              {
                 dbg(TRACE,"%s RAID = <%s> exists already",pclFunc,pclRaid2);
                 return ilRC;
              }
           }
           ilRC = GetNextUrno();
           sprintf(pclDataList,"%d,'%s','%s','%s','%s',%s,'%s',' '",
                   igLastUrno,pclRaid1,pclLoca1,pcpType,pcgIntfName,pcgUrno,pcgHomeAp);
           sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
           if (ilRCdb1 == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
           if (ilRaid2 > 0)
           {
              ilRC = GetNextUrno();
              sprintf(pclDataList,"%d,'%s','%s','%s','%s',%s,'%s'",
                      igLastUrno,pclRaid2,pclLoca2,pcpType,pcgIntfName,pcgUrno,pcgHomeAp);
              sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb2 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf2);
              if (ilRCdb2 == DB_SUCCESS)
                 commit_work();
              else
              {
                 dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
                 return ilRC;
              }
              close_my_cursor(&slCursor);
           }
           if (*pclLoca1 == ' ')
           {
              dbg(TRACE,"%s First Location Name is blank",pclFunc);
              return ilRC;
           }
           if (ilRaid2 > 0 && *pclLoca2 == ' ')
           {
              dbg(TRACE,"%s Second Location Name is blank",pclFunc);
              return ilRC;
           }
        }
        else
        {
           if (ilRCdb1 != DB_SUCCESS)
           {
              dbg(TRACE,"%s RAID = <%s> does not exist",pclFunc,pclRaid1);
              return ilRC;
           }
           if (ilRaid2 > 0 && ilRCdb2 != DB_SUCCESS)
           {
              dbg(TRACE,"%s RAID = <%s> does not exist",pclFunc,pclRaid2);
              return ilRC;
           }
           BuildItemBuffer(pclDataBuf1,"",8,",");
           get_real_item(pclLocaOld1,pclDataBuf1,3);
           TrimRight(pclLocaOld1);
           get_real_item(pclStat1,pclDataBuf1,8);
           TrimRight(pclStat1);
           BuildItemBuffer(pclDataBuf2,"",8,",");
           get_real_item(pclLocaOld2,pclDataBuf2,3);
           TrimRight(pclLocaOld2);
           get_real_item(pclStat2,pclDataBuf1,8);
           TrimRight(pclStat2);
           if (*pclFkt == 'U')
           {
              /*if (strcmp(pclLocaOld1,pclLoca1) != 0 && *pclLoca1 != ' ')*/
              if (strcmp(pclLocaOld1,pclLoca1) != 0 || *pclStat1 != ' ')
              {
                 sprintf(pclDataList,"RNAM = '%s'",pclLoca1);
                 get_real_item(pclRarUrno,pclDataBuf1,1);
                 sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
                 sprintf(pclSqlBuf,"UPDATE RARTAB SET %s,STAT=' ',TIME=' ' %s",pclDataList,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
                 if (ilRCdb1 == DB_SUCCESS)
                    commit_work();
                 else
                 {
                    dbg(TRACE,"%s Error updating RARTAB",pclFunc);
                    close_my_cursor(&slCursor);
                    return ilRC;
                 }
                 close_my_cursor(&slCursor);
              }
              if (ilRaid2 > 0)
              {
                 /*if (strcmp(pclLocaOld2,pclLoca2) != 0 && *pclLoca2 != ' ')*/
                 if (strcmp(pclLocaOld2,pclLoca2) != 0 || *pclStat2 != ' ')
                 {
                    sprintf(pclDataList,"RNAM = '%s'",pclLoca2);
                    get_real_item(pclRarUrno,pclDataBuf2,1);
                    sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
                    sprintf(pclSqlBuf,"UPDATE RARTAB SET %s,STAT=' ',TIME=' ' %s",pclDataList,pclSelection);
                    slCursor = 0;
                    slFkt = START;
                    dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                    ilRCdb2 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf2);
                    if (ilRCdb1 == DB_SUCCESS)
                       commit_work();
                    else
                    {
                       dbg(TRACE,"%s Error updating RARTAB",pclFunc);
                       close_my_cursor(&slCursor);
                       return ilRC;
                    }
                    close_my_cursor(&slCursor);
                 }
              }
           }
           else
           {
              get_real_item(pclRarUrno,pclDataBuf1,1);
              sprintf(pclDataList,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
              sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
              sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
              if (ilRCdb1 == DB_SUCCESS)
                 commit_work();
              else
              {
                 dbg(TRACE,"%s Error deleting from RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
                 return ilRC;
              }
              close_my_cursor(&slCursor);
              if (ilRaid2 > 0)
              {
                 get_real_item(pclRarUrno,pclDataBuf2,1);
                 sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
                 sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb2 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf2);
                 if (ilRCdb2 == DB_SUCCESS)
                    commit_work();
                 else
                 {
                    dbg(TRACE,"%s Error deleting from RARTAB",pclFunc);
                    close_my_cursor(&slCursor);
                    return ilRC;
                 }
                 close_my_cursor(&slCursor);
              }
           }
        }
     }
     ilNoItems = 0;
     strcpy(pclFieldList,"");
     if (strlen(pclFld1L1) > 0)
     {
        strcat(pclFieldList,pclFld1L1);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld2L1) > 0)
     {
        strcat(pclFieldList,pclFld2L1);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld3L1) > 0)
     {
        strcat(pclFieldList,pclFld3L1);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld4L1) > 0)
     {
        strcat(pclFieldList,pclFld4L1);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld5L1) > 0)
     {
        strcat(pclFieldList,pclFld5L1);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld1L2) > 0)
     {
        strcat(pclFieldList,pclFld1L2);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld2L2) > 0)
     {
        strcat(pclFieldList,pclFld2L2);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld3L2) > 0)
     {
        strcat(pclFieldList,pclFld3L2);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld4L2) > 0)
     {
        strcat(pclFieldList,pclFld4L2);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFld5L2) > 0)
     {
        strcat(pclFieldList,pclFld5L2);
        strcat(pclFieldList,",");
        ilNoItems++;
     }
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
     sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
     close_my_cursor(&slCursor);
     BuildItemBuffer(pclDataBuf1,"",ilNoItems,",");
     if (ilNoItems >= 1)
        get_real_item(pclAftLoca1,pclDataBuf1,1);
     TrimRight(pclAftLoca1);
     if (ilNoItems >= 2)
        get_real_item(pclAftSbgn1,pclDataBuf1,2);
     TrimRight(pclAftSbgn1);
     if (ilNoItems >= 3)
        get_real_item(pclAftSend1,pclDataBuf1,3);
     TrimRight(pclAftSend1);
     if (ilNoItems >= 4)
        get_real_item(pclAftAbgn1,pclDataBuf1,4);
     TrimRight(pclAftAbgn1);
     if (ilNoItems >= 5)
        get_real_item(pclAftAend1,pclDataBuf1,5);
     TrimRight(pclAftAend1);
     if (ilNoItems >= 6)
        get_real_item(pclAftLoca2,pclDataBuf1,6);
     TrimRight(pclAftLoca2);
     if (ilNoItems >= 7)
        get_real_item(pclAftSbgn2,pclDataBuf1,7);
     TrimRight(pclAftSbgn2);
     if (ilNoItems >= 8)
        get_real_item(pclAftSend2,pclDataBuf1,8);
     TrimRight(pclAftSend2);
     if (ilNoItems >= 9)
        get_real_item(pclAftAbgn2,pclDataBuf1,9);
     TrimRight(pclAftAbgn2);
     if (ilNoItems >= 10)
        get_real_item(pclAftAend2,pclDataBuf1,10);
     TrimRight(pclAftAend2);
    ilUseAft = 0;
    ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"DO_NOT_UPD",CFG_STRING,pclDoNotUpd);
    if (ilRC != RC_SUCCESS)
       strcpy(pclDoNotUpd,"");
    ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
    if (ilRC != RC_SUCCESS)
       strcpy(pclTable,"AFTTAB");
    ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
    if (ilRC != RC_SUCCESS)
       strcpy(pclModId,"7800");
    ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
    if (ilRC != RC_SUCCESS)
       strcpy(pclPriority,"3");
    ilPriority = atoi(pclPriority);
    ilModId = atoi(pclModId);
    strcpy(pclFieldList,"");
    strcpy(pclDataList,"");
    if (*pclFkt == 'I')
    {
       if (*pclLoca1 == ' ')
          ilRaid1 = -1;
       if (*pclLoca2 == ' ')
          ilRaid2 = -1;
       if (*pclAftLoca1 != ' ' && strcmp(pclAftLoca1,pclLoca1) == 0)
          ilRaid1 = -1;
       if (*pclAftLoca1 != ' ' && strcmp(pclAftLoca1,pclLoca2) == 0)
          ilRaid2 = -1;
       if (*pclAftLoca2 != ' ' && strcmp(pclAftLoca2,pclLoca1) == 0)
          ilRaid1 = -1;
       if (*pclAftLoca2 != ' ' && strcmp(pclAftLoca2,pclLoca2) == 0)
          ilRaid2 = -1;
       if (*pclAftLoca1 == ' ' || ipNo == 1)
       {
          ilUseAft = 1;
          sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1);
       }
       else if (*pclAftLoca2 == ' ')
       {
          ilUseAft = 2;
          sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
       }
       else if (ipNo == 2)
       {
          sprintf(pclRarFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO,STAT");
          sprintf(pclRarSelection,"WHERE RURN = %s AND RNAM = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",
                  pcgUrno,pclAftLoca1,pcpType);
          sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclRarFieldList,pclRarSelection);
          slCursor = 0;
          slFkt = START;
          dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
          ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
          close_my_cursor(&slCursor);
          if (ilRCdb1 == DB_SUCCESS)
          {
             strcpy(pclDataBuf1,"");
             sprintf(pclRarFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO,STAT");
             sprintf(pclRarSelection,"WHERE RURN = %s AND RNAM = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",
                     pcgUrno,pclAftLoca2,pcpType);
             sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclRarFieldList,pclRarSelection);
             slCursor = 0;
             slFkt = START;
             dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
             ilRCdb1 = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf1);
             close_my_cursor(&slCursor);
             if (ilRCdb1 != DB_SUCCESS)
             {
                ilUseAft = 2;
                sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
             }
          }
          else
          {
             ilUseAft = 1;
             sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1);
          }
       }
/* allow input without raid
       if (ilRaid1 >= 0)
          sprintf(pclDataList,"%s,%s,%s,%s,%s",pclLoca1,pclSbgn1,pclSend1,pclAbgn1,pclAend1);
       else
          strcpy(pclFieldList,"");
*/
       sprintf(pclDataList,"%s,%s,%s,%s,%s",pclLoca1,pclSbgn1,pclSend1,pclAbgn1,pclAend1);
       if (ilUseAft == 1 && ilRaid2 >= 0 && *pclAftLoca2 == ' ')
       {
          sprintf(pclTmpBuf,"%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
          if (strlen(pclFieldList) > 0)
             strcat(pclFieldList,",");
          strcat(pclFieldList,pclTmpBuf);
          sprintf(pclTmpBuf,"%s,%s,%s,%s,%s",pclLoca2,pclSbgn2,pclSend2,pclAbgn2,pclAend2);
          if (strlen(pclDataList) > 0)
             strcat(pclDataList,",");
          strcat(pclDataList,pclTmpBuf);
       }
       if (strlen(pclFieldList) > 0 && strlen(pclDataList) > 0)
       {
          sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
          if (strlen(pclDoNotUpd) > 0)
          {
             ilNoFlds = GetNoOfElements(pclFieldList,',');
             for (ilFldI = 1; ilFldI <= ilNoFlds; ilFldI++)
             {
                get_real_item(pclFldNam,pclFieldList,ilFldI);
                if (strlen(pclFldNam) > 0 && strstr(pclDoNotUpd,pclFldNam) != NULL)
                {
                   dbg(TRACE,"%s Command <%s> not sent, because Update of Field <%s> is not allowed.",
                       pclFunc,pclCommand,pclFldNam);
                   return RC_FAIL;
                }
             }
          }
          dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
          dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
          dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
          dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
          ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                               pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
       }
    }
    else
    {
       if (*pclFkt == 'D')
       {
          if (strcmp(pclAftLoca1,pclLocaOld1) == 0 || ipNo == 1)
          {
             sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1);
             if (*pclAftLoca2 == ' ')
                strcpy(pclDataList," , , , , ");
             else
             {
                if (ilRaid2 >= 0 && strcmp(pclAftLoca2,pclLocaOld2) == 0)
                {
                   sprintf(pclFieldList,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
                           pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1,
                           pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
                   strcpy(pclDataList," , , , , , , , , , ");
                }
                else
                {
/* Replace these 4 statements
                   sprintf(pclDataList,"%s,%s,%s,%s,%s",pclAftLoca2,pclAftSbgn2,pclAftSend2,pclAftAbgn2,pclAftAend2);
                   sprintf(pclTmpBuf,",%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
                   strcat(pclFieldList,pclTmpBuf);
                   strcat(pclDataList,", , , , , ");
*/
                   sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
                   strcpy(pclDataList," , , , , ");
                   sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
                   dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                   dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
                   dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
                   dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
                   ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                        pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
                   sprintf(pclDataList,"%s,%s,%s,%s,%s",pclAftLoca2,pclAftSbgn2,pclAftSend2,pclAftAbgn2,pclAftAend2);
                   sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1);
                }
             }
          }
          if (strcmp(pclAftLoca2,pclLocaOld1) == 0)
          {
             sprintf(pclFieldList,"%s,%s,%s,%s,%s",pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
             strcpy(pclDataList," , , , , ");
             if (ilRaid2 >= 0 && strcmp(pclAftLoca1,pclLocaOld2) == 0)
             {
                sprintf(pclFieldList,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
                        pclFld1L1,pclFld2L1,pclFld3L1,pclFld4L1,pclFld5L1,
                        pclFld1L2,pclFld2L2,pclFld3L2,pclFld4L2,pclFld5L2);
                strcpy(pclDataList," , , , , , , , , , ");
             }
          }
          if (strlen(pclFieldList) > 0 && strlen(pclDataList) > 0)
          {
             sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
             if (strlen(pclDoNotUpd) > 0)
             {
                ilNoFlds = GetNoOfElements(pclFieldList,',');
                for (ilFldI = 1; ilFldI <= ilNoFlds; ilFldI++)
                {
                   get_real_item(pclFldNam,pclFieldList,ilFldI);
                   if (strlen(pclFldNam) > 0 && strstr(pclDoNotUpd,pclFldNam) != NULL)
                   {
                      dbg(TRACE,"%s Command <%s> not sent, because Update of Field <%s> is not allowed.",
                          pclFunc,pclCommand,pclFldNam);
                      return RC_FAIL;
                   }
                }
             }
             dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
             dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
             dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
             dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
             ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                  pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
          }
       }
       else
       {
          TrimRight(pclAftLoca1);
          TrimRight(pclAftLoca2);
          TrimRight(pclLoca1);
          TrimRight(pclLoca2);
          TrimRight(pclLocaOld1);
          TrimRight(pclLocaOld2);
          ilUseAft = 0;
          if (strcmp(pclAftLoca1,pclLocaOld1) == 0 ||
              (*pclAftLoca1 == ' ' && *pclLoca1 != ' '))
             ilUseAft = 11;
          else if (ipNo > 1 && strcmp(pclAftLoca1,pclLocaOld2) == 0)
             ilUseAft = 12;
          else if (ipNo > 1 && (strcmp(pclAftLoca2,pclLocaOld1) == 0 ||
                                (*pclAftLoca2 == ' ' && *pclLoca1 != ' ')))
             ilUseAft = 21;
          else if (ipNo > 1 && strcmp(pclAftLoca2,pclLocaOld2) == 0)
             ilUseAft = 22;
          else
             ilUseAft = 11;
          if (ilUseAft == 11)
          {
             if (GetDataLineIdx(pclFld1L1,0) >= 0 && strcmp(pclAftLoca1,pclLoca1) != 0)
             {
                strcat(pclFieldList,pclFld1L1);
                strcat(pclDataList,pclLoca1);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld2L1,0) >= 0 && strcmp(pclAftSbgn1,pclSbgn1) != 0)
             {
                strcat(pclFieldList,pclFld2L1);
                strcat(pclDataList,pclSbgn1);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld3L1,0) >= 0 && strcmp(pclAftSend1,pclSend1) != 0)
             {
                strcat(pclFieldList,pclFld3L1);
                strcat(pclDataList,pclSend1);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld4L1,0) >= 0 && strcmp(pclAftAbgn1,pclAbgn1) != 0)
             {
                strcat(pclFieldList,pclFld4L1);
                strcat(pclDataList,pclAbgn1);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld5L1,0) >= 0 && strcmp(pclAftAend1,pclAend1) != 0)
             {
                strcat(pclFieldList,pclFld5L1);
                strcat(pclDataList,pclAend1);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
          }
          if (ilUseAft == 12)
          {
             if (GetDataLineIdx(pclFld1L2,0) >= 0 && strcmp(pclAftLoca1,pclLoca2) != 0)
             {
                strcat(pclFieldList,pclFld1L1);
                strcat(pclDataList,pclLoca2);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld2L2,0) >= 0 && strcmp(pclAftSbgn1,pclSbgn2) != 0)
             {
                strcat(pclFieldList,pclFld2L1);
                strcat(pclDataList,pclSbgn2);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld3L2,0) >= 0 && strcmp(pclAftSend1,pclSend2) != 0)
             {
                strcat(pclFieldList,pclFld3L1);
                strcat(pclDataList,pclSend2);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld4L2,0) >= 0 && strcmp(pclAftAbgn1,pclAbgn2) != 0)
             {
                strcat(pclFieldList,pclFld4L1);
                strcat(pclDataList,pclAbgn2);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
             if (GetDataLineIdx(pclFld5L2,0) >= 0 && strcmp(pclAftAend1,pclAend2) != 0)
             {
                strcat(pclFieldList,pclFld5L1);
                strcat(pclDataList,pclAend2);
                strcat(pclFieldList,",");
                strcat(pclDataList,",");
             }
          }
          if (ipNo == 2)
          {
             if (ilUseAft == 21)
             {
                if ((GetDataLineIdx(pclFld1L1,0) >= 0  || GetDataLineIdx(pclFld1L2,0) >= 0) &&
                    strcmp(pclAftLoca2,pclLoca1) != 0)
                {
                   strcat(pclFieldList,pclFld1L2);
                   strcat(pclDataList,pclLoca1);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if ((GetDataLineIdx(pclFld2L1,0) >= 0 || GetDataLineIdx(pclFld2L2,0) >= 0)  &&
                    strcmp(pclAftSbgn2,pclSbgn1) != 0)
                {
                   strcat(pclFieldList,pclFld2L2);
                   strcat(pclDataList,pclSbgn1);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if ((GetDataLineIdx(pclFld3L1,0) >= 0 || GetDataLineIdx(pclFld3L2,0) >= 0)  &&
                    strcmp(pclAftSend2,pclSend1) != 0)
                {
                   strcat(pclFieldList,pclFld3L2);
                   strcat(pclDataList,pclSend1);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if ((GetDataLineIdx(pclFld4L1,0) >= 0 || GetDataLineIdx(pclFld4L2,0) >= 0)  &&
                    strcmp(pclAftAbgn2,pclAbgn1) != 0)
                {
                   strcat(pclFieldList,pclFld4L2);
                   strcat(pclDataList,pclAbgn1);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if ((GetDataLineIdx(pclFld5L1,0) >= 0 || GetDataLineIdx(pclFld5L2,0) >= 0)  &&
                    strcmp(pclAftAend2,pclAend1) != 0)
                {
                   strcat(pclFieldList,pclFld5L2);
                   strcat(pclDataList,pclAend1);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
             }
             if (ilUseAft == 22)
             {
                if (GetDataLineIdx(pclFld1L2,0) >= 0 && strcmp(pclAftLoca2,pclLoca2) != 0)
                {
                   strcat(pclFieldList,pclFld1L2);
                   strcat(pclDataList,pclLoca2);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if (GetDataLineIdx(pclFld2L2,0) >= 0 && strcmp(pclAftSbgn2,pclSbgn2) != 0)
                {
                   strcat(pclFieldList,pclFld2L2);
                   strcat(pclDataList,pclSbgn2);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if (GetDataLineIdx(pclFld3L2,0) >= 0 && strcmp(pclAftSend2,pclSend2) != 0)
                {
                   strcat(pclFieldList,pclFld3L2);
                   strcat(pclDataList,pclSend2);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if (GetDataLineIdx(pclFld4L2,0) >= 0 && strcmp(pclAftAbgn2,pclAbgn2) != 0)
                {
                   strcat(pclFieldList,pclFld4L2);
                   strcat(pclDataList,pclAbgn2);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
                if (GetDataLineIdx(pclFld5L2,0) >= 0 && strcmp(pclAftAend2,pclAend2) != 0)
                {
                   strcat(pclFieldList,pclFld5L2);
                   strcat(pclDataList,pclAend2);
                   strcat(pclFieldList,",");
                   strcat(pclDataList,",");
                }
             }
          }
          if (strlen(pclFieldList) > 0 && strlen(pclDataList) > 0)
          {
             pclFieldList[strlen(pclFieldList)-1] = '\0';
             pclDataList[strlen(pclDataList)-1] = '\0';
             sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
             if (strlen(pclDoNotUpd) > 0)
             {
                ilNoFlds = GetNoOfElements(pclFieldList,',');
                for (ilFldI = 1; ilFldI <= ilNoFlds; ilFldI++)
                {
                   get_real_item(pclFldNam,pclFieldList,ilFldI);
                   if (strlen(pclFldNam) > 0 && strstr(pclDoNotUpd,pclFldNam) != NULL)
                   {
                      dbg(TRACE,"%s Command <%s> not sent, because Update of Field <%s> is not allowed.",
                          pclFunc,pclCommand,pclFldNam);
                      return RC_FAIL;
                   }
                }
             }
             dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
             dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
             dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
             dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
             ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                  pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
          }
       }
    }
  }

  return ilRC;
} /* End of HandleLocation */


static int GetLocaData(int ipRaid1, int ipRaid2,
                       char *pcpFld1L1, char *pcpFld2L1, char *pcpFld3L1, char *pcpFld4L1, char *pcpFld5L1,
                       char *pcpFld1L2, char *pcpFld2L2, char *pcpFld3L2, char *pcpFld4L2, char *pcpFld5L2,
                       char *pcpOut1L1, char *pcpOut2L1, char *pcpOut3L1, char *pcpOut4L1, char *pcpOut5L1, char *pcpOut6L1,
                       char *pcpOut1L2, char *pcpOut2L2, char *pcpOut3L2, char *pcpOut4L2, char *pcpOut5L2, char *pcpOut6L2)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetLocaData:";
  int ilLoca;
  int ilSbgn;
  int ilSend;
  int ilAbgn;
  int ilAend;

  ilLoca = GetDataLineIdx(pcpFld1L1,0);
  ilSbgn = GetDataLineIdx(pcpFld2L1,0);
  ilSend = GetDataLineIdx(pcpFld3L1,0);
  ilAbgn = GetDataLineIdx(pcpFld4L1,0);
  ilAend = GetDataLineIdx(pcpFld5L1,0);
  if (ipRaid1 >= 0)
     strcpy(pcpOut1L1,&rgData[ipRaid1].pclData[0]);
  if (ilLoca >= 0)
     strcpy(pcpOut2L1,&rgData[ilLoca].pclData[0]);
  if (ilSbgn >= 0)
     strcpy(pcpOut3L1,&rgData[ilSbgn].pclData[0]);
  if (ilSend >= 0)
     strcpy(pcpOut4L1,&rgData[ilSend].pclData[0]);
  if (ilAbgn >= 0)
     strcpy(pcpOut5L1,&rgData[ilAbgn].pclData[0]);
  if (ilAend >= 0)
     strcpy(pcpOut6L1,&rgData[ilAend].pclData[0]);
  if (ipRaid2 >= 0)
  {
     ilLoca = GetDataLineIdx(pcpFld1L2,ipRaid2-ipRaid1);
     ilSbgn = GetDataLineIdx(pcpFld2L2,ipRaid2-ipRaid1);
     ilSend = GetDataLineIdx(pcpFld3L2,ipRaid2-ipRaid1);
     ilAbgn = GetDataLineIdx(pcpFld4L2,ipRaid2-ipRaid1);
     ilAend = GetDataLineIdx(pcpFld5L2,ipRaid2-ipRaid1);
     strcpy(pcpOut1L2,&rgData[ipRaid2].pclData[0]);
  }
  else
  {
     if (ipRaid2 < 0)
     {
        ilLoca = GetDataLineIdx(pcpFld1L2,0);
        ilSbgn = GetDataLineIdx(pcpFld2L2,0);
        ilSend = GetDataLineIdx(pcpFld3L2,0);
        ilAbgn = GetDataLineIdx(pcpFld4L2,0);
        ilAend = GetDataLineIdx(pcpFld5L2,0);
     }
     else
     {
        ilLoca = -1;
        ilSbgn = -1;
        ilSend = -1;
        ilAbgn = -1;
        ilAend = -1;
     }
  }
  if (ilLoca >= 0)
     strcpy(pcpOut2L2,&rgData[ilLoca].pclData[0]);
  if (ilSbgn >= 0)
     strcpy(pcpOut3L2,&rgData[ilSbgn].pclData[0]);
  if (ilSend >= 0)
     strcpy(pcpOut4L2,&rgData[ilSend].pclData[0]);
  if (ilAbgn >= 0)
     strcpy(pcpOut5L2,&rgData[ilAbgn].pclData[0]);
  if (ilAend >= 0)
     strcpy(pcpOut6L2,&rgData[ilAend].pclData[0]);
  TrimRight(pcpOut1L1);
  TrimRight(pcpOut2L1);
  TrimRight(pcpOut3L1);
  TrimRight(pcpOut4L1);
  TrimRight(pcpOut5L1);
  TrimRight(pcpOut6L1);
  TrimRight(pcpOut1L2);
  TrimRight(pcpOut2L2);
  TrimRight(pcpOut3L2);
  TrimRight(pcpOut4L2);
  TrimRight(pcpOut5L2);
  TrimRight(pcpOut6L2);

/* Ignore Data in 2nd position, if there are values in 1st position */
  if (*pcpOut2L1 != ' ' &&
      (*pcpOut3L1 != ' ' || *pcpOut4L1 != ' ' || *pcpOut5L1 != ' ' || *pcpOut6L1 != ' '))
  {
     strcpy(pcpOut1L2," ");
     strcpy(pcpOut2L2," ");
     strcpy(pcpOut3L2," ");
     strcpy(pcpOut4L2," ");
     strcpy(pcpOut5L2," ");
     strcpy(pcpOut6L2," ");
  }

/* Resource Data should always be in 1st position */
  if (*pcpOut1L2 != ' ')
  {
     strcpy(pcpOut1L1,pcpOut1L2);
     strcpy(pcpOut1L2," ");
  }
  if (*pcpOut2L2 != ' ')
  {
     strcpy(pcpOut2L1,pcpOut2L2);
     strcpy(pcpOut2L2," ");
  }
  if (*pcpOut3L2 != ' ')
  {
     strcpy(pcpOut3L1,pcpOut3L2);
     strcpy(pcpOut3L2," ");
  }
  if (*pcpOut4L2 != ' ')
  {
     strcpy(pcpOut4L1,pcpOut4L2);
     strcpy(pcpOut4L2," ");
  }
  if (*pcpOut5L2 != ' ')
  {
     strcpy(pcpOut5L1,pcpOut5L2);
     strcpy(pcpOut5L2," ");
  }
  if (*pcpOut6L2 != ' ')
  {
     strcpy(pcpOut6L1,pcpOut6L2);
     strcpy(pcpOut6L2," ");
  }

  return ilRC;
} /* End of GetLocaData */


static int GetNextUrno()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetNextUrno:";
  int ilDebugLevel;

  if (igLastUrnoIdx == 1000 || igLastUrnoIdx < 0)
  {
     ilDebugLevel = debug_level;
     debug_level = 0;
     GetNextValues(pcgNewUrnos,1010);
     debug_level = ilDebugLevel;
     igLastUrnoIdx = 0;
     igLastUrno = atoi(pcgNewUrnos);
  }
  else
  {
     igLastUrnoIdx++;
     igLastUrno++;
  }

  return ilRC;
} /* End of GetNextUrno */


static int HandleCounter(char *pcpFkt, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleCounter:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclNewFieldList[1024];
  char pclActFieldList[1024];
  char pclDataList[1024];
  char pclActDataList[1024];
  char pclActDataList2[1024];
  char pclTmpBuf[1024];
  char pclTmpBuf1[1024];
  char pclName[1024];
  char pclMethod[32];
  char pclTable[32];
  int ilFound;
  int ilI;
  int ilRaid;
  char pclRaid[16];
  int ilCkic;
  char pclCkic[16];
  char pclOldCounter[16];
  char pclRarUrno[16];
  char pclCcaUrno[16];
  char pclOldRurn[16];
  int ilNoItems;
  int ilFlnu;
  char pclField[16];
  int ilIdx;
  int ilCount;
  int ilAlcd;
  char pclStat[16];
  char pclFlnu[16];
  char pclCtyp[16];
  char pclCkbs[16];
  char pclCkes[16];
  char pclOldCkic[16];
  int ilRaidNotFound;
  char pclFkt[16];
  char pclAirlineCodes[1024];
  char pclAlc2[16];
  int ilItemNo;
  int ilNoEle;
  char pclDoNotUpd[2048];

  if (strcmp(pcpType,"DC") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEDICATED_COUNTERS",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
  }
  else if (strcmp(pcpType,"CC") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","COMMON_COUNTERS",CFG_STRING,pclName);
     if (ilRC != RC_SUCCESS)
        return ilRC;
  }
  else
  {
     dbg(TRACE,"%s Invalid location <%s>",pclFunc,pcpType);
     ilRC = RC_FAIL;
  }
  if (ilRC != RC_SUCCESS)
     return ilRC;
  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0  && strstr(pclName,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  strcpy(pclFkt,pcpFkt);
  ilRaidNotFound = FALSE;
  ilRC = GetData(pclMethod,0,pclName);
  if (igNoData > 0)
  {
     strcpy(pclAirlineCodes,"");
     sprintf(pclTmpBuf,"ALLOWED_AIRLINE_CODES_FOR_%s",pcgIntfName);
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL",pclTmpBuf,CFG_STRING,pclAirlineCodes);
     if (strlen(pclAirlineCodes) > 0 && strcmp(pcpType,"CC") == 0)
     {
        strcpy(pclAlc2," ");
        ilItemNo = GetDataLineIdx("ALCD",0);
        if (ilItemNo > 0)
           strcpy(pclAlc2,&rgData[ilItemNo].pclData[0]);
        TrimRight(pclAlc2);
        ilFound = FALSE;
        ilNoEle = GetNoOfElements(pclAirlineCodes,',');
        for (ilI = 1; ilI <= ilNoEle && ilFound == FALSE; ilI++)
        {
           GetDataItem(pclTmpBuf,pclAirlineCodes,ilI,',',""," ");
           TrimRight(pclTmpBuf);
           if (strcmp(pclTmpBuf,pclAlc2) == 0)
              ilFound = TRUE;
        }
        if (ilFound == FALSE)
        {
           dbg(TRACE,"%s Airline <%s> not allowed for this Interface <%s>",pclFunc,pclAlc2,pcgIntfName);
           return RC_FAIL;
        }
     }
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"CCATAB");
     strcpy(pclCcaUrno,"");
     ilRaid = GetDataLineIdx("RAID",0);
     if (ilRaid < 0)
     {
        dbg(TRACE,"%s RAID not found",pclFunc);
        ilRaidNotFound = TRUE;
        /*return RC_FAIL;*/
     }
     ilIdx = GetDataLineIdx("CTYP",0);
     if (ilIdx >= 0)
     {
        if (strcmp(pcpType,"DC") == 0)
           if (rgData[ilIdx].pclData[0] == 'D')
              rgData[ilIdx].pclData[0] = ' ';
        if (strcmp(pcpType,"DC") == 0 && rgData[ilIdx].pclData[0] != ' ')
        {
           strcpy(pcpType,"CC");
           dbg(TRACE,"%s CTYP mismatch, switch to <%s>",pclFunc,pcpType);
           /*return RC_FAIL;*/
        }
        if (strcmp(pcpType,"CC") == 0 && rgData[ilIdx].pclData[0] != 'C')
        {
           dbg(TRACE,"%s CTYP mismatch",pclFunc);
           return RC_FAIL;
        }
     }
     else
     {
        strcpy(&rgData[igNoData].pclField[0],"CTYP");
        strcpy(&rgData[igNoData].pclType[0]," ");
        strcpy(&rgData[igNoData].pclMethod[0],&rgData[igNoData-1].pclMethod[0]);
        if (strcmp(pcpType,"DC") == 0)
        {
           strcpy(&rgData[igNoData].pclData[0]," ");
           strcpy(&rgData[igNoData].pclOldData[0]," ");
        }
        else
        {
           strcpy(&rgData[igNoData].pclData[0],"C");
           strcpy(&rgData[igNoData].pclOldData[0],"C");
        }
        igNoData++;
     }
     if (ilRaid >= 0)
     {
        ilRaidNotFound = FALSE;
        strcpy(pclRaid,&rgData[ilRaid].pclData[0]);
        TrimRight(pclRaid);
        sprintf(pclFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO,STAT");
        sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",pclRaid,pcpType);
        sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL 1 = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s RAID = <%s> does not exist",pclFunc,pclRaid);
           strcpy(pclOldCounter," ");
           strcpy(pclOldRurn," ");
           strcpy(pclFkt,"I");
        }
     }
     else
     {
        ilRaidNotFound = TRUE;
        strcpy(pclRaid," ");
        ilRCdb = DB_ERROR;
     }
     ilCkic = GetDataLineIdx("CKIC",0);
     if (ilCkic >= 0)
        strcpy(pclCkic,&rgData[ilCkic].pclData[0]);
     else
        strcpy(pclCkic," ");
     TrimRight(pclCkic);
     if (*pclFkt == 'U' && *pclCkic == ' ')
        strcpy(pclFkt,"D");
     if (*pclFkt == 'U')
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_U",CFG_STRING,pclTmpBuf);
        if (ilRC != RC_SUCCESS)
        {
           dbg(TRACE,"%s Update of CCA Record not allowed for this interface",pclFunc);
           return ilRC;
        }
     }
     else if (*pclFkt == 'I')
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_I",CFG_STRING,pclTmpBuf);
        if (ilRC != RC_SUCCESS)
        {
           dbg(TRACE,"%s Insert of CCA Record not allowed for this interface",pclFunc);
           return ilRC;
        }
     }
     else if (*pclFkt == 'D')
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_D",CFG_STRING,pclTmpBuf);
        if (ilRC != RC_SUCCESS)
        {
           dbg(TRACE,"%s Delete of CCA Record not allowed for this interface",pclFunc);
           return ilRC;
        }
     }
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"DO_NOT_UPD",CFG_STRING,pclDoNotUpd);
     if (ilRC != RC_SUCCESS)
        strcpy(pclDoNotUpd,"");
     if (*pclFkt == 'I')
     {
        if (ilRCdb == DB_SUCCESS)
        {
           BuildItemBuffer(pclDataBuf,"",8,",");
           get_real_item(pclStat,pclDataBuf,8);
           TrimRight(pclStat);
           if (*pclStat == ' ')
           {
              dbg(TRACE,"%s RAID = <%s> exists already",pclFunc,pclRaid);
              return ilRC;
           }
        }
        ilCkic = GetDataLineIdx("CKIC",0);
        if (ilCkic < 0)
        {
           dbg(TRACE,"%s CKIC not found",pclFunc);
           return RC_FAIL;
        }
        strcpy(pclCkic,&rgData[ilCkic].pclData[0]);
        TrimRight(pclCkic);
        ilRC = GetNextUrno();
        sprintf(pclCcaUrno,"%d",igLastUrno);
        if (ilRaidNotFound == FALSE)
        {
           ilRC = GetNextUrno();
           sprintf(pclDataList,"%d,'%s','%s','%s','%s',%s,'%s',' '",
                   igLastUrno,pclRaid,pclCkic,pcpType,pcgIntfName,pclCcaUrno,pcgHomeAp);
           sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL 2 = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
        }
        if (*pclCkic == ' ')
        {
           dbg(TRACE,"%s Counter Name is blank",pclFunc);
           return ilRC;
        }
     }
     else
     {
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s RAID = <%s> does not exist",pclFunc,pclRaid);
           /*return ilRC;*/
           strcpy(pclOldCounter," ");
           strcpy(pclOldRurn," ");
        }
        else
        {
           BuildItemBuffer(pclDataBuf,"",8,",");
           get_real_item(pclOldCounter,pclDataBuf,3);
           TrimRight(pclOldCounter);
           get_real_item(pclOldRurn,pclDataBuf,6);
           TrimRight(pclOldRurn);
        }
        ilCkic = GetDataLineIdx("CKIC",0);
        if (ilCkic < 0)
           strcpy(pclCkic,pclOldCounter);
        else
           strcpy(pclCkic,&rgData[ilCkic].pclData[0]);
        TrimRight(pclCkic);
        if (*pclFkt == 'U')
        {
           if (strcmp(pclOldCounter,pclCkic) != 0 && *pclCkic != ' ' && ilRCdb == DB_SUCCESS && ilRaidNotFound == FALSE)
           {
              sprintf(pclDataList,"RNAM = '%s'",pclCkic);
              get_real_item(pclRarUrno,pclDataBuf,1);
              sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
              sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL 3 = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              if (ilRCdb == DB_SUCCESS)
                 commit_work();
              else
              {
                 dbg(TRACE,"%s Error updating RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
                 return ilRC;
              }
              close_my_cursor(&slCursor);
           }
        }
        else
        {
           if (ilRaidNotFound == FALSE)
           {
              get_real_item(pclRarUrno,pclDataBuf,1);
              sprintf(pclDataList,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
              sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
              sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL 4 = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              if (ilRCdb == DB_SUCCESS)
                 commit_work();
              else
              {
                 dbg(TRACE,"%s Error deleting from RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
                 return ilRC;
              }
              close_my_cursor(&slCursor);
           }
        }
     }
     if (ilRaidNotFound == FALSE && *pclFkt == 'U')
     {
        if (*pclOldCounter == ' ')
        {
           ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_I",CFG_STRING,pclTmpBuf);
           if (ilRC != RC_SUCCESS)
           {
              dbg(TRACE,"%s Insert of CCA Record not allowed for this interface",pclFunc);
              return ilRC;
           }
           strcpy(pclFkt,"I");
           get_real_item(pclCcaUrno,pclDataBuf,6);
           TrimRight(pclCcaUrno);
        }
     }
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     ilNoItems = 0;
     for (ilI = 0; ilI < igNoData; ilI++)
     {
        if (strcmp(&rgData[ilI].pclField[0],"RAID") != 0 && strcmp(&rgData[ilI].pclField[0],"URNO") != 0 &&
            strcmp(&rgData[ilI].pclField[0],"FLNU") != 0 && strcmp(&rgData[ilI].pclField[0],"ALCD") != 0 &&
            strcmp(&rgData[ilI].pclField[0],"XXXX") != 0)
        {
           strcat(pclFieldList,&rgData[ilI].pclField[0]);
           strcat(pclFieldList,",");
           strcat(pclDataList,"'");
           strcat(pclDataList,&rgData[ilI].pclData[0]);
           strcat(pclDataList,"',");
           ilNoItems++;
        }
     }
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     if (strlen(pclDataList) > 0)
        pclDataList[strlen(pclDataList)-1] = '\0';
     if (*pclFkt == 'I')
     {
        ilIdx = GetDataLineIdx("CTYP",0);
        if (ilIdx < 0 && strcmp(pcpType,"CC") == 0)
        {
           strcat(pclFieldList,",CTYP");
           strcat(pclDataList,",'C'");
        }
        ilFlnu = GetDataLineIdx("FLNU",0);
        if (ilFlnu < 0)
        {
           if (strcmp(pcpType,"DC") == 0)
           {
              if (*pcgUrno != '\0')
              {
                 strcat(pclFieldList,",FLNU");
                 strcat(pclDataList,",");
                 strcat(pclDataList,pcgUrno);
              }
              else
              {
                 dbg(TRACE,"%s No Flight Information found for Dedicated Counter",pclFunc);
                 return ilRC;
              }
           }
        }
        else
        {
           if (strcmp(pcpType,"DC") == 0)
           {
              if (*pcgUrno != '\0' && strcmp(pcgUrno,&rgData[ilFlnu].pclData[0]) != 0)
              {
                 dbg(TRACE,"%s Flight URNO and FLNU mismatch for Dedicated Counter",pclFunc);
                 return ilRC;
              }
              strcat(pclFieldList,",FLNU");
              strcat(pclDataList,",");
              strcat(pclDataList,pcgUrno);
           }
        }
        ilAlcd = GetDataLineIdx("ALCD",0);
        if (ilAlcd < 0)
        {
           if (strcmp(pcpType,"CC") == 0)
           {
              dbg(TRACE,"%s No Airline Code found for Common Counter",pclFunc);
              return ilRC;
           }
        }
        else
        {
           ilCount = 1;
           if (strlen(&rgData[ilAlcd].pclData[0]) == 2)
              ilRC = syslibSearchDbData("ALTTAB","ALC2",&rgData[ilAlcd].pclData[0],"URNO",pclTmpBuf,&ilCount,"\n");
           else if (strlen(&rgData[ilAlcd].pclData[0]) == 3)
              ilRC = syslibSearchDbData("ALTTAB","ALC3",&rgData[ilAlcd].pclData[0],"URNO",pclTmpBuf,&ilCount,"\n");
           else
              ilRC = RC_FAIL;
           if (ilRC == RC_SUCCESS)
              strcpy(&rgData[ilAlcd].pclData[0],pclTmpBuf);
           else
           {
              dbg(TRACE,"%s Airline <%s> not found in ALTTAB",pclFunc,&rgData[ilAlcd].pclData[0]);
              return ilRC;
           }
           strcat(pclFieldList,",FLNU");
           strcat(pclDataList,",");
           strcat(pclDataList,&rgData[ilAlcd].pclData[0]);
        }
        strcat(pclFieldList,",URNO,HOPO");
        strcat(pclDataList,",");
        strcat(pclDataList,pclCcaUrno);
        strcat(pclDataList,",'");
        strcat(pclDataList,pcgHomeAp);
        strcat(pclDataList,"'");
        sprintf(pclSqlBuf,"INSERT INTO %s FIELDS(%s) VALUES(%s)",pclTable,pclFieldList,pclDataList);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL 5 = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error inserting into %s",pclFunc,pclTable);
           close_my_cursor(&slCursor);
           return ilRC;
        }
        close_my_cursor(&slCursor);
        strcpy(pclActDataList,"");
        for (ilI = 0; ilI < strlen(pclDataList); ilI++)
           if (strncmp(&pclDataList[ilI],"'",1) != 0)
              strncat(pclActDataList,&pclDataList[ilI],1);
        ilRC = TriggerBchdlAction("IRT",pclTable,pclCcaUrno,pclFieldList,pclActDataList,TRUE,TRUE);
     }
     else
     {
        if (*pclFkt == 'D')
        {
           sprintf(pclFieldList,"FLNU,URNO,CTYP,CKBS,CKES,CKIC");
           if (ilRaidNotFound == TRUE)
           {
              if (strcmp(pcpType,"DC") == 0)
              {
                 ilIdx = GetDataLineIdx("URNO",0);
                 if (ilIdx >= 0)
                 {
                    strcpy(pclOldRurn,&rgData[ilIdx].pclData[0]);
                    sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 }
                 else
                 {
                    sprintf(pclSelection,"WHERE FLNU = %s AND CKIC = '%s'",pcgUrno,pclCkic);
                 }
              }
              else
              {
                 ilIdx = GetDataLineIdx("URNO",0);
                 if (ilIdx >= 0)
                 {
                    strcpy(pclOldRurn,&rgData[ilIdx].pclData[0]);
                    sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 }
                 else
                 {
                    dbg(TRACE,"%s No RAID and no URNO specified. Can't do anything",pclFunc);
                    return ilRC;
                 }
              }
           }
           else
           {
              sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
           }
           sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL 6 = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb != DB_SUCCESS)
           {
              dbg(TRACE,"%s CCA Record with URNO <%s> not found",pclFunc,pclOldRurn);
              return ilRC;
           }
           BuildItemBuffer(pclDataBuf,"",6,",");
           get_real_item(pclFlnu,pclDataBuf,1);
           TrimRight(pclFlnu);
           get_real_item(pclOldRurn,pclDataBuf,2);
           TrimRight(pclOldRurn);
           get_real_item(pclCtyp,pclDataBuf,3);
           TrimRight(pclCtyp);
           get_real_item(pclCkbs,pclDataBuf,4);
           TrimRight(pclCkbs);
           get_real_item(pclCkes,pclDataBuf,5);
           TrimRight(pclCkes);
           get_real_item(pclOldCkic,pclDataBuf,6);
           TrimRight(pclOldCkic);
           if (ilRaidNotFound == FALSE)
           {
              sprintf(pclDataList,"RURN = %s",pclFlnu);
              sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
              sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL 7 = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              if (ilRCdb == DB_SUCCESS)
                 commit_work();
              else
              {
                 dbg(TRACE,"%s Error updating from RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
                 return ilRC;
              }
              close_my_cursor(&slCursor);
           }
           sprintf(pclSqlBuf,"DELETE FROM %s WHERE URNO = %s",pclTable,pclOldRurn);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL 8 = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error deleting from %s",pclFunc,pclTable);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
           strcpy(pclFieldList,"URNO,CKIC,CTYP,FLNU,CKBS,CKES");
           sprintf(pclActDataList,"%s,%s,%s,%s,%s,%s\n%s,%s,%s,%s,%s,%s",
                   pclOldRurn,pclCkic,pclCtyp,pclFlnu,pclCkbs,pclCkes,
                   pclOldRurn,pclOldCkic,pclCtyp,pclFlnu,pclCkbs,pclCkes);
           if (ilRaidNotFound == TRUE)
           {
              sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
              ilRC = TriggerBchdlAction("DRT",pclTable,pclSelection,pclFieldList,pclActDataList,TRUE,TRUE);
           }
           else
           {
              sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
              ilRC = TriggerBchdlAction("DRT",pclTable,pclSelection,pclFieldList,pclActDataList,TRUE,TRUE);
           }
        }
        else
        {
           strcpy(pclNewFieldList,"");
           strcpy(pclActFieldList,"");
           strcpy(pclActDataList,"");
           strcpy(pclActDataList2,"");
           ilAlcd = GetDataLineIdx("ALCD",0);
           ilFlnu = GetDataLineIdx("FLNU",0);
           if (ilAlcd >= 0 && strcmp(pcpType,"CC") == 0)
           {
              strcat(pclFieldList,",FLNU");
              ilCount = 1;
              if (strlen(&rgData[ilAlcd].pclData[0]) == 2)
                 ilRC = syslibSearchDbData("ALTTAB","ALC2",&rgData[ilAlcd].pclData[0],"URNO",pclTmpBuf1,&ilCount,"\n");
              else if (strlen(&rgData[ilAlcd].pclData[0]) == 3)
                 ilRC = syslibSearchDbData("ALTTAB","ALC3",&rgData[ilAlcd].pclData[0],"URNO",pclTmpBuf1,&ilCount,"\n");
              else
                 ilRC = RC_FAIL;
              TrimRight(pclTmpBuf1);
              if (ilRC != RC_SUCCESS)
              {
                 dbg(TRACE,"%s Airline <%s> not found in ALTTAB",pclFunc,&rgData[ilAlcd].pclData[0]);
                 return ilRC;
              }
              if (ilFlnu >= 0)
                 strcpy(&rgData[ilFlnu].pclData[0],pclTmpBuf1);
              else
              {
                 strcpy(&rgData[igNoData].pclField[0],"FLNU");
                 strcpy(&rgData[igNoData].pclType[0],"FLNU");
                 strcpy(&rgData[igNoData].pclMethod[0],&rgData[ilAlcd].pclMethod[0]);
                 strcpy(&rgData[igNoData].pclData[0],pclTmpBuf1);
                 strcpy(&rgData[igNoData].pclOldData[0],"");
                 igNoData++;
              }
           }
           if (ilRaidNotFound == TRUE)
           {
              strcat(pclFieldList,",URNO");
              if (strcmp(pcpType,"DC") == 0)
              {
                 ilIdx = GetDataLineIdx("URNO",0);
                 if (ilIdx >= 0)
                 {
                    strcpy(pclOldRurn,&rgData[ilIdx].pclData[0]);
                    sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 }
                 else
                    sprintf(pclSelection,"WHERE FLNU = %s AND CKIC = '%s'",pcgUrno,pclCkic);
              }
              else
              {
                 ilIdx = GetDataLineIdx("URNO",0);
                 if (ilIdx >= 0)
                 {
                    strcpy(pclOldRurn,&rgData[ilIdx].pclData[0]);
                    sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 }
                 else
                 {
                    dbg(TRACE,"%s No RAID and no URNO specified. Can't do anything",pclFunc);
                    return ilRC;
                 }
              }
           }
           else
           {
              sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
           }
           ilNoItems = GetNoOfElements(pclFieldList,',');
           sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pclTable,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL 9 = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf,"",ilNoItems,",");
              for (ilI = 1; ilI <= ilNoItems; ilI++)
              {
                 get_real_item(pclTmpBuf,pclDataBuf,ilI);
                 TrimRight(pclTmpBuf);
                 get_real_item(pclField,pclFieldList,ilI);
                 if (strcmp(pclField,"URNO") != 0)
                 {
                    ilIdx = GetDataLineIdx(pclField,0);
                    if (ilIdx >= 0)
                    {
                       if (strcmp(&rgData[ilIdx].pclData[0],pclTmpBuf) != 0)
                       {
                          if (strstr(pclDoNotUpd,pclField) != NULL)
                          {
                             dbg(TRACE,"%s Update of CCATAB not done, because Update of Field <%s> is not allowed.",
                                 pclFunc,pclField);
                             return RC_FAIL;
                          }
                          if (strcmp(pclField,"FLNU") == 0)
                          {
                             strcat(pclNewFieldList,pclField);
                             strcat(pclNewFieldList,"=");
                             strcat(pclNewFieldList,pclTmpBuf1);
                             strcat(pclNewFieldList,",");
                             strcat(pclActFieldList,pclField);
                             strcat(pclActFieldList,",");
                             strcat(pclActDataList,pclTmpBuf1);
                             strcat(pclActDataList,",");
                             strcat(pclActDataList2,pclTmpBuf);
                             strcat(pclActDataList2,",");
                          }
                          else
                          {
                             strcat(pclNewFieldList,pclField);
                             strcat(pclNewFieldList,"='");
                             strcat(pclNewFieldList,&rgData[ilIdx].pclData[0]);
                             strcat(pclNewFieldList,"',");
                             strcat(pclActFieldList,pclField);
                             strcat(pclActFieldList,",");
                             strcat(pclActDataList,&rgData[ilIdx].pclData[0]);
                             strcat(pclActDataList,",");
                             strcat(pclActDataList2,pclTmpBuf);
                             strcat(pclActDataList2,",");
                          }
                       }
                    }
                 }
                 else
                 {
                    strcpy(pclOldRurn,pclTmpBuf);
                    sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 }
              }
              if (strlen(pclNewFieldList) > 0)
              {
                 pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
                 sprintf(pclSqlBuf,"UPDATE %s SET %s %s",pclTable,pclNewFieldList,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL 10 = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 if (ilRCdb == DB_SUCCESS)
                    commit_work();
                 else
                 {
                    dbg(TRACE,"%s Error updating %s",pclFunc,pclTable);
                    close_my_cursor(&slCursor);
                    return ilRC;
                 }
                 close_my_cursor(&slCursor);
                 if (strlen(pclActFieldList) > 0)
                    pclActFieldList[strlen(pclActFieldList)-1] = '\0';
                 if (strlen(pclActDataList) > 0)
                    pclActDataList[strlen(pclActDataList)-1] = '\0';
                 if (strlen(pclActDataList2) > 0)
                    pclActDataList2[strlen(pclActDataList2)-1] = '\0';
                 if (*pclFkt == 'U')
                 {
                    if (strstr(pclActFieldList,"URNO") == NULL)
                    {
                       strcat(pclActFieldList,",URNO");
                       strcat(pclActDataList,",");
                       strcat(pclActDataList,pclOldRurn);
                       strcat(pclActDataList2,",");
                       strcat(pclActDataList2,pclOldRurn);
                    }
                 }
                 strcat(pclActDataList,"\n");
                 strcat(pclActDataList,pclActDataList2);
                 sprintf(pclSelection,"WHERE URNO = %s",pclOldRurn);
                 ilRC = TriggerBchdlAction("URT",pclTable,pclSelection,pclActFieldList,pclActDataList,TRUE,TRUE);
              }
           }
           else
           {
              dbg(TRACE,"%s CCA record does not exist",pclFunc);
              return ilRC;
           }
        }
     }
  }

  return ilRC;
} /* End of HandleCounter */

static int HandleFlightRelatedData(char *pcpFkt)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleFlightRelatedData:";
  char pclName[1024] = "\0";
  char pclMethod[32] = "\0";
  char pclCmd[16] = "\0";
  char pclTable[16] = "\0";
  char pclField[16] = "\0";
  char pclData[1024] = "\0";
  char pclSelKey[1024] = "\0";
  char pclFieldList[1024*2] = "\0";
  char pclDataList[1024*8] = "\0";
  char pclKeyList[1024] = "\0";
  char pclTmpBuf[1024] = "\0";
  char pclChkMethod[64] = "\0";
  char pclCurSection[64] = "\0";
  char pclDestModId[16] = "\0";
  int ilFound;
  int ilI;
  int ilJ;
  int ilNoEle;
  int ilNoItems;
  int ilItm;
  int ilPrio;
  int ilToModId;
  int ilHdIdx;
  int ilSendMoreTrigger = FALSE;
  int ilSendMultTrigger = TRUE;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_RELATED",CFG_STRING,pclName);
  dbg(TRACE,"FLIGHT RELATED SECTIONS <%s>",pclName);
  dbg(TRACE,"MULTIPLE USED DATA SECTIONS <%s>",pcgMultipleSectionSetTrigger);
  strcpy(pclMethod,"");
  ilNoEle = GetNoOfElements(pclName,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     ilFound = FALSE;
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strcmp(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) == 0)
        {
           strcpy(pclMethod,&rgXml.rlLine[ilJ].pclMethod[igIntfIdx][0]);
           strcpy(pclChkMethod,pclMethod),
           ilFound = TRUE;
        }
     }
     if (ilFound == TRUE)
     {
        dbg(TRACE,"FOUND <%s> WITH METHOD <%s>",pclTmpBuf,pclMethod);
        ilRC = GetData(pclMethod,0,pclTmpBuf);
        if (igNoData > 0)
        {
           dbg(TRACE,"SECTION <%s> CONTAINS %d DATA TAGS",pclTmpBuf,igNoData);
           dbg(TRACE,"SECTION TRIGGER NAME <%s>",pclChkMethod);

           /* MEI: No time, hardcode for Dubai EFPS. */
           if( !strcmp( pclTmpBuf, "EfpsUfisMessage" ) )
           {
               HardCodedEFPSDXB();
               return RC_SUCCESS;
           }//Frank v1.52 20121226
           else if( strcmp(pclTmpBuf,"flightRecord") == 0 || strcmp( pclTmpBuf, "flightRecordRequest" ) == 0 )
           {
							dbg(DEBUG,"pcgDXBHoneywell<%s>pcgDXBCustomization<%s>pcgInstallation<%s>",pcgDXBHoneywell,pcgDXBCustomization,pcgInstallation);

							if(strcmp(pcgDXBHoneywell,"YES") == 0 && strcmp(pcgDXBCustomization,"YES") == 0 && strcmp(pcgInstallation,"DXB") == 0)
							{
							 	dbg(DEBUG,"<%s>Calling HardCodedHoneyWellDXB",pclFunc);
							 	HardCodedHoneyWellDXB(pclTmpBuf);
								return RC_SUCCESS;
							}
							else
							{
							   dbg(DEBUG,"<%s>Not calling HardCodedHoneyWellDXB",pclFunc);
							   return RC_FAIL;
							}
           }
           //Frank v1.52 20121226

           /***************************/
           strcpy(pclCurSection,pclTmpBuf);
           strcpy(pclFieldList,"");
           strcpy(pclDataList,"");
           for (ilI = 0; ilI < igNoData; ilI++)
           {
              if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0)
              {
                 strcat(pclFieldList,&rgData[ilI].pclField[0]);
                 strcat(pclFieldList,",");
                 strcat(pclDataList,&rgData[ilI].pclData[0]);
                 strcat(pclDataList,",");
              }
           }
           if (strlen(pclFieldList) > 0)
           {
              pclFieldList[strlen(pclFieldList)-1] = '\0';
              pclDataList[strlen(pclDataList)-1] = '\0';
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"add_fields",CFG_STRING,pclTmpBuf);
              if (ilRC == RC_SUCCESS)
              {
                 strcat(pclFieldList,",");
                 strcat(pclFieldList,pclTmpBuf);
              }
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"add_values",CFG_STRING,pclTmpBuf);
              if (ilRC == RC_SUCCESS)
              {
                 strcat(pclDataList,",");
                 strcat(pclDataList,pclTmpBuf);
              }
              dbg(TRACE,"FIELDS <%s>",pclFieldList);
              dbg(TRACE,"DATA <%s>",pclDataList);
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclDestModId);
              dbg(TRACE,"MOD_ID <%s>",pclDestModId);
              ilToModId = atoi(pclDestModId);
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclTmpBuf);
              dbg(TRACE,"PRIO <%s>",pclTmpBuf);
              ilPrio = atoi(pclTmpBuf);
              sprintf(pclTmpBuf,"snd_cmd_%s",pcpFkt);
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,pclTmpBuf,CFG_STRING,pclCmd);
              dbg(TRACE,"CMD <%s>",pclCmd);
              ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
              dbg(TRACE,"TABLE <%s>",pclTable);

            /**/
            if( strcmp(pclCurSection,"FIS_Flight") == 0)
            {
                dbg(DEBUG,"pcgAUH_Hermes<%s>pcgCustomization<%s>pcgInstallation<%s>",pcgAUH_Hermes,pcgCustomization,
            pcgInstallation);

                if(strcmp(pcgAUH_Hermes,"YES") == 0 && strcmp(pcgCustomization,"AUH") == 0 && strcmp(pcgInstallation
            ,"AUH") == 0)
                {
                        dbg(DEBUG,"<%s>Calling handleHemers",pclFunc);
                        handleHemers(pclCurSection,pclFieldList,pclDataList,pclDestModId,ilPrio,pclCmd,pclTable);
                        return RC_SUCCESS;
                }
                else
                {
                   dbg(DEBUG,"<%s>Not calling handleHemers",pclFunc);
                   return RC_FAIL;
                }
            }
            /**/

              strcpy(pclSelKey,"");
              if (strlen(pcgMessageType) > 0)
              {
                 strcpy(pclTmpBuf,"XX");
                 ilHdIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
                 if (ilHdIdx >= 0)
                 {
                    sprintf(pclTmpBuf,"%s=%s",pcgMessageType,rgXml.rlLine[ilHdIdx].pclData);
                 }
                 strcat(pclSelKey,pclTmpBuf);
                 strcat(pclSelKey,",");
              }
              strcpy(pclTmpBuf,"XX");
              ilHdIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
              if (ilHdIdx >= 0)
              {
                 sprintf(pclTmpBuf,"%s=%s",pcgActionType,rgXml.rlLine[ilHdIdx].pclData);
              }
              strcat(pclSelKey,pclTmpBuf);

              sprintf(pclTmpBuf,",METH=%s",pclMethod);
              strcat(pclSelKey,pclTmpBuf);

              ilSendMoreTrigger = FALSE;
              if (strstr(pcgMultipleSectionSetTrigger,pclChkMethod) != NULL)
              {
                 ilSendMoreTrigger = TRUE;
              }
              if ((ilSendMoreTrigger == TRUE) ||
                  (strstr(pcgListOfMultipleMethodTriggers,pclChkMethod) != NULL) ||
                  (strstr(pcgMultipleSectionTriggered,pclChkMethod) != NULL))
              {
                 strcpy(pclTmpBuf,",TRIG=YES");
                 strcat(pclSelKey,pclTmpBuf);
                 strcpy(pclTmpBuf,",MULT=YES");
                 ilSendMultTrigger = TRUE;
              }
              else
              {
                 strcpy(pclTmpBuf,",TRIG=NO");
                 strcat(pclSelKey,pclTmpBuf);
                 strcpy(pclTmpBuf,",MULT=NO");
                 ilSendMultTrigger = FALSE;
              }
              strcat(pclSelKey,pclTmpBuf);

              if (ilSendMoreTrigger == TRUE)
              {
                 strcpy(pclTmpBuf,",MORE=YES");
              }
              else
              {
                 strcpy(pclTmpBuf,",MORE=NO");
              }
              strcat(pclSelKey,pclTmpBuf);

              sprintf(pclTmpBuf,",URNO=%s",pcgUrno);
              strcat(pclSelKey,pclTmpBuf);

              ilRC = GetKeys(pcgAdid);
              for (ilI = 0; ilI < igNoKeys; ilI++)
              {
                 if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0)
                 {
                    sprintf(pclTmpBuf,",%s=%s",&rgKeys[ilI].pclField[0],&rgKeys[ilI].pclData[0]);
                    strcat(pclSelKey,pclTmpBuf);
                 }
              }
              dbg(TRACE,"SEL_KEYS <%s>",pclSelKey);
              if ((ilSendMultTrigger == TRUE) || (ilSendMoreTrigger == TRUE))
              {
                 if (strstr(pcgMultipleSectionTriggered,pclChkMethod) == NULL)
                 {
                    strcat(pcgMultipleSectionTriggered,pclChkMethod);
                    dbg(TRACE,"FINAL TRIGGER EVENT OF MULTIPLE SECTION CREATED");
                    sprintf(pclDestModId,"%d,%d",ilToModId,ilPrio);
                    ilRC = SendCedaEvent(mod_id,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                             "FINAL",pclTable,pclSelKey,pclDestModId,pclCurSection,"",1,NETOUT_NO_ACK);
                 }
              }
              ilRC = SendCedaEvent(ilToModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCmd,pclTable,pclSelKey,pclFieldList,pclDataList,"",ilPrio,NETOUT_NO_ACK);
              dbg(TRACE,"MESSAGE SENT TO MOD %d",ilToModId);
           }
        }
        else
        {
           dbg(TRACE,"SECTION <%s> DOES NOT CONTAIN DATA TAGS",pclTmpBuf);
        }
     }
     else
     {
        dbg(TRACE,"ELEMENT <%s> DOES NOT EXIST",pclTmpBuf);
     }
  }
  return ilRC;
} /* End of HandleFlightRelatedData */

static int HandleBlkTab(char *pcpFkt)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleBlkTab:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclActDataList[1024];
  char pclName[1024];
  char pclMethod[32];
  int ilFound;
  int ilI;
  int ilJ;
  int ilNoEle;
  char pclTmpBuf[1024];
  char pclUrno[16];
  char pclBurn[16];
  char pclTable[16];
  char pclField[16];
  int ilIdx;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BLKTAB_DATA",CFG_STRING,pclName);
  strcpy(pclMethod,"");
  ilNoEle = GetNoOfElements(pclName,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclTmpBuf,pclName,ilI);
     ilFound = FALSE;
     for (ilJ = 0; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
     {
        if (strcmp(pclTmpBuf,&rgXml.rlLine[ilJ].pclName[0]) == 0)
        {
           strcpy(pclMethod,&rgXml.rlLine[ilJ].pclMethod[igIntfIdx][0]);
           ilFound = TRUE;
        }
     }
     ilRC = GetData(pclMethod,0,pclTmpBuf);
     strcpy(pclTable,"");
     ilIdx = GetDataLineIdx("PNAM",0);
     if (ilIdx >= 0)
     {
        strcpy(pclField,"PNAM");
        strcpy(pclTable,"PSTTAB");
     }
     else
     {
        ilIdx = GetDataLineIdx("BNAM",0);
        if (ilIdx >= 0)
        {
           strcpy(pclField,"BNAM");
           strcpy(pclTable,"BLTTAB");
        }
        else
        {
           ilIdx = GetDataLineIdx("GNAM",0);
           if (ilIdx >= 0)
           {
              strcpy(pclField,"GNAM");
              strcpy(pclTable,"GATTAB");
           }
           else
           {
              ilIdx = GetDataLineIdx("CNAM",0);
              if (ilIdx >= 0)
              {
                 strcpy(pclField,"CNAM");
                 strcpy(pclTable,"CICTAB");
              }
              else
              {
                 ilIdx = GetDataLineIdx("WNAM",0);
                 if (ilIdx >= 0)
                 {
                    strcpy(pclField,"WNAM");
                    strcpy(pclTable,"WROTAB");
                 }
                 else
                 {
                    ilIdx = GetDataLineIdx("ENAM",0);
                    if (ilIdx >= 0)
                    {
                       strcpy(pclField,"ENAM");
                       strcpy(pclTable,"EXTTAB");
                    }
                 }
              }
           }
        }
     }
     if (strlen(pclTable) > 0)
     {
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE %s = '%s'",pclTable,pclField,&rgData[ilIdx].pclData[0]);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclBurn);
        close_my_cursor(&slCursor);
        if (*pcpFkt == 'I')
        {
           ilRC = GetNextUrno();
           sprintf(pclUrno,"%d",igLastUrno);
           strcpy(pclFieldList,"URNO,BURN,");
           sprintf(pclDataList,"%s,%s,",pclUrno,pclBurn);
           strcat(pclFieldList,"TABN,HOPO");
           strcat(pclDataList,"'");
           strncat(pclDataList,pclTable,3);
           strcat(pclDataList,"','");
           strcat(pclDataList,pcgHomeAp);
           strcat(pclDataList,"'");
           for (ilJ = 0; ilJ < igNoData; ilJ++)
           {
              if (strcmp(&rgData[ilJ].pclField[0],pclField) != 0 &&
                  strcmp(&rgData[ilJ].pclField[0],"XXXX") != 0)
              {
                 strcat(pclFieldList,",");
                 strcat(pclFieldList,&rgData[ilJ].pclField[0]);
                 strcat(pclDataList,",'");
                 strcat(pclDataList,&rgData[ilJ].pclData[0]);
                 strcat(pclDataList,"'");
              }
           }
           sprintf(pclSqlBuf,"INSERT INTO BLKTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error inserting into BLKTAB",pclFunc);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
           strcpy(pclActDataList,"");
           for (ilJ = 0; ilJ < strlen(pclDataList); ilJ++)
              if (strncmp(&pclDataList[ilJ],"'",1) != 0)
                 strncat(pclActDataList,&pclDataList[ilJ],1);
           ilRC = TriggerBchdlAction("IRT","BLKTAB",pclUrno,pclFieldList,pclActDataList,TRUE,TRUE);
        }
     }
  }

  return ilRC;
} /* End of HandleBlkTab */


static int HandleTowIns(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleTowIns:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclSelectionRar[1024];
  char pclFieldList[1024];
  char pclFieldListSel[1024];
  char pclFieldListRar[1024];
  char pclDataList[1024];
  char pclDataListSel[1024];
  char pclDataListRar[1024];
  char pclMethod[32];
  char pclCommand[32];
  char pclTable[32];
  char pclModId[32];
  char pclPriority[32];
  int ilModId;
  int ilPriority;
  int ilI;
  int ilFound;
  int ilIdx;
  char pclToid[32];
  char pclTowRecUrno[32];
  char pclDate[16];
  int ilItemNo;
  char pclStat[16];

  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pcpSection,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_I",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Insert of Towing not allowed for this interface",pclFunc);
     return ilRC;
  }
  ilRC = GetData(pclMethod,0,pcpSection);
  if (igNoData <= 0)
  {
     dbg(TRACE,"%s Towing information missing",pclFunc);
     ilRC = RC_FAIL;
     return ilRC;
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTable,"AFTTAB");
  if (igNoKeys > 0)
     ilRC = SearchFlight(pclMethod,pclTable);
  if (*pcgUrno != '\0')
  {
     ilIdx = GetDataLineIdx("TOID",0);
     if (ilIdx < 0)
     {
        dbg(TRACE,"%s Towing ID missing",pclFunc);
        ilRC = RC_FAIL;
        return ilRC;
     }
     strcpy(pclToid,&rgData[ilIdx].pclData[0]);
     TrimRight(pclToid);
     strcpy(pclFieldListSel,"RKEY,FLNO,ALC2,ALC3,FLTN,FLNS");
     sprintf(pclSelection,"WHERE URNO = %s",pcgUrno);
     sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldListSel,pclTable,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataListSel);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s Record with URNO = <%s> not found",pclFunc,pcgUrno);
        ilRC = RC_FAIL;
        return ilRC;
     }
     BuildItemBuffer(pclDataListSel,"",6,",");
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     ilRC = GetNextUrno();
     sprintf(pclTowRecUrno,"%d",igLastUrno);
     strcpy(pclFieldList,"URNO,");
     sprintf(pclDataList,"%d,",igLastUrno);
     for (ilI = 0; ilI < igNoData; ilI++)
     {
        if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0 && strcmp(&rgData[ilI].pclField[0],"TOID") != 0)
        {
           strcat(pclFieldList,&rgData[ilI].pclField[0]);
           strcat(pclFieldList,",");
           strcat(pclDataList,&rgData[ilI].pclData[0]);
           strcat(pclDataList,",");
        }
     }
     ilIdx = GetDataLineIdx("FTYP",0);
     if (ilIdx < 0)
     {
        strcat(pclFieldList,"FTYP,");
        strcat(pclDataList,"T,");
     }
     else
     {
        if (strcmp(&rgData[ilIdx].pclData[0],"T") != 0 && strcmp(&rgData[ilIdx].pclData[0],"G") != 0)
        {
           dbg(TRACE,"%s Wrong FTYP = <%s> found",pclFunc,&rgData[ilIdx].pclData[0]);
           ilRC = RC_FAIL;
           return ilRC;
        }
     }
     sprintf(pclFieldListRar,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO,STAT");
     sprintf(pclSelectionRar,"WHERE RAID = '%s' AND TYPE = 'TOW' AND STAT <> 'DELETED'",pclToid);
     sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldListRar,pclSelectionRar);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",8,",");
        get_real_item(pclStat,pclDataBuf,8);
        TrimRight(pclStat);
        if (*pclStat == ' ')
        {
           dbg(TRACE,"%s TOID = <%s> exists already",pclFunc,pclToid);
           return ilRC;
        }
     }
     ilRC = GetNextUrno();
     sprintf(pclDataListRar,"%d,'%s','%s','TOW','%s',%s,'%s',' '",
             igLastUrno,pclToid,pcgUrno,pcgIntfName,pclTowRecUrno,pcgHomeAp);
     sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldListRar,pclDataListRar);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     if (ilRCdb == DB_SUCCESS)
        commit_work();
     else
     {
        dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
        close_my_cursor(&slCursor);
        return ilRC;
     }
     close_my_cursor(&slCursor);
     strcat(pclFieldList,"AURN,");
     strcat(pclDataList,pcgUrno);
     strcat(pclDataList,",");
     strcat(pclFieldList,"CSGN,ADID,");
     strcat(pclDataList,"TOWING,B,");
     strcat(pclFieldList,"DES3,ORG3,");
     strcat(pclDataList,pcgHomeAp);
     strcat(pclDataList,",");
     strcat(pclDataList,pcgHomeAp);
     strcat(pclDataList,",");
     strcat(pclFieldList,pclFieldListSel);
     strcat(pclDataList,pclDataListSel);
     strcpy(pclSelection,"");
     ilItemNo = get_item_no(pclFieldList,"STOD",5) + 1;
     if (ilItemNo > 0)
     {
        get_real_item(pclDate,pclDataList,ilItemNo);
        pclDate[8] = '\0';
        sprintf(pclSelection,"%s,%s,1234567,1\n%s",pclDate,pclDate,pcgUrno);
     }
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7800");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
     strcpy(pclCommand,"JOF");
     sprintf(pclSelection,"%s,,%s",pcgUrno,pclTowRecUrno);
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
  }

  return ilRC;
} /* End of HandleTowIns */


static int HandleTowUpd(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleTowUpd:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclToid[32];
  char pclAftUrno[32];
  char pclMethod[32];
  char pclCommand[32];
  int ilI;
  int ilFound;
  int ilIdx;
  char pclTable[32];
  char pclModId[32];
  char pclPriority[32];
  int ilModId;
  int ilPriority;

  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pcpSection,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_U",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Update of Towing not allowed for this interface",pclFunc);
     return ilRC;
  }
  ilRC = GetData(pclMethod,0,pcpSection);
  if (igNoData <= 0)
  {
     dbg(TRACE,"%s Towing information missing",pclFunc);
     ilRC = RC_FAIL;
     return ilRC;
  }
  ilIdx = GetDataLineIdx("TOID",0);
  if (ilIdx < 0)
  {
     dbg(TRACE,"%s Towing ID missing",pclFunc);
     ilRC = RC_FAIL;
     return ilRC;
  }
  strcpy(pclToid,&rgData[ilIdx].pclData[0]);
  sprintf(pclFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO");
  sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = 'TOW' AND STAT <> 'DELETED'",pclToid);
  sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     dbg(TRACE,"%s TOID = <%s> not found",pclFunc,pclToid);
     return ilRC;
  }
  BuildItemBuffer(pclDataBuf,"",7,",");
  get_real_item(pclAftUrno,pclDataBuf,6);
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  for (ilI = 0; ilI < igNoData; ilI++)
  {
     if (strcmp(&rgData[ilI].pclField[0],"XXXX") != 0 &&
         strcmp(&rgData[ilI].pclField[0],"TOID") != 0 && strcmp(&rgData[ilI].pclType[0],"TOID") != 0)
     {
        strcat(pclFieldList,&rgData[ilI].pclField[0]);
        strcat(pclFieldList,",");
        strcat(pclDataList,&rgData[ilI].pclData[0]);
        strcat(pclDataList,",");
     }
  }
  ilIdx = GetDataLineIdx("FTYP",0);
  if (ilIdx >= 0)
  {
     if (strcmp(&rgData[ilIdx].pclData[0],"T") != 0 && strcmp(&rgData[ilIdx].pclData[0],"G") != 0)
     {
        dbg(TRACE,"%s Wrong FTYP = <%s> found",pclFunc,&rgData[ilIdx].pclData[0]);
        ilRC = RC_FAIL;
        return ilRC;
     }
  }
  if (strlen(pclFieldList) > 0)
  {
     pclFieldList[strlen(pclFieldList)-1] = '\0';
     if (strlen(pclDataList) > 0)
        pclDataList[strlen(pclDataList)-1] = '\0';
     sprintf(pclSelection,"WHERE URNO = %s",pclAftUrno);
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"AFTTAB");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7800");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
  }

  return ilRC;
} /* End of HandleTowUpd */


static int HandleTowDel(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleTowDel:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclMethod[32];
  char pclCommand[32];
  char pclTable[32];
  char pclModId[32];
  char pclPriority[32];
  int ilModId;
  int ilPriority;
  int ilI;
  int ilFound;
  int ilIdx;
  char pclToid[32];
  char pclAftUrno[32];
  char pclRarUrno[32];

  strcpy(pclMethod,"");
  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strstr(pcpSection,&rgXml.rlLine[ilI].pclName[0]) != NULL)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_D",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Delete of Towing not allowed for this interface",pclFunc);
     return ilRC;
  }
  ilRC = GetData(pclMethod,0,pcpSection);
  ilIdx = GetDataLineIdx("TOID",0);
  if (ilIdx < 0)
  {
     dbg(TRACE,"%s Towing ID missing",pclFunc);
     ilRC = RC_FAIL;
     return ilRC;
  }
  strcpy(pclToid,&rgData[ilIdx].pclData[0]);
  sprintf(pclFieldList,"URNO,RAID,RNAM,TYPE,IFNA,RURN,HOPO");
  sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = 'TOW' AND STAT <> 'DELETED'",pclToid);
  sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     dbg(TRACE,"%s TOID = <%s> not found",pclFunc,pclToid);
     return ilRC;
  }
  BuildItemBuffer(pclDataBuf,"",7,",");
  get_real_item(pclAftUrno,pclDataBuf,6);
  get_real_item(pclRarUrno,pclDataBuf,1);
  sprintf(pclDataList,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
  sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
  sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  if (ilRCdb == DB_SUCCESS)
     commit_work();
  else
  {
     dbg(TRACE,"%s Error deleting from RARTAB",pclFunc);
     close_my_cursor(&slCursor);
     return ilRC;
  }
  close_my_cursor(&slCursor);
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  sprintf(pclSelection,"WHERE URNO = %s",pclAftUrno);
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTable,"AFTTAB");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"7800");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
  dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
  dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
  dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
  ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                       pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);

  return ilRC;
} /* End of HandleTowDel */


static int HandleOutput(char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleOutput:";
  char pclCommand[32] = "\0";
  char pclTable[32] = "\0";
  char pclSelection[1024] = "\0";
  char pclFields[1024] = "\0";
  char pclNewData[8000] = "\0";
  char pclOldData[8000] = "\0";
  char *pclTmpPtr;
  int ilI;
  char pclLine[1024] = "\0";
  int ilNoEle;
  char pclField[32] = "\0";
  char pclUrno[32] = "\0";
  int ilItemNo;
  char pclTableNames[1024] = "\0";

if (strcmp(pcpCommand,"XMLO") == 0)
{
   strcpy(pclCommand,pcpNewData);
   pclTmpPtr = GetLine(pcpOldData,1);
   if (pclTmpPtr != NULL)
     CopyLine(pclTable,pclTmpPtr);
   pclTmpPtr = GetLine(pcpOldData,2);
   if (pclTmpPtr != NULL)
     CopyLine(pclSelection,pclTmpPtr);
   pclTmpPtr = GetLine(pcpOldData,3);
   if (pclTmpPtr != NULL)
     CopyLine(pclFields,pclTmpPtr);
   pclTmpPtr = GetLine(pcpOldData,4);
   if (pclTmpPtr != NULL)
     CopyLine(pclNewData,pclTmpPtr);
   pclTmpPtr = GetLine(pcpOldData,5);
   if (pclTmpPtr != NULL)
     CopyLine(pclOldData,pclTmpPtr);
}
else
{
   strcpy(pclCommand,pcpCommand);
   strcpy(pclTable,pcpTable);
   strcpy(pclSelection,pcpSelection);
   strcpy(pclFields,pcpFields);
   strcpy(pclNewData,pcpNewData);
   strcpy(pclOldData,pcpOldData);
}

/*
dbg(TRACE,"%s Command:    <%s>",pclFunc,pclCommand);
dbg(TRACE,"%s Table:      <%s>",pclFunc,pclTable);
dbg(TRACE,"%s Selection:  <%s>",pclFunc,pclSelection);
dbg(TRACE,"%s Fields:     <%s>",pclFunc,pclFields);
dbg(TRACE,"%s Data (new): <%s>",pclFunc,pclNewData);
dbg(TRACE,"%s Data (old): <%s>",pclFunc,pclOldData);
*/

  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     strcpy(&rgXml.rlLine[ilI].pclData[0],"");
     strcpy(&rgXml.rlLine[ilI].pclOldData[0],"");
     rgXml.rlLine[ilI].ilRcvFlag = 0;
     rgXml.rlLine[ilI].ilPrcFlag = 0;
  }
  strcpy(pcgAdid,"");

  if (strcmp(pclCommand,"FRDY") == 0)
     ilRC = HandleFileReady(pclSelection);

  if (strcmp(pclCommand,"SYNC") == 0)
     ilRC = HandleDataSync(pclSelection);

  if (strcmp(pclCommand,"SPDA") == 0)
  {
     (void) tools_send_info_flag(igQueOut,0,"exchdo","","CEDA","","",pcgTwStart,pcgTwEnd,
                                 "SPDA","","","","",0);
     ilRC = HandlePDAOut("SPDA",pclSelection,"","","",0,"","ADR_PDA");
  }

  if (strcmp(pclCommand,"DLWR") == 0)
     ilRC = HandlePDAOut("DLWR",pclSelection,"","","",0,"","ADR_PDA");

  if (strcmp(pclCommand,"DLRA") == 0)
     ilRC = HandlePDAOut("DLRA",pclSelection,"","","",0,"","ADR_PDA");

  MakeTokenList(pclLine,pclSelection," ",',');
  ilNoEle = GetNoOfElements(pclLine,',');
  if (ilNoEle == 4)
  {
     get_real_item(pclField,pclLine,2);
     get_real_item(pclUrno,pclLine,4);
  }
  else
  {
     if (ilNoEle == 1)
     {
        strcpy(pclField,"URNO");
        get_real_item(pclUrno,pclLine,1);
     }
     else
     {
        ilItemNo = get_item_no(pclFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pclNewData,ilItemNo);
        }
        else
        {
           dbg(TRACE,"%s No URNO Found, can't do anything",pclFunc);
           return RC_FAIL;
        }
     }
  }

  igFtypChange = FALSE;
  igReturnFlight = 0;
  if (strcmp(pclCommand,"ACK") == 0)
  {
     ilRC = HandleAckOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
     return ilRC;
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TABLE_NAMES",CFG_STRING,pclTableNames);

  if (strcmp(pclTable,"FIDTAB") == 0)
     ilRC = HandleRemarksOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
  if (strcmp(pclTable,"ALTTAB") == 0)
     ilRC = HandleAirlineOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
  if (strcmp(pclTable,"APTTAB") == 0)
     ilRC = HandleAirportOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);

  //Frank v1.52 20121226
  //if (strcmp(pclTable,"AFTTAB") == 0 || strcmp(pclTable,pcgTableName) == 0)

  if( strncmp(pclTable,"EFPTAB",6) == 0 && strcmp(pcgTableName,"HWEVIEW") == 0)
	{
			strcpy(pclTable,pcgTableName);
			dbg(DEBUG,"Updates from EFPTAB, change it to HWEVIEW->pclTable(%s)",pclTable);
	}

  if (strcmp(pclTable,"AFTTAB") == 0 || strcmp(pclTable,pcgTableName) == 0)
  {
     if (strcmp(pcgInstallation,"BLR") == 0 && strcmp(pclCommand,"UBAT") == 0 )
     {
         ilRC = HandleBulkData(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
     }
     else
     {
         ilRC = HandleFlightOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
     }
  }
  //Frank v1.52 20121226

  if (strcmp(pclTable,"CCATAB") == 0)
     ilRC = HandleCounterOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
  if (strcmp(pclTable,"BLKTAB") == 0)
     ilRC = HandleBlktabOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
  if (strcmp(pclTable,"TLXTAB") == 0)
     ilRC = HandleSitaOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);
  if (strcmp(pclTable,"JOBTAB") == 0)
     ilRC = HandlePDAOut(pclCommand,pclUrno,pclFields,pclNewData,pclOldData,0,"","ADR_PDA");
  if (strstr(pclTableNames,pclTable) != NULL)
     ilRC = HandleTableOut(pclUrno,pclCommand,pclTable,pclFields,pclNewData,pclOldData);

  return ilRC;
} /* End of HandleOutput */

static int IsAnyOfCodeBemeNull(char *pcpFidCode,char *pcpFidBeme)
{
    if(strlen(pcpFidCode)==0 || strncmp(pcpFidBeme," ",1)==0)
    {
        dbg(TRACE,"CODE is null, exit");
        return RC_FAIL;
    }
    if(strlen(pcpFidBeme)==0 || strncmp(pcpFidBeme," ",1)==0)
    {
        dbg(TRACE,"BEME is null, exit");
        return RC_FAIL;
    }
    dbg(TRACE,"CODE,BEME are all not null,continue");
    return RC_SUCCESS;
}

static int IsAnyOfApc3Apc4ApfnNull(char *pcpAptApc3,char *pcpAptApc4,char *pcpAptApfn)
{
    if(strlen(pcpAptApc3)==0 || strncmp(pcpAptApc3," ",1)==0
        || strlen(pcpAptApc4)==0 || strncmp(pcpAptApc4," ",1)==0)
    {
        dbg(TRACE,"APC3 is null, exit");
        return RC_FAIL;
    }
    /*
    if(strlen(pcpAptApc4)==0 || strncmp(pcpAptApc4," ",1)==0)
    {
        dbg(TRACE,"APC4 is null, exit");
        return RC_FAIL;
    }
    if(strlen(pcpAptApfn)==0 || strncmp(pcpAptApfn," ",1)==0)
    {
        dbg(TRACE,"APFN is null, exit");
        return RC_FAIL;
    }
    dbg(TRACE,"APC3,APC4,APFN are all not null,continue");
    */
    return RC_SUCCESS;
}

static int IsAnyOfAlc2Alc3AlfnNull(char *pcpAltAlc2,char *pcpAltAlc3,char *pcpAltAlfn)
{
    if(strlen(pcpAltAlc2)==0 || strncmp(pcpAltAlc2," ",1)==0
        || strlen(pcpAltAlc3)==0 || strncmp(pcpAltAlc3," ",1)==0)
    {
        dbg(TRACE,"ALC2 is null, exit");
        return RC_FAIL;
    }
    /*
    if(strlen(pcpAltAlc3)==0 || strncmp(pcpAltAlc3," ",1)==0)
    {
        dbg(TRACE,"ALC3 is null, exit");
        return RC_FAIL;
    }
    if(strlen(pcpAltAlfn)==0 || strncmp(pcpAltAlfn," ",1)==0)
    {
        dbg(TRACE,"ALFN is null, exit");
        return RC_FAIL;
    }
    dbg(TRACE,"ALC2,ALC3,ALFN are all not null,continue");
    */
    return RC_SUCCESS;
}
static int HandleRemarksOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
    int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleRemarksOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt = 0;
  short slCursor = 0;
  char pclSqlBuf[2048] = "\0";
  char pclDataBuf[8000] = "\0";
  char pclSelection[1024] = "\0";
  char pclFieldList[1024] = "\0";
  char pclFidCode[16] = "\0";
  char pclFidBeme[16] = "\0";
  int ilItemNo = 0;
  int ilNoEle = 0;
  int ilI = 0;
  int ilJ = 0;
  char pclSections[1024] = "\0";
  int ilNoSec = 0;
  char pclCurSec[32] = "\0";
  int ilDatCnt = 0;
  char pclNewFieldList[8000] = "\0";
  char pclNewDataList[8000] = "\0";
  char pclOldDataList[8000] = "\0";
  int ilIdx = 0;
  char pclFkt1[8] = "\0";
  char pclCurIntf[32] = "\0";
  char pclCommand[32] = "\0";
  char pclAllowedActions[32] = "\0";
  int ilCount = 0;
    int ilcode = 0;
  int ilbeme = 0;
  char pclTmpBuf[128] = "\0";
  char pclSaveFields[1024] = "\0";
  char pclSaveNewData[32000] = "\0";
  char pclSaveOldData[32000] = "\0";
  int ilTmpNoEle = 0;
  char pclPendingAck[1024] = "\0";
  int ilSendOutput = 0;
  int ilNoIntf = 0;
  char pclMsgType[32] = "\0";

  dbg(DEBUG,"%s Fields:    <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s Data:      <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old Data:  <%s>",pclFunc,pcpOldData);

    ilNoEle = GetNoOfElements(pcpFields,',');
  ilTmpNoEle = GetNoOfElements(pcpOldData,',');
  if (ilTmpNoEle < ilNoEle)
  {
     for (ilI = ilTmpNoEle+1; ilI < ilNoEle; ilI++)
        strcat(pcpOldData,",");
  }

  if (strstr(pcpFields,"CODE") == NULL || strstr(pcpFields,"BEME") == NULL)
    {
    dbg(TRACE,"The fieldlist does not contain ALC2,ALC3 and ALFN together,exit");
    return RC_FAIL;
    }
    else
    {
            ilItemNo = get_item_no(pcpFields,"CODE",5) + 1;
            get_real_item(pclFidCode,pcpNewData,ilItemNo);
            ilItemNo = get_item_no(pcpFields,"BEME",5) + 1;
            get_real_item(pclFidBeme,pcpNewData,ilItemNo);
    }

    TrimRight(pclFidCode);
  TrimRight(pclFidBeme);

  ilRC = IsAnyOfCodeBemeNull(pclFidCode,pclFidBeme);
  if(ilRC == RC_FAIL)
  {
    return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BLR_BASCI_DATA_FIDTAB",CFG_STRING,pclSections);

  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for BLR Basic data of ALTTAB defined",pclFunc);
     return RC_FAIL;
  }

  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
    get_real_item(pclCurSec,pclSections,ilI);
    ilNoEle = GetNoOfElements(pcpFields,',');
    ilDatCnt = 0;
    ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);

    if (ilDatCnt > 0)
    {
        ilRC = GetManFieldList(pclCurSec,pclFieldList,&ilNoEle,pcpFields,TRUE);

        if (ilNoEle > 0)
      {
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

        sprintf(pclSqlBuf,"SELECT %s FROM FIDTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 1-SQL = <%s>",pclFunc,pclSqlBuf);

        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);

        if (ilRCdb != DB_SUCCESS)
        {
            dbg(TRACE,"%s 1-FID Record with URNO = <%s> not found",pclFunc,pcpUrno);
            return ilRC;
        }
        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");

        sprintf(pclNewFieldList,"%s,%s",pcpFields,pclFieldList);
        sprintf(pclNewDataList,"%s,%s",pcpNewData,pclDataBuf);
        sprintf(pclOldDataList,"%s,%s",pcpOldData,pclDataBuf);

        dbg(TRACE,"pclNewFieldList<%s>",pclNewFieldList);
        dbg(TRACE,"pclNewDataList<%s>",pclNewDataList);
        dbg(TRACE,"pclOldDataList<%s>",pclOldDataList);
        dbg(TRACE,"pclDataBuf<%s>",pclDataBuf);
      }
      else
      {
        strcpy(pclNewFieldList,pcpFields);
        strcpy(pclNewDataList,pcpNewData);
        strcpy(pclOldDataList,pcpOldData);
      }

      ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
      if (ilIdx >= 0)
      {
        ilNoEle = GetNoOfElements(pclNewFieldList,',');
        ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);

        if (ilRC != RC_SUCCESS)
         return ilRC;

        ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;

        for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
        {
            strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
          strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);

          dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
              pclFunc,pclCurIntf,pclCurSec,pclMsgType);


          strcpy(pclCommand,pcpCommand);
          if (strcmp(pcpCommand,pclFkt1) != 0)
          {
            strcpy(pclCommand,pclFkt1);
          }
          ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
          dbg(DEBUG,"pclCurIntf = %s",pclCurIntf);
          dbg(DEBUG,"pclCurSec = %s",pclCurSec);
          dbg(DEBUG,"pclAllowedActions = %s",pclAllowedActions);

                    if (ilRC != RC_SUCCESS)
          {
            dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
          }
          else
          {
            if (strstr(pclAllowedActions,pclCommand) == NULL)
            {
                dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pclCommand);
            }
            else
            {
                ilSendOutput = TRUE;
              ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                     CFG_STRING,pclPendingAck);
              if (ilRC == RC_SUCCESS && strstr(pclPendingAck,pclCurSec) != NULL)
              {
                /*
                sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s AND TYPE = 'ACK'",
                         pclCcaFlnu);
                slCursor = 0;
                slFkt = START;
                dbg(DEBUG,"%s 5-SQL = <%s>",pclFunc,pclSqlBuf);
                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                if (ilRCdb == DB_SUCCESS)
                {
                   dbg(TRACE,"%s Action not yet allowed due to pending ACK",pclFunc);
                   ilSendOutput = FALSE;
                }
                close_my_cursor(&slCursor);
                */
             }
             if (ilSendOutput == TRUE)
             {
                ilRC = MarkOutput();
                ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C");
             }
           }
          }
        }
      }
    }
  }

    return ilRC;
}

static int HandleAirlineOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
    int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleAirlineOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt = 0;
  short slCursor = 0;
  char pclSqlBuf[2048] = "\0";
  char pclDataBuf[8000] = "\0";
  char pclSelection[1024] = "\0";
  char pclFieldList[1024] = "\0";
  char pclAltAlc2[16] = "\0";
  char pclAltAlc3[16] = "\0";
  char pclAltAlfn[16] = "\0";
  int ilItemNo = 0;
  int ilNoEle = 0;
  int ilI = 0;
  int ilJ = 0;
  char pclSections[1024] = "\0";
  int ilNoSec = 0;
  char pclCurSec[32] = "\0";
  int ilDatCnt = 0;
  char pclNewFieldList[8000] = "\0";
  char pclNewDataList[8000] = "\0";
  char pclOldDataList[8000] = "\0";
  int ilIdx = 0;
  char pclFkt1[8] = "\0";
  char pclCurIntf[32] = "\0";
  char pclCommand[32] = "\0";
  char pclAllowedActions[32] = "\0";
  int ilCount = 0;
    int ilalc2 = 0;
  int ilalc3 = 0;
  int ilalfn = 0;
  char pclTmpBuf[128] = "\0";
  char pclSaveFields[1024] = "\0";
  char pclSaveNewData[32000] = "\0";
  char pclSaveOldData[32000] = "\0";
  int ilTmpNoEle = 0;
  char pclPendingAck[1024] = "\0";
  int ilSendOutput = 0;
  int ilNoIntf = 0;
  char pclMsgType[32] = "\0";

  dbg(DEBUG,"%s Fields:    <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s Data:      <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old Data:  <%s>",pclFunc,pcpOldData);

    ilNoEle = GetNoOfElements(pcpFields,',');
  ilTmpNoEle = GetNoOfElements(pcpOldData,',');
  if (ilTmpNoEle < ilNoEle)
  {
     for (ilI = ilTmpNoEle+1; ilI < ilNoEle; ilI++)
        strcat(pcpOldData,",");
  }

  if (strstr(pcpFields,"ALC2") == NULL || strstr(pcpFields,"ALC3") == NULL ||
      strstr(pcpFields,"ALFN") == NULL)
    {
    dbg(TRACE,"The fieldlist does not contain ALC2,ALC3 and ALFN together,exit");
    return RC_FAIL;
    }
    else
    {
            ilItemNo = get_item_no(pcpFields,"ALC2",5) + 1;
            get_real_item(pclAltAlc2,pcpNewData,ilItemNo);
            ilItemNo = get_item_no(pcpFields,"ALC3",5) + 1;
            get_real_item(pclAltAlc3,pcpNewData,ilItemNo);
            ilItemNo = get_item_no(pcpFields,"ALFN",5) + 1;
            get_real_item(pclAltAlfn,pcpNewData,ilItemNo);
    }

    TrimRight(pclAltAlc2);
  TrimRight(pclAltAlc3);
  TrimRight(pclAltAlfn);

  ilRC = IsAnyOfAlc2Alc3AlfnNull(pclAltAlc2,pclAltAlc3,pclAltAlfn);
  if(ilRC == RC_FAIL)
  {
    return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BLR_BASCI_DATA_ALTTAB",CFG_STRING,pclSections);

  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for BLR Basic data of ALTTAB defined",pclFunc);
     return RC_FAIL;
  }

  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
    get_real_item(pclCurSec,pclSections,ilI);
    ilNoEle = GetNoOfElements(pcpFields,',');
    ilDatCnt = 0;
    ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);

    if (ilDatCnt > 0)
    {
        ilRC = GetManFieldList(pclCurSec,pclFieldList,&ilNoEle,pcpFields,TRUE);

        if (ilNoEle > 0)
      {
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

        sprintf(pclSqlBuf,"SELECT %s FROM ALTTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 1-SQL = <%s>",pclFunc,pclSqlBuf);

        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);

        if (ilRCdb != DB_SUCCESS)
        {
            dbg(TRACE,"%s 1-ALT Record with URNO = <%s> not found",pclFunc,pcpUrno);
            return ilRC;
        }
        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");

        sprintf(pclNewFieldList,"%s,%s",pcpFields,pclFieldList);
        sprintf(pclNewDataList,"%s,%s",pcpNewData,pclDataBuf);
        sprintf(pclOldDataList,"%s,%s",pcpOldData,pclDataBuf);

        dbg(TRACE,"pclNewFieldList<%s>",pclNewFieldList);
        dbg(TRACE,"pclNewDataList<%s>",pclNewDataList);
        dbg(TRACE,"pclOldDataList<%s>",pclOldDataList);
        dbg(TRACE,"pclDataBuf<%s>",pclDataBuf);
      }
      else
      {
        strcpy(pclNewFieldList,pcpFields);
        strcpy(pclNewDataList,pcpNewData);
        strcpy(pclOldDataList,pcpOldData);
      }

      ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
      if (ilIdx >= 0)
      {
        ilNoEle = GetNoOfElements(pclNewFieldList,',');
        ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);

        if (ilRC != RC_SUCCESS)
         return ilRC;

        ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;

        for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
        {
            strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
          strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);

          dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
              pclFunc,pclCurIntf,pclCurSec,pclMsgType);
          strcpy(pclCommand,pcpCommand);
          if (strcmp(pcpCommand,pclFkt1) != 0)
          {
            strcpy(pclCommand,pclFkt1);
          }
          ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
          dbg(DEBUG,"pclCurIntf = %s",pclCurIntf);
          dbg(DEBUG,"pclCurSec = %s",pclCurSec);
          dbg(DEBUG,"pclAllowedActions = %s",pclAllowedActions);

                    if (ilRC != RC_SUCCESS)
          {
            dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
          }
          else
          {
            if (strstr(pclAllowedActions,pclCommand) == NULL)
            {
                dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pclCommand);
            }
            else
            {
                ilSendOutput = TRUE;
              ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                     CFG_STRING,pclPendingAck);
              if (ilRC == RC_SUCCESS && strstr(pclPendingAck,pclCurSec) != NULL)
              {
                /*
                sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s AND TYPE = 'ACK'",
                         pclCcaFlnu);
                slCursor = 0;
                slFkt = START;
                dbg(DEBUG,"%s 5-SQL = <%s>",pclFunc,pclSqlBuf);
                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                if (ilRCdb == DB_SUCCESS)
                {
                   dbg(TRACE,"%s Action not yet allowed due to pending ACK",pclFunc);
                   ilSendOutput = FALSE;
                }
                close_my_cursor(&slCursor);
                */
             }
             if (ilSendOutput == TRUE)
             {
                ilRC = MarkOutput();
                ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C");
             }
           }
          }
        }
      }
    }
  }

    return ilRC;
}

static int HandleAirportOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
    int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleAirportOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt = 0;
  short slCursor = 0;
  char pclSqlBuf[2048] = "\0";
  char pclDataBuf[8000] = "\0";
  char pclSelection[1024] = "\0";
  char pclFieldList[1024] = "\0";
  char pclAptApc3[16] = "\0";
  char pclAptApc4[16] = "\0";
  char pclAptApfn[16] = "\0";
  int ilItemNo = 0;
  int ilNoEle = 0;
  int ilI = 0;
  int ilJ = 0;
  char pclSections[1024] = "\0";
  int ilNoSec = 0;
  char pclCurSec[32] = "\0";
  int ilDatCnt = 0;
  char pclNewFieldList[8000] = "\0";
  char pclNewDataList[8000] = "\0";
  char pclOldDataList[8000] = "\0";
  int ilIdx = 0;
  char pclFkt1[8] = "\0";
  char pclCurIntf[32] = "\0";
  char pclCommand[32] = "\0";
  char pclAllowedActions[32] = "\0";
  int ilCount = 0;
    int ilapc3 = 0;
  int ilapc4 = 0;
  int ilapfn = 0;
  char pclTmpBuf[128] = "\0";
  char pclSaveFields[1024] = "\0";
  char pclSaveNewData[32000] = "\0";
  char pclSaveOldData[32000] = "\0";
  int ilTmpNoEle = 0;
  char pclPendingAck[1024] = "\0";
  int ilSendOutput = 0;
  int ilNoIntf = 0;
  char pclMsgType[32] = "\0";

  dbg(DEBUG,"%s Fields:    <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s Data:      <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old Data:  <%s>",pclFunc,pcpOldData);

    ilNoEle = GetNoOfElements(pcpFields,',');
  ilTmpNoEle = GetNoOfElements(pcpOldData,',');
  if (ilTmpNoEle < ilNoEle)
  {
     for (ilI = ilTmpNoEle+1; ilI < ilNoEle; ilI++)
        strcat(pcpOldData,",");
  }

  if (strstr(pcpFields,"APC3") == NULL || strstr(pcpFields,"APC4") == NULL ||
      strstr(pcpFields,"APFN") == NULL)
    {
    dbg(TRACE,"The fieldlist does not contain APC3,APC4 and APFN together,exit");
    return RC_FAIL;
    }
    else
    {
            ilItemNo = get_item_no(pcpFields,"APC3",5) + 1;
            get_real_item(pclAptApc3,pcpNewData,ilItemNo);
            ilItemNo = get_item_no(pcpFields,"APC4",5) + 1;
            get_real_item(pclAptApc4,pcpNewData,ilItemNo);
            ilItemNo = get_item_no(pcpFields,"APFN",5) + 1;
            get_real_item(pclAptApfn,pcpNewData,ilItemNo);
    }

    TrimRight(pclAptApc3);
  TrimRight(pclAptApc4);
  TrimRight(pclAptApfn);

  ilRC = IsAnyOfApc3Apc4ApfnNull(pclAptApc3,pclAptApc4,pclAptApfn);
  if(ilRC == RC_FAIL)
  {
    return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BLR_BASCI_DATA_APTTAB",CFG_STRING,pclSections);

  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for BLR Basic data of APTTAB defined",pclFunc);
     return RC_FAIL;
  }

  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
    get_real_item(pclCurSec,pclSections,ilI);
    ilNoEle = GetNoOfElements(pcpFields,',');
    ilDatCnt = 0;
    ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);

    if (ilDatCnt > 0)
    {
        ilRC = GetManFieldList(pclCurSec,pclFieldList,&ilNoEle,pcpFields,TRUE);

        if (ilNoEle > 0)
      {
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

        sprintf(pclSqlBuf,"SELECT %s FROM APTTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 1-SQL = <%s>",pclFunc,pclSqlBuf);

        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);

        if (ilRCdb != DB_SUCCESS)
        {
            dbg(TRACE,"%s 1-APT Record with URNO = <%s> not found",pclFunc,pcpUrno);
            return ilRC;
        }
        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");

        sprintf(pclNewFieldList,"%s,%s",pcpFields,pclFieldList);
        sprintf(pclNewDataList,"%s,%s",pcpNewData,pclDataBuf);
        sprintf(pclOldDataList,"%s,%s",pcpOldData,pclDataBuf);

        dbg(TRACE,"pclNewFieldList<%s>",pclNewFieldList);
        dbg(TRACE,"pclNewDataList<%s>",pclNewDataList);
        dbg(TRACE,"pclOldDataList<%s>",pclOldDataList);
        dbg(TRACE,"pclDataBuf<%s>",pclDataBuf);
      }
      else
      {
        strcpy(pclNewFieldList,pcpFields);
        strcpy(pclNewDataList,pcpNewData);
        strcpy(pclOldDataList,pcpOldData);
      }

      ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
      if (ilIdx >= 0)
      {
        ilNoEle = GetNoOfElements(pclNewFieldList,',');
        ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);

        if (ilRC != RC_SUCCESS)
         return ilRC;

        ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;

        for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
        {
            strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
          strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);

          dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
              pclFunc,pclCurIntf,pclCurSec,pclMsgType);
          strcpy(pclCommand,pcpCommand);
          if (strcmp(pcpCommand,pclFkt1) != 0)
          {
            strcpy(pclCommand,pclFkt1);
          }
          ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
          dbg(DEBUG,"pclCurIntf = %s",pclCurIntf);
          dbg(DEBUG,"pclCurSec = %s",pclCurSec);
          dbg(DEBUG,"pclAllowedActions = %s",pclAllowedActions);

                    if (ilRC != RC_SUCCESS)
          {
            dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
          }
          else
          {
            if (strstr(pclAllowedActions,pclCommand) == NULL)
            {
                dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pclCommand);
            }
            else
            {
                ilSendOutput = TRUE;
              ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                     CFG_STRING,pclPendingAck);
              if (ilRC == RC_SUCCESS && strstr(pclPendingAck,pclCurSec) != NULL)
              {
                /*
                sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s AND TYPE = 'ACK'",
                         pclCcaFlnu);
                slCursor = 0;
                slFkt = START;
                dbg(DEBUG,"%s 5-SQL = <%s>",pclFunc,pclSqlBuf);
                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                if (ilRCdb == DB_SUCCESS)
                {
                   dbg(TRACE,"%s Action not yet allowed due to pending ACK",pclFunc);
                   ilSendOutput = FALSE;
                }
                close_my_cursor(&slCursor);
                */
             }
             if (ilSendOutput == TRUE)
             {
                ilRC = MarkOutput();
                ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C");
             }
           }
          }
        }
      }
    }
  }

    return ilRC;
}

static int HandleBulkData(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
    int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleBulkData:";
  char pclFieldList[1024] = "\0";
  char pclFieldListTmp[1024] = "\0";
  int ilNoEle = 0;
  int ilI = 0;
  int ilJ = 0;
  int ilMtIdx;
  char pclSections[1024] = "\0";
  int ilNoSec = 0;
  char pclCurSec[32] = "\0";
  int ilDatCnt = 0;
  char pclNewFieldList[8000] = "\0";
  char pclNewDataList[8000] = "\0";
  char pclOldDataList[8000] = "\0";
  int ilIdx = 0;
  char pclCurIntf[32] = "\0";
  char pclCommand[32] = "\0";
  char pclAllowedActions[32] = "\0";
  int ilCount = 0;
  int ilTmpNoEle = 0;
  char pclPendingAck[1024] = "\0";
  int ilSendOutput = 0;
  int ilNoIntf = 0;
  char pclMsgType[32] = "\0";

  dbg(DEBUG,"%s Fields:    <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s Data:      <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old Data:  <%s>",pclFunc,pcpOldData);

    ilNoEle = GetNoOfElements(pcpFields,',');
  ilTmpNoEle = GetNoOfElements(pcpOldData,',');
  if (ilTmpNoEle < ilNoEle)
  {
     for (ilI = ilTmpNoEle+1; ilI < ilNoEle; ilI++)
        strcat(pcpOldData,",");
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","BULK_DATA",CFG_STRING,pclSections);

  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for BULK data",pclFunc);
     return RC_FAIL;
  }

  ilRC = GetKeyFieldList(pclFieldList,&ilNoEle);
  if (ilRC != RC_SUCCESS)
  {
    return ilRC;
  }

  memset(pclFieldListTmp,0,sizeof(pclFieldListTmp));
  if( strstr(pclFieldList,"URNO") == NULL )
  {
    //strcpy(pclFieldListTmp,"URNO,STOA,STOD,");
    strcpy(pclFieldListTmp,"URNO,");
    strcat(pclFieldListTmp,pclFieldList);
    strcat(pclFieldListTmp,"STOA,STOD");
    pclFieldListTmp[strlen(pclFieldListTmp)] = '\0';

    ilNoEle += 3;
  }

  if (strlen(pclFieldList) > 0)
  {
    pclFieldList[strlen(pclFieldList)-1] = '\0';
  }

  ilRC = StoreKeyData(pclFieldListTmp,pcpNewData,ilNoEle,pcpFields,pcpNewData,pcpNewData,pcpCommand,FALSE);

  /*
  dbg(DEBUG,"+++++++++++++++++++++++++++++++++++++++++++++++++");
  ilRC = ShowKeys();
  dbg(DEBUG,"+++++++++++++++++++++++++++++++++++++++++++++++++");
  */

  ilRC = BuildHeader(pcpCommand);
  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
    get_real_item(pclCurSec,pclSections,ilI);
    ilNoEle = GetNoOfElements(pcpFields,',');
    ilDatCnt = 0;
    ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);

    if (ilDatCnt > 0)
    {
      strcpy(pclNewFieldList,pcpFields);
      strcpy(pclNewDataList,pcpNewData);
      strcpy(pclOldDataList,pcpOldData);

      ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
      if (ilIdx >= 0)
      {
        ilNoEle = GetNoOfElements(pclNewFieldList,',');
        ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);

        if (ilRC != RC_SUCCESS)
         return ilRC;

        ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;

        for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
        {
            strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
          strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);

          ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
          if (ilMtIdx >= 0)
                    {
                        strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);
                    }

          dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
              pclFunc,pclCurIntf,pclCurSec,pclMsgType);


          strcpy(pclCommand,pcpCommand);
          ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
          dbg(DEBUG,"pclCurIntf = %s",pclCurIntf);
          dbg(DEBUG,"pclCurSec = %s",pclCurSec);
          dbg(DEBUG,"pclAllowedActions = %s",pclAllowedActions);

                    if (ilRC != RC_SUCCESS)
          {
            dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
          }
          else
          {
            if (strstr(pclAllowedActions,pclCommand) == NULL)
            {
                dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pclCommand);
            }
            else
            {

              ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                     CFG_STRING,pclPendingAck);

              ilRC = MarkOutput();
              ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C");
              ilRC = ClearSection(pclCurSec);
            }
          }
        }
      }
    }
  }
  return ilRC;
}

static int HandleFlightOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "HandleFlightOut:";
    int ilRCdb = DB_SUCCESS;
    short slFkt;
    short slCursor;
    char pclSqlBuf[2048];
    char pclDataBuf[32000] = "\0";
    char pclSelection[1024];
    char pclFieldList[1024];
    char pclDataList[1024];
    char pclTowSelection[1024];
    char pclTowFieldList[1024];
    int ilI;
    int ilJ;
    int ilNoEle;
    char pclField[8];
    char pclNewData[4000];
    char pclOldData[4000];
    int ilItemNo;
    char pclSections[1024];
    int ilNoSec;
    char pclCurSec[32];
    int ilNoIntf;
    char pclCurIntf[32];
    int ilIdx;
    int ilAtIdx;
    char pclAllowedActions[32];
    char pclNewFieldList[2048];
    char pclNewDataList[32000];
    char pclOldDataList[32000];
    int ilCount;
    int ilRaid;
    int ilDatCnt;
    char pclFkt1[16];
    char pclCommand[16];
    char pclTmpFld[16];
    char pclFtyp[16];
    char pclAurn[16];
    int ilTow;
    char pclTowId[32];
    char pclMsgType[32];
    int ilMtIdx;
    char pclFtypN[16];
    char pclFtypO[16];
    char pclFieldData[4000];
    int ilNoItem;
    char pclFieldName[8];
    //char pclManFields[] = "URNO,ADID,STOA,STOD,FLNO,CSGN,RKEY,DES3,DES4,ORG3,ORG4";
    char pclManFields[] = "URNO,ADID,STOA,STOD,FLNO,CSGN,RKEY,DES3,DES4,ORG3,ORG4,TIFD,LAND,PSTA,PSTD";
    int ilTowing;
    char pclNewAdid[8];
    char pclOldAdid[8];
    char pclSplitSections[1024];
    int ilSplitCnt;
    char pclAddFldLst[1024];
    char pclSaveFields[1024];
    char pclSaveNewData[32000];
    char pclSaveOldData[32000];
    int ilAddLocation = FALSE;
    int ilTmp;
    int ilTmpNoEle;
    char pclLocationSections[1024];
    int ilLocSec;
    int ilRetIdx;
    char pclVdgsSections[1024];
    char pclStoreForAck[1024];
    char pclPendingAck[1024];
    int ilSendOutput;
    int ilDes;
    char pclDes[8];
    int ilNoTtyp;
    char pclTtyp[16];

    //Frank v1.52 20121227
    char pclNpst[16] = "\0";
    int ilChgAdid = 0;

    // WON 20130423
    char pclChgAdidFiledTmp[1024] = "\0";
    char pclChgAdidDataTmp[1024] = "\0";
    int ilAdidB = 0;
    char pclNewDataListTmp[32000] = "\0";

    strcpy(pclSaveFields,pcpFields);
    strcpy(pclSaveNewData,pcpNewData);
    strcpy(pclSaveOldData,pcpOldData);
    if (*pcpCommand == 'D')
    {
        strcpy(pclFieldList,pcpFields);
        strcpy(pclDataBuf,pcpNewData);
        ilNoEle = GetNoOfElements(pclFieldList,',');
    }
    else
    {
        ilNoTtyp = 0;
        strcpy(pclTtyp,"");
        strcpy(pclNewFieldList,"");
        strcpy(pclNewDataList,"");
        strcpy(pclOldDataList,"");
        ilNoEle = GetNoOfElements(pcpFields,',');
        for (ilI = 1; ilI <= ilNoEle; ilI++)
        {
            get_real_item(pclFieldName,pcpFields,ilI);
            get_real_item(pclNewData,pcpNewData,ilI);
            get_real_item(pclOldData,pcpOldData,ilI);
            TrimRight(pclFieldName);
            TrimRight(pclNewData);
            TrimRight(pclOldData);

            //Frank v1.52 20121227
            //dbg(DEBUG,"%s pclFieldName<%s>",pclFunc,pclFieldName);
            //dbg(DEBUG,"%s pclNewData<%s>",pclFunc,pclNewData);
            //dbg(DEBUG,"%s pclOldData<%s>",pclFunc,pclOldData);
            //Frank v1.52 20121227

            if (strstr(pclManFields,pclFieldName) != NULL || strcmp(pclNewData,pclOldData) != 0)
            {
                strcat(pclNewFieldList,pclFieldName);
                strcat(pclNewFieldList,",");
                strcat(pclNewDataList,pclNewData);
                strcat(pclNewDataList,",");
                strcat(pclOldDataList,pclOldData);
                strcat(pclOldDataList,",");
            }
        }

        if (strlen(pclNewFieldList) > 0)
        {
            pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
            pclNewDataList[strlen(pclNewDataList)-1] = '\0';
            pclOldDataList[strlen(pclOldDataList)-1] = '\0';
        }

        strcpy(pcpFields,pclNewFieldList);
        strcpy(pcpNewData,pclNewDataList);
        strcpy(pcpOldData,pclOldDataList);
        ilRC = GetKeyFieldList(pclFieldList,&ilNoEle);

        if (ilRC != RC_SUCCESS)
            return ilRC;
        if (strlen(pclFieldList) > 0)
            pclFieldList[strlen(pclFieldList)-1] = '\0';

        if (strstr(pclFieldList,"STOA") == NULL)
        {
            //strcat(pclFieldList,",STOA");
            if(strlen(pclFieldList)==0)
            {
                //frank
                dbg(DEBUG,"pclFieldList's length is 0");
                strcat(pclFieldList,"STOA");
            }
            else
            {
                strcat(pclFieldList,",STOA");
            }
            ilNoEle++;
        }

        if (strstr(pclFieldList,"STOD") == NULL)
        {
            strcat(pclFieldList,",STOD");
            ilNoEle++;
        }

        if (strstr(pclFieldList,"ADID") == NULL)
        {
            strcat(pclFieldList,",ADID");
            ilNoEle++;
        }

        if (strstr(pclFieldList,"FTYP") == NULL)
        {
            strcat(pclFieldList,",FTYP");
            ilNoEle++;
        }

        if (strstr(pclFieldList,"AURN") == NULL)
        {
            strcat(pclFieldList,",AURN");
            ilNoEle++;
        }

        if (strstr(pclFieldList,"TTYP") == NULL)
        {
            strcat(pclFieldList,",TTYP");
            ilNoEle++;
            ilNoTtyp = ilNoEle;
        }
        else
        {
            ilNoTtyp = get_item_no(pclFieldList,"TTYP",5) + 1;
        }
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

        //Frank v1.52 20121226
        //sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
        sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pcpTable,pclSelection);
        //Frank v1.52 20121226

        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 1-SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
            dbg(TRACE,"%s Flight with URNO = <%s> not found",pclFunc,pcpUrno);
            return ilRC;
        }

        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
        get_real_item(pclTtyp,pclDataBuf,ilNoTtyp);
    }

    ilRC = CheckIfActualData("F",pclFieldList,pclDataBuf,ilNoEle);

    if (ilRC != RC_SUCCESS)
        return ilRC;

    if (igReturnFlight != 3)
    {
        ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
        get_real_item(pcgAdid,pclDataBuf,ilItemNo);
    }

    AddAdid_EFPTAB_HWE(pcpTable,pcpCommand,pclFieldList,pcpFields,pcpNewData,pclDataBuf,pcpOldData);

    ilItemNo = get_item_no(pclFieldList,"FTYP",5) + 1;
    get_real_item(pclFtyp,pclDataBuf,ilItemNo);
    ilItemNo = get_item_no(pcpFields,"FTYP",5) + 1;

    //Frank v1.52 20121227
    if(strcmp(pcgDXBHoneywell,"YES") == 0 && strcmp(pcgDXBCustomization,"YES") == 0 && strcmp(pcgInstallation,"DXB") == 0)
    {
        dbg(DEBUG,"calling FindNextStandId");
        FindNextStandId(pcpUrno,pclNpst);
    }

    //Frank v1.52 20121227
    if (ilItemNo > 0 && *pcpCommand == 'U')
    {
        get_real_item(pclFtypN,pcpNewData,ilItemNo);
        get_real_item(pclFtypO,pcpOldData,ilItemNo);
        TrimRight(pclFtypN);
        TrimRight(pclFtypO);

        /*Frank 20130523 v1.57*/
    		dbg(DEBUG,"%s line<%d>pclFtypN<%s>pclFtypO<%s>",pclFunc,__LINE__,pclFtypN,pclFtypO);

        if (*pclFtypN == ' ' || *pclFtypO == ' ')
        {
            if (*pcgAdid == 'D')
                ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_D_FTYP_CHANGE",CFG_STRING,pclSections);
            else if (*pcgAdid == 'A')
                ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_A_FTYP_CHANGE",CFG_STRING,pclSections);

            if (ilRC != RC_SUCCESS)
            {
                dbg(TRACE,"%s No Section(s) for Flight Updates defined",pclFunc);
                return RC_FAIL;
            }
            sprintf(pclNewFieldList,"%s,",pcpFields);
            ilNoEle = GetNoOfElements(pcpFields,',');
            ilNoSec = GetNoOfElements(pclSections,',');
            for (ilI = 1; ilI <= ilNoSec; ilI++)
            {
                get_real_item(pclCurSec,pclSections,ilI);
                ilRC = GetFieldList(pclCurSec,pclNewFieldList,&ilNoEle);
            }

            if (strlen(pclNewFieldList) > 0)
                pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
            sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

            //Frank v1.52 20121226
            //sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclNewFieldList,pclSelection);
            sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclNewFieldList,pcpTable,pclSelection);
      		//Frank v1.52 20121226

            slCursor = 0;
            slFkt = START;
            dbg(DEBUG,"%s 2-SQL = <%s>",pclFunc,pclSqlBuf);
            ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
            close_my_cursor(&slCursor);
            if (ilRCdb != DB_SUCCESS)
            {
                dbg(TRACE,"%s Flight with URNO = <%s> not found",pclFunc,pcpUrno);
                return ilRC;
            }

            strcpy(pclNewDataList,"");
            strcpy(pclOldDataList,"");
            ilNoItem = GetNoOfElements(pcpFields,',');
            BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
            for (ilJ = 0; ilJ < ilNoEle; ilJ++)
            {
                if (ilJ < ilNoItem)
                {
                    get_real_item(pclField,pcpFields,ilJ+1);
                    ilItemNo = get_item_no(pcpFields,pclField,5) + 1;
                    get_real_item(pclFieldData,pcpNewData,ilItemNo);
                    strcat(pclNewDataList,pclFieldData);
                    strcat(pclNewDataList,",");
                    strcat(pclOldDataList,",");
                }
                else
                {
                   get_real_item(pclFieldData,pclDataBuf,ilJ+1);
                   strcat(pclNewDataList,pclFieldData);
                   strcat(pclNewDataList,",");
                   strcat(pclOldDataList,",");
                }
            }
            pclNewDataList[strlen(pclNewDataList)-1] = '\0';
            pclOldDataList[strlen(pclOldDataList)-1] = '\0';
            igFtypChange = TRUE;
            ilRC = HandleFlightOut(pcpUrno,"DFR",pcpTable,pclNewFieldList,pclNewDataList,pclOldDataList);
            ilRC = HandleFlightOut(pcpUrno,"IFR",pcpTable,pclNewFieldList,pclNewDataList,pclOldDataList);
            return ilRC;
        }
    }

    strcpy(pclTowId,"");
    ilTowing = FALSE;
	dbg( TRACE, "%d:DatCnt <%d> ilNoEle <%d> pcpCmd <%c>", __LINE__,ilDatCnt,ilNoEle,pcpCommand[0]);

    if (igNBIATowing == TRUE && *pcgAdid == 'B' && (*pclFtyp == 'T' || *pclFtyp == 'G'))
    {
        ilTowing = TRUE;
        ilItemNo = get_item_no(pclFieldList,"AURN",5) + 1;
        if ( ilItemNo )
            get_real_item(pclAurn,pclDataBuf,ilItemNo);
        else
            pclAurn[0]=0;

        if (*pclAurn == '\0' || *pclAurn == ' ')
        {
            sprintf(pclTowFieldList,"RAID,RNAM");
            if (*pcpCommand == 'D')
                sprintf(pclTowSelection,"WHERE RURN = %s AND TYPE = 'TOW' AND STAT = 'DELETED'",pcpUrno);
            else
                sprintf(pclTowSelection,"WHERE RURN = %s AND TYPE = 'TOW' AND STAT <> 'DELETED'",pcpUrno);
            sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclTowFieldList,pclTowSelection);
            slCursor = 0;
            slFkt = START;
            dbg(DEBUG,"%s 3-SQL = <%s>",pclFunc,pclSqlBuf);
            ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
            close_my_cursor(&slCursor);
            if (ilRCdb == DB_SUCCESS)
            {
                BuildItemBuffer(pclDataBuf,"",2,",");
                get_real_item(pclTowId,pclDataBuf,1);
                TrimRight(pclTowId);
                get_real_item(pclAurn,pclDataBuf,2);
                TrimRight(pclTowId);
            }
            else
            {
                dbg(TRACE,"%s No Towing ID found",pclFunc);
                return ilRC;
            }
        }
        sprintf(pclSelection,"WHERE URNO = %s",pclAurn);

        //Frank v1.52 20121226
        //sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
        sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pcpTable,pclSelection);
        //Frank v1.52 20121226

        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 4-SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
            dbg(TRACE,"%s Flight with URNO = <%s> not found",pclFunc,pclAurn);
            return ilRC;
        }
        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
        strcat(pclFieldList,",URNO");
        strcat(pclDataBuf,",");
        strcat(pclDataBuf,pclAurn);
        ilNoEle++;
    }
    else
    {
        strcat(pclFieldList,",URNO");
        strcat(pclDataBuf,",");
        strcat(pclDataBuf,pcpUrno);
        ilNoEle++;
    }

    ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
    if (igNBIAReturnFlight == TRUE && ilItemNo > 0 && *pcpCommand == 'U')
    {
        get_real_item(pclNewAdid,pcpNewData,ilItemNo);
        get_real_item(pclOldAdid,pcpOldData,ilItemNo);
        TrimRight(pclNewAdid);
        TrimRight(pclOldAdid);
        if (strcmp(pclNewAdid,pclOldAdid) != 0 && *pclFtyp != 'T' && *pclFtyp != 'G')
        {
            igReturnFlight = 0;
            if (*pclNewAdid == 'B')
            {
                strcpy(pclNewDataList,pcpNewData);
                ilRC = ReplaceItem(pclNewDataList,ilItemNo,"D");
                strcpy(pclOldDataList,pcpOldData);
                ilRC = ReplaceItem(pclOldDataList,ilItemNo,"D");
                ilRC = HandleFlightOut(pcpUrno,"UFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
                if (strcmp(pcgHandleReturnFlights,"AUTO") == 0)
                {
                    strcpy(pclNewDataList,pcpNewData);
                    ilRC = ReplaceItem(pclNewDataList,ilItemNo,"A");
                    strcpy(pclOldDataList,pcpOldData);
                    ilRC = ReplaceItem(pclOldDataList,ilItemNo,"A");
                    strcpy(pclOldDataList,"");
                    ilDes = get_item_no(pcpFields,"ORG3",5) + 1;
                    if (ilDes > 0)
                    {
                        get_real_item(pclDes,pclNewDataList,ilDes);
                        if (*pclDes == ' ' || *pclDes == '\0')
                            ilRC = ReplaceItem(pclNewDataList,ilDes,pcgHomeAp);
                    }
                    else
                    {
                        strcat(pcpFields,",ORG3");
                        strcat(pclNewDataList,",");
                        strcat(pclNewDataList,pcgHomeAp);
                    }
                    ilDes = get_item_no(pcpFields,"ORG4",5) + 1;
                    if (ilDes > 0)
                    {
                        get_real_item(pclDes,pclNewDataList,ilDes);
                        if (*pclDes == ' ' || *pclDes == '\0')
                        {
                            ilCount = 1;
                            ilRC = syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pclDes,&ilCount,"\n");
                            ilRC = ReplaceItem(pclNewDataList,ilDes,pclDes);
                        }
                    }
                    else
                    {
                        ilCount = 1;
                        ilRC = syslibSearchDbData("APTTAB","APC3",pcgHomeAp,"APC4",pclDes,&ilCount,"\n");
                        strcat(pcpFields,",ORG4");
                        strcat(pclNewDataList,",");
                        strcat(pclNewDataList,pclDes);
                    }
                    ilDes = get_item_no(pcpFields,"TTYP",5) + 1;
                    if (ilDes <= 0)
                    {
                        strcat(pcpFields,",TTYP");
                        strcat(pclNewDataList,",");
                        strcat(pclNewDataList,pclTtyp);
                    }
                    igReturnFlight = 2;
                    ilRC = HandleFlightOut(pcpUrno,"IFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
                }
                else
                {
                    dbg(TRACE,"%s Save URNO <%s> of Return Flight , Index = %d",pclFunc,pcpUrno,igSavRetCnt);
                    strcpy(&pcgSaveReturnFlights[igSavRetCnt][0],pcpUrno);
                    igSavRetCnt++;
                }
            }
            else
            {
                if (*pclNewAdid == 'D')
                {
                    strcpy(pclNewDataList,pcpNewData);
                    ilRC = ReplaceItem(pclNewDataList,ilItemNo,"A");
                    strcpy(pclOldDataList,pcpOldData);
                    ilRC = ReplaceItem(pclOldDataList,ilItemNo,"A");
                    igReturnFlight = 1;
                    ilRC = HandleFlightOut(pcpUrno,"DFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
                    strcpy(pclNewDataList,pcpNewData);
                    ilRC = ReplaceItem(pclNewDataList,ilItemNo,"D");
                    strcpy(pclOldDataList,pcpOldData);
                    ilRC = ReplaceItem(pclOldDataList,ilItemNo,"D");
                    igReturnFlight = 0;
                    ilRC = HandleFlightOut(pcpUrno,"UFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
                }
                else
                {
                    dbg(TRACE,"%s Unknown New/Old ADID Combination <%s>/<%s>",pclFunc,pclNewAdid,pclOldAdid);
                }
            }
            igReturnFlight = 0;
            return ilRC;
        }
        else
        {
            if (*pclNewAdid == 'B' && *pclFtyp != 'T' && *pclFtyp != 'G')
            {
                ilRetIdx = -1;
                for (ilI = 0; ilI < igSavRetCnt && ilRetIdx < 0; ilI++)
                {
                    if (strcmp(&pcgSaveReturnFlights[ilI][0],pcpUrno) == 0)
                        ilRetIdx = ilI;
                }

                if (ilRetIdx >= 0)
                {
                    for (ilI = ilRetIdx; ilI < igSavRetCnt; ilI++)
                    {
                        strcpy(&pcgSaveReturnFlights[ilI][0],&pcgSaveReturnFlights[ilI+1][0]);
                    }
                    igSavRetCnt--;
                    dbg(TRACE,"%s Current Number of Return Flights in Cache = %d",pclFunc,igSavRetCnt);
                    strcpy(pcpFields,pclSaveFields);
                    strcpy(pclNewDataList,pclSaveNewData);
                    ilRC = ReplaceItem(pclNewDataList,ilItemNo,"A");
                    strcpy(pclOldDataList,"");
                    igReturnFlight = 2;
                    ilRC = HandleFlightOut(pcpUrno,"IFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
                    igReturnFlight = 0;
                    return ilRC;
                }
            }
        }
    }

    ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
    if (igNBIAReturnFlight == TRUE && ilItemNo > 0 && *pcpCommand == 'I')
    {
        get_real_item(pclNewAdid,pcpNewData,ilItemNo);
        TrimRight(pclNewAdid);
        if (*pclNewAdid == 'B' && *pclFtyp != 'T' && *pclFtyp != 'G')
        {
            igReturnFlight = 3;
            strcpy(pclNewDataList,pcpNewData);
            ilRC = ReplaceItem(pclNewDataList,ilItemNo,"D");
            strcpy(pclOldDataList,pcpOldData);
            ilRC = ReplaceItem(pclOldDataList,ilItemNo,"D");
            strcpy(pcgAdid,"D");
            ilRC = HandleFlightOut(pcpUrno,"IFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
            if (strcmp(pcgHandleReturnFlights,"AUTO") == 0)
            {
                strcpy(pclNewDataList,pcpNewData);
                ilRC = ReplaceItem(pclNewDataList,ilItemNo,"A");
                strcpy(pclOldDataList,pcpOldData);
                ilRC = ReplaceItem(pclOldDataList,ilItemNo,"A");
                strcpy(pcgAdid,"A");
                ilRC = HandleFlightOut(pcpUrno,"IFR",pcpTable,pcpFields,pclNewDataList,pclOldDataList);
            }
            else
            {
                dbg(TRACE,"%s Save URNO <%s> of TurnAround Flight , Index = %d",pclFunc,pcpUrno,igSavRetCnt);
                strcpy(&pcgSaveReturnFlights[igSavRetCnt][0],pcpUrno);
                igSavRetCnt++;
            }
            igReturnFlight = 0;
            return ilRC;
        }
        else
        {
            if (*pclNewAdid == 'D' && strlen(pcgDesForTouchAndGo) > 0)
            {
                ilDes = get_item_no(pcpFields,"DES3",5) + 1;
                if (ilDes > 0)
                {
                    get_real_item(pclDes,pcpNewData,ilDes);
                    if (*pclDes == '\0' || *pclDes == ' ')
                    {
                        get_real_item(pclDes,pcgDesForTouchAndGo,1);
                        ilRC = ReplaceItem(pcpNewData,ilDes,pclDes);
                    }
                }
                else
                {
                    get_real_item(pclDes,pcgDesForTouchAndGo,1);
                    strcat(pcpFields,",DES3");
                    strcat(pcpNewData,",");
                    strcat(pcpNewData,pclDes);
                    strcat(pcpOldData,",");
                }

                ilDes = get_item_no(pcpFields,"DES4",5) + 1;
                if (ilDes > 0)
                {
                    get_real_item(pclDes,pcpNewData,ilDes);
                    if (*pclDes == '\0' || *pclDes == ' ')
                    {
                        get_real_item(pclDes,pcgDesForTouchAndGo,2);
                       ilRC = ReplaceItem(pcpNewData,ilDes,pclDes);
                    }
                }
                else
                {
                    get_real_item(pclDes,pcgDesForTouchAndGo,2);
                    strcat(pcpFields,",DES4");
                    strcat(pcpNewData,",");
                    strcat(pcpNewData,pclDes);
                    strcat(pcpOldData,",");
                }
            }
        }
    }

    if (igNBIAReturnFlight == TRUE && *pcpCommand == 'U')
    {
        /*if (*pcgAdid == 'B' && *pclFtyp != 'Z' && *pclFtyp != 'B' && *pclFtyp != 'T' && *pclFtyp != 'G')*/
        if (*pcgAdid == 'B' && *pclFtyp != 'T' && *pclFtyp != 'G')
        {
            ilRC = CheckIfDepUpd(pclSaveFields,pclSaveNewData,pclSaveOldData);
            if (ilRC == RC_SUCCESS)
            {
                strcpy(pcgAdid,"D");
                ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
                ilRC = ReplaceItem(pclDataBuf,ilItemNo,"D");
                ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
                ilRC = ReplaceItem(pcpNewData,ilItemNo,"D");
                ilRC = ReplaceItem(pcpOldData,ilItemNo,"D");
            }
            else
            {
                strcpy(pcgAdid,"A");
                ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
                ilRC = ReplaceItem(pclDataBuf,ilItemNo,"A");
                ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
                ilRC = ReplaceItem(pcpNewData,ilItemNo,"A");
                ilRC = ReplaceItem(pcpOldData,ilItemNo,"A");
            }
        }
    }

	dbg( TRACE, "%d:DatCnt <%d> ilNoEle <%d> pcpCmd <%c>", __LINE__,ilDatCnt,ilNoEle,pcpCommand[0]);
    ilRC = StoreKeyData(pclFieldList,pclDataBuf,ilNoEle,pcpFields,pcpNewData,pcpOldData,pcpCommand,ilTowing);

    /*
    dbg(DEBUG,"+++++++++++++++++++++++++++++++++++++++++++++++++");
    ilRC = ShowKeys();
    dbg(DEBUG,"+++++++++++++++++++++++++++++++++++++++++++++++++");
    */

    if (ilRC != RC_SUCCESS)
        return ilRC;

    /*ilRC = ShowKeys();*/
    if (*pcgAdid == 'D')
    {
        if (igFtypChange == FALSE)
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_D_SECTION",CFG_STRING,pclSections);
        else
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_D_FTYP_CHANGE",CFG_STRING,pclSections);
    }
    else if (*pcgAdid == 'A')
    {
        if (igFtypChange == FALSE)
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_A_SECTION",CFG_STRING,pclSections);
        else
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_A_FTYP_CHANGE",CFG_STRING,pclSections);
    }
    else
    {
        if (*pclFtyp == 'T' || *pclFtyp == 'G')
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING_SECTION",CFG_STRING,pclSections);
        else
            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FLIGHT_B_SECTION",CFG_STRING,pclSections);
    }

    if (ilRC != RC_SUCCESS)
    {
        dbg(TRACE,"%s No Section(s) for Flight Updates defined",pclFunc);
        return RC_FAIL;
    }

    ilAddLocation = FALSE;
    dbg(DEBUG,"%s",pclFunc);
    dbg(DEBUG,"%s =================================== NEW SECTION ===================================",pclFunc);
	dbg( TRACE, "%d:DatCnt <%d> ilNoEle <%d> pcpCmd <%c>pclSections<%s>", __LINE__,ilDatCnt,ilNoEle,pcpCommand[0],pclSections);
    ilRC = BuildHeader(pcpCommand);
    strcpy(pclLocationSections,"");
    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","LOCATION_SECTIONS",CFG_STRING,pclLocationSections);
    strcpy(pclSplitSections,"");
    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SPLIT_SECTIONS",CFG_STRING,pclSplitSections);
    ilSplitCnt = 0;
    ilNoSec = GetNoOfElements(pclSections,',');

    for (ilI = 1; ilI <= ilNoSec; ilI++)
    {
        strcpy(pclAddFldLst,"");
        get_real_item(pclCurSec,pclSections,ilI);
        ilNoEle = GetNoOfElements(pcpFields,',');

        dbg(DEBUG,"<%d>pcpFields = <%s>",__LINE__,pcpFields);
        dbg(DEBUG,"<%d>ilNoEle = <%d>",__LINE__,ilNoEle);

        //Frank v1.52 20121227
        memset(pclNewDataListTmp,0,sizeof(pclNewDataListTmp));
        if(strncmp(pcgChgAdid,"YES",3)==0)
        {
     	    for(ilChgAdid=0;ilChgAdid<ilNoEle;ilChgAdid++)
            {
                memset(pclChgAdidFiledTmp,0,sizeof(pclChgAdidFiledTmp));
                memset(pclChgAdidDataTmp,0,sizeof(pclChgAdidDataTmp));

                GetDataItem(pclChgAdidFiledTmp,pcpFields,ilChgAdid+1,',',"","\0\0");
                GetDataItem(pclChgAdidDataTmp,pcpNewData,ilChgAdid+1,',',"","\0\0");

                if(strncmp(pclChgAdidFiledTmp,"ADID",4)==0)
                {
                    if( !strncmp(pclChgAdidDataTmp,"A",1) )
                    {
                        strcat(pclNewDataListTmp,"inbound");
                    }
                    else if( !strncmp(pclChgAdidDataTmp,"D",1) )
                    {
                        strcat(pclNewDataListTmp,"outbound");
                    }
                    else if( !strncmp(pclChgAdidDataTmp,"B",1) && !strncmp(pclFtyp,"T",1) )
                    {
                        strcat(pclNewDataListTmp,"towing");
                    }
                    else
                    {
                        strcat(pclNewDataListTmp,"unknown");
                    }
                }
                else
                {
                    strcat(pclNewDataListTmp,pclChgAdidDataTmp);
                }

                if(ilChgAdid < ilNoEle )
                {
                    strcat(pclNewDataListTmp,",");
                }
            }
            strcpy(pcpNewData,pclNewDataListTmp);
        }

        //Frank v1.52 20121227

        ilDatCnt = 0;
        ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);

        if (strstr(pclSplitSections,pclCurSec) != NULL && ilAddLocation == FALSE)
        {
            ilSplitCnt++;
            if (ilSplitCnt <= 2)
                ilRC = RemoveData(pcpFields,ilNoEle,pclCurSec,&ilDatCnt,ilSplitCnt);
        }

        if (ilDatCnt > 0)
        {
            if (strstr(pclLocationSections,pclCurSec) == NULL)
                ilLocSec = FALSE;
            else
                ilLocSec = TRUE;
            ilRC = GetManFieldList(pclCurSec,pclFieldList,&ilNoEle,pcpFields,ilLocSec);

            //Frank v1.52 20121227
            dbg(DEBUG,"%s %d GetManFieldList pclFieldList<%s>",pclFunc,__LINE__,pclFieldList);

            dbg( TRACE, "%d:DatCnt <%d> ilNoEle <%d> pcpCmd <%c>", __LINE__,ilDatCnt,ilNoEle,pcpCommand[0]);
            if (ilNoEle > 0 && *pcpCommand != 'D')
            {
                sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

                //Frank v1.52 20121226
                //sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
                sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pcpTable,pclSelection);
                //Frank v1.52 20121226

                slCursor = 0;
                slFkt = START;
                dbg(DEBUG,"%s 5-SQL = <%s>",pclFunc,pclSqlBuf);
                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                close_my_cursor(&slCursor);
                if (ilRCdb != DB_SUCCESS)
                {
                    dbg(TRACE,"%s Flight with URNO = <%s> not found",pclFunc,pcpUrno);
                    return ilRC;
                }
                BuildItemBuffer(pclDataBuf,"",ilNoEle,",");

                sprintf(pclNewFieldList,"%s,%s",pcpFields,pclFieldList);

                //Frank v1.52 20121228
                if( strcmp(pcgDXBHoneywell,"YES") == 0 )
                {
                    sprintf(pclNewDataList,"%s%s",pcpNewData,pclDataBuf);
                }
                else
                {
                    sprintf(pclNewDataList,"%s,%s",pcpNewData,pclDataBuf);
                }

                //Frank v1.52 20121227
                /*
                dbg(DEBUG,"%s pcpFields<%s>",pclFunc,pcpFields);
                dbg(DEBUG,"%s pclFieldList<%s>",pclFunc,pclFieldList);
                dbg(DEBUG,"%s pclNewFieldList<%s>",pclFunc,pclNewFieldList);

                dbg(DEBUG,"%s pcpNewData<%s>",pclFunc,pcpNewData);
                dbg(DEBUG,"%s pclDataBuf<%s>",pclFunc,pclDataBuf);
                dbg(DEBUG,"%s pclNewDataList<%s>",pclFunc,pclNewDataList);
                */

                if (ilLocSec == TRUE)
                    sprintf(pclOldDataList,"%s,%s",pcpOldData,pclDataBuf);
                else
                {
                    sprintf(pclOldDataList,"%s",pcpOldData);
                    for (ilJ = 0; ilJ < ilNoEle; ilJ++)
                        strcat(pclOldDataList,",");
                }
            }
            else
            {
                strcpy(pclNewFieldList,pcpFields);
                strcpy(pclNewDataList,pcpNewData);
                strcpy(pclOldDataList,pcpOldData);
            }

            ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
            if (ilIdx >= 0)
            {
                ilTow = GetXmlLineIdx("DAT","TOID",pclCurSec);
                if (ilTow >= 0)
                {
                    if (strlen(pclTowId) == 0)
                    {
                        sprintf(pclFieldList,"RAID");
                        if (*pcpCommand == 'D')
                            sprintf(pclSelection,"WHERE RURN = %s AND TYPE = 'TOW' AND STAT = 'DELETED'",pcpUrno);
                        else
                            sprintf(pclSelection,"WHERE RURN = %s AND TYPE = 'TOW' AND STAT <> 'DELETED'",pcpUrno);

                        sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
                        slCursor = 0;
                        slFkt = START;
                        dbg(DEBUG,"%s 6-SQL = <%s>",pclFunc,pclSqlBuf);
                        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclTowId);
                        close_my_cursor(&slCursor);
                        if (ilRCdb == DB_SUCCESS)
                        {
                            TrimRight(pclTowId);
                            strcat(pclNewFieldList,",");
                            strcat(pclNewFieldList,&rgXml.rlLine[ilTow].pclName[0]);
                            strcat(pclNewDataList,",");
                            strcat(pclNewDataList,pclTowId);
                            strcat(pclOldDataList,", ");
                        }
                        else
                        {
                            dbg(TRACE,"%s No Towing ID found",pclFunc);
                        }
                    }
                    else
                    {
                        strcat(pclNewFieldList,",");
                        strcat(pclNewFieldList,&rgXml.rlLine[ilTow].pclName[0]);
                        strcat(pclNewDataList,",");
                        strcat(pclNewDataList,pclTowId);
                        strcat(pclOldDataList,", ");
                    }
                }
                ilNoEle = GetNoOfElements(pclNewFieldList,',');

                if(strcmp(pcgDXBHoneywell,"YES") == 0 && strcmp(pcgDXBCustomization,"YES") == 0 && strcmp(pcgInstallation,"DXB") == 0)
                {
                    strcat(pclNewFieldList,",NPST");
                    //strcat(pclNewDataList,",");
                    strcat(pclNewDataList,pclNpst);
                    strcat(pclOldDataList,",");
                    ilNoEle++;
   			    }

   			    //dbg(DEBUG,"<%d>pclNewDataListTmp = <%s>",__LINE__,pclNewDataListTmp);
                dbg(DEBUG,"<%d>pclNewFieldList = <%s>",__LINE__,pclNewFieldList);
                dbg(DEBUG,"<%d>pclNewDataList = <%s>",__LINE__,pclNewDataList);
                dbg(DEBUG,"<%d>pclOldDataList = <%s>",__LINE__,pclOldDataList);
                dbg(DEBUG,"<%d>ilNoEle = <%d>",__LINE__,ilNoEle);

                ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);

                if (ilRC != RC_SUCCESS)
                    return ilRC;

                if (ilSplitCnt > 0)
                {
                    ilRC = RemoveData(pcpFields,ilNoEle,pclCurSec,&ilDatCnt,ilSplitCnt);
                }

                strcpy(pclFkt1,pcpCommand);
                ilRaid = GetXmlLineIdx("DAT","RAID",pclCurSec);
                if (ilRaid >= 0)
                    ilRC = HandleRaid(pcpUrno,ilRaid,pclCurSec,pclNewFieldList,pclNewDataList,pclOldDataList,pclFkt1,NULL,"",pclAddFldLst);

                ilAtIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
                ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;

                for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
                {
                    strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
                    strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);
                    ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
                    if (ilMtIdx >= 0)
                        strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);

                    dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
                        pclFunc,pclCurIntf,pclCurSec,pclMsgType);

                    dbg(DEBUG,"igChangeTheMessageTypeForBlrPortalDailyBulkData<%d>",igChangeTheMessageTypeForBlrPortalDailyBulkData);
                    if( igChangeTheMessageTypeForBlrPortalDailyBulkData == 1)
                    {
                        memset(&rgXml.rlLine[ilMtIdx].pclData[0],0,sizeof(&rgXml.rlLine[ilMtIdx].pclData[0]));
                        strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],"UFISFLTSC");
                        dbg(DEBUG,"%s +++++++++++++++Build Data for Interface <%s> and Section <%s>, Message Type = <UFISFLTSC>",pclFunc,pclCurIntf,pclCurSec);
                    }

                    dbg(DEBUG,"line<%d>pcpCommand<%s>",__LINE__,pcpCommand);
                    strcpy(pclCommand,pcpCommand);
                    if (strcmp(pcpCommand,pclFkt1) != 0)
                    {
                        if (*pcgAdid == 'D')
                            strcpy(pclTmpFld,&rgXml.rlLine[ilRaid].pclFieldD[ilJ][0]);
                        else
                            strcpy(pclTmpFld,&rgXml.rlLine[ilRaid].pclFieldA[ilJ][0]);

                        if (strcmp(pclTmpFld,"XXXX") != 0)
                        {
                            strcpy(pclCommand,pclFkt1);
                            if (ilAtIdx >= 0)
                                rgXml.rlLine[ilAtIdx].pclData[0] = *pclCommand;
                        }
                    }

                    ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);

                    if (ilRC != RC_SUCCESS)
                    {
                        dbg(TRACE,"%s Action not allowed for this Interface and/or Section",pclFunc);
                    }
                    else
                    {
                        if (strstr(pclAllowedActions,pclCommand) == NULL)
                        {
                            dbg(TRACE,"%s Action = <%s> not allowed for this Interface and/or Section",
                                pclFunc,pclCommand);
                        }
                        else
                        {
                            ilSendOutput = TRUE;
                            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                                   CFG_STRING,pclPendingAck);
                            if (ilRC == RC_SUCCESS && strstr(pclPendingAck,pclCurSec) != NULL)
                            {
                                sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s AND TYPE = 'ACK'",pcpUrno);
                                slCursor = 0;
                                slFkt = START;
                                dbg(DEBUG,"%s 7-SQL = <%s>",pclFunc,pclSqlBuf);
                                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                                if (ilRCdb == DB_SUCCESS)
                                {
                                    dbg(TRACE,"%s Action not yet allowed due to pending ACK",pclFunc);
                                    ilSendOutput = FALSE;
                                }
                                close_my_cursor(&slCursor);
                            }

                            if (ilSendOutput == TRUE)
                            {
                                /*ilRC = ShowData();*/
                                ilRC = MarkOutput();
                                /*ilRC = ShowAll();*/
                                ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","VDGS_SECTIONS",CFG_STRING,
                                                       pclVdgsSections);

                                dbg(DEBUG,"%s pclVdgsSections<%s>",pclFunc,pclVdgsSections);
                                dbg(DEBUG,"%s ilRC<%d>",pclFunc,ilRC);
                                dbg(DEBUG,"%s pclCurSec<%s>",pclFunc,pclCurSec);
                                dbg(DEBUG,"%s pcgInstallation<%s>",pclFunc,pcgInstallation);

                                if (ilRC == RC_SUCCESS && strstr(pclVdgsSections,pclCurSec) != NULL)
                                {
                                    if (strcmp(pcgInstallation,"WAW") == 0)
                                        ilRC = HardCodedVDGSWAW(pcpCommand,pcpFields,pcpNewData,pcpOldData,pclCurSec);
                                    else if (strcmp(pcgInstallation,"DXB") == 0)
                                        ilRC = HardCodedVDGSDXB(pcpCommand,pcpFields,pcpNewData,pcpOldData,pclCurSec);
                                    else
                                        ilRC = HardCodedVDGS(pcpCommand,pcpFields,pcpNewData,pcpOldData,pclCurSec);
                                }
                                else
                                    ilRC = RC_SUCCESS;

                                if (ilRC == RC_SUCCESS)
                                {
                                    if (strcmp(pclCurIntf,"ADR_PDA") == 0)
                                        ilRC = HandlePDAOut("FLIGHT",pcpUrno,"","","",ilJ,pclCurSec,pclCurIntf);
                                    else
                                    {
                                        //ShowData();
                                        //ShowAll();

                                        //Frank v1.52 20121226
                                        if(strcmp(pcgDXBCustomization,"YES")==0 && strcmp(pcgInstallation,"DXB")==0)
                                        {
                                              ilRC = BuildOutputDXB_HoneyWell(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C",pcpUrno,pclFtyp,pcpCommand);
                                        }
                                        else
                                        {
                                      	    ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,NULL,pclCurIntf,"C");
                                        }

                                        //Frank v1.52 20121226
                                        if (*pclCommand == 'I')
                                        {
                                            ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","STORE_FOR_ACK_SECTIONS",
                                                                   CFG_STRING,pclStoreForAck);
                                            if (ilRC == RC_SUCCESS && strstr(pclStoreForAck,pclCurSec) != NULL)
                                            {
                                                strcpy(pclFieldList,"URNO,HOPO,TYPE,RURN,TIME");
                                                ilRC = GetNextUrno();
                                                sprintf(pclDataList,"%d,'%s','ACK',%s,'%s'",
                                                        igLastUrno,pcgHomeAp,pcpUrno,pcgCurrentTime);
                                                sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",
                                                        pclFieldList,pclDataList);
                                                slCursor = 0;
                                                slFkt = START;
                                                dbg(DEBUG,"%s 8-SQL = <%s>",pclFunc,pclSqlBuf);
                                                ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                                                if (ilRCdb == DB_SUCCESS)
                                                    commit_work();
                                                else
                                                    dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
                                                close_my_cursor(&slCursor);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        dbg(DEBUG,"%s",pclFunc);
                        dbg(DEBUG,"%s =================================== NEW SECTION ===================================",pclFunc);
                    }

                    if (ilAtIdx >= 0)
                        rgXml.rlLine[ilAtIdx].pclData[0] = *pcpCommand;
                }
                ilRC = ClearSection(pclCurSec);
            }
        }

        if (strlen(pclAddFldLst) > 0)
        {
            ilAddLocation = TRUE;
            strcpy(pclSaveFields,pcpFields);
            strcpy(pclSaveNewData,pcpNewData);
            strcpy(pclSaveOldData,pcpOldData);
            strcpy(pcpFields,"URNO,");
            strcat(pcpFields,pclAddFldLst);
            sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);

            //Frank v1.52 20121226
            //sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pcpFields,pclSelection);
            sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pcpFields,pcpTable,pclSelection);
            //Frank v1.52 20121226

            slCursor = 0;
            slFkt = START;
            dbg(DEBUG,"%s 9-SQL = <%s>",pclFunc,pclSqlBuf);
            ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pcpNewData);
            close_my_cursor(&slCursor);
            if (ilRCdb == DB_SUCCESS)
            {
                ilTmpNoEle = GetNoOfElements(pcpFields,',');
                BuildItemBuffer(pcpNewData,"",ilTmpNoEle,",");
                strcpy(pcpOldData,pcpUrno);
                for (ilTmp = 0; ilTmp < ilTmpNoEle-1; ilTmp++)
                    strcat(pcpOldData,",");
                ilI--;
            }
            else
            {
                strcpy(pcpFields,pclSaveFields);
                strcpy(pcpNewData,pclSaveNewData);
                strcpy(pcpOldData,pclSaveOldData);
                ilAddLocation = FALSE;
            }
        }
        else
        {
            if (ilAddLocation == TRUE)
            {
                strcpy(pcpFields,pclSaveFields);
                strcpy(pcpNewData,pclSaveNewData);
                strcpy(pcpOldData,pclSaveOldData);
                ilAddLocation = FALSE;
            }
            if (ilSplitCnt == 1)
                ilI--;
            if (ilSplitCnt == 2)
               ilSplitCnt = 0;
        }
    }

    return ilRC;
} /* End of HandleFlightOut */


static int MarkOutput()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "MarkOutput:";
  int ilI;
  int ilJ;
  int ilFoundLevel;
  int ilCurLevel;
  int ilFound;

  for (ilI = 0; ilI < igCurXmlLines; ilI++)
  {
     if (rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        ilFoundLevel = rgXml.rlLine[ilI].ilLevel;
        ilFound = FALSE;
        ilCurLevel = ilFoundLevel - 1;
        for (ilJ = ilI - 1; ilJ >= 0 && ilFound == FALSE; ilJ--)
        {
           if (rgXml.rlLine[ilJ].ilLevel == ilCurLevel)
           {
              if (rgXml.rlLine[ilJ].ilRcvFlag == 0)
              {
                 rgXml.rlLine[ilJ].ilRcvFlag = 1;
                 ilCurLevel--;
              }
              else
                 ilFound = TRUE;
           }
        }
        ilFound = FALSE;
        ilCurLevel = ilFoundLevel - 1;
        for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
        {
           if (rgXml.rlLine[ilJ].ilLevel == ilCurLevel)
           {
              if (rgXml.rlLine[ilJ].ilRcvFlag == 0)
              {
                 rgXml.rlLine[ilJ].ilRcvFlag = 1;
                 ilCurLevel--;
              }
              else
                 ilFound = TRUE;
           }
        }
     }
  }

  return ilRC;
} /* End of MarkOutput */

static int BuildOutput(int ipIndex, int *ipCount, char *pcpCurSec, char *pcpType, char *pcpIntfNam,
                       char *pcpMode)
{
    int ilRC = RC_SUCCESS;
    char pclFunc[] = "BuildOutput:";
    int ilI;
    int ilJ;
    int ilK;
    int ilLevel;
    char pclName[32];   /* MEI */
    char pclAltName[32];  /* MEI */
    int ilIgnore;
    int ilStartCount;
    char clActionType;
    char pclModId[128];
    char pclCommand[128];
    char pclTable[128];
    char pclSelection[128];
    char pclFields[128];
    char pclPriority[128];
    char pclTws[128];
    char pclTmpBuf[128];
    int ilModId;
    int ilPriority;
    int ilDiscard;
    char pclData[4000];
    char pclDisName[16];
    char pclKeyFieldNames[2048] = "";
    int ilNoItems;
    char pclBlanks[2048];
    int ilStart;
    int ilFound;
    int ilTimeDiffLocalUtc;
    int ilTDIS = -1;
    int ilTDIW = -1;
    char pclTime[16];
    char pclTagAttribute[1024];
    char pclTagAttribute2[1024];
    char *pclTmpPtr;

    memset(pcgOutBuf,0x00,500000);
    ilDiscard = FALSE;
    clActionType = 'U';
    ilStartCount = FALSE;
    *ipCount = 0;

    if (*pcpMode == 'C' || *pcpMode == 'S')
    {
        if (strlen(pcgVersion) > 0)
            sprintf(pcgOutBuf,"%s\n",pcgVersion);
        else
            strcpy(pcgOutBuf,"");
    }

    strcpy(pclKeyFieldNames,"");
    ilStart = 0;
    if (*pcpMode == 'A' || *pcpMode == 'E')
    {
        ilFound = FALSE;
        for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
        {
            if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0 &&
                strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
            {
                ilStart = ilI;
                ilFound = TRUE;
            }
        }
    }

    for (ilI = ilStart; ilI < igCurXmlLines; ilI++)
    {
        ilIgnore = FALSE;
        if (rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
            if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0)
            {
                ilLevel = rgXml.rlLine[ilI].ilLevel;

                for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
                    strcat(pcgOutBuf," ");

                strcat(pcgOutBuf,"<");
                strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclName[0]);
                ilRC = iGetConfigEntry(pcgConfigFile,"TAG_ATTRIBUTE",&rgXml.rlLine[ilI].pclName[0],
                                       CFG_STRING,pclTagAttribute);

                if (ilRC == RC_SUCCESS)
                {
                    pclTmpPtr = strstr(pclTagAttribute,",");
                    if (pclTmpPtr != NULL)
                    {
                        *pclTmpPtr = '\0';
                        pclTmpPtr++;
                        strcpy(pclTagAttribute2,pclTmpPtr);
                        sprintf(pclTmpBuf,"%d",igMsgId++);
                        strcat(pclTagAttribute,pclTmpBuf);
                        strcat(pclTagAttribute,pclTagAttribute2);
                    }

                    for (ilK = 0; ilK < strlen(pclTagAttribute); ilK++)
                        if (pclTagAttribute[ilK] == '_')
                            pclTagAttribute[ilK] = ' ';
                    strcat(pcgOutBuf," ");
                    strcat(pcgOutBuf,pclTagAttribute);
                }
                strcat(pcgOutBuf,">");
                if (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
                    ilStartCount = TRUE;
            }
            else if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_END") == 0)
            {
                ilLevel = rgXml.rlLine[ilI].ilLevel;
                for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
                    strcat(pcgOutBuf," ");
                strcat(pcgOutBuf,"</");
                strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclName[0]);
                strcat(pcgOutBuf,">");
                if (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
                {
                    ilStartCount = FALSE;
                    if (*pcpMode == 'S' || *pcpMode == 'A')
                    {
                        strcat(pcgOutBuf,"\n");
                        return ilRC;
                    }
                }
            }
            else
            {
                /* MEI */
                if( strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0 )
                {
                    if( strcmp(&rgXml.rlLine[ilI].pclFlag[0],"F") == 0 )
                    {
                        strcpy(pclData,&rgXml.rlLine[ilI].pclData[0]);
                        if (*pclData == '\0' || *pclData == ' ')
                        {
                            strcpy(pclDisName,&rgXml.rlLine[ilI].pclName[0]);
                            ilDiscard = TRUE;
                        }
                    }
                    /* MEI v1.40 */
                    else if( (!strcmp(&rgXml.rlLine[ilI].pclFlag[0],"MD") && *pcgAdid != 'D') ||
                             (!strcmp(&rgXml.rlLine[ilI].pclFlag[0],"MA") && *pcgAdid != 'A') )
                    {
                        ilIgnore = TRUE;
                    }
                }

                if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0 && strcmp(&rgXml.rlLine[ilI].pclName[0],pcgActionType) == 0)
                {
                    clActionType = rgXml.rlLine[ilI].pclData[0];
                }

                strcpy(pclName,&rgXml.rlLine[ilI].pclName[0]);

                if (*pcgAdid == 'D')
                    strcpy(pclAltName,&rgXml.rlLine[ilI].pclFieldD[ipIndex][0]);
                else
                    strcpy(pclAltName,&rgXml.rlLine[ilI].pclFieldA[ipIndex][0]);
                if (strlen(pclAltName) > 0)
                {
                    if (strcmp(pclAltName,"XXXX") == 0)
                        ilIgnore = TRUE;
                    strcpy(pclName,pclAltName);
                }

                if (strcmp(&rgXml.rlLine[ilI].pclType[0],"IGNORE") == 0)
                    ilIgnore = TRUE;

                if( strcmp(&rgXml.rlLine[ilI].pclBasFld[0],"CSGN") == 0)
                {
                    /* Currently 15-May-2012 used in DXB EFPS v1.44 */
                    if( igDelZeroFromCsgn == TRUE )
                    {
                        DeleteZeroFromCsgn( &rgXml.rlLine[ilI].pclData[0] );
                    }
                }

                if (pcpType == NULL)
                {
                    if (clActionType == *pcgActionTypeDel)
                    {
                        if (strcmp(pclName,"RAID") != 0 && strcmp(pclName,"TOID") != 0 &&
                            strcmp(&rgXml.rlLine[ilI].pclType[0],"BAS") != 0 &&
                            strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
                            ilIgnore = TRUE;
                    }
                }

                if (strcmp(pcgRepeatKeyFieldsInBody,"YES") != 0 &&
                    strstr(pcgSectionsForKeyFieldsCheck,pcpCurSec) != NULL)
                {
                    if (clActionType == *pcgActionTypeIns && strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
                    {
                        if (strstr(pclKeyFieldNames,&rgXml.rlLine[ilI].pclName[0]) != NULL)
                            ilIgnore = TRUE;
                        else if (*pcgAdid == 'A' && strcmp(&rgXml.rlLine[ilI].pclName[0],"STOA") == 0)
                            ilIgnore = TRUE;
                        else if (*pcgAdid == 'D' && strcmp(&rgXml.rlLine[ilI].pclName[0],"STOD") == 0)
                            ilIgnore = TRUE;
                    }
                }

                if (ilIgnore == FALSE)
                {
                    if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0)
                    {
                        strcat(pclKeyFieldNames,&rgXml.rlLine[ilI].pclName[0]);
                        strcat(pclKeyFieldNames,",");
                    }

                    ilLevel = rgXml.rlLine[ilI].ilLevel;
                    for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
                        strcat(pcgOutBuf," ");
                    strcat(pcgOutBuf,"<");
                    strcat(pcgOutBuf,pclName);

                    if (strcmp(&pcgTagAttributeName[0][0],pclName) == 0)
                    {
                        strcat(pcgOutBuf," ");
                        strcat(pcgOutBuf,&pcgTagAttributeValue[0][0]);
                    }

                    if (igCompressTag == TRUE && rgXml.rlLine[ilI].pclData[0] == ' ' &&
                        strlen(&rgXml.rlLine[ilI].pclData[0]) == 1 &&
                        strcmp(&rgXml.rlLine[ilI].pclType[0],"TIM1") != 0)
                        strcat(pcgOutBuf," />");
                    else
                    {
                        strcat(pcgOutBuf,">");
                        /*
                        if (strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                            strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL)
                            strcat(pcgOutBuf,pcgCDStart);
                        */

                        if (
                 	          strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                              strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL ||
                              strcmp(&rgXml.rlLine[ilI].pclType[0],"CDATA") == 0
                           )
                            strcat(pcgOutBuf,pcgCDStart);

                        if (strcmp(&rgXml.rlLine[ilI].pclType[0],"SGDT") == 0)
                        {
                            strcpy(pclTime,&rgXml.rlLine[ilI].pclData[0]);
                            if (strlen(pclTime) > 1)
                            {
                                if (strlen(pclTime) == 12)
                                    strcat(pclTime,"00");
                                strncat(pcgOutBuf,&pclTime[0],4);
                                strcat(pcgOutBuf,"-");
                                strncat(pcgOutBuf,&pclTime[4],2);
                                strcat(pcgOutBuf,"-");
                                strncat(pcgOutBuf,&pclTime[6],2);
                                strcat(pcgOutBuf,"T");
                                strncat(pcgOutBuf,&pclTime[8],2);
                                strcat(pcgOutBuf,":");
                                strncat(pcgOutBuf,&pclTime[10],2);
                                strcat(pcgOutBuf,":");
                                strncat(pcgOutBuf,&pclTime[12],2);

                                //Frank v1.52 20121226
                                if(strlen(pcgModifySGDTTimeFormat) != 0)
                                {
                       		        strcat(pcgOutBuf,pcgModifySGDTTimeFormat);
                                }
                               //Frank v1.52 20121226
                            }
                            else
                                strcat(pcgOutBuf," ");
                        }
                        else
                        {
                            if (strcmp(&rgXml.rlLine[ilI].pclType[0],"TIM1") == 0)
                            {
                                if (strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                                    strcpy(pclTime,&rgXml.rlLine[ilI].pclData[0]);
                                else
                                    strcpy(pclTime,pcgCurrentTime);

                                ilTimeDiffLocalUtc = GetTimeDiffUtc(pclTime,&ilTDIS,&ilTDIW);
                                ilRC = MyAddSecondsToCEDATime(pclTime,ilTimeDiffLocalUtc*60,1);
                                strncat(pcgOutBuf,&pclTime[8],2);
                                strcat(pcgOutBuf,":");
                                strncat(pcgOutBuf,&pclTime[10],2);
                                strcat(pcgOutBuf,":00");
                            }
                            else
                            {
                                if (strcmp(&rgXml.rlLine[ilI].pclType[0],"DATE") == 0 &&
                                    strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                                {
                                    strncat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[0],8);
                                }
                                else if (strcmp(&rgXml.rlLine[ilI].pclType[0],"TIME") == 0 &&
                                         strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                                {
                                    strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[8]);
                                    if (strlen(&rgXml.rlLine[ilI].pclData[0]) == 12)
                                        strcat(pcgOutBuf,"00");
                                }
                                else
                                {
                                    strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[0]);
                                }

                                if (strcmp(&rgXml.rlLine[ilI].pclType[0],"DATI") == 0 &&
                                    strlen(&rgXml.rlLine[ilI].pclData[0]) == 12)
                                {
                                    strcat(pcgOutBuf,"00");
                                }
                                else if (strcmp(pclName,"JFNO") == 0)
                                {
                                    GetTrailingBlanks(pclBlanks,&rgXml.rlLine[ilI].pclData[0],9);
                                    strcat(pcgOutBuf,pclBlanks);
                                }
                                else if (strcmp(pclName,"VIAL") == 0)
                                {
                                    GetTrailingBlanks(pclBlanks,&rgXml.rlLine[ilI].pclData[0],120);
                                    strcat(pcgOutBuf,pclBlanks);
                                }
                            }
                        }

                        /*
                        if (strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                            strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL)
                            strcat(pcgOutBuf,pcgCDEnd);
                        */

                        if (
                 	           strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                               strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL ||
                               strcmp(&rgXml.rlLine[ilI].pclType[0],"CDATA") == 0
                           )
                            strcat(pcgOutBuf,pcgCDEnd);

                        strcat(pcgOutBuf,"</");
                        strcat(pcgOutBuf,pclName);
                        strcat(pcgOutBuf,">");
                    }
                    if (ilStartCount == TRUE)
                        *ipCount = *ipCount + 1;
                }
            }
            if (ilIgnore == FALSE)
                strcat(pcgOutBuf,"\n");
        }
    } /* for loop */

    if (ilDiscard == TRUE)
    {
        dbg(DEBUG,"%s Nothing send for Section <%s> because Field %s is empty",pclFunc,pcpCurSec,pclDisName);
        *ipCount = 0;
    }
    else
       dbg(DEBUG,"%s %d Lines of Information for Section <%s>",pclFunc,*ipCount,pcpCurSec);

    if (*ipCount > 0 || clActionType == *pcgActionTypeDel)
    {
        dbg(DEBUG,"%s Output = \n%s",pclFunc,pcgOutBuf);
        /*snap(pcgOutBuf,strlen(pcgOutBuf),outp);*/
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"MODID",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
        {
            ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"MODID",CFG_STRING,pclModId);
            if (ilRC == RC_SUCCESS)
            {
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"COMMAND",CFG_STRING,pclCommand);
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"TABLE",CFG_STRING,pclTable);
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"SELECTION",CFG_STRING,pclSelection);
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"FIELDS",CFG_STRING,pclFields);
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"PRIORITY",CFG_STRING,pclPriority);
               if (ilRC != RC_SUCCESS)
                   strcpy(pclPriority,"3");
               ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"TWS",CFG_STRING,pclTws);
               if (ilRC != RC_SUCCESS || *pclTws == ' ' || *pclTws == '\0')
                  strcpy(pclTws,pcgTwStart);
            }
            else
            {
                dbg(TRACE,"%s MODID not configured in [%s] nor [%s]",pclFunc,pcpIntfNam,pcpCurSec);
                return RC_FAIL;
            }
        }
        else
        {
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"COMMAND",CFG_STRING,pclCommand);
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"TABLE",CFG_STRING,pclTable);
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"SELECTION",CFG_STRING,pclSelection);
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"FIELDS",CFG_STRING,pclFields);
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"PRIORITY",CFG_STRING,pclPriority);
            if (ilRC != RC_SUCCESS)
               strcpy(pclPriority,"3");
            ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"TWS",CFG_STRING,pclTws);
            if (ilRC != RC_SUCCESS || *pclTws == ' ' || *pclTws == '\0')
               strcpy(pclTws,pcgTwStart);
        }
        ilNoItems = GetNoOfElements(pclModId,',');

        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclModId,1);
            else if (clActionType == *pcgActionTypeDel)
                get_real_item(pclTmpBuf,pclModId,3);
            else
                get_real_item(pclTmpBuf,pclModId,2);
            strcpy(pclModId,pclTmpBuf);
        }
        ilNoItems = GetNoOfElements(pclCommand,',');
        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclCommand,1);
            else if (clActionType == *pcgActionTypeDel)
                get_real_item(pclTmpBuf,pclCommand,3);
            else
                get_real_item(pclTmpBuf,pclCommand,2);
            strcpy(pclCommand,pclTmpBuf);
        }
        ilNoItems = GetNoOfElements(pclTable,',');

        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclTable,1);
            else if (clActionType == *pcgActionTypeDel)
               get_real_item(pclTmpBuf,pclTable,3);
            else
               get_real_item(pclTmpBuf,pclTable,2);
            strcpy(pclTable,pclTmpBuf);
        }

        ilNoItems = GetNoOfElements(pclSelection,',');
        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclSelection,1);
            else if (clActionType == *pcgActionTypeDel)
                get_real_item(pclTmpBuf,pclSelection,3);
            else
                get_real_item(pclTmpBuf,pclSelection,2);
            strcpy(pclSelection,pclTmpBuf);
        }
        ilNoItems = GetNoOfElements(pclFields,',');
        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclFields,1);
            else if (clActionType == *pcgActionTypeDel)
               get_real_item(pclTmpBuf,pclFields,3);
            else
               get_real_item(pclTmpBuf,pclFields,2);
            strcpy(pclFields,pclTmpBuf);
        }
        ilNoItems = GetNoOfElements(pclPriority,',');
        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclPriority,1);
            else if (clActionType == *pcgActionTypeDel)
                get_real_item(pclTmpBuf,pclPriority,3);
            else
                get_real_item(pclTmpBuf,pclPriority,2);
            strcpy(pclPriority,pclTmpBuf);
        }
        ilNoItems = GetNoOfElements(pclTws,',');
        if (ilNoItems > 1)
        {
            if (clActionType == *pcgActionTypeIns)
                get_real_item(pclTmpBuf,pclTws,1);
            else if (clActionType == *pcgActionTypeDel)
                get_real_item(pclTmpBuf,pclTws,3);
            else
                get_real_item(pclTmpBuf,pclTws,2);
            strcpy(pclTws,pclTmpBuf);
        }
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s ModId:     <%d>",pclFunc,ilModId);
        dbg(TRACE,"%s Command:   <%s>",pclFunc,pclCommand);
        dbg(TRACE,"%s Table:     <%s>",pclFunc,pclTable);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFields);
        dbg(TRACE,"%s Priority:  <%d>",pclFunc,ilPriority);
        dbg(TRACE,"%s TWS:       <%s>",pclFunc,pclTws);
        if (strcmp(pcgSendOutput,"YES") == 0)
            ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pclTws,pcgTwEnd,
                                 pclCommand,pclTable,pclSelection,pclFields,pcgOutBuf,"",ilPriority,NETOUT_NO_ACK);
        else
            dbg(TRACE,"%s ATTENTION: OUTPUT NOT SENT DUE TO CONFIG PARAMETER",pclFunc);
    }

    return ilRC;
} /* End of BuildOutput */


static int BuildHeader(char *pcpActType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "BuildHeader:";
  int ilI;
  int ilIdx;
  int ilNoSec;
  char pclTags[1024];
  char pclCurTag[128];
  char pclTmpBuf[1024];

/******************* NBIA *************************/
  ilIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],"XML");
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("HDR",pcgMessageOrigin,NULL);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],"UFIS");
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("HDR",pcgTimeId,NULL);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],"UTC");
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("HDR",pcgTimeStamp,NULL);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcgCurrentTime);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
  if (ilIdx >= 0)
  {
     rgXml.rlLine[ilIdx].pclData[0] = *pcpActType;
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }

/******************* LIS ATLANTIS (CONFIGURABLE) ******************/
  ilRC = iGetConfigEntry(pcgConfigFile,"HEADER","HEADER_TAGS",CFG_STRING,pclTags);
  if (ilRC == RC_SUCCESS)
  {
     ilNoSec = GetNoOfElements(pclTags,',');
     for (ilI = 1; ilI <= ilNoSec; ilI++)
     {
        get_real_item(pclCurTag,pclTags,ilI);
        ilIdx = GetXmlLineIdx("HDR",pclCurTag,NULL);
        if (ilIdx >= 0)
        {
           ilRC = iGetConfigEntry(pcgConfigFile,"HEADER",pclCurTag,CFG_STRING,pclTmpBuf);
           if (ilRC == RC_SUCCESS)
           {
              if (strlen(pclTmpBuf) == 0)
                 strcpy(pclTmpBuf," ");
              else if (strcmp(pclTmpBuf,"CurrentDateTime") == 0)
                 strcpy(pclTmpBuf,pcgCurrentTime);
              else if (strstr(pclTmpBuf,"ActionCode") != NULL)
              {
                 if (strcmp(pclTmpBuf,"ActionCode,F") == 0)
                    strcpy(pclTmpBuf,pcpActType);
                 else
                 {
                    strcpy(pclTmpBuf,pcpActType);
                    pclTmpBuf[1] = '\0';
                 }
              }
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpBuf);
              rgXml.rlLine[ilIdx].ilRcvFlag = 1;
           }
        }
     }
  }
/******************* LIS ATLANTIS (CONFIGURABLE) ******************/

  return ilRC;
} /* End of BuildHeader */


static int GetXmlLineIdx(char *pcpTag, char *pcpName, char *pcpSection)
{
  int ilRC = -1;
  char pclFunc[] = "GetXmlLineIdx:";
  int ilI;
  int ilJ;
  int ilFound;
  char pclFldNam[32];

  ilFound = FALSE;
  ilI = 0;
  if (pcpSection != NULL)
  {
     while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpSection) != 0 && ilI <igCurXmlLines)
        ilI++;
  }
  else
     ilI = -1;
  for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     if (*pcgAdid == 'D')
        strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclBasFld[0]);
     else
        strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclBasTab[0]);
     if (strlen(pclFldNam) == 0 || *pclFldNam == ' ')
        strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclName[0]);
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],pcpTag) == 0 &&
         (strcmp(pclFldNam,pcpName) == 0 || strcmp(&rgXml.rlLine[ilJ].pclType[0],pcpName) == 0))
     {
        ilRC = ilJ;
        ilFound = TRUE;
     }
     if (pcpSection != NULL)
     {
        if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpSection) == 0)
        {
           ilFound = TRUE;
        }
     }
  }

  return ilRC;
} /* End of GetXmlLineIdx */

//               GetXmlLineIdxFreeTag("DAT",pclField,pcpCurSec);
static int GetXmlLineIdxFreeTag(char *pcpTag, char *pcpName, char *pcpSection)
{
    int ilRC = -1;
    char pclFunc[] = "GetXmlLineIdxFreeTag:";
    int ilI;
    int ilJ;
    int ilFound;
    char pclFldNam[32];
    char pclFldNam2[32];

    dbg(DEBUG, "GetXmlLineIdxFreeTag:: Start");
    dbg(DEBUG, "GetXmlLineIdxFreeTag:: pcpTag[%s], pcpName[%s], pcpSection[%s]", pcpTag, pcpName, pcpSection);
    strcpy(pclFldNam,"");
    strcpy(pclFldNam2,"");
    ilFound = FALSE;
    ilI = 0;
    if (pcpSection != NULL)
    {
        while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpSection) != 0 && ilI <igCurXmlLines)
            ilI++;
    }
    else
        ilI = -1;
    dbg(DEBUG, "GetXmlLineIdxFreeTag:: Section Index[%d]", ilI);

    for (ilJ = ilI + 1; ilJ<igCurXmlLines && ilFound == FALSE; ilJ++)
    {
    	/*   WON Jira 3397   */
        strcpy(pclFldNam2, "");

        if (*pcgAdid == 'D')
            strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclBasFld[0]);
        else
            strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclBasTab[0]);

        if (*pcgAdid == 'B')
            strcpy(pclFldNam2,&rgXml.rlLine[ilJ].pclBasFld[0]);

        if (strlen(pclFldNam) == 0 || *pclFldNam == ' ')
            strcpy(pclFldNam,&rgXml.rlLine[ilJ].pclName[0]);

        if (strlen(pclFldNam2) == 0 || *pclFldNam2 == ' ')
            strcpy(pclFldNam2,&rgXml.rlLine[ilJ].pclName[0]);

        if (
              strcmp(&rgXml.rlLine[ilJ].pclTag[0],pcpTag) == 0 &&
              (
                  strcmp(pclFldNam,pcpName) == 0 ||
                  strcmp(pclFldNam2,pcpName) == 0 ||
                  strcmp(&rgXml.rlLine[ilJ].pclType[0],pcpName) == 0
              ) &&
              rgXml.rlLine[ilJ].ilRcvFlag == 0)
        {
            ilRC = ilJ;
            ilFound = TRUE;
        }

        if (pcpSection != NULL)
        {
            if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpSection) == 0)
            {
               ilFound = TRUE;
            }
        }
    }

    dbg(DEBUG, "GetXmlLineIdxFreeTag:: End Result[%d]", ilRC);
    return ilRC;
} /* End of GetXmlLineIdxFreeTag */


static int GetKeyFieldList(char *pcpFieldList, int *ipNoEle)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetKeyFieldList:";
  int ilI;
  int ilJ;
  char pclKeySection[32];
  int ilFound;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","KEY_SECTION",CFG_STRING,pclKeySection);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Key Section defined",pclFunc);
     return RC_FAIL;
  }
  strcpy(pcpFieldList,"");
  *ipNoEle = 0;
  ilRC = RC_FAIL;
  ilFound = FALSE;
  ilI = 0;
  while (strcmp(&rgXml.rlLine[ilI].pclName[0],pclKeySection) != 0 && ilI <igCurXmlLines)
     ilI++;
  for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"KEY") == 0 && strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"M") == 0)
     {
        if (strcmp(&rgXml.rlLine[ilJ].pclName[0],"URNO") != 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],"STDT") != 0)
        {
           strcat(pcpFieldList,&rgXml.rlLine[ilJ].pclBasTab[0]);
           strcat(pcpFieldList,",");
           *ipNoEle = *ipNoEle + 1;
           if (strcmp(&rgXml.rlLine[ilJ].pclBasTab[0],&rgXml.rlLine[ilJ].pclBasFld[0]) != 0)
           {
              strcat(pcpFieldList,&rgXml.rlLine[ilJ].pclBasFld[0]);
              strcat(pcpFieldList,",");
              *ipNoEle = *ipNoEle + 1;
           }
        }
     }
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pclKeySection) == 0)
     {
        ilFound = TRUE;
        ilRC = RC_SUCCESS;
     }
  }

  return ilRC;
} /* End of GetKeyFieldList */


static int StoreKeyData(char *pcpFieldList, char *pcpDataBuf, int ipNoEle, char *pcpOrgFields, char *pcpNewData, char *pcpOldData, char *pcpCommand, int ipTowing)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "StoreKeyData:";
  char pclKeySection[32];
  int ilI;
  int ilIdx;
  int ilItemNo;
  char pclField[8];
  char pclData[4000];

/*
dbg(DEBUG,"*****************************************");
dbg(TRACE,"Fields:  <%s>",pcpFieldList);
dbg(TRACE,"Data:    <%s>",pcpDataBuf);
dbg(TRACE,"ipNoEle: <%d>",ipNoEle);
dbg(TRACE,"Org Fld: <%s>",pcpOrgFields);
dbg(TRACE,"Old Dat: <%s>",pcpNewData);
dbg(TRACE,"Org Old: <%s>",pcpOldData);
dbg(TRACE,"pcpCommand: <%s>",pcpCommand);
dbg(TRACE,"ipTowing: <%d>",ipTowing);
dbg(TRACE,"igReturnFlight: <%d>",igReturnFlight);
*/
/*
dbg(TRACE,"Fields:  <%s>",pcpFieldList);
dbg(TRACE,"Org Old: <%s>",pcpOldData);
dbg(TRACE,"Org Fld: <%s>",pcpOrgFields);
*/
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","KEY_SECTION",CFG_STRING,pclKeySection);
  for (ilI = 1; ilI <= ipNoEle; ilI++)
  {
     get_real_item(pclField,pcpFieldList,ilI);
     //dbg(TRACE,"pclField: <%s>",pclField);
     ilItemNo = get_item_no(pcpOrgFields,pclField,5) + 1;
     //dbg(TRACE,"ilItemNo: <%d>",ilItemNo);

     if (ilItemNo > 0 && *pcpCommand == 'U' && ipTowing == FALSE)
     {
            //dbg(DEBUG,"%s the first letter of command is U", pclFunc);
        get_real_item(pclData,pcpOldData,ilItemNo);
     }
     else if (ilItemNo > 0 && *pcpCommand == 'D' && igReturnFlight == 1)
     {
             //dbg(DEBUG,"%s the first letter of command is D", pclFunc);
        get_real_item(pclData,pcpOldData,ilItemNo);
     }
     else if (ilItemNo > 0 && *pcpCommand == 'I' && (igReturnFlight == 2 || igReturnFlight == 3))
     {
             //dbg(DEBUG,"%s the first letter of command is I", pclFunc);
        get_real_item(pclData,pcpNewData,ilItemNo);
     }
     else
     {
            //dbg(DEBUG,"%s the first letter of command is not U,I and D", pclFunc);
        get_real_item(pclData,pcpDataBuf,ilI);
     }

     ilIdx = GetXmlLineIdx("KEY",pclField,pclKeySection);
     if (ilIdx >= 0)
     {
          //dbg(DEBUG,"%s 1 pclData<%s>",pclFunc,pclData);
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);
        rgXml.rlLine[ilIdx].ilRcvFlag = 1;

        if( strncmp(pclField,"ADID",4) == 0 )
        {
            memset(pcgAdid,0,sizeof(pcgAdid));
            strncpy(pcgAdid,pclData,1);
            dbg(DEBUG,"++++pcgAdid<%s>",pcgAdid);
        }
     }
  }
  if (*pcgAdid == 'D')
     strcpy(pclField,"STOD");
  else
     strcpy(pclField,"STOA");

  //dbg(DEBUG,"^^^pclField<%s>",pclField);

  ilIdx = GetXmlLineIdx("KEY",pclField,pclKeySection);
  if (ilIdx >= 0)
  {
     //dbg(DEBUG,"%s 2 pclData<%s>",pclFunc,pclData);

     ilItemNo = get_item_no(pcpOrgFields,pclField,5) + 1;
     if (ilItemNo > 0 && *pcpCommand == 'U' && ipTowing == FALSE)
     {
        get_real_item(pclData,pcpOldData,ilItemNo);
     }
     else if (ilItemNo > 0 && *pcpCommand == 'D' && igReturnFlight == 1)
     {
          get_real_item(pclData,pcpOldData,ilItemNo);
     }
     else if (ilItemNo > 0 && *pcpCommand == 'B')
     {

          //dbg(DEBUG,"++++++++pcpOldData<%s>ilItemNo<%d>",pcpOldData,ilItemNo);
        get_real_item(pclData,pcpOldData,ilItemNo);
     }
     else
     {
        ilItemNo = get_item_no(pcpFieldList,pclField,5) + 1;
        get_real_item(pclData,pcpDataBuf,ilItemNo);
     }
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);

     //dbg(DEBUG,"^^^^^name<%s>data<%s>",&rgXml.rlLine[ilIdx].pclName[0],&rgXml.rlLine[ilIdx].pclData[0]);

     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
dbg( TRACE, "MEI: [LAST_KEY] Field <%s> Data <%s>", pclField, pclData );
  }
  ilIdx = GetXmlLineIdx("KEY","ADID",pclKeySection);
  if (ilIdx >= 0 && ipTowing == FALSE)
  {
     if (rgXml.rlLine[ilIdx].pclData[0] == 'B')
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"A");
  }
    dbg(DEBUG,"*****************************************");
  return ilRC;
} /* End of StoreKeyData */


static int GetInterfaceIndex(char *pcpInterfaceName)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetInterfacIndex:";
  int ilI;
  int ilFound;
  char pclIntfNam[32];

  ilFound = FALSE;
  for (ilI = 0; ilI < igCurInterface && ilFound == FALSE; ilI++)
  {
     if (strcmp(&rgInterface[ilI].pclIntfName[0],pcpInterfaceName) == 0)
     {
        igIntfIdx = rgInterface[ilI].ilIntfIdx;
        ilFound = TRUE;
     }
  }
  if (ilFound == FALSE)
  {
     if (strcmp(pcpInterfaceName,"STD") == 0)
     {
        igIntfIdx = 0;
     }
     else
     {
        strcpy(pclIntfNam,"STD");
        ilFound = FALSE;
        for (ilI = 0; ilI < igCurInterface && ilFound == FALSE; ilI++)
        {
           if (strcmp(&rgInterface[ilI].pclIntfName[0],pclIntfNam) == 0)
           {
              igIntfIdx = rgInterface[ilI].ilIntfIdx;
              ilFound = TRUE;
           }
        }
     }
  }
  dbg(DEBUG,"Use Interface Index = %d for Interface = %s (%s)",igIntfIdx,pcpInterfaceName,&rgInterface[igIntfIdx].pclIntfName[0]);

  return ilRC;
} /* End of GetInterfacIndex */


static int GetManFieldList(char *pcpSection, char *pcpFieldList, int *ipNoEle, char *pcpRcvFieldList, int ipLocSec)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetManFieldList:";
  int ilI;
  int ilJ;
  int ilFound;
  char pclField[16];
  int ilLoca1;
  int ilLoca2;
  int ilMand;
  int ilNoEle;
  int ilK;
  char pclFldNam[16];

  if (ipLocSec == TRUE)
  {
     ilLoca1 = GetXmlLineIdxBas(pcpSection,1);
     ilLoca2 = GetXmlLineIdxBas(pcpSection,2);
  }
  else
  {
     ilLoca1 = -1;
     ilLoca2 = -1;
  }
  strcpy(pcpFieldList,"");
  *ipNoEle = 0;
  ilFound = FALSE;
  ilI = 0;
  while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpSection) != 0 && ilI <igCurXmlLines)
     ilI++;
  for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     ilMand = FALSE;
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"DAT") == 0)
     {
        if (strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"M") != 0 && strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"F") != 0)
        {
           ilNoEle = GetNoOfElements(&rgXml.rlLine[ilJ].pclFlag[0],'|');
           for (ilK = 1; ilK <= ilNoEle; ilK++)
           {
              GetDataItem(pclFldNam,&rgXml.rlLine[ilJ].pclFlag[0],ilK,'|',""," ");
              TrimRight(pclFldNam);
              if (pclFldNam[strlen(pclFldNam)-1] == '-')
                 pclFldNam[strlen(pclFldNam)-1] = '\0';
              if (strstr(pcpRcvFieldList,pclFldNam) != NULL)
                 ilMand = TRUE;
           }
        }
     }
     /* MEI v1.40 */
     /*if ((strcmp(&rgXml.rlLine[ilJ].pclTag[0],"DAT") == 0 &&
         (strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"M") == 0 || strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"F") == 0)) ||
         ilJ == ilLoca1 || ilJ == ilLoca2 || ilMand == TRUE)*/
     if ((strcmp(&rgXml.rlLine[ilJ].pclTag[0],"DAT") == 0 &&
         (rgXml.rlLine[ilJ].pclFlag[0] == 'M' || strcmp(&rgXml.rlLine[ilJ].pclFlag[0],"F") == 0)) ||
         ilJ == ilLoca1 || ilJ == ilLoca2 || ilMand == TRUE)
     {
        if (*pcgAdid == 'D')
           strcpy(pclField,&rgXml.rlLine[ilJ].pclBasFld[0]);
        else
           strcpy(pclField,&rgXml.rlLine[ilJ].pclBasTab[0]);
        if (strcmp(pclField,"XXXX") != 0 && strcmp(pclField,"RAID") != 0 && strcmp(pclField,"ALCD") != 0 &&
            strcmp(&rgXml.rlLine[ilJ].pclType[0],"IGNORE") != 0)
        {
           strcat(pcpFieldList,pclField);
           strcat(pcpFieldList,",");
           *ipNoEle = *ipNoEle + 1;
        }
     }
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpSection) == 0)
     {
        ilFound = TRUE;
     }
  }
  if (strlen(pcpFieldList) > 0)
     pcpFieldList[strlen(pcpFieldList)-1] = '\0';

  return ilRC;
} /* End of GetManFieldList */

//               GetFieldList(pclCurSec,pclNewFieldList,&ilNoEle);
static int GetFieldList(char *pcpSection, char *pcpFieldList, int *ipNoEle)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetFieldList:";
  int ilI;
  int ilJ;
  int ilFound;
  char pclField[16];

  ilFound = FALSE;
  ilI = 0;
  while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpSection) != 0 && ilI <igCurXmlLines)
     ilI++;
  for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"DAT") == 0)
     {
        if (*pcgAdid == 'D')
           strcpy(pclField,&rgXml.rlLine[ilJ].pclBasFld[0]);
        else
           strcpy(pclField,&rgXml.rlLine[ilJ].pclBasTab[0]);
        if (strcmp(pclField,"XXXX") != 0 && strcmp(pclField,"RAID") != 0 && strcmp(pclField,"ALCD") != 0 &&
            strcmp(&rgXml.rlLine[ilJ].pclType[0],"IGNORE") != 0 && strstr(pcpFieldList,pclField) == NULL)
        {
           strcat(pcpFieldList,pclField);
           strcat(pcpFieldList,",");
           *ipNoEle = *ipNoEle + 1;
        }
     }
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpSection) == 0)
     {
        ilFound = TRUE;
     }
  }

  return ilRC;
} /* End of GetFieldList */


static int StoreData(char *pcpFieldList, char *pcpNewData, char *pcpOldData, int ipNoEle, char *pcpCurSec, int *ipDatCnt)
{
    int ilRC = RC_SUCCESS;
    int ilI;
    int ilIdx;
    int ilItemNo;
    int ilChanged;
    char pclFunc[] = "StoreData:";
    char pclField[8];
    char pclData[4000];
    char pclOldData[4000];
    char pclFldNam[128];
    char pclKeySection[512] = "\0";
    char pclTmpStr[512] = "\0";
    char *pclTmpPtr;

    char pclVia4CurVal[16] = "\0";
    char pclVia4OldVal[16] = "\0";

    memset(pclVia4CurVal,0,sizeof(pclVia4CurVal));
    memset(pclVia4OldVal,0,sizeof(pclVia4OldVal));

    dbg(DEBUG, "StoreData:: Start");
    dbg(DEBUG, "StoreData:: pcpFieldList[%s] pcpNewData[%s] pcpOldData[%s] ipNoEle[%d] pcpCurSec[%s] ipDatCntchar[%d]",
        pcpFieldList, pcpNewData, pcpOldData, ipNoEle, pcpCurSec, *ipDatCnt);

    /* MEI v1.45 */
    if( igUseViaAsOrg == TRUE || igUseViaAsDes == TRUE )
        iGetConfigEntry(pcgConfigFile,"EXCHDL","KEY_SECTION",CFG_STRING,pclKeySection);


    for (ilI = 1; ilI <= ipNoEle; ilI++)
    {
        get_real_item(pclField,pcpFieldList,ilI);
        TrimRight(pclField);
        get_real_item(pclData,pcpNewData,ilI);
        TrimRight(pclData);
        get_real_item(pclOldData,pcpOldData,ilI);
        TrimRight(pclOldData);


        //Frank v1.52 20121227
        //dbg(DEBUG,"%s ilI<%d> pclField<%s> pclData<%s> pclOldData<%s>",pclFunc,ilI,pclField,pclData,pclOldData);

        if( (igUseViaAsOrg == TRUE) || (igUseViaAsDes == TRUE) )
        {
	        if( !strcmp(pclField,"VIA4") )
	        {
	     	    dbg(DEBUG,"%s 1-Get the VIA4 update",pclFunc);
	     	    strcpy(pclVia4CurVal,pclData);
	     	    strcpy(pclVia4OldVal,pclOldData);
	        }
        }

        if (strcmp(pclData,pclOldData) != 0)
        {
     		if( strncmp(pcgDXBHoneywell,"YES",3) != 0 )
     		{
                ilIdx = GetXmlLineIdxFreeTag("DAT",pclField,pcpCurSec);
            }
            else
            {
        	    ilIdx = GetXmlLineIdx("DAT",pclField,pcpCurSec);
            }

            //Frank v1.52 20121227
            //dbg(DEBUG,"%s pcpCurSec<%s> Idx<%d> ",pclFunc,pcpCurSec,ilIdx);
            if (ilIdx >= 0)
            {
        	    //Frank v1.52 20121227
        	    //dbg(DEBUG,"%s rgXml.rlLine[%d].pclBasTab<%s> ",pclFunc,ilIdx,rgXml.rlLine[ilIdx].pclBasTab);

                if (rgXml.rlLine[ilIdx].ilRcvFlag == 0)
                {
                    if (strlen(&rgXml.rlLine[ilIdx].pclFlag[0]) > 0)
                    {
                        strcpy(pclFldNam,&rgXml.rlLine[ilIdx].pclFlag[0]);
                        if (pclFldNam[strlen(pclFldNam)-1] == '-' && *pclData == ' ')
                        {
                            pclFldNam[strlen(pclFldNam)-1] = '\0';
                            ilItemNo = get_item_no(pcpFieldList,pclFldNam,5) + 1;
                            if (ilItemNo > 0)
                            {
                                get_real_item(pclData,pcpNewData,ilItemNo);
                                TrimRight(pclData);
                            }
                        }
                    }

                    /* Convert back for SITATEX 6 */
                    pclTmpPtr = strstr(pclData,"STX.");
                    while (pclTmpPtr != NULL)
                    {
                        pclTmpPtr += 3;
                        *pclTmpPtr = ',';
                        pclTmpPtr = strstr(pclTmpPtr,"STX.");
                    }

                    //strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);
                    if ( strncmp(pcgInstallation,"BLR",3) == 0 && strncmp(pcgBLRCustomization,"YES",3) == 0)
                    {
                        if(strcmp(rgXml.rlLine[ilIdx].pclBasTab,pclField)==0)
                        {
                            strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);
                            dbg(DEBUG,"Copying 1 [%d][%s][%s]", ilIdx, &rgXml.rlLine[ilIdx].pclData, pclData);
                        }
                        else
                        {
                            dbg(DEBUG,"*****Do not copy the data*******");
                            dbg(DEBUG,"rgXml.rlLine[ilIdx].pclName<%s> != pclField<%s>",&rgXml.rlLine[ilIdx].pclBasTab,pclField);
                        }
                    }
                    else
          	        {
          		        strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);
          		        dbg(DEBUG,"Copying 2 [%d][%s][%s]", ilIdx, &rgXml.rlLine[ilIdx].pclData, pclData);
          	        }

                    rgXml.rlLine[ilIdx].ilRcvFlag = 1;
                    *ipDatCnt = *ipDatCnt + 1;
                }
            }
        }
    }

  if (*ipDatCnt > 0)
  {
     for (ilI = 1; ilI <= ipNoEle; ilI++)
     {
        get_real_item(pclField,pcpFieldList,ilI);
        TrimRight(pclField);
        get_real_item(pclData,pcpNewData,ilI);
        TrimRight(pclData);
        get_real_item(pclOldData,pcpOldData,ilI);
        TrimRight(pclOldData);

	//dbg(DEBUG,"%s =====%d======",pclFunc,__LINE__);

        /* MEI v1.45 For arrival flight only */
        /* MEI v1.47 For departure and return flights */
        ilChanged = FALSE;
        if( (igUseViaAsOrg == TRUE && !strncmp(pclField, "ORG", 3) && pcgAdid[0] == 'A') ||
            (igUseViaAsDes == TRUE && !strncmp(pclField, "DES", 3) && pcgAdid[0] != 'A') )
        {
            sprintf( pclTmpStr, "VIA%c", pclField[3] );
            ilIdx = GetXmlLineIdx("KEY",pclTmpStr,pclKeySection);
            if (ilIdx >= 0)
            {
                if( strlen(rgXml.rlLine[ilIdx].pclData) > 0 )
                {
                		if( strlen(pclVia4CurVal) == 0 )
                    {
                    	ilChanged = TRUE;
                    	dbg( TRACE, "%s: Fill field1 <%s><%s> with field <%s><%s>", pclFunc, pclField, pclData, pclTmpStr, &rgXml.rlLine[ilIdx].pclData[0] );
                    	strcpy( pclData, rgXml.rlLine[ilIdx].pclData );
                    }
                    else
                    {
                    	dbg(DEBUG,"%s 2-Get the VIA4 update, pclVia4OldVal<%s>pclVia4CurVal<%s>",pclFunc,pclVia4OldVal,pclVia4CurVal);
                    	strcpy( pclData, pclVia4CurVal );
                    }
            		}
            		/*
								else
								{
									dbg(DEBUG,"%s strlen(rgXml.rlLine[ilIdx].pclData)==0",pclFunc);

									if(strlen(pclVia4CurVal)!=0)
									{
										dbg( TRACE, "%s: Fill field2 <%s><%s> with field <%s><%s>", pclFunc, pclField, pclData, pclTmpStr, pclVia4CurVal );
                  	strcpy( pclData, pclVia4CurVal );
									}
									else
									{
										dbg(DEBUG,"%s strlen(pclVia4CurVal)==0",pclFunc);
									}
								}
								*/
       			}
       			/*
				    else
				    {
						dbg(DEBUG,"%s ilIdx<%d>",pclFunc,ilIdx);
				    }
				    */
    		}
				else
				{
					dbg(DEBUG,"%s igUseViaAsOrg<%d>,igUseViaAsDes<%d>pclField<%s>,pcgAdid<%s>",pclFunc,igUseViaAsOrg,igUseViaAsDes,pclField,pcgAdid);
				}

        ilIdx = GetXmlLineIdx("DAT",pclField,pcpCurSec);
        if (ilIdx >= 0)
        {
           if (rgXml.rlLine[ilIdx].ilRcvFlag == 0 || ilChanged == TRUE )
           {
              if (rgXml.rlLine[ilIdx].pclFlag[0] == 'M' || rgXml.rlLine[ilIdx].pclFlag[0] == 'F')
              {
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclData);
                 rgXml.rlLine[ilIdx].ilRcvFlag = 1;
                 *ipDatCnt = *ipDatCnt + 1;
              }
           }
        }
     } /* for */
  }
  return ilRC;
} /* End of StoreData */


static int RemoveData(char *pcpFieldList, int ipNoEle, char *pcpCurSec, int *ipDatCnt, int ipStep)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "RemoveData:";
  int ilI;
  int ilIdx;
  char pclField[8];
  int ilLoca1;
  int ilLoca2;

  ilLoca1 = GetXmlLineIdxBas(pcpCurSec,1);
  ilLoca2 = GetXmlLineIdxBas(pcpCurSec,2);
  if (*ipDatCnt > 0)
  {
     for (ilI = 1; ilI <= ipNoEle; ilI++)
     {
        get_real_item(pclField,pcpFieldList,ilI);
        TrimRight(pclField);
        ilIdx = GetXmlLineIdx("DAT",pclField,pcpCurSec);
        if (ilIdx >= 0)
        {
           if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
           {
              if (ipStep == 1)
              {
                 if (ilIdx >= ilLoca2)
                 {
                    strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
                    rgXml.rlLine[ilIdx].ilRcvFlag = 0;
                    *ipDatCnt = *ipDatCnt - 1;
                 }
              }
              else
              {
                 if (ilIdx < ilLoca2)
                 {
                    strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
                    rgXml.rlLine[ilIdx].ilRcvFlag = 0;
                    *ipDatCnt = *ipDatCnt - 1;
                 }
              }
           }
        }
     }
  }

  return ilRC;
} /* End of RemoveData */


static int ClearSection(char *pcpCurSec)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ClearSection:";
  int ilI;
  int ilJ;
  int ilFound;
  char pclLastName[32];
  int ilLastIdx;
  int ilCount;

  ilFound = FALSE;
  ilI = 0;
  while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) != 0 && ilI <igCurXmlLines)
     ilI++;
  for (ilJ = ilI; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     strcpy(&rgXml.rlLine[ilJ].pclData[0],"");
     strcpy(&rgXml.rlLine[ilJ].pclOldData[0],"");
     rgXml.rlLine[ilJ].ilRcvFlag = 0;
     if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpCurSec) == 0)
     {
        ilFound = TRUE;
     }
  }
  ilJ = 0;
  ilFound = TRUE;
  while (ilFound == TRUE && ilJ < 10)
  {
     ilFound = FALSE;
     ilJ++;
     strcpy(pclLastName,"");
     ilCount = 0;
     for (ilI = 0; ilI < igCurXmlLines; ilI++)
     {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
        {
           strcpy(pclLastName,&rgXml.rlLine[ilI].pclName[0]);
           ilLastIdx = ilI;
           ilCount = 0;
        }
        else
        {
           if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_END") == 0 && rgXml.rlLine[ilI].ilRcvFlag == 1)
           {
              if (strcmp(pclLastName,&rgXml.rlLine[ilI].pclName[0]) == 0 && ilCount == 0)
              {
                 rgXml.rlLine[ilI].ilRcvFlag = 0;
                 rgXml.rlLine[ilLastIdx].ilRcvFlag = 0;
                 strcpy(pclLastName,"");
                 ilCount = 0;
                 ilFound = TRUE;
              }
           }
           else
           {
              if (rgXml.rlLine[ilI].ilRcvFlag == 1)
                 ilCount++;
           }
        }
     }
  }

  return ilRC;
} /* End of ClearSection */


static int GetXmlLineIdxBas(char *pcpSection, int ipCount)
{
  int ilRC = -1;
  char pclFunc[] = "GetXmlLineIdxBas:";
  int ilI;
  int ilJ;
  int ilFound;
  char pclFldNam[32];
  int ilCount;

  ilCount = 0;
  ilFound = FALSE;
  ilI = 0;
  if (pcpSection != NULL)
  {
     while (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpSection) != 0 && ilI <igCurXmlLines)
        ilI++;
  }
  else
     ilI = -1;
  for (ilJ = ilI + 1; ilJ < igCurXmlLines && ilFound == FALSE; ilJ++)
  {
     if (strcmp(&rgXml.rlLine[ilJ].pclType[0],"BAS") == 0)
     {
        ilCount++;
        if (ilCount == ipCount)
        {
           ilRC = ilJ;
           ilFound = TRUE;
        }
     }
     if (pcpSection != NULL)
     {
        if (strcmp(&rgXml.rlLine[ilJ].pclTag[0],"STR_END") == 0 && strcmp(&rgXml.rlLine[ilJ].pclName[0],pcpSection) == 0)
        {
           ilFound = TRUE;
        }
     }
  }

  return ilRC;
} /* End of GetXmlLineIdxBas */


static int HandleRaid(char *pcpUrno, int ipRaid, char *pcpCurSec, char *pcpFields,
                      char *pcpNewData, char *pcpOldData, char *pcpFkt1, char *pcpCtyp, char *pcpRarUrno, char *pcpAddFldLst)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleRaid:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  int ilLoca1;
  int ilLoca2;
  char pclLoca1[16] = "";
  char pclLoca2[16] = "";
  char pclRarType[16];
  char pclRarRaid[16];
  char pclRarRnam[16];
  char pclRarRnam2[16];
  int ilItemNo1;
  int ilItemNo2;
  char pclNew[16];
  char pclNew1[16];
  char pclNew2[16];
  char pclOld[16];
  char pclOld1[16];
  char pclOld2[16];
  char pclFieldList1[1024];
  char pclFieldList2[1024];
  int ilI;
  int ilFound;
  char pclFldNam[16];
  int ilLocNamChg;
  int ilIdx;

  ilLoca1 = GetXmlLineIdxBas(pcpCurSec,1);
  ilLoca2 = GetXmlLineIdxBas(pcpCurSec,2);
  if (ilLoca1 >= 0)
  {
     if (*pcgAdid == 'D')
        strcpy(pclLoca1,&rgXml.rlLine[ilLoca1].pclBasFld[0]);
     else
        strcpy(pclLoca1,&rgXml.rlLine[ilLoca1].pclBasTab[0]);
  }
  if (ilLoca2 >= 0)
  {
     if (*pcgAdid == 'D')
        strcpy(pclLoca2,&rgXml.rlLine[ilLoca2].pclBasFld[0]);
     else
        strcpy(pclLoca2,&rgXml.rlLine[ilLoca2].pclBasTab[0]);
  }
  strcpy(pclFieldList1,"");
  strcpy(pclFieldList2,"");
  if (strcmp(pclLoca1,"PSTA") == 0)
  {
     strcpy(pclRarType,"PA");
     strcpy(pclFieldList1,"PSTA,PABS,PAES,PABA,PAEA");
  }
  else if (strcmp(pclLoca1,"PSTD") == 0)
  {
     strcpy(pclRarType,"PD");
     strcpy(pclFieldList1,"PSTD,PDBS,PDES,PDBA,PDEA");
  }
  else if (strcmp(pclLoca1,"BLT1") == 0)
  {
     strcpy(pclRarType,"BB");
     strcpy(pclFieldList1,"BLT1,B1BS,B1ES,B1BA,B1EA");
     strcpy(pclFieldList2,"BLT2,B2BS,B2ES,B2BA,B2EA");
  }
  else if (strcmp(pclLoca1,"GTA1") == 0)
  {
     strcpy(pclRarType,"GA");
     strcpy(pclFieldList1,"GTA1,GA1B,GA1E,GA1X,GA1Y");
     strcpy(pclFieldList2,"GTA2,GA2B,GA2E,GA2X,GA2Y");
  }
  else if (strcmp(pclLoca1,"GTD1") == 0)
  {
     strcpy(pclRarType,"GD");
     strcpy(pclFieldList1,"GTD1,GD1B,GD1E,GD1X,GD1Y");
     strcpy(pclFieldList2,"GTD2,GD2B,GD2E,GD2X,GD2Y");
  }
  else if (strcmp(pclLoca1,"CKIC") == 0)
  {
     if (*pcpCtyp == ' ')
     {
        strcpy(pclRarType,"DC");
        strcpy(pclFieldList1,"CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
     }
     else
     {
        strcpy(pclRarType,"CC");
        strcpy(pclFieldList1,"CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
     }
  }
  else
  {
     dbg(TRACE,"%s Can't assign Location Type for <%s>",pclFunc,pclLoca1);
     return RC_FAIL;
  }
  ilItemNo1 = 0;
  ilItemNo2 = 0;
  strcpy(pclNew," ");
  strcpy(pclNew1," ");
  strcpy(pclNew2," ");
  strcpy(pclOld," ");
  strcpy(pclOld1," ");
  strcpy(pclOld2," ");
  ilItemNo1 = get_item_no(pcpFields,pclLoca1,5) + 1;
  if (ilLoca2 >= 0)
     ilItemNo2 = get_item_no(pcpFields,pclLoca2,5) + 1;
  if (ilItemNo1 > 0)
  {
     get_real_item(pclNew1,pcpNewData,ilItemNo1);
     TrimRight(pclNew1);
     get_real_item(pclOld1,pcpOldData,ilItemNo1);
     TrimRight(pclOld1);
  }
  if (ilItemNo2 > 0)
  {
     get_real_item(pclNew2,pcpNewData,ilItemNo2);
     TrimRight(pclNew2);
     get_real_item(pclOld2,pcpOldData,ilItemNo2);
     TrimRight(pclOld2);
  }
  strcpy(pclNew,pclNew1);
  strcpy(pclOld,pclOld1);
  if (strlen(pclFieldList1) > 0 && ilItemNo2 > 0)
  {
     ilFound = FALSE;
     for (ilI = 1; ilI <= 5 && ilFound == FALSE; ilI++)
     {
        get_real_item(pclFldNam,pclFieldList1,ilI);
        ilIdx = GetXmlLineIdx("DAT",pclFldNam,pcpCurSec);
        if (ilIdx >= 0)
           if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
              ilFound = TRUE;
     }
     if (ilFound == FALSE)
     {
        strcpy(pclNew,pclNew2);
        strcpy(pclOld,pclOld2);
        ilLoca1 = -1;
     }
     else
     {
        strcpy(pclNew,pclNew1);
        strcpy(pclOld,pclOld1);
     }
  }
  ilLocNamChg = FALSE;
  strcpy(pclRarRnam2,"");
  if (strcmp(pclNew,pclOld) == 0)
  {
     strcpy(pcpFkt1,"UFR");
     strcpy(pclRarRnam,pclNew);
  }
  else
  {
     if (*pclNew == ' ')
     {
#if 0
        if (ilLoca1 == -1 && strcmp(pclNew1,pclOld1) != 0)
        {
           for (ilI = 1; ilI <= 5; ilI++)
           {
              get_real_item(pclFldNam,pclFieldList2,ilI);
              ilIdx = GetXmlLineIdx("DAT",pclFldNam,pcpCurSec);
              if (ilIdx >= 0)
                 rgXml.rlLine[ilIdx].ilRcvFlag = 0;
           }
           return ilRC;
        }
        else
        {
           strcpy(pcpFkt1,"DFR");
           strcpy(pclRarRnam,pclOld);
           strcpy(pclRarRnam2,pclNew);
        }
#endif
        strcpy(pcpFkt1,"DFR");
        strcpy(pclRarRnam,pclOld);
        strcpy(pclRarRnam2,pclNew);
     }
     else
     {
        if (*pclOld == ' ')
        {
           strcpy(pcpFkt1,"IFR");
           strcpy(pclRarRnam,pclNew);
        }
        else
        {
           strcpy(pcpFkt1,"DFR");
           strcpy(pclRarRnam,pclOld);
           strcpy(pclRarRnam2,pclNew);
           /*ilLocNamChg = TRUE;*/
           if (ilLoca1 >= 0 && *pclNew != ' ' && pcpAddFldLst != NULL)
           {
              if (strcmp(pclNew2,pclOld2) == 0 || *pclNew2 != ' ')
              {
                 strcpy(pcpAddFldLst,pclFieldList1);
              }
           }
           if (ilLoca1 == -1 && *pclNew != ' ' && pcpAddFldLst != NULL)
           {
              strcpy(pcpAddFldLst,pclFieldList2);
           }
        }
     }
  }
  if (pcpCtyp != NULL)
  {
     if (strcmp(pcpFkt1,"IFR") == 0)
        strcpy(pcpFkt1,"IRT");
     else if (strcmp(pcpFkt1,"UFR") == 0)
        strcpy(pcpFkt1,"URT");
     else if (strcmp(pcpFkt1,"DFR") == 0)
        strcpy(pcpFkt1,"DRT");
  }
  strcpy(pclFieldList,"RAID");
  if (strlen(pcpRarUrno) > 0)
     sprintf(pclSelection,"WHERE URNO = %s",pcpRarUrno);
  else
     sprintf(pclSelection,"WHERE RURN = %s AND TYPE = '%s' AND RNAM = '%s'",pcpUrno,pclRarType,pclRarRnam);
  if (*pcpFkt1 != 'D')
     strcat(pclSelection," AND STAT <> 'DELETED'");
  sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     if (strlen(pclRarRnam2) > 0)
     {
        if (strlen(pcpRarUrno) > 0)
           sprintf(pclSelection,"WHERE URNO = %s",pcpRarUrno);
        else
           sprintf(pclSelection,"WHERE RURN = %s AND TYPE = '%s' AND RNAM = '%s'",pcpUrno,pclRarType,pclRarRnam2);
        if (*pcpFkt1 != 'D')
           strcat(pclSelection," AND STAT <> 'DELETED'");
        sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s Record in RARTAB with RURN = <%s> not found",pclFunc,pcpUrno);
        }
     }
     else
        dbg(TRACE,"%s Record in RARTAB with RURN = <%s> not found",pclFunc,pcpUrno);
  }
  if (ilRCdb == DB_SUCCESS)
  {
     strcpy(pclRarRaid,pclDataBuf);
     TrimRight(pclRarRaid);
     strcpy(&rgXml.rlLine[ipRaid].pclData[0],pclRarRaid);
     rgXml.rlLine[ipRaid].ilRcvFlag = 1;
  }
  if (ilLoca1 >= 0)
  {
     strcpy(&rgXml.rlLine[ilLoca1].pclData[0],pclRarRnam);
     rgXml.rlLine[ilLoca1].ilRcvFlag = 1;
  }
  else
  {
     strcpy(&rgXml.rlLine[ilLoca2].pclData[0],pclRarRnam);
     rgXml.rlLine[ilLoca2].ilRcvFlag = 1;
  }

  return ilRC;
} /* End of HandleRaid */


void GetTheFieldAndOldDataForProceedWithoutCCATAB(char *pcpFields,char *pcpData,char *pcpCcaCkicTemp,
                                 char *pcpCcaCtypTemp,char *pcpCcaCkitTemp,char *pcpCcaCkbsTemp,char *pcpCcaCkesTemp,
                                 char *pcpCcaFlnuTemp,char *pcpCcaUrnoTemp)
{
    int ilItemNo=0;
    char pclFunc[] = "GetTheFieldAndOldDataForProceedWithoutCCATAB:";

    if(strstr(pcpFields,"CKIC")!=NULL)
    {
        ilItemNo = get_item_no(pcpFields,"CKIC",5) + 1;
    get_real_item(pcpCcaCkicTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s CKIC<%s>",pclFunc,pcpCcaCkicTemp);
  }

  if(strstr(pcpFields,"CTYP")!=NULL)
  {
        ilItemNo = get_item_no(pcpFields,"CTYP",5) + 1;
    get_real_item(pcpCcaCtypTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s CTYP<%s>",pclFunc,pcpCcaCtypTemp);
  }

  if(strstr(pcpFields,"CKIT")!=NULL)
  {
    ilItemNo = get_item_no(pcpFields,"CKIT",5) + 1;
    get_real_item(pcpCcaCkitTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s CKIT<%s>",pclFunc,pcpCcaCkitTemp);
  }

  if(strstr(pcpFields,"CKBS")!=NULL)
  {
    ilItemNo = get_item_no(pcpFields,"CKBS",5) + 1;
    get_real_item(pcpCcaCkbsTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s CKBS<%s>",pclFunc,pcpCcaCkbsTemp);
  }

  if(strstr(pcpFields,"CKES")!=NULL)
  {
    ilItemNo = get_item_no(pcpFields,"CKES",5) + 1;
    get_real_item(pcpCcaCkesTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s CKES<%s>",pclFunc,pcpCcaCkesTemp);
  }

  if(strstr(pcpFields,"FLNU")!=NULL)
  {
    ilItemNo = get_item_no(pcpFields,"FLNU",5) + 1;
    get_real_item(pcpCcaFlnuTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s FLNU<%s>",pclFunc,pcpCcaFlnuTemp);
  }

  if(strstr(pcpFields,"URNO")!=NULL)
  {
    ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
    get_real_item(pcpCcaUrnoTemp,pcpData,ilItemNo);
    dbg(DEBUG,"%s URNO<%s>",pclFunc,pcpCcaUrnoTemp);
  }
}

static int HandleCounterOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleCounterOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclCcaCtyp[16];
  char pclCcaFlnu[16];
  char pclCcaCkic[16];
  int ilItemNo;
  int ilNoEle;
  int ilI;
  int ilJ;
  char pclSections[1024];
  int ilNoSec;
  char pclCurSec[32];
  int ilDatCnt;
  char pclNewFieldList[8000];
  char pclNewDataList[8000];
  char pclOldDataList[8000];
  int ilIdx;
  char pclFkt1[8];
  int ilRaid;
  int ilAtIdx;
  int ilNoIntf;
  char pclCurIntf[32];
  char pclCommand[32];
  char pclTmpFld[32];
  char pclAllowedActions[32];
  int ilCount;
  int ilAlcd;
  int ilCtyp;
  char pclTmpBuf[128];
  char pclRarUrno[16];
  char pclMsgType[32];
  int ilMtIdx;
  char pclAddFldLst[1024];
  char pclSaveFields[1024];
  char pclSaveNewData[32000];
  char pclSaveOldData[32000];
  int ilAddLocation = FALSE;
  int ilTmp;
  int ilTmpNoEle;
  char pclPendingAck[1024];
  int ilSendOutput;

  //FYA v1.45 UFIS-1901
  char pclCcaCkicTemp[16] = "\0";
  char pclCcaCtypTemp[16] = "\0";
  char pclCcaCkitTemp[16] = "\0";
  char pclCcaCkbsTemp[16] = "\0";
  char pclCcaCkesTemp[16] = "\0";
  char pclCcaFlnuTemp[16] = "\0";
  char pclCcaUrnoTemp[16] = "\0";
  char pclData[8000] = "\0";

  dbg(DEBUG,"%s Fields:    <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s Data:      <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old Data:  <%s>",pclFunc,pcpOldData);

    GetTheFieldAndOldDataForProceedWithoutCCATAB(pcpFields,pcpNewData,pclCcaCkicTemp,pclCcaCtypTemp,
                                                                                             pclCcaCkitTemp,pclCcaCkbsTemp,pclCcaCkesTemp,
                                                                                             pclCcaFlnuTemp,pclCcaUrnoTemp);
    //FYA v1.45 UFIS-1901

  ilNoEle = GetNoOfElements(pcpFields,',');
  ilTmpNoEle = GetNoOfElements(pcpOldData,',');
  if (ilTmpNoEle < ilNoEle)
  {
     for (ilI = ilTmpNoEle+1; ilI < ilNoEle; ilI++)
        strcat(pcpOldData,",");
  }
  ilRC = CheckIfActualData("C",pcpFields,pcpNewData,ilNoEle);
  if (ilRC != RC_SUCCESS)
     return ilRC;
  strcpy(pclRarUrno,"");
  if (strstr(pcpFields,"CTYP") == NULL || strstr(pcpFields,"FLNU") == NULL ||
      strstr(pcpFields,"CKIC") == 0 || *pcpCommand == 'D')
  {
     if (*pcpCommand == 'D')
     {
        strcpy(pclFieldList,"TYPE,RURN,RNAM");
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 1-SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s RAR Record with URNO = <%s> not found",pclFunc,pcpUrno);
           /*return ilRC;*/
           if (strstr(pcpFields,"CTYP") != NULL && strstr(pcpFields,"FLNU") != NULL &&
               strstr(pcpFields,"CKIC") != 0 && *pcpCommand == 'D')
           {
              ilItemNo = get_item_no(pcpFields,"CTYP",5) + 1;
              get_real_item(pclCcaCtyp,pcpNewData,ilItemNo);
              ilItemNo = get_item_no(pcpFields,"FLNU",5) + 1;
              get_real_item(pclCcaFlnu,pcpNewData,ilItemNo);
              ilItemNo = get_item_no(pcpFields,"CKIC",5) + 1;
              get_real_item(pclCcaCkic,pcpOldData,ilItemNo);
           }
           else
              return ilRC;
        }
        else
        {
           BuildItemBuffer(pclDataBuf,"",3,",");
           get_real_item(pclCcaCtyp,pclDataBuf,1);
           TrimRight(pclCcaCtyp);
           if (strcmp(pclCcaCtyp,"CC") == 0)
              strcpy(pclCcaCtyp,"C");
           else
              strcpy(pclCcaCtyp," ");
           get_real_item(pclCcaFlnu,pclDataBuf,2);
           get_real_item(pclCcaCkic,pclDataBuf,3);
           strcpy(pclRarUrno,pcpUrno);
        }
     }
     else
     {
        strcpy(pclFieldList,"CTYP,FLNU,CKIC");
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 2-SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s 1-CCA Record with URNO = <%s> not found",pclFunc,pcpUrno);
           dbg(TRACE,"This maybe CTYP and FLNU are not added in action.cfg or CKIC is the first received field instead of URNO");
           return ilRC;
        }
        BuildItemBuffer(pclDataBuf,"",3,",");
        get_real_item(pclCcaCtyp,pclDataBuf,1);
        get_real_item(pclCcaFlnu,pclDataBuf,2);
        get_real_item(pclCcaCkic,pclDataBuf,3);
     }
  }
  else
  {
     ilItemNo = get_item_no(pcpFields,"CTYP",5) + 1;
     get_real_item(pclCcaCtyp,pcpNewData,ilItemNo);
     ilItemNo = get_item_no(pcpFields,"FLNU",5) + 1;
     get_real_item(pclCcaFlnu,pcpNewData,ilItemNo);
     ilItemNo = get_item_no(pcpFields,"CKIC",5) + 1;
     get_real_item(pclCcaCkic,pcpNewData,ilItemNo);
  }
  TrimRight(pclCcaCtyp);
  TrimRight(pclCcaFlnu);
  TrimRight(pclCcaCkic);

  if (*pclCcaCtyp == ' ')
  {
     ilRC = GetKeyFieldList(pclFieldList,&ilNoEle);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     strcat(pclFieldList,"STOA,STOD");
     ilNoEle += 2;
     if (strstr(pclFieldList,"ADID") == NULL)
     {
        strcat(pclFieldList,",ADID");
        ilNoEle++;
     }
     sprintf(pclSelection,"WHERE URNO = %s",pclCcaFlnu);
     sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s 3-SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s Flight with URNO = <%s> not found",pclFunc,pclCcaFlnu);
        return ilRC;
     }
     BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
     strcat(pclFieldList,",URNO");
     strcat(pclDataBuf,",");
     strcat(pclDataBuf,pclCcaFlnu);
     ilNoEle++;
     ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
     get_real_item(pcgAdid,pclDataBuf,ilItemNo);
     ilRC = StoreKeyData(pclFieldList,pclDataBuf,ilNoEle,pcpFields,pcpNewData,pcpOldData,pcpCommand,TRUE);
     if (ilRC != RC_SUCCESS)
        return ilRC;
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DEDICATED_COUNTER_SECTION",CFG_STRING,pclSections);
  }
  else
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","COMMON_COUNTER_SECTION",CFG_STRING,pclSections);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for Counters defined",pclFunc);
     return RC_FAIL;
  }
  ilAddLocation = FALSE;
  ilRC = BuildHeader(pcpCommand);
  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
     strcpy(pclAddFldLst,"");
     get_real_item(pclCurSec,pclSections,ilI);
     ilNoEle = GetNoOfElements(pcpFields,',');
     ilDatCnt = 0;
     ilRC = StoreData(pcpFields,pcpNewData,pcpOldData,ilNoEle,pclCurSec,&ilDatCnt);
     if (ilDatCnt > 0 || *pcpCommand == 'D')
     {
        if (*pcpCommand == 'D')
        {
           strcpy(pcpFields,"CKIC");
           strcpy(pcpNewData,"");
           strcpy(pcpOldData,pclCcaCkic);
           ilNoEle = 0;
        }
        else
           ilRC = GetManFieldList(pclCurSec,pclFieldList,&ilNoEle,pcpFields,TRUE);
        if (ilNoEle > 0)
        {
           sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
           sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s 4-SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);

            memset(pclData,0,sizeof(pclData));
                        strcat(pclData,pclCcaCkicTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaCtypTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaCkitTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaCkbsTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaCkesTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaFlnuTemp);
                        strcat(pclData,",");
                        strcat(pclData,pclCcaUrnoTemp);
                        dbg(TRACE,"%s pclData<%s>",pclFunc,pclData);

           if (ilRCdb != DB_SUCCESS)
           {
                //FYA v1.45 UFIS-1901
                //dbg(TRACE,"%s PROCEED_WITHOUT_CCATAB = %s",pclFunc,pcgProceedWithoutCCATAB);

                        if(strncmp(pcgProceedWithoutCCATAB,"YES",3) == 0)
                        {
                            dbg(TRACE,"%s PROCEED_WITHOUT_CCATAB = YES",pclFunc);
                            memset(pclDataBuf,0,sizeof(pclDataBuf));
                            strcpy(pclDataBuf,pclData);
                            dbg(TRACE,"%s pclDataBuf<%s>",pclFunc,pclDataBuf);
                        }
                        else
                        {
                            dbg(TRACE,"%s PROCEED_WITHOUT_CCATAB != YES",pclFunc);
                  dbg(TRACE,"%s 2-CCA Record with URNO = <%s> not found",pclFunc,pcpUrno);
              return ilRC;
           }
                        //FYA v1.45 UFIS-1901

           }
           BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
           sprintf(pclNewFieldList,"%s,%s",pcpFields,pclFieldList);
           sprintf(pclNewDataList,"%s,%s",pcpNewData,pclDataBuf);
           sprintf(pclOldDataList,"%s,%s",pcpOldData,pclDataBuf);

           //FYA v1.45 UFIS-1901
           dbg(TRACE,"pclNewFieldList<%s>",pclNewFieldList);
           dbg(TRACE,"pclNewDataList<%s>",pclNewDataList);
           dbg(TRACE,"pclOldDataList<%s>",pclOldDataList);
           dbg(TRACE,"pclDataBuf<%s>",pclDataBuf);
           //FYA v1.45 UFIS-1901
/*
           sprintf(pclOldDataList,"%s",pcpOldData);
           for (ilJ = 0; ilJ < ilNoEle; ilJ++)
              strcat(pclOldDataList,",");
*/
        }
        else
        {
           strcpy(pclNewFieldList,pcpFields);
           strcpy(pclNewDataList,pcpNewData);
           strcpy(pclOldDataList,pcpOldData);
        }
        ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
        if (ilIdx >= 0)
        {
           ilNoEle = GetNoOfElements(pclNewFieldList,',');
           ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);
           if (ilRC != RC_SUCCESS)
              return ilRC;
           strcpy(pclFkt1,pcpCommand);
           ilRaid = GetXmlLineIdx("DAT","RAID",pclCurSec);
           if (ilRaid >= 0)
           {
              ilItemNo = get_item_no(pclNewFieldList,"CKIC",5) + 1;
              if (ilItemNo <= 0)
              {
                 strcat(pclNewFieldList,",CKIC");
                 strcat(pclNewDataList,",");
                 strcat(pclNewDataList,pclCcaCkic);
                 strcat(pclOldDataList,",");
                 strcat(pclOldDataList,pclCcaCkic);
              }
              ilRC = HandleRaid(pcpUrno,ilRaid,pclCurSec,pclNewFieldList,pclNewDataList,pclOldDataList,
                                pclFkt1,pclCcaCtyp,pclRarUrno,pclAddFldLst);
           }
           ilAlcd = GetXmlLineIdx("DAT","ALCD",pclCurSec);
           if (ilAlcd >= 0)
           {
              ilCount = 1;
              ilRC = syslibSearchDbData("ALTTAB","URNO",pclCcaFlnu,"ALC2",pclTmpBuf,&ilCount,"\n");
              TrimRight(pclTmpBuf);
              if (*pclTmpBuf == ' ')
              {
                 ilCount = 1;
                 ilRC = syslibSearchDbData("ALTTAB","URNO",pclCcaFlnu,"ALC3",pclTmpBuf,&ilCount,"\n");
                 TrimRight(pclTmpBuf);
              }
              if (ilRC == RC_SUCCESS)
              {
                 strcpy(&rgXml.rlLine[ilAlcd].pclData[0],pclTmpBuf);
                 rgXml.rlLine[ilAlcd].ilRcvFlag = 1;
              }
           }
           ilCtyp = GetXmlLineIdx("DAT","CTYP",pclCurSec);
           if (ilCtyp >= 0)
           {
              if (*pclCcaCtyp == ' ')
              {
                 strcpy(&rgXml.rlLine[ilCtyp].pclData[0],"D");
                 rgXml.rlLine[ilCtyp].ilRcvFlag = 1;
              }
           }
           ilAtIdx = GetXmlLineIdx("HDR",pcgActionType,NULL);
           ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
           for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
           {
              strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
              strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);
              ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);

              if (ilMtIdx >= 0)
                 strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);

              dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
                  pclFunc,pclCurIntf,pclCurSec,pclMsgType);

              strcpy(pclCommand,pcpCommand);
              if (strcmp(pcpCommand,pclFkt1) != 0)
              {
                 strcpy(pclTmpFld,&rgXml.rlLine[ilRaid].pclFieldD[ilJ][0]);
                 strcpy(pclCommand,pclFkt1);
                 if (ilAtIdx >= 0)
                    rgXml.rlLine[ilAtIdx].pclData[0] = *pclCommand;
              }
              ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
              dbg(DEBUG,"%s pclCurIntf=%s,pclCurSec=%s,pclAllowedActions=%s",pclFunc,pclCurIntf,pclCurSec,pclAllowedActions);

              if (ilRC != RC_SUCCESS)
              {
                 dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
              }
              else
              {
                 if (strstr(pclAllowedActions,pclCommand) == NULL)
                 {
                    dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pclCommand);
                 }
                 else
                 {
                    ilSendOutput = TRUE;
                    ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PENDING_ACK_SECTIONS",
                                           CFG_STRING,pclPendingAck);
                    if (ilRC == RC_SUCCESS && strstr(pclPendingAck,pclCurSec) != NULL)
                    {
                       sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s AND TYPE = 'ACK'",
                               pclCcaFlnu);
                       slCursor = 0;
                       slFkt = START;
                       dbg(DEBUG,"%s 5-SQL = <%s>",pclFunc,pclSqlBuf);
                       ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                       if (ilRCdb == DB_SUCCESS)
                       {
                          dbg(TRACE,"%s Action not yet allowed due to pending ACK",pclFunc);
                          ilSendOutput = FALSE;
                       }
                       close_my_cursor(&slCursor);
                    }
                    if (ilSendOutput == TRUE)
                    {
                       /*ilRC = ShowData();*/
                       ilRC = MarkOutput();
                       /*ilRC = ShowAll();*/

                       ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,pclCcaCtyp,pclCurIntf,"C");

                    }
                 }
              }
              if (ilAtIdx >= 0)
                 rgXml.rlLine[ilAtIdx].pclData[0] = *pcpCommand;
           }
        }
     }
     if (strlen(pclAddFldLst) > 0)
     {
        dbg(DEBUG,"%s",pclFunc);
        dbg(DEBUG,"%s =================================== NEW SECTION ===================================",pclFunc);
        ilAddLocation = TRUE;
        strcpy(pclSaveFields,pcpFields);
        strcpy(pclSaveNewData,pcpNewData);
        strcpy(pclSaveOldData,pcpOldData);
        strcpy(pcpFields,"URNO,");
        strcat(pcpFields,pclAddFldLst);
        sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pcpFields,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s 6-SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pcpNewData);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           ilTmpNoEle = GetNoOfElements(pcpFields,',');
           BuildItemBuffer(pcpNewData,"",ilTmpNoEle,",");
           strcpy(pcpOldData,pcpUrno);
           for (ilTmp = 0; ilTmp < ilTmpNoEle-1; ilTmp++)
              strcat(pcpOldData,",");
           ilI--;
           ilRC = ClearSection(pclCurSec);
        }
        else
        {
           strcpy(pcpFields,pclSaveFields);
           strcpy(pcpNewData,pclSaveNewData);
           strcpy(pcpOldData,pclSaveOldData);
           ilAddLocation = FALSE;
        }
     }
     else
     {
        if (ilAddLocation == TRUE)
        {
           strcpy(pcpFields,pclSaveFields);
           strcpy(pcpNewData,pclSaveNewData);
           strcpy(pcpOldData,pclSaveOldData);
           ilAddLocation = FALSE;
        }
     }
  }

  return ilRC;
} /* End of HandleCounterOut */


static int HandleBlktabOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleBlktabOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclNewFieldList[1024];
  char pclNewDataList[1024];
  char pclOldDataList[1024];
  int ilItemNo;
  char pclBlkBurn[16];
  char pclBlkTabn[16];
  int ilLnam;
  char pclLnam[16];
  char pclSections[1024];
  char pclCurSec[32];
  int ilNoSec;
  int ilI;
  int ilJ;
  char pclLocNam[16];
  int ilIdx;
  int ilNoEle;
  int ilNoFlds;
  char pclFld[16];
  char pclDat[4000];
  int ilDatCnt;
  int ilNoIntf;
  char pclCurIntf[32];
  char pclAllowedActions[32];
  int ilCount;
  char pclMsgType[32];
  int ilMtIdx;

  if (strstr(pcpFields,"BURN") == NULL || strstr(pcpFields,"TABN") == NULL)
  {
     strcpy(pclFieldList,"BURN,TABN");
     sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
     sprintf(pclSqlBuf,"SELECT %s FROM BLKTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s BLK Record with URNO = <%s> not found",pclFunc,pcpUrno);
        return ilRC;
     }
     BuildItemBuffer(pclDataBuf,"",2,",");
     get_real_item(pclBlkBurn,pclDataBuf,1);
     get_real_item(pclBlkTabn,pclDataBuf,2);
  }
  else
  {
     ilItemNo = get_item_no(pcpFields,"BURN",5) + 1;
     get_real_item(pclBlkBurn,pcpNewData,ilItemNo);
     ilItemNo = get_item_no(pcpFields,"TABN",5) + 1;
     get_real_item(pclBlkTabn,pcpNewData,ilItemNo);
  }
  strcat(pclBlkTabn,"TAB");
  TrimRight(pclBlkBurn);
  TrimRight(pclBlkTabn);
  if (strcmp(pclBlkTabn,"PSTTAB") == 0)
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","STATIC_SECTION_POSITION",CFG_STRING,pclSections);
  else if (strcmp(pclBlkTabn,"BLTTAB") == 0)
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","STATIC_SECTION_BELT",CFG_STRING,pclSections);
  else if (strcmp(pclBlkTabn,"GATTAB") == 0)
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","STATIC_SECTION_GATE",CFG_STRING,pclSections);
  else if (strcmp(pclBlkTabn,"CICTAB") == 0)
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","STATIC_SECTION_COUNTER",CFG_STRING,pclSections);
  else
     ilRC = RC_FAIL;
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Section(s) for Static Table <%s> defined",pclFunc,pclBlkTabn);
     return RC_FAIL;
  }
  ilRC = BuildHeader(pcpCommand);
  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
     get_real_item(pclCurSec,pclSections,ilI);
     ilLnam = GetXmlLineIdxBas(pclCurSec,1);
     strcpy(pclLnam,&rgXml.rlLine[ilLnam].pclBasTab[0]);
     sprintf(pclFieldList,"%s",pclLnam);
     sprintf(pclSelection,"WHERE URNO = %s",pclBlkBurn);
     sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclFieldList,pclBlkTabn,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclLocNam);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s %s Record with URNO = <%s> not found",pclFunc,pclBlkTabn,pclBlkBurn);
        return ilRC;
     }
     TrimRight(pclLocNam);
     ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
     if (ilIdx >= 0)
     {
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcpUrno);
        rgXml.rlLine[ilIdx].ilRcvFlag = 1;
     }
     strcpy(pclFieldList,"");
     ilNoEle = 0;
     ilRC = GetFieldList(pclCurSec,pclFieldList,&ilNoEle);
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     if (ilNoEle == 0)
        return RC_FAIL;
     strcpy(pclNewFieldList,"");
     strcpy(pclNewDataList,"");
     strcpy(pclOldDataList,"");
     ilNoEle = 0;
     ilNoFlds = GetNoOfElements(pclFieldList,',');
     for (ilJ = 1; ilJ <= ilNoFlds; ilJ++)
     {
        get_real_item(pclFld,pclFieldList,ilJ);
        if (strcmp(pclFld,pclLnam) == 0)
        {
           strcat(pclNewFieldList,pclFld);
           strcat(pclNewFieldList,",");
           strcat(pclNewDataList,pclLocNam);
           strcat(pclNewDataList,",");
           strcat(pclOldDataList," ,");
           ilNoEle++;
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,pclFld,5) + 1;
           if (ilItemNo > 0)
           {
              strcat(pclNewFieldList,pclFld);
              strcat(pclNewFieldList,",");
              get_real_item(pclDat,pcpNewData,ilItemNo);
              strcat(pclNewDataList,pclDat);
              strcat(pclNewDataList,",");
              strcat(pclOldDataList," ,");
              ilNoEle++;
           }
        }
     }
     if (strlen(pclNewFieldList) > 0)
     {
        pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
        pclNewDataList[strlen(pclNewDataList)-1] = '\0';
        pclOldDataList[strlen(pclOldDataList)-1] = '\0';
     }
     ilDatCnt = 0;
     ilRC = StoreData(pclNewFieldList,pclNewDataList,pclOldDataList,ilNoEle,pclCurSec,&ilDatCnt);
     if (ilRC != RC_SUCCESS || ilDatCnt == 0)
        return ilRC;
     ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
     if (ilIdx >= 0)
     {
        ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
        for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
        {
           strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
           strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);
           ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
           if (ilMtIdx >= 0)
              strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);
           dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
               pclFunc,pclCurIntf,pclCurSec,pclMsgType);
           ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
           if (ilRC != RC_SUCCESS)
           {
              dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
           }
           else
           {
              if (strstr(pclAllowedActions,pcpCommand) == NULL)
              {
                 dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pcpCommand);
              }
              else
              {
                 /*ilRC = ShowData();*/
                 ilRC = MarkOutput();
                 /*ilRC = ShowAll();*/
                 ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,"A",pclCurIntf,"C");
              }
           }
        }
     }
  }

  return ilRC;
} /* End of HandleBlktabOut */


static int HandleFileRequest(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleFileRequest:";
  char pclMethod[32];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclSelection[1024];
  char pclModId[32];
  char pclPriority[32];
  char pclCommand[32];
  char pclTable[32];
  int ilModId;
  int ilPriority;
  int ilIdx;
  char pclTmpBuf[1024];

  strcpy(pclMethod,"");
  ilRC = GetData(pclMethod,0,pcpSection);
  strcpy(pclMethod,&rgData[0].pclMethod[0]);
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  strcpy(pclTable,"");
  ilIdx = GetDataLineIdx("FILENAME",0);
  if (ilIdx < 0)
  {
     dbg(TRACE,"%s No File Name specified",pclFunc);
     return RC_FAIL;
  }
  strcpy(pclTmpBuf,&rgData[ilIdx].pclData[0]);
  ilRC = iGetConfigEntry(pcgConfigFile,"FILE_NAMES",pclTmpBuf,CFG_STRING,pclSelection);
  if (ilRC != RC_SUCCESS)
     strcpy(pclSelection,pclTmpBuf);
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"PCC");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"7924");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
  dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
  dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
  dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
  ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                       pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);

  return ilRC;
} /* End of HandleFileRequest */


static int HandleFileReady(char *pcpSelection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleFileReady:";
  char pclSection[1024];
  int ilIdx;
  int ilNoIntf;
  int ilI;
  char pclCurIntf[32];
  int ilCount;
  char pclOrigin[128];
  char pclAction[128];

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FILE_READY_ORIGIN",CFG_STRING,pclOrigin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclOrigin,"UFIS");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FILE_READY_ACTION",CFG_STRING,pclAction);
  if (ilRC != RC_SUCCESS)
     strcpy(pclAction,"HANDLEFTP");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","FILE_READY",CFG_STRING,pclSection);
  ilIdx = GetXmlLineIdx("DAT","ORIGIN",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclOrigin);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","FILENAME",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcpSelection);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","FILEACTION",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclAction);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT",pcgTimeStamp,pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcgCurrentTime);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","CONTENT_OUT",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  else
  {
     ilIdx = GetXmlLineIdx("DAT","CONTENT_IN",pclSection);
     if (ilIdx >= 0)
     {
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
        rgXml.rlLine[ilIdx].ilRcvFlag = 1;
     }
  }

  ilIdx = GetXmlLineIdx("STR_BGN",pclSection,NULL);
  if (ilIdx >= 0)
  {
     ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
     for (ilI = 0; ilI < ilNoIntf; ilI++)
     {
        strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilI][0]);
        dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>",
            pclFunc,pclCurIntf,pclSection);
        /*ilRC = ShowData();*/
        ilRC = MarkOutput();
        /*ilRC = ShowAll();*/
        ilRC = BuildOutput(ilI,&ilCount,pclSection,"A",pclCurIntf,"C");
     }
  }

  return ilRC;
} /* End of HandleFileReady */


static int HandleDataSync(char *pcpSelection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleDataSync:";
  char pclSection[1024];
  char pclOrigSection[1024];
  int ilIdx;
  int ilNoIntf;
  int ilI;
  char pclCurIntf[32];
  int ilCount;
  char pclOrigin[128];
  char pclAction[128];
  char pclTags[1024];
  int ilNoSec;
  char pclCurTag[128];
  char pclTmpBuf[1024];

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DATA_SYNC_ORIGIN",CFG_STRING,pclOrigin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclOrigin,"ARL-FIDS");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DATA_SYNC_ACTION",CFG_STRING,pclAction);
  if (ilRC != RC_SUCCESS)
     strcpy(pclAction,"FILEREQUEST");

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DATA_SYNC",CFG_STRING,pclSection);
  strcpy(pclOrigSection,pclSection);
  ilIdx = GetXmlLineIdx("DAT","ORIGIN",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclOrigin);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","FILENAME",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcpSelection);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","FILEACTION",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclAction);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT",pcgTimeStamp,pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],pcgCurrentTime);
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilIdx = GetXmlLineIdx("DAT","CONTENT_OUT",pclSection);
  if (ilIdx >= 0)
  {
     strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  else
  {
     ilIdx = GetXmlLineIdx("DAT","CONTENT_IN",pclSection);
     if (ilIdx >= 0)
     {
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"");
        rgXml.rlLine[ilIdx].ilRcvFlag = 1;
     }
  }

/********************** LIS ATLANTIS (CONFIGURABLE) ****************************/
  ilRC = iGetConfigEntry(pcgConfigFile,pclSection,"TAGS",CFG_STRING,pclTags);
  strcpy(pclSection,pclOrigSection);
  if (ilRC == RC_SUCCESS)
  {
     ilNoSec = GetNoOfElements(pclTags,',');
     for (ilI = 1; ilI <= ilNoSec; ilI++)
     {
        get_real_item(pclCurTag,pclTags,ilI);
        ilIdx = GetXmlLineIdx("DAT",pclCurTag,pclSection);
        if (ilIdx >= 0)
        {
           ilRC = iGetConfigEntry(pcgConfigFile,pclSection,pclCurTag,CFG_STRING,pclTmpBuf);
           strcpy(pclSection,pclOrigSection);
           if (ilRC == RC_SUCCESS)
           {
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpBuf);
              rgXml.rlLine[ilIdx].ilRcvFlag = 1;
           }
        }
     }
     ilRC = iGetConfigEntry(pcgConfigFile,pclSection,"HEADER_COMMAND",CFG_STRING,pclTmpBuf);
     strcpy(pclSection,pclOrigSection);
     if (ilRC == RC_SUCCESS)
        ilRC = BuildHeader(pclTmpBuf);
  }
/********************** LIS ATLANTIS (CONFIGURABLE) ****************************/

  ilIdx = GetXmlLineIdx("STR_BGN",pclSection,NULL);
  if (ilIdx >= 0)
  {
     ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
     for (ilI = 0; ilI < ilNoIntf; ilI++)
     {
        strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilI][0]);
        dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>",
            pclFunc,pclCurIntf,pclSection);
        /*ilRC = ShowData();*/
        ilRC = MarkOutput();
        /*ilRC = ShowAll();*/
        ilRC = BuildOutput(ilI,&ilCount,pclSection,"A",pclCurIntf,"C");
     }
  }

  return ilRC;
} /* End of HandleDataSync */


static int HandleRequest(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleRequest:";
  char pclMethod[32];
  char pclData[32];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclSelection[1024];
  char pclModId[32];
  char pclPriority[32];
  char pclCommand[32];
  char pclTable[32];
  int ilModId;
  int ilPriority;
  int ilIdx;
  char pclTmpBuf[1024];

  strcpy(pclMethod,"");
  ilRC = GetData(pclMethod,0,pcpSection);
  strcpy(pclData,&rgData[0].pclData[0]);
  strcpy(pclSelection,"");
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  strcpy(pclTable,"");
  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,"REQUEST_LIST",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     if (strstr(pclTmpBuf,pclData) != NULL)
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pclData,"snd_cmd",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"PCC");
        ilRC = iGetConfigEntry(pcgConfigFile,pclData,"mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7924");
        ilRC = iGetConfigEntry(pcgConfigFile,pclData,"priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
        ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                             ilPriority,RC_SUCCESS);
     }
  }

  return ilRC;
} /* End of HandleRequest */


static int HandleFile(char *pcpCommand, char *pcpTable, char *pcpSelection, char *pcpFields, char *pcpNewData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleFile:";
  char pclFieldList[1024];
  char pclSeperator[16];

  if (strcmp(pcpSelection,"GMS_LONG_TERM") == 0)
  {
     ilRC = HandleGMSLongTerm(pcpNewData);
  }
  else if (strcmp(pcpSelection,"GMS_SHORT_TERM") == 0)
  {
     ilRC = HandleGMSShortTerm(pcpNewData);
  }
  else if (strcmp(pcpSelection,"GMS_RESOURCE") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_RES_FLD_LST",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"RSID,RSTY,RAID,ALCD,HANM,ACTS,ACTE,SCHS,SCHE,DISP,RMRK,URNO,FLNO,CSGN,STDT,ADID");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     igRsid = get_item_no(pclFieldList,"RSID",5) + 1;
     igRsty = get_item_no(pclFieldList,"RSTY",5) + 1;
     igRaid = get_item_no(pclFieldList,"RAID",5) + 1;
     igAlcd = get_item_no(pclFieldList,"ALCD",5) + 1;
     igHanm = get_item_no(pclFieldList,"HANM",5) + 1;
     igActs = get_item_no(pclFieldList,"ACTS",5) + 1;
     igActe = get_item_no(pclFieldList,"ACTE",5) + 1;
     igSchs = get_item_no(pclFieldList,"SCHS",5) + 1;
     igSche = get_item_no(pclFieldList,"SCHE",5) + 1;
     igDisp = get_item_no(pclFieldList,"DISP",5) + 1;
     igRmrk = get_item_no(pclFieldList,"RMRK",5) + 1;
     igUrno = get_item_no(pclFieldList,"URNO",5) + 1;
     igFlno = get_item_no(pclFieldList,"FLNO",5) + 1;
     igCsgn = get_item_no(pclFieldList,"CSGN",5) + 1;
     igStdt = get_item_no(pclFieldList,"STDT",5) + 1;
     igAdid = get_item_no(pclFieldList,"ADID",5) + 1;
     if (igRsid <= 0 || igRsty <= 0 || igRaid <= 0 || igAlcd <= 0 ||
         igHanm <= 0 || igActs <= 0 || igActe <= 0 || igSchs <= 0 ||
         igSche <= 0 || igDisp <= 0 || igRmrk <= 0 || igUrno <= 0 ||
         igFlno <= 0 || igCsgn <= 0 || igStdt <= 0 || igAdid <= 0)
     {
        dbg(TRACE,"%s Invalid FieldList <%s> for GMS RESOURCE specified",pclFunc,pclFieldList);
        return RC_FAIL;
     }
     ilRC = HandleGMSBelt(pcpNewData,pclSeperator,pcpFields);
     ilRC = HandleGMSGate(pcpNewData,pclSeperator,pcpFields);
     ilRC = HandleGMSPos(pcpNewData,pclSeperator,pcpFields);
     ilRC = HandleGMSCounterDedi(pcpNewData,pclSeperator,pcpFields);
     ilRC = HandleGMSCounterCom(pcpNewData,pclSeperator,pcpFields);
  }
  else if (strcmp(pcpSelection,"GMS_TOWING") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_TOW_FLD_LST",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"TOID,SCHS,SCHE,ACTS,ACTE,PSTD,PSTA,TWTP,ADID,URNO,FLNO,CSGN,STDT");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     igToid = get_item_no(pclFieldList,"TOID",5) + 1;
     igSchs = get_item_no(pclFieldList,"SCHS",5) + 1;
     igSche = get_item_no(pclFieldList,"SCHE",5) + 1;
     igActs = get_item_no(pclFieldList,"ACTS",5) + 1;
     igActe = get_item_no(pclFieldList,"ACTE",5) + 1;
     igPstd = get_item_no(pclFieldList,"PSTD",5) + 1;
     igPsta = get_item_no(pclFieldList,"PSTA",5) + 1;
     igTwtp = get_item_no(pclFieldList,"TWTP",5) + 1;
     igAdid = get_item_no(pclFieldList,"ADID",5) + 1;
     igUrno = get_item_no(pclFieldList,"URNO",5) + 1;
     igFlno = get_item_no(pclFieldList,"FLNO",5) + 1;
     igCsgn = get_item_no(pclFieldList,"CSGN",5) + 1;
     igStdt = get_item_no(pclFieldList,"STDT",5) + 1;
     if (igToid <= 0 || igSchs <= 0 || igSche <= 0 || igActs <= 0 ||
         igActe <= 0 || igPstd <= 0 || igPsta <= 0 || igTwtp <= 0 ||
         igAdid <= 0 || igUrno <= 0 || igFlno <= 0 || igCsgn <= 0 ||
         igStdt <= 0)
     {
        dbg(TRACE,"%s Invalid FieldList <%s> for GMS TOWING specified",pclFunc,pclFieldList);
        return RC_FAIL;
     }
     ilRC = HandleGMSTowing(pcpNewData,pclSeperator,pcpFields);
  }
  else if (strcmp(pcpSelection,"GMS_RES_NOOP") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_NOOP_FLD_LST",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"RSID,RSTY,SCHS,SCHE,RMRK");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     igRsid = get_item_no(pclFieldList,"RSID",5) + 1;
     igRsty = get_item_no(pclFieldList,"RSTY",5) + 1;
     igSchs = get_item_no(pclFieldList,"SCHS",5) + 1;
     igSche = get_item_no(pclFieldList,"SCHE",5) + 1;
     igRmrk = get_item_no(pclFieldList,"RMRK",5) + 1;
     if (igRsid <= 0 || igSchs <= 0 || igSche <= 0 || igRsty <= 0 ||
         igRmrk <= 0)
     {
        dbg(TRACE,"%s Invalid FieldList <%s> for GMS NOOP RESOURCES specified",pclFunc,pclFieldList);
        return RC_FAIL;
     }
     ilRC = HandleGMSNoop(pcpNewData,pclSeperator,pcpFields);
  }
  else if (strcmp(pcpSelection,"GMS_RESOURCE_OUT") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_RES_FLD_LST_OUT",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"RSID,RSTY,RAID,ALCD,HANM,ACTS,ACTE,SCHS,SCHE,DISP,RMRK,URNO,FLNO,CSGN,STDT,ADID");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR_OUT",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     ilRC = HandleGMSFileOut(pcpNewData,pclSeperator,pclFieldList,pcpSelection);
  }
  else if (strcmp(pcpSelection,"GMS_TOWING_OUT") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_TOW_FLD_LST_OUT",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"TOID,SCHS,SCHE,ACTS,ACTE,PSTD,PSTA,TWTP,ADID,URNO,FLNO,CSGN,STDT");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR_OUT",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     ilRC = HandleGMSFileOut(pcpNewData,pclSeperator,pclFieldList,pcpSelection);
  }
  else if (strcmp(pcpSelection,"GMS_RES_NOOP_OUT") == 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_NOOP_FLD_LST_OUT",CFG_STRING,pclFieldList);
     if (ilRC != RC_SUCCESS)
        strcpy(pclFieldList,"RSID,RSTY,SCHS,SCHE,RMRK");
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","GMS_SEPERATOR_OUT",CFG_STRING,pclSeperator);
     if (ilRC != RC_SUCCESS)
        strcpy(pclSeperator,",");
     ilRC = HandleGMSFileOut(pcpNewData,pclSeperator,pclFieldList,pcpSelection);
  }
  else if (strcmp(pcpSelection,"EQI_USAGE") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleEqiUsage(pcpNewData);
  }
  else if (strcmp(pcpSelection,"ARS_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleArsData(pcpNewData);
  }
  else if (strcmp(pcpSelection,"SEASON_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleSeasonData(pcpNewData);
  }
  else if (strcmp(pcpSelection,"DAILY_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleDailyData(pcpNewData);
  }
  else if (strstr(pcpSelection,"D_DATA_UPD") != NULL)
  {
     GetFileName(pcpNewData);
     ilRC = HandleDailyDataUpdate(pcpNewData,pcpSelection);
  }
  else if (strcmp(pcpSelection,"COUNTER_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleCounterData(pcpNewData);
  }
  else if (strcmp(pcpSelection,"BASIC_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandleBasicData(pcpNewData,pcpFields);
  }
  else if (strcmp(pcpSelection,"PLB_DATA") == 0)
  {
     GetFileName(pcpNewData);
     ilRC = HandlePlbData(pcpNewData);
  }
  else
  {
     dbg(TRACE,"%s Invalid Selection <%s>",pclFunc,pcpSelection);
     return RC_FAIL;
  }

  return ilRC;
} /* End of HandleFile */


static int HandleGMSBelt(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSBelt:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"BB") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"BB");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"AFTTAB","BB","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"BB") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSResLine(pclLine,pcpSeperator,"BB");
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
        {
           ilRC = HandleGMSResource(pclUrno,"BB",pclLine);
           if (ilRC == RC_SUCCESS)
              ilRC = RemoveUrno(pclUrno,"A");
        }
     }
  }
  fclose(fp);
  ilRC = HandleGMSDelete("A","BB");

  return ilRC;
} /* End of HandleGMSBelt */


static int HandleGMSGate(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSGate:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"GT") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"GT");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"AFTTAB","GT","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"GT") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSResLine(pclLine,pcpSeperator,"GT");
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
        {
           ilRC = HandleGMSResource(pclUrno,"GT",pclLine);
           if (ilRC == RC_SUCCESS)
           {
              if (*pcgGMSResAdid == 'A')
                 ilRC = RemoveUrno(pclUrno,"A");
              else
                 ilRC = RemoveUrno(pclUrno,"D");
           }
        }
     }
  }
  fclose(fp);
  ilRC = HandleGMSDelete("A","GT");
  ilRC = HandleGMSDelete("D","GT");

  return ilRC;
} /* End of HandleGMSGate */


static int HandleGMSPos(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSPos:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"POS") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"POS");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"AFTTAB","POS","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"POS") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSResLine(pclLine,pcpSeperator,"POS");
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
        {
           ilRC = HandleGMSResource(pclUrno,"POS",pclLine);
           if (ilRC == RC_SUCCESS)
           {
              if (*pcgGMSResAdid == 'A')
                 ilRC = RemoveUrno(pclUrno,"A");
              else
                 ilRC = RemoveUrno(pclUrno,"D");
           }
        }
     }
  }
  fclose(fp);
  ilRC = HandleGMSDelete("A","POS");
  ilRC = HandleGMSDelete("D","POS");

  return ilRC;
} /* End of HandleGMSPos */


static int HandleGMSCounterDedi(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSCounterDedi:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"CI") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"CI");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"CCATAB","CI","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"CI") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSResLine(pclLine,pcpSeperator,"CI");
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
           ilRC = HandleGMSResourceDcnt(pclUrno,"CI",pclLine);
     }
  }
  fclose(fp);
  ilRC = HandleGMSDeleteCnt("D","CI");

  return ilRC;
} /* End of HandleGMSCounterDedi */


static int HandleGMSCounterCom(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSCounterCom:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"CCI") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"CCI");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"CCATAB","CCI","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"CCI") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSResLine(pclLine,pcpSeperator,"CCI");
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
           ilRC = HandleGMSResourceCcnt(pclUrno,"CCI",pclLine);
     }
  }
  fclose(fp);
  ilRC = HandleGMSDeleteCnt("D","CCI");

  return ilRC;
} /* End of HandleGMSCounterCom */


static int HandleGMSTowing(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSTowing:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"TOW");
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"AFTTAB","TOW","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
     ilRC = CheckGMSTowLine(pclLine,pcpSeperator);
     if (ilRC != RC_SUCCESS)
        dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
     else
     {
        ilRC = HandleGMSResTow(pclUrno,pclLine);
     }
  }
  fclose(fp);
  ilRC = HandleGMSDeleteTow("A");
  ilRC = HandleGMSDeleteTow("D");

  return ilRC;
} /* End of HandleGMSTowing */


static int HandleGMSNoop(char *pcpFile, char *pcpSeperator, char *pcpFields)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSNoop:";
  FILE *fp;
  int ilI;
  char pclLine[8000];
  char pclRsty[16];
  char pclMinArr[16] = "";
  char pclMaxArr[16] = "";
  char pclMinDep[16] = "";
  char pclMaxDep[16] = "";
  char pclUrno[16];
  char pclBegin[16];
  char pclEnd[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  dbg(DEBUG,"%s",pclFunc);
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"BB") == 0 || strcmp(pclRsty,"GT") == 0 || strcmp(pclRsty,"POS") == 0 ||
         strcmp(pclRsty,"CI") == 0 || strcmp(pclRsty,"CCI") == 0)
     {
        ilRC = GetMinMaxTimes(pclLine,pcpSeperator,pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"NOOP");
     }
  }
  fclose(fp);
  if (GetNoOfElements(pcpFields,',') == 2)
  {
     get_real_item(pclBegin,pcpFields,1);
     get_real_item(pclEnd,pcpFields,2);
     if (strlen(pclMinArr) > 0)
     {
        if (strcmp(pclBegin,pclMinArr) < 0)
           strcpy(pclMinArr,pclBegin);
        if (strcmp(pclEnd,pclMaxArr) > 0)
           strcpy(pclMaxArr,pclEnd);
     }
     if (strlen(pclMinDep) > 0)
     {
        if (strcmp(pclBegin,pclMinDep) < 0)
           strcpy(pclMinDep,pclBegin);
        if (strcmp(pclEnd,pclMaxDep) > 0)
           strcpy(pclMaxDep,pclEnd);
     }
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"BLKTAB","NOOP","");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
     TrimRight(pclRsty);
     if (strcmp(pclRsty,"BB") == 0 || strcmp(pclRsty,"GT") == 0 || strcmp(pclRsty,"POS") == 0 ||
         strcmp(pclRsty,"CI") == 0 || strcmp(pclRsty,"CCI") == 0)
     {
        dbg(DEBUG,"%s Input: %s",pclFunc,pclLine);
        ilRC = CheckGMSNoopLine(pclLine,pcpSeperator);
        if (ilRC != RC_SUCCESS)
           dbg(TRACE,"%s Invalid Input Line <%s>",pclFunc,pclLine);
        else
        {
           ilRC = HandleGMSResNoop(pclUrno,pclLine);
           if (ilRC == RC_SUCCESS)
              ilRC = RemoveUrno(pclUrno,"A");
        }
     }
  }
  fclose(fp);
  ilRC = HandleGMSDeleteNoop();

  return ilRC;
} /* End of HandleGMSNoop */


static int GetMinMaxTimes(char *pcpLine, char *pcpSeperator, char *pcpMinArr, char *pcpMaxArr,
                          char *pcpMinDep, char *pcpMaxDep, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetMinMaxTimes:";
  char pclTime[16];
  char pclAdid[16];
  int ilDay;
  int ilMin;
  int ilWkDay;

  if (strcmp(pcpType,"BB") == 0 || strcmp(pcpType,"GT") == 0 || strcmp(pcpType,"POS") == 0)
  {
     GetDataItem(pclAdid,pcpLine,igAdid,*pcpSeperator,""," ");
     TrimRight(pclAdid);
     GetDataItem(pclTime,pcpLine,igStdt,*pcpSeperator,""," ");
     TrimRight(pclTime);
  }
  else if (strcmp(pcpType,"CI") == 0 || strcmp(pcpType,"CCI") == 0)
  {
     strcpy(pclAdid,"D");
     GetDataItem(pclTime,pcpLine,igSchs,*pcpSeperator,""," ");
     TrimRight(pclTime);
  }
  else if (strcmp(pcpType,"TOW") == 0)
  {
     GetDataItem(pclAdid,pcpLine,igAdid,*pcpSeperator,""," ");
     TrimRight(pclAdid);
     GetDataItem(pclTime,pcpLine,igSchs,*pcpSeperator,""," ");
     TrimRight(pclTime);
  }
  else if (strcmp(pcpType,"NOOP") == 0)
  {
     strcpy(pclAdid,"A");
     GetDataItem(pclTime,pcpLine,igSchs,*pcpSeperator,""," ");
     TrimRight(pclTime);
  }
  if (strlen(pclTime) < 9)
     ilRC = RC_FAIL;
  else
  {
     if (strlen(pclTime) == 12)
        strcat(pclTime,"00");
     ilRC = GetFullDay(pclTime,&ilDay,&ilMin,&ilWkDay);
  }
  if (ilRC != RC_SUCCESS)
     return ilRC;
  if (*pclAdid == 'A')
  {
     if (strlen(pcpMinArr) == 0)
     {
        strcpy(pcpMinArr,pclTime);
        strcpy(pcpMaxArr,pclTime);
     }
     else
     {
        if (strcmp(pclTime,pcpMinArr) < 0)
           strcpy(pcpMinArr,pclTime);
        if (strcmp(pclTime,pcpMaxArr) > 0)
           strcpy(pcpMaxArr,pclTime);
     }
  }
  else if (*pclAdid == 'D')
  {
     if (strlen(pcpMinDep) == 0)
     {
        strcpy(pcpMinDep,pclTime);
        strcpy(pcpMaxDep,pclTime);
     }
     else
     {
        if (strcmp(pclTime,pcpMinDep) < 0)
           strcpy(pcpMinDep,pclTime);
        if (strcmp(pclTime,pcpMaxDep) > 0)
           strcpy(pcpMaxDep,pclTime);
     }
  }

  return ilRC;
} /* End of GetMinMaxTimes */


static int GetUrnoList(char *pcpMinArr, char *pcpMaxArr, char *pcpMinDep, char *pcpMaxDep, char *pcpTable, char *pcpType,
                       char *pcpExclCounterList)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetUrnoList:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  int ilI;

  strcpy(pcgUrnoListArr,"");
  strcpy(pcgUrnoListDep,"");
  if (strcmp(pcpType,"CI") == 0 || strcmp(pcpType,"CCI") == 0)
  {
     if (strlen(pcpMinDep) > 0)
     {
        ilI = 0;
        if (strcmp(pcpType,"CI") == 0)
           sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE CKBS BETWEEN '%s' AND '%s' AND CTYP = ' '",pcpTable,pcpMinDep,pcpMaxDep);
        else
           sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE CKBS BETWEEN '%s' AND '%s' AND CTYP = 'C'",pcpTable,pcpMinDep,pcpMaxDep);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListDep) < 119500)
        {
           strcat(pcgUrnoListDep,pclDataBuf);
           strcat(pcgUrnoListDep,",");
           ilI++;
           slFkt = NEXT;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        }
        close_my_cursor(&slCursor);
        if (strlen(pcgUrnoListDep) > 0)
           pcgUrnoListDep[strlen(pcgUrnoListDep)-1] = '\0';
        dbg(DEBUG,"%s Number of CCATAB Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListDep));
     }
     return ilRC;
  }
  if (strcmp(pcpType,"COUNTER") == 0)
  {
     if (strlen(pcpMinDep) > 0)
     {
        ilI = 0;
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE CKES BETWEEN '%s' AND '%s' AND CKIC NOT IN (%s)",
                pcpTable,pcpMinDep,pcpMaxDep,pcpExclCounterList);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListDep) < 119500)
        {
           strcat(pcgUrnoListDep,pclDataBuf);
           strcat(pcgUrnoListDep,",");
           ilI++;
           slFkt = NEXT;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        }
        close_my_cursor(&slCursor);
        if (strlen(pcgUrnoListDep) > 0)
           pcgUrnoListDep[strlen(pcgUrnoListDep)-1] = '\0';
        dbg(DEBUG,"%s Number of CCATAB Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListDep));
     }
     return ilRC;
  }
  if (strcmp(pcpType,"NOOP") == 0)
  {
     if (strlen(pcpMinArr) > 0)
     {
        ilI = 0;
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE NAFR BETWEEN '%s' AND '%s' AND TABN IN ('PST','BLT','GAT','CIC')",
                pcpTable,pcpMinArr,pcpMaxArr);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListArr) < 119500)
        {
           strcat(pcgUrnoListArr,pclDataBuf);
           strcat(pcgUrnoListArr,",");
           ilI++;
           slFkt = NEXT;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        }
        close_my_cursor(&slCursor);
        if (strlen(pcgUrnoListArr) > 0)
           pcgUrnoListDep[strlen(pcgUrnoListArr)-1] = '\0';
        dbg(DEBUG,"%s Number of BLKTAB Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListArr));
     }
     return ilRC;
  }
  if (strcmp(pcpType,"BASIC") == 0)
  {
     ilI = 0;
     sprintf(pclSqlBuf,"SELECT URNO FROM %s",pcpTable);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListArr) < 119500)
     {
        strcat(pcgUrnoListArr,pclDataBuf);
        strcat(pcgUrnoListArr,",");
        ilI++;
        slFkt = NEXT;
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     }
     close_my_cursor(&slCursor);
     if (strlen(pcgUrnoListArr) > 0)
        pcgUrnoListDep[strlen(pcgUrnoListArr)-1] = '\0';
     dbg(DEBUG,"%s Number of %s Urnos = %d , Len = %d",pclFunc,pcpTable,ilI,strlen(pcgUrnoListArr));
     return ilRC;
  }
  if (strlen(pcpMinArr) > 0)
  {
     ilI = 0;
     if (strcmp(pcpType,"TOW") == 0)
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE STOD BETWEEN '%s' AND '%s' AND FTYP IN ('T','G')",pcpTable,pcpMinArr,pcpMaxArr);
     else
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE STOA BETWEEN '%s' AND '%s' AND ADID = 'A'",pcpTable,pcpMinArr,pcpMaxArr);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListArr) < 119500)
     {
        strcat(pcgUrnoListArr,pclDataBuf);
        strcat(pcgUrnoListArr,",");
        ilI++;
        slFkt = NEXT;
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     }
     close_my_cursor(&slCursor);
/*
     if (strlen(pcgUrnoListArr) > 0)
        pcgUrnoListArr[strlen(pcgUrnoListArr)-1] = '\0';
*/
     if (strcmp(pcpType,"TOW") == 0)
        dbg(DEBUG,"%s Number of Towing (Arr) Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListDep));
     else
        dbg(DEBUG,"%s Number of Arrival Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListArr));
  }
  if (strlen(pcpMinDep) > 0)
  {
     ilI = 0;
     if (strcmp(pcpType,"TOW") == 0)
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE STOD BETWEEN '%s' AND '%s' AND FTYP IN ('T','G')",pcpTable,pcpMinDep,pcpMaxDep);
     else
        sprintf(pclSqlBuf,"SELECT URNO FROM %s WHERE STOD BETWEEN '%s' AND '%s' AND ADID = 'D'",pcpTable,pcpMinDep,pcpMaxDep);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     while (ilRCdb == DB_SUCCESS && strlen(pcgUrnoListDep) < 119500)
     {
        strcat(pcgUrnoListDep,pclDataBuf);
        strcat(pcgUrnoListDep,",");
        ilI++;
        slFkt = NEXT;
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     }
     close_my_cursor(&slCursor);
/*
     if (strlen(pcgUrnoListDep) > 0)
        pcgUrnoListDep[strlen(pcgUrnoListDep)-1] = '\0';
*/
     if (strcmp(pcpType,"TOW") == 0)
        dbg(DEBUG,"%s Number of Towing (Dep) Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListDep));
     else
        dbg(DEBUG,"%s Number of Departure Urnos = %d , Len = %d",pclFunc,ilI,strlen(pcgUrnoListDep));
  }

  return ilRC;
} /* End of GetUrnoList */


static int CheckGMSResLine(char *pcpLine, char *pcpSeperator, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckGMSResLine:";
  int ilCount;
  char pclTmpBuf[1024];
  int ilDay;
  int ilMin;
  int ilWkDay;

  GetDataItem(pcgGMSResRsid,pcpLine,igRsid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRsid);
  GetDataItem(pcgGMSResRaid,pcpLine,igRaid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRaid);
  GetDataItem(pcgGMSResAlcd,pcpLine,igAlcd,*pcpSeperator,""," ");
  TrimRight(pcgGMSResAlcd);
  GetDataItem(pcgGMSResHanm,pcpLine,igHanm,*pcpSeperator,""," ");
  TrimRight(pcgGMSResHanm);
  GetDataItem(pcgGMSResActs,pcpLine,igActs,*pcpSeperator,""," ");
  TrimRight(pcgGMSResActs);
  GetDataItem(pcgGMSResActe,pcpLine,igActe,*pcpSeperator,""," ");
  TrimRight(pcgGMSResActe);
  GetDataItem(pcgGMSResSchs,pcpLine,igSchs,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSchs);
  GetDataItem(pcgGMSResSche,pcpLine,igSche,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSche);
  GetDataItem(pcgGMSResDisp,pcpLine,igDisp,*pcpSeperator,""," ");
  TrimRight(pcgGMSResDisp);
  GetDataItem(pcgGMSResRmrk,pcpLine,igRmrk,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRmrk);
  GetDataItem(pcgGMSResUrno,pcpLine,igUrno,*pcpSeperator,""," ");
  TrimRight(pcgGMSResUrno);
  GetDataItem(pcgGMSResFlno,pcpLine,igFlno,*pcpSeperator,""," ");
  TrimRight(pcgGMSResFlno);
  GetDataItem(pcgGMSResCsgn,pcpLine,igCsgn,*pcpSeperator,""," ");
  TrimRight(pcgGMSResCsgn);
  GetDataItem(pcgGMSResStdt,pcpLine,igStdt,*pcpSeperator,""," ");
  TrimRight(pcgGMSResStdt);
  GetDataItem(pcgGMSResAdid,pcpLine,igAdid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResAdid);

  if (*pcgGMSResRsid != ' ')
  {
     if (strcmp(pcpType,"BB") == 0)
        ilRC = syslibSearchDbData("BLTTAB","BNAM",pcgGMSResRsid,"BNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcpType,"GT") == 0)
        ilRC = syslibSearchDbData("GATTAB","GNAM",pcgGMSResRsid,"GNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcpType,"POS") == 0)
        ilRC = syslibSearchDbData("PSTTAB","PNAM",pcgGMSResRsid,"PNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcpType,"CI") == 0 || strcmp(pcpType,"CCI") == 0)
        ilRC = syslibSearchDbData("CICTAB","CNAM",pcgGMSResRsid,"CNAM",pclTmpBuf,&ilCount,"\n");
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field RSID",pclFunc,pcgGMSResRsid);
        return ilRC;
     }
  }
  if (*pcgGMSResAlcd != ' ')
  {
     if (strlen(pcgGMSResAlcd) == 2)
        ilRC = syslibSearchDbData("ALTTAB","ALC2",pcgGMSResAlcd,"ALC2",pclTmpBuf,&ilCount,"\n");
     else
        ilRC = syslibSearchDbData("ALTTAB","ALC3",pcgGMSResAlcd,"ALC3",pclTmpBuf,&ilCount,"\n");
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ALCD",pclFunc,pcgGMSResAlcd);
        return ilRC;
     }
  }
  if (*pcgGMSResActs != ' ')
  {
     if (strlen(pcgGMSResActs) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResActs) == 12)
           strcat(pcgGMSResActs,"00");
        ilRC = GetFullDay(pcgGMSResActs,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ACTS",pclFunc,pcgGMSResActs);
        return ilRC;
     }
  }
  if (*pcgGMSResActe != ' ')
  {
     if (strlen(pcgGMSResActe) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResActe) == 12)
           strcat(pcgGMSResActe,"00");
        ilRC = GetFullDay(pcgGMSResActe,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ACTE",pclFunc,pcgGMSResActe);
        return ilRC;
     }
  }
  if (*pcgGMSResSchs != ' ')
  {
     if (strlen(pcgGMSResSchs) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSchs) == 12)
           strcat(pcgGMSResSchs,"00");
        ilRC = GetFullDay(pcgGMSResSchs,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHS",pclFunc,pcgGMSResSchs);
        return ilRC;
     }
  }
  if (*pcgGMSResSche != ' ')
  {
     if (strlen(pcgGMSResSche) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSche) == 12)
           strcat(pcgGMSResSche,"00");
        ilRC = GetFullDay(pcgGMSResSche,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHE",pclFunc,pcgGMSResSche);
        return ilRC;
     }
  }
  if (*pcgGMSResFlno != ' ')
  {
     ilRC = CheckFlightNo(pcgGMSResFlno);
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field FLNO",pclFunc,pcgGMSResFlno);
        return ilRC;
     }
  }
  if (*pcgGMSResStdt != ' ')
  {
     if (strlen(pcgGMSResStdt) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResStdt) == 12)
           strcat(pcgGMSResStdt,"00");
        ilRC = GetFullDay(pcgGMSResStdt,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field STDT",pclFunc,pcgGMSResStdt);
        return ilRC;
     }
  }
  if (strcmp(pcpType,"BB") == 0)
  {
     if (strcmp(pcgGMSResAdid,"A") != 0 && strcmp(pcgGMSResAdid," ") != 0)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ADID",pclFunc,pcgGMSResAdid);
        ilRC = RC_FAIL;
        return ilRC;
     }
  }
  else if (strcmp(pcpType,"GT") == 0 || strcmp(pcpType,"POS") == 0)
  {
     if (strcmp(pcgGMSResAdid,"A") != 0 && strcmp(pcgGMSResAdid,"D") != 0)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ADID",pclFunc,pcgGMSResAdid);
        ilRC = RC_FAIL;
        return ilRC;
     }
  }
  else if (strcmp(pcpType,"CI") == 0 || strcmp(pcpType,"CCI") == 0)
  {
     if (strcmp(pcgGMSResAdid,"D") != 0 && strcmp(pcgGMSResAdid," ") != 0)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ADID",pclFunc,pcgGMSResAdid);
        ilRC = RC_FAIL;
        return ilRC;
     }
  }

  return ilRC;
} /* End of CheckGMSResLine */


static int CheckGMSTowLine(char *pcpLine, char *pcpSeperator)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckGMSTowLine:";
  int ilCount;
  char pclTmpBuf[1024];
  int ilDay;
  int ilMin;
  int ilWkDay;

  GetDataItem(pcgGMSResToid,pcpLine,igToid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResToid);
  GetDataItem(pcgGMSResSchs,pcpLine,igSchs,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSchs);
  GetDataItem(pcgGMSResSche,pcpLine,igSche,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSche);
  GetDataItem(pcgGMSResActs,pcpLine,igActs,*pcpSeperator,""," ");
  TrimRight(pcgGMSResActs);
  GetDataItem(pcgGMSResActe,pcpLine,igActe,*pcpSeperator,""," ");
  TrimRight(pcgGMSResActe);
  GetDataItem(pcgGMSResPstd,pcpLine,igPstd,*pcpSeperator,""," ");
  TrimRight(pcgGMSResPstd);
  GetDataItem(pcgGMSResPsta,pcpLine,igPsta,*pcpSeperator,""," ");
  TrimRight(pcgGMSResPsta);
  GetDataItem(pcgGMSResTwtp,pcpLine,igTwtp,*pcpSeperator,""," ");
  TrimRight(pcgGMSResTwtp);
  GetDataItem(pcgGMSResAdid,pcpLine,igAdid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResAdid);
  GetDataItem(pcgGMSResUrno,pcpLine,igUrno,*pcpSeperator,""," ");
  TrimRight(pcgGMSResUrno);
  GetDataItem(pcgGMSResFlno,pcpLine,igFlno,*pcpSeperator,""," ");
  TrimRight(pcgGMSResFlno);
  GetDataItem(pcgGMSResCsgn,pcpLine,igCsgn,*pcpSeperator,""," ");
  TrimRight(pcgGMSResCsgn);
  GetDataItem(pcgGMSResStdt,pcpLine,igStdt,*pcpSeperator,""," ");
  TrimRight(pcgGMSResStdt);

  if (*pcgGMSResSchs != ' ')
  {
     if (strlen(pcgGMSResSchs) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSchs) == 12)
           strcat(pcgGMSResSchs,"00");
        ilRC = GetFullDay(pcgGMSResSchs,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHS",pclFunc,pcgGMSResSchs);
        return ilRC;
     }
  }
  if (*pcgGMSResSche != ' ')
  {
     if (strlen(pcgGMSResSche) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSche) == 12)
           strcat(pcgGMSResSche,"00");
        ilRC = GetFullDay(pcgGMSResSche,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHE",pclFunc,pcgGMSResSche);
        return ilRC;
     }
  }
  if (*pcgGMSResActs != ' ')
  {
     if (strlen(pcgGMSResActs) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResActs) == 12)
           strcat(pcgGMSResActs,"00");
        ilRC = GetFullDay(pcgGMSResActs,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ACTS",pclFunc,pcgGMSResActs);
        return ilRC;
     }
  }
  if (*pcgGMSResActe != ' ')
  {
     if (strlen(pcgGMSResActe) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResActe) == 12)
           strcat(pcgGMSResActe,"00");
        ilRC = GetFullDay(pcgGMSResActe,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field ACTE",pclFunc,pcgGMSResActe);
        return ilRC;
     }
  }
  if (*pcgGMSResPstd != ' ')
  {
     ilRC = syslibSearchDbData("PSTTAB","PNAM",pcgGMSResPstd,"PNAM",pclTmpBuf,&ilCount,"\n");
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field PSTD",pclFunc,pcgGMSResPstd);
        return ilRC;
     }
  }
  if (*pcgGMSResPsta != ' ')
  {
     ilRC = syslibSearchDbData("PSTTAB","PNAM",pcgGMSResPsta,"PNAM",pclTmpBuf,&ilCount,"\n");
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field PSTA",pclFunc,pcgGMSResPsta);
        return ilRC;
     }
  }
  if (strcmp(pcgGMSResTwtp,"T") != 0 && strcmp(pcgGMSResTwtp,"G") != 0)
  {
     dbg(TRACE,"%s Wrong Data <%s> in Field TWTP",pclFunc,pcgGMSResTwtp);
     ilRC = RC_FAIL;
     return ilRC;
  }
  if (strcmp(pcgGMSResAdid,"A") != 0 && strcmp(pcgGMSResAdid,"D") != 0)
  {
     dbg(TRACE,"%s Wrong Data <%s> in Field ADID",pclFunc,pcgGMSResAdid);
     ilRC = RC_FAIL;
     return ilRC;
  }
  if (*pcgGMSResFlno != ' ')
  {
     ilRC = CheckFlightNo(pcgGMSResFlno);
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field FLNO",pclFunc,pcgGMSResFlno);
        return ilRC;
     }
  }
  if (*pcgGMSResStdt != ' ')
  {
     if (strlen(pcgGMSResStdt) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResStdt) == 12)
           strcat(pcgGMSResStdt,"00");
        ilRC = GetFullDay(pcgGMSResStdt,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field STDT",pclFunc,pcgGMSResStdt);
        return ilRC;
     }
  }

  return ilRC;
} /* End of CheckGMSTowLine */


static int CheckGMSNoopLine(char *pcpLine, char *pcpSeperator)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckGMSNoopLine:";
  int ilCount;
  char pclTmpBuf[1024];
  int ilDay;
  int ilMin;
  int ilWkDay;

  GetDataItem(pcgGMSResRsid,pcpLine,igRsid,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRsid);
  GetDataItem(pcgGMSResRsty,pcpLine,igRsty,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRsty);
  GetDataItem(pcgGMSResSchs,pcpLine,igSchs,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSchs);
  GetDataItem(pcgGMSResSche,pcpLine,igSche,*pcpSeperator,""," ");
  TrimRight(pcgGMSResSche);
  GetDataItem(pcgGMSResRmrk,pcpLine,igRmrk,*pcpSeperator,""," ");
  TrimRight(pcgGMSResRmrk);

  if (*pcgGMSResRsid != ' ')
  {
     if (strcmp(pcgGMSResRsty,"BB") == 0)
        ilRC = syslibSearchDbData("BLTTAB","BNAM",pcgGMSResRsid,"BNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcgGMSResRsty,"GT") == 0)
        ilRC = syslibSearchDbData("GATTAB","GNAM",pcgGMSResRsid,"GNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcgGMSResRsty,"POS") == 0)
        ilRC = syslibSearchDbData("PSTTAB","PNAM",pcgGMSResRsid,"PNAM",pclTmpBuf,&ilCount,"\n");
     else if (strcmp(pcgGMSResRsty,"CI") == 0 || strcmp(pcgGMSResRsty,"CCI") == 0)
        ilRC = syslibSearchDbData("CICTAB","CNAM",pcgGMSResRsid,"CNAM",pclTmpBuf,&ilCount,"\n");
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field RSID",pclFunc,pcgGMSResRsid);
        return ilRC;
     }
  }
  if (*pcgGMSResSchs != ' ')
  {
     if (strlen(pcgGMSResSchs) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSchs) == 12)
           strcat(pcgGMSResSchs,"00");
        ilRC = GetFullDay(pcgGMSResSchs,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHS",pclFunc,pcgGMSResSchs);
        return ilRC;
     }
  }
  if (*pcgGMSResSche != ' ')
  {
     if (strlen(pcgGMSResSche) < 9)
        ilRC = RC_FAIL;
     else
     {
        if (strlen(pcgGMSResSche) == 12)
           strcat(pcgGMSResSche,"00");
        ilRC = GetFullDay(pcgGMSResSche,&ilDay,&ilMin,&ilWkDay);
     }
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Wrong Data <%s> in Field SCHE",pclFunc,pcgGMSResSche);
        return ilRC;
     }
  }

  return ilRC;
} /* End of CheckGMSNoopLine */


static int RemoveUrno(char *pcpUrno, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "RemoveUrno:";
  char *pclTmpPtr;

  if (*pcpType == 'A')
  {
     pclTmpPtr = strstr(pcgUrnoListArr,pcpUrno);
     if (pclTmpPtr != NULL)
     {
        while (*pclTmpPtr != ',' && *pclTmpPtr != '\0')
        {
           *pclTmpPtr = ' ';
           pclTmpPtr++;
        }
     }
  }
  else
  {
     pclTmpPtr = strstr(pcgUrnoListDep,pcpUrno);
     if (pclTmpPtr != NULL)
     {
        while (*pclTmpPtr != ',' && *pclTmpPtr != '\0')
        {
           *pclTmpPtr = ' ';
           pclTmpPtr++;
        }
     }
  }

  return ilRC;
} /* End of RemoveUrno */


static int HandleGMSResource(char *pcpUrno, char *pcpType, char *pcpLine)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSResource:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclFieldListUpd[1024];
  char pclFieldListUpd2[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  char pclRsi1[16] = " ";
  char pclStb1[16] = " ";
  char pclSte1[16] = " ";
  char pclAtb1[16] = " ";
  char pclAte1[16] = " ";
  char pclRsi2[16] = " ";
  char pclStb2[16] = " ";
  char pclSte2[16] = " ";
  char pclAtb2[16] = " ";
  char pclAte2[16] = " ";
  int ilNoEle;
  char pclCommand[16];
  char pclTable[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  char pclRarUrno[16];
  char pclRarType[16];
  char pclRarIfna[32];
  char pclRarRurn[16];
  char pclRarRnam[16];

  if (strcmp(pcpType,"BB") == 0)
  {
     strcpy(pclFieldList,"URNO,BLT1,B1BS,B1ES,B1BA,B1EA,BLT2,B2BS,B2ES,B2BA,B2EA");
     strcpy(pclFieldListUpd,"BLT1,B1BS,B1ES,B1BA,B1EA,BLT2,B2BS,B2ES,B2BA,B2EA");
     strcpy(pclFieldListUpd2,"BLT2,B2BS,B2ES,B2BA,B2EA");
     ilNoEle = 11;
  }
  else if (strcmp(pcpType,"GT") == 0)
  {
     if (*pcgGMSResAdid == 'A')
     {
        strcpy(pclFieldList,"URNO,GTA1,GA1B,GA1E,GA1X,GA1Y,GTA2,GA2B,GA2E,GA2X,GA2Y");
        strcpy(pclFieldListUpd,"GTA1,GA1B,GA1E,GA1X,GA1Y,GTA2,GA2B,GA2E,GA2X,GA2Y");
        strcpy(pclFieldListUpd2,"GTA2,GA2B,GA2E,GA2X,GA2Y");
     }
     else
     {
        strcpy(pclFieldList,"URNO,GTD1,GD1B,GD1E,GD1X,GD1Y,GTD2,GD2B,GD2E,GD2X,GD2Y");
        strcpy(pclFieldListUpd,"GTD1,GD1B,GD1E,GD1X,GD1Y,GTD2,GD2B,GD2E,GD2X,GD2Y");
        strcpy(pclFieldListUpd2,"GTD2,GD2B,GD2E,GD2X,GD2Y");
     }
     ilNoEle = 11;
  }
  else if (strcmp(pcpType,"POS") == 0)
  {
     if (*pcgGMSResAdid == 'A')
     {
        strcpy(pclFieldList,"URNO,PSTA,PABS,PAES,PABA,PAEA");
        strcpy(pclFieldListUpd,"PSTA,PABS,PAES,PABA,PAEA");
     }
     else
     {
        strcpy(pclFieldList,"URNO,PSTD,PDBS,PDES,PDBA,PDEA");
        strcpy(pclFieldListUpd,"PSTD,PDBS,PDES,PDBA,PDEA");
     }
     ilNoEle = 11;
  }
  else
  {
     dbg(TRACE,"%s Invalid Type <%s> defined",pclFunc,pcpType);
     return RC_FAIL;
  }
  if (*pcgGMSResUrno != ' ')
     sprintf(pclSelection,"WHERE URNO = %s",pcgGMSResUrno);
  else
  {
     if (strcmp(pcpType,"BB") == 0)
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s' AND DES3 = '%s'",
                pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
     else if (strcmp(pcpType,"GT") == 0 || strcmp(pcpType,"POS") == 0)
     {
        if (*pcgGMSResAdid == 'A')
           sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s' AND DES3 = '%s'",
                   pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
        else
           sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s' AND ORG3 = '%s'",
                   pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
     }
  }
  sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     dbg(TRACE,"%s Record not found, Line = <%s>",pclFunc,pcpLine);
     return RC_FAIL;
  }
  BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
  get_real_item(pcpUrno,pclDataBuf,1);
  TrimRight(pcpUrno);
  get_real_item(pclRsi1,pclDataBuf,2);
  TrimRight(pclRsi1);
  get_real_item(pclStb1,pclDataBuf,3);
  TrimRight(pclStb1);
  get_real_item(pclSte1,pclDataBuf,4);
  TrimRight(pclSte1);
  get_real_item(pclAtb1,pclDataBuf,5);
  TrimRight(pclAtb1);
  get_real_item(pclAte1,pclDataBuf,6);
  TrimRight(pclAte1);
  if (ilNoEle > 6)
  {
     get_real_item(pclRsi2,pclDataBuf,7);
     TrimRight(pclRsi2);
     get_real_item(pclStb2,pclDataBuf,8);
     TrimRight(pclStb2);
     get_real_item(pclSte2,pclDataBuf,9);
     TrimRight(pclSte2);
     get_real_item(pclAtb2,pclDataBuf,10);
     TrimRight(pclAtb2);
     get_real_item(pclAte2,pclDataBuf,11);
     TrimRight(pclAte2);
  }
  dbg(DEBUG,"%s Found Record with URNO <%s>",pclFunc,pcpUrno);
  if (strcmp(pcpType,"POS") == 0)
  {
     sprintf(pclDataList,"%s,%s,%s,%s,%s",pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe);
  }
  else
  {
     if (strcmp(pcpType,"BB") == 0 ||
         (strcmp(pcpType,"GT") == 0 && *pcgGMSResAdid == 'A'))
     {
        if (strstr(pcgUrnoListArr,pcpUrno) != NULL)
        {
           sprintf(pclDataList,"%s,%s,%s,%s,%s, , , , , ",pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe);
        }
        else
        {
           strcpy(pclFieldListUpd,pclFieldListUpd2);
           sprintf(pclDataList,"%s,%s,%s,%s,%s",pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe);
        }
     }
     else
     {
        if (strstr(pcgUrnoListDep,pcpUrno) != NULL)
        {
           sprintf(pclDataList,"%s,%s,%s,%s,%s, , , , , ",pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe);
        }
        else
        {
           strcpy(pclFieldListUpd,pclFieldListUpd2);
           sprintf(pclDataList,"%s,%s,%s,%s,%s",pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe);
        }
     }
  }
  sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"UFR");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTable,"AFTTAB");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"7800");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
  dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
  dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldListUpd);
  dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
  ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                       pclCommand,pclTable,pclSelection,pclFieldListUpd,pclDataList,"",ilPriority,NETOUT_NO_ACK);

  if (*pcgGMSResRaid != '\0' && *pcgGMSResRaid != ' ')
  {
     if (strcmp(pcpType,"BB") == 0)
        strcpy(pclRarType,"BB");
     else if (strcmp(pcpType,"GT") == 0)
     {
        if (*pcgGMSResAdid == 'A')
           strcpy(pclRarType,"GA");
        else
           strcpy(pclRarType,"GD");
     }
     else if (strcmp(pcpType,"POS") == 0)
     {
        if (*pcgGMSResAdid == 'A')
           strcpy(pclRarType,"PA");
        else
           strcpy(pclRarType,"PD");
     }
     strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN");
     sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = '%s' AND STAT <> 'DELETED'",
             pcgGMSResRaid,pclRarType);
     sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",6,",");
        get_real_item(pclRarUrno,pclDataBuf,1);
        TrimRight(pclRarUrno);
        get_real_item(pclRarRnam,pclDataBuf,3);
        TrimRight(pclRarRnam);
        get_real_item(pclRarIfna,pclDataBuf,4);
        TrimRight(pclRarIfna);
        get_real_item(pclRarRurn,pclDataBuf,6);
        TrimRight(pclRarRurn);
        if (strcmp(pclRarRnam,pcgGMSResRsid) != 0 || strcmp(pclRarIfna,"GMS") != 0 || strcmp(pclRarRurn,pcpUrno) != 0)
        {
           sprintf(pclDataList,"RNAM = '%s',IFNA = 'GMS',RURN = %s",pcgGMSResRsid,pcpUrno);
           sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor);
        }
     }
     else
     {
        strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','GMS','%s',%s,'%s',' ',' '",
                igLastUrno,pcgGMSResRaid,pcgGMSResRsid,pclRarType,pcpUrno,pcgHomeAp);
        sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
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
     }
  }

  return ilRC;
} /* End of HandleGMSResource */


static int HandleGMSResourceDcnt(char *pcpUrno, char *pcpType, char *pcpLine)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSResourceDcnt:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclFieldListUpd[1024];
  char pclFieldListUpd2[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  char pclActDataList[1024];
  char pclDataListOld[1024];
  char pclUrno[50][16];
  char pclCkic[50][16];
  char pclCtyp[50][16];
  char pclCkit[50][16];
  char pclCkbs[50][16];
  char pclCkes[50][16];
  char pclCkba[50][16];
  char pclCkea[50][16];
  char pclDisp[50][64];
  char pclRema[50][64];
  int ilNoEle;
  char pclFlnu[16];
  int ilRecCnt = 0;
  int ilI;
  int ilIdx;
  int ilFound;
  char pclCcaUrno[16];
  char pclRarUrno[16];
  char pclRarIfna[32];
  char pclRarRurn[16];
  char pclRarRnam[16];

  if (*pcgGMSResUrno != ' ')
     strcpy(pclFlnu,pcgGMSResUrno);
  else
  {
     sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s' AND ORG3 = '%s'",
             pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
     sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclFlnu);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s Record not found, Line = <%s>",pclFunc,pcpLine);
        return RC_FAIL;
     }
  }
  TrimRight(pclFlnu);
  ilRecCnt = 0;
  strcpy(pclFieldList,"URNO,CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
  strcpy(pclFieldListUpd,"CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
  ilNoEle = 10;
  sprintf(pclSelection,"WHERE FLNU = %s",pclFlnu);
  sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  while (ilRCdb == DB_SUCCESS)
  {
     BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
     get_real_item(&pclUrno[ilRecCnt][0],pclDataBuf,1);
     TrimRight(&pclUrno[ilRecCnt][0]);
     get_real_item(&pclCkic[ilRecCnt][0],pclDataBuf,2);
     TrimRight(&pclCkic[ilRecCnt][0]);
     get_real_item(&pclCtyp[ilRecCnt][0],pclDataBuf,3);
     TrimRight(&pclCtyp[ilRecCnt][0]);
     get_real_item(&pclCkit[ilRecCnt][0],pclDataBuf,4);
     TrimRight(&pclCkit[ilRecCnt][0]);
     get_real_item(&pclCkbs[ilRecCnt][0],pclDataBuf,5);
     TrimRight(&pclCkbs[ilRecCnt][0]);
     get_real_item(&pclCkes[ilRecCnt][0],pclDataBuf,6);
     TrimRight(&pclCkes[ilRecCnt][0]);
     get_real_item(&pclCkba[ilRecCnt][0],pclDataBuf,7);
     TrimRight(&pclCkba[ilRecCnt][0]);
     get_real_item(&pclCkea[ilRecCnt][0],pclDataBuf,8);
     TrimRight(&pclCkea[ilRecCnt][0]);
     get_real_item(&pclDisp[ilRecCnt][0],pclDataBuf,9);
     TrimRight(&pclDisp[ilRecCnt][0]);
     get_real_item(&pclRema[ilRecCnt][0],pclDataBuf,10);
     TrimRight(&pclRema[ilRecCnt][0]);
     ilRecCnt++;
     slFkt = NEXT;
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  }
  close_my_cursor(&slCursor);
  ilFound = FALSE;
  for (ilI = 0; ilI < ilRecCnt && ilFound == FALSE; ilI++)
  {
     if (strcmp(pcgGMSResRsid,&pclCkic[ilI][0]) == 0)
     {
        strcpy(pclCcaUrno,&pclUrno[ilI][0]);
        ilIdx = ilI;
        ilFound = TRUE;
     }
  }
  if (ilFound == TRUE)
  {
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     strcpy(pclDataListOld,"");
     if (strcmp(pcgGMSResSchs,&pclCkbs[ilIdx][0]) != 0)
        strcat(pclFieldList,"CKBS,");
     strcat(pclDataListOld,&pclCkbs[ilIdx][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResSche,&pclCkes[ilIdx][0]) != 0)
        strcat(pclFieldList,"CKES,");
     strcat(pclDataListOld,&pclCkes[ilIdx][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResActs,&pclCkba[ilIdx][0]) != 0)
        strcat(pclFieldList,"CKBA,");
     strcat(pclDataListOld,&pclCkba[ilIdx][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResActe,&pclCkea[ilIdx][0]) != 0)
        strcat(pclFieldList,"CKEA,");
     strcat(pclDataListOld,&pclCkea[ilIdx][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResDisp,&pclDisp[ilIdx][0]) != 0)
        strcat(pclFieldList,"DISP,");
     strcat(pclDataListOld,&pclDisp[ilIdx][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResRmrk,&pclRema[ilIdx][0]) != 0)
        strcat(pclFieldList,"REMA,");
     strcat(pclDataListOld,&pclRema[ilIdx][0]);
     strcat(pclDataListOld,",");
     strcat(pclDataListOld,&pclUrno[ilIdx][0]);
     if (strlen(pclFieldList) > 0)
     {
        sprintf(pclFieldList,"CKBS='%s',CKES='%s',CKBA='%s',CKEA='%s',DISP='%s',REMA='%s'",
                pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResDisp,pcgGMSResRmrk);
        sprintf(pclSelection,"WHERE URNO = %s",&pclUrno[ilIdx][0]);
        sprintf(pclSqlBuf,"UPDATE CCATAB SET %s %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error updating CCATAB",pclFunc);
           close_my_cursor(&slCursor);
           return ilRC;
        }
        close_my_cursor(&slCursor);
        strcpy(pclFieldList,"CKBS,CKES,CKBA,CKEA,DISP,REMA,URNO");
        sprintf(pclDataList,"%s,%s,%s,%s,%s,%s,%s",
                pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResDisp,pcgGMSResRmrk,&pclUrno[ilIdx][0]);
        strcat(pclDataList,"\n");
        strcat(pclDataList,pclDataListOld);
        ilRC = TriggerBchdlAction("URT","CCATAB",pclSelection,pclFieldList,pclDataList,TRUE,TRUE);
     }
     if (ilRC == RC_SUCCESS)
        ilRC = RemoveUrno(&pclUrno[ilIdx][0],"D");
  }
  else
  {
     ilRC = GetNextUrno();
     sprintf(pclCcaUrno,"%d",igLastUrno);
     strcpy(pclFieldList,"URNO,CKIC,CTYP,CKBS,CKES,CKBA,CKEA,REMA,DISP,FLNU,HOPO");
     sprintf(pclDataList,"%s,'%s',' ','%s','%s','%s','%s','%s','%s',%s,'%s'",
             pclCcaUrno,pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,
             pcgGMSResActe,pcgGMSResRmrk,pcgGMSResDisp,pclFlnu,pcgHomeAp);
     sprintf(pclSqlBuf,"INSERT INTO CCATAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     if (ilRCdb == DB_SUCCESS)
        commit_work();
     else
     {
        dbg(TRACE,"%s Error inserting into CCATAB",pclFunc);
        close_my_cursor(&slCursor);
        return ilRC;
     }
     close_my_cursor(&slCursor);
     strcpy(pclActDataList,"");
     for (ilI = 0; ilI < strlen(pclDataList); ilI++)
        if (strncmp(&pclDataList[ilI],"'",1) != 0)
           strncat(pclActDataList,&pclDataList[ilI],1);
     ilRC = TriggerBchdlAction("IRT","CCATAB",pclCcaUrno,pclFieldList,pclActDataList,TRUE,TRUE);
  }

  if (*pcgGMSResRaid != '\0' && *pcgGMSResRaid != ' ')
  {
     strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN");
     sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = 'DC' AND STAT <> 'DELETED'",pcgGMSResRaid);
     sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",6,",");
        get_real_item(pclRarUrno,pclDataBuf,1);
        TrimRight(pclRarUrno);
        get_real_item(pclRarRnam,pclDataBuf,3);
        TrimRight(pclRarRnam);
        get_real_item(pclRarIfna,pclDataBuf,4);
        TrimRight(pclRarIfna);
        get_real_item(pclRarRurn,pclDataBuf,6);
        TrimRight(pclRarRurn);
        if (strcmp(pclRarRnam,pcgGMSResRsid) != 0 || strcmp(pclRarIfna,"GMS") != 0 || strcmp(pclRarRurn,pclCcaUrno) != 0)
        {
           sprintf(pclDataList,"RNAM = '%s',IFNA = 'GMS',RURN = %s",pcgGMSResRsid,pclCcaUrno);
           sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor);
        }
     }
     else
     {
        strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','GMS','DC',%s,'%s',' ',' '",
                igLastUrno,pcgGMSResRaid,pcgGMSResRsid,pclCcaUrno,pcgHomeAp);
        sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
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
     }
  }

  return ilRC;
} /* End of HandleGMSResourceDcnt */


static int HandleGMSResourceCcnt(char *pcpUrno, char *pcpType, char *pcpLine)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSResourceCcnt:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclFieldListUpd[1024];
  char pclFieldListUpd2[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  char pclActDataList[1024];
  char pclDataListOld[1024];
  char pclUrno[100][16];
  char pclCkic[100][16];
  char pclCtyp[100][16];
  char pclCkit[100][16];
  char pclCkbs[100][16];
  char pclCkes[100][16];
  char pclCkba[100][16];
  char pclCkea[100][16];
  char pclDisp[100][64];
  char pclRema[100][64];
  int ilNoEle;
  char pclFlnu[16];
  int ilRecCnt = 0;
  int ilI;
  int ilIdx;
  int ilFound;
  char pclCcaUrno[16];
  int ilCount;
  char pclRarUrno[16];
  char pclRarIfna[32];
  char pclRarRurn[16];
  char pclRarRnam[16];

  if (strlen(pcgGMSResAlcd) == 2)
     ilRC = syslibSearchDbData("ALTTAB","ALC2",pcgGMSResAlcd,"URNO",pclFlnu,&ilCount,"\n");
  else
     ilRC = syslibSearchDbData("ALTTAB","ALC3",pcgGMSResAlcd,"URNO",pclFlnu,&ilCount,"\n");
  ilRecCnt = 0;
  strcpy(pclFieldList,"URNO,CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
  strcpy(pclFieldListUpd,"CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA");
  ilNoEle = 10;
  ilFound = FALSE;
  sprintf(pclSelection,"WHERE FLNU = %s AND CKIC = '%s' AND CKBS = '%s' AND CTYP = 'C'",pclFlnu,pcgGMSResRsid,pcgGMSResSchs);
  sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  if (ilRCdb == DB_SUCCESS)
  {
     BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
     get_real_item(&pclUrno[ilRecCnt][0],pclDataBuf,1);
     TrimRight(&pclUrno[ilRecCnt][0]);
     strcpy(pclCcaUrno,&pclUrno[ilRecCnt][0]);
     get_real_item(&pclCkic[ilRecCnt][0],pclDataBuf,2);
     TrimRight(&pclCkic[ilRecCnt][0]);
     get_real_item(&pclCtyp[ilRecCnt][0],pclDataBuf,3);
     TrimRight(&pclCtyp[ilRecCnt][0]);
     get_real_item(&pclCkit[ilRecCnt][0],pclDataBuf,4);
     TrimRight(&pclCkit[ilRecCnt][0]);
     get_real_item(&pclCkbs[ilRecCnt][0],pclDataBuf,5);
     TrimRight(&pclCkbs[ilRecCnt][0]);
     get_real_item(&pclCkes[ilRecCnt][0],pclDataBuf,6);
     TrimRight(&pclCkes[ilRecCnt][0]);
     get_real_item(&pclCkba[ilRecCnt][0],pclDataBuf,7);
     TrimRight(&pclCkba[ilRecCnt][0]);
     get_real_item(&pclCkea[ilRecCnt][0],pclDataBuf,8);
     TrimRight(&pclCkea[ilRecCnt][0]);
     get_real_item(&pclDisp[ilRecCnt][0],pclDataBuf,9);
     TrimRight(&pclDisp[ilRecCnt][0]);
     get_real_item(&pclRema[ilRecCnt][0],pclDataBuf,10);
     TrimRight(&pclRema[ilRecCnt][0]);
     ilRecCnt++;
     ilFound = TRUE;
  }
  close_my_cursor(&slCursor);
  if (ilFound == TRUE)
  {
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     strcpy(pclDataListOld,"");
     if (strcmp(pcgGMSResSchs,&pclCkbs[0][0]) != 0)
        strcat(pclFieldList,"CKBS,");
     strcat(pclDataListOld,&pclCkbs[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResSche,&pclCkes[0][0]) != 0)
        strcat(pclFieldList,"CKES,");
     strcat(pclDataListOld,&pclCkes[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResActs,&pclCkba[0][0]) != 0)
        strcat(pclFieldList,"CKBA,");
     strcat(pclDataListOld,&pclCkba[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResActe,&pclCkea[0][0]) != 0)
        strcat(pclFieldList,"CKEA,");
     strcat(pclDataListOld,&pclCkea[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResDisp,&pclDisp[0][0]) != 0)
        strcat(pclFieldList,"DISP,");
     strcat(pclDataListOld,&pclDisp[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResRmrk,&pclRema[0][0]) != 0)
        strcat(pclFieldList,"REMA,");
     strcat(pclDataListOld,&pclRema[0][0]);
     strcat(pclDataListOld,",");
     strcat(pclDataListOld,&pclUrno[0][0]);
     if (strlen(pclFieldList) > 0)
     {
        sprintf(pclFieldList,"CKBS='%s',CKES='%s',CKBA='%s',CKEA='%s',DISP='%s',REMA='%s'",
                pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResDisp,pcgGMSResRmrk);
        sprintf(pclSelection,"WHERE URNO = %s",&pclUrno[0][0]);
        sprintf(pclSqlBuf,"UPDATE CCATAB SET %s %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error updating CCATAB",pclFunc);
           close_my_cursor(&slCursor);
           return ilRC;
        }
        close_my_cursor(&slCursor);
        strcpy(pclFieldList,"CKBS,CKES,CKBA,CKEA,DISP,REMA,URNO");
        sprintf(pclDataList,"%s,%s,%s,%s,%s,%s,%s",
                pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResDisp,pcgGMSResRmrk,&pclUrno[0][0]);
        strcat(pclDataList,"\n");
        strcat(pclDataList,pclDataListOld);
        ilRC = TriggerBchdlAction("URT","CCATAB",pclSelection,pclFieldList,pclDataList,TRUE,TRUE);
     }
     if (ilRC == RC_SUCCESS)
        ilRC = RemoveUrno(&pclUrno[0][0],"D");
  }
  else
  {
     ilRC = GetNextUrno();
     sprintf(pclCcaUrno,"%d",igLastUrno);
     strcpy(pclFieldList,"URNO,CKIC,CTYP,CKBS,CKES,CKBA,CKEA,REMA,DISP,FLNU,HOPO");
     sprintf(pclDataList,"%s,'%s','C','%s','%s','%s','%s','%s','%s',%s,'%s'",
             pclCcaUrno,pcgGMSResRsid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,
             pcgGMSResActe,pcgGMSResRmrk,pcgGMSResDisp,pclFlnu,pcgHomeAp);
     sprintf(pclSqlBuf,"INSERT INTO CCATAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     if (ilRCdb == DB_SUCCESS)
        commit_work();
     else
     {
        dbg(TRACE,"%s Error inserting into CCATAB",pclFunc);
        close_my_cursor(&slCursor);
        return ilRC;
     }
     close_my_cursor(&slCursor);
     strcpy(pclActDataList,"");
     for (ilI = 0; ilI < strlen(pclDataList); ilI++)
        if (strncmp(&pclDataList[ilI],"'",1) != 0)
           strncat(pclActDataList,&pclDataList[ilI],1);
     ilRC = TriggerBchdlAction("IRT","CCATAB",pclCcaUrno,pclFieldList,pclActDataList,TRUE,TRUE);
  }

  if (*pcgGMSResRaid != '\0' && *pcgGMSResRaid != ' ')
  {
     strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN");
     sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = 'CC' AND STAT <> 'DELETED'",pcgGMSResRaid);
     sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",6,",");
        get_real_item(pclRarUrno,pclDataBuf,1);
        TrimRight(pclRarUrno);
        get_real_item(pclRarRnam,pclDataBuf,3);
        TrimRight(pclRarRnam);
        get_real_item(pclRarIfna,pclDataBuf,4);
        TrimRight(pclRarIfna);
        get_real_item(pclRarRurn,pclDataBuf,6);
        TrimRight(pclRarRurn);
        if (strcmp(pclRarRnam,pcgGMSResRsid) != 0 || strcmp(pclRarIfna,"GMS") != 0 || strcmp(pclRarRurn,pclCcaUrno) != 0)
        {
           sprintf(pclDataList,"RNAM = '%s',IFNA = 'GMS',RURN = %s",pcgGMSResRsid,pclCcaUrno);
           sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor);
        }
     }
     else
     {
        strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','GMS','CC',%s,'%s',' ',' '",
                igLastUrno,pcgGMSResRaid,pcgGMSResRsid,pclCcaUrno,pcgHomeAp);
        sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
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
     }
  }

  return ilRC;
} /* End of HandleGMSResourceCcnt */


static int HandleGMSResTow(char *pcpUrno, char *pcpLine)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSResTow:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclFieldListSel[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  char pclDataListSel[1024];
  char pclFlnu[16];
  char pclUrno[50][16];
  char pclStod[50][16];
  char pclStoa[50][16];
  char pclOfbl[50][16];
  char pclOnbl[50][16];
  char pclPsta[50][16];
  char pclPstd[50][16];
  char pclFtyp[50][16];
  char pclTowUrno[16];
  int ilNoEle;
  int ilRecCnt = 0;
  int ilI;
  int ilIdx;
  int ilFound;
  char pclDate[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  char pclRarUrno[16];
  char pclRarIfna[32];
  char pclRarRurn[16];
  char pclRarRnam[16];

  if (*pcgGMSResUrno != ' ')
     strcpy(pclFlnu,pcgGMSResUrno);
  else
  {
     if (*pcgGMSResAdid == 'A')
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s' AND DES3 = '%s'",
                pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
     else
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s' AND ORG3 = '%s'",
                pcgGMSResFlno,pcgGMSResStdt,pcgHomeAp);
     sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclFlnu);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s Record not found, Line = <%s>",pclFunc,pcpLine);
        return RC_FAIL;
     }
  }
  TrimRight(pclFlnu);
  ilRecCnt = 0;
  strcpy(pclFieldList,"URNO,STOD,STOA,OFBL,ONBL,PSTD,PSTA,FTYP");
  ilNoEle = 8;
  sprintf(pclSelection,"WHERE AURN = '%s' AND FTYP IN ('T','G')",pclFlnu);
  sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  while (ilRCdb == DB_SUCCESS)
  {
     BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
     get_real_item(&pclUrno[ilRecCnt][0],pclDataBuf,1);
     TrimRight(&pclUrno[ilRecCnt][0]);
     get_real_item(&pclStod[ilRecCnt][0],pclDataBuf,2);
     TrimRight(&pclStod[ilRecCnt][0]);
     get_real_item(&pclStoa[ilRecCnt][0],pclDataBuf,3);
     TrimRight(&pclStoa[ilRecCnt][0]);
     get_real_item(&pclOfbl[ilRecCnt][0],pclDataBuf,4);
     TrimRight(&pclOfbl[ilRecCnt][0]);
     get_real_item(&pclOnbl[ilRecCnt][0],pclDataBuf,5);
     TrimRight(&pclOnbl[ilRecCnt][0]);
     get_real_item(&pclPstd[ilRecCnt][0],pclDataBuf,6);
     TrimRight(&pclPstd[ilRecCnt][0]);
     get_real_item(&pclPsta[ilRecCnt][0],pclDataBuf,7);
     TrimRight(&pclPsta[ilRecCnt][0]);
     get_real_item(&pclFtyp[ilRecCnt][0],pclDataBuf,8);
     TrimRight(&pclFtyp[ilRecCnt][0]);
     ilRecCnt++;
     slFkt = NEXT;
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  }
  close_my_cursor(&slCursor);
  ilFound = FALSE;
  for (ilI = 0; ilI < ilRecCnt && ilFound == FALSE; ilI++)
  {
     if (strcmp(pcgGMSResSchs,&pclStod[ilI][0]) == 0 && strcmp(pcgGMSResSche,&pclStoa[ilI][0]) == 0)
     {
        strcpy(pclTowUrno,&pclUrno[ilI][0]);
        ilIdx = ilI;
        ilFound = TRUE;
     }
  }
  if (ilFound == TRUE)
  {
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     if (strcmp(pcgGMSResSchs,&pclStod[ilIdx][0]) != 0)
        strcat(pclFieldList,"STOD,");
     if (strcmp(pcgGMSResSche,&pclStoa[ilIdx][0]) != 0)
        strcat(pclFieldList,"STOA,");
     if (strcmp(pcgGMSResActs,&pclOfbl[ilIdx][0]) != 0)
        strcat(pclFieldList,"OFBL,");
     if (strcmp(pcgGMSResActe,&pclOnbl[ilIdx][0]) != 0)
        strcat(pclFieldList,"ONBL,");
     if (strcmp(pcgGMSResPstd,&pclPstd[ilIdx][0]) != 0)
        strcat(pclFieldList,"PSTD,");
     if (strcmp(pcgGMSResPsta,&pclPsta[ilIdx][0]) != 0)
        strcat(pclFieldList,"PSTA,");
     if (strcmp(pcgGMSResTwtp,&pclFtyp[ilIdx][0]) != 0)
        strcat(pclFieldList,"FTYP,");
     if (strlen(pclFieldList) > 0)
     {
        strcpy(pclFieldList,"STOD,STOA,OFBD,ONBD,PSTD,PSTA,FTYP");
        sprintf(pclDataList,"%s,%s,%s,%s,%s,%s,%s",
                pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResPstd,pcgGMSResPsta,pcgGMSResTwtp);
        sprintf(pclSelection,"WHERE URNO = %s",pclTowUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","snd_cmd_U",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"UFR");
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7800");
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
        ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,"AFTTAB",pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
     }
     if (ilRC == RC_SUCCESS)
        if (*pcgGMSResAdid == 'A')
           ilRC = RemoveUrno(pclTowUrno,"A");
        else
           ilRC = RemoveUrno(pclTowUrno,"D");
  }
  else
  {
     strcpy(pclFieldListSel,"RKEY,FLNO,ALC2,ALC3,FLTN,FLNS");
     sprintf(pclSelection,"WHERE URNO = %s",pclFlnu);
     sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldListSel,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataListSel);
     close_my_cursor(&slCursor);
     if (ilRCdb != DB_SUCCESS)
     {
        dbg(TRACE,"%s Record with URNO = <%s> not found",pclFunc,pclFlnu);
        ilRC = RC_FAIL;
        return ilRC;
     }
     BuildItemBuffer(pclDataListSel,"",6,",");
     ilRC = GetNextUrno();
     sprintf(pclTowUrno,"%d",igLastUrno);
     strcpy(pclFieldList,"URNO,STOD,STOA,OFBD,ONBD,PSTD,PSTA,FTYP,AURN,CSGN,ADID,DES3,ORG3,");
     sprintf(pclDataList,"%s,%s,%s,%s,%s,%s,%s,%s,%s,TOWING,B,%s,%s,",
             pclTowUrno,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,pcgGMSResActe,pcgGMSResPstd,pcgGMSResPsta,
             pcgGMSResTwtp,pclFlnu,pcgHomeAp,pcgHomeAp);
     strcat(pclFieldList,pclFieldListSel);
     strcat(pclDataList,pclDataListSel);
     strcpy(pclSelection,"");
     strcpy(pclDate,pcgGMSResSchs);
     pclDate[8] = '\0';
     sprintf(pclSelection,"%s,%s,1234567,1\n%s",pclDate,pclDate,pclFlnu);
     ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","snd_cmd_I",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"IFR");
     ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7800");
     ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,"AFTTAB",pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
     strcpy(pclCommand,"JOF");
     sprintf(pclSelection,"%s,,%s",pclFlnu,pclTowUrno);
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,"AFTTAB",pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
  }

  if (*pcgGMSResToid != '\0' && *pcgGMSResToid != ' ')
  {
     strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN");
     sprintf(pclSelection,"WHERE RAID = '%s' AND TYPE = 'TOW' AND STAT <> 'DELETED'",pcgGMSResToid);
     sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",6,",");
        get_real_item(pclRarUrno,pclDataBuf,1);
        TrimRight(pclRarUrno);
        get_real_item(pclRarRnam,pclDataBuf,3);
        TrimRight(pclRarRnam);
        get_real_item(pclRarIfna,pclDataBuf,4);
        TrimRight(pclRarIfna);
        get_real_item(pclRarRurn,pclDataBuf,6);
        TrimRight(pclRarRurn);
        if (strcmp(pclRarRnam,pclFlnu) != 0 || strcmp(pclRarIfna,"GMS") != 0 || strcmp(pclRarRurn,pclTowUrno) != 0)
        {
           sprintf(pclDataList,"RNAM = '%s',IFNA = 'GMS',RURN = %s",pclFlnu,pclTowUrno);
           sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor);
        }
     }
     else
     {
        strcpy(pclFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','GMS','TOW',%s,'%s',' ',' '",
                igLastUrno,pcgGMSResToid,pclFlnu,pclTowUrno,pcgHomeAp);
        sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
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
     }
  }

  return ilRC;
} /* End of HandleGMSResTow */


static int HandleGMSResNoop(char *pcpUrno, char *pcpLine)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSResNoop:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclFieldListUpd[1024];
  char pclFieldListUpd2[1024];
  char pclSelection[1024];
  char pclLocUrno[16];
  char pclType[16];
  int ilCount;
  int ilNoEle;
  int ilRecCnt = 0;
  char pclUrno[10][16];
  char pclBurn[10][16];
  char pclNafr[10][16];
  char pclNato[10][16];
  char pclResn[10][64];
  char pclBlkUrno[16];
  int ilFound;
  char pclDataList[1024];
  char pclActDataList[1024];
  char pclDataListOld[1024];
  int ilI;

  if (strcmp(pcgGMSResRsty,"BB") == 0)
  {
     ilRC = syslibSearchDbData("BLTTAB","BNAM",pcgGMSResRsid,"URNO",pclLocUrno,&ilCount,"\n");
     strcpy(pclType,"BLT");
  }
  else if (strcmp(pcgGMSResRsty,"GT") == 0)
  {
     ilRC = syslibSearchDbData("GATTAB","GNAM",pcgGMSResRsid,"URNO",pclLocUrno,&ilCount,"\n");
     strcpy(pclType,"GAT");
  }
  else if (strcmp(pcgGMSResRsty,"POS") == 0)
  {
     ilRC = syslibSearchDbData("PSTTAB","PNAM",pcgGMSResRsid,"URNO",pclLocUrno,&ilCount,"\n");
     strcpy(pclType,"PST");
  }
  else if (strcmp(pcgGMSResRsty,"CI") == 0 || strcmp(pcgGMSResRsty,"CCI") == 0)
  {
     ilRC = syslibSearchDbData("CICTAB","CNAM",pcgGMSResRsid,"URNO",pclLocUrno,&ilCount,"\n");
     strcpy(pclType,"CIC");
  }
  ilRecCnt = 0;
  strcpy(pclFieldList,"URNO,BURN,NAFR,NATO,RESN");
  strcpy(pclFieldListUpd,"BURN,NAFR,NATO,RESN");
  ilNoEle = 5;
  ilFound = FALSE;
  sprintf(pclSelection,"WHERE BURN = %s AND NAFR = '%s' AND TABN = '%s'",pclLocUrno,pcgGMSResSchs,pclType);
  sprintf(pclSqlBuf,"SELECT %s FROM BLKTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  if (ilRCdb == DB_SUCCESS)
  {
     BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
     get_real_item(&pclUrno[ilRecCnt][0],pclDataBuf,1);
     TrimRight(&pclUrno[ilRecCnt][0]);
     strcpy(pclBlkUrno,&pclUrno[ilRecCnt][0]);
     strcpy(pcpUrno,pclBlkUrno);
     get_real_item(&pclBurn[ilRecCnt][0],pclDataBuf,2);
     TrimRight(&pclBurn[ilRecCnt][0]);
     get_real_item(&pclNafr[ilRecCnt][0],pclDataBuf,3);
     TrimRight(&pclNafr[ilRecCnt][0]);
     get_real_item(&pclNato[ilRecCnt][0],pclDataBuf,4);
     TrimRight(&pclNato[ilRecCnt][0]);
     get_real_item(&pclResn[ilRecCnt][0],pclDataBuf,5);
     TrimRight(&pclResn[ilRecCnt][0]);
     ilRecCnt++;
     ilFound = TRUE;
  }
  close_my_cursor(&slCursor);
  if (ilFound == TRUE)
  {
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     strcpy(pclDataListOld,"");
     if (strcmp(pcgGMSResSche,&pclNato[0][0]) != 0)
        strcat(pclFieldList,"NATO,");
     strcat(pclDataListOld,&pclNato[0][0]);
     strcat(pclDataListOld,",");
     if (strcmp(pcgGMSResRmrk,&pclResn[0][0]) != 0)
        strcat(pclFieldList,"RESN,");
     strcat(pclDataListOld,&pclResn[0][0]);
     strcat(pclDataListOld,",");
     strcat(pclDataListOld,&pclUrno[0][0]);
     if (strlen(pclFieldList) > 0)
     {
        sprintf(pclFieldList,"NATO='%s',RESN='%s'",
                pcgGMSResSche,pcgGMSResRmrk);
        sprintf(pclSelection,"WHERE URNO = %s",&pclUrno[0][0]);
        sprintf(pclSqlBuf,"UPDATE BLKTAB SET %s %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error updating BLKTAB",pclFunc);
           close_my_cursor(&slCursor);
           return ilRC;
        }
        close_my_cursor(&slCursor);
        strcpy(pclFieldList,"NATO,RESN,URNO");
        sprintf(pclDataList,"%s,%s,%s",
                pcgGMSResSche,pcgGMSResRmrk,&pclUrno[0][0]);
        strcat(pclDataList,"\n");
        strcat(pclDataList,pclDataListOld);
        ilRC = TriggerBchdlAction("URT","BLKTAB",pclSelection,pclFieldList,pclDataList,TRUE,TRUE);
     }
  }
  else
  {
     strcpy(pcpUrno,"");
     ilRC = GetNextUrno();
     sprintf(pclBlkUrno,"%d",igLastUrno);
     strcpy(pclFieldList,"URNO,BURN,NAFR,NATO,RESN,TABN,DAYS,HOPO");
     sprintf(pclDataList,"'%s','%s','%s','%s','%s','%s','1234567','%s'",
             pclBlkUrno,pclLocUrno,pcgGMSResSchs,pcgGMSResSche,pcgGMSResRmrk,pclType,pcgHomeAp);
     sprintf(pclSqlBuf,"INSERT INTO BLKTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     if (ilRCdb == DB_SUCCESS)
        commit_work();
     else
     {
        dbg(TRACE,"%s Error inserting into BLKTAB",pclFunc);
        close_my_cursor(&slCursor);
        return ilRC;
     }
     close_my_cursor(&slCursor);
     strcpy(pclActDataList,"");
     for (ilI = 0; ilI < strlen(pclDataList); ilI++)
        if (strncmp(&pclDataList[ilI],"'",1) != 0)
           strncat(pclActDataList,&pclDataList[ilI],1);
     ilRC = TriggerBchdlAction("IRT","BLKTAB",pclBlkUrno,pclFieldList,pclActDataList,TRUE,TRUE);
  }

  return ilRC;
} /* End of HandleGMSResNoop */


static int HandleGMSDelete(char *pcpAdid, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSDelete:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldListRead[1024];
  char pclFieldListUpd[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  int ilRCdb1 = DB_SUCCESS;
  short slFkt1;
  short slCursor1;
  char pclSqlBuf1[2048];
  char pclDataBuf1[8000];
  char pclSelection1[1024];
  char pclDataList1[1024];
  int ilI;
  int ilNoEle;
  int ilNoFlds;
  char pclUrno[16];
  char pclRsid1[16];
  char pclRsid2[16];
  char pclCommand[16];
  char pclTable[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  char pclRarUrno[16];
  char pclRarType[16];

  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"UFR");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTable,"AFTTAB");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"7800");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_RES","priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  if (*pcpAdid == 'A')
  {
     ilNoEle = GetNoOfElements(pcgUrnoListArr,',');
     if (strcmp(pcpType,"BB") == 0)
     {
        strcpy(pclFieldListRead,"BLT1,BLT2");
        strcpy(pclFieldListUpd,"BLT1,B1BS,B1ES,B1BA,B1EA,BLT2,B2BS,B2ES,B2BA,B2EA");
        strcpy(pclDataList," , , , , , , , , , ");
        ilNoFlds = 2;
     }
     else if (strcmp(pcpType,"GT") == 0)
     {
        strcpy(pclFieldListRead,"GTA1,GTA2");
        strcpy(pclFieldListUpd,"GTA1,GA1B,GA1E,GA1X,GA1Y,GTA2,GA2B,GA2E,GA2X,GA2Y");
        strcpy(pclDataList," , , , , , , , , , ");
        ilNoFlds = 2;
     }
     else if (strcmp(pcpType,"POS") == 0)
     {
        strcpy(pclFieldListRead,"PSTA");
        strcpy(pclFieldListUpd,"PSTA,PABS,PAES,PABA,PAEA");
        strcpy(pclDataList," , , , , ");
        ilNoFlds = 1;
     }
     else
     {
        dbg(TRACE,"%s Invalid Type <%s> defined",pclFunc,pcpType);
        return RC_FAIL;
     }
  }
  else
  {
     ilNoEle = GetNoOfElements(pcgUrnoListDep,',');
     if (strcmp(pcpType,"GT") == 0)
     {
        strcpy(pclFieldListRead,"GTD1,GTD2");
        strcpy(pclFieldListUpd,"GTD1,GD1B,GD1E,GD1X,GD1Y,GTD2,GD2B,GD2E,GD2X,GD2Y");
        strcpy(pclDataList," , , , , , , , , , ");
        ilNoFlds = 2;
     }
     else if (strcmp(pcpType,"POS") == 0)
     {
        strcpy(pclFieldListRead,"PSTD");
        strcpy(pclFieldListUpd,"PSTD,PDBS,PDES,PDBA,PDEA");
        strcpy(pclDataList," , , , , ");
        ilNoFlds = 1;
     }
     else
     {
        dbg(TRACE,"%s Invalid Type <%s> defined",pclFunc,pcpType);
        return RC_FAIL;
     }
  }
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     if (*pcpAdid == 'A')
        get_real_item(pclUrno,pcgUrnoListArr,ilI);
     else
        get_real_item(pclUrno,pcgUrnoListDep,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldListRead,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s Record not found, Urno = <%s>",pclFunc,pclUrno);
        }
        else
        {
           BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
           get_real_item(pclRsid1,pclDataBuf,1);
           TrimRight(pclRsid1);
           if (ilNoFlds == 2)
           {
              get_real_item(pclRsid2,pclDataBuf,2);
              TrimRight(pclRsid2);
           }
           else
              strcpy(pclRsid2," ");
           if (*pclRsid1 != ' ' || *pclRsid2 != ' ')
           {
              dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
              dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
              dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldListUpd);
              dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
              ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                   pclCommand,pclTable,pclSelection,pclFieldListUpd,pclDataList,"",ilPriority,NETOUT_NO_ACK);
           }
        }
        if (strcmp(pcpType,"BB") == 0)
           strcpy(pclRarType,"BB");
        else if (strcmp(pcpType,"GT") == 0)
        {
           if (*pcgGMSResAdid == 'A')
              strcpy(pclRarType,"GA");
           else
              strcpy(pclRarType,"GD");
        }
        else if (strcmp(pcpType,"POS") == 0)
        {
           if (*pcgGMSResAdid == 'A')
              strcpy(pclRarType,"PA");
           else
              strcpy(pclRarType,"PD");
        }
        sprintf(pclSelection,"WHERE RURN = %s AND TYPE = '%s' AND STAT <> 'DELETED'",
                pclUrno,pclRarType);
        sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclRarUrno);
        while (ilRCdb == DB_SUCCESS)
        {
           sprintf(pclDataList1,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
           sprintf(pclSelection1,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf1,"UPDATE RARTAB SET %s %s",pclDataList1,pclSelection1);
           slCursor1 = 0;
           slFkt1 = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf1);
           ilRCdb1 = sql_if(slFkt1,&slCursor1,pclSqlBuf1,pclDataBuf1);
           if (ilRCdb1 == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor1);
           slFkt = START;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclRarUrno);
        }
        close_my_cursor(&slCursor);
     }
  }

  return ilRC;
} /* End of HandleGMSDelete */


static int HandleGMSDeleteCnt(char *pcpAdid, char *pcpType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSDeleteCnt:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  int ilRCdb1 = DB_SUCCESS;
  short slFkt1;
  short slCursor1;
  char pclSqlBuf1[2048];
  char pclDataBuf1[8000];
  char pclSelection1[1024];
  char pclDataList1[1024];
  int ilI;
  int ilNoEle;
  char pclUrno[16];
  char pclCkic[16];
  char pclCtyp[16];
  char pclFlnu[16];
  char pclCkbs[16];
  char pclCkes[16];
  char pclRarUrno[16];
  char pclRarType[16];

  ilNoEle = GetNoOfElements(pcgUrnoListDep,',');
  strcpy(pclFieldList,"URNO,CKIC,CTYP,FLNU,CKBS,CKES");
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListDep,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s Record not found, Urno = <%s>",pclFunc,pclUrno);
        }
        else
        {
           BuildItemBuffer(pclDataBuf,"",6,",");
           get_real_item(pclCkic,pclDataBuf,2);
           TrimRight(pclCkic);
           get_real_item(pclCtyp,pclDataBuf,3);
           TrimRight(pclCtyp);
           get_real_item(pclFlnu,pclDataBuf,4);
           TrimRight(pclFlnu);
           get_real_item(pclCkbs,pclDataBuf,5);
           TrimRight(pclCkbs);
           get_real_item(pclCkes,pclDataBuf,6);
           TrimRight(pclCkes);
           sprintf(pclSqlBuf,"DELETE FROM CCATAB WHERE URNO = %s",pclUrno);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error deleting from CCATAB",pclFunc);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
           strcpy(pclFieldList,"URNO,CKIC,CTYP,FLNU,CKBS,CKES");
           sprintf(pclDataList,"%s, ,%s,%s,%s,%s\n%s,%s,%s,%s,%s,%s",
                   pclUrno,pclCtyp,pclFlnu,pclCkbs,pclCkes,
                   pclUrno,pclCkic,pclCtyp,pclFlnu,pclCkbs,pclCkes);
           sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
           ilRC = TriggerBchdlAction("DRT","CCATAB",pclSelection,pclFieldList,pclDataList,TRUE,TRUE);
        }
        if (strcmp(pcpType,"CI") == 0)
           strcpy(pclRarType,"DC");
        else
           strcpy(pclRarType,"CC");
        sprintf(pclSelection,"WHERE RURN = %s AND TYPE = '%s' AND STAT <> 'DELETED'",
                pclUrno,pclRarType);
        sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclRarUrno);
        while (ilRCdb == DB_SUCCESS)
        {
           sprintf(pclDataList1,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
           sprintf(pclSelection1,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf1,"UPDATE RARTAB SET %s %s",pclDataList1,pclSelection1);
           slCursor1 = 0;
           slFkt1 = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf1);
           ilRCdb1 = sql_if(slFkt1,&slCursor1,pclSqlBuf1,pclDataBuf1);
           if (ilRCdb1 == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor1);
           slFkt = START;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclRarUrno);
        }
        close_my_cursor(&slCursor);
     }
  }

  return ilRC;
} /* End of HandleGMSDeleteCnt */


static int HandleGMSDeleteTow(char *pcpAdid)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSDeleteTow:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  int ilI;
  int ilNoEle;
  char pclUrno[16];
  char pclRarUrno[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;

  if (*pcpAdid == 'A')
     ilNoEle = GetNoOfElements(pcgUrnoListArr,',');
  else
     ilNoEle = GetNoOfElements(pcgUrnoListDep,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     if (*pcpAdid == 'A')
        get_real_item(pclUrno,pcgUrnoListArr,ilI);
     else
        get_real_item(pclUrno,pcgUrnoListDep,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        strcpy(pclFieldList,"");
        strcpy(pclDataList,"");
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","snd_cmd_D",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"DFR");
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7800");
        ilRC = iGetConfigEntry(pcgConfigFile,"GMS_TOW","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
        ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,"AFTTAB",pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);

        sprintf(pclSelection,"WHERE RURN = %s AND TYPE = 'TOW' AND STAT <> 'DELETED'",pclUrno);
        sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclRarUrno);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           sprintf(pclDataList,"STAT = 'DELETED' , TIME = '%s'",pcgCurrentTime);
           sprintf(pclSelection,"WHERE URNO = %s",pclRarUrno);
           sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclDataList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error updating RARTAB",pclFunc);
              ilRC = RC_FAIL;
           }
           close_my_cursor(&slCursor);
        }
     }
  }

  return ilRC;
} /* End of HandleGMSDeleteTow */


static int HandleGMSDeleteNoop()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSDeleteNoop:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFieldList[1024];
  char pclSelection[1024];
  char pclDataList[1024];
  int ilI;
  int ilNoEle;
  char pclUrno[16];
  char pclBurn[16];
  char pclNafr[16];
  char pclNato[16];
  char pclTabn[16];

  ilNoEle = GetNoOfElements(pcgUrnoListArr,',');
  strcpy(pclFieldList,"URNO,BURN,NAFR,NATO,TABN");
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListArr,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM BLKTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           dbg(TRACE,"%s Record not found, Urno = <%s>",pclFunc,pclUrno);
        }
        else
        {
           BuildItemBuffer(pclDataBuf,"",5,",");
           get_real_item(pclBurn,pclDataBuf,2);
           TrimRight(pclBurn);
           get_real_item(pclNafr,pclDataBuf,3);
           TrimRight(pclNafr);
           get_real_item(pclNato,pclDataBuf,4);
           TrimRight(pclNato);
           get_real_item(pclTabn,pclDataBuf,5);
           TrimRight(pclTabn);
           sprintf(pclSqlBuf,"DELETE FROM BLKTAB WHERE URNO = %s",pclUrno);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           else
           {
              dbg(TRACE,"%s Error deleting from BLKTAB",pclFunc);
              close_my_cursor(&slCursor);
              return ilRC;
           }
           close_my_cursor(&slCursor);
           strcpy(pclFieldList,"URNO,BURN,NAFR,NATO,TABN");
           sprintf(pclDataList,"%s,%s,%s,%s,%s\n%s, ,%s,%s,%s",
                   pclUrno,pclBurn,pclNafr,pclNato,pclTabn,
                   pclUrno,pclNafr,pclNato,pclTabn);
           sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
           ilRC = TriggerBchdlAction("DRT","BLKTAB",pclSelection,pclFieldList,pclDataList,TRUE,TRUE);
        }
     }
  }

  return ilRC;
} /* End of HandleGMSDeleteNoop */


static int HandleGMSFileOut(char *pcpFile, char *pcpSeperator, char *pcpFieldList, char *pcpSelection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSFileOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  FILE *fp;
  FILE *fp_out;
  char pclFileOut[1024];
  char *pclTmpPtr;
  char pclLine[32000];
  int ilNoFld;
  int ilNoEle;
  int ilI;
  char pclField[16];
  char pclData[2000];
  char pclNewData[2000];
  char pclNewLine[2000];
  char pclCharAfterField[16];
  char pclCharAfterRecord[16];
  char pclCommand[16];
  char pclModId[16];
  char pclPriority[16];
  int ilModId;
  int ilPriority;
  char pclRsty[16];
  char pclRsid[16];
  char pclType[16];
  char pclFlnu[16];
  int ilCount;
  char pclUrno[16];
  char pclFlno[16];
  char pclCsgn[16];
  char pclStoa[16];
  char pclStod[16];
  char pclAdid[16];
  char pclAurn[16];

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CHAR_AFTER_FIELD",CFG_STRING,pclCharAfterField);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCharAfterField,"");
  else
  {
     if (strcmp(pclCharAfterField,"NL") == 0)
        strcpy(pclCharAfterField,"\n");
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CHAR_AFTER_RECORD",CFG_STRING,pclCharAfterRecord);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCharAfterRecord,"\n");
  else
  {
     if (strcmp(pclCharAfterRecord,"NL") == 0)
        strcpy(pclCharAfterRecord,"\n");
  }
  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  strcpy(pclFileOut,pcpFile);
  pclTmpPtr = strstr(pclFileOut,".");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';
  strcat(pclFileOut,".xml");
  if ((fp_out = (FILE *)fopen(pclFileOut,"w")) == (FILE *)NULL)
  {
     fclose(fp);
     dbg(TRACE,"%s Cannot create File <%s>",pclFunc,pclFileOut);
     return RC_FAIL;
  }
  if (strcmp(pcpSelection,"GMS_RESOURCE_OUT") == 0 || strcmp(pcpSelection,"GMS_RES_NOOP_OUT") == 0)
  {
     igRsty = get_item_no(pcpFieldList,"RSTY",5) + 1;
  }
  ilNoFld = GetNoOfElements(pcpFieldList,',');
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line <%s>",pclFunc,pclLine);
     if (strcmp(pcpSelection,"GMS_RESOURCE_OUT") == 0 || strcmp(pcpSelection,"GMS_RES_NOOP_OUT") == 0)
     {
        igRsty = get_item_no(pcpFieldList,"RSTY",5) + 1;
        GetDataItem(pclRsty,pclLine,igRsty,*pcpSeperator,""," ");
        TrimRight(pclRsty);
        igRsid = get_item_no(pcpFieldList,"RSID",5) + 1;
        GetDataItem(pclRsid,pclLine,igRsid,*pcpSeperator,""," ");
        TrimRight(pclRsid);
     }
     else if (strcmp(pcpSelection,"GMS_TOWING_OUT") == 0)
     {
        igRsty = get_item_no(pcpFieldList,"TOID",5) + 1;
        GetDataItem(pclFlnu,pclLine,igRsty,*pcpSeperator,""," ");
        TrimRight(pclFlnu);
        igRsty = get_item_no(pcpFieldList,"URNO",5) + 1;
        GetDataItem(pclAurn,pclLine,igRsty,*pcpSeperator,""," ");
        TrimRight(pclAurn);
     }
     ilNoEle = GetNoOfElements(pclLine,*pcpSeperator);
     if (ilNoEle < ilNoFld)
        dbg(TRACE,"%s Ignore Line. No Items does not match (%d) (%d).",pclFunc,ilNoFld,ilNoEle);
     else
     {
        for (ilI = 1; ilI <= ilNoFld; ilI++)
        {
           GetDataItem(pclField,pcpFieldList,ilI,',',""," ");
           TrimRight(pclField);
           GetDataItem(pclData,pclLine,ilI,*pcpSeperator,""," ");
           TrimRight(pclData);
           if (strcmp(pcpSelection,"GMS_RESOURCE_OUT") == 0)
           {
              if (strcmp(pclField,"RAID") == 0)
              {
                 if (strcmp(pclRsty,"BB") == 0)
                    strcpy(pclType,"('BB')");
                 else if (strcmp(pclRsty,"CI") == 0)
                    strcpy(pclType,"('DC')");
                 else if (strcmp(pclRsty,"CCI") == 0)
                    strcpy(pclType,"('CC')");
                 else if (strcmp(pclRsty,"GT") == 0)
                    strcpy(pclType,"('GA','GD')");
                 else if (strcmp(pclRsty,"CI") == 0)
                    strcpy(pclType,"('PA','PD')");
                 sprintf(pclSqlBuf,"SELECT RAID FROM RARTAB WHERE RURN = %s AND RNAM = '%s' AND TYPE IN %s",
                         pclData,pclRsid,pclType);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 TrimRight(pclDataBuf);
                 if (ilRCdb == DB_SUCCESS)
                    strcpy(pclData,pclDataBuf);
                 else
                    strcpy(pclData," ");
              }
              if (strcmp(pclField,"ALCD") == 0)
              {
                 if (strcmp(pclRsty,"CCI") == 0 || strcmp(pclRsty,"CI") == 0)
                 {
                    strcpy(pclFlnu,pclData);
                    if (strcmp(pclRsty,"CCI") == 0)
                    {
                       ilCount = 0;
                       ilRC = syslibSearchDbData("ALTTAB","URNO",pclFlnu,"ALC2",pclData,&ilCount,"\n");
                       if (*pclData == ' ' || *pclData == '\0')
                          ilRC = syslibSearchDbData("ALTTAB","URNO",pclFlnu,"ALC3",pclData,&ilCount,"\n");
                       TrimRight(pclData);
                    }
                    else
                    {
                       strcpy(pclData," ");
                       sprintf(pclSqlBuf,"SELECT URNO,FLNO,CSGN,STOA,STOD,ADID FROM AFTTAB WHERE URNO = %s",pclFlnu);
                       slCursor = 0;
                       slFkt = START;
                       dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                       ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                       close_my_cursor(&slCursor);
                       if (ilRC == DB_SUCCESS)
                       {
                          BuildItemBuffer(pclDataBuf,"",6,",");
                          get_real_item(pclUrno,pclDataBuf,1);
                          TrimRight(pclUrno);
                          get_real_item(pclFlno,pclDataBuf,2);
                          TrimRight(pclFlno);
                          get_real_item(pclCsgn,pclDataBuf,3);
                          TrimRight(pclCsgn);
                          get_real_item(pclStoa,pclDataBuf,4);
                          TrimRight(pclStoa);
                          get_real_item(pclStod,pclDataBuf,5);
                          TrimRight(pclStod);
                          get_real_item(pclAdid,pclDataBuf,6);
                          TrimRight(pclAdid);
                       }
                       else
                       {
                          strcpy(pclUrno," ");
                          strcpy(pclFlno," ");
                          strcpy(pclCsgn," ");
                          strcpy(pclStoa," ");
                          strcpy(pclStod," ");
                          strcpy(pclAdid," ");
                       }
                    }
                 }
              }
              if (strcmp(pclRsty,"CI") == 0 && strcmp(pclField,"URNO") == 0)
                 strcpy(pclData,pclUrno);
              if (strcmp(pclRsty,"CI") == 0 && strcmp(pclField,"FLNO") == 0)
                 strcpy(pclData,pclFlno);
              if (strcmp(pclRsty,"CI") == 0 && strcmp(pclField,"CSGN") == 0)
                 strcpy(pclData,pclCsgn);
              if (strcmp(pclRsty,"CI") == 0 && strcmp(pclField,"STDT") == 0)
              {
                 if (*pclAdid == 'A')
                    strcpy(pclData,pclStoa);
                 else
                    strcpy(pclData,pclStod);
              }
              if (strcmp(pclRsty,"CI") == 0 && strcmp(pclField,"ADID") == 0)
                 strcpy(pclData,pclAdid);
           }
           if (strcmp(pcpSelection,"GMS_TOWING_OUT") == 0)
           {
              if (strcmp(pclField,"TOID") == 0)
              {
                 sprintf(pclSqlBuf,"SELECT RAID FROM RARTAB WHERE RURN = %s AND TYPE = 'TOW'",pclFlnu);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 TrimRight(pclDataBuf);
                 if (ilRCdb == DB_SUCCESS)
                    strcpy(pclData,pclDataBuf);
                 else
                    strcpy(pclData," ");
                 sprintf(pclSqlBuf,"SELECT URNO,FLNO,CSGN,STOA,STOD,ADID FROM AFTTAB WHERE URNO = %s",pclAurn);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 if (ilRC == DB_SUCCESS)
                 {
                    BuildItemBuffer(pclDataBuf,"",6,",");
                    get_real_item(pclUrno,pclDataBuf,1);
                    TrimRight(pclUrno);
                    get_real_item(pclFlno,pclDataBuf,2);
                    TrimRight(pclFlno);
                    get_real_item(pclCsgn,pclDataBuf,3);
                    TrimRight(pclCsgn);
                    get_real_item(pclStoa,pclDataBuf,4);
                    TrimRight(pclStoa);
                    get_real_item(pclStod,pclDataBuf,5);
                    TrimRight(pclStod);
                    get_real_item(pclAdid,pclDataBuf,6);
                    TrimRight(pclAdid);
                 }
                 else
                 {
                    strcpy(pclUrno," ");
                    strcpy(pclFlno," ");
                    strcpy(pclCsgn," ");
                    strcpy(pclStoa," ");
                    strcpy(pclStod," ");
                    strcpy(pclAdid," ");
                 }
              }
              if (strcmp(pclField,"ADID") == 0)
                 strcpy(pclData,pclAdid);
              if (strcmp(pclField,"URNO") == 0)
                 strcpy(pclData,pclUrno);
              if (strcmp(pclField,"FLNO") == 0)
                 strcpy(pclData,pclFlno);
              if (strcmp(pclField,"CSGN") == 0)
                 strcpy(pclData,pclCsgn);
              if (strcmp(pclField,"STDT") == 0)
              {
                 if (*pclAdid == 'A')
                    strcpy(pclData,pclStoa);
                 else
                    strcpy(pclData,pclStod);
              }
           }
           if (strcmp(pcpSelection,"GMS_RES_NOOP_OUT") == 0)
           {
              if (strcmp(pclField,"RSID") == 0)
              {
                 ilCount = 0;
                 strcpy(pclNewData," ");
                 if (strcmp(pclRsty,"BLT") == 0)
                    ilRC = syslibSearchDbData("BLTTAB","URNO",pclData,"BNAM",pclNewData,&ilCount,"\n");
                 else if (strcmp(pclRsty,"CIC") == 0)
                    ilRC = syslibSearchDbData("CICTAB","URNO",pclData,"CNAM",pclNewData,&ilCount,"\n");
                 else if (strcmp(pclRsty,"GAT") == 0)
                    ilRC = syslibSearchDbData("GATTAB","URNO",pclData,"GNAM",pclNewData,&ilCount,"\n");
                 else if (strcmp(pclRsty,"PST") == 0)
                    ilRC = syslibSearchDbData("PSTTAB","URNO",pclData,"PNAM",pclNewData,&ilCount,"\n");
                 if (ilRC == RC_SUCCESS)
                    strcpy(pclData,pclNewData);
                 else
                    strcpy(pclData," ");
                 TrimRight(pclData);
              }
              if (strcmp(pclField,"RSTY") == 0)
              {
                 if (strcmp(pclRsty,"BLT") == 0)
                    strcpy(pclData,"BB");
                 else if (strcmp(pclRsty,"CIC") == 0)
                    strcpy(pclData,"CI");
                 else if (strcmp(pclRsty,"GAT") == 0)
                    strcpy(pclData,"GT");
                 else if (strcmp(pclRsty,"PST") == 0)
                    strcpy(pclData,"POS");
              }
           }
           sprintf(pclNewLine,"<%s>%s</%s>",pclField,pclData,pclField);
           fprintf(fp_out,"%s%s",pclNewLine,pclCharAfterField);
        }
        if (strlen(pclCharAfterField) == 0 && strlen(pclCharAfterRecord) > 0)
           fprintf(fp_out,"%s",pclCharAfterRecord);
     }
  }
  fclose(fp);
  fclose(fp_out);
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_FILE_OUT","snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"ACK");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_FILE_OUT","mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"7924");
  ilRC = iGetConfigEntry(pcgConfigFile,"GMS_FILE_OUT","priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  dbg(TRACE,"%s Send <%s> to <%d> , Prio = %d",pclFunc,pclCommand,ilModId,ilPriority);
  dbg(TRACE,"%s Selection: <%s>",pclFunc,pclFileOut);
  ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                       pclCommand,"",pclFileOut,"","","",ilPriority,NETOUT_NO_ACK);

  return ilRC;
} /* End of HandleGMSFileOut */


static int CheckXmlInput(char *pcpData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckXmlInput:";
  char pclNewData[512000];
  int ilCurLine;
  char *pclTmpPtr;
  char *pclTmpPtr2;
  char *pclTmpPtr3;
  char pclLine[512000];
  char pclTag[1024];
  char pclTmpTag[1024];
  char pclData[4096];
  char *pclTmpBgn;
  char *pclTmpEnd;
  int ilIdx;
  char pclSearchItem[2048];
  char pclStack[50][1024];
  char pclStack2[50][1024];
  int ilStack;
  char pclSitaSections[1024];
  int ilSitaSecFound;
  int ilContentFound;
  int ilI;
  int ilNoEle;
  char pclTmpBuf[1024];
  char pclSecBgn[1024];
  char pclSecEnd[1024];

  CheckCDATA(pcpData);

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SITA_SECTIONS",CFG_STRING,pclSitaSections);
  if (ilRC == RC_SUCCESS)
     ilNoEle = GetNoOfElements(pclSitaSections,',');
  else
     ilNoEle = 0;
  ilSitaSecFound = FALSE;
  ilContentFound = FALSE;
  memset(pclNewData,0x00,512000);
  strcpy(pcgSitaText,"");
  pclTmpPtr = pclNewData;
  pclTmpPtr2 = pcpData;
  while (*pclTmpPtr2 != '\0')
  {
     if (*pclTmpPtr2 != '\n')
     {
        *pclTmpPtr = *pclTmpPtr2;
        pclTmpPtr++;
     }
     else
     {
        CopyLine(pclLine,pclTmpPtr2+1);
        for (ilI = 1; ilI <= ilNoEle; ilI++)
        {
           get_real_item(pclTmpBuf,pclSitaSections,ilI);
           sprintf(pclSecBgn,"<%s>",pclTmpBuf);
           sprintf(pclSecEnd,"</%s>",pclTmpBuf);
           if (strstr(pclLine,pclSecBgn) != NULL)
              ilSitaSecFound = TRUE;
           else if (strstr(pclLine,pclSecEnd) != NULL)
              ilSitaSecFound = FALSE;
        }
        if (ilSitaSecFound == TRUE)
        {
           if (strstr(pclLine,"<CONTENT>") != NULL)
              ilContentFound = TRUE;
           else if (strstr(pclLine,"</CONTENT>") != NULL)
           {
              ilContentFound = FALSE;
              strcat(pcgSitaText,pclLine);
              strcat(pcgSitaText,"\n");
           }
           if (ilContentFound == TRUE)
           {
              strcat(pcgSitaText,pclLine);
              strcat(pcgSitaText,"\n");
              while (*pclTmpPtr != '>')
              {
                 *pclTmpPtr = '\0';
                 pclTmpPtr--;
              }
              pclTmpPtr++;
           }
        }
     }
     pclTmpPtr2++;
  }
  strcpy(pcpData,pclNewData);
/*dbg(TRACE,"ConfData = \n%s",pcpData);*/
  ilStack = 0;
  strcpy(pclNewData,"");
  ilCurLine = 1;
  pclTmpPtr = GetLine(pcpData,ilCurLine);
  while (pclTmpPtr != NULL)
  {
     CopyLine(pclLine,pclTmpPtr);
     TrimRight(pclLine);
     pclTmpBgn = pclLine;
     while (*pclTmpBgn != '\0')
     {
        while (*pclTmpBgn != '\0' && *pclTmpBgn != '<')
           pclTmpBgn++;
        pclTmpEnd = pclTmpBgn;
        while (*pclTmpEnd != '\0' && *pclTmpEnd != '>')
           pclTmpEnd++;
        if (*pclTmpEnd != '\0')
           pclTmpEnd++;
        memset(pclTag,0x00,1024);
        strncpy(pclTag,pclTmpBgn,pclTmpEnd-pclTmpBgn);
        strcat(pclNewData,pclTag);
        pclTmpBgn = pclTmpEnd;
        if (strstr(pclTag,"!--") != NULL || strstr(pclTag,"?xml") != NULL)
           strcat(pclNewData,"\n");
        else
        {
           ilRC = RemoveCharFromTag(pclTag,pclTmpTag);
           ilIdx = GetXmlLineIdx("STR_BGN",pclTmpTag,NULL);
           if (ilIdx >= 0)
           {
              strcat(pclNewData,"\n");
              if (pclTag[strlen(pclTag)-2] != '/')
              {
                 if (pclTag[1] != '/')
                 {
                    strcpy(&pclStack[ilStack][0],pclTmpTag);
                    strcpy(&pclStack2[ilStack][0],pclTag);
                    ilStack++;
                 }
                 else
                 {
                    if (ilStack == 0)
                    {
                       strcpy(&pclStack[ilStack][0],pclTmpTag);
                       strcpy(&pclStack2[ilStack][0],pclTag);
                       ilStack++;
                    }
                    else
                    {
                       if (strcmp(&pclStack[ilStack-1][0],pclTmpTag) != 0)
                       {
                          strcpy(&pclStack[ilStack][0],pclTmpTag);
                          strcpy(&pclStack2[ilStack][0],pclTag);
                          ilStack++;
                       }
                       else
                          ilStack--;
                    }
                 }
              }
           }
           else
           {
/*
              pclTmpEnd = pclTmpBgn;
              while (*pclTmpEnd != '\0' && *pclTmpEnd != '>')
                 pclTmpEnd++;
              if (*pclTmpEnd != '\0')
                 pclTmpEnd++;
              memset(pclData,0x00,4096);
              strncpy(pclData,pclTmpBgn,pclTmpEnd-pclTmpBgn);
              strcat(pclNewData,pclData);
              pclTmpBgn = pclTmpEnd;
              while (*pclTmpBgn != '\0' && *pclTmpBgn != '<')
                 pclTmpBgn++;
              pclTmpEnd = pclTmpBgn;
              while (*pclTmpEnd != '\0' && *pclTmpEnd != '>')
                 pclTmpEnd++;
              if (*pclTmpEnd != '\0')
                 pclTmpEnd++;
*/
              sprintf(pclSearchItem,"</%s>",pclTmpTag);
              pclTmpEnd = strstr(pclTmpBgn,pclSearchItem);
              if (pclTmpEnd != NULL)
              {
                 while (*pclTmpEnd != '\0' && *pclTmpEnd != '>')
                    pclTmpEnd++;
                 if (*pclTmpEnd != '\0')
                    pclTmpEnd++;
                 memset(pclTag,0x00,1024);
                 strncpy(pclTag,pclTmpBgn,pclTmpEnd-pclTmpBgn);
                 strcat(pclNewData,pclTag);
                 strcat(pclNewData,"\n");
                 pclTmpBgn = pclTmpEnd;
              }
              else
              {
                 if (pclTag[strlen(pclTag)-2] != '/')
                 {
                    dbg(TRACE,"%s Ending Tag <%s> not found",pclFunc,pclTmpTag);
                    sprintf(pcgStatusMsgTxt,"ENDING TAG </%s> NOT FOUND\n",pclTmpTag);
                    SetStatusMessage(pcgStatusMsgTxt);
                    return RC_FAIL;
                 }
                 else
                 {
                    strcat(pclNewData,pclSearchItem);
                    strcat(pclNewData,"\n");
                 }
              }
           }
        }
     }
     ilCurLine++;
     pclTmpPtr = GetLine(pcpData,ilCurLine);
  }
  if (ilStack > 0)
  {
     dbg(TRACE,"%s Structure Mismatch",pclFunc);
     for (ilIdx = 0; ilIdx < ilStack; ilIdx++)
     {
        dbg(TRACE,"%s Stack %d %s",pclFunc,ilIdx,&pclStack2[ilIdx][0]);
        sprintf(pcgStatusMsgTxt,"XML STRUCTURE MISMATCH ON LEVEL %d: TAG %s\n",(ilStack-ilIdx),&pclStack2[ilIdx][0]);
        SetStatusMessage(pcgStatusMsgTxt);
     }
     return RC_FAIL;
  }
/*dbg(TRACE,"NewData = \n%s",pclNewData);*/
  strcpy(pcpData,pclNewData);

  return ilRC;
} /* End of CheckXmlInput */


static int RemoveCharFromTag(char *pcpTag, char *pcpNewTag)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "RemoveCharFromTag:";
  int ilI;
  char *pclTmpPtr;

  ilI = 0;
  pclTmpPtr = pcpTag;
  while (*pclTmpPtr != '\0')
  {
     if (*pclTmpPtr != '<' && *pclTmpPtr != '>' && *pclTmpPtr != '/')
     {
        pcpNewTag[ilI] = *pclTmpPtr;
        ilI++;
     }
     pclTmpPtr++;
  }
  pcpNewTag[ilI] = '\0';
  pclTmpPtr = strstr(pcpNewTag," ");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';

  return ilRC;
} /* End of RemoveCharFromTag */


static int ReplaceItem(char *pcpList, int ipItemNo, char *pcpNew)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "ReplaceItem:";
  int ilI;
  int ilNoItem;
  char pclTmpData[32000];
  char pclTmpItem[32000];

  ilNoItem = GetNoOfElements(pcpList,',');
  strcpy(pclTmpData,"");
  for (ilI = 1; ilI < ipItemNo; ilI++)
  {
     get_real_item(pclTmpItem,pcpList,ilI);
     strcat(pclTmpData,pclTmpItem);
     strcat(pclTmpData,",");
  }
  strcat(pclTmpData,pcpNew);
  strcat(pclTmpData,",");
  for (ilI = ipItemNo + 1; ilI <= ilNoItem; ilI++)
  {
     get_real_item(pclTmpItem,pcpList,ilI);
     strcat(pclTmpData,pclTmpItem);
     strcat(pclTmpData,",");
  }
  pclTmpData[strlen(pclTmpData)-1] = '\0';
  strcpy(pcpList,pclTmpData);

  return ilRC;
} /* End of ReplaceItem */


static int RemoveItem(char *pcpFields, char *pcpData, int ipItemNo)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "RemoveItem:";
  int ilI;
  int ilNoItem;
  char pclTmpFields[32000];
  char pclTmpData[32000];
  char pclTmpItem[32000];

  ilNoItem = GetNoOfElements(pcpFields,',');
  strcpy(pclTmpFields,"");
  strcpy(pclTmpData,"");
  for (ilI = 1; ilI < ipItemNo; ilI++)
  {
     get_real_item(pclTmpItem,pcpFields,ilI);
     strcat(pclTmpFields,pclTmpItem);
     strcat(pclTmpFields,",");
     get_real_item(pclTmpItem,pcpData,ilI);
     strcat(pclTmpData,pclTmpItem);
     strcat(pclTmpData,",");
  }
  for (ilI = ipItemNo + 1; ilI <= ilNoItem; ilI++)
  {
     get_real_item(pclTmpItem,pcpFields,ilI);
     strcat(pclTmpFields,pclTmpItem);
     strcat(pclTmpFields,",");
     get_real_item(pclTmpItem,pcpData,ilI);
     strcat(pclTmpData,pclTmpItem);
     strcat(pclTmpData,",");
  }
  pclTmpFields[strlen(pclTmpFields)-1] = '\0';
  strcpy(pcpFields,pclTmpFields);
  pclTmpData[strlen(pclTmpData)-1] = '\0';
  strcpy(pcpData,pclTmpData);

  return ilRC;
} /* End of RemoveItem */


static int CheckForInsert()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckForInsert:";
  int ilI;
  int ilIdx;
  char pclKeyAdid[16] = " ";
  char pclKeyFlno[16] = " ";
  char pclKeyTime[16] = " ";
  char pclSavAdid[16] = " ";
  char pclSavFlno[16] = " ";
  char pclSavTime[16] = " ";
  int ilFound;

  if (igSavInsCnt == 0)
  {
     return RC_SUCCESS;
  }

  ilRC = CheckForDelFromCache();
  ilFound = FALSE;
  for (ilI = 0; ilI < igSavInsCnt && ilFound == FALSE; ilI += 2)
  {
     ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
     if (ilIdx >= 0)
        strcpy(pclKeyAdid,&rgXml.rlLine[ilIdx].pclData[0]);
     ilIdx = GetXmlLineIdx("KEY","FLNO",NULL);
     if (ilIdx >= 0)
        strcpy(pclKeyFlno,&rgXml.rlLine[ilIdx].pclData[0]);
     ilIdx = GetXmlLineIdx("KEY","STDT",NULL);
     if (ilIdx >= 0)
        strcpy(pclKeyTime,&rgXml.rlLine[ilIdx].pclData[0]);
     ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"ADID",5) + 1;
     if (ilIdx > 0)
        get_real_item(pclSavAdid,&pcgSaveInserts[ilI+1][0],ilIdx);
     ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"FLNO",5) + 1;
     if (ilIdx > 0)
        get_real_item(pclSavFlno,&pcgSaveInserts[ilI+1][0],ilIdx);
     if (*pclKeyAdid == 'D')
        ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"STOD",5) + 1;
     else
        ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"STOA",5) + 1;
     if (ilIdx > 0)
        get_real_item(pclSavTime,&pcgSaveInserts[ilI+1][0],ilIdx);
     if (strcmp(pclKeyAdid,pclSavAdid) == 0 &&
         strcmp(pclKeyFlno,pclSavFlno) == 0 &&
         strcmp(pclKeyTime,pclSavTime) == 0)
     {
        ilFound = TRUE;
        ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"URNO",5) + 1;
        if (ilIdx > 0)
           get_real_item(pcgUrno,&pcgSaveInserts[ilI+1][0],ilIdx);
        dbg(TRACE,"%s Record with URNO <%s> was just inserted",pclFunc,pcgUrno);
     }
  }
  if (ilFound == TRUE)
     return RC_FAIL;
  else
     return RC_SUCCESS;
} /* End of CheckForInsert */


static int CheckForInsertDel()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckForInsertDel:";
  int ilI;
  int ilIdx;
  char pclSavUrno[16] = " ";
  int ilFound;

  if (igSavDelCnt == 0)
  {
     return RC_FAIL;
  }

  ilRC = CheckForDelFromCacheDel();
  ilFound = FALSE;
  for (ilI = 0; ilI < igSavDelCnt && ilFound == FALSE; ilI += 2)
  {
     ilIdx = get_item_no(&pcgSaveDeletes[ilI][0],"URNO",5) + 1;
     if (ilIdx > 0)
        get_real_item(pclSavUrno,&pcgSaveDeletes[ilI+1][0],ilIdx);
     if (strcmp(pcgUrno,pclSavUrno) == 0)
     {
        ilFound = TRUE;
        dbg(TRACE,"%s Record with URNO <%s> was just deleted",pclFunc,pcgUrno);
     }
  }
  if (ilFound == TRUE)
     return RC_SUCCESS;
  else
     return RC_FAIL;
} /* End of CheckForInsertDel */


static int CheckForDelFromCache()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckForDelFromCache:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  int ilI;
  int ilJ;
  int ilIdx;
  char pclUrno[16];
  char pclIndexList[1024];
  int ilNoItem;
  char pclItem[16];

  if (igSavInsCnt == 0)
     return ilRC;
  strcpy(pclIndexList,"");
  for (ilI = 0; ilI < igSavInsCnt; ilI += 2)
  {
     ilIdx = get_item_no(&pcgSaveInserts[ilI][0],"URNO",5) + 1;
     if (ilIdx > 0)
     {
        get_real_item(pclUrno,&pcgSaveInserts[ilI+1][0],ilIdx);
        strcpy(pclFieldList,"FLNO");
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           sprintf(pclDataBuf,"%d",ilI);
           strcat(pclIndexList,pclDataBuf);
           strcat(pclIndexList,",");
        }
     }
  }
  if (strlen(pclIndexList) > 0)
  {
     pclIndexList[strlen(pclIndexList)-1] = '\0';
     ilNoItem = GetNoOfElements(pclIndexList,',');
     for (ilI = ilNoItem; ilI > 0; ilI--)
     {
        get_real_item(pclItem,pclIndexList,ilI);
        ilIdx = atoi(pclItem);
        for (ilJ = ilIdx; ilJ < igSavInsCnt; ilJ++)
        {
           strcpy(&pcgSaveInserts[ilJ][0],&pcgSaveInserts[ilJ+2][0]);
           strcpy(&pcgSaveInserts[ilJ+1][0],&pcgSaveInserts[ilJ+3][0]);
        }
        igSavInsCnt -= 2;
     }
     dbg(TRACE,"%s Current Number of inserted Records in Cache = %d",pclFunc,igSavInsCnt/2);
  }

  return ilRC;
} /* End of CheckForDelFromCache */


static int CheckForDelFromCacheDel()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckForDelFromCacheDel:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclSelection[1024];
  char pclFieldList[1024];
  int ilI;
  int ilJ;
  int ilIdx;
  char pclUrno[16];
  char pclIndexList[1024];
  int ilNoItem;
  char pclItem[16];

  if (igSavDelCnt == 0)
     return ilRC;
  strcpy(pclIndexList,"");
  for (ilI = 0; ilI < igSavDelCnt; ilI += 2)
  {
     ilIdx = get_item_no(&pcgSaveDeletes[ilI][0],"URNO",5) + 1;
     if (ilIdx > 0)
     {
        get_real_item(pclUrno,&pcgSaveDeletes[ilI+1][0],ilIdx);
        strcpy(pclFieldList,"FLNO");
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb != DB_SUCCESS)
        {
           sprintf(pclDataBuf,"%d",ilI);
           strcat(pclIndexList,pclDataBuf);
           strcat(pclIndexList,",");
        }
     }
  }
  if (strlen(pclIndexList) > 0)
  {
     pclIndexList[strlen(pclIndexList)-1] = '\0';
     ilNoItem = GetNoOfElements(pclIndexList,',');
     for (ilI = ilNoItem; ilI > 0; ilI--)
     {
        get_real_item(pclItem,pclIndexList,ilI);
        ilIdx = atoi(pclItem);
        for (ilJ = ilIdx; ilJ < igSavDelCnt; ilJ++)
        {
           strcpy(&pcgSaveDeletes[ilJ][0],&pcgSaveDeletes[ilJ+2][0]);
           strcpy(&pcgSaveDeletes[ilJ+1][0],&pcgSaveDeletes[ilJ+3][0]);
        }
        igSavDelCnt -= 2;
     }
     dbg(TRACE,"%s Current Number of deleted Records in Cache = %d",pclFunc,igSavDelCnt/2);
  }

  return ilRC;
} /* End of CheckForDelFromCacheDel */


static int HandleGMSLongTerm(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSLongTerm:";
  char pclInboundBegin[128];
  char pclOutboundBegin[128];
  char pclComCntBegin[128];
  char pclResInfoBegin[128];
  char pclTowInfoBegin[128];
  int ilInboundBegin;
  int ilOutboundBegin;
  int ilComCntBegin;
  int ilResInfoBegin;
  int ilTowInfoBegin;
  char pclTagInBgn[128];
  char pclTagInEnd[128];
  char pclTagOutBgn[128];
  char pclTagOutEnd[128];
  char pclTagComBgn[128];
  char pclTagComEnd[128];
  char pclTagResBgn[128];
  char pclTagResEnd[128];
  char pclTagTowBgn[128];
  char pclTagTowEnd[128];
  char pclTagRsid[16];
  char pclTagRsty[16];
  char pclTagRaid[16];
  char pclTagAlcd[16];
  char pclTagHanm[16];
  char pclTagActs[16];
  char pclTagActe[16];
  char pclTagSchs[16];
  char pclTagSche[16];
  char pclTagDisp[16];
  char pclTagRmrk[16];
  char pclTagUrno[16];
  char pclTagFlno[16];
  char pclTagCsgn[16];
  char pclTagStdt[16];
  char pclTagAdid[16];
  char pclTagToid[16];
  char pclTagPstd[16];
  char pclTagPsta[16];
  char pclTagTwtp[16];
  char pclArrUrno[16];
  char pclArrFlno[16];
  char pclArrCsgn[16];
  char pclArrStdt[16];
  FILE *fp;
  FILE *fp_res;
  FILE *fp_tow;
  char pclLine[2100];
  char pclFileOutRes[128];
  char pclFileOutTow[128];
  char *pclTmpPtr;
  int ilNoResRecs;
  int ilNoTowRecs;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","INBOUND_BEGIN",CFG_STRING,pclInboundBegin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclInboundBegin,"INBOUND");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","OUTBOUND_BEGIN",CFG_STRING,pclOutboundBegin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclOutboundBegin,"OUTBOUND");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","COMMON_COUNTER_BEGIN",CFG_STRING,pclComCntBegin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclComCntBegin,"STATIC");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","RESOURCE_INFO_BEGIN",CFG_STRING,pclResInfoBegin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclResInfoBegin,"RESOURCETYPE");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TOWING_INFO_BEGIN",CFG_STRING,pclTowInfoBegin);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTowInfoBegin,"TOW");

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  strcpy(pclFileOutRes,pcpFile);
  pclTmpPtr = strstr(pclFileOutRes,".");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';
  strcat(pclFileOutRes,"_res.tmp");
  if ((fp_res = (FILE *)fopen(pclFileOutRes,"w")) == (FILE *)NULL)
  {
     fclose(fp);
     dbg(TRACE,"%s Cannot create File <%s>",pclFunc,pclFileOutRes);
     return RC_FAIL;
  }
  strcpy(pclFileOutTow,pcpFile);
  pclTmpPtr = strstr(pclFileOutTow,".");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';
  strcat(pclFileOutTow,"_tow.tmp");
  if ((fp_tow = (FILE *)fopen(pclFileOutTow,"w")) == (FILE *)NULL)
  {
     fclose(fp);
     fclose(fp_res);
     dbg(TRACE,"%s Cannot create File <%s>",pclFunc,pclFileOutTow);
     return RC_FAIL;
  }

  strcpy(pcgGMSResRsid,"");
  strcpy(pcgGMSResRsty,"");
  strcpy(pcgGMSResRaid,"");
  strcpy(pcgGMSResAlcd,"");
  strcpy(pcgGMSResHanm,"");
  strcpy(pcgGMSResActs,"");
  strcpy(pcgGMSResActe,"");
  strcpy(pcgGMSResSchs,"");
  strcpy(pcgGMSResSche,"");
  strcpy(pcgGMSResDisp,"");
  strcpy(pcgGMSResRmrk,"");
  strcpy(pcgGMSResUrno,"");
  strcpy(pcgGMSResFlno,"");
  strcpy(pcgGMSResCsgn,"");
  strcpy(pcgGMSResStdt,"");
  strcpy(pcgGMSResAdid,"");
  strcpy(pcgGMSResToid,"");
  strcpy(pcgGMSResPstd,"");
  strcpy(pcgGMSResPsta,"");
  strcpy(pcgGMSResTwtp,"");
  strcpy(pclArrUrno,"");
  strcpy(pclArrFlno,"");
  strcpy(pclArrCsgn,"");
  strcpy(pclArrStdt,"");
  ilInboundBegin = FALSE;
  ilOutboundBegin = FALSE;
  ilComCntBegin = FALSE;
  ilResInfoBegin = FALSE;
  ilTowInfoBegin = FALSE;
  sprintf(pclTagInBgn,"<%s>",pclInboundBegin);
  sprintf(pclTagInEnd,"</%s>",pclInboundBegin);
  sprintf(pclTagOutBgn,"<%s>",pclOutboundBegin);
  sprintf(pclTagOutEnd,"</%s>",pclOutboundBegin);
  sprintf(pclTagComBgn,"<%s>",pclComCntBegin);
  sprintf(pclTagComEnd,"</%s>",pclComCntBegin);
  sprintf(pclTagResBgn,"<%s>",pclResInfoBegin);
  sprintf(pclTagResEnd,"</%s>",pclResInfoBegin);
  sprintf(pclTagTowBgn,"<%s>",pclTowInfoBegin);
  sprintf(pclTagTowEnd,"</%s>",pclTowInfoBegin);
  strcpy(pclTagRsid,"<RSID>");
  strcpy(pclTagRsty,"<RSTY>");
  strcpy(pclTagRaid,"<RAID>");
  strcpy(pclTagAlcd,"<ALCD>");
  strcpy(pclTagHanm,"<HANM>");
  strcpy(pclTagActs,"<ACTS>");
  strcpy(pclTagActe,"<ACTE>");
  strcpy(pclTagSchs,"<SCHS>");
  strcpy(pclTagSche,"<SCHE>");
  strcpy(pclTagDisp,"<DISP>");
  strcpy(pclTagRmrk,"<RMRK>");
  strcpy(pclTagUrno,"<URNO>");
  strcpy(pclTagFlno,"<FLNO>");
  strcpy(pclTagCsgn,"<CSGN>");
  strcpy(pclTagStdt,"<STDT>");
  strcpy(pclTagAdid,"<ADID>");
  strcpy(pclTagToid,"<TOID>");
  strcpy(pclTagPstd,"<PSTD>");
  strcpy(pclTagPsta,"<PSTA>");
  strcpy(pclTagTwtp,"<TWTP>");
  ilNoResRecs = 0;
  ilNoTowRecs = 0;

  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     if (strstr(pclLine,pclTagInBgn) != NULL)
     {
        ilInboundBegin = TRUE;
        strcpy(pcgGMSResRsid,"");
        strcpy(pcgGMSResRsty,"");
        strcpy(pcgGMSResRaid,"");
        strcpy(pcgGMSResAlcd,"");
        strcpy(pcgGMSResHanm,"");
        strcpy(pcgGMSResActs,"");
        strcpy(pcgGMSResActe,"");
        strcpy(pcgGMSResSchs,"");
        strcpy(pcgGMSResSche,"");
        strcpy(pcgGMSResDisp,"");
        strcpy(pcgGMSResRmrk,"");
        strcpy(pcgGMSResUrno,"");
        strcpy(pcgGMSResFlno,"");
        strcpy(pcgGMSResCsgn,"");
        strcpy(pcgGMSResStdt,"");
        strcpy(pcgGMSResAdid,"A");
        strcpy(pclArrUrno,"");
        strcpy(pclArrFlno,"");
        strcpy(pclArrCsgn,"");
        strcpy(pclArrStdt,"");
     }
     else if (strstr(pclLine,pclTagInEnd) != NULL)
     {
        ilInboundBegin = FALSE;
     }
     else if (strstr(pclLine,pclTagOutBgn) != NULL)
     {
        ilOutboundBegin = TRUE;
        strcpy(pcgGMSResRsid,"");
        strcpy(pcgGMSResRsty,"");
        strcpy(pcgGMSResRaid,"");
        strcpy(pcgGMSResAlcd,"");
        strcpy(pcgGMSResHanm,"");
        strcpy(pcgGMSResActs,"");
        strcpy(pcgGMSResActe,"");
        strcpy(pcgGMSResSchs,"");
        strcpy(pcgGMSResSche,"");
        strcpy(pcgGMSResDisp,"");
        strcpy(pcgGMSResRmrk,"");
        strcpy(pcgGMSResUrno,"");
        strcpy(pcgGMSResFlno,"");
        strcpy(pcgGMSResCsgn,"");
        strcpy(pcgGMSResStdt,"");
        strcpy(pcgGMSResAdid,"D");
     }
     else if (strstr(pclLine,pclTagOutEnd) != NULL)
     {
        ilOutboundBegin = FALSE;
     }
     else if (strstr(pclLine,pclTagComBgn) != NULL)
     {
        ilComCntBegin = TRUE;
        strcpy(pcgGMSResRsid,"");
        strcpy(pcgGMSResRsty,"");
        strcpy(pcgGMSResRaid,"");
        strcpy(pcgGMSResAlcd,"");
        strcpy(pcgGMSResHanm,"");
        strcpy(pcgGMSResActs,"");
        strcpy(pcgGMSResActe,"");
        strcpy(pcgGMSResSchs,"");
        strcpy(pcgGMSResSche,"");
        strcpy(pcgGMSResDisp,"");
        strcpy(pcgGMSResRmrk,"");
        strcpy(pcgGMSResAdid,"D");
     }
     else if (strstr(pclLine,pclTagComEnd) != NULL)
     {
        ilComCntBegin = FALSE;
     }
     else if (strstr(pclLine,pclTagResBgn) != NULL)
     {
        ilResInfoBegin = TRUE;
        strcpy(pcgGMSResRaid,"");
        strcpy(pcgGMSResAlcd,"");
        strcpy(pcgGMSResHanm,"");
        strcpy(pcgGMSResActs,"");
        strcpy(pcgGMSResActe,"");
        strcpy(pcgGMSResSchs,"");
        strcpy(pcgGMSResSche,"");
        strcpy(pcgGMSResDisp,"");
        strcpy(pcgGMSResRmrk,"");
     }
     else if (strstr(pclLine,pclTagResEnd) != NULL)
     {
        if (ilResInfoBegin == TRUE)
        {
           ilResInfoBegin = FALSE;
           ilNoResRecs++;
           if (ilComCntBegin == FALSE)
           {
              dbg(DEBUG,"%s Write Resource Record",pclFunc);
              fprintf(fp_res,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                      pcgGMSResRsid,pcgGMSResRsty,pcgGMSResRaid,pcgGMSResAlcd,
                      pcgGMSResHanm,pcgGMSResActs,pcgGMSResActe,pcgGMSResSchs,
                      pcgGMSResSche,pcgGMSResDisp,pcgGMSResRmrk,pcgGMSResUrno,
                      pcgGMSResFlno,pcgGMSResCsgn,pcgGMSResStdt,pcgGMSResAdid);
           }
           else
           {
              dbg(DEBUG,"%s Write Common Counter Record",pclFunc);
              fprintf(fp_res,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,,,,,\n",
                      pcgGMSResRsid,pcgGMSResRsty,pcgGMSResRaid,pcgGMSResAlcd,
                      pcgGMSResHanm,pcgGMSResActs,pcgGMSResActe,pcgGMSResSchs,
                      pcgGMSResSche,pcgGMSResDisp,pcgGMSResRmrk);
           }
        }
     }
     else if (strstr(pclLine,pclTagTowBgn) != NULL)
     {
        ilTowInfoBegin = TRUE;
        strcpy(pcgGMSResToid,"");
        strcpy(pcgGMSResSchs,"");
        strcpy(pcgGMSResSche,"");
        strcpy(pcgGMSResActs,"");
        strcpy(pcgGMSResActe,"");
        strcpy(pcgGMSResPstd,"");
        strcpy(pcgGMSResPsta,"");
        strcpy(pcgGMSResTwtp,"");
     }
     else if (strstr(pclLine,pclTagTowEnd) != NULL)
     {
        if (ilTowInfoBegin == TRUE)
        {
           ilTowInfoBegin = FALSE;
           ilNoTowRecs++;
           dbg(DEBUG,"%s Write Towing Record",pclFunc);
           fprintf(fp_tow,"%s,%s,%s,%s,%s,%s,%s,%s,A,%s,%s,%s,%s\n",
                   pcgGMSResToid,pcgGMSResSchs,pcgGMSResSche,pcgGMSResActs,
                   pcgGMSResActe,pcgGMSResPstd,pcgGMSResPsta,pcgGMSResTwtp,
                   pclArrUrno,pclArrFlno,pclArrCsgn,pclArrStdt);
        }
     }
     else if (strstr(pclLine,pclTagRsid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRsid,pclLine,pclTagRsid);
     else if (strstr(pclLine,pclTagRsty) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRsty,pclLine,pclTagRsty);
     else if (strstr(pclLine,pclTagRaid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRaid,pclLine,pclTagRaid);
     else if (strstr(pclLine,pclTagAlcd) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResAlcd,pclLine,pclTagAlcd);
     else if (strstr(pclLine,pclTagHanm) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResHanm,pclLine,pclTagHanm);
     else if (strstr(pclLine,pclTagActs) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResActs,pclLine,pclTagActs);
     else if (strstr(pclLine,pclTagActe) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResActe,pclLine,pclTagActe);
     else if (strstr(pclLine,pclTagSchs) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResSchs,pclLine,pclTagSchs);
     else if (strstr(pclLine,pclTagSche) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResSche,pclLine,pclTagSche);
     else if (strstr(pclLine,pclTagDisp) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResDisp,pclLine,pclTagDisp);
     else if (strstr(pclLine,pclTagRmrk) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRmrk,pclLine,pclTagRmrk);
     else if (strstr(pclLine,pclTagUrno) != NULL)
     {
        ilRC = GetXMLDataItem(pcgGMSResUrno,pclLine,pclTagUrno);
        if (ilInboundBegin == TRUE)
           strcpy(pclArrUrno,pcgGMSResUrno);
     }
     else if (strstr(pclLine,pclTagFlno) != NULL)
     {
        ilRC = GetXMLDataItem(pcgGMSResFlno,pclLine,pclTagFlno);
        if (ilInboundBegin == TRUE)
           strcpy(pclArrFlno,pcgGMSResFlno);
     }
     else if (strstr(pclLine,pclTagCsgn) != NULL)
     {
        ilRC = GetXMLDataItem(pcgGMSResCsgn,pclLine,pclTagCsgn);
        if (ilInboundBegin == TRUE)
           strcpy(pclArrCsgn,pcgGMSResCsgn);
     }
     else if (strstr(pclLine,pclTagStdt) != NULL)
     {
        ilRC = GetXMLDataItem(pcgGMSResStdt,pclLine,pclTagStdt);
        if (ilInboundBegin == TRUE)
           strcpy(pclArrStdt,pcgGMSResStdt);
     }
     else if (strstr(pclLine,pclTagAdid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResAdid,pclLine,pclTagAdid);
     else if (strstr(pclLine,pclTagToid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResToid,pclLine,pclTagToid);
     else if (strstr(pclLine,pclTagPstd) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResPstd,pclLine,pclTagPstd);
     else if (strstr(pclLine,pclTagPsta) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResPsta,pclLine,pclTagPsta);
     else if (strstr(pclLine,pclTagTwtp) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResTwtp,pclLine,pclTagTwtp);
  }
  fclose(fp);
  fclose(fp_res);
  fclose(fp_tow);
  dbg(DEBUG,"%s %d Resource Records created",pclFunc,ilNoResRecs);
  dbg(DEBUG,"%s %d Towing Records created",pclFunc,ilNoTowRecs);
  if (ilNoResRecs > 0)
     ilRC = HandleFile("XMLF","","GMS_RESOURCE","",pclFileOutRes);
  if (ilNoTowRecs > 0)
     ilRC = HandleFile("XMLF","","GMS_TOWING","",pclFileOutTow);

  return ilRC;
} /* End of HandleGMSLongTerm */


static int GetXMLDataItem(char *pcpResult, char *pcpLine, char *pcpTag)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetXMLDataItem:";
  char *pclBgn;
  char *pclEnd;

  pclBgn = strstr(pcpLine,pcpTag);
  while (*pclBgn != '>' && *pclBgn != '\0')
     pclBgn++;
  if (*pclBgn != '\0')
     pclBgn++;
  pclEnd = pcpLine + strlen(pcpLine);
  while (*pclEnd != '<' && pclEnd > pclBgn)
     pclEnd--;
  strncpy(pcpResult,pclBgn,pclEnd-pclBgn);
  pcpResult[pclEnd-pclBgn] = '\0';

  return ilRC;
} /* End of GetXMLDataItem */


static int HandleGMSShortTerm(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleGMSShortTerm:";
  FILE *fp;
  FILE *fp_res;
  char pclLine[2100];
  char pclFileOutRes[128];
  char *pclTmpPtr;
  int ilNoResRecs;
  char pclTagRsid[16];
  char pclTagRsty[16];
  char pclTagRaid[16];
  char pclTagAlcd[16];
  char pclTagHanm[16];
  char pclTagActs[16];
  char pclTagActe[16];
  char pclTagSchs[16];
  char pclTagSche[16];
  char pclTagDisp[16];
  char pclTagRmrk[16];
  char pclTagUrno[16];
  char pclTagFlno[16];
  char pclTagCsgn[16];
  char pclTagStdt[16];
  char pclTagAdid[16];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }
  strcpy(pclFileOutRes,pcpFile);
  pclTmpPtr = strstr(pclFileOutRes,".");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';
  strcat(pclFileOutRes,"_res.tmp");
  if ((fp_res = (FILE *)fopen(pclFileOutRes,"w")) == (FILE *)NULL)
  {
     fclose(fp);
     dbg(TRACE,"%s Cannot create File <%s>",pclFunc,pclFileOutRes);
     return RC_FAIL;
  }

  strcpy(pcgGMSResRsid,"");
  strcpy(pcgGMSResRsty,"");
  strcpy(pcgGMSResRaid,"");
  strcpy(pcgGMSResAlcd,"");
  strcpy(pcgGMSResHanm,"");
  strcpy(pcgGMSResActs,"");
  strcpy(pcgGMSResActe,"");
  strcpy(pcgGMSResSchs,"");
  strcpy(pcgGMSResSche,"");
  strcpy(pcgGMSResDisp,"");
  strcpy(pcgGMSResRmrk,"");
  strcpy(pcgGMSResUrno,"");
  strcpy(pcgGMSResFlno,"");
  strcpy(pcgGMSResCsgn,"");
  strcpy(pcgGMSResStdt,"");
  strcpy(pcgGMSResAdid,"");
  strcpy(pclTagRsid,"<RSID>");
  strcpy(pclTagRsty,"<RSTY>");
  strcpy(pclTagRaid,"<RAID>");
  strcpy(pclTagAlcd,"<ALCD>");
  strcpy(pclTagHanm,"<HANM>");
  strcpy(pclTagActs,"<ACTS>");
  strcpy(pclTagActe,"<ACTE>");
  strcpy(pclTagSchs,"<SCHS>");
  strcpy(pclTagSche,"<SCHE>");
  strcpy(pclTagDisp,"<DISP>");
  strcpy(pclTagRmrk,"<RMRK>");
  strcpy(pclTagUrno,"<URNO>");
  strcpy(pclTagFlno,"<FLNO>");
  strcpy(pclTagCsgn,"<CSGN>");
  strcpy(pclTagStdt,"<STDT>");
  strcpy(pclTagAdid,"<ADID>");
  ilNoResRecs = 0;

  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     if (strstr(pclLine,pclTagRsid) != NULL)
     {
        if (strlen(pcgGMSResRsid) > 0)
        {
           dbg(DEBUG,"%s Write Resource Record",pclFunc);
           ilNoResRecs++;
           fprintf(fp_res,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
                   pcgGMSResRsid,pcgGMSResRsty,pcgGMSResRaid,pcgGMSResAlcd,
                   pcgGMSResHanm,pcgGMSResActs,pcgGMSResActe,pcgGMSResSchs,
                   pcgGMSResSche,pcgGMSResDisp,pcgGMSResRmrk,pcgGMSResUrno,
                   pcgGMSResFlno,pcgGMSResCsgn,pcgGMSResStdt,pcgGMSResAdid);
           strcpy(pcgGMSResRsid,"");
           strcpy(pcgGMSResRsty,"");
           strcpy(pcgGMSResRaid,"");
           strcpy(pcgGMSResAlcd,"");
           strcpy(pcgGMSResHanm,"");
           strcpy(pcgGMSResActs,"");
           strcpy(pcgGMSResActe,"");
           strcpy(pcgGMSResSchs,"");
           strcpy(pcgGMSResSche,"");
           strcpy(pcgGMSResDisp,"");
           strcpy(pcgGMSResRmrk,"");
           strcpy(pcgGMSResUrno,"");
           strcpy(pcgGMSResFlno,"");
           strcpy(pcgGMSResCsgn,"");
           strcpy(pcgGMSResStdt,"");
           strcpy(pcgGMSResAdid,"");
        }
        ilRC = GetXMLDataItem(pcgGMSResRsid,pclLine,pclTagRsid);
     }
     else if (strstr(pclLine,pclTagRsty) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRsty,pclLine,pclTagRsty);
     else if (strstr(pclLine,pclTagRaid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRaid,pclLine,pclTagRaid);
     else if (strstr(pclLine,pclTagAlcd) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResAlcd,pclLine,pclTagAlcd);
     else if (strstr(pclLine,pclTagHanm) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResHanm,pclLine,pclTagHanm);
     else if (strstr(pclLine,pclTagActs) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResActs,pclLine,pclTagActs);
     else if (strstr(pclLine,pclTagActe) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResActe,pclLine,pclTagActe);
     else if (strstr(pclLine,pclTagSchs) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResSchs,pclLine,pclTagSchs);
     else if (strstr(pclLine,pclTagSche) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResSche,pclLine,pclTagSche);
     else if (strstr(pclLine,pclTagDisp) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResDisp,pclLine,pclTagDisp);
     else if (strstr(pclLine,pclTagRmrk) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResRmrk,pclLine,pclTagRmrk);
     else if (strstr(pclLine,pclTagUrno) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResUrno,pclLine,pclTagUrno);
     else if (strstr(pclLine,pclTagFlno) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResFlno,pclLine,pclTagFlno);
     else if (strstr(pclLine,pclTagCsgn) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResCsgn,pclLine,pclTagCsgn);
     else if (strstr(pclLine,pclTagStdt) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResStdt,pclLine,pclTagStdt);
     else if (strstr(pclLine,pclTagAdid) != NULL)
        ilRC = GetXMLDataItem(pcgGMSResAdid,pclLine,pclTagAdid);
  }
  if (strlen(pcgGMSResRsid) > 0)
  {
     dbg(DEBUG,"%s Write Resource Record",pclFunc);
     ilNoResRecs++;
     fprintf(fp_res,"%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
             pcgGMSResRsid,pcgGMSResRsty,pcgGMSResRaid,pcgGMSResAlcd,
             pcgGMSResHanm,pcgGMSResActs,pcgGMSResActe,pcgGMSResSchs,
             pcgGMSResSche,pcgGMSResDisp,pcgGMSResRmrk,pcgGMSResUrno,
             pcgGMSResFlno,pcgGMSResCsgn,pcgGMSResStdt,pcgGMSResAdid);
  }
  fclose(fp);
  fclose(fp_res);
  dbg(DEBUG,"%s %d Resource Records created",pclFunc,ilNoResRecs);
  if (ilNoResRecs > 0)
     ilRC = HandleFile("XMLF","","GMS_RESOURCE","",pclFileOutRes);

  return ilRC;
} /* End of HandleGMSShortTerm */


static int HandleSitaSections(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleSitaSections:";
  char pclMethod[32];
  int ilIdx;
  char pclSelection[1024];
  char pclModId[32];
  char pclPriority[32];
  char pclCommand[32];
  char pclTable[32];
  int ilModId;
  int ilPriority;
  char pclFieldList[1024];
  char pclDataList[512000];
  char pclFileName[1024];
  char pclLine[2100];
  char *pclTmpPtr;

  dbg(TRACE,"%s GET METHOD OF SECTION <%s>",pclFunc,pcpSection);

  strcpy(pclMethod,"");
  ilRC = GetData(pclMethod,0,pcpSection);
  ilIdx = GetDataLineIdx("FILE_NAME",0);
  if (ilIdx < 0)
  {
     dbg(TRACE,"%s No File Name specified - Using default",pclFunc);
     /*
     return RC_FAIL;
     */
     strcpy(pclFileName,"NoFileName");
     ilIdx = GetXmlLineIdx("STR_BGN",pcpSection,NULL);
     if (ilIdx < 0)
     {
        dbg(TRACE,"%s No Section Reference specified",pclFunc);
        return RC_FAIL;
     }
     strcpy(pclMethod,&rgXml.rlLine[ilIdx].pclMethod[0][0]);
  }
  else
  {
     strcpy(pclFileName,&rgData[ilIdx].pclData[0]);
     strcpy(pclMethod,&rgData[ilIdx].pclMethod[0]);
  }

  strcpy(pclTable,"");
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  pclTmpPtr = strstr(pcgSitaText,"<CONTENT>");
  if (pclTmpPtr != NULL)
  {
     pclTmpPtr = strstr(pclTmpPtr,">");
     pclTmpPtr++;
     strcpy(pclDataList,pclTmpPtr);
     pclTmpPtr = strstr(pclDataList,"</CONTENT>");
     if (pclTmpPtr != NULL)
        *pclTmpPtr = '\0';
  }
  CheckCDATA(pclDataList);

  dbg(TRACE,"%s GET CONFIG FOR METHOD <%s>",pclFunc,pclMethod);
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"selection",CFG_STRING,pclSelection);
  if (ilRC != RC_SUCCESS)
     strcpy(pclSelection,"TELEX,2");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"FDI");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"8450");
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
  dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
  dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
  dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
  ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                       pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,NETOUT_NO_ACK);
  return ilRC;
} /* End of HandleSitaSections */


static int HandleTableData(char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleTableData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  int ilI;
  int ilFound;
  char pclMethod[1024];
  char pclTable[16];
  int ilIdx;
  char pclOrig[128];
  char pclUrno[16];
  char pclSelection[2048];
  char pclFieldList[2048];
  char pclDataList[512000];
  char pclActDataList[512000];
  int ilSendToAction;
  int ilSendBroadcast;
  char pclTmpBuf[1024];
  char pclModId[16];
  int ilModId;
  char pclPriority[16];
  int ilPriority;
  char pclCommand[16];

  ilFound = FALSE;
  for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
  {
     if (strcmp(pcpSection,&rgXml.rlLine[ilI].pclName[0]) == 0)
     {
        strcpy(pclMethod,&rgXml.rlLine[ilI].pclMethod[igIntfIdx][0]);
        ilFound = TRUE;
     }
  }
  ilRC = GetData(pclMethod,0,pcpSection);
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Table Name specified",pclFunc);
     return RC_FAIL;
  }
  strcpy(pclOrig,"");
  ilIdx = GetXmlLineIdx("HDR",pcgMessageOrigin,NULL);
  if (ilIdx >= 0)
  {
     if (rgXml.rlLine[ilIdx].ilRcvFlag == 1)
     {
        strcpy(pclOrig,&rgXml.rlLine[ilIdx].pclData[0]);
     }
  }
  ilRC = GetNextUrno();
  sprintf(pclUrno,"%d",igLastUrno);
  strcpy(pclFieldList,"URNO,HOPO,USEC,CDAT");
  sprintf(pclDataList,"%s,'%s','%s/%s','%s'",pclUrno,pcgHomeAp,mod_name,pclOrig,pcgCurrentTime);
  for (ilI = 0; ilI < igNoData; ilI++)
  {
     strcat(pclFieldList,",");
     strcat(pclFieldList,&rgData[ilI].pclField[0]);
     strcat(pclDataList,",'");
     strcat(pclDataList,&rgData[ilI].pclData[0]);
     strcat(pclDataList,"'");
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
  {
     sprintf(pclSqlBuf,"INSERT INTO %s FIELDS(%s) VALUES(%s)",pclTable,pclFieldList,pclDataList);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     if (ilRCdb == DB_SUCCESS)
        commit_work();
     else
     {
        dbg(TRACE,"%s Error inserting into %s",pclFunc,pclTable);
        close_my_cursor(&slCursor);
        return ilRC;
     }
     close_my_cursor(&slCursor);
     ilSendToAction = TRUE;
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"send_to_action",CFG_STRING,pclTmpBuf);
     if (ilRC == RC_SUCCESS)
        if (strcmp(pclTmpBuf,"NO") == 0)
           ilSendToAction = FALSE;
     ilSendBroadcast = TRUE;
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"send_broadcast",CFG_STRING,pclTmpBuf);
     if (ilRC == RC_SUCCESS)
        if (strcmp(pclTmpBuf,"NO") == 0)
           ilSendBroadcast = FALSE;
     strcpy(pclActDataList,"");
     for (ilI = 0; ilI < strlen(pclDataList); ilI++)
        if (strncmp(&pclDataList[ilI],"'",1) != 0)
           strncat(pclActDataList,&pclDataList[ilI],1);
     ilRC = TriggerBchdlAction("IRT",pclTable,pclUrno,pclFieldList,pclActDataList,
                               ilSendBroadcast,ilSendToAction);
  }
  else
  {
     ilModId = atoi(pclModId);
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"snd_cmd_I",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"IRT");
     ilRC = iGetConfigEntry(pcgConfigFile,pclMethod,"priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     strcpy(pclSelection,"");
     strcpy(pclActDataList,"");
     for (ilI = 0; ilI < strlen(pclDataList); ilI++)
        if (strncmp(&pclDataList[ilI],"'",1) != 0)
           strncat(pclActDataList,&pclDataList[ilI],1);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclActDataList);
     ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pclFieldList,pclActDataList,"",ilPriority,
                          NETOUT_NO_ACK);
  }

  return ilRC;
} /* End of HandleTableData */


static int RemoveSection(char *pcpData)
{
  int ilRC = RC_SUCCESS;
  int ilLen = 0;
  char pclFunc[] = "RemoveSection:";
  char pclNewData[512000];
  char pclSearchItem[64];
  char *pclTmpPtr;
  char *pclTmpPtr2;
  char *pclTmpPtr3;
  if (igUsingMultiTypeFlag == TRUE)
  {
     pclTmpPtr = strstr(pcpData,"<@>");
     while (pclTmpPtr != NULL)
     {
        if (pclTmpPtr != NULL)
        {
           ilLen = pclTmpPtr-pcpData;
           strncpy(pclNewData,pcpData,ilLen);
           pclNewData[ilLen] = '\0';
           pclTmpPtr = strstr(pclTmpPtr,"</@>");
           if (pclTmpPtr != NULL)
           {
              pclTmpPtr += 4;
              pclTmpPtr2 = strstr(pclTmpPtr,"<");
              if (pclTmpPtr2 != NULL)
              {
                 pclTmpPtr3 = strstr(pclTmpPtr,"\n");
                 if (pclTmpPtr3 != NULL)
                 {
                    if (pclTmpPtr3 < pclTmpPtr2)
                    {
                       pclTmpPtr = pclTmpPtr3 + 1;
                    }
                 }
              }
              strcat(pclNewData,pclTmpPtr);
              strcpy(pcpData,pclNewData);
           }
           else
           {
              strcpy(pcpData,"");
           }
        }
        else
        {
           strcpy(pcpData,"");
        }
        pclTmpPtr = strstr(pcpData,"<@>");
     } /* end while */
  }
  else
  {
     memset(pclNewData,0x00,512000);
     sprintf(pclSearchItem,"<%s>",pcgMultipleSection);
     pclTmpPtr = strstr(pcpData,pclSearchItem);
     if (pclTmpPtr != NULL)
     {
        strncpy(pclNewData,pcpData,pclTmpPtr-pcpData);
        sprintf(pclSearchItem,"</%s>",pcgMultipleSection);
        pclTmpPtr = strstr(pclTmpPtr,pclSearchItem);
        if (pclTmpPtr != NULL)
        {
           pclTmpPtr = strstr(pclTmpPtr,"\n");
           pclTmpPtr++;
           strcat(pclNewData,pclTmpPtr);
           strcpy(pcpData,pclNewData);
        }
        else
        {
           strcpy(pcpData,"");
        }
     }
     else
     {
        strcpy(pcpData,"");
     }
  }

  return ilRC;
} /* End of RemoveSection */


static int HandleTableOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields,
                          char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleTableOut:";
  int ilI;
  int ilFound;
  char pclTmpBuf[1024];
  char pclTableSections[1024];
  char pclTableNames[1024];
  int ilNoSec;
  char pclSection[64];
  int ilNoEle;
  char pclSecFieldList[2048];
  char pclFieldList[2048];
  char pclDataList[512000];
  char pclOldDataList[512000];
  int ilNoFlds;
  char pclFld[8];
  char pclDat[4048];
  int ilItemNo;
  int ilDatCnt;
  int ilIdx;
  int ilNoIntf;
  char pclCurIntf[64];
  char pclMsgType[64];
  int ilMtIdx;
  char pclAllowedActions[64];
  int ilCount;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TABLE_SECTIONS",CFG_STRING,pclTableSections);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Table Section(s) defined",pclFunc);
     return RC_FAIL;
  }
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TABLE_NAMES",CFG_STRING,pclTableNames);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No Table Name(s) defined",pclFunc);
     return RC_FAIL;
  }
  ilFound = FALSE;
  ilNoSec = GetNoOfElements(pclTableNames,',');
  for (ilI = 1; ilI <= ilNoSec && ilFound == FALSE; ilI++)
  {
     get_real_item(pclTmpBuf,pclTableNames,ilI);
     if (strcmp(pcpTable,pclTmpBuf) == 0)
     {
        get_real_item(pclSection,pclTableSections,ilI);
        ilFound = TRUE;
     }
  }
  if (ilFound == FALSE)
  {
     dbg(TRACE,"%s Table <%s> not defined",pclFunc,pcpTable);
     return RC_FAIL;
  }
  ilRC = BuildHeader(pcpCommand);
  strcpy(pclSecFieldList,"");
  ilNoEle = 0;
  ilRC = GetFieldList(pclSection,pclSecFieldList,&ilNoEle);
  if (strlen(pclSecFieldList) > 0)
     pclSecFieldList[strlen(pclSecFieldList)-1] = '\0';
  if (ilNoEle == 0)
     return RC_FAIL;
  strcpy(pclFieldList,"");
  strcpy(pclDataList,"");
  strcpy(pclOldDataList,"");
  ilNoEle = 0;
  ilNoFlds = GetNoOfElements(pclSecFieldList,',');
  for (ilI = 1; ilI <= ilNoFlds; ilI++)
  {
     get_real_item(pclFld,pclSecFieldList,ilI);
     ilItemNo = get_item_no(pcpFields,pclFld,5) + 1;
     if (ilItemNo > 0)
     {
        strcat(pclFieldList,pclFld);
        strcat(pclFieldList,",");
        get_real_item(pclDat,pcpNewData,ilItemNo);
        strcat(pclDataList,pclDat);
        strcat(pclDataList,",");
        get_real_item(pclDat,pcpOldData,ilItemNo);
        strcat(pclOldDataList,pclDat);
        strcat(pclOldDataList,",");
        ilNoEle++;
     }
  }
  if (strlen(pclFieldList) > 0)
  {
     pclFieldList[strlen(pclFieldList)-1] = '\0';
     pclDataList[strlen(pclDataList)-1] = '\0';
     pclOldDataList[strlen(pclOldDataList)-1] = '\0';
  }
  ilDatCnt = 0;
  ilRC = StoreData(pclFieldList,pclDataList,pclOldDataList,ilNoEle,pclSection,&ilDatCnt);
  if (ilRC != RC_SUCCESS || ilDatCnt == 0)
     return ilRC;
  ilIdx = GetXmlLineIdx("STR_BGN",pclSection,NULL);
  if (ilIdx >= 0)
  {
     ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
     for (ilI = 0; ilI < ilNoIntf; ilI++)
     {
        strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilI][0]);
        strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilI][0]);
        ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
        if (ilMtIdx >= 0)
           strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);
        dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
            pclFunc,pclCurIntf,pclSection,pclMsgType);
        ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclSection,CFG_STRING,pclAllowedActions);
        if (ilRC != RC_SUCCESS)
        {
           dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
        }
        else
        {
           if (strstr(pclAllowedActions,pcpCommand) == NULL)
           {
              dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pcpCommand);
           }
           else
           {
              /*ilRC = ShowData();*/
              ilRC = MarkOutput();
              /*ilRC = ShowAll();*/
              ilRC = BuildOutput(ilI,&ilCount,pclSection,"A",pclCurIntf,"C");
           }
        }
     }
  }

  return ilRC;
} /* End of HandleTableOut */


static int CheckIfDepUpd(char *pcpFields, char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckIfDepUpd:";
  char pclDepFldLst[] = "STOD,ETDI,OFBL,AIRB,PSTD,GTD1,GTD2,DES3,DES4,FTYP";
  int ilNoItems;
  int ilI;
  int ilItemNo;
  char pclFld[8];
  char pclNewDat[4000];
  char pclOldDat[4000];

  ilRC = RC_FAIL;
  ilNoItems = GetNoOfElements(pclDepFldLst,',');
  for (ilI = 1; ilI <= ilNoItems && ilRC == RC_FAIL; ilI++)
  {
     get_real_item(pclFld,pclDepFldLst,ilI);
     ilItemNo = get_item_no(pcpFields,pclFld,5) + 1;
     if (ilItemNo > 0)
     {
        get_real_item(pclNewDat,pcpNewData,ilItemNo);
        TrimRight(pclNewDat);
        get_real_item(pclOldDat,pcpOldData,ilItemNo);
        TrimRight(pclOldDat);
        if (strcmp(pclNewDat,pclOldDat) != 0)
        {
           dbg(DEBUG,"%s Field <%s> was updated, so it must be departure part",pclFunc,pclFld);
           ilRC = RC_SUCCESS;
        }
     }
  }
  if (ilRC == RC_FAIL)
     dbg(DEBUG,"%s None of the Fields <%s> was updated, so it must be arrival part",pclFunc,pclDepFldLst);

  return ilRC;
} /* End of CheckIfDepUpd */


static int ChangeCharFromTo(char *pcpData,char *pcpFrom,char *pcpTo)
{
  int ilRC = RC_SUCCESS;
  int i;
  char pclFunc[]="ChangeCharFromTo:";
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
        for (i=0; pcpFrom[i] != 0x00 ; i++ )
        {
           if (pcpFrom[i] == *pclData)
              *pclData = pcpTo[i];
        }
        pclData++;
     }
  }

  return RC_SUCCESS;
} /* End of ChangeCharFromTo */


static int HandleSitaOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields, char *pcpNewData, char *pcpOldData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleSitaOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8192];
  char pclSelection[1024];
  char pclFieldList[1024];
  char pclSections[1024];
  char pclCurSec[32];
  int ilNoSec;
  int ilI;
  char pclTlxText1[8192] = "";
  char pclTlxText2[8192] = "";
  char pclTlxText[8192] = "";
  char pclTlxSere[16] = "";
  int ilDatCnt;
  int ilIdx;
  int ilNoIntf;
  int ilJ;
  char pclCurIntf[32];
  char pclMsgType[32];
  int ilMtIdx;
  char pclAllowedActions[32];
  int ilCount;
  char pclFileName[32];
  char *pclTmpPtr;
  int ilLen;
  int ilSITA;

  strcpy(pclFieldList,"TXT1,TXT2,SERE");
  sprintf(pclSelection,"WHERE URNO = %s",pcpUrno);
  sprintf(pclSqlBuf,"SELECT %s FROM TLXTAB %s",pclFieldList,pclSelection);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     dbg(TRACE,"%s TLX Record with URNO = <%s> not found",pclFunc,pcpUrno);
     return ilRC;
  }
  BuildItemBuffer(pclDataBuf,"",3,",");
  get_real_item(pclTlxText1,pclDataBuf,1);
  get_real_item(pclTlxText2,pclDataBuf,2);
  get_real_item(pclTlxSere,pclDataBuf,3);
  pclTmpPtr = strstr(pclTlxText1,"ZCZC");
  if (pclTmpPtr == NULL)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","SITA_SECTIONS",CFG_STRING,pclSections);
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s No Section(s) for SITA defined",pclFunc);
        return RC_FAIL;
     }
     ilSITA = TRUE;
  }
  else
  {
     ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","AFTN_SECTIONS",CFG_STRING,pclSections);
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s No Section(s) for AFTN defined",pclFunc);
        return RC_FAIL;
     }
     strcpy(pclTlxText,pclTmpPtr);
     strcpy(pclTlxText1,pclTlxText);
     ilSITA = FALSE;
  }
  ChangeCharFromTo(pclTlxText1,pcgServerChars,pcgClientChars);
  if (strlen(pclTlxText2) > 0)
     ChangeCharFromTo(pclTlxText2,pcgServerChars,pcgClientChars);
  strcpy(pclTlxText,pclTlxText1);
  if (strlen(pclTlxText2) > 0)
  {
     if (pclTlxText[strlen(pclTlxText)-1] != '\n')
        strcat(pclTlxText,"\n");
     strcat(pclTlxText,pclTlxText2);
  }
  if (ilSITA == TRUE)
  {
     ilLen = strlen(pclTlxText);
     while (ilLen > 0 &&
            (pclTlxText[ilLen-1] == '\n' || pclTlxText[ilLen-1] == 0x03 || pclTlxText[ilLen-1] == 0x0d))
     {
        pclTlxText[ilLen-1] = '\0';
        ilLen = strlen(pclTlxText);
     }
     /*strcat(pclTlxText,"\n\003");*/
     pclTmpPtr = strstr(pclTlxText,",");
     while (pclTmpPtr != NULL)
     {
        *pclTmpPtr = '.';
        pclTmpPtr = strstr(pclTmpPtr,",");
     }
  }
  else
  {
     strcpy(pclTlxText1,pclTlxText);
     pclTmpPtr = strstr(pclTlxText1,"ZCZC");
     pclTmpPtr = strstr(pclTmpPtr,"\n");
     if (pclTmpPtr != NULL)
     {
        pclTmpPtr++;
        strcpy(pclTlxText,pclTmpPtr);
     }
     ilLen = strlen(pclTlxText);
     while (ilLen > 0 &&
            (pclTlxText[ilLen-1] == 0x0a || pclTlxText[ilLen-1] == 0x0d))
     {
        pclTlxText[ilLen-1] = '\0';
        ilLen = strlen(pclTlxText);
     }
     strcpy(pclTlxText1,pclTlxText);
     memset(pclTlxText,0x00,8100);
     strcpy(pclTlxText,"\001");
     pclTmpPtr = strstr(pclTlxText1,"SVC");
     if (pclTmpPtr == NULL)
        strcat(pclTlxText,pclTlxText1);
     else
     {
        strncat(pclTlxText,pclTlxText1,pclTmpPtr-pclTlxText1);
        strcat(pclTlxText,"\002");
        strcat(pclTlxText,pclTmpPtr);
     }
     strcat(pclTlxText,"\015\n\t\003");
  }
  ilRC = BuildHeader(pcpCommand);
  ilNoSec = GetNoOfElements(pclSections,',');
  for (ilI = 1; ilI <= ilNoSec; ilI++)
  {
     get_real_item(pclCurSec,pclSections,ilI);
     if (*pclTlxSere == 'S')
     {
        sprintf(pclFileName,"%s.snd",pcpUrno);
        ilDatCnt = 0;
        ilRC = StoreData("FILE_NAME",pclFileName,"",1,pclCurSec,&ilDatCnt);
        ilRC = StoreData("CONTENT",pclTlxText,"",1,pclCurSec,&ilDatCnt);
        if (ilRC != RC_SUCCESS || ilDatCnt == 0)
           return ilRC;
        ilIdx = GetXmlLineIdx("STR_BGN",pclCurSec,NULL);
        if (ilIdx >= 0)
        {
           ilNoIntf = rgXml.rlLine[ilIdx].ilNoMethod;
           for (ilJ = 0; ilJ < ilNoIntf; ilJ++)
           {
              strcpy(pclCurIntf,&rgXml.rlLine[ilIdx].pclMethod[ilJ][0]);
              strcpy(pclMsgType,&rgXml.rlLine[ilIdx].pclFieldA[ilJ][0]);
              ilMtIdx = GetXmlLineIdx("HDR",pcgMessageType,NULL);
              if (ilMtIdx >= 0)
                 strcpy(&rgXml.rlLine[ilMtIdx].pclData[0],pclMsgType);
              dbg(DEBUG,"%s Build Data for Interface <%s> and Section <%s>, Message Type = <%s>",
                  pclFunc,pclCurIntf,pclCurSec,pclMsgType);
              ilRC = iGetConfigEntry(pcgConfigFile,pclCurIntf,pclCurSec,CFG_STRING,pclAllowedActions);
              if (ilRC != RC_SUCCESS)
              {
                 dbg(TRACE,"%s Action not allowed for this Interface",pclFunc);
              }
              else
              {
                 if (strstr(pclAllowedActions,pcpCommand) == NULL)
                 {
                    dbg(TRACE,"%s Action = <%s> not allowed for this Interface",pclFunc,pcpCommand);
                 }
                 else
                 {
                    /*ilRC = ShowData();*/
                    ilRC = MarkOutput();
                    /*ilRC = ShowAll();*/
                    ilRC = BuildOutput(ilJ,&ilCount,pclCurSec,"A",pclCurIntf,"C");
                 }
              }
           }
        }
     }
     else
     {
        dbg(TRACE,"%s Don't send Output, Telex was not send, SERE = <%s>",pclFunc,pclTlxSere);
     }
  }

  return ilRC;
} /* End of HandleSitaOut */


static int HardCodedVDGS(char *pcpCommand, char *pcpFields, char *pcpNewData, char *pcpOldData, char *pcpSec)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HardCodedVDGS:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[1024];
  char pclDataBuf[1024];
  int ilItemNo;
  char pclAdid[16];
  char pclNewLand[16];
  char pclOldLand[16];
  char pclNewTifd[16];
  char pclOldTifd[16];
  char pclNewTifa[16];
  char pclOldTifa[16];
  char pclTmpTime[16];
  char pclNewStoa[16];
  int ilIdx;
  char pclNewOnbl[16];
  char pclOldOnbl[16];
  char pclNewOfbl[16];
  char pclOldOfbl[16];
  char pclNewPsta[16];
  char pclOldPsta[16];
  char pclUrno[16];

  dbg(DEBUG,"%s Cmd = <%s>",pclFunc,pcpCommand);
  dbg(DEBUG,"%s Fld = <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s New = <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old = <%s>",pclFunc,pcpOldData);
  dbg(DEBUG,"%s Sec = <%s>",pclFunc,pcpSec);
  ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
  if (ilItemNo <= 0)
  {
     dbg(TRACE,"%s No ADID received ==> No Check done",pclFunc);
     return RC_SUCCESS;
  }
  get_real_item(pclAdid,pcpNewData,ilItemNo);
  if (*pclAdid == 'A')
  {
     if (strcmp(pcpCommand,"PDE") == 0)
     {
        dbg(TRACE,"%s PDE Command for Arrival Flight received ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     ilItemNo = get_item_no(pcpFields,"LAND",5) + 1;
     if (ilItemNo <= 0)
     {
        ilItemNo = get_item_no(pcpFields,"PSTA",5) + 1;
        if (ilItemNo <= 0)
        {
           dbg(TRACE,"%s No LAND received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        else
        {
           get_real_item(pclNewPsta,pcpNewData,ilItemNo);
           get_real_item(pclOldPsta,pcpOldData,ilItemNo);
           TrimRight(pclNewPsta);
           TrimRight(pclOldPsta);
           if (*pclNewPsta != ' ' && *pclOldPsta == ' ')
           {
              ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
              if (ilItemNo <= 0)
              {
                 dbg(TRACE,"%s No LAND received ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
              get_real_item(pclUrno,pcpNewData,ilItemNo);
              sprintf(pclSqlBuf,"SELECT LAND FROM AFTTAB WHERE URNO = %s",pclUrno);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              close_my_cursor(&slCursor);
              if (ilRCdb == DB_SUCCESS)
              {
                 TrimRight(pclDataBuf);
                 if (*pclDataBuf != ' ')
                 {
                    dbg(TRACE,"%s PSTA was assigned after LAND ==> Send Message",pclFunc);
                    return RC_SUCCESS;
                 }
                 else
                 {
                    dbg(TRACE,"%s No LAND received ==> Don't send Message",pclFunc);
                    return RC_FAIL;
                 }
              }
              else
              {
                 dbg(TRACE,"%s No LAND received ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No LAND received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
     }
     get_real_item(pclNewLand,pcpNewData,ilItemNo);
     get_real_item(pclOldLand,pcpOldData,ilItemNo);

     if (strcmp(pcgInstallation,"BLR") != 0)
     {
     if (strcmp(pclNewLand,pclOldLand) == 0)
     {
        dbg(TRACE,"%s LAND has not changed ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
  }
  }
  else if (*pclAdid == 'D')
  {
     if (strcmp(pcpCommand,"PDE") != 0)
     {
        ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
        if (ilItemNo <= 0)
        {
           dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        get_real_item(pclNewTifd,pcpNewData,ilItemNo);
        get_real_item(pclOldTifd,pcpOldData,ilItemNo);

        if (strcmp(pcgInstallation,"BLR") != 0)
        {
        if (strcmp(pclNewTifd,pclOldTifd) == 0)
        {
           dbg(TRACE,"%s TIFD has not changed ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        }

        ilItemNo = get_item_no(pcpFields,"OFBL",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewOfbl,pcpNewData,ilItemNo);
           get_real_item(pclOldOfbl,pcpOldData,ilItemNo);
           if (strcmp(pclNewOfbl,pclOldOfbl) != 0)
           {
              dbg(TRACE,"%s OFBL has changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        strcpy(pclTmpTime,pcgCurrentTime);
        MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
        if (strcmp(pclNewTifd,pcgCurrentTime) < 0 || strcmp(pclNewTifd,pclTmpTime) > 0)
        {
           dbg(TRACE,"%s TIFD (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifd,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
           return RC_FAIL;
        }
     }
     else
     {
        ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewTifd,pcpNewData,ilItemNo);
           if (strcmp(pclNewTifd,pcgCurrentTime) < 0)
           {
              dbg(TRACE,"%s TIFD (%s) has passed current time (%s) ==> Don't send Message",
                  pclFunc,pclNewTifd,pcgCurrentTime);
              return RC_FAIL;
           }
        }
     }
  }
  else
  {
     if (strstr(pcpSec,"ARR") != NULL)
     {
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           get_real_item(pclNewTifa,pcpNewData,ilItemNo);
           get_real_item(pclOldTifa,pcpOldData,ilItemNo);

           if (strcmp(pcgInstallation,"BLR") != 0)
             {
           if (strcmp(pclNewTifa,pclOldTifa) == 0)
           {
              dbg(TRACE,"%s TIFA has not changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           }

           ilItemNo = get_item_no(pcpFields,"ONBL",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewOnbl,pcpNewData,ilItemNo);
              get_real_item(pclOldOnbl,pcpOldData,ilItemNo);
              if (strcmp(pclNewOnbl,pclOldOnbl) != 0)
              {
                 dbg(TRACE,"%s ONBL has changed ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           strcpy(pclTmpTime,pcgCurrentTime);
           MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
           if (strcmp(pclNewTifa,pcgCurrentTime) < 0 || strcmp(pclNewTifa,pclTmpTime) > 0)
           {
              dbg(TRACE,"%s TIFA (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifa,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
              return RC_FAIL;
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewTifa,pcpNewData,ilItemNo);
              if (strcmp(pclNewTifa,pcgCurrentTime) < 0)
              {
                 dbg(TRACE,"%s TIFA (%s) has passed current time (%s) ==> Don't send Message",
                     pclFunc,pclNewTifa,pcgCurrentTime);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No TIFA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        ilItemNo = get_item_no(pcpFields,"STOA",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewStoa,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("DAT","LAND",pcpSec);
           if (ilIdx >= 0)
           {
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclNewStoa);
              rgXml.rlLine[ilIdx].ilRcvFlag = 1;
           }
        }
/* Arrival */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"A");
        ilItemNo = get_item_no(pcpFields,"STOA",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
        }
     }
     else
     {
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           get_real_item(pclNewTifd,pcpNewData,ilItemNo);
           get_real_item(pclOldTifd,pcpOldData,ilItemNo);

           if (strcmp(pcgInstallation,"BLR") != 0)
           {
           if (strcmp(pclNewTifd,pclOldTifd) == 0)
           {
              dbg(TRACE,"%s TIFD has not changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
             }

           ilItemNo = get_item_no(pcpFields,"OFBL",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewOfbl,pcpNewData,ilItemNo);
              get_real_item(pclOldOfbl,pcpOldData,ilItemNo);
              if (strcmp(pclNewOfbl,pclOldOfbl) != 0)
              {
                 dbg(TRACE,"%s OFBL has changed ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           strcpy(pclTmpTime,pcgCurrentTime);
           MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
           if (strcmp(pclNewTifd,pcgCurrentTime) < 0 || strcmp(pclNewTifd,pclTmpTime) > 0)
           {
              dbg(TRACE,"%s TIFD (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifd,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
              return RC_FAIL;
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewTifd,pcpNewData,ilItemNo);
              if (strcmp(pclNewTifd,pcgCurrentTime) < 0)
              {
                 dbg(TRACE,"%s TIFD (%s) has passed current time (%s) ==> Don't send Message",
                     pclFunc,pclNewTifd,pcgCurrentTime);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
/* Departure */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"D");
        ilItemNo = get_item_no(pcpFields,"STOD",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
        }
     }
  }

  return ilRC;
} /* End of HardCodedVDGS */


static int HandleAckOut(char *pcpUrno, char *pcpCommand, char *pcpTable, char *pcpFields,
                          char *pcpNewData, char *pcpOldData)

{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleAckOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8192];
  char pclFieldList[1024];
  int ilNoFlds;
  char pclNewFieldList[1024];
  char pclNewDataList[8192];
  char pclOldDataList[8192];
  char pclTmpFld[16];
  char pclTmpData[128];
  int ilI;
  char pclCcaUrno[16];

  sprintf(pclSqlBuf,"SELECT URNO FROM RARTAB WHERE RURN = %s",pcpUrno);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb != DB_SUCCESS)
  {
     dbg(TRACE,"%s Flight with URNO = <%s> not marked in RARTAB for pending ACK",pclFunc,pcpUrno);
     return ilRC;
  }

  sprintf(pclSqlBuf,"DELETE FROM RARTAB WHERE RURN = %s",pcpUrno);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  commit_work();
  close_my_cursor(&slCursor);

  strcpy(pclNewFieldList,"URNO,ADID,");
  sprintf(pclNewDataList,"%s,%s,",pcpUrno,pcpNewData);
  strcpy(pclOldDataList,pclNewDataList);
  if (strcmp(pcpNewData,"A") == 0)
  {
     strcpy(pclFieldList,"PSTA,PABS,PAES,PABA,PAEA");
     strcat(pclFieldList,",BLT1,B1BS,B1ES,B1BA,B1EA,BLT2,B2BS,B2ES,B2BA,B2EA");
     strcat(pclFieldList,",GTA1,GA1B,GA1E,GA1X,GA1Y,GTA2,GA2B,GA2E,GA2X,GA2Y");
     ilNoFlds = 25;
  }
  else
  {
     strcpy(pclFieldList,"PSTD,PDBS,PDES,PDBA,PDEA");
     strcat(pclFieldList,",GTD1,GD1B,GD1E,GD1X,GD1Y,GTD2,GD2B,GD2E,GD2X,GD2Y");
     ilNoFlds = 15;
  }
  sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB WHERE URNO = %s",pclFieldList,pcpUrno);
  slCursor = 0;
  slFkt = START;
  dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb == DB_SUCCESS)
  {
     BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
     get_real_item(pclTmpData,pclDataBuf,1);
     TrimRight(pclTmpData);
     if (*pclTmpData != ' ')
     {
        for (ilI = 1; ilI <= 5; ilI++)
        {
           get_real_item(pclTmpFld,pclFieldList,ilI);
           get_real_item(pclTmpData,pclDataBuf,ilI);
           TrimRight(pclTmpData);
           strcat(pclNewFieldList,pclTmpFld);
           strcat(pclNewFieldList,",");
           strcat(pclNewDataList,pclTmpData);
           strcat(pclNewDataList,",");
           strcat(pclOldDataList," ,");
        }
     }
     get_real_item(pclTmpData,pclDataBuf,6);
     TrimRight(pclTmpData);
     if (*pclTmpData != ' ')
     {
        for (ilI = 6; ilI <= 10; ilI++)
        {
           get_real_item(pclTmpFld,pclFieldList,ilI);
           get_real_item(pclTmpData,pclDataBuf,ilI);
           TrimRight(pclTmpData);
           strcat(pclNewFieldList,pclTmpFld);
           strcat(pclNewFieldList,",");
           strcat(pclNewDataList,pclTmpData);
           strcat(pclNewDataList,",");
           strcat(pclOldDataList," ,");
        }
     }
     get_real_item(pclTmpData,pclDataBuf,11);
     TrimRight(pclTmpData);
     if (*pclTmpData != ' ')
     {
        for (ilI = 11; ilI <= 15; ilI++)
        {
           get_real_item(pclTmpFld,pclFieldList,ilI);
           get_real_item(pclTmpData,pclDataBuf,ilI);
           TrimRight(pclTmpData);
           strcat(pclNewFieldList,pclTmpFld);
           strcat(pclNewFieldList,",");
           strcat(pclNewDataList,pclTmpData);
           strcat(pclNewDataList,",");
           strcat(pclOldDataList," ,");
        }
     }
     if (ilNoFlds > 15)
     {
        get_real_item(pclTmpData,pclDataBuf,16);
        TrimRight(pclTmpData);
        if (*pclTmpData != ' ')
        {
           for (ilI = 16; ilI <= 20; ilI++)
           {
              get_real_item(pclTmpFld,pclFieldList,ilI);
              get_real_item(pclTmpData,pclDataBuf,ilI);
              TrimRight(pclTmpData);
              strcat(pclNewFieldList,pclTmpFld);
              strcat(pclNewFieldList,",");
              strcat(pclNewDataList,pclTmpData);
              strcat(pclNewDataList,",");
              strcat(pclOldDataList," ,");
           }
        }
        get_real_item(pclTmpData,pclDataBuf,21);
        TrimRight(pclTmpData);
        if (*pclTmpData != ' ')
        {
           for (ilI = 21; ilI <= 25; ilI++)
           {
              get_real_item(pclTmpFld,pclFieldList,ilI);
              get_real_item(pclTmpData,pclDataBuf,ilI);
              TrimRight(pclTmpData);
              strcat(pclNewFieldList,pclTmpFld);
              strcat(pclNewFieldList,",");
              strcat(pclNewDataList,pclTmpData);
              strcat(pclNewDataList,",");
              strcat(pclOldDataList," ,");
           }
        }
     }
     if (strlen(pclNewFieldList) > 0)
     {
        pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
        pclNewDataList[strlen(pclNewDataList)-1] = '\0';
        pclOldDataList[strlen(pclOldDataList)-1] = '\0';
        ilRC = HandleFlightOut(pcpUrno,"UFR",pcpTable,pclNewFieldList,pclNewDataList,pclOldDataList);
     }
  }

  if (strcmp(pcpNewData,"D") == 0)
  {
     strcpy(pclFieldList,"URNO,CKIC,CTYP,CKIT,CKBS,CKES,CKBA,CKEA,DISP,REMA,FLNU");
     sprintf(pclSqlBuf,"SELECT %s FROM CCATAB WHERE FLNU = %s AND CKIC <> ' '",pclFieldList,pcpUrno);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     while (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",11,",");
        strcpy(pclNewFieldList,"URNO,");
        get_real_item(pclCcaUrno,pclDataBuf,1);
        TrimRight(pclCcaUrno);
        sprintf(pclNewDataList,"%s,",pclCcaUrno);
        strcpy(pclOldDataList," ,");
        for (ilI = 2; ilI <= 11; ilI++)
        {
           get_real_item(pclTmpFld,pclFieldList,ilI);
           get_real_item(pclTmpData,pclDataBuf,ilI);
           TrimRight(pclTmpData);
           strcat(pclNewFieldList,pclTmpFld);
           strcat(pclNewFieldList,",");
           strcat(pclNewDataList,pclTmpData);
           strcat(pclNewDataList,",");
           strcat(pclOldDataList," ,");
        }
        if (strlen(pclNewFieldList) > 0)
        {
           pclNewFieldList[strlen(pclNewFieldList)-1] = '\0';
           pclNewDataList[strlen(pclNewDataList)-1] = '\0';
           pclOldDataList[strlen(pclOldDataList)-1] = '\0';
           for (ilI = 0; ilI < igCurXmlLines; ilI++)
           {
              strcpy(&rgXml.rlLine[ilI].pclData[0],"");
              strcpy(&rgXml.rlLine[ilI].pclOldData[0],"");
              rgXml.rlLine[ilI].ilRcvFlag = 0;
              rgXml.rlLine[ilI].ilPrcFlag = 0;
           }
           strcpy(pcgAdid,"");
           ilRC = HandleCounterOut(pclCcaUrno,"IFR","CCATAB",pclNewFieldList,pclNewDataList,pclOldDataList);
        }
        slFkt = NEXT;
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     }
     close_my_cursor(&slCursor);
  }

  return ilRC;
} /* End of HandleAckOut */


static int CheckIfActualData(char *pcpType, char *pcpFields, char *pcpData, int ipNoEle)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "CheckIfActualData:";
  int ilI;
  char pclFldNam1[8];
  char pclFldNam2[8];
  char pclFldDat[4000];
  int ilItemNo;
  char pclAdid[8];
  char pclTime1[16];
  char pclTime2[16];
  char pclPeriod[16];

  strcpy(pclTime1,"");
  strcpy(pclTime2,"");
  strcpy(pclFldNam1,"");
  strcpy(pclFldNam2,"");
  if (*pcpType == 'F')
  {
     ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
     get_real_item(pclAdid,pcpData,ilItemNo);
     if (*pclAdid == 'A')
        strcpy(pclFldNam1,"STOA");
     else if (*pclAdid == 'D')
        strcpy(pclFldNam1,"STOD");
     else
     {
        strcpy(pclFldNam1,"STOA");
        strcpy(pclFldNam2,"STOD");
     }
  }
  else
     strcpy(pclFldNam1,"CKBS");

  if (*pclFldNam1 != '\0')
  {
     ilItemNo = get_item_no(pcpFields,pclFldNam1,5) + 1;
     if (ilItemNo > 0)
        get_real_item(pclTime1,pcpData,ilItemNo);
  }
  if (*pclFldNam2 != '\0')
  {
     ilItemNo = get_item_no(pcpFields,pclFldNam2,5) + 1;
     if (ilItemNo > 0)
        get_real_item(pclTime2,pcpData,ilItemNo);
  }

  strcpy(pclPeriod,pcgCurrentTime);
  MyAddSecondsToCEDATime(pclPeriod,igActualPeriod*60*60,1);
  if (*pclTime1 != '\0')
  {
     if (strlen(pclTime1) == 12)
        strcat(pclTime1,"00");
     if (strcmp(pclTime1,pclPeriod) > 0)
     {
        dbg(TRACE,"%s Field <%s> = <%s> is out of actual data window <%s>",
            pclFunc,pclFldNam1,pclTime1,pclPeriod);
        dbg(TRACE,"%s ==> No Data is sent",pclFunc);
        ilRC = RC_FAIL;
     }
  }
  if (*pclTime2 != '\0')
  {
     if (strlen(pclTime2) == 12)
        strcat(pclTime2,"00");
     if (strcmp(pclTime2,pclPeriod) > 0)
     {
        dbg(TRACE,"%s Field <%s> = <%s> is out of actual data window <%s>",
            pclFunc,pclFldNam2,pclTime2,pclPeriod);
        dbg(TRACE,"%s ==> No Data is sent",pclFunc);
        ilRC = RC_FAIL;
     }
  }
  if (ilRC == RC_FAIL)
  {
     for (ilI = 1; ilI <= ipNoEle; ilI++)
     {
        get_real_item(pclFldNam1,pcpFields,ilI);
        get_real_item(pclFldDat,pcpData,ilI);
        dbg(TRACE,"%s Found %s = <%s>",pclFunc,pclFldNam1,pclFldDat);
     }
  }

  return ilRC;
} /* End of CheckIfActualData */


static int GetTrailingBlanks(char *pcpBlanks, char *pcpData, int ipLen)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetTrailingBlanks:";
  int ilI;
  int ilDiv;
  int ilAdd;

  strcpy(pcpBlanks,"");
  if (strlen(pcpData) <= 1)
     return ilRC;

  ilDiv = strlen(pcpData) / ipLen;
  if (ilDiv * ipLen < strlen(pcpData))
     ilAdd = (ilDiv + 1) * ipLen - strlen(pcpData);
  else
     ilAdd = 0;
  for (ilI = 0; ilI < ilAdd; ilI++)
     strcat(pcpBlanks," ");

  return ilRC;
} /* End of GetTrailingBlanks */


static int HandleEqiUsage(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleEqiUsage:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  FILE *fp;
  char pclLine[2100];
  char pclEqiName[128];
  char pclEqiStat[128];
  char pclEqiDatim[128];
  char pclEqiDate[128];
  char pclEqiTime[128];
  char pclYear[8];
  char pclMonth[8];
  char pclDay[8];
  char pclTmpBuf[1024];
  char *pclTmpPtr;
  char pclFieldList[1024];
  char pclDataList[1024];
  int ilTimeDiffLocalUtc;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_DIFF_LOCAL_UTC",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     ilTimeDiffLocalUtc = atoi(pclTmpBuf);
  else
     ilTimeDiffLocalUtc = 7*60;

  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     get_real_item(pclEqiName,pclLine,1);
     get_real_item(pclEqiStat,pclLine,2);
     get_real_item(pclEqiDatim,pclLine,3);

     strcpy(pclEqiName,&pclEqiName[1]);
     pclEqiName[strlen(pclEqiName)-1] = '\0';
     TrimLeftRight(pclEqiName);
     pclTmpPtr = strstr(pclEqiName,"-STATUS");
     if (pclTmpPtr != NULL)
        *pclTmpPtr = '\0';

     strcpy(pclTmpBuf,&pclEqiStat[1]);
     pclTmpBuf[strlen(pclTmpBuf)-1] = '\0';
     TrimLeftRight(pclTmpBuf);
     pclTmpPtr = strstr(pclTmpBuf,"STATUS-ON");
     if (pclTmpPtr != NULL)
        strcpy(pclEqiStat,"START");
     else
     {
        pclTmpPtr = strstr(pclTmpBuf,"STATUS-OFF");
        if (pclTmpPtr != NULL)
           strcpy(pclEqiStat,"STOP");
        else
           strcpy(pclEqiStat,"INVALID");
     }

     strcpy(pclDay,pclEqiDatim);
     pclDay[2] = '\0';
     strcpy(pclMonth,&pclEqiDatim[3]);
     pclMonth[3] = '\0';
     strcpy(pclYear,&pclEqiDatim[7]);
     pclYear[4] = '\0';
     strcpy(pclEqiDatim,"");
     if (strcmp(pclMonth,"Jan") == 0)
        sprintf(pclEqiDate,"%s01%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Feb") == 0)
        sprintf(pclEqiDate,"%s02%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Mar") == 0)
        sprintf(pclEqiDate,"%s03%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Apr") == 0)
        sprintf(pclEqiDate,"%s04%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"May") == 0)
        sprintf(pclEqiDate,"%s05%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Jun") == 0)
        sprintf(pclEqiDate,"%s06%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Jul") == 0)
        sprintf(pclEqiDate,"%s07%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Aug") == 0)
        sprintf(pclEqiDate,"%s08%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Sep") == 0)
        sprintf(pclEqiDate,"%s09%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Oct") == 0)
        sprintf(pclEqiDate,"%s10%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Nov") == 0)
        sprintf(pclEqiDate,"%s11%s",pclYear,pclDay);
     else if (strcmp(pclMonth,"Dec") == 0)
        sprintf(pclEqiDate,"%s12%s",pclYear,pclDay);
     else
        strcpy(pclEqiDatim,"INVALID");

     strcpy(pclEqiTime,&pclEqiDatim[12]);
     pclEqiTime[2] = '\0';
     strcat(pclEqiTime,&pclEqiDatim[15]);
     pclEqiTime[4] = '\0';
     strcat(pclEqiTime,&pclEqiDatim[18]);
     pclEqiTime[6] = '\0';

     if (strcmp(pclEqiDatim,"INVALID") != 0)
     {
        sprintf(pclEqiDatim,"%s%s",pclEqiDate,pclEqiTime);
        ilRC = MyAddSecondsToCEDATime(pclEqiDatim,ilTimeDiffLocalUtc*60*(-1),1);
        if (ilRC != RC_SUCCESS)
           strcpy(pclEqiDatim,"INVALID");
     }

     if (strcmp(pclEqiStat,"INVALID") == 0)
        dbg(TRACE,"%s Invalid Status ==> Event ignored.",pclFunc);
     else if (strcmp(pclEqiDatim,"INVALID") == 0)
        dbg(TRACE,"%s Invalid Date/Time ==> Event ignored.",pclFunc);
     else
     {
        strcpy(pclFieldList,"URNO,NAME,TIME,OPST");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','%s'",
                igLastUrno,pclEqiName,pclEqiDatim,pclEqiStat);
        sprintf(pclSqlBuf,"INSERT INTO EQITAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error inserting into EQITAB",pclFunc);
        }
        close_my_cursor(&slCursor);
     }
  }

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);
  return ilRC;
} /* End of HandleEqiUsage */


static int HandleEqiUsageWAW()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleEqiUsageWAW:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclDataBuf[8000];
  char pclFile[128];
  FILE *fp;
  char pclLine[2100];
  char pclEqiName[128];
  char pclEqiStat[128];
  char pclEqiDatim[128];
  char pclEqiDate[128];
  char pclEqiTime[128];
  char pclEqiItem1[128];
  char pclEqiItem2[128];
  char pclYear[8];
  char pclMonth[8];
  char pclDay[8];
  char pclTmpBuf[1024];
  char *pclTmpPtr;
  char pclFieldList[1024];
  char pclDataList[1024];
  int ilTimeDiffLocalUtc;
  int ilTDIS = -1;
  int ilTDIW = -1;

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","WAW_BAS_DATA_FILE",CFG_STRING,pclFile);
  if ((fp = (FILE *)fopen(pclFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pclFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","TIME_DIFF_LOCAL_UTC",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
     ilTimeDiffLocalUtc = atoi(pclTmpBuf);
  else
     ilTimeDiffLocalUtc = 7*60;

  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     get_real_item(pclEqiName,pclLine,1);
     get_real_item(pclEqiStat,pclLine,2);
     get_real_item(pclEqiDatim,pclLine,3);

     strcpy(pclEqiName,&pclEqiName[1]);
     pclEqiName[strlen(pclEqiName)-1] = '\0';
     TrimLeftRight(pclEqiName);
     strcpy(pclEqiItem1,pclEqiName);
     pclTmpPtr = strstr(pclEqiName,"-STATUS");
     if (pclTmpPtr != NULL)
        *pclTmpPtr = '\0';

     strcpy(pclTmpBuf,&pclEqiStat[1]);
     pclTmpBuf[strlen(pclTmpBuf)-1] = '\0';
     TrimLeftRight(pclTmpBuf);
     strcpy(pclEqiItem2,pclTmpBuf);
     pclTmpPtr = strstr(pclTmpBuf,"ON");
     if (pclTmpPtr != NULL)
        strcpy(pclEqiStat,"START");
     else
     {
        pclTmpPtr = strstr(pclTmpBuf,"OFF");
        if (pclTmpPtr != NULL)
           strcpy(pclEqiStat,"STOP");
     }

     strcpy(pclEqiDatim,&pclEqiDatim[1]);
     pclEqiDatim[strlen(pclEqiDatim)-1] = '\0';
     TrimLeftRight(pclEqiDatim);
     GetDataItem(pclEqiDate,pclEqiDatim,1,' ',""," ");
     GetDataItem(pclEqiTime,pclEqiDatim,2,' ',""," ");
     sprintf(pclEqiDatim,"%s %s",pclEqiDate,pclEqiTime);

     strcpy(pclEqiDate,&pclEqiDatim[0]);
     pclEqiDate[4] = '\0';
     strcat(pclEqiDate,&pclEqiDatim[5]);
     pclEqiDate[6] = '\0';
     strcat(pclEqiDate,&pclEqiDatim[8]);
     pclEqiDate[8] = '\0';

     strcpy(pclEqiTime,&pclEqiDatim[11]);
     pclEqiTime[2] = '\0';
     strcat(pclEqiTime,&pclEqiDatim[14]);
     pclEqiTime[4] = '\0';
     strcat(pclEqiTime,&pclEqiDatim[17]);
     pclEqiTime[6] = '\0';

     if (strcmp(pclEqiDatim,"INVALID") != 0)
     {
        sprintf(pclEqiDatim,"%s%s",pclEqiDate,pclEqiTime);
        ilTimeDiffLocalUtc = GetTimeDiffUtc(pclEqiDatim,&ilTDIS,&ilTDIW);
        ilRC = MyAddSecondsToCEDATime(pclEqiDatim,ilTimeDiffLocalUtc*60*(-1),1);
        if (ilRC != RC_SUCCESS)
           strcpy(pclEqiDatim,"INVALID");
     }

     if (strcmp(pclEqiStat,"START") != 0 && strcmp(pclEqiStat,"STOP") != 0)
        dbg(TRACE,"%s Invalid Status ==> Event ignored.",pclFunc);
     else if (strcmp(pclEqiDatim,"INVALID") == 0)
        dbg(TRACE,"%s Invalid Date/Time ==> Event ignored.",pclFunc);
     else
     {
        strcpy(pclFieldList,"URNO,NAME,TIME,OPST");
        ilRC = GetNextUrno();
        sprintf(pclDataList,"%d,'%s','%s','%s'",
                igLastUrno,pclEqiName,pclEqiDatim,pclEqiStat);
        sprintf(pclSqlBuf,"INSERT INTO EQITAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           dbg(TRACE,"%s Error inserting into EQITAB",pclFunc);
        }
        close_my_cursor(&slCursor);
     }

     TimeToStr(pcgCurrentTime,time(NULL));
     strcpy(pclFieldList,"URNO,TIME,TEXT,STAT,ADDR,HOPO,CDAT,USEC");
     ilRC = GetNextUrno();
     sprintf(pclDataList,"%d,%s,%s,%s,BASBMS,%s,%s,exchdl",
             igLastUrno,pclEqiDatim,pclEqiItem1,pclEqiItem2,pcgHomeAp,pcgCurrentTime);
     dbg(TRACE,"%s Send <IRT> for <BASTAB> to <506>",pclFunc);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     ilRC = SendCedaEvent(506,0,"BASBMS","CEDA",pcgTwStart,pcgTwEnd,
                          "IRT","BASTAB","",pclFieldList,pclDataList,"",
                          3,NETOUT_NO_ACK);
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","RENAME_FILE",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf,"YES") == 0)
  {
     strcpy(pclTmpBuf,pclFile);
     strcat(pclTmpBuf,"_");
     strcat(pclTmpBuf,pcgCurrentTime);
     ilRC = rename(pclFile,pclTmpBuf);
     dbg(TRACE,"%s File renamed to <%s>",pclFunc,pclTmpBuf);
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","CMD_TO_EQIHDL",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     dbg(TRACE,"%s Send <%s> to <9820>",pclFunc,pclTmpBuf);
     ilRC = SendCedaEvent(9820,0,"BASBMS","CEDA",pcgTwStart,pcgTwEnd,
                          pclTmpBuf,"","","","","",3,NETOUT_NO_ACK);
  }

  fclose(fp);
  return ilRC;
} /* End of HandleEqiUsageWAW */


static int HardCodedVDGSWAW(char *pcpCommand, char *pcpFields, char *pcpNewData,
                            char *pcpOldData, char *pcpSec)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HardCodedVDGSWAW:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[1024];
  char pclDataBuf[1024];
  int ilItemNo;
  int ilItemNoTifa;
  int ilItemNoTifd;
  int ilItemNoLand;
  int ilItemNoPsta;
  int ilItemNoPstd;
  int ilItemNoAct5;
  int ilItemNoFtyp;
  int ilIdx;
  char pclAdid[16] = "";
  char pclNewFtyp[16] = "";
  char pclOldFtyp[16] = "";
  char pclTmpTime[16] = "";
  char pclUrno[16] = "";
  char pclAftPsta[16] = "";
  char pclAftLand[16] = "";
  char pclAftPstd[16] = "";
  char pclAftOfbl[16] = "";
  char pclAftOnbl[16] = "";
  char pclAftStoa[16] = "";
  char pclAftStod[16] = "";

  dbg(DEBUG,"%s Cmd = <%s>",pclFunc,pcpCommand);
  dbg(DEBUG,"%s Fld = <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s New = <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old = <%s>",pclFunc,pcpOldData);
  dbg(DEBUG,"%s Sec = <%s>",pclFunc,pcpSec);
  ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
  if (ilItemNo <= 0)
  {
     dbg(TRACE,"%s No ADID received ==> No Check done",pclFunc);
     return RC_SUCCESS;
  }
  get_real_item(pclAdid,pcpNewData,ilItemNo);
  if (*pclAdid == 'A')
  {
     if (strcmp(pcpCommand,"PDE") == 0)
     {
        dbg(TRACE,"%s PDE Command for Arrival Flight received ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
     if (ilItemNo <= 0)
     {
        dbg(TRACE,"%s No URNO received ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     ilItemNoLand = get_item_no(pcpFields,"LAND",5) + 1;
     ilItemNoPsta = get_item_no(pcpFields,"PSTA",5) + 1;
     ilItemNoAct5 = get_item_no(pcpFields,"ACT5",5) + 1;
     ilItemNoFtyp = get_item_no(pcpFields,"FTYP",5) + 1;
     get_real_item(pclUrno,pcpNewData,ilItemNo);
     sprintf(pclSqlBuf,"SELECT PSTA,LAND FROM AFTTAB WHERE URNO = %s",pclUrno);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",2,",");
        get_real_item(pclAftPsta,pclDataBuf,1);
        TrimRight(pclAftPsta);
        get_real_item(pclAftLand,pclDataBuf,2);
        TrimRight(pclAftLand);
        if (*pclAftPsta == ' ' && ilItemNoPsta <= 0)
        {
           dbg(TRACE,"%s Flight has no PSTA ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        if (*pclAftLand == ' ' && ilItemNoLand <= 0)
        {
           dbg(TRACE,"%s Flight has no LAND ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        if (ilItemNoLand <= 0 && ilItemNoPsta <= 0 && ilItemNoAct5 <= 0 && ilItemNoFtyp <= 0)
        {
           dbg(TRACE,"%s No LAND, PSTA or ACT5 received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        else
        {
           if (ilItemNoLand > 0)
              dbg(TRACE,"%s LAND has changed ==> Send Message",pclFunc);
           else if (ilItemNoPsta > 0)
              dbg(TRACE,"%s PSTA has changed after LAND ==> Send Message",pclFunc);
           else if (ilItemNoAct5 > 0)
              dbg(TRACE,"%s ACT5 has changed after LAND ==> Send Message",pclFunc);
           else
           {
              get_real_item(pclNewFtyp,pcpNewData,ilItemNoFtyp);
              get_real_item(pclOldFtyp,pcpOldData,ilItemNoFtyp);
              if (*pclNewFtyp == 'X' || *pclOldFtyp == 'X')
                 dbg(TRACE,"%s FTYP=CXX was set or removed after LAND ==> Send Message",pclFunc);
              else
              {
                 dbg(TRACE,"%s Flight is not cancelled ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           return RC_SUCCESS;
        }
     }
     else
     {
        dbg(TRACE,"%s Flight not found ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
  }
  else if (*pclAdid == 'D')
  {
     ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
     if (ilItemNo <= 0)
     {
        dbg(TRACE,"%s No URNO received ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     get_real_item(pclUrno,pcpNewData,ilItemNo);
     sprintf(pclSqlBuf,"SELECT PSTD,OFBL FROM AFTTAB WHERE URNO = %s",pclUrno);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",2,",");
        get_real_item(pclAftPstd,pclDataBuf,1);
        TrimRight(pclAftPstd);
        get_real_item(pclAftOfbl,pclDataBuf,2);
        TrimRight(pclAftOfbl);
        if (*pclAftOfbl != ' ')
        {
           dbg(TRACE,"%s Flight has OFBL ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
     }
     else
     {
        dbg(TRACE,"%s Flight not found ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     if (strcmp(pcpCommand,"PDE") != 0)
     {
        if (strstr(pcgUrnoListPdeDep,pclUrno) == NULL)
        {
           dbg(TRACE,"%s There was no PDE command for that Flight ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        ilItemNoTifd = get_item_no(pcpFields,"TIFD",5) + 1;
        ilItemNoPstd = get_item_no(pcpFields,"PSTD",5) + 1;
        ilItemNoAct5 = get_item_no(pcpFields,"ACT5",5) + 1;
        ilItemNoFtyp = get_item_no(pcpFields,"FTYP",5) + 1;
        if (*pclAftPstd == ' ' && ilItemNoPstd <= 0)
        {
           dbg(TRACE,"%s Flight has no PSTD ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        if (ilItemNoTifd <= 0 && ilItemNoPstd <= 0 && ilItemNoAct5 <= 0 && ilItemNoFtyp <= 0)
        {
           dbg(TRACE,"%s No TIFD, PSTD or ACT5 received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        else
        {
           if (ilItemNoTifd > 0)
              dbg(TRACE,"%s TIFD has changed after PDE ==> Send Message",pclFunc);
           else if (ilItemNoPstd > 0)
              dbg(TRACE,"%s PSTD has changed after PDE ==> Send Message",pclFunc);
           else if (ilItemNoAct5 > 0)
              dbg(TRACE,"%s ACT5 has changed after PDE ==> Send Message",pclFunc);
           else
           {
              get_real_item(pclNewFtyp,pcpNewData,ilItemNoFtyp);
              get_real_item(pclOldFtyp,pcpOldData,ilItemNoFtyp);
              if (*pclNewFtyp == 'X' || *pclOldFtyp == 'X')
                 dbg(TRACE,"%s FTYP=CXX was set or removed after PDE ==> Send Message",pclFunc);
              else
              {
                 dbg(TRACE,"%s Flight is not cancelled ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           return RC_SUCCESS;
        }
     }
     else
     {
        strcat(pclUrno,"#");
        if (strstr(pcgUrnoListPdeDep,pclUrno) == NULL)
        {
           strcat(pcgUrnoListPdeDep,pclUrno);
           if (strlen(pcgUrnoListPdeDep) > 9000)
           {
              strcpy(pcgUrnoListPdeDep,&pcgUrnoListPdeDep[1000]);
           }
        }
        if (*pclAftPstd == ' ')
        {
           dbg(TRACE,"%s Flight has no PSTD ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
     }
  }
  else
  {
     if (strstr(pcpSec,"ARR") != NULL)
     {
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo <= 0)
        {
           dbg(TRACE,"%s No URNO received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        get_real_item(pclUrno,pcpNewData,ilItemNo);
        if (strcmp(pcpCommand,"DFR") != 0)
        {
           sprintf(pclSqlBuf,"SELECT PSTA,ONBL,STOA FROM AFTTAB WHERE URNO = %s",pclUrno);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf,"",3,",");
              get_real_item(pclAftPsta,pclDataBuf,1);
              TrimRight(pclAftPsta);
              get_real_item(pclAftOnbl,pclDataBuf,2);
              TrimRight(pclAftOnbl);
              get_real_item(pclAftStoa,pclDataBuf,3);
              TrimRight(pclAftStoa);
              if (*pclAftOnbl != ' ')
              {
                 dbg(TRACE,"%s Flight has ONBL ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s Flight not found ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           if (strstr(pcgUrnoListPdeTowA,pclUrno) == NULL)
           {
              dbg(TRACE,"%s There was no PDE command for that Flight ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           ilItemNoTifa = get_item_no(pcpFields,"TIFA",5) + 1;
           ilItemNoPsta = get_item_no(pcpFields,"PSTA",5) + 1;
           ilItemNoAct5 = get_item_no(pcpFields,"ACT5",5) + 1;
           ilItemNoFtyp = get_item_no(pcpFields,"FTYP",5) + 1;
           if (*pclAftPsta == ' ' && ilItemNoPsta <= 0)
           {
              dbg(TRACE,"%s Flight has no PSTA ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           if (ilItemNoTifa <= 0 && ilItemNoPsta <= 0 && ilItemNoAct5 <= 0 && ilItemNoFtyp <= 0)
           {
              dbg(TRACE,"%s No TIFA, PSTA or ACT5 received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           else
           {
              if (ilItemNoTifa > 0)
                 dbg(TRACE,"%s TIFA has changed after PDE ==> Send Message",pclFunc);
              else if (ilItemNoPsta > 0)
                 dbg(TRACE,"%s PSTA has changed after PDE ==> Send Message",pclFunc);
              else if (ilItemNoAct5 > 0)
                 dbg(TRACE,"%s ACT5 has changed after PDE ==> Send Message",pclFunc);
              else
              {
                 get_real_item(pclNewFtyp,pcpNewData,ilItemNoFtyp);
                 get_real_item(pclOldFtyp,pcpOldData,ilItemNoFtyp);
                 if (*pclNewFtyp == 'X' || *pclOldFtyp == 'X')
                    dbg(TRACE,"%s FTYP=CXX was set or removed after PDE ==> Send Message",pclFunc);
                 else
                 {
                    dbg(TRACE,"%s Flight is not cancelled ==> Don't send Message",pclFunc);
                    return RC_FAIL;
                 }
              }
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           strcat(pclUrno,"#");
           if (strstr(pcgUrnoListPdeTowA,pclUrno) == NULL)
           {
              strcat(pcgUrnoListPdeTowA,pclUrno);
              if (strlen(pcgUrnoListPdeTowA) > 9000)
              {
                 strcpy(pcgUrnoListPdeTowA,&pcgUrnoListPdeTowA[1000]);
              }
           }
           if (*pclAftPsta == ' ')
           {
              dbg(TRACE,"%s Flight has no PSTA ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
/* Arrival */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"A");
        ilItemNo = get_item_no(pcpFields,"STOA",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
           else
           {
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclAftStoa);
           }
        }
     }
     else
     {
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo <= 0)
        {
           dbg(TRACE,"%s No URNO received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        get_real_item(pclUrno,pcpNewData,ilItemNo);
        if (strcmp(pcpCommand,"DFR") != 0)
        {
           sprintf(pclSqlBuf,"SELECT PSTD,OFBL,STOD FROM AFTTAB WHERE URNO = %s",pclUrno);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf,"",3,",");
              get_real_item(pclAftPstd,pclDataBuf,1);
              TrimRight(pclAftPstd);
              get_real_item(pclAftOfbl,pclDataBuf,2);
              TrimRight(pclAftOfbl);
              get_real_item(pclAftStod,pclDataBuf,3);
              TrimRight(pclAftStod);
              if (*pclAftOfbl != ' ')
              {
                 dbg(TRACE,"%s Flight has OFBL ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s Flight not found ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           if (strstr(pcgUrnoListPdeTowD,pclUrno) == NULL)
           {
              dbg(TRACE,"%s There was no PDE command for that Flight ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           ilItemNoTifd = get_item_no(pcpFields,"TIFD",5) + 1;
           ilItemNoPstd = get_item_no(pcpFields,"PSTD",5) + 1;
           ilItemNoAct5 = get_item_no(pcpFields,"ACT5",5) + 1;
           ilItemNoFtyp = get_item_no(pcpFields,"FTYP",5) + 1;
           if (*pclAftPstd == ' ' && ilItemNoPstd <= 0)
           {
              dbg(TRACE,"%s Flight has no PSTD ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           if (ilItemNoTifd <= 0 && ilItemNoPstd <= 0 && ilItemNoAct5 <= 0 && ilItemNoFtyp <= 0)
           {
              dbg(TRACE,"%s No TIFD, PSTD or ACT5 received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           else
           {
              if (ilItemNoTifd > 0)
                 dbg(TRACE,"%s TIFD has changed after PDE ==> Send Message",pclFunc);
              else if (ilItemNoPstd > 0)
                 dbg(TRACE,"%s PSTD has changed after PDE ==> Send Message",pclFunc);
              else if (ilItemNoAct5 > 0)
                 dbg(TRACE,"%s ACT5 has changed after PDE ==> Send Message",pclFunc);
              else
              {
                 get_real_item(pclNewFtyp,pcpNewData,ilItemNoFtyp);
                 get_real_item(pclOldFtyp,pcpOldData,ilItemNoFtyp);
                 if (*pclNewFtyp == 'X' || *pclOldFtyp == 'X')
                    dbg(TRACE,"%s FTYP=CXX was set or removed after PDE ==> Send Message",pclFunc);
                 else
                 {
                    dbg(TRACE,"%s Flight is not cancelled ==> Don't send Message",pclFunc);
                    return RC_FAIL;
                 }
              }
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           strcat(pclUrno,"#");
           if (strstr(pcgUrnoListPdeTowD,pclUrno) == NULL)
           {
              strcat(pcgUrnoListPdeTowD,pclUrno);
              if (strlen(pcgUrnoListPdeTowD) > 9000)
              {
                 strcpy(pcgUrnoListPdeTowD,&pcgUrnoListPdeTowD[1000]);
              }
           }
           if (*pclAftPstd == ' ')
           {
              dbg(TRACE,"%s Flight has no PSTD ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
/* Departure */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"D");
        ilItemNo = get_item_no(pcpFields,"STOD",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
           else
           {
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclAftStod);
           }
        }
     }
  }

  return ilRC;
} /* End of HardCodedVDGSWAW */


static int GetTimeDiffUtc(char *pcpDate, int *ipTDIS, int *ipTDIW)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetTimeDiffUtc:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[1024];
  char pclDataBuf[1024];
  char pclTmpBuf[1024];
  int ilDiff = 0;

  if (*ipTDIS == -1)
  {
     sprintf(pclSqlBuf,"SELECT TDIS,TDIW FROM APTTAB WHERE APC3 = '%s'",pcgHomeAp);
     slCursor = 0;
     slFkt = START;
     /*dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);*/
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",2,",");
        get_real_item(pclTmpBuf,pclDataBuf,1);
        *ipTDIS = atoi(pclTmpBuf);
        get_real_item(pclTmpBuf,pclDataBuf,2);
        *ipTDIW = atoi(pclTmpBuf);
     }
  }
  sprintf(pclSqlBuf,"SELECT SEAS FROM SEATAB WHERE VPFR <= '%s' AND VPTO >= '%s'",pcpDate,pcpDate);
  slCursor = 0;
  slFkt = START;
  /*dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);*/
  ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
  close_my_cursor(&slCursor);
  if (ilRCdb == DB_SUCCESS)
  {
     if (*pclDataBuf == 'S')
        ilDiff = *ipTDIS;
     else
        ilDiff = *ipTDIW;
  }

  return ilDiff;
} /* End of GetTimeDiffUtc */


static int HandleArsData(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleArsData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclSelection[2048];
  char pclDataBuf[8000];
  char pclFieldList[2048];
  char pclDataList[2048];
  FILE *fp;
  char pclLine[2100];
  char pclFLNO[16];
  char pclADID[16];
  char pclSTAD[16];
  char pclAftUrno[16];
  char pclData[16];
  REC_DESC rlRecDesc;
  char pclDataLine[1024];
  int ilArrayPtr;
  char pclTmpBuf[1024];

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  strcpy(pclFieldList,"URNO,HOPO,FLNU,DSSN,TYPE,STYP,SSTP,SSST,VALU");
  strcpy(pclDataList,":VURNO,:VHOPO,:VFLNU,:VDSSN,:VTYPE,:VSTYP,:VSSTP,:VSSST,:VVALU");

  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     get_real_item(pclFLNO,pclLine,1);
     get_real_item(pclADID,pclLine,2);
     get_real_item(pclSTAD,pclLine,3);
     CheckFlightNo(pclFLNO);
     if (strlen(pclSTAD) == 12)
        strcat(pclSTAD,"00");
     if (*pclADID == 'A')
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s'",pclFLNO,pclSTAD);
     else
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s'",pclFLNO,pclSTAD);
     sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclAftUrno);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        sprintf(pclSqlBuf,"DELETE FROM LOATAB WHERE FLNU = %s",pclAftUrno);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        sql_if(slFkt,&slCursor,pclSqlBuf,pclAftUrno);
        commit_work();
        close_my_cursor(&slCursor);

        InitRecordDescriptor(&rlRecDesc,1024,0,FALSE);
        memset(pclDataBuf,0x00,4096);
        ilArrayPtr = 0;
        get_real_item(pclData,pclLine,4);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T, ,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,5);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T, ,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,6);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T,R,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,7);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T,R,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,8);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T,T,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,9);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,PAX,T,T,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,10);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,I,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,11);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,I,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,12);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,F,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,13);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,F,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,14);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,E,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,15);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,E,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,16);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,T,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,17);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,T,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,18);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,O,DOM,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,19);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,EXP,T,O,INT,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,20);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,LOA,C, , ,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");
        get_real_item(pclData,pclLine,21);
        TrimRight(pclData);
        ilRC = GetNextUrno();
        sprintf(pclDataLine,"%d,%s,%s,USR,LOA,M, , ,%s",
                igLastUrno,pcgHomeAp,pclAftUrno,pclData);
        StrgPutStrg(pclDataBuf,&ilArrayPtr,pclDataLine,0,-1,"\n");

        ilArrayPtr--;
        pclDataBuf[ilArrayPtr] = '\0';
        sprintf(pclSqlBuf,"INSERT INTO LOATAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>\n%s",pclFunc,pclSqlBuf,pclDataBuf);
        ilRCdb = SqlIfArray(slFkt,&slCursor,pclSqlBuf,pclDataBuf,0,&rlRecDesc);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        else
        {
           rollback();
           dbg(TRACE,"%s Error inserting into LOATAB",pclFunc);
        }
        close_my_cursor(&slCursor);
     }
     else
     {
        dbg(TRACE,"%s Flight <%s><%s><%s> not found",pclFunc,pclFLNO,pclADID,pclSTAD);
     }
  }

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);
  return ilRC;
} /* End of HandleArsData */


static int GetFieldConfig(char *pcpFieldList, char *pcpFieldListUpd, char *pcpFieldLenList, char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetFieldConfig:";
  char pclFieldList[4096];
  char pclFieldListUpd[4096];
  char pclFieldLen[1024];
  int ilI;
  int ilNoFld;
  char pclFldNam[16];
  char pclFldNamUpd[16];
  int ilFldLen;
  char pclTmpBuf[128];
  int ilIdx;
  int ilVia;

  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,pcpFieldList,CFG_STRING,pclFieldList);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Field List Definition for <%s> missing",pclFunc,pcpFieldList);
     return ilRC;
  }
  strcpy(pclFieldListUpd,pclFieldList);
  if (strlen(pcpFieldListUpd) > 0)
  {
     ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,pcpFieldListUpd,CFG_STRING,pclFieldListUpd);
     if (ilRC != RC_SUCCESS)
     {
        dbg(TRACE,"%s Field List Definition for Updates missing",pclFunc);
        return ilRC;
     }
  }
  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,pcpFieldLenList,CFG_STRING,pclFieldLen);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s Field Length Definition missing",pclFunc);
     return ilRC;
  }
  ilNoFld = GetNoOfElements(pclFieldList,',');
  ilI = GetNoOfElements(pclFieldLen,',');
  if (ilNoFld != ilI)
  {
     dbg(TRACE,"%s No of Items (%d) in Field List Definition different from No of Items (%d) Field Length Definition",
         pclFunc,ilNoFld,ilI);
     return ilRC;
  }
  ilIdx = 0;
  ilVia = 0;
  igNoFileData = 0;
  for (ilI = 1; ilI <= ilNoFld; ilI++)
  {
     get_real_item(pclFldNam,pclFieldList,ilI);
     get_real_item(pclFldNamUpd,pclFieldListUpd,ilI);
     get_real_item(pclTmpBuf,pclFieldLen,ilI);
     ilFldLen = atoi(pclTmpBuf);
     if (ilVia > 0 && strstr(pclFldNam,"VIX") == NULL)
     {
        strcpy(&rgFileData[igNoFileData].pclFldNam[0],"VIAL");
        strcpy(&rgFileData[igNoFileData].pclFldNamUpd[0],"VIAL");
        rgFileData[igNoFileData].ilBgn = ilIdx;
        rgFileData[igNoFileData].ilLen = ilVia;
        igNoFileData++;
        ilIdx += ilVia;
        ilVia = 0;
     }
     if (strstr(pclFldNam,"FILL") != NULL)
     {
        ilIdx += ilFldLen;
     }
     else if (strstr(pclFldNam,"VIX") != NULL)
     {
        ilVia += ilFldLen;
     }
     else
     {
        strcpy(&rgFileData[igNoFileData].pclFldNam[0],pclFldNam);
        strcpy(&rgFileData[igNoFileData].pclFldNamUpd[0],pclFldNamUpd);
        rgFileData[igNoFileData].ilBgn = ilIdx;
        rgFileData[igNoFileData].ilLen = ilFldLen;
        igNoFileData++;
        ilIdx += ilFldLen;
     }
  }

  return ilRC;
} /* End of GetFieldConfig */


static int HandleSeasonData(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleSeasonData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[1024];
  char pclDataBuf[1024];
  int ilI;
  int ilJ;
  FILE *fp;
  char pclLine[4096];
  char pclFldDat[4096];
  char pclFieldList[1024];
  char pclDataList[4096];
  char pclSelection[1024];
  char pclAdid[16];
  char pclVafr[16];
  char pclVato[16];
  char pclDoop[16];
  char pclFreq[16];
  char pclStod[16];
  char pclNewStod[16];
  char pclStoa[16];
  char pclSeas[16];
  char pclNewStoa[16];
  char pclNewDate[16];
  char pclTable[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  char pclVial[1024];
  char pclNewVial[1024];
  char pclDelFlag[1024];
  int ilLineCnt;
  char pclTmpBuf[1024];

  ilRC = GetFieldConfig("FLD_LST_SEASON","","FLD_LEN_SEASON","EXCHDL");
  if (ilRC != RC_SUCCESS)
     return ilRC;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","DELETE_SEASON",CFG_STRING,pclDelFlag);
  if (ilRC != RC_SUCCESS)
     strcpy(pclDelFlag,"NO");

  ilLineCnt = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) < 10)
     {
        fclose(fp);
        sprintf(pclTmpBuf,"%s.sav",pcpFile);
        ilRC = rename(pcpFile,pclTmpBuf);
        return ilRC;
     }
     ilLineCnt++;
     dbg(DEBUG,"%s Got Line %d: %s",pclFunc,ilLineCnt,pclLine);
     for (ilI = 0; ilI < igNoFileData; ilI++)
     {
        strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
        pclFldDat[rgFileData[ilI].ilLen] = '\0';
        TrimRight(pclFldDat);
        strcpy(&rgFileData[ilI].pclFldDat[0],pclFldDat);
        strcpy(pclSeas,"");
        if (strcmp(&rgFileData[ilI].pclFldNam[0],"ADID") == 0)
           strcpy(pclAdid,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"VAFR") == 0)
           strcpy(pclVafr,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"VATO") == 0)
           strcpy(pclVato,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"DOOP") == 0)
           strcpy(pclDoop,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FREQ") == 0)
           strcpy(pclFreq,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
           strcpy(pclStod,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOA") == 0)
           strcpy(pclStoa,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"VIAL") == 0)
           strcpy(pclVial,pclFldDat);
        else if (strcmp(&rgFileData[ilI].pclFldNam[0],"SEAS") == 0)
           strcpy(pclSeas,pclFldDat);
     }
     if (ilLineCnt == 1 && *pclSeas != '\0')
     {
        if (strcmp(pclDelFlag,"YES") == 0 && igSimulation == FALSE)
        {
           sprintf(pclSqlBuf,"DELETE FROM AFTTAB WHERE SEAS = '%s'",pclSeas);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           commit_work();
           close_my_cursor(&slCursor);
        }
        else
           dbg(DEBUG,"%s Season <%s> will not be deleted",pclFunc,pclSeas);
     }
     strcpy(pclNewStod," ");
     strcpy(pclNewStoa," ");
     if (*pclStod != ' ')
        sprintf(pclNewStod,"%s%s00",pclVafr,pclStod);
     if (*pclStoa != ' ')
        sprintf(pclNewStoa,"%s%s00",pclVafr,pclStoa);
     strcpy(pclNewVial,pclVial);
     if (*pclAdid == 'A')
     {
        if (*pclStod != ' ' && strcmp(pclStod,pclStoa) > 0)
        {
           sprintf(pclNewDate,"%s120000",pclVafr);
           AddSecondsToCEDATime(pclNewDate,24*60*60*(-1),1);
           pclNewDate[8] = '\0';
           sprintf(pclNewStod,"%s%s00",pclNewDate,pclStod);
           sprintf(pclNewStoa,"%s%s00",pclVafr,pclStoa);
        }
     }
     else
     {
        if (*pclStoa != ' ' && strcmp(pclStod,pclStoa) > 0)
        {
           sprintf(pclNewDate,"%s120000",pclVafr);
           AddSecondsToCEDATime(pclNewDate,24*60*60,1);
           pclNewDate[8] = '\0';
           sprintf(pclNewStod,"%s%s00",pclVafr,pclStod);
           sprintf(pclNewStoa,"%s%s00",pclNewDate,pclStoa);
        }
     }
     if (strlen(pclVial) > 1)
     {
        memset(pclNewVial,0x00,1024);
        strcpy(pclNewVial," ");
        for (ilI = 0; ilI < strlen(pclVial); ilI +=3)
        {
           strncat(pclNewVial,&pclVial[ilI],3);
           strcat(pclNewVial,"       ");
           for (ilJ = 0; ilJ < 11; ilJ++)
              strcat(pclNewVial,"          ");
        }
        TrimRight(pclNewVial);
     }
     strcpy(pclFieldList,"");
     strcpy(pclDataList,"");
     for (ilI = 0; ilI < igNoFileData; ilI++)
     {
        if (strcmp(&rgFileData[ilI].pclFldNam[0],"VAFR") != 0  &&
            strcmp(&rgFileData[ilI].pclFldNam[0],"VATO") != 0  &&
            strcmp(&rgFileData[ilI].pclFldNam[0],"DOOP") != 0  &&
            strcmp(&rgFileData[ilI].pclFldNam[0],"FREQ") != 0)
        {
           strcat(pclFieldList,&rgFileData[ilI].pclFldNam[0]);
           strcat(pclFieldList,",");
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
              strcat(pclDataList,pclNewStod);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOA") == 0)
              strcat(pclDataList,pclNewStoa);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"VIAL") == 0)
              strcat(pclDataList,pclNewVial);
           else
              strcat(pclDataList,&rgFileData[ilI].pclFldDat[0]);
           strcat(pclDataList,",");
        }
     }
     pclFieldList[strlen(pclFieldList)-1] = '\0';
     pclDataList[strlen(pclDataList)-1] = '\0';
     sprintf(pclSelection,"WHERE %s,%s,%s,%s,%s",pclVafr,pclVato,pclDoop,pclFreq,pclVafr);
     ilRC = iGetConfigEntry(pcgConfigFile,"SEASON","table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"AFTTAB");
     ilRC = iGetConfigEntry(pcgConfigFile,"SEASON","snd_cmd",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"ISF");
     ilRC = iGetConfigEntry(pcgConfigFile,"SEASON","priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilRC = iGetConfigEntry(pcgConfigFile,"SEASON","mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7805");
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
     if (igSimulation == FALSE)
        ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",ilPriority,
                             NETOUT_NO_ACK);
  }

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);
  return ilRC;
} /* End of HandleSeasonData */


static int HandleDailyData(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleDailyData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  int ilI;
  int ilJ;
  FILE *fp;
  char pclLine[4096];
  int ilFound;
  char pclFldDat[4096];
  char pclUrno[16];
  char pclAdid[16];
  char pclStoa[16];
  char pclStod[16];
  char pclFlno[16];
  char pclFlnoArr[16];
  char pclFlnoDep[16];
  char pclMinArr[16];
  char pclMaxArr[16];
  char pclMinDep[16];
  char pclMaxDep[16];
  char pclFldNam[16];
  char pclTmpBuf[1024];
  char pclSelFieldListArr[2048];
  char pclSelFieldListDep[2048];
  int ilNoFlds;
  int ilItemNo;
  char pclUpdSelection[1024];
  char pclUpdFieldList[2048];
  char pclUpdDataList[8192];
  char pclTable[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  int ilLineCntArr;
  int ilLineCntDep;
  int ilFoundRecsArr;
  int ilFoundRecsDep;
  int ilRejectedRecsArr;
  int ilRejectedRecsDep;
  int ilUpdRecsArr;
  int ilUpdRecsDep;
  int ilInsRecsArr;
  int ilInsRecsDep;
  int ilInsRecsTow;
  int ilDelRecsArr;
  int ilDelRecsDep;
  int ilNoEle;
  int ilTowing;
  int ilMinDiff;
  char pclMinTime[16];

  ilRC = GetFieldConfig("FLD_LST_DAILY","","FLD_LEN_DAILY","EXCHDL");
  if (ilRC != RC_SUCCESS)
     return ilRC;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MIN_DIFF_FAFT",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     ilMinDiff = atoi(pclTmpBuf);
     strcpy(pclMinTime,pcgCurrentTime);
     AddSecondsToCEDATime(pclMinTime,ilMinDiff*60*60,1);
  }
  else
     ilMinDiff = 0;

  strcpy(pclMaxArr,"20000101000000");
  strcpy(pclMinArr,"20301231235959");
  strcpy(pclMaxDep,"20000101000000");
  strcpy(pclMinDep,"20301231235959");
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 10)
     {
        ilFound = 0;
        for (ilI = 0; ilI < igNoFileData && ilFound < 3; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"ADID") == 0)
           {
              strcpy(pclAdid,pclFldDat);
              ilFound++;
           }
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOA") == 0)
           {
              strcpy(pclStoa,pclFldDat);
              ilFound++;
           }
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
           {
              strcpy(pclStod,pclFldDat);
              ilFound++;
           }
        }
        if (ilFound == 3)
        {
           if (*pclAdid == 'A' && ilMinDiff > 0 && strcmp(pclStoa,pclMinTime) < 0)
              dbg(DEBUG,"%s Record rejected because STOA <%s> less than Minimum <%s>",
                  pclFunc,pclStoa,pclMinTime);
           else if (*pclAdid == 'D' && ilMinDiff > 0 && strcmp(pclStod,pclMinTime) < 0)
              dbg(DEBUG,"%s Record rejected because STOD <%s> less than Minimum <%s>",
                  pclFunc,pclStod,pclMinTime);
           else
           {
              if (*pclAdid == 'A')
              {
                 if (strcmp(pclStoa,pclMinArr) < 0)
                    strcpy(pclMinArr,pclStoa);
                 if (strcmp(pclMaxArr,pclStoa) < 0)
                    strcpy(pclMaxArr,pclStoa);
              }
              else
              {
                 if (strcmp(pclStod,pclMinDep) < 0)
                    strcpy(pclMinDep,pclStod);
                 if (strcmp(pclMaxDep,pclStod) < 0)
                    strcpy(pclMaxDep,pclStod);
              }
           }
        }
     }
  }
  fclose(fp);
  pclMinArr[10] = '\0';
  strcat(pclMinArr,"0000");
  pclMaxArr[10] = '\0';
  strcat(pclMaxArr,"5959");
  pclMinDep[10] = '\0';
  strcat(pclMinDep,"0000");
  pclMaxDep[10] = '\0';
  strcat(pclMaxDep,"5959");
  if (strcmp(pclMinArr,"20301231230000") == 0)
  {
     strcpy(pclMinArr,"");
     strcpy(pclMaxArr,"");
  }
  if (strcmp(pclMinDep,"20301231230000") == 0)
  {
     strcpy(pclMinDep,"");
     strcpy(pclMaxDep,"");
  }
  ilRC = GetUrnoList(pclMinArr,pclMaxArr,pclMinDep,pclMaxDep,"AFTTAB","FLT","");

  strcpy(pclSelFieldListArr,"URNO,");
  strcpy(pclSelFieldListDep,"URNO,");
  for (ilI = 0; ilI < igNoFileData; ilI++)
  {
     strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
     if (strcmp(pclFldNam,"FLNOA") == 0)
        strcat(pclSelFieldListArr,"FLNO,");
     else if (strcmp(pclFldNam,"FLNOD") == 0)
        strcat(pclSelFieldListDep,"FLNO,");
     else
     {
        if (strstr(pclFldNam,";") != NULL)
        {
           get_item(1,pclFldNam,pclTmpBuf,0,";","\0","\0");
           strcat(pclSelFieldListArr,pclTmpBuf);
           get_item(2,pclFldNam,pclTmpBuf,0,";","\0","\0");
           strcat(pclSelFieldListDep,pclTmpBuf);
        }
        else
        {
           strcat(pclSelFieldListArr,pclFldNam);
           strcat(pclSelFieldListDep,pclFldNam);
        }
        strcat(pclSelFieldListArr,",");
        strcat(pclSelFieldListDep,",");
     }
  }
  pclSelFieldListArr[strlen(pclSelFieldListArr)-1] = '\0';
  pclSelFieldListDep[strlen(pclSelFieldListDep)-1] = '\0';
  ilNoFlds = GetNoOfElements(pclSelFieldListArr,',');

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilLineCntArr = 0;
  ilLineCntDep = 0;
  ilFoundRecsArr = 0;
  ilFoundRecsDep = 0;
  ilRejectedRecsArr = 0;
  ilRejectedRecsDep = 0;
  ilUpdRecsArr = 0;
  ilUpdRecsDep = 0;
  ilInsRecsArr = 0;
  ilInsRecsDep = 0;
  ilInsRecsTow = 0;
  ilDelRecsArr = 0;
  ilDelRecsDep = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 10)
     {
        dbg(DEBUG,"%s Got Line %d: %s",pclFunc,ilLineCntArr+ilLineCntDep+1,pclLine);
        strcpy(pclAdid," ");
        strcpy(pclStoa," ");
        strcpy(pclStod," ");
        strcpy(pclFlno," ");
        strcpy(pclFlnoArr," ");
        strcpy(pclFlnoDep," ");
        for (ilI = 0; ilI < igNoFileData; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           strcpy(&rgFileData[ilI].pclFldDat[0],pclFldDat);
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"ADID") == 0)
              strcpy(pclAdid,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOA") == 0)
              strcpy(pclStoa,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
              strcpy(pclStod,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNO") == 0)
              strcpy(pclFlno,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNOA") == 0)
              strcpy(pclFlnoArr,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNOD") == 0)
              strcpy(pclFlnoDep,pclFldDat);
        }
        if (*pclAdid == 'A' && ilMinDiff > 0 && strcmp(pclStoa,pclMinTime) < 0)
        {
           ilLineCntArr++;
           ilRejectedRecsArr++;
           dbg(DEBUG,"%s Record rejected because STOA <%s> less than Minimum <%s>",
               pclFunc,pclStoa,pclMinTime);
        }
        else if (*pclAdid == 'D' && ilMinDiff > 0 && strcmp(pclStod,pclMinTime) < 0)
        {
           ilLineCntDep++;
           ilRejectedRecsDep++;
           dbg(DEBUG,"%s Record rejected because STOD <%s> less than Minimum <%s>",
               pclFunc,pclStod,pclMinTime);
        }
        else
        {
           if (*pclAdid == 'A')
           {
              ilLineCntArr++;
              pclStoa[12] = '\0';
              if (*pclFlno == ' ')
                 strcpy(pclFlno,pclFlnoArr);
              sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA LIKE '%s%%' AND ADID = 'A'",pclFlno,pclStoa);
              sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclSelFieldListArr,pclSelection);
           }
           else
           {
              ilLineCntDep++;
              pclStod[12] = '\0';
              if (*pclFlno == ' ')
                 strcpy(pclFlno,pclFlnoDep);
              sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD LIKE '%s%%' AND ADID = 'D'",pclFlno,pclStod);
              sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclSelFieldListDep,pclSelection);
           }
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              if (*pclAdid == 'A')
                 ilFoundRecsArr++;
              else
                 ilFoundRecsDep++;
              BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
              ilItemNo = get_item_no(pclSelFieldListArr,"URNO",5) + 1;
              get_real_item(pclUrno,pclDataBuf,ilItemNo);
              if (*pclAdid == 'A')
                 ilRC = RemoveUrno(pclUrno,"A");
              else
                 ilRC = RemoveUrno(pclUrno,"D");
              strcpy(pclUpdFieldList,"");
              strcpy(pclUpdDataList,"");
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 if (*pclAdid == 'A')
                 {
                    if (strstr(&rgFileData[ilI].pclFldNam[0],";") != NULL)
                       get_item(1,&rgFileData[ilI].pclFldNam[0],pclFldNam,0,";","\0","\0");
                    else
                       strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
                 }
                 else
                 {
                    if (strstr(&rgFileData[ilI].pclFldNam[0],";") != NULL)
                       get_item(2,&rgFileData[ilI].pclFldNam[0],pclFldNam,0,";","\0","\0");
                    else
                       strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
                 }
                 if (strcmp(pclFldNam,"FLNOA") != 0 && strcmp(pclFldNam,"FLNOD") != 0)
                 {
                    ilItemNo = get_item_no(pclSelFieldListArr,pclFldNam,5) + 1;
                    get_real_item(pclFldDat,pclDataBuf,ilItemNo);
                    TrimRight(pclFldDat);
                    if (strcmp(&rgFileData[ilI].pclFldDat[0],pclFldDat) != 0)
                    {
                       if (strcmp(pclFldNam,"VIAL") == 0)
                       {
                          if(strlen(&rgFileData[ilI].pclFldDat[0]) > 3)
                          {
                             strcpy(pclFldNam,"ROUT");
                             ConvertVialToRout(&rgFileData[ilI].pclFldDat[0]);
                          }
                          else if(strlen(&rgFileData[ilI].pclFldDat[0]) == 3)
                             strcpy(pclFldNam,"ROUT");
                       }
                       strcat(pclUpdFieldList,pclFldNam);
                       strcat(pclUpdFieldList,",");
                       strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
                       strcat(pclUpdDataList,",");
                    }
                 }
              }
              if (strlen(pclUpdFieldList) > 0)
              {
                 if (*pclAdid == 'A')
                    ilUpdRecsArr++;
                 else
                    ilUpdRecsDep++;
                 pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
                 pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
                 sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","table",CFG_STRING,pclTable);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclTable,"AFTTAB");
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","snd_cmd_U",CFG_STRING,pclCommand);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclCommand,"UFR");
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","priority",CFG_STRING,pclPriority);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclPriority,"3");
                 ilPriority = atoi(pclPriority);
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","mod_id",CFG_STRING,pclModId);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclModId,"7805");
                 ilModId = atoi(pclModId);
                 dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                 dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
                 dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
                 dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
                 if (igSimulation == FALSE)
                    ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                         pclCommand,pclTable,pclUpdSelection,pclUpdFieldList,
                                         pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
              }
           }
           else
           {
              ilTowing = FALSE;
              if (*pclAdid == 'A')
                 ilInsRecsArr++;
              else
                 ilInsRecsDep++;
              ilRC = GetNextUrno();
              strcpy(pclUpdFieldList,"URNO,");
              sprintf(pclUpdDataList,"%d,",igLastUrno);
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 if (strstr(&rgFileData[ilI].pclFldNam[0],";") != NULL)
                 {
                    if (*pclAdid == 'A')
                       get_item(1,&rgFileData[ilI].pclFldNam[0],pclTmpBuf,0,";","\0","\0");
                    else
                       get_item(2,&rgFileData[ilI].pclFldNam[0],pclTmpBuf,0,";","\0","\0");
                 }
                 else
                    strcpy(pclTmpBuf,&rgFileData[ilI].pclFldNam[0]);
                 if (strcmp(pclTmpBuf,"FLNOA") == 0 && *pclAdid == 'D')
                 { }
                 else if (strcmp(pclTmpBuf,"FLNOD") == 0 && *pclAdid == 'A')
                 { }
                 else
                 {
                    pclTmpBuf[4] = '\0';
                    if (strcmp(pclTmpBuf,"VIAL") == 0)
                    {
                       if(strlen(&rgFileData[ilI].pclFldDat[0]) > 3)
                       {
                          strcpy(pclTmpBuf,"ROUT");
                          ConvertVialToRout(&rgFileData[ilI].pclFldDat[0]);
                       }
                       else if(strlen(&rgFileData[ilI].pclFldDat[0]) == 3)
                          strcpy(pclTmpBuf,"ROUT");
                    }
                    strcat(pclUpdFieldList,pclTmpBuf);
                    strcat(pclUpdFieldList,",");
                    strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
                    strcat(pclUpdDataList,",");
                    if (strcmp(pclTmpBuf,"FTYP") == 0 &&
                        (rgFileData[ilI].pclFldDat[0] == 'T' || rgFileData[ilI].pclFldDat[0] == 'G'))
                       ilTowing = TRUE;
                 }
              }
              if (strlen(pclUpdFieldList) > 0)
              {
                 if (strstr(pclUpdFieldList,"ALC2") == NULL && strstr(pclUpdFieldList,"ALC3") == NULL)
                 {
                    if (pclFlno[2] == ' ')
                    {
                       strcat(pclUpdFieldList,"ALC2,");
                       pclFlno[2] = '\0';
                    }
                    else
                    {
                       strcat(pclUpdFieldList,"ALC3,");
                       pclFlno[3] = '\0';
                    }
                    strcat(pclUpdDataList,pclFlno);
                    strcat(pclUpdDataList,",");
                 }
                 pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
                 pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
                 strcpy(pclUpdSelection,"");
                 if (ilTowing == FALSE)
                 {
                    ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","table",CFG_STRING,pclTable);
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclTable,"AFTTAB");
                    ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","snd_cmd_I",CFG_STRING,pclCommand);
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclCommand,"IFR");
                    ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","priority",CFG_STRING,pclPriority);
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclPriority,"3");
                    ilPriority = atoi(pclPriority);
                    ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","mod_id",CFG_STRING,pclModId);
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclModId,"7805");
                    ilModId = atoi(pclModId);
                    dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
                        pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                    dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
                    dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
                    dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
                    if (igSimulation == FALSE)
                       ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                            pclCommand,pclTable,pclUpdSelection,pclUpdFieldList,
                                            pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
                 }
                 else
                 {
                    ilInsRecsTow++;
                    if (*pclAdid == 'A')
                       ilInsRecsArr--;
                    else
                       ilInsRecsDep--;
                 }
              }
           }
        }
     }
  }

  ilNoEle = GetNoOfElements(pcgUrnoListArr,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListArr,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        ilDelRecsArr++;
        strcpy(pclUpdFieldList,"");
        strcpy(pclUpdDataList,"");
        sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","snd_cmd_D",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"DFR");
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7800");
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
        if (igSimulation == FALSE)
           ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,"AFTTAB",pclUpdSelection,pclUpdFieldList,
                                pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
     }
  }
  ilNoEle = GetNoOfElements(pcgUrnoListDep,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListDep,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        ilDelRecsDep++;
        strcpy(pclUpdFieldList,"");
        strcpy(pclUpdDataList,"");
        sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","snd_cmd_D",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"DFR");
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7800");
        ilRC = iGetConfigEntry(pcgConfigFile,"DAILY","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"AFTTAB",ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
        if (igSimulation == FALSE)
           ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,"AFTTAB",pclUpdSelection,pclUpdFieldList,
                                pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
     }
  }

  dbg(DEBUG,"%s No of Lines            = %d + %d = %d",pclFunc,ilLineCntArr,ilLineCntDep,ilLineCntArr+ilLineCntDep);
  dbg(DEBUG,"%s No of rejected Records = %d + %d = %d",pclFunc,ilRejectedRecsArr,ilRejectedRecsDep,ilRejectedRecsArr+ilRejectedRecsDep);
  dbg(DEBUG,"%s No of found Records    = %d + %d = %d",pclFunc,ilFoundRecsArr,ilFoundRecsDep,ilFoundRecsArr+ilFoundRecsDep);
  dbg(DEBUG,"%s No of updated Records  = %d + %d = %d",pclFunc,ilUpdRecsArr,ilUpdRecsDep,ilUpdRecsArr+ilUpdRecsDep);
  dbg(DEBUG,"%s No of inserted Records = %d + %d = %d",pclFunc,ilInsRecsArr,ilInsRecsDep,ilInsRecsArr+ilInsRecsDep);
  dbg(DEBUG,"%s No of Towing Records   = %d",pclFunc,ilInsRecsTow);
  dbg(DEBUG,"%s No of deleted Records  = %d + %d = %d",pclFunc,ilDelRecsArr,ilDelRecsDep,ilDelRecsArr+ilDelRecsDep);

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);

  return ilRC;
} /* End of HandleDailyData */


static int HandleDailyDataUpdate(char *pcpFile, char *pcpSection)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleDailyDataUpdate:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  FILE *fp;
  char pclLine[4096];
  int ilMinDiff;
  char pclMinTime[16];
  char pclTmpBuf[2048];
  int ilLineCntArr = 0;
  int ilLineCntDep = 0;
  int ilFoundRecsArr = 0;
  int ilFoundRecsDep = 0;
  int ilRejectedRecsArr = 0;
  int ilRejectedRecsDep = 0;
  int ilUpdRecsArr = 0;
  int ilUpdRecsDep = 0;
  int ilI;
  char pclFixAdid[16];
  char pclAdid[16];
  char pclFlno[16];
  char pclStoa[16];
  char pclStod[16];
  char pclUrno[16];
  char pclFldDat[1024];
  char pclFldLst[1024];
  char pclDatLst[4096];
  int ilRejected;
  int ilNoFlds;
  int ilItemNo;
  char pclUpdFieldList[1024];
  char pclUpdDataList[4096];
  char pclFld[8];
  char pclDatNew[1024];
  char pclDatOld[1024];
  char pclUpdSelection[1024];
  char pclTable[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;

  ilRC = GetFieldConfig("FLD_LST_DAILY_UPD","","FLD_LEN_DAILY_UPD",pcpSection);
  if (ilRC != RC_SUCCESS)
     return ilRC;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,"MIN_DIFF_FAFT_UPD",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     ilMinDiff = atoi(pclTmpBuf);
     strcpy(pclMinTime,pcgCurrentTime);
     AddSecondsToCEDATime(pclMinTime,ilMinDiff*60*60,1);
  }
  else
     ilMinDiff = 0;

  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,"ADID_DAILY_UPD",CFG_STRING,pclFixAdid);
  if (ilRC == RC_SUCCESS)
  {
     if (strcmp(pclFixAdid,"A") != 0 && strcmp(pclFixAdid,"D") != 0)
        strcpy(pclFixAdid,"");
  }
  else
     strcpy(pclFixAdid,"");

  ilRC = iGetConfigEntry(pcgConfigFile,pcpSection,"SEP_DAILY_UPD",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf,"YES") == 0)
  {
     for (ilI = 0; ilI < igNoFileData; ilI++)
        rgFileData[ilI].ilBgn += ilI;
  }

  ilLineCntArr = 0;
  ilLineCntDep = 0;
  ilFoundRecsArr = 0;
  ilFoundRecsDep = 0;
  ilRejectedRecsArr = 0;
  ilRejectedRecsDep = 0;
  ilUpdRecsArr = 0;
  ilUpdRecsDep = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 10)
     {
        strcpy(pclAdid,pclFixAdid);
        strcpy(pclFlno,"");
        strcpy(pclStoa,"");
        strcpy(pclStod,"");
        strcpy(pclFldLst,"");
        strcpy(pclDatLst,"");
        for (ilI = 0; ilI < igNoFileData; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"ADID") == 0 && *pclFixAdid == ' ')
              strcpy(pclAdid,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNO") == 0)
              strcpy(pclFlno,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOA") == 0)
              strcpy(pclStoa,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
              strcpy(pclStod,pclFldDat);
           else
           {
              strcat(pclFldLst,&rgFileData[ilI].pclFldNam[0]);
              strcat(pclFldLst,",");
              strcat(pclDatLst,pclFldDat);
              strcat(pclDatLst,",");
           }
        }
        if (*pclAdid == 'A' && strlen(pclStod) > 0)
        {
           strcat(pclFldLst,"STOD");
           strcat(pclFldLst,",");
           strcat(pclDatLst,pclStod);
           strcat(pclDatLst,",");
        }
        else if (*pclAdid == 'D' && strlen(pclStoa) > 0)
        {
           strcat(pclFldLst,"STOA");
           strcat(pclFldLst,",");
           strcat(pclDatLst,pclStoa);
           strcat(pclDatLst,",");
        }
        if (strlen(pclFldLst) > 0)
        {
           pclFldLst[strlen(pclFldLst)-1] = '\0';
           pclDatLst[strlen(pclDatLst)-1] = '\0';
        }
        ilRC = CheckFlightNo(pclFlno);
        if (strstr(pclFldLst,"URNO") == NULL)
           strcat(pclFldLst,",URNO");
        ilNoFlds = GetNoOfElements(pclFldLst,',');
        ilRejected = FALSE;
        if (*pclAdid == 'A')
        {
           ilLineCntArr++;
           if (ilMinDiff > 0 && strcmp(pclStoa,pclMinTime) < 0)
           {
              ilRejectedRecsArr++;
              ilRejected = TRUE;
              dbg(DEBUG,"%s Record rejected because STOA <%s> less than Minimum <%s>",
                  pclFunc,pclStoa,pclMinTime);
           }
        }
        else if (*pclAdid == 'D')
        {
           ilLineCntDep++;
           if (ilMinDiff > 0 && strcmp(pclStod,pclMinTime) < 0)
           {
              ilRejectedRecsDep++;
              ilRejected = TRUE;
              dbg(DEBUG,"%s Record rejected because STOD <%s> less than Minimum <%s>",
                  pclFunc,pclStod,pclMinTime);
           }
        }
        else
        {
           ilRejected = TRUE;
           dbg(DEBUG,"%s Record rejected because of invalid ADID <%s>",
               pclFunc,pclAdid);
        }
        if (ilRejected == FALSE)
        {
           strcpy(pclUpdFieldList,"");
           strcpy(pclUpdDataList,"");
           if (*pclAdid == 'A')
           {
              sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s' AND ADID = 'A'",pclFlno,pclStoa);
              sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFldLst,pclSelection);
           }
           else
           {
              sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s' AND ADID = 'D'",pclFlno,pclStod);
              sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFldLst,pclSelection);
           }
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              if (*pclAdid == 'A')
                 ilFoundRecsArr++;
              else
                 ilFoundRecsDep++;
              BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
              ilItemNo = get_item_no(pclFldLst,"URNO",5) + 1;
              get_real_item(pclUrno,pclDataBuf,ilItemNo);
              for (ilI = 1; ilI <= ilNoFlds; ilI++)
              {
                 get_real_item(pclFld,pclFldLst,ilI);
                 if (strcmp(pclFld,"URNO") != 0)
                 {
                    get_real_item(pclDatOld,pclDataBuf,ilI);
                    TrimRight(pclDatOld);
                    get_real_item(pclDatNew,pclDatLst,ilI);
                    TrimRight(pclDatNew);
                    if (strcmp(pclDatNew,pclDatOld) != 0)
                    {
                       strcat(pclUpdFieldList,pclFld);
                       strcat(pclUpdFieldList,",");
                       strcat(pclUpdDataList,pclDatNew);
                       strcat(pclUpdDataList,",");
                    }
                 }
              }
              if (strlen(pclUpdFieldList) > 0)
              {
                 pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
                 pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
                 if (*pclAdid == 'A')
                    ilUpdRecsArr++;
                 else
                    ilUpdRecsDep++;
                 sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY_UPD","table",CFG_STRING,pclTable);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclTable,"AFTTAB");
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY_UPD","snd_cmd",CFG_STRING,pclCommand);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclCommand,"UFR");
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY_UPD","priority",CFG_STRING,pclPriority);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclPriority,"3");
                 ilPriority = atoi(pclPriority);
                 ilRC = iGetConfigEntry(pcgConfigFile,"DAILY_UPD","mod_id",CFG_STRING,pclModId);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclModId,"7805");
                 ilModId = atoi(pclModId);
                 dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                 dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
                 dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
                 dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
                 if (igSimulation == FALSE)
                    ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                         pclCommand,pclTable,pclUpdSelection,pclUpdFieldList,
                                         pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
              }
           }
        }
     }
  }

  dbg(DEBUG,"%s No of Lines            = %d + %d = %d",pclFunc,ilLineCntArr,ilLineCntDep,ilLineCntArr+ilLineCntDep);
  dbg(DEBUG,"%s No of rejected Records = %d + %d = %d",pclFunc,ilRejectedRecsArr,ilRejectedRecsDep,ilRejectedRecsArr+ilRejectedRecsDep);
  dbg(DEBUG,"%s No of found Records    = %d + %d = %d",pclFunc,ilFoundRecsArr,ilFoundRecsDep,ilFoundRecsArr+ilFoundRecsDep);
  dbg(DEBUG,"%s No of updated Records  = %d + %d = %d",pclFunc,ilUpdRecsArr,ilUpdRecsDep,ilUpdRecsArr+ilUpdRecsDep);

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);

  return ilRC;
} /* End of HandleDailyDataUpdate */


static int HandleCounterData(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleCounterData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  int ilI;
  FILE *fp;
  char pclLine[4096];
  char pclMinDep[16];
  char pclMaxDep[16];
  int ilFound;
  char pclFldNam[16];
  char pclFldDat[4096];
  char pclUrno[16];
  char pclCkic[16];
  char pclCkbs[16];
  char pclCkes[16];
  char pclCtyp[16];
  char pclFlnu[16];
  char pclFlno[16];
  char pclStod[16];
  char pclTmpBuf[1024];
  char pclExclCounterList[1024];
  int ilNoEle;
  char pclItem[128];
  char pclItem2[128];
  int ilLineCnt;
  int ilRejectedRecs;
  int ilFoundRecs;
  int ilUpdRecs;
  int ilInsRecs;
  int ilDelRecs;
  char pclSelFieldList[1024];
  int ilNoFlds;
  int ilItemNo;
  char pclUpdSelection[1024];
  char pclUpdFieldList[2048];
  char pclUpdDataList[8192];
  char pclTable[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  int ilCount;
  char pclNewFlnu[16];
  int ilRaid;
  char pclRaid[16];
  char pclRarSelection[512];
  char pclRarFieldList[512];
  char pclRarUrno[16];
  char pclRarRnam[16];
  char pclRarType[16];
  char pclRarRurn[16];
  char pclCcaUrno[16];
  int ilMinDiff;
  char pclMinTime[16];

  ilRC = GetFieldConfig("FLD_LST_COUNTER","","FLD_LEN_COUNTER","EXCHDL");
  if (ilRC != RC_SUCCESS)
     return ilRC;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","MIN_DIFF_FCCA",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     ilMinDiff = atoi(pclTmpBuf);
     strcpy(pclMinTime,pcgCurrentTime);
     AddSecondsToCEDATime(pclMinTime,ilMinDiff*60*60,1);
  }
  else
     ilMinDiff = 0;

  strcpy(pclMaxDep,"20000101000000");
  strcpy(pclMinDep,"20301231235959");
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 10)
     {
        ilFound = 0;
        for (ilI = 0; ilI < igNoFileData && ilFound < 2; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"CKBS") == 0)
           {
              strcpy(pclCkbs,pclFldDat);
              ilFound++;
           }
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"CKES") == 0)
           {
              strcpy(pclCkes,pclFldDat);
              ilFound++;
           }
        }
        if (ilMinDiff > 0 && strcmp(pclCkes,pclMinTime) < 0)
           dbg(DEBUG,"%s Record rejected because CKES <%s> less than Minimum <%s>",
               pclFunc,pclCkes,pclMinTime);
        else
        {
           if (ilFound == 2)
           {
              if (strcmp(pclCkes,pclMinDep) < 0)
                 strcpy(pclMinDep,pclCkes);
              if (strcmp(pclMaxDep,pclCkes) < 0)
                 strcpy(pclMaxDep,pclCkes);
           }
        }
     }
  }
  fclose(fp);
  pclMinDep[10] = '\0';
  strcat(pclMinDep,"0000");
  pclMaxDep[10] = '\0';
  strcat(pclMaxDep,"5959");
  if (strcmp(pclMinDep,"20301231230000") == 0)
  {
     strcpy(pclMinDep,"");
     strcpy(pclMaxDep,"");
  }
  strcpy(pclExclCounterList,"");
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","EXCL_COUNTER",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS)
  {
     ilNoEle = GetNoOfElements(pclTmpBuf,',');
     for (ilI = 1; ilI <= ilNoEle; ilI++)
     {
        get_real_item(pclItem,pclTmpBuf,ilI);
        TrimRight(pclItem);
        sprintf(pclItem2,"'%s',",pclItem);
        strcat(pclExclCounterList,pclItem2);
     }
     if (strlen(pclExclCounterList) > 0)
        pclExclCounterList[strlen(pclExclCounterList)-1] = '\0';
  }
  ilRC = GetUrnoList("","",pclMinDep,pclMaxDep,"CCATAB","COUNTER",pclExclCounterList);

  ilRaid = FALSE;
  strcpy(pclSelFieldList,"URNO,");
  for (ilI = 0; ilI < igNoFileData; ilI++)
  {
     strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
     if (strcmp(pclFldNam,"RAID") == 0)
     {
        ilRaid = TRUE;
     }
     else
     {
        strcat(pclSelFieldList,pclFldNam);
        strcat(pclSelFieldList,",");
     }
  }
  pclSelFieldList[strlen(pclSelFieldList)-1] = '\0';
  ilNoFlds = GetNoOfElements(pclSelFieldList,',');

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilLineCnt = 0;
  ilRejectedRecs = 0;
  ilFoundRecs = 0;
  ilUpdRecs = 0;
  ilInsRecs = 0;
  ilDelRecs = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 10)
     {
        ilLineCnt++;
        dbg(DEBUG,"%s Got Line %d: %s",pclFunc,ilLineCnt,pclLine);
        for (ilI = 0; ilI < igNoFileData; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           strcpy(&rgFileData[ilI].pclFldDat[0],pclFldDat);
           if (strcmp(&rgFileData[ilI].pclFldNam[0],"CKIC") == 0)
              strcpy(pclCkic,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"CKBS") == 0)
              strcpy(pclCkbs,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"CKES") == 0)
              strcpy(pclCkes,pclFldDat);
           else if (strcmp(&rgFileData[ilI].pclFldNam[0],"RAID") == 0)
              strcpy(pclRaid,pclFldDat);
        }
        pclCkbs[12] = '\0';
        pclCkes[12] = '\0';
        if (ilMinDiff > 0 && strcmp(pclCkbs,pclMinTime) < 0)
        {
           ilRejectedRecs++;
           dbg(DEBUG,"%s Record rejected because CKBS <%s> less than Minimum <%s>",
               pclFunc,pclCkbs,pclMinTime);
        }
        else
        {
           sprintf(pclSelection,"WHERE CKIC = '%s' AND CKBS LIKE '%s%%' AND CKES LIKE '%s%%'",
                   pclCkic,pclCkbs,pclCkes);
           sprintf(pclSqlBuf,"SELECT %s FROM CCATAB %s",pclSelFieldList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              ilFoundRecs++;
              BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
              ilItemNo = get_item_no(pclSelFieldList,"URNO",5) + 1;
              get_real_item(pclUrno,pclDataBuf,ilItemNo);
              ilRC = RemoveUrno(pclUrno,"D");
              strcpy(pclCcaUrno,pclUrno);
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 strcpy(pclFldDat,&rgFileData[ilI].pclFldDat[0]);
                 if (strcmp(&rgFileData[ilI].pclFldNam[0],"CTYP") == 0)
                    strcpy(pclCtyp,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNU") == 0)
                    strcpy(pclFlnu,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNO") == 0)
                    strcpy(pclFlno,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
                    strcpy(pclStod,pclFldDat);
              }
              strcpy(pclNewFlnu," ");
              if (*pclCtyp == ' ')
              {
                 pclStod[12] = '\0';
                 sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD LIKE '%s%%' AND ADID = 'D'",pclFlno,pclStod);
                 sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclNewFlnu);
                 close_my_cursor(&slCursor);
                 if (ilRCdb == DB_SUCCESS)
                 {
                    strcpy(pclFlnu,pclNewFlnu);
                    TrimRight(pclNewFlnu);
                 }
              }
              else
              {
                 if (*pclFlno != ' ')
                 {
                    ilCount = 0;
                    if (strlen(pclFlno) == 2)
                       ilRC = syslibSearchDbData("ALTTAB","ALC2",pclFlno,"URNO",pclNewFlnu,&ilCount,"\n");
                    else
                       ilRC = syslibSearchDbData("ALTTAB","ALC3",pclFlno,"URNO",pclNewFlnu,&ilCount,"\n");
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclNewFlnu," ");
                    TrimRight(pclNewFlnu);
                 }
              }
              if (*pclNewFlnu != ' ')
              {
                 ilFound = 0;
                 for (ilI = 0; ilI < igNoFileData && ilFound == 0; ilI++)
                 {
                    if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNU") == 0)
                    {
                       strcpy(&rgFileData[ilI].pclFldDat[0],pclNewFlnu);
                       ilFound++;
                    }
                 }
              }
              strcpy(pclUpdFieldList,"");
              strcpy(pclUpdDataList,"");
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
                 ilItemNo = get_item_no(pclSelFieldList,pclFldNam,5) + 1;
                 if (ilItemNo > 0)
                 {
                    get_real_item(pclFldDat,pclDataBuf,ilItemNo);
                    TrimRight(pclFldDat);
                    if (strcmp(&rgFileData[ilI].pclFldDat[0],pclFldDat) != 0)
                    {
                       strcat(pclUpdFieldList,pclFldNam);
                       strcat(pclUpdFieldList,",");
                       strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
                       strcat(pclUpdDataList,",");
                    }
                 }
              }
              if (strlen(pclUpdFieldList) > 0)
              {
                 ilUpdRecs++;
                 pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
                 pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
                 sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
                 ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","table",CFG_STRING,pclTable);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclTable,"CCATAB");
                 ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","snd_cmd_U",CFG_STRING,pclCommand);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclCommand,"URT");
                 ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","priority",CFG_STRING,pclPriority);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclPriority,"4");
                 ilPriority = atoi(pclPriority);
                 ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","mod_id",CFG_STRING,pclModId);
                 if (ilRC != RC_SUCCESS)
                    strcpy(pclModId,"506");
                 ilModId = atoi(pclModId);
                 dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
                 dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
                 dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
                 dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
                 if (igSimulation == FALSE)
                    ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                         pclCommand,pclTable,pclUpdSelection,pclUpdFieldList,
                                         pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
              }
              if (ilRaid == TRUE)
              {
                 strcpy(pclRarFieldList,"URNO,RNAM,TYPE,RURN");
                 sprintf(pclRarSelection,"WHERE RAID = '%s'",pclRaid);
                 sprintf(pclSqlBuf,"SELECT %s FROM RARTAB %s",pclRarFieldList,pclRarSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 if (ilRCdb == DB_SUCCESS)
                 {
                    BuildItemBuffer(pclDataBuf,"",4,",");
                    get_real_item(pclRarUrno,pclDataBuf,1);
                    TrimRight(pclRarUrno);
                    get_real_item(pclRarRnam,pclDataBuf,2);
                    TrimRight(pclRarRnam);
                    get_real_item(pclRarType,pclDataBuf,3);
                    TrimRight(pclRarType);
                    get_real_item(pclRarRurn,pclDataBuf,4);
                    TrimRight(pclRarRurn);
                    if (strcmp(pclRarType,"CC") == 0)
                       strcpy(pclRarType,"C");
                    else
                       strcpy(pclRarType," ");
                    strcpy(pclUpdDataList,"");
                    if (strcmp(pclRarRnam,pclCkic) != 0)
                    {
                       strcat(pclUpdDataList,"RNAM='");
                       strcat(pclUpdDataList,pclCkic);
                       strcat(pclUpdDataList,"',");
                    }
                    if (strcmp(pclRarType,pclCtyp) != 0)
                    {
                       if (*pclCtyp == 'C')
                          strcat(pclUpdDataList,"TYPE='CC',");
                       else
                          strcat(pclUpdDataList,"TYPE='DC',");
                    }
                    if (strcmp(pclRarRurn,pclCcaUrno) != 0)
                    {
                       strcat(pclUpdDataList,"RURN=");
                       strcat(pclUpdDataList,pclCcaUrno);
                       strcat(pclUpdDataList,",");
                    }
                    if (strlen(pclUpdDataList) > 0 && igSimulation == FALSE)
                    {
                       pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
                       sprintf(pclUpdSelection,"WHERE URNO = %s",pclRarUrno);
                       sprintf(pclSqlBuf,"UPDATE RARTAB SET %s %s",pclUpdDataList,pclUpdSelection);
                       slCursor = 0;
                       slFkt = START;
                       dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                       ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                       if (ilRCdb == DB_SUCCESS)
                          commit_work();
                       else
                          dbg(TRACE,"%s Error updating RARTAB",pclFunc);
                       close_my_cursor(&slCursor);
                    }
                 }
                 else
                 {
                    if (igSimulation == FALSE)
                    {
                       strcpy(pclUpdFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
                       ilRC = GetNextUrno();
                       if (*pclCtyp == 'C')
                          sprintf(pclUpdDataList,"%d,'%s','%s','AIMS','CC',%s,'%s',' ','%s'",
                                  igLastUrno,pclRaid,pclCkic,pclCcaUrno,pcgHomeAp,pclCkes);
                       else
                          sprintf(pclUpdDataList,"%d,'%s','%s','AIMS','DC',%s,'%s',' ','%s'",
                                  igLastUrno,pclRaid,pclCkic,pclCcaUrno,pcgHomeAp,pclCkes);
                       sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",
                               pclUpdFieldList,pclUpdDataList);
                       slCursor = 0;
                       slFkt = START;
                       dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                       ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                       if (ilRCdb == DB_SUCCESS)
                          commit_work();
                       else
                          dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
                       close_my_cursor(&slCursor);
                    }
                 }
              }
           }
           else
           {
              ilInsRecs++;
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 strcpy(pclFldDat,&rgFileData[ilI].pclFldDat[0]);
                 if (strcmp(&rgFileData[ilI].pclFldNam[0],"CTYP") == 0)
                    strcpy(pclCtyp,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNU") == 0)
                    strcpy(pclFlnu,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNO") == 0)
                    strcpy(pclFlno,pclFldDat);
                 else if (strcmp(&rgFileData[ilI].pclFldNam[0],"STOD") == 0)
                    strcpy(pclStod,pclFldDat);
              }
              strcpy(pclNewFlnu," ");
              if (*pclCtyp == ' ')
              {
                 pclStod[12] = '\0';
                 sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD LIKE '%s%%' AND ADID = 'D'",pclFlno,pclStod);
                 sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclNewFlnu);
                 close_my_cursor(&slCursor);
                 if (ilRCdb == DB_SUCCESS)
                 {
                    strcpy(pclFlnu,pclNewFlnu);
                    TrimRight(pclNewFlnu);
                 }
              }
              else
              {
                 if (*pclFlno != ' ')
                 {
                    ilCount = 0;
                    if (strlen(pclFlno) == 2)
                       ilRC = syslibSearchDbData("ALTTAB","ALC2",pclFlno,"URNO",pclNewFlnu,&ilCount,"\n");
                    else
                       ilRC = syslibSearchDbData("ALTTAB","ALC3",pclFlno,"URNO",pclNewFlnu,&ilCount,"\n");
                    if (ilRC != RC_SUCCESS)
                       strcpy(pclNewFlnu," ");
                    TrimRight(pclNewFlnu);
                 }
              }
              if (*pclNewFlnu != ' ')
              {
                 ilFound = 0;
                 for (ilI = 0; ilI < igNoFileData && ilFound == 0; ilI++)
                 {
                    if (strcmp(&rgFileData[ilI].pclFldNam[0],"FLNU") == 0)
                    {
                       strcpy(&rgFileData[ilI].pclFldDat[0],pclNewFlnu);
                       ilFound++;
                    }
                 }
              }
              ilRC = GetNextUrno();
              strcpy(pclUpdFieldList,"URNO,");
              sprintf(pclUpdDataList,"%d,",igLastUrno);
              sprintf(pclCcaUrno,"%d",igLastUrno);
              for (ilI = 0; ilI < igNoFileData; ilI++)
              {
                 if (strcmp(&rgFileData[ilI].pclFldNam[0],"RAID") != 0)
                 {
                    strcat(pclUpdFieldList,&rgFileData[ilI].pclFldNam[0]);
                    strcat(pclUpdFieldList,",");
                    strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
                    strcat(pclUpdDataList,",");
                 }
              }
              pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
              pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
              strcpy(pclUpdSelection,"");
              ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","table",CFG_STRING,pclTable);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclTable,"CCATAB");
              ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","snd_cmd_I",CFG_STRING,pclCommand);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclCommand,"IRT");
              ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","priority",CFG_STRING,pclPriority);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclPriority,"4");
              ilPriority = atoi(pclPriority);
              ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","mod_id",CFG_STRING,pclModId);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclModId,"506");
              ilModId = atoi(pclModId);
              dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
              dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
              dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
              dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
              if (igSimulation == FALSE)
                 ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                      pclCommand,pclTable,pclUpdSelection,pclUpdFieldList,
                                      pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
              if (ilRaid == TRUE && igSimulation == FALSE)
              {
                 sprintf(pclSqlBuf,"DELETE FROM RARTAB WHERE RAID = '%s'",pclRaid);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 if (ilRCdb == DB_SUCCESS)
                    commit_work();
                 close_my_cursor(&slCursor);
                 strcpy(pclUpdFieldList,"URNO,RAID,RNAM,IFNA,TYPE,RURN,HOPO,STAT,TIME");
                 ilRC = GetNextUrno();
                 if (*pclCtyp == 'C')
                    sprintf(pclUpdDataList,"%d,'%s','%s','AIMS','CC',%s,'%s',' ','%s'",
                            igLastUrno,pclRaid,pclCkic,pclCcaUrno,pcgHomeAp,pclCkes);
                 else
                    sprintf(pclUpdDataList,"%d,'%s','%s','AIMS','DC',%s,'%s',' ','%s'",
                            igLastUrno,pclRaid,pclCkic,pclCcaUrno,pcgHomeAp,pclCkes);
                 sprintf(pclSqlBuf,"INSERT INTO RARTAB FIELDS(%s) VALUES(%s)",pclUpdFieldList,pclUpdDataList);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 if (ilRCdb == DB_SUCCESS)
                    commit_work();
                 else
                    dbg(TRACE,"%s Error inserting into RARTAB",pclFunc);
                 close_my_cursor(&slCursor);
              }
           }
        }
     }
  }

  ilNoEle = GetNoOfElements(pcgUrnoListDep,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListDep,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        ilDelRecs++;
        strcpy(pclUpdFieldList,"");
        strcpy(pclUpdDataList,"");
        sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","snd_cmd_D",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"DRT");
        ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"506");
        ilRC = iGetConfigEntry(pcgConfigFile,"COUNTER","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"4");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,"CCATAB",ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
        if (igSimulation == FALSE)
           ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,"CCATAB",pclUpdSelection,pclUpdFieldList,
                                pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
        if (igSimulation == FALSE)
        {
           sprintf(pclSqlBuf,"DELETE FROM RARTAB WHERE RURN = %s",pclUrno);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           if (ilRCdb == DB_SUCCESS)
              commit_work();
           close_my_cursor(&slCursor);
        }
     }
  }

  dbg(DEBUG,"%s No of Lines            = %d",pclFunc,ilLineCnt);
  dbg(DEBUG,"%s No of rejected Records = %d",pclFunc,ilRejectedRecs);
  dbg(DEBUG,"%s No of found Records    = %d",pclFunc,ilFoundRecs);
  dbg(DEBUG,"%s No of updated Records  = %d",pclFunc,ilUpdRecs);
  dbg(DEBUG,"%s No of inserted Records = %d",pclFunc,ilInsRecs);
  dbg(DEBUG,"%s No of deleted Records  = %d",pclFunc,ilDelRecs);

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);
  return ilRC;
} /* End of HandleCounterData */


static int HandleBasicData(char *pcpFile, char *pcpTableName)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleBasicData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  int ilI;
  int ilJ;
  FILE *fp;
  char pclLine[4096];
  int ilLineCnt;
  int ilFoundRecs;
  int ilUpdRecs;
  int ilInsRecs;
  int ilDelRecs;
  char pclCfgFldLstIns[128];
  char pclCfgFldLstUpd[128];
  char pclCfgFldLen[128];
  char pclCfgKeys[128];
  char pclFldNam[16];
  char pclFldNamUpd[16];
  char pclFldDat[4096];
  char pclSelFieldList[2048];
  char pclSelFieldListUpd[2048];
  int ilNoFlds;
  int ilFound;
  int ilUpdUrno;
  char pclKey[10][16];
  char pclKeyData[10][128];
  char ilNoKeys;
  char pclKeyList[256];
  char pclTmpBuf[1024];
  char pclCommand[16];
  char pclPriority[16];
  char pclModId[16];
  int ilPriority;
  int ilModId;
  char pclUpdSelection[1024];
  char pclUpdFieldList[2048];
  char pclUpdDataList[8192];
  int ilNoEle;
  int ilItemNo;
  char pclUrno[16];
  char pclNewUrno[16];

  sprintf(pclCfgFldLstIns,"FLD_LST_%s_I",pcpTableName);
  sprintf(pclCfgFldLstUpd,"FLD_LST_%s_U",pcpTableName);
  sprintf(pclCfgFldLen,"FLD_LEN_%s",pcpTableName);
  ilRC = GetFieldConfig(pclCfgFldLstIns,pclCfgFldLstUpd,pclCfgFldLen,"EXCHDL");
  if (ilRC != RC_SUCCESS)
     return ilRC;

  ilRC = GetUrnoList("","","","",pcpTableName,"BASIC","");

  sprintf(pclCfgKeys,"TAB_SEL_%s",pcpTableName);
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL",pclCfgKeys,CFG_STRING,pclKeyList);
  if (ilRC != RC_SUCCESS)
     strcpy(pclKeyList,"");
  ilUpdUrno = FALSE;
  for (ilI = 0; ilI < igNoFileData && ilUpdUrno == FALSE; ilI++)
     if (strcmp(&rgFileData[ilI].pclFldNam[0],"URNO") == 0)
        ilUpdUrno = TRUE;
  if (ilUpdUrno == FALSE)
     strcpy(pclSelFieldList,"URNO,");
  else
     strcpy(pclSelFieldList,"");
  strcpy(pclSelFieldListUpd,pclSelFieldList);
  ilNoKeys = 0;
  for (ilI = 0; ilI < igNoFileData; ilI++)
  {
     strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
     strcat(pclSelFieldList,pclFldNam);
     strcat(pclSelFieldList,",");
     if (strstr(pclKeyList,pclFldNam) != NULL)
     {
        strcpy(&pclKey[ilNoKeys][0],pclFldNam);
        ilNoKeys++;
     }
     strcpy(pclFldNam,&rgFileData[ilI].pclFldNamUpd[0]);
     strcat(pclSelFieldListUpd,pclFldNam);
     strcat(pclSelFieldListUpd,",");
  }
  pclSelFieldList[strlen(pclSelFieldList)-1] = '\0';
  pclSelFieldListUpd[strlen(pclSelFieldListUpd)-1] = '\0';
  ilNoFlds = GetNoOfElements(pclSelFieldList,',');

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilLineCnt = 0;
  ilFoundRecs = 0;
  ilUpdRecs = 0;
  ilInsRecs = 0;
  ilDelRecs = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     if (strlen(pclLine) > 0)
     {
        ilLineCnt++;
        dbg(DEBUG,"%s Got Line %d: %s",pclFunc,ilLineCnt,pclLine);
        strcpy(pclNewUrno,"");
        for (ilI = 0; ilI < igNoFileData; ilI++)
        {
           strncpy(pclFldDat,&pclLine[rgFileData[ilI].ilBgn],rgFileData[ilI].ilLen);
           pclFldDat[rgFileData[ilI].ilLen] = '\0';
           TrimRight(pclFldDat);
           strcpy(&rgFileData[ilI].pclFldDat[0],pclFldDat);
           if (ilUpdUrno == FALSE)
           {
              ilFound = FALSE;
              for (ilJ = 0; ilJ < igNoFileData && ilFound == FALSE; ilJ++)
              {
                 if (strcmp(&rgFileData[ilI].pclFldNam[0],&pclKey[ilJ][0]) == 0)
                 {
                    strcpy(&pclKeyData[ilJ][0],pclFldDat);
                    ilFound = TRUE;
                 }
              }
           }
           else
           {
              if (strcmp(&rgFileData[ilI].pclFldNam[0],"URNO") == 0)
                 strcpy(pclNewUrno,pclFldDat);
           }
        }
        if (ilUpdUrno == FALSE)
        {
           strcpy(pclSelection, "WHERE ");
           for (ilI = 0; ilI < ilNoKeys; ilI++)
           {
              if (ilI == ilNoKeys-1)
                 sprintf(pclTmpBuf,"%s = '%s'",&pclKey[ilI][0],&pclKeyData[ilI][0]);
              else
                 sprintf(pclTmpBuf,"%s = '%s' AND ",&pclKey[ilI][0],&pclKeyData[ilI][0]);
              strcat(pclSelection,pclTmpBuf);
           }
        }
        else
           sprintf(pclSelection,"WHERE URNO = %s",pclNewUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM %s %s",pclSelFieldList,pcpTableName,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           ilFoundRecs++;
           BuildItemBuffer(pclDataBuf,"",ilNoFlds,",");
           ilItemNo = get_item_no(pclSelFieldList,"URNO",5) + 1;
           get_real_item(pclUrno,pclDataBuf,ilItemNo);
           ilRC = RemoveUrno(pclUrno,"A");
           strcpy(pclUpdFieldList,"");
           strcpy(pclUpdDataList,"");
           for (ilI = 0; ilI < igNoFileData; ilI++)
           {
              strcpy(pclFldNam,&rgFileData[ilI].pclFldNam[0]);
              strcpy(pclFldNamUpd,&rgFileData[ilI].pclFldNamUpd[0]);
              ilItemNo = get_item_no(pclSelFieldList,pclFldNam,5) + 1;
              get_real_item(pclFldDat,pclDataBuf,ilItemNo);
              TrimRight(pclFldDat);
              if (strcmp(&rgFileData[ilI].pclFldDat[0],pclFldDat) != 0 && strcmp(pclFldNamUpd,"FILL") != 0)
              {
                 strcat(pclUpdFieldList,pclFldNam);
                 strcat(pclUpdFieldList,",");
                 strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
                 strcat(pclUpdDataList,",");
              }
           }
           if (strlen(pclUpdFieldList) > 0)
           {
              ilUpdRecs++;
              pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
              pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
              sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
              ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","snd_cmd_U",CFG_STRING,pclCommand);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclCommand,"URT");
              ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","priority",CFG_STRING,pclPriority);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclPriority,"4");
              ilPriority = atoi(pclPriority);
              ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","mod_id",CFG_STRING,pclModId);
              if (ilRC != RC_SUCCESS)
                 strcpy(pclModId,"506");
              ilModId = atoi(pclModId);
              dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pcpTableName,ilModId,ilPriority);
              dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
              dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
              dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
              if (igSimulation == FALSE)
                 ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                      pclCommand,pcpTableName,pclUpdSelection,pclUpdFieldList,
                                      pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
           }
        }
        else
        {
           ilInsRecs++;
           if (ilUpdUrno == FALSE)
           {
              ilRC = GetNextUrno();
              strcpy(pclUpdFieldList,"URNO,");
              sprintf(pclUpdDataList,"%d,",igLastUrno);
           }
           else
           {
              strcpy(pclUpdFieldList,"");
              strcpy(pclUpdDataList,"");
           }
           for (ilI = 0; ilI < igNoFileData; ilI++)
           {
              strcat(pclUpdFieldList,&rgFileData[ilI].pclFldNam[0]);
              strcat(pclUpdFieldList,",");
              strcat(pclUpdDataList,&rgFileData[ilI].pclFldDat[0]);
              strcat(pclUpdDataList,",");
           }
           pclUpdFieldList[strlen(pclUpdFieldList)-1] = '\0';
           pclUpdDataList[strlen(pclUpdDataList)-1] = '\0';
           strcpy(pclUpdSelection,"");
           ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","snd_cmd_I",CFG_STRING,pclCommand);
           if (ilRC != RC_SUCCESS)
              strcpy(pclCommand,"IRT");
           ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","priority",CFG_STRING,pclPriority);
           if (ilRC != RC_SUCCESS)
              strcpy(pclPriority,"4");
           ilPriority = atoi(pclPriority);
           ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","mod_id",CFG_STRING,pclModId);
           if (ilRC != RC_SUCCESS)
              strcpy(pclModId,"506");
           ilModId = atoi(pclModId);
           dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pcpTableName,ilModId,ilPriority);
           dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
           dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
           dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
           if (igSimulation == FALSE)
              ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                   pclCommand,pcpTableName,pclUpdSelection,pclUpdFieldList,
                                   pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
        }
     }
  }

  ilNoEle = GetNoOfElements(pcgUrnoListArr,',');
  for (ilI = 1; ilI <= ilNoEle; ilI++)
  {
     get_real_item(pclUrno,pcgUrnoListArr,ilI);
     TrimRight(pclUrno);
     if (*pclUrno != ' ')
     {
        ilDelRecs++;
        strcpy(pclUpdFieldList,"");
        strcpy(pclUpdDataList,"");
        sprintf(pclUpdSelection,"WHERE URNO = %s",pclUrno);
        ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","snd_cmd_D",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"DRT");
        ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"506");
        ilRC = iGetConfigEntry(pcgConfigFile,"BASIC","priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"4");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pcpTableName,ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclUpdSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclUpdFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclUpdDataList);
        if (igSimulation == FALSE)
           ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,pcpTableName,pclUpdSelection,pclUpdFieldList,
                                pclUpdDataList,"",ilPriority,NETOUT_NO_ACK);
     }
  }

  dbg(DEBUG,"%s No of Lines            = %d",pclFunc,ilLineCnt);
  dbg(DEBUG,"%s No of found Records    = %d",pclFunc,ilFoundRecs);
  dbg(DEBUG,"%s No of updated Records  = %d",pclFunc,ilUpdRecs);
  dbg(DEBUG,"%s No of inserted Records = %d",pclFunc,ilInsRecs);
  dbg(DEBUG,"%s No of deleted Records  = %d",pclFunc,ilDelRecs);

  fclose(fp);
  sprintf(pclTmpBuf,"%s.sav",pcpFile);
  ilRC = rename(pcpFile,pclTmpBuf);
  return ilRC;
} /* End of HandleBasicData */


static int GetFileName(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetFileName:";
  char pclTmpBuf[1024];
  FILE *fp;
  char pclLine[1024];

  if (strstr(pcpFile,"*") != NULL)
  {
     sprintf(pclTmpBuf,"ls -C1 %s > /tmp/exchdl_filelist",pcpFile);
     system(pclTmpBuf);
     if ((fp = (FILE *)fopen("/tmp/exchdl_filelist","r")) != (FILE *)NULL)
     {
        if (fgets(pclLine,1024,fp))
        {
           pclLine[strlen(pclLine)-1] = '\0';
           strcpy(pcpFile,pclLine);
           dbg(DEBUG,"%s Found File <%s>",pclFunc,pcpFile);
        }
        fclose(fp);
     }
  }

  return ilRC;
} /* End of GetFileName */


/* ************************************** */
static void SetStatusMessage(char *pcpTxt)
{
  int ilLen = 0;
  int ilNewLen = 0;
  char *pclPtr = NULL;
  pclPtr = pcpTxt;

  char * pclTemp = NULL;

  if (igStatusMsgBufLen <= 0)
  {
    igStatusMsgBufLen = 1024;
    pcgStatusMsgBuf = (char *)malloc(igStatusMsgBufLen);
    pcgStatusMsgBuf[0] = '\0';
    igStatusMsgLen = 0;
    igStatusMsgIni = 0;
  }
  ilLen =  strlen(pclPtr);
  if (*pclPtr == '?')
  {
    pcgStatusMsgBuf[0] = '\0';
    igStatusMsgLen = 0;
    pclPtr++;
    ilLen--;
    igStatusMsgIni = ilLen;
  }
  if (ilLen > 0)
  {
    ilNewLen = igStatusMsgLen + ilLen + 1;
    if (ilNewLen > igStatusMsgBufLen)
    {
      pclTemp = (char *)realloc(pcgStatusMsgBuf,ilNewLen);
      pclTemp = NULL;
    }
    strcat(pcgStatusMsgBuf,pclPtr);
    igStatusMsgLen = ilNewLen - 1;
  }
  return;
} /* End of SetStatusMessage */

static void ConvertVialToRout(char *pcpData)
{
  char pclTmpBuf[1024];
  char pclApc[32];
  int ilI;

  if (strlen(pcpData) > 3)
  {
     strncpy(pclTmpBuf,pcpData,3);
     pclTmpBuf[3] = '\0';
     ilI = 3;
     strncpy(pclApc,&pcpData[ilI],3);
     pclApc[3] = '\0';
     while (strlen(pclApc) > 0)
     {
        strcat(pclTmpBuf,"|");
        strcat(pclTmpBuf,pclApc);
        ilI += 3;
        strncpy(pclApc,&pcpData[ilI],3);
        pclApc[3] = '\0';
     }
     strcpy(pcpData,pclTmpBuf);
  }
} /* End of ConvertVialToRout */

static void AddViaInfoToRoute(char *pcpField, char *pcpData)
{
  if (*pcgViaFldLst == '\0')
     strcpy(pcgViaFldLst,"[");
  strcat(pcgViaFldLst,pcpField);
  strcat(pcgViaDatLst,pcpData);
  strcat(pcgViaFldLst,";");
  strcat(pcgViaDatLst,";");
  if (strncmp(pcpField,"APC",3) == 0)
     strcpy(pcgViaApc,pcpData);
} /* End of AddViaInfoToRoute */

static int HandlePDAIn()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandlePDAIn:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  int ilIdx;
  char pclType[16];
  char pclIp[32];
  char pclUrno[16];
  char pclValue[1024];
  char pclFieldList[1024];
  char pclDataList[1024];
  char pclTable[16];
  char pclModId[16];
  char pclCommand[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;
  char pclSectionType[32];
  char pclTime[16];
  int ilTimeDiffLocalUtc;
  int ilTDIS = -1;
  int ilTDIW = -1;

  ilIdx = GetXmlLineIdx("HDR","TYPE",NULL);
  if (ilIdx > 0)
  {
     strcpy(pclType,&rgXml.rlLine[ilIdx].pclData[0]);
     if (strcmp(pclType,"13") == 0 || strcmp(pclType,"14") == 0)
     {
        ilIdx = GetXmlLineIdx("DAT","IP","IDSECTION");
        if (ilIdx > 0)
        {
           strcpy(pclIp,&rgXml.rlLine[ilIdx].pclData[0]);
           sprintf(pclSectionType,"TYPE_%s",pclType);
           strcpy(pclTable,"");
           strcpy(pclFieldList,"");
           strcpy(pclDataList,"");
           strcpy(pclSelection,pclIp);
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"mod_id",CFG_STRING,pclModId);
           if (ilRC != RC_SUCCESS)
              strcpy(pclModId,"7922");
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"snd_cmd",CFG_STRING,pclCommand);
           if (ilRC != RC_SUCCESS)
              strcpy(pclCommand,"DLWR");
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"priority",CFG_STRING,pclPriority);
           if (ilRC != RC_SUCCESS)
              strcpy(pclPriority,"3");
           ilPriority = atoi(pclPriority);
           ilModId = atoi(pclModId);
           dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
               pclFunc,pclCommand,pclTable,ilModId,ilPriority);
           dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
           dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
           dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
           ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                                ilPriority,NETOUT_NO_ACK);
        }
     }
     else if (strcmp(pclType,"7") == 0 || strcmp(pclType,"8") == 0)
     {
        strcpy(pclUrno,"0");
        strcpy(pclValue,"");
        ilIdx = GetXmlLineIdx("DAT","URNO","DATA");
        if (ilIdx > 0)
           strcpy(pclUrno,&rgXml.rlLine[ilIdx].pclData[0]);
        ilIdx = GetXmlLineIdx("DAT","VALUE","DATA");
        if (ilIdx > 0)
           strcpy(pclValue,&rgXml.rlLine[ilIdx].pclData[0]);
        if (strcmp(pclType,"7") == 0)
           strcpy(pclFieldList,"ACFR,STAT");
        else
           strcpy(pclFieldList,"ACTO,STAT");
        strcpy(pclTime,pcgCurrentTime);
        pclTime[8] = '\0';
        strncat(pclTime,pclValue,2);
        strncat(pclTime,&pclValue[3],2);
        strncat(pclTime,&pclValue[6],2);
        ilTimeDiffLocalUtc = GetTimeDiffUtc(pclTime,&ilTDIS,&ilTDIW);
        ilRC = MyAddSecondsToCEDATime(pclTime,ilTimeDiffLocalUtc*60*(-1),1);
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT STAT FROM JOBTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           if (strcmp(pclType,"7") == 0)
              *pclDataBuf = 'C';
           else
              *pclDataBuf = 'F';
        }
        else
        {
           if (strcmp(pclType,"7") == 0)
              strcpy(pclDataBuf,"C");
           else
              strcpy(pclDataBuf,"F");
        }
        sprintf(pclDataList,"%s,%s",pclTime,pclDataBuf);
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSectionType,"TYPE_%s",pclType);
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"table",CFG_STRING,pclTable);
        if (ilRC != RC_SUCCESS)
           strcpy(pclTable,"JOBTAB");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7922");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"snd_cmd",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"URT");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
            pclFunc,pclCommand,pclTable,ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
        ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                             ilPriority,NETOUT_NO_ACK);
     }
     else if (strcmp(pclType,"9") == 0)
     {
        strcpy(pclUrno,"0");
        strcpy(pclValue,"");
        ilIdx = GetXmlLineIdx("DAT","URNO","DATA");
        if (ilIdx > 0)
           strcpy(pclUrno,&rgXml.rlLine[ilIdx].pclData[0]);
        ilIdx = GetXmlLineIdx("DAT","VALUE","DATA");
        if (ilIdx > 0)
           strcpy(pclValue,&rgXml.rlLine[ilIdx].pclData[0]);
        strcpy(pclFieldList,"TEXT");
        strcpy(pclDataList,pclValue);
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSectionType,"TYPE_%s",pclType);
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"table",CFG_STRING,pclTable);
        if (ilRC != RC_SUCCESS)
           strcpy(pclTable,"JOBTAB");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"mod_id",CFG_STRING,pclModId);
        if (ilRC != RC_SUCCESS)
           strcpy(pclModId,"7922");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"snd_cmd",CFG_STRING,pclCommand);
        if (ilRC != RC_SUCCESS)
           strcpy(pclCommand,"URT");
        ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"priority",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilPriority = atoi(pclPriority);
        ilModId = atoi(pclModId);
        dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
            pclFunc,pclCommand,pclTable,ilModId,ilPriority);
        dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
        dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
        dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
        ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                             pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                             ilPriority,NETOUT_NO_ACK);
     }
     else if (strcmp(pclType,"11") == 0)
     {
        strcpy(pclUrno,"0");
        strcpy(pclValue,"");
        ilIdx = GetXmlLineIdx("DAT","URNO","DATA");
        if (ilIdx > 0)
           strcpy(pclUrno,&rgXml.rlLine[ilIdx].pclData[0]);
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT STAT FROM JOBTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS && *pclDataBuf == 'P')
        {
           strcpy(pclFieldList,"STAT");
           strcpy(pclDataList,pclDataBuf);
           if (strlen(pclDataList) == 1)
              strcat(pclDataList,"   A");
           else if (strlen(pclDataList) == 2)
              strcat(pclDataList,"  A");
           else if (strlen(pclDataList) == 3)
              strcat(pclDataList," A");
           else
              pclDataList[4] = 'A';
           sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
           sprintf(pclSectionType,"TYPE_%s",pclType);
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"table",CFG_STRING,pclTable);
           if (ilRC != RC_SUCCESS)
              strcpy(pclTable,"JOBTAB");
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"mod_id",CFG_STRING,pclModId);
           if (ilRC != RC_SUCCESS)
              strcpy(pclModId,"7922");
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"snd_cmd",CFG_STRING,pclCommand);
           if (ilRC != RC_SUCCESS)
              strcpy(pclCommand,"URT");
           ilRC = iGetConfigEntry(pcgConfigFile,pclSectionType,"priority",CFG_STRING,pclPriority);
           if (ilRC != RC_SUCCESS)
              strcpy(pclPriority,"3");
           ilPriority = atoi(pclPriority);
           ilModId = atoi(pclModId);
           dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
               pclFunc,pclCommand,pclTable,ilModId,ilPriority);
           dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
           dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFieldList);
           dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDataList);
           ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                pclCommand,pclTable,pclSelection,pclFieldList,pclDataList,"",
                                ilPriority,NETOUT_NO_ACK);
        }
     }
     else
        dbg(TRACE,"%s Invalid Type <%s> received",pclFunc,pclType);
  }
  else
     dbg(TRACE,"%s No Type found",pclFunc);

  return ilRC;
} /* End of HandlePDAIn */


static int HandlePDAOut(char *pcpType, char *pcpParam, char *pcpFields, char *pcpNewData, char *pcpOldData,
                        int ipIndex, char *pcpCurSec, char *pcpCurIntf)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandlePDAOut:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor = 0;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  char pclOldDataBuf[8192];
  char pclTmpBuf[8192];
  int ilRCdb2 = DB_SUCCESS;
  short slFkt2;
  short slCursor2 = 0;
  char pclSelection2[1024];
  char pclSqlBuf2[1024];
  int ilCount = 0;
  char pclPDASections[128];
  int ilIdx;
  int ilMsgUrno;
  int ilI;
  char pclFieldList[1024];
  char pclSection[128];
  int ilNoEle;
  int ilDatCnt;
  char pclFieldList2[1024];
  char pclSection2[128];
  char pclSectionFields[1024];
  int ilNoEle2;
  int ilDatCnt2;
  int ilRecs;
  char pclUstf[16];
  char pclUprm[16];
  char pclUaft[16];
  int ilItemNo;
  char pclIpad[32];
  char pclText[256];
  char pclPlfr[16];
  char pclPlto[16];
  char pclUaid[16];
  char pclUghs[16];
  char pclDtyp[8];
  char pclFrmt[8];
  char pclUrno[16];
  char pclDPXUrno[16];
  char pclSTFUrno[16];
  char pclAsgnType[8];
  char pclArrivalDesk[32];
  char pclAftAdid[8];
  char pclAftPosn[16];

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PDA_SECTIONS",CFG_STRING,pclPDASections);
  if (ilRC != RC_SUCCESS)
  {
     dbg(TRACE,"%s No PDA Sections defined.",pclFunc);
     return ilRC;
  }

  ilIdx = GetXmlLineIdx("HDR","TYPE",NULL);
  if (ilIdx >= 0)
  {
     if (strcmp(pcpType,"FLIGHT") == 0)
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"4");
     else if (strcmp(pcpType,"DLRA") == 0)
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"5");
     else if (strcmp(pcpType,"DLWR") == 0)
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"6");
     else if (strcmp(pcpType,"URT") == 0)
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],"3");
     else if (strcmp(pcpType,"SPDA") == 0)
     {
        get_real_item(pclDPXUrno,pcpParam,1);
        get_real_item(pclSTFUrno,pcpParam,2);
        get_real_item(pclAsgnType,pcpParam,3);
        strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclAsgnType);
     }
     else
     {
        dbg(TRACE,"%s Invalid Type = <%s> defined",pclFunc,pcpType);
        return ilRC;
     }
     rgXml.rlLine[ilIdx].ilRcvFlag = 1;
  }
  ilMsgUrno = GetXmlLineIdx("HDR","URNO",NULL);
  if (ilMsgUrno >= 0)
  {
     ilRC = GetNextUrno();
     sprintf(&rgXml.rlLine[ilMsgUrno].pclData[0],"%d",igLastUrno);
     rgXml.rlLine[ilMsgUrno].ilRcvFlag = 1;
  }

  if (strcmp(pcpType,"FLIGHT") == 0)
  {
     strcpy(pclAftAdid,"");
     strcpy(pclArrivalDesk,"STAND:");
     sprintf(pclSqlBuf,"SELECT ADID,PSTA FROM AFTTAB WHERE URNO = %s",pcpParam);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",2,",");
        get_real_item(pclAftAdid,pclDataBuf,1);
        get_real_item(pclAftPosn,pclDataBuf,2);
        if (*pclAftAdid == 'A')
           strcat(pclArrivalDesk,pclAftPosn);
     }
     get_real_item(pclSection,pclPDASections,1);
     ilNoEle = 0;
     strcpy(pclFieldList,"");
     ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     sprintf(pclSelection,"WHERE FLNU = %s",pcpParam);
     sprintf(pclSqlBuf,"SELECT %s FROM DPXTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     while (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
        if (*pclAftAdid == 'A')
        {
           ilItemNo = get_item_no(pclFieldList,"ALOC",5) + 1;
           if (ilItemNo > 0)
              ilRC = ReplaceItem(pclDataBuf,ilItemNo,pclArrivalDesk);
        }
        strcpy(pclOldDataBuf,"");
        for (ilI = 1; ilI < ilNoEle; ilI++)
           strcat(pclOldDataBuf,",");
        ilDatCnt = 0;
        ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
        ilItemNo = get_item_no(pclFieldList,"URNO",5) + 1;
        get_real_item(pclUprm,pclDataBuf,ilItemNo);
        sprintf(pclSelection2,"WHERE UAFT = %s AND UPRM = %s",pcpParam,pclUprm);
        sprintf(pclSqlBuf2,"SELECT USTF FROM JOBTAB %s",pclSelection2);
        slCursor2 = 0;
        slFkt2 = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf2);
        ilRCdb2 = sql_if(slFkt2,&slCursor2,pclSqlBuf2,pclUstf);
        close_my_cursor(&slCursor2);
        if (ilRCdb2 == DB_SUCCESS)
        {
           sprintf(pclSelection2,"WHERE USTF = %s",pclUstf);
           sprintf(pclSqlBuf2,"SELECT IPAD FROM PDATAB %s",pclSelection2);
           slCursor2 = 0;
           slFkt2 = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf2);
           ilRCdb2 = sql_if(slFkt2,&slCursor2,pclSqlBuf2,pclIpad);
           close_my_cursor(&slCursor2);
           if (ilRCdb2 == DB_SUCCESS)
           {
              get_real_item(pclSection2,pclPDASections,3);
              ilNoEle2 = 0;
              strcpy(pclFieldList2,"");
              ilRC = GetFieldList(pclSection2,pclFieldList2,&ilNoEle2);
              if (strlen(pclFieldList2) > 0)
                 pclFieldList2[strlen(pclFieldList2)-1] = '\0';
              ilDatCnt2 = 0;
              ilRC = StoreData(pclFieldList2,pclIpad," ",ilNoEle2,pclSection2,&ilDatCnt2);
              ilIdx = GetXmlLineIdx("STR_BGN","DATA",NULL);
              if (ilIdx >= 0)
                 rgXml.rlLine[ilIdx].ilRcvFlag = 1;
              ilIdx = GetXmlLineIdx("STR_END","DATA",NULL);
              if (ilIdx >= 0)
                 rgXml.rlLine[ilIdx].ilRcvFlag = 1;
              ilRC = MarkOutput();
              ilRC = BuildOutput(ipIndex,&ilCount,pcpCurSec,NULL,pcpCurIntf,"C");
           }
        }
        ilRC = ClearSection(pclSection);
        ilRC = ClearSection(pclSection2);
        if (ilMsgUrno >= 0)
        {
           ilRC = GetNextUrno();
           sprintf(&rgXml.rlLine[ilMsgUrno].pclData[0],"%d",igLastUrno);
        }
        slFkt = NEXT;
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     }
     close_my_cursor(&slCursor);
  }
  else if (strcmp(pcpType,"SPDA") == 0)
  {
     sprintf(pclSelection,"WHERE USTF = %s",pclSTFUrno);
     sprintf(pclSqlBuf,"SELECT IPAD FROM PDATAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclIpad);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        get_real_item(pclSection,pclPDASections,3);
        ilNoEle = 0;
        strcpy(pclFieldList,"");
        ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
        if (strlen(pclFieldList) > 0)
           pclFieldList[strlen(pclFieldList)-1] = '\0';
        ilDatCnt = 0;
        ilRC = StoreData(pclFieldList,pclIpad," ",ilNoEle,pclSection,&ilDatCnt);
        get_real_item(pclSection,pclPDASections,1);
        ilNoEle = 0;
        strcpy(pclFieldList,"");
        ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
        if (strlen(pclFieldList) > 0)
           pclFieldList[strlen(pclFieldList)-1] = '\0';
        sprintf(pclSelection,"WHERE URNO = %s",pclDPXUrno);
        sprintf(pclSqlBuf,"SELECT %s FROM DPXTAB %s",pclFieldList,pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
           strcpy(pclOldDataBuf,"");
           for (ilI = 1; ilI < ilNoEle; ilI++)
              strcat(pclOldDataBuf,",");
           ilDatCnt = 0;
           ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
           sprintf(pclSelection,"WHERE UPRM = %s AND UAFT <> 0 AND SUBSTR(STAT,1,1) <> 'F' ORDER BY PLFR",
                   pclDPXUrno);
           sprintf(pclSqlBuf,"SELECT UAFT FROM JOBTAB %s",pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclUaft);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              sprintf(pclSelection,"WHERE URNO = %s",pclUaft);
              sprintf(pclSqlBuf,"SELECT ADID,PSTA FROM AFTTAB %s",pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              close_my_cursor(&slCursor);
              if (ilRCdb == DB_SUCCESS)
              {
                 BuildItemBuffer(pclDataBuf,"",2,",");
                 get_real_item(pcgAdid,pclDataBuf,1);
                 get_real_item(pclAftPosn,pclDataBuf,2);
                 if (*pcgAdid == 'A')
                 {
                    sprintf(pclArrivalDesk,"STAND:%s",pclAftPosn);
                    ilIdx = GetXmlLineIdx("DAT","ALOC","INFOBJ_PAX");
                    if (ilIdx >= 0)
                       strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclArrivalDesk);
                 }
                 get_real_item(pclSection,pclPDASections,4);
                 ilNoEle = 0;
                 strcpy(pclFieldList,"");
                 ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
                 if (strlen(pclFieldList) > 0)
                    pclFieldList[strlen(pclFieldList)-1] = '\0';
                 sprintf(pclSelection,"WHERE URNO = %s",pclUaft);
                 sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 if (ilRCdb == DB_SUCCESS)
                 {
                    BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
                    strcpy(pclOldDataBuf,"");
                    for (ilI = 1; ilI < ilNoEle; ilI++)
                       strcat(pclOldDataBuf,",");
                    ilDatCnt = 0;
                    ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
                    get_real_item(pclSection,pclPDASections,2);
                    ilNoEle = 0;
                    strcpy(pclSectionFields,"");
                    ilRC = GetFieldList(pclSection,pclSectionFields,&ilNoEle);
                    if (strlen(pclSectionFields) > 0)
                       pclSectionFields[strlen(pclSectionFields)-1] = '\0';
                    ilRecs = 0;
                    strcpy(pclFieldList,"URNO,UPRM,UAFT,TEXT,PLFR,PLTO,UAID,UGHS");
                    sprintf(pclSelection,"WHERE UPRM = %s AND USTF = %s AND SUBSTR(STAT,1,1) <> 'F' ORDER BY PLFR",
                            pclDPXUrno,pclSTFUrno);
                    sprintf(pclSqlBuf,"SELECT %s FROM JOBTAB %s",pclFieldList,pclSelection);
                    slCursor = 0;
                    slFkt = START;
                    dbg(DEBUG,"%s SQL (NEW) = <%s>",pclFunc,pclSqlBuf);
                    ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                    while (ilRCdb == DB_SUCCESS)
                    {
                       BuildItemBuffer(pclDataBuf,"",8,",");
                       get_real_item(pclUrno,pclDataBuf,1);
                       get_real_item(pclUprm,pclDataBuf,2);
                       get_real_item(pclUaft,pclDataBuf,3);
                       get_real_item(pclText,pclDataBuf,4);
                       get_real_item(pclPlfr,pclDataBuf,5);
                       get_real_item(pclPlto,pclDataBuf,6);
                       get_real_item(pclUaid,pclDataBuf,7);
                       get_real_item(pclUghs,pclDataBuf,8);
                       if (*pclUaid == '0')
                          strcpy(pclUaid,"");
                       sprintf(pclSelection2,"WHERE URNO = %s",pclUghs);
                       sprintf(pclSqlBuf2,"SELECT DTYP,FRMT,SNAM FROM SERTAB %s",pclSelection2);
                       slCursor2 = 0;
                       slFkt2 = START;
                       dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf2);
                       ilRCdb2 = sql_if(slFkt2,&slCursor2,pclSqlBuf2,pclTmpBuf);
                       close_my_cursor(&slCursor2);
                       if (ilRCdb2 == DB_SUCCESS)
                       {
                          BuildItemBuffer(pclTmpBuf,"",3,",");
                          get_real_item(pclDtyp,pclTmpBuf,1);
                          get_real_item(pclFrmt,pclTmpBuf,2);
                          get_real_item(pclText,pclTmpBuf,3);
                       }
                       else
                       {
                          strcpy(pclDtyp,"");
                          strcpy(pclFrmt,"");
                          strcpy(pclText,"");
                       }
                       if ((*pclAsgnType == '1' && *pclDtyp != 'O') ||
                           (*pclAsgnType == '2' && *pclDtyp == 'O'))
                       {
                          if (ilRecs > 0)
                          {
                             ilRC = MarkOutput();
                             if (ilRecs == 1)
                                ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"S");
                             else
                                ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"A");
                             ilRC = ClearSection(pclSection);
                          }
                          sprintf(pclDataBuf,"%d,%s,%s,%s,%s,%s,%s",
                                  ilRecs+1,pclUrno,pclText,pclDtyp,pclFrmt,pclPlfr,pclUaid);
                          strcpy(pclOldDataBuf,",,,,,,");
                          ilDatCnt = 0;
                          ilRC = StoreData(pclSectionFields,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
                          ilRecs++;
                          if (*pclPlto != ' ' && *pclPlto != '\0')
                          {
                             ilRC = MarkOutput();
                             if (ilRecs == 1)
                                ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"S");
                             else
                                ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"A");
                             ilRC = ClearSection(pclSection);
                             sprintf(pclDataBuf,"%d,%s,%s,%s,%s,%s,%s",
                                     ilRecs+1,pclUrno,pclText,pclDtyp,pclFrmt,pclPlto,pclUaid);
                             strcpy(pclOldDataBuf,",,,,,,");
                             ilDatCnt = 0;
                             ilRC = StoreData(pclSectionFields,pclDataBuf,pclOldDataBuf,ilNoEle,
                                              pclSection,&ilDatCnt);
                             ilRecs++;
                          }
                       }
                       slFkt = NEXT;
                       ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                    }
                    close_my_cursor(&slCursor);
                    ilRC = MarkOutput();
                    if (ilRecs == 1)
                       ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"C");
                    else
                       ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"E");
                 }
              }
           }
        }
     }
  }
  else if (strcmp(pcpType,"URT") == 0)
  {
     if (strncmp(pcpParam,"'",1) == 0)
     {
        strcpy(pclTmpBuf,&pcpParam[1]);
        pclTmpBuf[strlen(pclTmpBuf)-1] = '\0';
        strcpy(pcpParam,pclTmpBuf);
     }
     strcpy(pclFieldList,"USTF,UPRM,UAFT,TEXT,PLFR,PLTO,UAID,UGHS");
     /*sprintf(pclSelection,"WHERE URNO = %s AND SUBSTR(STAT,1,1) <> 'F'",pcpParam);*/
     sprintf(pclSelection,"WHERE URNO = %s AND SUBSTR(STAT,1,1) <> 'F' AND SUBSTR(STAT,5,1) = 'A'",pcpParam);
     sprintf(pclSqlBuf,"SELECT %s FROM JOBTAB %s",pclFieldList,pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",8,",");
        get_real_item(pclUstf,pclDataBuf,1);
        get_real_item(pclUprm,pclDataBuf,2);
        get_real_item(pclUaft,pclDataBuf,3);
        get_real_item(pclText,pclDataBuf,4);
        get_real_item(pclPlfr,pclDataBuf,5);
        get_real_item(pclPlto,pclDataBuf,6);
        get_real_item(pclUaid,pclDataBuf,7);
        get_real_item(pclUghs,pclDataBuf,8);
        if (*pclUstf == '0' || *pclUprm == '0' || *pclUaft == '0')
           return ilRC;
        sprintf(pclSelection,"WHERE USTF = %s",pclUstf);
        sprintf(pclSqlBuf,"SELECT IPAD FROM PDATAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclIpad);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           get_real_item(pclSection,pclPDASections,3);
           ilNoEle = 0;
           strcpy(pclFieldList,"");
           ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
           if (strlen(pclFieldList) > 0)
              pclFieldList[strlen(pclFieldList)-1] = '\0';
           ilDatCnt = 0;
           ilRC = StoreData(pclFieldList,pclIpad," ",ilNoEle,pclSection,&ilDatCnt);
           get_real_item(pclSection,pclPDASections,1);
           ilNoEle = 0;
           strcpy(pclFieldList,"");
           ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
           if (strlen(pclFieldList) > 0)
              pclFieldList[strlen(pclFieldList)-1] = '\0';
           sprintf(pclSelection,"WHERE URNO = %s",pclUprm);
           sprintf(pclSqlBuf,"SELECT %s FROM DPXTAB %s",pclFieldList,pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
           close_my_cursor(&slCursor);
           if (ilRCdb == DB_SUCCESS)
           {
              BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
              strcpy(pclOldDataBuf,"");
              for (ilI = 1; ilI < ilNoEle; ilI++)
                 strcat(pclOldDataBuf,",");
              ilDatCnt = 0;
              ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
              sprintf(pclSelection,"WHERE URNO = %s",pclUaft);
              sprintf(pclSqlBuf,"SELECT ADID FROM AFTTAB %s",pclSelection);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pcgAdid);
              close_my_cursor(&slCursor);
              if (ilRCdb == DB_SUCCESS)
              {
                 get_real_item(pclSection,pclPDASections,4);
                 ilNoEle = 0;
                 strcpy(pclFieldList,"");
                 ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
                 if (strlen(pclFieldList) > 0)
                    pclFieldList[strlen(pclFieldList)-1] = '\0';
                 sprintf(pclSelection,"WHERE URNO = %s",pclUaft);
                 sprintf(pclSqlBuf,"SELECT %s FROM AFTTAB %s",pclFieldList,pclSelection);
                 slCursor = 0;
                 slFkt = START;
                 dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                 ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
                 close_my_cursor(&slCursor);
                 if (ilRCdb == DB_SUCCESS)
                 {
                    BuildItemBuffer(pclDataBuf,"",ilNoEle,",");
                    strcpy(pclOldDataBuf,"");
                    for (ilI = 1; ilI < ilNoEle; ilI++)
                       strcat(pclOldDataBuf,",");
                    ilDatCnt = 0;
                    ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
                    get_real_item(pclSection,pclPDASections,2);
                    strcpy(pclFieldList,"");
                    ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
                    if (strlen(pclFieldList) > 0)
                       pclFieldList[strlen(pclFieldList)-1] = '\0';
                    if (*pclUaid == '0')
                       strcpy(pclUaid,"");
                    sprintf(pclSelection,"WHERE URNO = %s",pclUghs);
                    sprintf(pclSqlBuf,"SELECT DTYP,FRMT,SNAM FROM SERTAB %s",pclSelection);
                    slCursor = 0;
                    slFkt = START;
                    dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
                    ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclTmpBuf);
                    close_my_cursor(&slCursor);
                    if (ilRCdb == DB_SUCCESS)
                    {
                       BuildItemBuffer(pclTmpBuf,"",3,",");
                       get_real_item(pclDtyp,pclTmpBuf,1);
                       get_real_item(pclFrmt,pclTmpBuf,2);
                       get_real_item(pclText,pclTmpBuf,3);
                    }
                    else
                    {
                       strcpy(pclDtyp,"");
                       strcpy(pclFrmt,"");
                       strcpy(pclText,"");
                    }
                    sprintf(pclDataBuf,"1,%s,%s,%s,%s,%s,%s",
                            pcpParam,pclText,pclDtyp,pclFrmt,pclPlfr,pclUaid);
                    strcpy(pclOldDataBuf,",,,,,,");
                    ilDatCnt = 0;
                    ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
                    if (*pclPlto != ' ' && *pclPlto != '\0')
                    {
                       ilRC = MarkOutput();
                       ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"S");
                       ilRC = ClearSection(pclSection);
                       sprintf(pclDataBuf,"2,%s,%s,%s,%s,%s,%s",
                               pcpParam,pclText,pclDtyp,pclFrmt,pclPlto,pclUaid);
                       strcpy(pclOldDataBuf,",,,,,,");
                       ilDatCnt = 0;
                       ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
                       ilRC = MarkOutput();
                       ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"E");
                    }
                    else
                    {
                       ilRC = MarkOutput();
                       ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"C");
                    }
                 }
              }
           }
        }
     }
  }
  else if (strcmp(pcpType,"DLWR") == 0 || strcmp(pcpType,"DLRA") == 0)
  {
     get_real_item(pclSection,pclPDASections,3);
     ilNoEle = 0;
     strcpy(pclFieldList,"");
     ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     ilDatCnt = 0;
     ilRC = StoreData(pclFieldList,pcpParam," ",ilNoEle,pclSection,&ilDatCnt);
     get_real_item(pclSection,pclPDASections,1);
     ilNoEle = 0;
     strcpy(pclFieldList,"");
     ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     sprintf(pclDataBuf,",,,BLND,,,,%s",pcgCurrentTime);
     strcpy(pclOldDataBuf,",,,,,,,");
     ilDatCnt = 0;
     ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
     get_real_item(pclSection,pclPDASections,4);
     ilNoEle = 0;
     strcpy(pclFieldList,"");
     ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
     if (strlen(pclFieldList) > 0)
        pclFieldList[strlen(pclFieldList)-1] = '\0';
     sprintf(pclDataBuf,"A,,%s,",pcgCurrentTime);
     strcpy(pclOldDataBuf,",,,");
     ilDatCnt = 0;
     ilRC = StoreData(pclFieldList,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
     if (strcmp(pcpType,"DLWR") == 0)
     {
        get_real_item(pclSection,pclPDASections,2);
        ilNoEle = 0;
        strcpy(pclFieldList,"");
        ilRC = GetFieldList(pclSection,pclFieldList,&ilNoEle);
        if (strlen(pclFieldList) > 0)
           pclFieldList[strlen(pclFieldList)-1] = '\0';
        ilRecs = 0;
        sprintf(pclSqlBuf,"SELECT URNO,WNAM FROM WROTAB");
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        while (ilRCdb == DB_SUCCESS)
        {
           if (ilRecs > 0)
           {
              ilRC = MarkOutput();
              if (ilRecs == 1)
                 ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"S");
              else
                 ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"A");
              ilRC = ClearSection(pclSection);
           }
           BuildItemBuffer(pclDataBuf,"",2,",");
           sprintf(pclTmpBuf,"%d,%s,E,T,,",ilRecs,pclDataBuf);
           strcpy(pclOldDataBuf,"");
           for (ilI = 1; ilI < ilNoEle; ilI++)
              strcat(pclOldDataBuf,",");
           ilDatCnt = 0;
           ilRC = StoreData(pclFieldList,pclTmpBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
           ilRecs++;
           slFkt = NEXT;
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        }
        close_my_cursor(&slCursor);
        ilRC = MarkOutput();
        if (ilRecs == 1)
           ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"C");
        else
           ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"E");
     }
     else
     {
        get_real_item(pclSection,pclPDASections,2);
        ilNoEle = 0;
        strcpy(pclSectionFields,"");
        ilRC = GetFieldList(pclSection,pclSectionFields,&ilNoEle);
        if (strlen(pclSectionFields) > 0)
           pclSectionFields[strlen(pclSectionFields)-1] = '\0';
        sprintf(pclSqlBuf,"SELECT USTF FROM PDATAB WHERE IPAD = '%s'",pcpParam);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclUstf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           ilRecs = 0;
           sprintf(pclSelection,"WHERE DTYP = 'O'");
           sprintf(pclSqlBuf,"SELECT URNO,DTYP,FRMT,SNAM FROM SERTAB %s",pclSelection);
           slCursor = 0;
           slFkt = START;
           dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
           ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclTmpBuf);
           while (ilRCdb == DB_SUCCESS)
           {
              BuildItemBuffer(pclTmpBuf,"",4,",");
              get_real_item(pclUrno,pclTmpBuf,1);
              get_real_item(pclDtyp,pclTmpBuf,2);
              get_real_item(pclFrmt,pclTmpBuf,3);
              get_real_item(pclText,pclTmpBuf,4);
              if (ilRecs > 0)
              {
                 ilRC = MarkOutput();
                 if (ilRecs == 1)
                    ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"S");
                 else
                    ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"A");
                 ilRC = ClearSection(pclSection);
              }
              sprintf(pclDataBuf,"%d,%s,%s,%s,%s,,",
                      ilRecs+1,pclUrno,pclText,pclDtyp,pclFrmt);
              strcpy(pclOldDataBuf,",,,,,,");
              ilDatCnt = 0;
              ilRC = StoreData(pclSectionFields,pclDataBuf,pclOldDataBuf,ilNoEle,pclSection,&ilDatCnt);
              ilRecs++;
              slFkt = NEXT;
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclTmpBuf);
           }
           close_my_cursor(&slCursor);
           ilRC = MarkOutput();
           if (ilRecs == 1)
              ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"C");
           else
              ilRC = BuildOutput(ipIndex,&ilCount,pclSection,NULL,pcpCurIntf,"E");
        }
     }
  }

  return ilRC;
} /* End of HandlePDAOut */


static int HandleWEBIn()
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleWEBIn:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSelection[1024];
  char pclSqlBuf[1024];
  char pclDataBuf[8192];
  int ilIdx;
  int ilPSA;
  char pclFldLstA[1024];
  char pclDatLstA[1024];
  char pclFldLstD[1024];
  char pclDatLstD[1024];
  char pclAlcArr[8];
  char pclFlnArr[8];
  char pclDatArr[16];
  char pclAlcDep[8];
  char pclFlnDep[8];
  char pclDatDep[16];
  char pclFlnoArr[16];
  char pclAlc2Arr[16];
  char pclAlc3Arr[16];
  char pclFltnArr[16];
  char pclStoaBgn[16];
  char pclStoaEnd[16];
  char pclFlnoDep[16];
  char pclAlc2Dep[16];
  char pclAlc3Dep[16];
  char pclFltnDep[16];
  char pclStodBgn[16];
  char pclStodEnd[16];
  int ilTimeDiffLocalUtc;
  int ilTDIS = -1;
  int ilTDIW = -1;
  char pclUrnoArr[16];
  char pclStofArr[16];
  char pclFltiArr[16];
  char pclUrnoDep[16];
  char pclStofDep[16];
  char pclFltiDep[16];
  char pclTmpBuf[1024];
  char pclTable[16];
  char pclModId[16];
  char pclCommand[16];
  char pclPriority[16];
  int ilPriority;
  int ilModId;
  int ilWriteEmpty = FALSE;
  /* Frank */
  int ilI;
  char pclAction[16];
  int ilNoEleField = 0;
  int ilNoEleData = 0;
  char pclTmpBuf1[1024];
  char pclTmpBuf2[1024];
  char pclTmpBuf3[1024];
  char pclUrnoDxp[16];
  /* Frank */

  ilPSA = FALSE;
  ilIdx = GetXmlLineIdx("DAT","Scalo","form_table_PSA");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
  {
     if (strcmp(&rgXml.rlLine[ilIdx].pclData[0],"Fiumicino") == 0)
     {
        ilPSA = TRUE;
        dbg(DEBUG,"%s Accept PSA Data for Fiumicino",pclFunc);
     }
  }
  if (ilPSA == FALSE)
  {
     ilIdx = GetXmlLineIdx("DAT","Scalo","form_table_PPV");
     if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     {
        if (strcmp(&rgXml.rlLine[ilIdx].pclData[0],"Fiumicino") == 0)
           dbg(DEBUG,"%s Accept PPV Data for Fiumicino",pclFunc);
        else
        {
           dbg(DEBUG,"%s No Data for Fiumicino received",pclFunc);
           return ilRC;
        }
     }
     else
     {
        dbg(DEBUG,"%s No Data for Fiumicino received",pclFunc);
        return ilRC;
     }
  }

  strcpy(pclFldLstA,"");
  strcpy(pclDatLstA,"");
  strcpy(pclFldLstD,"");
  strcpy(pclDatLstD,"");
  ilRC = GetWebData(ilPSA,"Id","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Nome","Cognome",'N','N',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Lingua_preferita","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Sesso","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Email","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  if (ilPSA == TRUE)
  {
     ilIdx = GetXmlLineIdx("DAT","Disabilita_motoria","form_table_PSA");
     if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     {
        strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pclFldLstA,",");
        strcat(pclDatLstA,&rgXml.rlLine[ilIdx].pclData[0]);
        strcat(pclDatLstA,",");
        strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pclFldLstD,",");
        strcat(pclDatLstD,&rgXml.rlLine[ilIdx].pclData[0]);
        strcat(pclDatLstD,",");
     }
     else
     {
        ilIdx = GetXmlLineIdx("DAT","Non_vedenti","form_table_PSA");
        if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0 &&
            strcmp(&rgXml.rlLine[ilIdx].pclData[0],"YES") == 0)
        {
           strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstA,",");
           strcat(pclDatLstA,"BLND,");
           strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstD,",");
           strcat(pclDatLstD,"BLND,");
        }
        else
        {
           ilIdx = GetXmlLineIdx("DAT","Non_udenti","form_table_PSA");
           if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0 &&
               strcmp(&rgXml.rlLine[ilIdx].pclData[0],"YES") == 0)
           {
              strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
              strcat(pclFldLstA,",");
              strcat(pclDatLstA,"DEAF,");
              strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
              strcat(pclFldLstD,",");
              strcat(pclDatLstD,"DEAF,");
           }
        }
     }
  }
  if (ilPSA == TRUE)
  {
     ilIdx = GetXmlLineIdx("DAT","Sedia_a_ruote_propria","form_table_PSA");
     if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     {
        strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pclFldLstA,",");
        if (strcmp(&rgXml.rlLine[ilIdx].pclData[0],"YES") == 0)
           strcat(pclDatLstA,"O,");
        else
           strcat(pclDatLstA,"R,");
        strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pclFldLstD,",");
        if (strcmp(&rgXml.rlLine[ilIdx].pclData[0],"YES") == 0)
           strcat(pclDatLstD,"O,");
        else
           strcat(pclDatLstD,"R,");
     }
  }
  if (ilPSA == TRUE)
  {
     ilIdx = GetXmlLineIdx("DAT","Tipologia_di_sedia_utilizzata","form_table_PSA");
     if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     {
        sprintf(pclSelection,"WHERE ENAM = '%s'",&rgXml.rlLine[ilIdx].pclData[0]);
        sprintf(pclSqlBuf,"SELECT GCDE FROM EQUTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstA,",");
           strcat(pclDatLstA,pclDataBuf);
           strcat(pclDatLstA,",");
           strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstD,",");
           strcat(pclDatLstD,pclDataBuf);
           strcat(pclDatLstD,",");
        }
     }
  }
  ilRC = GetWebData(ilPSA,"Presenza_accompagnatore","",'N',' ',1,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Presenza_cane_guida","",'N',' ',1,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  if (ilPSA == TRUE)
  {
     ilIdx = GetXmlLineIdx("DAT","Mezzo_di_trasporto_utilizzato","form_table_PSA");
     if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     {
        sprintf(pclSelection,"WHERE ENAM = '%s'",&rgXml.rlLine[ilIdx].pclData[0]);
        sprintf(pclSqlBuf,"SELECT GCDE FROM EQUTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           strcat(pclFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstA,",");
           strcat(pclDatLstA,pclDataBuf);
           strcat(pclDatLstA,",");
           strcat(pclFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
           strcat(pclFldLstD,",");
           strcat(pclDatLstD,pclDataBuf);
           strcat(pclDatLstD,",");
        }
     }
  }
  ilRC = GetWebData(ilPSA,"Assistenza_richiesta","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
/*
  ilRC = GetWebData(ilPSA,"Note","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
*/
  ilRC = GetWebData(ilPSA,"Posto_preassegnato_partenza","",'N',' ',0,NULL,NULL,pclFldLstD,pclDatLstD);
  ilRC = GetWebData(ilPSA,"Posto_preassegnato_arrivo","",'N',' ',0,pclFldLstA,pclDatLstA,NULL,NULL);
/*
  ilRC = GetWebData(ilPSA,"User_vettore","",'N',' ',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
*/
  ilRC = GetWebData(ilPSA,"System_data","System_ora",'D','T',0,pclFldLstA,pclDatLstA,pclFldLstD,pclDatLstD);
  strcat(pclFldLstA,"ABSR,");
  strcat(pclDatLstA,"WEB,");
  strcat(pclFldLstD,"ABSR,");
  strcat(pclDatLstD,"WEB,");

  strcpy(pclAlcArr,"");
  strcpy(pclFlnArr,"");
  strcpy(pclDatArr,"");
  strcpy(pclAlcDep,"");
  strcpy(pclFlnDep,"");
  strcpy(pclDatDep,"");
  strcpy(pclFlnoArr,"");
  strcpy(pclStoaBgn,"");
  strcpy(pclStoaEnd,"");
  strcpy(pclFlnoDep,"");
  strcpy(pclStodBgn,"");
  strcpy(pclStodEnd,"");
  strcpy(pclUrnoArr,"");
  strcpy(pclStofArr,"");
  strcpy(pclFltiArr,"");
  strcpy(pclUrnoDep,"");
  strcpy(pclStofDep,"");
  strcpy(pclFltiDep,"");
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Vettore_volo_arrivo","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Vettore_volo_arrivo","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclAlcArr,&rgXml.rlLine[ilIdx].pclData[0]);
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Numero_volo_arrivo","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Numero_volo_arrivo","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclFlnArr,&rgXml.rlLine[ilIdx].pclData[0]);
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Data_volo_arrivo","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Data_volo_arrivo","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclDatArr,&rgXml.rlLine[ilIdx].pclData[0]);
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Vettore_volo_partenza","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Vettore_volo_partenza","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclAlcDep,&rgXml.rlLine[ilIdx].pclData[0]);
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Numero_volo_partenza","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Numero_volo_partenza","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclFlnDep,&rgXml.rlLine[ilIdx].pclData[0]);
  if (ilPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT","Data_volo_partenza","form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT","Data_volo_partenza","form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
     strcpy(pclDatDep,&rgXml.rlLine[ilIdx].pclData[0]);
  if (strlen(pclAlcArr) > 1 && strlen(pclFlnArr) > 1 && strlen(pclDatArr) > 1)
  {
     strcpy(pclAlc2Arr,"");
     strcpy(pclAlc3Arr,"");
     strcpy(pclFlnoArr,pclAlcArr);
     if (strlen(pclFlnoArr) == 2)
     {
        strcat(pclFlnoArr," ");
        strcpy(pclAlc2Arr,pclAlcArr);
     }
     else
        strcpy(pclAlc3Arr,pclAlcArr);
     strcat(pclFlnoArr,pclFlnArr);
     strcpy(pclFltnArr,pclFlnArr);
     if (pclDatArr[4] == '/')
     {
        strncpy(pclStoaBgn,pclDatArr,4);
        pclStoaBgn[4] = '\0';
        strncat(pclStoaBgn,&pclDatArr[5],2);
        pclStoaBgn[6] = '\0';
        strncat(pclStoaBgn,&pclDatArr[8],2);
        pclStoaBgn[8] = '\0';
     }
     else
     {
        strncpy(pclStoaBgn,&pclDatArr[6],4);
        pclStoaBgn[4] = '\0';
        strncat(pclStoaBgn,&pclDatArr[3],2);
        pclStoaBgn[6] = '\0';
        strncat(pclStoaBgn,pclDatArr,2);
        pclStoaBgn[8] = '\0';
     }
     strcpy(pclStoaEnd,pclStoaBgn);
     strcat(pclStoaBgn,"000000");
     strcat(pclStoaEnd,"235959");
     ilTimeDiffLocalUtc = GetTimeDiffUtc(pclStoaBgn,&ilTDIS,&ilTDIW);
     ilRC = MyAddSecondsToCEDATime(pclStoaBgn,ilTimeDiffLocalUtc*60*(-1),1);
     ilTimeDiffLocalUtc = GetTimeDiffUtc(pclStoaEnd,&ilTDIS,&ilTDIW);
     ilRC = MyAddSecondsToCEDATime(pclStoaEnd,ilTimeDiffLocalUtc*60*(-1),1);
     if (strlen(pclAlc2Arr) > 0)
        sprintf(pclSelection,"WHERE ALC2 = '%s' AND FLTN = '%s' AND STOA BETWEEN '%s' AND '%s' AND ADID = 'A'",
                pclAlc2Arr,pclFltnArr,pclStoaBgn,pclStoaEnd);
     else if (strlen(pclAlc3Arr) > 0)
        sprintf(pclSelection,"WHERE ALC3 = '%s' AND FLTN = '%s' AND STOA BETWEEN '%s' AND '%s' AND ADID = 'A'",
                pclAlc3Arr,pclFltnArr,pclStoaBgn,pclStoaEnd);
     else
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA BETWEEN '%s' AND '%s' AND ADID = 'A'",
                pclFlnoArr,pclStoaBgn,pclStoaEnd);
     sprintf(pclSqlBuf,"SELECT URNO,STOA,FLTI FROM AFTTAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",3,",");
        get_real_item(pclUrnoArr,pclDataBuf,1);
        get_real_item(pclStofArr,pclDataBuf,2);
        get_real_item(pclFltiArr,pclDataBuf,3);
        strcat(pclFldLstA,"FLNU,SATI,AATI,");
        sprintf(pclTmpBuf,"%s,%s,%s,",pclUrnoArr,pclStofArr,pclStofArr);
        strcat(pclDatLstA,pclTmpBuf);
     }
  }
  else
     dbg(TRACE,"%s No Arrival Flight Found",pclFunc);
  if (strlen(pclAlcDep) > 1 && strlen(pclFlnDep) > 1 && strlen(pclDatDep) > 1)
  {
     strcpy(pclAlc2Dep,"");
     strcpy(pclAlc3Dep,"");
     strcpy(pclFlnoDep,pclAlcDep);
     if (strlen(pclFlnoDep) == 2)
     {
        strcat(pclFlnoDep," ");
        strcpy(pclAlc2Dep,pclAlcDep);
     }
     else
        strcpy(pclAlc3Dep,pclAlcDep);
     strcat(pclFlnoDep,pclFlnDep);
     strcpy(pclFltnDep,pclFlnDep);
     if (pclDatDep[4] == '/')
     {
        strncpy(pclStodBgn,pclDatDep,4);
        pclStodBgn[4] = '\0';
        strncat(pclStodBgn,&pclDatDep[5],2);
        pclStodBgn[6] = '\0';
        strncat(pclStodBgn,&pclDatDep[8],2);
        pclStodBgn[8] = '\0';
     }
     else
     {
        strncpy(pclStodBgn,&pclDatDep[6],4);
        pclStodBgn[4] = '\0';
        strncat(pclStodBgn,&pclDatDep[3],2);
        pclStodBgn[6] = '\0';
        strncat(pclStodBgn,pclDatDep,2);
        pclStodBgn[8] = '\0';
     }
     strcpy(pclStodEnd,pclStodBgn);
     strcat(pclStodBgn,"000000");
     strcat(pclStodEnd,"235959");
     ilTimeDiffLocalUtc = GetTimeDiffUtc(pclStodBgn,&ilTDIS,&ilTDIW);
     ilRC = MyAddSecondsToCEDATime(pclStodBgn,ilTimeDiffLocalUtc*60*(-1),1);
     ilTimeDiffLocalUtc = GetTimeDiffUtc(pclStodEnd,&ilTDIS,&ilTDIW);
     ilRC = MyAddSecondsToCEDATime(pclStodEnd,ilTimeDiffLocalUtc*60*(-1),1);
     if (strlen(pclAlc2Dep) > 0)
        sprintf(pclSelection,"WHERE ALC2 = '%s' AND FLTN = '%s' AND STOD BETWEEN '%s' AND '%s' AND ADID = 'D'",
                pclAlc2Dep,pclFltnDep,pclStodBgn,pclStodEnd);
     else if (strlen(pclAlc3Dep) > 0)
        sprintf(pclSelection,"WHERE ALC3 = '%s' AND FLTN = '%s' AND STOD BETWEEN '%s' AND '%s' AND ADID = 'D'",
                pclAlc3Dep,pclFltnDep,pclStodBgn,pclStodEnd);
     else
        sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD BETWEEN '%s' AND '%s' AND ADID = 'D'",
                pclFlnoDep,pclStodBgn,pclStodEnd);
     sprintf(pclSqlBuf,"SELECT URNO,STOD,FLTI FROM AFTTAB %s",pclSelection);
     slCursor = 0;
     slFkt = START;
     dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
     ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
     close_my_cursor(&slCursor);
     if (ilRCdb == DB_SUCCESS)
     {
        BuildItemBuffer(pclDataBuf,"",3,",");
        get_real_item(pclUrnoDep,pclDataBuf,1);
        get_real_item(pclStofDep,pclDataBuf,2);
        get_real_item(pclFltiDep,pclDataBuf,3);
        if (*pclFltiDep == 'I')
           ilRC = MyAddSecondsToCEDATime(pclStofDep,120*60*(-1),1);
        else if (*pclFltiDep == 'S')
           ilRC = MyAddSecondsToCEDATime(pclStofDep,75*60*(-1),1);
        else
           ilRC = MyAddSecondsToCEDATime(pclStofDep,60*60*(-1),1);
        strcat(pclFldLstD,"FLNU,SATI,AATI,");
        sprintf(pclTmpBuf,"%s,%s,%s,",pclUrnoDep,pclStofDep,pclStofDep);
        strcat(pclDatLstD,pclTmpBuf);
     }
  }
  else
     dbg(TRACE,"%s No Departure Flight Found",pclFunc);

  strcat(pclFldLstA,"CDAT,USEC,");
  sprintf(pclTmpBuf,"%s,EXCHDL,",pcgCurrentTime);
  strcat(pclDatLstA,pclTmpBuf);
  strcat(pclFldLstD,"CDAT,USEC,");
  sprintf(pclTmpBuf,"%s,EXCHDL,",pcgCurrentTime);
  strcat(pclDatLstD,pclTmpBuf);

  if (strlen(pclFldLstA) > 0)
  {
     pclFldLstA[strlen(pclFldLstA)-1] = '\0';
     pclDatLstA[strlen(pclDatLstA)-1] = '\0';
  }
  if (strlen(pclFldLstD) > 0)
  {
     pclFldLstD[strlen(pclFldLstD)-1] = '\0';
     pclDatLstD[strlen(pclDatLstD)-1] = '\0';
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"WEB_STD","table",CFG_STRING,pclTable);
  if (ilRC != RC_SUCCESS)
     strcpy(pclTable,"DPXTAB");
  ilRC = iGetConfigEntry(pcgConfigFile,"WEB_STD","mod_id",CFG_STRING,pclModId);
  if (ilRC != RC_SUCCESS)
     strcpy(pclModId,"506");

  //Frank@Rome
  ilRC = iGetConfigEntry(pcgConfigFile,"WEB_STD","snd_cmd",CFG_STRING,pclCommand);
  if (ilRC != RC_SUCCESS)
     strcpy(pclCommand,"IRT");
  else if( !strcmp( pclCommand, "RequestType" ) )
  {
      dbg(DEBUG,"line<%d>choose the command according to RequestType:DELETE->DBT; INSERT->IRT",__LINE__);

      for (ilI = 0; ilI < igCurXmlLines; ilI++)
      {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0)
        {
            strcpy(pclAction,&rgXml.rlLine[ilI].pclData[0]);
            break;
        }
      }

      if (strcmp(pclAction,pcgActionTypeDel) == 0)//delete  sqlhdl:DRT,DBT - Delete a record (ibthdl,damhdl).
      {
          strcpy(pclCommand,"DRT");
          dbg(DEBUG,"the command is DRT");
      }
      else if (strcmp(pclAction,pcgActionTypeIns) == 0)//insert  sqlhdl:IBT - Insert a record   (damhdl);IRT - Insert record specifying URNO explicitly.
      {
          strcpy(pclCommand,"IRT");
          dbg(DEBUG,"the command is IRT");
       }
  }
//Frank@Rome

  ilRC = iGetConfigEntry(pcgConfigFile,"WEB_STD","priority",CFG_STRING,pclPriority);
  if (ilRC != RC_SUCCESS)
     strcpy(pclPriority,"3");
  ilPriority = atoi(pclPriority);
  ilModId = atoi(pclModId);
  ilWriteEmpty = FALSE;
  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","WRITE_EMPTY_DPX",CFG_STRING,pclTmpBuf);
  if (ilRC == RC_SUCCESS && strcmp(pclTmpBuf,"YES") == 0)
     ilWriteEmpty = TRUE;

/* Inform other process specified by ilModId <start>*/
  //Arrival
  if (strlen(pclUrnoArr) > 0 || ilWriteEmpty == TRUE)
  {
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
         pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFldLstA);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDatLstA);

     //PSA only has insert operation
     //if(igPSAInsertOnly == 1)
     if(ilPSA == TRUE)
     {
        dbg(DEBUG,"ilPSA=<%d>",ilPSA);
        ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                              "IRT",pclTable,"",pclFldLstA,pclDatLstA,"",
                                ilPriority,NETOUT_NO_ACK);
     }
     else
     {
         if(strncmp(pclCommand,"IRT",3)==0)
         {
                ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                  pclCommand,pclTable,"",pclFldLstA,pclDatLstA,"",
                                ilPriority,NETOUT_NO_ACK);
         }
         else if(strncmp(pclCommand,"DRT",3)==0)
         {
             memset(pclTmpBuf, 0,sizeof(pclTmpBuf));
             sprintf(pclTmpBuf,"SELECT URNO FROM %s WHERE ",pclTable);
             memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
             memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
             memset(pclTmpBuf3,0,sizeof(pclTmpBuf3));
             ilNoEleField = 0;
             ilNoEleData  = 0;
                 ilNoEleField = GetNoOfElements(pclFldLstA,',');
                 ilNoEleData  = GetNoOfElements(pclDatLstA,',');

                 if( (ilNoEleField != 0) && ( ilNoEleField == ilNoEleData ))
                 {
                        for (ilI = 1; ilI <= ilNoEleData; ilI++)
                        {
                            memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
                        memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
                        memset(pclTmpBuf3,0,sizeof(pclTmpBuf3));
                            get_real_item(pclTmpBuf1,pclFldLstA,ilI);
                            if(strncmp(pclTmpBuf1,"CDAT",4)==0)
                            {
                                continue;
                            }
                            get_real_item(pclTmpBuf2,pclDatLstA,ilI);
                            if(strlen(pclTmpBuf2) == 0)
                            {
                                if(ilI < ilNoEleData)
                                {
                                    sprintf(pclTmpBuf3,"%s IS NULL AND ",pclTmpBuf1,pclTmpBuf2);
                                }
                                else
                                {
                                    sprintf(pclTmpBuf3,"%s IS NULL",pclTmpBuf1,pclTmpBuf2);
                                }
                            }
                            else
                            {
                                if(ilI < ilNoEleData)
                                {
                                    sprintf(pclTmpBuf3,"%s='%s' AND ",pclTmpBuf1,pclTmpBuf2);
                                }
                                else
                                {
                                    sprintf(pclTmpBuf3,"%s='%s'",pclTmpBuf1,pclTmpBuf2);
                                }
                            }
                            strcat(pclTmpBuf,pclTmpBuf3);
                        }
                 }
                 //dbg(DEBUG,"line<%d>Where clause<%s>--------------",__LINE__,pclTmpBuf);

                 slCursor = 0;
             slFkt = START;
             memset(pclDataBuf,0,sizeof(pclDataBuf));
             memset(pclSqlBuf,0,sizeof(pclSqlBuf));
             strcpy(pclSqlBuf,pclTmpBuf);
             dbg(DEBUG,"--------------------------line<%d><Arrival>----------------------------------",__LINE__);
             dbg(DEBUG,"line<%d>SQL<%s>",__LINE__,pclSqlBuf);
             ilRC = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
             close_my_cursor(&slCursor);
             if(ilRC == DB_SUCCESS)
             {
                    memset(pclUrnoDxp,0,sizeof(pclUrnoDxp));
                    BuildItemBuffer(pclDataBuf,"",1,",");
                get_real_item(pclUrnoDxp,pclDataBuf,1);

                memset(pclSelection,0,sizeof(pclSelection));
                sprintf(pclSelection,"WHERE URNO=%s",pclUrnoDxp);
                dbg(DEBUG,"line<%d>Where clause<%s>++++++++++",__LINE__,pclSelection);

                ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                                  pclCommand,pclTable,pclSelection,pclFldLstA,pclDatLstA,"",
                                  ilPriority,NETOUT_NO_ACK);
             }
             else
             {
                    dbg(DEBUG,"line<%d> URNO is not found in DXPTAB, SendCedaEvent is not called<Arrival>",__LINE__);
             }
         }
       }
     dbg(DEBUG,"--------------------------line<%d><Arrival>----------------------------------",__LINE__);

     if (strlen(pclUrnoArr) > 0)
     {
        sprintf(pclSelection,"WHERE URNO = %s",pclUrnoArr);
        ilRC = SendCedaEvent(7800,0,pcgIntfName,"CEDA",
                             pcgTwStart,pcgTwEnd,
                             "UFR","AFTTAB",pclSelection,
                             "PRMC","Y","",3,NETOUT_NO_ACK);
        dbg(DEBUG,"%s Selection <%s>",pclFunc,pclSelection);
        dbg(DEBUG,"%s FieldList <PRMC>",pclFunc);
        dbg(DEBUG,"%s ValueList <Y>",pclFunc);
        dbg(DEBUG,"%s Sent <UFR>. Returned <%d>",pclFunc,ilRC);
     }
  }

  //Departure
  if (strlen(pclUrnoDep) > 0)
  {
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",
         pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFldLstD);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclDatLstD);

     //----------------------
   if(ilPSA == TRUE)
   {
    dbg(DEBUG,"ilPSA=<%d>",ilPSA);
    ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                            "IRT",pclTable,"",pclFldLstD,pclDatLstD,"",
                              ilPriority,NETOUT_NO_ACK);
   }
   else
   {
     if(strncmp(pclCommand,"IRT",3)==0)
     {
        ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                            pclCommand,pclTable,"",pclFldLstD,pclDatLstD,"",
                              ilPriority,NETOUT_NO_ACK);
     }
     else if(strncmp(pclCommand,"DRT",3)==0)
     {
         memset(pclTmpBuf, 0,sizeof(pclTmpBuf));
         sprintf(pclTmpBuf,"SELECT URNO FROM %s WHERE ",pclTable);
         memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
         memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
         memset(pclTmpBuf3,0,sizeof(pclTmpBuf3));
         ilNoEleField = 0;
         ilNoEleData  = 0;
             ilNoEleField = GetNoOfElements(pclFldLstD,',');
             ilNoEleData  = GetNoOfElements(pclDatLstD,',');

             if( (ilNoEleField != 0) && ( ilNoEleField == ilNoEleData ))
             {
                    for (ilI = 1; ilI <= ilNoEleData; ilI++)
                    {
                        memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
                    memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
                    memset(pclTmpBuf3,0,sizeof(pclTmpBuf3));
                        get_real_item(pclTmpBuf1,pclFldLstD,ilI);
                        if(strncmp(pclTmpBuf1,"CDAT",4)==0)
                        {
                            continue;
                        }
                        get_real_item(pclTmpBuf2,pclDatLstD,ilI);
                        if(strlen(pclTmpBuf2) == 0)
                        {
                            if(ilI < ilNoEleData)
                            {
                                sprintf(pclTmpBuf3,"%s IS NULL AND ",pclTmpBuf1,pclTmpBuf2);
                            }
                            else
                            {
                                sprintf(pclTmpBuf3,"%s IS NULL",pclTmpBuf1,pclTmpBuf2);
                            }
                        }
                        else
                        {
                            if(ilI < ilNoEleData)
                            {
                                sprintf(pclTmpBuf3,"%s='%s' AND ",pclTmpBuf1,pclTmpBuf2);
                            }
                            else
                            {
                                sprintf(pclTmpBuf3,"%s='%s'",pclTmpBuf1,pclTmpBuf2);
                            }
                        }
                        strcat(pclTmpBuf,pclTmpBuf3);
                    }
             }
             //dbg(DEBUG,"line<%d>Where clause<%s>--------------",__LINE__,pclTmpBuf);

             slCursor = 0;
         slFkt = START;
         memset(pclDataBuf,0,sizeof(pclDataBuf));
         memset(pclSqlBuf,0,sizeof(pclSqlBuf));
         strcpy(pclSqlBuf,pclTmpBuf);
         dbg(DEBUG,"--------------------------line<%d><Departure>-------------------------",__LINE__);
         dbg(DEBUG,"line<%d>SQL<%s>",__LINE__,pclSqlBuf);
         ilRC = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
         close_my_cursor(&slCursor);
         if(ilRC == DB_SUCCESS)
         {
                memset(pclUrnoDxp,0,sizeof(pclUrnoDxp));
                BuildItemBuffer(pclDataBuf,"",1,",");
            get_real_item(pclUrnoDxp,pclDataBuf,1);

            memset(pclSelection,0,sizeof(pclSelection));
            sprintf(pclSelection,"WHERE URNO=%s",pclUrnoDxp);
            dbg(DEBUG,"line<%d>Where clause<%s>++++++++++",__LINE__,pclSelection);

            ilRC = SendCedaEvent(ilModId,0,pcgIntfName,"CEDA",pcgTwStart,pcgTwEnd,
                              pclCommand,pclTable,pclSelection,pclFldLstD,pclDatLstD,"",
                              ilPriority,NETOUT_NO_ACK);
         }
         else
         {
                dbg(DEBUG,"line<%d> URNO is not found in DXPTAB, SendCedaEvent is not called",__LINE__);
         }
     }
   }
     //-------------
     //dbg(DEBUG,"igPSAInsertOnly=<%d>",igPSAInsertOnly);
     dbg(DEBUG,"--------------------------line<%d><Departure>-------------------------",__LINE__);
     sprintf(pclSelection,"WHERE URNO = %s",pclUrnoDep);
     ilRC = SendCedaEvent(7800,0,pcgIntfName,"CEDA",
                          pcgTwStart,pcgTwEnd,
                          "UFR","AFTTAB",pclSelection,
                          "PRMC","Y","",3,NETOUT_NO_ACK);
     dbg(DEBUG,"%s Selection <%s>",pclFunc,pclSelection);
     dbg(DEBUG,"%s FieldList <PRMC>",pclFunc);
     dbg(DEBUG,"%s ValueList <Y>",pclFunc);
     dbg(DEBUG,"%s Sent <UFR>. Returned <%d>",pclFunc,ilRC);
  }
/* Inform other process specified by ilModId <end>*/

  if (strlen(pclUrnoArr) != 0 || strlen(pclUrnoDep) != 0)
  {
     if (strlen(pclUrnoArr) != 0 && strlen(pclUrnoDep) != 0)
        sprintf(pclSelection,"WHERE URNO IN (%s,%s)",pclUrnoArr,pclUrnoDep);
     else if (strlen(pclUrnoArr) != 0)
        sprintf(pclSelection,"WHERE URNO IN (%s)",pclUrnoArr);
     else
        sprintf(pclSelection,"WHERE URNO IN (%s)",pclUrnoDep);
     ilRC = SendCedaEvent(7540,0,pcgIntfName,"CEDA",
                          pcgTwStart,pcgTwEnd,
                          "SFC","",pclSelection,
                          "","","",3,NETOUT_NO_ACK);
     dbg(DEBUG,"%s Selection <%s>",pclFunc,pclSelection);
     dbg(DEBUG,"%s Sent <SFC> to flicol. Returned <%d>",pclFunc,ilRC);
  }

  return ilRC;
} /* End of HandleWEBIn */


static int GetWebData(int ipPSA, char *pcpTag1, char *pcpTag2, char cpMode1, char cpMode2, int ipLen,
                      char *pcpFldLstA, char *pcpDatLstA, char *pcpFldLstD, char *pcpDatLstD)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "GetWebData:";
  int ilIdx;

  if (ipPSA == TRUE)
     ilIdx = GetXmlLineIdx("DAT",pcpTag1,"form_table_PSA");
  else
     ilIdx = GetXmlLineIdx("DAT",pcpTag1,"form_table_PPV");
  if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
  {
     if (pcpFldLstA != NULL)
     {
        strcat(pcpFldLstA,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pcpFldLstA,",");
     }
     if (pcpFldLstD != NULL)
     {
        strcat(pcpFldLstD,&rgXml.rlLine[ilIdx].pclFieldA[0][0]);
        strcat(pcpFldLstD,",");
     }
     if (ipLen == 0)
     {
        if (cpMode1 == 'N')
        {
           if (pcpDatLstA != NULL)
              strcat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0]);
           if (pcpDatLstD != NULL)
              strcat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0]);
        }
        else if (cpMode1 == 'D')
        {
           if (pcpDatLstA != NULL)
           {
              if (rgXml.rlLine[ilIdx].pclData[4] == '/')
              {
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],4);
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[5],2);
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[8],2);
              }
              else
              {
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[6],4);
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[3],2);
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],2);
              }
           }
           if (pcpDatLstD != NULL)
           {
              if (rgXml.rlLine[ilIdx].pclData[4] == '/')
              {
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],4);
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[5],2);
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[8],2);
              }
              else
              {
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[6],4);
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[3],2);
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],2);
              }
           }
        }
        else if (cpMode1 == 'T')
        {
           if (pcpDatLstA != NULL)
           {
              strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],2);
              strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[3],2);
              strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[6],2);
           }
           if (pcpDatLstD != NULL)
           {
              strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],2);
              strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[3],2);
              strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[6],2);
           }
        }
     }
     else
     {
        if (pcpDatLstA != NULL)
           strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],ipLen);
        if (pcpDatLstD != NULL)
           strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],ipLen);
     }
     if (strlen(pcpTag2) > 0)
     {
        if (ipPSA == TRUE)
           ilIdx = GetXmlLineIdx("DAT",pcpTag2,"form_table_PSA");
        else
           ilIdx = GetXmlLineIdx("DAT",pcpTag2,"form_table_PPV");
        if (ilIdx >= 0 && rgXml.rlLine[ilIdx].ilRcvFlag >= 0)
        {
           if (cpMode2 == 'N')
           {
              if (pcpDatLstA != NULL)
                 strcat(pcpDatLstA," ");
              if (pcpDatLstD != NULL)
                 strcat(pcpDatLstD," ");
           }
           if (ipLen == 0)
           {
              if (cpMode2 == 'N')
              {
                 if (pcpDatLstA != NULL)
                    strcat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0]);
                 if (pcpDatLstD != NULL)
                    strcat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0]);
              }
              else if (cpMode2 == 'D')
              {
                 if (pcpDatLstA != NULL)
                 {
                    if (rgXml.rlLine[ilIdx].pclData[4] == '/')
                    {
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],4);
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[5],2);
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[8],2);
                    }
                    else
                    {
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[6],4);
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[3],2);
                       strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],2);
                    }
                 }
                 if (pcpDatLstD != NULL)
                 {
                    if (rgXml.rlLine[ilIdx].pclData[4] == '/')
                    {
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],4);
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[5],2);
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[8],2);
                    }
                    else
                    {
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[6],4);
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[3],2);
                       strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],2);
                    }
                 }
              }
              else if (cpMode2 == 'T')
              {
                 if (pcpDatLstA != NULL)
                 {
                    strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],2);
                    strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[3],2);
                    strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[6],2);
                 }
                 if (pcpDatLstD != NULL)
                 {
                    strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],2);
                    strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[3],2);
                    strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[6],2);
                 }
              }
           }
           else
           {
              if (pcpDatLstA != NULL)
                 strncat(pcpDatLstA,&rgXml.rlLine[ilIdx].pclData[0],ipLen);
              if (pcpDatLstD != NULL)
                 strncat(pcpDatLstD,&rgXml.rlLine[ilIdx].pclData[0],ipLen);
           }
        }
     }
     if (pcpDatLstA != NULL)
        strcat(pcpDatLstA,",");
     if (pcpDatLstD != NULL)
        strcat(pcpDatLstD,",");
  }

  return ilRC;
} /* End of GetWebData */


static int HandlePlbData(char *pcpFile)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandlePlbData:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[2048];
  char pclSelection[2048];
  char pclDataBuf[8000];
  char pclFieldList[2048];
  char pclDataList[2048];
  FILE *fp;
  char pclLine[2048];
  char pclTmpBuf[2048];
  char *pclTmpPtr;
  char *pclTmpPtr2;
  char pclUrno[16];
  char pclFlno[16];
  char pclStoa[16];
  char pclStod[16];
  char pclAdid[16];
  char pclL1fr[3][16];
  char pclL1to[3][16];
  int ilFlightDataOK;
  char pclAftUrno[16];
  char pclAftFlno[16];
  char pclAftStoa[16];
  char pclAftStod[16];
  char pclAftAdid[16];
  int ilI;
  int ilAseq;
  char pclAseq[8];
  int ilLinesTot;
  int ilLinesRej;
  int ilRecsIns;
  char pclUtcToLocal[16];
  int ilUtcToLocal;

  if ((fp = (FILE *)fopen(pcpFile,"r")) == (FILE *)NULL)
  {
     dbg(TRACE,"%s Cannot open File <%s>",pclFunc,pcpFile);
     return RC_FAIL;
  }

  ilRC = iGetConfigEntry(pcgConfigFile,"EXCHDL","PLB_LOCAL_TO_UTC",CFG_STRING,pclUtcToLocal);
  if (ilRC == RC_SUCCESS)
     ilUtcToLocal = atoi(pclUtcToLocal);
  else
     ilUtcToLocal = 0;

  ilLinesTot = 0;
  ilLinesRej = 0;
  ilRecsIns = 0;
  while (fgets(pclLine,2048,fp))
  {
     pclLine[strlen(pclLine)-1] = '\0';
     dbg(DEBUG,"%s Got Line: %s",pclFunc,pclLine);
     ilLinesTot++;
     strncpy(pclUrno,&pclLine[0],10);
     pclUrno[10] = '\0';
     TrimRight(pclUrno);
     strncpy(pclFlno,&pclLine[10],9);
     pclFlno[9] = '\0';
     TrimRight(pclFlno);
     strncpy(pclStoa,&pclLine[19],14);
     pclStoa[14] = '\0';
     TrimRight(pclStoa);
     ilRC = MyAddSecondsToCEDATime(pclStoa,ilUtcToLocal*60*(-1),1);
     strncpy(pclStod,&pclLine[33],14);
     pclStod[14] = '\0';
     TrimRight(pclStod);
     ilRC = MyAddSecondsToCEDATime(pclStod,ilUtcToLocal*60*(-1),1);
     strncpy(pclAdid,&pclLine[47],1);
     pclAdid[1] = '\0';
     TrimRight(pclAdid);
     if (strlen(pclLine) >= 48)
     {
        strncpy(&pclL1fr[0][0],&pclLine[48],14);
        pclL1fr[0][14] = '\0';
     }
     else
        pclL1fr[0][0] = '\0';
     TrimRight(&pclL1fr[0][0]);
     if (strlen(pclLine) >= 62)
     {
        strncpy(&pclL1to[0][0],&pclLine[62],14);
        pclL1to[0][14] = '\0';
     }
     else
        pclL1to[0][0] = '\0';
     TrimRight(&pclL1to[0][0]);
     if (strlen(pclLine) >= 76)
     {
        strncpy(&pclL1fr[1][0],&pclLine[76],14);
        pclL1fr[1][14] = '\0';
     }
     else
        pclL1fr[1][0] = '\0';
     TrimRight(&pclL1fr[1][0]);
     if (strlen(pclLine) >= 90)
     {
        strncpy(&pclL1to[1][0],&pclLine[90],14);
        pclL1to[1][14] = '\0';
     }
     else
        pclL1to[1][0] = '\0';
     TrimRight(&pclL1to[1][0]);
     if (strlen(pclLine) >= 104)
     {
        strncpy(&pclL1fr[2][0],&pclLine[104],14);
        pclL1fr[2][14] = '\0';
     }
     else
        pclL1fr[2][0] = '\0';
     TrimRight(&pclL1fr[2][0]);
     if (strlen(pclLine) >= 118)
     {
        strncpy(&pclL1to[2][0],&pclLine[118],14);
        pclL1to[2][14] = '\0';
     }
     else
        pclL1to[2][0] = '\0';
     TrimRight(&pclL1to[2][0]);
     for (ilI = 0; ilI < 3; ilI++)
     {
        if (pclL1to[ilI][0] != ' ')
        {
           ilRC = MyAddSecondsToCEDATime(&pclL1fr[ilI][0],ilUtcToLocal*60*(-1),1);
           ilRC = MyAddSecondsToCEDATime(&pclL1to[ilI][0],ilUtcToLocal*60*(-1),1);
           ilAseq = atoi(&pclL1fr[ilI][12]);
           strcpy(&pclL1fr[ilI][12],"00");
           if (ilAseq >= 30)
              ilRC = MyAddSecondsToCEDATime(&pclL1fr[ilI][0],60,1);
           ilAseq = atoi(&pclL1to[ilI][12]);
           strcpy(&pclL1to[ilI][12],"00");
           if (ilAseq >= 30)
              ilRC = MyAddSecondsToCEDATime(&pclL1to[ilI][0],60,1);
        }
     }
     ilFlightDataOK = FALSE;
     if (*pclUrno != ' ')
     {
        sprintf(pclSelection,"WHERE URNO = %s",pclUrno);
        sprintf(pclSqlBuf,"SELECT FLNO,STOA,STOD,ADID FROM AFTTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           BuildItemBuffer(pclDataBuf,"",4,",");
           get_real_item(pclAftFlno,pclDataBuf,1);
           TrimRight(pclAftFlno);
           get_real_item(pclAftStoa,pclDataBuf,2);
           TrimRight(pclAftStoa);
           get_real_item(pclAftStod,pclDataBuf,3);
           TrimRight(pclAftStod);
           get_real_item(pclAftAdid,pclDataBuf,4);
           TrimRight(pclAftAdid);
           if (*pclAftAdid == 'A')
           {
              if (strcmp(pclFlno,pclAftFlno) == 0 && strcmp(pclAdid,pclAftAdid) == 0 &&
                  strcmp(pclStoa,pclAftStoa) == 0)
                 ilFlightDataOK = TRUE;
           }
           else
           {
              if (strcmp(pclFlno,pclAftFlno) == 0 && strcmp(pclAdid,pclAftAdid) == 0 &&
                  strcmp(pclStod,pclAftStod) == 0)
                 ilFlightDataOK = TRUE;
           }
        }
        else
           dbg(TRACE,"%s Flight not found",pclFunc);
     }
     else
     {
        if (*pclAdid == 'A')
           sprintf(pclSelection,"WHERE FLNO = '%s' AND STOA = '%s' AND ADID = 'A'",pclFlno,pclStoa);
        else
           sprintf(pclSelection,"WHERE FLNO = '%s' AND STOD = '%s' AND ADID = 'D'",pclFlno,pclStod);
        sprintf(pclSqlBuf,"SELECT URNO FROM AFTTAB %s",pclSelection);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclUrno);
        close_my_cursor(&slCursor);
        if (ilRCdb == DB_SUCCESS)
        {
           TrimRight(pclUrno);
           ilFlightDataOK = TRUE;
        }
        else
           dbg(TRACE,"%s Flight not found",pclFunc);
     }
     if (ilFlightDataOK == FALSE)
     {
        ilLinesRej++;
        dbg(TRACE,"%s Flight Data Conflict",pclFunc);
     }
     else
     {
        sprintf(pclSqlBuf,"DELETE FROM PLBTAB WHERE FLNU = %s",pclUrno);
        slCursor = 0;
        slFkt = START;
        dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
        ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
        if (ilRCdb == DB_SUCCESS)
           commit_work();
        close_my_cursor(&slCursor);
        strcpy(pclFieldList,"URNO,CDAT,USEC,HOPO,FLNU,GAAB,GAAE,ASEQ");
        for (ilI = 0; ilI < 3; ilI++)
        {
           if (pclL1fr[ilI][0] != ' ')
           {
              ilRC = GetNextUrno();
              sprintf(pclAseq,"%d",ilI+1);
              sprintf(pclDataList,"%d,'%s','EXCHDF','%s',%s,'%s','%s','%s'",
                      igLastUrno,pcgCurrentTime,pcgHomeAp,pclUrno,&pclL1fr[ilI][0],&pclL1to[ilI][0],pclAseq);
              sprintf(pclSqlBuf,"INSERT INTO PLBTAB FIELDS(%s) VALUES(%s)",pclFieldList,pclDataList);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              if (ilRCdb == DB_SUCCESS)
              {
                 ilRecsIns++;
                 commit_work();
              }
              close_my_cursor(&slCursor);
           }
        }
     }
     dbg(TRACE,"---------------------------------------------------------------");
  }

  fclose(fp);
  dbg(DEBUG,"%s Total Lines      = %d",pclFunc,ilLinesTot);
  dbg(DEBUG,"%s Rejected Lines   = %d",pclFunc,ilLinesRej);
  dbg(DEBUG,"%s Inserted Records = %d",pclFunc,ilRecsIns);
  pclTmpPtr2 = pcpFile;
  pclTmpPtr = strstr(pcpFile,"/");
  while (pclTmpPtr != NULL)
  {
     pclTmpPtr2 = pclTmpPtr + 1;
     pclTmpPtr = strstr(pclTmpPtr2,"/");
  }
  strncpy(pclTmpBuf,pcpFile,pclTmpPtr2-pcpFile);
  pclTmpBuf[pclTmpPtr2-pcpFile] = '\0';
  strcat(pclTmpBuf,"SAVE_");
  strcat(pclTmpBuf,pclTmpPtr2);
  pclTmpPtr = strstr(pclTmpBuf,".");
  if (pclTmpPtr != NULL)
     *pclTmpPtr = '\0';
  strcat(pclTmpBuf,"_");
  strcat(pclTmpBuf,pcgCurrentTime);
  pclTmpPtr = strstr(pcpFile,".");
  if (pclTmpPtr != NULL)
     strcat(pclTmpBuf,pclTmpPtr);
  ilRC = rename(pcpFile,pclTmpBuf);

  return ilRC;
} /* End of HandlePlbData */


static int HandleAftSync(char *pcpFields, char *pcpData)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HandleAftSync:";
  int ilCurLine;
  char *pclTmpPtr;
  char pclLine[16000];
  int ilUrnoItemNo;
  char pclUrno[16];
  char pclSelection[128];
  char pclModId[16];
  char pclCommand[16];
  char pclPriority[16];
  char pclTable[16];
  int ilModId;
  int ilPriority;

  dbg(DEBUG,"%s Fields = <%s>",pclFunc,pcpFields);
  ilUrnoItemNo = get_item_no(pcpFields,"URNO",5) + 1;
  if (ilUrnoItemNo <= 0)
  {
     dbg(TRACE,"%s No URNO in Field List ==> Ignore Input",pclFunc);
     return RC_FAIL;
  }
  ilCurLine = 1;
  pclTmpPtr = GetLine(pcpData,ilCurLine);
  while (pclTmpPtr != NULL)
  {
     CopyLine(pclLine,pclTmpPtr);
     dbg(DEBUG,"%s Line %d <%s>",pclFunc,ilCurLine,pclLine);
     get_real_item(pclUrno,pclLine,ilUrnoItemNo);
     sprintf(pclSelection,"WHERE URNO=%s",pclUrno);
     ilRC = iGetConfigEntry(pcgConfigFile,"AFTSYNC","table",CFG_STRING,pclTable);
     if (ilRC != RC_SUCCESS)
        strcpy(pclTable,"AFTTAB");
     ilRC = iGetConfigEntry(pcgConfigFile,"AFTSYNC","mod_id",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
        strcpy(pclModId,"7922");
     ilRC = iGetConfigEntry(pcgConfigFile,"AFTSYNC","snd_cmd",CFG_STRING,pclCommand);
     if (ilRC != RC_SUCCESS)
        strcpy(pclCommand,"IFR");
     ilRC = iGetConfigEntry(pcgConfigFile,"AFTSYNC","priority",CFG_STRING,pclPriority);
     if (ilRC != RC_SUCCESS)
        strcpy(pclPriority,"3");
     ilPriority = atoi(pclPriority);
     ilModId = atoi(pclModId);
     dbg(TRACE,"%s Send <%s> for <%s> to <%d> , Prio = %d",pclFunc,pclCommand,pclTable,ilModId,ilPriority);
     dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
     dbg(TRACE,"%s Fields:    <%s>",pclFunc,pcpFields);
     dbg(TRACE,"%s Data:      <%s>",pclFunc,pclLine);
     ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pcgTwStart,pcgTwEnd,
                          pclCommand,pclTable,pclSelection,pcpFields,pclLine,"",
                          ilPriority,NETOUT_NO_ACK);
     ilCurLine++;
     pclTmpPtr = GetLine(pcpData,ilCurLine);
  }

  return ilRC;
} /* End of HandleAftSync */


static int HardCodedVDGSDXB(char *pcpCommand, char *pcpFields, char *pcpNewData, char *pcpOldData, char *pcpSec)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "HardCodedVDGSDXB:";
  int ilRCdb = DB_SUCCESS;
  short slFkt;
  short slCursor;
  char pclSqlBuf[1024];
  char pclDataBuf[1024];
  int ilItemNo;
  int ilItemNoP;
  int ilItemNoR;
  char pclAdid[16];
  char pclNewTmoa[16];
  char pclOldTmoa[16];
  char pclNewTifd[16];
  char pclOldTifd[16];
  char pclNewTifa[16];
  char pclOldTifa[16];
  char pclTmpTime[16];
  char pclNewStoa[16];
  int ilIdx;
  char pclNewOnbl[16];
  char pclOldOnbl[16];
  char pclNewOfbl[16];
  char pclOldOfbl[16];
  char pclNewPsta[16];
  char pclOldPsta[16];
  char pclNewRegn[16];
  char pclOldRegn[16];
  char pclUrno[16];

  dbg(DEBUG,"%s Cmd = <%s>",pclFunc,pcpCommand);
  dbg(DEBUG,"%s Fld = <%s>",pclFunc,pcpFields);
  dbg(DEBUG,"%s New = <%s>",pclFunc,pcpNewData);
  dbg(DEBUG,"%s Old = <%s>",pclFunc,pcpOldData);
  dbg(DEBUG,"%s Sec = <%s>",pclFunc,pcpSec);
  ilItemNo = get_item_no(pcpFields,"ADID",5) + 1;
  if (ilItemNo <= 0)
  {
     dbg(TRACE,"%s No ADID received ==> No Check done",pclFunc);
     return RC_SUCCESS;
  }
  get_real_item(pclAdid,pcpNewData,ilItemNo);
  if (*pclAdid == 'A')
  {
     if (strcmp(pcpCommand,"PDE") == 0)
     {
        dbg(TRACE,"%s PDE Command for Arrival Flight received ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     ilItemNo = get_item_no(pcpFields,"TMOA",5) + 1;
     if (ilItemNo <= 0)
     {
        ilItemNoP = get_item_no(pcpFields,"PSTA",5) + 1;
        ilItemNoR = get_item_no(pcpFields,"REGN",5) + 1;
        if (ilItemNoP <= 0 && ilItemNoR <= 0)
        {
           dbg(TRACE,"%s No TMOA received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        else
        {
           get_real_item(pclNewPsta,pcpNewData,ilItemNoP);
           get_real_item(pclOldPsta,pcpOldData,ilItemNoP);
           TrimRight(pclNewPsta);
           TrimRight(pclOldPsta);
           get_real_item(pclNewRegn,pcpNewData,ilItemNoR);
           get_real_item(pclOldRegn,pcpOldData,ilItemNoR);
           TrimRight(pclNewRegn);
           TrimRight(pclOldRegn);
           /*if (*pclNewPsta != ' ' && *pclOldPsta == ' ')*/
           if (strcmp(pclNewPsta,pclOldPsta) != 0 || strcmp(pclNewRegn,pclOldRegn) != 0)
           {
              ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
              if (ilItemNo <= 0)
              {
                 dbg(TRACE,"%s No TMOA received ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
              get_real_item(pclUrno,pcpNewData,ilItemNo);
              sprintf(pclSqlBuf,"SELECT TMOA FROM AFTTAB WHERE URNO = %s",pclUrno);
              slCursor = 0;
              slFkt = START;
              dbg(DEBUG,"%s SQL = <%s>",pclFunc,pclSqlBuf);
              ilRCdb = sql_if(slFkt,&slCursor,pclSqlBuf,pclDataBuf);
              close_my_cursor(&slCursor);
              if (ilRCdb == DB_SUCCESS)
              {
                 TrimRight(pclDataBuf);
                 if (*pclDataBuf != ' ')
                 {
                    dbg(TRACE,"%s PSTA or REGN was changed after TMOA ==> Send Message",pclFunc);
                    return RC_SUCCESS;
                 }
                 else
                 {
                    dbg(TRACE,"%s Not yet TMOA received ==> Don't send Message",pclFunc);
                    return RC_FAIL;
                 }
              }
              else
              {
                 dbg(TRACE,"%s Not yet TMOA received ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No TMOA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
     }
     get_real_item(pclNewTmoa,pcpNewData,ilItemNo);
     get_real_item(pclOldTmoa,pcpOldData,ilItemNo);
     if (strcmp(pclNewTmoa,pclOldTmoa) == 0)
     {
        dbg(TRACE,"%s TMOA has not changed ==> Don't send Message",pclFunc);
        return RC_FAIL;
     }
     strcpy(&pcgTagAttributeName[0][0],"acType");
     strcpy(&pcgTagAttributeValue[0][0],"towIn=\0420\042");
  }
  else if (*pclAdid == 'D')
  {
     if (strcmp(pcpCommand,"PDE") != 0)
     {
        ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
        if (ilItemNo <= 0)
        {
           dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        get_real_item(pclNewTifd,pcpNewData,ilItemNo);
        get_real_item(pclOldTifd,pcpOldData,ilItemNo);
        if (strcmp(pclNewTifd,pclOldTifd) == 0)
        {
           dbg(TRACE,"%s TIFD has not changed ==> Don't send Message",pclFunc);
           return RC_FAIL;
        }
        ilItemNo = get_item_no(pcpFields,"OFBL",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewOfbl,pcpNewData,ilItemNo);
           get_real_item(pclOldOfbl,pcpOldData,ilItemNo);
           if (strcmp(pclNewOfbl,pclOldOfbl) != 0)
           {
              dbg(TRACE,"%s OFBL has changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        strcpy(pclTmpTime,pcgCurrentTime);
        MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
        if (strcmp(pclNewTifd,pcgCurrentTime) < 0 || strcmp(pclNewTifd,pclTmpTime) > 0)
        {
           dbg(TRACE,"%s TIFD (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifd,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
           return RC_FAIL;
        }
     }
     else
     {
        ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewTifd,pcpNewData,ilItemNo);
           if (strcmp(pclNewTifd,pcgCurrentTime) < 0)
           {
              dbg(TRACE,"%s TIFD (%s) has passed current time (%s) ==> Don't send Message",
                  pclFunc,pclNewTifd,pcgCurrentTime);
              return RC_FAIL;
           }
        }
     }
     strcpy(&pcgTagAttributeName[0][0],"acType");
     strcpy(&pcgTagAttributeValue[0][0],"towIn=\0420\042");
  }
  else
  {
     if (strstr(pcpFields,"TIFA") != NULL)
     {
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           get_real_item(pclNewTifa,pcpNewData,ilItemNo);
           get_real_item(pclOldTifa,pcpOldData,ilItemNo);
           if (strcmp(pclNewTifa,pclOldTifa) == 0)
           {
              dbg(TRACE,"%s TIFA has not changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           ilItemNo = get_item_no(pcpFields,"ONBL",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewOnbl,pcpNewData,ilItemNo);
              get_real_item(pclOldOnbl,pcpOldData,ilItemNo);
              if (strcmp(pclNewOnbl,pclOldOnbl) != 0)
              {
                 dbg(TRACE,"%s ONBL has changed ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           strcpy(pclTmpTime,pcgCurrentTime);
           MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
           if (strcmp(pclNewTifa,pcgCurrentTime) < 0 || strcmp(pclNewTifa,pclTmpTime) > 0)
           {
              dbg(TRACE,"%s TIFA (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifa,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
              return RC_FAIL;
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewTifa,pcpNewData,ilItemNo);
              if (strcmp(pclNewTifa,pcgCurrentTime) < 0)
              {
                 dbg(TRACE,"%s TIFA (%s) has passed current time (%s) ==> Don't send Message",
                     pclFunc,pclNewTifa,pcgCurrentTime);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No TIFA received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
        ilItemNo = get_item_no(pcpFields,"STOA",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclNewStoa,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("DAT","TMOA",pcpSec);
           if (ilIdx >= 0)
           {
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclNewStoa);
              rgXml.rlLine[ilIdx].ilRcvFlag = 1;
           }
        }
/* Arrival */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"A");
        ilItemNo = get_item_no(pcpFields,"STOA",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFA",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
        }
     }
     else
     {
        if (strcmp(pcpCommand,"PDE") != 0)
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo <= 0)
           {
              dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           get_real_item(pclNewTifd,pcpNewData,ilItemNo);
           get_real_item(pclOldTifd,pcpOldData,ilItemNo);
           if (strcmp(pclNewTifd,pclOldTifd) == 0)
           {
              dbg(TRACE,"%s TIFD has not changed ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
           ilItemNo = get_item_no(pcpFields,"OFBL",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewOfbl,pcpNewData,ilItemNo);
              get_real_item(pclOldOfbl,pcpOldData,ilItemNo);
              if (strcmp(pclNewOfbl,pclOldOfbl) != 0)
              {
                 dbg(TRACE,"%s OFBL has changed ==> Don't send Message",pclFunc);
                 return RC_FAIL;
              }
           }
           strcpy(pclTmpTime,pcgCurrentTime);
           MyAddSecondsToCEDATime(pclTmpTime,igVdgsTimeDiff*60,1);
           if (strcmp(pclNewTifd,pcgCurrentTime) < 0 || strcmp(pclNewTifd,pclTmpTime) > 0)
           {
              dbg(TRACE,"%s TIFD (%s) is not in range of Current Time (%s) and Current Time + %d mins (%s) ==> Don't send Message",pclFunc,pclNewTifd,pcgCurrentTime,igVdgsTimeDiff,pclTmpTime);
              return RC_FAIL;
           }
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclNewTifd,pcpNewData,ilItemNo);
              if (strcmp(pclNewTifd,pcgCurrentTime) < 0)
              {
                 dbg(TRACE,"%s TIFD (%s) has passed current time (%s) ==> Don't send Message",
                     pclFunc,pclNewTifd,pcgCurrentTime);
                 return RC_FAIL;
              }
           }
           else
           {
              dbg(TRACE,"%s No TIFD received ==> Don't send Message",pclFunc);
              return RC_FAIL;
           }
        }
/* Departure */
        ilItemNo = get_item_no(pcpFields,"URNO",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclUrno,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","URNO",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclUrno);
        }
        ilIdx = GetXmlLineIdx("KEY","ADID",NULL);
        if (ilIdx >= 0)
           strcpy(&rgXml.rlLine[ilIdx].pclData[0],"D");
        ilItemNo = get_item_no(pcpFields,"STOD",5) + 1;
        if (ilItemNo > 0)
        {
           get_real_item(pclTmpTime,pcpNewData,ilItemNo);
           ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
           if (ilIdx >= 0)
              strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
        }
        else
        {
           ilItemNo = get_item_no(pcpFields,"TIFD",5) + 1;
           if (ilItemNo > 0)
           {
              get_real_item(pclTmpTime,pcpNewData,ilItemNo);
              ilIdx = GetXmlLineIdx("KEY","STOA",NULL);
              if (ilIdx >= 0)
                 strcpy(&rgXml.rlLine[ilIdx].pclData[0],pclTmpTime);
           }
        }
     }
     strcpy(&pcgTagAttributeName[0][0],"acType");
     strcpy(&pcgTagAttributeValue[0][0],"towIn=\0421\042");
  }
  return ilRC;
} /* End of HardCodedVDGSDXB */

static int HardCodedEFPSDXB()
{
    int ilRc;
    int ilI;
    char pclSelKey[1024];
    char pclFieldList[1024*2] = "\0";
    char pclDataList[1024*8] = "\0";
    char pclAftFields[512] = "\0";
    char pclAftData[512] = "\0";
    char pclTmpStr[512];
    char pclSelection[1024];
    char pclUaft[12];
    char pclSqlBuf[1024];
    char pclSqlData[1024];
    char pclCurrentTime[20] = "\0";
    char *pclFunc = "HardCodedEFPSDXB";


    for (ilI = 0; ilI < igNoData; ilI++)
    {
        dbg( DEBUG, "%s: FIELD <%s> DATA <%s> TYPE <%s> METHOD <%s>", pclFunc,
                    rgData[ilI].pclField, rgData[ilI].pclData, rgData[ilI].pclType, rgData[ilI].pclMethod );
        if( !strcmp(rgData[ilI].pclField, "URNO") )
            strcpy( pclUaft, rgData[ilI].pclData );
        if( !strcmp(rgData[ilI].pclField, "AIRB") )
        {
            strcat( pclAftFields, "AIRA," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "LAND") )
        {
            strcat( pclAftFields, "LNDA," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "ETOA") )
        {
            strcat( pclAftFields, "ETAA," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "ETOD") )
        {
            strcat( pclAftFields, "ETDA," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "PUSI") && igCOBHandling )
        {
            strcat( pclAftFields, "ETDC," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }

        sprintf( pclTmpStr, "%s,", rgData[ilI].pclField );
        strcat( pclFieldList, pclTmpStr );
        sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
        strcat( pclDataList, pclTmpStr );
    }
    if( strlen(pclAftFields) > 0 )
    {
        pclAftFields[strlen(pclAftFields)-1] = '\0';
        if( strlen(pclAftData) > 0 )
            pclAftData[strlen(pclAftData)-1] = '\0';
    }

    if( strlen(pclFieldList) > 0 )
    {
         pclFieldList[strlen(pclFieldList)-1] = '\0';
         if( strlen(pclDataList) > 0 )
             pclDataList[strlen(pclDataList)-1] = '\0';
    }
    if( strlen(pclUaft) <= 0 )
        return RC_SUCCESS;
    dbg( DEBUG, "%s: UAFT <%s>", pclFunc, pclUaft );
    sprintf( pclSelection, "WHERE URNO = %s", pclUaft );
    /* Find out any records in AFTTAB first */
    sprintf( pclSqlBuf, "SELECT ADID FROM AFTTAB %s", pclSelection );
    ilRc = RunSQL( pclSqlBuf, pclSqlData );
    if( ilRc != DB_SUCCESS )
    {
        dbg( DEBUG, "%s: UAFT <%s> not found in AFTTAB", pclFunc, pclUaft );
        return RC_SUCCESS;
    }
    if( strlen(pclAftData) > 0 && strlen(pclAftFields) > 0 )
    {
        dbg( DEBUG, "%s: <UFR><AFTTAB>", pclFunc );
        dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclAftFields );
        dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclAftData );

        SendCedaEvent(igEFPSDestination,0,mod_name,mod_name," "," ","UFR","AFTTAB",pclSelection,
                      pclAftFields,pclAftData,"",3,NETOUT_NO_ACK);
    }
    if( strlen(pclDataList) > 0 && strlen(pclFieldList) > 0 )
    {
        sprintf( pclSelection, "WHERE UAFT = %s", pclUaft );
        sprintf( pclSqlBuf, "SELECT EFPI FROM EFPTAB %s", pclSelection );
        ilRc = RunSQL( pclSqlBuf, pclSqlData );
        GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
        if( ilRc != DB_SUCCESS )
        {
            strcat( pclFieldList, ",EFPI,UAFT,USEC,CDAT" );
            sprintf( pclTmpStr, ",%14.14s,%s,%s,%14.14s", pclCurrentTime, pclUaft, mod_name, pclCurrentTime );
            strcat( pclDataList, pclTmpStr );

            dbg( DEBUG, "%s: <IRT><EFPTAB>", pclFunc );
            dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclFieldList );
            dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclDataList );

            SendCedaEvent(506,0,mod_name,mod_name," "," ","IRT","EFPTAB"," ",
                          pclFieldList,pclDataList,"",3,NETOUT_NO_ACK);
        }
        else
        {

            strcat( pclFieldList, ",EFPU,USEU,LSTU" );
            sprintf( pclTmpStr, ",%14.14s,%s,%14.14s", pclCurrentTime, mod_name, pclCurrentTime );
            strcat( pclDataList, pclTmpStr );
            dbg( DEBUG, "%s: <URT><EFPTAB>", pclFunc );
            dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclFieldList );
            dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclDataList );
            sprintf( pclSelection, "WHERE URNO = %s", pclUaft );
            SendCedaEvent(506,0,mod_name,mod_name," "," ","URT","EFPTAB",pclSelection,
                          pclFieldList,pclDataList,"",3,NETOUT_NO_ACK);
        }
    }
    return RC_SUCCESS;
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


static int DeleteZeroFromCsgn( char *pcpCsgn )
{
    int ilRC = RC_SUCCESS;
    int ilLen;
    int ili, ilj;
    char pclNewCsgn[12] = "\0";
    char pclTmpStr[128] = "\0";
    char *pclFunc = "DeleteZeroFromCsgn";

    strcpy( pclNewCsgn, pcpCsgn );
    ilLen = strlen(pclNewCsgn);
    if( ilLen <= 3 )
        return RC_SUCCESS;
    ilj = 3;
    memcpy( pclTmpStr, pclNewCsgn, 3 );

    for( ili = 3; ili < ilLen; ili++ )
    {
        /* suffix or adhoc callsign */
        if( !isdigit(pclNewCsgn[ili]) ||
            (isdigit(pclNewCsgn[ili]) && pclNewCsgn[ili] > '0') )
        {
            memcpy( &pclTmpStr[ilj], &pclNewCsgn[ili], ilLen-ili);
            ilj+=(ilLen-ili+1);
            break;
        }

        /* digit part */
        if( isdigit(pclNewCsgn[ili]) && (pclNewCsgn[ili] == '0') )
            continue;
    }
    pclTmpStr[ilj] = '\0';
    dbg( TRACE, "%s: Manipulated CSGN = <%s> original CSGN = <%s>", pclFunc, pclTmpStr, pclNewCsgn );
    strcpy( pcpCsgn, pclTmpStr );

} /* DeleteZeroFromCsgn */

//Frank v1.52 20121226
static void AddUrnoIntoXml(char *pcpData,char *pclTmpLine)
{
	int ilI=0;
	char *pclFunc = "AddUrnoIntoXml";
	char pclTmpUrno[32] = "\0";
	//char pclTmpLine[4096] = "\0";
	char pclTmpTmp[1024] = "\0";
	char pclSubString[1024] = "\0";

  strcpy(pclTmpLine,pcpData);

	if( !strcmp(pcgDXBHoneywell,"YES") )
	{
		if(strstr(pcpData,pcgGetUrnoHead) != 0)
		{
			strcpy(pclSubString,strstr(pcpData,pcgGetUrnoHead));
			dbg(DEBUG,"%s SubString\n<%s>",pclFunc,pclSubString);

			memset(pclTmpUrno,0,sizeof(pclTmpUrno));

			for(ilI=0;ilI<strlen(pclSubString);ilI++)
			{
				if((pclSubString+strlen(pcgGetUrnoHead))[ilI] == '"')
				{
					break;
				}
				else
				{
					pclTmpUrno[ilI] = (pclSubString+strlen(pcgGetUrnoHead))[ilI];
					//dbg(DEBUG,"%c",pclTmpUrno[ilI]);
				}
			}
			dbg(DEBUG,"%s pclTmpUrno<%s>",pclFunc,pclTmpUrno);
			dbg(DEBUG,"%s atoi(pclTmpUrno)<%d>",pclFunc,atoi(pclTmpUrno));

			if( atoi(pclTmpUrno)!=0 )
			{
			    //dbg(DEBUG,"+++++++++strlen(pcpData)<%d>strlen( </flightRecord></message>)<%d>",strlen(pcpData),strlen(" </flightRecord></message>"));

			    memset(pclTmpLine,0,sizeof(pclTmpLine));
			    strncpy(pclTmpLine,pcpData,strlen(pcpData)-strlen(pcgGetUrnoTail)-2);
			    dbg(DEBUG,"%s pclTmpLine\n[%s]",pclFunc,pclTmpLine);
			}

	    sprintf(pclTmpTmp,"<URNO>%s</URNO>",pclTmpUrno);
	    dbg(DEBUG,"%s pclTmpTmp[%s]",pclFunc,pclTmpTmp);
	    strcat(pclTmpLine,pclTmpTmp);
	    strcat(pclTmpLine,pcgGetUrnoTail);
	    dbg(DEBUG,"%s pclTmpLine\n[%s]",pclFunc,pclTmpLine);
		}

		strcpy(pcpData,pclTmpLine);
	}
}

static int HardCodedHoneyWellDXB(char *pcpTagName)
{
    int ilRc;
    int ilI;
    char pclSelKey[1024];
    char pclFieldList[1024*2] = "\0";
    char pclDataList[1024*8] = "\0";
    char pclAftFields[512] = "\0";
    char pclAftData[512] = "\0";
    char pclTmpStr[512];
    char pclSelection[1024];
    char pclUaft[12];
    char pclSqlBuf[1024];
    char pclSqlData[1024];
    char pclCurrentTime[20] = "\0";
    char *pclFunc = "HardCodedHoneyWellDXB";

    //Frank 20121218 v1.58
    int ilNO = 0;
    int ilNO_AfttabField = 0;
    int ilProcessId = 0;
    ilProcessId = tool_get_q_id ("hwepde");

    for (ilI = 0; ilI < igNoData; ilI++)
    {
        dbg( DEBUG, "%s: FIELD <%s> DATA <%s> TYPE <%s> METHOD <%s>", pclFunc,
                    rgData[ilI].pclField, rgData[ilI].pclData, rgData[ilI].pclType, rgData[ilI].pclMethod );

        if( !strcmp(rgData[ilI].pclField, "URNO") )
            strcpy( pclUaft, rgData[ilI].pclData );

        if( !strcmp(rgData[ilI].pclField, "AETT") )
        {
            strcat( pclAftFields, "AETT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "ASTT") )
        {
            strcat( pclAftFields, "ASTT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "EETT") )
        {
            strcat( pclAftFields, "EETT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "EXIT") )
        {
            strcat( pclAftFields, "EXIT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "EXOT") )
        {
            strcat( pclAftFields, "EXOT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }
        else if( !strcmp(rgData[ilI].pclField, "CTOT") )
        {
            strcat( pclAftFields, "CTOT," );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclAftData, pclTmpStr );
        }

				if( !strcmp(rgData[ilI].pclField, "LDIR") || !strcmp(rgData[ilI].pclField, "OFTX") || !strcmp(rgData[ilI].pclField, "VISC"))
        {
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclField );
            strcat( pclFieldList, pclTmpStr );
            sprintf( pclTmpStr, "%s,", rgData[ilI].pclData );
            strcat( pclDataList, pclTmpStr );
        }
    }
    if( strlen(pclAftFields) > 0 )
    {
        pclAftFields[strlen(pclAftFields)-1] = '\0';
        if( strlen(pclAftData) > 0 )
            pclAftData[strlen(pclAftData)-1] = '\0';
    }

    if( strlen(pclFieldList) > 0 )
    {
         pclFieldList[strlen(pclFieldList)-1] = '\0';
         if( strlen(pclDataList) > 0 )
             pclDataList[strlen(pclDataList)-1] = '\0';
    }
    if( strlen(pclUaft) <= 0 )
        return RC_SUCCESS;
    dbg( DEBUG, "%s: UAFT <%s>", pclFunc, pclUaft );

    ilNO = GetNoOfElements(pclFieldList,',');
    ilNO_AfttabField = GetNoOfElements(pclAftFields,',');
    dbg(DEBUG,"%s: pclFieldList<%s>The field number of pclFieldList<%d>",pclFunc,pclFieldList,ilNO);
    dbg(DEBUG,"%s: pclAftFields<%s>The field number of pclAftFields<%d>",pclFunc,pclAftFields,ilNO_AfttabField);

    //if(strcmp(pclUaft,"ALL") != 0)
    if(ilNO_AfttabField > 0)
  	{
      sprintf( pclSelection, "WHERE URNO = %s", pclUaft );
      /* Find out any records in AFTTAB first */
      sprintf( pclSqlBuf, "SELECT ADID FROM AFTTAB %s", pclSelection );

      ilRc = RunSQL( pclSqlBuf, pclSqlData );
      if( ilRc != DB_SUCCESS )
      {
          dbg( DEBUG, "%s: UAFT <%s> not found in AFTTAB", pclFunc, pclUaft );
          return RC_SUCCESS;
      }

      if( strlen(pclAftData) > 0 && strlen(pclAftFields) > 0 )
      {
          dbg( DEBUG, "%s: <UFR><AFTTAB>", pclFunc );
          dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclAftFields );
          dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclAftData );
          dbg( DEBUG, "%s: Selection <%s>", pclFunc, pclSelection );
          dbg( DEBUG, "%s: Destination <%d>", pclFunc, igEFPSDestination );

          SendCedaEvent(igEFPSDestination,0,mod_name,mod_name," "," ","UFR","AFTTAB",pclSelection,
                        pclAftFields,pclAftData,"",3,NETOUT_NO_ACK);

           dbg(DEBUG,"--------------------------");
      }

      if( strlen(pclDataList) > 0 && strlen(pclFieldList) > 0 )
      {
          sprintf( pclSelection, "WHERE UAFT = %s", pclUaft );
          sprintf( pclSqlBuf, "SELECT EFPI FROM EFPTAB %s", pclSelection );
          ilRc = RunSQL( pclSqlBuf, pclSqlData );

          GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );
          if( ilRc != DB_SUCCESS )
          {
              //strcat( pclFieldList, ",EFPI,UAFT,USEC,CDAT" );
              //sprintf( pclTmpStr, ",%14.14s,%s,%s,%14.14s", pclCurrentTime, pclUaft, mod_name, pclCurrentTime );
              //strcat( pclDataList, pclTmpStr );

              strcat(pclFieldList,",UAFT,URNO");
              strcat(pclDataList,",");
              strcat(pclDataList,pclUaft);
              strcat(pclDataList,",");
              strcat(pclDataList,pclUaft);

              dbg( DEBUG, "%s: <IRT><EFPTAB>", pclFunc );
              dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclFieldList );
              dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclDataList );
              dbg( DEBUG, "%s: Destination <%d>", pclFunc, 506 );

              SendCedaEvent(506,0,mod_name,mod_name," "," ","IRT","EFPTAB"," ",
                            pclFieldList,pclDataList,"",3,NETOUT_NO_ACK);
          		 dbg(DEBUG,"--------------------------");
          }
          else
          {
              //strcat( pclFieldList, ",EFPU,USEU,LSTU" );
              //sprintf( pclTmpStr, ",%14.14s,%s,%14.14s", pclCurrentTime, mod_name, pclCurrentTime );
              //strcat( pclDataList, pclTmpStr );

              dbg( DEBUG, "%s: <URT><EFPTAB>", pclFunc );
              dbg( DEBUG, "%s: FIELDS <%s>", pclFunc, pclFieldList );
              dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclDataList );
              sprintf( pclSelection, "WHERE URNO = %s", pclUaft );
              dbg( DEBUG, "%s: Selection <%s>", pclFunc, pclSelection );
              SendCedaEvent(506,0,mod_name,mod_name," "," ","URT","EFPTAB",pclSelection,
                            pclFieldList,pclDataList,"",3,NETOUT_NO_ACK);
          		 dbg(DEBUG,"--------------------------");
          }
      }
    }
    else
    {
      		if(strcmp(pclUaft,"ALL") == 0)
      		{
          //Frank 20121116 v1.55
            ilNO = GetNoOfElements(pclFieldList,',');
            dbg(DEBUG,"%s: pclFieldList<%s>,The field number of pclFieldList<%d>",pclFunc,pclFieldList,ilNO);

              if(strcmp(pcpTagName,"flightRecordRequest") == 0)
              {
              dbg( DEBUG, "%s: FIELDS <URNO>", pclFunc );
              dbg( DEBUG, "%s: DATA <%s>", pclFunc, pclUaft );
              dbg( DEBUG, "%s: pclSelection<%s>", pclFunc,pclSelection );
              dbg( DEBUG, "%s: <BAT>", pclFunc );

              memset(pclSelection,0,sizeof(pclSelection));

              if ((ilProcessId == RC_FAIL) || (ilProcessId == RC_NOT_FOUND))
                {
                  dbg(TRACE,"%s: ====== ERROR ====== hwepde Mod Id not found",pclFunc);
                }
              else
              {
                  if(ilProcessId!=0)
                      {
                      	SendCedaEvent(ilProcessId,0,mod_name,mod_name," "," ","BAT","AFTTAB",pclSelection,
                        "URNO",pclUaft,"",3,NETOUT_NO_ACK);

                  				dbg(TRACE,"%s SendCedaEvent1 has executed,send to<%d>",pclFunc,ilProcessId);
                  				dbg(DEBUG,"--------------------------");
                      }
                      else
                      {
                          dbg(TRACE,"%s ilProcessId<%d>==0",pclFunc,ilProcessId);
                      }
                  }
              }
          }
          else
          {
              //Frank 20121116 v1.52
              ilNO = GetNoOfElements(pclFieldList,',');
              dbg(DEBUG,"%s: pclFieldList<%s>,The field number of pclFieldList<%d>",pclFunc,pclFieldList,ilNO);

              if(strcmp(pcpTagName,"flightRecordRequest") == 0)
              {
                  sprintf( pclSelection, "WHERE UAFT = %s", pclUaft );

              dbg( DEBUG, "%s: FIELDS <URNO>", pclFunc );
              dbg( DEBUG, "%s: DATA <%s>", pclFunc,pclUaft );
              dbg( DEBUG, "%s: pclSelection<%s>", pclFunc,pclSelection );
              dbg( DEBUG, "%s: <REQ>", pclFunc );

              if ((ilProcessId == RC_FAIL) || (ilProcessId == RC_NOT_FOUND))
                {
                  dbg(TRACE,"%s: ====== ERROR ====== hwepde Mod Id not found",pclFunc);
                }
              else
              {
                  if(ilProcessId!=0)
                      {
                          SendCedaEvent(ilProcessId,0,mod_name,mod_name," "," ","REQ","AFTTAB",pclSelection,
                              "URNO",pclUaft,"",3,NETOUT_NO_ACK);
                  dbg(TRACE,"%s SendCedaEvent2 has executed,send to<%d>",pclFunc,ilProcessId);
                   dbg(DEBUG,"--------------------------");
                      }
                      else
                      {
                          dbg(TRACE,"%s ilProcessId<%d>==0",pclFunc,ilProcessId);
                      }
              }
              }//Frank 20121116 v1.52
          }
        //Frank 20121116 v1.55
    }
    return RC_SUCCESS;
}

static int BuildOutputDXB_HoneyWell(int ipIndex, int *ipCount, char *pcpCurSec, char *pcpType, char *pcpIntfNam,
                       char *pcpMode, char *pcpUrno,char *pcpFtyp,char *pcpActionType)
{
  int ilRC = RC_SUCCESS;
  char pclFunc[] = "BuildOutputDXB_HoneyWell:";
  int ilI;
  int ilJ;
  int ilK;
  int ilLevel;
  char pclName[32];   /* MEI */
  char pclAltName[32];  /* MEI */
  int ilIgnore;
  int ilStartCount;
  char clActionType;
  char pclModId[128];
  char pclCommand[128];
  char pclTable[128];
  char pclSelection[128];
  char pclFields[128];
  char pclPriority[128];
  char pclTws[128];
  char pclTmpBuf[128] = "\0";
  char pclTmpBuf1[10][128];
  char pclTmpBuf2[10][128];
  int ilModId;
  int ilPriority;
  int ilDiscard;
  char pclData[4000];
  char pclDisName[16];
  char pclKeyFieldNames[2048] = "";
  int ilNoItems;
  char pclBlanks[2048];
  int ilStart;
  int ilFound;
  int ilTimeDiffLocalUtc;
  int ilTDIS = -1;
  int ilTDIW = -1;
  char pclTime[16];
  char pclTagAttribute[1024] = "\0";
  char pclTagAttribute2[1024] = "\0";
  char pclCurAttr[1024] = "\0";
  char *pclTmpPtr = NULL;

  char pclMsgTmp[32]  = "\0";

  int ilAttr = 0;
  int ilCount = 0;
  char pclCurrentTime[128]="\0";
  char pclFormatTime[128]="\0";
  char pclTmp[128]="\0";

  memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
  memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
  memset(pclMsgTmp,0,sizeof(pclMsgTmp));

  dbg(DEBUG,"%s pcpUrno<%s>pcpFtyp<%s>pcpActionType<%s>",pclFunc,pcpUrno,pcpFtyp,pcpActionType);
  strcpy(pclTmpBuf2[0],pcpUrno);
  //strcpy(pclTmpBuf2[1],pcpFtyp);
  sprintf(pclTmpBuf2[2],"%c",pcpActionType[0]);

  switch(pcpFtyp[0])
  {
      case 'B':
      case 'D':
      case 'R':
      case 'T':
      case 'Z':
      case 'O':
      case 'G':
      	strcpy(pclTmpBuf2[1],"O");
        break;
      case 'X':
      case 'N':
      case 'S':
      	strncpy(pclTmpBuf2[1],pcpFtyp,1);
      	break;
      default:
        strcpy(pclTmpBuf2[1]," ");
        break;
  }

  memset(pcgOutBuf,0x00,500000);
  ilDiscard = FALSE;
  clActionType = 'U';
  ilStartCount = FALSE;
  *ipCount = 0;
  if (*pcpMode == 'C' || *pcpMode == 'S')
  {
     if (strlen(pcgVersion) > 0)
        sprintf(pcgOutBuf,"%s\n",pcgVersion);
     else
        strcpy(pcgOutBuf,"");
  }
  strcpy(pclKeyFieldNames,"");
  ilStart = 0;
  if (*pcpMode == 'A' || *pcpMode == 'E')
  {
     ilFound = FALSE;
     for (ilI = 0; ilI < igCurXmlLines && ilFound == FALSE; ilI++)
     {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0 &&
            strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
        {
           ilStart = ilI;
           ilFound = TRUE;
        }
     }
  }
  for (ilI = ilStart; ilI < igCurXmlLines; ilI++)
  {
     ilIgnore = FALSE;
     if (rgXml.rlLine[ilI].ilRcvFlag == 1)
     {
        if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_BGN") == 0)
        {
           ilLevel = rgXml.rlLine[ilI].ilLevel;

           for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
              strcat(pcgOutBuf," ");

           strcat(pcgOutBuf,"<");
           strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclName[0]);

           ilRC = iGetConfigEntry(pcgConfigFile,"TAG_ATTRIBUTE_message",&rgXml.rlLine[ilI].pclName[0],
                                  CFG_STRING,pclTagAttribute);

           if (ilRC == RC_SUCCESS && strstr(pclTagAttribute,"version")!=0)
           //if (ilRC == RC_SUCCESS)
           {
              pclTmpPtr = strstr(pclTagAttribute,",");
              if (pclTmpPtr != NULL)
              {
               ilAttr = GetNoOfElements(pclTagAttribute,',');

               //dbg(DEBUG,"%s pclTagAttribute<%s>",pclFunc,pclTagAttribute);

               for (ilCount = 1; ilCount <= ilAttr; ilCount++)
               {
               		memset(pclCurAttr,0,sizeof(pclCurAttr));
                  get_real_item(pclCurAttr,pclTagAttribute,ilCount);
                  //dbg(DEBUG,"%s pclCurAttr<%s>",pclFunc,pclCurAttr);

                  if(strncmp(pclCurAttr,"version",strlen("version")) == 0)
                  {
                  		strcat(pclTmpBuf1[ilCount-1]," ");
                  		strcat(pclTmpBuf1[ilCount-1],pclCurAttr);
                  }
                  else if(strncmp(pclCurAttr,"messageId",strlen("messageId"))==0)
                  {
                  	#ifdef FRANK
                  		//igMsgIdHoneyWell = 3;
                  	#endif

                  	if( igMsgIdHoneyWell == 65535 )
                  	{
                  		dbg(DEBUG,"%s igMsgIdHoneyWell has reached 65535, reset it to 1",pclFunc);
                  		igMsgIdHoneyWell = 1;
                  	}
                     	sprintf(pclTmpBuf1[ilCount-1]," %s=\"%d\"",pclCurAttr,igMsgIdHoneyWell);
                     	igMsgIdHoneyWell++;
                     	//strcat(pclTmpBuf1[ilCount-1]," ");
                  		//strcat(pclTmpBuf1[ilCount-1],pclCurAttr);
                  }
                  else if(strncmp(pclCurAttr,"messageType",strlen("messageType"))==0)
                  {
                  	strcat(pclTmpBuf1[ilCount-1]," ");
                      strcat(pclTmpBuf1[ilCount-1],pclCurAttr);
                  }
                  else if(strncmp(pclCurAttr,"recipientId",strlen("recipientId"))==0)
                  {
                  	strcat(pclTmpBuf1[ilCount-1]," ");
                      strcat(pclTmpBuf1[ilCount-1],pclCurAttr);
                  }
                  else if(strncmp(pclCurAttr,"senderId",strlen("senderId"))==0)
                  {
                  	strcat(pclTmpBuf1[ilCount-1]," ");
                      strcat(pclTmpBuf1[ilCount-1],pclCurAttr);
                  }
                  else if(strncmp(pclCurAttr,"messageStamp",strlen("messageStamp"))==0)
                  {
                      GetServerTimeStamp( "UTC", 1, 0, pclCurrentTime );

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

                      sprintf(pclTmpBuf1[ilCount-1]," %s=\"%s\"",pclCurAttr,pclFormatTime);
                  }
               }

               for(ilCount=0;ilCount<ilAttr;ilCount++)
               {
                      //dbg(DEBUG,"%s +++pclTmpBuf1[%d]<%s>",pclFunc,ilCount,pclTmpBuf1[ilCount]);
                      strcat(pcgOutBuf,pclTmpBuf1[ilCount]);
               }
              }
           }

           ilAttr = 0;
           memset(pclCurAttr,0,sizeof(pclCurAttr));
           memset(pclTmpPtr,0,sizeof(pclTmpPtr));
           memset(pclTmpBuf1,0,sizeof(pclTmpBuf1));
           //memset(pclTmpBuf2,0,sizeof(pclTmpBuf2));
           memset(pclTagAttribute,0,sizeof(pclTagAttribute));

           ilRC = iGetConfigEntry(pcgConfigFile,"TAG_ATTRIBUTE_flightRecord",&rgXml.rlLine[ilI].pclName[0],
                                  CFG_STRING,pclTagAttribute);
           if (ilRC == RC_SUCCESS)
           {
              pclTmpPtr = strstr(pclTagAttribute,",");
              if (pclTmpPtr != NULL)
              {
                 ilAttr = GetNoOfElements(pclTagAttribute,',');

                 dbg(DEBUG,"%s +pclTagAttribute<%s>",pclFunc,pclTagAttribute);

                 for (ilCount = 1; ilCount <= ilAttr; ilCount++)
                 {
                    get_real_item(pclCurAttr,pclTagAttribute,ilCount);

                    //dbg(DEBUG,"%s pclCurAttr<%s>",pclFunc,pclCurAttr);
                    sprintf(pclTmpBuf1[ilCount-1]," %s=\"%s\"",pclCurAttr,pclTmpBuf2[ilCount-1]);
                 }

                 for(ilCount=0;ilCount<ilAttr;ilCount++)
                 {
                        //dbg(DEBUG,"%s pclTmpBuf1[%d]<%s>",pclFunc,ilCount,pclTmpBuf1[ilCount]);
                        strcat(pcgOutBuf,pclTmpBuf1[ilCount]);
                 }
              }
           }

           strcat(pcgOutBuf,">");
           if (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
              ilStartCount = TRUE;
        }
        else if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"STR_END") == 0)
        {
        	 if( !strcmp(pcgIgnoreKeySection,"YES") )
        	 {
        	 	if( strstr(&rgXml.rlLine[ilI].pclName[0],"INFOBJ_GENERIC")!=0 )
        	 	{
        		dbg(DEBUG,"%s %d the STR_END name:<%s> continue",pclFunc,__LINE__,&rgXml.rlLine[ilI].pclName[0]);
        		continue;
        		}
        	}

           ilLevel = rgXml.rlLine[ilI].ilLevel;
           for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
              strcat(pcgOutBuf," ");
           strcat(pcgOutBuf,"</");
           strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclName[0]);
           strcat(pcgOutBuf,">");
           if (strcmp(&rgXml.rlLine[ilI].pclName[0],pcpCurSec) == 0)
           {
              ilStartCount = FALSE;
              if (*pcpMode == 'S' || *pcpMode == 'A')
              {
                 strcat(pcgOutBuf,"\n");
                 return ilRC;
              }
           }
        }
        else //DAT,HDR,KEY
        {
           /* MEI */
           if( strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0 )
           {
              if( strcmp(&rgXml.rlLine[ilI].pclFlag[0],"F") == 0 )
               {
                    strcpy(pclData,&rgXml.rlLine[ilI].pclData[0]);
                    if (*pclData == '\0' || *pclData == ' ')
                    {
                        strcpy(pclDisName,&rgXml.rlLine[ilI].pclName[0]);
                        ilDiscard = TRUE;
                    }
              }
              /* MEI v1.40 */
              else if( (!strcmp(&rgXml.rlLine[ilI].pclFlag[0],"MD") && *pcgAdid != 'D') ||
                       (!strcmp(&rgXml.rlLine[ilI].pclFlag[0],"MA") && *pcgAdid != 'A') )
              {
                  ilIgnore = TRUE;
              }
           }

           if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"HDR") == 0 && strcmp(&rgXml.rlLine[ilI].pclName[0],pcgActionType) == 0)
           {
              clActionType = rgXml.rlLine[ilI].pclData[0];
           }

           strcpy(pclName,&rgXml.rlLine[ilI].pclName[0]);

           if (*pcgAdid == 'D')
              strcpy(pclAltName,&rgXml.rlLine[ilI].pclFieldD[ipIndex][0]);
           else
              strcpy(pclAltName,&rgXml.rlLine[ilI].pclFieldA[ipIndex][0]);
           if (strlen(pclAltName) > 0)
           {
              if (strcmp(pclAltName,"XXXX") == 0)
                 ilIgnore = TRUE;
              strcpy(pclName,pclAltName);
           }
           if (strcmp(&rgXml.rlLine[ilI].pclType[0],"IGNORE") == 0)
              ilIgnore = TRUE;

           if( strcmp(&rgXml.rlLine[ilI].pclBasFld[0],"CSGN") == 0)
           {
               /* Currently 15-May-2012 used in DXB EFPS v1.44 */
               if( igDelZeroFromCsgn == TRUE )
               {
                   DeleteZeroFromCsgn( &rgXml.rlLine[ilI].pclData[0] );
               }
           }

           if (pcpType == NULL)
           {
              if (clActionType == *pcgActionTypeDel)
              {
                 if (strcmp(pclName,"RAID") != 0 && strcmp(pclName,"TOID") != 0 &&
                     strcmp(&rgXml.rlLine[ilI].pclType[0],"BAS") != 0 &&
                     strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
                    ilIgnore = TRUE;
              }
           }
           if (strcmp(pcgRepeatKeyFieldsInBody,"YES") != 0 &&
               strstr(pcgSectionsForKeyFieldsCheck,pcpCurSec) != NULL)
           {
              if (clActionType == *pcgActionTypeIns && strcmp(&rgXml.rlLine[ilI].pclTag[0],"DAT") == 0)
              {
                 if (strstr(pclKeyFieldNames,&rgXml.rlLine[ilI].pclName[0]) != NULL)
                    ilIgnore = TRUE;
                 else if (*pcgAdid == 'A' && strcmp(&rgXml.rlLine[ilI].pclName[0],"STOA") == 0)
                    ilIgnore = TRUE;
                 else if (*pcgAdid == 'D' && strcmp(&rgXml.rlLine[ilI].pclName[0],"STOD") == 0)
                    ilIgnore = TRUE;
              }
           }
           if (ilIgnore == FALSE)
           {
              if (strcmp(&rgXml.rlLine[ilI].pclTag[0],"KEY") == 0)
              {
                 strcat(pclKeyFieldNames,&rgXml.rlLine[ilI].pclName[0]);
                 strcat(pclKeyFieldNames,",");
              }
              ilLevel = rgXml.rlLine[ilI].ilLevel;
              for (ilJ = 0; ilJ < (ilLevel-1)*3; ilJ++)
                 strcat(pcgOutBuf," ");
              strcat(pcgOutBuf,"<");
              strcat(pcgOutBuf,pclName);
              if (strcmp(&pcgTagAttributeName[0][0],pclName) == 0)
              {
                 strcat(pcgOutBuf," ");
                 strcat(pcgOutBuf,&pcgTagAttributeValue[0][0]);
              }
              if (igCompressTag == TRUE && rgXml.rlLine[ilI].pclData[0] == ' ' &&
                  strlen(&rgXml.rlLine[ilI].pclData[0]) == 1 &&
                  strcmp(&rgXml.rlLine[ilI].pclType[0],"TIM1") != 0)
                 strcat(pcgOutBuf," />");
              else
              {
                 strcat(pcgOutBuf,">");
/*
                 if (strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                     strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL)
                    strcat(pcgOutBuf,pcgCDStart);
*/
                 if (
                 	    strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                        strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL ||
                        strcmp(&rgXml.rlLine[ilI].pclType[0],"CDATA") == 0
                    )
                    strcat(pcgOutBuf,pcgCDStart);

                 if (strcmp(&rgXml.rlLine[ilI].pclType[0],"SGDT") == 0)
                 {
                    strcpy(pclTime,&rgXml.rlLine[ilI].pclData[0]);
                    if (strlen(pclTime) > 1)
                    {
                       if (strlen(pclTime) == 12)
                          strcat(pclTime,"00");
                       strncat(pcgOutBuf,&pclTime[0],4);
                       strcat(pcgOutBuf,"-");
                       strncat(pcgOutBuf,&pclTime[4],2);
                       strcat(pcgOutBuf,"-");
                       strncat(pcgOutBuf,&pclTime[6],2);
                       strcat(pcgOutBuf,"T");
                       strncat(pcgOutBuf,&pclTime[8],2);
                       strcat(pcgOutBuf,":");
                       strncat(pcgOutBuf,&pclTime[10],2);
                       strcat(pcgOutBuf,":");
                       strncat(pcgOutBuf,&pclTime[12],2);

                       //Frank v1.52 20121226
                       if(strlen(pcgModifySGDTTimeFormat) != 0)
                       {
                       		strcat(pcgOutBuf,pcgModifySGDTTimeFormat);
                       }
                       //Frank v1.52 20121226
                    }
                    else
                       strcat(pcgOutBuf," ");
                 }
                 else
                 {
                    if (strcmp(&rgXml.rlLine[ilI].pclType[0],"TIM1") == 0)
                    {
                       if (strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                          strcpy(pclTime,&rgXml.rlLine[ilI].pclData[0]);
                       else
                          strcpy(pclTime,pcgCurrentTime);
                       ilTimeDiffLocalUtc = GetTimeDiffUtc(pclTime,&ilTDIS,&ilTDIW);
                       ilRC = MyAddSecondsToCEDATime(pclTime,ilTimeDiffLocalUtc*60,1);
                       strncat(pcgOutBuf,&pclTime[8],2);
                       strcat(pcgOutBuf,":");
                       strncat(pcgOutBuf,&pclTime[10],2);
                       strcat(pcgOutBuf,":00");
                    }
                    else
                    {
                       if (strcmp(&rgXml.rlLine[ilI].pclType[0],"DATE") == 0 &&
                           strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                          strncat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[0],8);
                       else if (strcmp(&rgXml.rlLine[ilI].pclType[0],"TIME") == 0 &&
                           strlen(&rgXml.rlLine[ilI].pclData[0]) > 1)
                       {
                          strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[8]);
                          if (strlen(&rgXml.rlLine[ilI].pclData[0]) == 12)
                             strcat(pcgOutBuf,"00");
                       }
                       else
                       {
                            //Frank v1.08 20121210
                          strcat(pcgOutBuf,&rgXml.rlLine[ilI].pclData[0]);
                       }
                       if (strcmp(&rgXml.rlLine[ilI].pclType[0],"DATI") == 0 &&
                           strlen(&rgXml.rlLine[ilI].pclData[0]) == 12)
                          strcat(pcgOutBuf,"00");
                       else if (strcmp(pclName,"JFNO") == 0)
                       {
                          GetTrailingBlanks(pclBlanks,&rgXml.rlLine[ilI].pclData[0],9);
                          strcat(pcgOutBuf,pclBlanks);
                       }
                       else if (strcmp(pclName,"VIAL") == 0)
                       {
                          GetTrailingBlanks(pclBlanks,&rgXml.rlLine[ilI].pclData[0],120);
                          strcat(pcgOutBuf,pclBlanks);
                       }
                    }
                 }
/*
                 if (strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                     strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL)
                    strcat(pcgOutBuf,pcgCDEnd);
*/
                 if (
                       strstr(&rgXml.rlLine[ilI].pclData[0],"<") != NULL ||
                       strstr(&rgXml.rlLine[ilI].pclData[0],">") != NULL ||
                       strcmp(&rgXml.rlLine[ilI].pclType[0],"CDATA") == 0
                    )
                    strcat(pcgOutBuf,pcgCDEnd);


                 strcat(pcgOutBuf,"</");
                 strcat(pcgOutBuf,pclName);
                 strcat(pcgOutBuf,">");
              }
              if (ilStartCount == TRUE)
                 *ipCount = *ipCount + 1;
           }
        }
        if (ilIgnore == FALSE)
           strcat(pcgOutBuf,"\n");
     }
  } /* for loop */
  if (ilDiscard == TRUE)
  {
     dbg(DEBUG,"%s Nothing send for Section <%s> because Field %s is empty",pclFunc,pcpCurSec,pclDisName);
     *ipCount = 0;
  }
  else
     dbg(DEBUG,"%s %d Lines of Information for Section <%s>",pclFunc,*ipCount,pcpCurSec);
  if (*ipCount > 0 || clActionType == *pcgActionTypeDel)
  {
     dbg(DEBUG,"%s Output = \n%s",pclFunc,pcgOutBuf);
/*snap(pcgOutBuf,strlen(pcgOutBuf),outp);*/
     ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"MODID",CFG_STRING,pclModId);
     if (ilRC != RC_SUCCESS)
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"MODID",CFG_STRING,pclModId);
        if (ilRC == RC_SUCCESS)
        {
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"COMMAND",CFG_STRING,pclCommand);
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"TABLE",CFG_STRING,pclTable);
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"SELECTION",CFG_STRING,pclSelection);
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"FIELDS",CFG_STRING,pclFields);
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"PRIORITY",CFG_STRING,pclPriority);
           if (ilRC != RC_SUCCESS)
              strcpy(pclPriority,"3");
           ilRC = iGetConfigEntry(pcgConfigFile,pcpCurSec,"TWS",CFG_STRING,pclTws);
           if (ilRC != RC_SUCCESS || *pclTws == ' ' || *pclTws == '\0')
              strcpy(pclTws,pcgTwStart);
        }
        else
        {
           dbg(TRACE,"%s MODID not configured in [%s] nor [%s]",pclFunc,pcpIntfNam,pcpCurSec);
           return RC_FAIL;
        }
     }
     else
     {
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"COMMAND",CFG_STRING,pclCommand);
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"TABLE",CFG_STRING,pclTable);
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"SELECTION",CFG_STRING,pclSelection);
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"FIELDS",CFG_STRING,pclFields);
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"PRIORITY",CFG_STRING,pclPriority);
        if (ilRC != RC_SUCCESS)
           strcpy(pclPriority,"3");
        ilRC = iGetConfigEntry(pcgConfigFile,pcpIntfNam,"TWS",CFG_STRING,pclTws);
        if (ilRC != RC_SUCCESS || *pclTws == ' ' || *pclTws == '\0')
           strcpy(pclTws,pcgTwStart);
     }

     ilNoItems = GetNoOfElements(pclModId,',');
     //Frank 20121125 v1.53
     if(strcmp(pcgDXBHoneywell,"YES")!=0)
     {
         if (ilNoItems > 1)
         {
            if (clActionType == *pcgActionTypeIns)
               get_real_item(pclTmpBuf,pclModId,1);
            else if (clActionType == *pcgActionTypeDel)
               get_real_item(pclTmpBuf,pclModId,3);
            else
               get_real_item(pclTmpBuf,pclModId,2);
            strcpy(pclModId,pclTmpBuf);
         }
     }

     ilNoItems = GetNoOfElements(pclCommand,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclCommand,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclCommand,3);
        else
           get_real_item(pclTmpBuf,pclCommand,2);
        strcpy(pclCommand,pclTmpBuf);
     }

     ilNoItems = GetNoOfElements(pclTable,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclTable,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclTable,3);
        else
           get_real_item(pclTmpBuf,pclTable,2);
        strcpy(pclTable,pclTmpBuf);
     }

     ilNoItems = GetNoOfElements(pclSelection,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclSelection,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclSelection,3);
        else
           get_real_item(pclTmpBuf,pclSelection,2);
        strcpy(pclSelection,pclTmpBuf);
     }
     ilNoItems = GetNoOfElements(pclFields,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclFields,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclFields,3);
        else
           get_real_item(pclTmpBuf,pclFields,2);
        strcpy(pclFields,pclTmpBuf);
     }
     ilNoItems = GetNoOfElements(pclPriority,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclPriority,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclPriority,3);
        else
           get_real_item(pclTmpBuf,pclPriority,2);
        strcpy(pclPriority,pclTmpBuf);
     }
     ilNoItems = GetNoOfElements(pclTws,',');
     if (ilNoItems > 1)
     {
        if (clActionType == *pcgActionTypeIns)
           get_real_item(pclTmpBuf,pclTws,1);
        else if (clActionType == *pcgActionTypeDel)
           get_real_item(pclTmpBuf,pclTws,3);
        else
           get_real_item(pclTmpBuf,pclTws,2);
        strcpy(pclTws,pclTmpBuf);
     }
     ilPriority = atoi(pclPriority);

      //Frank 20121125 v1.53
     if(strcmp(pcgDXBHoneywell,"YES")!=0)
     {
         ilModId = atoi(pclModId);
         dbg(TRACE,"%s ModId:     <%d>",pclFunc,ilModId);
         dbg(TRACE,"%s Command:   <%s>",pclFunc,pclCommand);
         dbg(TRACE,"%s Table:     <%s>",pclFunc,pclTable);
         dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
         dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFields);
         dbg(TRACE,"%s Priority:  <%d>",pclFunc,ilPriority);
         dbg(TRACE,"%s TWS:       <%s>",pclFunc,pclTws);

         if (strcmp(pcgSendOutput,"YES") == 0)
         {
            ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pclTws,pcgTwEnd,
                                 pclCommand,pclTable,pclSelection,pclFields,pcgOutBuf,"",ilPriority,NETOUT_NO_ACK);
         }
         else
         {
                dbg(TRACE,"%s ATTENTION: OUTPUT NOT SENT DUE TO CONFIG PARAMETER",pclFunc);
         }
       }
       else
       {

       	sprintf(pclMsgTmp,"%d",igMsgIdHoneyWell-1);
     		strcpy(pclSelection,pclMsgTmp);

            ilNoItems = 0;
            ilModId = 0;
            memset(pclTmpBuf,0,sizeof(pclTmpBuf));
            ilNoItems = GetNoOfElements(pclModId,',');
            for (ilI = 1; ilI <= ilNoItems; ilI++)
            {
                get_real_item(pclTmpBuf,pclModId,ilI);
                ilModId = atoi(pclTmpBuf);

                dbg(TRACE,"%s ModId:     <%d>",pclFunc,ilModId);
                    dbg(TRACE,"%s Command:   <%s>",pclFunc,pclCommand);
                    dbg(TRACE,"%s Table:     <%s>",pclFunc,pclTable);
                    dbg(TRACE,"%s Selection: <%s>",pclFunc,pclSelection);
                    dbg(TRACE,"%s Fields:    <%s>",pclFunc,pclFields);
                    dbg(TRACE,"%s Priority:  <%d>",pclFunc,ilPriority);
                    dbg(TRACE,"%s TWS:       <%s>",pclFunc,pclTws);

                    if (strcmp(pcgSendOutput,"YES") == 0)
                    {
                      ilRC = SendCedaEvent(ilModId,0,mod_name,"CEDA",pclTws,pcgTwEnd,
                                           pclCommand,pclTable,pclSelection,pclFields,pcgOutBuf,"",ilPriority,NETOUT_NO_ACK);
                    }
                    else
                    {
                        dbg(TRACE,"%s ATTENTION: OUTPUT NOT SENT DUE TO CONFIG PARAMETER",pclFunc);
                    }
            }
       }
  }

  return ilRC;
}

static void FindNextStandId(char *pcpUrno,char *pcpNpst)
{
    char *pclFunc = "FindNextStandId";
    int ilCol = 0,ilPos = 0;
    int ilRc = 0;
    int flds_count = 0;
    static int ilPsta = 0;
    static int ilRkey = 0;
    static int ilPstd = 0;

    short slSqlFunc1 = 0;
    short slCursor1 = 0;
    char pclPSTA1[16] = "\0";
    char pclRKEY[16] = "\0";
    char slSqlBuf1[1024] = "\0";
    char pclDataArea1[1024] = "\0";

    short slSqlFunc2 = 0;
    short slCursor2 = 0;
    char pclPSTA2[16] = "\0";
    char pclPSTD2[16] = "\0";
    char slSqlBuf2[1024] = "\0";
    char pclDataArea2[1024] = "\0";

    char pclNextStandIdFieldList[1024] = "\0";

    dbg(DEBUG,"<%s>pcpUrno<%s>",pclFunc,pcpUrno);

    strcpy(pclNextStandIdFieldList,"URNO,PSTA,ADID,RKEY,OFBL,ONBL,FTYP");
    FindItemInList(pclNextStandIdFieldList,"PSTA",',',&ilPsta,&ilCol,&ilPos);
    FindItemInList(pclNextStandIdFieldList,"RKEY",',',&ilRkey,&ilCol,&ilPos);

    slSqlFunc1 = START;
    slCursor1 = 0;

    flds_count = get_flds_count(pclNextStandIdFieldList);

    sprintf( slSqlBuf1, "SELECT %s FROM AFTTAB WHERE URNO = '%s'",pclNextStandIdFieldList,pcpUrno);
    dbg(DEBUG,"%s slSqlBuf1<%s>",pclFunc,slSqlBuf1);

    if((ilRc = sql_if(slSqlFunc1,&slCursor1,slSqlBuf1,pclDataArea1)) == DB_SUCCESS)
    {
        BuildItemBuffer(pclDataArea1, NULL, flds_count, ",");
        dbg(TRACE,"<%s> pclDataArea1<%s>",pclFunc,pclDataArea1);

        get_fld_value(pclDataArea1,ilPsta,pclPSTA1); TrimRight(pclPSTA1);
        get_fld_value(pclDataArea1,ilRkey,pclRKEY); TrimRight(pclRKEY);

        dbg(DEBUG,"<%s> PSTA1 <%s>",pclFunc,pclPSTA1);
        dbg(DEBUG,"<%s> RKEY <%s>",pclFunc,pclRKEY);

        if(strlen(pclPSTA1) == 0 || strcmp(pclPSTA1," ") == 0)
        {
            memset(pcpNpst,0,sizeof(pcpNpst));
            dbg(DEBUG,"<%s> pclPSTA1<%s>pcpNpst<NULL>",pclFunc,pclPSTA1);
        }
        else
        {
            slSqlFunc2 = START;
            slCursor2 = 0;
            sprintf( slSqlBuf2, "SELECT %s FROM AFTTAB WHERE RKEY = '%s' AND PSTD = '%s'",pclNextStandIdFieldList,pclRKEY,pclPSTA1);

            dbg(DEBUG,"%s slSqlBuf2<%s>",pclFunc,slSqlBuf2);

            if((ilRc = sql_if(slSqlFunc2,&slCursor2,slSqlBuf2,pclDataArea2)) == DB_SUCCESS)
            {
                BuildItemBuffer(pclDataArea2, NULL, flds_count, ",");
                dbg(TRACE,"<%s> pclDataArea2<%s>",pclFunc,pclDataArea2);

                get_fld_value(pclDataArea2,ilPsta,pclPSTA2); TrimRight(pclPSTA2);
                get_fld_value(pclDataArea2,ilPstd,pclPSTD2); TrimRight(pclPSTD2);

                dbg(DEBUG,"<%s> PSTA <%s>",pclFunc,pclPSTA2);
                dbg(DEBUG,"<%s> PSTD <%s>",pclFunc,pclPSTD2);
            }
            close_my_cursor(&slCursor2);

            if(strlen(pclPSTA2) != 0)
            {
                strcpy(pcpNpst,pclPSTA2);
                dbg(DEBUG,"<%s> pcpNpst<%s>",pclFunc,pcpNpst);
            }
            else
            {
                memset(pcpNpst,0,sizeof(pcpNpst));
                dbg(DEBUG,"<%s> pclPSTA2<%s>pcpNpst<NULL>",pclFunc,pclPSTA2);
            }
        }
    }
    close_my_cursor(&slCursor1);
}

static void get_fld_value(char *buf,short fld_no,char *dest)
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

static int get_flds_count(char *buf)
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
//Frank v1.52 20121226

static int AddAdid_EFPTAB_HWE(char *pcpTable,char *pcpCommand,char *pclFieldList,char *pclFields,char *pclNewData,char *pclDataBuf,char *pclOldData)
{
	int ilRc = RC_SUCCESS;
	int ilItemNo = 0;
	int ilNoEle = 0;
	int ilI = 0;
	char *pclFunc = "AddAdid_EFPTAB_HWE";

	TrimRight(pcpTable);
	TrimRight(pcpCommand);
	TrimRight(pclFieldList);
	TrimRight(pclFields);
	TrimRight(pclNewData);
	TrimRight(pclDataBuf);

	if(strlen(pcpTable) == 0 || strlen(pcpCommand) == 0 || strlen(pclFieldList) == 0 || strlen(pclFields) == 0 || strlen(pclNewData) == 0 || strlen(pclDataBuf) == 0)
	{
		ilRc = RC_FAIL;
		dbg(DEBUG,"%s pcpTableName or pcpCommand or pcpFieldList or pclDataBuf is invalid",pclFunc);
		return ilRc;
	}

	if( strcmp(pcpTable,pcgTableName) == 0 )
  {
  	if(strstr("DFR,IFR,UFR",pcpCommand) != NULL)
		{
			if(strncmp(pcgChgAdid,"YES",3) == 0)
			{
				ilNoEle = GetNoOfElements(pclFields,',');

				if(ilNoEle == 1 && strcmp(pclFields,"URNO") == 0)
				{
        		ilRc = RC_FAIL;

						dbg(DEBUG,"%s IFR from HoneyWell interface",pclFunc,pclFields);

						return ilRc;
				}

				ilItemNo = get_item_no(pclFieldList,"ADID",5) + 1;
     		get_real_item(pcgAdid,pclDataBuf,ilItemNo);

				dbg(DEBUG,"%s pcgChgAdid<%s>pcpTableName<%s>pcpCommand<%s>",pclFunc,pcgChgAdid,pcgTableName,pcpCommand);
				dbg(DEBUG,"%s pclFields<%s>pclNewData<%s>pcgAdid<%s>",pclFunc,pclFields,pclNewData,pcgAdid);
				dbg(DEBUG,"%s pclFields<%s>pclOldData<%s>pcgAdid<%s>",pclFunc,pclFields,pclOldData,pcgAdid);

				if(pclFields[strlen(pclFields)-1] != ',')
				{
					strcat(pclFields,",");
				}
				strcat(pclFields,"ADID");

				if(pclNewData[strlen(pclNewData)-1] != ',')
				{
					strcat(pclNewData,",");
					strcat(pclOldData,",");
				}
				strcat(pclNewData,pcgAdid);
				strcat(pclOldData,pcgAdid);
			}
			else
			{
				dbg(DEBUG,"%s pcgChgAdid<%s> is not enable",pclFunc,pcgChgAdid);
				ilRc = RC_FAIL;
				return ilRc;
			}
		}
		else
		{
			dbg(DEBUG,"%s pcpCommand<%s> is not DFR,IFR,UFR",pclFunc,pcpCommand);
			ilRc = RC_FAIL;
			return ilRc;
		}
  }
  else
  {
  	dbg(DEBUG,"%s pcpTable<%s> is not hweview",pclFunc,pcpTable);
		ilRc = RC_FAIL;
		return ilRc;
  }

	dbg(DEBUG,"%s pclFields<%s>pcpNewData<%s>pclOldData<%s>pcgAdid<%s>",pclFunc,pclFields,pclNewData,pclOldData,pcgAdid);

  return ilRc;
}


int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData)
{
    char *pclFunc = "extractField";

    int ilItemNo = 0;

    ilItemNo = get_item_no(pcpFields, pcpFieldName, 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No <%s> Found in the field list, return", pclFunc, pcpFieldName);
        return RC_FAIL;
    }

    get_real_item(pcpFieldVal, pcpNewData, ilItemNo);
    dbg(TRACE,"<%s> The New %s is <%s>", pclFunc, pcpFieldName, pcpFieldVal);

    return RC_SUCCESS;
}

static int getFltId(FLIGHT_ID *rpFltId, char *pcpFieldList, char *pcpDatalist)
{
	int ilRc = RC_FAIL;
	char *pclFunc = "getFltId";
	char pclTmp[256] = "\0";
	char pclTmpFlno[256] = "\0";

	memset(rpFltId,0,sizeof(rpFltId));

	ilRc = extractField(pclTmp, "ALC3", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The ALC3 value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The ALC3 value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclAlc3,pclTmp);
	}

	ilRc = extractField(pclTmp, "FLNO", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The FLNO value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The FLNO value<%s> is valid", pclFunc, pclTmp);
		strcat(rpFltId->pclFltn, pclTmp);

		if (isalpha(pclTmp[strlen(pclTmp)-1]))
		{
            rpFltId->pclFltn[strlen(rpFltId->pclFltn)-1] = '0';
            rpFltId->pclFlns[0] = pclTmp[strlen(pclTmp)-1];
		}
		else
		{
		   strcpy(rpFltId->pclFlns," ");
		}
	}

	ilRc = extractField(pclTmp, "ARRDEP", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The ARRDEP value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The ARRDEP value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclAdid,pclTmp);
	}

	if (strncmp(rpFltId->pclAdid,"A",1) == 0)
	{
		ilRc = extractField(pclTmp, "STOA", pcpFieldList, pcpDatalist);
	}
	else
	{
		ilRc = extractField(pclTmp, "STOD", pcpFieldList, pcpDatalist);
	}
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The STOAD value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The STOAD value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclStoad,pclTmp);
	}

	return RC_SUCCESS;
}

static int getFlightUrno(char *pcpUrno, FLIGHT_ID rlFltId)
{
    int ilRc = RC_FAIL;
	char *pclFunc = "getFlightUrno";
	char pclSqlBuf[2048] = "\0";
    char pclSqlData[2048] = "\0";

	memset(pclSqlData,0,sizeof(pclSqlData));
	memset(pclSqlBuf,0,sizeof(pclSqlBuf));
	if (strncmp(rlFltId.pclAdid,"A",1) == 0)
	{
		sprintf(pclSqlBuf, "SELECT URNO from AFTTAB WHERE ALC3='%s' AND FLTN='%s' AND FLNS = '%s' AND STOA='%s' AND ADID='%s'",rlFltId.pclAlc3, rlFltId.pclFltn, rlFltId.pclFlns, rlFltId.pclStoad, rlFltId.pclAdid);
	}
	else
	{
		sprintf(pclSqlBuf, "SELECT URNO from AFTTAB WHERE ALC3='%s' AND FLTN='%s' AND FLNS = '%s' AND STOD='%s' AND ADID='%s'",rlFltId.pclAlc3, rlFltId.pclFltn, rlFltId.pclFlns, rlFltId.pclStoad, rlFltId.pclAdid);
	}

	ilRc = RunSQL(pclSqlBuf, pclSqlData);
	if (ilRc != DB_SUCCESS)
	{
		dbg(TRACE, "<%s>: Retrieving flight urno - Fails", pclFunc);
		return RC_FAIL;
	}
	switch(ilRc)
	{
		case NOTFOUND:
			dbg(TRACE, "<%s> Retrieving flight urno - Not Found", pclFunc);
			ilRc = NOTFOUND;
			break;
		default:
			dbg(TRACE, "<%s> Retrieving flight urno - Found <%s>", pclFunc, pclSqlData);
			strcpy(pcpUrno,pclSqlData);
			ilRc = RC_SUCCESS;
			break;
	}
	return ilRc;
}

static int getHermesFieldList(char *pcphermesDataList, char *pcpFlightDataList, char *pcpFlightFieldList, char *pcpFieldList, char *pcpDataList, char *pcpHermesFields)
{
	int ilFieldListNo = 0;
    int ilDataListNo = 0;
	int ilEleCount = 0;
	char *pclFunc = "getHermesFieldList";
	char pclTmpFieldName[64] = "\0";
    char pclTmpData[64] = "\0";

	ilFieldListNo = GetNoOfElements(pcpFieldList, ',');
	ilDataListNo  = GetNoOfElements(pcpDataList, ',');

	if (ilFieldListNo != ilDataListNo)
	{
		dbg(TRACE,"%s The data<%d> and field<%d> list number is not matched", pclFunc, ilDataListNo, ilFieldListNo);
		return RC_FAIL;
	}

	for(ilEleCount = 1; ilEleCount <= ilFieldListNo; ilEleCount++)
	{
		get_item(ilEleCount, pcpFieldList, pclTmpFieldName, 0, ",", "\0", "\0");
		TrimRight(pclTmpFieldName);

		if(strcmp(pclTmpFieldName,"ARRDEP")==0)
		{
		    strcpy(pclTmpFieldName,"ADID");
		}

		get_item(ilEleCount, pcpDataList, pclTmpData, 0, ",", "\0", "\0");
		TrimRight(pclTmpData);

		if (strstr(pcpHermesFields, pclTmpFieldName) == 0)
		{
			if (strlen(pcpFlightFieldList) == 0 )
			{
				strcat(pcpFlightFieldList, pclTmpFieldName);
				strcat(pcpFlightDataList,  pclTmpData);
			}
			else
			{
				strcat(pcpFlightFieldList,",");
				strcat(pcpFlightFieldList, pclTmpFieldName);
				strcat(pcpFlightDataList,",");
				strcat(pcpFlightDataList,  pclTmpData);
			}
		}
		else
		{
			if (strlen(pcphermesDataList) == 0 )
			{
				strcat(pcphermesDataList, pclTmpData);
			}
			else
			{
				strcat(pcphermesDataList,",");
				strcat(pcphermesDataList,  pclTmpData);
			}
		}
	}
	return RC_SUCCESS;
}

static int handleHemers(char *pcpSecName, char *pcpFieldList, char *pcpDataList, char *pcpDestModId, int ipPrio, char *pcpCmd, char *pcpTable)
{
	char *pclFunc = "handleHemers";
	char pclUrno[16] = "\0";
	char pclHemersFieldList[1024] = "\0";
	char pclFlightFieldList[1024] = "\0";
	char pclHemersDataList[1024] = "\0";
	char pclFlightDataList[1024] = "\0";
	FLIGHT_ID rlFltId;

	/*
	herhdl 20140903074903:092:SECTION <FIS_Flight> CONTAINS 14 DATA TAGS
	herhdl 20140903074903:092:SECTION TRIGGER NAME <OUT>
	herhdl 20140903074903:093:FIELDS <ALC3,FLNO,STOA,ARRDEP,ACT3,REGN,TCAR,TMAI,LCAR,TRCAR,TSCAR,LMAI,TRMA,TSMA>
	herhdl 20140903074903:093:DATA <ETD,407,20140301103000,A,320,A6ETB,12473,0,12473,0,2222,0,0,0>
	herhdl 20140903074903:093:MOD_ID <7800>
	herhdl 20140903074903:093:PRIO <3>
	herhdl 20140903074903:093:CMD <UFR>
	herhdl 20140903074903:093:TABLE <AFTTAB>
	*/

	/*1-Get flight identification*/
	if (getFltId(&rlFltId, pcpFieldList, pcpDataList) == RC_FAIL)
	{
		dbg(TRACE,"%s Not all flt_id are valid->return",pclFunc);
		return RC_FAIL;
	}

	/*2-Search flight*/
	if (getFlightUrno(pclUrno, rlFltId) != RC_SUCCESS)
	{
		dbg(TRACE,"%s Flight is not found, return",pclFunc);
		return RC_FAIL;
	}
	dbg(TRACE,"%s Found flight URNO<%s>",pclFunc, pclUrno);

	/*3-Get hermes field list*/
	getHermesFieldList(pclHemersDataList, pclFlightDataList, pclFlightFieldList, pcpFieldList, pcpDataList, pcgHermesFields);

	dbg(TRACE,"%s pcgHermesFields<%s>",pclFunc,pcgHermesFields);
	dbg(TRACE,"%s pclHemersDataList<%s>",pclFunc,pclHemersDataList);
	dbg(TRACE,"%s pclFlightFieldList<%s>",pclFunc,pclFlightFieldList);
	dbg(TRACE,"%s pclFlightDataList<%s>",pclFunc,pclFlightDataList);
}

static void removeTag(char *pcpData, char *pcpTagName)
{
	char pclTagBegin[256] = "\0";
	char pclTagEnd[256] = "\0";
	char pclResult[4096] = "\0";
	char *pclTmpBegin = NULL;
	char *pclTmpEnd = NULL;
	char pclTmp[1024] = "\0";

	memset(pclResult,0,sizeof(pclResult));
	memset(pclTmp,0,sizeof(pclTmp));

	sprintf(pclTagBegin,"<%s>",pcpTagName);
	sprintf(pclTagEnd,"</%s>",pcpTagName);
	//dbg(DEBUG,"pclTagBegin<%s> pclTagEnd<%s>",pclTagBegin,pclTagEnd);

	pclTmpBegin = strstr(pcpData,pclTagBegin);
	pclTmpEnd = strstr(pcpData,pclTagEnd);

	strncpy(pclResult,pcpData,strlen(pcpData)-strlen(pclTmpBegin));
	//dbg(DEBUG,"1-<%s>",pclResult);

	strncpy(pclTmp,pclTmpBegin+strlen(pclTagBegin)+1,strlen(pclTmpBegin)-strlen(pclTmpEnd)-strlen(pclTagEnd));
	//dbg(DEBUG,"2-<%s>",pclTmp);

	strcat(pclResult,pclTmp);
	//dbg(DEBUG,"3-<%s>",pclResult);

	strcpy(pclTmp,pclTmpEnd+1+strlen(pclTagEnd));
	//dbg(DEBUG,"4-<%s>",pclTmp);
	strcat(pclResult,pclTmp);

	strcpy(pcpData,pclResult);
}
