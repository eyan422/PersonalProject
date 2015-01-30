#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] MARK_UNUSED = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/stxhdl.c 1.3a 2015/01/27 00:11:35SGT fya Exp  $";
#endif /* _DEF_mks_version */
/*****************************************************************************/
/*                                                                           */
/* CCS Program Skeleton                                                      */
/*                                                                           */
/* Author         :                                                          */
/* Date           :                                                          */
/* Description    :                                                          */
/*                                                                           */
/* Update history : rkl 16.05.00 read iniatial debug_level from configfile   */
/*    20020425 jim : HandleCommand: NEVER !!! use HOPO for syslibSearchDbData*/
/*    20040810 jim : removed sccs_ version string                            */
/*    20150121 FYA: v1.3a UFIS-8302 HOPO implementation                      */
/*****************************************************************************/
/* This program is a CCS main program */
/*                                                                           */
/* source-code-control-system version string                                 */

/* be carefule with strftime or similar functions !!!                        */
/*****************************************************************************/

#define U_MAIN
#define UGCCS_PRG
#define STH_USE

/* The master header file */
#include  "stxhdl.h"
#include  "buffer.h"
#include  "multiapt.h"
#include  "send.h"

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE       *outp       = NULL;*/
int        debug_level = TRACE;

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */

/******************************************************************************/
/* My Global variables                                                        */
/******************************************************************************/
static TSTMain      rgTM;
static int         igRouterID;
static char         pcgTABEnd[iMIN];

/*FYA v1.3a UFIS-8302*/
/*static char pcgHomeAP[iMIN];*/
static char	pcgHomeAP[iMIN_BUF_SIZE];
static char	pcgHomeAP_sgstab[iMIN_BUF_SIZE];
static char cgProcessName[iMIN] = "\0";

static char cgCfgBuffer[1024];
static int igMultiHopo = FALSE;
static char pcgCfgFile[32];

static int          igOrgAth;
char pcgTwEnd[64];

/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int  init_que(void);                      /* Attach to CCS queues */
extern int  que(int,int,int,int,int,char*);      /* CCS queuing routine  */
extern int  send_message(int,int,int,int,char*); /* CCS msg-handler i/f  */
extern int   snap(char *, long, FILE *);
extern void str_trm_lft(char *, char *);
extern void str_trm_rgt(char *, char *, int);
extern void str_trm_all(char *, char *, int);
extern int   SendRemoteShutdown(int);

/******************************************************************************/
/* Function prototypes                                                         */
/******************************************************************************/
static int  init_stxhdl(void);
static int   Reset(void);                       /* Reset program          */
static void   Terminate(UINT);                   /* Terminate program      */
static void   HandleSignal(int);                 /* Handles signals        */
static void   HandleErr(int);                    /* Handles general errors */
static void   HandleQueErr(int);                 /* Handles queuing errors */
static int   HandleData(void);                  /* Handles event data     */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

/******************************************************************************/
/* My Function prototypes                                                      */
/******************************************************************************/
static int HandleCommand(int, char *, long);
static int HandleSwissFlukoData(int, char *, long);
static int HandleWarsawFlukoData(int, char *, long);
static int HandleSSIMFlukoData(int, char *, long);
static int HandlePrognoseData(int, char *, long);
static int Get_xLC_ALC(char*,char*,char*,char*,char*);
static int UpdLastLine (char *pcpDataLine, char *pcpDataBuffer,
                        int ipSecNo, int ipTabNo);
static int Get3LCAirline (int ipSecNo, char *pcpAlc2, char *pcpAlc3);
static int GetCurrentSeason (char *pcpSeaBeg, char *pcpSeaEnd);

static int GLEHandleWhereClause (TSTSection, TSTTabDef, char*, char*);

static int 	HandleBaselFlukoData(int, char *, char *,long);
static int 	RNECreateRow(char *,char *,char *, char *,int,int,int);
static int 	RNEDecodeNature(char *, char *);

static char	*TWECopyNextField(char *, char, char *);
static void TWECheckPerformance    (int ipFlag);
static void SetFieldValue(int, char *,char *,char *,char *);

extern int StrgPutStrg(char*,int*,char*,int,int,char*);
extern int get_item_no(char*,char*,short);
extern int get_real_item(char*,char*,int);
/******************************************************************************/
/* Something else                                                             */
/******************************************************************************/
static const char *pcgMONTH[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};

/*FYA v1.3a UFIS-8302*/
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
MAIN
{
   int   ilRC  = RC_SUCCESS;
   int   ilCnt = 0;
   char   pclFileName[512];

   /* General initialization   */
   INITIALIZE;

   /*FYA v1.3a UFIS-8302*/
   strcpy(cgProcessName, argv[0]);

   /* all signals */
   (void)SetSignals(HandleSignal);
   (void)UnsetSignals();

   /* get data from sgs.tab */
   if ((ilRC = tool_search_exco_data("ALL", "TABEND", pcgTABEnd)) != RC_SUCCESS)
   {
      dbg(TRACE,"<MAIN> %05d cannot find entry <TABEND> in sgs.tab", __LINE__);
      Terminate(30);
   }
   dbg(DEBUG,"<MAIN> %05d found TABEND: <%s>", __LINE__, pcgTABEnd);

   /* FYA v1.3a UFIS-8302*/
   /* read HomeAirPort from SGS.TAB */
   /*
   if ((ilRC = tool_search_exco_data("SYS", "HOMEAP", pcgHomeAP)) != RC_SUCCESS)
   {
      dbg(TRACE,"<MAIN> %05d cannot find entry <HOMEAP> in SGS.TAB", __LINE__);
      Terminate(30);
   }
   dbg(DEBUG,"<MAIN> %05d found HOME-AIRPORT: <%s>", __LINE__, pcgHomeAP);
   */

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
   dbg(TRACE,"<MAIN> %05d TransferFile: <%s>", __LINE__, pclFileName);
   if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
   {
      dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
      set_system_state(HSB_STANDALONE);
   }

   /* transfer configuration file to remote machine... */
   pclFileName[0] = 0x00;
   sprintf(pclFileName, "%s/%s.cfg", getenv("CFG_PATH"), argv[0]);
   dbg(TRACE,"<MAIN> %05d TransferFile: <%s>", __LINE__, pclFileName);
   if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
   {
      dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
      set_system_state(HSB_STANDALONE);
   }

   /* ask VBL about this!! */
   dbg(TRACE,"<MAIN> %05d SendRemoteShutdown: %d", __LINE__, mod_id);
   if ((ilRC = SendRemoteShutdown(mod_id)) != RC_SUCCESS)
   {
      dbg(TRACE,"<MAIN> SendRemoteShutdown(%d) returns: %d", mod_id, ilRC);
      set_system_state(HSB_STANDALONE);
   }

   if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
   {
      dbg(DEBUG,"<MAIN> waiting for status switch ...");
      HandleQueues();
   }

   if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
   {
      dbg(DEBUG,"<MAIN> initializing ...");
      if ((ilRC = init_stxhdl()) != RC_SUCCESS)
      {
         dbg(DEBUG,"<MAIN> init_stdhdl returns: %d", ilRC);
         Terminate(30);
      }
   }
   else
   {
      Terminate(30);
   }

   /* default message */
   dbg(TRACE,"<MAIN> initializing OK");

   /* forever */
   while(1)
   {
      /* get item... */
      ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

      /* set event pointer       */
      prgEvent = (EVENT*)prgItem->text;

      if (ilRC == RC_SUCCESS)
      {
         /* Acknowledge the item */
         ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
         if (ilRC != RC_SUCCESS)
         {
            HandleQueErr(ilRC);
         }

         switch (prgEvent->command)
         {
            case HSB_STANDBY:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               HandleQueues();
               break;

            case HSB_COMING_UP:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               HandleQueues();
               break;

            case HSB_ACTIVE:
            case HSB_STANDALONE:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               break;

            case HSB_ACT_TO_SBY:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               HandleQueues();
               break;

            case HSB_DOWN:
               ctrl_sta = prgEvent->command;
               Terminate(0);
               break;

            case SHUTDOWN:
               Terminate(0);
               break;

            case RESET:
               ilRC = Reset();
               break;

            case EVENT_DATA:
               if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
               {
                  ilRC = HandleData();
               }
               else
               {
                  dbg(DEBUG,"<MAIN> %05d wrong hsb status <%d>", __LINE__, ctrl_sta);
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
         if(ilRC != RC_SUCCESS)
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
static int init_stxhdl(void)
{
   int      i;
   int      ilRC;
   int      ilCurSec;
   int      ilULOCnt;
   int      ilAlignCnt;
   int      ilCurULO;
   int      ilCurAlign;
   int      ilCurTable;
   int      ilCurField;
   int      ilCurDataLength;
   int      ilCurFieldLength;
   int      ilCurDataPos;
   int      ilSyslibNo;
   int      ilSysTmpMax;
   int      ilSysTmpNo;
   int      ilCurString;
   char      *pclS         = NULL;
   char      *pclCfgPath   = NULL;
   char      pclCfgFile[iMIN_BUF_SIZE];
   char      pclSections[iMAX_BUF_SIZE];
   char      pclCurSec[iMIN_BUF_SIZE];
   char      pclTmpBuf[iMIN_BUF_SIZE];
   char      pclAllTables[iMIN_BUF_SIZE];
   char      pclCurTable[iMIN_BUF_SIZE];
   char      pclCurSysLib[iMIN_BUF_SIZE];
   char      pclAllFields[iMAXIMUM];
   char      pclAllSyslib[iMAXIMUM];
   char      pclWhereFields[iMAXIMUM];
   char      pclAllCharacter[iMIN_BUF_SIZE];
   char      pclStartKeyWord[iMIN_BUF_SIZE];
   char      pclEndKeyWord[iMIN_BUF_SIZE];
   char      pclAllKeyWord[iMIN_BUF_SIZE];
   char      pclCurPos[iMIN_BUF_SIZE];
   char      pclIgnoreBuffer[iMAX_BUF_SIZE];

   /* clear memory */
   memset((void*)&rgTM, 0x00, sizeof(TSTMain));

   /* get the path */
   if ((pclCfgPath = getenv("CFG_PATH")) == NULL)
   {
      /* cannot work with this path */
      dbg(TRACE,"<init_stxhdl> missing CFG_PATH in environment");
      Terminate(30);
   }

   /* clear buffer */
   memset((void*)pclCfgFile, 0x00, iMIN_BUF_SIZE);

   /* copy path to buffer */
   strcpy(pclCfgFile, pclCfgPath);

   /* check last character */
   if (pclCfgFile[strlen(pclCfgFile)-1] != '/')
      strcat(pclCfgFile, "/");

   /* add filename to path */
   strcat(pclCfgFile, "stxhdl.cfg");

   /* debug messge */
   dbg(DEBUG,"<init_stxhdl> built filename: <%s>", pclCfgFile);

   strcpy(pcgCfgFile,pclCfgFile);

	/* -------------------------------------------------------------- */
	/* debug_level setting ------------------------------------------ */
	/* -------------------------------------------------------------- */
	pclTmpBuf[0] = 0x00;
	if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "debug_level", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
	{
		/* default */
		dbg(DEBUG,"<init_stxhdl> no debug_level in section <MAIN>");
		strcpy(pclTmpBuf, "TRACE");
	}

	/* which debug_level is set in the Configfile ? */
	StringUPR((UCHAR*)pclTmpBuf);
	if (!strcmp(pclTmpBuf,"DEBUG"))
	{
		debug_level = DEBUG;
		dbg(TRACE,"<init_stxhdl> debug_level set to DEBUG");
	}
	else if (!strcmp(pclTmpBuf,"TRACE"))
	{
		debug_level = TRACE;
		dbg(TRACE,"<init_stxhdl> debug_level set to TRACE");
	}
	else if (!strcmp(pclTmpBuf,"OFF"))
	{
		debug_level = 0;
		dbg(TRACE,"<init_stxhdl> debug_level set to OFF");
	}
	else
	{
		debug_level = 0;
		dbg(TRACE,"<init_stxhdl> unknown debug_level set to default OFF");
	}

	/* -------------------------------------------------------------- */

   setHomeAirport(cgProcessName, pcgHomeAP_sgstab);

   /* try to read configuration file */
   memset((void*)pclSections, 0x00, iMAX_BUF_SIZE);
   if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "sections",
                               CFG_STRING, pclSections)) != RC_SUCCESS)
   {
      /* cannot run without sections to read */
      dbg(TRACE,"<init_stxhdl> missing sections in MAIN");
      Terminate(30);
   }

   /* convert to uppercase */
   StringUPR((UCHAR*)pclSections);

   /* count sections */
   if ((rgTM.iNoOfSections = GetNoOfElements(pclSections, ',')) < 0)
   {
      dbg(TRACE,"<init_stxhdl> GetNoOfElements (pclSection) returns: %d",
                                                      rgTM.iNoOfSections);
      Terminate(30);
   }

   /* we NEEEED memory */
   if ((rgTM.prSection = (TSTSection*)malloc((size_t)(rgTM.iNoOfSections*sizeof(TSTSection)))) == NULL)
   {
      dbg(TRACE,"<init_stxhdl> Section malloc failure");
      rgTM.prSection = NULL;
      Terminate(30);
   }

   /* for all sections... */
   for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
   {
      /* get n.th section */
      if ((pclS = GetDataField(pclSections, ilCurSec, ',')) == NULL)
      {
         dbg(TRACE,"<init_stdhdl> GetDataField (%d) returns NULL", ilCurSec);
         Terminate(30);
      }

      /* copy section name to loacl buffer */
      memset((void*)pclCurSec, 0x00, iMIN_BUF_SIZE);
      strcpy(pclCurSec, pclS);

      dbg(DEBUG,"<init_stxhdl> ========== START <%s> ===========", pclCurSec);

      /* system kennzeichen */
      if ((ilRC = iGetConfigRow(pclCfgFile, pclCurSec, "selection",
                CFG_STRING, rgTM.prSection[ilCurSec].pcSystemKnz)) != RC_SUCCESS)
      {
         rgTM.prSection[ilCurSec].iUseSystemKnz = 0;
      }
      else
      {
         rgTM.prSection[ilCurSec].iUseSystemKnz = 1;
         dbg(DEBUG,"<init_stxhdl> found selection: <%s>", rgTM.prSection[ilCurSec].pcSystemKnz);
				 if (strstr(rgTM.prSection[ilCurSec].pcSystemKnz,"#FC")!=NULL)
				 {
						dbg(DEBUG,"<init_stxhdl> found WRD-selection for SQLHDL! Overwrite system identifier inside selection!");
				 }
      }

      /*---------------------------------------------------------------*/
      /*---------------------------------------------------------------*/
      /* the home airport */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "home_airport", CFG_STRING, rgTM.prSection[ilCurSec].pcHomeAirport)) != RC_SUCCESS)
      {
         //strcpy(rgTM.prSection[ilCurSec].pcHomeAirport, pcgHomeAP);

         memset(rgTM.prSection[ilCurSec].pcHomeAirport,0,sizeof(rgTM.prSection[ilCurSec].pcHomeAirport));

         dbg(DEBUG,"<init_stxhdl> NO Home-Airport in Section <%s>: keep it null",
			pclCurSec,rgTM.prSection[ilCurSec].pcHomeAirport);
      }

        if(rgTM.prSection[ilCurSec].pcHomeAirport != NULL && strlen(rgTM.prSection[ilCurSec].pcHomeAirport) > 0)
        {
            if (CheckAPC3(rgTM.prSection[ilCurSec].pcHomeAirport) != RC_SUCCESS)
            {
                dbg(DEBUG,"<init_stxhdl> No Valid Home-Airport: <%s>",
                    rgTM.prSection[ilCurSec].pcHomeAirport);

                //strcpy(rgTM.prSection[ilCurSec].pcHomeAirport, pcgHomeAP);
            }
            dbg(DEBUG,"<init_stxhdl> Home-Airport: <%s>", rgTM.prSection[ilCurSec].pcHomeAirport);
        }

      /* the table extension */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "table_extension", CFG_STRING, rgTM.prSection[ilCurSec].pcTableExtension)) != RC_SUCCESS)
      {
         strcpy(rgTM.prSection[ilCurSec].pcTableExtension, pcgTABEnd);
      }
      dbg(DEBUG,"<init_fileif> TableExtension: <%s>", rgTM.prSection[ilCurSec].pcTableExtension);



      /*---------------------------------------------------------------*/
      /*---------------------------------------------------------------*/
      /* read command */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "command",
                CFG_STRING, rgTM.prSection[ilCurSec].pcCommand)) != RC_SUCCESS)
      {
         /* cannot run without command to recognize */
         dbg(TRACE,"<init_stxhdl> missing command in <%s>", pclCurSec);
         Terminate(30);
      }

      /* read end of line characters */
      memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "eol",
                               CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
         /* cannot run without end of line characters */
         dbg(TRACE,"<init_stxhdl> missing eol in <%s>", pclCurSec);
         Terminate(30);
      }
      else
      {
         /* convert to uppercase */
         StringUPR((UCHAR*)pclTmpBuf);

         /* we know end of line charcters */
         if ((rgTM.prSection[ilCurSec].iNoOfEOLCharacters =
                                 GetNoOfElements(pclTmpBuf, ',')) < 0)
         {
            dbg(TRACE,"<init_stxhdl> GetNoOfElements (eol) returns: %d",
                              rgTM.prSection[ilCurSec].iNoOfEOLCharacters);
            Terminate(30);
         }

         /* for all charaters */
         for (i=0; i<rgTM.prSection[ilCurSec].iNoOfEOLCharacters; i++)
         {
            /* get the character */
            if ((pclS = GetDataField(pclTmpBuf, i, ',')) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> GetDataField EOL retuns NULL");
               Terminate(30);
            }

            /* store it */
            dbg(DEBUG,"<init_stxhdl> found EOL-Character <%s>", pclS);
            if (IsCR(pclS))
               rgTM.prSection[ilCurSec].pcEndOfLine[i] = 0x0D;
            else if (IsLF(pclS))
               rgTM.prSection[ilCurSec].pcEndOfLine[i] = 0x0A;
            else
            {
               dbg(TRACE,"<init_stxhdl> unknown EOL-Character");
               Terminate(30);
            }
         }

         /* set end */
         rgTM.prSection[ilCurSec].pcEndOfLine[i] = 0x00;
      }

      /* read sign of comment */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "comment_sign",
                CFG_STRING, rgTM.prSection[ilCurSec].pcCommentSign)) != RC_SUCCESS)
      {
         memset((void*)rgTM.prSection[ilCurSec].pcCommentSign, 0x00, iMIN);
         rgTM.prSection[ilCurSec].iUseComment = 0;
      }
      else
         rgTM.prSection[ilCurSec].iUseComment = 1;

      /* file type to read */
      memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "file_type",
                                        CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
         dbg(TRACE,"<init_stxhdl> missing file_type in <%s>", pclCurSec);
         Terminate(30);
      }
      else
      {
         /* convert to uppercase */
         StringUPR((UCHAR*)pclTmpBuf);

         /* check it */
         if (!strcmp(pclTmpBuf, "FIX"))
            rgTM.prSection[ilCurSec].iFileType = iIS_FIX;
         else if (!strcmp(pclTmpBuf, "DYNAMIC"))
            rgTM.prSection[ilCurSec].iFileType = iIS_DYNAMIC;
         else
         {
            dbg(TRACE,"<init_stxhdl> unknown FileType");
            Terminate(30);
         }
      }

      /* if file type is dynamic-we must read global data separator */
      if (IsDynamic(rgTM.prSection[ilCurSec].iFileType))
      {
         memset((void*)rgTM.prSection[ilCurSec].pcDataSeparator, 0x00, iMIN);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "data_separator",
             CFG_STRING, rgTM.prSection[ilCurSec].pcDataSeparator)) != RC_SUCCESS)
         {
            dbg(TRACE,"<init_stxhdl> missing data_separator in <%s>",pclCurSec);
            Terminate(30);
         }
         else
            dbg(DEBUG,"<init_stxhdl> data-separator: <%s>",
                           rgTM.prSection[ilCurSec].pcDataSeparator);
      }

      /* character set to read */
      memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "character_set",
                                        CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
         dbg(TRACE,"<init_stxhdl> missing character_set in <%s>", pclCurSec);
         Terminate(30);
      }
      else
      {
         /* convert to uppercase */
         StringUPR((UCHAR*)pclTmpBuf);

         /* check it */
         if (!strcmp(pclTmpBuf, "ASCII"))
            rgTM.prSection[ilCurSec].iCharacterSet = iASCII;
         else if (!strcmp(pclTmpBuf, "ANSI"))
            rgTM.prSection[ilCurSec].iCharacterSet = iANSI;
         else
         {
            dbg(DEBUG,"<init_stxhdl> unknown character set: <%s>", pclTmpBuf);
            Terminate(30);
         }
      }

      /* read file flag for field names */
      memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "field_name",
                                        CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
         dbg(DEBUG,"<init_stxhdl> missing field_name in <%s>", pclCurSec);
         rgTM.prSection[ilCurSec].iFieldName = iUSE_NONE_FIELDNAME;
      }
      else
      {
         /* to uppercase */
         StringUPR((UCHAR*)pclTmpBuf);

         if (!strcmp(pclTmpBuf, "YES"))
            rgTM.prSection[ilCurSec].iFieldName = iUSE_FIELDNAME;
         else if (!strcmp(pclTmpBuf,"NO"))
            rgTM.prSection[ilCurSec].iFieldName = iUSE_NONE_FIELDNAME;
         else
         {
            dbg(DEBUG,"<init_stxhdl> unknown type of fielname in <%s>-use default", pclCurSec);
            rgTM.prSection[ilCurSec].iFieldName = iUSE_NONE_FIELDNAME;
         }
      }

      /* read all start of block and corresponding end of block characters */
      memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "sblk_character",
                                        CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
      {
         memset((void*)rgTM.prSection[ilCurSec].pcSBlk, 0x00, iMIN);
         rgTM.prSection[ilCurSec].iNoOfBlk = 0;
      }
      else
      {
         /* count block characters */
         if ((rgTM.prSection[ilCurSec].iNoOfBlk =
                        GetNoOfElements(pclTmpBuf, ',')) < 0)
         {
            dbg(TRACE,"<init_stxhdl> GetNoOfElements (iNoOfBlk) returns: %d",
                                             rgTM.prSection[ilCurSec].iNoOfBlk);
            Terminate(30);
         }

         /* for all block characters */
         for (i=0; i<rgTM.prSection[ilCurSec].iNoOfBlk; i++)
         {
            if ((pclS = GetDataField(pclTmpBuf, i, ',')) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> cannot read data field %d", i);
               Terminate(30);
            }
            else
            {
               rgTM.prSection[ilCurSec].pcSBlk[i] = (char)pclS[0];
            }
         }
      }

      if (rgTM.prSection[ilCurSec].iNoOfBlk > 0)
      {
         /* all for end of block */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "eblk_character",
                                           CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
         {
            memset((void*)rgTM.prSection[ilCurSec].pcSBlk, 0x00, iMIN);
            memset((void*)rgTM.prSection[ilCurSec].pcEBlk, 0x00, iMIN);
            rgTM.prSection[ilCurSec].iNoOfBlk = 0;
         }
         else
         {
            /* count block characters */
            if (rgTM.prSection[ilCurSec].iNoOfBlk !=
                           GetNoOfElements(pclTmpBuf, ','))
            {
               dbg(DEBUG,"<init_stxhdl> sblk not equal to eblk");
               memset((void*)rgTM.prSection[ilCurSec].pcSBlk, 0x00, iMIN);
               memset((void*)rgTM.prSection[ilCurSec].pcEBlk, 0x00, iMIN);
               rgTM.prSection[ilCurSec].iNoOfBlk = 0;
            }
            else
            {
               /* for all block characters */
               for (i=0; i<rgTM.prSection[ilCurSec].iNoOfBlk; i++)
               {
                  if ((pclS = GetDataField(pclTmpBuf, i, ',')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read data field %d", i);
                     Terminate(30);
                  }
                  else
                  {
                     rgTM.prSection[ilCurSec].pcEBlk[i] = (char)pclS[0];
                  }
               }
            }
         }
      }

      /* record length... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "ignore_records_shorter_than", CFG_INT,(char*)&rgTM.prSection[ilCurSec].iIgnoreRecordsShorterThan)) != RC_SUCCESS)
      {
         rgTM.prSection[ilCurSec].iIgnoreRecordsShorterThan = 0;
      }
      else
      {
         dbg(DEBUG,"<init_stxhdl> ignore records with length shorter than: %d", rgTM.prSection[ilCurSec].iIgnoreRecordsShorterThan);
      }

      /* strings we want to ignore */
      rgTM.prSection[ilCurSec].iNoOfIgnoreStrings = 0;
      rgTM.prSection[ilCurSec].pcIgnoreStrings = NULL;
      memset((void*)pclIgnoreBuffer, 0x00, iMAX_BUF_SIZE);
      if ((ilRC = iGetConfigRow(pclCfgFile, pclCurSec, "ignore_records_with_first_strings", CFG_STRING,(char*)pclIgnoreBuffer)) == RC_SUCCESS)
      {
         /* count strings */
         rgTM.prSection[ilCurSec].iNoOfIgnoreStrings = GetNoOfElements(pclIgnoreBuffer, ',');

         /* only for valid number of strings */
         if (rgTM.prSection[ilCurSec].iNoOfIgnoreStrings > 0)
         {
            /* get memory for it */
            if ((rgTM.prSection[ilCurSec].pcIgnoreStrings = (char**)malloc(rgTM.prSection[ilCurSec].iNoOfIgnoreStrings*sizeof(char*))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> %05d malloc failure", __LINE__);
               rgTM.prSection[ilCurSec].iNoOfIgnoreStrings = 0;
               Terminate(30);
            }

            /* copy all strings */
            for (ilCurString=0; ilCurString<rgTM.prSection[ilCurSec].iNoOfIgnoreStrings; ilCurString++)
            {
               /* get next string */
               pclS = GetDataField(pclIgnoreBuffer, ilCurString, ',');

               /* look for \" at start and end of string... */
               if (pclS[strlen(pclS)-1] == '"')
                  pclS[strlen(pclS)-1] = 0x00;
               if (pclS[0] == '"')
               {
                  memmove(pclS, &pclS[1], strlen(&pclS[1]));
                  pclS[strlen(pclS)-1] = 0x00;
               }

               /* and store it */
               if ((rgTM.prSection[ilCurSec].pcIgnoreStrings[ilCurString] = (char*)strdup(pclS)) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> %05d strdup failure", __LINE__);
                  Terminate(30);
               }
               dbg(DEBUG,"<init_stxhdl> IgnoreString: <%s>", rgTM.prSection[ilCurSec].pcIgnoreStrings[ilCurString]);
            }
         }
      }

      /* header/trailer lines... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "header", CFG_INT,(char*)&rgTM.prSection[ilCurSec].iHeader)) != RC_SUCCESS)
      {
         rgTM.prSection[ilCurSec].iHeader = 0;
      }
      if (rgTM.prSection[ilCurSec].iHeader < 0)
         rgTM.prSection[ilCurSec].iHeader = 0;
      dbg(DEBUG,"<init_stxhdl> found %d header rows...", rgTM.prSection[ilCurSec].iHeader);

      /* header/trailer lines... */
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "trailer", CFG_INT,(char*)&rgTM.prSection[ilCurSec].iTrailer)) != RC_SUCCESS)
      {
         rgTM.prSection[ilCurSec].iTrailer = 0;
      }
      if (rgTM.prSection[ilCurSec].iTrailer < 0)
         rgTM.prSection[ilCurSec].iTrailer = 0;
      dbg(DEBUG,"<init_stxhdl> found %d trailer rows...", rgTM.prSection[ilCurSec].iTrailer);

      /* read all table entries for this section */
      memset((void*)pclAllTables, 0x00, iMIN_BUF_SIZE);
      if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "tables",
                                     CFG_STRING, pclAllTables)) != RC_SUCCESS)
      {
         dbg(TRACE,"<init_stxhdl> cannot run without tables");
         Terminate(30);
      }

      /* convert to uppercase */
      StringUPR((UCHAR*)pclAllTables);

      /* count table sections */
      if ((rgTM.prSection[ilCurSec].iNoOfTables =
                  GetNoOfElements(pclAllTables, ',')) < 0)
      {
         dbg(TRACE,"<init_stxhdl> GetNoOfElements (iNoOfTables) returns: %d",
                                          rgTM.prSection[ilCurSec].iNoOfTables);
         Terminate(30);
      }

      /* get memory for it */
      if ((rgTM.prSection[ilCurSec].prTabDef = (TSTTabDef*)malloc((size_t)(rgTM.prSection[ilCurSec].iNoOfTables*sizeof(TSTTabDef)))) == NULL)
      {
         dbg(TRACE,"<init_stxhdl> table malloc failure");
         rgTM.prSection[ilCurSec].prTabDef = NULL;
         Terminate(30);
      }

      /* for all tables */
      for (ilCurTable=0;
           ilCurTable<rgTM.prSection[ilCurSec].iNoOfTables;
           ilCurTable++)
      {
         /* get this entry */
         if ((pclS = GetDataField(pclAllTables, ilCurTable, ',')) ==NULL)
         {
            dbg(TRACE,"<init_stxhdl> cannot read data field %d from all tables",                                                                      ilCurTable);
            Terminate(30);
         }

         /* copy to buffer */
         strcpy(pclCurTable, pclS);

         dbg(DEBUG,"<init_stxhdl> ====== START TABLE <%s> ======", pclCurTable);

         /* debug message */
         dbg(DEBUG,"<init_stxhdl> trying to read table section <%s>", pclS);

         /* read all for syslib... */
         memset((void*)pclAllSyslib, 0x00, iMAXIMUM);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "syslib", CFG_STRING, pclAllSyslib)) != RC_SUCCESS)
         {
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib = 0;
         }
         else
         {
            /* convert to uppercase */
            StringUPR((UCHAR*)pclAllSyslib);

            /* compute number of syslib strings... */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib = GetNoOfElements(pclAllSyslib, '"')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements (syslib) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib);
               Terminate(30);
            }

            /* delete one element */
            dbg(DEBUG,"<init_stxhdl> found %d syslib entries...", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib);
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib--;
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib /= 2;
            dbg(DEBUG,"<init_stxhdl> now %d syslib entries...", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib);

            /* get memory for all syslib entries... */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib = (SysLib*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib*sizeof(SysLib)))) == NULL)
            {
               /* syslib malloc failure... */
               dbg(TRACE,"<init_stxhdl> syslib malloc failure...");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib = NULL;
               Terminate(30);
            }

            /* separate all syslib entries... */
            for (ilSyslibNo=0; ilSyslibNo<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoSyslib; ilSyslibNo++)
            {
               /* get n.th element */
               memset((void*)pclCurSysLib, 0x00, iMIN_BUF_SIZE);
               if ((ilRC = GetAllBetween(pclAllSyslib, '"', ilSyslibNo, pclCurSysLib)) < 0)
               {
                  dbg(TRACE,"<init_stxhdl> GetAllBetween returns: %d", ilRC);
                  Terminate(30);
               }

               /* work with current entry */
               if ((ilSysTmpMax = GetNoOfElements(pclCurSysLib, ',')) < 0)
               {
                  dbg(TRACE,"<init_stxhdl> GetNoOfElements (CurSysLib) returns: %d", ilRC);
                  Terminate(30);
               }

               /* check result */
               if (ilSysTmpMax != 4)
               {
                  dbg(TRACE,"<init_stxhdl> SysTmpMax isn't 4...");
                  Terminate(30);
               }

               /* over all entries... */
               for (ilSysTmpNo=0; ilSysTmpNo<ilSysTmpMax; ilSysTmpNo++)
               {
                  /* get next field... */
                  if ((pclS = GetDataField(pclCurSysLib, ilSysTmpNo, ',')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> GetDataField (syslib) returns NULL");
                     Terminate(30);
                  }

                  /* store it... */
                  switch (ilSysTmpNo)
                  {
                     case 0:
                        strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib[ilSyslibNo].pcTabName, pclS);
                        break;

                     case 1:
                        strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib[ilSyslibNo].pcTabField, pclS);
                        break;

                     case 2:
                        strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib[ilSyslibNo].pcIField, pclS);
                        break;

                     case 3:
                        strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].prSyslib[ilSyslibNo].pcResultField, pclS);
                        break;
                  }
               }
            }
         }

         /* read entry delete_blanks_at_end */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "delete_end_blanks", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
         {
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteEndBlanks = 0;
         }
         else
         {
            /* convert to uppercase */
            StringUPR((UCHAR*)pclTmpBuf);

            if (!strcmp(pclTmpBuf, "YES"))
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteEndBlanks = 1;
            else if (!strcmp(pclTmpBuf, "NO"))
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteEndBlanks = 0;
            else
            {
               dbg(DEBUG,"<init_stxhdl> unknown entry %s (delete_end_blanks)", pclTmpBuf);
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteEndBlanks = 0;
            }
         }

         /* read entry delete_blanks_at_head */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "delete_start_blanks", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
         {
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteStartBlanks = 0;
         }
         else
         {
            /* convert to uppercase */
            StringUPR((UCHAR*)pclTmpBuf);

            if (!strcmp(pclTmpBuf, "YES"))
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteStartBlanks = 1;
            else if (!strcmp(pclTmpBuf, "NO"))
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteStartBlanks = 0;
            else
            {
               dbg(DEBUG,"<init_stxhdl> unknown entry %s (delete_start_blanks)", pclTmpBuf);
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iDeleteStartBlanks = 0;
            }
         }

         /* read mod_id... */
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "mod_id", CFG_INT, (char*)&rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iModID)) != RC_SUCCESS)
         {
            dbg(DEBUG,"<init_stxhdl> mod_id not found (%d)", ilRC);
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iModID = -1;
         }
         dbg(DEBUG,"<init_stxhdl> route is %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iModID);

         /* read table name */
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "table", CFG_STRING, rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcTableName)) != RC_SUCCESS)
         {
            dbg(TRACE,"<init_stxhdl> cannot run without table name");
            Terminate(30);
         }

         /* convert table name to uppercase */
         StringUPR((UCHAR*)rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcTableName);

         /* read field name */
         memset((void*)pclAllFields, 0x00, iMAXIMUM);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "fields", CFG_STRING, pclAllFields)) != RC_SUCCESS)
         {
            dbg(TRACE,"<init_stxhdl> cannot run without fields");
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piDataLength = NULL;
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcFields  = NULL;
            Terminate(30);
         }
         else
         {
            /* convert fields to uppercase */
            StringUPR((UCHAR*)pclAllFields);

            /* Hopo in FieldList ? */
            if(strstr(pclAllFields,"HOPO") == NULL)
						  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iHopoInFieldlist = 1;
						else
						  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iHopoInFieldlist = 0;

            /* count fields */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields = GetNoOfElements(pclAllFields, ',')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements(iNoOfFields) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields);
               Terminate(30);
            }

            /* get memory for it */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcFields = (char**)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char*)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> field malloc failure");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcFields = NULL;
               Terminate(30);
            }

            /* memory for delete_fieldname_charater... */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldCharacter = (char**)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char*)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> Error allocating memory (ALL CHARACTER)");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldCharacter = NULL;
               Terminate(30);
            }

            /* memory for delete_fieldname_start_charater... */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldStartCharacter = (char**)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char*)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> Error allocating memory (START)");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldStartCharacter = NULL;
               Terminate(30);
            }

            /* memory for delete_fieldname_start_charater... */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldEndCharacter = (char**)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char*)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> Error allocating memory (END)");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldEndCharacter = NULL;
               Terminate(30);
            }

            /* for all fields */
            for (ilCurField=0; ilCurField<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields; ilCurField++)
            {
               if ((pclS = GetDataField(pclAllFields, ilCurField, ',')) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> cannot read field no. %d", ilCurField);
                  Terminate(30);
               }

               /* store this field */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcFields[ilCurField] = strdup(pclS)) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> cannot duplicate field <%s>", pclS);
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcFields[ilCurField] = NULL;
                  Terminate(30);
               }

               /* build keyword for this field... */
               memset((void*)pclStartKeyWord, 0x00, iMIN_BUF_SIZE);
               memset((void*)pclEndKeyWord, 0x00, iMIN_BUF_SIZE);
               memset((void*)pclAllKeyWord, 0x00, iMIN_BUF_SIZE);
               sprintf(pclStartKeyWord, "delete_%s_start_character", pclS);
               sprintf(pclEndKeyWord, "delete_%s_end_character", pclS);
               sprintf(pclAllKeyWord, "delete_%s_character", pclS);

               /* convert to lowercase */
               StringLWR((UCHAR*)pclStartKeyWord);
               StringLWR((UCHAR*)pclEndKeyWord);
               StringLWR((UCHAR*)pclAllKeyWord);

               /* try to read the characters... */
               /* first start */
               memset((void*)pclAllCharacter, 0x00, iMIN_BUF_SIZE);
               if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, pclStartKeyWord, CFG_STRING, pclAllCharacter)) == RC_SUCCESS)
               {
                  /* found all characters... */
                  if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldStartCharacter[ilCurField] = strdup(pclAllCharacter)) == NULL)
                  {
                     /* not enough memory to run... */
                     dbg(TRACE,"<init_stxhdl> can't duplicate start character");
                     Terminate(30);
                  }
               }
               else
               {
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldStartCharacter[ilCurField] = NULL;
               }

               /* second end */
               memset((void*)pclAllCharacter, 0x00, iMIN_BUF_SIZE);
               if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, pclEndKeyWord, CFG_STRING, pclAllCharacter)) == RC_SUCCESS)
               {
                  /* found all characters... */
                  if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldEndCharacter[ilCurField] = strdup(pclAllCharacter)) == NULL)
                  {
                     /* not enough memory to run... */
                     dbg(TRACE,"<init_stxhdl> can't duplicate start character");
                     Terminate(30);
                  }
               }
               else
               {
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldEndCharacter[ilCurField] = NULL;
               }

               /* third all */
               memset((void*)pclAllCharacter, 0x00, iMIN_BUF_SIZE);
               if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, pclAllKeyWord, CFG_STRING, pclAllCharacter)) == RC_SUCCESS)
               {
                  /* found all characters... */
                  if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldCharacter[ilCurField] = strdup(pclAllCharacter)) == NULL)
                  {
                     /* not enough memory to run... */
                     dbg(TRACE,"<init_stxhdl> can't duplicate all character");
                     Terminate(30);
                  }
               }
               else
               {
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDeleteFieldCharacter[ilCurField] = NULL;
               }
            }
         }

         /* get memory for alignment */
         if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcAlignment = (char*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char)))) == NULL)
         {
            dbg(TRACE,"<init_stxhdl> alignment malloc failure");
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcAlignment = NULL;
            Terminate(30);
         }

         /* read alignment... */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "alignment", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
         {
            /* convert to uppercase */
            StringUPR((UCHAR*)pclTmpBuf);

            /* count elements */
            if ((ilAlignCnt = GetNoOfElements(pclTmpBuf, ',')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements(alignment) returns: %d", ilAlignCnt);
               Terminate(30);
            }

            /* if there are more alignment entries than fields... */
            if (ilAlignCnt > rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields)
               ilAlignCnt = rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields;

            /* for all elements... */
            for (ilCurAlign=0; ilCurAlign<ilAlignCnt; ilCurAlign++)
            {
               /* get n.th element from list... */
               if ((pclS = GetDataField(pclTmpBuf, ilCurAlign, ',')) == NULL)
               {
                  dbg(DEBUG,"<init_stxhdl> GetDataField(alignment) returns NULL");
                  Terminate(30);
               }

               /* store this element in intenal structure... */
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcAlignment[ilCurAlign] = pclS[0];
            }

            /* if there are not enough elements in list... */
            if (ilAlignCnt < rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields)
            {
               dbg(DEBUG,"<init_stxhdl> not enough Align-Signs in list...");

               /* new starting point... */
               ilCurAlign = ilAlignCnt;

               dbg(DEBUG,"<init_stxhdl> missing %d elements...", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields - ilAlignCnt);

               /* calculate number of missing elements... */
               ilAlignCnt = rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields;
               /* for all missing elements */
               for (; ilCurAlign<ilAlignCnt; ilCurAlign++)
               {
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcAlignment[ilCurAlign] = 'O';
               }
            }
         }
         else
         {
            /* cannot find entry in file... */
            /* for all missing elements */
            for (ilCurAlign=0; ilCurAlign<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields; ilCurAlign++)
            {
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcAlignment[ilCurAlign] = 'O';
            }
         }

         /* read upper/lower/original sign... (ulo) */
         /* get memory for it */
         if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcULOSign = (char*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields*sizeof(char)))) == NULL)
         {
            dbg(TRACE,"<init_stxhdl> ulo-sign malloc failure");
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcULOSign = NULL;
            Terminate(30);
         }

         /* read upper/lower/original sign... (ulo) */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "ulo_sign", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
         {
            /* convert to uppercase */
            StringUPR((UCHAR*)pclTmpBuf);

            /* count elements */
            if ((ilULOCnt = GetNoOfElements(pclTmpBuf, ',')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements(ulo_sign) returns: %d", ilULOCnt);
               Terminate(30);
            }

            /* if there are more ULO's than fields... */
            if (ilULOCnt > rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields)
               ilULOCnt = rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields;

            /* for all elements... */
            for (ilCurULO=0; ilCurULO<ilULOCnt; ilCurULO++)
            {
               /* get n.th element from list... */
               if ((pclS = GetDataField(pclTmpBuf, ilCurULO, ',')) == NULL)
               {
                  dbg(DEBUG,"<init_stxhdl> GetDataField(ulo_sign) returns NULL");
                  Terminate(30);
               }

               /* store this element in intenal structure... */
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcULOSign[ilCurULO] = pclS[0];
            }

            /* if there are not enough elements in list... */
            if (ilULOCnt < rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields)
            {
               dbg(DEBUG,"<init_stxhdl> not enough ULO-Signs in list...");

               /* new starting point... */
               ilCurULO = ilULOCnt;

               dbg(DEBUG,"<init_stxhdl> missing %d elements...", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields - ilULOCnt);

               /* calculate number of missing elements... */
               ilULOCnt = rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields;
               /* for all missing elements */
               for (; ilCurULO<ilULOCnt; ilCurULO++)
               {
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcULOSign[ilCurULO] = 'O';
               }
            }
         }
         else
         {
            /* cannot find entry in file... */
            /* for all missing elements */
            for (ilCurULO=0; ilCurULO<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFields; ilCurULO++)
            {
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcULOSign[ilCurULO] = 'O';
            }
         }

         /* get length of fields in database, important for HandleArray
         (ArrayInset, ArrayUpdate)
         */
         memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "field_length", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
         {
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].psFieldLength = NULL;
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFieldLength = 0;
         }
         else
         {
            /* count elements */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFieldLength = GetNoOfElements(pclTmpBuf, ',')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements(iNoOfFieldLength) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFieldLength);
               Terminate(30);
            }

            /* get memory for it */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].psFieldLength = (short*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFieldLength*sizeof(short)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> field-length malloc failure");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].psFieldLength = NULL;
               Terminate(30);
            }

            /* for all data lengthes... */
            for (ilCurFieldLength=0; ilCurFieldLength<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfFieldLength; ilCurFieldLength++)
            {
               /* get element no. n */
               if ((pclS = GetDataField(pclTmpBuf, ilCurFieldLength, ',')) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> cannot read field-length element number %d", ilCurFieldLength);
                  Terminate(30);
               }

               /* store it */
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].psFieldLength[ilCurFieldLength] = atoi(pclS);
            }
         }

         /* default for iNoOfDataPosition is zero... */
         rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition = 0;

         /* get data length (if necessary) */
         if (IsFix(rgTM.prSection[ilCurSec].iFileType))
         {
            /* this is for positions */
            memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
            if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "data_position", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
            {
               /* set flag */
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength = 0;
               rgTM.prSection[ilCurSec].iSULength = 0;

               dbg(DEBUG,"<init_stxhdl> DATA_POSITION: <%s>", pclTmpBuf);
               /* count elements */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition = GetNoOfElements(pclTmpBuf, ',')) < 0)
               {
                  dbg(TRACE,"<init_stxhdl> GetNoOfElements(iNoOfDataPosition) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength);
                  Terminate(30);
               }
               dbg(DEBUG,"<init_stxhdl> POSITION-COUNTER: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition);

               /* get memory for it */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition = (UINT*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition*sizeof(UINT)))) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> start-data-position malloc failure");
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition = NULL;
                  Terminate(30);
               }

               /* get memory for it */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piEndDataPosition = (UINT*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition*sizeof(UINT)))) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> end-data-length malloc failure");
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piEndDataPosition = NULL;
                  Terminate(30);
               }

               /* for all data lengthes... */
               for (ilCurDataPos=0; ilCurDataPos<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition; ilCurDataPos++)
               {
                  /* get element no. n */
                  if ((pclS = GetDataField(pclTmpBuf, ilCurDataPos, ',')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read data-position element number %d", ilCurDataPos);
                     Terminate(30);
                  }

                  /* store it */
                  strcpy(pclCurPos, pclS);

                  /* get start position */
                  if ((pclS = GetDataField(pclCurPos, 0, ':')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read start-data-position element");
                     Terminate(30);
                  }

                  /* store start position */
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition[ilCurDataPos] = atoi(pclS);

                  /* get end position */
                  if ((pclS = GetDataField(pclCurPos, 1, ':')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read end-data-position element");
                     Terminate(30);
                  }

                  /* store end position */
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piEndDataPosition[ilCurDataPos] = atoi(pclS);
               }
            }
            else
            {
               /* data_length */
               memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
               if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "data_length", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
               {
                  dbg(TRACE,"<init_stxhdl> missing data_length and data_position, can't work...");
                  Terminate(30);
               }

               /* count elements */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength = GetNoOfElements(pclTmpBuf, ',')) < 0)
               {
                  dbg(TRACE,"<init_stxhdl> GetNoOfElements(iNoOfDataLength) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength);
                  Terminate(30);
               }

               /* get memory for it */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piDataLength = (UINT*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength*sizeof(UINT)))) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> data-length malloc failure");
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piDataLength =
                                                                           NULL;
                  Terminate(30);
               }

               /* for all data lengthes... */
               rgTM.prSection[ilCurSec].iSULength = 0;
               for (ilCurDataLength=0; ilCurDataLength<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataLength; ilCurDataLength++)
               {
                  /* get element no. n */
                  if ((pclS = GetDataField(pclTmpBuf, ilCurDataLength, ',')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read data-length element number %d", ilCurDataLength);
                     Terminate(30);
                  }

                  /* store it */
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piDataLength[ilCurDataLength] = atoi(pclS);

                  /* store whole data length */
                  rgTM.prSection[ilCurSec].iSULength += atoi(pclS);
               }
               dbg(DEBUG,"<init_stxhdl> SU Length: %d", rgTM.prSection[ilCurSec].iSULength);
            }
         }
         else
         {
            /* this is dynamic... */
            /* in this case the data_position is like a counter */
            memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
            if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "data_position", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
            {
               /* here we use StartDataPosition */
               /* count elements */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition = GetNoOfElements(pclTmpBuf, ',')) < 0)
               {
                  dbg(TRACE,"<init_stxhdl> %05d GetNoOfElements(iNoOfDataPosition) returns: %d", __LINE__, rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition);
                  Terminate(30);
               }
               dbg(DEBUG,"<init_stxhdl> POSITION-COUNTER: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition);

               /* get memory for it */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition = (UINT*)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition*sizeof(UINT)))) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> start-data-position malloc failure");
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition = NULL;
                  Terminate(30);
               }

               /* set positions in internal structures */
               for (ilCurDataPos=0; ilCurDataPos<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfDataPosition; ilCurDataPos++)
               {
                  /* get element no. n */
                  if ((pclS = GetDataField(pclTmpBuf, ilCurDataPos, ',')) == NULL)
                  {
                     dbg(TRACE,"<init_stxhdl> cannot read data-position element number %d", ilCurDataLength);
                     Terminate(30);
                  }

                  /* store start position */
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition[ilCurDataPos] = atoi(pclS);
                  dbg(DEBUG,"<init_stxhdl> POSITION: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].piStartDataPosition[ilCurDataPos]);
               }
            }
         }

