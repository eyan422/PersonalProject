#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/action.c 1.24 2013/08/22 18:14:21SGT fya Exp  $";
#endif /* _DEF_mks_version */
/******************************************************************************/
/*                                                                            */
/* CCS Program Skeleton                                                       */
/*                                                                            */
/* Author         :                                                           */
/* Date           :                                                           */
/* Description    :                                                           */
/*                                                                            */
/* Update history : JHi 28/02/00  add dynamic configuration functionality     */
/*                  JHi 05/05/00  buffsize in init_action from iMIN_BUF_SIZE  */
/*                                to iMAX_BUF_SIZE because of problems with   */
/*                                to many fieldconditions in Shanghai         */
/*                      MOS 21/08/00  revived datafilters other than NOT          */
/*                                      added BETWEEN datafilter:           */
/*                                      This filter is configured as following: */
/*                                        MORE|002,004,BETWEEN,CKIF,CKIT        */
/*                                    where changes in afttab rows for ckeckin 2-5  */
/*                                      are forwarded to the configured process */
/*                                    PLEASE BE SURE TO HAVE THE FLIGHT.CFG CONFIGURED  */
/*                                WITH CKIF=SEND AND CKIT=SEND!!!        */
/*                      HAG 04.06.00  initialize ilGetRc inside loop function HandleData */
/*                  JWE 23/01/03  added keep_org_twstart variable           */
/*                                to set behavoiur of cmdblk->tw_start manipul. */
/*                  JWE 15.10.03  resolved various PRF's regarding:           */
/*                                                              -change of default sending events back to */
/*                                                              originator to NO*/
/*                                                              -events with empty fieldlist (i.e. DRT) will*/
/*                                                              now also be forwarded if table and command*/
/*                                                              matches*/
/*                                                              -fixed dynmic configuration handling to proceed*/
/*                                                              on error instead of ignoring the rest of all*/
/*                                                              dynamic config entries*/
/*                                                              -change of log-file information & output*/
/*                                                              -checked that old data is really sent if */
/*                                                              it is configured*/
/*                       JIM 20040107  avoid core (alignment error) by copying trigger event */
/*                               contents before accessing                             */
/*                  JWE 02.06.04 start adding CEI functions (UFIS to UFIS if.)*/
/* JIM 20050915: PRF 7771: check of '!' flag deleted '?' flag */
/* MEI 20100525: Add in the functionality to compare time in UTC in CheckDataFiler i.e. TCHU */
/* FYA 20130822: Initialization and extending the size */
/*		 Lable: v1.24  */
/*               UFIS-3983 AOT - "Action" process stop running after got a long Telex message sent out*/
/******************************************************************************/
/* This program is a CCS main program                                         */
/* source-code-control-system version string                                  */

/* be carefule with strftime or similar functions !!!                         */
/******************************************************************************/

#define U_MAIN
#define UGCCS_PRG
#define STH_USE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ugccsma.h"
#include "glbdef.h"
#include "helpful.h"
#include "msgno.h"
#include "quedef.h"
#include "libccstr.h"
#include "cedatime.h"
#include "tools.h"
#include "uevent.h"
#include "action.h"
#include "debugrec.h"

#define INIT 0
#define HANDLE_DATA 1

#define DYNCONFIGNEW 1
#define DYNCONFIGOLD 2

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE *outp       = NULL;*/
int  debug_level = DEBUG;
int  igRuntimeDebugLevel = TRACE;
static unsigned long ulgEventsReceived = 0;
static unsigned long ulgEventsSent = 0;

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer   */
static EVENT *prgEvent     = NULL;                /* The event pointer        */

/******************************************************************************/
/* my global variables                                                        */
/******************************************************************************/
static TMain    rgTM;

/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int  init_que();                          /* Attach to CCS queues */
extern int  que(int,int,int,int,int,char*);      /* CCS queuing routine  */
extern int  send_message(int,int,int,int,char*); /* CCS msg-handler i/f  */
extern void str_trm_all(char *, char *, int);
extern int  SendRemoteShutdown(int);
extern int get_item_no(char*,char*,short);
extern int StrgPutStrg(char*,int*,char*,int,int,char*);
    
/******************************************************************************/
/* Function prototypes                                                        */
/******************************************************************************/
static int  init_action();
static int  Reset(void);                       /* Reset program          */
static void Terminate(UINT);                   /* Terminate program      */
static void HandleSignal(int);                 /* Handles signals        */
static void HandleErr (int);                    /* Handles general errors */
static void HandleQueErr(int);                 /* Handles queuing errors */
static int  HandleData(void);                  /* Handles event data     */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

/******************************************************************************/
/* my function prototypes                                                     */
/******************************************************************************/
static int _ScheduleAction(char *, CMDBLK *, char *, char *, char *, int);
static int MatchCondition(char *, char *, int);
static int CreateValidFile(void);       
static int CheckFields(int, char *, char *);

static void SetNullToBlank(char *pcpData);
static int CheckDataFilter(char *,char *, char *, char *, char *);
static int CheckChangedFields(char *pcpCedaCmd, char *pcpFields, char *,
                char *pcpData, char *pcpOldData, int ipTableNo);

static int CheckDataMapping(char *pcpRule, char *pcpCedaCmd,
                              char *pcpRecFields, char *pcpSndFields,
                              char *pcpRecData, char *pcpSndData);
static void SetFieldValue(char *pcpSndFields,char *pcpSndData,char *pcpSndFld,char *pcpSndDat);

/*Funktions for dynamic configuration*/
static char     *ACTIONGetDataField(char *, UINT, char, int);
static int  SendAnswer(char *);
static int  AddDynamicConfig(ACTIONConfig *, int);
static int  CreateDynamicCfgFile(void);
static int  ReadDynamicCfgFile(void);

static char *pcgFirstMallocPointer = NULL;
static int CheckOriginator(TSection*,char*);
static void ShowConfigState();
static void Build_CEI_TableList(int ipCalledBy);
static int Check_CEI_Tables(int ipSection,char *pcpTable);
static int SendEventToCei(int ipSection, char *pcpCmd,char *pcpObjName, char *pcpTwStart, char *pcpTwEnd,char *pcpSelection, char *pcpFields, char *pcpData, char *pcpOldData);
static char *GetXmlValue(char *pcpResultBuff, long *plpResultSize,char *pcpTextBuff, char *pcpTagStart, char *pcpTagEnd, int bpCopyData);
static void Use_CEI_TableList(char *pcpOrig,char *pcpListOfTables);

/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
  int   ilRC = RC_SUCCESS;  
  int   ilCnt = 0;
  char  pclFileName[512];
    
  INITIALIZE;

  dbg(TRACE,"<MAIN> VERSION: <%s>", mks_version);

  /* for all signals... */
  (void)SetSignals(HandleSignal);
  (void)UnsetSignals();
  


  /* Attach to the CCS queues */
  do
    {
      if ((ilRC = init_que()) != RC_SUCCESS)
    {
      sleep(6); 
      ilCnt++;
    }
    }while((ilCnt < 10) && (ilRC != RC_SUCCESS));

  if (ilRC != RC_SUCCESS)
    {
      dbg(TRACE,"<MAIN> init_que failed!");
      Terminate(30);
    }

  /* transfer binary file to remote machine... */
  pclFileName[0] = 0x00;
  sprintf(pclFileName, "%s/%s", getenv("BIN_PATH"), argv[0]);
  dbg(DEBUG,"<MAIN> TransferFile: <%s>", pclFileName);
  if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
    {
      dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
      set_system_state(HSB_STANDALONE);
    }

  /* transfer configuration file to remote machine... */
  pclFileName[0] = 0x00;
  sprintf(pclFileName, "%s/%s.cfg", getenv("CFG_PATH"), argv[0]);
  dbg(DEBUG,"<MAIN> TransferFile: <%s>", pclFileName);
  if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
    {
      dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
      set_system_state(HSB_STANDALONE);
    }

  /* ask VBL about this!! */
  dbg(DEBUG,"<MAIN> SendRemoteShutdown: %d",  mod_id);
  if ((ilRC = SendRemoteShutdown(mod_id)) != RC_SUCCESS)
    {
      dbg(TRACE,"<MAIN> SendRemoteShutdown(%d) returns: %d", mod_id, ilRC);
      set_system_state(HSB_STANDALONE);
    }

  if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
    {
      dbg(TRACE,"<MAIN> waiting for status switch ...");
      HandleQueues();
    }

  if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
    {
        dbg(TRACE,"========================== INIT START ==========================");
      if ((ilRC = init_action()) != RC_SUCCESS)
    {       
      Terminate(30);
    }   
    } 
  else 
    {
      Terminate(30);
    }

  /* only a message... */
    dbg(TRACE,"========================== INIT END ============================");
  debug_level = igRuntimeDebugLevel;

  /* forever... */
  while(1)
    {
      dbg(TRACE,"========================== START/END ===========================");
      /* get next item */
      ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

      /* set event pointer      */
      prgEvent = (EVENT*)prgItem->text;
    
      /* check returncode */
      if( ilRC == RC_SUCCESS )
    {
      /* Acknowledge the item */
      ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
      if( ilRC != RC_SUCCESS ) 
        {
          HandleQueErr(ilRC);
        }  
            
      switch (prgEvent->command)
        {
        case    HSB_STANDBY:
          ctrl_sta = prgEvent->command;
          send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
          HandleQueues();
          break;    

        case    HSB_COMING_UP:
          ctrl_sta = prgEvent->command;
          send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
          HandleQueues();
          break;    

        case    HSB_ACTIVE:
        case    HSB_STANDALONE:
          ctrl_sta = prgEvent->command;
          send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
          break;    

        case    HSB_ACT_TO_SBY:
          ctrl_sta = prgEvent->command;
          send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
          HandleQueues();
          break;    

        case    HSB_DOWN:
          ctrl_sta = prgEvent->command;
          Terminate(0);
          break;    

        case    SHUTDOWN:
          Terminate(0);
          break;
                        
        case    RESET:
          ilRC = Reset();
          break;
                        
        case    EVENT_DATA:
          if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
        {
          ilRC = HandleData();
        }
          else
        {
          dbg(DEBUG,"<MAIN> wrong hsb status <%d>", ctrl_sta);
          DebugPrintItem(TRACE,prgItem);
          DebugPrintEvent(TRACE,prgEvent);
        }
          break;
                        
        case TRACE_ON:
          dbg_handle_debug(prgEvent->command);
          break;

        case TRACE_OFF:
          dbg_handle_debug(prgEvent->command);
          break;

        default:
          dbg(DEBUG,"<MAIN> unknown event");
          DebugPrintItem(TRACE,prgItem);
          DebugPrintEvent(TRACE,prgEvent);
          break;
        } 
            
      /* Handle error conditions */
      if (ilRC != RC_SUCCESS)
        {
          HandleErr(ilRC);
        } 
    } 
      else 
    {
      HandleQueErr(ilRC);
    } 
    }
}


