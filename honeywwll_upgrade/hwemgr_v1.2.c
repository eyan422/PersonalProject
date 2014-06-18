#ifndef _DEF_mks_version
    #define _DEF_mks_version
    #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
    static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Interface/hwemgr.c 1/18/2013 5:05:39 PM Exp won $";
#endif /* _DEF_mks_version */
    


/********************************************************************************
*                      
* Author         : won 
* Date           : 20121218                                                    
* Description    : UFIS-2770 - Honeywell Connection Manager for outgoing data from UFIS 
*                                                                     
* Update history :                                                   
*  20120127 DKA 1.00   First release.
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


/******************************************************************************/
/* Macros */
/******************************************************************************/
#define HWEMGR_QUEUE_ID     1005
/* For record status from hwecon */
#define NOACK -1
#define IDLE 0
#define SENT 1
#define ACK 2

/*   Database Field Size   */
#define MAX_HWETAB_URNO 14
#define MAX_HWETAB_DATA 8192
#define MAX_HWETAB_STAT 8
#define MAX_HWETAB_DATE 14
#define MAX_HWETAB_MGID 5

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
int  debug_level = DEBUG;



/******************************************************************************/
/* Declarations                                                               */
/******************************************************************************/

/* Holds all the parameters defined in cfg file. */
typedef struct
{
    int iHwetabRetention;    
    int iConn1;
    int iConn2;
    int iHwepde;
    int iHweexco;
}CONFIGREC;

typedef struct
{
    char URNO[MAX_HWETAB_URNO+1];
    char DATA[MAX_HWETAB_DATA+1];
    char STAT[MAX_HWETAB_STAT+1];
    char CDAT[MAX_HWETAB_DATE+1];
    char SDAT[MAX_HWETAB_DATE+1];
    char ADAT[MAX_HWETAB_DATE+1];
    char DDAT[MAX_HWETAB_DATE+1];	
    char MGID[MAX_HWETAB_MGID+1];
}HWETAB;


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
static char pcgDestName [64];
static char pcgRecvName [64];
static int igGlNoAck = 0; /* flag for acknowledgement */
static char pcgCedaCmd [64];/* last command */
char cgConnectionMask=0; 
char cgOldConnectionMask=0; 
CONFIGREC rgConfig; /* variable to hold all configuration values */
char pcgCurrentHwetabUrno[32]="";
char pcgNewHwetabUrno[32]="";
int igConn1RecStatus=IDLE; /* hwecon1 record status */
int igConn2RecStatus=IDLE; /* hwecon2 record status */

HWETAB rgHwetabRec; /* placeholder for a record in hwetab */
char *pcgHwetabFields="URNO,DATA,STAT,CDAT,SDAT,ADAT,DDAT,MGID";
int igHwetabFieldCount=8;

/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
int duplicateChar(char *pcpIn, char *pcpOut, char cpDel);
int executeSql(char *pcpSql, char *pcpErr);
int getConfig(char *pcpFileName);
int getRetentionDate(int ipRetentionDays, char *pcpOut);
int getSystemDate(char *pcpOut);
int getNextHwetabRecord(HWETAB *rpHwetabRec);
static int HandleData(void); /* CEDA  event data     */
static void HandleErr(int); /* General errors */
static void HandleQueues(void); 
static void HandleQueErr(int);
static void HandleSignal(int);
static int InitPgm();
int insertHwetab(char *pcpData, char *pcpMGID);
void printConfig();
void printHwetabRecord(HWETAB *rpHwetab);
int processCDB();
int processCONX();
int processDROP();
int processNACK();
int processRACK();
int processRst();
int processSTAT();
int processXMLO(char *pcpData, char *pcpMGID);
static int ReadConfigEntry(char *pcpSection,char *pcpKeyword,char *pcpCfgBuffer, char *pcpDefault);
static int Reset(void);
int sendCedaEventWithLog(int ipRouteId, int ipOrgId, char *pcpDstNam, char *pcpRcvNam,
                  char *pcpStart, char *pcpEnd, char *pcpCommand, char *pcpTabName,
                  char *pcpSelection, char *pcpFields, char *pcpData,
                  char *pcpErrDscr, int ipPrior, int ipRetCode);
int sendDataHweConn(char *pcpUrno, char *pcpData, char *pcpMGID);
static void Terminate(void); 
int updateHwetab(char *pcpWhere, int ipArgc,...);


/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/