/* gle: read where-field-names add in starts here */

         /* read where field names */
         memset((void*)pclWhereFields, 0x00, iMAXIMUM);
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "where_fields", CFG_STRING, pclWhereFields)) != RC_SUCCESS)
         {
            dbg(DEBUG,"<init_stxhdl> no where fields");
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfWhereFields = 0;
            rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcWhereFields  = NULL;
         }
         else
         {
            /* convert fields to uppercase */
            StringUPR((UCHAR*)pclWhereFields);

            /* count where fields */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfWhereFields = GetNoOfElements(pclWhereFields, ',')) < 0)
            {
               dbg(TRACE,"<init_stxhdl> GetNoOfElements(iNoOfWhereFields) returns: %d", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfWhereFields);
               Terminate(30);
            }

            /* get memory for it */
            if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcWhereFields = (char**)malloc((size_t)(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfWhereFields*sizeof(char*)))) == NULL)
            {
               dbg(TRACE,"<init_stxhdl> where field malloc failure");
               rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcWhereFields = NULL;
               Terminate(30);
            }

	/* for all where fields */
            for (ilCurField=0; ilCurField<rgTM.prSection[ilCurSec].prTabDef[ilCurTable].iNoOfWhereFields; ilCurField++)
            {
               if ((pclS = GetDataField(pclWhereFields, ilCurField, ',')) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> cannot read field no. %d", ilCurField);
                  Terminate(30);
               }

               /* store this field */
               if ((rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcWhereFields[ilCurField] = strdup(pclS)) == NULL)
               {
                  dbg(TRACE,"<init_stxhdl> cannot duplicate field <%s>", pclS);
                  rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcWhereFields[ilCurField] = NULL;
                  Terminate(30);
               }
	}
         } /* gle: read where-field-names add in ends here */


         /* send command snd_cmd */
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurTable, "snd_cmd", CFG_STRING, rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcSndCmd)) != RC_SUCCESS)
         {
            dbg(TRACE,"<init_stxhdl> cannot run without snd_cmd");
            Terminate(30);
         }

         /* convert commant to uppercase */
         StringUPR((UCHAR*)rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcSndCmd);

         /*rkl 21.10.99 --------------------------------------------------*/
         /*keywords recv_name, dest_name ---------------------------------*/
         /* the recv_name  */
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "recv_name", CFG_STRING, rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcRecvName)) != RC_SUCCESS)
         {
            strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcRecvName, "EXCO");
         }
         dbg(DEBUG,"<init_stxhdl> RecvName: <%s>", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcRecvName);

         /* the dest_name  */
         if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurSec, "dest_name", CFG_STRING, rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDestName)) != RC_SUCCESS)
         {
            strcpy(rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDestName, "STXHDL");
         }
         dbg(DEBUG,"<init_stxhdl> DestName: <%s>", rgTM.prSection[ilCurSec].prTabDef[ilCurTable].pcDestName);

         /*rkl -----------------------------------------------------------*/

         dbg(DEBUG,"<init_stxhdl> ====== END TABLE <%s> ======", pclCurTable);
      }
      dbg(DEBUG,"<init_stxhdl> ========== END <%s> ===========", pclCurSec);
   }

   /* get mod-id of router */
   if ((igRouterID = tool_get_q_id("router")) == RC_NOT_FOUND ||
        igRouterID == RC_FAIL)
   {
      dbg(TRACE,"<init_stdhxl>: tool_get_q_id(router) returns: %d", igRouterID);
      Terminate(30);
   }

   /* good bye */
   return RC_SUCCESS;
}


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset(void)
{
   int   i;
   int   j;
   int   k;
   int   ilRC = RC_SUCCESS;

   dbg(DEBUG,"<Reset> now resetting");

   /* free all memory */
   if (prgItem != NULL)
   {
      dbg(DEBUG,"<Reset> free prgItem");
      free((void*)prgItem);
      prgItem = NULL;
   }

   /* free structures */
   for (i=0; i<rgTM.iNoOfSections; i++)
   {
      for (j=0; j<rgTM.prSection[i].iNoOfTables; j++)
      {
         for (k=0; k<rgTM.prSection[i].prTabDef[j].iNoOfFields; k++)
         {
            if (rgTM.prSection[i].prTabDef[j].pcFields[k] != NULL)
            {
               dbg(DEBUG,"<Reset> free Fields");
               free((void*)rgTM.prSection[i].prTabDef[j].pcFields[k]);
               rgTM.prSection[i].prTabDef[j].pcFields[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Reset> free DeleteFieldStartCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Reset> free DeleteFieldEndCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Reset> free DeleteFieldCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k] = NULL;
            }
         }

         if (rgTM.prSection[i].prTabDef[j].pcFields != NULL)
         {
            dbg(DEBUG,"<Reset> free Pointer to Fields");
            free((void*)rgTM.prSection[i].prTabDef[j].pcFields);
            rgTM.prSection[i].prTabDef[j].pcFields = NULL;
         }

/* gle: reset where-clause-fields add in starts here */

         for (k = 0; k < rgTM.prSection[i].prTabDef[j].iNoOfWhereFields; k ++)
         {
            if (rgTM.prSection[i].prTabDef[j].pcWhereFields[k] != NULL)
            {
               dbg(DEBUG,"<Reset> free Where-Fields");
               free((void*)rgTM.prSection[i].prTabDef[j].pcWhereFields[k]);
               rgTM.prSection[i].prTabDef[j].pcWhereFields[k] = NULL;
            }
	 } /* end for freeing where-fields */

	 if (rgTM.prSection[i].prTabDef[j].pcWhereFields != NULL)
         {
            dbg(DEBUG,"<Reset> free Pointer to Where-Fields");
            free((void*)rgTM.prSection[i].prTabDef[j].pcWhereFields);
            rgTM.prSection[i].prTabDef[j].pcWhereFields = NULL;
         }

/* gle: reset where-clause-fields add in ends here */

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter != NULL)
         {
            dbg(DEBUG,"<Reset> free pointer to DeleteFieldStartCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter != NULL)
         {
            dbg(DEBUG,"<Reset> free pointer to DeleteFieldEndCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter != NULL)
         {
            dbg(DEBUG,"<Reset> free pointer to DeleteFieldCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].iNoOfFieldLength > 0)
         {
            if (rgTM.prSection[i].prTabDef[j].psFieldLength != NULL)
            {
               dbg(DEBUG,"<Reset> free field length");
               free((void*)rgTM.prSection[i].prTabDef[j].psFieldLength);
               rgTM.prSection[i].prTabDef[j].psFieldLength = NULL;
            }
         }

         if (rgTM.prSection[i].prTabDef[j].iNoSyslib > 0)
         {
            if (rgTM.prSection[i].prTabDef[j].prSyslib != NULL)
            {
               dbg(DEBUG,"<Reset> free syslib");
               free((void*)rgTM.prSection[i].prTabDef[j].prSyslib);
               rgTM.prSection[i].prTabDef[j].prSyslib = NULL;
            }
         }

         if (IsFix(rgTM.prSection[i].iFileType))
         {
            if (rgTM.prSection[i].prTabDef[j].iNoOfDataLength > 0)
            {
               if (rgTM.prSection[i].prTabDef[j].piDataLength != NULL)
               {
                  dbg(DEBUG,"<Reset> free data length");
                  free((void*)rgTM.prSection[i].prTabDef[j].piDataLength);
                  rgTM.prSection[i].prTabDef[j].piDataLength = NULL;
               }
            }
            if (rgTM.prSection[i].prTabDef[j].iNoOfDataPosition > 0)
            {
               if (rgTM.prSection[i].prTabDef[j].piStartDataPosition != NULL)
               {
                  dbg(DEBUG,"<Reset> free start data position");
                  free((void*)rgTM.prSection[i].prTabDef[j].piStartDataPosition);
                  rgTM.prSection[i].prTabDef[j].piStartDataPosition = NULL;
               }
               if (rgTM.prSection[i].prTabDef[j].piEndDataPosition != NULL)
               {
                  dbg(DEBUG,"<Reset> free end data position");
                  free((void*)rgTM.prSection[i].prTabDef[j].piEndDataPosition);
                  rgTM.prSection[i].prTabDef[j].piEndDataPosition = NULL;
               }
            }
         }

         if (rgTM.prSection[i].prTabDef[j].pcULOSign != NULL)
         {
            dbg(DEBUG,"<Reset> free ULO-Sign");
            free((void*)rgTM.prSection[i].prTabDef[j].pcULOSign);
            rgTM.prSection[i].prTabDef[j].pcULOSign = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcAlignment != NULL)
         {
            dbg(DEBUG,"<Reset> free Alignment");
            free((void*)rgTM.prSection[i].prTabDef[j].pcAlignment);
            rgTM.prSection[i].prTabDef[j].pcAlignment = NULL;
         }
      }

      if (rgTM.prSection[i].prTabDef != NULL)
      {
         dbg(DEBUG,"<Reset> prTabDef");
         free((void*)rgTM.prSection[i].prTabDef);
         rgTM.prSection[i].prTabDef = NULL;
      }
   }

   if (rgTM.prSection != NULL)
   {
      dbg(DEBUG,"<Reset> prSection");
      free((void*)rgTM.prSection);
      rgTM.prSection = NULL;
   }

   /* now initialize it */
   if ((ilRC = init_stxhdl()) != RC_SUCCESS)
   {
      dbg(TRACE,"<Reset> init_stxhdl retuns with: %d", ilRC);
      Terminate(0);
   }

   /* for debugging */
   dbg(DEBUG,"<Reset> will return with: %d", ilRC);

   /* bye bye */
   return ilRC;
}


/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(UINT   ipSleepTime)
{
   int   i;
   int   j;
   int   k;

   /* ignore all signals */
   (void)UnsetSignals();

   /* thats good */
   if (ipSleepTime > 0)
   {
      dbg(DEBUG,"<Terminate> sleeping %d seconds...", ipSleepTime);
      sleep(ipSleepTime);
   }

   /* free all memory */
   if (prgItem != NULL)
   {
      dbg(DEBUG,"<Terminate> free prgItem");
      free((void*)prgItem);
      prgItem = NULL;
   }

   /* free structures */
   for (i=0; i<rgTM.iNoOfSections; i++)
   {
      for (j=0; j<rgTM.prSection[i].iNoOfTables; j++)
      {
         for (k=0; k<rgTM.prSection[i].prTabDef[j].iNoOfFields; k++)
         {
            if (rgTM.prSection[i].prTabDef[j].pcFields[k] != NULL)
            {
               dbg(DEBUG,"<Terminate> free Fields");
               free((void*)rgTM.prSection[i].prTabDef[j].pcFields[k]);
               rgTM.prSection[i].prTabDef[j].pcFields[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Terminate> free DeleteFieldStartCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Terminate> free DeleteFieldEndCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter[k] = NULL;
            }
            if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k] != NULL)
            {
               dbg(DEBUG,"<Terminate> free DeleteFieldCharacter");
               free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k]);
               rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter[k] = NULL;
            }
         }

         if (rgTM.prSection[i].prTabDef[j].pcFields != NULL)
         {
            dbg(DEBUG,"<Terminate> free Pointer to Fields");
            free((void*)rgTM.prSection[i].prTabDef[j].pcFields);
            rgTM.prSection[i].prTabDef[j].pcFields = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter != NULL)
         {
            dbg(DEBUG,"<Terminate> free pointer to DeleteFieldStartCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldStartCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter != NULL)
         {
            dbg(DEBUG,"<Terminate> free pointer to DeleteFieldEndCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldEndCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter != NULL)
         {
            dbg(DEBUG,"<Terminate> free pointer to DeleteFieldCharacter");
            free((void*)rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter);
            rgTM.prSection[i].prTabDef[j].pcDeleteFieldCharacter = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].iNoOfFieldLength > 0)
         {
            if (rgTM.prSection[i].prTabDef[j].psFieldLength != NULL)
            {
               dbg(DEBUG,"<Terminate> free field length");
               free((void*)rgTM.prSection[i].prTabDef[j].psFieldLength);
               rgTM.prSection[i].prTabDef[j].psFieldLength = NULL;
            }
         }

         if (rgTM.prSection[i].prTabDef[j].iNoSyslib > 0)
         {
            if (rgTM.prSection[i].prTabDef[j].prSyslib != NULL)
            {
               dbg(DEBUG,"<Terminate> free syslib");
               free((void*)rgTM.prSection[i].prTabDef[j].prSyslib);
               rgTM.prSection[i].prTabDef[j].prSyslib = NULL;
            }
         }

         if (IsFix(rgTM.prSection[i].iFileType))
         {
            if (rgTM.prSection[i].prTabDef[j].iNoOfDataLength > 0)
            {
               if (rgTM.prSection[i].prTabDef[j].piDataLength != NULL)
               {
                  dbg(DEBUG,"<Terminate> free data length");
                  free((void*)rgTM.prSection[i].prTabDef[j].piDataLength);
                  rgTM.prSection[i].prTabDef[j].piDataLength = NULL;
               }
            }
            if (rgTM.prSection[i].prTabDef[j].iNoOfDataPosition > 0)
            {
               if (rgTM.prSection[i].prTabDef[j].piStartDataPosition != NULL)
               {
                  dbg(DEBUG,"<Terminate> free start data position");
                  free((void*)rgTM.prSection[i].prTabDef[j].piStartDataPosition);
                  rgTM.prSection[i].prTabDef[j].piStartDataPosition = NULL;
               }
               if (rgTM.prSection[i].prTabDef[j].piEndDataPosition != NULL)
               {
                  dbg(DEBUG,"<Terminate> free end data position");
                  free((void*)rgTM.prSection[i].prTabDef[j].piEndDataPosition);
                  rgTM.prSection[i].prTabDef[j].piEndDataPosition = NULL;
               }
            }
         }

         if (rgTM.prSection[i].prTabDef[j].pcULOSign != NULL)
         {
            dbg(DEBUG,"<Terminate> free ULO-Sign");
            free((void*)rgTM.prSection[i].prTabDef[j].pcULOSign);
            rgTM.prSection[i].prTabDef[j].pcULOSign = NULL;
         }

         if (rgTM.prSection[i].prTabDef[j].pcAlignment != NULL)
         {
            dbg(DEBUG,"<Reset> free Alignment");
            free((void*)rgTM.prSection[i].prTabDef[j].pcAlignment);
            rgTM.prSection[i].prTabDef[j].pcAlignment = NULL;
         }
      }

      if (rgTM.prSection[i].prTabDef != NULL)
      {
         dbg(DEBUG,"<Terminate> prTabDef");
         free((void*)rgTM.prSection[i].prTabDef);
         rgTM.prSection[i].prTabDef = NULL;
      }
   }

   if (rgTM.prSection != NULL)
   {
      dbg(DEBUG,"<Terminate> prSection");
      free((void*)rgTM.prSection);
      rgTM.prSection = NULL;
   }

   /* only a message */
   dbg(TRACE,"<Terminate>: now leaving ...");

   /* close logfile */
   CLEANUP;

   /* the end */
   exit(0);
}


/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/
static void HandleSignal(int pipSig)
{
   dbg(DEBUG,"<HandleSignal> signal <%d> received",pipSig);

   switch(pipSig)
   {
      default:
         Terminate(0);
         break;
   }

   /* back to calling function */
   return;
}


/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int ipErr)
{
   dbg(DEBUG,"<HandleErr> calling with %d", ipErr);

   /* back to calling function */
   return;
}


/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr(int pipErr)
{
   switch(pipErr)
   {
      case   QUE_E_FUNC   :   /* Unknown function */
         dbg(TRACE,"<%d> : unknown function",pipErr);
         break;

      case   QUE_E_MEMORY   :   /* Malloc reports no memory */
         dbg(TRACE,"<%d> : malloc failed",pipErr);
         break;

      case   QUE_E_SEND   :   /* Error using msgsnd */
         dbg(TRACE,"<%d> : msgsnd failed",pipErr);
         break;

      case   QUE_E_GET   :   /* Error using msgrcv */
         dbg(TRACE,"<%d> : msgrcv failed",pipErr);
         break;

      case   QUE_E_EXISTS   :
         dbg(TRACE,"<%d> : route/queue already exists ",pipErr);
         break;

      case   QUE_E_NOFIND   :
         dbg(TRACE,"<%d> : route not found ",pipErr);
         break;

      case   QUE_E_ACKUNEX   :
         dbg(TRACE,"<%d> : unexpected ack received ",pipErr);
         break;

      case   QUE_E_STATUS   :
         dbg(TRACE,"<%d> :  unknown queue status ",pipErr);
         break;

      case   QUE_E_INACTIVE   :
         dbg(TRACE,"<%d> : queue is inaktive ",pipErr);
         break;

      case   QUE_E_MISACK   :
         dbg(TRACE,"<%d> : missing ack ",pipErr);
         break;

      case   QUE_E_NOQUEUES   :
         dbg(TRACE,"<%d> : queue does not exist",pipErr);
         break;

      case   QUE_E_RESP   :   /* No response on CREATE */
         dbg(TRACE,"<%d> : no response on create",pipErr);
         break;

      case   QUE_E_FULL   :
         dbg(TRACE,"<%d> : too many route destinations",pipErr);
         break;

      case   QUE_E_NOMSG   :   /* No message on queue */
         dbg(TRACE,"<%d> : no messages on queue",pipErr);
         break;

      case   QUE_E_INVORG   :   /* Mod id by que call is 0 */
         dbg(TRACE,"<%d> : invalid originator=0",pipErr);
         break;

      case   QUE_E_NOINIT   :   /* Queues is not initialized*/
         dbg(TRACE,"<%d> : queues are not initialized",pipErr);
         break;

      case   QUE_E_ITOBIG   :
         dbg(TRACE,"<%d> : requestet itemsize to big ",pipErr);
         break;

      case   QUE_E_BUFSIZ   :
         dbg(TRACE,"<%d> : receive buffer to small ",pipErr);
         break;

      default         :   /* Unknown queue error */
         dbg(TRACE,"<%d> : unknown error",pipErr);
         break;
   }
   return;
}


/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues(void)
{
   int   ilRC = RC_SUCCESS;
   int   ilBreakOut = FALSE;

   do
   {
      /* get item... */
      ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

      /* set event pointer       */
      prgEvent = (EVENT*)prgItem->text;

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
            case HSB_STANDBY:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               break;

            case HSB_COMING_UP:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               break;

            case HSB_ACTIVE:
            case HSB_STANDALONE:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               ilBreakOut = TRUE;
               break;

            case HSB_ACT_TO_SBY:
               ctrl_sta = prgEvent->command;
               send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
               break;

            case HSB_DOWN:
               ctrl_sta = prgEvent->command;
               Terminate(0);
               break;

            case SHUTDOWN:
               Terminate(0);
               break;

            case RESET:
               ilRC = Reset();
               break;

            case EVENT_DATA:
               dbg(DEBUG,"<HandleQueues> wrong hsb status <%d>",ctrl_sta);
               DebugPrintItem(TRACE,prgItem);
               DebugPrintEvent(TRACE,prgEvent);
               break;

            case TRACE_ON:
               dbg_handle_debug(prgEvent->command);
               break;

            case TRACE_OFF:
               dbg_handle_debug(prgEvent->command);
               break;

            default:
               dbg(DEBUG,"HandleQueues: unknown event");
               DebugPrintItem(TRACE,prgItem);
               DebugPrintEvent(TRACE,prgEvent);
               break;
         }

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

   /* return now */
   return;
}


/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData()
{
   int         ilRC;
   int         ilCurSec;
   long          llFileLength;
   BC_HEAD      *prlBCHead      = NULL;
   CMDBLK      *prlCmdblk      = NULL;
   CTXBLK      *prlCtxblk      = NULL;
   char         *pclSelection    = NULL;
   char         *pclFields       = NULL;
   char         *pclDatablk      = NULL;

   char *pclFunc = "HandleData";

   /* only a message */
   dbg(DEBUG,"<HandleData> -------- START ---------");

   TWECheckPerformance(TRUE);

   /* set pointer */
   prlBCHead  = (BC_HEAD*)((char*)prgEvent + sizeof(EVENT));
   prlCmdblk  = (CMDBLK*)prlBCHead->data;

   /*FYA v1.3a UFIS-8302*/
   strcpy(pcgTwEnd, prlCmdblk->tw_end);

   if(checkAndsetHopo(pcgTwEnd, pcgHomeAP_sgstab) == RC_FAIL)
        return RC_FAIL;

  dbg(TRACE,"%s: TABEND = <%s>",pclFunc,pcgTABEnd);
  memset(pcgTwEnd,0x00,sizeof(pcgTwEnd));
  sprintf(pcgTwEnd,"%s,%s,%s",pcgHomeAP,pcgTABEnd,mod_name);
  dbg(TRACE,"%s : TW_END = <%s>",pclFunc,pcgTwEnd);

   /* is there a ctxblk? */
   if (strstr(prlBCHead->recv_name, "CTX") != NULL)
   {
      dbg(DEBUG,"<HandleData> found ctxblk...");
      /* yes, found ctxblk... */
      prlCtxblk  = (CTXBLK*)prlCmdblk->data;
      pclDatablk = (char*)prlCtxblk->data;

      /* calcuate length of file */
      llFileLength = prgEvent->data_length - sizeof(BC_HEAD) -
                     sizeof(CMDBLK) - sizeof(CTXBLK) - 5;
   }
   else
   {
      dbg(DEBUG,"<HandleData> cannot find ctxblk...");

      /* no, not such a block... */
      pclSelection = (char*)prlCmdblk->data;

      /* fields */
      pclFields = pclSelection + strlen(pclSelection) + 1;

      /* data */
      pclDatablk = pclFields + strlen(pclFields) + 1;

      /* calcuate length of file */
      llFileLength = prgEvent->data_length - sizeof(BC_HEAD) -
                     sizeof(CMDBLK) - strlen(pclSelection) -
                     strlen(pclFields) - 5;
   }

   dbg(DEBUG,"<HandleData> FileLength is: %ld", llFileLength);

   /* convert command to uppercase */
   StringUPR((UCHAR*)prlCmdblk->command);

   /* do we know this command */
   for (ilCurSec=0; ilCurSec<rgTM.iNoOfSections; ilCurSec++)
   {
      /* convert to uppercase */
      StringUPR((UCHAR*)rgTM.prSection[ilCurSec].pcCommand);

      /* compare commands */
      dbg(DEBUG,"<HandleData> compare command <%s> and <%s>",
               prlCmdblk->command, rgTM.prSection[ilCurSec].pcCommand);

    /*FYA v1.3a UFIS-8302*/
    if(rgTM.prSection[ilCurSec].pcHomeAirport == NULL || strlen(rgTM.prSection[ilCurSec].pcHomeAirport) == 0)
    {
        strcpy(rgTM.prSection[ilCurSec].pcHomeAirport, pcgHomeAP);
    }

      if (!strcmp(prlCmdblk->command, rgTM.prSection[ilCurSec].pcCommand))
      {
         /* found command!! */
         dbg(DEBUG,"<HandleData> found command <%s>", prlCmdblk->command);
         /* handle this command */
         if (strstr("GVA,ZRH", prlCmdblk->command) != NULL)
         {
            if ((ilRC = HandleSwissFlukoData(ilCurSec, pclDatablk,
                                             llFileLength))!= RC_SUCCESS)
            {
               dbg(TRACE,"<HandleData> HandleSwissFlukoData returns %d", ilRC);
            }
         }
         else if (strstr("BSLAI,BSLDI", prlCmdblk->command) != NULL)
         {
            if ((ilRC = HandleBaselFlukoData(ilCurSec, pclDatablk,prlCmdblk->command,
                                             llFileLength))!= RC_SUCCESS)
            {
               dbg(TRACE,"<HandleData> HandleBaselFlukoData returns %d", ilRC);
            }
         }
         else if (strcmp("WAWIM", prlCmdblk->command) == 0)
         {
            if ((ilRC = HandleWarsawFlukoData(ilCurSec, pclDatablk,
                                              llFileLength))!= RC_SUCCESS)
            {
               dbg(TRACE,"<HandleData> HandleSSIMFlukoData returns %d", ilRC);
            }
         }
         else if (strcmp("SSIM", prlCmdblk->command) == 0)
         {
            if ((ilRC = HandleSSIMFlukoData(ilCurSec, pclDatablk,
                                            llFileLength))!= RC_SUCCESS)
            {
               dbg(TRACE,"<HandleData> HandleSSIMFlukoData returns %d", ilRC);
            }
         }
         else if ( strcmp("PGN",prlCmdblk->command) == 0)
         {
            if((ilRC = HandlePrognoseData(ilCurSec,pclDatablk,
                                      llFileLength)) != RC_SUCCESS)
            {
               dbg(TRACE,"<HandleData> HandlePrognoseData returns %d", ilRC);
            }
         }
         else if ((ilRC = HandleCommand(ilCurSec, pclDatablk,
                                        llFileLength)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleData> HandleCommand(%d) returns %d",
                                                      ilCurSec, ilRC);
            Terminate(0);
         }
      }
   }

TWECheckPerformance(FALSE);

   dbg(DEBUG,"<HandleData> -------- END ---------");

   /* good bye */
   return ilRC;
}

/******************************************************************************/
/* The handle command routine                                                 */
/******************************************************************************/
static int HandleCommand(int ipSecNo, char *pcpData, long lpFileLength)
{
	int			i;
	int			j;
	int			ilPL;
	int			ilRC;
	int			ilCnt;
	int			ilInc;
	int			ilStart;
	int			ilAlloc;
	int			ilTabNo;
	int			iIsBlock;
	int			ilLineNo;
	int			ilCurRow;
	int			ilNoBlanks;
	int			ilCurField;
	int			ilLastLine;
	int			ilAnzFields;
	int			ilNoCurElems;
	int			ilFieldBufLength;
	int			ilOldDebugLevel;
	int			ilSysCnt;
	int			ilSyslibNo;
	int			ilTmpInt;
	int			ilErrorLog;
	int			ilDelLength;
	int			ilIgnore;
	int			ilCurIgnore;
	int			ilYear3;
	int			ilYear4;
	int			ilMonth3;
	int			ilMonth4;
	int			ilDay3;
	int			ilDay4;
	int			pilMemSteps[iTABLE_STEP];
	long			llLen;
	long			llPos;
	long			llClsPos;
	float			flTmpFloat;
	char			*pclS;
	char			*pclCfgPath;
	char			*pclTmpDataPtr;
	char			**pclDataArray;
	char			pclSyslibTable[iMIN];
	char			pclSyslibField[iMIN];
	char			pclSyslibData[iMAX_BUF_SIZE];
	char			pclCurrentTime[iMIN];
	char			pclErrFile[iMIN_BUF_SIZE];
	char			pclFields[iMAX_BUF_SIZE];
	char			pclDataBuf[iMAXIMUM];
	char			pclFieldBuf[iMAXIMUM];
	char 			pclWhereClauseSelection[iMAXIMUM];
	char			pclSyslibResultBuf[iMAXIMUM];
	char			pclSyslibTableFields[iMAXIMUM];
	char			pclSyslibFieldData[iMAXIMUM];
	char			pclCurrentDate[iMIN];
	char			pclMonth3[iMIN];
	char			pclMonth4[iMIN];
	char			pclDay3[iMIN];
	char			pclDay4[iMIN];
	EVENT			*prlOutEvent 	= NULL;
	BC_HEAD		*prlOutBCHead	= NULL;
	CMDBLK		*prlOutCmdblk	= NULL;
	FILE			*fh 				= NULL;
	FILE			*pMaqErrorFile = NULL;

	/* only for debug... */
	dbg(DEBUG,"<HandleCommand> ---------------- START----------------");

	/* initialize something */
	ilTabNo 		= iINITIAL;
	ilAlloc 		= iINITIAL;
	ilLastLine 	= iINITIAL;
	llPos			= 0;
	ilRC			= RC_SUCCESS;
	memset((void*)pclCurrentTime, 0x00, iMIN);
	memset((void*)pclDataBuf, 0x00, iMAXIMUM);
	memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
	for (i=0; i<iTABLE_STEP; i++)
		pilMemSteps[i] = 1;

	/* get timestamp */
	strcpy(pclCurrentTime, GetTimeStamp());

	/* get path for info file... */
	if ((pclCfgPath = getenv("TMP_PATH")) == NULL)
	{
		strcpy(pclErrFile, "./stxhdl.err");
	}
	else
	{
		/* clear buffer */
		memset((void*)pclErrFile, 0x00, iMIN_BUF_SIZE);

		/* copy path to buffer */
		strcpy(pclErrFile, pclCfgPath);

		/* check last character */
		if (pclErrFile[strlen(pclErrFile)-1] != '/')
			strcat(pclErrFile, "/");

		/* add filename to path */
		strcat(pclErrFile, "stxhdl.err");
	}

	/* open information file... */
	if ((fh = fopen(pclErrFile, "a")) == NULL)
		ilErrorLog = 0;
	else
		ilErrorLog = 1;

	/* delete last separator */
	i = strlen(rgTM.prSection[ipSecNo].pcEndOfLine);
	while (!memcmp(&pcpData[lpFileLength-i], rgTM.prSection[ipSecNo].pcEndOfLine, i))
	{
		lpFileLength -= i;
		pcpData[lpFileLength] = 0x00;
	}

	/* count lines of this file */
	if (strlen(pcpData) > 0)
	{  /*! char* pclHlp = pcpData;
	  ilLineNo = 0;
	   do
	   {
	      if ((
	        pclHlp = strpbrk(pclHlp, rgTM.prSection[ipSecNo].pcEndOfLine))
		 != NULL)
	      {
			pclHlp ++;
			ilLineNo ++;
	      }
	   } while ((pclHlp != NULL) && (ilLineNo < 101));
	   !*/
		if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
		{
			dbg(TRACE,"<HandleCommand> GetNoOfElements2(pcpData) returns: %d", ilLineNo);
			Terminate(0);
		}

	}
	else
		ilLineNo = 0;
	dbg(DEBUG, "<HandleCommand> eol : <%s>",
		rgTM.prSection[ipSecNo].pcEndOfLine);
	dbg(DEBUG,"<HandleCommand> found %d lines...", ilLineNo);
	rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows = ilLineNo;
	rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure = 0;
	rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows = 0;

	/* more the 0 lines? */
	if (ilLineNo)
	{
		/* so get memory for 128 Tables, this should be enough */
		dbg(DEBUG,"<HandleCommand> allocate %d bytes (pclDataArray)", iTABLE_STEP*sizeof(char*));
		if ((pclDataArray = (char**)malloc(iTABLE_STEP*sizeof(char*))) == NULL)
		{
			dbg(TRACE,"<HandleCommand> can't allocte memory for DataArray");
			pclDataArray = NULL;
			Terminate(0);
		}
	}
	else
		return RC_SUCCESS;

	/* dbg(DEBUG,"<HandleCommand> received data: <%s>", pcpData); */

	/* convert form ascii to ansi? */
	if (IsAscii(rgTM.prSection[ipSecNo].iCharacterSet))
	{
		dbg(DEBUG,"<HandleCommand> start calling AsciiToAnsi...");
		AsciiToAnsi((UCHAR*)pcpData);
		dbg(DEBUG,"<HandleCommand> end calling AsciiToAnsi...");
	}

	/* ------------ H A R D C O D E D  START ------------ */
	/* for ATHEN's daily fluko import (ask Berni, JBE)    */
	if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "ATHD"))
	{
		rgTM.prSection[ipSecNo].pcSystemKnz[0] = 0x00;
	}
	/* ------------ H A R D C O D E D  END ------------ */

	/* ------------ H A R D C O D E D  START ------------ */
	/* for LISBOA's LFZ import (ask Berni)    */
	if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "LFZL"))
	{
		pclTmpDataPtr = pcpData;
		DeleteCharacterBetween(pclTmpDataPtr, ';', ' ', '"');
		pclTmpDataPtr = pcpData;
		DeleteCharacterBetween(pclTmpDataPtr, ',', ' ', '"');
		pclTmpDataPtr = pcpData;
		DeleteCharacterBetween(pclTmpDataPtr, '\'', ' ', '"');
		dbg(DEBUG,"<HandleCommand> changed data: <%s>", pcpData);
	}
	/* ------------ H A R D C O D E D  END ------------ */

	/* set tmp pointer... */
	pclTmpDataPtr = pcpData;

	/* for all lines */
	for (ilCurRow=0; ilCurRow<ilLineNo; ilCurRow++)
	{
		if (!(ilCurRow%15))
			dbg(DEBUG,"<HandleCommand> working...");

		/* get n.th line */
		memset((void*)pclDataBuf, 0x00, iMAXIMUM);
        if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr,
                                            rgTM.prSection[ipSecNo].pcEndOfLine,
                                            pclDataBuf)) == NULL)
        {
          dbg(TRACE,"<HandleCommand> CopyNextField2 returns NULL");
          Terminate(0);
        }

		for (ilCurIgnore=0, ilIgnore=0; ilCurIgnore<rgTM.prSection[ipSecNo].iNoOfIgnoreStrings && ilIgnore == 0; ilCurIgnore++)
		{
			/* must we ignore this line */
			if (!strncmp(pclDataBuf, rgTM.prSection[ipSecNo].pcIgnoreStrings[ilCurIgnore], strlen(rgTM.prSection[ipSecNo].pcIgnoreStrings[ilCurIgnore])))
			{
				dbg(DEBUG,"<HandleCommand> Ignore: <%s>", pclDataBuf);
				ilIgnore = 1;
			}

			if (!strcmp(rgTM.prSection[ipSecNo].pcIgnoreStrings[ilCurIgnore], "LINE_FEED"))
			{
				if (pclDataBuf[0] == 0x0A)
				{
					dbg(DEBUG,"<HandleCommand> Ignore: <%s>", pclDataBuf);
					ilIgnore = 1;
				}
			}
		}

		if (ilIgnore)
			continue;

		if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iIgnoreRecordsShorterThan)
		{
			dbg(DEBUG,"<HandleCommand> Ignore: <%s>", pclDataBuf);
			continue;
		}

		/* is this header or trailer? */
		if (rgTM.prSection[ipSecNo].iHeader > 0)
		{
			if (ilCurRow < rgTM.prSection[ipSecNo].iHeader)
			{
				dbg(DEBUG,"<HandleCommand> ignore line: %d", ilCurRow);
				continue;
			}
		}
		if (rgTM.prSection[ipSecNo].iTrailer > 0)
		{
			if (ilCurRow > ilLineNo-rgTM.prSection[ipSecNo].iTrailer-1)
			{
				dbg(DEBUG,"<HandleCommand> ignore line: %d", ilCurRow);
				continue;
			}
		}

		/* first of all, check data length... */
		if (!strlen(pclDataBuf))
		{
			dbg(DEBUG,"<HandleCommand> found empty buffer...");
			rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows++;
		}
		else
		{
			/* search comment */
			if (!rgTM.prSection[ipSecNo].iUseComment || strncmp(pclDataBuf, rgTM.prSection[ipSecNo].pcCommentSign, strlen(rgTM.prSection[ipSecNo].pcCommentSign)))
			{
				/* is this a block? */
				for (i=0, iIsBlock=0;
					  i<rgTM.prSection[ipSecNo].iNoOfBlk && !iIsBlock;
					  i++)
				{
					if (IsBlock(pclDataBuf[0],
									rgTM.prSection[ipSecNo].pcSBlk[i],
									pclDataBuf[strlen(pclDataBuf)-1],
									rgTM.prSection[ipSecNo].pcEBlk[i])) iIsBlock = 1;
				}

				/* more then one table? */
				if (rgTM.prSection[ipSecNo].iNoOfTables > 1)
				{
					/*--------------------------------------------------------*/
					/*--------------------------------------------------------*/
					/* many tables */
					/*--------------------------------------------------------*/
					/*--------------------------------------------------------*/
					/* this is data or block */
					if (iIsBlock)
					{
						/* increment block counter */
						if (ilLastLine != iBLOCK &&
							 ilTabNo+1 < rgTM.prSection[ipSecNo].iNoOfTables)
						{
							/* next table, block = table!! */
							ilTabNo++;

							/* so, if we set next table set position to zero... */
							llPos = 0;

							/* block or data flag */
							ilLastLine = iBLOCK;

							/* now get memory for data of this table... */
							dbg(DEBUG,"<HandleCommand> allocate %ld bytes for pclDataArray[%d]", iMEMORY_STEP*sizeof(char), ilTabNo);
							if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP*sizeof(char))) == NULL)
							{
								dbg(TRACE,"<HandleCommand> can't allocate data memory for table: %d", ilTabNo);
								pclDataArray[ilTabNo] = NULL;
								Terminate(0);
							}

							/* clear this memory */
							memset((void*)pclDataArray[ilTabNo],
									 0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
						}

						/* continue... */
						continue;
					}
				}
				else
				{
					/*--------------------------------------------------------*/
					/*--------------------------------------------------------*/
					/* only one table */
					/*--------------------------------------------------------*/
					/*--------------------------------------------------------*/
					/* Block-Number is always 0 */
					ilTabNo = 0;

					/* get memory for data array */
					if (ilAlloc == iINITIAL)
					{
						/* set flag */
						ilAlloc = 1;

						/* now get memory for data of this table... */
						dbg(DEBUG,"<HandleCommand> allocate %ld bytes for pclDataArray[%d]", iMEMORY_STEP*sizeof(char), ilTabNo);
						if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP*sizeof(char))) == NULL)
						{
							dbg(TRACE,"<HandleCommand> can't allocate data memory for table: %d", ilTabNo);
							pclDataArray[ilTabNo] = NULL;
							Terminate(0);
						}

						/* clear this memory */
						memset((void*)pclDataArray[ilTabNo],
								 0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
					}

					/* this is data or block */
					if (iIsBlock)
					{
						/* yes, don't use it */
						continue;
					}
				}

				/* check length of data buffer, only for filetype = FIX... */
				if (IsFix(rgTM.prSection[ipSecNo].iFileType))
				{
					if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iSULength)
					{
						ilNoBlanks = rgTM.prSection[ipSecNo].iSULength - strlen(pclDataBuf);
						memset((void*)&pclDataBuf[strlen(pclDataBuf)], 0x20, ilNoBlanks);
						pclDataBuf[rgTM.prSection[ipSecNo].iSULength] = 0x00;
					}
				}
				else
				{
					/* check number of separator, only for filetype = dynamic */
					if (rgTM.prSection[ipSecNo].iFieldName == iUSE_FIELDNAME)
						ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields*2;
					else
						ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
					ilNoCurElems = GetNoOfElements2(pclDataBuf, rgTM.prSection[ipSecNo].pcDataSeparator);
					if (ilNoCurElems < ilAnzFields)
					{
						/* if number of elements in current record is smaller than
						number of fields, add separator at end of databock to get
						correct number
						*/
						ilAnzFields = ilAnzFields-ilNoCurElems;
						for (ilNoCurElems=0; ilNoCurElems<=ilAnzFields; ilNoCurElems++)
						{
							strcat(pclDataBuf, rgTM.prSection[ipSecNo].pcDataSeparator);
						}
					}
				}

				/* this is data */
				/* set flag */
				if (ilLastLine != iDATA)
					ilLastLine = iDATA;

				/* ------------ H A R D C O D E D  START ------------ */
				/* for ATHEN's daily fluko import (ask Berni, JBE)    */
				if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "ATHD") && !strlen(rgTM.prSection[ipSecNo].pcSystemKnz))
				{
					strncpy(rgTM.prSection[ipSecNo].pcSystemKnz, pclDataBuf, 127);
					rgTM.prSection[ipSecNo].iUseSystemKnz = 1;
					continue;
				}
				/* ------------ H A R D C O D E D  END   ------------ */

				/* is this file with or without fieldnames? */
				if (rgTM.prSection[ipSecNo].iFieldName == iUSE_FIELDNAME)
				{
					ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields*2;
					ilInc   		= 2;
					ilStart 		= 1;
				}
				else
				{
					ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
					ilInc   		= 1;
					ilStart 		= 0;
				}

				/* check file format, fix or dynamic with separator */
				if (IsFix(rgTM.prSection[ipSecNo].iFileType))
				{
					/* file is FIX-type */
					/* get data for all fields */
					pclS = pclDataBuf;

					for (ilCurField=ilStart,ilCnt=0;
						  ilCurField<ilAnzFields;
						  ilCurField+=ilInc,ilCnt++)
					{
						/* first and first+2... is always fieldname */
						if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
						{
							if (ilStart)
								pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField-1];
						}

						/* must we ignore this field?, in this case we continue... */
						if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "FILL_FIELD"))
						{
							/* set pointer to next valid position */
							pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
							/* delete last comma */
							if (ilCurField == ilAnzFields-1)
							{
								if (pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
								{
									pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
									--llPos;
								}
							}
						}
						else
						{
							/* get n.th field */
							memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
							if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
							{
								strncpy(pclFieldBuf, pclS, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
							}
							else
							{
								/* calculate length... */
								ilPL = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piEndDataPosition[ilCurField] - rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piStartDataPosition[ilCurField] + 1;
								strncpy(pclFieldBuf, pclS+rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piStartDataPosition[ilCurField], ilPL);
							}

							/* set flag... */
							ilRC = RC_SUCCESS;

							/* must we use syslib for this field?... */
							for (ilSyslibNo=0; ilSyslibNo<rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoSyslib; ilSyslibNo++)
							{
								if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcIField))
								{
									/* found syslib entry for current field */
									ilSysCnt=1;
									memset((void*)pclSyslibResultBuf, 0x00, iMAXIMUM);
									strcpy(pclSyslibTable, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName);
									strcpy(pclSyslibField, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField);
									strcpy(pclSyslibData, pclFieldBuf);

									ilOldDebugLevel = debug_level;
									debug_level = TRACE;
/* ------------------------------------------------------------------------ */
									if ( 1 == 2 )  /* 20020425 jim : NEVER !!! (!strcmp(rgTM.prSection[ipSecNo].pcTableExtension, "TAB")) */
									{
										/* neuer Zustand -> hier muss HOPO benutzt
											und die Daten angepasst werden
										*/
										sprintf(pclSyslibTableFields, "%s,HOPO", rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField);
										dbg(DEBUG,"<HandleCommand> syslibTabFields: <%s>", pclSyslibTableFields);
										sprintf(pclSyslibFieldData, "%s,%s", pclFieldBuf, rgTM.prSection[ipSecNo].pcHomeAirport);
										dbg(DEBUG,"<HandleCommand> syslibFieldData: <%s>", pclSyslibFieldData);
										ilRC = syslibSearchDbData(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName, pclSyslibTableFields, pclSyslibFieldData, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcResultField, pclSyslibResultBuf, &ilSysCnt, "");
									}
									else
									{
										/* alter Zustand, nicht aendert sich
										*/
										ilRC = syslibSearchDbData(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField, pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcResultField, pclSyslibResultBuf, &ilSysCnt, "");
									}
/* ------------------------------------------------------------------------ */
									debug_level = ilOldDebugLevel;

									if (ilRC == RC_FAIL)
									{
										dbg(TRACE,"<HandleCommand> syslibSearchDbData returns with RC_FAIL");
										Terminate(0);
									}
									else if (ilRC == RC_NOT_FOUND)
									{
										memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
									}
									else
									{
										memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
										strcpy(pclFieldBuf, pclSyslibResultBuf);
									}
								}
							}

							/* if we cannot found data in SHM.. */
							if (ilRC == RC_SUCCESS)
							{
								/* delete all blank at end of field... */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iDeleteEndBlanks)
								{
									while (pclFieldBuf[strlen(pclFieldBuf)-1] == ' ')
										pclFieldBuf[strlen(pclFieldBuf)-1] = 0x00;
								}

								/* delete all blanks at start if field... */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iDeleteStartBlanks)
								{
									while (pclFieldBuf[0] == ' ')
									{
										memmove((void*)&pclFieldBuf[0],
												  (const void*)&pclFieldBuf[1],
												  (size_t)strlen(&pclFieldBuf[1]));
										pclFieldBuf[strlen(pclFieldBuf)-1] = 0x00;
									}
								}

								/* enough memory to store data? */
								if (strlen(pclFieldBuf)+strlen(pclDataArray[ilTabNo]) >
									 iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
								{
									/* realloc here */
									pilMemSteps[ilTabNo]++;
									llClsPos = (long)strlen(pclDataArray[ilTabNo]);
									dbg(DEBUG,"<HandleCommand> realloc now %ld bytes...", pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));
									if ((pclDataArray[ilTabNo] = (char*)realloc(pclDataArray[ilTabNo], pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char))) == NULL)
									{
										dbg(TRACE,"<HandleCommand> cannot realloc for table %d", ilTabNo);
										Terminate(0);
									}

									/* clear new buffer */
									dbg(DEBUG,"<HandleCommand> clear at pos: %ld", llClsPos);
									memset((void*)&pclDataArray[ilTabNo][llClsPos], 0x00, iMEMORY_STEP*sizeof(char));
								}

								/* convert this field? UPPER/LOWER/ORIGINAL... */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] != 'O')
								{
									if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] == 'U')
									{
										StringUPR((UCHAR*)pclFieldBuf);
									}
									else if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] == 'L')
									{
										StringLWR((UCHAR*)pclFieldBuf);
									}
								}