/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int init_action()
{
  int       ilRC;
  int       ilTabNo;
  int       ilTmpCmdNo;
  int       ilTmpTabNo;
  UINT      ilGlobalCommand;
  UINT      ilCurrentSection;
  UINT      ilCurrentKey;
  UINT      ilCurrentField;
  UINT      ilCurrentSectionCommand;
  char      *pclCfgPath;
  char      *pclS;
  char      pclCfgFile[iMAX_BUF_SIZE];
  char      pclTmpBuf[iMAX_BUF_SIZE];
  char      pclTabType[iMAX_BUF_SIZE];
  char      pclSection[iMAX_BUF_SIZE];
  char      pclCurrentSection[iMAX_BUF_SIZE];
  char      pclKeys[iMAXIMUM];
  char      pclFields[iMAXIMUM];
  char      pclSectionCommands[iMAX_BUF_SIZE];
  char      pclCondition[iMAX_BUF_SIZE];
  char      pclCurrentCondition[iMAX_BUF_SIZE];
  FILE      *pfFh = NULL;
     
  /* read cfg path from environment */
  if ((pclCfgPath = getenv("CFG_PATH")) == NULL)
    {
      dbg(TRACE,"<init_action> cannot read environment for CFG_PATH");
      Terminate(30);
    }
  else
    {
      /* add filename to path */
      memset((void*)pclCfgFile, 0x00, iMAX_BUF_SIZE);
      strcpy(pclCfgFile, pclCfgPath);
      if (pclCfgFile[strlen(pclCfgFile)-1] != '/')
                strcat(pclCfgFile, "/");
      strcat(pclCfgFile, "action.cfg");

      /* read cfg-file */
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "RUNTIME_MODE",
                  CFG_STRING,pclTmpBuf)) == RC_SUCCESS)
            {
                            dbg(DEBUG,"<init_action> RUNTIME DBG-MODE = <%s>",pclTmpBuf);
                            if(strcmp(pclTmpBuf,"TRACE")==0)
                            {
                                    igRuntimeDebugLevel = TRACE;
                                    dbg(TRACE,"<init_action> RUNTIME DBG-MODE = <TRACE>");
                            }
                            else if(strcmp(pclTmpBuf,"DEBUG")==0) {
                                    igRuntimeDebugLevel = DEBUG;
                                    dbg(TRACE,"<init_action> RUNTIME DBG-MODE = <DEBUG>");
                            } /* end else if */
                            else if(strcmp(pclTmpBuf,"OFF")==0) { 
                                igRuntimeDebugLevel = 0;
                                    dbg(TRACE,"<init_action> RUNTIME DBG-MODE = <OFF>");
                            } /* end else */
            }
            else
            {
                igRuntimeDebugLevel = TRACE;
                dbg(TRACE,"<init_action> RUNTIME DBG-MODE = <TRACE> - default");
            }

      /* first global commands... */
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "global_commands",
                  CFG_STRING,pclTmpBuf)) != RC_SUCCESS)
    {
      rgTM.iNoOfGlobalCommands = 0;
    }
      else
    {
      /* convert to uppercase */
      StringUPR((UCHAR*)pclTmpBuf);
            
      /* calculate number of global commands... */
      rgTM.iNoOfGlobalCommands = GetNoOfElements(pclTmpBuf, ',');

      /* get memory for commands */
      if ((rgTM.pcGlobalCommands = (char**)malloc(rgTM.iNoOfGlobalCommands*sizeof(char*))) == NULL)
        {
                /* can't work without memory */
          dbg(TRACE,"<init_action> not enough memory to run (1)");
          rgTM.pcGlobalCommands = NULL;
          Terminate(30);
        }

      /* memory is OK, read all commands... */
      for (ilGlobalCommand=0; 
           ilGlobalCommand<rgTM.iNoOfGlobalCommands; 
           ilGlobalCommand++)
        {
          if ((pclS = GetDataField(pclTmpBuf, ilGlobalCommand, ',')) == NULL)
        {
          dbg(TRACE,"<init_action> can't read global command number %d in section MAIN", ilGlobalCommand);
          Terminate(30);
        }

                /* store command */
          if ((rgTM.pcGlobalCommands[ilGlobalCommand] = strdup(pclS)) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory to run (2)");
          rgTM.pcGlobalCommands[ilGlobalCommand] = NULL;
          Terminate(30);
        }
        }
    }

      /* all uesd sections */
      memset((void*)pclSection, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "section",
                  CFG_STRING, pclSection)) != RC_SUCCESS)
    {
      dbg(TRACE,"<init_action> missing section in MAIN");
      Terminate(30);
    }

      /* convert to uppercase */
      StringUPR((UCHAR*)pclSection);

      /* get number of currently used sections... */
      rgTM.iNoOfSections = GetNoOfElements(pclSection, ',');

      /* memory for section */
      if ((rgTM.prSection = (TSection*)malloc(rgTM.iNoOfSections*sizeof(TSection))) == NULL)
    {
      dbg(TRACE,"<init_action> can't run without memory (3)");
      rgTM.prSection = NULL;
      Terminate(30);
    }


        /********************************************/
        /* start reading all section configurations */
        /********************************************/


      /* for all sections... */
      for (ilCurrentSection=0;
       ilCurrentSection<rgTM.iNoOfSections;
       ilCurrentSection++)
    {
      /* set valid flag to VALID..... */
      rgTM.prSection[ilCurrentSection].iValidSection = iVALID;
      rgTM.prSection[ilCurrentSection].iSectionType  = iSTATIC_SECTION;

      /* clear memory */
      if ((pclS = GetDataField(pclSection, ilCurrentSection, ',')) == NULL)
        {
          dbg(TRACE,"<init_action> can't read current section number %d in section MAIN", ilCurrentSection);
          Terminate(30);
        }
                
      /* store current section */
      memset((void*)pclCurrentSection, 0x00, iMAX_BUF_SIZE);
      strcpy(pclCurrentSection, pclS);

      /* store section name in structure... */
      strcpy(rgTM.prSection[ilCurrentSection].pcSectionName, pclCurrentSection);

        /*******************/
        /* CEI-stuff start */
        /*******************/
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "TYPE", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcInternalType)) != RC_SUCCESS)
      {
            /* default is usual section (means = 0) */
         rgTM.prSection[ilCurrentSection].cei_iInternalType = 0;
            dbg(TRACE,"<init_action>-------------- READ NORMAL SECTION <%s> ----------------",rgTM.prSection[ilCurrentSection].pcSectionName);
         dbg(DEBUG,"<init_action> Set CEI-TYPE to default <0>!");
      }
      else
      {
            /**********************************************************/
            /* Read special CEI parameters                            */
            /**********************************************************/
        StringUPR((UCHAR*)rgTM.prSection[ilCurrentSection].cei_pcInternalType);

            if (strcmp(rgTM.prSection[ilCurrentSection].cei_pcInternalType,"CEI")==0)
                rgTM.prSection[ilCurrentSection].cei_iInternalType=1;
            else
                rgTM.prSection[ilCurrentSection].cei_iInternalType=0;

            if (rgTM.prSection[ilCurrentSection].cei_iInternalType==1)
            {
                dbg(TRACE,"<init_action>-------------- READ SPECIAL <CEI>-SECTION <%s> ----------------",rgTM.prSection[ilCurrentSection].pcSectionName);
                dbg(TRACE,"<init_action> TYPE=<%s>",rgTM.prSection[ilCurrentSection].cei_pcInternalType);
                /* CEI-server or client  ? */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "TASK", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcTask)) != RC_SUCCESS)
                {
                        rgTM.prSection[ilCurrentSection].cei_iTask=CEIUNK;
                }
                else
                {
                    StringUPR((UCHAR*)rgTM.prSection[ilCurrentSection].cei_pcTask);

                    if (strcmp(rgTM.prSection[ilCurrentSection].cei_pcTask,"SRV")==0)
                        rgTM.prSection[ilCurrentSection].cei_iTask=CEISRV;
                    else if (strcmp(rgTM.prSection[ilCurrentSection].cei_pcTask,"CLI")==0)
                        rgTM.prSection[ilCurrentSection].cei_iTask=CEICLI;
                    else
                        rgTM.prSection[ilCurrentSection].cei_iTask=CEIUNK;
                }
                dbg(TRACE,"<init_action> TASK=<%s>",rgTM.prSection[ilCurrentSection].cei_pcTask);
                /* CEI-Identifier */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ID", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcId)) != RC_SUCCESS)
                {
                    rgTM.prSection[ilCurrentSection].cei_pcId[0] = 0x00;
                }
                dbg(TRACE,"<init_action> ID=<%s>",rgTM.prSection[ilCurrentSection].cei_pcId);
                /* CEI-receiving modid */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "RQ", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcRcvModId)) != RC_SUCCESS)
                {
                    rgTM.prSection[ilCurrentSection].cei_iRcvModId = 0;
                }
                else
                {
                    rgTM.prSection[ilCurrentSection].cei_iRcvModId = atoi(rgTM.prSection[ilCurrentSection].cei_pcRcvModId);
                }
                dbg(TRACE,"<init_action> RQ=<%d>",rgTM.prSection[ilCurrentSection].cei_iRcvModId);
                /* CEI-sending modid */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "SQ", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcSndModId)) != RC_SUCCESS)
                {
                    rgTM.prSection[ilCurrentSection].cei_iSndModId = 0;
                    rgTM.prSection[ilCurrentSection].iModID = 0;
                }
                else
                {
                    rgTM.prSection[ilCurrentSection].cei_iSndModId = atoi(rgTM.prSection[ilCurrentSection].cei_pcSndModId);
                    /* schedule action uses this variable to send events, so we better fill it */
                    rgTM.prSection[ilCurrentSection].iModID = atoi(rgTM.prSection[ilCurrentSection].cei_pcSndModId);
                }
                dbg(TRACE,"<init_action> SQ=<%d>",rgTM.prSection[ilCurrentSection].cei_iSndModId);

                /* CEI-deny tables */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "DENY_TABLES", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcDenyTab)) != RC_SUCCESS)
                {
                 *rgTM.prSection[ilCurrentSection].cei_pcDenyTab = 0x00;
                }
                dbg(TRACE,"<init_action> DENY_TABLES=<%s>",rgTM.prSection[ilCurrentSection].cei_pcDenyTab);

                /* CEI-Patch orginitator */
                if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "PATCH_EVENT_ORIGINATOR", CFG_STRING, rgTM.prSection[ilCurrentSection].cei_pcEventOriginator)) != RC_SUCCESS)
                {
                  rgTM.prSection[ilCurrentSection].cei_iEventOriginator = mod_id;
                }
                else
                {
                  rgTM.prSection[ilCurrentSection].cei_iEventOriginator = atoi(rgTM.prSection[ilCurrentSection].cei_pcEventOriginator);
                }
                dbg(TRACE,"<init_action> PATCH_EVENT_ORIGINATOR=<%s>",rgTM.prSection[ilCurrentSection].cei_pcEventOriginator);
            }
      }
        /*******************/
        /* CEI-stuff end */
        /*******************/

        /* this is a usual, static, section */
        if (rgTM.prSection[ilCurrentSection].cei_iInternalType==0)
        {
      /* -------------------------------------------------------------- */
      /* -------------------------------------------------------------- */
      /* -------------------------------------------------------------- */
      /* ignore data field                          */
      rgTM.prSection[ilCurrentSection].pcIgnoreDataFields[0] = 0x00;
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "field_list", CFG_STRING, rgTM.prSection[ilCurrentSection].pcIgnoreDataFields)) != RC_SUCCESS)
        {
                /* there is no field-list */
          rgTM.prSection[ilCurrentSection].iNoOfIgnoreDataFields = 0;
        }
      else
        {
                /* count current no... */
          rgTM.prSection[ilCurrentSection].iNoOfIgnoreDataFields = GetNoOfElements(rgTM.prSection[ilCurrentSection].pcIgnoreDataFields, ',');

                /* convert to uppercase */
          StringUPR((UCHAR*)rgTM.prSection[ilCurrentSection].pcIgnoreDataFields);
        }

      dbg(DEBUG,"<init_action> found %d ignore fields", rgTM.prSection[ilCurrentSection].iNoOfIgnoreDataFields);
      dbg(DEBUG,"<init_action> found ignore fields <%s>", rgTM.prSection[ilCurrentSection].pcIgnoreDataFields);

      /* should we ignore empty data fields in this section? */
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ignore_empty_fields", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
        {
                /* set defaults... */
                /* we ignore empty fields (default) */
          rgTM.prSection[ilCurrentSection].iIgnoreEmptyFields = 0;  
        }
      else
        {
                /* convert to uppercase */
          StringUPR((UCHAR*)pclTmpBuf);

                /* check it */
          if (0==strcmp(pclTmpBuf,"YES"))
        rgTM.prSection[ilCurrentSection].iIgnoreEmptyFields = 1;    
          else if (0==strcmp(pclTmpBuf,"NO"))
        rgTM.prSection[ilCurrentSection].iIgnoreEmptyFields = 0;    
          else
        {
          dbg(TRACE,"<init_action> unknown entry (ignore_empty_fields), use default (NO)");
          rgTM.prSection[ilCurrentSection].iIgnoreEmptyFields = 0;  
        }
        }
      /*send old data ?*/
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "snd_old_data", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
        {
                /* set defaults... */
                /* we don't send old data (default) */
          rgTM.prSection[ilCurrentSection].iSendOldData = FALSE;    
        }
      else
        {
                /* convert to uppercase */
          StringUPR((UCHAR*)pclTmpBuf);

                /* check it */
          if (0==strcmp(pclTmpBuf,"YES"))
        rgTM.prSection[ilCurrentSection].iSendOldData = TRUE;   
          else if (0==strcmp(pclTmpBuf,"NO"))
        rgTM.prSection[ilCurrentSection].iSendOldData = FALSE;  
          else
        {
          dbg(TRACE,"<init_action> unknown entry (snd_old_data), use default (NO)");
          rgTM.prSection[ilCurrentSection].iSendOldData = 0;    
        }
        }


            /* suspend events which would be forwarded to the originator ? default = yes;  means events are NOT forwarded */
            if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "suspend_own_id", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
            /* set defaults... */
          rgTM.prSection[ilCurrentSection].iSuspendOwn = TRUE;
        }
        else
        {
          /* convert to uppercase */
          StringUPR((UCHAR*)pclTmpBuf);
        /* check it */
        if (0==strcmp(pclTmpBuf,"YES"))
            rgTM.prSection[ilCurrentSection].iSuspendOwn = TRUE;
        else if (0==strcmp(pclTmpBuf,"NO"))
            rgTM.prSection[ilCurrentSection].iSuspendOwn = FALSE;
        else
        {
          dbg(TRACE,"<init_action> unknown entry (suspend_own_id), using default (YES)");
          rgTM.prSection[ilCurrentSection].iSuspendOwn = TRUE;
        }
      }

      /* table number for tw_start... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "snd_cmd", CFG_STRING, rgTM.prSection[ilCurrentSection].pcSndCmd)) != RC_SUCCESS)
        {
          /* set defaults... */
          rgTM.prSection[ilCurrentSection].iUseSndCmd = 0;
        }
      else
        {
          rgTM.prSection[ilCurrentSection].iUseSndCmd = 1;
          dbg(DEBUG,"<init_action> SndCmd: <%s>", rgTM.prSection[ilCurrentSection].pcSndCmd);
        }
      /* shall action leave tw_start untouched (yes/no) ; default=no (meaning it will be manipulated by ACTION */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "keep_org_twstart", CFG_STRING, rgTM.prSection[ilCurrentSection].pcKeepOrgTwstart)) != RC_SUCCESS)
      {
         /* set defaults... */
         rgTM.prSection[ilCurrentSection].iKeepOrgTwstart = FALSE;
         dbg(DEBUG,"<init_action> default-keep_org_twstart: <FALSE>=manipulating tw_start-entry");
      }
      else
      {
         dbg(TRACE,"<init_action> conf.-keep_org_twstart: <%s>",rgTM.prSection[ilCurrentSection].pcKeepOrgTwstart);
         if (strncmp(rgTM.prSection[ilCurrentSection].pcKeepOrgTwstart,"Y",1) == 0 ||
             strncmp(rgTM.prSection[ilCurrentSection].pcKeepOrgTwstart,"y",1) == 0)
         {
            rgTM.prSection[ilCurrentSection].iKeepOrgTwstart = TRUE;
            dbg(DEBUG,"<init_action> keep_org_twstart: <TRUE>=NOT manipulating tw_start-entry");
         }
         else
         {
            rgTM.prSection[ilCurrentSection].iKeepOrgTwstart = FALSE;
            dbg(DEBUG,"<init_action> keep_org_twstart: <FALSE>=manipulating tw_start-entry");
         }      
      }
      /* shall action clear tw_start (yes/no) ; default=no */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "clear_twstart", CFG_STRING, rgTM.prSection[ilCurrentSection].pcClearTwstart)) != RC_SUCCESS)
      {
         /* set defaults... */
         rgTM.prSection[ilCurrentSection].iClearTwstart = FALSE;
         dbg(DEBUG,"<init_action> default-clear_twstart: <FALSE>");
      }
      else
      {
         dbg(TRACE,"<init_action> conf.-clear_twstart: <%s>",rgTM.prSection[ilCurrentSection].pcClearTwstart);
         if (strncmp(rgTM.prSection[ilCurrentSection].pcClearTwstart,"Y",1) == 0 ||
             strncmp(rgTM.prSection[ilCurrentSection].pcClearTwstart,"y",1) == 0)
         {
            rgTM.prSection[ilCurrentSection].iClearTwstart = TRUE;
            dbg(DEBUG,"<init_action> clear_twstart: <TRUE>");
         }
         else
         {
            rgTM.prSection[ilCurrentSection].iClearTwstart = FALSE;
            dbg(DEBUG,"<init_action> clear_twstart: <FALSE>");
         }      
      }
    
      /* table number for tw_start... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "table_no", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iTabNo)) != RC_SUCCESS)
        {
                /* set defaults... */
          rgTM.prSection[ilCurrentSection].iTabNo = -1;
        }

      /* command number for tw_start... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "command_no", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iCmdNo)) != RC_SUCCESS)
        {
                /* set defaults... */
          rgTM.prSection[ilCurrentSection].iCmdNo = -1;
        }

      /* table... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "table", CFG_STRING, rgTM.prSection[ilCurrentSection].pcTableName)) != RC_SUCCESS)
        {
          dbg(TRACE,"<init_action> missing table in %s", pclCurrentSection);
          Terminate(30);
        }

            if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "suspend_originator", CFG_STRING, rgTM.prSection[ilCurrentSection].
            pcSuspendOriginator)) != RC_SUCCESS)
      {
        dbg(DEBUG,"<init_action> missing suspend_originator in %s default -> nobody or use \"1234,5678\"", pclCurrentSection);
                          rgTM.prSection[ilCurrentSection].pcSuspendOriginator[0]='\0';
      }

      /* convert to uppercase */
      StringUPR((UCHAR*)rgTM.prSection[ilCurrentSection].pcTableName);

      /* mod_id... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "mod_id", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iModID)) != RC_SUCCESS)
        {
          dbg(TRACE,"<init_action> missing mod_id in section <%s>", pclCurrentSection);
          Terminate(30);
        }
      /* prio... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "prio", CFG_STRING, (char*)&rgTM.prSection[ilCurrentSection].pcPrio)) != RC_SUCCESS)
        {
          dbg(DEBUG,"<init_action> send events on def. PRIORITY_4 in section <%s>", pclCurrentSection);
                rgTM.prSection[ilCurrentSection].iPrio = PRIORITY_4;
        }
            else
            {
                if (strncmp(rgTM.prSection[ilCurrentSection].pcPrio,"orig",4)==0 ||
                        strncmp(rgTM.prSection[ilCurrentSection].pcPrio,"ORIG",4)==0)
                {
                    rgTM.prSection[ilCurrentSection].iPrio=iORG_EVENT_PRIO;
                }
                else
                {
                    rgTM.prSection[ilCurrentSection].iPrio=atoi(rgTM.prSection[ilCurrentSection].pcPrio);
                    if (rgTM.prSection[ilCurrentSection].iPrio<1 || rgTM.prSection[ilCurrentSection].iPrio>5)
                    {
                dbg(DEBUG,"<init_action> invalid Prio. configured. Send events PRIORITY_4 in section <%s>", pclCurrentSection);
                        rgTM.prSection[ilCurrentSection].iPrio = PRIORITY_4;
                    }
                }
            }

      /* command_type... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "command_type", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iCommandType)) != RC_SUCCESS)
        {
          rgTM.prSection[ilCurrentSection].iCommandType = EVENT_DATA;
        }

      /* keys... */
      memset((void*)pclKeys, 0x00, iMAXIMUM);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "key", CFG_STRING, pclKeys)) != RC_SUCCESS)
        {
          rgTM.prSection[ilCurrentSection].iNoOfKeys = 0;
        }
      else
        {
                /* convert to uppercase */
          StringUPR((UCHAR*)pclKeys);

                /* count fields */
          rgTM.prSection[ilCurrentSection].iNoOfKeys = GetNoOfElements(pclKeys, ',');

                /* memory for keys */
          if ((rgTM.prSection[ilCurrentSection].pcKeys = (char**)malloc(rgTM.prSection[ilCurrentSection].iNoOfKeys*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough menory to run (4.1)");
          rgTM.prSection[ilCurrentSection].pcKeys = NULL;
          Terminate(30);
        }

                /* for all keys... */
          for (ilCurrentKey=0;
           ilCurrentKey<rgTM.prSection[ilCurrentSection].iNoOfKeys;
           ilCurrentKey++)
        {
          if ((pclS = GetDataField(pclKeys, ilCurrentKey, ',')) == NULL)
            {
              dbg(TRACE,"<init_action> can't read key number %d in section %s", ilCurrentKey, pclCurrentSection);
              Terminate(30);
            }

          /* store command */
          if ((rgTM.prSection[ilCurrentSection].pcKeys[ilCurrentKey] = strdup(pclS)) == NULL)
            {
              dbg(TRACE,"<init_action> not enough memory to run (5.1)");
              rgTM.prSection[ilCurrentSection].pcKeys[ilCurrentKey] = NULL;
              Terminate(30);
            }
        }

                /* memory for key conditions...... */
          if ((rgTM.prSection[ilCurrentSection].pcKeyConditions = (char**)malloc(rgTM.prSection[ilCurrentSection].iNoOfKeys*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<init_action> can't allocate memory for key conditions");
          rgTM.prSection[ilCurrentSection].pcKeyConditions = NULL;
          Terminate(30);
        }

                /* .....and key condition types */
          if ((rgTM.prSection[ilCurrentSection].piKeyConditionTypes = (UINT*)malloc(rgTM.prSection[ilCurrentSection].iNoOfKeys*sizeof(UINT))) == NULL)
        {
          dbg(TRACE,"<init_action> can't allocate memory for key conditions types");
          rgTM.prSection[ilCurrentSection].piKeyConditionTypes = NULL;
          Terminate(30);
        }

                /* we've got the space */
          for (ilCurrentKey=0;
           ilCurrentKey<rgTM.prSection[ilCurrentSection].iNoOfKeys;
           ilCurrentKey++)
        {
          /* copy field to temporary buffer... */
          memset((void*)pclCondition, 0x00, iMAX_BUF_SIZE);
          strcpy(pclCondition, rgTM.prSection[ilCurrentSection].pcKeys[ilCurrentKey]);

          /* add "_condition"... */
          strcat(pclCondition, "_condition");
          StringLWR((UCHAR*)pclCondition);

          /* get entries */
          memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
          if ((ilRC = iGetConfigRow(pclCfgFile, pclCurrentSection, pclCondition, CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
            {
              rgTM.prSection[ilCurrentSection].pcKeyConditions[ilCurrentKey] = NULL;
            }
          else
            {
              /* delete blanks */
              DeleteCharacterInString(pclTmpBuf, cBLANK); 
                        
              /* format is: condition,type */
              /* if not -> we can't work with it */
              /* get condition... */
              if ((pclS = GetDataField(pclTmpBuf, 0, ',')) != NULL)
            {
              /* save current condition */
              memset((void*)pclCurrentCondition, 0x00, iMAX_BUF_SIZE);
              strcpy(pclCurrentCondition, pclS);

              /* ... then get type */
              if ((pclS = GetDataField(pclTmpBuf, 1, ',')) != NULL)
                {
                  /* we know type and condition (hope so) */
                  /* store both in structure... */
                  
                  /* first condition */
                  if ((rgTM.prSection[ilCurrentSection].pcKeyConditions[ilCurrentKey] = strdup(pclCurrentCondition)) == NULL)
                {
                  dbg(TRACE,"<init_action> not enough memory to store (key) conditions");
                  rgTM.prSection[ilCurrentSection].pcKeyConditions[ilCurrentKey] = NULL;
                  Terminate(30);
                }

                  /* second type */
                  StringUPR((UCHAR*)pclS);
                  if (!strncmp(pclS, "INT", 3))
                rgTM.prSection[ilCurrentSection].piKeyConditionTypes[ilCurrentKey] = iINTEGER;
                  else if (!strncmp(pclS, "STR", 3))
                rgTM.prSection[ilCurrentSection].piKeyConditionTypes[ilCurrentKey] = iSTRING;
                  else if (!strncmp(pclS, "TIM", 3))
                rgTM.prSection[ilCurrentSection].piKeyConditionTypes[ilCurrentKey] = iTIME;
                  else
                {
                  rgTM.prSection[ilCurrentSection].piKeyConditionTypes[ilCurrentKey] = iINTEGER;
                }
                }
            }
            }
        }
        }

      /* fields... */
      memset((void*)pclFields, 0x00, iMAXIMUM);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "fields", CFG_STRING, pclFields)) != RC_SUCCESS)
        {
          rgTM.prSection[ilCurrentSection].iNoOfFields = 0;
          rgTM.prSection[ilCurrentSection].pcFields = NULL;
          rgTM.prSection[ilCurrentSection].pcFieldResponseType = NULL; 
          rgTM.prSection[ilCurrentSection].pcFieldResponseFlag = NULL; 
        }
      else
        {
                /* convert to uppercase */
          StringUPR((UCHAR*)pclFields);

                /* count fields */
          rgTM.prSection[ilCurrentSection].iNoOfFields = GetNoOfElements(pclFields, ',');
         
                /* memory for fields */
          if ((rgTM.prSection[ilCurrentSection].pcFields = (char**)malloc(rgTM.prSection[ilCurrentSection].iNoOfFields*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough menory to run (4)");
          rgTM.prSection[ilCurrentSection].pcFields = NULL;
          Terminate(30);
        }
          /*memory for field response type*/
          if ((rgTM.prSection[ilCurrentSection].pcFieldResponseType = (char*)malloc(rgTM.prSection[ilCurrentSection].iNoOfFields*sizeof(char))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory to run (4)");
          rgTM.prSection[ilCurrentSection].pcFieldResponseType = NULL;
          Terminate(30);
        }
          if ((rgTM.prSection[ilCurrentSection].pcFieldResponseFlag = (char*)malloc(rgTM.prSection[ilCurrentSection].iNoOfFields*sizeof(char))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory to run (4)");
          rgTM.prSection[ilCurrentSection].pcFieldResponseFlag = NULL;
          Terminate(30);
        }
                /* for all fields... */
          for (ilCurrentField=0;
           ilCurrentField<rgTM.prSection[ilCurrentSection].iNoOfFields;
           ilCurrentField++)
        {
          if ((pclS = GetDataField(pclFields, ilCurrentField, ',')) == NULL)
            {
              dbg(TRACE,"<init_action> can't read field number %d in section %s", ilCurrentField, pclCurrentSection);
              Terminate(30);
            }
          /*set field response flag to NULL */
          rgTM.prSection[ilCurrentSection].pcFieldResponseFlag[ilCurrentField] = '0';
          /* store command */
          if ((rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField] = strdup(pclS)) == NULL)
            {
              dbg(TRACE,"<init_action> not enough memory to run (5)");
              rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField] = NULL;
              Terminate(30);
            }
          
          /* how shall we handle this field*/   
      /* JIM 20050915: first init this flag: */
      rgTM.prSection[ilCurrentSection].pcFieldResponseType[ilCurrentField]= 0;
          if (strrchr(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField],'?') != NULL)
            {
             dbg(DEBUG,"<init_action> Field command <?> was found: <%s>",
             rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField]);
            
             /* extract the fieldname*/ 
             rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField][strlen(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField])-1]= '\0';
             DeleteCharacterInString(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField], cBLANK);
            
             /*store field response type*/ 
         /* JIM 20050915: set ? flag: */
             rgTM.prSection[ilCurrentSection].pcFieldResponseType[ilCurrentField] = '?';
            }
      if (strrchr(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField],'!') != NULL)
            {
             dbg(DEBUG,"<init_action> Field command <!> was found: <%s>",
             rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField]);
             
             /* extract the fieldname*/
             rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField][strlen(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField])-1]= '\0';
              DeleteCharacterInString(rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField], cBLANK);
             /*store field response type*/ 
         /* JIM 20050915: overwrite ? flag by ! : */
             rgTM.prSection[ilCurrentSection].pcFieldResponseType[ilCurrentField] = '!';
            }
        }
                /* field conditions */
                /* first memory for conditions... */
      if ((rgTM.prSection[ilCurrentSection].pcFieldConditions = (char**)malloc(rgTM.prSection[ilCurrentSection].iNoOfFields*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory for FieldConditions");
          rgTM.prSection[ilCurrentSection].pcFieldConditions = NULL;
          Terminate(30);
        }

                /* ... and memory for types */
      if ((rgTM.prSection[ilCurrentSection].piFieldConditionTypes = (UINT*)malloc(rgTM.prSection[ilCurrentSection].iNoOfFields*sizeof(UINT))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory for FieldConditionsTypes");
          rgTM.prSection[ilCurrentSection].piFieldConditionTypes = NULL;
          Terminate(30);
        }

                /* we've got the space */
      for (ilCurrentField=0;
           ilCurrentField<rgTM.prSection[ilCurrentSection].iNoOfFields;
           ilCurrentField++)
        {
          /* copy field to temporary buffer... */
          memset((void*)pclCondition, 0x00, iMAX_BUF_SIZE);
          strcpy(pclCondition, rgTM.prSection[ilCurrentSection].pcFields[ilCurrentField]);

          /* add "_condition"... */
          strcat(pclCondition, "_condition");
          StringLWR((UCHAR*)pclCondition);

          /* get entries */
          memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);        
          if ((ilRC = iGetConfigRow(pclCfgFile, pclCurrentSection, pclCondition, CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
        {
          rgTM.prSection[ilCurrentSection].pcFieldConditions[ilCurrentField] = NULL;
        }
          else
        {
          /* delete blanks */
          DeleteCharacterInString(pclTmpBuf, cBLANK);
                        
          /* format is: condition,type */
          /* if not -> we can't work with it */
          /* get condition... */
          if ((pclS = GetDataField(pclTmpBuf, 0, ',')) != NULL)
            {
              /* save current condition */
              memset((void*)pclCurrentCondition, 0x00, iMAX_BUF_SIZE);
              strcpy(pclCurrentCondition, pclS);

              /* ... then get type */
              if ((pclS = GetDataField(pclTmpBuf, 1, ',')) != NULL)
            {
              /* we know type and condition (hope so) */
              /* store both in structure... */

              /* first condition */
              if ((rgTM.prSection[ilCurrentSection].pcFieldConditions[ilCurrentField] = strdup(pclCurrentCondition)) == NULL)
                {
                  dbg(TRACE,"<init_action> not enough memory to store conditions");
                  rgTM.prSection[ilCurrentSection].pcFieldConditions[ilCurrentField] = NULL;
                  Terminate(30);
                }
              dbg(DEBUG,"<init_action> FieldConditions <%s>",rgTM.prSection[ilCurrentSection].pcFieldConditions[ilCurrentField]);
              /* second type */
              StringUPR((UCHAR*)pclS);
              if (!strncmp(pclS, "INT", 3))
                rgTM.prSection[ilCurrentSection].piFieldConditionTypes[ilCurrentField] = iINTEGER;
              else if (!strncmp(pclS, "STR", 3))
                rgTM.prSection[ilCurrentSection].piFieldConditionTypes[ilCurrentField] = iSTRING;
              else if (!strncmp(pclS, "TIM", 3))
                rgTM.prSection[ilCurrentSection].piFieldConditionTypes[ilCurrentField] = iTIME;
              else if (!strcmp(pclS, "DYNAMIC_TIME"))
                rgTM.prSection[ilCurrentSection].piFieldConditionTypes[ilCurrentField] = iDYNAMIC_TIME;
              else
                {
                  rgTM.prSection[ilCurrentSection].piFieldConditionTypes[ilCurrentField] = iINTEGER;
                }
            }
            }
        }
        }
    }
      rgTM.prSection[ilCurrentSection].DataFilter[0] = 0x00;
      iGetConfigRow(pclCfgFile, pclCurrentSection, "data_filter", CFG_STRING, rgTM.prSection[ilCurrentSection].DataFilter);
      if (strlen(rgTM.prSection[ilCurrentSection].DataFilter) > 0)
    {
      dbg(DEBUG,"<init_action> SECTION <%s> FILTER <%s>", pclCurrentSection,rgTM.prSection[ilCurrentSection].DataFilter);
    } /* end if */

      rgTM.prSection[ilCurrentSection].DataMapping[0] = 0x00;
      iGetConfigRow(pclCfgFile, pclCurrentSection, "data_mapping", CFG_STRING, rgTM.prSection[ilCurrentSection].DataMapping);
      if (strlen(rgTM.prSection[ilCurrentSection].DataMapping) > 0)
    {
      dbg(DEBUG,"<init_action> SECTION <%s> MAPPING <%s>", pclCurrentSection,rgTM.prSection[ilCurrentSection].DataMapping);
    } /* end if */

      /* section commands... */
      memset((void*)pclSectionCommands, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "section_commands", CFG_STRING, pclSectionCommands)) != RC_SUCCESS)
    {
      rgTM.prSection[ilCurrentSection].iNoOfSectionCommands = 0;
    }
      else
    {
                /* convert to uppercase */
      StringUPR((UCHAR*)pclSectionCommands);

                /* count fields */
      rgTM.prSection[ilCurrentSection].iNoOfSectionCommands = GetNoOfElements(pclSectionCommands, ',');

                /* memory for fields */
      if ((rgTM.prSection[ilCurrentSection].pcSectionCommands = (char**)malloc(rgTM.prSection[ilCurrentSection].iNoOfSectionCommands*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<init_action> not enough menory to run (6)");
          rgTM.prSection[ilCurrentSection].pcSectionCommands = NULL;
          Terminate(30);
        }

                /* for all fields... */
      for (ilCurrentSectionCommand=0; ilCurrentSectionCommand<rgTM.prSection[ilCurrentSection].iNoOfSectionCommands; ilCurrentSectionCommand++)
        {
          if ((pclS = GetDataField(pclSectionCommands, ilCurrentSectionCommand, ',')) == NULL)
        {
          dbg(TRACE,"<init_action> can't read section-command number %d in section %s", ilCurrentSectionCommand, pclCurrentSection);
          Terminate(30);
        }

          /* store command */
          if ((rgTM.prSection[ilCurrentSection].pcSectionCommands[ilCurrentSectionCommand] = strdup(pclS)) == NULL)
        {
          dbg(TRACE,"<init_action> not enough memory to run (7)");
          rgTM.prSection[ilCurrentSection].pcSectionCommands[ilCurrentSectionCommand] = NULL;
          Terminate(30);
        }
        }
    }
        }/*JWE*/
    }
    }

  /* try to open file for setting/resetting section in internal structures */
  if ((pfFh = fopen(sVALID_FILE_NAME,"r")) == NULL)
    {
      dbg(TRACE,"<init_action> cannot open file <%s>", sVALID_FILE_NAME);
    }
  else
    {
      while (!feof(pfFh))
    {
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      if (fscanf(pfFh,"%[^\n]%*c", pclTmpBuf) == 1)
        {
          /* delete all banks in front of first character!! */
          while (pclTmpBuf[0] == 0x20)
        {
          memmove((void*)pclTmpBuf, 
              (const void*)&pclTmpBuf[1], 
              strlen(pclTmpBuf)-1);
          pclTmpBuf[strlen(pclTmpBuf)-1] = 0x00;
        }

          /* if first character is comment sign -> ignore it */
          if (pclTmpBuf[0] != '*' && pclTmpBuf[0] != ';')
        {
          ilTabNo = -1;
          memset((void*)pclTabType, 0x00, iMAX_BUF_SIZE);
            /*dbg(DEBUG,"<init_action> SECTION-STATUS-INFO = <%s>",pclTmpBuf);*/
          if (sscanf(pclTmpBuf, "%d %s %d %d", &ilTabNo, pclTabType, &ilTmpCmdNo, &ilTmpTabNo) == 4)
            {
              /*dbg(DEBUG,"<init_action> TabNo=<%d> TabType=<%s> CmdNo=<%d> TmpTabNo=<%d>",ilTabNo,pclTabType,ilTmpCmdNo,ilTmpTabNo);*/
    
              StringUPR((UCHAR*)pclTabType);
              if (!strcmp(pclTabType,"VALID"))
            {
              if (ilTabNo < rgTM.iNoOfSections && ilTabNo >= 0)
                {
                  dbg(DEBUG,"<init_action> setting Section <%s> to VALID", rgTM.prSection[ilTabNo].pcSectionName);
                  rgTM.prSection[ilTabNo].iValidSection = iVALID;
                }
            }
              else if (!strcmp(pclTabType,"NOT_VALID"))
            {
              if (ilTabNo < rgTM.iNoOfSections && ilTabNo >= 0)
                {
                        #if 0
                  dbg(DEBUG,"<init_action> setting Section <%s> to NOT_VALID", rgTM.prSection[ilTabNo].pcSectionName);
                  rgTM.prSection[ilTabNo].iValidSection = iNOT_VALID;
                        #endif
                  dbg(DEBUG,"<init_action> clear NOT_VALID of Section <%s>. Setting to VALID due to init!", rgTM.prSection[ilTabNo].pcSectionName);
                  rgTM.prSection[ilTabNo].iValidSection = iVALID;
                }
            }
              else
            {
              dbg(TRACE,"<init_action> unknown tab type <%s>", pclTabType);
            }
            }
        }
        }
    }

      /* close it now */
      fclose (pfFh); 

      /* delete control file */
      if ((ilRC = remove((const char*)sVALID_FILE_NAME)) != 0)
    {
      dbg(TRACE,"<init_action> remove <%s> failed, %d", 
          sVALID_FILE_NAME, ilRC);
    }
    }

  /* try to read dynamic configuration file */
  if ((ilRC = ReadDynamicCfgFile()) != RC_SUCCESS)
    {
      dbg(TRACE,"<init_action> error <%d> reading dynamic configuration file", ilRC);
    }

  /* everything looks good... */

    /* Building internal structures for CEI handling */
    Build_CEI_TableList(INIT);

  return RC_SUCCESS;
} 


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset()
{
  UINT          i;
  UINT          j;
  int           ilRC;
  char pclPath[iMIN_BUF_SIZE];
  /* first save status vaild or not valid of all sections */
  ilRC = get_system_state();
  dbg(DEBUG,"<Reset> get_system_state returns: %d", ilRC);
  if (ilRC != HSB_DOWN)
    {
      if ((ilRC = CreateValidFile()) == RC_FAIL)
    dbg(TRACE,"<Reset> CreateVaildFile returns: %d", ilRC);
      if ((ilRC = CreateDynamicCfgFile()) == RC_FAIL)
    dbg(TRACE,"<Reset> CreateDynamicCfgFile returns: %d", ilRC);
    }else{
      
      /* delete valid-file and dynamic configuration file here */
      remove(sVALID_FILE_NAME);
      sprintf(pclPath, "%s/action.dyn.cfg", getenv("CFG_PATH"));
      remove(pclPath);
    }
  /* free all memory... */
  if (prgItem != NULL)
    {
      free((void*)prgItem);
      prgItem = NULL;
    }

  if (rgTM.iNoOfGlobalCommands)
    {
      for (i=0; i<rgTM.iNoOfGlobalCommands; i++)
    {
      if (rgTM.pcGlobalCommands[i] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.pcGlobalCommands[%d]", i);
          free ((void*)rgTM.pcGlobalCommands[i]);
          rgTM.pcGlobalCommands[i] = NULL;
        }
    }
      if (rgTM.pcGlobalCommands != NULL)
    {
      dbg(DEBUG,"<Reset> free rgTM.pcGlobalCommands");
      free ((void*)rgTM.pcGlobalCommands);
      rgTM.pcGlobalCommands = NULL;
    }
    }

  for (i=0; i<rgTM.iNoOfSections; i++)
    {
      if (rgTM.prSection[i].iNoOfSectionCommands)
    {
      for (j=0; j<rgTM.prSection[i].iNoOfSectionCommands; j++)
        {
          if (rgTM.prSection[i].pcSectionCommands[j] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcSectionsCommands[%d]", i, j);
          free((void*)rgTM.prSection[i].pcSectionCommands[j]);
          rgTM.prSection[i].pcSectionCommands[j] = NULL;
        }
        }
      if (rgTM.prSection[i].pcSectionCommands != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcSectionsCommands", i);
          free((void*)rgTM.prSection[i].pcSectionCommands);
          rgTM.prSection[i].pcSectionCommands = NULL;
        }
    }

      for (j=0; j<rgTM.prSection[i].iNoOfFields; j++)
    {
      if (rgTM.prSection[i].pcFields[j] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFields[%d]", i, j);
          free((void*)rgTM.prSection[i].pcFields[j]);
          rgTM.prSection[i].pcFields[j] = NULL;
        }
      if (rgTM.prSection[i].pcFieldConditions[j] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldConditions[%d]", i, j);
          free((void*)rgTM.prSection[i].pcFieldConditions[j]);
          rgTM.prSection[i].pcFieldConditions[j] = NULL;
        }
    }
      if (rgTM.prSection[i].iNoOfFields)
    {
      if (rgTM.prSection[i].pcFields != NULL) 
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFields", i);
          free((void*)rgTM.prSection[i].pcFields);
          rgTM.prSection[i].pcFields = NULL;
        }
      if (rgTM.prSection[i].pcFieldResponseType != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldResponseType", i);
          free((void*)rgTM.prSection[i].pcFieldResponseType);
          rgTM.prSection[i].pcFieldResponseType = NULL;
        }
      if (rgTM.prSection[i].pcFieldResponseFlag != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldResponseFlag", i);
          free((void*)rgTM.prSection[i].pcFieldResponseFlag);
          rgTM.prSection[i].pcFieldResponseFlag = NULL;
        }

      if (rgTM.prSection[i].pcFieldConditions != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldConditions", i);
          free((void*)rgTM.prSection[i].pcFieldConditions);
          rgTM.prSection[i].pcFieldConditions = NULL;
        }
      if (rgTM.prSection[i].piFieldConditionTypes != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].piFieldConditionTypes", i);
          free((void*)rgTM.prSection[i].piFieldConditionTypes);
          rgTM.prSection[i].piFieldConditionTypes = NULL;
        }
    }
      for (j=0; j<rgTM.prSection[i].iNoOfKeys; j++)
    {
      if (rgTM.prSection[i].pcKeys[j] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcKeys[%d]", i, j);
          free((void*)rgTM.prSection[i].pcKeys[j]);
          rgTM.prSection[i].pcKeys[j] = NULL;
        }
      if (rgTM.prSection[i].pcKeyConditions[j] != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcKeyConditions[%d]", i, j);
          free((void*)rgTM.prSection[i].pcKeyConditions[j]);
          rgTM.prSection[i].pcKeyConditions[j] = NULL;
        }
    }
      if (rgTM.prSection[i].iNoOfKeys)
    {
      if (rgTM.prSection[i].pcKeys != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcKeys", i);
          free((void*)rgTM.prSection[i].pcKeys);
          rgTM.prSection[i].pcKeys = NULL;
        }
      if (rgTM.prSection[i].pcKeyConditions != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcKeyConditions", i);
          free((void*)rgTM.prSection[i].pcKeyConditions);
          rgTM.prSection[i].pcKeyConditions = NULL;
        }
      if (rgTM.prSection[i].piKeyConditionTypes != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].piKeyConditionTypes", i);
          free((void*)rgTM.prSection[i].piKeyConditionTypes);
          rgTM.prSection[i].piKeyConditionTypes = NULL;
        }
    }
    }

  if (rgTM.prSection != NULL)
    {
      dbg(DEBUG,"<Reset> free rgTM.prSection");
      free((void*)rgTM.prSection);
      rgTM.prSection = NULL;
    }

  /* new init */
  if ((ilRC = init_action()) != RC_SUCCESS)
    {       
      dbg(TRACE,"<Reset> init_action returns: %d", ilRC);
      Terminate(0);
    }   

  /* bye bye */
  return RC_SUCCESS;
}
 

