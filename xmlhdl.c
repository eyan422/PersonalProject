#ifndef _DEF_mks_version
    #define _DEF_mks_version
    #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
    static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Interface/xmlhdl.c 1.18 05/09/2014 10:00:00 AM Exp FYA $";
#endif /* _DEF_mks_version */
    

/********************************************************************************
*                      
* Author         : won 
* Date           : 20130322
* Description    : For EK.  XML parser then save in DB.
*                                                                     
* Update history :                                                   
*  20130322 WON 1.00   	First release.
*  20130408 WON 1.01	Introduce REMOVE_NEW_LINE in config file.  This variable holds all fields that new line must be removed.
*                       Make sure USR when passing to flight is program name (mod_name).
*  20130416 WON 1.02	Completed Towings (insert only)                
*  20130417 WON 1.03	Make <ACTIONTYPE> a priority.   
*						Modified Towing to handle update.   
*  20130424 WON 1.04	Introduce REM1 = CNLD
*                       Remove memset on MAP and RECORD.
*  20130514 WON 1.05	Do performance enhancing.
*  20130523 WON 1.06	Crashing after receiving "constructWhereClause:: ERROR!!! RL is NULL"
*                       If afttab record is more than 1, do not process msg.
*  20130528 WON 1.07    Commented REM1 = CNLD.
*  20130528 WON 1.08    Make searching for afttab in memory configurable.
*  20130607 YYA 1.09    Modified for the Select condition to support OR (|)
*  20130917 YYA 1.11    Skip 1.10 change for TOWING DELAY UPDATE/DELET/INSERT
*                       Implement for the refer table support
*  20131107 YYA 1.12    Reimplment the Towing flight
*  20140106 YYA 1.13    Enhance For Patch Fld
*  20140106 YYA 1.14    Enhance to accept Join flight 
*  20140221 YYA 1.14a   Fix for the GROUP LIST URNO_GENERATOR
*  20140313 YYA 1.14b   Enhance For Patch Fld for INFOBJ_GENERIC/INFOBJ_FLIGHT
*  20140313 YYA 1.15    Arr REGN Change not apply to dep
*  20140325 YYA 1.15a   Pass Selection for the Insert CMD
*  20140826 YYA 1.15b   Increase the setting MAX_RECORD from 32 to 128
                        Disable the sumary log
*  20140905 YYA 1.15c   UFIS-5844 Implement for FullSet List Check(Delete Old Data)
*  20141124 FYA 1.16    UFIS-5844 Implement for basic data check
********************************************************************************/



/**********************************************************************************/
/*                                                                                */
/* be carefule with strftime or similar functions !!!                             */
/*                                                                                */
/**********************************************************************************/
/* This program is a MIKE main program */
#define U_MAIN
#define UGCCS_PRG
#define STH_USE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>

#include "debugrec.h"
#include "db_if.h"
#include "urno_fn.inc"

#include "tools.h"
#include "cli.h"
#include "queutil.h"
#include "syslib.h"



/******************************************************************************/
/* Macros */
/******************************************************************************/
#define MAX_ACTIONTYPE  1
#define MAX_COMMAND 10
#define MAX_FLIGHT  1000
#define MAX_KEY		127
#define MAX_MAP     512
#define MAX_RECORD	128
#define MAX_SELECT  20
#define MAX_VALUE	1023
#define MAX_XML     32767


/*  DB FIELDS */
#define MAX_ADID    1
#define MAX_ACT3    3
#define MAX_ACT5    5
#define MAX_ALC2    2
#define MAX_ALC3    3
#define MAX_DATE    14
#define MAX_DES3    3
#define MAX_DES4    4
#define MAX_ORG3    3
#define MAX_ORG4    4
#define MAX_RTYP    1
#define MAX_FTYP    1
#define MAX_REGN    12
#define MAX_FLNO    9
#define MAX_URNO2	32

#define MAX_DEN_DECA 8
#define MAX_DEN_DECN 2
#define MAX_DEN_URNO 10

#define MAX_TAB_LEN 16
#define MAX_LST_LEN 512
#define MAX_DAT_LEN 1024

static char pcgDentabFields[]="DECA,DECN,URNO";
static int igDentabFieldCount=3;
typedef struct
{
    char DECA[MAX_DEN_DECA+1];
    char DECN[MAX_DEN_DECN+1];	
    char URNO[MAX_DEN_URNO+1];
}_DENTAB;
_DENTAB rgDentab;

typedef struct
{
    char URNO[MAX_URNO2+1];
    char WHERE[MAX_VALUE+1];	
}_URNOBUFFER;
_URNOBUFFER *rgpUrnoBuffer;
int igMaxUrnoBufferSize;
int igInsertUrno;


typedef struct
{
    char TYPE; /*T for table, X for XML */
    char HEADER[MAX_KEY+1];
    char FIELD[MAX_KEY+1];	
    char OPS[MAX_KEY+1];
    char VALUE[MAX_VALUE+1];    
}_SELECTIONFIELD;

typedef struct
{
    char TYPE;
    char HEADER[MAX_KEY+1];
    char FIELD[MAX_KEY+1];
    char OPS[MAX_KEY+1];
    char VALUE[MAX_VALUE+1];
    int igSelectSize;
    _SELECTIONFIELD rgSelect[MAX_SELECT];	
    char ADDITIONALWHERE[MAX_VALUE+1];
}_SELECTGROUP;

static char pcgAfttabFields[]="URNO,FLNO,ADID,RKEY,STOA,STOD,ACT3,ACT5,ALC2,ALC3,DES3,DES4,ORG3,ORG4,RTYP,FTYP,REGN";
static int igAfttabFieldCount=17;
typedef struct
{
    char URNO[MAX_URNO2+1];	
    char FLNO[MAX_FLNO+1];
    char ADID[MAX_ADID+1];    
    char RKEY[MAX_URNO2+1];
    char STOA[MAX_DATE+1];
    char STOD[MAX_DATE+1];
    char ACT3[MAX_ACT3+1];
    char ACT5[MAX_ACT5+1];
    char ALC2[MAX_ALC2+1];
    char ALC3[MAX_ALC3+1];
    char DES3[MAX_DES3+1];
    char DES4[MAX_DES4+1];
    char ORG3[MAX_ORG3+1];
    char ORG4[MAX_ORG4+1];
    char RTYP[MAX_RTYP+1];
    char FTYP[MAX_FTYP+1];
    char REGN[MAX_REGN+1];
}_AFTTAB;
_AFTTAB rgAfttab;

typedef struct 
{
	char MESSAGETYPE[MAX_VALUE+1];
	char MESSAGEORIGIN[MAX_VALUE+1];	
	char ACTIONTYPE[MAX_ACTIONTYPE+1];
    char ADID[MAX_ADID+1];
    char STDT[MAX_DATE+1];
    char FLNO[MAX_FLNO+1];	
}_INFOGENERIC;

typedef struct
{
    char key[MAX_KEY+1];
    char value[MAX_VALUE+1];	
}_MAP;

typedef struct
{
	char LIST[MAX_KEY+1];
	char GROUP[MAX_KEY+1];
	char TABLE[MAX_KEY+1];
	int ilSendQueue;
	char DELCMD[MAX_COMMAND+1];
	char UPDCMD[MAX_COMMAND+1];
	char INSCMD[MAX_COMMAND+1];	
	
	char URNOGENERATOR[MAX_KEY+1];
	char MODE[MAX_VALUE+1];
	int igFullSet;
	char FullSet_InclFld[MAX_LST_LEN+1];
	char FullSet_InclDat[MAX_LST_LEN+1];
	char FullSet_ExclFld[MAX_LST_LEN+1];
	char FullSet_ExclDat[MAX_LST_LEN+1];
	int igSelectGroup;
	_SELECTGROUP *rgSelect;
}_XMLLIST;

typedef struct
{
    int iSize;
    _XMLLIST *rgXMLList;
    char WHERE[MAX_VALUE+1];
    _MAP MAP[MAX_MAP];     	
}_XMLRECORD;

typedef struct
{
    char GrpNam[MAX_LST_LEN];
    int  Patch_LstCnt;
    char Patch_OrgLst[MAX_DAT_LEN];
    char Patch_NewLst[MAX_DAT_LEN];
    char Patch_DatLst[MAX_DAT_LEN];
    int  RefValid;
    char RefTab[MAX_TAB_LEN];
    char RefKeyLst[MAX_LST_LEN];
    char RefKeyDat[MAX_DAT_LEN];
    char RefGetLst[MAX_LST_LEN];
    char RefSetLst[MAX_LST_LEN];
    char RefVarLst[MAX_LST_LEN];
    char RefVarDat[MAX_DAT_LEN];
}_XMLGrpSet;

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
int  debug_level = DEBUG;
int igUpdate = 0;
char pcgProgramName[MAX_KEY+1];

char pclWon[32768];
int iWonCnt=0;
FILE *fptr;

/******************************************************************************/
/* Declarations                                                               */
/******************************************************************************/



/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static char pcgConfigFile[512];
static int igItemLen=0; /* length of incoming item */
static ITEM *prgItem = NULL; /* The queue item pointer  */
static EVENT *prgEvent=NULL; /* The event pointer */
static int igInitOK = FALSE;
int  igDbgSave = DEBUG;
char cgHopo[8] = "";      /* default home airport    */
static int igQueOut = 0;
static char pcgTwStart [64];
static char pcgTwEnd [64];
static char pcgAftDestName [64];
static char pcgAftRecvName [64];
static int igGlNoAck = 0; /* flag for acknowledgement */
static char pcgCedaCmd [64];/* last command */
char pcgRemoveNewLine[128];

int pcgListCount;
int pcgRecordCount;
_XMLRECORD rgXMLRecords[MAX_RECORD];
_XMLLIST *rgXMLList;
_INFOGENERIC rgInfoGeneric;

static char pcgTowing_Table[16];

_AFTTAB rgFlight[MAX_FLIGHT];
int igFlightIndexStart;
int igFlightIndexCounter;
int igFlightIndexEnd=0;

static int igFlightQueue=7800;
static int igSqlhdlQueue=506;

static int igGrpSetCnt = 0;
_XMLGrpSet prgGrpSet[MAX_RECORD];

static int igArrRegnStk = FALSE;
static int igBasicDataCheck = FALSE;
static int igBasicDataCheck_FieldList = FALSE;
static int igBasicDataCheck_TableList = FALSE;

static char pcgBasicDataCheck_Field_list[2048] = "\0";
static char pcgBasicDataCheck_Table_list[2048] = "\0";
/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
static int HandleData(void); /* CEDA  event data     */
static void HandleErr(int); /* General errors */
static void HandleQueues(void); 
static void HandleQueErr(int);
static void HandleSignal(int);
static int InitPgm();
static int ReadConfigEntry(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer, char *pcpDefault);
static int ReadConfig(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer, char *pcpDefault);
static int Reset(void);
static void Terminate(void); 

void printConfig();
void printRecords();

int processXML(char *pcpXML);
int parseGroupXML(char *pcpXML, char *pcpHeader, char *pcpOut);
int parseXMLMap(char *pcpXML, _XMLRECORD *pXMLRecord);
int parseMultiXML(char *pcpXML, _XMLLIST *rpXMLList, char *pcpHeader, char *pcpTable, int *ipIndex);

int formUpdateSQLCommand(char *pcpOut, _XMLRECORD *pXMLRecord);
int formInsertSQLCommand(char *pcpOut, _XMLRECORD *pXMLRecord);

int processXMLRecords();
int getXMLRecordIndexByGroup(char *pcpGroup, int ipStart);

int populateInfoGeneric(_INFOGENERIC *pInfoGeneric, _XMLRECORD *pXMLRecord);
void printInfoGeneric(_INFOGENERIC *pInfoGeneric);

int getAfttabRecord(_AFTTAB *rpAft, char *pcpWhere);
int getDentabRecord(_DENTAB *rpDen, char *pcpDURN);
int processAfttabRecord(_AFTTAB *rpAft, _INFOGENERIC *rpInfo, _XMLRECORD *rpXMLRecord);
int parseSelectConfig(char *pcpSelect, char *pcpGroup, _SELECTGROUP *rpSelect);
int constructWhereClause(int ipSelf, char *pcpWhere, char *pcpFields, char *pcpValues, _SELECTGROUP *rpSelect, int ipCount); 
int constructAdditionalKeyValuePair(char *pcpFields, char *pcpValues, _SELECTGROUP *rpSelect, int ipCount); 
int getUrnoInList(char *pcpFlno, char *pcpAdid, char *pcpStdt);
int getKeyValue(char *pcpOut, char *pcpHeader, char *pcpKey);
int getKeyValuePair(char *pcpOut, char *pcpKey, _XMLRECORD *rpXML);

int evaluate(char *pcpDataType, char *pcpOp1, char *pcpOp, char *pcpOp2);
int checkRecordInDB(char *pcpSql);
int searchBufferUrno(char *pcpOut, char *pcpWhere);
int clearBufferUrno(char *pcpWhere);
int replaceChar(char *pcpString, char cpOld, char cpNew);
int getSystemDate(char *pcpOut);

int processTowingRecord(_AFTTAB *prpAft, _XMLRECORD *rpXMLRecord, char *pcpWhere);
int processTowingRecordOld(_AFTTAB *prpAft, _XMLRECORD *rpXMLRecord);
int processTowingDelayRecord(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord);


static int ReferTable(_AFTTAB *prpAft, _XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpDat);
static int GetAFTDat(_AFTTAB *prpAft, char *pcpFld, char *pcpDat);
static int GetXmlGrpDat(_AFTTAB *prpAft,_XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpDat);

static int BaseTblQry (char *pcpTbl, char *pcpKeyLst, char *pcpKeyDat, char *pcpQryLst, char *pcpQryRst, int ipShmFlg, int ipOrcFlg, char *pcpVarLst, char *pcpVarDat);

static int PatchFldLst(_AFTTAB *prpAft, _XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpVal, char *pcpGrpNam);

static int processJoinFlight(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord, char *pcpWhere);

static int ArrRegnStk(_AFTTAB *prpAft, char *pcpFldLst, char *pcpDatLst, int ipSendQ, char *pcpCmd, char *pcpTab);

static int FullSetCheckWhere(_AFTTAB *prpAft,_XMLRECORD *prpXMLRecord, char *pcpDatLst, char *pcpWhere);

static int GetElementValue(char *pclDest, char *pclOrigData, char *pclBegin, char *pclEnd);
static int basicDataCheck(char *pcpData, char *pcpBasicDataCheck_Field_list, char *pcpBasicDataCheck_Table_list);
static void TrimRight(char *pcpBuffer);
/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/