#if 0
								/* convert this field? LEFT/RIGHT/ORIGINAL */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] != 'O')
								{
									if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] == 'L')
									{
										/* alignment is LEFT */
									}
									else if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] == 'R')
									{
										/* alignment is RIGHT */
									}
								}
#endif

								/* delete start characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldStartCharacter[ilCnt] != NULL)
								{
									/* delete left characters */
									str_trm_lft(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldStartCharacter[ilCnt]);
								}

								/* delete end characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldEndCharacter[ilCnt] != NULL)
								{
									/* delete right characters */
									str_trm_rgt(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldEndCharacter[ilCnt], TRUE);
								}

								/* delete all defined characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt] != NULL)
								{
									for (ilDelLength=0; ilDelLength<(int)strlen(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt]); ilDelLength++)
									{
										DeleteCharacterInString(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt][ilDelLength]);
									}
								}

				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ---------------- H A R D C O D E D -------------------------- */
				#if 1

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields ALC2 in combination with command SWISL or */
								/*  ALT for SWISSPORT */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "SWISL"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "ALC2"))
									{
										if(strlen(pclFieldBuf) != 2)
										   strcpy(pclFieldBuf,"  ");
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields ALC3 in combination with command SWISL or */
								/*  ALT for SWISSPORT */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "SWISL"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "ALC3"))
									{
										if(strlen(pclFieldBuf) != 3)
										   strcpy(pclFieldBuf,"   ");
									}
								}


								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields MTOW in combination with command LFZL */
								/* for LISSABON */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "LFZL"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MTOW"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										flTmpFloat /= 1000;
										sprintf(pclFieldBuf,"%10.5f", flTmpFloat);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields MTOW,MOWE in combination with command LFZ */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "LFZ"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MTOW"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										sprintf(pclFieldBuf,"%010.2f", flTmpFloat);
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "NOSE"))
									{
										sscanf(pclFieldBuf,"%d", &ilTmpInt);
										sprintf(pclFieldBuf,"%03d", ilTmpInt);
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MOWE"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										sprintf(pclFieldBuf,"%.2f", flTmpFloat);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields CDAT,DODM in combination with command STM */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "STM"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "CDAT"))
									{
										if (strlen(pclFieldBuf) == 8)
											strcat(pclFieldBuf, "000000");
										else
										{
											dbg(TRACE,"<HandleCommand> CDAT has wrong format, can't use");
											memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
										}
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "DODM"))
									{
										if (!strncmp(pclFieldBuf, "9999", 4))
											strcpy(pclFieldBuf, " ");
										else
										{
											if (strlen(pclFieldBuf) == 8)
												strcat(pclFieldBuf, "235959");
											else
											{
												dbg(TRACE,"<HandleCommand> DODM has wrong format, can't use");
												memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
											}
										}
									}
      								/*  fields TOHR in combination with command STM */
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "TOHR"))
									{
								      /* rkl 23.11. 99 ----------------------------------- */
								      /* ------------------------------------------------- */
								      /* ------------------------------------------------- */
								      /*  field TOHR in combination with command STM       */
								      /*  for HANNOVER (HAJ)                               */

									      /* Fieldvalue = 1 ...  */
									      if( strcmp(pclFieldBuf,"1") == 0)
                                          {
                                            /* ... then Value = N */
                                            pclFieldBuf[0] = 'N';
                                          }
                                          else
                                          { /* ... anything else = J */
                                            pclFieldBuf[0] = 'J';
                                          }
                                          /* dbg(DEBUG,"HAJ special : Field TOHR = <%s>",pclFieldBuf); */
								    }
                                }
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields VPFR in combination with command MAQ */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "MAQ"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "VPFR"))
									{
										strcpy(pclFieldBuf, pclCurrentTime);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields STOA,STOD and command ATH0 */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "ATH0"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "STOA") || !strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "STOD"))
									{
										memset((void*)pclCurrentDate, 0x00, iMIN);
										strncpy(pclCurrentDate, pclDataBuf, 8);
										strcat(pclCurrentDate, pclFieldBuf);
										strcpy(pclFieldBuf, ConvertToCEDATimeStamp("DD/MM/YYHHMI", pclCurrentDate));
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* the third and fourth field of FLUKO-Import, don't
									know the names */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "HFK"))
								{
									if (ilCnt == 3)
									{
										/* Year */
										ilYear3 = atoi(&pclFieldBuf[5]);
										if (ilYear3 < 90)
											ilYear3 += 2000;
										else
											ilYear3 += 1900;

										/* Month */
										pclMonth3[0] = 0x00;
										strncpy(pclMonth3, &pclFieldBuf[2], 3);
										pclMonth3[3] = 0x00;
										for (i=0, ilMonth3=-1; i<12 && ilMonth3==-1; i++)
											if (!strcmp(pclMonth3, pcgMONTH[i]))
												ilMonth3 = ++i;

										/* Day */
										pclDay3[0] = 0x00;
										strncpy(pclDay3, pclFieldBuf, 2);
										pclDay3[2] = 0x00;
										ilDay3 = atoi(pclDay3);

										/* copy to field (data) buffer */
										sprintf(pclFieldBuf, "%04d%02d%02d", ilYear3, ilMonth3, ilDay3);
									}
									else if (ilCnt == 4)
									{
										/* Month */
										pclMonth4[0] = 0x00;
										strncpy(pclMonth4, &pclFieldBuf[2], 3);
										pclMonth4[3] = 0x00;
										for (i=0, ilMonth4=-1; i<12 && ilMonth4==-1; i++)
											if (!strcmp(pclMonth4, pcgMONTH[i]))
												ilMonth4 = ++i;

										/* Day */
										pclDay4[0] = 0x00;
										strncpy(pclDay4, pclFieldBuf, 2);
										pclDay4[2] = 0x00;
										ilDay4 = atoi(pclDay4);

										/* Year */
										ilYear4 = ilYear3;
										if ((ilMonth3 > ilMonth4) || (ilMonth3 == ilMonth4 && ilDay3 > ilDay4))
											ilYear4++;

										/* copy to field (data) buffer */
										sprintf(pclFieldBuf, "%04d%02d%02d", ilYear4, ilMonth4, ilDay4);
									}
								}
				#endif
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------- H A R D C O D E D   E N D --------------------- */

								/* store n.th field */
								/* we don't like empty strings... */
								if (!strlen(pclFieldBuf))
									strcpy(pclFieldBuf, " ");

								ilFieldBufLength = strlen(pclFieldBuf);
								memcpy((void*)&pclDataArray[ilTabNo][llPos],
										 (const void*)pclFieldBuf, (size_t)ilFieldBufLength);
								llPos += (long)ilFieldBufLength;

								/* add separator at end of data */
								if (ilCurField < ilAnzFields-1)
								{
									memcpy((void*)&pclDataArray[ilTabNo][llPos],
											 (const void*)",", (size_t)1);
									llPos++;
								}

								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
								{
									/* set to next position */
									pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
								}
							}
							else
							{
								/* stop working with this entry */
								ilCurField = ilAnzFields;
								rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure++;

								if (ilErrorLog)
								{
									fprintf(fh,"<%s> COMMAND <%s> --- Error reading SHM: Table: <%s> Field: <%s> Data: <%s>\n", GetTimeStamp(), rgTM.prSection[ipSecNo].pcCommand, pclSyslibTable, pclSyslibField, pclSyslibData);
								}

								/* ------------ H A R D C O D E D  START ------------ */
								/* for MAQ-Import, ask GSA  */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "MAQ"))
								{
									if (pMaqErrorFile == NULL)
									{
										rgTM.prSection[ipSecNo].pcSystemKnz[0] = 0x00;
										sprintf(rgTM.prSection[ipSecNo].pcSystemKnz, "%s/maq.error.%s", getenv("TMP_PATH"), GetTimeStamp());
										dbg(DEBUG,"<HandleCommand> creating filename: <%s>", rgTM.prSection[ipSecNo].pcSystemKnz);
										if ((pMaqErrorFile = fopen(rgTM.prSection[ipSecNo].pcSystemKnz, "w")) == NULL)
										{
											dbg(TRACE,"<HandleCommand> error opening file: <%s>", rgTM.prSection[ipSecNo].pcSystemKnz);
										}
										else
										{
											fprintf(pMaqErrorFile, "%s\n", pclDataBuf);
										}

									}
									else
									{
										fprintf(pMaqErrorFile, "%s\n", pclDataBuf);
									}
								}
								/* ------------ H A R D C O D E D  END ------------ */
							}
						}
					}

					if (ilRC != RC_NOT_FOUND)
					{
						/* add '\n' as row separator */
						memcpy((void*)&pclDataArray[ilTabNo][llPos],
								 (const void*)"\n", (size_t)1);
						llPos++;
					}
				}
				else
				{
					/* file is dynamic */
					for (ilCurField=ilStart,ilCnt=0;
						  ilCurField<ilAnzFields;
						  ilCurField+=ilInc,ilCnt++)
					{
						/* must we ignore this field?, in this case we continue... */
						if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "FILL_FIELD"))
						{
							/* delete last comma */
							if (ilCurField == ilAnzFields-1)
							{
								if (pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
								{
									pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
									--llPos;
								}
							}
						}
						else
						{
							/* get data for all fields */
							memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
							if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataPosition > 0)
							{
								if ((ilRC = CopyDataField2(pclDataBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piStartDataPosition[ilCurField], rgTM.prSection[ipSecNo].pcDataSeparator, pclFieldBuf)) != 0)
								{
									dbg(TRACE,"<HandleCommand> %05d CopyDataField2 returns: %d", __LINE__, ilRC);
									Terminate(0);
								}
							}
							else
							{
								if ((ilRC = CopyDataField2(pclDataBuf, ilCurField, rgTM.prSection[ipSecNo].pcDataSeparator, pclFieldBuf)) != 0)
								{
									dbg(TRACE,"<HandleCommand> %05d CopyDataField2 returns: %d", __LINE__, ilRC);
									Terminate(0);
								}
							}

							/* set flag... */
							ilRC = RC_SUCCESS;

							/* must we use syslib for this field?... */
							for (ilSyslibNo=0; ilSyslibNo<rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoSyslib; ilSyslibNo++)
							{
								if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcIField))
								{
									/* found syslib entry for current field */
									ilSysCnt=1;
									memset((void*)pclSyslibResultBuf, 0x00, iMAXIMUM);
									strcpy(pclSyslibTable, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName);
									strcpy(pclSyslibField, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField);
									strcpy(pclSyslibData, pclFieldBuf);

									ilOldDebugLevel = debug_level;
									debug_level = TRACE;
/* ------------------------------------------------------------------------ */
									if ( 1 == 2 )  /* 20020425 jim : NEVER !!! (!strcmp(rgTM.prSection[ipSecNo].pcTableExtension, "TAB")) */
									{
										/* neuer Zustand -> hier muss HOPO benutzt
											und die Daten angepasst werden
										*/
										sprintf(pclSyslibTableFields, "%s,HOPO", rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField);
										dbg(DEBUG,"<HandleCommand> syslibTabFields: <%s>", pclSyslibTableFields);
										sprintf(pclSyslibFieldData, "%s,%s", pclFieldBuf, rgTM.prSection[ipSecNo].pcHomeAirport);
										dbg(DEBUG,"<HandleCommand> syslibFieldData: <%s>", pclSyslibFieldData);
										ilRC = syslibSearchDbData(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName, pclSyslibTableFields, pclSyslibFieldData, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcResultField, pclSyslibResultBuf, &ilSysCnt, "");
									}
									else
									{
										/* alter Zustand, nicht aendert sich
										*/
										ilRC = syslibSearchDbData(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabName, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcTabField, pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].prSyslib[ilSyslibNo].pcResultField, pclSyslibResultBuf, &ilSysCnt, "");
									}