/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(UINT ipSleepTime)
{
  int           ilRC;
  UINT          i;
  UINT          j;

  /* ignore all signals */
  (void)UnsetSignals();

  /* wait some seconds */
  if (ipSleepTime > 0)
    {
      dbg(DEBUG,"<Terminate> sleep %d seconds", ipSleepTime);
      sleep(ipSleepTime);
    }

  /* first save status (valid or not valid of all sections) */
  ilRC = get_system_state();
  dbg(DEBUG,"<Terminate> get_system_state returns: %d", ilRC);
  if (ilRC != HSB_DOWN)
    if ((ilRC = CreateValidFile()) == RC_FAIL)
      dbg(TRACE,"<Terminate> CreateVaildFile returns: %d", ilRC);

  /* free all memory... */
  if (prgItem != NULL)
    {
      free((void*)prgItem);
      prgItem = NULL;
    }


#if 0
  if (rgTM.iNoOfGlobalCommands)
    {
      for (i=0; i<rgTM.iNoOfGlobalCommands; i++)
    {
      if (rgTM.pcGlobalCommands[i] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.pcGlobalCommands[%d]", i);
          free ((void*)rgTM.pcGlobalCommands[i]);
          rgTM.pcGlobalCommands[i] = NULL;
        }
    }
      if (rgTM.pcGlobalCommands != NULL)
    {
      dbg(DEBUG,"<Terminate> free rgTM.pcGlobalCommands");
      free ((void*)rgTM.pcGlobalCommands);
      rgTM.pcGlobalCommands = NULL;
    }
    }

  for (i=0; i<rgTM.iNoOfSections; i++)
    {
      if (rgTM.prSection[i].iNoOfSectionCommands)
    {
      for (j=0; j<rgTM.prSection[i].iNoOfSectionCommands; j++)
        {
          if (rgTM.prSection[i].pcSectionCommands[j] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcSectionsCommands[%d]", i, j);
          free((void*)rgTM.prSection[i].pcSectionCommands[j]);
          rgTM.prSection[i].pcSectionCommands[j] = NULL;
        }
        }
      if (rgTM.prSection[i].pcSectionCommands != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcSectionsCommands", i);
          free((void*)rgTM.prSection[i].pcSectionCommands);
          rgTM.prSection[i].pcSectionCommands = NULL;
        }
    }

      for (j=0; j<rgTM.prSection[i].iNoOfFields; j++)
    {
      if (rgTM.prSection[i].pcFields[j] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcFields[%d]", i, j);
          free((void*)rgTM.prSection[i].pcFields[j]);
          rgTM.prSection[i].pcFields[j] = NULL;
        }
      if (rgTM.prSection[i].pcFieldConditions[j] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcFieldConditions[%d]", i, j);
          free((void*)rgTM.prSection[i].pcFieldConditions[j]);
          rgTM.prSection[i].pcFieldConditions[j] = NULL;
        }
    }
      if (rgTM.prSection[i].iNoOfFields)
    {
      if (rgTM.prSection[i].pcFields != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcFields", i);
          free((void*)rgTM.prSection[i].pcFields);
          rgTM.prSection[i].pcFields = NULL;
        }
      if (rgTM.prSection[i].pcFieldResponseType != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldResponseType", i);
          free((void*)rgTM.prSection[i].pcFieldResponseType);
          rgTM.prSection[i].pcFieldResponseType = NULL;
        }
      if (rgTM.prSection[i].pcFieldResponseFlag != NULL)
        {
          dbg(DEBUG,"<Reset> free rgTM.prSection[%d].pcFieldResponseFlag", i);
          free((void*)rgTM.prSection[i].pcFieldResponseFlag);
          rgTM.prSection[i].pcFieldResponseFlag = NULL;
        }
      if (rgTM.prSection[i].pcFieldConditions != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcFieldConditions", i);
          free((void*)rgTM.prSection[i].pcFieldConditions);
          rgTM.prSection[i].pcFieldConditions = NULL;
        }
      if (rgTM.prSection[i].piFieldConditionTypes != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].piFieldConditionTypes", i);
          free((void*)rgTM.prSection[i].piFieldConditionTypes);
          rgTM.prSection[i].piFieldConditionTypes = NULL;
        }
    }
      for (j=0; j<rgTM.prSection[i].iNoOfKeys; j++)
    {
      if (rgTM.prSection[i].pcKeys[j] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcKeys[%d]", i, j);
          free((void*)rgTM.prSection[i].pcKeys[j]);
          rgTM.prSection[i].pcKeys[j] = NULL;
        }
      if (rgTM.prSection[i].pcKeyConditions[j] != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcKeyConditions[%d]", i, j);
          free((void*)rgTM.prSection[i].pcKeyConditions[j]);
          rgTM.prSection[i].pcKeyConditions[j] = NULL;
        }
    }
      if (rgTM.prSection[i].iNoOfKeys)
    {
      if (rgTM.prSection[i].pcKeys != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcKeys", i);
          free((void*)rgTM.prSection[i].pcKeys);
          rgTM.prSection[i].pcKeys = NULL;
        }
      if (rgTM.prSection[i].pcKeyConditions != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].pcKeyConditions", i);
          free((void*)rgTM.prSection[i].pcKeyConditions);
          rgTM.prSection[i].pcKeyConditions = NULL;
        }
      if (rgTM.prSection[i].piKeyConditionTypes != NULL)
        {
          dbg(DEBUG,"<Terminate> free rgTM.prSection[%d].piKeyConditionTypes", i);
          free((void*)rgTM.prSection[i].piKeyConditionTypes);
          rgTM.prSection[i].piKeyConditionTypes = NULL;
        }
    }
    }
#endif
  if (rgTM.prSection != NULL)
    {
      dbg(DEBUG,"<Terminate> free rgTM.prSection");
      free((void*)rgTM.prSection);
      rgTM.prSection = NULL;
    }

  dbg(TRACE,"<Terminate> now leaving ...");
  exit(0);
}


/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/
static void HandleSignal(int ipSig)
{
    dbg(DEBUG,"<HandleSignal> signal <%d> received", ipSig);

    switch (ipSig)
    {
        default:
            Terminate(0);
            break;
    } 
    return;
}


/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int pipErr)
{
    dbg(DEBUG,"<HandleErr> calling with error code: %d", pipErr);

    return;
}


/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr(int pipErr)
{
    switch (pipErr) 
    {
        case    QUE_E_FUNC: /* Unknown function */
            dbg(TRACE,"<%d> : unknown function",pipErr);
            break;

        case    QUE_E_MEMORY:   /* Malloc reports no memory */
            dbg(TRACE,"<%d> : malloc failed",pipErr);
            break;

        case    QUE_E_SEND: /* Error using msgsnd */
            dbg(TRACE,"<%d> : msgsnd failed",pipErr);
            break;

        case    QUE_E_GET:  /* Error using msgrcv */
            dbg(TRACE,"<%d> : msgrcv failed",pipErr);
            break;

        case    QUE_E_EXISTS:
            dbg(TRACE,"<%d> : route/queue already exists ",pipErr);
            break;

        case    QUE_E_NOFIND:
            dbg(TRACE,"<%d> : route not found ",pipErr);
            break;

        case    QUE_E_ACKUNEX:
            dbg(TRACE,"<%d> : unexpected ack received ",pipErr);
            break;

        case    QUE_E_STATUS:
            dbg(TRACE,"<%d> :  unknown queue status ",pipErr);
            break;

        case    QUE_E_INACTIVE:
            dbg(TRACE,"<%d> : queue is inaktive ",pipErr);
            break;

        case    QUE_E_MISACK:
            dbg(TRACE,"<%d> : missing ack ",pipErr);
            break;

        case    QUE_E_NOQUEUES:
            dbg(TRACE,"<%d> : queue does not exist",pipErr);
            break;

        case    QUE_E_RESP: /* No response on CREATE */
            dbg(TRACE,"<%d> : no response on create",pipErr);
            break;

        case    QUE_E_FULL:
            dbg(TRACE,"<%d> : too many route destinations",pipErr);
            break;

        case    QUE_E_NOMSG:    /* No message on queue */
            dbg(TRACE,"<%d> : no messages on queue",pipErr);
            break;

        case    QUE_E_INVORG:   /* Mod id by que call is 0 */
            dbg(TRACE,"<%d> : invalid originator=0",pipErr);
            break;

        case    QUE_E_NOINIT:   /* Queues is not initialized*/
            dbg(TRACE,"<%d> : queues are not initialized",pipErr);
            break;

        case    QUE_E_ITOBIG:
            dbg(TRACE,"<%d> : requestet itemsize to big ",pipErr);
            break;

        case    QUE_E_BUFSIZ:
            dbg(TRACE,"<%d> : receive buffer to small ",pipErr);
            break;

        default:    /* Unknown queue error */
            dbg(TRACE,"<%d> : unknown error",pipErr);
            break;
    } 
         
    return;
}


/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues()
{
    int ilRC = RC_SUCCESS;          /* Return code */
    int ilBreakOut = FALSE;
    
    do
    {
        /* get next item */
        ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

        /* set event pointer        */
        prgEvent = (EVENT*)prgItem->text;
    
        /* check return code */
        if (ilRC == RC_SUCCESS)
        {
            /* Acknowledge the item */
            ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
            if( ilRC != RC_SUCCESS ) 
            {
                HandleQueErr(ilRC);
            } 
        
            switch( prgEvent->command )
            {
            case    HSB_STANDBY :
                ctrl_sta = prgEvent->command;
                send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
                break;  
    
            case    HSB_COMING_UP   :
                ctrl_sta = prgEvent->command;
                send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
                break;  
    
            case    HSB_ACTIVE  :
            case    HSB_STANDALONE  :
                ctrl_sta = prgEvent->command;
                send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
                ilBreakOut = TRUE;
                break;  

            case    HSB_ACT_TO_SBY  :
                ctrl_sta = prgEvent->command;
                send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
                break;  
    
            case    HSB_DOWN    :
                ctrl_sta = prgEvent->command;
                Terminate(0);
                break;  
    
            case    SHUTDOWN    :
                Terminate(0);
                break;
                        
            case    RESET       :
                ilRC = Reset();
                break;
                        
            case    EVENT_DATA  :
                dbg(DEBUG,"<HandleQueues> wrong hsb status <%d>",ctrl_sta);
                DebugPrintItem(TRACE,prgItem);
                DebugPrintEvent(TRACE,prgEvent);
                break;
                    
            case TRACE_ON :
                dbg_handle_debug(prgEvent->command);
                break;

            case TRACE_OFF :
                dbg_handle_debug(prgEvent->command);
                break;

            default         :
                dbg(DEBUG,"<HandleQueues> unknown event");
                DebugPrintItem(TRACE,prgItem);
                DebugPrintEvent(TRACE,prgEvent);
                break;
            } /* end switch */
            
            /* Handle error conditions */
        
            if(ilRC != RC_SUCCESS)
            {
                HandleErr(ilRC);
            } 
        } 
        else 
        {
            HandleQueErr(ilRC);
        } 
    } while (ilBreakOut == FALSE);
    
    /* bye bye */
    return;
}
    

static int CreateDynamicConfigFromDatalist(ACTIONConfig *prpConfig,char *pcpData)
{
    int ilRc = RC_SUCCESS;
    char clTmpBuffer[iMAX_BUF_SIZE+1];

    get_real_item(clTmpBuffer,pcpData,2); /* ADFlag */
    prpConfig->iADFlag = atoi(clTmpBuffer);
    get_real_item(clTmpBuffer,pcpData,3); /* EmptyFieldHandling */
    prpConfig->iEmptyFieldHandling = atoi(clTmpBuffer);
    get_real_item(clTmpBuffer,pcpData,4); /* IgnoreEmptyFields */
    prpConfig->iIgnoreEmptyFields = atoi(clTmpBuffer);
    get_real_item(prpConfig->pcSndCmd,pcpData,5); /* SndCmd */
    get_real_item(prpConfig->pcSectionName,pcpData,6); /* TableName */
    get_real_item(prpConfig->pcTableName,pcpData,7); /* TableName */
    get_real_item(prpConfig->pcFields,pcpData,8); /* Fields */
    ConvertDbStringToClient(prpConfig->pcFields);
    get_real_item(prpConfig->pcSectionCommands,pcpData,9); /* SectionCommands */
    ConvertDbStringToClient(prpConfig->pcSectionCommands);
    get_real_item(clTmpBuffer,pcpData,10); /* ModID */
    prpConfig->iModID = atoi(clTmpBuffer);
    get_real_item(clTmpBuffer,pcpData,11); /* SuspendOwn */
    prpConfig->iSuspendOwn = atoi(clTmpBuffer);
    get_real_item(prpConfig->pcSuspendOriginator,pcpData,12); /* SuspendOriginator */
    return ilRc;
}