MAIN
{
    int ilRC = RC_SUCCESS; /* Return code            */
    int ilCnt = 0;
    
    INITIALIZE; /* General initialization    */
    strcpy(pcgProgramName, argv[0]);
    dbg(TRACE,"MAIN: version <%s>, program[%s]",mks_version, pcgProgramName);
    
    /* handles signal       */
    (void)SetSignals(HandleSignal);
    (void)UnsetSignals();


    /* Attach to the MIKE queues */
    do
    {
        ilRC = init_que();
        if(ilRC != RC_SUCCESS)
        {
            dbg(TRACE,"MAIN: init_que() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        }/* end of if */
    }
    while((ilCnt < 10) && (ilRC != RC_SUCCESS));
    
    if(ilRC != RC_SUCCESS)
    {
        dbg(TRACE,"MAIN: init_que() failed! waiting 60 sec ...");
        sleep(60);
        exit(1);
    }
    else
    {
        dbg(TRACE,"MAIN: init_que() OK! mod_id <%d>",mod_id);
    }/* end of if */

    sprintf(pcgConfigFile,"%s/%s.cfg",getenv("CFG_PATH"),mod_name);

    ilRC = SendRemoteShutdown(mod_id);
    if(ilRC != RC_SUCCESS)
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
            ilRC = InitPgm();
            if(ilRC != RC_SUCCESS)
            {
                dbg(TRACE,"MAIN: init failed!");
            } /* end of if */
        }/* end of if */
    } 
    else 
    {
        Terminate();
    }/* end of if */
    

    dbg(TRACE,"MAIN: initializing OK");
    dbg(TRACE,"==================== Entering main pgm loop  =================");
     
    for(;;)
    {
        dbg(TRACE,"==================== START/END =================");
        ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,igItemLen,(char *)&prgItem);
        
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = (EVENT *) prgItem->text;
                
        if( ilRC == RC_SUCCESS )
        {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
            if( ilRC != RC_SUCCESS ) 
            {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            } /* fi */
            
            switch( prgEvent->command )
            {
                case HSB_STANDBY:
                    ctrl_sta = prgEvent->command;
                    HandleQueues();
                    break;    
                case HSB_COMING_UP:
                    ctrl_sta = prgEvent->command;
                    HandleQueues();
                    break;    
                case HSB_ACTIVE :
                    ctrl_sta = prgEvent->command;
                    break;    
                case HSB_ACT_TO_SBY:
                    ctrl_sta = prgEvent->command;
                    /* CloseConnection(); */
                    HandleQueues();
                    break;    
                case HSB_DOWN :
                    /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    Terminate();
                    break;    
                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    break;    
                case SHUTDOWN:
                    /* process shutdown - maybe from uutil */
                    Terminate();
                    break;
                case RESET:
                    ilRC = Reset();
                    break;
                case EVENT_DATA:
                    if((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
                    {
                        ilRC = HandleData();
                        if(ilRC != RC_SUCCESS)
                        {
                            HandleErr(ilRC);
                        }/* end of if */
                    }
                    else
                    {
                        dbg(TRACE,"MAIN: wrong hsb status <%d>",ctrl_sta);
                        DebugPrintItem(TRACE,prgItem);
                        DebugPrintEvent(TRACE,prgEvent);
                    }/* end of if */
                    break;
                case TRACE_ON :
                    dbg_handle_debug(prgEvent->command);
                    break;
                case TRACE_OFF :
                    dbg_handle_debug(prgEvent->command);
                    break;
                default :
                    dbg(TRACE,"MAIN: unknown event");
                    DebugPrintItem(TRACE,prgItem);
                    DebugPrintEvent(TRACE,prgEvent);
                    break;
            } /* end switch */ 
        }
   
    } /* end for */    
    exit(0);
    
} /* end of MAIN */


static void HandleSignal(int ipSig)
{
    int ilRC = RC_SUCCESS; /* Return code */
    
    switch(ipSig)
    {
        case SIGTERM:
            dbg(TRACE,"HandleSignal: Received Signal <%d> (SIGTERM). Terminating now...",ipSig);
            break;
        case SIGALRM:
            dbg(TRACE,"HandleSignal: Received Signal<%d>(SIGALRM)",ipSig);
            return;
            break;
        default:
            dbg(TRACE,"HandleSignal: Received Signal<%d>",ipSig);
            return;
            break;
    } /* end of switch */
    exit(0);
    
} /* end of HandleSignal */



/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues()
{
    int    ilRC = RC_SUCCESS; /* Return code */
    int    ilBreakOut = FALSE;
    
    do
    {
        ilRC = que(QUE_GETBIGNW,0,mod_id,PRIORITY_3,igItemLen,(char *)&prgItem);
        /* depending on the size of the received item  */
        /* a realloc could be made by the que function */
        /* so do never forget to set event pointer !!! */
        prgEvent = (EVENT *) prgItem->text;    
        if( ilRC == RC_SUCCESS )
        {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
            if( ilRC != RC_SUCCESS ) 
            {
                /* handle que_ack error */
                HandleQueErr(ilRC);
            } /* fi */
        
            switch( prgEvent->command )
            {
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
                case HSB_ACT_TO_SBY    :
                    ctrl_sta = prgEvent->command;
                    break;    
                case HSB_DOWN:
                    /* whole system shutdown - do not further use que(), send_message() or timsch() ! */
                    ctrl_sta = prgEvent->command;
                    Terminate();
                    break;    
                case HSB_STANDALONE:
                    ctrl_sta = prgEvent->command;
                    ilBreakOut = TRUE;
                    break;    
                case SHUTDOWN:
                    Terminate();
                    break;                        
                case RESET:
                    ilRC = Reset();
                    break;
                case EVENT_DATA:
                    dbg(TRACE,"HandleQueues: wrong hsb status <%d>",ctrl_sta);
                    DebugPrintItem(TRACE,prgItem);
                    DebugPrintEvent(TRACE,prgEvent);
                    break;
                case TRACE_ON:
                    dbg_handle_debug(prgEvent->command);
                    break;
                case TRACE_OFF :
                    dbg_handle_debug(prgEvent->command);
                    break;
                default            :
                    dbg(TRACE,"HandleQueues: unknown event");
                    DebugPrintItem(TRACE,prgItem);
                    DebugPrintEvent(TRACE,prgEvent);
                    break;
            } /* end switch */
        }        
    } 
    while (ilBreakOut == FALSE);
    
    if(igInitOK == FALSE)
    {
            ilRC = InitPgm();
            if(ilRC != RC_SUCCESS)
            {
                dbg(TRACE,"HandleQueues: : init failed!");
            } /* end of if */
    }/* end of if */
} /* end of HandleQueues */
    


/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr(int pipErr)
{
    int    ilRC = RC_SUCCESS;
    
    switch(pipErr) 
    {
        case QUE_E_FUNC: /* Unknown function */
            dbg(TRACE,"HandleQueErr: <%d> : unknown function",pipErr);
            break;
        case QUE_E_MEMORY: /* Malloc reports no memory */
            dbg(TRACE,"HandleQueErr: <%d> malloc failed",pipErr);
            break;
        case QUE_E_SEND: /* Error using msgsnd */
            dbg(TRACE,"HandleQueErr: <%d> msgsnd failed",pipErr);
            break;
        case QUE_E_GET: /* Error using msgrcv */
            dbg(TRACE,"HandleQueErr: <%d> msgrcv failed",pipErr);
            break;
        case QUE_E_EXISTS:
            dbg(TRACE,"HandleQueErr: <%d> route/queue already exists ",pipErr);
            break;
        case QUE_E_NOFIND:
            dbg(TRACE,"HandleQueErr: <%d> route not found ",pipErr);
            break;
        case QUE_E_ACKUNEX:
            dbg(TRACE,"HandleQueErr: <%d> unexpected ack received ",pipErr);
            break;
        case QUE_E_STATUS:
            dbg(TRACE,"HandleQueErr: <%d>  unknown queue status ",pipErr);
            break;
        case QUE_E_INACTIVE:
            dbg(TRACE,"HandleQueErr: <%d> queue is inaktive ",pipErr);
            break;
        case QUE_E_MISACK:
            dbg(TRACE,"HandleQueErr: <%d> missing ack ",pipErr);
            break;
        case QUE_E_NOQUEUES:
            dbg(TRACE,"HandleQueErr: <%d> queue does not exist",pipErr);
            break;
        case QUE_E_RESP: /* No response on CREATE */
            dbg(TRACE,"HandleQueErr: <%d> no response on create",pipErr);
            break;
        case QUE_E_FULL:
            dbg(TRACE,"HandleQueErr: <%d> too many route destinations",pipErr);
            break;
        case QUE_E_NOMSG: /* No message on queue */
            dbg(TRACE,"HandleQueErr: <%d> no messages on queue",pipErr);
            break;
        case QUE_E_INVORG: /* Mod id by que call is 0 */
            dbg(TRACE,"HandleQueErr: <%d> invalid originator=0",pipErr);
            break;
        case QUE_E_NOINIT    :    /* Queues is not initialized*/
            dbg(TRACE,"HandleQueErr: <%d> queues are not initialized",pipErr);
            break;
        case QUE_E_ITOBIG:
            dbg(TRACE,"HandleQueErr: <%d> requestet itemsize to big ",pipErr);
            break;
        case QUE_E_BUFSIZ:
            dbg(TRACE,"HandleQueErr: <%d> receive buffer to small ",pipErr);
            break;
        default: /* Unknown queue error */
            dbg(TRACE,"HandleQueErr: <%d> unknown error",pipErr);
            break;
    } /* end switch */
         
    return;
} /* end of HandleQueErr */


/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate()
{ 
    dbg(TRACE,"Terminate: now leaving ...");
    
    exit(0);    
} /* end of Terminate */


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{
    int ilRC = RC_SUCCESS; /* Return code */
    
    return ilRC;    
} /* end of Reset */


/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int InitPgm()
{
    int ilRC = RC_SUCCESS; /* Return code */
    char pclDbgLevel [iMIN_BUF_SIZE];
    char pclGroup[1024];
    char pclTmp[1024];
    char pclSelect[1024];
    int ilCnt = 0 ;
    int a=0, b=0;
    int ilItmCnt1 = 0;
    int ilItmCnt2 = 0;
    int ilItmCnt3 = 0;

    /* now reading from configfile or from database */
    SetSignals(HandleSignal);

    do
    {
        ilRC = init_db();
        if (ilRC != RC_SUCCESS)
        {
            check_ret(ilRC);
            dbg(TRACE,"InitPgm: init_db() failed! waiting 6 sec ...");
            sleep(6);
            ilCnt++;
        } /* end of if */
    } 
    while((ilCnt < 10) && (ilRC != RC_SUCCESS));

    if(ilRC != RC_SUCCESS)
    {
        dbg(TRACE,"InitPgm: init_db() failed! waiting 60 sec ...");
        sleep(60);
        exit(2);
    }
    else
    {
        dbg(TRACE,"InitPgm: init_db() OK!");
    } /* end of if */
	
	(void) ReadConfigEntry("MAIN","TOWING_TABLE",pcgTowing_Table,"AFTTAB");
	dbg(DEBUG,"InitPgm: TOWING_TABLE IS <%s>",pcgTowing_Table);

    (void) ReadConfigEntry("MAIN","DEBUG_LEVEL",pclDbgLevel,"TRACE");
    if (strcmp(pclDbgLevel,"DEBUG") == 0)
    {
        dbg(DEBUG,"InitPgm: DEBUG_LEVEL IS <DEBUG>");
        igDbgSave = DEBUG;
    }             
    else if (strcmp(pclDbgLevel,"OFF") == 0) 
    {
        dbg(DEBUG,"InitPgm: DEBUG_LEVEL IS <OFF>");
        igDbgSave = 0; 
    } 
    else 
    {
        dbg(DEBUG,"InitPgm: DEBUG_LEVEL IS <TRACE>");
        igDbgSave = TRACE;
    }

    memset(&pcgRemoveNewLine, 0, sizeof(pcgRemoveNewLine));
    ReadConfigEntry("MAIN","REMOVE_NEW_LINE",pcgRemoveNewLine,"");

    ReadConfigEntry("MAIN","FLIGHT_QUEUE",pclTmp,"7800");
    igFlightQueue=atoi(pclTmp);
    
    ReadConfigEntry("MAIN","SQLHDL_QUEUE",pclTmp,"506");
    igSqlhdlQueue=atoi(pclTmp);

    ReadConfigEntry("MAIN","DEP_REGN_KEEP",pclTmp,"NO");
    if (strcmp(pclTmp, "YES") == 0)
       igArrRegnStk = TRUE;
	   
	ReadConfigEntry("BASIC_DATA_CHECK","FIELD_LIST",pclTmp,"NO");
    if (pclTmp!= NULL && strlen(pclTmp)>0 && strcmp(pclTmp, "NO") != 0 )
	{
		strcpy(pcgBasicDataCheck_Field_list,pclTmp);
		igBasicDataCheck_FieldList = TRUE;
	}
	
	ReadConfigEntry("BASIC_DATA_CHECK","TABLE_LIST",pclTmp,"NO");
    if (pclTmp!= NULL && strlen(pclTmp)>0 && strcmp(pclTmp, "NO") != 0)
	{
		strcpy(pcgBasicDataCheck_Table_list,pclTmp);
		igBasicDataCheck_TableList = TRUE;
	}
	
	if( (int) GetNoOfElements(pcgBasicDataCheck_Field_list, ',') == (int) GetNoOfElements(pcgBasicDataCheck_Table_list, ',') )
	{
		igBasicDataCheck = igBasicDataCheck_FieldList & igBasicDataCheck_TableList;
	}
	else
	{
		dbg(TRACE,"InitPgm: Number of field and table is not matched");
		igBasicDataCheck = FALSE;
	}
	
    ReadConfigEntry("MAIN","GROUP_COUNT",pclTmp,"0");
    pcgListCount=atoi(pclTmp);
    rgXMLList=malloc(sizeof(_XMLLIST)*pcgListCount);
    if ( rgXMLList==NULL )
    {
        dbg(TRACE, "InitPgm:  Cannot allocate memory for pgXMLGROUP");
        sleep(10);
        Terminate();    	
    }
    else
    {
        igGrpSetCnt = 0;
    	memset(rgXMLList, 0, sizeof(_XMLLIST)*pcgListCount);
        for (a=0; a<pcgListCount; a++)
        {
            sprintf(pclGroup, "XML_GROUP%d", a+1);
            ReadConfigEntry(pclGroup,"LIST",rgXMLList[a].LIST,"");
            ReadConfigEntry(pclGroup,"NAME",rgXMLList[a].GROUP,"");
            ReadConfigEntry(pclGroup,"TABLE_NAME",rgXMLList[a].TABLE,"");

            ReadConfigEntry(pclGroup,"SELECT_GROUP",pclTmp,"0");   
            ilCnt=atoi(pclTmp);
            if ( ilCnt>0 )
            {
              	rgXMLList[a].rgSelect = malloc(sizeof(_SELECTGROUP)*ilCnt);
               	if ( rgXMLList[a].rgSelect==NULL )
               	{
               	    dbg(TRACE, "InitPgm: ERROR allocatinng rgSelect");
               	    Terminate();	
               	}
               	else
               	{                            	
               		memset(rgXMLList[a].rgSelect, 0, sizeof(_SELECTGROUP)*ilCnt);
               		rgXMLList[a].igSelectGroup=ilCnt;
                    for (b=0; b<ilCnt; b++)
                    {
                      	sprintf(pclSelect, "SELECTION_KEY%d", b+1);
                        ReadConfigEntry(pclGroup,pclSelect,pclTmp,"");  
                        if ( strcmp(pclTmp, "") )
                        {
                            ilRC=parseSelectConfig(pclTmp, rgXMLList[a].GROUP, &rgXMLList[a].rgSelect[b]);	
                        }
                        sprintf(pclSelect, "ADDITIONAL_WHERE%d", b+1);
                        ReadConfig(pclGroup,pclSelect,rgXMLList[a].rgSelect[b].ADDITIONALWHERE,"");   
                    }
                }
            }    
                
            if ( strcmp(rgXMLList[a].TABLE, "") )
            {
                ReadConfigEntry(pclGroup,"SEND_QUEUE",pclTmp,"");
                if ( !strcmp(pclTmp, "") )
                {
                    if ( !strcmp(rgXMLList[a].TABLE, "AFTTAB") )
                        rgXMLList[a].ilSendQueue=igFlightQueue;
                    else 
                        rgXMLList[a].ilSendQueue=igSqlhdlQueue;	
                }
                else
                    rgXMLList[a].ilSendQueue=atoi(pclTmp);
            
                if ( !strcmp(rgXMLList[a].TABLE, "AFTTAB") )
                {
                    ReadConfigEntry(pclGroup,"DELETE_COMMAND",rgXMLList[a].DELCMD,"DFR");     
                    ReadConfigEntry(pclGroup,"INSERT_COMMAND",rgXMLList[a].INSCMD,"IFR");     
                    ReadConfigEntry(pclGroup,"UPDATE_COMMAND",rgXMLList[a].UPDCMD,"UFR");   
                }
                else
                {
                    ReadConfigEntry(pclGroup,"DELETE_COMMAND",rgXMLList[a].DELCMD,"DRT");     
                    ReadConfigEntry(pclGroup,"INSERT_COMMAND",rgXMLList[a].INSCMD,"IRT");     
                    ReadConfigEntry(pclGroup,"UPDATE_COMMAND",rgXMLList[a].UPDCMD,"URT");                 	
                }
                
                ReadConfigEntry(pclGroup,"URNO_GENERATOR",rgXMLList[a].URNOGENERATOR,""); 
                ReadConfigEntry(pclGroup,"MODE", rgXMLList[a].MODE, "INSERT/UPDATE/DELETE"); 
           }

            ReadConfigEntry(pclGroup,"FULLSET", pclTmp, "NO"); 
            str_trm_all (pclTmp, " ", TRUE);
            if (strcmp(pclTmp, "YES") == 0)
                rgXMLList[a].igFullSet = TRUE;
            else
                rgXMLList[a].igFullSet = FALSE;

            if (rgXMLList[a].igFullSet == TRUE)
            {
                ReadConfigEntry(pclGroup,"FULLSET_INCLFLD",rgXMLList[a].FullSet_InclFld,""); 
                str_trm_all (rgXMLList[a].FullSet_InclFld, " ", TRUE);
                ReadConfigEntry(pclGroup,"FULLSET_INCLDAT",rgXMLList[a].FullSet_InclDat,""); 
                str_trm_all (rgXMLList[a].FullSet_InclDat, " ", TRUE);
                ilItmCnt1 = field_count(rgXMLList[a].FullSet_InclFld);
                ilItmCnt2 = field_count(rgXMLList[a].FullSet_InclDat);
                if ((strlen(rgXMLList[a].FullSet_InclFld) < 2) || 
                   (strlen(rgXMLList[a].FullSet_InclDat) < 2) ||
                    (ilItmCnt1 != ilItmCnt2) )
                {
                    rgXMLList[a].igFullSet = FALSE;
                }

                ReadConfigEntry(pclGroup,"FULLSET_EXCLFLD",rgXMLList[a].FullSet_ExclFld,""); 
                str_trm_all (rgXMLList[a].FullSet_ExclFld, " ", TRUE);
                ReadConfigEntry(pclGroup,"FULLSET_EXCLDAT",rgXMLList[a].FullSet_ExclDat,""); 
                str_trm_all (rgXMLList[a].FullSet_ExclDat, " ", TRUE);
                ilItmCnt1 = field_count(rgXMLList[a].FullSet_ExclFld);
                ilItmCnt2 = field_count(rgXMLList[a].FullSet_ExclDat);
                if ((strlen(rgXMLList[a].FullSet_ExclFld) < 2) || 
                   (strlen(rgXMLList[a].FullSet_ExclDat) < 2) ||
                    (ilItmCnt1 != ilItmCnt2) )
                {
                    rgXMLList[a].igFullSet = FALSE;
                }
                if (rgXMLList[a].igFullSet == FALSE)
                    dbg(DEBUG, "Group<%s> FULLSET as Disable as Wrong FULLSET_INCL/FULLSET_EXCL FLD/DAT Config", pclGroup);
            }

            strcpy(prgGrpSet[a].GrpNam, rgXMLList[a].GROUP);
            ReadConfigEntry(pclGroup, "PATCH_ORG_LST", prgGrpSet[a].Patch_OrgLst,""); 
            str_trm_all (prgGrpSet[a].Patch_OrgLst, " ", TRUE);
            prgGrpSet[a].Patch_LstCnt = 0;
            ReadConfigEntry(pclGroup, "PATCH_NEW_LST", prgGrpSet[a].Patch_NewLst,"");
            str_trm_all (prgGrpSet[a].Patch_NewLst, " ", TRUE);
            if ((strlen(prgGrpSet[a].Patch_OrgLst) > 0) || (strlen(prgGrpSet[a].Patch_NewLst) > 0))
            {
                ReadConfigEntry(pclGroup, "PATCH_DAT_LST", prgGrpSet[a].Patch_DatLst,"");
                str_trm_all (prgGrpSet[a].Patch_DatLst, " ", TRUE);
                ilItmCnt1 = field_count(prgGrpSet[a].Patch_OrgLst);
                ilItmCnt2 = field_count(prgGrpSet[a].Patch_NewLst);
                ilItmCnt3 = field_count(prgGrpSet[a].Patch_DatLst);
                
                if (((ilItmCnt1 == ilItmCnt2) && (strlen(prgGrpSet[a].Patch_DatLst) == 0)) ||
                    ((ilItmCnt1 == ilItmCnt3) && (strlen(prgGrpSet[a].Patch_NewLst) == 0)) ||
                    ((ilItmCnt1 == ilItmCnt3) && (ilItmCnt2 == ilItmCnt3)) )
                    prgGrpSet[a].Patch_LstCnt = ilItmCnt1;
                else if ((ilItmCnt2 == ilItmCnt3) && (strlen(prgGrpSet[a].Patch_OrgLst) == 0))
                    prgGrpSet[a].Patch_LstCnt = ilItmCnt2;
                dbg(DEBUG, "Group<%s> Patch List Cnt<%d>", pclGroup, prgGrpSet[a].Patch_LstCnt);
            }
            ReadConfigEntry(pclGroup, "REF_TAB", prgGrpSet[a].RefTab,""); 
            str_trm_all (prgGrpSet[a].RefTab, " ", TRUE);
            prgGrpSet[a].RefValid = FALSE;
            if (strlen(prgGrpSet[a].RefTab) > 0)
            {
                ReadConfigEntry(pclGroup, "REF_KEY_LST", prgGrpSet[a].RefKeyLst,""); 
                ReadConfigEntry(pclGroup, "REF_KEY_DAT", prgGrpSet[a].RefKeyDat,""); 
                ReadConfigEntry(pclGroup, "REF_GET_LST", prgGrpSet[a].RefGetLst,""); 
                ReadConfigEntry(pclGroup, "REF_SET_LST", prgGrpSet[a].RefSetLst,""); 
                ReadConfigEntry(pclGroup, "REF_VAR_LST", prgGrpSet[a].RefVarLst,""); 
                ReadConfigEntry(pclGroup, "REF_VAR_DAT", prgGrpSet[a].RefVarDat,""); 
                str_trm_all (prgGrpSet[a].RefKeyLst, " ", TRUE);
                str_trm_all (prgGrpSet[a].RefKeyDat, " ", TRUE);
                str_trm_all (prgGrpSet[a].RefGetLst, " ", TRUE);
                str_trm_all (prgGrpSet[a].RefSetLst, " ", TRUE);
                str_trm_all (prgGrpSet[a].RefVarLst, " ", TRUE);
                str_trm_all (prgGrpSet[a].RefVarDat, " ", TRUE);

                if ((strlen(prgGrpSet[a].RefTab) > 0) &&
                   (strlen(prgGrpSet[a].RefKeyLst) > 0) &&  
                   (strlen(prgGrpSet[a].RefKeyDat) > 0) &&  
                   (strlen(prgGrpSet[a].RefGetLst) > 0) &&  
                   (strlen(prgGrpSet[a].RefSetLst) > 0))
                {
                    ilItmCnt1 = field_count(prgGrpSet[a].RefKeyLst);
                    ilItmCnt2 = field_count(prgGrpSet[a].RefKeyDat);
                    if ((ilItmCnt1 == ilItmCnt2) && (ilItmCnt1 > 0))
                    {
                        ilItmCnt1 = field_count(prgGrpSet[a].RefKeyLst);
                        ilItmCnt2 = field_count(prgGrpSet[a].RefKeyDat);
                        if ((ilItmCnt1 == ilItmCnt2) && (ilItmCnt1 > 0))
                        {
                            prgGrpSet[a].RefValid = TRUE;
                        }     
                   
                    }
                }
            }
            igGrpSetCnt++;
        }
    }
    
    rgpUrnoBuffer=NULL;
    igMaxUrnoBufferSize=0;
    igInsertUrno=0;
    if ( ilRC==RC_SUCCESS )
    {
        ReadConfigEntry("MAIN","MAX_URNO_BUFFER",pclTmp,"0");
        igMaxUrnoBufferSize=atoi(pclTmp);    	
        
        if ( igMaxUrnoBufferSize>0 )
        {
            rgpUrnoBuffer=malloc(sizeof(_URNOBUFFER)*igMaxUrnoBufferSize);
            if ( rgpUrnoBuffer==NULL )
            {
                ilRC=RC_FAIL;
                dbg(TRACE, "InitPgm:: Cannot allocate memory for rgpUrnoBuffer");	
            }     	
            else
            {
                memset(rgpUrnoBuffer, 0, sizeof(_URNOBUFFER)*igMaxUrnoBufferSize);	
            }
        } 
    }
   
    if(ilRC == RC_SUCCESS)
    {
        /* read HomeAirPort from SGS.TAB */
        ilRC = tool_search_exco_data("SYS", "HOMEAP", cgHopo);
        if (ilRC != RC_SUCCESS)
        {
            dbg(TRACE,"InitPgm: EXTAB,SYS,HOMEAP not found in SGS.TAB");
            Terminate();
        }
        else
        {
            dbg(TRACE,"InitPgm: home airport <%s>", cgHopo);
        }
    }

    memset(&rgFlight, 0, sizeof(_AFTTAB)*MAX_FLIGHT);
    igFlightIndexStart=igFlightIndexCounter=igFlightIndexEnd=0;

    printConfig();
    
    return(ilRC);
} /* end of initialize */


/*******************************************************************************
* ReadConfigEntry
*******************************************************************************/
static int ReadConfigEntry(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer, char *pcpDefault)
{
    int ilRC = RC_SUCCESS;
    char pclSection[256] = "";
    char pclKeyword[256] = "";

    strcpy(pclSection,pcpSection);
    strcpy(pclKeyword,pcpKeyword);

    ilRC = iGetConfigEntry(pcgConfigFile, pclSection, pclKeyword, CFG_STRING,pcpCfgBuffer);
    if(ilRC != RC_SUCCESS)
    {
        dbg(TRACE,"ReadConfigEntry: Not found in %s: [%s] <%s>",pcgConfigFile, pclSection, pclKeyword);
        dbg(TRACE,"ReadConfigEntry: use default-value: <%s>",pcpDefault);
        strcpy(pcpCfgBuffer,pcpDefault);
    } 
    else
    {
        dbg(TRACE,"ReadConfigEntry: Config Entry [%s],<%s>:<%s> found in %s", pclSection, pclKeyword ,pcpCfgBuffer, pcgConfigFile);
    }
    
    return ilRC;
}

static int ReadConfig(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer, char *pcpDefault)
{
    int ilRC=RC_SUCCESS;
    int ilFlag=0;
    FILE *fptr;
    char pclSectionStart[MAX_VALUE+1];
    char pclSectionEnd[MAX_VALUE+1];
    char pclLine[MAX_XML+1];
    char *pclPtr;
    
    dbg(DEBUG, "ReadConfig:: Start");
    dbg(DEBUG, "ReadConfig:: pcpSelection:[%s], pcpKeyword[%s]", pcpSection, pcpKeyword);

    strcpy(pcpCfgBuffer, pcpDefault);
    sprintf(pclSectionStart, "[%s]", pcpSection);
    sprintf(pclSectionEnd, "[%s_END]", pcpSection);
    dbg(DEBUG, "ReadConfig:: pclSectionStart[%s], pclSectionEnd[%s]", pclSectionStart, pclSectionEnd);
        
    fptr=fopen(pcgConfigFile, "r");
    while ( fgets(pclLine, MAX_XML, fptr) )
    {
    	pclLine[strlen(pclLine)-1]=0;
    	if ( pclLine[0]=='#' )
    	    continue;
    	    
    	if ( !strcmp(pclLine, pclSectionStart) )
    	{
    	    dbg(DEBUG, "ReadConfig:: pclSectionStart found");	
    	    ilFlag=1;
    	    break;
    	}    
    }
    
    dbg(DEBUG, "ReadConfig:: Section Start[%d]", ilFlag);
    if ( !ilFlag )
        dbg(DEBUG, "ReadConfig:: pclSectionStart not found");
    else
    {
    	ilFlag=0;
        while ( fgets(pclLine, MAX_XML, fptr) )
        {
    	    pclLine[strlen(pclLine)-1]=0;
    	    if ( pclLine[0]=='#' )
    	        continue;
    	    else if ( !strcmp(pclLine, pclSectionEnd) )
    	    {
    	    	dbg(DEBUG, "ReadConfig::  pclSectionEnd found");
    	        break;	
    	    }
    	    else if ( !strncmp(pcpKeyword, pclLine, strlen(pcpKeyword)) )
    	    {
    	        dbg(DEBUG, "ReadConfig:: Found Keyword.");
    	        ilFlag=1;
    	        break;	
    	    }    	    	
    	}
    	
    }
    
    if ( !ilFlag )
    {
        dbg(DEBUG, "ReadConfig::  Keyword not found");    	
    }
    else
    {
        dbg(DEBUG, "ReadConfig:: Line[%s]", pclLine);	
        pclPtr=&pclLine[strlen(pcpKeyword)];
        while ( pclPtr && (pclPtr[0]==' ' || pclPtr[0]=='=') )
            pclPtr++;
        if ( pclPtr )
            strcpy(pcpCfgBuffer, pclPtr);    
    }
    fclose(fptr);
    
    
    dbg(DEBUG, "ReadConfig:: End");
    return ilRC;	
}

/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int pipErr)
{
    int    ilRC = RC_SUCCESS;
    return;
} /* end of HandleErr */


/******************************************************************************
* HandleData
*******************************************************************************/
static int HandleData()
{
    int ilRC = RC_SUCCESS; 
    int que_out;  /* Sender que id */
    BC_HEAD *prlBchd = NULL;  /* Broadcast heade */
    CMDBLK  *prlCmdblk = NULL;    /* Command Block */
    char *pclSel=NULL;
    char *pclFld = NULL;
    char *pclData = NULL;
    char pclFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    char pclTable [32];
    char pclTmp[128];
 
    igQueOut = prgEvent->originator;   /* Queue-id des Absenders */
    prlBchd = (BC_HEAD *) ((char *)prgEvent + sizeof(EVENT));
    prlCmdblk = (CMDBLK  *) ((char *)prlBchd->data);

    pclSel = prlCmdblk->data;
    pclFld = pclSel + strlen(pclSel)+1;
    pclData = pclFld + strlen(pclFld)+1;
    
    strcpy (pclFields, pclFld);
    strcpy (pclValues, pclData);

    dbg(TRACE,"HandleData:: FROM <%d> TBL <%s>",igQueOut,prlCmdblk->obj_name);
  
    strcpy (pclTable,prlCmdblk->obj_name);
    dbg(TRACE,"HandleData:: Cmd <%s> Que (%d) WKS <%s> Usr <%s>",
        prlCmdblk->command, prgEvent->originator, prlBchd->recv_name, prlBchd->dest_name);
    dbg(TRACE,"HandleData:: Prio (%d) TWS <%s> TWE <%s>", prgItem->priority, prlCmdblk->tw_start, prlCmdblk->tw_end);
 
    strcpy (pcgTwStart,prlCmdblk->tw_start);
    strcpy (pcgTwEnd,prlCmdblk->tw_end);
    memset(pcgAftDestName, 0, (sizeof(prlBchd->dest_name) + 1));
    
    strncpy (pcgAftDestName, prlBchd->dest_name, sizeof(prlBchd->dest_name));
    memset(pcgAftRecvName, 0, (sizeof(prlBchd->recv_name) + 1));
    strncpy(pcgAftRecvName, prlBchd->recv_name, sizeof(prlBchd->recv_name));
    dbg(TRACE,"HandleData:: Originator <%d> ", igQueOut);
    dbg(TRACE,"HandleData:: Sel <%s> ", pclSel);
    dbg(TRACE,"HandleData:: Fld <%s> ", pclFld);
    dbg(TRACE,"HandleData:: Dat <%s> ", pclData);

    if (prlBchd->rc == NETOUT_NO_ACK)
    {
        igGlNoAck = TRUE;
        //dbg(TRACE,"HandleData:: No Answer Expected (NO_ACK is true)"); 
    }
    else
    {
        igGlNoAck = FALSE;
        //dbg(TRACE,"HandleData:: Sender Expects Answer (NO_ACK is false)");
    }
 
    if (prlBchd->rc != RC_SUCCESS && prlBchd->rc != NETOUT_NO_ACK)
    {
        dbg(TRACE,"HandleData:: Originator %d", igQueOut);
        dbg(TRACE,"HandleData:: Error prlBchd->rc <%d>", prlBchd->rc);
        dbg(TRACE,"HandleData:: Error Description: <%s>", prlBchd->data);
    }
    else
    {
		if(igBasicDataCheck == TRUE)
		{
			dbg(TRACE,"Basic data check is enabled");
			if(basicDataCheck(pclData, pcgBasicDataCheck_Field_list, pcgBasicDataCheck_Table_list) != RC_SUCCESS)
			{
				dbg(TRACE,"Basic data check fails");
				return RC_FAIL;
			}
			else
			{
				dbg(TRACE,"Basic data check is passed");
			}
		}
		else
		{
			dbg(TRACE,"Basic data check is disabled");
		}
	
        strcpy (pcgCedaCmd,prlCmdblk->command);
        if ( !strcmp(prlCmdblk->command, "XML") )
        {
             dbg(TRACE, "----------     HandleData:: XML Start     ----------");   
             iWonCnt++;
             sprintf(pclWon, "Record[%d][%s]", iWonCnt, pclData);	
             
             ilRC=processXML(pclData);
             dbg(TRACE, "----------     HandleData:: XML END Result[%d]     ----------", ilRC);   	
            /*   Disable the summary log
             sprintf(pclWon, "%s, Result[%d]\n", pclWon, ilRC);
             sprintf(pclTmp, "/ceda/bin/%s.summary.log", mod_name);
             if ( iWonCnt>1 )
                 fptr=fopen(pclTmp, "a");
             else
                 fptr=fopen(pclTmp, "w");
             if ( fptr )
             {
                 fprintf(fptr, pclWon);
                 fclose(fptr);	
             }             	
            */
        }
        else
            dbg(TRACE, "HandleData:: Invalid CMD[%s]", prlCmdblk->command);
            
    }  
    return ilRC;    
} /* end of HandleData */