/* ------------------------------------------------------------------------ */
									debug_level = ilOldDebugLevel;

									if (ilRC == RC_FAIL)
									{
										dbg(TRACE,"<HandleCommand> syslibSearchDbData returns with RC_FAIL");
										Terminate(0);
									}
									else if (ilRC == RC_NOT_FOUND)
									{
										memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
									}
									else
									{
										memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
										strcpy(pclFieldBuf, pclSyslibResultBuf);
									}
								}
							}

							/* if we cannot found data in SHM.. */
							if (ilRC == RC_SUCCESS)
							{
								/* enough memory to store data? */
								if (strlen(pclFieldBuf)+strlen(pclDataArray[ilTabNo]) >
									 iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
								{
									/* realloc here */
									pilMemSteps[ilTabNo]++;
									llClsPos = (long)strlen(pclDataArray[ilTabNo]);
									if ((pclDataArray[ilTabNo] = (char*)realloc(pclDataArray[ilTabNo], pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char))) == NULL)
									{
										dbg(TRACE,"<HandleCommand> cannot realloc for table %d", ilTabNo);
										Terminate(0);
									}

									/* clear new buffer */
									memset((void*)&pclDataArray[ilTabNo][llClsPos], 0x00, iMEMORY_STEP*sizeof(char));
								}

								/* convert this field? UPPER/LOWER/ORIGINAL... */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] != 'O')
								{
									if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] == 'U')
									{
										StringUPR((UCHAR*)pclFieldBuf);
									}
									else if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcULOSign[ilCnt] == 'L')
									{
										StringLWR((UCHAR*)pclFieldBuf);
									}
								}

#if 0
								/* convert this field? LEFT/RIGHT/ORIGINAL */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] != 'O')
								{
									if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] == 'L')
									{
										/* alignment is LEFT */
									}
									else if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcAlignment[ilCnt] == 'R')
									{
										/* alignment is RIGHT */
									}
								}