/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData()
{
  int           ilGetRc=RC_SUCCESS;
  int           i;
  int           j;
  int           ilRc;
  int           ilRC;
  int           ilCnt1;
  int           ilCnt2;
  int           ilIdx;
  int           ilMatch;
  int           ilTableNo;
  int           ilFieldCnt;
  int           ilGlobalCmdNo;
  int           ilLocalCmdNo;
  int           ilFoundGlobal;
  int           ilFoundLocal;
  int           ilUrnoPos;
  int           ilCheckOK;
  int           ilDbgLevel1;
  int           ilDbgLevel2;
  long      llDataLen=0L;
  BC_HEAD *prlBCHead        = NULL;
  CMDBLK    *prlCmdblk      = NULL;
  char      *pclDatablk     = NULL;
  char      *pclSelection   = NULL;
  char      *pclFields      = NULL;
  char      *pclFieldPos    = NULL;
  char      *pclTableList   = NULL;
  char      *pclListOfTables    = NULL;
  char      *pclS               = NULL;
  char      *pclS2              = NULL;
  char      *pclOldData             = NULL;
  char      *pclOldDumm = "\0";
  char      pclUrnoBuf[iMIN];
  char      pclUrnoDat[iMIN];
  char      pclFieldNames[iMAX_BUF_SIZE];
  char      pclMsgOriginator[iMAX_BUF_SIZE];
  char      pclXmlBuf[iMAX_BUF_SIZE];
  char      pclTmpBuf[iMAX_BUF_SIZE];
  char      pclFieldData[iMAXIMUM];
  char      pclNewFields[iMAXIMUM];
  char      pclNewData[iMAXIMUM];
  char              pclTmpFieldName[iMIN_BUF_SIZE];
  ACTIONConfig  *prlConfig      = NULL;
  ACTIONConfig  rlConfig; /* 20040107 JIM: copy to avoid alignment errors */
  char      pclOldFieldData[iMAXIMUM];

  /* init */
  ilRC = RC_SUCCESS;

  /* set pointer... */
  prlBCHead      = (BC_HEAD*)((char*)prgEvent + sizeof(EVENT));
  if (prlBCHead->rc != RC_SUCCESS && prlBCHead->rc != NETOUT_NO_ACK)
  {
    dbg(DEBUG,"<HandleData> BCHead->rc != RC_SUCCESS && prlBCHead->rc != NETOUT_NO_ACK...");
    return RC_SUCCESS;
  }
  prlCmdblk = (CMDBLK*)prlBCHead->data;
  dbg(TRACE,"<HandleData> INCOMING-EVENTS <%ld>, OUTGOING-EVENTS <%ld>",ulgEventsReceived,ulgEventsSent);
  dbg(TRACE,"<HandleData> ORIGINATOR IS: <%d> PRIO:<%d> - CHECK %d SECTIONS!"
        ,prgEvent->originator,prgItem->priority,rgTM.iNoOfSections);
  dbg(TRACE,"<HandleData> TW_START<%s>-TW_END<%s>",prlCmdblk->tw_start,prlCmdblk->tw_end);

  /* should we delete the section with this command or should we handle it */
  if (!strcmp(prlCmdblk->obj_name, "INSERT_THIS_SECTION") ||
      !strcmp(prlCmdblk->obj_name, "DELETE_THIS_SECTION"))
  {
    dbg(DEBUG,"<HandleData> -- START INSERT/DELETE-SECTION --");
    /* search for table */
    for (ilTableNo=0; ilTableNo<rgTM.iNoOfSections; ilTableNo++)
        {
            /* search for (local) command... */
            for (ilLocalCmdNo=0; 
           ilLocalCmdNo<rgTM.prSection[ilTableNo].iNoOfSectionCommands; 
           ilLocalCmdNo++)
        {
          if (!strcmp(prlCmdblk->command, rgTM.prSection[ilTableNo].pcSectionCommands[ilLocalCmdNo]))
                {
                    /* found section with command to delete */
                    /* alter only if originator is like mod_id in cfg-file */
                    if (rgTM.prSection[ilTableNo].iModID == prgEvent->originator)
                    {
                        if (!strcmp(prlCmdblk->obj_name, "INSERT_THIS_SECTION"))
                        {
                            rgTM.prSection[ilTableNo].iValidSection = iVALID;
                            dbg(DEBUG,"<HandleData> Section <%s> set to VALID",rgTM.prSection[ilTableNo].pcSectionName); 
                        }
                        else
                        {
                            rgTM.prSection[ilTableNo].iValidSection = iNOT_VALID;
                            dbg(DEBUG,"<HandleData> Section <%s> set to NOT_VALID",rgTM.prSection[ilTableNo].pcSectionName); 
                        }
                    }
                }
        }
        }
    dbg(DEBUG,"<HandleData> -- END INSERT/DELETE-SECTION --");
  }
  else
  {
        int ilConfigType = 0;

    pclSelection = (char*)prlCmdblk->data;
    pclFields    = (char*)pclSelection + strlen(pclSelection) + 1;
    pclDatablk   = (char*)pclFields + strlen(pclFields) + 1;
    
        dbg(DEBUG,"HandleData, Datablk: <%s>",pclDatablk);

    /* is this a new (dynamic) configuration */
    if (!strcmp(pclDatablk, "DYN"))
        {
            ilConfigType = DYNCONFIGOLD;
        }
    if (!strncmp(pclDatablk, "NDC",3))
        {
            ilConfigType = DYNCONFIGNEW;
        }
    if (ilConfigType)
        {
            dbg(DEBUG,"<HandleData> -- DYN-SUBSCRIPTION-REQUEST. %s ORIG.=<%d> --",
                ilConfigType == DYNCONFIGOLD ? "OLD FORMAT" : "NEW FORMAT",prgEvent->originator);
            if (ilConfigType == DYNCONFIGOLD)
            {
                /*snap((pclDatablk+strlen(pclDatablk)+1),sizeof(ACTIONConfig),outp);*/
                prlConfig = (ACTIONConfig*)(pclDatablk+strlen(pclDatablk)+1);
        /* 20040107 JIM: copy to avoid alignment errors: */
        memcpy(&rlConfig,prlConfig,sizeof(ACTIONConfig));
            }
            else
            {
                CreateDynamicConfigFromDatalist(&rlConfig,pclDatablk);
            }
            dbg(TRACE,"<HandleData> SEC:<%s>, CMD:<%s>, TAB:<%s>, FLD:<%s> ==> MOD-ID:<%d> CMD:<%s>"
                ,rlConfig.pcSectionName,rlConfig.pcSectionCommands,rlConfig.pcTableName,rlConfig.pcFields,rlConfig.iModID,rlConfig.pcSndCmd);
            if ((ilRC = AddDynamicConfig(&rlConfig, iCREATE_DYNAMIC_FILE)) != RC_SUCCESS)
            {
                dbg(TRACE,"<HandleData> -- ERROR: ADDING/DELETE OF DYN-SUBSCRIPTION-REQUEST !! --"); 
            }
            else
            {
                dbg(DEBUG,"<HandleData> -- DYN-SUBSCRIPTION-REQUEST PROCESSED SUCCESSFULL ! --"); 
                /* Rebuild list if a process registers during runtime */
                Build_CEI_TableList(HANDLE_DATA);
            }
            return RC_SUCCESS;
        }
    /* convert to uppercase */
    StringUPR((UCHAR*)prlCmdblk->command);
    StringUPR((UCHAR*)prlCmdblk->obj_name);
    StringUPR((UCHAR*)pclFields);
    pclOldData = strstr(pclDatablk,"\n");
    if (pclOldData != NULL)
        {
            *pclOldData = 0x00;
            pclOldData++;
        } /* end if */
    else
        {
            pclOldData = pclOldDumm;
        } /* end else */

        dbg(TRACE,"<HandleData> Command  :<%s>", prlCmdblk->command);
        dbg(TRACE,"<HandleData> Obj_name :<%s>", prlCmdblk->obj_name);
        dbg(TRACE,"<HandleData> Selection:<%s>", pclSelection);
        dbg(TRACE,"<HandleData> Fields   :<%s>", pclFields);
        dbg(TRACE,"<HandleData> Datablk  :<%s>", pclDatablk);
        dbg(TRACE,"<HandleData> OldData  :<%s>", pclOldData);

        /* UBT und URT haben oft keine Urno in der Feldliste, aber in der selection */
        if (!strcmp(prlCmdblk->command, "UBT") || 
            !strcmp(prlCmdblk->command, "URT") ||
            !strcmp(prlCmdblk->command, "DRT") ||
            !strcmp(prlCmdblk->command, "DBT"))
        {
            dbg(DEBUG,"<HandleData> fetch URNO out of event.");
            if ((pclS = strstr(pclFields, "URNO")) == NULL)
        {
          dbg(DEBUG,"<HandleData> command: <%s>, no URNO in fieldlist!", prlCmdblk->command);
          if ((pclS = strstr(pclSelection, "URNO")) == NULL)
                {
                    dbg(DEBUG,"<HandleData> cannot find URNO in Selection...");
                    return RC_FAIL;
                }
          else
                {
                    /* get urno and urno-data */ 
            dbg(DEBUG,"<HandleData> fetching URNO from selection.");
                    if ((ilRC = SeparateIt(pclS, pclUrnoBuf, pclUrnoDat, '=')) < 0)
                    {
                        /* this is an error */
                        dbg(DEBUG,"<HandleData> SeparateIt() returns: <%d>",ilRC);
                        return RC_FAIL;
                    }
                    else
                    {
                        /* delete left and right blanks... */
                        str_trm_all(pclUrnoBuf, " ", TRUE);
                        str_trm_all(pclUrnoDat, " ", TRUE);

                        /* clear buffer */
                        memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);

                        /* if urno is enclosed by ''' */
                        if ((ilRC = GetAllBetween(pclUrnoDat, '\'', 0, pclTmpBuf)) == 0)
                        {
                            /* copy without ''' */
                            strcpy(pclUrnoDat, pclTmpBuf);  
                        }

                        /* clear buffer */
                        memset((void*)pclNewFields, 0x00, iMAXIMUM);
                        memset((void*)pclNewData, 0x00, iMAXIMUM);

                        /* add urno to field- and datalist */
                        /*sprintf(pclNewFields, "%s,%s", pclFields, pclUrnoBuf);*/
                        if (strlen(pclFields)>0)
                        {
                            strcpy(pclNewFields,pclFields);
                            strcat(pclNewFields,",");
                        }
                        strcat(pclNewFields,pclUrnoBuf);

                        /*sprintf(pclNewData, "%s,%s", pclDatablk, pclUrnoDat); */
                        if (strlen(pclDatablk)>0)
                        {
                            strcpy(pclNewData,pclDatablk);
                            strcat(pclNewData,",");
                        }
                        strcat(pclNewData,pclUrnoDat);

                        /* set pointer */
                        pclFields = pclNewFields;
                        pclDatablk = pclNewData;

                        /* debug messages */
                        dbg(DEBUG,"<HandleData> new Fields: <%s>", pclFields);
                        dbg(DEBUG,"<HandleData> new Datablk: <%s>", pclDatablk);
            }
            }
      }
    }
  else if (strcmp(prlCmdblk->command, "SCFG")==0)
    {
        ShowConfigState();
        return RC_SUCCESS;
    }
  else if (strcmp(prlCmdblk->command, "BCEI")==0)
    {
        Build_CEI_TableList(HANDLE_DATA);
        return RC_SUCCESS;
    }
  else if (strcmp(prlCmdblk->command, "CEI")==0)
    {
        if (strstr(pclSelection,"<MSG>")!=NULL)
        {
            dbg(DEBUG,"<HandleData> ----------------------");
            dbg(DEBUG,"<HandleData> CEI-MESSAGE");
            memset((void*)pclMsgOriginator, 0x00, iMAX_BUF_SIZE);
            GetXmlValue(pclMsgOriginator,&llDataLen,pclSelection,"<ORIGINATOR>","</ORIGINATOR>",TRUE);
            dbg(DEBUG,"<HandleData> ORIGINATOR:[%s]",pclMsgOriginator);
            memset((void*)pclXmlBuf, 0x00, iMAX_BUF_SIZE);
            GetXmlValue(pclXmlBuf,&llDataLen,pclSelection,"<TYPE>","</TYPE>",TRUE);
            dbg(DEBUG,"<HandleData> TYPE      :[%s]",pclXmlBuf);
            if (strncmp(pclXmlBuf,"CFG",3)==0)
            {
                memset((void*)pclXmlBuf, 0x00, iMAX_BUF_SIZE);
                GetXmlValue(pclXmlBuf,&llDataLen,pclSelection,"<SUBTYPE>","</SUBTYPE>",TRUE);
                dbg(DEBUG,"<HandleData> SUBTYPE   :[%s]",pclXmlBuf);
                if (strncmp(pclXmlBuf,"TABLELIST-UPD",13)==0)
                {
                    pclTableList = GetXmlValue(NULL,&llDataLen,pclSelection,"<CONTENT>","</CONTENT>",FALSE);
                    if ((pclListOfTables=(char*)calloc(1,llDataLen+16)) != NULL) 
                    {
                        strncpy(pclListOfTables,pclTableList,llDataLen);
                        dbg(DEBUG,"<HandleData> CONTENT   :[%s]",pclListOfTables);
                        Use_CEI_TableList(pclMsgOriginator,pclListOfTables);
                    }
                }
            }
        }
        return RC_SUCCESS;
    }

  /* serach for (global) command... */
  for (ilGlobalCmdNo=0, ilFoundGlobal=0; 
     ilGlobalCmdNo<rgTM.iNoOfGlobalCommands; 
     ilGlobalCmdNo++)
    {
      if (!strcmp(prlCmdblk->command, rgTM.pcGlobalCommands[ilGlobalCmdNo]))
      {
        ilFoundGlobal = 1;
        break;
      }
    }

    dbg(DEBUG,"<HandleData> start checking all sections.");
    ulgEventsReceived++;
  /* search for table */
  for (ilTableNo=0; ilTableNo<rgTM.iNoOfSections; ilTableNo++)
    {
        ilGetRc = RC_SUCCESS;        /*hag 20020604 */

        dbg(DEBUG,"<HandleData> ############ SEC.:<%s> ##############",rgTM.prSection[ilTableNo].pcSectionName); 
        /* this is a CEI section and we are the sever part, so we might need to send information to CEI-client */
        if (rgTM.prSection[ilTableNo].cei_iInternalType==1 && rgTM.prSection[ilTableNo].cei_iTask==CEISRV)
        {
            if ((ilRc=Check_CEI_Tables(ilTableNo,prlCmdblk->obj_name)) == RC_SUCCESS)
            {
                dbg(DEBUG,"<HandleData> SEC.:<%s> => FORWARDING EVENT TO CEI_OUT=<%d>",rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].cei_iSndModId);

                ilRc = SendEventToCei(ilTableNo,prlCmdblk->command,prlCmdblk->obj_name,prlCmdblk->tw_start,prlCmdblk->tw_end,pclSelection,pclFields,pclDatablk,pclOldData);
            }
            else
            {
                dbg(DEBUG,"<HandleData> TABLE_NAME=<%s> NOT FOUND IN CEI-LIST OR FORWARDING-EXCLUDE SET! NOT FORWARDING TO CEI-OUT!",prlCmdblk->obj_name);
            }
        }
        else /* this is a normal, static or dynamic section */
        {
            /* use only valid sections.... */
            if (rgTM.prSection[ilTableNo].iValidSection == iNOT_VALID)
            {
                dbg(TRACE,"<HandleData> SEC.:<%s> => state is NOT VALID! Skiping!", rgTM.prSection[ilTableNo].pcSectionName);
            }
            else
            {
                /* compare obj_name (TableName) and configuration via CFG-File */
                if (!strcmp(rgTM.prSection[ilTableNo].pcTableName, prlCmdblk->obj_name))
                {
                    dbg(DEBUG,"<HandleData> SEC.:<%s> => db-table filter matches.",rgTM.prSection[ilTableNo].pcSectionName); 
                    /* this is correct table... */
                    /* not a global command?, search for section specific command. */
                    if (!ilFoundGlobal)
                    {
                        dbg(DEBUG,"<HandleData> SEC.:<%s> => check command-filter.",rgTM.prSection[ilTableNo].pcSectionName); 
                        /* search for (global) command... */
                        for (ilLocalCmdNo=0, ilFoundLocal=0; ilLocalCmdNo<rgTM.prSection[ilTableNo].iNoOfSectionCommands; ilLocalCmdNo++)
                        {
                            if (!strcmp(prlCmdblk->command, rgTM.prSection[ilTableNo].pcSectionCommands[ilLocalCmdNo]))
                            {
                                ilFoundLocal = 1;
                                break;
                            }
                        }
                    }

                    /* we found a table, now check the command */
                    if (ilFoundGlobal || ilFoundLocal)
                    {
                        /* checking if event needs to be forwarded or not */
                        dbg(DEBUG,"<HandleData> SEC.:<%s> => check originator/destination.",rgTM.prSection[ilTableNo].pcSectionName); 
                        ilRC=CheckOriginator(&rgTM.prSection[ilTableNo],prlCmdblk->tw_end);
                        if (ilRC == RC_SUCCESS)
                        {
                            /* clear buffer */
                            memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                            memset((void*)pclFieldData, 0x00, iMAXIMUM);

                            /* count fields... */
                            ilFieldCnt = GetNoOfElements(pclFields, ',');

                            /* check if received field list is equal or less than configured list */
                            ilCheckOK = TRUE;
                            if (ilFieldCnt <= rgTM.prSection[ilTableNo].iNoOfIgnoreDataFields)
                            {
                                /* yes it is, so check the fields */
                                ilCheckOK = CheckFields(ilFieldCnt, pclFields, rgTM.prSection[ilTableNo].pcIgnoreDataFields); 
                            }
                            dbg(DEBUG,"<HandleData> SEC.:<%s> => ilCheckOK is: <%s> FLD-CNT=<%d>"
                                ,rgTM.prSection[ilTableNo].pcSectionName,ilCheckOK==TRUE?"TRUE":"FALSE",ilFieldCnt); 
                            /* valid fields? */
                            if (ilCheckOK == TRUE)
                            {
                                /* initialize buffer for old data since we get here for every section and have to evaluate every sections field list */
                                memset(pclOldFieldData,0x00,iMAXIMUM);
                                
                                /* for all possible fields... */
                                for (i=0; i<ilFieldCnt; i++)
                                {
                                        /* get next fieldname... */
                                        pclS = GetDataField(pclFields, i, ',');
                                        strcpy(pclTmpFieldName, pclS);
                                        dbg(DEBUG,"<HandleData> SEC.:<%s> => check field <%s>.",rgTM.prSection[ilTableNo].pcSectionName,pclTmpFieldName); 
                                        /* hope it works in general */
                                        if (rgTM.prSection[ilTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS)
                                        {
                                            /* search this field in CFG-Fieldlist... */
                                            for (j=0, pclFieldPos=NULL; j<rgTM.prSection[ilTableNo].iNoOfFields && pclFieldPos == NULL; j++)
                                            {
                                                pclFieldPos = strstr(pclS, rgTM.prSection[ilTableNo].pcFields[j]);
                                            }
                                        }
                                        else
                                        {
                                            /* 
                                                 handle all received fields here. as a result, 
                                                 it's not possible to have field_conditions 
                                                 without configured fields. 
                                            */
                                            pclFieldPos = NULL;
                                        }
                                        /* we found this field... */
                                        if (pclFieldPos!=NULL || rgTM.prSection[ilTableNo].iNoOfFields==iUSE_ALL_RECEIVED_FIELDS)
                                        {
                                            if (rgTM.prSection[ilTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS)
                                            {
                                                /* decr j, necessary for field-condition check */
                                                --j;
                                            }
                                            /* get data for this field */
                                            if ((pclS = GetDataField(pclDatablk, i, ',')) != NULL)
                                            {
                                                /* check data for vaild conditions */
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => check field condition FLD<%s>=DAT<%s>"
                                                    ,rgTM.prSection[ilTableNo].pcSectionName,pclTmpFieldName,pclS);

                                                ilMatch = 1;
                                                if (rgTM.prSection[ilTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS)
                                                {
                                                    if (rgTM.prSection[ilTableNo].pcFieldConditions[j] != NULL)
                                                    {
                                                        /* save data to temporary buffer */
                                                        memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
                                                        strcpy(pclTmpBuf, pclS);

                                                        if ((ilRC = MatchCondition(pclTmpBuf, rgTM.prSection[ilTableNo].pcFieldConditions[j], rgTM.prSection[ilTableNo].piFieldConditionTypes[j])) != 1)
                                                        {
                                                            ilMatch = 0;
                                                            i = ilFieldCnt;
                                                            memset((void*)pclFieldData, 0x00, iMAXIMUM);
                                                            memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                                                        }
                                                    }
                                                }               
                                                if (ilMatch)
                                                {
                                                    /* ilMatch is 1... */
                                                    /* should i ignore empty fields... */
                                                    if (rgTM.prSection[ilTableNo].iIgnoreEmptyFields)
                                                    {
                                                        /* here we ignore this empty field... */
                                                        if (!strlen(pclS))
                                                            ilMatch = 0;
                                                        dbg(DEBUG,"<HandleData> SEC.:<%s> => Ignore empty fields!"
                                                            ,rgTM.prSection[ilTableNo].pcSectionName);
                                                    }
                                                    if (ilMatch)
                                                    {
                                                        /* add field to list */
                                                        strcat(pclFieldNames, pclTmpFieldName);
                                                        strcat(pclFieldNames, ",");
                                                        /* copy to temporary datablk */
                                                        strcat(pclFieldData, pclS);
                                                        strcat(pclFieldData, ",");
                                                        if(rgTM.prSection[ilTableNo].iSendOldData==TRUE && strlen(pclOldData)>0)
                                                        {
                                                            /* copy this old data field value to old data block, which will be appended to data block later on */
                                                            if ((pclS2 = GetDataField(pclOldData, i, ',')) != NULL)
                                                            {
                                                                strcat(pclOldFieldData, pclS2);
                                                                strcat(pclOldFieldData, ",");
                                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => ADD FLD<%s>=DAT<%s>,DAT(old)<%s> to out.-event."
                                                                        ,rgTM.prSection[ilTableNo].pcSectionName,pclTmpFieldName,pclS,pclS2);
                                                            }
                                                        }
                                                    } /*endif data matches, so adding fieldname and data */
                                                } /* endif does field content match ? */
                                            } /* endif get field data */
                                        }/* endif found field */
                                    }/* end for: all fields */

                                    /* check last character... */
                                    if (pclFieldNames[strlen(pclFieldNames)-1] == ',')
                                        pclFieldNames[strlen(pclFieldNames)-1] = 0x00;

                                    /* count no of fields... */
                                    ilCnt1 = GetNoOfElements(pclFieldNames, ',');   
                                    ilCnt2 = GetNoOfElements(pclFieldData, ',');
                        
                                    /* check last character... */
                                    if (ilCnt2 > ilCnt1)
                                        if (pclFieldData[strlen(pclFieldData)-1] == ',')
                                            pclFieldData[strlen(pclFieldData)-1] = 0x00;

                                    /* set flag */
                                    ilMatch=1;

                                    /****************************/
                                    /* following the keys-check */
                                    /****************************/
                                    for (i=rgTM.prSection[ilTableNo].iNoOfKeys-1; i>=0; i--)
                                    {
                                        dbg(DEBUG,"<HandleData> SEC.:<%s> => check key <%s>."
                                            ,rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].pcKeys[i]); 
                                        /* key in field-list? */
                                        if (strstr(pclFieldNames, rgTM.prSection[ilTableNo].pcKeys[i]) == NULL)
                                        {
                                            /* key is not in field-list... */
                                            /* compute index */
                                            if ((ilIdx = GetIndex(pclFields, rgTM.prSection[ilTableNo].pcKeys[i],',')) < 0)
                                            {
                                                /* Error getting index */
                                                /* break here */
                                                i = -1;
                                                ilMatch = 0;

                                                /* clear buffer */
                                                memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                                                memset((void*)pclFieldData, 0x00, iMAXIMUM);
                                            }
                                            else
                                            {
                                                /* now get Data for this field */
                                                if ((pclS = ACTIONGetDataField(pclDatablk, ilIdx, ',', rgTM.prSection[ilTableNo].iEmptyFieldHandling)) == NULL)
                                                {
                                                    /* Error getting data... */
                                                    /* break here */
                                                    i = -1;
                                                    ilMatch = 0;
                                                    /* clear buffer */
                                                    memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                                                    memset((void*)pclFieldData, 0x00, iMAXIMUM);
                                                }
                                                else
                                                {
                                                    /* there must be data */
                                                    if (!strlen(pclS))
                                                    {
                                                        i = -1;
                                                        ilMatch = 0;

                                                        /* clear buffer */
                                                        memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                                                        memset((void*)pclFieldData, 0x00, iMAXIMUM);
                                                    }
                                                    else
                                                    {
                                                        /* check data for valid conditions */
                                                        if (rgTM.prSection[ilTableNo].pcKeyConditions[i] != NULL)
                                                        {
                                                            /* save data to temporary buffer */
                                                            memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
                                                            strcpy(pclTmpBuf, pclS);

                                                            if ((ilRC = MatchCondition(pclTmpBuf, rgTM.prSection[ilTableNo].pcKeyConditions[i], rgTM.prSection[ilTableNo].piKeyConditionTypes[i])) != 1)
                                                            {
                                                                i = -1;
                                                                ilMatch = 0;

                                                                /* clear buffer */
                                                                memset((void*)pclFieldNames, 0x00, iMAX_BUF_SIZE);
                                                                memset((void*)pclFieldData, 0x00, iMAXIMUM);
                                                            }
                                                        }

                                                        if (ilMatch)
                                                        {
                                                            /* copy this to buffer... */
                                                            /* first fieldname.... */
                                                            memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
                                                            sprintf(pclTmpBuf, "%s,", rgTM.prSection[ilTableNo].pcKeys[i]);
                                                            if ((ilRC = InsertIntoString(pclFieldNames, 0, pclTmpBuf)) != 0)
                                                            {
                                                                /* Insert fail */
                                                                dbg(TRACE,"<HandleData> InsertIntoString (field) returns: <%d>! Terminating!", ilRC);
                                                                Terminate(0);
                                                            }

                                                            /*...then data */
                                                            memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
                                                            sprintf(pclTmpBuf, "%s,", pclS);
                                                            if ((ilRC = InsertIntoString(pclFieldData, 0, pclTmpBuf)) != 0)
                                                            {
                                                                /* Insert fail */
                                                                dbg(TRACE,"<HandleData> InsetIntoString (data) returns: <%d>! Terminating!", ilRC);
                                                                Terminate(0);
                                                            }
                                                        }
                                                    }
                                                }/*end else getting data*/
                                            }/*end else getting data*/
                                        }/* endif key in field list*/
                                    }/*end for all keys */

                                    /* check last character... */
                                    if (pclFieldNames[strlen(pclFieldNames)-1] == ',')
                                        pclFieldNames[strlen(pclFieldNames)-1] = 0x00;

                                    /* count no of fields... */
                                    ilCnt1 = GetNoOfElements(pclFieldNames, ',');   
                                    ilCnt2 = GetNoOfElements(pclFieldData, ',');
                        
                                    /* check last character... */
                                    if (ilCnt2 > ilCnt1)
                                        if (pclFieldData[strlen(pclFieldData)-1] == ',')
                                            pclFieldData[strlen(pclFieldData)-1] = 0x00;

                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => SEL.     = <%s>"
                                        ,rgTM.prSection[ilTableNo].pcSectionName, pclSelection);
                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => FIELDS   = <%s>"
                                        ,rgTM.prSection[ilTableNo].pcSectionName, pclFieldNames);
                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => DATA     = <%s>"
                                        ,rgTM.prSection[ilTableNo].pcSectionName, pclFieldData);
                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => DATA(old)= <%s>"
                                        ,rgTM.prSection[ilTableNo].pcSectionName, pclOldFieldData);

                                    /* schedule only if there is a field to schedule... */
                                    /*if (strlen(pclFieldNames))
                                    {*/
                                        if (!strlen(pclFieldNames))
                                            dbg(DEBUG,"<HandleData> SEC.:<%s> => event contains no fields to be sent!",rgTM.prSection[ilTableNo].pcSectionName); 
                                        /* iUSE_ALL_RECEIVED_FIELDS = only on empty fieldlist (no "fields" in action.cfg defined) */
                                        /*if (rgTM.prSection[ilTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS || strlen(pclFieldNames)>0)*/
                                        if (rgTM.prSection[ilTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS)
                                        {
                                            dbg(DEBUG,"<HandleData> SEC.:<%s> => check changed fields.",rgTM.prSection[ilTableNo].pcSectionName); 
                                            ilGetRc = CheckChangedFields(prlCmdblk->command, pclFields, pclFieldNames,
                                                                 pclDatablk, pclOldData,ilTableNo);

                                            if (ilGetRc != RC_SUCCESS)
                                            {
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => REJECTED DUE TO NO CHANGED FIELDS",rgTM.prSection[ilTableNo].pcSectionName); 
                                            } /* end if */
                                                    
                                            if (ilGetRc == RC_SUCCESS)
                                            {
                                                /* NEW FILTER FUNCTION */
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => check data filter.",rgTM.prSection[ilTableNo].pcSectionName); 
                                                ilGetRc = CheckDataFilter(rgTM.prSection[ilTableNo].pcSectionName,prlCmdblk->command, pclFields, pclDatablk,rgTM.prSection[ilTableNo].DataFilter);
                                                if (ilGetRc != RC_SUCCESS)
                                                {
                                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => REJECTED DUE TO DATA FILTER",rgTM.prSection[ilTableNo].pcSectionName); 
                                                } /* end if */
                                            } /* end if */
                                                
                                            if (ilGetRc == RC_SUCCESS)
                                            {
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => check data mapping.",rgTM.prSection[ilTableNo].pcSectionName); 
                                                CheckDataMapping(rgTM.prSection[ilTableNo].DataMapping,
                                                         prlCmdblk->command, pclFields, pclFieldNames,
                                                         pclDatablk, pclFieldData);
                                                CheckDataMapping(rgTM.prSection[ilTableNo].DataMapping,prlCmdblk->command,  pclFields, pclFieldNames,pclDatablk, pclOldFieldData);
                                            }
                                        }

                                        if (ilGetRc == RC_SUCCESS)
                                        {
                                            if (rgTM.prSection[ilTableNo].iSendOldData)
                                            {
                                                if ((pclOldFieldData != NULL) && (*pclOldFieldData != '\0') && strlen(pclOldFieldData))
                                                {
                                                    /* restore old data block in pointer on record data */
                                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => appending [\\n] and [old-data-block] to event data!"
                                                        ,rgTM.prSection[ilTableNo].pcSectionName); 
                                                    strcat(pclFieldData,"\n");
                                                    strcat(pclFieldData,pclOldFieldData);
                                                }
                                            }
                                            if (rgTM.prSection[ilTableNo].iUseSndCmd)
                                            {
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => sending event for section <%s> with cfg-cmd <%s>"
                                                    ,rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].pcSndCmd);
                                                ilRC = _ScheduleAction(rgTM.prSection[ilTableNo].pcSndCmd, prlCmdblk, pclSelection, pclFieldNames, pclFieldData,ilTableNo);
                                            }
                                            else
                                            {
                                                dbg(DEBUG,"<HandleData> SEC.:<%s> => sending event for section <%s> with org-cmd <%s>"
                                                    ,rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].pcSectionName,rgTM.prSection[ilTableNo].pcSndCmd);
                                                ilRC = _ScheduleAction(prlCmdblk->command, prlCmdblk, pclSelection, pclFieldNames, pclFieldData, ilTableNo);
                                            }   
                                        } /* end if */
                                    /*}
                                    else
                                    {
                                        dbg(DEBUG,"<HandleData> SEC.:<%s> => can't schedule", rgTM.prSection[ilTableNo].pcSectionName);
                                    }*/
                                }
                                else
                                {
                                    dbg(DEBUG,"<HandleData> SEC.:<%s> => ignore this field-/datalist",rgTM.prSection[ilTableNo].pcSectionName );
                                }
                            }
                            else
                            {
                                dbg(DEBUG,"<HandleData> SEC.:<%s> => rejected due to originator/destination filter.",rgTM.prSection[ilTableNo].pcSectionName);
                            }
                 }
                 else
                 {
                     dbg(DEBUG,"<HandleData> SEC.:<%s> => can't find command <%s>",rgTM.prSection[ilTableNo].pcSectionName , prlCmdblk->command); 
                 }
                }
                else
                {
                    dbg(DEBUG,"<HandleData> SEC.:<%s> => can't find table <%s>",rgTM.prSection[ilTableNo].pcSectionName ,prlCmdblk->obj_name); 
                }
        }   
        }/* end else normal,static or dynamic section */
    }/* end for all tables */
 }
 return ilRC;
}