int processXML(char *pcpXML)
{
    int ilRC=RC_SUCCESS;
    int ilTmp=0;
    int a=0;
    char pclTmpXML[MAX_XML+1];
    
    dbg(TRACE, "processXML:: Start");    

    // memset(rgXMLRecords, 0, sizeof(rgXMLRecords));    
    pcgRecordCount=0;            
    for (a=0; a<pcgListCount; a++)
    {
        dbg(DEBUG, "processXML:: List [%d] Name[%s] Group[%s] Table[%s]", 
            a+1, rgXMLList[a].LIST, rgXMLList[a].GROUP, rgXMLList[a].TABLE);
        
        if ( strcmp(rgXMLList[a].LIST,"") )
        {
            dbg(DEBUG, "processXML:: Extracting Record Lists [%s]", rgXMLList[a].LIST);    
            memset(pclTmpXML, 0, sizeof(pclTmpXML));
            ilTmp=parseGroupXML(pcpXML, rgXMLList[a].LIST, pclTmpXML);	
            dbg(DEBUG, "processXML::Lists XML[%s]", pclTmpXML);
            
            if ( ilTmp==RC_SUCCESS )
            {                

                ilRC=parseMultiXML(pclTmpXML, &rgXMLList[a], rgXMLList[a].GROUP, rgXMLList[a].TABLE, &pcgRecordCount);    	
                if ( ilRC==RC_FAIL )
                {
                	dbg(TRACE, "processXML:: ERROR in parseMultiXML");
                    break;	
                }
                else if (rgXMLList[a].igFullSet == TRUE)
                {
                    if (pcgRecordCount < MAX_RECORD -1)
                    {
                	    dbg(TRACE, "processXML:: Add Record for FullSet Check");
            	        rgXMLRecords[pcgRecordCount].rgXMLList = &rgXMLList[a];
                        rgXMLRecords[pcgRecordCount].iSize = 0;
                        strcpy(rgXMLRecords[pcgRecordCount].WHERE,"") ;
            	        pcgRecordCount++;	
                    }
                    else
                    {
                        pcgRecordCount = MAX_RECORD - 1;
                	    dbg(TRACE, "processXML:: ERROR! Exceed MAX Records ");
                        ilRC = RC_FAIL;
                        break;
                    }
                }
            }
        }
        else
        {
            dbg(DEBUG, "processXML:: Extracting Record [%s]", rgXMLList[a].GROUP);        	
        	if ( !strcmp(rgXMLList[a].GROUP, "") )
        	{
        	     dbg(DEBUG, "processXML:: Nothing to parse for record");		
        	}
        	else
            {               
                memset(pclTmpXML, 0, sizeof(pclTmpXML));
                ilTmp=parseGroupXML(pcpXML, rgXMLList[a].GROUP, pclTmpXML);	
//                dbg(DEBUG, "processXML::Group XML[%s]", pclTmpXML);
                
                if ( ilTmp==RC_SUCCESS )
                {
                    ilRC=parseXMLMap(pclTmpXML, &rgXMLRecords[pcgRecordCount]);
                    if ( ilRC==RC_FAIL )
                    {
            	        dbg(TRACE, "processXML:: ERROR in parseXMLMap!!!");
                        break;	
                    }
                    else
                    {
                        if (pcgRecordCount < MAX_RECORD -1)
                        {
                    	    rgXMLRecords[pcgRecordCount].rgXMLList=&rgXMLList[a];
                            pcgRecordCount++;	
                        }
                        else
                        {
                	        dbg(TRACE, "processXML:: ERROR! Exceed MAX Records ");
                            pcgRecordCount = MAX_RECORD - 1;
                            ilRC = RC_FAIL;
                            break;
                        }
                    }                	
                }
            }
        }
    }
    
    if ( ilRC==RC_SUCCESS )
    {
        printRecords();
        ilRC=processXMLRecords();
    }   
    dbg(TRACE, "processXML:: End [%d]", ilRC);    
    return ilRC;	
}

int processXMLRecords()
{
    int ilRC=RC_SUCCESS;
    int ilTmp=0;
    int a=0, b=0;
    int ilIndex=0;
    int ilFlightIndex=0;
    int ilCommaFlag=0;
    long llUrno=0;
    char pclSql[MAX_XML+1];
    char pclWhere[MAX_XML+1];
    char pclFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    char pclTmpFields[MAX_XML+1];
    char pclTmpValues[MAX_XML+1];
    char pclCmd[10];
    int ilSendCeda=1;
    int ilInsert=0;
    char pclTmpBuf[MAX_XML+1];
    char pclTmpDat[MAX_XML+1];
    char pclExclLstDat[MAX_XML+1];
    int  ilActFullSet = FALSE;
    int ilDelete = FALSE;
    
    dbg(TRACE,"processXMLRecords:: Start");
    
    ilIndex=getXMLRecordIndexByGroup("INFOBJ_GENERIC", 0);
    dbg(DEBUG, "processXMLRecords:: Getting [INFOBJ_GENERIC] index[%d]", ilIndex);
    if ( ilIndex==RC_FAIL )
    {
        dbg(TRACE, "processXMLRecords:: <INFOBJ_GENERIC> missing");
        ilRC=RC_FAIL;
    }
    else
    {
    	ilRC=populateInfoGeneric(&rgInfoGeneric, &rgXMLRecords[0]);
    	printInfoGeneric(&rgInfoGeneric);

        memset(&rgAfttab, 0, sizeof(_AFTTAB));
       	ilFlightIndex=getXMLRecordIndexByGroup("INFOBJ_FLIGHT", 0);
        dbg(DEBUG, "processXMLRecords:: Getting [INFOBJ_FLIGHT] index[%d]", ilFlightIndex);
        
        if ( ilFlightIndex==RC_FAIL )
        {
            dbg(DEBUG, "processXMLRecords:: No <INFOBJ_FLIGHT>... ");	
            ilRC=processAfttabRecord(&rgAfttab, &rgInfoGeneric,  &rgXMLRecords[ilIndex]);	
        }
        else
        {
    	    ilRC=processAfttabRecord(&rgAfttab, &rgInfoGeneric,  &rgXMLRecords[ilFlightIndex]);
    	}

        dbg(TRACE, "processXMLRecords:: rgAfttab.URNO[%s], ilRC[%d]", rgAfttab.URNO, ilRC);
        if ( ilRC==RC_SUCCESS && strcmp(rgAfttab.URNO, "") )
        {    
        	dbg(DEBUG, "processXMLRecords:: URNO [%s]", rgAfttab.URNO);
            for (a=0; a<pcgRecordCount; a++)
            {
                ilSendCeda=TRUE;
                ilInsert=FALSE;
                ilDelete=FALSE;
                dbg(DEBUG, "processXMLRecords:: Record [%d/%d], Group[%s], Table[%s]", 
                    a+1, pcgRecordCount, rgXMLRecords[a].rgXMLList->GROUP, rgXMLRecords[a].rgXMLList->TABLE);   
                
                if ( !strcmp(rgXMLRecords[a].rgXMLList->GROUP, "INFOBJ_GENERIC") )
                {
                    dbg(DEBUG, "processXMLRecords:: Skipping <INFOBJ_GENERIC>");	
                }	
                else if ( !strcmp(rgXMLRecords[a].rgXMLList->GROUP, "INFOBJ_FLIGHT") )
                {
                    dbg(DEBUG, "processXMLRecords:: Skipping <INFOBJ_FLIGHT> because it was processed first.");	
                }                
                else if ( !strcmp(rgXMLRecords[a].rgXMLList->GROUP, "INFOBJ_TOWING") )
                {
                    dbg(DEBUG, "processXMLRecords:: Towing Record.");	
                    ilRC=constructWhereClause(a, pclWhere, pclTmpFields, pclTmpValues, rgXMLRecords[a].rgXMLList->rgSelect, rgXMLRecords[a].rgXMLList->igSelectGroup); 
                    
                    if ( ilRC==RC_SUCCESS && strncmp(pcgTowing_Table,"AFTTAB",6) == 0 )
                        ilRC=processTowingRecord(&rgAfttab, &rgXMLRecords[a], pclWhere);
                    //ilRC=processTowingRecordOld(&rgAfttab, &rgXMLRecords[a]);
                }
/*              Use Universel format to handle INFOBJ_TOWING_DELAY  
                else if ( !strcmp(rgXMLRecords[a].rgXMLList->GROUP, "INFOBJ_TOWING_DELAY") )
                {
                    dbg(DEBUG, "processXMLRecords:: Towing Delay.");	
                    ilRC=processTowingDelayRecord(&rgAfttab, &rgXMLRecords[a]);                	
                }
*/
                else if ( !strcmp(rgXMLRecords[a].rgXMLList->GROUP, "INFOBJ_JOINFLT") )
                {
                    dbg(DEBUG, "processXMLRecords:: Join Flight");	
                    ilRC=constructWhereClause(a, pclWhere, pclTmpFields, pclTmpValues, rgXMLRecords[a].rgXMLList->rgSelect, rgXMLRecords[a].rgXMLList->igSelectGroup); 
                    
                    if ( ilRC==RC_SUCCESS )
                        ilRC=processJoinFlight(&rgAfttab, &rgXMLRecords[a], pclWhere);
                }
                else
                {
                	ilCommaFlag=0;
                    memset(pclFields, 0, sizeof(pclFields));
                	/*
                    memset(pclValues, 0, sizeof(pclValues));
                    memset(pclWhere, 0, sizeof(pclWhere));
                    */
                    

                    ilActFullSet = FALSE;
                    if ( rgXMLRecords[a].rgXMLList->igFullSet == TRUE)
                    {
                        if ((a >= 1) && (a <= pcgRecordCount -1) )
                        {
                            if(strcmp(rgXMLRecords[a].rgXMLList->LIST, rgXMLRecords[a-1].rgXMLList->LIST) != 0)
                                strcpy(pclExclLstDat, "");
                        }

                        if (a == 0)
                           strcpy(pclExclLstDat, "");

                        if (a < pcgRecordCount -1)
                        {
                            if (strcmp(rgXMLRecords[a].rgXMLList->LIST, rgXMLRecords[a+1].rgXMLList->LIST) != 0)
                                ilActFullSet = TRUE;
                        }
                        if (a == (pcgRecordCount - 1 ))
                            ilActFullSet = TRUE;

                       if ( rgXMLRecords[a].iSize > 0)
                       {
                           ilRC = GetXmlGrpDat(&rgAfttab, &rgXMLRecords[a], rgXMLRecords[a].rgXMLList->FullSet_ExclDat, pclTmpDat);
                           if ( ilRC == RC_SUCCESS )
                           {
                                if (strlen(pclExclLstDat) > 0)
                                    sprintf(pclTmpBuf, ",'%s'", pclTmpDat);
                                else
                                    sprintf(pclTmpBuf, "'%s'", pclTmpDat);
                                strcat(pclExclLstDat, pclTmpBuf);
                           }
                       }
 
                       if (ilActFullSet == TRUE)
                       {
                           dbg(DEBUG, "processXMLRecords:: EXCLUDE DATE LIST<%s>", pclExclLstDat);	
                           ilRC = FullSetCheckWhere(&rgAfttab, &rgXMLRecords[a], pclExclLstDat, pclWhere);
                           ilTmp = ilRC;
                       }
                    }
                    else
                        strcpy(pclExclLstDat, "");

                    if (ilActFullSet == FALSE)
                    {
                        ilTmp=constructWhereClause(a, pclWhere, pclTmpFields, pclTmpValues, rgXMLRecords[a].rgXMLList->rgSelect, rgXMLRecords[a].rgXMLList->igSelectGroup); 
                    }

                    if ( ilTmp==RC_SUCCESS )
                    {
                        sprintf(pclSql, "SELECT (1) FROM %s %s", rgXMLRecords[a].rgXMLList->TABLE, pclWhere);
                        ilTmp=checkRecordInDB(pclSql);
                    }
                     
                    if ( ilTmp==RC_FAIL )
                    {
                    	dbg(TRACE, "processXMLRecords:: ERROR!!! executing checkRecordInDB()");
                    	ilRC=RC_FAIL;
                        break;	
                    }
                    else
                    {
                      if (ilActFullSet == TRUE)
                      {
                          strcpy(pclCmd, rgXMLRecords[a].rgXMLList->DELCMD);  
                          strcpy(pclFields, "");
                          strcpy(pclValues, "");
                          if (ilTmp == SQL_NOTFOUND)
                          {
                              ilSendCeda = FALSE;	
                    	      dbg(DEBUG, "processXMLRecords:: Nothing to Clear for Full Set");
                          }
                          else
                          {
                              ilSendCeda = TRUE;	
            		          ilInsert = FALSE;
            		          ilDelete = TRUE;
                          }
                      }
                      else
                      {
                         switch (rgInfoGeneric.ACTIONTYPE[0])
                         {
                             case 'D':    
                             {
            		             ilDelete = TRUE;
                                 strcpy(pclCmd, rgXMLRecords[a].rgXMLList->DELCMD);  
                                 if ( ilTmp==SQL_NOTFOUND )    
                                 {
                                     ilSendCeda=0;	
                                 }
                                 break;
                             }	
                             
                             case 'I':
                             case 'U':
                             case 0:
                             {
            	                 if ( ilTmp==SQL_NOTFOUND )    
            	                 {
            		                 ilInsert=1;
                                     strcpy(pclCmd, rgXMLRecords[a].rgXMLList->INSCMD);  
                                     /* strcpy(pclWhere, ""); */
                        
                                     if ( !strcmp(pclFields, "") )
                                     {
                                         if ( !strcmp(rgXMLRecords[a].rgXMLList->URNOGENERATOR, "YES") )
                                         {
                                         	 dbg(TRACE, "processXMLRecords:: Generating new urno");
                                         	 strcpy(pclFields, "URNO");
                                             sprintf(pclValues, "%ld", NewUrnos(rgXMLRecords[a].rgXMLList->TABLE,1)); 	
                                             dbg(TRACE, "processXMLRecords:: New Urno[%s]", pclValues);
                                             ilCommaFlag=1;
                                         }
                                         else if ( !strcmp(rgXMLRecords[a].rgXMLList->URNOGENERATOR, "AFTTAB") )
                                         {
                                         	 strcpy(pclFields, "URNO");
                                             strcpy(pclValues,rgAfttab.URNO); 	
                                             ilCommaFlag=1;      	
                                         }
                                     }                           
                                 }                      
                                 else
                                 {
                                     strcpy(pclCmd, rgXMLRecords[a].rgXMLList->UPDCMD);	
                                 }

                                 for (b=0; b<rgXMLRecords[a].iSize; b++)
                                 {                    	
                    	             if ( ilCommaFlag )
                                     {
                                         sprintf(pclFields, "%s,%s", pclFields, rgXMLRecords[a].MAP[b].key);	
                                         sprintf(pclValues, "%s,%s", pclValues, rgXMLRecords[a].MAP[b].value);	           	
                                     }    
                                     else
                                     {
                        	             ilCommaFlag=1;
                                         sprintf(pclFields, "%s", rgXMLRecords[a].MAP[b].key);	
                                         sprintf(pclValues, "%s", rgXMLRecords[a].MAP[b].value);	
                                     }
                                 }                        	
                                 break;
                             }
                             default:
                             {
                                 dbg(TRACE, "processAfttabRecord:: Invalid <ACTIONTYPE>");
                                 ilRC=RC_FAIL;
                                 break;
                             }
                         }	
                      }
                    }

                    dbg(TRACE, "processXMLRecords:: pclCmd[%s], ilSendCeda[%d], ilInsert[%d], ilRC[%d]", pclCmd, ilSendCeda, ilInsert, ilRC);
                    
                    if ( ilRC==RC_SUCCESS )
                    {
                    	if ( ilSendCeda )
                    	{
                            if (ilDelete == FALSE)
                            {
                                ilRC = PatchFldLst(&rgAfttab, &rgXMLRecords[a], pclFields, pclValues, rgXMLRecords[a].rgXMLList->GROUP);
                                ilRC = ReferTable(&rgAfttab, &rgXMLRecords[a], pclFields,pclValues);
                            }
                    		if ( ilInsert )
        	                {
        	                    ilRC=constructAdditionalKeyValuePair(pclFields, pclValues, rgXMLRecords[a].rgXMLList->rgSelect, rgXMLRecords[a].rgXMLList->igSelectGroup);   	
        	                }
        	
                            ilRC=sendCedaEventWithLog(rgXMLRecords[a].rgXMLList->ilSendQueue,
                                                      0,             
                                                      mod_name,
                                                      mod_name,
                                                      pcgTwStart,
                                                      pcgTwEnd,     
                                                      pclCmd,
                                                      rgXMLRecords[a].rgXMLList->TABLE,
                                                      pclWhere,
                                                      pclFields,
                                                      pclValues,
                                                      "",
                                                      3,
                                                      NETOUT_NO_ACK);
                        }
                    }   
                }
            }
        }
    }
    dbg(TRACE, "processXMLRecords:: End Result[%d]", ilRC);
    return ilRC;	
}

int getAfttabRecord(_AFTTAB *rpAft, char *pcpWhere)
{
	int ilRC=RC_SUCCESS;
	char pclSql[1024];
	char pclData[MAX_XML+1];
	short slFkt = 0;
	short slCursor = 0;
	
    dbg(TRACE, "getAfttabRecord:: Start");	
    
    sprintf(pclSql, "SELECT %s FROM afttab %s", pcgAfttabFields, pcpWhere);
    dbg(TRACE, "getAfttabRecord:: executing[%s]", pclSql);
       
    slFkt = START;
    slCursor = 0;
    ilRC = sql_if (slFkt, &slCursor, pclSql, pclData);

    if (ilRC == RC_SUCCESS)
    {
    	dbg(TRACE,"getAfttabRecord:: Record found");
        BuildItemBuffer (pclData, pcgAfttabFields, igAfttabFieldCount, ",");
        get_real_item (rpAft->URNO, pclData, 1);
        get_real_item (rpAft->FLNO, pclData, 2);
        get_real_item (rpAft->ADID, pclData, 3);
        get_real_item (rpAft->RKEY, pclData, 4);
        get_real_item (rpAft->STOA, pclData, 5);
        get_real_item (rpAft->STOD, pclData, 6);
        get_real_item (rpAft->ACT3, pclData, 7);
        get_real_item (rpAft->ACT5, pclData, 8);
        get_real_item (rpAft->ALC2, pclData, 9);
        get_real_item (rpAft->ALC3, pclData, 10);
        get_real_item (rpAft->DES3, pclData, 11);
        get_real_item (rpAft->DES4, pclData, 12);
        get_real_item (rpAft->ORG3, pclData, 13);
        get_real_item (rpAft->ORG4, pclData, 14);
        get_real_item (rpAft->RTYP, pclData, 15);
        get_real_item (rpAft->FTYP, pclData, 16);
        get_real_item (rpAft->REGN, pclData, 17);
        
        slFkt = NEXT;
        if ( sql_if(slFkt, &slCursor, pclSql, pclData)==RC_SUCCESS )
        {
        	dbg(TRACE, "getAfttabRecord::  ERROR! SELECT return more than 1 record");
        	memset(rpAft, 0, sizeof(_AFTTAB));
        	ilRC=RC_FAIL;	
        }
    }
    else if (ilRC == SQL_NOTFOUND)
    {
        dbg(TRACE,"getAfttabRecord:: No record found");
        memset(rpAft, 0, sizeof(_AFTTAB));
    }
    else
    {
        dbg(TRACE,"getAfttabRecord:: DB ERROR!!! ");
        memset(rpAft, 0, sizeof(_AFTTAB));
    }

    //commit_work();
    close_my_cursor (&slCursor);

    dbg(TRACE, "getAfttabRecord:: End [%d]", ilRC);	
    return ilRC;
}

int getDentabRecord(_DENTAB *rpDen, char *pcpDURN)
{
	int ilRC=RC_SUCCESS;
	char pclSql[1024];
	char pclData[MAX_XML+1];
	short slFkt = 0;
	short slCursor = 0;
	
    dbg(TRACE, "getDentabRecord:: Start");   
    
    memset(rpDen, 0, sizeof(_DENTAB));
    
    sprintf(pclSql, "SELECT %s FROM dentab WHERE (DECA='%s' or DECN='%s') ", pcgDentabFields, pcpDURN, pcpDURN);
    dbg(TRACE, "getDentabRecord:: executing[%s]", pclSql);
       
    slFkt = START;
    slCursor = 0;
    ilRC = sql_if (slFkt, &slCursor, pclSql, pclData);

    if (ilRC == RC_SUCCESS)
    {
    	dbg(TRACE,"getDentabRecord:: Record found");
        BuildItemBuffer (pclData, pcgDentabFields, igDentabFieldCount, ",");
        get_real_item (rpDen->DECA, pclData, 1);
        get_real_item (rpDen->DECN, pclData, 2);
        get_real_item (rpDen->URNO, pclData, 3);

    }
    else if (ilRC == SQL_NOTFOUND)
    {
        dbg(TRACE,"getDentabRecord:: No record found");
    }
    else
    {
        dbg(TRACE,"getDentabRecord:: DB ERROR!!! ");
    }

    //commit_work();
    close_my_cursor (&slCursor);

    dbg(TRACE, "getAfttabRecord:: End [%d]", ilRC);	
    return ilRC;
}