#endif

								/* delete start characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldStartCharacter[ilCnt] != NULL)
								{
									/* delete left characters */
									str_trm_lft(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldStartCharacter[ilCnt]);
								}

								/* delete end characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldEndCharacter[ilCnt] != NULL)
								{
									/* delete right characters */
									str_trm_rgt(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldEndCharacter[ilCnt], TRUE);
								}

								/* delete all defined characters of this field?! */
								if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt] != NULL)
								{
									for (ilDelLength=0; ilDelLength<(int)strlen(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt]); ilDelLength++)
									{
										DeleteCharacterInString(pclFieldBuf, rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcDeleteFieldCharacter[ilCnt][ilDelLength]);
									}
								}

				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ---------------- H A R D C O D E D -------------------------- */
				#if 1
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields MTOW in combination with command LFZL */
								/* for LISSABON */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "LFZL"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MTOW"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										flTmpFloat /= 1000;
										sprintf(pclFieldBuf,"%10.5f", flTmpFloat);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields MTOW,MOWE in combination with command LFZ */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "LFZ"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MTOW"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										sprintf(pclFieldBuf,"%010.2f", flTmpFloat);
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "NOSE"))
									{
										sscanf(pclFieldBuf,"%d", &ilTmpInt);
										sprintf(pclFieldBuf,"%03d", ilTmpInt);
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "MOWE"))
									{
										sscanf(pclFieldBuf,"%f", &flTmpFloat);
										sprintf(pclFieldBuf,"%.2f", flTmpFloat);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields CDAT,DODM in combination with command STM */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "STM"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "CDAT"))
									{
										if (strlen(pclFieldBuf) == 8)
											strcat(pclFieldBuf, "000000");
										else
										{
											dbg(TRACE,"<HandleCommand> CDAT has wrong format, can't use");
											memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
										}
									}
									else if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "DODM"))
									{
										if (!strncmp(pclFieldBuf, "9999", 4))
											strcpy(pclFieldBuf, " ");
										else
										{
											if (strlen(pclFieldBuf) == 8)
												strcat(pclFieldBuf, "235959");
											else
											{
												dbg(TRACE,"<HandleCommand> DODM has wrong format, can't use");
												memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
											}
										}
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/*  fields VPFR in combination with command MAQ */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "MAQ"))
								{
									if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt], "VPFR"))
									{
										strcpy(pclFieldBuf, pclCurrentTime);
									}
								}

								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* ------------------------------------------------- */
								/* the third and fourth field of FLUKO-Import, don't
									know the names */
								else if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "HFK"))
								{
									if (ilCnt == 3)
									{
										/* Year */
										ilYear3 = atoi(&pclFieldBuf[5]);
										if (ilYear3 < 90)
											ilYear3 += 2000;
										else
											ilYear3 += 1900;

										/* Month */
										pclMonth3[0] = 0x00;
										strncpy(pclMonth3, &pclFieldBuf[2], 3);
										pclMonth3[3] = 0x00;
										for (i=0, ilMonth3=-1; i<12 && ilMonth3==-1; i++)
											if (!strcmp(pclMonth3, pcgMONTH[i]))
												ilMonth3 = ++i;

										/* Day */
										pclDay3[0] = 0x00;
										strncpy(pclDay3, pclFieldBuf, 2);
										pclDay3[2] = 0x00;
										ilDay3 = atoi(pclDay3);

										/* copy to field (data) buffer */
										sprintf(pclFieldBuf, "%04d%02d%02d", ilYear3, ilMonth3, ilDay3);
									}
									else if (ilCnt == 4)
									{
										/* Month */
										pclMonth4[0] = 0x00;
										strncpy(pclMonth4, &pclFieldBuf[2], 3);
										pclMonth4[3] = 0x00;
										for (i=0, ilMonth4=-1; i<12 && ilMonth4==-1; i++)
											if (!strcmp(pclMonth4, pcgMONTH[i]))
												ilMonth4 = ++i;

										/* Day */
										pclDay4[0] = 0x00;
										strncpy(pclDay4, pclFieldBuf, 2);
										pclDay4[2] = 0x00;
										ilDay4 = atoi(pclDay4);

										/* Year */
										ilYear4 = ilYear3;
										if ((ilMonth3 > ilMonth4) || (ilMonth3 == ilMonth4 && ilDay3 > ilDay4))
											ilYear4++;

										/* copy to field (data) buffer */
										sprintf(pclFieldBuf, "%04d%02d%02d", ilYear4, ilMonth4, ilDay4);
									}
								}
				#endif
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------------------------------------------------------- */
				/* ------------- H A R D C O D E D   E N D --------------------- */

								/* we don't like empty strings... */
								if (!strlen(pclFieldBuf))
									strcpy(pclFieldBuf, " ");

								ilFieldBufLength = strlen(pclFieldBuf);
								memcpy((void*)&pclDataArray[ilTabNo][llPos],
									 (const void*)pclFieldBuf, (size_t)ilFieldBufLength);
								llPos += (long)ilFieldBufLength;

								/* add separator at end of data */
								if (ilCurField < ilAnzFields-1)
								{
									memcpy((void*)&pclDataArray[ilTabNo][llPos],
											 (const void*)",", (size_t)1);
									llPos++;
								}
							}
							else
							{
								/* stop working with this entry */
								ilCurField = ilAnzFields;
								rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure++;

								if (ilErrorLog)
								{
									fprintf(fh,"<%s> COMMAND <%s> --- Error reading SHM: Table: <%s> Field: <%s> Data: <%s>\n", GetTimeStamp(), rgTM.prSection[ipSecNo].pcCommand, pclSyslibTable, pclSyslibField, pclSyslibData);
								}

								/* ------------ H A R D C O D E D  START ------------ */
								/* for MAQ-Import, ask GSA  */
								if (!strcmp(rgTM.prSection[ipSecNo].pcCommand, "MAQ"))
								{
									if (pMaqErrorFile == NULL)
									{
										rgTM.prSection[ipSecNo].pcSystemKnz[0] = 0x00;
										sprintf(rgTM.prSection[ipSecNo].pcSystemKnz, "%s/maq.error.%s", getenv("TMP_PATH"), GetTimeStamp());
										dbg(DEBUG,"<HandleCommand> creating filename: <%s>", rgTM.prSection[ipSecNo].pcSystemKnz);
										if ((pMaqErrorFile = fopen(rgTM.prSection[ipSecNo].pcSystemKnz, "w")) == NULL)
										{
											dbg(TRACE,"<HandleCommand> error opening file: <%s>", rgTM.prSection[ipSecNo].pcSystemKnz);
										}
										else
										{
											rgTM.prSection[ipSecNo].iUseSystemKnz = 1;
											fprintf(pMaqErrorFile, "%s\n", pclDataBuf);
										}

									}
									else
									{
										fprintf(pMaqErrorFile, "%s\n", pclDataBuf);
									}
								}
								/* ------------ H A R D C O D E D  END ------------ */
							}
						}
					}

					if (ilRC != RC_NOT_FOUND)
					{
						/* add '\n' as row separator */
						memcpy((void*)&pclDataArray[ilTabNo][llPos],
								 (const void*)"\n", (size_t)1);
						llPos++;
					}
				}
			}
		}
	}

	if (ilErrorLog)
	{
		fclose(fh);
		fh = NULL;
	}

	if (pMaqErrorFile != NULL)
	{
		fclose(pMaqErrorFile);
		pMaqErrorFile = NULL;
	}

	dbg(DEBUG,"--------------------------------------------------");
	dbg(DEBUG,"--------------------------------------------------");
	dbg(DEBUG,"--------------------------------------------------");
	dbg(DEBUG,"<HandleCommand> Number of lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows);
	dbg(DEBUG,"<HandleCommand> Number of SHM-Failure: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure);
	dbg(DEBUG,"<HandleCommand> Number of empty lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows);
	dbg(DEBUG,"--------------------------------------------------");
	dbg(DEBUG,"--------------------------------------------------");
	dbg(DEBUG,"--------------------------------------------------");
	/* write it to database, for all used tables */
	for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
	{
		/* delete last \n at end of data... */
		if (pclDataArray[i][strlen(pclDataArray[i])-1] == 0x0A)
			pclDataArray[i][strlen(pclDataArray[i])-1] = 0x00;

		/* build field-list with comma separator... */
		memset((void*)pclFields, 0x00, iMAX_BUF_SIZE);
		for (j=0; j<rgTM.prSection[ipSecNo].prTabDef[i].iNoOfFields; j++)
		{
			/* don't use FILL_FIELDS... */
			if (strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j], "FILL_FIELD"))
			{
				strcat(pclFields, rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j]);
				strcat(pclFields, ",");
			}
		}

		/* set end of fieldlist... */
		while (pclFields[strlen(pclFields)-1] == ',')
			pclFields[strlen(pclFields)-1] = 0x00;
		dbg(DEBUG,"<HandleCommand> FieldList: <%s>", pclFields);

/* gle: call either GLEHandleWhereClause or proceed as usual starts here */

           /* check for where_fields */
           if (rgTM.prSection[ipSecNo].prTabDef[i].iNoOfWhereFields > 0)
           {
              memset((void*)pclWhereClauseSelection, 0x00, iMAXIMUM);
              *pclWhereClauseSelection = '\0';
              /* dbg(DEBUG, "gle: pclDataArray[i] : <%s>", pclDataArray[i]);*/
              ilRC = GLEHandleWhereClause
                        (
			 rgTM.prSection[ipSecNo],
                         rgTM.prSection[ipSecNo].prTabDef[i],
                         pclDataArray[i], pclWhereClauseSelection
                        );
           } /* end if where_fields entered */

	   else
	   { /* gle: ... else proceed as usual with original code */


		/* calculate size of memory we need */
		if (rgTM.prSection[ipSecNo].iUseSystemKnz)
			llLen = strlen(rgTM.prSection[ipSecNo].pcSystemKnz);
		else
			llLen = 0;

		llLen += sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) +
				  strlen(pclFields) + strlen(pclDataArray[i]) + 3;

		/* get memory for out event */
		if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
		{
			dbg(TRACE,"<HandleCommand> not enough memory for OutEvent!!");
			prlOutEvent = NULL;
			Terminate(0);
		}

		/* clear memory */
		memset((void*)prlOutEvent, 0x00, llLen);

		/* set structure members */
		prlOutEvent->type				= SYS_EVENT;
		prlOutEvent->command			= EVENT_DATA;
		prlOutEvent->originator		= mod_id;
		prlOutEvent->retry_count	= 0;
		prlOutEvent->data_offset	= sizeof(EVENT);
		prlOutEvent->data_length	= llLen - sizeof(EVENT);

		/* BC-Head Structure */
		prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
		prlOutBCHead->rc = RC_SUCCESS;
		strcpy(prlOutBCHead->dest_name, rgTM.prSection[ipSecNo].prTabDef[i].pcDestName);
		strcpy(prlOutBCHead->recv_name, rgTM.prSection[ipSecNo].prTabDef[i].pcRecvName);

		/* command block structure */
		prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
		strcpy(prlOutCmdblk->command, rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
		strcpy(prlOutCmdblk->obj_name, rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);

		sprintf(prlOutCmdblk->tw_end, "%s,%s,STXHDL%1d",
		rgTM.prSection[ipSecNo].pcHomeAirport,
		rgTM.prSection[ipSecNo].pcTableExtension,
		rgTM.prSection[ipSecNo].prTabDef[i].iHopoInFieldlist);

		dbg(DEBUG,"<HandleCommand> Command: <%s>", rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
		dbg(DEBUG,"<HandleCommand> TableName: <%s>", rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);

		/* selection */
		/* we write system sign to selection (for GSA) */
		if (rgTM.prSection[ipSecNo].iUseSystemKnz)
		{
			strcpy(prlOutCmdblk->data, rgTM.prSection[ipSecNo].pcSystemKnz);
			dbg(DEBUG,"<HandleCommand> Selection: <%s>", rgTM.prSection[ipSecNo].pcSystemKnz);
		  /* we only accept SQLHDL special selection if the command WRD is supplied inside the section*/
			if (strstr(rgTM.prSection[ipSecNo].pcSystemKnz,"#FC")!=NULL && strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd,"WRD")!=0)
			{
			 dbg(DEBUG,"<HandleCommand> Selection looks WRD-SQLHDL selection but Cmd <%s> is NOT <WRD>! Cleaning Selection!"
				,rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
			 rgTM.prSection[ipSecNo].pcSystemKnz[0]=0x00;
			}
		}

		/* fields */
		if (rgTM.prSection[ipSecNo].iUseSystemKnz)
		{
			strcpy((prlOutCmdblk->data+
					  strlen(rgTM.prSection[ipSecNo].pcSystemKnz)+1), pclFields);
		}
		else
			strcpy((prlOutCmdblk->data+1), pclFields);

		/* data */
		if (rgTM.prSection[ipSecNo].iUseSystemKnz)
		{
			strcpy((prlOutCmdblk->data+
					strlen(rgTM.prSection[ipSecNo].pcSystemKnz) +
					strlen(pclFields)+2), pclDataArray[i]);
		}
		else
			strcpy((prlOutCmdblk->data+strlen(pclFields)+2), pclDataArray[i]);
		dbg(DEBUG,"<HandleCommand> Data: <%s>", pclDataArray[i]);

		/* send message */
		if (rgTM.prSection[ipSecNo].prTabDef[i].iModID > 0)
		{
			dbg(TRACE,"<HandleCommand> 1. sending to route %d",
							rgTM.prSection[ipSecNo].prTabDef[i].iModID);
			if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].prTabDef[i].iModID,
						 mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
			{
				dbg(TRACE,"<HandleCommand> QuePut returns with: %d", ilRC);
				Terminate(0);
			}
			dbg(TRACE,"<HandleCommand> 1. QuePut returns with: %d", ilRC);
		}
		else
		{
			dbg(TRACE,"<HandleCommand> 2. sending to route %d", igRouterID);
			if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4,
								 llLen, (char*)prlOutEvent)) != RC_SUCCESS)
			{
				dbg(TRACE,"<HandleCommand> QuePut returns with: %d", ilRC);
				Terminate(0);
			}
			dbg(TRACE,"<HandleCommand> 2. QuePut returns with: %d", ilRC);
		}

		/* free memory... */
		if (prlOutEvent != NULL)
		{
			free((void*)prlOutEvent);
			prlOutEvent = NULL;
		}

	   } /* gle: end else if where_fields entered */
/* gle: call either GLEHandleWhereClause or proceed as usual ends here */

	}

	/* delete memory we doesn't need */
	for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
	{
		dbg(DEBUG,"<HandleCommand> delete pclDataArray[%d]", i);
		free((void*)pclDataArray[i]);
	}
	dbg(DEBUG,"<HandleCommand> delete pclDataArray");
	free((void*)pclDataArray);

	/* only for debug... */
	dbg(DEBUG,"<HandleCommand> ---------------- END ----------------");

	/* bye bye */
	return RC_SUCCESS;
}


/******************************************************************************/
/* The handle command routine  only for fix datalengths (Command TST)         */
/* Hard Codings from HandleCommand are not implemented !!!!!                  */
/******************************************************************************/
static int HandleSwissFlukoData(int ipSecNo, char *pcpData,
                                      long lpFileLength)
{
int         i;
int         j;
int         ilPL;
int         ilRC;
int         ilCnt;
int         ilInc;
int         ilStart;
int         ilAlloc;
int         ilTabNo;
int         iIsBlock;
int         ilLineNo;
int         ilCurRow;
int         ilNoBlanks;
int         ilCurField;
int         ilAnzFields;
int         ilNoCurElems;
int         ilFieldBufLength;
int         ilOldDebugLevel;
int         ilSysCnt;
int         ilSyslibNo;
int         ilTmpInt;
int         ilDelLength;
int         ilIgnore;
int         ilCurIgnore;
int         ilYear3;
int         ilYear4;
int         ilMonth3;
int         ilMonth4;
int         ilDay3;
int         ilDay4;
int         pilMemSteps[iTABLE_STEP];
long         llLen;
long         llPos;
long         llClsPos;
float         flTmpFloat;
char         *pclS;
char         *pclCfgPath;
char         *pclTmpDataPtr;
char         **pclDataArray;
char         pclSyslibTable[iMIN];
char         pclSyslibField[iMIN];
char         pclSyslibData[iMAX_BUF_SIZE];
char         pclCurrentTime[iMIN];
char         pclFields[iMAX_BUF_SIZE];
char         pclDataBuf[iMAXIMUM];
char         pclFieldBuf[iMAXIMUM];
char         pclSyslibResultBuf[iMAXIMUM];
char         pclSyslibTableFields[iMAXIMUM];
char         pclSyslibFieldData[iMAXIMUM];
char         pclCurrentDate[iMIN];
char         pclMonth3[iMIN];
char         pclMonth4[iMIN];
char         pclDay3[iMIN];
char         pclDay4[iMIN];
char         pclAlc3FldDat[5];
long         llDataLength = 0;
EVENT        *prlOutEvent    = NULL;
BC_HEAD      *prlOutBCHead   = NULL;
CMDBLK       *prlOutCmdblk   = NULL;

   dbg(TRACE,"<HandleSwissFlukoData> -------- START -------------");

   /* initialize something */
   ilTabNo       = iINITIAL;
   ilAlloc       = iINITIAL;
   llPos         = 0;
   ilRC          = RC_SUCCESS;
   llDataLength  = 0;
   memset((void*)pclCurrentTime, 0x00, iMIN);
   memset((void*)pclDataBuf, 0x00, iMAXIMUM);
   memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
   for (i=0; i<iTABLE_STEP; i++)
      pilMemSteps[i] = 1;

   /* get timestamp */
   strcpy(pclCurrentTime, GetTimeStamp());

   /* delete last separator */
   i = strlen(rgTM.prSection[ipSecNo].pcEndOfLine);
   while (!memcmp(&pcpData[lpFileLength-i], rgTM.prSection[ipSecNo].pcEndOfLine, i))
   {
      lpFileLength -= i;
      pcpData[lpFileLength] = 0x00;
   }

   /* count lines of this file */
   if (strlen(pcpData) > 0)
   {
      if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
      {
         dbg(TRACE,"<HandleSwissFlukoData> GetNoOfElements2(pcpData) returns: %d", ilLineNo);
         Terminate(0);
      }
   }
   else
   {
      ilLineNo = 0;
   }

   dbg(TRACE, "FOUND %d LINES", ilLineNo);
   rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows = ilLineNo;
   rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure = 0;
   rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows = 0;

   /* more than 0 lines? */
   if (ilLineNo)
   {
      /* so get memory for 128 Tables, this should be enough */
      if ((pclDataArray = (char**)malloc(iTABLE_STEP*sizeof(char*))) == NULL)
      {
         dbg(TRACE,"<HandleSwissFlukoData> can't allocte memory for DataArray");
         pclDataArray = NULL;
         Terminate(0);
      }
   }
   else
      return RC_SUCCESS;

   /* convert form ascii to ansi? */
   if (IsAscii(rgTM.prSection[ipSecNo].iCharacterSet))
   {
      dbg(TRACE,"<HandleSwissFlukoData> start calling AsciiToAnsi...");
      AsciiToAnsi((UCHAR*)pcpData);
      dbg(TRACE,"<HandleSwissFlukoData> end calling AsciiToAnsi...");
   }

   /* set tmp pointer... */
   pclTmpDataPtr = pcpData;

   /* for all lines */
   for (ilCurRow=0; ilCurRow<ilLineNo; ilCurRow++)
   {
      if (!(ilCurRow%15))
      {
         if (ilCurRow > 0)
         {
            dbg(TRACE,"<HandleSwissFlukoData> working till line %d...",ilCurRow);
         }
      }

      /* get n.th line */
      memset((void*)pclDataBuf, 0x00, iMAXIMUM);
      if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr,
                                          rgTM.prSection[ipSecNo].pcEndOfLine,
                                          pclDataBuf)) == NULL)
      {
         dbg(TRACE,"<HandleSwissFlukoData> CopyNextField2 returns NULL");
         Terminate(0);
      }

      /* first of all, check data length, header, trailer ... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iIgnoreRecordsShorterThan)
      {
         dbg(TRACE,"<HandleSwissFlukoData> Ignore: <%s> (too short)", pclDataBuf);
         continue;
      }

      /* is this header or trailer? */
      if (rgTM.prSection[ipSecNo].iHeader > 0)
      {
         if (ilCurRow < rgTM.prSection[ipSecNo].iHeader)
         {
       	    dbg(DEBUG,"<HandleSwissFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (rgTM.prSection[ipSecNo].iTrailer > 0)
      {
         if (ilCurRow > ilLineNo-rgTM.prSection[ipSecNo].iTrailer-1)
         {
	          dbg(DEBUG,"<HandleSwissFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (!strlen(pclDataBuf))
      {
         dbg(DEBUG,"<HandleSwissFlukoData> found empty buffer...");
         rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows++;
         continue;
      }

      /* Block-Number is always 0 */
      ilTabNo = 0;
      /* get memory for data array */
      if (ilAlloc == iINITIAL)
      {
         /* set flag */
         ilAlloc = 1;
         /* now get memory for data of this table... */
         dbg(TRACE,"ALLOCATE %ld bytes for pclDataArray[%d]",
                    iMEMORY_STEP*sizeof(char), ilTabNo);
         if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP*
                                                sizeof(char))) == NULL)
         {
            dbg(TRACE,"Can't allocate data memory for table: %d",
                       ilTabNo);
            pclDataArray[ilTabNo] = NULL;
            Terminate(0);
         }
         /* clear this memory */
         memset((void*)pclDataArray[ilTabNo],
                0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
      }

      /* check length of data buffer, only for filetype = FIX... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iSULength)
      {
         ilNoBlanks= rgTM.prSection[ipSecNo].iSULength-strlen(pclDataBuf);
         memset((void*)&pclDataBuf[strlen(pclDataBuf)], 0x20, ilNoBlanks);
         pclDataBuf[rgTM.prSection[ipSecNo].iSULength] = 0x00;
      }

      /* is this file with or without fieldnames? */
      ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
      ilInc         = 1;
      ilStart       = 0;

      /* file is FIX-type */
      /* get data for all fields */
      pclS = pclDataBuf;
      for (ilCurField=ilStart,ilCnt=0;
           ilCurField<ilAnzFields;
           ilCurField+=ilInc,ilCnt++)
      {
         /* first and first+2... is always fieldname */
         if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
         {
            if (ilStart)
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField-1];
         }

         /* must we ignore this field?, in this case we continue... */
         if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                     "FILL_FIELD"))
         {
            /* set pointer to next valid position */
            pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
            /* delete last comma */
            if (ilCurField == ilAnzFields-1)
            {
               if(pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
               {
                  pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
                  --llPos;
               }
            }
         }
         else
         {
            /* get n.th field */
            memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               strncpy(pclFieldBuf, pclS,
                       rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
            }
            /* store n.th field */
            /* we don't like empty strings... */
            if (!strlen(pclFieldBuf))
               strcpy(pclFieldBuf, " ");

            if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                "ALC?", 4))
            {
               Get3LCAirline(ipSecNo, pclFieldBuf, pclAlc3FldDat);
               if (pclAlc3FldDat[0] != '\0')
               {
                  strcpy(pclFieldBuf, pclAlc3FldDat);
               }
            }

            ilFieldBufLength = strlen(pclFieldBuf);
            llDataLength += (long)ilFieldBufLength+1;  /* seperator also */

            /* enough memory to store data? */
            if (llDataLength >=
                iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
            {
               /* realloc here */
               pilMemSteps[ilTabNo]++;
               dbg(TRACE,"REALLOC now %ld bytes...",
                    pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));

               if ((pclDataArray[ilTabNo] =
                   (char*)realloc(pclDataArray[ilTabNo],
                   pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char)))
                     == NULL)
               {
                  dbg(TRACE,"Cannot realloc for table %d", ilTabNo);
                  Terminate(0);
               }

               /* clear new buffer */
               llClsPos = llDataLength - (long)ilFieldBufLength;
               dbg(DEBUG,"clear from pos: %ld", llClsPos);
               memset((void*)&pclDataArray[ilTabNo][llClsPos],
                       0x00, iMEMORY_STEP*sizeof(char));
            }

            /* copy data to dataline (enough allocated) */
            memcpy((void*)&pclDataArray[ilTabNo][llPos],
                   (const void*)pclFieldBuf, (size_t)ilFieldBufLength);
            llPos += (long)ilFieldBufLength;

            /* add separator at end of data */
            if (ilCurField < ilAnzFields-1)
            {
               memcpy((void*)&pclDataArray[ilTabNo][llPos],
                      (const void*)",", (size_t)1);
               llPos++;
            }

            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               /* set to next position */
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
            }

         }  /* no FILL_FIELD */

      }  /* for all fields */

      /* add '\n' as row separator */
      memcpy((void*)&pclDataArray[ilTabNo][llPos],
             (const void*)"\n", (size_t)1);
      llPos++;

   }  /* for all rows */

   dbg(TRACE, "STEP THROUGH LINES ENDS HERE");

   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"<HandleSwissFlukoData> Number of lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows);
   dbg(TRACE,"<HandleSwissFlukoData> Number of SHM-Failure: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure);
   dbg(TRACE,"<HandleSwissFlukoData> Number of empty lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows);
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");

   /* write it to database, for all used tables */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      /* delete last \n at end of data... */
      if (pclDataArray[i][strlen(pclDataArray[i])-1] == 0x0A)
         pclDataArray[i][strlen(pclDataArray[i])-1] = 0x00;

      /* build FIELD-LIST with comma separator... */

      memset((void*)pclFields, 0x00, iMAX_BUF_SIZE);
      for (j=0; j<rgTM.prSection[ipSecNo].prTabDef[i].iNoOfFields; j++)
      {
         /* don't use FILL_FIELDS... */
         if (strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j], "FILL_FIELD"))
         {
            strcat(pclFields, rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j]);
            strcat(pclFields, ",");
         }
      }

      /* set end of fieldlist... */
      while (pclFields[strlen(pclFields)-1] == ',')
         pclFields[strlen(pclFields)-1] = 0x00;
      dbg(DEBUG,"<HandleSwissFlukoData> FieldList: <%s>", pclFields);

      /* calculate size of memory we need */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
         llLen = strlen(rgTM.prSection[ipSecNo].pcSystemKnz);
      else
         llLen = 0;

      llLen += sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) +
              strlen(pclFields) + strlen(pclDataArray[i]) + 3;

      /* get memory for out event */
      if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
      {
         dbg(TRACE,"<HandleSwissFlukoData> not enough memory for OutEvent!!");
         prlOutEvent = NULL;
         Terminate(0);
      }

      /* clear memory */
      memset((void*)prlOutEvent, 0x00, llLen);

      /* set structure members */
      prlOutEvent->type            = SYS_EVENT;
      prlOutEvent->command         = EVENT_DATA;
      prlOutEvent->originator      = mod_id;
      prlOutEvent->retry_count   = 0;
      prlOutEvent->data_offset   = sizeof(EVENT);
      prlOutEvent->data_length   = llLen - sizeof(EVENT);

      /* BC-Head Structure */
      prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
      prlOutBCHead->rc = RC_SUCCESS;
      strcpy(prlOutBCHead->dest_name, "STXHDL");
      strcpy(prlOutBCHead->recv_name, "EXCO");

      /* command block structure */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,
             rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
      strcpy(prlOutCmdblk->obj_name,
             rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);
      sprintf(prlOutCmdblk->tw_end, "%s,%s,STXHDL",
              rgTM.prSection[ipSecNo].pcHomeAirport,
              rgTM.prSection[ipSecNo].pcTableExtension);

      /* selection */
      /* we write system sign to selection (for GSA) */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy(prlOutCmdblk->data, rgTM.prSection[ipSecNo].pcSystemKnz);
      }

      /* fields */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
                 strlen(rgTM.prSection[ipSecNo].pcSystemKnz)+1), pclFields);
      }
      else
         strcpy((prlOutCmdblk->data+1), pclFields);

      /* data */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
               strlen(rgTM.prSection[ipSecNo].pcSystemKnz) +
               strlen(pclFields)+2), pclDataArray[i]);
      }
      else
         strcpy((prlOutCmdblk->data+strlen(pclFields)+2), pclDataArray[i]);

      /* send message */
      if (rgTM.prSection[ipSecNo].prTabDef[i].iModID > 0)
      {
         dbg(TRACE,"<HandleSwissFlukoData> sending to mod_id %d",
                     rgTM.prSection[ipSecNo].prTabDef[i].iModID);
         if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].prTabDef[i].iModID,
                   mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleSwissFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleSwissFlukoData> QuePut returns with: %d", ilRC);
      }
      else
      {
         dbg(TRACE,"<HandleSwissFlukoData> sending to router %d", igRouterID);
         if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4,
                         llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleSwissFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleSwissFlukoData> QuePut returns with: %d", ilRC);
      }

      /* free memory... */
      if (prlOutEvent != NULL)
      {
         free((void*)prlOutEvent);
         prlOutEvent = NULL;
      }
   }

   /* delete memory we doesn't need */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      dbg(DEBUG,"<HandleSwissFlukoData> delete pclDataArray[%d]", i);
      free((void*)pclDataArray[i]);
   }
   dbg(DEBUG,"<HandleSwissFlukoData> delete pclDataArray");
   free((void*)pclDataArray);

   /* only for debug... */
   dbg(TRACE,"<HandleSwissFlukoData> ---------------- END ----------------");

   /* bye bye */
   return RC_SUCCESS;
} /* end of HandleSwissFlukoData */