/******************************************************************************/
/* The schedult action routine                                                */
/******************************************************************************/
static int _ScheduleAction(char *pcpCommand, CMDBLK *pcpCmdblk, 
               char *pcpSelection, char *pcpFields,char *pcpData, int ipTableNo)
{
  int       ilRC = RC_SUCCESS;
  int       ilLen;
    int ilTmpLen = 0;
    int ilFldLstLen = 0;
    int ilDatLstLen = 0;
    int ilOldData = FALSE;
  EVENT     *prlOutEvent = NULL;
  BC_HEAD   *prlOutBCHead = NULL;
  CMDBLK    *prlOutCmdblk = NULL;
  int            ilCurrentField =0;
  char           *pclSndFields = NULL; 
  char           *pclSndData = NULL;
  char           pclFieldBuf[8];
  char           pclDataBuf[iMAXIMUM];
  char           pclOldDataBuf[iMAXIMUM];
  char           pclOldData[iMAXIMUM];
  char           pclSndOldData[iMAXIMUM];
  char            *pclTmpPtr=NULL;
  int            ilFieldNo = 0;
  int            ilFieldCnt = 0;
  int            ilFldNbr = 0;
  int            ilMaxfields =0;
  int            ilSendPrio = PRIORITY_4;
 
  /* init */
  /* SetNullToBlank(pcpData); */ 
  memset(pclOldDataBuf,0x00,iMAXIMUM);
  memset(pclOldData,0x00,iMAXIMUM);
  memset(pclSndOldData,0x00,iMAXIMUM);

    /* BY BERNI 23.10.2000 */
  /*ilFldLstLen= strlen(pcpFields) + 1 + 128;*/
  ilFldLstLen= strlen(pcpFields) + 8;
  /*ilDatLstLen= strlen(pcpData) + 1 + 128;*/
    /* pcpData already contains [DATA\nOLDDATA] */
  ilDatLstLen= (strlen(pcpData) * 4) + 256;

    /* allocate (lentgh of fields) */
  pclSndFields = (char*)calloc(1,ilFldLstLen); 
    /* allocate (lentgh of data) + (length of old data) + (\n) + (few bytes safety) */
  pclSndData = (char*)calloc(1,ilDatLstLen);
    /* END BERNI */
  /*dbg(DEBUG,"<ScheduleAction> calloc-info(get memory): FLD-len<%d>, DAT-len<%d> (incl. old-data)!",ilFldLstLen,ilDatLstLen);*/
      
    /* separating the old data from pcpData-buffer */
    pclTmpPtr=strstr(pcpData,"\n");
    if(pclTmpPtr != NULL)
    {
      *pclTmpPtr='\0';
       pclTmpPtr++;
         strcpy(pclOldData,pclTmpPtr);
       if(pclOldData[strlen(pclOldData)-1]==',')
         pclOldData[strlen(pclOldData)-1]='\0';
         if (strlen(pclOldData) > 0)
            ilOldData = TRUE;
    }
    #if 0
    dbg(TRACE,"<ScheduleAction>Content before filter!");
    dbg(TRACE,"<ScheduleAction>Field    = \n<%s>",pcpFields);
    dbg(TRACE,"<ScheduleAction>Data     = \n<%s>",pcpData);
    dbg(TRACE,"<ScheduleAction>Data(old)= \n<%s>",pclOldData);
    #endif

    /* this if is only entered when the fieldlist is not empty (iUSE_ALL_RECEIVED_FIELDS)*/
  if(rgTM.prSection[ipTableNo].iNoOfFields != iUSE_ALL_RECEIVED_FIELDS)
  {
        for(ilCurrentField = 0; ilCurrentField< rgTM.prSection[ipTableNo].iNoOfFields; ilCurrentField++)
        {
            /*lookingfor a sign*/ 
            if((strstr(pcpFields,rgTM.prSection[ipTableNo].pcFields[ilCurrentField]) != NULL)
                 &&(((rgTM.prSection[ipTableNo].pcFieldResponseType[ilCurrentField] =='?')
                 &&(rgTM.prSection[ipTableNo].pcFieldResponseFlag[ilCurrentField] == '1'))
                ||(rgTM.prSection[ipTableNo].pcFieldResponseType[ilCurrentField] == 0)
                ||(rgTM.prSection[ipTableNo].pcFieldResponseType[ilCurrentField] == '!')))
            {
                if( ilFieldCnt > 0 )
                {
                    strcat(pclSndFields,",");
                    strcat(pclSndData,",");
                    /* only put it together when old-data is to be sent and if it exists */
                    if(rgTM.prSection[ipTableNo].iSendOldData == TRUE && ilOldData==TRUE)
                                    strcat(pclSndOldData,",");
                }            
                ilFldNbr = get_item_no(pcpFields,rgTM.prSection[ipTableNo].pcFields[ilCurrentField],5)+1;
                if (ilFldNbr > 0)
                {
                    get_real_item(pclDataBuf,pcpData,ilFldNbr);
                 
                    strcat(pclSndFields,rgTM.prSection[ipTableNo].pcFields[ilCurrentField]);
                    strcat(pclSndData,pclDataBuf);

                    /* only put it together when old-data is to be sent and if it exists */
                    if(rgTM.prSection[ipTableNo].iSendOldData == TRUE && ilOldData==TRUE)
                    {
                        get_real_item(pclOldDataBuf,pclOldData,ilFldNbr);         
                        strcat(pclSndOldData,pclOldDataBuf);
                    }

                    ilFieldCnt++;
                } /* end if */
            }/*end if*/
                 
            /*reset response flag*/
            if( rgTM.prSection[ipTableNo].pcFieldResponseFlag[ilCurrentField]!=NULL)
            {
                rgTM.prSection[ipTableNo].pcFieldResponseFlag[ilCurrentField] = '0';
            }
        } /*end for */  
  }/*end if*/

    /*******************************************************/
    /* putting together fields/data/old-data for out.-event */
    /*******************************************************/
    /* copy values of fields,data,old-data in case we did not */
    /* have a fieldlist configured and therefore the above */
    /* function which puts in field by field and data by data */
    /* was not used!! */

    if(*pclSndFields=='\0')
        strcpy(pclSndFields,pcpFields);
    if(*pclSndData=='\0')
        strcpy(pclSndData,pcpData);
    if(*pclSndOldData=='\0')
        strcpy(pclSndOldData,pclOldData);

    /* pcpData has been cut above by setting a 0x00 on \n of pcpData-buffer */
    /* now we need to put it togehter again if old-data is to be sent */
    /* (buffer is large enough because calloc was done before separation of data & old-data) */
    if(rgTM.prSection[ipTableNo].iSendOldData == TRUE)
    {
        strcat(pclSndData,"\n");
        if (ilOldData == TRUE)
            strcat(pclSndData,pclSndOldData);
    }
    #if 0
    dbg(TRACE,"<ScheduleAction>Content after filter!");
    dbg(TRACE,"<ScheduleAction>SndField = \n<%s>",pclSndFields);
    dbg(TRACE,"<ScheduleAction>SndData  = \n<%s>",pclSndData);
    dbg(TRACE,"<ScheduleAction>Data(old)= \n<%s>",pclSndOldData);
    #endif
  
    /* BY BERNI 23.10.2000 */
    ilTmpLen = strlen(pclSndFields) + 1;
    if (ilTmpLen > ilFldLstLen)
    {
        dbg(TRACE,"<ScheduleAction> ALLOC BUFFER OVERFLOW: SndFields %d=>>%d",ilFldLstLen,ilTmpLen);
    } /* end if */
    ilTmpLen = strlen(pclSndData) + 1;
    if (ilTmpLen > ilDatLstLen)
    {
        dbg(TRACE,"<ScheduleAction> ALLOC BUFFER OVERFLOW: SndData %d=>>%d",ilDatLstLen,ilTmpLen);
    } /* end if */
    /* END BERNI */
 
  /*dbg(DEBUG,"<ScheduleAction> calloc-info(use memory): FLD-len<%d>, DAT-len<%d> (incl. old-data)!"
        ,strlen(pclSndFields),strlen(pclSndData)+strlen(pclSndOldData));*/

  /* preparing out event */
  /* calculate len of out-event */
  ilLen = sizeof(EVENT) + sizeof(BC_HEAD) +sizeof(CMDBLK) +strlen(pcpSelection) + strlen(pclSndFields)+strlen(pclSndData) + 128;
  /* get memory for out event */
  prlOutEvent = NULL;
  if ((prlOutEvent = (EVENT*)malloc((size_t)ilLen)) == NULL)
  {
    dbg(TRACE, "<ScheduleAction> cannot allocate %d bytes for out event", ilLen);
    return RC_FAIL;
  }
 
  /* set memory to 0x00 */
  memset((void*)prlOutEvent, 0x00, ilLen);
 
  /* set structure members */
  prlOutEvent->type         = SYS_EVENT;
  prlOutEvent->command          = rgTM.prSection[ipTableNo].iCommandType;
  prlOutEvent->originator   = prgEvent->originator;
  prlOutEvent->retry_count  = 0;    
  prlOutEvent->data_length  = ilLen - sizeof(EVENT);
  prlOutEvent->data_offset  = sizeof(EVENT);

  /* BC-Head -Structure */
  prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
  prlOutBCHead->rc = RC_SUCCESS;
 
  /* Cmdblk-Structure */
  prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
  strcpy(prlOutCmdblk->command, pcpCommand);
  strcpy(prlOutCmdblk->obj_name, pcpCmdblk->obj_name);
 
    if (rgTM.prSection[ipTableNo].iKeepOrgTwstart == FALSE)
    {
        sprintf(prlOutCmdblk->tw_start, "%s,%d,%d", pcpCommand, rgTM.prSection[ipTableNo].iCmdNo, rgTM.prSection[ipTableNo].iTabNo);
    }
    else
    {
         sprintf(prlOutCmdblk->tw_start, "%s",pcpCmdblk->tw_start);
    }
    if (rgTM.prSection[ipTableNo].iClearTwstart == TRUE)
           strcpy(prlOutCmdblk->tw_start,"");
 
  strcpy(prlOutCmdblk->tw_end, pcpCmdblk->tw_end);
  dbg(TRACE,"----------------------------------------------------------------");
  dbg(TRACE,"<ScheduleAction> tw_start       : <%s>", prlOutCmdblk->tw_start);
  dbg(TRACE,"<ScheduleAction> tw_end         : <%s>", prlOutCmdblk->tw_end);

  /* Selection */
  strcpy(prlOutCmdblk->data, pcpSelection);

  /* Fields */
  strcpy((prlOutCmdblk->data+strlen(pcpSelection)+1), pclSndFields);

  /* Datablk */
  strcpy((prlOutCmdblk->data+strlen(pcpSelection)+strlen(pclSndFields)+2),pclSndData);
  /* send now */
  dbg(TRACE,"<ScheduleAction> Sel.           : <%s>", pcpSelection);
  dbg(TRACE,"<ScheduleAction> Fields         : <%s>", pclSndFields);
  dbg(TRACE,"<ScheduleAction> Data(incl. old): <%s>", pclSndData);
    if (rgTM.prSection[ipTableNo].iPrio == iORG_EVENT_PRIO)
    {
        ilSendPrio = prgItem->priority;
    }
    else
    {
        ilSendPrio = rgTM.prSection[ipTableNo].iPrio;
    }
  dbg(TRACE,"<ScheduleAction> to route       : <%d> on prio: <%d>",rgTM.prSection[ipTableNo].iModID,ilSendPrio);
  dbg(TRACE,"----------------------------------------------------------------");
  /*if(( ilRC = que(QUE_PUT, rgTM.prSection[ipTableNo].iModID, mod_id, 4,ilLen, (char*)prlOutEvent))!= RC_SUCCESS)*/
  if(( ilRC = que(QUE_PUT, rgTM.prSection[ipTableNo].iModID, mod_id, ilSendPrio,ilLen, (char*)prlOutEvent))!= RC_SUCCESS)
  {
        dbg(TRACE,"<ScheduleAction> que QUE_PUT failed with <%d>", ilRC);
        /* JWE PRF-6279 */
        dbg(TRACE,"<ScheduleAction> Setting section to NOT_VALID. Only restart of ACTION can enable this section again.");
        rgTM.prSection[ipTableNo].iValidSection = iNOT_VALID;
  }
    else
    {
        ulgEventsSent++;    
    }
  /* delete memory for out-event... */
  free((void*)prlOutEvent);
  free((void*)pclSndFields); 
  free((void*)pclSndData);
      
  /* everything looks good (hope so)... */
  return ilRC;
}

/******************************************************************************/
/* The match condition routine                                                */
/******************************************************************************/
static int MatchCondition(char *pcpData, char *pcpCondition, int ipType)
{
  int       ilRC1;
  int       ilRC2;
  int       ilCondition;
  char      *pclS;
  char      pclTmpBuf[iMAX_BUF_SIZE];

  /* only for valid data */
  if (!strlen(pcpData))
    return 0;

  dbg(DEBUG,"<MatchCondition> calling with: <%s> <%s> (%d)", pcpData, pcpCondition, ipType);
  
  if ((strstr(pcpCondition,"&&") == NULL) &&
      (strstr(pcpCondition,"||") == NULL))
    {
      switch (ipType)
    {
    case iDYNAMIC_TIME:
                /* build timestamp */
      pclS = AddToCurrentUtcTime(iMINUTES, atoi(&pcpCondition[1]), iSTART);
      dbg(DEBUG,"<MatchCondition> compare <%s> and <%s>", pcpData, pclS); 
      
                /* this is for integers */
      switch (pcpCondition[0])
        {
        case '<':
          return strcmp(pcpData, pclS) < 0 ? 1 : 0; 
        case '>':
          return strcmp(pcpData, pclS) > 0 ? 1 : 0; 
        case '=':
          return !strcmp(pcpData, pclS) ? 1 : 0;
        default:
          return -1;
        }

    case iINTEGER:
                /* this is for integers */
      switch (pcpCondition[0])
        {
        case '<':
          if (pcpCondition[1] == '>')
        {
          /* unequal */
          return atol(pcpData) != atol(&pcpCondition[2]) ? 1 : 0;
        }
          else
        {
          /* smaller than */
          return atol(pcpData) < atol(&pcpCondition[1]) ? 1 : 0;
        }
        case '>':
          {
        /* greater than */
        return atol(pcpData) > atol(&pcpCondition[1]) ? 1 : 0;
          }
        case '=':
          {
        /* equal */
        return atol(pcpData) == atol(&pcpCondition[1]) ? 1 : 0;
          }
        default:
          /* Hae? */
          return -2;
        }

    case iSTRING:
                /* this is for strings... */
      if ((ilRC1 = MatchPattern(pcpData, pcpCondition)) == 1)
        return 1;
      return 0;

    case iTIME:
                /* this is for timestamps */
      switch (pcpCondition[0])
        {
        case '<':
          if (pcpCondition[1] == '>')
        {
          /* unequal */
          return strcmp(pcpData, &pcpCondition[2]) ? 1 : 0;
        }
          else
        {
          /* smaller than */
          /* here we must check length... */
          if (strlen(pcpData) != strlen(&pcpCondition[1]))
            return -3;
          else
            return strcmp(pcpData, &pcpCondition[1]) < 0 ? 1 : 0;   
        }
        case '>':
          {
        /* greater than */
        /* here we must check length... */
        if (strlen(pcpData) != strlen(&pcpCondition[1]))
          return -4;
        else
          return strcmp(pcpData, &pcpCondition[1]) > 0 ? 1 : 0; 
          }
        case '=':
          {
        /* equal */
        return !strcmp(pcpData, &pcpCondition[1]) ? 1 : 0;
          }
        default:
          /* Hae? */
          return -5;
        }

    default:
                /* unknown type */
      return -6;
    }
    }
  else
    {
      /* find condition */
      if ((pclS = strstr(pcpCondition, "&&")) != NULL)
    ilCondition = iAND;
      else if ((pclS = strstr(pcpCondition, "||")) != NULL)
    ilCondition = iOR;
      else
    return -7;

      /* check both part of condition */
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      strncpy(pclTmpBuf, pcpCondition, (pclS - pcpCondition));
      ilRC1 = MatchCondition(pcpData, pclTmpBuf, ipType);
      memset((void*)pclTmpBuf, 0x00, iMAX_BUF_SIZE);
      strcpy(pclTmpBuf, (pclS+2));
      ilRC2 = MatchCondition(pcpData, pclTmpBuf, ipType);

      /* check result */
      if (ilCondition == iAND)
    {
      if (ilRC1 == 1 && ilRC2 == 1)
        return 1;
      return 0;
    }
      else
    {
      if (ilRC1 == 1 || ilRC2 == 1)
        return 1;
      return 0;
    }
    }
}

/******************************************************************************/
/* The create valid file routine                                              */
/******************************************************************************/
static int CreateValidFile(void)        
{
    int     i;
    FILE        *pfFh = NULL;

    if ((pfFh = fopen(sVALID_FILE_NAME, "w")) == NULL)
    {
        dbg(TRACE,"<CreateValidFile> cannot open");
        return RC_FAIL;
    }
    else
    {
        /* write header */
        fprintf(pfFh,"*TableNumber Type CmdNo TabNo\n");

        /* write all.. */
        for (i=0; i<rgTM.iNoOfSections; i++)
            fprintf(pfFh, "%d %s %d %d\n", i, rgTM.prSection[i].iValidSection == iVALID ? "VALID" : "NOT_VALID", rgTM.prSection[i].iCmdNo, rgTM.prSection[i].iTabNo);
        
        /* close file now */
        fclose (pfFh);

        /* everything looks good */
        return RC_SUCCESS;
    }
}

/******************************************************************************/
/* The CheckFields routine                                                    */
/******************************************************************************/
static int CheckFields(int ipNoRF, char *pcpReceivedFields, char *pcpConfiguredFields)
{
    int     i;
    char        *pclS = NULL;

    /* for all received fields */
    for (i=0; i<ipNoRF; i++)
    {
        /* get them separatly */
        pclS = GetDataField(pcpReceivedFields, i, ',');
        dbg(DEBUG,"<CheckFields> compare <%s>, <%s>", pcpConfiguredFields, pclS);
        if (strstr(pcpConfiguredFields, pclS) == NULL)
            return TRUE;
    }

    /* i'm an optimist, ho ho ho */
    return FALSE;
}


/******************************************************************************/
/******************************************************************************/


/***************************************************************************/
/* SetNullToBlank()                                                        */
/* IN/OUT : char *pcpData - String                                         */
/* Description: Replace all ",," to ", ," in ParameterString               */
/* History : rkl 19.05.00  Written                                         */
/***************************************************************************/ 
static void SetNullToBlank(char *pcpData)
{
  char pclTmpData[iMAXIMUM];
  char *pclS;
  char *pclD;
  
  pclS = pcpData;
  pclD = pclTmpData;       
  do{
      if((*pclS == ',') && (*(pclS+1) ==','))
        {
      *pclD++ = *pclS++;
      *pclD++ = ' ';
        }else{
      *pclD++ = *pclS++;
        }
    }while((*pclS) != '\0');
  
  *pclD = '\0';

  strcpy(pcpData,pclTmpData);
  return;
  

} 

#if 0  
int ilSrcPos = 0;
int ilSrcLen = 0;
int ilDstPos = 0;
int ilComma = TRUE; 
  strcpy(pclCopy,pcpData); 
  ilSrcLen = strlen(pclCopy); 
  


/*????????????????????????????????????????????????????????????????*/
  for (ilSrcPos = 0; ilSrcPos < ilSrcLen; ilSrcPos++)
  {
    if ((pclCopy[ilSrcPos] == ',') || (pclCopy[ilSrcPos] == '\0'))
    {
      if (ilComma == TRUE)
      {
    pcpData[ilDstPos] = ' ';
    ilDstPos++; 
      } /* end if */
      ilComma = TRUE;
    } /* end if */
    else
    {
      ilComma = FALSE;
    } /* end else */
    pcpData[ilDstPos] = pclCopy[ilSrcPos];
    ilDstPos++;
    pcpData[ilSrcLen]='\0';
  } /* end for */