MAIN
{
    int ilRC = RC_SUCCESS; /* Return code            */
    int ilCnt = 0;
    char pclSql[1024];
    char pclSqlErr[513];
    
    INITIALIZE; /* General initialization    */
    dbg(TRACE,"MAIN: version <%s>",mks_version);
       
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
    
    ilRC = getConfig(pcgConfigFile);
    printConfig();
    cgConnectionMask=0;
    igConn1RecStatus=igConn2RecStatus=IDLE; 

    dbg(TRACE,"MAIN: initializing OK");
    dbg(TRACE,"==================== Entering main pgm loop  =================");
     
    /* 
    getNextHwetabRecord(&rgHwetabRec);
    printHwetabRecord(&rgHwetabRec);  
    */
    
    strcpy(pclSql, "DELETE FROM hwetab ");
    ilRC=executeSql(pclSql, pclSqlErr);
    dbg(TRACE,"MAIN:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
    
    ilRC = sendCedaEventWithLog(rgConfig.iConn1, 0, pcgDestName, pcgRecvName,
                                pcgTwStart, pcgTwEnd, "CONX", "HWETAB", "",
                                "", "", 
                                "", 3, NETOUT_NO_ACK);
    ilRC = sendCedaEventWithLog(rgConfig.iConn2, 0, pcgDestName, pcgRecvName,
                                pcgTwStart, pcgTwEnd, "CONX", "HWETAB", "",
                                "", "", 
                                "", 3, NETOUT_NO_ACK);
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
    char pclDummy [iMIN_BUF_SIZE];
    char pclDummy2 [iMIN_BUF_SIZE],pclTmpBuf [64];
    int ilCnt = 0 ;
    int ilItem = 0 ;
    int ilLoop = 0 ;
    int jj, kk;

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
    char pclFields [4096], pclValues [4096];
    char *pclOldData = NULL;  
    char pclTable [32];
    char pclFieldTmp[2048];
    char pclValueTmp[2048];
    char *pclFieldPtr;
    char *pclFieldPtr2;
    char *pclValuePtr;
    char *pclValuePtr2;
  
  
    igQueOut = prgEvent->originator;   /* Queue-id des Absenders */
    prlBchd = (BC_HEAD *) ((char *)prgEvent + sizeof(EVENT));
    prlCmdblk = (CMDBLK  *) ((char *)prlBchd->data);

    pclSel = prlCmdblk->data;
    pclFld = pclSel + strlen(pclSel)+1;
    pclData = pclFld + strlen(pclFld)+1;
    strncpy (pclFields, pclFld, 4096);
    strncpy (pclValues, pclData, 4096);

		//Frank 20130102
    //pclOldData= strtok(pclData,"\n");
    //pclOldData= strtok(NULL,"\n");
    dbg(TRACE,"HandleData:: FROM <%d> TBL <%s>",igQueOut,prlCmdblk->obj_name);
  
    strcpy (pclTable,prlCmdblk->obj_name);
    dbg(TRACE,"HandleData:: Cmd <%s> Que (%d) WKS <%s> Usr <%s>",
        prlCmdblk->command, prgEvent->originator, prlBchd->recv_name, prlBchd->dest_name);
    dbg(TRACE,"HandleData:: Prio (%d) TWS <%s> TWE <%s>", prgItem->priority, prlCmdblk->tw_start, prlCmdblk->tw_end);
 
    strcpy (pcgTwStart,prlCmdblk->tw_start);
    strcpy (pcgTwEnd,prlCmdblk->tw_end);
    
    memset(pcgDestName, 0, (sizeof(prlBchd->dest_name) + 1));    
    strncpy (pcgDestName, prlBchd->dest_name, sizeof(prlBchd->dest_name));
    
    memset(pcgRecvName, 0, (sizeof(prlBchd->recv_name) + 1));
    strncpy(pcgRecvName, prlBchd->recv_name, sizeof(prlBchd->recv_name));
    
    dbg(TRACE,"HandleData:: Originator <%d> ", igQueOut);
    dbg(TRACE,"HandleData:: Sel <%s> ", pclSel);
    dbg(TRACE,"HandleData:: Fld <%s> ", pclFld);
    dbg(TRACE,"HandleData:: Dat <%s> ", pclData);
    //if (pclOldData != 0)
      //  dbg(TRACE,"Old <%s> ", pclOldData);

    if (prlBchd->rc == NETOUT_NO_ACK)
    {
        igGlNoAck = TRUE;
        /*   dbg(TRACE,"HandleData:: No Answer Expected (NO_ACK is true)");   */ 
    }
    else
    {
        igGlNoAck = FALSE;
        /*   dbg(TRACE,"HandleData:: Sender Expects Answer (NO_ACK is false)");   */
    }
 
    if (prlBchd->rc != RC_SUCCESS && prlBchd->rc != NETOUT_NO_ACK)
    {
        dbg(TRACE,"HandleData:: Originator %d", igQueOut);
        dbg(TRACE,"HandleData:: Error prlBchd->rc <%d>", prlBchd->rc);
        dbg(TRACE,"HandleData:: Error Description: <%s>", prlBchd->data);
    }
    else
    {
        strcpy (pcgCedaCmd,prlCmdblk->command);
        if ( !strcmp(prlCmdblk->command, "CDB") ) 
        {
        	dbg(TRACE, "----------     HandleData:: CDB Start     ----------");
            ilRC=processCDB();  
          	dbg(TRACE, "----------     HandleData:: CDB End Result[%d]     ----------", ilRC);
        }
        else if ( !strcmp(prlCmdblk->command, "CONX") )
        {
        	dbg(TRACE, "----------     HandleData:: CONX Start     ----------");
            ilRC=processCONX(); 
            dbg(TRACE, "----------     HandleData:: CONX End Result[%d]     ----------", ilRC);            
        }//Frank 20130107
        else if ( !strcmp(prlCmdblk->command, "DROP") || !strcmp(prlCmdblk->command, "INI") )
        {
        	dbg(TRACE, "----------     HandleData:: DROP Start     ----------");
        	ilRC=processDROP();
            dbg(TRACE, "----------     HandleData:: DROP End Result[%d]     ----------", ilRC);          	
        }
        else if ( !strcmp(prlCmdblk->command, "XMLO") )
        {
        	dbg(TRACE, "----------     HandleData:: XMLO Start     ----------");
            
            //Frank
            /*
            if ( strcmp(pclFld, "DATA") )
            {
                dbg(TRACE, "HandleData:: Invalid field name[%s]", pclFld);
                ilRC=RC_FAIL;     
            }    
            else
            */
            {
                ilRC=processXMLO(pclData, pclSel);    
            }
            dbg(TRACE, "----------     HandleData:: XMLO End Result[%d]     ----------", ilRC);
        }
        else if ( !strcmp(prlCmdblk->command, "RACK") )
        {
            dbg(TRACE, "----------     HandleData:: RACK Start     ----------");
            ilRC=processRACK();
            dbg(TRACE, "----------     HandleData:: RACK End[%d]     ----------", ilRC);
        }          
        else if ( !strcmp(prlCmdblk->command, "NACK") )
        {
            dbg(TRACE, "----------     HandleData:: NACK Start     ----------");
            ilRC=processNACK();
            dbg(TRACE, "----------     HandleData:: NACK End[%d]     ----------", ilRC);
        }     
        else if ( !strcmp(prlCmdblk->command, "RST") )
        {
        	dbg(TRACE, "----------     HandleData:: RST Start     ----------");
            ilRC=processRST();
            dbg(TRACE, "----------     HandleData:: RST End Result[%d]     ----------", ilRC);          	
        }
        else if ( !strcmp(prlCmdblk->command, "STAT") )
        {
            dbg(TRACE, "----------     HandleData:: STAT Start     ----------");
            ilRC=processSTAT();
            dbg(TRACE, "----------     HandleData:: STAT End Result[%d]     ----------", ilRC);   	
        }
        else
            dbg(TRACE, "HandleData:: Invalid CMD[%s]", prlCmdblk->command);
            
    }  
    return ilRC;    
} /* end of HandleData */

/*********************************************************************************
printConfig::  For debugging purposes.  To view program's configuration.
*********************************************************************************/
void printConfig()
{
    dbg(TRACE, "printConfig:: Start");
    dbg(TRACE, "printConfig:: Hwetab Retention in Days [%d]", rgConfig.iHwetabRetention);
    dbg(TRACE, "printConfig:: Connection 1 [%d]", rgConfig.iConn1);
    dbg(TRACE, "printConfig:: Connection 2 [%d]", rgConfig.iConn2);
    dbg(TRACE, "printConfig:: HWEPDE[%d]", rgConfig.iHwepde);
    dbg(TRACE, "printConfig:: End");
    return;    
}

/*********************************************************************************
getConfig:  Read configuration file (pcpFileName) and store it in a structure 
            of CONFIGREC (stConfig).
*********************************************************************************/
int getConfig(char *pcpFileName)
{
    int ilRC=RC_SUCCESS;
    char pclTmp[1024];
    
    dbg(TRACE, "getConfig:: Start");
    
    dbg(DEBUG, "getConfig:: Filename[%s]", pcpFileName);
    memset(&rgConfig, 0, sizeof(rgConfig));
    ilRC=ReadConfigEntry("MAIN","HWETAB_RETENTION_DAYS", pclTmp, "5");
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"getConfig:: Error in opening config file");        
    }
    else
    {
        rgConfig.iHwetabRetention=atoi(pclTmp);    
    } 

    ilRC=ReadConfigEntry("MAIN","HWECONN1", pclTmp, "0");
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"getConfig:: Error in opening config file");   
    }
    else
    {
        rgConfig.iConn1=atoi(pclTmp);    
    }

    ilRC=ReadConfigEntry("MAIN","HWECONN2", pclTmp, "0");
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"getConfig:: Error in opening config file");        
    }
    else
    {
        rgConfig.iConn2=atoi(pclTmp);    
    }
 
    ilRC=ReadConfigEntry("MAIN","HWEPDE", pclTmp, "0");
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"getConfig:: Error in opening config file");        
    }
    else
    {
        rgConfig.iHwepde=atoi(pclTmp);    
    }   
    
    ilRC=ReadConfigEntry("MAIN","HWEEXCO", pclTmp, "0");
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"getConfig:: Error in opening config file");        
    }
    else
    {
        rgConfig.iHweexco=atoi(pclTmp);    
    }     
              
    dbg(TRACE, "getConfig:: End Result[%d]", ilRC);
    return ilRC;    
}