int populateInfoGeneric(_INFOGENERIC *pInfoGeneric, _XMLRECORD *pXMLRecord)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    
    dbg(DEBUG, "populateInfoGeneric:: Start");
    
    memset(pInfoGeneric, 0, sizeof(_INFOGENERIC));
    
    for (a=0; a<pXMLRecord->iSize; a++)
    {
    	if ( !strcmp(pXMLRecord->MAP[a].key, "MESSAGETYPE") )
        {
            strcpy(pInfoGeneric->MESSAGETYPE, pXMLRecord->MAP[a].value);
        }    	
        else if ( !strcmp(pXMLRecord->MAP[a].key, "MESSAGEORIGIN") )
        {
            strcpy(pInfoGeneric->MESSAGEORIGIN, pXMLRecord->MAP[a].value);
        }    	
        else if ( !strcmp(pXMLRecord->MAP[a].key, "ACTIONTYPE") )
        {
            strcpy(pInfoGeneric->ACTIONTYPE, pXMLRecord->MAP[a].value);
        }
        else if ( !strcmp(pXMLRecord->MAP[a].key, "FLNO") )
        {
            strcpy(pInfoGeneric->FLNO, pXMLRecord->MAP[a].value);
        }
        else if ( !strcmp(pXMLRecord->MAP[a].key, "ADID") )
        {
            strcpy(pInfoGeneric->ADID, pXMLRecord->MAP[a].value);
        }
        else if ( !strcmp(pXMLRecord->MAP[a].key, "STDT") )
        {
            strcpy(pInfoGeneric->STDT, pXMLRecord->MAP[a].value);
        }
    }
    
    dbg(DEBUG, "populateInfoGeneric:: End Result[%d]", ilRC);
    return ilRC;    	
}

int getXMLRecordIndexByGroup(char *pcpGroup, int ipStart)
{
    int ilRC=-1;
    int a=0;
    
    dbg(DEBUG, "getXMLRecordIndexByGroup:: Start");
    
    dbg(DEBUG, "getXMLRecordIndexByGroup:: Group[%s]", pcpGroup);
    for (a=ipStart; a<pcgRecordCount;a++)
    {
        if ( !strcmp(pcpGroup, 	rgXMLRecords[a].rgXMLList->GROUP) )
            break;
    }
    
    if ( a<pcgRecordCount )
         ilRC=a;
             
    dbg(DEBUG, "getXMLRecordIndexByGroup:: End Result[%d]", ilRC);
    return ilRC;	
}

int parseGroupXML(char *pcpXML, char *pcpHeader, char *pcpOut)
{
    int ilRC=RC_SUCCESS;
    char *pclStart, *pclEnd;
    char pclTmp[1024]; 
    int iSize=0;       
    	
    dbg(DEBUG, "parseGroupXML:: Start");

    strcpy(pcpOut, "");
    
    sprintf(pclTmp, "<%s/>", pcpHeader);
    pclStart=strstr(pcpXML, pclTmp);
    
    if ( pclStart!=NULL )
    {
        dbg(DEBUG, "parseGroup::  Found [%s].  No group members... Skipping", pclTmp);    	
    }
    else
    {
        sprintf(pclTmp, "<%s>", pcpHeader);
        pclStart=strstr(pcpXML, pclTmp);
        if ( pclStart==NULL )
        {
            dbg(DEBUG, "parseGroupXML:: Header Not Found in XML [%s]... Skipping", pclTmp);   
            ilRC=RC_FAIL;
        }
        else
        {
            dbg(DEBUG, "parseGroupXML:: Header Found in XML");    
            
            pclStart+=strlen(pclTmp);
            sprintf(pclTmp, "</%s>", pcpHeader);
            pclEnd=strstr(pcpXML, pclTmp);   
            
            if ( pclEnd==NULL )
            {
                dbg(DEBUG, "parseGroupXML:: Ending for Header Not Found in XML [%s]", pclTmp);    
                ilRC=RC_FAIL;	    	
            }  
            else
            {
                dbg(DEBUG, "parseGroupXML:: Ender Found in XML");    
                iSize=strlen(pclStart)-strlen(pclEnd);
                	
                sprintf(pclTmp, "%%%d.%ds",iSize, iSize);
                sprintf(pcpOut, pclTmp, pclStart);
        
//    		dbg(DEBUG, "parseGroupXML:: Content of Group[%s]", pcpOut);
            }           	
        }
    }
    
    dbg(DEBUG, "parseGroupXML:: End Result[%d]", ilRC);    
    return ilRC;	
}

int parseXMLMap(char *pcpXML, _XMLRECORD *pXMLRecord)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    int iTmp;
    int iSize=strlen(pcpXML);
    char pclKey[MAX_KEY+1];
    char pclValue[MAX_VALUE+1];
    char *pclPtr;
    
    dbg(DEBUG, "parseXMLMap:: Start");
 
    pXMLRecord->iSize=0;
    pXMLRecord->MAP[pXMLRecord->iSize].key[0]=0;
    pXMLRecord->MAP[pXMLRecord->iSize].value[0]=0;
    //memset(pXMLRecord->MAP, 0, sizeof(_MAP)*MAX_MAP);
                 
    for (a=0; a<iSize; a++)
    {
    	/*   get key   */
    	iTmp=0;
    	memset(pclKey, 0, sizeof(pclKey));
    	if ( pcpXML[a]=='<' )
    	{
    		a++;
    		for (; a<iSize; a++)
    	    {
    	         if ( pcpXML[a]=='>' )
    	             break;	
    	             
    	         if ( iTmp<MAX_KEY )
    	             pclKey[iTmp++]= pcpXML[a];
    	         else
    	         {
    	             dbg(TRACE,"parseXMLMap::  ERROR!!!  MAX_KEY IS NOT ENOUGH!!!  PLS ADVISE UFIS");
    	             ilRC=RC_FAIL;
    	             break;
    	         }
    	    }
    	    
    	    if ( ilRC==RC_FAIL )
    	        break;	
            else if ( !strcmp(pclKey, "") )
    	        dbg(DEBUG, "parseXMLMap:: Nothing more to parse");	
    	    else
    	    {
    	     //   dbg(DEBUG, "parseXMLMap:: Key Found[%s]", pclKey);	
    	        strcpy(pXMLRecord->MAP[pXMLRecord->iSize].key, pclKey); 
    	        sprintf(pclKey, "</%s>", pXMLRecord->MAP[pXMLRecord->iSize].key);
    	        
    	        pclPtr=&pcpXML[a];
    	        if ( strstr(pclPtr, pclKey)==NULL )
    	        {
    	        	dbg(DEBUG, "parseXMLMap:: ERROR! Key End[%s] not found! ", pclKey);
    	            ilRC=RC_FAIL;	
    	        }
    	        else
    	        {
    	     //       dbg(DEBUG, "parseXMLMap:: Key End Found[%s]", pclKey);	
    	            a++;
    	            
    	            /*   get value   */
    	            iTmp=0;
    	            memset(pclValue, 0, sizeof(pclValue));
    	            for (; a<iSize; a++)
    	            {
    	                if ( pcpXML[a]=='<' )
    	                    break;	
    	             
    	                if ( iTmp<MAX_VALUE )
    	                {
    	                	if ( strstr(pcgRemoveNewLine, pXMLRecord->MAP[pXMLRecord->iSize].key)!=NULL )
    	                	{
    	                	    if ( pcpXML[a]=='\n' )
    	                	    	pclValue[iTmp++]= ' ';
    	                	    else
    	                	        pclValue[iTmp++]= pcpXML[a];	
    	                	}
    	                	else
    	                        pclValue[iTmp++]= pcpXML[a];
    	                }
    	                else
    	                {
    	                    dbg(TRACE,"parseXMLMap::  ERROR!!!  MAY_VALUE IS NOT ENOUGH!!!  PLS ADVISE UFIS");
    	                    ilRC=RC_FAIL;
    	                    break;
    	                }
    	            }
    	            
    	            if ( ilRC==RC_SUCCESS )
    	            {
    	    //            dbg(DEBUG, "parseXMLMap:: Value[%s]", pclValue);
    	                pclPtr=&pcpXML[a];

    	                if ( strncmp(pclPtr, pclKey, strlen(pclKey)) )
    	                {
    	                   ilRC=RC_FAIL;
    	                   dbg(DEBUG, "parseXMLMap:: ERROR!!! Immediate XML pair invalid for [%s]", pXMLRecord->MAP[pXMLRecord->iSize].key);	
    	                } 
    	                else
    	                {
    	            	    strcpy(pXMLRecord->MAP[pXMLRecord->iSize].value, pclValue); 
    	                    a+=strlen(pclKey);	
    	                }
    	                pclPtr=&pcpXML[a];
    	            }
    	        }
    	        
    	        if ( ilRC==RC_SUCCESS )
    	            dbg(DEBUG, "parseXMLMap:: MAP[%d]: Key [%s], Value[%s]", 
    	                pXMLRecord->iSize, pXMLRecord->MAP[pXMLRecord->iSize].key, pXMLRecord->MAP[pXMLRecord->iSize].value);
    	        else
    	            break;   
    	        
    	        if ( pXMLRecord->iSize < MAX_MAP )
    	            pXMLRecord->iSize++;
    	        else
    	        {
    	            dbg(DEBUG, "parseXMLMap:: ERROR!!! KEY-VALUE pair MAXIMUM REACH!");
    	            ilRC=RC_FAIL;
    	            break;
    	        }
    	    }
    	} 
    }         
    
    if ( ilRC==RC_FAIL )
    {
        pXMLRecord->iSize=0;
        pXMLRecord->MAP[pXMLRecord->iSize].key[0]=0;
        pXMLRecord->MAP[pXMLRecord->iSize].value[0]=0;
     
        //memset(pXMLRecord->MAP, 0, sizeof(_MAP)*MAX_MAP);    	
    }  
    
    dbg(DEBUG, "parseXMLMap:: End Result[%d]", ilRC);
    return ilRC;	
}

int parseMultiXML(char *pcpXML, _XMLLIST *rpXMLList, char *pcpGroup, char *pcpTable, int *ipIndex)
{
	int ilRC=RC_SUCCESS;
	char pclTmpHeader[MAX_KEY+10];
	char pclTmpEndHeader[MAX_KEY+10];
	char pclTmpXML[MAX_XML+1];
	char pclKey[MAX_KEY+1];
	char *pclStart, *pclEnd;
	char *pclPtr;
	int iSize=0;

	
    dbg(DEBUG, "parseMultiXML:: Start");	
    dbg(DEBUG, "parseMultiXML:: Group[%s] Current Record Count[%d]", pcpGroup, *ipIndex);
    dbg(DEBUG, "parseMultiXML:: XML [%s]", pcpXML);
    
    sprintf(pclTmpHeader, "<%s>", pcpGroup);
    sprintf(pclTmpEndHeader, "</%s>", pcpGroup);    
    pclPtr=pcpXML;
    while (1)
    {
        dbg(DEBUG, "parseMultiXML:: Looking for [%s][%s] in [%s]", pclTmpHeader, pclTmpEndHeader, pclPtr);
        pclStart=strstr(pclPtr, pclTmpHeader);
        pclEnd=strstr(pclPtr, pclTmpEndHeader);
        
        if ( pclStart==NULL && pclEnd==NULL )
        {
            dbg(DEBUG, "parseMultXML:: No more records to parse");	
            break;
        }
        else if ( pclStart!=NULL && pclEnd!=NULL )
        {
        	pclStart+=strlen(pclTmpHeader);
            iSize=strlen(pclStart) - strlen(pclEnd);	
            sprintf(pclKey, "%%%d.%ds", iSize, iSize);
            sprintf(pclTmpXML, pclKey, pclStart);
            
            dbg(DEBUG, "parseMultiXML:: parsing[%s]", pclTmpXML);
            ilRC=parseXMLMap(pclTmpXML, &rgXMLRecords[pcgRecordCount]);
            if ( ilRC==RC_FAIL )
            {
            	dbg(DEBUG, "parseMultXML:: ERROR in parseXMLMap!!!");
                break;	
            }
            else
            {
                if (pcgRecordCount < MAX_RECORD -1)
                {
            	    rgXMLRecords[pcgRecordCount].rgXMLList=rpXMLList;
            	    pcgRecordCount++;	
                }
                else
                {
                    dbg(TRACE, "processXML:: ERROR! Exceed MAX Records ");
                    pcgRecordCount = MAX_RECORD - 1;
                    ilRC = RC_FAIL;
                    break;
                }
            }
            pclPtr=pclEnd+strlen(pclTmpEndHeader);
        }
        else
        {
            dbg(DEBUG, "parseMultiXML:: Invalid XML pair");
            ilRC=RC_FAIL;
            break; 	
        }
    }
    
    dbg(DEBUG, "parseMultiXML:: End Result[%d]", ilRC);
    return ilRC;
}

int formUpdateSQLCommand(char *pcpOut, _XMLRECORD *pXMLRecord)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    char pclSQL[MAX_XML+1];
    
    
    dbg(DEBUG, "formUpdateSQLCommand:: Start");
        
    memset(pclSQL, 0, sizeof(pclSQL));
    
    if ( !strcmp(pXMLRecord->rgXMLList->TABLE, "") )
    {
        dbg(DEBUG, "formUpdateSQLCOmmand:: Table name empty");
        ilRC=RC_FAIL;   	
    }
    else
    {
        if ( pXMLRecord->iSize>0 )
        {
            sprintf(pclSQL, "UPDATE %s SET ", pXMLRecord->rgXMLList->TABLE);
            for (a=0;a<pXMLRecord->iSize; a++)
            {
        	    if ( a==0 )
                    sprintf(pclSQL, "%s SET %s='%s' ", pclSQL, pXMLRecord->MAP[a].key, pXMLRecord->MAP[a].value);  
                else 
                    sprintf(pclSQL, "%s, %s='%s' ", pclSQL, pXMLRecord->MAP[a].key, pXMLRecord->MAP[a].value);    	
            }
            
            if ( strcmp(pXMLRecord->WHERE, "") )
                sprintf(pclSQL, "%s %s ", pclSQL, pXMLRecord->WHERE);
        }
        else
            dbg(DEBUG, "formUpdateSQLCommand::  Nothing to form");
    }
    
    if ( ilRC==RC_SUCCESS )
        strcpy(pcpOut, pclSQL);
    else
        strcpy(pcpOut, "");
    
    dbg(DEBUG, "formUpdateSQLCommand:: SQL[%s]", pclSQL);
        
    dbg(DEBUG, "formUpdateSQLCommand:: End Result[%d]", ilRC);
    return ilRC;	
}

int formInsertSQLCommand(char *pcpOut, _XMLRECORD *pXMLRecord)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    char pclSQL[MAX_XML+1];
    char pclHeader[MAX_XML+1];
    char pclValues[MAX_XML+1];
    
    
    dbg(DEBUG, "formInsertSQLCommand:: Start");
        
    memset(pclHeader, 0, sizeof(pclHeader));
    memset(pclValues, 0, sizeof(pclValues));
    
    if ( !strcmp(pXMLRecord->rgXMLList->TABLE, "") )
    {
        dbg(DEBUG, "formInsertSQLCOmmand:: Table name empty");
        ilRC=RC_FAIL;   	
    }
    else
    {
        if ( pXMLRecord->iSize>0 )
        {
            sprintf(pclHeader, "INSERT INTO %s ( ", pXMLRecord->rgXMLList->TABLE);
            sprintf(pclValues, "VALUE ( ");
            for (a=0;a<pXMLRecord->iSize; a++)
            {
        	    if ( a==0 )
        	    {
                    sprintf(pclHeader, "%s %s", pclHeader, pXMLRecord->MAP[a].key);  
                    sprintf(pclValues, "%s '%s'", pclValues, pXMLRecord->MAP[a].value);  
                }
                else 
                {
                    sprintf(pclHeader, "%s, %s", pclHeader, pXMLRecord->MAP[a].key);    
                    sprintf(pclValues, "%s, '%s'", pclValues, pXMLRecord->MAP[a].value);    
                }	
            }
            
            sprintf(pclHeader, "%s)", pclHeader);
            sprintf(pclValues, "%s)", pclValues);
            
            sprintf(pclSQL, "%s %s ", pclHeader, pclValues);
        }
        else
            dbg(DEBUG, "formInsertSQLCommand::  Nothing to form");
    }
    
    if ( ilRC==RC_SUCCESS )
        strcpy(pcpOut, pclSQL);
    else
        strcpy(pcpOut, "");
    
    dbg(DEBUG, "formInsertSQLCommand:: SQL[%s]", pclSQL);
        
    dbg(DEBUG, "formInsertSQLCommand:: End Result[%d]", ilRC);
    return ilRC;	
}

int evaluate(char *pcpDataType, char *pcpOp1, char *pcpOp, char *pcpOp2)
{
	int ilRC=RC_SUCCESS;
	
    dbg(DEBUG, "evaluate:: Start");
    
    dbg(DEBUG, "evaluate:: pcpDataType[%s], pcpOp1[%s], pcpOp[%s], pcpOp2[%s]", 
        pcpDataType, pcpOp1, pcpOp, pcpOp2);
    
    if ( !strcmp(pcpDataType, "STRING") )
    {
        if ( !strcmp(pcpOp, "=") )
            ilRC=!strcmp(pcpOp1,pcpOp2);
        else if ( !strcmp(pcpOp, "!=") )
            ilRC=strcmp(pcpOp1, pcpOp2);
        else if ( !strcmp(pcpOp, "<") )
            ilRC=strcmp(pcpOp1,pcpOp2)<0;
        else if ( !strcmp(pcpOp, "<=") )
            ilRC=strcmp(pcpOp1,pcpOp2)<=0;
        else if ( !strcmp(pcpOp, ">") )
            ilRC=strcmp(pcpOp1,pcpOp2)>0;
        else if ( !strcmp(pcpOp, ">=") )
            ilRC=strcmp(pcpOp1,pcpOp2)>=0;            	
    }
    dbg(DEBUG, "evaluate:: End Result[%d]", ilRC);
    return ilRC;    	
}

int getUrnoInList(char *pcpFlno, char *pcpAdid, char *pcpStdt)
{
    int ilRC=RC_FAIL;
    int a=0;
    
    dbg(DEBUG, "getUrnoInList:: Start");
    dbg(DEBUG, "getUrnoInList:: pcpFlno[%s], pcpAdid[%s], pcpStdt[%s]", pcpFlno, pcpAdid, pcpStdt);
    
    if ( igFlightIndexEnd==0 )
    {
        dbg(DEBUG, "getUrnoInList:: No records in List");	
    }
    else
    {
        for (a=0; a<igFlightIndexEnd; a++)
        {
        	if ( !strcmp(rgFlight[a].ADID, "A") )
        	{
                if ( !strcmp(rgFlight[a].FLNO, pcpFlno) &&
                	 !strcmp(rgFlight[a].STOA, pcpStdt) )
                {
                    dbg(DEBUG, "getUrnoInList:: Found!");	
                    break;
                }	
            }
            else if ( !strcmp(rgFlight[a].ADID, "D") )
            {
                if ( !strcmp(rgFlight[a].FLNO, pcpFlno) &&
                	 !strcmp(rgFlight[a].STOD, pcpStdt) )
                {
                    dbg(DEBUG, "getUrnoInList:: Found!");	
                    break;
                }            	
            }
        }	
        
        if ( a<igFlightIndexEnd )
            ilRC=a;
    }
    
    dbg(DEBUG, "getUrnoInList:: End Result[%d]", ilRC);	
    return ilRC;
}

int sendCedaEventWithLog(int  ipRouteId, /* adress to send the answer   */
                         int  ipOrgId,       /* Set to mod_id if < 1        */
                         char *pcpDstNam,    /* BC_HEAD.dest_name           */
                         char *pcpRcvNam,    /* BC_HEAD.recv_name           */
                         char *pcpStart,     /* CMDBLK.tw_start             */
                         char *pcpEnd,       /* CMDBLK.tw_end               */
                         char *pcpCommand,   /* CMDBLK.command              */
                         char *pcpTabName,   /* CMDBLK.obj_name             */
                         char *pcpSelection, /* Selection Block             */
                         char *pcpFields,    /* Field Block                 */
                         char *pcpData,      /* Data Block                  */
                         char *pcpErrDscr,   /* Error description           */
                         int  ipPrio,        /* 0 or Priority               */
                         int  ipRetCode)  
{
    int ilRC=RC_SUCCESS;
    
    dbg(TRACE, "sendCedaEventWithLog:: Start");
    
    dbg(TRACE, "sendCedaEventWithLog:: ipRouteId[%d]", ipRouteId);
    dbg(TRACE, "sendCedaEventWithLog:: ipOrgId[%d]", ipOrgId);
    dbg(TRACE, "sendCedaEventWithLog:: pcpDstNam[%s]", pcpDstNam);
    dbg(TRACE, "sendCedaEventWithLog:: pcpRcvNam[%s]", pcpRcvNam);
    dbg(TRACE, "sendCedaEventWithLog:: pcpStart[%s]", pcpStart);
    dbg(TRACE, "sendCedaEventWithLog:: pcpEnd[%s]", pcpEnd);
    dbg(TRACE, "sendCedaEventWithLog:: pcpCommand[%s]", pcpCommand);
    dbg(TRACE, "sendCedaEventWithLog:: pcpTabName[%s]", pcpTabName);
    dbg(TRACE, "sendCedaEventWithLog:: pcpSelection[%s]", pcpSelection);
    dbg(TRACE, "sendCedaEventWithLog:: pcpFields[%s]", pcpFields);
    dbg(TRACE, "sendCedaEventWithLog:: pcpData[%s]", pcpData);
    dbg(TRACE, "sendCedaEventWithLog:: pcpErrDscr[%s]", pcpErrDscr);
    dbg(TRACE, "sendCedaEventWithLog:: ipPrio[%d]", ipPrio);
    dbg(TRACE, "sendCedaEventWithLog:: ipRetCode[%d]", ipRetCode);

    ilRC = SendCedaEvent (ipRouteId, 
                          ipOrgId,
                          pcpDstNam,
                          pcpRcvNam,
                          pcpStart, 
                          pcpEnd, 
                          pcpCommand,
                          pcpTabName, 
                          pcpSelection,
                          pcpFields,
                          pcpData,
                          pcpErrDscr,
                          ipPrio,
                          ipRetCode);

    dbg(TRACE, "sendCedaEventWithLog:: End [%d]", ilRC);
    return ilRC;
}

int searchBufferUrno(char *pcpOut, char *pcpWhere)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    
    dbg(TRACE, "searchBufferUrno:: Start");
    
    strcpy(pcpOut, "");
    dbg(TRACE, "searchBufferUrno:: pcpWhere[%s], igInsertUrno[%d], igMaxUrnoBufferSize[%d]", pcpWhere, igInsertUrno, igMaxUrnoBufferSize);
    for (a=0; a<igMaxUrnoBufferSize; a++)
    {
        if ( !strcmp(rgpUrnoBuffer[a].WHERE, "") )
            break;
        if ( !strcmp(rgpUrnoBuffer[a].WHERE, pcpWhere) )
        {
            strcpy(pcpOut, rgpUrnoBuffer[a].URNO);
            break;	
        }	
    }
    
    dbg(TRACE, "searchBufferUrno:: URNO[%s]", pcpOut);
    dbg(TRACE, "searchBufferUrno:: End Result[%d]", ilRC);
    return ilRC;	
}