/*????????????????????????????????????????????????????????????????*/

  
  return;


} /* end SetNullToBlank */
#endif 
/******************************************************************************/
/******************************************************************************/
static int CheckDataFilter(char *pcpSection,char *pcpCedaCmd, char *pcpFields, char *pcpData, char *pcpFilter)
{

int ilRC = RC_SUCCESS;

int ilCnt  = 0;
char pclFldDat2[1024];
char pcldummy[8];
/* EO CHANGES */

int ilHitAll = TRUE;
int ilBreakOut = FALSE;
int i = 0;
int ilFldNbr = 0;
int ilLen = 0;
int ilLen1 = 0;
int ilLen2 = 0;
int ilDatLen = 0;
int ilIntVal1 = 0;
int ilIntVal2 = 0;
int ilKeys = 0;
int ilHits = 0;
int ilStampFormat = 1;
char pclFldNam[8];
char pclCmdKey[8];
char pclChkKey[8];
char pclActFilter[2048];
char pclChrVal1[1024];
char pclChrVal2[1024];
char pclChkVal1[1024];
char pclChkVal2[1024];
char pclFldDat[1024];
char pclTime1[32];
char pclTime2[32];
char *pclRulBgn = NULL;
char *pclRulEnd = NULL;
char *pclBgnPtr = NULL;
char *pclEndPtr = NULL;
if (strlen(pcpFilter) > 0)
{
    dbg(DEBUG,"<CheckDataFilter>:<%s> entering",pcpSection);
    strcpy(pclActFilter,pcpFilter);
    /*
        dbg(TRACE,"<CheckDataFilter>:<%s> CMD <%s>",pcpSection, pcpCedaCmd);
        dbg(TRACE,"<CheckDataFilter>:<%s> FLD <%s>",pcpSection, pcpFields);
        dbg(TRACE,"<CheckDataFilter>:<%s> DAT <%s>",pcpSection, pcpData);
    */
    pclRulBgn = pclActFilter;
    pclRulEnd = strstr(pclRulBgn,"|");
    /* CHANGES BY MOS 17.08.00 */
    /* Setting pclRulEnd to NULL prevents execution of */
    /* all data filters except NOT */
    /*
    if (pclRulEnd != NULL)
    {
    *pclRulEnd = 0x00;
    } 
    ilLen = get_real_item(pclCmdKey,pclRulBgn,2);
    if (ilLen > 0)
    {
        if (strcmp(pclCmdKey,"NOT") == 0)
        {
            get_real_item(pclCmdKey,pclRulBgn,3);
            if (strcmp(pclCmdKey,pcpCedaCmd) == 0)
            {
                get_real_item(pclFldNam,pclRulBgn,4);
                get_real_item(pclChrVal1,pclRulBgn,5);
                ilFldNbr = get_item_no(pcpFields,pclFldNam,5) + 1;
                if (ilFldNbr > 0)
                {
                    get_real_item(pclFldDat,pcpData,ilFldNbr);
                    if (strcmp(pclFldDat,pclChrVal1) == 0)
                    {
                        dbg(DEBUG,"<CheckDataFilter>:<%s> TO IGNORE: <NOT> <%s> <%s> <%s>",pcpSection,
                        pclCmdKey,pclFldNam,pclFldDat);
                        return RC_FAIL;
                    } 
                } 
            }
        } 
    }*/ 

    /* ORIG VERSION */
    /* NOW CHECk FOR NOT COMMAND and process the following block*/
    if (strstr(pclRulBgn,"NOT") != NULL) 
    {
        if (pclRulEnd != NULL)
        {
        *pclRulEnd = 0x00;
        } /* end if*/
        ilLen = get_real_item(pclCmdKey,pclRulBgn,2);
        if (ilLen > 0)
        {
            if (strcmp(pclCmdKey,"NOT") == 0)
            {
                get_real_item(pclCmdKey,pclRulBgn,3);
                if (strcmp(pclCmdKey,pcpCedaCmd) == 0)
                {
                    get_real_item(pclFldNam,pclRulBgn,4);
                    get_real_item(pclChrVal1,pclRulBgn,5);
                    ilFldNbr = get_item_no(pcpFields,pclFldNam,5) + 1;
                    if (ilFldNbr > 0)
                    {
                        get_real_item(pclFldDat,pcpData,ilFldNbr);
                        if (strcmp(pclFldDat,pclChrVal1) == 0)
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> TO IGNORE: <NOT> <%s> <%s> <%s>",pcpSection,
                                pclCmdKey,pclFldNam,pclFldDat);
                            return RC_FAIL;
                        } /* end if */
                    } /* end if */
                } /* end if */
             } /* end if */
            } /* end if */
        } /* If command == NOT */
        /******* END OF CHANGES **********/

        get_real_item(pclCmdKey,pclRulBgn,1);
        if (strcmp(pclCmdKey,"ONE") == 0)
        {
            ilHitAll = FALSE;
            dbg(DEBUG,"<CheckDataFilter>:<%s> ONLY ONE MUST HIT",pcpSection);
        } /* end if */

        while ((pclRulEnd != NULL) && (ilBreakOut == FALSE))
        {
            pclRulBgn = pclRulEnd + 1;
            pclRulEnd = strstr(pclRulBgn,"|");
            if (pclRulEnd != NULL)
            {
                *pclRulEnd = 0x00;
            } /* end if */
            ilLen = get_real_item(pclFldNam,pclRulBgn,1);
            if (ilLen > 0)
            {

                /* CHANGES BY MOS FOR CHECKING OF  VALUES BETWEEN TO LIMITS*/
                /* VALUE eg: MORE|002,004,BETWEEN,CKIF,CKIT */
                /* PLEASE BE SURE TO HAVE THE FLIGHT COJFIGURED WITH CKIF=SEND AND CKIT=SEND!!!! */
                if (strstr(pclRulBgn,"BETWEEN") != NULL)
                {
                        /* Are the fields in the list?*/
                        ilLen = get_real_item(pclFldNam,pclRulBgn,4);
                        ilFldNbr = get_item_no(pcpFields,pclFldNam,5) + 1;
                        /*Get the value*/
                        ilLen1 = get_real_item(pclChrVal1,pcpData,ilFldNbr);

                        /* Also the second field */
                        ilLen = get_real_item(pclFldNam,pclRulBgn,5);
                        ilFldNbr = get_item_no(pcpFields,pclFldNam,5) + 1;
                        /*Get the value*/
                        ilLen2 = get_real_item(pclChrVal2,pcpData,ilFldNbr);

                        /* YES, NOW CHECK FOR the values */
                     /* NOW CHECK IF VAL1 is < than pclFldDat and val 2 > than pclFldDat */
                        ilDatLen = get_real_item(pclFldDat,pclRulBgn,1);    
                        ilDatLen = get_real_item(pclFldDat2,pclRulBgn,2);   
                        for (ilCnt=atoi(pclFldDat); ilCnt<=atoi(pclFldDat2);ilCnt++)
                        {
                            sprintf(pcldummy,"%d",ilCnt);
                            sprintf(pclChrVal1,"%d",atoi(pclChrVal1));
                            sprintf(pclChrVal2,"%d",atoi(pclChrVal2));
                            if ((strcmp(pclChrVal1,pcldummy) <= 0) && (strcmp(pclChrVal2,pcldummy) >= 0))
                            {
                                ilHits++;
                                ilRC = RC_SUCCESS;
                                break;
                            }
                            else
                            {
                                dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT BETWEEN <%s> AND <%s>",pcpSection,pclFldDat,pclChrVal1,pclChrVal2);
                                ilRC = RC_FAIL;
                            } /* end else */
                     } /* for all given values */
                 } /* BETWEEN ?? */ 
                /* EO CHANGES BY MOS FOR BETWEEN SWITCH*/

                ilFldNbr = get_item_no(pcpFields,pclFldNam,5) + 1;
                if (ilFldNbr > 0)
                {
                        get_real_item(pclCmdKey,pclRulBgn,2);
                        sprintf(pclChkKey,",%s,",pclCmdKey);
                        ilLen1 = get_real_item(pclChrVal1,pclRulBgn,3);
                        ilLen2 = get_real_item(pclChrVal2,pclRulBgn,4);
                        ilDatLen = get_real_item(pclFldDat,pcpData,ilFldNbr);
                        if (strstr(",TC,TCM,TCH,TCHU,TCD,DC,DCM,DCH,DCD,DCA,DCAU", pclChkKey) != NULL)
                        {
                            if (ilDatLen > 0)
                            {
                                ilKeys++;
                                if (pclCmdKey[0] == 'T')
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s>: TIME CONTROL",pcpSection, pclCmdKey);
                                } /* end if */
                                else
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s>: DATE CONTROL",pcpSection, pclCmdKey);
                                } /* end else */
                                dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> --> VALUE=<%s> LOW=<%s> HIGH=<%s>"
                                                                  ,pcpSection,pclFldNam,pclFldDat,pclChrVal1,pclChrVal2);
                                ilIntVal1 = atoi(pclChrVal1) * 60;
                                ilIntVal2 = atoi(pclChrVal2) * 60;
                                if (pclCmdKey[2] == 'H')
                                {
                                    ilIntVal1 *= 60;
                                    ilIntVal2 *= 60;
                                } /* end if */
                                if (pclCmdKey[2] == 'D')
                                {
                                    ilIntVal1 *= 60 * 24;
                                    ilIntVal2 *= 60 * 24;
                                } /* end if */
                                
                                /* Check if we use actual day as config-entry (from DCA or DCAU)*/
                                if (pclCmdKey[2] == 'A')
                                {
                                    /* set format for GetServerTimeStamp()-function to only get date part */
                                    ilStampFormat = 2;
                                }
                                else
                                {
                                    /* set format for GetServerTimeStamp()-function to only get date part */
                                    ilStampFormat = 1;
                                }

                                /* Get date stamps in local - default */
                                GetServerTimeStamp("LOC",ilStampFormat,ilIntVal1,pclTime1);
                                GetServerTimeStamp("LOC",ilStampFormat,ilIntVal2,pclTime2);
                                if (pclCmdKey[0] == 'D')
                                {
                                    /* Use "000000" for lower limit in day check and "235959" for upper limit */
                                    if (pclCmdKey[2] != 'A')
                                    {
                                        strcpy(&pclTime1[8],"000000");
                                        strcpy(&pclTime2[8],"235959");
                                    }
                                    /* Now start calculation from actual day midnight "000000" for both limits (low/high) */
                                    /* Example: ONE|STOA,DCA,-30,1440 ==> STOA must be between today"000000" -30min. and today"000000"+1440 min.*/
                                    else
                                    {
                                        /* Get date stamps in UTC */
                                        if (pclCmdKey[3] == 'U')
                                        {
                                            GetServerTimeStamp("UTC",ilStampFormat,0,pclTime1);
                                            GetServerTimeStamp("UTC",ilStampFormat,0,pclTime2);
                                        }
                                        else
                                        {
                                            GetServerTimeStamp("LOC",ilStampFormat,0,pclTime1);
                                            GetServerTimeStamp("LOC",ilStampFormat,0,pclTime2);
                                        }
                                        strcpy(&pclTime1[8],"000000");
                                        AddSecondsToCEDATime (pclTime1,ilIntVal1,1);
                                        strcpy(&pclTime2[8],"000000");
                                        AddSecondsToCEDATime (pclTime2,ilIntVal2,1);
                                    }
                                } /* end if */

                                /* MEI 25-MAY-2010 */
                                if (pclCmdKey[0] == 'T')
                                {
                                    if (pclCmdKey[3] == 'U')
                                    {
                                        GetServerTimeStamp("UTC",ilStampFormat,ilIntVal1,pclTime1);
                                        GetServerTimeStamp("UTC",ilStampFormat,ilIntVal2,pclTime2);
                                    }
                                }

                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK RANGE (%s) < (%s) > (%s)",pcpSection,pclTime1,pclFldDat,pclTime2);

                            ilHits++;
                            if ((strcmp(pclFldDat,pclTime1) < 0) || (strcmp(pclFldDat,pclTime2) > 0))
                            {
                                dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> TIME OUT OF RANGE",pcpSection,pclFldNam);
                                ilRC = RC_FAIL;
                                ilHits--;
                            } /* end if */
                        } /* end if */
                    } /* end if */
                        else if ((strcmp(pclCmdKey,"=") == 0) ||
                             (strcmp(pclCmdKey,"==") == 0) ||
                             (strcmp(pclCmdKey,"EQUAL") == 0))
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK EQUALITY",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilKeys++;
                            if ((strcmp(pclChrVal1,pclFldDat) == 0) ||
                                    (strcmp(pclChrVal2,pclFldDat) == 0))
                                {
                                    ilHits++;
                                    ilRC = RC_SUCCESS;
                                } /* end if */
                            else
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT EQUAL",pcpSection,pclFldNam);
                                    ilRC = RC_FAIL;
                                } /* end else */
                        } /* end else if */
                        else if (strcmp(pclCmdKey,"IN") == 0)
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK DATALIST",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilKeys++;
                            sprintf(pclChkKey,";%s;",pclFldDat);
                            sprintf(pclChkVal1,";%s;",pclChrVal1);
                            sprintf(pclChkVal2,";%s;",pclChrVal2);
                            if ((strstr(pclChkVal1,pclChkKey) != NULL) ||
                                    (strstr(pclChkVal2,pclChkKey) != NULL))
                                {
                                    ilHits++;
                                    ilRC = RC_SUCCESS;
                                } /* end if */
                            else
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT IN LIST",pcpSection,pclFldNam);
                                    ilRC = RC_FAIL;
                                } /* end else */
                        } /* end else if */
                        else if ((strcmp(pclCmdKey,"?") == 0) ||
                             (strcmp(pclCmdKey,"LIKE") == 0))
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK CHARACTERS",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilKeys++;
                            ilRC = RC_FAIL;
                            strcpy(pclChkVal1,pclChrVal1);
                            for (i=1;((i<=2)&&(ilRC!=RC_SUCCESS));i++)
                                {
                                    pclBgnPtr = pclChkVal1;
                                    pclEndPtr = pclBgnPtr;
                                    while ((pclEndPtr != NULL) && (ilRC != RC_SUCCESS))
                            {
                                pclEndPtr = strstr(pclBgnPtr,";");
                                if (pclEndPtr != NULL)
                                    {
                                        *pclEndPtr = 0x00;
                                    } /* end if */
                                ilLen = strlen(pclBgnPtr);
                                if ((ilLen > 0) && (strncmp(pclFldDat,pclBgnPtr,ilLen) == 0))
                                    {
                                        ilHits++;
                                        ilRC = RC_SUCCESS;
                                    } /* end if */
                                if (pclEndPtr != NULL)
                                    {
                                        pclBgnPtr = pclEndPtr + 1;
                                    } /* end if */
                            } /* end while */
                                    strcpy(pclChkVal1,pclChrVal2);
                                } /* end for */
                            if (ilRC != RC_SUCCESS)
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT FOUND",pcpSection,pclFldNam);
                                } /* end if */
                        } /* end else if */
                        else if ((strcmp(pclCmdKey,"!=") == 0)||(strcmp(pclCmdKey,"<>") == 0))
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK INEQUALITY",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilRC = RC_FAIL;
                            ilKeys++;
                            if (strcmp(pclChrVal1,pclFldDat) != 0)
                                {
                                    ilHits++;
                                    ilRC = RC_SUCCESS;
                                } /* end if */
                            if (ilRC == RC_SUCCESS)
                                {
                                    if ((ilLen2 > 0) && (strcmp(pclChrVal2,pclFldDat) == 0))
                            {
                                ilHits--;
                                ilRC = RC_FAIL;
                            } /* end if */
                                } /* end if */
                            if (ilRC != RC_SUCCESS)
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT INEQUAL",pcpSection,pclFldNam);
                                } /* end if */
                        } /* end else if */
                        else if (strcmp(pclCmdKey,"<") == 0)
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK CASE LESS",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilKeys++;
                            if ((strcmp(pclFldDat,pclChrVal1) < 0) ||
                                    (strcmp(pclFldDat,pclChrVal2) < 0))
                                {
                                    ilHits++;
                                    ilRC = RC_SUCCESS;
                                } /* end if */
                            else
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT LESS",pcpSection,pclFldNam);
                                    ilRC = RC_FAIL;
                                } /* end else */
                        } /* end else if */
                        else if (strcmp(pclCmdKey,">") == 0)
                        {
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CMD KEY <%s> CHECK CASE GREATER",pcpSection, pclCmdKey);
                            dbg(DEBUG,"<CheckDataFilter>:<%s> CHECK FIELD <%s> <%s> <%s> <%s>",pcpSection,
                                    pclFldNam,pclChrVal1,pclChrVal2,pclFldDat);
                            ilKeys++;
                            if ((strcmp(pclFldDat,pclChrVal1) > 0) ||
                                    (strcmp(pclFldDat,pclChrVal2) > 0))
                                {
                                    ilHits++;
                                    ilRC = RC_SUCCESS;
                                } /* end if */
                            else
                                {
                                    dbg(DEBUG,"<CheckDataFilter>:<%s> <%s> DATA NOT GREATER",pcpSection,pclFldNam);
                                    ilRC = RC_FAIL;
                                } /* end else */
                        } /* end else if */
                        else
                        {
                            dbg(TRACE,"<CheckDataFilter>:<%s> WARNING: CMD KEY <%s> NOT DEFINED",pcpSection, pclCmdKey);
                        } /* end else */
                    } /* end if */
            } /* end if */
            if ((ilHitAll == FALSE) && (ilHits > 0))
            {
                ilBreakOut = TRUE;
                ilRC = RC_SUCCESS;
            } /* end if */
                else if ((ilHitAll == TRUE) && (ilRC == RC_FAIL))
            {
                ilBreakOut = TRUE;
            } /* end else if */
        } /* end while */
    /* CHANGES BY MOS FOR ACTIVATING THE DATA FILTERS*/
    }
    else
    {
        ilRC = RC_SUCCESS;
        /* EO CHANGES */
    } /* end if */
 return ilRC;
} /* end CheckDataFilter */

/******************************************************************************/
/******************************************************************************/
static int CheckChangedFields(char *pcpCedaCmd,
                              char *pcpRecFields, char *pcpSndFields,
                              char *pcpData, char *pcpOldData,int ipTableNo)
{
  int ilRC = RC_FAIL;
  int ilLen = 0;
  int ilFldCnt = 0;
  int ilFld = 0;
  int ilFldNbr = 0;
  char pclFldNam[8];
  
  /* FYA 20130822 v1.24
  char pclNewDat[2048];
  char pclOldDat[2048];
  */
  char pclNewDat[4096] = "\0";
  char pclOldDat[4096] = "\0";

  int ilCurrentField = 0;
  dbg(DEBUG,"<CheckChgdFields>:<%s> CHECK CHANGED FIELD VALUES <%s>",rgTM.prSection[ipTableNo].pcSectionName,pcpSndFields);
  ilFldCnt = field_count(pcpSndFields);
  for (ilFld = 1; ilFld <= ilFldCnt; ilFld++)
  {
    ilLen = get_real_item(pclFldNam,pcpSndFields,ilFld);
    if (ilLen > 0)
        {
            ilFldNbr = get_item_no(pcpRecFields,pclFldNam,5) + 1;
            if (ilFldNbr > 0)
            {
                get_real_item(pclNewDat,pcpData,ilFldNbr);
                get_real_item(pclOldDat,pcpOldData,ilFldNbr);

                for(ilCurrentField = 0; ilCurrentField< rgTM.prSection[ipTableNo].iNoOfFields; ilCurrentField++) 
                {
                    if(!strcmp(rgTM.prSection[ipTableNo].pcFields[ilCurrentField],pclFldNam))
                    {
                        /*dbg(DEBUG,"<CheckChgdFields>:<%s> rtype=(%c)",rgTM.prSection[ipTableNo].pcSectionName,rgTM.prSection[ipTableNo].pcFieldResponseType[ilCurrentField]); */
                        if (strcmp(pclOldDat,pclNewDat) != 0 || rgTM.prSection[ipTableNo].pcFieldResponseType[ilCurrentField] == '!')
                        {
                            rgTM.prSection[ipTableNo].pcFieldResponseFlag[ilCurrentField] = '1';
                            dbg(DEBUG,"<CheckChgdFields>:<%s> CHANGED/REQUIRED: <%s> OLD<%s>  NEW<%s>",rgTM.prSection[ipTableNo].pcSectionName,pclFldNam,pclOldDat,pclNewDat);
                            ilRC = RC_SUCCESS;
                        }
                    } /* end if */
                }
            } /* end if */
        } /* end if */
  } /* end for */
  return ilRC;
} /* end CheckChangedFields */


/******************************************************************************/
/******************************************************************************/
static int CheckDataMapping(char *pcpRule, char *pcpCedaCmd,
                              char *pcpRecFields, char *pcpSndFields,
                              char *pcpRecData, char *pcpSndData)
{
  int ilRC = RC_FAIL;
  int ilLen = 0;
  int ilFldCnt = 0;
  int ilFld = 0;
  int ilFldNbr = 0;
  char pclCmdKey[32];
  char pclRecFld[8];
  char pclSndFld[8];
  char pclRecDat[2048];
  char pclSndDat[2048];
  char pclActRul[2048];
  char pclFldChk[4];
  char *pclBgn = NULL;
  char *pclEnd = NULL;
  if (strlen(pcpRule) > 0)
  {
    dbg(DEBUG,"<CheckDataMapping> entering.");
    strcpy(pclActRul,pcpRule);
    pclBgn = pclActRul;
    pclEnd = pclBgn;
    while (pclEnd != NULL)
    {
      pclEnd = strstr(pclBgn,"|");
      if (pclEnd != NULL)
      {
    *pclEnd = 0x00;
      } /* end if */
      get_real_item(pclCmdKey,pclBgn,1);
      if (strcmp(pclCmdKey,"NOT_EMPTY") == 0)
      {
        get_real_item(pclRecFld,pclBgn,2);
        ilFldNbr = get_item_no(pcpRecFields,pclRecFld,5) + 1;
        if (ilFldNbr > 0)
        {
          ilLen = get_real_item(pclRecDat,pcpRecData,ilFldNbr);
      if (ilLen > 0)
      {
        ilLen = -1;
            get_real_item(pclSndFld,pclBgn,3);
        pclFldChk[0] = pclSndFld[4];
        pclSndFld[4] = 0x00;
            ilFldNbr = get_item_no(pcpRecFields,pclSndFld,5) + 1;
            if ((ilFldNbr > 0) && (pclFldChk[0] == '?'))
            {
              ilLen = get_real_item(pclRecDat,pcpRecData,ilFldNbr);
            } /* end if */
        if (ilLen < 1)
        {
              ilLen = get_real_item(pclSndDat,pclBgn,-4);
          if (ilLen < 1)
          {
            strcpy(pclSndDat," ");
              } /* end if */
          dbg(TRACE,"<CheckDataMapping> FIELD <%s> NOT EMPTY <%s>",pclRecFld,pclRecDat);
          dbg(TRACE,"<CheckDataMapping> VALUE <%s> SET <%s>",pclSndFld,pclSndDat);
          SetFieldValue(pcpSndFields,pcpSndData,pclSndFld,pclSndDat);
            } /* end if */
          } /* end if */
        } /* end if */
      } /* end if */
      else if (strcmp(pclCmdKey,"CHK_EMPTY") == 0)
      {
        get_real_item(pclRecFld,pclBgn,2);
        ilFldNbr = get_item_no(pcpRecFields,pclRecFld,5) + 1;
        if (ilFldNbr > 0)
        {
          ilLen = get_real_item(pclRecDat,pcpRecData,ilFldNbr);
      if (ilLen < 1)
      {
            get_real_item(pclSndFld,pclBgn,3);
        pclFldChk[0] = pclSndFld[4];
        pclSndFld[4] = 0x00;
            ilFldNbr = get_item_no(pcpRecFields,pclSndFld,5) + 1;
            if ((ilFldNbr > 0) && (pclFldChk[0] == '?'))
            {
              ilLen = get_real_item(pclRecDat,pcpRecData,ilFldNbr);
            } /* end if */
        if (ilLen < 1)
        {
              ilLen = get_real_item(pclSndDat,pclBgn,-4);
          if (ilLen < 1)
          {
        strcpy(pclSndDat," ");
              } /* end if */
          dbg(TRACE,"<CheckDataMapping> FIELDS <%s> <%s> ARE EMPTY",pclRecFld,pclSndFld);
          dbg(TRACE,"<CheckDataMapping> VALUE <%s> SET <%s>",pclSndFld,pclSndDat);
          SetFieldValue(pcpSndFields,pcpSndData,pclSndFld,pclSndDat);
            } /* end if */
          } /* end if */
        } /* end if */
      } /* end else if */
      else
      {
    dbg(TRACE,"<CheckDataMapping> UNKNOWN KEY <%s>",pclCmdKey);
      } /* end else */
      if (pclEnd != NULL)
      {
    pclBgn = pclEnd + 1;
      } /* end if */
    } /* end while */
  } /* end if */
  return ilRC;
} /* end CheckDataMapping */

/******************************************************************************/
/******************************************************************************/
static void SetFieldValue(char *pcpSndFields,char *pcpSndData,char *pcpSndFld,char *pcpSndDat)
{
  int ilFldNbr = 0;
  int ilFldCnt = 0;
  int ilFld = 0;
  int ilFldPos = 0;
  int ilDatPos = 0;
  char pclFldNam[16];
  char pclFldDat[2048];
  char pclOldFld[iMAXIMUM];
  char pclOldDat[iMAXIMUM];
  ilFldNbr = get_item_no(pcpSndFields,pcpSndFld,5) + 1;
  if (ilFldNbr > 0)
  {
    strcpy(pclOldFld,pcpSndFields);
    strcpy(pclOldDat,pcpSndData);
    ilFldCnt = field_count(pcpSndFields);
    for (ilFld = 1; ilFld <= ilFldCnt; ilFld++)
    {
      get_real_item(pclFldNam,pclOldFld,ilFld);
      get_real_item(pclFldDat,pclOldDat,ilFld);
      if (strcmp(pclFldNam,pcpSndFld) == 0)
      {
    strcpy(pclFldDat,pcpSndDat);
      } /* end if */
      StrgPutStrg(pcpSndFields,&ilFldPos,pclFldNam,0,-1,",");
      StrgPutStrg(pcpSndData,&ilDatPos,pclFldDat,0,-1,",");
    } /* end for */
    if (ilFldPos > 0)
    {
      ilFldPos--;
      ilDatPos--;
    } /* end if */
    pcpSndFields[ilFldPos] = 0x00;
    pcpSndData[ilDatPos] = 0x00;
  } /* end if */
  else
  {
    strcat(pcpSndFields,",");
    strcat(pcpSndFields,pcpSndFld);
    strcat(pcpSndData,",");
    strcat(pcpSndData,pcpSndDat);
  } /* end else */
  return;
} /* end SetFieldValue */