/*******************************************************************************
processCDB: Perform actions for CDB commands:  Delete all records whose cdat
            (creation date) is less than system date - retention days.
            Note.  Retention Days can be found in hwemgr.cfg [MAIN] 
            HWETAB_RETENTION_DAYS.  If this variable is not found it is set to
            5.  See getConfig for more details about the config.            
*******************************************************************************/
int processCDB()
{
    int ilRC=RC_SUCCESS;
    char pclDate[16];
    char pclSql[1024];
    char pclSqlErr[513];
    
    dbg(TRACE, "processCDB:: Start");
    
    ilRC=getRetentionDate(rgConfig.iHwetabRetention, pclDate);
    sprintf(pclSql,"DELETE FROM hwetab WHERE cdat<'%s'", pclDate);
    
    ilRC=executeSql(pclSql, pclSqlErr);
    dbg(TRACE,"processCDB:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
    if ( ilRC==RC_FAIL )
    {
        dbg(TRACE,"processCDB:: DB error!");        
    }
    else if ( ilRC==1 )
    {
        dbg(TRACE, "processCDB:: No records deleted for cdat<[%s]", pclDate);    
        ilRC=RC_SUCCESS;
    }
    else
    {
        dbg(TRACE, "processCDB:: Successfully deleted all records whose cdat is < [%s]", pclDate);    
    }
    
    dbg(TRACE, "processCDB:: End Result[%d]", ilRC);      
    return ilRC;    
}

/*******************************************************************************
getRetentionDate::  Gets retention date in yyyymmdd and store it in pcpOut.
                    Formula is:
                    1. Get system date.
                    2. convert ipRetentionDays to number of seconds.
                    3. Subtract item 2 from item 1.        
*******************************************************************************/
int getRetentionDate(int ipRetentionDays, char *pcpOut)
{
    int ilRC=RC_SUCCESS;
    time_t tlNow = time(NULL);
    time_t tlTmp = tlNow - (ipRetentionDays*60*60*24);
    struct tm *ptmlTmp;
    struct tm tmlRetentionDate;
     
    dbg(DEBUG, "getRetentionDate:: Start");
    dbg(DEBUG, "getRetentionDate:: Retention Days [%d]", ipRetentionDays);

    ptmlTmp=gmtime_r(&tlTmp, &tmlRetentionDate);

    if ( ptmlTmp==NULL )
    {
        dbg(TRACE,"getRetentionDate:: RetentionDays[%d]: Error in executing gmtime_r", ipRetentionDays);
        pcpOut[0]=0;     
        ilRC=RC_FAIL;
    }    
    else
    {
        sprintf(pcpOut, "%04d%02d%02d", 
                tmlRetentionDate.tm_year+1900, tmlRetentionDate.tm_mon+1, tmlRetentionDate.tm_mday);    
    }
    
    dbg(DEBUG, "getRetentionDate:: pcpOut[%s]", pcpOut);
    dbg(DEBUG, "getRetentionDate:: End Result[%d]", ilRC);
    return ilRC;    
}


/*******************************************************************************
executeSql:: Pass the parameter pcpSql to Oracle and execute.  If there are
             errors, it will store in pcpErr
*******************************************************************************/
int executeSql(char *pcpSql, char *pcpErr)
{
    int ilRC=RC_SUCCESS;
    short slActCursor;
    char pclData[1024];
    unsigned long ilBufLen = 512;    
    unsigned long ilErrLen = 0;
    short slFkt = START;

    dbg(DEBUG, "executeSql:: Start");
    dbg(DEBUG, "executeSql:: SQL Statement[%s]", pcpSql);
    
    ilRC = sql_if(slFkt, &slActCursor, pcpSql, pclData);
    if ( ilRC==RC_FAIL )
    {
        dbg(DEBUG,"executeSql:: Error in executing [%s][%s]", pcpSql, pcpErr);
        sqlglm(pcpErr,&ilBufLen,&ilErrLen);
        pcpErr[ilErrLen]=0;
    } 
    else
        pcpErr[0]=0;
        
    commit_work();        
    close_my_cursor(&slActCursor);    

    dbg(DEBUG, "executeSql:: End Result[%d]", ilRC);
    return ilRC;    
}


/*******************************************************************************
prcessXMLO:  Process the XMLO command.  
             1. Insert data in hwetab.
             2. Send to hwecon1/hwecon2
*******************************************************************************/
int processXMLO(char *pcpData, char *pcpMGID)
{
    int ilRC=RC_SUCCESS;
    int ilSend=0;
	char pclDate[16];
	char pclWhere[1024];
	char pclMGID[MAX_HWETAB_MGID+1];
    
    dbg(TRACE, "processXMLO:: Start");
    dbg(TRACE, "processXMLO:: Data[%s]", pcpData);
    
    if ( !strcmp(pcpMGID, pclMGID) )
        strcpy(pclMGID, "1");
    else
        strcpy(pclMGID, pcpMGID);
        
    dbg(TRACE, "processXMLO:: PCPMGID[%s], PCLMGID[%s]", pcpMGID, pclMGID);
  
    dbg(TRACE, "processXMLO:: Connection Mask[%d], igConn1RecStatus[%d], igConn2RecStatus[%d]",
        cgConnectionMask, igConn1RecStatus, igConn2RecStatus);

    if ( ilRC==RC_SUCCESS )
    {
        if ( cgConnectionMask==0 )
        {
            dbg(TRACE, "processXMLO:: No connection");	
        }
        else
        {
        	ilRC=insertHwetab(pcpData, pclMGID);
    	    ilRC=sendDataHweConn(pcgNewHwetabUrno, pcpData, pclMGID);          
        }
    }
    
    dbg(TRACE, "processXMLO:: Final: Hwetab Urno[%s], Connection Mask[%d], igConn1RecStatus[%d], igConn2RecStatus[%d]",
        pcgCurrentHwetabUrno, cgConnectionMask, igConn1RecStatus, igConn2RecStatus);
            
    dbg(TRACE, "processXMLO:: End Result[%d]", ilRC);
    return ilRC;    
}

/*******************************************************************************
prcessRST:  Process the RST command.  
             1. Set outstanding records in hwetab to status DROP.
             2. Send to hwecon1/hwecon2
*******************************************************************************/
int processRST()
{
	int ilRC=RC_SUCCESS;
	int ilTmp1;
	int ilTmp2;
	char pclDate[16];
	char pclWhere[1024];
	
    dbg(TRACE, "processRST:: Start");
    
    if ( !strcmp(pcgCurrentHwetabUrno, "") )
        strcpy(pclWhere, "WHERE stat=' ' ");
    else
        sprintf(pclWhere, "WHERE stat=' ' or URNO='%s' ", pcgCurrentHwetabUrno);
        
    getSystemDate(pclDate);
    ilRC=updateHwetab(pclWhere, 4, "stat", "DROP", "ddat", pclDate);

    if ( ilRC!=RC_FAIL )
    {
        cgConnectionMask=0;
        igConn1RecStatus=igConn2RecStatus=IDLE;
    	memset(&pcgCurrentHwetabUrno, 0, sizeof(pcgCurrentHwetabUrno));
    	
    	ilTmp1=sendCedaEventWithLog(rgConfig.iConn1, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "DROP", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);
        
    	ilTmp2 = sendCedaEventWithLog(rgConfig.iConn2, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "DROP", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);
        dbg(TRACE, "processRST:: Sending DROP to conn1[%d] && conn2[%d]", 
            ilTmp1, ilTmp2);
    }  
    
    dbg(TRACE, "processRST:: End Result[%d]", ilRC);
    return ilRC;   	
}


/*******************************************************************************
getSystemDate:  get system date in UTC and format them to yyyymmddhhmiss and 
                store it in pcpOut.
*******************************************************************************/
int getSystemDate(char *pcpOut)
{
    int ilRC=RC_SUCCESS;
    time_t tlNow = time(NULL);
    struct tm *ptmlTmp;
    struct tm tmlResult;
        
    dbg(DEBUG,"getSystemDate:: Start");

    ptmlTmp=gmtime_r(&tlNow, &tmlResult);

    if ( ptmlTmp==NULL )
    {
        dbg(TRACE,"getSystemDate:: Error in executing gmtime_r");
        pcpOut[0]=0;     
        ilRC=RC_FAIL;
    }    
    else
    {
        sprintf(pcpOut, "%04d%02d%02d"
                        "%02d%02d%02d", 
                tmlResult.tm_year+1900, tmlResult.tm_mon+1, tmlResult.tm_mday,
                tmlResult.tm_hour, tmlResult.tm_min, tmlResult.tm_sec);    
    }
    
    dbg(TRACE, "getSystemDate:: System Date[%s]", pcpOut);    
    dbg(DEBUG, "getSystemDate:: End Result[%d]", ilRC);
    return ilRC;        
}



/*******************************************************************************
insertHwetab: Insert a record in hwetab. 
*******************************************************************************/
int insertHwetab(char *pcpData, char *pcpMGID)
{
    int ilRC=RC_SUCCESS;
    char pclTmp[2048];
    char pclDate[16];
    char pclSql[2048];
    char pclSqlErr[513];
    int ilUrno;
    
    dbg(DEBUG, "insertHwetab:: Start");
  
    getSystemDate(pclDate);
    ilUrno=NewUrnos("HWETAB",1);
    
    sprintf(pcgNewHwetabUrno, "%2.2s%2.2s%d",
            &pclDate[4], &pclDate[6], ilUrno);
    
    strcpy(pclTmp, pcpData);        
    ConvertClientStringToDb(pclTmp);
       
        
    sprintf(pclSql, "INSERT INTO hwetab "
                    "(URNO, DATA, STAT, CDAT, SDAT, ADAT, DDAT, MGID) "
                    "VALUES "
                    "('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s')",
            pcgNewHwetabUrno, pclTmp, " ", pclDate, " ", " ", " ", pcpMGID);

    ilRC=executeSql(pclSql, pclSqlErr);
    dbg(TRACE,"insertHwetab:: SQL Result[%d][%s]", ilRC, pclSqlErr);
    if ( ilRC==RC_FAIL )
    {
        dbg(TRACE,"insertHwetab:: DB error!");        
    }
    else
    {
        dbg(TRACE, "insertHwetab:: Record Added Successfully");    
    }
       
    dbg(DEBUG, "insertHwetab:: End Result[%d]", ilRC);
    return ilRC;    
}

/*******************************************************************************
updateHwetab:  Update a record in hwetab using pcpUrno as key.  Arguments must
               be of the form "FieldName 1", "Value 1', "FieldName 2", "Value 2",
               ..., "FieldName N", "Value N"
*******************************************************************************/
int updateHwetab(char *pcpWhere, int ipArgc, ...)
{
    int ilRC=RC_SUCCESS;
    va_list valArgv;  
    int ilCounter; 
    char pclSql[2048];
    char pclSqlErr[513];
     
    dbg(DEBUG, "updateHwetab:: Start");
    dbg(DEBUG, "updateHwetab:: Condition[%s], Fields to be updated [%d]", pcpWhere, ipArgc);
    
    if ( ipArgc<1 )
    {
        dbg(TRACE, "updateHwetab:: No field to update!");    
    }
    else if ( (ipArgc%2)==1 )
    {
        dbg(TRACE, "updateHwetab:: Field/Value pair not match. ");    
        ilRC=RC_FAIL;
    }
    else
    {
        memset(&pclSql, 0, sizeof(pclSql));
        strcpy(pclSql, "UPDATE hwetab SET ");
        va_start(valArgv, ipArgc); 
        /*
        for ( ilCounter = 0; ilCounter< ipArgc; ilCounter++ )
           dbg(DEBUG, "updateHwetab:: Count[%d][%s]", ilCounter, va_arg(valArgv, char *));
        */
        for ( ilCounter = 0; ilCounter< ipArgc; ilCounter+=2 )
        {
            if ( ilCounter )
                strcat(pclSql,", ");

              sprintf(pclSql, "%s %s =", pclSql, va_arg(valArgv, char *));    
               sprintf(pclSql, "%s '%s'", pclSql, va_arg(valArgv, char *));    
        }
        va_end(valArgv);
        
        sprintf(pclSql, "%s %s ", pclSql, pcpWhere);
        
        ilRC=executeSql(pclSql, pclSqlErr);
        dbg(TRACE,"updateHwetab:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
        
        if ( ilRC==RC_FAIL )
        {
            dbg(TRACE,"updateHwetab:: DB error!");        
        }
        else if ( ilRC==1 )
        {
            dbg(TRACE, "updateHwetab:: No Record to update");    
        }
        else
        {
            dbg(TRACE, "updateHwetab:: Record(s) Updated Successfully");    
        }
    }        
    dbg(DEBUG, "updateHwetab:: End Result[%d]", ilRC);
    return ilRC;    
}

/*******************************************************************************
processDROP: Process DROP command.
             1.  Set connection Mask.
             2.  If there is a change from non zero to zero in connection mask,
                 set all records in hwetab with stat=' ' or 'SENT' to drop.
*******************************************************************************/
int processDROP()
{
    int ilRC=RC_SUCCESS;
    char pclSql[1024];
    char pclSqlErr[513];
    char pclWhere[128];
    char pclDate[16];
    
    dbg(TRACE, "processDROP:: Start");
    
    if ( igQueOut==rgConfig.iConn1 )
        cgConnectionMask&=2;
    else if ( igQueOut==rgConfig.iConn2 )
        cgConnectionMask&=1;

		//Wilson 20130103
    dbg(TRACE, "processDROP:: Old Connection Mask[%d] New Connection Mask[%d]", 
        cgOldConnectionMask, cgConnectionMask); 	
    dbg(TRACE, "processDROP:: igConn1RecStatus[%d], igConn2RecStatus[%d]",
        igConn1RecStatus, igConn2RecStatus);
        
    if ( cgOldConnectionMask && !cgConnectionMask )
    {
    	dbg(TRACE, "processDROP::  No connections");
    	strcpy(pclSql, "DELETE FROM hwetab ");
        ilRC=executeSql(pclSql, pclSqlErr);
        dbg(TRACE,"processDROP:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
    
        memset(&pcgCurrentHwetabUrno, 0, sizeof(pcgCurrentHwetabUrno));
        igConn1RecStatus=igConn2RecStatus=IDLE;      	
    }
    
    //Frank
	//if there is only one connection and received the corresponding NOACK from it when the DROP command comes, then hwemgr should gets the next record in hwetab which is similar to processNACK
	// The extreme case is such as only hwecon1 sends NOACK back to hwemgr, meanwhile hwecon2 diconnected, so there is no NOACK from hwecon2 to hwemgr. So, at this moment, when hwecon2 informs hwemgr it is down, hwemgr should follow the same logic in processNOACK
    if ( 
    	     ( cgConnectionMask==1 && igConn1RecStatus==NOACK ) ||
    	     ( cgConnectionMask==2 && igConn2RecStatus==NOACK )
    	 )
    {
    	dbg(TRACE, "processDROP::  No connections");
    	strcpy(pclSql, "DELETE FROM hwetab ");
        ilRC=executeSql(pclSql, pclSqlErr);
        dbg(TRACE,"processDROP:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);

    	ilRC = sendCedaEventWithLog(rgConfig.iHweexco, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "RES", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK); 
        ilRC = sendCedaEventWithLog(rgConfig.iHwepde, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "BAT", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);    

 	
/*    	
        sprintf(pclWhere, "WHERE STAT='SENT' ");
		getSystemDate(pclDate);
		ilRC=updateHwetab(pclWhere, 4, "STAT", "DROP", "DDAT", pclDate);
		    
		if ( ilRC==RC_SUCCESS )
		{
		    igConn1RecStatus=igConn2RecStatus=IDLE;
		    memset(&pcgCurrentHwetabUrno, 0, sizeof(pcgCurrentHwetabUrno));
		    ilRC=getNextHwetabRecord(&rgHwetabRec);
		    printHwetabRecord(&rgHwetabRec);  
		}    
		
		if ( ilRC==RC_SUCCESS )
		{
		    ilRC=sendDataHweConn(rgHwetabRec.URNO, rgHwetabRec.DATA);          
		}
		else if ( ilRC==1 )
		{
		    ilRC=RC_SUCCESS;
		    dbg(TRACE, "processDROP::  No record to process.");	
	    }
*/          
    }
    cgOldConnectionMask=cgConnectionMask;	 
    
    dbg(TRACE, "processDROP:: End Result[%d]", ilRC);
    return ilRC;   
}

/*******************************************************************************
processCONX: Process CONX command.
             1.   Set connection mask
             2.   If there is a change from 0 to non zero in connection mask,
                  send BAT to hwepde.
*******************************************************************************/
int processCONX()
{
    int ilRC=RC_SUCCESS;
    char pclSql[1024];
    char pclSqlErr[513];
    
    dbg(TRACE, "processCONX:: Start");
    
    if ( igQueOut==rgConfig.iConn1 )
        cgConnectionMask|=1;
   	else if ( igQueOut==rgConfig.iConn2 )
        cgConnectionMask|=2;
   
    dbg(TRACE, "processCONX:: Current Connection Mask[%d], New Connection Mask[%d]", 
        cgOldConnectionMask, cgConnectionMask); 
                   
    if ( !cgOldConnectionMask && cgConnectionMask )
    {
    	strcpy(pclSql, "DELETE FROM hwetab ");
        ilRC=executeSql(pclSql, pclSqlErr);
        dbg(TRACE,"processCONX:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
        
    	igConn1RecStatus=igConn2RecStatus=IDLE;
    	
    	ilRC = sendCedaEventWithLog(rgConfig.iHweexco, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "RES", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);
        ilRC = sendCedaEventWithLog(rgConfig.iHwepde, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "BAT", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);
    }
    
    cgOldConnectionMask=cgConnectionMask;	
    
    dbg(TRACE, "processCONX:: End Result[%d]", ilRC);   	
    return ilRC;
}



/*******************************************************************************
processRACK: Process RACK command.
             1.   Set status to ACK in hwetab.
             2.   Get next record.
             3.   Send for processing.
*******************************************************************************/
int processRACK()
{
    int ilRC=RC_SUCCESS;
    char pclDate[16];
    char pclWhere[1024];
    
    dbg(TRACE, "processRACK:: Start");
    
    sprintf(pclWhere, "WHERE STAT='SENT' ");
    getSystemDate(pclDate);
    ilRC=updateHwetab(pclWhere, 4, "STAT", "ACK", "ADAT", pclDate);
    
    if ( ilRC==RC_SUCCESS )
    {
    	igConn1RecStatus=igConn2RecStatus=IDLE;
    	memset(&pcgCurrentHwetabUrno, 0, sizeof(pcgCurrentHwetabUrno));
        ilRC=getNextHwetabRecord(&rgHwetabRec);
        printHwetabRecord(&rgHwetabRec);  
    }    

    if ( ilRC==RC_SUCCESS )
    {
        ilRC=sendDataHweConn(rgHwetabRec.URNO, rgHwetabRec.DATA, rgHwetabRec.MGID);          
    }
    else if ( ilRC==1 )
    {
        ilRC=RC_SUCCESS;
        dbg(TRACE, "processRACK::  No record to process.");	
    }          
     
    dbg(TRACE, "processRACK:: End Result[%d]", ilRC);   	
    return ilRC;
}


/*******************************************************************************
processNACK():   Process the NACK command.
                 1.    Set Connection record status to NOACK.
                 2.    If both connection 1 and 2 has NOACK, then set the record
                       DROP in hwetab
*******************************************************************************/
int processNACK()
{
    int ilRC=RC_SUCCESS;
    char pclDate[16];
    char pclWhere[1024];
    char pclSql[1024];
    char pclSqlErr[513];
        
    dbg(TRACE, "processNACK:: Start");
    
    dbg(TRACE, "processNACK:: Connection Mask[%d] Record Status: Conn1[%d], Conn2[%d]",
        cgConnectionMask, igConn1RecStatus, igConn2RecStatus);
        
    if ( igQueOut==rgConfig.iConn1 )
        igConn1RecStatus=NOACK;	
    else if ( igQueOut==rgConfig.iConn2 )
        igConn2RecStatus=NOACK;	

    dbg(TRACE, "processNACK:: New Record Status: Conn1[%d], Conn2[%d]",
        igConn1RecStatus, igConn2RecStatus);
        
    if (  
           (igConn1RecStatus==NOACK && igConn2RecStatus==NOACK) ||
           (igConn1RecStatus==NOACK && cgConnectionMask==1) ||
           (igConn2RecStatus==NOACK && cgConnectionMask==2) 
       )
    {
    	strcpy(pclSql, "DELETE FROM hwetab ");
        ilRC=executeSql(pclSql, pclSqlErr);
        dbg(TRACE,"processNACK:: executing [%s][%d][%s]", pclSql, ilRC, pclSqlErr);
        
    	igConn1RecStatus=igConn2RecStatus=IDLE;

    	ilRC = sendCedaEventWithLog(rgConfig.iHweexco, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "RES", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);
        ilRC = sendCedaEventWithLog(rgConfig.iHwepde, 0, pcgDestName, pcgRecvName,
                                    pcgTwStart, pcgTwEnd, "BAT", "HWETAB",
                                    "", "", "",
                                    "", 3, NETOUT_NO_ACK);    	
                                    
/*    	
        sprintf(pclWhere, "WHERE STAT='SENT' ");
        getSystemDate(pclDate);
        ilRC=updateHwetab(pclWhere, 4, "STAT", "DROP", "DDAT", pclDate);
        
        if ( ilRC==RC_SUCCESS )
        {
        	igConn1RecStatus=igConn2RecStatus=IDLE;
    	    memset(&pcgCurrentHwetabUrno, 0, sizeof(pcgCurrentHwetabUrno));
            ilRC=getNextHwetabRecord(&rgHwetabRec);
            printHwetabRecord(&rgHwetabRec);  
        }    

        if ( ilRC==RC_SUCCESS )
        {
            ilRC=sendDataHweConn(rgHwetabRec.URNO, rgHwetabRec.DATA, rgHwetabRec.MGID);          
        }
        else if ( ilRC==1 )
	    {
	        ilRC=RC_SUCCESS;
	        dbg(TRACE, "processNACK::  No record to process.");	
	    } 
*/
    }
           
    dbg(TRACE, "processNACK:: End Result[%d]", ilRC);
    return ilRC;	
}


/*******************************************************************************
getNextHwetabRecord:  Get the next record in hwetab.  STAT=' ' order by URNO
*******************************************************************************/
int getNextHwetabRecord(HWETAB *rpHwetabRec)
{
    int ilRC=RC_SUCCESS;
    short slCursor;
    char pclDataBuf[32768];
    char pclSql[1024];

    dbg(TRACE, "getNextHwetabRecord:: Start");

    memset(rpHwetabRec, 0, sizeof(HWETAB));
 
    sprintf(pclSql, "SELECT %s "
                    "FROM HWETAB "
                    "WHERE STAT=' ' "
                    "ORDER BY URNO ",
            pcgHwetabFields);
    
    
    dbg(TRACE, "getNextHwetabRecord:: Executing [%s]", pclSql);
    ilRC = sql_if (START, &slCursor, pclSql, pclDataBuf);    
 		
    if( ilRC==0 )
    {
        BuildItemBuffer (pclDataBuf, pcgHwetabFields, igHwetabFieldCount, ",");
        get_real_item(rpHwetabRec->URNO,pclDataBuf,1); 
        get_real_item(rpHwetabRec->DATA,pclDataBuf,2); 
        ConvertDbStringToClient(rpHwetabRec->DATA);
        get_real_item(rpHwetabRec->STAT,pclDataBuf,3); 
        get_real_item(rpHwetabRec->CDAT,pclDataBuf,4); 
        get_real_item(rpHwetabRec->SDAT,pclDataBuf,5); 
        get_real_item(rpHwetabRec->ADAT,pclDataBuf,6); 
        get_real_item(rpHwetabRec->DDAT,pclDataBuf,7); 
        get_real_item(rpHwetabRec->MGID,pclDataBuf,8); 
    }
    else if ( ilRC==1 )
    {
        dbg(TRACE,"getNextHwetabRecord:: No record found");	
    }
    else
        dbg(TRACE,"getNextHwetabRecord:: DB ERROR");	 

    close_my_cursor(&slCursor);

    dbg(TRACE, "getNextHwetabRecord:: End Result[%d]", ilRC);
    return ilRC;	
}

/*******************************************************************************
printHwetabRecord:  For debugging purposes.  Print hwetab record.
*******************************************************************************/
void printHwetabRecord(HWETAB *rpHwetab)
{
    dbg(DEBUG, "printHwetabRecord:: Start");
    
    dbg(DEBUG, "printHwetabRecord:: HWETAB");
    dbg(DEBUG, "printHwetabRecord::      URNO[%s]", rpHwetab->URNO);
    dbg(DEBUG, "printHwetabRecord::      DATA[%s]", rpHwetab->DATA);
    dbg(DEBUG, "printHwetabRecord::      STAT[%s]", rpHwetab->STAT);
    dbg(DEBUG, "printHwetabRecord::      CDAT[%s]", rpHwetab->CDAT);
    dbg(DEBUG, "printHwetabRecord::      SDAT[%s]", rpHwetab->SDAT);
    dbg(DEBUG, "printHwetabRecord::      ADAT[%s]", rpHwetab->ADAT);
    dbg(DEBUG, "printHwetabRecord::      DDAT[%s]", rpHwetab->DDAT);
    dbg(DEBUG, "printHwetabRecord::      MGID[%s]", rpHwetab->MGID);
    
    dbg(DEBUG, "printHwetabRecord:: End");    
    return;	
}


/*******************************************************************************
sendCedaEvent::  call the sendCedaEvent library.  Parameters were printed for
                 debugging purposes.
*******************************************************************************/
int sendCedaEventWithLog(int ipRouteId, int ipOrgId, char *pcpDstNam, char *pcpRcvNam,
                  char *pcpStart, char *pcpEnd, char *pcpCommand, char *pcpTabName,
                  char *pcpSelection, char *pcpFields, char *pcpData,
                  char *pcpErrDscr, int ipPrior, int ipRetCode)
{
	int ilRC=RC_SUCCESS;
	
	dbg(TRACE, "sendCedaEvent:: Start");
	
    dbg(TRACE, "sendCedaEvent:: ipRouteId[%d], ipOrgId[%d], pcpDstNam[%s], pcpRcvNam[%s]",
        ipRouteId, ipOrgId, pcpDstNam, pcpRcvNam);
    dbg(TRACE, "sendCedaEvent:: pcpStart[%s], pcpEnd[%s], pcpCommand[%s], pcpTabName[%s], pcpSelection[%s]",
        pcpStart, pcpEnd, pcpCommand, pcpTabName, pcpSelection); 
    dbg(TRACE, "sendCedaEvent:: pcpFields[%s], pcpData[%s]",
        pcpFields, pcpData); 
    dbg(TRACE, "sendCedaEvent:: SendCedaEvent: pcpErrDscr[%s], ipPrior[%d], ipRetCode[%d]",
        pcpErrDscr, ipPrior, ipRetCode); 

    ilRC = SendCedaEvent(ipRouteId, ipOrgId, pcpDstNam, pcpRcvNam, 
                         pcpStart, pcpEnd, pcpCommand, pcpTabName, pcpSelection, 
                         pcpFields, pcpData, 
                         pcpErrDscr, ipPrior, ipRetCode);

	dbg(TRACE, "sendCedaEvent:: End Result[%d]", ilRC);
	return ilRC;	
}


/*******************************************************************************
sendDataHweConn:  Update record in hwetab to STAT='SENT'.  
                  Send data to hwecon1 and hwecon2 if connected and idle.
*******************************************************************************/
int sendDataHweConn(char *pcpUrno, char *pcpData, char *pcpMGID)
{
    int ilRC=RC_SUCCESS;
    int ilTmp1;
    int ilTmp2;
    int ilSend=0;
    char pclDate[16];
    char pclWhere[128];
    
    dbg(TRACE, "sendDataHweConn:: Start");
    dbg(TRACE, "sendDataHweConn:: pcpUrno[%s], pcpMGID[%s], pcpData[%s]", pcpUrno, pcpMGID, pcpData);
    dbg(TRACE, "sendDataHweConn:: cgConnectionMask[%d], igConn1RecStatus[%d], igConn2RecStatus[%d]",
        cgConnectionMask, igConn1RecStatus, igConn2RecStatus);
        
    if ( 
    	  ( (cgConnectionMask&1)==1 && igConn1RecStatus==IDLE) || 
    	  ( (cgConnectionMask&2)==2 && igConn2RecStatus==IDLE)
       )
    {
        getSystemDate(pclDate);
        sprintf(pclWhere, "WHERE URNO='%s' ", pcpUrno);
        ilRC=updateHwetab(pclWhere, 4, "stat", "SENT", "sdat", pclDate);     
        
        if ( ilRC==RC_SUCCESS )
        {
   	        ilTmp1 = sendCedaEventWithLog(rgConfig.iConn1, 0, pcgDestName, pcgRecvName,
                                         pcgTwStart, pcgTwEnd, "XMLO", "HWETAB", pcpMGID,
                                         "DATA", pcpData, 
                                         "", 3, NETOUT_NO_ACK);
            ilTmp2 = sendCedaEventWithLog(rgConfig.iConn2, 0, pcgDestName, pcgRecvName,
                                          pcgTwStart, pcgTwEnd, "XMLO", "HWETAB", pcpMGID,
                                          "DATA", pcpData, 
                                          "", 3, NETOUT_NO_ACK);
            dbg(TRACE, "sendDataHweConn:: Sending data to conn1[%d] & conn2[%d]",
                ilTmp1, ilTmp2);
            
            igConn1RecStatus=igConn2RecStatus=SENT;
            strcpy(pcgCurrentHwetabUrno, pcpUrno);     
        }                       
/*    	
        if ( (cgConnectionMask&1)==1 )
        {
            dbg(DEBUG, "sendDataHweConn:: Connection 1 connected...  Sending data");
        	ilRC = sendCedaEventWithLog(rgConfig.iConn1, 0, pcgDestName, pcgRecvName,
                                        pcgTwStart, pcgTwEnd, "XMLO", "HWETAB", "",
                                        "DATA", pcpData, 
                                        "", 3, NETOUT_NO_ACK);
            igConn1RecStatus=SENT;                            
            ilSend=1;
        }       

        if ( (cgConnectionMask&2)==2 )
        {
      	    dbg(DEBUG, "sendDataHweConn:: Connection 2 connected...  Sending data");
       	    ilRC = sendCedaEventWithLog(rgConfig.iConn2, 0, pcgDestName, pcgRecvName,
                                        pcgTwStart, pcgTwEnd, "XMLO", "HWETAB", "",
                                        "DATA", pcpData, 
                                        "", 3, NETOUT_NO_ACK);
            igConn2RecStatus=SENT;                            
            ilSend=1;
        }
            
        if ( ilSend ) 
        {
            getSystemDate(pclDate);
            sprintf(pclWhere, "WHERE URNO='%s' ", pcpUrno);
            ilRC=updateHwetab(pclWhere, 4, "stat", "SENT", "sdat", pclDate);            	
        }
*/
    }	
    
    dbg(TRACE, "sendDataHweConn:: End Result[%d]", ilRC);
    return ilRC;
}


/*******************************************************************************
processSTAT::  Process the command STAT.  This command is use for debugging 
               purposes.  Print out necessary information to know the current 
               status of the system. 
*******************************************************************************/
int processSTAT()
{
    int ilRC=RC_SUCCESS;
    
    dbg(TRACE, "processSTAT:: Start");
    
    dbg(TRACE, "processSTAT:: Connection Mask[%d]", cgConnectionMask);
    dbg(TRACE, "processSTAT:: Connection 1 Record Status[%d], Connection 2 Record Status[%d]",
        igConn1RecStatus, igConn2RecStatus);
    dbg(TRACE, "processSTAT:: Current Hwetab URNO in Process[%s]", pcgCurrentHwetabUrno);
    dbg(TRACE, "processSTAT:: Notes: Connection Mask: No Connection[0], Conn1 connected[1], Conn2 connected[2], Both Conn1 and Conn2 connected[3]");
    dbg(TRACE, "processSTAT:: Record Status: IDLE[%d], SENT[%d], ACK[%d], NOACK[%d]",
        IDLE, SENT, ACK, NOACK);
    
    dbg(TRACE, "processSTAT:: End Result[%d]", ilRC);    
    return ilRC;   	
}

/*******************************************************************************
duplicateChar:: copy pcpIn to pcpOut but duplicating each occurence of cpDel.
                i.e pcpIn="The", cpDel='h', then pcpOut="Thhe"
*******************************************************************************/
int duplicateChar(char *pcpIn, char *pcpOut, char cpDel)
{
	int ilRC=RC_SUCCESS;
	int a=0, b=0;
	int ilSize=strlen(pcpIn);
	
    dbg(DEBUG, "duplicateChar:: Start");
    for (a=0; a<ilSize; a++)
    {
        if ( pcpIn[a]==cpDel )
        {
            pcpOut[b++]=pcpIn[a];		
        }
        
        pcpOut[b++]=pcpIn[a];	
    }
    
    pcpOut[b]=0;
    
    dbg(DEBUG, "duplicateChar:: pcpIn[%s], pcpOut[%s], Delimeter[%c]", 
        pcpIn, pcpOut, cpDel);
    dbg(DEBUG, "duplicateChar:: End Result[%d]", ilRC);
    return ilRC;    	
}