int clearBufferUrno(char *pcpWhere)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    
    dbg(DEBUG, "clearBufferUrno:: Start");
    
    dbg(DEBUG, "clearBufferUrno:: pcpWhere[%s], igInsertUrno[%d], igMaxUrnoBufferSize[%d]", pcpWhere, igInsertUrno, igMaxUrnoBufferSize);
    for (a=0; a<igMaxUrnoBufferSize; a++)
    {
        if ( !strcmp(rgpUrnoBuffer[a].WHERE, "") )
            break;
            
        if ( !strcmp(rgpUrnoBuffer[a].WHERE, pcpWhere) )
        {
        	dbg(DEBUG, "clearBufferUrno::  Clearing Where Clause [%s]", pcpWhere);
        	strcpy(rgpUrnoBuffer[a].WHERE, "DELETED");
            strcpy(rgpUrnoBuffer[a].URNO, "");
            break;	
        }	
    }
    
    if ( a>=igMaxUrnoBufferSize )
    {
        dbg(DEBUG, "clearBufferUrno:: Cannot find [%s] in buffer", pcpWhere);
        ilRC=RC_FAIL;	
    }
    	
    dbg(DEBUG, "clearBufferUrno:: End Result[%d]", ilRC);
    return ilRC;	
}

int processAfttabRecord(_AFTTAB *rpAft, _INFOGENERIC *rpInfo, _XMLRECORD *rpXMLRecord)
{
    int ilRC=RC_SUCCESS;
    int iCommaFlag=0;
    int a=0;
    int ilInsert=0;
    long llUrno;
    char pclCmd[MAX_COMMAND+1];
    char pclFields[MAX_XML+1];
    char pclTmpFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    char pclTmpValues[MAX_XML+1];
    char pclWhere[MAX_XML+1];
    char pclMode[MAX_VALUE+1];
    
    dbg(TRACE, "processAfttabRecord:: Start");
    
    dbg(DEBUG, "processAfttabRecord:: From <INFOGENERIC> Flno[%s], Adid[%s], STDT[%s], igInsertUrno[%d]", 
        rpInfo->FLNO, rpInfo->ADID, rpInfo->STDT, igInsertUrno);    
    dbg(DEBUG, "processAfttabRecord:: From Afttab Urno[%s], Flno[%s], Adid[%s], STOA[%s], STOD[%s]", 
        rpAft->URNO, rpAft->FLNO, rpAft->ADID, rpAft->STOA, rpAft->STOD);
      
    pclCmd[0]=0;
    pclFields[0]=0;
    pclValues[0]=0;
    pclWhere[0]=0;
    pclMode[0]=0;
    ilRC=constructWhereClause(0, pclWhere, pclTmpFields, pclTmpValues, rpXMLRecord->rgXMLList->rgSelect, rpXMLRecord->rgXMLList->igSelectGroup);   

/*
    DO NOT DELETE.  Will be use for duplicate checking.
    {
        FILE *fptr;
        char pclTmp[1001];
        
        sprintf(pclTmp, "/ceda/debug/%s.where", mod_name);
        fptr=fopen(pclTmp, "a+");
        if ( fptr )
        {
            fprintf(fptr, "%s\n", pclWhere);
            fclose(fptr);	
        }	
    }
 */   
    if ( ilRC==RC_SUCCESS )
    {
        sprintf(pclWon, "%s, pclWhere[%s]", pclWon, pclWhere);          	 
        ilRC=getAfttabRecord(rpAft, pclWhere);
  
        if ( ilRC==RC_FAIL )
        {
            dbg(TRACE, "processAfttabRecord:: ERROR in getAfttabRecord!");
        }
        else
        {
        	ilRC=RC_SUCCESS;
        	if ( !strcmp(rpXMLRecord->rgXMLList->TABLE,"") )
        	    dbg(DEBUG, "processAfttabRecord:: Nothing to process for afttab");
        	else
        	{ 
        	    ilRC=RC_SUCCESS;
        	
        	    if ( !strcmp(rpAft->URNO, "") )        
                    searchBufferUrno(rpAft->URNO, pclWhere);
        	
                switch (rgInfoGeneric.ACTIONTYPE[0])
                {
                    case 'D':
                    {
                    	dbg(TRACE, "processAfttabRecord:: ACTIONTYPE is DELETE");
                    	
                        if ( !strcmp(rpAft->URNO, "") )   
                        { 
                            dbg(TRACE, "processAfttabRecord::  Record not found.  Nothing to delete.");
                            ilRC=RC_FAIL;	
                        }
                        else
                        {
                 	        strcpy(pclMode, "DELETE");
                            strcpy(pclCmd, rpXMLRecord->rgXMLList->DELCMD);      
                            clearBufferUrno(pclWhere);               	
                        }	
                        break;
                    }
                    case 'I':
                    {
                    	dbg(TRACE, "processAfttabRecord:: ACTIONTYPE is INSERT");
        
                        if ( strcmp(rpAft->URNO, "") )   
                	    {
                            dbg(TRACE, "processAfttabRecord:: Record found in DB.  Duplicate");
                            ilRC=RC_FAIL;            	    	
                	    }
                	    else
                	    {
                	        strcpy(pclMode, "INSERT");
                		    ilInsert=1;
                            strcpy(pclCmd, rpXMLRecord->rgXMLList->INSCMD); 
                        }
                        break;                                  	
                    }
                    case 'U':
                    {
                    	dbg(TRACE, "processAfttabRecord:: ACTIONTYPE is UPDATE");
        
                        if ( !strcmp(rpAft->URNO, "") )   
                        {
                            dbg(TRACE, "processAfttabRecord:: Record not found.  Nothing to update.");
                            ilRC=RC_FAIL;	                    	
                        }
                        else 
                        {
                            sprintf(pclWhere, "WHERE URNO='%s' ", rpAft->URNO);   
                            strcpy(pclMode, "UPDATE");
                            strcpy(pclCmd, rpXMLRecord->rgXMLList->UPDCMD);	
                        }
                        break;
                    }
                    case 0:
                    {
                    	dbg(TRACE, "processAfttabRecord:: ACTIONTYPE is Missing");
                        	
                	    if ( !strcmp(rpAft->URNO, "") )    
                	    {
                	    	if ( !strcmp(mod_name, "egdsexc") )
                	    	{
                	    	    ilRC=RC_FAIL;
                	    	    dbg(DEBUG, "processAfttabRecord:: No insert in afttab allowed in [%s]", mod_name);
                	    	}
                	    	else
                	    	{
                		        strcpy(pclMode, "INSERT");
                		        ilInsert=1;
                                strcpy(pclCmd, rpXMLRecord->rgXMLList->INSCMD);  
                            }
                        }
                        else
                        {
                    	    strcpy(pclMode, "UPDATE");
                            strcpy(pclCmd, rpXMLRecord->rgXMLList->UPDCMD);	
                            sprintf(pclWhere, "WHERE URNO='%s' ", rpAft->URNO);   
                        }
                        break;
                    }
                    default:
                        dbg(TRACE, "processAfttabRecord:: Invalid <ACTIONTYPE>");
                        ilRC=RC_FAIL;
                        break;
                }
        
                dbg(TRACE, "processAfttabRecord:: pclCmd[%s], ilInsert[%d], ilRC[%d]", pclCmd, ilInsert, ilRC);
        
                if ( ilRC==RC_SUCCESS )
                {
                    for (a=0; a< rpXMLRecord->iSize; a++)
                    {
                    	/*
                    	if ( !strcmp(rpXMLRecord->MAP[a].key, "REM1") )
                    	{
                    	    if ( !strcmp(rpXMLRecord->MAP[a].value, "CNCLD") )
                    	    {
                    	        if ( iCommaFlag )
                                {
                                    sprintf(pclFields, "%s,%s", pclFields, "FTYP");	
                                    sprintf(pclValues, "%s,%s", pclValues, "X");	        	
                                }
                                else
                                {
                                    iCommaFlag=1;
                                    strcpy(pclFields, "FTYP");	
                                    strcpy(pclValues, "X");	
                                }	
                    	    }	
                    	}
                    	else 
                    	*/
                    	if ( iCommaFlag )
                        {
                            sprintf(pclFields, "%s,%s", pclFields, rpXMLRecord->MAP[a].key);	
                            sprintf(pclValues, "%s,%s", pclValues, rpXMLRecord->MAP[a].value);	        	
                        }
                        else
                        {
                            iCommaFlag=1;
                            strcpy(pclFields, rpXMLRecord->MAP[a].key);	
                            strcpy(pclValues, rpXMLRecord->MAP[a].value);	
                        }	
                    }
                        
                    if ( ilInsert )
            	    {         
                        //GetNextValues(rpAft->URNO,1);  
                        dbg(TRACE, "processAfttabRecord:: Getting new urno");
                        sprintf(rpAft->URNO, "%ld", NewUrnos("AFTTAB",1));                     
                        dbg(TRACE, "processAfttabRecord:: New Urno[%s]", rpAft->URNO);
                        
                        strcpy(pclCmd, rpXMLRecord->rgXMLList->INSCMD);              
                        
                        if ( iCommaFlag )
                        {
                            sprintf(pclFields, "%s,%s", pclFields, "URNO");	
                            sprintf(pclValues, "%s,%s", pclValues, rpAft->URNO);	                  	
                        }
                        else
                        {
                            strcpy(pclFields, "URNO");
                            strcpy(pclValues, rpAft->URNO);
                        }
                         
                        if (igMaxUrnoBufferSize > 0)
                        {
                	    strcpy(rgpUrnoBuffer[igInsertUrno].URNO, rpAft->URNO);
                	    strcpy(rgpUrnoBuffer[igInsertUrno].WHERE, pclWhere);
                            igInsertUrno++;
                            if ( igInsertUrno>=igMaxUrnoBufferSize )
                                igInsertUrno=0;
                        }
                              	        	                           
            	        ilRC=constructAdditionalKeyValuePair(pclFields, pclValues, rpXMLRecord->rgXMLList->rgSelect, rpXMLRecord->rgXMLList->igSelectGroup);  
            	        strcpy(pclWhere,"");   	
                    }           	       
            	       
            	    if ( strstr(rpXMLRecord->rgXMLList->MODE, pclMode)!=NULL )
            	    {
            	    	sprintf(pclWon, "%s, FlightSend[%s]", pclWon, pclCmd);
                        ilRC = PatchFldLst(&rgAfttab, rpXMLRecord, pclFields, pclValues, rpXMLRecord->rgXMLList->GROUP);
                        ilRC = ReferTable(&rgAfttab, rpXMLRecord, pclFields,pclValues);
                        
                        ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                                                  0,             
                                                  mod_name,
                                                  mod_name,
                                                  pcgTwStart,
                                                  pcgTwEnd,     
                                                  pclCmd,
                                                  rpXMLRecord->rgXMLList->TABLE,
                                                  pclWhere,
                                                  pclFields,
                                                  pclValues,
                                                  "",
                                                  3,
                                                  NETOUT_NO_ACK);
                       if (igArrRegnStk == TRUE)
                       {
                          (void) ArrRegnStk(&rgAfttab, pclFields, pclValues, rpXMLRecord->rgXMLList->ilSendQueue, pclCmd, rpXMLRecord->rgXMLList->TABLE);
                       }
                    }
                    else
                    {
                    	sprintf(pclWon, "%s, FlightSend[%s]", pclWon, "No SEND");
                        dbg(DEBUG, "processAfttabRecord:: Skipping [%s] not in [%s]", pclMode, rpXMLRecord->rgXMLList->MODE);
                    }                               	
                }
            }
        }
    }
    dbg(TRACE, "processAfttabRecord:: End Result[%d]", ilRC);
    return ilRC	;
}

int parseSelectConfig(char *pcpSelect, char *pcpGroup, _SELECTGROUP *rpSelect)
{
    int ilRC=0;
    int a=0, ilSize=0, b=0;
    char pclSelect[strlen(pcpSelect)+1];
    char *pclCondition, *pclWhere;
    char *pclPtr;

    
    dbg(DEBUG, "parseSelectConfig:: Start");
    dbg(DEBUG, "parseSelectConfig:: Group[%s] Select String[%s]", pcpGroup, pcpSelect);
    strcpy(pclSelect, pcpSelect);
    
    pclCondition=strtok_r(pclSelect, ":", &pclPtr);
    pclWhere=strtok_r(NULL, ":", &pclPtr);
    
    dbg(DEBUG, "parseSelectConfig:: pclCondition[%s]", pclCondition);
    dbg(DEBUG, "parseSelectConfig:: pclWhere[%s]", pclWhere);
    
    if ( !strcmp(pclCondition,"") )
        ;
    else if ( !strcmp(pclCondition,"DEFAULT") )
    {
        rpSelect->TYPE='D';   
        strcpy(rpSelect->HEADER,"DEFAULT");
    }    
    else
    {
    	ilSize=strlen(pclCondition);
    	a=b=0;
    	if ( pclCondition[a]=='<' )
    	{
    	    rpSelect->TYPE='X';
    	    a++;
    	}
    	else
    	    rpSelect->TYPE='T';
    	    
        for (; a<ilSize; a++)
        {
            if ( pclCondition[a]!='.' && pclCondition[a]!='>' )	
                rpSelect->HEADER[b++]=pclCondition[a];
            else
                break;
        }
        rpSelect->HEADER[b++]=0;
        
        for (;a<ilSize; a++)
        {
            if (pclCondition[a]=='.')
                break;	
        }
        
        a++;
        if ( pclCondition[a]=='<' )
            a++;
        
        b=0;
        for (; a<ilSize; a++)
        {
            if ( isalnum(pclCondition[a]) )	
                rpSelect->FIELD[b++]=pclCondition[a];
            else
                break;
        }
        rpSelect->FIELD[b++]=0;      
        
        if ( pclCondition[a]=='>' )  
            a++;
        
        b=0;
        rpSelect->OPS[b++]=pclCondition[a++];
        if ( pclCondition[a]=='=' )
            rpSelect->OPS[b++]=pclCondition[a++];	
        rpSelect->OPS[b++]=0;    
        
        b=0;
        for (; a<ilSize; a++)
        {
            rpSelect->VALUE[b++]=pclCondition[a];
        }
        rpSelect->VALUE[b++]=0;           
             
    }
    
    pclCondition=strtok_r(pclWhere, ",", &pclPtr);
    
    memset(rpSelect->rgSelect,0,sizeof(_SELECTIONFIELD)*MAX_SELECT);
    rpSelect->igSelectSize=0;
    while ( pclCondition )
    {
    	dbg(DEBUG, "parseSelectConfig:: parsing[%s]", pclCondition);
    	a=b=0;
    	ilSize=strlen(pclCondition);
    	for (a=0; a<ilSize; a++)
    	{
    		if ( isalnum(pclCondition[a]) )
    	        rpSelect->rgSelect[rpSelect->igSelectSize].FIELD[b++]=pclCondition[a];	
    	    else
    	        break;
    	}
    	rpSelect->rgSelect[rpSelect->igSelectSize].FIELD[b++]=0;

        b=0;
        rpSelect->rgSelect[rpSelect->igSelectSize].OPS[b++]=pclCondition[a++];
        if ( pclCondition[a]=='=' )
            rpSelect->rgSelect[rpSelect->igSelectSize].OPS[b++]=pclCondition[a++];	
        rpSelect->rgSelect[rpSelect->igSelectSize].OPS[b++]=0;    
        
        
        if ( pclCondition[a]=='<' || pclCondition[a]=='[' )
        {
            if ( pclCondition[a]=='<' )
                rpSelect->rgSelect[rpSelect->igSelectSize].TYPE='X';
	        else if ( pclCondition[a]=='[' )
                rpSelect->rgSelect[rpSelect->igSelectSize].TYPE='T';
            
            a++;
            b=0;
            for (; a<ilSize; a++)
            {
                if ( pclCondition[a]!=']' && pclCondition[a]!='>' )	
                    rpSelect->rgSelect[rpSelect->igSelectSize].HEADER[b++]=pclCondition[a];
                else
                    break;
            }
            rpSelect->rgSelect[rpSelect->igSelectSize].HEADER[b++]=0;
        
            dbg(DEBUG, "parseSelectConfig:: Group[%s] Header[%s]", rpSelect->rgSelect[rpSelect->igSelectSize].HEADER, pcpGroup);
            if ( !strcmp(pcpGroup, rpSelect->rgSelect[rpSelect->igSelectSize].HEADER) )
            {
                dbg(DEBUG, "parseSelectConfig:: Change type to point to self");
                rpSelect->rgSelect[rpSelect->igSelectSize].TYPE='S';	
            }
            
            for (;a<ilSize; a++)
            {
                if (pclCondition[a]=='.')
                    break;	
            }
        
            a++;
            if ( pclCondition[a]=='<' || pclCondition[a]=='[' )
                a++;
        
            b=0;
            for (; a<ilSize; a++)
            {
                if ( pclCondition[a]!=']' && pclCondition[a]!='>' )	
                    rpSelect->rgSelect[rpSelect->igSelectSize].VALUE[b++]=pclCondition[a];
                else
                    break;
            }
            rpSelect->rgSelect[rpSelect->igSelectSize].VALUE[b++]=0;    
        }
        else
        {
        	rpSelect->rgSelect[rpSelect->igSelectSize].TYPE='V';
            b=0;
            for (; a<ilSize; a++)
                rpSelect->rgSelect[rpSelect->igSelectSize].VALUE[b++]=pclCondition[a];

            rpSelect->rgSelect[rpSelect->igSelectSize].VALUE[b++]=0;           	
        }            	
        rpSelect->igSelectSize++;
    	pclCondition=strtok_r(NULL, ",", &pclPtr);    	
    }

    dbg(DEBUG, "parseSelectConfig:: End Result[%d]", ilRC);
    return ilRC;  	
}

int getKeyValue(char *pcpOut, char *pcpHeader, char *pcpKey)
{
    int ilRC=RC_SUCCESS;
    int ilIndex;
    
    dbg(DEBUG, "getKeyValue:: Start");
    
    dbg(DEBUG, "getKeyValue:: pcpHeader[%s], pcpKey[%s]", pcpHeader, pcpKey);
    ilIndex = getXMLRecordIndexByGroup(pcpHeader, 0);
    dbg(DEBUG, "getKeyValue:: Index[%d]", ilIndex);
    if ( ilIndex==RC_FAIL )
    {
        dbg(DEBUG, "getKeyValue:: ERROR! Header Not Found!");	
        ilRC=RC_FAIL;
    }
    else
    {
        ilRC=getKeyValuePair(pcpOut, pcpKey, &rgXMLRecords[ilIndex]);	
    }
    dbg(DEBUG, "getKeyValue:: End Result[%d]", ilRC);
    return ilRC;    	
}

int getKeyValuePair(char *pcpOut, char *pcpKey, _XMLRECORD *rpXML)
{
    int ilRC=RC_SUCCESS;
    int a=0;
    
    dbg(DEBUG, "getKeyValuePair:: Start");
    dbg(DEBUG, "getKeyValuePair:: iSize[%d], pcpKey[%s]", rpXML->iSize, pcpKey);
    
    for ( a=0; a<rpXML->iSize; a++)
    {
        if ( !strcmp(rpXML->MAP[a].key, pcpKey) )
            break;	
    }
    
    if ( a<rpXML->iSize )
    {
    	if ( !strcmp(rpXML->MAP[a].value, "") )
    	    strcpy(pcpOut, " ");
        else
            strcpy(pcpOut, rpXML->MAP[a].value);
    }
    else
    {
    	ilRC=RC_FAIL;
        strcpy(pcpOut, "");
    }
        
    dbg(DEBUG, "getKeyValuePair:: pcpOut[%s]", pcpOut);
    dbg(DEBUG, "getKeyValuePair:: End Result[%d]", ilRC);
    return ilRC;	
}

int constructWhereClause(int ipSelf, char *pcpWhere, char *pcpFields, char *pcpValues, _SELECTGROUP *rpSelect, int ipCount)
{
    int ilRC=RC_SUCCESS;
    int ilTmp=0;
    int ilDefault=0;
    int a=0;
    char pclValue[MAX_VALUE+1];
    _SELECTGROUP *rl=NULL;
    int ilAndFlag=0;
        
    dbg(DEBUG, "constructWhereClause:; Start");
    
    dbg(DEBUG, "constructWhereClause:: Self[%d]", ipSelf);
    strcpy(pcpWhere, "");
    dbg(DEBUG, "constructWhereClause:: Count[%d]", ipCount);
    for (a=0; a<ipCount; a++)
    {
        /*
        dbg(DEBUG, "constructWhereClause:: Processing[%d/%d]", a+1, ipCount);
        dbg(DEBUG, "constructWhereClause:: TYPE[%c]", rpSelect[a].TYPE);
        dbg(DEBUG, "constructWhereClause:: HEADER[%s]", rpSelect[a].HEADER);
        dbg(DEBUG, "constructWhereClause:: FIELD[%s]", rpSelect[a].FIELD);
        dbg(DEBUG, "constructWhereClause:: OPS[%s]", rpSelect[a].OPS);
        dbg(DEBUG, "constructWhereClause:: VALUE[%s]", rpSelect[a].VALUE);
        */
        if ( rpSelect[a].TYPE=='D' )
        {
        	ilDefault=a;
            dbg(DEBUG, "constructWhereClause:: Skipping this first");
        }
        else
        {
            memset(pclValue, 0, sizeof(pclValue));
            ilTmp=getKeyValue(pclValue, rpSelect[a].HEADER, rpSelect[a].FIELD);
  //          if ( ilTmp==RC_SUCCESS )
            {
                ilTmp=evaluate("STRING", pclValue, rpSelect[a].OPS, rpSelect[a].VALUE);
                if ( ilTmp )
                    break;
            }
        }        
    }
    
    if ( a<ipCount )
    {
    	dbg(DEBUG,"constructWhereClause::  Use SELECTION[%d]", a);
        rl=&rpSelect[a];
    }
    else
    {
    	dbg(DEBUG,"constructWhereClause::  Use Default SELECTION");
        rl=&rpSelect[ilDefault];	
    }
    
    if ( rl==NULL )
    {
        dbg(TRACE, "constructWhereClause:: ERROR!!! RL is NULL");	
        ilRC=RC_FAIL;
    }
    else
    {
        for (a=0; a<rl->igSelectSize; a++)
        {
            dbg(DEBUG, "constructWhereClause:: Field[%d/%d]", a+1, rl->igSelectSize);	
            dbg(DEBUG, "constructWhereClause::      FIELD[%s]", rl->rgSelect[a].FIELD);	
            dbg(DEBUG, "constructWhereClause::      OPS[%s]", rl->rgSelect[a].OPS);	
            dbg(DEBUG, "constructWhereClause::      TYPE[%c]", rl->rgSelect[a].TYPE);	
            dbg(DEBUG, "constructWhereClause::      HEADER[%s]", rl->rgSelect[a].HEADER);	
            dbg(DEBUG, "constructWhereClause::      VALUE[%s]", rl->rgSelect[a].VALUE);	
            
            memset(pclValue, 0, sizeof(pclValue));
            ilTmp=RC_SUCCESS;
            if ( rl->rgSelect[a].TYPE=='S' )
            {
            	if ( 
            	      !strcmp(rl->rgSelect[a].HEADER, "INFOBJ_GENERIC") && 
            	      ( !strcmp(rl->rgSelect[a].FIELD, "STOA") || !strcmp(rl->rgSelect[a].FIELD, "STOD") )
            	   )
                    ilTmp=getKeyValuePair(pclValue, "STDT", &rgXMLRecords[ipSelf]);
                else		
                    ilTmp=getKeyValuePair(pclValue, rl->rgSelect[a].VALUE, &rgXMLRecords[ipSelf]);		
            }
            else if ( rl->rgSelect[a].TYPE=='X' )
                ilTmp=getKeyValue(pclValue,  rl->rgSelect[a].HEADER, rl->rgSelect[a].VALUE);
            else if ( rl->rgSelect[a].TYPE=='T' )
            {
                if ( !strcmp(rl->rgSelect[a].HEADER, "AFTTAB") )
                {
                    if ( !strcmp(rl->rgSelect[a].VALUE, "URNO") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.URNO);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "FLNO") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.FLNO);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ADID") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ADID);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "RKEY") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.RKEY);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "STOA") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.STOA);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "STOD") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.STOD);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ACT3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ACT3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ACT5") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ACT5);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ALC2") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ALC2);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ALC3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ALC3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "DES3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.DES3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "DES4") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.DES4);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ORG3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ORG3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ORG4") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ORG4);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "RTYP") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.RTYP);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "FTYP") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.FTYP);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "REGN") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.REGN);
                    }
                }
            }
            else if ( rl->rgSelect[a].TYPE=='V' )
                strcpy(pclValue, rl->rgSelect[a].VALUE);
                
            if ( ilTmp==RC_SUCCESS )
            {
              if (strcmp(rl->rgSelect[a].OPS,"|") == 0)
              {
                 sprintf(pcpWhere, "%s) OR (", pcpWhere);
              }
              else
              {
                if ( ilAndFlag )
                {
                    if (strcmp(rl->rgSelect[a-1].OPS,"|") == 0)
                        sprintf(pcpWhere, "%s %s%s'%s'", 
                         pcpWhere, rl->rgSelect[a].FIELD, rl->rgSelect[a].OPS, pclValue);
                    else
                        sprintf(pcpWhere, "%s AND %s%s'%s'", 
                         pcpWhere, rl->rgSelect[a].FIELD, rl->rgSelect[a].OPS, pclValue);	  
                    sprintf(pcpFields, "%s,%s", pcpFields, rl->rgSelect[a].FIELD);                  	
                    sprintf(pcpValues, "%s,%s", pcpValues, pclValue);                  	
                }
                else
                {
                    ilAndFlag=1;
                    sprintf(pcpWhere, "WHERE (( %s%s'%s'", 
                            rl->rgSelect[a].FIELD, rl->rgSelect[a].OPS, pclValue);	
                    strcpy(pcpFields, rl->rgSelect[a].FIELD);
                    strcpy(pcpValues, pclValue);       
                }         
              }
            } 
        }
    
        if (ilAndFlag)
             sprintf(pcpWhere, "%s)) ", pcpWhere);

        if ( strcmp(rl->ADDITIONALWHERE, "") )
        {
            if ( strcmp(pcpWhere, "") )
                sprintf(pcpWhere, "%s AND %s ", pcpWhere, rl->ADDITIONALWHERE);
            else
        	    strcpy(pcpWhere, rl->ADDITIONALWHERE);    	
        }
    }
    
    if ( !strcmp(pcpWhere, "") )
    {
        dbg(DEBUG, "constructWhereClause:: ERROR!!! Empty Where Clause");
        ilRC=RC_FAIL;	
    }
    
    dbg(DEBUG, "constructWhereClause:: Where[%s] Fields[%s] Values[%s]", pcpWhere, pcpFields, pcpValues);
    dbg(DEBUG, "constructWhereClause:: End Result[%d]", ilRC);
    return ilRC;	
}