/*/////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////// ACTIONGetDataField ->
returns a field spcified with field-number to the calling routine 
*/
static char *ACTIONGetDataField(char *s1, UINT No, char c, int ipFlag)
{
    UINT                    i = 0;
    char                    *pclS;
    static char         pclTmpBuf[iMAXIMUM];

    if (s1 == NULL)
        return NULL;

    /* clear buffer */
    memset ((void*)pclTmpBuf, 0x00, iMAXIMUM);
    pclS = pclTmpBuf;

    /* search the field */  
    while (*s1)
    {
        if (*s1 == c)
            i++; 
        else if (i == No)
        {
            while ((*s1 != c) && *s1)
                *pclS++ = *s1++;
            break;
        }
        s1++;
    }
    *pclS = 0x00;

    /* delete all blanks at end of string... */
    /* ...or keep the data untouched, it depends on the flag only */
    if (ipFlag == iDELETE_ALL_BLANKS || ipFlag == iONE_BLANK_REMAINING)
    {
        while (pclS > pclTmpBuf && *--pclS == cBLANK)
            *pclS = 0x00;
        if (ipFlag == iONE_BLANK_REMAINING)
        {
            if (!strlen(pclS))
            {
                /* 
                    if there isn't any character left, we must attach 
                    at least on blank 
                */
                *pclS++ = cBLANK;
                *pclS = 0x00;
            }
        }
    }

    return pclTmpBuf;
}