/******************************************************************************/
/* The handle command routine  only for fix datalengths (Command TST)         */
/* Hard Codings from HandleCommand are not implemented !!!!!                  */
/******************************************************************************/
static int HandleWarsawFlukoData(int ipSecNo, char *pcpData,
                                 long lpFileLength)
{
int         i;
int         j;
int         ilPL;
int         ilRC;
int         ilCnt;
int         ilInc;
int         ilStart;
int         ilAlloc;
int         ilTabNo;
int         iIsBlock;
int         ilLineNo;
int         ilCurRow;
int         ilNoBlanks;
int         ilCurField;
int         ilAnzFields;
int         ilNoCurElems;
int         ilFieldBufLength;
int         ilOldDebugLevel;
int         ilSysCnt;
int         ilSyslibNo;
int         ilTmpInt;
int         ilDelLength;
int         ilIgnore;
int         ilCurIgnore;
int         ilYear3;
int         ilYear4;
int         ilMonth3;
int         ilMonth4;
int         ilDay3;
int         ilDay4;
int         pilMemSteps[iTABLE_STEP];
long         llLen;
long         llPos;
long         llClsPos;
float         flTmpFloat;
char         *pclS;
char         *pclCfgPath;
char         *pclTmpDataPtr;
char         **pclDataArray;
char         pclSyslibTable[iMIN];
char         pclSyslibField[iMIN];
char         pclSyslibData[iMAX_BUF_SIZE];
char         pclCurrentTime[iMIN];
char         pclFields[iMAX_BUF_SIZE];
char         pclDataBuf[iMAXIMUM];
char         pclFieldBuf[iMAXIMUM];
char         pclSyslibResultBuf[iMAXIMUM];
char         pclSyslibTableFields[iMAXIMUM];
char         pclSyslibFieldData[iMAXIMUM];
char         pclCurrentDate[iMIN];
char         pclMonth3[iMIN];
char         pclMonth4[iMIN];
char         pclDay3[iMIN];
char         pclDay4[iMIN];
char         pclAlc3FldDat[4];
char         pclSeaBeg[8];
char         pclSeaEnd[8];
long         llDataLength = 0;
EVENT        *prlOutEvent    = NULL;
BC_HEAD      *prlOutBCHead   = NULL;
CMDBLK       *prlOutCmdblk   = NULL;

   dbg(TRACE,"<HandleWarsawFlukoData> -------- START -------------");

   /* initialize something */
   ilTabNo       = iINITIAL;
   ilAlloc       = iINITIAL;
   llPos         = 0;
   ilRC          = RC_SUCCESS;
   llDataLength  = 0;
   memset((void*)pclCurrentTime, 0x00, iMIN);
   memset((void*)pclDataBuf, 0x00, iMAXIMUM);
   memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
   for (i=0; i<iTABLE_STEP; i++)
      pilMemSteps[i] = 1;

   /* get timestamp */
   strcpy(pclCurrentTime, GetTimeStamp());

   /* delete last separator */
   i = strlen(rgTM.prSection[ipSecNo].pcEndOfLine);
   while (!memcmp(&pcpData[lpFileLength-i], rgTM.prSection[ipSecNo].pcEndOfLine, i))
   {
      lpFileLength -= i;
      pcpData[lpFileLength] = 0x00;
   }

   /* count lines of this file */
   if (strlen(pcpData) > 0)
   {
      if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> GetNoOfElements2(pcpData) returns: %d", ilLineNo);
         Terminate(0);
      }
   }
   else
   {
      ilLineNo = 0;
   }

   dbg(TRACE, "FOUND %d LINES", ilLineNo);
   rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows = ilLineNo;
   rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure = 0;
   rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows = 0;

   /* more than 0 lines? */
   if (ilLineNo)
   {
      /* so get memory for 128 Tables, this should be enough */
      if ((pclDataArray = (char**)malloc(iTABLE_STEP*sizeof(char*))) == NULL)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> can't allocte memory for DataArray");
         pclDataArray = NULL;
         Terminate(0);
      }
   }
   else
      return RC_SUCCESS;

   /* convert form ascii to ansi? */
   if (IsAscii(rgTM.prSection[ipSecNo].iCharacterSet))
   {
      dbg(TRACE,"<HandleWarsawFlukoData> start calling AsciiToAnsi...");
      AsciiToAnsi((UCHAR*)pcpData);
      dbg(TRACE,"<HandleWarsawFlukoData> end calling AsciiToAnsi...");
   }

   /* set tmp pointer... */
   pclTmpDataPtr = pcpData;

   /* for all lines */
   for (ilCurRow=0; ilCurRow<ilLineNo; ilCurRow++)
   {
      if (!(ilCurRow%15))
      {
         if (ilCurRow > 0)
         {
            dbg(TRACE,"<HandleWarsawFlukoData> working till line %d...",ilCurRow);
         }
      }

      /* get n.th line */
      memset((void*)pclDataBuf, 0x00, iMAXIMUM);
      if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr,
                                          rgTM.prSection[ipSecNo].pcEndOfLine,
                                          pclDataBuf)) == NULL)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> CopyNextField2 returns NULL");
         Terminate(0);
      }

      /* first of all, check data length, header, trailer ... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iIgnoreRecordsShorterThan)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> Ignore: <%s> (too short)", pclDataBuf);
         continue;
      }

      /* is this header or trailer? */
      if (rgTM.prSection[ipSecNo].iHeader > 0)
      {
         if (ilCurRow < rgTM.prSection[ipSecNo].iHeader)
         {
       	    dbg(DEBUG,"<HandleWarsawFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (rgTM.prSection[ipSecNo].iTrailer > 0)
      {
         if (ilCurRow > ilLineNo-rgTM.prSection[ipSecNo].iTrailer-1)
         {
	          dbg(DEBUG,"<HandleWarsawFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (!strlen(pclDataBuf))
      {
         dbg(DEBUG,"<HandleWarsawFlukoData> found empty buffer...");
         rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows++;
         continue;
      }

      /* Block-Number is always 0 */
      ilTabNo = 0;
      /* get memory for data array */
      if (ilAlloc == iINITIAL)
      {
         /* set flag */
         ilAlloc = 1;
         /* now get memory for data of this table... */
         dbg(TRACE,"ALLOCATE %ld bytes for pclDataArray[%d]",
                    iMEMORY_STEP*sizeof(char), ilTabNo);
         if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP*
                                                sizeof(char))) == NULL)
         {
            dbg(TRACE,"Can't allocate data memory for table: %d",
                       ilTabNo);
            pclDataArray[ilTabNo] = NULL;
            Terminate(0);
         }
         /* clear this memory */
         memset((void*)pclDataArray[ilTabNo],
                0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
      }

      /* check length of data buffer, only for filetype = FIX... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iSULength)
      {
         ilNoBlanks= rgTM.prSection[ipSecNo].iSULength-strlen(pclDataBuf);
         memset((void*)&pclDataBuf[strlen(pclDataBuf)], 0x20, ilNoBlanks);
         pclDataBuf[rgTM.prSection[ipSecNo].iSULength] = 0x00;
      }

      /* is this file with or without fieldnames? */
      ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
      ilInc         = 1;
      ilStart       = 0;

      /* file is FIX-type */
      /* get data for all fields */
      pclS = pclDataBuf;
      for (ilCurField=ilStart,ilCnt=0;
           ilCurField<ilAnzFields;
           ilCurField+=ilInc,ilCnt++)
      {
         /* first and first+2... is always fieldname */
         if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
         {
            if (ilStart)
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField-1];
         }

         /* must we ignore this field?, in this case we continue... */
         if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                     "FILL_FIELD"))
         {
            /* set pointer to next valid position */
            pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
            /* delete last comma */
            if (ilCurField == ilAnzFields-1)
            {
               if(pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
               {
                  pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
                  --llPos;
               }
            }
         }
         else
         {
            /* get n.th field */
            memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               strncpy(pclFieldBuf, pclS,
                       rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
            }

            if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                "ALC?", 4))
            {
               Get3LCAirline(ipSecNo, pclFieldBuf, pclAlc3FldDat);
               if (pclAlc3FldDat[0] != '\0')
               {
                  strcpy(pclFieldBuf, pclAlc3FldDat);
               }
            }
            else if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                     "VPFR", 4))
            {
               str_trm_all(pclFieldBuf, " ", TRUE);
               pclSeaBeg[0] = 0x00;
               if ((long)(strlen(pclFieldBuf)) != 5)
               {
                  ilRC = GetCurrentSeason(pclSeaBeg, pclSeaEnd);
                  ilRC = RC_SUCCESS;
               }
               if (pclSeaBeg[0] != '\0')
               {
                  strcpy(pclFieldBuf, pclSeaBeg);
               }
            }
            else if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                     "VPTO", 4))
            {
               str_trm_all(pclFieldBuf, " ", TRUE);
               pclSeaEnd[0] = 0x00;
               if ((long)(strlen(pclFieldBuf)) != 5)
               {
                  ilRC = GetCurrentSeason(pclSeaBeg, pclSeaEnd);
                  ilRC = RC_SUCCESS;
               }
               if (pclSeaEnd[0] != '\0')
               {
                  strcpy(pclFieldBuf, pclSeaEnd);
               }
            }

            /* store n.th field */
            /* we don't like empty strings... */
            if (!strlen(pclFieldBuf))
               strcpy(pclFieldBuf, " ");
            ilFieldBufLength = strlen(pclFieldBuf);
            llDataLength += (long)ilFieldBufLength+1;  /* seperator also */

            /* enough memory to store data? */
            if (llDataLength >=
                iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
            {
               /* realloc here */
               pilMemSteps[ilTabNo]++;
               dbg(TRACE,"REALLOC now %ld bytes...",
                    pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));

               if ((pclDataArray[ilTabNo] =
                   (char*)realloc(pclDataArray[ilTabNo],
                   pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char)))
                     == NULL)
               {
                  dbg(TRACE,"Cannot realloc for table %d", ilTabNo);
                  Terminate(0);
               }

               /* clear new buffer */
               llClsPos = llDataLength - (long)ilFieldBufLength;
               dbg(DEBUG,"clear from pos: %ld", llClsPos);
               memset((void*)&pclDataArray[ilTabNo][llClsPos],
                       0x00, iMEMORY_STEP*sizeof(char));
            }

            /* copy data to dataline (enough allocated) */
            memcpy((void*)&pclDataArray[ilTabNo][llPos],
                   (const void*)pclFieldBuf, (size_t)ilFieldBufLength);
            llPos += (long)ilFieldBufLength;

            /* add separator at end of data */
            if (ilCurField < ilAnzFields-1)
            {
               memcpy((void*)&pclDataArray[ilTabNo][llPos],
                      (const void*)",", (size_t)1);
               llPos++;
            }

            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               /* set to next position */
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
            }

         }  /* no FILL_FIELD */

      }  /* for all fields */

      /* add '\n' as row separator */
      memcpy((void*)&pclDataArray[ilTabNo][llPos],
             (const void*)"\n", (size_t)1);
      llPos++;

   }  /* for all rows */

   dbg(TRACE, "STEP THROUGH LINES ENDS HERE");

   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"<HandleWarsawFlukoData> Number of lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows);
   dbg(TRACE,"<HandleWarsawFlukoData> Number of SHM-Failure: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure);
   dbg(TRACE,"<HandleWarsawFlukoData> Number of empty lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows);
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");

   /* write it to database, for all used tables */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      /* delete last \n at end of data... */
      if (pclDataArray[i][strlen(pclDataArray[i])-1] == 0x0A)
         pclDataArray[i][strlen(pclDataArray[i])-1] = 0x00;

      /* build FIELD-LIST with comma separator... */

      memset((void*)pclFields, 0x00, iMAX_BUF_SIZE);
      for (j=0; j<rgTM.prSection[ipSecNo].prTabDef[i].iNoOfFields; j++)
      {
         /* don't use FILL_FIELDS... */
         if (strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j], "FILL_FIELD"))
         {
            strcat(pclFields, rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j]);
            strcat(pclFields, ",");
         }
      }

      /* set end of fieldlist... */
      while (pclFields[strlen(pclFields)-1] == ',')
         pclFields[strlen(pclFields)-1] = 0x00;
      dbg(DEBUG,"<HandleWarsawFlukoData> FieldList: <%s>", pclFields);

      /* calculate size of memory we need */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
         llLen = strlen(rgTM.prSection[ipSecNo].pcSystemKnz);
      else
         llLen = 0;

      llLen += sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) +
              strlen(pclFields) + strlen(pclDataArray[i]) + 3;

      /* get memory for out event */
      if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> not enough memory for OutEvent!!");
         prlOutEvent = NULL;
         Terminate(0);
      }

      /* clear memory */
      memset((void*)prlOutEvent, 0x00, llLen);

      /* set structure members */
      prlOutEvent->type            = SYS_EVENT;
      prlOutEvent->command         = EVENT_DATA;
      prlOutEvent->originator      = mod_id;
      prlOutEvent->retry_count   = 0;
      prlOutEvent->data_offset   = sizeof(EVENT);
      prlOutEvent->data_length   = llLen - sizeof(EVENT);

      /* BC-Head Structure */
      prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
      prlOutBCHead->rc = RC_SUCCESS;
      strcpy(prlOutBCHead->dest_name, "STXHDL");
      strcpy(prlOutBCHead->recv_name, "EXCO");

      /* command block structure */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,
             rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
      strcpy(prlOutCmdblk->obj_name,
             rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);
      sprintf(prlOutCmdblk->tw_end, "%s,%s,STXHDL",
              rgTM.prSection[ipSecNo].pcHomeAirport,
              rgTM.prSection[ipSecNo].pcTableExtension);

      /* selection */
      /* we write system sign to selection (for GSA) */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy(prlOutCmdblk->data, rgTM.prSection[ipSecNo].pcSystemKnz);
      }

      /* fields */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
                 strlen(rgTM.prSection[ipSecNo].pcSystemKnz)+1), pclFields);
      }
      else
         strcpy((prlOutCmdblk->data+1), pclFields);

      /* data */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
               strlen(rgTM.prSection[ipSecNo].pcSystemKnz) +
               strlen(pclFields)+2), pclDataArray[i]);
      }
      else
         strcpy((prlOutCmdblk->data+strlen(pclFields)+2), pclDataArray[i]);

      /* send message */
      if (rgTM.prSection[ipSecNo].prTabDef[i].iModID > 0)
      {
         dbg(TRACE,"<HandleWarsawFlukoData> sending to mod_id %d",
                     rgTM.prSection[ipSecNo].prTabDef[i].iModID);
         if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].prTabDef[i].iModID,
                   mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleWarsawFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleWarsawFlukoData> QuePut returns with: %d", ilRC);
      }
      else
      {
         dbg(TRACE,"<HandleWarsawFlukoData> sending to router %d", igRouterID);
         if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4,
                         llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleWarsawFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleWarsawFlukoData> QuePut returns with: %d", ilRC);
      }

      /* free memory... */
      if (prlOutEvent != NULL)
      {
         free((void*)prlOutEvent);
         prlOutEvent = NULL;
      }
   }

   /* delete memory we doesn't need */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      dbg(DEBUG,"<HandleWarsawFlukoData> delete pclDataArray[%d]", i);
      free((void*)pclDataArray[i]);
   }
   dbg(DEBUG,"<HandleWarsawFlukoData> delete pclDataArray");
   free((void*)pclDataArray);

   /* only for debug... */
   dbg(TRACE,"<HandleWarsawFlukoData> ---------------- END ----------------");

   /* bye bye */
   return RC_SUCCESS;
} /* end of HandleWarsawFlukoData */


/******************************************************************************/
/* The HandleSSIMFlukoData routine is only for fix datapositions and lengths  */
/* in SSIM File Format. (COMMAND: SSIM)                                       */
/******************************************************************************/
static int HandleSSIMFlukoData(int ipSecNo, char *pcpData,
                               long lpFileLength)
{
int         i;
int         j;
int         ilPL;
int         ilRC;
int         ilCnt;
int         ilInc;
int         ilStart;
int         ilAlloc;
int         ilTabNo;
int         iIsBlock;
int         ilLineNo;
int         ilCurRow;
int         ilNoBlanks;
int         ilCurField;
int         ilAnzFields;
int         ilNoCurElems;
int         ilFieldBufLength;
int         ilOldDebugLevel;
int         ilSysCnt;
int         ilSyslibNo;
int         ilTmpInt;
int         ilDelLength;
int         ilIgnore;
int         ilCurIgnore;
int         ilYear3;
int         ilYear4;
int         ilMonth3;
int         ilMonth4;
int         ilDay3;
int         ilDay4;
int         pilMemSteps[iTABLE_STEP];
long         llLen;
long         llPos;
long         llClsPos;
float         flTmpFloat;
char         *pclS;
char         *pclCfgPath;
char         *pclTmpDataPtr;
char         **pclDataArray;
char         pclSyslibTable[iMIN];
char         pclSyslibField[iMIN];
char         pclSyslibData[iMAX_BUF_SIZE];
char         pclCurrentTime[iMIN];
char         pclFields[iMAX_BUF_SIZE];
char         pclDataBuf[iMAXIMUM];
char         pclFieldBuf[iMAXIMUM];
char         pclSyslibResultBuf[iMAXIMUM];
char         pclSyslibTableFields[iMAXIMUM];
char         pclSyslibFieldData[iMAXIMUM];
char         pclCurrentDate[iMIN];
char         pclMonth3[iMIN];
char         pclMonth4[iMIN];
char         pclDay3[iMIN];
char         pclDay4[iMIN];
char         pclAlc3FldDat[5];
char         pclTblNam[10];
long         llDataLength = 0;
EVENT        *prlOutEvent    = NULL;
BC_HEAD      *prlOutBCHead   = NULL;
CMDBLK       *prlOutCmdblk   = NULL;
int          ilVialFlt;
int          ilLegNum;
char         pclOrg[5];

   dbg(TRACE,"<HandleSSIMFlukoData> -------- START -------------");

   /* initialize something */
   ilTabNo       = iINITIAL;
   ilAlloc       = iINITIAL;
   llPos         = 0;
   ilRC          = RC_SUCCESS;
   llDataLength  = 0;
   memset((void*)pclCurrentTime, 0x00, iMIN);
   memset((void*)pclDataBuf, 0x00, iMAXIMUM);
   memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
   for (i=0; i<iTABLE_STEP; i++)
      pilMemSteps[i] = 1;

   /* get timestamp */
   strcpy(pclCurrentTime, GetTimeStamp());

   /* delete last separator */
   i = strlen(rgTM.prSection[ipSecNo].pcEndOfLine);
   while (!memcmp(&pcpData[lpFileLength-i], rgTM.prSection[ipSecNo].pcEndOfLine, i))
   {
      lpFileLength -= i;
      pcpData[lpFileLength] = 0x00;
   }

   /* count lines of this file */
   if (strlen(pcpData) > 0)
   {
      if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> GetNoOfElements2(pcpData) returns: %d", ilLineNo);
         Terminate(0);
      }
   }
   else
   {
      ilLineNo = 0;
   }

   dbg(TRACE, "FOUND %d LINES", ilLineNo);
   rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows = ilLineNo;
   rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure = 0;
   rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows = 0;

   /* more than 0 lines? */
   if (ilLineNo)
   {
      /* so get memory for 128 Tables, this should be enough */
      if ((pclDataArray = (char**)malloc(iTABLE_STEP*sizeof(char*))) == NULL)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> can't allocte memory for DataArray");
         pclDataArray = NULL;
         Terminate(0);
      }
   }
   else
      return RC_SUCCESS;

   /* convert form ascii to ansi? */
   if (IsAscii(rgTM.prSection[ipSecNo].iCharacterSet))
   {
      dbg(TRACE,"<HandleSSIMFlukoData> start calling AsciiToAnsi...");
      AsciiToAnsi((UCHAR*)pcpData);
      dbg(TRACE,"<HandleSSIMFlukoData> end calling AsciiToAnsi...");
   }

   /* set tmp pointer... */
   pclTmpDataPtr = pcpData;

   /* for all lines */
   for (ilCurRow=0; ilCurRow<ilLineNo; ilCurRow++)
   {
      if (!(ilCurRow%15))
      {
         if (ilCurRow > 0)
         {
            dbg(TRACE,"<HandleSSIMFlukoData> working till line %d...",ilCurRow);
         }
      }

      /* get n.th line */
      memset((void*)pclDataBuf, 0x00, iMAXIMUM);
      if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr,
                                          rgTM.prSection[ipSecNo].pcEndOfLine,
                                          pclDataBuf)) == NULL)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> CopyNextField2 returns NULL");
         Terminate(0);
      }

      /* first of all, check data length, header, trailer ... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iIgnoreRecordsShorterThan)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> Ignore: <%s> (too short)", pclDataBuf);
         continue;
      }

      /* ONLY RECORD 3 IS DATA FOR AFTTAB */
      if (pclDataBuf[0] != '3')
      {
         dbg(TRACE,"<HandleSSIMFlukoData> Ignore (Header or trailer)");
         continue;
      }

      /* is this header or trailer? */
      if (rgTM.prSection[ipSecNo].iHeader > 0)
      {
         if (ilCurRow < rgTM.prSection[ipSecNo].iHeader)
         {
       	    dbg(DEBUG,"<HandleSSIMFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (rgTM.prSection[ipSecNo].iTrailer > 0)
      {
         if (ilCurRow > ilLineNo-rgTM.prSection[ipSecNo].iTrailer-1)
         {
	          dbg(DEBUG,"<HandleSSIMFlukoData> ignore line: %d", ilCurRow);
	          continue;
         }
      }

      if (!strlen(pclDataBuf))
      {
         dbg(DEBUG,"<HandleSSIMFlukoData> found empty buffer...");
         rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows++;
         continue;
      }

      /* Block-Number is always 0 */
      ilTabNo = 0;
      /* get memory for data array */
      if (ilAlloc == iINITIAL)
      {
         /* set flag */
         ilAlloc = 1;
         /* now get memory for data of this table... */
         dbg(TRACE,"ALLOCATE %ld bytes for pclDataArray[%d]",
                    iMEMORY_STEP*sizeof(char), ilTabNo);
         if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP*
                                                sizeof(char))) == NULL)
         {
            dbg(TRACE,"Can't allocate data memory for table: %d",
                       ilTabNo);
            pclDataArray[ilTabNo] = NULL;
            Terminate(0);
         }
         /* clear this memory */
         memset((void*)pclDataArray[ilTabNo],
                0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
      }

      /* check length of data buffer, only for filetype = FIX... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iSULength)
      {
         ilNoBlanks= rgTM.prSection[ipSecNo].iSULength-strlen(pclDataBuf);
         memset((void*)&pclDataBuf[strlen(pclDataBuf)], 0x20, ilNoBlanks);
         pclDataBuf[rgTM.prSection[ipSecNo].iSULength] = 0x00;
      }

      /* is this file with or without fieldnames? */
      ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
      ilInc         = 1;
      ilStart       = 0;


      /* GET LEGNUMBER (LEGN) */
      pclS = pclDataBuf;
      ilVialFlt = FALSE;
      pclS += 11;                       /* LEGN AT POS 11 (hardcoded) */
      memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
      strncpy(pclFieldBuf, pclS, 2);
      ilLegNum = atoi(pclFieldBuf);
      if ((ilLegNum > 1) && (ilLegNum < 11))        /* only place for 10 VIAS */
      {
         ilVialFlt = TRUE;
      }
      else if (ilLegNum > 10)
      {
         ilVialFlt = -1;
      }
      else
      {
         pclS = pclDataBuf;
         for (ilCnt = 0; ilCnt < 9; ilCnt++)      /* field 9 is ORG3 */
         {                                                                                  pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCnt
]+1;
         }
         memcpy (pclOrg, pclS, 3);
         pclOrg[3] = 0x00;
         if (!strcmp(pclOrg, "ATH"))
         {
            igOrgAth = TRUE;
         }
         else
         {
            igOrgAth = FALSE;
         }
      }

      if (ilVialFlt)
      {
         /* manipulate line before in pclDataArray => no realloc */

         ilRC = UpdLastLine(pclDataBuf,pclDataArray[ilTabNo],ipSecNo,ilTabNo);

      } /* LEGN > 1 */
      else if (ilVialFlt == -1)
      {
          dbg(TRACE, "TOO MANY LEGNUMBERS FOR FLIGHT (ONLY 10 ALLOWED");
      }
      else  /* LEGN == 1 */
      {
         /* file is FIX-type */
         /* get data for all fields */
         pclS = pclDataBuf;
         for (ilCurField=ilStart,ilCnt=0;
              ilCurField<ilAnzFields;
              ilCurField+=ilInc,ilCnt++)
         {
            /* first and first+2... is always fieldname */
            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               if (ilStart)
                  pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField-1];
            }

            /* must we ignore this field?, in this case we continue... */
            if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                        "FILL_FIELD"))
            {
               /* set pointer to next valid position */
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
               /* delete last comma */
               if (ilCurField == ilAnzFields-1)
               {
                  if(pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
                  {
                     pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
                     --llPos;
                  }
               }
            }
            else /* not FILL_FIELD */
            {
               /* get n.th field */
               memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
               if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
               {
                  /* pclFieldBuf = DATA OF Nth FIELD */
                  strncpy(pclFieldBuf, pclS,
                          rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
               }

               /* store n.th field */
               /* we don't like empty strings... */
               if (!strlen(pclFieldBuf))
               {
                  memset((void*)pclFieldBuf, ' ', rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
               }
               if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],
                   "ALC?", 4))
               {
                  Get3LCAirline(ipSecNo, pclFieldBuf, pclAlc3FldDat);
                  if (pclAlc3FldDat[0] != '\0')
                  {
                     strcpy(pclFieldBuf, pclAlc3FldDat);
                  }
               }

               ilFieldBufLength = strlen(pclFieldBuf);
               llDataLength += (long)ilFieldBufLength+1;  /* seperator also */

               /* enough memory to store data? */
               if (llDataLength >=
                   iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
               {
                  /* realloc here */
                  pilMemSteps[ilTabNo]++;
                  dbg(TRACE,"REALLOC now %ld bytes...",
                       pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));

                  if ((pclDataArray[ilTabNo] =
                      (char*)realloc(pclDataArray[ilTabNo],
                      pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char)))
                        == NULL)
                  {
                     dbg(TRACE,"Cannot realloc for table %d", ilTabNo);
                     Terminate(0);
                  }

                  /* clear new buffer */
                  llClsPos = llDataLength - (long)ilFieldBufLength;
                  dbg(DEBUG,"clear from pos: %ld", llClsPos);
                  memset((void*)&pclDataArray[ilTabNo][llClsPos],
                          0x00, iMEMORY_STEP*sizeof(char));
               }

               /* ADD pclFieldBuf TO pclDataArray */
               /* copy data to dataline (enough allocated) */
               memcpy((void*)&pclDataArray[ilTabNo][llPos],
                      (const void*)pclFieldBuf, (size_t)ilFieldBufLength);
               llPos += (long)ilFieldBufLength;

               /* add separator at end of data */
               if (ilCurField < ilAnzFields-1)
               {
                  memcpy((void*)&pclDataArray[ilTabNo][llPos],
                         (const void*)",", (size_t)1);
                  llPos++;
               }

               if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
               {
                  /* set to next position */
                  pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
               }

            }  /* no FILL_FIELD */

         }  /* for all fields */

         /* add '\n' as row separator */
         memcpy((void*)&pclDataArray[ilTabNo][llPos],
                (const void*)"\n", (size_t)1);
         llPos++;

      } /* LEGN == 1  */

   }  /* for all rows */

   dbg(TRACE, "STEP THROUGH LINES ENDS HERE");

   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"<HandleSSIMFlukoData> Number of lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows);
   dbg(TRACE,"<HandleSSIMFlukoData> Number of SHM-Failure: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure);
   dbg(TRACE,"<HandleSSIMFlukoData> Number of empty lines: %d", rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows);
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");
   dbg(TRACE,"--------------------------------------------------");

   /* write it to database, for all used tables */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      /* delete last \n at end of data... */
      if (pclDataArray[i][strlen(pclDataArray[i])-1] == 0x0A)
         pclDataArray[i][strlen(pclDataArray[i])-1] = 0x00;

      strcat(pclDataArray[i], "\nEND");          /* flight searches for */

      /* build FIELD-LIST with comma separator... */

      memset((void*)pclFields, 0x00, iMAX_BUF_SIZE);
      for (j=0; j<rgTM.prSection[ipSecNo].prTabDef[i].iNoOfFields; j++)
      {
         /* don't use FILL_FIELDS... */
         if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j], "FILL_FIELD"))
         {
         }
         else if (!strncmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j],
                           "ALC?", 4))
         {
            strcat(pclFields, "ALC3?,");
         }
         else
         {
            strcat(pclFields, rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j]);
            strcat(pclFields, ",");
         }
      }

      /* set end of fieldlist... */
      while (pclFields[strlen(pclFields)-1] == ',')
         pclFields[strlen(pclFields)-1] = 0x00;
      dbg(DEBUG,"<HandleSSIMFlukoData> FieldList: <%s>", pclFields);

      /* calculate size of memory we need */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
         llLen = strlen(rgTM.prSection[ipSecNo].pcSystemKnz);
      else
         llLen = 0;

      llLen += sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) +
              strlen(pclFields) + strlen(pclDataArray[i]) + 3;

      /* get memory for out event */
      if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> not enough memory for OutEvent!!");
         prlOutEvent = NULL;
         Terminate(0);
      }

      /* clear memory */
      memset((void*)prlOutEvent, 0x00, llLen);

      /* set structure members */
      prlOutEvent->type            = SYS_EVENT;
      prlOutEvent->command         = EVENT_DATA;
      prlOutEvent->originator      = mod_id;
      prlOutEvent->retry_count   = 0;
      prlOutEvent->data_offset   = sizeof(EVENT);
      prlOutEvent->data_length   = llLen - sizeof(EVENT);

      /* BC-Head Structure */
      prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
      prlOutBCHead->rc = RC_SUCCESS;
      strcpy(prlOutBCHead->dest_name, "STXHDL");
      strcpy(prlOutBCHead->recv_name, "EXCO");

      /* command block structure */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,
             rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
      strcpy(prlOutCmdblk->obj_name,
             rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);
      sprintf(prlOutCmdblk->tw_end, "%s,%s,STXHDL",
              rgTM.prSection[ipSecNo].pcHomeAirport,
              rgTM.prSection[ipSecNo].pcTableExtension);

      /* selection */
      /* we write system sign to selection (for GSA) */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy(prlOutCmdblk->data, rgTM.prSection[ipSecNo].pcSystemKnz);
      }

      /* fields */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
                 strlen(rgTM.prSection[ipSecNo].pcSystemKnz)+1), pclFields);
      }
      else
         strcpy((prlOutCmdblk->data+1), pclFields);

      /* data */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
               strlen(rgTM.prSection[ipSecNo].pcSystemKnz) +
               strlen(pclFields)+2), pclDataArray[i]);
      }
      else
         strcpy((prlOutCmdblk->data+strlen(pclFields)+2), pclDataArray[i]);

      /* send message */
      if (rgTM.prSection[ipSecNo].prTabDef[i].iModID > 0)
      {
         dbg(TRACE,"<HandleSSIMFlukoData> sending to mod_id %d",
                     rgTM.prSection[ipSecNo].prTabDef[i].iModID);
         if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].prTabDef[i].iModID,
                   mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleSSIMFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleSSIMFlukoData> QuePut returns with: %d", ilRC);
      }
      else
      {
         dbg(TRACE,"<HandleSSIMFlukoData> sending to router %d", igRouterID);
         if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4,
                         llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<HandleSSIMFlukoData> QuePut returns with: %d", ilRC);
            Terminate(0);
         }
         dbg(TRACE,"<HandleSSIMFlukoData> QuePut returns with: %d", ilRC);
      }

      /* free memory... */
      if (prlOutEvent != NULL)
      {
         free((void*)prlOutEvent);
         prlOutEvent = NULL;
      }
   }

   /* delete memory we doesn't need */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      dbg(DEBUG,"<HandleSSIMFlukoData> delete pclDataArray[%d]", i);
      free((void*)pclDataArray[i]);
   }
   dbg(DEBUG,"<HandleSSIMFlukoData> delete pclDataArray");
   free((void*)pclDataArray);

   /* only for debug... */
   dbg(TRACE,"<HandleSSIMFlukoData> ---------------- END ----------------");

   /* bye bye */
   return RC_SUCCESS;
} /* end of HandleSSIMFlukoData */