int constructAdditionalKeyValuePair(char *pcpFields, char *pcpValues, _SELECTGROUP *rpSelect, int ipCount)
{
    int ilRC=RC_SUCCESS;
    int ilTmp=0;
    int ilDefault=0;
    int a=0;
    char pclValue[MAX_VALUE+1];
    _SELECTGROUP *rl=NULL;
    int ilAndFlag=0;
        
    dbg(DEBUG, "constructAdditionalKeyValuePair:; Start");
    
    dbg(DEBUG, "constructAdditionalKeyValuePair:: Count[%d]", ipCount);
    for (a=0; a<ipCount; a++)
    {
        dbg(DEBUG, "constructAdditionalKeyValuePair:: Processing[%d/%d]", a+1, ipCount);
        dbg(DEBUG, "constructAdditionalKeyValuePair:: TYPE[%c]", rpSelect[a].TYPE);
        dbg(DEBUG, "constructAdditionalKeyValuePair:: HEADER[%s]", rpSelect[a].HEADER);
        dbg(DEBUG, "constructAdditionalKeyValuePair:: FIELD[%s]", rpSelect[a].FIELD);
        dbg(DEBUG, "constructAdditionalKeyValuePair:: OPS[%s]", rpSelect[a].OPS);
        dbg(DEBUG, "constructAdditionalKeyValuePair:: VALUE[%s]", rpSelect[a].VALUE);
        
        if ( rpSelect[a].TYPE=='D' )
        {
        	ilDefault=a;
            dbg(DEBUG, "constructAdditionalKeyValuePair:: Skipping this");
        }
        else
        {
            memset(pclValue, 0, sizeof(pclValue));
            ilTmp=getKeyValue(pclValue, rpSelect[a].HEADER, rpSelect[a].FIELD);
            if ( ilTmp==RC_SUCCESS )
            {
                ilTmp=evaluate("STRING", pclValue, rpSelect[a].OPS, rpSelect[a].VALUE);
                if ( ilTmp )
                    break;
            }
        }        
    }
    
    if ( a<ipCount )
    {
        rl=&rpSelect[a];
    }
    else
    {
        rl=&rpSelect[ilDefault];	
    }
    
    if ( rl==NULL )
        ;
    else
    {
        for (a=0; a<rl->igSelectSize; a++)
        {
            dbg(DEBUG, "constructAdditionalKeyValuePair:: Field[%d/%d]", a+1, rl->igSelectSize);	
            dbg(DEBUG, "constructAdditionalKeyValuePair::      FIELD[%s]", rl->rgSelect[a].FIELD);	
            dbg(DEBUG, "constructAdditionalKeyValuePair::      OPS[%s]", rl->rgSelect[a].OPS);	
            dbg(DEBUG, "constructAdditionalKeyValuePair::      TYPE[%c]", rl->rgSelect[a].TYPE);	
            dbg(DEBUG, "constructAdditionalKeyValuePair::      HEADER[%s]", rl->rgSelect[a].HEADER);	
            dbg(DEBUG, "constructAdditionalKeyValuePair::      VALUE[%s]", rl->rgSelect[a].VALUE);	
            
            memset(pclValue, 0, sizeof(pclValue));
            if ( rl->rgSelect[a].TYPE=='X' )
                ilTmp=getKeyValue(pclValue,  rl->rgSelect[a].HEADER, rl->rgSelect[a].VALUE);
            else if ( rl->rgSelect[a].TYPE=='T' )
            {
                if ( !strcmp(rl->rgSelect[a].HEADER, "AFTTAB") )
                {
                    if ( !strcmp(rl->rgSelect[a].VALUE, "URNO") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.URNO);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "FLNO") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.FLNO);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ADID") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ADID);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "RKEY") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.RKEY);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "STOA") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.STOA);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "STOD") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.STOD);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ACT3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ACT3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ACT5") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ACT5);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ALC2") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ALC2);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ALC3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ALC3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "DES3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.DES3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "DES4") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.DES4);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ORG3") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ORG3);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "ORG4") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.ORG4);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "RTYP") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.RTYP);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "FTYP") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.FTYP);
                    }
                    else if ( !strcmp(rl->rgSelect[a].VALUE, "REGN") )
                    {
                        sprintf(pclValue, "%s", rgAfttab.REGN);
                    }
                }
            }

            if ( ilTmp==RC_SUCCESS )
            {
            	if ( strstr(pcpFields, rl->rgSelect[a].FIELD)==NULL )
            	{
                    if ( strlen(pcpFields) )
                    {
                        sprintf(pcpFields, "%s,%s", pcpFields, rl->rgSelect[a].FIELD);                  	
                        sprintf(pcpValues, "%s,%s", pcpValues, pclValue);                  	
                    }
                    else
                    {
                        strcpy(pcpFields, rl->rgSelect[a].FIELD);
                        strcpy(pcpValues, pclValue); 
                    }      
                }  
                else
                    dbg(DEBUG, "constructAdditionalKeyValuePair:: Skipping field[%s]! Already in string[%s]",   
                        rl->rgSelect[a].FIELD,  pcpFields);     
            }
        }	
    }
    
    dbg(DEBUG, "constructAdditionalKeyValuePair:: Fields[%s] Values[%s]", pcpFields, pcpValues);
    dbg(DEBUG, "constructAdditionalKeyValuePair:: End Result[%d]", ilRC);
    return ilRC;	
}

int checkRecordInDB(char *pcpSql)
{
    int ilRC=RC_SUCCESS;
	char pclData[MAX_XML+1];
	short slFkt = 0;
	short slCursor = 0;
	
	dbg(TRACE, "checkRecordInDB:: Start");
	    
    dbg(TRACE, "checkRecordInDB:: executing[%s]", pcpSql);

    slFkt = START;
    slCursor = 0;
    ilRC = sql_if (slFkt, &slCursor, pcpSql, pclData);
    if (ilRC == RC_SUCCESS)
    {
        dbg(TRACE,"checkRecordInDB:: Record found");
    }
    else if (ilRC == SQL_NOTFOUND)
    {
        dbg(TRACE,"checkRecordInDB:: No record found");
    }
    else
    {
        dbg(TRACE,"checkRecordInDB:: DB ERROR!!! ");
    }

    //commit_work();
    close_my_cursor (&slCursor);
    
    dbg(TRACE, "checkRecordInDB:: End [%d]", ilRC);
    return ilRC;	
}

void printConfig()
{
	int a=0, b=0, c=0;
	
    dbg(TRACE, "printConfig:: Start");
    
    dbg(TRACE, "printConfig:: Mod Name[%s]", mod_name);
    dbg(TRACE, "printConfig:: pcgRemoveNewLine[%s]", pcgRemoveNewLine);
    
    dbg(TRACE, "List Count [%d]", pcgListCount);
    for (a=0; a<pcgListCount; a++)
    {
        dbg(TRACE, "List[%d]", a+1);
        dbg(TRACE, "     List Name[%s]", rgXMLList[a].LIST);
        dbg(TRACE, "     Group Name[%s]", rgXMLList[a].GROUP);
        dbg(TRACE, "     Table Name[%s]", rgXMLList[a].TABLE);
        dbg(TRACE, "     Send Queue[%d]", rgXMLList[a].ilSendQueue);
        dbg(TRACE, "     Delete Command[%s]", rgXMLList[a].DELCMD);
        dbg(TRACE, "     Insert Command[%s]", rgXMLList[a].INSCMD);
        dbg(TRACE, "     Update Command[%s]", rgXMLList[a].UPDCMD);
        
        dbg(TRACE, "     Urno Generator[%s]", rgXMLList[a].URNOGENERATOR);
        dbg(TRACE, "     MODE[%s]", rgXMLList[a].MODE);
        dbg(TRACE, "     FULLSET[%d]", rgXMLList[a].igFullSet);
        if (rgXMLList[a].igFullSet == TRUE)
        {
            dbg(TRACE, "     FULLSET_INCLUDE FLD List[%s]", rgXMLList[a].FullSet_InclFld);
            dbg(TRACE, "     FULLSET_INCLUDE DAT List[%s]", rgXMLList[a].FullSet_InclDat);
            dbg(TRACE, "     FULLSET_EXCLUDE FLD[%s]", rgXMLList[a].FullSet_ExclFld);
            dbg(TRACE, "     FULLSET_EXCLUDE DAT[%s]", rgXMLList[a].FullSet_ExclDat);
        }

        dbg(TRACE, "     Select Group Count[%d]", rgXMLList[a].igSelectGroup);
        for ( b=0; b<rgXMLList[a].igSelectGroup; b++)
        {
            dbg(TRACE, "     Select Group [%d/%d]", b+1, rgXMLList[a].igSelectGroup);
            dbg(TRACE, "          TYPE[%c]", rgXMLList[a].rgSelect[b].TYPE);
            dbg(TRACE, "          HEADER[%s]", rgXMLList[a].rgSelect[b].HEADER);
            dbg(TRACE, "          FIELD[%s]", rgXMLList[a].rgSelect[b].FIELD);
            dbg(TRACE, "          OPS[%s]", rgXMLList[a].rgSelect[b].OPS);
            dbg(TRACE, "          VALUE[%s]", rgXMLList[a].rgSelect[b].VALUE);
            dbg(TRACE, "          ADDITIONAL WHERE[%s]", rgXMLList[a].rgSelect[b].ADDITIONALWHERE);
          	
          	for (c=0;c<rgXMLList[a].rgSelect[b].igSelectSize; c++)
          	{
          	    dbg(TRACE, "          Select Keys[%d/%d]", c+1, rgXMLList[a].rgSelect[b].igSelectSize);	
          	    dbg(TRACE, "               OPS[%s]", rgXMLList[a].rgSelect[b].rgSelect[c].OPS);
          	    dbg(TRACE, "               FIELD[%s]", rgXMLList[a].rgSelect[b].rgSelect[c].FIELD);
          	    dbg(TRACE, "               TYPE[%c]", rgXMLList[a].rgSelect[b].rgSelect[c].TYPE);
          	    dbg(TRACE, "               HEADER[%s]", rgXMLList[a].rgSelect[b].rgSelect[c].HEADER);
          	    dbg(TRACE, "               VALUE[%s]", rgXMLList[a].rgSelect[b].rgSelect[c].VALUE);
          	}
        }
        
    }
    dbg(TRACE, "printConfig:: Start");
    return;    	
}

void printRecords()
{
	int a=0, b=0;
	
    dbg(TRACE, "printRecords:: Start");
    
    for (a=0; a<pcgRecordCount; a++)
    {
        dbg(TRACE, "Record No[%d/%d]",a+1, pcgRecordCount);
        if ( rgXMLRecords[a].rgXMLList!=NULL )
        {
            dbg(TRACE, "     LIST[%s] GROUP[%s] TABLE[%s] FieldCount[%d]", 
                rgXMLRecords[a].rgXMLList->LIST, rgXMLRecords[a].rgXMLList->GROUP, rgXMLRecords[a].rgXMLList->TABLE, rgXMLRecords[a].iSize);
        }
                
        for (b=0; b<rgXMLRecords[a].iSize; b++)
        {      	
                dbg(TRACE, "     Key[%s] Value[%s]", rgXMLRecords[a].MAP[b].key, rgXMLRecords[a].MAP[b].value);    	
        }
    }
    dbg(TRACE, "printRecords:: End");
    return;    	
}

void printInfoGeneric(_INFOGENERIC *pInfoGeneric)
{
    dbg(DEBUG, "printInfoGeneric:: Start");
    
    dbg(DEBUG, "printInfoGeneric:: FLNO[%s]", pInfoGeneric->FLNO);
    dbg(DEBUG, "printInfoGeneric:: ADID[%s]", pInfoGeneric->ADID);
    dbg(DEBUG, "printInfoGeneric:: STDT[%s]", pInfoGeneric->STDT);
    dbg(DEBUG, "printInfoGeneric:: ACTIONTYPE[%s]", pInfoGeneric->ACTIONTYPE);
    
    dbg(DEBUG, "printInfoGeneric:: End");
    return;    	
}

int replaceChar(char *pcpString, char cpOld, char cpNew)
{
    int ilRC=RC_SUCCESS;
    char pclTmp[strlen(pcpString)+1];
    int a, ilSize;
    
    
    dbg(DEBUG, "replaceChar:: Start");
    
    dbg(DEBUG, "replaceChar:: pcpString[%s], cpOld[%c], cpNew[%c]", pcpString, cpOld, cpNew);
    
    memset(&pclTmp, 0, sizeof(pclTmp));
    ilSize=strlen(pcpString);
    for (a=0; a<ilSize; a++)
    {
        if ( pcpString[a]==cpOld )
            pclTmp[a]=cpNew;
        else
            pclTmp[a]=pcpString[a];	
    }
    
    strcpy(pcpString, pclTmp);
    
    dbg(DEBUG, "replaceChar:: New String[%s]", pcpString);    
    dbg(DEBUG, "replaceChar:: End Result[%d]", ilRC);
    
    return ilRC;	
}

int processTowingRecord(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord, char *pcpWhere)
{
    int ilRC=RC_SUCCESS;
    char pclCmd[6];
    char pclFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    char pclFltn[5];
    char pclUrno[16];
    int a=0;
    char pclWhere[MAX_XML+1] = "\0";
    _AFTTAB rlTow;
    
    dbg(DEBUG, "processTowingRecord:: Start");
    ilRC=getAfttabRecord(&rlTow, pcpWhere);
   
    if ( ilRC==RC_FAIL )
    {
       	dbg(TRACE, "processTowingRecord:: ERROR!!! executing checkRecordInDB()");
    }
    else 
    {
    	pclFields[0]=0;
    	pclValues[0]=0;
    	pclCmd[0]=0;
        if ( ilRC==SQL_NOTFOUND )
        {
            strcpy(pclCmd, rpXMLRecord->rgXMLList->INSCMD);
            strcpy(pclWhere, " ");
        	
        	dbg(TRACE, "processTowingRecord:: Getting new urno");
            sprintf(pclUrno, "%ld", NewUrnos(rpXMLRecord->rgXMLList->TABLE,1)); 	
            dbg(TRACE, "processTowingRecord:: New Urno[%s]", pclUrno);
            
            memset(&pclFltn, 0, sizeof(pclFltn));
            pclFltn[0]=prpAft->FLNO[3];
            pclFltn[1]=prpAft->FLNO[4];
            pclFltn[2]=prpAft->FLNO[5];
            pclFltn[3]=prpAft->FLNO[6];
            
            strcpy(pclFields, "URNO,FLNO,FLTN,RKEY,ADID,"
                              "ACT3,ACT5,ALC2,ALC3,DES3,"
                              "DES4,FTYP,ORG3,ORG4,RTYP");
            sprintf(pclValues, "%s,%s,%3.3d,%s,B,"
                               "%s,%s,%s,%s,%s,"
                               " ,T,%s, ,J", 
                    pclUrno,
                    prpAft->FLNO[0]==0?" ":prpAft->FLNO,
                    atoi(pclFltn),
                    prpAft->URNO,
                    prpAft->ACT3[0]==0?" ":prpAft->ACT3,
                    prpAft->ACT5[0]==0?" ":prpAft->ACT5,
                    prpAft->ALC2[0]==0?" ":prpAft->ALC2,
                    prpAft->ALC3[0]==0?" ":prpAft->ALC3,
                    cgHopo,
                    cgHopo
                   );

            /*
            if ( strcmp(prpAft->ADID, "D") == 0)
            {
                sprintf(pclFields, "%s,%s,%s",
                        pclFields, "STOA","STOD");
                sprintf(pclValues, "%s,%s,%s",
                        pclValues, prpAft->STOD, prpAft->STOD);	
            }
            else
            {
                sprintf(pclFields, "%s,%s,%s",
                        pclFields, "STOA","STOD");
                sprintf(pclValues, "%s,%s,%s",
                        pclValues, prpAft->STOA, prpAft->STOA);	
            }                    
            */
        }
        else
        {
            strcpy(pclCmd, rpXMLRecord->rgXMLList->UPDCMD);        	
            sprintf(pclWhere,"WHERE URNO = %s", rlTow.URNO); 
        }
        for (a=0; a< rpXMLRecord->iSize; a++)
        {
        	if ( !strcmp(pclFields, "") )
        	{
                strcpy(pclFields, rpXMLRecord->MAP[a].key);	
                strcpy(pclValues, rpXMLRecord->MAP[a].value);	        		
        	}
        	else
        	{
                sprintf(pclFields, "%s,%s", pclFields, rpXMLRecord->MAP[a].key);	
                sprintf(pclValues, "%s,%s", pclValues, rpXMLRecord->MAP[a].value);	
            }        	
        }
                                                      
        dbg(DEBUG, "processTowingRecord:: pclCmd[%s], pclFields[%s]", pclCmd, pclFields);
        dbg(DEBUG, "processTowingRecord:: pclValues[%s]", pclValues);
     
        ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                                  0,             
                                  mod_name,
                                  mod_name,
                                  pcgTwStart,
                                  pcgTwEnd,     
                                  pclCmd,
                                  rpXMLRecord->rgXMLList->TABLE,
                                  pclWhere,
                                  pclFields,
                                  pclValues,
                                  "",
                                  3,
                                  NETOUT_NO_ACK);

        /* Join Towing flight to Operation flight */
        if (pclCmd[0] == 'I')
        {
            dbg(DEBUG, "processTowingRecord:: Join Towing<%s> to Flight<%s>", pclUrno,prpAft->URNO);
            sprintf(pclWhere, "%s,0,%s,0", prpAft->URNO, pclUrno);
            ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                                  0,             
                                  mod_name,
                                  mod_name,
                                  pcgTwStart,
                                  pcgTwEnd,     
                                  "JOF",
                                  rpXMLRecord->rgXMLList->TABLE,
                                  pclWhere,
                                  "RTYP",
                                  "R",
                                  "",
                                  3,
                                  NETOUT_NO_ACK);
       }
 
    }
    
    dbg(DEBUG, "processTowingRecord:: End Result[%d]", ilRC);
    return ilRC; 	
}



int processTowingRecordOld(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord)
{
    int ilRC=RC_SUCCESS;
    char pclFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    char pclFltn[5];
    char pclUrno[16];
    int a=0;
    
    
    dbg(DEBUG, "processTowingRecord:: Start");
    
    pclFields[0]=0;
    pclValues[0]=0;
    
    sprintf(pclUrno, "%ld", NewUrnos(rpXMLRecord->rgXMLList->TABLE,1)); 	
    
    memset(&pclFltn, 0, sizeof(pclFltn));
    pclFltn[0]=prpAft->FLNO[3];
    pclFltn[1]=prpAft->FLNO[4];
    pclFltn[2]=prpAft->FLNO[5];
    pclFltn[3]=prpAft->FLNO[6];
    
    strcpy(pclFields, "URNO,FLNO,FLTN,RKEY,ADID,"
                      "ACT3,ACT5,ALC2,ALC3,DES3,"
                      "DES4,FTYP,ORG3,ORG4,RTYP");
    sprintf(pclValues, "%s,%s,%3.3d,%s,%s,"
                       "%s,%s,%s,%s,%s,"
                       "%s,T,%s,%s,%s", 
            pclUrno,
            prpAft->FLNO[0]==0?" ":prpAft->FLNO,
            atoi(pclFltn),
            prpAft->RKEY[0]==0?" ":prpAft->RKEY,
            "B",	
            prpAft->ACT3[0]==0?" ":prpAft->ACT3,
            prpAft->ACT5[0]==0?" ":prpAft->ACT5,
            prpAft->ALC2[0]==0?" ":prpAft->ALC2,
            prpAft->ALC3[0]==0?" ":prpAft->ALC3,
            cgHopo,
            prpAft->DES4[0]==0?" ":prpAft->DES4,
            cgHopo,
            prpAft->ORG4[0]==0?" ":prpAft->ORG4,
            prpAft->RTYP[0]==0?" ":prpAft->RTYP
            );

    for (a=0; a< rpXMLRecord->iSize; a++)
    {
        sprintf(pclFields, "%s,%s", pclFields, rpXMLRecord->MAP[a].key);	
        sprintf(pclValues, "%s,%s", pclValues, rpXMLRecord->MAP[a].value);	        	
    }

    if ( strstr(pclFields, "STOA")==NULL )
    {
        sprintf(pclFields, "%s,%s,%s",
                pclFields, "STOA","STOD");
        sprintf(pclValues, "%s,%s,%s",
                pclValues, prpAft->STOA, prpAft->STOA);	
    } 
                                                      
    dbg(DEBUG, "processTowingRecord:: pclFields[%s]", pclFields);
    dbg(DEBUG, "processTowingRecord:: pclValues[%s]", pclValues);
     
    ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                              0,             
                              mod_name,
                              mod_name,
                              pcgTwStart,
                              pcgTwEnd,     
                              rpXMLRecord->rgXMLList->INSCMD,
                              rpXMLRecord->rgXMLList->TABLE,
                              " ",
                              pclFields,
                              pclValues,
                              "",
                              3,
                              NETOUT_NO_ACK);

    
    dbg(DEBUG, "processTowingRecord:: End Result[%d]", ilRC);
    return ilRC; 	
}