/******************************************************************************/
/* AddDynamicConfig routine                                                   */
/******************************************************************************/
static int AddDynamicConfig(ACTIONConfig *prpConfig, int ipFlg)
{
  int       ilCurSec;
  int       ilCurrentField;
  int       ilFoundSection;
  int       ilMoveSection;
  int       ilCurrentSectionCommand;
  char      pclAnswerString[iMIN_BUF_SIZE];
  char      *pclS = NULL;
  char      pclCondition[iMAX_BUF_SIZE];
  char      pclCurrentCondition[iMAX_BUF_SIZE];


  dbg(DEBUG,"<AddDynamicConfig> -- START --");

  if (prpConfig == NULL)
    {
      dbg(TRACE,"<AddDynamicConfig> got NULL-Pointer...");
      return RC_FAIL;
    }
  /* print received configuration structure... */
  DebugPrintACTIONConfig(DEBUG, prpConfig); 

  /* clear buffer */
  pclAnswerString[0] = 0x00;
  
  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  /*                        first check add/delete flag                     */
  if (prpConfig->iADFlag == iADD_SECTION)
    {
      dbg(TRACE,"<AddDynamicConfig> Adding section!");
      /* check for valid section name */
            if (0==strcmp(prpConfig->pcSectionName,"-") || strlen(prpConfig->pcSectionName)<1)
            {
                dbg(TRACE,"<AddDynamicConfig> SectionName <%s> is none descriptive!", prpConfig->pcSectionName);
                if (ipFlg == iCREATE_DYNAMIC_FILE)
                {
                    pclAnswerString[0] = 0x00;
                    strcpy(pclAnswerString, "ADD,NDSN"); /* NoneDescriptiveSectionName */
                    SendAnswer(pclAnswerString);
                }
                return RC_FAIL;
            }
      /* check section name, is there already a section with same name */
            for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
            { 

                if (0==strcmp(prpConfig->pcSectionName, rgTM.prSection[ilCurSec].pcSectionName))
                {
                        dbg(TRACE,"<AddDynamicConfig> SectionName <%s> already exist", prpConfig->pcSectionName);
                        if (ipFlg == iCREATE_DYNAMIC_FILE)
                        {
                                    pclAnswerString[0] = 0x00;
                                    strcpy(pclAnswerString, "ADD,SAE"); /* SectionAlreadyExist */
                                    SendAnswer(pclAnswerString);
                        }
                        return RC_FAIL;
                }
            }
      /* increment counter */
      dbg(DEBUG,"<AddDynamicConfig> add new section...");
      ++rgTM.iNoOfSections;
      dbg(DEBUG,"<AddDynamicConfig> handle now %d sections", rgTM.iNoOfSections);
     
      /* set current section, it's always the last one */
      ilCurSec = rgTM.iNoOfSections - 1;
      
      /* memory for new section */
      if ((rgTM.prSection = (TSection*)realloc(rgTM.prSection, rgTM.iNoOfSections*sizeof(TSection))) == NULL)
            {
                dbg(TRACE,"<AddDynamicConfig> cannot realloc %d bytes",(int)rgTM.iNoOfSections*sizeof(TSection));
                if (ipFlg == iCREATE_DYNAMIC_FILE)
                {
                        pclAnswerString[0] = 0x00;
                        strcpy(pclAnswerString, "ADD,SRAE"); /* SectionReAllocError */
                        SendAnswer(pclAnswerString);
                }
                return RC_FAIL;
            }
            /* init section with 0x00 */
            memset(&rgTM.prSection[ilCurSec],0x00,sizeof(TSection));
      /* set new values */
            /* snap(rgTM.prSection[ilCurSec],sizeof(TSection),outp);*/
      rgTM.prSection[ilCurSec].iValidSection = iVALID;
      rgTM.prSection[ilCurSec].iSectionType  = iDYNAMIC_SECTION;

      /*temporary default: sending old data for dynamic section with \n after data*/
      rgTM.prSection[ilCurSec].iSendOldData = TRUE;
      rgTM.prSection[ilCurSec].iSuspendOwn = TRUE;
      strcpy(rgTM.prSection[ilCurSec].pcSectionName, prpConfig->pcSectionName);
      rgTM.prSection[ilCurSec].iEmptyFieldHandling = prpConfig->iEmptyFieldHandling;
      rgTM.prSection[ilCurSec].iNoOfIgnoreDataFields = 0;
      rgTM.prSection[ilCurSec].iIgnoreEmptyFields = prpConfig->iIgnoreEmptyFields;  
      if (strlen(prpConfig->pcSndCmd))
    {
      rgTM.prSection[ilCurSec].iUseSndCmd = 1;
      strcpy(rgTM.prSection[ilCurSec].pcSndCmd, prpConfig->pcSndCmd);
    }
      else
    {
      rgTM.prSection[ilCurSec].iUseSndCmd = 0;
      rgTM.prSection[ilCurSec].pcSndCmd[0] = 0x00;
    }
      rgTM.prSection[ilCurSec].iTabNo = -1;
      rgTM.prSection[ilCurSec].iCmdNo = -1;
      strcpy(rgTM.prSection[ilCurSec].pcTableName, prpConfig->pcTableName);
      StringUPR((UCHAR*)rgTM.prSection[ilCurSec].pcTableName);
      rgTM.prSection[ilCurSec].iModID = prpConfig->iModID;
      rgTM.prSection[ilCurSec].iPrio = PRIORITY_4;
      rgTM.prSection[ilCurSec].iCommandType = EVENT_DATA;
      rgTM.prSection[ilCurSec].iNoOfKeys = 0;
      if (!strlen(prpConfig->pcFields))
    {
      /*  we are missing the configuration entry "fields", as a  */
        /*  consequence of this, we reckon it's necessary to use all  */
        /*  received (by the CEDA queueing system) fields.  */
        /*  It's supposed to be a great deal... */
      
      rgTM.prSection[ilCurSec].iNoOfFields = iUSE_ALL_RECEIVED_FIELDS;
      rgTM.prSection[ilCurSec].pcFields = NULL;
      rgTM.prSection[ilCurSec].pcFieldConditions = NULL;
      rgTM.prSection[ilCurSec].piFieldConditionTypes = 0;
      rgTM.prSection[ilCurSec].pcFieldResponseType = NULL;

    }
      else
    {
      /* convert to uppercase */
      StringUPR((UCHAR*)prpConfig->pcFields);
      /* count fields */
      rgTM.prSection[ilCurSec].iNoOfFields = GetNoOfElements(prpConfig->pcFields, ',');
      /* memory for fields */
       if ((rgTM.prSection[ilCurSec].pcFieldResponseType = (char*)malloc(rgTM.prSection[ilCurSec].iNoOfFields*sizeof(char))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> not enough menory to run (4)");
          rgTM.prSection[ilCurSec].pcFieldResponseType = NULL;
          Terminate(30);
        }
       if ((rgTM.prSection[ilCurSec].pcFieldResponseFlag = (char*)malloc(rgTM.prSection[ilCurSec].iNoOfFields*sizeof(char))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> not enough menory to run (4)");
          rgTM.prSection[ilCurSec].pcFieldResponseFlag = NULL;
          Terminate(30);
        }
       if ((rgTM.prSection[ilCurSec].pcFields = (char**)malloc(rgTM.prSection[ilCurSec].iNoOfFields*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<AddDynamicSection> not enough menory to run");
          rgTM.prSection[ilCurSec].iNoOfFields = iUSE_ALL_RECEIVED_FIELDS;
          rgTM.prSection[ilCurSec].pcFields = NULL;
          rgTM.prSection[ilCurSec].pcFieldResponseType = NULL;
          rgTM.prSection[ilCurSec].pcFieldResponseFlag = NULL;
        }
      else
        {
                /* for all fields... */
          for (ilCurrentField=0;
           ilCurrentField<rgTM.prSection[ilCurSec].iNoOfFields;
           ilCurrentField++)
        {
          /* get next fieldname */
          pclS = GetDataField(prpConfig->pcFields, ilCurrentField, ',');
          /* store fieldname */
          if ((rgTM.prSection[ilCurSec].pcFields[ilCurrentField] = strdup(pclS)) == NULL)
            {
              dbg(TRACE,"<AddDynamicConfig> not enough memory to run");
              rgTM.prSection[ilCurSec].pcFields[ilCurrentField] = NULL;
              if (ipFlg == iCREATE_DYNAMIC_FILE)
            { 
              pclAnswerString[0] = 0x00;
              strcpy(pclAnswerString, "ADD,FSDE"); /* FieldStrDupError */
              SendAnswer(pclAnswerString);
            }
              Terminate(0);
            }
          /*set field response flag to NULL*/
          rgTM.prSection[ilCurSec].pcFieldResponseFlag[ilCurrentField] = '0';

          /* how shall we handle this field*/   
      /* JIM 20050915: first init this flag: */
      rgTM.prSection[ilCurSec].pcFieldResponseType[ilCurrentField] = 0;
          if (strrchr(rgTM.prSection[ilCurSec].pcFields[ilCurrentField],'?') != NULL)
            {
             dbg(DEBUG,"<AddDynamicConfig> Field command <?> was found");
            
             /* extract the fieldname*/ 
             rgTM.prSection[ilCurSec].pcFields[ilCurrentField][strlen(rgTM.prSection[ilCurSec].pcFields[ilCurrentField])-1]= '\0';
             DeleteCharacterInString(rgTM.prSection[ilCurSec].pcFields[ilCurrentField], cBLANK);
             /*store field response type*/ 
         /* JIM 20050915: set ? flag: */
             rgTM.prSection[ilCurSec].pcFieldResponseType[ilCurrentField] = '?' ;
            }
          
           /* how shall we handle this field*/
              if (strrchr(rgTM.prSection[ilCurSec].pcFields[ilCurrentField],'!') != NULL)
            {
             dbg(DEBUG,"<AddDynamicConfig> Field command <!> was found");
              
             /* extract the fieldname*/
             rgTM.prSection[ilCurSec].pcFields[ilCurrentField][strlen(rgTM.prSection[ilCurSec].pcFields[ilCurrentField])-1]= '\0';
             
             /*store field response type*/
         /* JIM 20050915: overwrite ? flag by ! : */
             rgTM.prSection[ilCurSec].pcFieldResponseType[ilCurrentField] = '!' ;
            }
        }
        }

      /* field conditions */
          /* first memory for conditions... */
      if ((rgTM.prSection[ilCurSec].pcFieldConditions = (char**)malloc(rgTM.prSection[ilCurSec].iNoOfFields*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> not enough menory for FieldConditions");
          rgTM.prSection[ilCurSec].pcFieldConditions = NULL;
          Terminate(30);
        }

                /* ... and memory for types */
      if ((rgTM.prSection[ilCurSec].piFieldConditionTypes = (UINT*)malloc(rgTM.prSection[ilCurSec].iNoOfFields*sizeof(UINT))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> not enough memory for FieldConditionsTypes");
          rgTM.prSection[ilCurSec].piFieldConditionTypes = NULL;
          Terminate(30);
        }

                /* we've got the space */
      for (ilCurrentField=0;
           ilCurrentField<rgTM.prSection[ilCurSec].iNoOfFields;
           ilCurrentField++)
        {
          /* copy field to temporary buffer... */
          memset((void*)pclCondition, 0x00, iMAX_BUF_SIZE);
          strcpy(pclCondition, rgTM.prSection[ilCurSec].pcFields[ilCurrentField]);
          rgTM.prSection[ilCurSec].pcFieldConditions[ilCurrentField] = NULL;
        }
    }

      /* all section commands */
      if (!strlen(prpConfig->pcSectionCommands))
    {
      rgTM.prSection[ilCurSec].iNoOfSectionCommands = 0;
    }
      else
    {
      /* convert to uppercase */
      StringUPR((UCHAR*)prpConfig->pcSectionCommands);

      /* count fields */
      rgTM.prSection[ilCurSec].iNoOfSectionCommands = GetNoOfElements(prpConfig->pcSectionCommands, ',');

      /* memory for section commands */
      if ((rgTM.prSection[ilCurSec].pcSectionCommands = (char**)malloc(rgTM.prSection[ilCurSec].iNoOfSectionCommands*sizeof(char*))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> not enough menory to run");
          rgTM.prSection[ilCurSec].pcSectionCommands = NULL;
          if (ipFlg == iCREATE_DYNAMIC_FILE)
        {
          pclAnswerString[0] = 0x00;
          strcpy(pclAnswerString, "ADD,CMAE"); /* CommandMAllocError */
          SendAnswer(pclAnswerString);
        }
          Terminate(0);
        }
      else
        {
                /* for all fields... */
          for (ilCurrentSectionCommand=0; ilCurrentSectionCommand<rgTM.prSection[ilCurSec].iNoOfSectionCommands; ilCurrentSectionCommand++)
        {
          pclS = GetDataField(prpConfig->pcSectionCommands, ilCurrentSectionCommand, ',');

          /* store command */
          if ((rgTM.prSection[ilCurSec].pcSectionCommands[ilCurrentSectionCommand] = strdup(pclS)) == NULL)
            {
              dbg(TRACE,"<AddDynamicConfig> not enough memory to run");
              rgTM.prSection[ilCurSec].pcSectionCommands[ilCurrentSectionCommand] = NULL;
              if (ipFlg == iCREATE_DYNAMIC_FILE)
            {
              pclAnswerString[0] = 0x00;
              strcpy(pclAnswerString, "ADD,CSDE"); /* CommandStrDupError */
              SendAnswer(pclAnswerString);
            }
              Terminate(0);
            }
        }
        }
    }
    }

  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  else if (prpConfig->iADFlag == iDELETE_SECTION)
    {
      dbg(TRACE,"<AddDynamicConfig> deleting section!");
      /* delete specified section */
      /* look for specified section in memory */
      for (ilCurSec=0, ilFoundSection=-1; ilCurSec<rgTM.iNoOfSections && ilFoundSection==-1; ilCurSec++)
    {
      if (0==strcmp(prpConfig->pcSectionName, rgTM.prSection[ilCurSec].pcSectionName) && rgTM.prSection[ilCurSec].iSectionType == iDYNAMIC_SECTION)
        {
                /* found this secton */
          ilFoundSection = ilCurSec;
        }
    }

      /* Did we find a section ? */
      if (ilFoundSection > -1)
    {
      /* count no of section to move */
      ilMoveSection = rgTM.iNoOfSections - ilFoundSection - 1;
      dbg(DEBUG,"<AddDynamicConfig> move %d sections...", ilMoveSection);

      /* move memory */
      if (ilMoveSection > 0)
        memmove(&rgTM.prSection[ilFoundSection], &rgTM.prSection[ilFoundSection+1], ilMoveSection*sizeof(TSection));
            
      /* decrement counter */
      --rgTM.iNoOfSections;
      dbg(DEBUG,"<AddDynamicConfig> delete section now...");

      /* change memory size */
      if ((rgTM.prSection = (TSection*)realloc(rgTM.prSection, rgTM.iNoOfSections*sizeof(TSection))) == NULL)
        {
          dbg(TRACE,"<AddDynamicConfig> cannot realloc %d bytes",(int)rgTM.iNoOfSections*sizeof(TSection));
          if (ipFlg == iCREATE_DYNAMIC_FILE)
        {
          pclAnswerString[0] = 0x00;
          strcpy(pclAnswerString, "DELETE,SRAE"); /* SectionReAllocError */
          SendAnswer(pclAnswerString);
        }
          return RC_FAIL;
        }
    }
      else
    {
      /* no we didn't, possible a wrong section name */
      pclAnswerString[0] = 0x00;
      strcpy(pclAnswerString, "DELETE,WSN"); /* WongSectionName */
    }
    }

  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  /* ---------------------------------------------------------------------- */
  else
    {
      dbg(TRACE,"<AddDynamicConfig> unknown section-command received!");
      /* unknown entry */
      pclAnswerString[0] = 0x00;
      strcpy(pclAnswerString, "UNKNOWN,WADF"); /* WongAddDeleteFlag */
    }

  /* it's supposed to be a good idea to send an answer */
  if (!strlen(pclAnswerString))
    strcpy(pclAnswerString, "SUCCESS");

  /* send answer to originator... */
  if (ipFlg == iCREATE_DYNAMIC_FILE)
    {
      dbg(DEBUG,"<AddDynamicConfig> sending answer now...");
      SendAnswer(pclAnswerString);

      /* create new dynamic configuration file */
      (void)CreateDynamicCfgFile();
    }
  dbg(DEBUG,"<AddDynamicConfig> -- END --");
  return RC_SUCCESS;
}


/*******************************************************************************/
/*                      SendAnswer routine                                     */
/*******************************************************************************/

static int SendAnswer(char *pcpAnswer)
{
  int           ilRC;
  int           ilLen;
  EVENT         *prlOutEvent    = NULL;
  BC_HEAD       *prlOutBCHead   = NULL;
  CMDBLK        *prlOutCmdblk   = NULL;

  dbg(DEBUG,"<SendAnswer> ----- START -----");

  /* calculate size of memory we need */
  ilLen = sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + strlen(pcpAnswer) + 32; 

  /* get memory for out event */
  if ((prlOutEvent = (EVENT*)malloc((size_t)ilLen)) == NULL)
    {
      dbg(TRACE,"<SendAnswer> malloc failed, can't allocate %d bytes", ilLen);
      dbg(DEBUG,"<SendAnswer> ----- END -----");
      return RC_FAIL;
    }

  /* clear buffer */
  memset((void*)prlOutEvent, 0x00, ilLen);

  /* set structure members */
  prlOutEvent->type  = SYS_EVENT;
  prlOutEvent->command   = EVENT_DATA;
  prlOutEvent->originator  = mod_id;
  prlOutEvent->retry_count = 0;
  prlOutEvent->data_offset = sizeof(EVENT);
  prlOutEvent->data_length = ilLen - sizeof(EVENT);

  /* BCHead members */
  prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
  prlOutBCHead->rc = NETOUT_NO_ACK;
  strncpy(prlOutBCHead->dest_name, "ACTION", 10);

  /* CMDBLK members */
  prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
  strcpy(prlOutCmdblk->command, "ANSWER");
  strcpy(prlOutCmdblk->tw_end, "ANS,WER,ACTION");

  /* data */
  strcpy(prlOutCmdblk->data+2, pcpAnswer);

  /* send this message */
  dbg(DEBUG,"<SendAnswer> sending to Mod-ID: %d", prgEvent->originator);
  if ((ilRC = que(QUE_PUT, prgEvent->originator, mod_id, PRIORITY_4, ilLen, (char*)prlOutEvent)) != RC_SUCCESS)
    {
      /* delete memory */
      free((void*)prlOutEvent);

      dbg(TRACE,"<SendAnswer> QUE_PUT returns: %d", ilRC);
      dbg(DEBUG,"<SendAnswer> ----- END -----");
      return RC_FAIL;
    }

  /* delete memory */
  free((void*)prlOutEvent);

  dbg(DEBUG,"<SendAnswer> ----- END -----");
  /* bye bye */
  return RC_SUCCESS;
}



/*******************************************************************************/
/* CreateDynamicCfgFile routine                                                */
/*******************************************************************************/
static int CreateDynamicCfgFile(void)
{
  int       i;
  int       ilCurSec;
  FILE      *fh = NULL;
  TSection  *prlSec = NULL;
  char      pclPath[iMIN_BUF_SIZE];
  char      pclFieldBuf[iMAX_BUF_SIZE];
  char      pclCmdBuf[iMIN_BUF_SIZE];

  dbg(DEBUG,"<CreateDynamicCfgFile> ----- START -----");

  sprintf(pclPath, "%s/action.dyn.cfg", getenv("CFG_PATH"));
  if ((fh = fopen(pclPath, "w")) == NULL)
    {
      dbg(TRACE,"<CreateDynamicCfgFile> cannot open file");
      return RC_FAIL;
    }
  dbg(DEBUG,"<CreateDynamicCfgFile> found %d sections", rgTM.iNoOfSections);
  for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
    {
      prlSec = &rgTM.prSection[ilCurSec];
      dbg(DEBUG,"<CreateDynamicCfgFile> section %d is: <%s>", ilCurSec, (prlSec->iSectionType == iDYNAMIC_SECTION ? "DYNAMIC" : "STATIC"));
      if (prlSec->iSectionType == iDYNAMIC_SECTION)
    {
      pclFieldBuf[0] = 0x00;
      for (i=0; i<prlSec->iNoOfFields; i++)
        {
          
          if (i < prlSec->iNoOfFields-1)
        {
          strcat(pclFieldBuf, prlSec->pcFields[i]);
          DeleteCharacterInString(pclFieldBuf, cBLANK);
          sprintf(pclFieldBuf,"%s%c",pclFieldBuf,prlSec->pcFieldResponseType[i]);
          DeleteCharacterInString(pclFieldBuf, cBLANK);
          strcat(pclFieldBuf, ",");
        }else{
          DeleteCharacterInString(pclFieldBuf, cBLANK);
          strcat(pclFieldBuf, prlSec->pcFields[i]);
          sprintf(pclFieldBuf,"%s%c",pclFieldBuf,prlSec->pcFieldResponseType[i]);
          
        }
        }
      dbg(DEBUG,"<CreateDynamicCfgFile> FieldBuf: <%s>", pclFieldBuf);

      pclCmdBuf[0] = 0x00;
      for (i=0; i<prlSec->iNoOfSectionCommands; i++)
        {
          strcat(pclCmdBuf, prlSec->pcSectionCommands[i]);
          if (i < prlSec->iNoOfSectionCommands-1)
        strcat(pclCmdBuf, ",");
        }
      dbg(DEBUG,"<CreateDynamicCfgFile> CmdBuf: <%s>", pclCmdBuf);

      /* check send-command and field-buffer, it's possible that both buffers are not filled. */
      if (!strlen(prlSec->pcSndCmd))
        {
          prlSec->pcSndCmd[0] = '-';
          prlSec->pcSndCmd[1] = 0x00;
          dbg(DEBUG,"<CreateDynamicCfgFile> setting snd_cmd to \"-\"");
        }
      if (!strlen(pclFieldBuf))
        {
          pclFieldBuf[0] = '-';
          pclFieldBuf[1] = 0x00;
          dbg(DEBUG,"<CreateDynamicCfgFile> setting fields to \"-\"");
        }

      fprintf(fh, "%d %d %d %s %s %s %s %s %d\n", iADD_SECTION, prlSec->iEmptyFieldHandling, prlSec->iIgnoreEmptyFields, prlSec->pcSndCmd, prlSec->pcSectionName, prlSec->pcTableName, pclFieldBuf, pclCmdBuf, prlSec->iModID);
    }
    }

  fclose(fh);
  dbg(DEBUG,"<CreateDynamicCfgFile> ----- END -----");
  return RC_SUCCESS;
}

/******************************************************************/
/* ReadDynamicCfgFile routine                                     */
/******************************************************************/
static int ReadDynamicCfgFile(void)
{
  int       ilRC=0;
  int       ilCnt=0;
  int       ilDbgLevel=0;
  char      pclPath[iMIN_BUF_SIZE];
  ACTIONConfig  rlAC;
  FILE      *fh;
  struct stat   rlInfo;

  dbg(DEBUG,"<ReadDynamicCfgFile> ----- START -----");
    
  sprintf(pclPath, "%s/action.dyn.cfg", getenv("CFG_PATH"));

  /* is this an existing file? */
  if ((fh = fopen(pclPath, "r")) == NULL)
    {
      dbg(TRACE,"<ReadDynamicCfgFile> cannot open file");
      dbg(DEBUG,"<ReadDynamicCfgFile> ----- END -----");
      return RC_FAIL;
    }
  else
    {
      /* check file first */
      errno = 0;
  if ((ilRC = stat(pclPath, &rlInfo)) != RC_SUCCESS)
    {
      dbg(TRACE,"<ReadDynamicCfgFile> stat returns: %s, <%s>", ilRC, strerror(errno));
      dbg(DEBUG,"<ReadDynamicCfgFile> ----- END -----");
      return ilRC;
    }
  else
    {
      if (rlInfo.st_size > 0)
      {
                        ilCnt=0;
                        while (feof(fh)==0)
                        {
                            dbg(DEBUG,"<ReadDynamicCfgFile> line <%d>",ilCnt);
                            /* read file */
                            fscanf(fh, "%d %d %d %s %s %s %s %s %d\n", &rlAC.iADFlag, &rlAC.iEmptyFieldHandling, &rlAC.iIgnoreEmptyFields, rlAC.pcSndCmd, rlAC.pcSectionName, rlAC.pcTableName, rlAC.pcFields, rlAC.pcSectionCommands, &rlAC.iModID); 

                            /* check field-buffer, send-command */
                            if (!strcmp(rlAC.pcSndCmd, "-"))
                            {
                                 rlAC.pcSndCmd[0] = 0x00;
                                 dbg(DEBUG,"<ReadDynamicFile> setting snd_cmd to NULL");
                            }
                            if (0==strcmp(rlAC.pcFields, "-")
                                    ||0==strcmp(rlAC.pcFields, "*")
                                    ||0==strcmp(rlAC.pcFields, "-1")
                                    ||0==strcmp(rlAC.pcFields, "")
                                    ||(strlen(rlAC.pcFields)< 4))
                            {
                                rlAC.pcFields[0] = 0x00;
                                dbg(DEBUG,"<ReadDynamicFile> setting fields to NULL");
                            }

                            /* and add it to static configuration... */
                            dbg(TRACE,"<ReadDynamicFile> Try adding - SEC:<%s>, CMD:<%s>, TAB:<%s>, FLD:<%s> ==> MOD-ID:<%d> CMD:<%s>"
                                ,rlAC.pcSectionName,rlAC.pcSectionCommands,rlAC.pcTableName,rlAC.pcFields,rlAC.iModID,rlAC.pcSndCmd);
                            if((ilRC = AddDynamicConfig(&rlAC, iDONT_CREATE_DYNAMIC_FILE)) != RC_SUCCESS)
                            {
                                ilDbgLevel=debug_level;
                                debug_level=TRACE;
                                dbg(TRACE,"<ReadDynamicFile> ERROR: ADDING/DELETE OF DYN-SUBSCRIPTION-REQUEST !!"); 
                                dbg(TRACE,"<ReadDynamicFile>        SEC:<%s>, CMD:<%s>, TAB:<%s>, FLD:<%s> ==> MOD-ID:<%d> CMD:<%s>"
                                        ,rlAC.pcSectionName,rlAC.pcSectionCommands,rlAC.pcTableName,rlAC.pcFields,rlAC.iModID,rlAC.pcSndCmd);
                                dbg(TRACE,"<ReadDynamicFile>        ACTION-CONFIGURATION IS INCOMPLETE!");
                                debug_level=ilDbgLevel;
                            }
                            else
                            {
                                dbg(TRACE,"<ReadDynamicFile> DYN-SUBSCRIPTION-REQUEST PROCESSED SUCCESSFULL !\n"); 
                            }
                            ilCnt++;
                        }
      }
      else
      {
        dbg(DEBUG,"<ReadDynamicCfgFile> file is zero bytes...");
      }
    }
      fclose(fh);
  }
  dbg(DEBUG,"<ReadDynamicCfgFile> ----- END -----");
  return ilRC;
}
static int CheckOriginator(TSection *prlS,char* pcpTw_end)
{
  int ilRc= RC_SUCCESS;
  char pclOriginator[20];
  char pclUser[20];
  char pclHopo[20];
  sprintf(pclOriginator,"%d",prgEvent->originator);

  dbg(DEBUG,"<CheckOriginator>:<%s> originator:<%d> destination:<%d>"
        ,prlS->pcSectionName,prgEvent->originator,prlS->iModID);
  dbg(DEBUG,"<CheckOriginator>:<%s> suspend-own=<%s>, suspend from other mod-id's=<%s>"
        ,prlS->pcSectionName,(prlS->iSuspendOwn==TRUE?"TRUE":"FALSE"),prlS->pcSuspendOriginator);

  if(prlS->iModID==prgEvent->originator && prlS->iSuspendOwn==TRUE)
  {
    ilRc=RC_FAIL;
    dbg(DEBUG,"<CheckOriginator>:<%s> REJECTED DUE TO OWN-ID ORIGINATOR",prlS->pcSectionName);
  }
  if(ilRc==RC_SUCCESS&&strstr(prlS->pcSuspendOriginator,pclOriginator)!=NULL)
  {
    ilRc=RC_FAIL;
    dbg(DEBUG,"<CheckOriginator>:<%s> REJECTED DUE TO SUSPEND ORIGINATOR",prlS->pcSectionName);
  }
  return ilRc;
}
static void ShowConfigState()
{
    int ilCurSec = 0;
    int ilCurFld = 0;
    int ilCurCmd = 0;
    dbg(debug_level,"<SCFG>  -- START DUMPING INTERNAL CONFIG-STATUS --");
  for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
  {
        dbg(debug_level,"<SCFG>  %03d. <%s> <%s> <%s>-<%s> -> <%s> to <%d> SO<%s> SF<%s>"
            ,ilCurSec,(rgTM.prSection[ilCurSec].iSectionType == iDYNAMIC_SECTION ? "DYNAMIC" : "STATIC")
            ,(rgTM.prSection[ilCurSec].iValidSection==iVALID?"VALID":"NOT_VALID")
            ,rgTM.prSection[ilCurSec].pcSectionName,rgTM.prSection[ilCurSec].pcTableName
            ,rgTM.prSection[ilCurSec].pcSndCmd,rgTM.prSection[ilCurSec].iModID
            ,(rgTM.prSection[ilCurSec].iSuspendOwn==TRUE?"TRUE":"FALSE"),rgTM.prSection[ilCurSec].pcSuspendOriginator);
            if (rgTM.prSection[ilCurSec].iNoOfFields > 0)
            {
                for (ilCurFld = 0; ilCurFld < rgTM.prSection[ilCurSec].iNoOfFields;ilCurFld++)
                {
                    if (rgTM.prSection[ilCurSec].pcFields[ilCurFld]!=NULL)
                    {
                        dbg(debug_level,"<SCFG>       FLD-%02d. <%s> RESPONSE-TYPE<%c> RESPONSE-FLAG<%c>"
                            ,ilCurFld,rgTM.prSection[ilCurSec].pcFields[ilCurFld]
                            ,rgTM.prSection[ilCurSec].pcFieldResponseType[ilCurFld],rgTM.prSection[ilCurSec].pcFieldResponseFlag[ilCurFld]);
                    }
                }
            }
            if (rgTM.prSection[ilCurSec].iNoOfSectionCommands > 0)
            {
                for (ilCurCmd = 0; ilCurCmd < rgTM.prSection[ilCurSec].iNoOfSectionCommands;ilCurCmd++)
                {   
                    if (rgTM.prSection[ilCurSec].pcSectionCommands[ilCurCmd]!=NULL)
                    {
                        dbg(debug_level,"<SCFG>       CMD-%02d. <%s>",ilCurCmd,rgTM.prSection[ilCurSec].pcSectionCommands[ilCurCmd]);
                    }
                }
            }
    }
    dbg(debug_level,"<SCFG>  -- END   DUMPING INTERNAL CONFIG-STATUS --");
}
static void Build_CEI_TableList(int ipCalledBy)
{
    int ilRc = 0;
    int ilSec = 0;
    int ilLen = 0;
    int ilTable = 0;
    int ilRegTables = 0;
    int ilBreak = FALSE;
    int ilCurSec = 0;
    int ilCurFld = 0;
    int ilCurCmd = 0;
    char *pclMsg = NULL;
    char *pclMsgOriginatorStart="<ORIGINATOR>";
    char *pclMsgOriginatorEnd="</ORIGINATOR>";
    char *pclMsgType="<TYPE>CFG</TYPE>";
    char *pclMsgSubType="<SUBTYPE>TABLELIST-UPD</SUBTYPE>";
    char *pclMsgStart="<CONTENT>";
    char *pclMsgEnd="</CONTENT>";
    char *pclRootTagStart="<MSG>";  
    char *pclRootTagEnd="</MSG>";   

  for (ilSec=0; ilSec<rgTM.iNoOfSections; ilSec++)
  {
        /* only perform the list build for CEI sections and if I am the client part */
        if (rgTM.prSection[ilSec].cei_iInternalType == 1 && rgTM.prSection[ilSec].cei_iTask==CEICLI)
        {
            /*dbg(DEBUG,"<Build_CEI_TableList> --- SECTION-NAME=<%s> ---",rgTM.prSection[ilSec].pcSectionName);*/
            memset(rgTM.prSection[ilSec].cei_rPublTab,0x00,sizeof(CEITable)*iMAX_CEI_TABLES);

            ilRegTables=0;
            for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
            {
                /* only include static & dynamic sections into the check/list */
                if (rgTM.prSection[ilCurSec].cei_iInternalType == 0)
                {
                    /*dbg(DEBUG,"<Build_CEI_TableList> CHECK %03d.SECTION <%s>",ilCurSec,rgTM.prSection[ilCurSec].pcSectionName);*/
                StringUPR((UCHAR*)rgTM.prSection[ilCurSec].pcTableName);
                    ilBreak=FALSE;
                    for (ilTable=0; ilTable<iMAX_CEI_TABLES && ilBreak==FALSE; ilTable++)
                    {
                        /* convert all table-names to uppercase */
                    StringUPR((UCHAR*)rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName);

                        /* table is already existing in array, so increase the counter */
                        if (strcmp(rgTM.prSection[ilCurSec].pcTableName,rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName)==0)
                        {
                            /*dbg(DEBUG,"<Build_CEI_TableList> INCREASE TABLE COUNTER");*/
                            rgTM.prSection[ilSec].cei_rPublTab[ilTable].iCount++;
                            ilBreak=TRUE;
                        }
                        /* table could not be found until the first empty array-record, so this table is new */
                        /* and must be inserted into the array */
                        if (ilBreak==FALSE && strlen(rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName)==0)
                        {
                            /*dbg(DEBUG,"<Build_CEI_TableList> INSERT TABLE");*/
                            strncpy(rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName,rgTM.prSection[ilCurSec].pcTableName,10);
                            rgTM.prSection[ilSec].cei_rPublTab[ilTable].iCount++;
                            ilRegTables++;
                            ilBreak=TRUE;
                        }
                    }
                }
            }
            /* now creating string out of array to forward it to my server-counterpart using queue's */
            /* and ceihdl-transmission */
            ilLen = (ilRegTables * 10) + 1 + strlen(pclRootTagStart) + strlen(pclRootTagEnd) + strlen(pclMsgStart) + strlen(pclMsgEnd)
                                                                         + strlen(pclMsgType) + strlen(pclMsgSubType) + strlen(rgTM.prSection[ilSec].cei_pcId) 
                                                                         + strlen(pclMsgOriginatorStart) + strlen(pclMsgOriginatorEnd) + 32; 
            if ((pclMsg = (char*)calloc(1,(size_t)ilLen)) == NULL)
            {
                dbg(TRACE, "<Build_CEI_TableList> cannot allocate %d.bytes for out event", ilLen);
            }
            else
            {
                sprintf(pclMsg,"%s%s%s%s%s%s%s"
                    ,pclRootTagStart
                    ,pclMsgOriginatorStart
                    ,rgTM.prSection[ilSec].cei_pcId
                    ,pclMsgOriginatorEnd
                    ,pclMsgType
                    ,pclMsgSubType
                    ,pclMsgStart);
            }
            for (ilTable=0; ilTable<ilRegTables; ilTable++)
            {
                dbg(DEBUG,"<Build_CEI_TableList> %03d.REG.-TABLE-NAME=<%s>, REG.-CNT=<%d>"
                    ,ilTable
                    ,rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName
                    ,rgTM.prSection[ilSec].cei_rPublTab[ilTable].iCount);
                if (pclMsg!=NULL)
                {
                    strcat(pclMsg,rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName); 
                    if (ilTable < ilRegTables-1)
                        strcat(pclMsg,","); 
                }
            }
            if (pclMsg!=NULL)
            {
                strcat(pclMsg,pclMsgEnd);
                strcat(pclMsg,pclRootTagEnd);
                dbg(DEBUG,"<Build_CEI_TableList> MSG. TO SEND <%s>",pclMsg);
                SendEventToCei(ilSec,"CEI","","","",pclMsg,"","","");
            }
        }
        /* triggering the remote action to send a CEI-tablelist to me */
        else if (rgTM.prSection[ilSec].cei_iInternalType == 1 && rgTM.prSection[ilSec].cei_iTask==CEISRV && ipCalledBy==INIT)
        {
            ilRc = SendEventToCei(ilSec,"BCEI","","","","","","","");
        }
    }
}
static int Check_CEI_Tables(int ipSection,char *pcpTable)
{
    int ilTable = 0;
    int ilBreak = FALSE;
    int ilRc = RC_FAIL;

    for (ilTable=0; ilTable<iMAX_CEI_TABLES && ilBreak==FALSE; ilTable++)
    {
        if (strlen(rgTM.prSection[ipSection].cei_rPublTab[ilTable].pcTableName)<=0)
        {
            ilBreak=TRUE;
        }
        else if (strstr(rgTM.prSection[ipSection].cei_pcDenyTab,pcpTable)!=NULL) 
        {
            ilBreak=TRUE;
            ilRc=RC_FAIL;
            dbg(TRACE,"<Check_CEI_Tables> TABLE-NAME=<%s> IS EXPLICITLY EXCLUDED FROM CEI-FORWARDING!",pcpTable);
        }
        else if (strcmp(rgTM.prSection[ipSection].cei_rPublTab[ilTable].pcTableName,pcpTable)==0)
        {
            ilBreak=TRUE;
            ilRc=RC_SUCCESS;
            dbg(DEBUG,"<Check_CEI_Tables> TABLE-NAME=<%s> FOUND IN CEI-LIST. RETURN <RC_SUCCESS>!",pcpTable);
        }
    }
    return ilRc;
}
static int SendEventToCei(int ipSection, char *pcpCmd,char *pcpObjName, char *pcpTwStart, char *pcpTwEnd,char *pcpSelection, char *pcpFields, char *pcpData, char *pcpOldData)
{
  int       ilRC = RC_SUCCESS;
  int       ilLen,ilDataLen;
  EVENT     *prlOutEvent = NULL;
  BC_HEAD   *prlBCHead,*prlOutBCHead = NULL;
  CMDBLK    *prlCmdblk,*prlOutCmdblk = NULL;
    char *pclSelection = NULL;
    char *pclFields = NULL;
    char *pclData = NULL;
    char *pclOutData = NULL;
 
    #if 0
  dbg(TRACE,"--------------------");
  dbg(TRACE,"<SendEventToCei> IN: SECTION   : <%d>", ipSection);
  dbg(TRACE,"<SendEventToCei> IN: command   : <%s>", pcpCmd);
  dbg(TRACE,"<SendEventToCei> IN: obj_name  : <%s>", pcpObjName);
  dbg(TRACE,"<SendEventToCei> IN: tw_start  : <%s>", pcpTwStart);
  dbg(TRACE,"<SendEventToCei> IN: tw_end    : <%s>", pcpTwEnd);
  dbg(TRACE,"<SendEventToCei> IN: Sel.      : <%s>", pcpSelection);
  dbg(TRACE,"<SendEventToCei> IN: Fields    : <%s>", pcpFields);
  dbg(TRACE,"<SendEventToCei> IN: Data      : <%s>", pcpData);
  dbg(TRACE,"<SendEventToCei> IN: Data(old) : <%s>", pcpOldData);
  dbg(TRACE,"");
    #endif

  /* preparing out event */
  /* calculate len of out-event */
  ilLen = sizeof(EVENT) + sizeof(BC_HEAD) +sizeof(CMDBLK) +strlen(pcpSelection) + strlen(pcpFields) + strlen(pcpData) + strlen(pcpOldData) + 128;
  /* get memory for out event */
  prlOutEvent = NULL;
  if ((prlOutEvent = (EVENT*)calloc(1,(size_t)ilLen)) == NULL)
  {
    dbg(TRACE, "<SendEventToCei> cannot allocate %d.bytes for out event", ilLen);
    return RC_FAIL;
  }
 
  /* set structure members */
  prlOutEvent->type         = SYS_EVENT;
  prlOutEvent->command          = EVENT_DATA;
  prlOutEvent->originator   = rgTM.prSection[ipSection].cei_iEventOriginator;
  prlOutEvent->retry_count  = 0;    
  prlOutEvent->data_length  = ilLen - sizeof(EVENT);
  prlOutEvent->data_offset  = sizeof(EVENT);

  /* BC-Head -Structure */
  prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
  /*prlOutBCHead->rc = prlBCHead->rc;*/
 
  /* CmdBlk-Structure */
  prlOutCmdblk = (CMDBLK*)prlOutBCHead->data;
  strcpy(prlOutCmdblk->command, pcpCmd);
  strcpy(prlOutCmdblk->obj_name, pcpObjName);
    sprintf(prlOutCmdblk->tw_start, "%s",pcpTwStart);
  strcpy(prlOutCmdblk->tw_end, pcpTwEnd);
  /* Selection */
  strcpy(prlOutCmdblk->data, pcpSelection);
  /* Fields */
  strcpy((prlOutCmdblk->data+strlen(pcpSelection)+1), pcpFields);
  /* Data */
    ilDataLen = strlen(pcpData) + strlen(pcpOldData) + 8;
    if ((pclOutData = (char*)calloc(1,(size_t)ilDataLen)) == NULL)
    {
    dbg(TRACE, "<SendEventToCei> cannot allocate %d.bytes for out event", ilDataLen);
    return RC_FAIL;
    }
    if ((pcpOldData != NULL) && (*pcpOldData != '\0') && strlen(pcpOldData))
    {
        strcpy(pclOutData,pcpData);
        strcat(pclOutData,"\n");
        strcat(pclOutData,pcpOldData);
    }
    else
    {
        strcpy(pclOutData,pcpData);
    }
    strcpy((prlOutCmdblk->data+strlen(pcpSelection)+strlen(pcpFields)+2),pclOutData);

  dbg(TRACE,"---------------------");
  dbg(TRACE,"<SendEventToCei> OUT: command   : <%s>", prlOutCmdblk->command);
  dbg(TRACE,"<SendEventToCei> OUT: obj_name  : <%s>", prlOutCmdblk->obj_name);
  dbg(TRACE,"<SendEventToCei> OUT: tw_start  : <%s>", prlOutCmdblk->tw_start);
  dbg(TRACE,"<SendEventToCei> OUT: tw_end    : <%s>", prlOutCmdblk->tw_end);
  dbg(TRACE,"<SendEventToCei> OUT: Sel.      : <%s>", pcpSelection);
  dbg(TRACE,"<SendEventToCei> OUT: Fields    : <%s>", pcpFields);
  dbg(TRACE,"<SendEventToCei> OUT: Data      : <%s>", pclOutData);
  dbg(TRACE,"<SendEventToCei> OUT: to route  : <%d>", rgTM.prSection[ipSection].iModID);
  dbg(TRACE,"<SendEventToCei> OUT: as orig.  : <%d>", rgTM.prSection[ipSection].cei_iEventOriginator);
  if(( ilRC = que(QUE_PUT, rgTM.prSection[ipSection].iModID, mod_id, 4,ilLen, (char*)prlOutEvent))!= RC_SUCCESS)
  {
     dbg(TRACE,"<SendEventToCei> que QUE_PUT failed with <%d>", ilRC);
  }

  /* delete memory for out-event... */
    if (pclOutData != NULL)
        free(pclOutData);
    if (prlOutEvent != NULL)
    free((void*)prlOutEvent);
      
  /* everything looks good (hope so)... */
  return ilRC;
}
/* ======================================================================================== */
static char *GetXmlValue(char *pcpResultBuff, long *plpResultSize,
 char *pcpTextBuff, char *pcpTagStart, char *pcpTagEnd, int bpCopyData)
{
 long llDataSize    = 0L;
 char *pclDataBegin = NULL;
 char *pclDataEnd   = NULL;

 if (bpCopyData == TRUE)
 {
    pcpResultBuff[0] = 0x00;
 }
 pclDataBegin = strstr(pcpTextBuff, pcpTagStart);       /* Search the keyword   */
 if (pclDataBegin != NULL)
 {                             /* Did we find it? Yes.   */
     pclDataBegin += strlen(pcpTagStart);           /* Skip behind the keyword  */
     pclDataEnd = strstr(pclDataBegin, pcpTagEnd);      /* Search end of data   */
     if (pclDataEnd == NULL)
     {                           /* End not found?     */
         pclDataEnd = pclDataBegin + strlen(pclDataBegin); /* Take the whole string  */
     } /* end if */
     llDataSize = pclDataEnd - pclDataBegin;         /* Now calculate the length */
     if (bpCopyData==TRUE)
     {  
         strncpy(pcpResultBuff, pclDataBegin, llDataSize); /* Yes, strip out the data  */
     } /* end if */
 } /* end if */
 if (bpCopyData == TRUE)
 {                             /* Allowed to set EOS?    */
     pcpResultBuff[llDataSize] = 0x00;
     /*dbg(DEBUG,"<GetXmlValue> XML-TAG=[%s] --> RETURN CONTENT SIZE=[%ld], VALUE=[%s]",pcpTagStart,llDataSize,pcpResultBuff);*/
 } /* end if */
 *plpResultSize = llDataSize;                /* Pass the length back   */
 return pclDataBegin;                    /* Return the data's begin  */
} /* end GetXmlValue */
/* ======================================================================================== */
static void Use_CEI_TableList(char *pcpOrig,char *pcpListOfTables)
{
    int ilSec = 0;
    int ilCnt = 0;
    int ilCount = 0;
    int ilTable = 0;
    int ilRegTables = 0;
    char pclTable[iMIN_BUF_SIZE];

    for (ilSec=0; ilSec<rgTM.iNoOfSections; ilSec++)
  {
        /* only perform the list-processing for CEI_tables if this is a CEI section, the server part & ID matches */
        if (rgTM.prSection[ilSec].cei_iInternalType == 1 && rgTM.prSection[ilSec].cei_iTask==CEISRV &&
                strcmp(rgTM.prSection[ilSec].cei_pcId,pcpOrig) == 0)
        {
            dbg(DEBUG,"<Use_CEI_TableList> --- SECTION-NAME=<%s>, ID=<%s> ---"
                ,rgTM.prSection[ilSec].pcSectionName,rgTM.prSection[ilSec].cei_pcId);
            memset(rgTM.prSection[ilSec].cei_rPublTab,0x00,sizeof(CEITable)*iMAX_CEI_TABLES);

            /* now processing string of tables and store it inside the current section */
            ilCount = GetNoOfElements(pcpListOfTables,',');
            for (ilCnt=1;ilCnt<=ilCount;ilCnt++)
            {
                memset(pclTable,0x00,iMIN_BUF_SIZE);
                GetDataItem(pclTable,pcpListOfTables,ilCnt,',',"","");
                /* convert all table-names to uppercase */
            StringUPR((UCHAR*)pclTable);
                /* table must be inserted into the array */
                /*dbg(DEBUG,"<Use_CEI_TableList> inserting %03d.Table <%s>",ilCnt,pclTable);*/
                strncpy(rgTM.prSection[ilSec].cei_rPublTab[ilCnt-1].pcTableName,pclTable,10);
                rgTM.prSection[ilSec].cei_rPublTab[ilCnt-1].iCount++;
                ilRegTables++;
            }
            for (ilTable=0; ilTable<ilRegTables; ilTable++)
            {
                dbg(DEBUG,"<Use_CEI_TableList> %03d.REG.-TABLE-NAME=<%s>"
                    ,ilTable+1,rgTM.prSection[ilSec].cei_rPublTab[ilTable].pcTableName);
            }
        }
    }
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