/*****************************************************************************/
/* This Routine handles the Import of PAX-Prognosedata for Swissport         */
/*                                                                           */
/*****************************************************************************/
static int HandlePrognoseData(int ipSecNo, char *pcpData, long lpFileLength)
{
  char *pclFct = "HandlePrognoseData";
  int    ilRC = RC_SUCCESS;
  int    ili = 0;
  int    ilj = 0;
  int    ilLineNo = 0;
  int	 ilDataSize = 0;
  int    ilCurTable = 0;
  int    ilItemLen = 0;
  int    ilStrgLen = 0;
  int    ilDataLen = 0;
  int    ilAddCDAT;
  int    ilAddUSEC;
  char   pclTime[16] = "";
  char   pclDataLine[iMAXIMUM];
  char   pclFieldList[iMAXIMUM];
  char   pclTmpStr[iMIN];
  char   pclALC[iMAXIMUM];

  /* Buffer */
  static char *pclDataBuf         = NULL;
  static int    ilDataBufSize     = 0;
  static int    ilDataBufSizeUsed = 0;
  /* pointer */
  char *pclDataBufPtr = NULL;
  char *pclTmpDataPtr = NULL;
  char *pclPtrDataLine= NULL;

  /* Get Time */
  ilRC = GetCedaTimestamp(&pclTime[0],15,0);

  /* count lines of this file */
  if (strlen(pcpData) > 0)
  {
    if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
    {
      dbg(TRACE,"<%s> GetNoOfElements2(pcpData) returns: %d", pclFct, ilLineNo);
      Terminate(0);
    }
  }
  else
  {
    ilLineNo = 0;
  }

  dbg(TRACE,"<%s> FOUND %d Lines",pclFct,ilLineNo);

  /* Calculating Buffer for output */
  ilDataSize = (((rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iNoOfFields+1)*5)*ilLineNo);
  dbg(TRACE,"<%s> No of Fields = %d",pclFct, rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iNoOfFields);
  dbg(TRACE,"<%s> DataSize = %d Bytes",pclFct,ilDataSize);
  ilRC = HandleBuffer(&pclDataBuf,&ilDataBufSize,0,ilDataSize+1,512);
  if(ilRC != RC_SUCCESS)
  {
    dbg(TRACE,"<%s> ERROR - Malloc for DataBuffer failed !",pclFct);
    return ilRC;
  }
  /* building fieldlist */
  pclFieldList[0] = 0x00;
  pclDataBuf[0] = 0x00;
  for(ili=0;ili<rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iNoOfFields;ili++)
  {
/*    dbg(TRACE,"<%s> Field %2d:%s DataLength:%d",pclFct,ili,
 *	           rgTM.prSection[ipSecNo].prTabDef[ilCurTable].pcFields[ili],
 *             rgTM.prSection[ipSecNo].prTabDef[ilCurTable].piDataLength[ili]);
 */
     /* first field is airlinecode 2 and 3 lettercode mixed -> must be split */
    if(ili==0)
    {
       strcat(pclFieldList,"ALC2,ALC3,");
    }
    else
    {
      strcat(pclFieldList,rgTM.prSection[ipSecNo].prTabDef[ilCurTable].pcFields[ili]);
      strcat(pclFieldList,",");
    }
  } /* end for */

  /* replace last comma -> '\n'*/
  ilStrgLen = strlen(pclFieldList);
  pclFieldList[ilStrgLen-1] = '\n';
  pclDataBuf[0]  = 0x00;
  strcpy(pclDataBuf,pclFieldList);
  ilDataBufSizeUsed = ilStrgLen;


  /* dbg(TRACE,"<%s> DataBuf<%s>",pclFct,pclDataBuf);*/
  /* set pointer to data and buffer */
  pclTmpDataPtr = pcpData;
  pclDataBufPtr = &pclDataBuf[ilDataBufSizeUsed];

  for(ili=1;ili<(ilLineNo-rgTM.prSection[ipSecNo].iTrailer);ili++)
  {
    /*pclDataBufPtr = &pclDataBuf[0];*/

    /* get Dataline */
	if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr, rgTM.prSection[ipSecNo].pcEndOfLine, pclDataLine)) == NULL)
	{
      dbg(TRACE,"<HandleCommand> CopyNextField2 returns NULL");
	  Terminate(0);
	}
    /* dbg(DEBUG,"<%s> get %d. DataLine <%s>",pclFct,ili,pclDataLine);*/
    ilItemLen = strlen(pclDataLine);
    pclPtrDataLine = pclDataLine;

    /* check is Buffer big enough */
    ilRC = HandleBuffer(&pclDataBuf,&ilDataBufSize,ilDataBufSizeUsed,
                        ilItemLen+rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iNoOfFields+64,512);
    for(ilj=0;ilj<rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iNoOfFields;ilj++)
    {
      ilDataLen = rgTM.prSection[ipSecNo].prTabDef[ilCurTable].piDataLength[ilj];
      strncpy(pclTmpStr,pclPtrDataLine,ilDataLen);
      pclTmpStr[ilDataLen] = 0x00;

      /* first field is airlinecode  2 and 3 lettercode mixed -> must be split */
      if(ilj==0)
      {
        if(pclTmpStr[2] == ' ')
        {
          pclTmpStr[2] = 0x00;
		  ilRC = Get_xLC_ALC("ALTTAB","ALC2",pclTmpStr,"ALC3",pclALC);
		  sprintf(pclDataBufPtr,"%.2s,%.3s,",pclTmpStr,pclALC);
        }
        else
        {
          pclTmpStr[3] = 0x00;
  		  ilRC = Get_xLC_ALC("ALTTAB","ALC3",pclTmpStr,"ALC2",pclALC);
          sprintf(pclDataBufPtr,"%.2s,%.3s,",pclALC,pclTmpStr);
        }
        pclDataBufPtr +=7;
		ilDataBufSizeUsed +=6;
      }
      else
      {
        sprintf(pclDataBufPtr,"%s,",pclTmpStr);
        pclDataBufPtr += (ilDataLen+1);
		ilDataBufSizeUsed += (ilDataLen+1);
      }

      pclPtrDataLine += ilDataLen;

    } /* end for(ilj) */

    /* replace comma at end of line */
    *(pclDataBufPtr-1) = '\n';


  } /* end for(ili) */

  sprintf(pclTmpStr,"%s,%s,STXHDL",rgTM.prSection[ipSecNo].pcHomeAirport,
                                   rgTM.prSection[ipSecNo].pcTableExtension);
  ilRC = SendCedaEvent(rgTM.prSection[ipSecNo].prTabDef[ilCurTable].iModID,0,
                       "STXHDL","EXCO","",pclTmpStr,
					   rgTM.prSection[ipSecNo].prTabDef[ilCurTable].pcSndCmd,
					   rgTM.prSection[ipSecNo].prTabDef[ilCurTable].pcTableName,
					   "","",pclDataBuf,"",0,ilRC);

  if(ilRC != RC_SUCCESS)
  {
    dbg(TRACE,"<%s> ERROR - SendCedaEvent failed",pclFct);
    ilRC = RC_FAIL;
  }
  return ilRC;

} /* end HandlePrognoseData */
/*****************************************************************************/
/* GetAirlineCode search 2 or 3  Lettercode                                  */
/*****************************************************************************/

static int Get_xLC_ALC(char *pcpTable, char *pcpField,char *pcpData,
                       char *pcpResultField, char *pcpResultData)
{
  char *pclFct="Get_xLC_ALC";
  int ilRC = RC_SUCCESS;
  int ilGetRc = RC_SUCCESS;
  int ilCount = 0;

  int ilUseOra = FALSE;
  short slCursor = 0;
  short slFkt = START;
  char *pclNlPos;
  char pclSqlBuf[512];
  char pclSqlKey[128];


/*
  ilRC = CheckSysShmField(pcpTable, pcpField, RC_SUCCESS);
  ilRC = CheckSysShmField(pcpTable, pcpResultField, ilRC);
*/
  if(ilRC == RC_SUCCESS)
  {
/*
    sprintf(pclSqlBuf,"%s,HOPO",pcpFields);
    sprintf(pclSqlKey,"%s,%s",pcpData,pcgH3LC);
*/
    ilCount=1;
    ilRC = syslibSearchDbData(pcpTable,pcpField,pcpData,
                          pcpResultField,pcpResultData,&ilCount,"\n");
    if((ilCount<=0)||(ilRC!=RC_SUCCESS))
	  strcpy(pcpResultData,"   ");
	dbg(DEBUG,"<%s> Field: <%s><%s>  Result: <%s><%s>",pclFct,
	                       pcpField,pcpData,pcpResultField,pcpResultData);

  }
  return ilRC;
} /* end GetBasicData */


static int UpdLastLine (char *pcpDataLine, char *pcpDataBuffer,
                        int ipSecNo, int ipTabNo)
{
int   ilRC = RC_SUCCESS;
int   ilPos;
int   ilCnt;
int   ilBegPos;
char  *pclS;
char  *pclB;
char  pclOrg[5];
char  pclDes[5];
char  pclViaStod[5];
char  pclDesStoa[5];


  ilPos = strlen(pcpDataBuffer)-1;
  pclB = pcpDataBuffer;
  pclB += ilPos-1;
  while(*pclB != '\n')
  {
     pclB--;
     ilPos--;
  }
  pclB++;
  ilBegPos = ilPos;
  dbg(TRACE, "pclB <%s>", pclB);

  dbg(TRACE, "pcpDataLine <%s>", pcpDataLine);

  dbg(TRACE, "ADD VIAL TO LIST");
  pclS = pcpDataLine;
  for (ilCnt = 0; ilCnt < 11; ilCnt++)      /* field 11 is ORG3 */
  {
     pclS += rgTM.prSection[ipSecNo].prTabDef[ipTabNo].piDataLength[ilCnt];
  }
  memcpy (pclOrg, pclS, 3);
  pclOrg[3] = 0x00;
  pclS += 3;

  memcpy (pclViaStod, pclS, 4);             /* field 12 is ORG STOD */
  pclViaStod[4] = 0x00;
  pclS += 4;

  for (ilCnt = 13; ilCnt < 16; ilCnt++)    /* field 16 is DES3 */
  {
    pclS += rgTM.prSection[ipSecNo].prTabDef[ipTabNo].piDataLength[ilCnt];
  }
  memcpy (pclDes, pclS, 3);
  pclDes[3] = 0x00;
  pclS += 3;

  memcpy (pclDesStoa, pclS, 4);             /* field 17 is DES STOA */
  pclDesStoa[4] = 0x00;
  pclS += 4;

  pclS = pclB;
  if (!strcmp(pclOrg, "ATH"))
  {
     dbg(TRACE, "ERROR: THIS SHOULD NOT HAPPEN");
     dbg(TRACE, "LEGNUM > 1 AND ORIGIN IS ATH");
  }
  else
  {
     dbg(TRACE, "ADD ORG <%s> TO VIAL AND SET DES TO <%s>",
                 pclOrg, pclDes);
     pclS += 51;
     memcpy(pclS, pclDes, 3);
     pclS += 4;
     memcpy(pclS, pclDesStoa, 4);

     pclS = strrchr(pclB, ',');               /* search last komma */
     pclS++;
     while (*pclS != ' ')
     {
        pclS++;
     }
     memcpy(pclS, pclOrg, 3);
     pclS += 3;
     memcpy(pclS, pclViaStod, 4);
  }
  dbg(TRACE, "pclB <%s>", pclB);

  return ilRC;

} /* end of UpdLastLine */



/* Check length of pcpAlc2 AirlineCode and put 3-Letter-Code to pcpAlc3 */

static int Get3LCAirline (int ipSecNo, char *pcpAlc2, char *pcpAlc3)
{
int  ilRC = RC_SUCCESS;
int  ilSysCnt = 1;
char pclAlc2[5];
char pclAlc3[5];
char pclAlc[5];
char pclTblNam[10];
char pclSyslibFlds[50];

  strcpy(pclAlc2, pcpAlc2);
  pcpAlc3[0] = 0x00;

  str_trm_all(pclAlc2, " ", TRUE);
  pclAlc3[0] = 0x00;
  pclAlc[0] = 0x00;
  ilSysCnt = 1;

  if ((long)(strlen(pclAlc2)) == 2)
  {
     pclAlc[0] = 0x00;
     if (!strcmp(rgTM.prSection[ipSecNo].pcTableExtension, "TAB"))
     {
        strcpy(pclTblNam, "ALTTAB");
        sprintf(pclSyslibFlds, "%s,%s", pclAlc2,
                         rgTM.prSection[ipSecNo].pcHomeAirport);
        ilRC = syslibSearchDbData(pclTblNam, "ALC2,HOPO",
                                  pclSyslibFlds, "ALC3",
                                  pclAlc, &ilSysCnt,"");
     }
     else
     {
        sprintf(pclTblNam, "ALT%s",
                        rgTM.prSection[ipSecNo].pcHomeAirport);
        ilRC = syslibSearchDbData(pclTblNam, "ALC2",
                                  pclAlc2, "ALC3",
                                  pclAlc, &ilSysCnt,"");
     }

     if (ilRC == RC_SUCCESS)
     {
        str_trm_all(pclAlc, " ", TRUE);
        if ((long)(strlen(pclAlc)) == 3)
        {
           strcpy(pclAlc3, pclAlc);
        }
     }

     if (pclAlc3[0] == '\0')
     {
        dbg(TRACE, "HandleSSIMFlukoData: TWEREM: No ALC3 for ALC2 <%s> found",
                    pclAlc2);
     }
     else
     {
        strcpy(pcpAlc3, pclAlc3);
     }
  } /* ALC2 FOUND */

 return ilRC;

} /* end of Get3LCAirline */


static int GetCurrentSeason (char *pcpSeaBeg, char *pcpSeaEnd)
{
int  ilRC = RC_SUCCESS;

  /* first hard coded because of import only once */
  /* else grep valid season from SEATAB */

dbg(TRACE,"GetCurrentSeason entered");

  strcpy(pcpSeaBeg, "26MAR");
  strcpy(pcpSeaEnd, "27OCT");

  return ilRC;

} /* end of GetCurrentSeason */

/******************************************************************************/
/* The handle where clause routine                                            */
/* by gle:                                                                    */
/* checks if rgTM.prSection[int].prTabDef[int].iNoOfWhereFields > 0           */
/* is true. if so, the where clause is generated in a format:                 */
/* WHERE [SQL]Fld1='VFld1' [AND Fld...='VFld...']                                  */
/* remark: pcpSelection must not be NULL, pcpSelection must                   */
/*         be allocated big enough !                                          */
/*         former contents of pcpSelection remains due to the                 */
/*         use of sprintf!  <==>  '\0' !!!                                    */
/******************************************************************************/
static int GLEHandleWhereClause(TSTSection rpSec, TSTTabDef rpDef,
								char* pcpData, char* pcpSelection)
{
   int		ilRC = RC_SUCCESS;
   int		ilWCnt;		/* where field counter */
   int		ilCounter = 0;
   int		ilPos;		/* data position       */
   int 		ilFillNo = 0;
   int      ilIgnore = FALSE;
   char     pclTmpStr[iMAXIMUM];
   char     pclTmpStrg[iMAXIMUM];
   char		pclSelection[iMAXIMUM];
   char     pclFields[iMAXIMUM];
   char*	pclWhereValue = NULL;
   char*	pclData = NULL;
   char*	pclPtr  = NULL;
   char     pclFieldList[iMAXIMUM];
   char     pclDataList[iMAXIMUM];

/*		dbg(TRACE, "<GLEHandleWhereClause> -------------- START -------------");
 *		dbg(DEBUG, "pcpData : <%s>", pcpData);
 *		dbg(DEBUG, "pcpSelection : <%s>", pcpSelection);
 */

   /* check parameter */
   if ((pcpSelection == NULL) || (pcpData == NULL))
   {
	ilRC = RC_FAIL;
   } /* end if param check */

   /* check global variables */
   if ((rpDef.iNoOfWhereFields <= 0) || (rpDef.iNoOfFields <= 0)
       || (rpDef.iNoOfWhereFields > rpDef.iNoOfFields))
   {
	ilRC = RC_FAIL;
	dbg(DEBUG, "<GLEHandleWhereClause> checked <iNoOfWhereFields> and <iNoOfFields> : (%d) <= (%d) are invalid", rpDef.iNoOfWhereFields, rpDef.iNoOfFields);
   } /* end if global variables check */

   if ( (pclData = (char*) malloc( sizeof(char) * ((int)strlen(pcpData) + 1) ))
       == NULL)
   {
	ilRC = RC_FAIL;
	dbg(TRACE, "<GLEHandleWhereClause> cannot allocate %d bytes",
		sizeof(char) * ((int)strlen(pcpData) + 1));
	Terminate(0);
   }

   if (ilRC == RC_SUCCESS)
   { /* GO */
      pclTmpStr[0] = '\0';
      pclSelection[0] = '\0';
      pclFields[0]    = '\0';
      /* make list of fields */
      while (ilCounter < rpDef.iNoOfFields)
      {
	 if (strcmp(rpDef.pcFields[ilCounter], "FILL_FIELD") != 0)
	 {
		strcat(pclFields, rpDef.pcFields[ilCounter]);
		strcat(pclFields, ",");
	 } /* end if skipping fill-fields */
	 ilCounter++;
      }
      *(pclFields + strlen(pclFields) - 1) = '\0';
	  /* dbg(DEBUG,"pclFields : <%s>", pclFields);*/
      *pclData = '\0';
      strcpy(pclData, pcpData);
      /* dbg(DEBUG,"pclData : <%s>",pclData);*/
      pclPtr = strtok(pclData, rpSec.pcEndOfLine);

	  /* dbg(DEBUG,"pclData : <%s>", pclData);*/
	  /* dbg(DEBUG,"pcEndOfLine : (%d)", *(rpSec.pcEndOfLine));*/
      /* send records with where-clause */
      while ((pclPtr != NULL) && (ilRC == RC_SUCCESS))
	  {
	 	ilCounter = 0;
        strcpy(pclDataList,pclPtr);
        strcpy(pclFieldList,pclFields);
        dbg(DEBUG,"\nFieldlist:<%s>\nDataList<%s>",pclFieldList,pclDataList);

	  	/*dbg(DEBUG,"pclPtr : [%p], <%s>", pclPtr, pclPtr); */
      	/* search where field in field list */
      	 for (ilWCnt = 0; ilWCnt < rpDef.iNoOfWhereFields; ilWCnt ++)
      	 {
		ilPos = -1;
		ilFillNo = 0;
		/* compare with field list values */
		do
		{
	   	   ilPos ++;
	   	   /* dbg(DEBUG, "rpDef.pcFields[%d] : <%s>", ilPos,
			*             rpDef.pcFields[ilPos]); */
	   	   /* dbg(DEBUG, "rpDef.pcWhereFields[%d] : <%s>", ilWCnt,
			*             rpDef.pcWhereFields[ilWCnt]);*/
	   	   if (strcmp(rpDef.pcFields[ilPos], "FILL_FIELD") == 0)
	   	   {
			ilFillNo ++;
	   	   }
		} while ( (strcmp(rpDef.pcFields[ilPos], rpDef.pcWhereFields[ilWCnt]) != 0)
			 && (ilPos < rpDef.iNoOfFields) );
		ilPos -= ilFillNo;
		/* need a match to run */
		if (ilPos < rpDef.iNoOfFields)
		{
	   	   ilCounter ++;
	   	   if ((pclWhereValue = GetDataField(pclPtr, ilPos, ','))
		       == NULL)
	   	   {
   			dbg(TRACE,"<GLEHandleWhereClause> GetDataField (%d) returns NULL", ilPos);
   			Terminate(0);
	   	   }
		   /* value must not be less than at least one blank */
		   if (*pclWhereValue == '\0')
		   {
			strcpy(pclWhereValue, " ");
		   }

		   /* special for swissport handling ALC2,ALC3 */
		   if( strcmp(rpDef.pcWhereFields[ilWCnt],"ALC2") == 0)
		   {
		   	 if(strlen(pclWhereValue) != 2)   /* in ALC2 is ALC3 code */
			 {
			 	strcpy(pclWhereValue, " ");
				ilIgnore = TRUE;
				ilCounter--;
				dbg(DEBUG,"SetFieldValue remove ALC2 from<%s><%s>",pclFieldList,pclDataList);
				SetFieldValue(FALSE,pclFieldList,pclDataList,"ALC2","");
				dbg(DEBUG,"Removed List <%s><%s>",pclFieldList,pclDataList);
             }
			 else
			 {
			   ilIgnore = FALSE;
			 }
		   }
		   else if( strcmp(rpDef.pcWhereFields[ilWCnt],"ALC3") == 0)
		   {
		   	 if(strlen(pclWhereValue) != 3) /* in ALC3 is ALC2 code */
			 {
			 	strcpy(pclWhereValue, " ");
             	ilIgnore = TRUE;
				ilCounter--;
				dbg(DEBUG,"SetFieldValue remove ALC3 from<%s><%s>",pclFieldList,pclDataList);
				SetFieldValue(FALSE,pclFieldList,pclDataList,"ALC3","");
     			dbg(DEBUG,"Removed List <%s><%s>",pclFieldList,pclDataList);
			 }
			 else
			 {
			   ilIgnore = FALSE;
			 }
		   }

	   	   if ((ilCounter == 1) && (ilIgnore == FALSE))
	   	   { /* add first where field with value */

				sprintf(pclSelection, "WHERE [SQL]%s='%s'",
									rpDef.pcWhereFields[ilWCnt],
				  					pclWhereValue);

	   	   } /* end if 1st where field with value */
	   	   else if ((ilCounter >=2) && (ilIgnore == FALSE))
	   	   { /* add additional where field with value */
            strcpy( pclTmpStrg,pclSelection);

            sprintf(pclSelection, "%s AND %s='%s'",
				 	 pclTmpStrg,
			         rpDef.pcWhereFields[ilWCnt],
				  	 pclWhereValue);

	   	   } /* end else if 1st where field with value */

	   	   /* dbg (DEBUG, "<GLEHandleWhereClause> selection : <%s>", pclSelection);*/
		} /* end if match found */
		else
		{
	   	   dbg(DEBUG, "<GLEHandleWhereClause> cannot run with missing field name : <%s>", rpDef.pcWhereFields[ilWCnt]);
	   	   Terminate(0);
		} /* end else if match found */
      	   } /* end for scanning field positions */
	   /* data is ready to be send now */
	   sprintf(pclTmpStr,"%s,%s,STXHDL", rpSec.pcHomeAirport,
			rpSec.pcTableExtension);

	   dbg(DEBUG,"HandleWhereClause: Before SendCedaEvent");
	   dbg(DEBUG,"Selection<%s>\nFields<%s>\nData<%s>",
	              pclSelection,pclFieldList,pclDataList);
	   ilRC = SendCedaEvent(rpDef.iModID, 0, "STXHDL", "EXCO", "",
			pclTmpStr, rpDef.pcSndCmd, rpDef.pcTableName,
 			pclSelection, pclFieldList, pclDataList, "", 0, NETOUT_NO_ACK);

	   pclPtr = strtok(NULL, rpSec.pcEndOfLine);
      } /* end while sending records */
      if (ilRC != RC_SUCCESS)
      {
        dbg(TRACE, "<GLEHandleWhereClause> cannot run with (%d) from SendCedaEvent", ilRC);
        Terminate(0);
      }
   } /* WENT */

   free(pclData);

dbg(DEBUG, "<GLEHandleWhereClause> ilRC : (%d)", ilRC);
dbg(TRACE, "<GLEHandleWhereClause> ------------ END -----------------");

   return (ilRC);
} /* end of GLEHandleWhereClause */