int processTowingDelayRecord(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord)
{
	int ilRC=RC_SUCCESS;
    char pclFields[MAX_XML+1];
    char pclValues[MAX_XML+1];
    int a=0;
    char pclSystemDate[15];
    char pclDURN[11];
    char pclDURA[5];
    
    dbg(DEBUG, "processTowingRecord:: Start");
    
    memset(&pclFields, 0, sizeof(pclFields));
    memset(&pclValues, 0, sizeof(pclValues));
    memset(&pclDURN, 0, sizeof(pclDURN));
    for (a=0; a< rpXMLRecord->iSize; a++)
    {
	    if ( !strcmp(rpXMLRecord->MAP[a].key, "DURN") )
	        strcpy(pclDURN, rpXMLRecord->MAP[a].value);    
	    else if ( !strcmp(rpXMLRecord->MAP[a].key, "DURA") )   
	    	strcpy(pclDURA, rpXMLRecord->MAP[a].value);      
	    else
	    {
	        if ( pclFields[0]==0 )
	        {
	            strcpy(pclFields, rpXMLRecord->MAP[a].key);
	            strcpy(pclValues, rpXMLRecord->MAP[a].value);	
	        }
	        else
	        {
	            sprintf(pclFields, "%s,%s", pclFields, rpXMLRecord->MAP[a].key);
	            sprintf(pclValues, "%s,%s", pclValues, rpXMLRecord->MAP[a].value);	
	        }	
	    } 	
    }

    ilRC=getDentabRecord(&rgDentab, pclDURN);
    
    if ( ilRC==RC_FAIL )
        ;
    else if ( ilRC==SQL_NOTFOUND )
        ilRC=RC_FAIL;
    else
    {
    	getSystemDate(pclSystemDate);
    	sprintf(pclFields, "%s,DURN,DURA,FURN,READ"
    	                   ",CDAT,HOPO,URNO,ALC3"
    	                   ",DECA,DECN",
    	        pclFields);

    	sprintf(pclValues, "%s,%s,%04d,%s,%s"
    	                   ",%s,%s,%ld,%s"
    	                   ",%s,%s",
    	        pclValues,rgDentab.URNO, atoi(pclDURA), prpAft->URNO, " ",
    	        pclSystemDate, cgHopo,  NewUrnos(rpXMLRecord->rgXMLList->TABLE,1), prpAft->ALC3,
    	        rgDentab.DECA, rgDentab.DECN);

  
        dbg(DEBUG, "processTowingDelayRecord:: pclFields[%s] Table[%s]", pclFields, rpXMLRecord->rgXMLList->TABLE);
        dbg(DEBUG, "processTowingDelayRecord:: pclValues[%s]", pclValues);		
        
        
        ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                                  0,             
                                  mod_name,
                                  mod_name,
                                  pcgTwStart,
                                  pcgTwEnd,     
                                  rpXMLRecord->rgXMLList->INSCMD,
                                  rpXMLRecord->rgXMLList->TABLE,
                                  "",
                                  pclFields,
                                  pclValues,
                                  "",
                                  3,
                                  NETOUT_NO_ACK);
                                                              
    }
    	    
    dbg(DEBUG, "processTowingDelayRecord:: End Result[%d]", ilRC);
    return ilRC; 		
}

int getSystemDate(char *pcpOut)
{
    int ilRC=RC_SUCCESS;
    time_t tNow=time(NULL);
    struct tm *rlTm=gmtime(&tNow);
    
    sprintf(pcpOut, "%04d%02d%02d%02d%02d%02d", 
            rlTm->tm_year+1900, rlTm->tm_mon+1, rlTm->tm_mday,
            rlTm->tm_hour, rlTm->tm_min, rlTm->tm_sec);
    dbg(DEBUG, "getSystemDate:: Date & Time [%s]", pcpOut);
    return ilRC;	
}

/* ******************************************************************** */
static int ReferTable(_AFTTAB *prpAft, _XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpDat)
{
    int ilRC = RC_SUCCESS;
    int ilGetRc = RC_SUCCESS;
    char pclFunc[] = "ReferTable:";
    char pclRefTab[16] = "\0";
    char pclRefKeyLst[512] = "\0";
    char pclRefKeyDat[1024] = "\0";
    char pclRefGetLst[1024] = "\0";
    char pclRefSetLst[1024] = "\0";
    char pclRefVarLst[1024] = "\0";
    char pclRefVarDat[1024] = "\0";
    int ilRefTabEnable = FALSE;

    int ilFldCnt = 0;
    int ilItmCnt = 0;
    int ilCnt = 0;
    int ilLen = 0;
    short slCursor = 0;
    short slFkt = START;
    char pclSel[2048] = "\0";
    char pclSqlBuf[2048] = "\0";
    char pclDatBuf[4096] = "\0";
    char pclBuf[1024] = "\0";

    char pclRefKeyValLst[1024] = "\0";
    char pclKeyFld[32] = "\0";
    char pclKeyDat[128] = "\0";
    char pclKeyVal[512] = "\0";
    
    ilRefTabEnable = FALSE;
    for (ilItmCnt = 0; ilItmCnt < igGrpSetCnt; ilItmCnt++)
    {
        if ((strcmp(prgGrpSet[ilItmCnt].GrpNam, prpXMLRecord->rgXMLList->GROUP) == 0))
        {
             if (prgGrpSet[ilItmCnt].RefValid == TRUE)
             {
                 ilRefTabEnable = TRUE;
                 strcpy(pclRefTab, prgGrpSet[ilItmCnt].RefTab);
                 strcpy(pclRefKeyLst, prgGrpSet[ilItmCnt].RefKeyLst);
                 strcpy(pclRefKeyDat, prgGrpSet[ilItmCnt].RefKeyDat);
                 strcpy(pclRefGetLst, prgGrpSet[ilItmCnt].RefGetLst);
                 strcpy(pclRefSetLst, prgGrpSet[ilItmCnt].RefSetLst);
                 strcpy(pclRefVarLst, prgGrpSet[ilItmCnt].RefVarLst);
                 strcpy(pclRefVarDat, prgGrpSet[ilItmCnt].RefVarDat);
             }
             break;
        }
    }
    
    if (ilRefTabEnable == TRUE)
    {
        dbg(DEBUG, "%s Start", pclFunc);

        strcpy(pclSel, "WHERE ");
        strcpy(pclRefKeyValLst, "");
        ilFldCnt = field_count(pclRefKeyLst);
        for (ilCnt = 1; ilCnt <= ilFldCnt; ilCnt++)
        {
            ilLen = get_real_item(pclKeyFld, pclRefKeyLst, ilCnt);
            ilLen = get_real_item(pclKeyDat, pclRefKeyDat, ilCnt);
            ilGetRc = GetXmlGrpDat(prpAft, prpXMLRecord, pclKeyDat, pclKeyVal);

            if (ilGetRc == RC_SUCCESS)
            {
                if (ilCnt == 1)
                    strcpy(pclRefKeyValLst, pclKeyVal);
                else
                {
                    strcat(pclRefKeyValLst, ",");
                    strcat(pclRefKeyValLst, pclKeyVal);
                }

                if (strlen(pclSel) > 7)
                    sprintf(pclBuf,"AND ( %s = '%s') ", pclKeyFld, pclKeyVal);
                else
                    sprintf(pclBuf,"( %s = '%s') ", pclKeyFld, pclKeyVal);
                strcat(pclSel, pclBuf);
            }
            else
                break;
        }

        if (ilGetRc == RC_SUCCESS)
            ilGetRc = BaseTblQry (pclRefTab, pclRefKeyLst, pclRefKeyValLst, pclRefGetLst, 
           pclDatBuf, TRUE, TRUE, pclRefVarLst, pclRefVarDat);

        if (ilGetRc == RC_SUCCESS)
        {
            strcat(pcpFld, ",");
            strcat(pcpFld, pclRefSetLst);
            strcat(pcpDat, ",");
            strcat(pcpDat, pclDatBuf);
            dbg(DEBUG,"%s Build New Fld<%s>  Value<%s>",pclFunc, pcpFld, pcpDat);
        }
        else
            dbg(DEBUG,"%s Nothing Found, No Fld Attach", pclFunc);
    }
    return ilRC;
} 

static int GetAFTDat(_AFTTAB *prpAft, char *pcpFld, char *pcpDat)
{
    int ilRC=RC_SUCCESS;
    int ilCnt = 0;
    char pclFunc[] = "GetAFTDat:";
    char pclFld[32] = "\0";
   
    strcpy(pclFld, pcpFld);
    if (strcmp(pclFld, "URNO") == 0)
    {
        strcpy(pcpDat, prpAft->URNO);
    }
    else if (strcmp(pclFld, "FLNO") == 0)
    {
        strcpy(pcpDat, prpAft->FLNO);
    }
    else if (strcmp(pclFld, "ADID") == 0)
    {
        strcpy(pcpDat, prpAft->ADID);
    }
    else if (strcmp(pclFld, "RKEY") == 0)
    {
        strcpy(pcpDat, prpAft->RKEY);
    }
    else if (strcmp(pclFld, "STOA") == 0)
    {
        strcpy(pcpDat, prpAft->STOA);
    }
    else if (strcmp(pclFld, "STOD") == 0)
    {
        strcpy(pcpDat, prpAft->STOD);
    }
    else if (strcmp(pclFld, "ACT3") == 0)
    {
        strcpy(pcpDat, prpAft->ACT3);
    }
    else if (strcmp(pclFld, "ACT5") == 0)
    {
        strcpy(pcpDat, prpAft->ACT5);
    }
    else if (strcmp(pclFld, "ALC2") == 0)
    {
        strcpy(pcpDat, prpAft->ALC2);
    }
    else if (strcmp(pclFld, "ALC3") == 0)
    {
        strcpy(pcpDat, prpAft->ALC3);
    }
    else if (strcmp(pclFld, "DES3") == 0)
    {
        strcpy(pcpDat, prpAft->DES3);
    }
    else if (strcmp(pclFld, "DES4") == 0)
    {
        strcpy(pcpDat, prpAft->DES4);
    }
    else if (strcmp(pclFld, "ORG3") == 0)
    {
        strcpy(pcpDat, prpAft->ORG3);
    }
    else if (strcmp(pclFld, "ORG4") == 0)
    {
        strcpy(pcpDat, prpAft->ORG4);
    }
    else if (strcmp(pclFld, "RTYP") == 0)
    {
        strcpy(pcpDat, prpAft->RTYP);
    }
    else if (strcmp(pclFld, "FTYP") == 0)
    {
        strcpy(pcpDat, prpAft->FTYP);
    }
    else if (strcmp(pclFld, "REGN") == 0)
    {
        strcpy(pcpDat, prpAft->REGN);
    }
    else
    {
        dbg(TRACE, "%s: WARNING! Field <%s> Not Support", pclFunc, pclFld); 
        strcpy(pcpDat, " ");
        ilRC = RC_FAIL;
    }

    return ilRC;
}

/* ******************************************************************** */
/* Get value from AFT/XMLGrp for a Fld name                             */
/* ******************************************************************** */
static int GetXmlGrpDat(_AFTTAB *prpAft,_XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpDat)
{
    int ilRC=RC_SUCCESS;
    int ilCnt = 0;
    int ilGrpCnt = 0;
    char pclFunc[] = "GetXmlGrpDat:";
    char pclFld[32] = "\0";
    char pclGrpNam[128] = "\0";
    char pclFullFld[512] = "\0";
    char *pclGrpNamPtr = NULL;
    char *pclFldPtr = NULL;
  
    _XMLRECORD *rpXMLRecord = NULL;
   
    strcpy(pclFullFld, pcpFld);
    if (strlen(pclFullFld) > 5)
    {
        pclGrpNamPtr = pclFullFld;
        pclFldPtr = strstr(pclFullFld,"."); 
        if (pclFldPtr == NULL)
        {
            dbg(TRACE, "%s: WARNING! Field <%s> Not Found", pclFunc, pclFullFld); 
            strcpy(pcpDat, " ");
            ilRC = RC_FAIL;
        }
        else
        {
            *pclFldPtr = 0x00;
            pclFldPtr ++;
            strcpy(pclGrpNam, pclGrpNamPtr);
            strcpy(pclFld, pclFldPtr);
        
            if (strcmp(pclGrpNam, "AFT") != 0)
            {
                for (ilCnt = 0; ilCnt < pcgRecordCount; ilCnt++)
                {
                    if (strcmp(rgXMLRecords[ilCnt].rgXMLList->GROUP, pclGrpNam) == 0)
                    {
                        rpXMLRecord = &rgXMLRecords[ilCnt];
                        break;
                    }
                }

                if (ilCnt >= pcgRecordCount)
                {
                    dbg(TRACE, "%s: WARNING! Grp<%s> of Field <%s> Not Found", 
                       pclFunc, pclGrpNam, pclFullFld); 
                    strcpy(pcpDat, " ");
                    ilRC = RC_FAIL;
                }
            }
        }
    }
    else
    {
        rpXMLRecord = prpXMLRecord;
        strcpy(pclFld, pclFullFld);
    }

    if ((ilRC == RC_SUCCESS) && (strcmp(pclGrpNam, "AFT") == 0))
    {
        ilRC = GetAFTDat(prpAft, pclFld, pcpDat);
    }
    else if (ilRC == RC_SUCCESS)
    {
        for (ilCnt = 0; ilCnt < rpXMLRecord->iSize; ilCnt++)
        {
            if (strcmp(rpXMLRecord->MAP[ilCnt].key, pclFld) == 0)
            {
                strcpy(pcpDat, rpXMLRecord->MAP[ilCnt].value);
                break;
            }
        }
        if (ilCnt >= rpXMLRecord->iSize)
        {
            dbg(TRACE, "%s: WARNING! Field <%s> Not Found", pclFunc, pcpFld); 
            strcpy(pcpDat, " ");
            ilRC = RC_FAIL;
        }
    }

    if (ilRC == RC_SUCCESS)
        dbg(DEBUG, "%s:Field <%s> Got Value <%s>", pclFunc, pcpFld, pcpDat); 
    return ilRC;
}

/* ******************************************************************** */
/* Usage for BaseTblQry                                                 */
/* pcpTbl: Table name to Query                                          */
/* pcpKeyLst: Search Key List                                           */
/* pcpKeyDat: Search Key Date                                           */
/* pcpQryLst: Qry flied list from the Table                             */
/* pcpQryRst: Qry Result from the Table                                 */
/* ipShmFlg:  Use Share memory searh or not                             */
/* ipOrcFlg:  Use Oracle DB searh or not                                */
/* pcpVarLst: define certain Key field to use specail query             */
/* pcpVarDat: define the certain Key fields specail value               */
/* ******************************************************************** */
static int BaseTblQry (char *pcpTbl, char *pcpKeyLst, char *pcpKeyDat, char *pcpQryLst, char *pcpQryRst, int ipShmFlg, int ipOrcFlg, char *pcpVarLst, char *pcpVarDat)
{
    int ilRc=RC_SUCCESS;
    int ilGetRc=RC_SUCCESS;
    char pclFunc[] = "BaseTblQry:";
    int ilVarCnt = 0;
    int ilFldCnt = 0;
    int ilDatCnt = 0;
    int ilItmCnt = 0;
    int ilCnt = 0;
    int ilVarFunc = FALSE;

    char pclTmpBuf[1024] = "\0";
    char pclFldLst[1024] = "\0";
    char pclDatLst[1024] = "\0";
    char pclFld[128] = "\0";
    char pclVar[128] = "\0";
    char pclDat[128] = "\0";
   
    int ilLen = 0;
    short slCursor = 0;
    short slFkt = START;
    char pclSel[2048] = "\0";
    char pclSqlBuf[2048] = "\0";
    char pclDatBuf[4096] = "\0";
    char *pclPtr = NULL;

    ilVarFunc = FALSE;
    if ((strlen(pcpVarLst) > 0) && (strcmp(pcpVarLst," ") != 0))
    {
        ilVarCnt = field_count(pcpVarLst);
        if (ilVarCnt > 0)
        {
            for (ilCnt = 1; ilCnt <= ilVarCnt; ilCnt++)
            {
                 ilLen = get_real_item(pclFld, pcpVarLst, ilCnt);
                 str_trm_all (pclFld, " ", TRUE);
                 if (strlen(pclFld) < 1)
                     break;
                 if (strstr(pcpKeyLst,pclFld) ==  NULL)
                     break;
            }
            if (ilCnt > ilVarCnt)
                ilVarFunc = TRUE;
        }
    }

    for (ilCnt = 0; ilCnt <= ilVarCnt; ilCnt++)
    {
        if (ilCnt == 0)
            strcpy(pclDatLst, pcpKeyDat);
        else if (ilVarFunc == TRUE)
        {
            ilFldCnt = field_count(pcpKeyLst); 
            ilLen = get_real_item(pclVar, pcpVarLst, ilCnt);
            str_trm_all (pclVar, " ", TRUE);
               
            for (ilItmCnt = 1; ilItmCnt <= ilFldCnt; ilItmCnt++)
            {
                ilLen = get_real_item(pclFld, pcpKeyLst, ilItmCnt);
                str_trm_all (pclFld, " ", TRUE);
                if (strcmp(pclVar, pclFld) == 0)
                    ilLen = get_real_item(pclDat, pcpVarDat, ilCnt);
                else
                    ilLen = get_real_item(pclDat, pcpKeyDat, ilItmCnt);

                str_trm_all (pclDat, " ", TRUE);
                if (strlen(pclDat) == 0)
                    strcpy(pclDat, " ");

                if (ilItmCnt == 1)
                    strcpy(pclDatLst, pclDat);
                else
                {
                    strcat(pclDatLst, ",");
                    strcat(pclDatLst, pclDat);
                }
            }
        }
        else
            break;
        if (ipShmFlg == TRUE)
        {
            dbg(DEBUG,"SYSLIB: TBL <%s> FLD <%s> DAT <%s>", 
                pcpTbl, pcpKeyLst, pclDatLst);
            ilDatCnt = 0;
            ilGetRc = syslibSearchDbData(pcpTbl,pcpKeyLst,pclDatLst,
                pcpQryLst,pclDatBuf,&ilDatCnt,"\n");

            if (ilGetRc == RC_SUCCESS)
            {
                dbg(DEBUG,"FOUND = %d ENTRIES", ilDatCnt);
                if (ilDatCnt > 1)
                {
                    pclPtr = strstr(pclDatBuf, "\n");
                    *pclPtr = 0x00;
                }

                dbg(DEBUG,"GOT RESULT <%s> <%s>", pcpQryLst, pclDatBuf);
                break;
            } 
        }

        if ((ipOrcFlg == TRUE) && ((ipShmFlg == TRUE) && (ilGetRc != RC_SUCCESS)))
        {
            ilFldCnt = field_count(pcpKeyLst); 
            strcpy(pclSel, "");

            for (ilDatCnt = 1; ilDatCnt <= ilFldCnt; ilDatCnt++)
            {
                ilLen = get_real_item(pclFld, pcpKeyLst, ilDatCnt);
                ilLen = get_real_item(pclDat, pclDatLst, ilDatCnt);
                if (strlen(pclDat) == 0)
                    strcpy(pclDat, " ");
                if (ilDatCnt == 1)
                    sprintf(pclTmpBuf, "WHERE (%s = '%s') ", pclFld, pclDat);
                else
                    sprintf(pclTmpBuf, " AND (%s = '%s') ", pclFld, pclDat);
                strcat(pclSel,pclTmpBuf);
            }

            dbg(TRACE,"GET_BASIC USING ORACLE");
            sprintf(pclSqlBuf,"SELECT %s FROM %s %s", pcpQryLst, pcpTbl, pclSel);
            dbg(DEBUG,"<%s>",pclSqlBuf);
            slFkt = START;
            slCursor = 0;
            ilGetRc = sql_if(slFkt, &slCursor, pclSqlBuf, pclDatBuf);
            if (ilGetRc == DB_SUCCESS)
            {
                BuildItemBuffer(pclDatBuf, pcpQryLst, 0, ",");
                dbg(DEBUG,"RESULT <%s> <%s>",pcpQryLst, pclDatBuf);
                close_my_cursor(&slCursor);
                break;
            }
            else
            {
                dbg(DEBUG,"RESULT NOT FOUND RECORD");
                close_my_cursor(&slCursor);
            }
        }
    }
    if (ilGetRc == DB_SUCCESS)
    {
        /*    Trim Fld for space   */
        /*    strcpy(pcpQryRst, pclDatBuf);   */
        ilFldCnt = field_count(pcpQryLst); 
        for (ilCnt = 1; ilCnt <= ilFldCnt; ilCnt++)
        {
            ilLen = get_real_item(pclDat, pclDatBuf, ilCnt);
            str_trm_all (pclDat, " ", TRUE);
            if (strlen(pclDat) < 1)
                strcpy(pclDat, " ");
            if (ilCnt > 1)
                strcat(pcpQryRst, ",");
            strcat(pcpQryRst, pclDat);
        }
    }
    else
        ilRc = RC_FAIL;
    return ilRc;
}

static int PatchFldLst(_AFTTAB *prpAft, _XMLRECORD *prpXMLRecord, char *pcpFld, char *pcpVal, char *pcpGrpNam)
{
    int ilRc=RC_SUCCESS;
    char pclFunc[] = "PatchFldLst";
    int ilLstCnt = 0;
    int ilFldCnt = 0;
    int ilCnt = 0;
    int ilCntPat = 0;
    int ilLen = 0;

    int ilFnd = FALSE;
    int ilChg = FALSE;
    int ilFldChg = FALSE;
    int ilDatChg = FALSE;
    int ilAppdOnly = FALSE;

    int ilPatchEnable = FALSE;
    char pclPctNewLst[1024] = "\0";
    char pclPctOrgLst[1024] = "\0";
    char pclPctDatLst[1024] = "\0";

    char pclKepFld[1024] = "\0";
    char pclKepVal[1024*4] = "\0";
    char pclNewFld[32] = "\0";
    char pclOrgFld[32] = "\0";
    char pclDat[256] = "\0";
    char pclVal[1024] = "\0";
    char pclNewVal[1024] = "\0";
    char pclFld[32] = "\0";

    for (ilCnt = 0; ilCnt < igGrpSetCnt; ilCnt++)
    {
        if ((strcmp(prgGrpSet[ilCnt].GrpNam, pcpGrpNam) == 0))
        {
            ilLstCnt = prgGrpSet[ilCnt].Patch_LstCnt;
            if (ilLstCnt > 0)
            {
                ilPatchEnable = TRUE;
                strcpy(pclPctNewLst, prgGrpSet[ilCnt].Patch_NewLst);
                strcpy(pclPctOrgLst, prgGrpSet[ilCnt].Patch_OrgLst);
                strcpy(pclPctDatLst, prgGrpSet[ilCnt].Patch_DatLst);
            }
            break;
        }
    }
    if (ilPatchEnable == FALSE)
    {
        if (ilCnt >= igGrpSetCnt)
            dbg(DEBUG,"%s, Group<%s> not found", pclFunc, pcpGrpNam);
        return ilRc;
    }

    
    if (strlen(pclPctNewLst) > 0)
         ilFldChg = TRUE;

    if (strlen(pclPctDatLst) > 0)
         ilDatChg = TRUE;

    if ((strlen(pclPctOrgLst) == 0) && (ilFldChg == TRUE) && (ilDatChg == TRUE))
         ilAppdOnly = TRUE;

    ilFldCnt = field_count(pcpFld);
    strcpy(pclKepFld, "");
    strcpy(pclKepVal, "");
    ilChg = FALSE;
    if (ilAppdOnly == FALSE)
    {
        for (ilCnt = 1; ilCnt <= ilFldCnt; ilCnt++)
        {
            ilLen = get_real_item (pclFld, pcpFld, ilCnt);
            ilLen = get_real_item (pclVal, pcpVal, ilCnt);
            str_trm_all (pclFld, " ", TRUE);
            ilFnd = FALSE;
            for (ilCntPat = 1; ilCntPat <= ilLstCnt; ilCntPat++)
            {
                ilLen = get_real_item (pclOrgFld, pclPctOrgLst, ilCntPat);
                str_trm_all (pclOrgFld, " ", TRUE);
                if (strcmp(pclOrgFld, pclFld) == 0)
                {
                 if (ilFldChg == TRUE)
                 {
                     ilLen = get_real_item (pclNewFld, pclPctNewLst, ilCntPat);
                     str_trm_all (pclNewFld, " ", TRUE);
                 }
                 if (ilDatChg == TRUE)
                 {
                     ilLen = get_real_item (pclDat, pclPctDatLst, ilCntPat);
                     str_trm_all (pclDat, " ", TRUE);
                     (void) GetXmlGrpDat(prpAft, prpXMLRecord, pclDat, pclNewVal);
                 }
                 ilFnd = TRUE;
                 ilChg = TRUE;
                }
            }
            if (ilCnt > 1)
            {
                strcat(pclKepFld, ",");
                strcat(pclKepVal, ",");
            }

            if ((ilFldChg == TRUE) && (ilFnd == TRUE))
                strcat(pclKepFld, pclNewFld);
            else
                strcat(pclKepFld, pclFld);

            if ((ilDatChg == TRUE) && (ilFnd == TRUE))
                strcat(pclKepVal, pclNewVal);
            else
                strcat(pclKepVal, pclVal);
        }
    }
    else
    {
        ilChg = TRUE;
        strcpy(pclKepFld, pcpFld);
        strcpy(pclKepVal, pcpVal);
        for (ilCntPat = 1; ilCntPat <= ilLstCnt; ilCntPat++)
        {
            ilLen = get_real_item (pclNewFld, pclPctNewLst, ilCntPat);
            str_trm_all (pclNewFld, " ", TRUE);

            ilLen = get_real_item (pclDat, pclPctDatLst, ilCntPat);
            str_trm_all (pclDat, " ", TRUE);
            (void)GetXmlGrpDat(prpAft, prpXMLRecord, pclDat, pclNewVal);
            if (strlen(pclKepFld) > 0)
            {
                strcat(pclKepFld, ",");
                strcat(pclKepVal, ",");
            }
            
            strcat(pclKepFld, pclNewFld);
            strcat(pclKepVal, pclNewVal);
        }
    }
    if (ilChg == TRUE)
    {
       if (ilFldChg == TRUE)
       {
           dbg(DEBUG,"%s, Org FldLst<%s> New FldLst<%s>", pclFunc, pcpFld, pclKepFld);
           strcpy(pcpFld, pclKepFld);
       }

       if (ilDatChg == TRUE)
       {
           dbg(DEBUG,"%s, Org ValLst<%s> New ValLst<%s>", pclFunc, pcpVal, pclKepVal);
           strcpy(pcpVal, pclKepVal);
       }

    }
    return ilRc;
}

int processJoinFlight(_AFTTAB *prpAft,  _XMLRECORD *rpXMLRecord, char *pcpWhere)
{
    int ilRC=RC_SUCCESS;
    char pclFunc[] = "processJoinFlight:";
    char pclCmd[6] = "\0";
    char pclFltn[5] = "\0";
    char pclUrno[16] = "\0";
    char pclLftUrno[32] = "\0";
    char pclRgtUrno[32] = "\0";
    char pclWhere[MAX_XML+1] = "\0";
    int ilAction = TRUE;
    _AFTTAB BodyAft;

    char pclSpeRkey[32] = "\0";
    int  ilSpeJoin = FALSE;
    
    dbg(DEBUG, "%s: Start", pclFunc);
    ilRC=getAfttabRecord(&BodyAft, pcpWhere);
   
    if ( ilRC==RC_FAIL )
    {
       	dbg(TRACE, ": ERROR!!! executing checkRecordInDB()", pclFunc);
    }
    else 
    {
        if ( ilRC==SQL_NOTFOUND )
        {
        	dbg(TRACE, "%s: Can't found Afttab Record", pclFunc);
        }
        else
        {
            ilAction = TRUE;
            if (strlen(prpAft->ADID) < 1)
            {
               (void) GetXmlGrpDat(prpAft, rpXMLRecord, "INFOBJ_FLIGHT.ADID", prpAft->ADID); 
               (void) GetXmlGrpDat(prpAft, rpXMLRecord, "INFOBJ_FLIGHT.FTYP", prpAft->FTYP); 
               strcpy(prpAft->RKEY, prpAft->URNO); 
            }
            
        	dbg(DEBUG, "%s: Header URNO<%s>, ADID<%s>, FTYP<%s>, RKEY<%s>", pclFunc, prpAft->URNO, prpAft->ADID, prpAft->FTYP, prpAft->RKEY);
        	dbg(DEBUG, "%s: BODY URNO<%s>, ADID<%s>, FTYP<%s>, RKEY<%s>", pclFunc, BodyAft.URNO, BodyAft.ADID, BodyAft.FTYP, BodyAft.RKEY);
            if (strcmp(BodyAft.RKEY, prpAft->RKEY) == 0)
            {
        	    dbg(DEBUG, "%s: Already in the Same rotation", pclFunc);
                ilAction = FALSE;
            }
            else if( ((strcmp(BodyAft.ADID,"A") == 0) && (strcmp(prpAft->ADID, "A") == 0)) 
                  || ((strcmp(BodyAft.ADID,"D") == 0) && (strcmp(prpAft->ADID, "D") == 0)) 
                  || ((strcmp(BodyAft.FTYP,"T") == 0) && (strcmp(prpAft->FTYP, "T") == 0)) )
            {
        	    dbg(DEBUG, "%s: Invalid Join to flight", pclFunc);
                ilAction = FALSE;
            }
            else if (strcmp(prpAft->ADID, "A") == 0)
            {
                strcpy(pclLftUrno, prpAft->URNO);
                strcpy(pclRgtUrno, BodyAft.URNO);
            }
            else if (strcmp(BodyAft.ADID, "A") == 0)
            {
                strcpy(pclLftUrno, BodyAft.URNO);
                strcpy(pclRgtUrno, prpAft->URNO);
            }
            else if (strcmp(prpAft->FTYP,"T") == 0)
            {
                strcpy(pclLftUrno, BodyAft.URNO);
                strcpy(pclRgtUrno, prpAft->URNO);
            }
            else if (strcmp(BodyAft.FTYP,"T") == 0)
            {
                strcpy(pclLftUrno, prpAft->URNO);
                strcpy(pclRgtUrno, BodyAft.URNO);
            }
            else if ((strcmp(BodyAft.ADID, "B") == 0) && (strcmp(BodyAft.FTYP, "T") != 0)
              && (strcmp(BodyAft.URNO, BodyAft.RKEY) == 0) && (strcmp(prpAft->ADID, "D") == 0))
            {
                strcpy(pclLftUrno, prpAft->URNO);
                strcpy(pclRgtUrno, BodyAft.URNO);
                strcpy(pclSpeRkey, prpAft->RKEY);
                ilSpeJoin = TRUE;
            }
            else if ((strcmp(prpAft->ADID, "B") == 0) && (strcmp(prpAft->FTYP, "T") != 0)
               && (strcmp(prpAft->URNO, prpAft->RKEY) == 0)
               && (strcmp(BodyAft.ADID, "D") == 0))
            {
                strcpy(pclLftUrno, BodyAft.URNO);
                strcpy(pclRgtUrno, prpAft->URNO);
                strcpy(pclSpeRkey, BodyAft.RKEY);
                ilSpeJoin = TRUE;
            }
            else
            {
                strcpy(pclLftUrno, prpAft->URNO);
                strcpy(pclRgtUrno, BodyAft.URNO);
            }


            if (ilAction == TRUE)
            {
                /* Join Body flight to Header flight */
                dbg(DEBUG, "%s: Join Flight<%s> to <%s>", pclFunc, pclRgtUrno, pclLftUrno);
                sprintf(pclWhere, "%s,0,%s,0", pclLftUrno, pclRgtUrno);
                ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                  0, mod_name, mod_name, pcgTwStart, pcgTwEnd,     
                  "JOF", "AFTTAB", pclWhere, "RTYP", "R", "", 3, NETOUT_NO_ACK);
                
                if (ilRC == RC_SUCCESS)
                {
                     /* Join B flight to Dep flight, need correct Aurn */
                    if (ilSpeJoin == TRUE)
                    {
                       sprintf(pclWhere,"WHERE URNO = %s", pclRgtUrno);
                       ilRC=sendCedaEventWithLog(rpXMLRecord->rgXMLList->ilSendQueue,
                          0, mod_name, mod_name, pcgTwStart, pcgTwEnd,     
                         "UFR", "AFTTAB", pclWhere, "AURN", pclSpeRkey, "", 3, NETOUT_NO_ACK);
                    }
                }
            }
        }
    }
    
    dbg(DEBUG, "%s: End Result[%d]", pclFunc, ilRC);
    return ilRC; 	
} /* processJoinFlight */


static int ArrRegnStk(_AFTTAB *prpAft, char *pcpFldLst, char *pcpDatLst, int ipSendQ, char *pcpCmd, char *pcpTab)
{
    int ilRC=RC_SUCCESS;
    int ilGetRc=RC_SUCCESS;
    char pclFunc[] = "ArrRegnStk:";
    char pclDbFldLst[] = "URNO,REGN,RKEY,ADID";
    int ilDbFldCnt = 0;

    char pclSqlBuf[1024] = "\0";
    char pclDatBuf[2048] = "\0";

    char pclFields[512] = "\0";
    char pclValues[512] = "\0";
    char pclWhere[512] = "\0";
    int ilItm = 0;
    int ilItmLen = 0;
    
    char pclRegnRcv[32] = "\0";
    char pclRegnDb[32] = "\0";
    char pclUrnoDb[32] = "\0";
    int ilRegnFld = 0;

    short slFkt = 0;
    short slCursor = 0;

    if ((strcmp(prpAft->ADID, "A") == 0) && (pcpCmd[0] == 'U'))
    {
        ilRegnFld = get_item_no (pcpFldLst, "REGN", 5) + 1;
        if (ilRegnFld > 0)
        {
            get_real_item (pclRegnRcv, pcpDatLst, ilRegnFld);
            strcpy(pclRegnDb, prpAft->REGN);
            str_trm_all (pclRegnDb, " ", TRUE);           
            str_trm_all (pclRegnRcv, " ", TRUE);           
            if ((strcmp(pclRegnRcv,pclRegnDb) != 0) && (strlen(pclRegnDb) >  0))
            {
                sprintf(pclSqlBuf, " SELECT %s FROM %s WHERE RKEY = %s AND ADID = 'D'", 
                   pclDbFldLst, pcpTab, prpAft->URNO);
                dbg(DEBUG,"Search for Dep Flight: <%s>", pclSqlBuf);
                ilDbFldCnt = field_count(pclDbFldLst);
                slCursor = 0;
                slFkt = START;
                ilGetRc = sql_if(slFkt, &slCursor, pclSqlBuf,pclDatBuf);
                close_my_cursor(&slCursor);
                if (ilGetRc == DB_SUCCESS) 
                {
                    BuildItemBuffer(pclDatBuf, "", ilDbFldCnt, ",");
                    dbg(DEBUG, "GOT Record:<%s>", pclDatBuf);

                    ilItm = get_item_no(pclDbFldLst,"URNO",5) + 1;
                    ilItmLen = get_real_item(pclUrnoDb, pclDatBuf, ilItm);
                    str_trm_all (pclUrnoDb, " ", TRUE);           

                    ilItm = get_item_no(pclDbFldLst,"REGN",5) + 1;
                    ilItmLen = get_real_item(pclRegnDb, pclDatBuf, ilItm);
                    str_trm_all (pclRegnDb, " ", TRUE);           
 
                    sprintf(pclWhere, "WHERE URNO = '%s' ", pclUrnoDb);
                    strcpy(pclFields, "REGN");
                    strcpy(pclValues, pclRegnDb);

                    ilRC=sendCedaEventWithLog(ipSendQ, 0, mod_name, mod_name,
                         pcgTwStart, pcgTwEnd, pcpCmd, pcpTab,
                         pclWhere, pclFields, pclValues, "", 3, NETOUT_NO_ACK);
                }
           }

        }
    }
    return ilRC; 	
} /* ArrRegnStk  */


static int FullSetCheckWhere(_AFTTAB *prpAft,_XMLRECORD *prpXMLRecord, char *pcpDatLst, char *pcpWhere)
{
    int ilRc = RC_SUCCESS;
    int ilGetRc = RC_SUCCESS;
    char pclFunc[] = "FullSetCheckWhere:";
    char pclDatLst[2048] = "\0";
    char pclWhere[4096] = "\0";
    char pclInclFld[64] = "\0";
    char pclInclDat[128] = "\0";
    char pclInclVal[128] = "\0";
    char pclTmpBuf[512] = "\0";
    int  ilLstCnt = 0;
    int  ilItm = 0;
    int  ilLen = 0;
    _XMLLIST *prlXMLList = NULL;
   
    prlXMLList = prpXMLRecord->rgXMLList;
    strcpy(pclDatLst, pcpDatLst);
    if (prlXMLList->igFullSet == FALSE)
        ilRc = RC_FAIL;

    if (ilRc == RC_SUCCESS)
    {
        ilLstCnt = field_count(prlXMLList->FullSet_InclFld);
        for (ilItm = 1; ilItm <= ilLstCnt; ilItm++)
        {
            strcpy(pclInclVal,"");
            ilLen = get_real_item (pclInclFld, prlXMLList->FullSet_InclFld, ilItm);
            ilLen = get_real_item (pclInclDat, prlXMLList->FullSet_InclDat, ilItm);
            ilGetRc = GetXmlGrpDat(prpAft, prpXMLRecord, pclInclDat, pclInclVal);
            if (ilGetRc == RC_SUCCESS)
            {
                sprintf(pclTmpBuf, " %s = '%s' ", pclInclFld, pclInclVal);
                if (strlen(pclWhere) >  0)
                    strcat(pclWhere, " AND ");
                strcat(pclWhere, pclTmpBuf);
            }
            else
            {
                ilRc = RC_FAIL;
                break;
            }
        }
    }
 
    if (ilRc == RC_SUCCESS)
    {
        if (strlen(pcpDatLst) > 0)
            sprintf(pcpWhere, " WHERE (%s) AND (%s NOT IN (%s) ) ", pclWhere, prlXMLList->FullSet_ExclFld, pcpDatLst);
        else
            sprintf(pcpWhere, " WHERE (%s) ", pclWhere);
    }
    else
        strcpy(pcpWhere,"");


    dbg(DEBUG, "%s: Result<%d> Build String<%s>", pclFunc, ilRc, pcpWhere);
    return ilRc; 	
} /* FullSetCheckWhere */

static int GetElementValue(char *pclDest, char *pclOrigData, char *pclBegin, char *pclEnd)
{
    char *pclTemp = NULL;
    long llSize = 0;
    char clBuffer[200] = "\0";

    pclTemp = (char*)CedaGetKeyItem(pclDest, &llSize, pclOrigData, pclBegin, pclEnd, TRUE);
    if(pclTemp != NULL)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static int basicDataCheck(char *pcpData, char *pcpBasicDataCheck_Field_list, char *pcpBasicDataCheck_Table_list)
{
	int ilRC = 0;
	int ilAdid = FALSE;
	int ilValue = FALSE;
	int ilEleCount = 0;
	int ilNoEleField = 0;
	int ilNoEleTable = 0;
	char *pclFunc = "basicDataCheck";
	char *pclP = NULL;
	char pclAdid[4] = "\0";
	char pclRegn[16] = "\0";
	char pclTmpFieldName[256] = "\0";
	char pclTmp[256] = "\0";
	char pclFieldName[256] = "\0";
    char pclTmpTableName[256] = "\0";
	
	char pclFieldNameOld[256] = "\0";
	char pclFieldNameNew[256] = "\0";
	
	char pclTagStart[256] = "\0";
	char pclTagEnd[256] = "\0";
	
	char pclValue[256] = "\0";
	
	char pclSqlBuf[2560] = "\0";
    char pclSqlData[2560] = "\0";
	/*
	1-get one item from the field list
	2-get the field name if there is no "-", 
	otherwise get the ADID:
		2.1-if ADID='A', then get the part before "-"
		2.2-if ADID='D', then get the part after  "-"
		2.3-if extracted field name contains ":", then replace it using the part after ":"
	3-get one item from the table list
	4-retrieve basic data table
	*/
	
	ilNoEleField = GetNoOfElements(pcpBasicDataCheck_Field_list, ',');
	ilNoEleTable = GetNoOfElements(pcpBasicDataCheck_Table_list, ',');
	
	if (ilNoEleField != ilNoEleTable)
	{
		dbg(TRACE,"%s The number of source and dest list does not euqal <%d>!=<%d>, return", pclFunc, ilNoEleField, ilNoEleTable);
		return RC_FAIL;
	}
	
	for(ilEleCount = 1; ilEleCount <= ilNoEleField; ilEleCount++)
	{
		memset(pclTmpFieldName,0,sizeof(pclTmpFieldName));
		memset(pclTmpTableName,0,sizeof(pclTmpTableName));
		
		memset(pclFieldNameNew,0,sizeof(pclFieldNameNew));
		memset(pclFieldNameOld,0,sizeof(pclFieldNameOld));
		memset(pclFieldName,0,sizeof(pclFieldName));
		
		memset(pclTagStart,0,sizeof(pclTagStart));
		memset(pclTagEnd,0,sizeof(pclTagEnd));
		memset(pclValue,0,sizeof(pclValue));
		memset(pclAdid,0,sizeof(pclAdid));
		
		get_item(ilEleCount, pcpBasicDataCheck_Field_list, pclTmpFieldName, 0, ",", "\0", "\0");
		TrimRight(pclTmpFieldName);
		
		if( strstr(pclTmpFieldName, "-")!=NULL )
		{
			//dbg(DEBUG, "%s 1-Field Name<%s>", pclFunc, pclTmpFieldName);
			ilAdid  = GetElementValue(pclAdid, pcpData, "<ADID>", "</ADID>");
			if( ilAdid == FALSE)
			{
				dbg(TRACE,"%s ADID is not found, return", pclFunc);
				return RC_FAIL;
			}
			else
			{
				//dbg(TRACE,"%s ADID <%c>", pclFunc, pclAdid);
			}
			
			TrimRight(pclAdid);
			switch(pclAdid[0])
			{
				case 'A':
					dbg(TRACE,"%s ADID <A>", pclFunc, pclAdid);
					get_item(1, pclTmpFieldName, pclFieldName, 0, "-", "\0", "\0");
					break;
				case 'D':
					dbg(TRACE,"%s ADID <D>", pclFunc, pclAdid);
					get_item(2, pclTmpFieldName, pclFieldName, 0, "-", "\0", "\0");
					break;
				default:
					dbg(TRACE,"%s ADID <%s> is invalid", pclFunc, pclAdid);
					break;
			}
			TrimRight(pclFieldName);
			dbg(DEBUG, "%s 2-Field Name<%s>", pclFunc, pclFieldName);
		}
		else
		{
			strcpy(pclFieldName,pclTmpFieldName);
		}
		
		if(strstr(pclFieldName,":") != NULL)
		{	
			get_item(1, pclFieldName, pclFieldNameNew, 0, ":", "\0", "\0");
			get_item(2, pclFieldName, pclFieldNameOld, 0, ":", "\0", "\0");
			//TrimRight(pclFieldNameNew);
			//TrimRight(pclFieldNameOld);
			dbg(DEBUG,"Field Name<%s> <%s> <%s>",pclFieldName,pclFieldNameNew,pclFieldNameOld);
		}
		else
		{
			strcpy(pclFieldNameNew,pclFieldName);
			strcpy(pclFieldNameOld,pclFieldName);
		}
		
		sprintf(pclTagStart, "<%s>",pclFieldNameNew);
		sprintf(pclTagEnd,  "</%s>",pclFieldNameNew);
		ilValue  = GetElementValue(pclValue, pcpData, pclTagStart, pclTagEnd);
		
		if( ilValue == FALSE)
		{
			dbg(TRACE,"%s <%s> is not found, continue", pclFunc, pclFieldNameNew);
			continue;
			//return RC_FAIL;
		}
		
		get_item(ilEleCount, pcpBasicDataCheck_Table_list, pclTmpTableName, 0, ",", "\0", "\0");
		TrimRight(pclTmpTableName);

		dbg(DEBUG,"%s <%d> New Field <%s> Old Filed <%s> Value <%s>- Table <%s>",pclFunc, ilEleCount, pclFieldNameNew, pclFieldNameOld, pclValue,pclTmpTableName);
		
		if( strcmp(pclFieldNameNew,"REGN") == 0 )
		{
			strcpy(pclRegn,pclValue);
		}
		
		if(strcmp(pclFieldNameNew,"DECA") == 0)
		{
			ilValue = TRUE;
			pclP = pcpData;
			while(ilValue != FALSE)
			{
				ilValue  = GetElementValue(pclValue, pclP, pclTagStart, pclTagEnd);
				
				if( ilValue == FALSE)
				{
					break;
				}
								
				pclP = strstr(pclP,pclTagEnd);
				if(pclP != NULL && strlen(pclP) >= 2)
				{
					pclP += 1;
				}
			
				sprintf(pclSqlBuf,"SELECT %s FROM %s WHERE %s='%s'", pclFieldNameOld, pclTmpTableName, pclFieldNameOld, pclValue);
				dbg(TRACE, "%s pclSqlBuf<%s>", pclFunc, pclSqlBuf);
				ilRC = RunSQL(pclSqlBuf, pclSqlData);
				if (ilRC != DB_SUCCESS)
				{
					if (ilRC != NOTFOUND)
					{
						dbg(TRACE, "<%s>: Retrieving data - Fails, return", pclFunc);
						return RC_FAIL;
					}
				}

				switch(ilRC)
				{
					case NOTFOUND:
						dbg(TRACE, "<%s> Retrieving data - Not Found\n", pclFunc);
						return NOTFOUND;
						break;
					default:
						TrimRight(pclSqlData);
						dbg(TRACE, "<%s> Retrieving data - Found <%s>\n", pclFunc, pclSqlData);
						ilRC = RC_SUCCESS;
						break;
				}
			}
		}
		else
		{
			if(strcmp(pclFieldNameOld,"OWNE") != 0)
			{
				sprintf(pclSqlBuf,"SELECT %s FROM %s WHERE %s='%s'", pclFieldNameOld, pclTmpTableName, pclFieldNameOld, pclValue);
			}
			else
			{
				sprintf(pclSqlBuf,"SELECT %s FROM %s WHERE %s='%s' AND REGN='%s'", pclFieldNameOld, pclTmpTableName, pclFieldNameOld, pclValue, pclRegn);
			}
			
			dbg(TRACE, "%s pclSqlBuf<%s>", pclFunc, pclSqlBuf);
			ilRC = RunSQL(pclSqlBuf, pclSqlData);
			if (ilRC != DB_SUCCESS)
			{
				if (ilRC != NOTFOUND)
				{
					dbg(TRACE, "<%s>: Retrieving data - Fails, return", pclFunc);
					return RC_FAIL;
				}
			}

			switch(ilRC)
			{
				case NOTFOUND:
					dbg(TRACE, "<%s> Retrieving data - Not Found\n", pclFunc);
					return NOTFOUND;
					break;
				default:
					TrimRight(pclSqlData);
					dbg(TRACE, "<%s> Retrieving data - Found <%s>\n", pclFunc, pclSqlData);
					ilRC = RC_SUCCESS;
					break;
			}
		}
	}
	return ilRC;
}

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