/*****************************************************************************/
/*****************************************************************************/
/*								*/
/*        Function              HandleBaselFlukoData()			*/
/*								*/
/*			 RNE implemented BSLAI, BSLDI -cmd	*/
/*								*/
/*****************************************************************************/
/*****************************************************************************/
static int HandleBaselFlukoData(int ipSecNo, char *pcpData, char *pcpCommand,long lpFileLength)
{
	int         i;
	int         j;
	int         ilPL;
	int         ilRC;
	int         ilCnt;
	int         ilInc;
	int         ilStart;
	int         ilAlloc;
	int         ilTabNo;
	int         iIsBlock;
	int         ilLineNo;
	int         ilCurRow;
	int         ilNoBlanks;
	int         ilCurField;
	int         ilAnzFields;
	int         ilNoCurElems;
	int         ilFieldBufLength;
	int         ilOldDebugLevel;
	int         ilSysCnt;
	int         ilSyslibNo;
	int         ilTmpInt;
	int         ilDelLength;
	int         ilIgnore;
	int         ilCurIgnore;
	int         ilYear3;
	int         ilYear4;
	int         ilMonth3;
	int         ilMonth4;
	int         ilDay3;
	int         ilDay4;
	int         pilMemSteps[iTABLE_STEP];
	long         llLen;
	long         llPos;
	long         llClsPos;
	float         flTmpFloat;
	char         *pclS;
	char         *pclCfgPath;
	char         *pclTmpDataPtr;
	char         **pclDataArray;
	char         pclSyslibTable[iMIN];
	char         pclSyslibField[iMIN];
	char         pclSyslibData[iMAX_BUF_SIZE];
	char         pclCurrentTime[iMIN];
	char         pclFields[iMAX_BUF_SIZE];
	char         pclDataBuf[iMAXIMUM];
	char         pclFieldBuf[iMAXIMUM];
	char         pclSyslibResultBuf[iMAXIMUM];
	char         pclSyslibTableFields[iMAXIMUM];
	char         pclSyslibFieldData[iMAXIMUM];
	char         pclCurrentDate[iMIN];
	char         pclMonth3[iMIN];
	char         pclMonth4[iMIN];
	char         pclDay3[iMIN];
	char         pclDay4[iMIN];
	char         pclAlc3FldDat[5];
	long         llDataLength = 0;
	EVENT        *prlOutEvent    = NULL;
	BC_HEAD      *prlOutBCHead   = NULL;
	CMDBLK       *prlOutCmdblk   = NULL;

/* by RNE */

	int ilAddFieldLen = 0;
	char pclFct[] = "HandleBaselFlukoData";
	char pclAddFields[1000];
	char pclAddRowData[1000];

	dbg(TRACE,"<%s> ------------- START -------------",pclFct);

	/******* initialize something */
	ilTabNo       = iINITIAL;
	ilAlloc       = iINITIAL;
   	llPos         = 0;
  	ilRC          = RC_SUCCESS;
   	llDataLength  = 0;
   	memset((void*)pclCurrentTime, 0x00, iMIN);
   	memset((void*)pclDataBuf, 0x00, iMAXIMUM);
   	memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
   	for (i=0; i<iTABLE_STEP; i++)
      	pilMemSteps[i] = 1;


   /******* get timestamp */
   strcpy(pclCurrentTime, GetTimeStamp());

   /******* delete last separator */
   i = strlen(rgTM.prSection[ipSecNo].pcEndOfLine);
   while (!memcmp(&pcpData[lpFileLength-i], rgTM.prSection[ipSecNo].pcEndOfLine, i))
   {
	lpFileLength -= i;
	pcpData[lpFileLength] = 0x00;
   } /* end while */

   /******* count lines of this file */
   if (strlen(pcpData) > 0)
   {
      if ((ilLineNo = GetNoOfElements2(pcpData, rgTM.prSection[ipSecNo].pcEndOfLine)) < 0)
      {
         dbg(TRACE,"<%s> GetNoOfElements2(pcpData) returns: %d",pclFct, ilLineNo);
         Terminate(0);
      } /* end if   */
   } /* end if   */
   else
   {
      ilLineNo = 0;
   } /* end else */

   dbg(TRACE, "<%s> FOUND %d LINES", pclFct ,ilLineNo);
   rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows = ilLineNo;
   rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure = 0;
   rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows = 0;

   /******* more than 0 lines? */
   if (ilLineNo)
   {
      /* so get memory for 128 Tables, this should be enough */
      if ((pclDataArray = (char**)malloc(iTABLE_STEP*sizeof(char*))) == NULL)
      {
         dbg(TRACE,"<%s> can't allocte memory for DataArray",pclFct);
         pclDataArray = NULL;
         Terminate(0);
      } /* end if   */
   } /* end if   */
   else
      return RC_SUCCESS;

   /******* convert form ascii to ansi? */
   if (IsAscii(rgTM.prSection[ipSecNo].iCharacterSet))
   {
      dbg(TRACE,"<%s> start calling AsciiToAnsi...",pclFct);
      AsciiToAnsi((UCHAR*)pcpData);
      dbg(TRACE,"<%s> end calling AsciiToAnsi...",pclFct);
   } /* end if   */

   /******* set tmp pointer... */
   pclTmpDataPtr = pcpData;


   /*******************************/
   /******* FOR (ALL LINES)  */
   /*******************************/
   for (ilCurRow=0; ilCurRow<ilLineNo; ilCurRow++)
   {
      if (!(ilCurRow%15))
      {
         if (ilCurRow > 0)
         {
            dbg(TRACE,"<%s> working till line %d...",pclFct,ilCurRow);
         } /* end if   */
      } /* end if   */

      /******* get n.th line */
      memset((void*)pclDataBuf, 0x00, iMAXIMUM);
      if ((pclTmpDataPtr = CopyNextField2(pclTmpDataPtr, rgTM.prSection[ipSecNo].pcEndOfLine,pclDataBuf)) == NULL)
      {
         dbg(TRACE,"<%s> CopyNextField2 returns NULL",pclFct);
         Terminate(0);
      } /* end if   */

      /******* first of all, check data length, header, trailer ... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iIgnoreRecordsShorterThan)
      {
         dbg(TRACE,"<%s> Ignore: <%s> (too short)",pclFct, pclDataBuf);
         continue;
      } /* end if   */

      /******* is this header or trailer? */
      if (rgTM.prSection[ipSecNo].iHeader > 0)
      {
         if (ilCurRow < rgTM.prSection[ipSecNo].iHeader)
         {
       	    dbg(DEBUG,"<%s> ignore line: %d",pclFct, ilCurRow);
	          continue;
         } /* end if   */
      } /* end if   */

      if (rgTM.prSection[ipSecNo].iTrailer > 0)
      {
         if (ilCurRow > ilLineNo-rgTM.prSection[ipSecNo].iTrailer-1)
         {
	          dbg(DEBUG,"<%s> ignore line: %d",pclFct, ilCurRow);
	          continue;
         } /* end if   */
      } /* end if   */

      /******* buffer empty? */
      if (!strlen(pclDataBuf))
      {
         dbg(DEBUG,"<%s> found empty buffer...",pclFct);
         rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows++;
         continue;
      } /* end if   */

      /******* Block-Number is always 0 */
      ilTabNo = 0;

      /******* get memory for data array */
      if (ilAlloc == iINITIAL)
      {
         /* set flag */
         ilAlloc = 1;
         /* now get memory for data of this table... */
         dbg(TRACE,"<%s> ALLOCATE %ld bytes for pclDataArray[%d]",pclFct,iMEMORY_STEP*sizeof(char), ilTabNo);
         if ((pclDataArray[ilTabNo] = (char*)malloc(iMEMORY_STEP * sizeof(char))) == NULL)
         {
            dbg(TRACE,"<%s> Can't allocate data memory for table: %d",pclFct,
                       ilTabNo);
            pclDataArray[ilTabNo] = NULL;
            Terminate(0);
         } /* end if   */
         /* clear this memory */
         memset((void*)pclDataArray[ilTabNo],
                0x00, (size_t)(iMEMORY_STEP*sizeof(char)));
      } /* end if   */

      /******* check length of data buffer, only for filetype = FIX... */
      if ((int)strlen(pclDataBuf) < rgTM.prSection[ipSecNo].iSULength)
      {
         ilNoBlanks= rgTM.prSection[ipSecNo].iSULength-strlen(pclDataBuf);
         memset((void*)&pclDataBuf[strlen(pclDataBuf)], 0x20, ilNoBlanks);
         pclDataBuf[rgTM.prSection[ipSecNo].iSULength] = 0x00;
      } /* end if   */

      /******* is this file with or without fieldnames? */
      ilAnzFields = rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfFields;
      ilInc         = 1;
      ilStart       = 0;

      /******* file is FIX-type */

      /************************************/
      /******* FOR all fields, get data */
      /************************************/
      pclS = pclDataBuf;
      for (ilCurField=ilStart,ilCnt=0;ilCurField<ilAnzFields;ilCurField+=ilInc,ilCnt++)
      {
         /******* first and first+2... is always fieldname */
         if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
         {
            if (ilStart)
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField-1];
         }   /* end if   */

         /*******  ignore this field?, in this case we continue... */
         if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ilTabNo].pcFields[ilCnt],"FILL_FIELD"))
         {
            /******* set pointer to next valid position, jump over fill_field */
            pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];

            /******* delete last comma after the fill_field */
            if (ilCurField == ilAnzFields-1)
            {
               if(pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] == ',')
               {
                  pclDataArray[ilTabNo][strlen(pclDataArray[ilTabNo])-1] = 0x00;
                  --llPos;
               } /* end if   */
            } /* end if   */
         } /* end if   */

         else
         {
            /******* NO FILL_FIELD - we cannot ignore this field */

            /******* get n.th field */
            memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               strncpy(pclFieldBuf, pclS,rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField]);
            } /* end if   */

            /******* store n.th field */
            /* we don't like empty strings... */
            if (!strlen(pclFieldBuf))
                strcpy(pclFieldBuf, " ");
	          dbg(DEBUG,"<%s> CONTENT OF pclFieldBuf: <%s>",pclFct,pclFieldBuf);

	          ilRC = RNECreateRow(pclFieldBuf, pclAddRowData, pclAddFields, pcpCommand, ipSecNo, ilTabNo, ilCnt);

            ilFieldBufLength = strlen(pclFieldBuf);
            llDataLength += (long)ilFieldBufLength+1;  /* separator also */

            /******* enough memory to store data? */
            if (llDataLength >=
                iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
            {
               /******* realloc here */
               pilMemSteps[ilTabNo]++;
               dbg(TRACE,"<%s> REALLOC now %ld bytes...",pclFct,
                    pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));

               if ((pclDataArray[ilTabNo] = (char*)realloc(pclDataArray[ilTabNo],
                   pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char)))
                     == NULL)
               {
                  dbg(TRACE,"<%s> Cannot realloc for table %d",pclFct, ilTabNo);
                  Terminate(0);
               } /* end if   */

               /******* clear new buffer */
               llClsPos = llDataLength - (long)ilFieldBufLength;
               dbg(DEBUG,"clear from pos: %ld", llClsPos);
               memset((void*)&pclDataArray[ilTabNo][llClsPos],
                       0x00, iMEMORY_STEP*sizeof(char));
            } /* end if   */

            /******* copy data to dataline (enough allocated) */
            memcpy((void*)&pclDataArray[ilTabNo][llPos],(const void*)pclFieldBuf, (size_t)ilFieldBufLength);
            llPos += (long)ilFieldBufLength;

            /******* add separator at end of data */
            if (ilCurField < ilAnzFields-1)
            {
               memcpy((void*)&pclDataArray[ilTabNo][llPos],
                      (const void*)",", (size_t)1);
               llPos++;
            } /* end if   */

            if (rgTM.prSection[ipSecNo].prTabDef[ilTabNo].iNoOfDataLength > 0)
            {
               /******* set to next position */
               pclS += rgTM.prSection[ipSecNo].prTabDef[ilTabNo].piDataLength[ilCurField];
            } /* end if   */

         }  /* END ELSE: no FILL_FIELD */

      }  /* END FOR: all fields */

      dbg(DEBUG,"pclAddRowData: <%s>", pclAddRowData);

      ilAddFieldLen = strlen(pclAddRowData);
      llDataLength += (long)ilAddFieldLen+1;  /* separator also */

      /******* enough memory to store data? */
     if (llDataLength >=
        iMEMORY_STEP*sizeof(char)*pilMemSteps[ilTabNo])
     {
      /******* realloc here */
      pilMemSteps[ilTabNo]++;
      dbg(TRACE,"<%s> REALLOC now %ld bytes...",pclFct,
              pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char));

        if ((pclDataArray[ilTabNo] = (char*)realloc(pclDataArray[ilTabNo],
          pilMemSteps[ilTabNo]*iMEMORY_STEP*sizeof(char)))
           == NULL)
        {
            dbg(TRACE,"<%s> Cannot realloc for table %d",pclFct, ilTabNo);
            Terminate(0);
        } /* end if   */

         /******* clear new buffer */
         llClsPos = llDataLength - (long)ilFieldBufLength;
        dbg(DEBUG,"clear from pos: %ld", llClsPos);
        memset((void*)&pclDataArray[ilTabNo][llClsPos],
        0x00, iMEMORY_STEP*sizeof(char));
     } /* end if   */

     /******* copy data to dataline (enough allocated) */
    memcpy(&(pclDataArray[ilTabNo][llPos]),pclAddRowData, ilAddFieldLen);
    llPos += (long)ilAddFieldLen;

    strcpy(pclAddRowData,"");

    /******* add '\n' as row separator */
    memcpy((void*)&pclDataArray[ilTabNo][llPos],(const void*)"\n", (size_t)1);
    llPos++;

   }  /* END FOR: all rows */

   dbg(TRACE, "<%s> STEP THROUGH LINES ENDS HERE",pclFct);

   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);
   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);
   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);
   dbg(TRACE,"<%s> Number of lines: %d",pclFct, rgTM.prSection[ipSecNo].rlMyStat.iNoAllRows);
   dbg(TRACE,"<%s> Number of SHM-Failure: %d",pclFct, rgTM.prSection[ipSecNo].rlMyStat.iNoRowWithSHMFailure);
   dbg(TRACE,"<%s> Number of empty lines: %d",pclFct, rgTM.prSection[ipSecNo].rlMyStat.iNoEmptyRows);
   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);
   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);
   dbg(TRACE,"<%s> --------------------------------------------------",pclFct);


   /******* write it to database, for all used tables */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      /******* delete last \n at end of data... */
      if (pclDataArray[i][strlen(pclDataArray[i])-1] == 0x0A)
         pclDataArray[i][strlen(pclDataArray[i])-1] = 0x00;

      /******* build FIELD-LIST with comma separator... */
      memset((void*)pclFields, 0x00, iMAX_BUF_SIZE);
      for (j=0; j<rgTM.prSection[ipSecNo].prTabDef[i].iNoOfFields; j++)
      {
         /******* don't use FILL_FIELDS... */
         if (strcmp(rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j], "FILL_FIELD"))
         {
            strcat(pclFields, rgTM.prSection[ipSecNo].prTabDef[i].pcFields[j]);
            strcat(pclFields, ",");
         } /* end if */
      }  /* end for fill_fields */

      /******* set end of fieldlist... */
      while (pclFields[strlen(pclFields)-1] == ',')
         pclFields[strlen(pclFields)-1] = 0x00;

      strcat(pclFields, pclAddFields);

      /******* calculate size of memory we need */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
         llLen = strlen(rgTM.prSection[ipSecNo].pcSystemKnz);
      else
         llLen = 0;

      llLen += sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + strlen(pclFields) + strlen(pclDataArray[i]) + 3;

      /******* get memory for out event */
      if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
      {
         dbg(TRACE,"<%s> not enough memory for OutEvent!!",pclFct);
         prlOutEvent = NULL;
         Terminate(0);
      } /* end if */

      /******* clear memory */
      memset((void*)prlOutEvent, 0x00, llLen);

      /******* set structure members */
      prlOutEvent->type            	= SYS_EVENT;
      prlOutEvent->command	= EVENT_DATA;
      prlOutEvent->originator      	= mod_id;
      prlOutEvent->retry_count   	= 0;
      prlOutEvent->data_offset   	= sizeof(EVENT);
      prlOutEvent->data_length   	= llLen - sizeof(EVENT);

      /******* BC-Head Structure */
      prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
      prlOutBCHead->rc = RC_SUCCESS;
      strcpy(prlOutBCHead->dest_name, "STXHDL");
      strcpy(prlOutBCHead->recv_name, "EXCO");

      /******* command block structure */
      prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
      strcpy(prlOutCmdblk->command,
             rgTM.prSection[ipSecNo].prTabDef[i].pcSndCmd);
      strcpy(prlOutCmdblk->obj_name,
             rgTM.prSection[ipSecNo].prTabDef[i].pcTableName);
      sprintf(prlOutCmdblk->tw_end, "%s,%s,STXHDL",
              rgTM.prSection[ipSecNo].pcHomeAirport,
              rgTM.prSection[ipSecNo].pcTableExtension);

      /******* selection */
      /* we write system sign to selection (for GSA) */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy(prlOutCmdblk->data, rgTM.prSection[ipSecNo].pcSystemKnz);
      } /* end if */

      /* fields */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
                 strlen(rgTM.prSection[ipSecNo].pcSystemKnz)+1), pclFields);
      }  /* end if */
      else
         strcpy((prlOutCmdblk->data+1), pclFields);

      /* data */
      if (rgTM.prSection[ipSecNo].iUseSystemKnz)
      {
         strcpy((prlOutCmdblk->data+
               strlen(rgTM.prSection[ipSecNo].pcSystemKnz) +
               strlen(pclFields)+2), pclDataArray[i]);
      }  /* end if */
      else
         strcpy((prlOutCmdblk->data+strlen(pclFields)+2), pclDataArray[i]);

      /******* send message */
      if (rgTM.prSection[ipSecNo].prTabDef[i].iModID > 0)
      {
         dbg(TRACE,"<%s> sending to mod_id %d",pclFct,rgTM.prSection[ipSecNo].prTabDef[i].iModID);
         if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].prTabDef[i].iModID, mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<%s> QuePut returns with: %d",pclFct, ilRC);
            Terminate(0);
         }  /* end if */
         dbg(TRACE,"<%s> QuePut returns with: %d",pclFct, ilRC);
      }  /* end if */
      else
      {
         dbg(TRACE,"<%s> sending to router %d",pclFct, igRouterID);
         if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4,
                         llLen, (char*)prlOutEvent)) != RC_SUCCESS)
         {
            dbg(TRACE,"<%s> QuePut returns with: %d",pclFct, ilRC);
            Terminate(0);
         }  /* end if */
         dbg(TRACE,"<%s> QuePut returns with: %d",pclFct, ilRC);
      }  /* end else */

      /******* free memory... */
      if (prlOutEvent != NULL)
      {
         free((void*)prlOutEvent);
         prlOutEvent = NULL;
      } /* end if */
   } /* end for (all used tables) */

   /******* delete memory we don't need */
   for (i=0; i<rgTM.prSection[ipSecNo].iNoOfTables; i++)
   {
      dbg(DEBUG,"<%s> delete pclDataArray[%d]",pclFct, i);
      free((void*)pclDataArray[i]);
   }  /* end for */
   dbg(DEBUG,"<%s> delete pclDataArray",pclFct);
   free((void*)pclDataArray);

   free(pclAddRowData);

   free(pclAddFields);

   /******** only for debug... */
   dbg(TRACE,"<%s> ---------------- END ----------------",pclFct);
   return RC_SUCCESS;

} /* end of HandleBaselFlukoData by RNE */



/*****************************************************************************/
/*****************************************************************************/
/*								*/
/*        Function              RNECreateRow()			*/
/*								*/
/*	IN:		char *pcpFieldBuf - field content		*/
/*			char *pcpCommand - Act. Command    	*/
/*			int ipSecNo - Section number		*/
/*			int ipTabNo - Tab number		*/
/*			int ipCnt - counter for-loop		*/
/*								*/
/*	OUT:		char *pcpAddRowData			*/
/*			char *pcpAddFields  			*/
/*								*/
/*	History		 20.01.2000 RNE written			*/
/*								*/
/*****************************************************************************/
/*****************************************************************************/
static int RNECreateRow(char *pcpFieldBuf, char *pcpAddRowData,
			char *pcpAddFields, char *pcpCommand,
			int ipSecNo, int ipTabNo, int ipCnt)
{
  const int iTRUE = 1;
  const int iFALSE = 0;

  const char ARR_CMD[7] = "BSLAI";
  const char DEP_CMD[7] = "BSLDI";
  const char ARR_FIELDS[] = ",STOAA,ALC3A,STYPA,VIA1A,VIA2A,VIA3A";
  const char DEP_FIELDS[] = ",STODD,ALC3D,STYPD,VIA1D,VIA2D,VIA3D";
  const char ARR_FLD_NAMES[] = "A_DATE,ALC?A,NATURE,VIA_1,VIA_2,VIA_3,A_TIME";
  const char DEP_FLD_NAMES[] = "D_DATE,ALC?D,NATURE,VIA_1,VIA_2,VIA_3,D_TIME";

  int ilFound;
  int ilLoopCount = 0;
  int ilNoOfElements = 0;
  int ilRC = RC_SUCCESS;
  char pclFct[] = "RNECreateRow";
  char pclAlc3FldDat[4];
  char *pclActualFlds;
  char *pclActualFldNames;

  static char pclDate[9];
  static char pclTime1[15];
  static char pclTime2[5];
  static char pclALC3Buf[4];
  static char pclNatureBuf[3];
  static char pclVi1TmpBuf[4];
  static char pclVi2TmpBuf[4];
  static char pclVi3TmpBuf[4];

  char pclRowBuf[100];

  ilFound = iFALSE;

  pclRowBuf[0] = '\0';

  dbg(DEBUG,"---- <%s> ---- ENTERING FUNCTION ----",pclFct);
  DeleteCharacterInString(pcpFieldBuf, ' ');


  /****** checking received command */
  if(strcmp(pcpCommand,ARR_CMD))
  {  /* pcpCommand is NOT ArrivalCmd */
     if(strcmp(pcpCommand,DEP_CMD))
     {    /* pcpCommand is NOT DepartureCmd */
          dbg(TRACE,"! -- <%s> UNKNOWN Command: <%s> ",pclFct,pcpCommand);
          Terminate(0);
     }
     else
     {  /* pcpCommand IS DepartureCmd */
        pclActualFldNames = (char *)DEP_FLD_NAMES;
        pclActualFlds = (char *)DEP_FIELDS;
     }
  }  /* end if */
  else
  {  /* pcpCommand IS ArrivalCmd */
     pclActualFldNames = (char *)ARR_FLD_NAMES;
     pclActualFlds = (char *)ARR_FIELDS;
  }  /* end else */

  ilNoOfElements = (GetNoOfElements(pclActualFldNames, ','));

  strcpy(pcpAddFields,pclActualFlds);

  /******* comparing fieldnames with fieldlist */
  while((ilLoopCount <= ilNoOfElements) && (ilFound == iFALSE))
  {
     if (!strcmp(rgTM.prSection[ipSecNo].prTabDef[ipTabNo].pcFields[ipCnt],
			GetDataField(pclActualFldNames, ilLoopCount , ',')))
      {
        ilFound = iTRUE;

        switch (ilLoopCount)
        {
	case 0: /******* field name: <A_DATE> OR <D_DATE> */
		pclDate[0] = '\0';
		strcpy(pclDate, pcpFieldBuf);
		break;
	case 1: /******* field <ALC??> */
		pclALC3Buf[0] = '\0';
		Get3LCAirline(ipSecNo, pcpFieldBuf, pclAlc3FldDat);
		if (pclAlc3FldDat[0] != '\0')
		{
	    strcpy(pclALC3Buf, pclAlc3FldDat);
	  } /* end if   */
		else
    {
	    strcpy(pclALC3Buf,pcpFieldBuf);
    }
		break;
	case 2:/******* field name: <NATURE> */
		pclNatureBuf[0] = '\0';
		ilRC = RNEDecodeNature(pcpFieldBuf, pclNatureBuf);
		break;
	case 3:/******* field name: <VIA_1> */
		pclVi1TmpBuf[0] = '\0';
		strcpy(pclVi1TmpBuf,pcpFieldBuf);
		break;
	case 4:/******* field name: <VIA_2> */
		pclVi2TmpBuf[0] = '\0';
		strcpy(pclVi2TmpBuf,pcpFieldBuf);
		break;
	case 5:/******* field name: <VIA_3> */
		pclVi3TmpBuf[0] = '\0';
		strcpy(pclVi3TmpBuf,pcpFieldBuf);
		break;
	case 6:/******* field name: <A_TIME> OR <D_TIME> Time at home */
		sprintf(pclTime1,"%s%s00",pclDate,pcpFieldBuf);
		break;
	case 7:/******* field name: <D_TIME> OR <A_TIME> Time at other airp */
		strcpy(pclTime2,pcpFieldBuf);
		break;
	default: 	break;
         } /* end switch */
       }/* end if */
      ilLoopCount++;
    } /* end while */

  strcat(pclRowBuf,",");

  if (pclTime1[0] != '\0')
  {
     strcat(pclRowBuf,pclTime1);
  }

  strcat(pclRowBuf,",");

  if (pclALC3Buf[0] != '\0')
  {
     strcat(pclRowBuf,pclALC3Buf);
  }

  strcat(pclRowBuf,",");

  if (pclNatureBuf[0] != '\0')
  {
     strcat(pclRowBuf,pclNatureBuf);
  }

  /******* checking VIA-Fields */
  if (pclVi1TmpBuf[0] != '\0')
  /* VIA1 Found */
  {
     if (pclVi2TmpBuf[0] != '\0')
     /* VIA2 Found */
     {
        if (pclVi3TmpBuf[0] != '\0')
        /* VIA3 Found */
        {
          strcat(pclRowBuf,",");
          strcat(pclRowBuf,pclVi3TmpBuf);
          strcat(pclRowBuf,",");
          strcat(pclRowBuf,pclVi2TmpBuf);
          strcat(pclRowBuf,",");
          strcat(pclRowBuf,pclVi1TmpBuf);
        }
        else
        { /* no VIA3 found */
          strcat(pclRowBuf,",");
          strcat(pclRowBuf,pclVi2TmpBuf);
          strcat(pclRowBuf,",");
          strcat(pclRowBuf,pclVi1TmpBuf);
          strcat(pclRowBuf,",");
        }
     }
     else
     { /* no VIA2 found */
       strcat(pclRowBuf,",");
       strcat(pclRowBuf,pclVi1TmpBuf);
       strcat(pclRowBuf,",,");
     }
  }
  else /* no VIA found */
  {
     strcat(pclRowBuf,",,,");
  }   /* end else */

  strcat(pclRowBuf,",");
  if (pclTime2[0] != '\0')
  {
    strcat(pclRowBuf,pclTime2);
  }

  strcpy(pcpAddRowData, pclRowBuf);

  dbg (DEBUG, "---- <%s> ---- LEAVING FUNCTION, ilRC:<%d> ----",pclFct,ilRC);
  return(ilRC);
}  /* end of RNECreateRow */



/************************************************************************/
/* Function:   RNEDecodeNature()                                        */
/************************************************************************/
/* Parameter:                                                                      */
/* In  :                                                                                */
/*     char  *pcpNatureStrg - contains string:                          */
/*                              nature of flight in short form                  */
/* Out  :                                                                              */
/*     char  *pcpTTYP - points to TTYP-Number                       */
/*                                                                                        */
/* Global Variables: none                                                      */
/*                                                                                       */
/* Returncode: ilRC = RC_SUCCESS when ok                      */
/*                                                                                       */
/*                                                                                       */
/* History:                                                                           */
/*           12.01.2000 RNE  written                                         */
/*                                                                                        */
/*************************************************************************/
static int RNEDecodeNature(char *pcpNatureStrg, char *pcpTTYP)
{
  const int iTRUE = 1;
  const int iFALSE = 0;

  const char NATURE_STRINGS[] = "CGO,CHT,DEM,F-S,FRY,GAV,JNT,MIX,NTR,PAX,SPE,SRF,TFL,TRG,TRL,TRN,VIP,XTR,NTF";

  const char TTYP_NUMBERS[] = "15,36,86,60,45,21,46,47,48,11,58,61,87,82,62,63,94,13,48";

  /* RELATIONS of ttyp-numbers to natures of flight (next comment-line): */
  /* CGO = 15;CHT = 36;DEM = 86;F-S = 60;FRY = 45;GAV = 21;JNT = 46;MIX = 47;
     NTR = 48;PAX = 11;SPE = 58;SRF = 61;TFL = 87;TRG = 82;TRL = 62;TRN = 63;
     VIP = 94;XTR = 13;NTF = 48                     */

  int  ilRC = RC_SUCCESS;
  int  ilItem = 0;
  int  ilFound;
  char pclNature[10];
  char pclFct[] = "RNEDecodeNature";
  char *pclFld = NULL;
  char pclFldName[1000];
  char *pclNum = NULL;
  char pclNatures[1000];

  strcpy(pclNatures,"CGO,CHT,DEM,F-S,FRY,GAV,JNT,MIX,NTR,PAX,SPE,SRF,TFL,TRG,TRL,TRN,VIP,XTR,NTF");

  pclFld = pclNatures;

  ilFound = iFALSE;

  dbg (DEBUG, "---- <%s> ---- ENTERING FUNCTION ----",pclFct);

  while ((*pclFld != '\0')&&(ilFound == iFALSE)&&(ilItem < 25))
  {
     dbg (DEBUG, "---- <%s> while - LOOP: <%d> ----",pclFct,ilItem);

     pclFld = TWECopyNextField(pclFld,',', pclFldName);

     if ((strcmp(pclFldName, pcpNatureStrg))==0)
     {
        /* found */
        ilFound = TRUE;
        pclNum = GetDataField((char *)TTYP_NUMBERS, ilItem, ',');
        strcpy(pcpTTYP, pclNum);
     }  /* end if */
     ilItem++;
  }  /* end while */

  if (ilFound == FALSE)
  {
    dbg (TRACE, "! -- <%s> STRING NOT FOUND: <%s> ----",pclFct,pcpNatureStrg);
    ilRC = RC_FAIL;
  } /* end if */

  dbg (DEBUG, "---- <%s> ---- LEAVING FUNCTION, ilRC:<%d> ----",pclFct,ilRC);
  return ilRC;
} /* end of RNEDecodeNature */



/*****************************************************************************
* Function: TWECopyNextField
* Parameter:  s1      pointer on field list
*             c       delimiter
*             s3      fieldname
* Description: Function steps with each call one field forward in fieldlist.
*              Fields must be delimited by c. Fieldcontent is returned in s3.
******************************************************************************/
static char *TWECopyNextField(char *s1, char c, char *s3)
{
	UINT		i;

	/* check params */
	if (s1 == NULL || s3 == NULL)
		return NULL;

	/* init */
	i = 0;

	/* copy to buffer */
	while (*s1 != c && *s1)
		s3[i++] = *s1++;
	s3[i] = 0x00;
/* TWE */
  if (*s1 != '\0')
	s1++;

	/* delete all blanks at end of string... */
	while (s3[strlen((char*)s3)-1] == cBLANK)
		s3[strlen((char*)s3)-1] = 0x00;

	/* bye bye */
	return *s1 == 0x00 ? NULL : s1;

} /*  TWECopyNextField  */

/* **************************************************
 * Function:    TWECheckPerformance
 * Parameter:   IN : ipFlag    begin or end of measure
 * Return:      none
 * Description:
 **************************************************** */
static void TWECheckPerformance (int ipFlag)
{
static time_t  tlBeginStamp = 0;
time_t         tlEndStamp   = 0;
time_t         tlStampDiff = 0;

  if (ipFlag == TRUE)
  {
     dbg(TRACE,"---- START TIME MEASURE");
     tlBeginStamp = time(0L);
  } /* end if */
  else
  {
     tlEndStamp = time(0L);
     tlStampDiff = tlEndStamp - tlBeginStamp;
     dbg(TRACE,"---- END TIME MEASURE (needed %d SEC)",tlStampDiff);
  } /* end else */
  return;
} /* end TWECheckPerformance */

/******************************************************************************/
/******************************************************************************/
static void SetFieldValue(int ipFlag, char *pcpFields,char *pcpData,char *pcpSetFld,char *pcpSetDat)
{
  int ilFldNbr = 0;
  int ilFldCnt = 0;
  int ilFld = 0;
  int ilFldPos = 0;
  int ilDatPos = 0;
  int ilSetField = TRUE;
  char pclFldNam[16];
  char pclFldDat[2048];
  char pclOldFld[4096];
  char pclOldDat[524288]; /* 512K */

  ilFldNbr = get_item_no(pcpFields,pcpSetFld,5) + 1;
  if (ilFldNbr > 0)
  {
    strcpy(pclOldFld,pcpFields);
    strcpy(pclOldDat,pcpData);
    ilFldCnt = field_count(pcpFields);
    for (ilFld = 1; ilFld <= ilFldCnt; ilFld++)
    {
      ilSetField = TRUE;
      get_real_item(pclFldNam,pclOldFld,ilFld);
      get_real_item(pclFldDat,pclOldDat,-ilFld);
      if (strcmp(pclFldNam,pcpSetFld) == 0)
      {
	strcpy(pclFldDat,pcpSetDat);
	ilSetField = ipFlag;
      } /* end if */
      if (ilSetField == TRUE)
      {
        StrgPutStrg(pcpFields,&ilFldPos,pclFldNam,0,-1,",");
        StrgPutStrg(pcpData,&ilDatPos,pclFldDat,0,-1,",");
      } /* end if */
    } /* end for */
    if (ilFldPos > 0)
    {
      ilFldPos--;
      ilDatPos--;
    } /* end if */
    pcpFields[ilFldPos] = 0x00;
    pcpData[ilDatPos] = 0x00;
  } /* end if */
  else
  {
    if (ipFlag == TRUE)
    {
      strcat(pcpFields,",");
      strcat(pcpFields,pcpSetFld);
      strcat(pcpData,",");
      strcat(pcpData,pcpSetDat);
    } /* end if */
  } /* end else */
  return;
} /* end SetFieldValue */

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
		ilLen = get_real_item(pcgHomeAP, pcpTwEnd, 1);
		if (strlen(pcgHomeAP) == 0 || ilLen < 3)
		{
		    dbg(TRACE, "%s: Extracting pcpTwEnd = <%s> for received hopo fails or length < 3",pclFunc, pcpTwEnd);
		    ilRc = RC_FAIL;
		}
		else
		{
		    strcat(pcgHomeAP,",");
		    sprintf(pclTmp,"%s,",pcpHopo_sgstab);

            if  ( strstr(pclTmp, pcgHomeAP) == 0 )
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is not in SGS.TAB HOPO<%s>",
                           pclFunc, pcgHomeAP, pclTmp);
                ilRc = RC_FAIL;
            }
            else
            {
                dbg(TRACE, "%s: Received HOPO = <%s> is in SGS.TAB HOPO<%s>",
                           pclFunc, pcgHomeAP, pclTmp);
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
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  */


