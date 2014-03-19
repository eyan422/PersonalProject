#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/fileif.c 1.20a 2014/03/19 16:02:59CEST fya Exp $";
#endif /* _DEF_mks_version */
/*****************************************************************************	*/
/*                                                                            	*/
/* CCS Program Skeleton                                                       	*/
/*                                                                            	*/
/* Author         :                                                           	*/
/* Date           :                                                           	*/
/* Description    :                                                           	*/
/*                                                                            	*/
/* Update history :                                                           	*/
/*			mos 8 Apr 00 adjusted some functionalitites	                          	*/
/*				VIA handling is now set to sql selection from	                        */
/*				VIA3 or VIA4 field directly.			                                    */
/*				BE SURE TO COMPILE WITH DBMAKE!			                                  */
/*										                                                          */
/*				Because of faulty time formats in the WAW	*/
/*				database, the format will be corrected to 	*/
/*				char(14).					*/
/*										*/
/*				The DUBAI version has got hardcoded UTC+4h time	*/
/*				conversion (map_utc_to_local didn't work there	*/
/*				If you are not in the sunshine state, then check*/
/*				for the correct fileif version			*/
/*				(search for AddToCurrentUtcTime(**,,)		*/
/*          rkl 11.08.00 Insert Config-Parameter use_2400 <YES/NO> for 		*/
/*                       Time 24:00 instead of 23:59 or 00:00 (SAP need this )	*/
/*                                                                            	*/
/*										*/
/*			mos 5 Oct 00	added Multihopo fct.			*/
/*					home_airport config entry is required	*/
/*					for each section			*/
/*					tw_end with 4. field "TRUE"		*/
/*					so that SQLHDL adds the provided hopo	*/
/*					to the sql statement			*/
/*										*/
/*																				*/
/*			mos 30 Mar 01	IDS functionality (file creation with .			*/
/*					number circle value from Numtab as extension	*/
/*					ftp timeout and rename remote file fct.			*/
/*										*/
/*										*/
/*			mos 20 June 01 extended memory for command string		*/
/*										*/
/*			JWE 15 May 02 removed garbage from MOS 8 Apr 00. Via handling works now as it should do */
/*										*/
/*			JWE 19 May 03 extendend with some features for SIN SAP-SD,SMC interfaces */
/*			              creating generic functions and beautified some output */
/*			JWE 21 May 03 finished first revised version for SIN:SAP-SD,SMC interfaces */
/*			JWE 27 May 03 fixed bug to have continous linenumbers if they shall be printed inside the*/
/*                    file as #DATALINENUMBER*/
/*			JWE 08 FEB 05 PRF 6998 & PRF 6999 fixe                                                   */
/*          FYA 19 APR 2014 UFIS-4846 */
/******************************************************************************/
/* This program is a CCS main program 						*/
/*                                                                            */
static char sccs_version[] ="%Z% UFIS 4.4 (c) ABB AAT/I fileif.c %M% %I% / %E% %U% / JWE";
/* source-code-control-system version string                                  */
/* be carefule with strftime or similar functions !!!                         */
/******************************************************************************/


#define	U_MAIN
#define	UGCCS_PRG
#define 	STH_USE
#define 	__SHOW_PASSWD

/* The master header file */
#include  "fileif.h"

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE			*outp       = NULL;*/
int  			debug_level = TRACE;

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */

/******************************************************************************/
/* my private Global variables                                                */
/******************************************************************************/
static int		igRouterID = iINITIALIZE;
static int		igSignalNo = 0;
static int    igUse2400 = 0;   /* = 1; use of 2400 instead of 2359 or 0000 */
static char		pcgCfgFile[iMIN_BUF_SIZE];
static char		pcgTABEnd[iMIN];
static char		pcgHomeAP[iMIN];
static char		pcgTmpTabName[iMIN];
static TFMain	rgTM;
static int    igCurrDataLines = 0;
static int    igTotalLineCount = 0;
static int    igStartupMode = 0;
static int    igRuntimeMode = 0;

/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int  init_que(void);                      /* Attach to CCS queues */
extern int  que(int,int,int,int,int,char*);      /* CCS queuing routine  */
extern int  send_message(int,int,int,int,char*); /* CCS msg-handler i/f  */
extern int	snap(char*, long, FILE*);
extern void	str_trm_all(char *, char *, int);
extern int	SendRemoteShutdown(int);
extern int 	init_db(void);
extern int 	logoff(void);

/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
static int 	init_fileif(void);
static int	Reset(void);                       /* Reset program          */
static void	Terminate(UINT);                   /* Terminate program      */
static void	HandleSignal(int);                 /* Handles signals        */
static void	HandleErr(int);                    /* Handles general errors */
static void	HandleQueErr(int);                 /* Handles queuing errors */
static int	HandleData(void);                  /* Handles event data     */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

/******************************************************************************/
/* my private Function prototypes                                             */
/******************************************************************************/
static int 	GetCmdNumber(char *);
static int 	OpenFile(int, int);
static int 	WriteDataToFile(int, int, int *, char *, char *,int ipRecordNr);
static int 	SendFileToMachine(int, int);
static int 	WriteEOB(int);
static int 	GetFileContents(char *, long *, char **);
static int 	UtcToLocal(char *);
static void 	TrimRight(char *pcpBuffer);

/******* Changes by MOS 28 June 2000 for FTPHDL implementation *****/
static int 	SendEvent(char *pcpCmd,char *pcpSelection,char *pcpFields,char *pcpData,
			char *pcpAddStruct,int ipAddLen,int ipModID,int ipPriority,
			char *pcpTable,char *pcpType,char *pcpFile, char *pcptw_startvalue, int ipCmdNo);
static 		FTPConfig prgFtp;			/* struct for dynamic configuration of FTPHDL */
static char 	pcgProcessName[20];      	/* buffer for process name of htmhdl*/
static int 	igModID_Ftphdl  = 0;		/* MOD-ID of Router  */
/*******EO Changes *********/
static int ReplaceStdTokens(int ipCmd,char *pcpLine,char *pcpDynData,char *pcpDateFormat,char *pcpTimeFormat,int ipLineNumber);
static int FormatDate(char *pcpOldDate,char *pcpFormat,char *pcpNewDate);
static void GetLogFileMode(int *ipModeLevel, char *pcpLine, char *pcpDefault);
static void GetSeqNumber(char *pcpSeq,char *pcpKey,char *pcpSeqLen,char *pcpResetDay,char* pcpStartValue);

/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
	int	ilRC = RC_SUCCESS;
	int	ilCnt = 0;
	char	pclFileName[512];

	/* set errno to zero */
	errno = 0;

	/* General initialization	*/
	INITIALIZE;

	/* for signal handling */
	(void)SetSignals(HandleSignal);
	(void)UnsetSignals();

	/* copy process-name to global buffer */
	memset(pcgProcessName,0x00,sizeof(pcgProcessName));
	strcpy(pcgProcessName, argv[0]);

	dbg(TRACE,"<MAIN> <%s>",mks_version);
	memset(&rgTM,0x00,sizeof(TFMain));

	/* read tableend from SGS.TAB */
	memset((void*)pcgTABEnd, 0x00, iMIN);
	if ((ilRC = tool_search_exco_data("ALL", "TABEND", pcgTABEnd)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> cannot find entry \"TABEND\" in SGS.TAB");
		Terminate(30);
	}
	dbg(DEBUG,"<MAIN> found TABEND: <%s>", pcgTABEnd);

	/* read HomeAirPort from SGS.TAB */
	memset((void*)pcgHomeAP, 0x00, iMIN);
	if ((ilRC = tool_search_exco_data("SYS", "HOMEAP", pcgHomeAP)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> cannot find entry \"HOMEAP\" in SGS.TAB");
		Terminate(30);
	}
	dbg(DEBUG,"<MAIN> found HOME-AIRPORT: <%s>", pcgHomeAP);

	/* Attach to the CCS queues */
	do
	{
		ilRC = init_que();
		if(ilRC != RC_SUCCESS)
		{
			sleep(6);
			ilCnt++;
		}
	} while((ilCnt < 10) && (ilRC != RC_SUCCESS));

	if(ilRC != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> init_que failed!");
		Terminate(30);
	}

	/* transfer binary file to remote machine... */
	pclFileName[0] = 0x00;
	sprintf(pclFileName, "%s/%s", getenv("BIN_PATH"), argv[0]);
	dbg(TRACE,"<MAIN> TransferFile: <%s>", pclFileName);
	if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
		set_system_state(HSB_STANDALONE);
	}

	/* transfer configuration file to remote machine... */
	pclFileName[0] = 0x00;
	sprintf(pclFileName, "%s/%s.cfg", getenv("CFG_PATH"), argv[0]);
	dbg(TRACE,"<MAIN> TransferFile: <%s>", pclFileName);
	if ((ilRC = TransferFile(pclFileName)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> TransferFile <%s> returns: %d", pclFileName, ilRC);
		set_system_state(HSB_STANDALONE);
	}

	/* ask VBL about this!! */
	dbg(TRACE,"<MAIN> SendRemoteShutdown: %d", mod_id);
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

  /*GetLogFileMode(&igStartupMode,"STARTUP_MODE","OFF");
  debug_level = igStartupMode;
  GetLogFileMode(&igRuntimeMode,"RUNTIME_MODE","OFF");*/

	if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
	{
		dbg(DEBUG,"<MAIN> initializing ...");
		if ((ilRC = init_fileif()) != RC_SUCCESS)
		{
			dbg(TRACE,"<MAIN> init_fileif() returns: %d\n", ilRC);
			Terminate(30);
		}
	}
	else
	{
		Terminate(30);
	}

	/* only a message */
	dbg(TRACE,"<MAIN> initializing OK");

  /*debug_level = igRuntimeMode;*/

	/* forever */
	while (1)
	{
		/* get item... */
		ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

		/* set event pointer	*/
		prgEvent = (EVENT *) prgItem->text;

		if (ilRC == RC_SUCCESS)
		{
			/* Acknowledge the item */
			ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
			if (ilRC != RC_SUCCESS)
			{
				dbg(DEBUG,"<HandleQueues> calling HandleQueErr");
				HandleQueErr(ilRC);
			}

			switch (prgEvent->command)
			{
				case	HSB_STANDBY:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					HandleQueues();
					break;

				case	HSB_COMING_UP:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					HandleQueues();
					break;

				case	HSB_ACTIVE:
				case	HSB_STANDALONE:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					break;

				case	HSB_ACT_TO_SBY:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					HandleQueues();
					break;

				case	HSB_DOWN:
					ctrl_sta = prgEvent->command;
					Terminate(0);
					break;

				case	SHUTDOWN:
					Terminate(0);
					break;

				case	RESET:
					ilRC = Reset();
					break;

				case	EVENT_DATA:
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
			dbg(DEBUG,"<HandleQueues> calling HandleQueErr");
			HandleQueErr(ilRC);
		}
	}
}


/******************************************************************************/
/* The initialization routine                                                 */
/******************************************************************************/
static int init_fileif(void)
{
	int		i;
	int		j;
	int		ilRC;
	int		ilCnt;
	int		ilCmdNo;
	int		ilTableNo;
	int		ilFieldNo;
	int		ilAssignNo;
	int		ilSyslibNo;
	int		ilSysTmpNo;
	int		ilCurConst;
	int		ilSysTmpMax;
	int		ilFieldPos;
	int		ilConditionNo;
	int		ilIgnoreFieldNo;
	int		ilCurKeyword;
	int		ilMaxMapData;
	int		ilCurMapData;
	int		ilOtherData;
	int		ilCurDataAlign;
	int		ilDataAlignCnt;
	int		ilOldDebugLevel;
	int		ilCurUtc;
	int		ilNoUtcToLocal;
	int		ilCurSubStr;
	int		ilNoOfSubStr;
	char		*pclS;
	char		*pclPtr;
	char		*pclCfgPath;
	char		*pclBlank;
	char		pclCurSubStr[iMIN];
	char		pclCurrentTable[iMIN_BUF_SIZE];
	char		pclCurrentCommand[iMIN_BUF_SIZE];
	char		pclCurrentSyslib[iMIN_BUF_SIZE];
	char		pclTmpBuf[iMIN_BUF_SIZE];
	char		pclTmpStrg[iMIN_BUF_SIZE];
	char		pclDataCounter[iMAXIMUM];
	char		pclAllCommands[iMAXIMUM];
	char		pclAllTables[iMIN_BUF_SIZE];
	char		pclAllFields[iMAXIMUM];
	char		pclAllAssignments[iMAXIMUM];
	char		pclAllSyslib[iMAXIMUM];
	char		pclAllConditions[iMAXIMUM];
	char		pclAllIgnore[iMAXIMUM];
	char		pclAllUtcToLocal[iMAXIMUM];
	char		pclAllSQLKeywords[iMIN_BUF_SIZE];
	char		pclAllSubStr[iMIN_BUF_SIZE];
	char		pclMapKeyWord[iMIN];
	char		pclMappingData[iMIN_BUF_SIZE];
	char		pclSyslibResultBuf[iMIN_BUF_SIZE];
	char		pclFieldTimestampFormat[iMIN_BUF_SIZE];
	char		pclTimestampFormat[iMIN];
	char		pclSyslibFields[iMAXIMUM];
	char		pclSyslibFieldData[iMAXIMUM];
	/* Variables for numtab key inserting */
	char 		pclSqlBuf[iMIN_BUF_SIZE];
	char 		pclDataArea[iMIN_BUF_SIZE];
	char 		pclOraErrorMsg[iMAXIMUM];
	short 	slCursor 	= 0;		/* Cursor for SQL call		*/
	short 	slSqlFunc;			/* Type of SQl Call		*/
	BOOL		blinsert = FALSE;		/* tag for automatic numtab key insertation*/

	dbg(TRACE,"");
	/* to inform status-handler about startup time */
	dbg(DEBUG,"<init_fileif> calling SendStatus(..START..)");
	if ((ilRC = SendStatus("FILEIO", "", "START", 1, "", "", "")) != RC_SUCCESS)
	{
		/* bloody fuckin shit */
		dbg(TRACE,"<init_fileif> SendStatus returns: %d",ilRC);
		Terminate(30);
	}

	/* changes by mos 6 Apr 2000 */
	while (init_db())
	{
		sleep(5);
	}
	/* eof changes */

	/* get path */
	if ((pclCfgPath = getenv("CFG_PATH")) == NULL)
	{
		dbg(TRACE,"<init_fileif> cannot read environment variable CFG_PATH");
		Terminate(30);
	}
	else
	{
		/* clear memory */
		memset((void*)pcgCfgFile, 0x00, iMIN_BUF_SIZE);

		/* copy path to filename buffer */
		strcpy(pcgCfgFile, pclCfgPath);

		/* check last character */
		if (pcgCfgFile[strlen(pcgCfgFile)-1] != '/')
			strcat(pcgCfgFile,"/");

		/* attach filename to buffer */
		/*strcat(pcgCfgFile, "fileif.cfg");*/
		strcat(pcgCfgFile,mod_name);
		strcat(pcgCfgFile,".cfg");
		/*---------------------------------------------------------------------*/
		/*---------------------------------------------------------------------*/
		/*---------------------------------------------------------------------*/
		/*------------------------ someting to MAIN ---------------------------*/
		pclTmpBuf[0] = 0x00;
		if ((ilRC = iGetConfigEntry(pcgCfgFile, "MAIN", "use_2400",
							CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
		{
			dbg(TRACE,"<init_fileif> missing use_2400 in section MAIN");
			dbg(TRACE,"<init_fileif> Using Defaultvalue use_2400 = <NO>");
			igUse2400 = 0;
		}

		/* convert to uppercase */
		StringUPR((UCHAR*)pclTmpBuf);
		if (!strcmp(pclTmpBuf, "YES"))
		{
			dbg(TRACE,"<init_fileif> setting use_2400-Flag to YES");
			igUse2400 = 1;
		}
		else if (!strcmp(pclTmpBuf, "NO"))
		{
			dbg(TRACE,"<init_fileif> setting use_2400-Flag to NO");
			igUse2400 = 0;
		}
		else
		{
			dbg(TRACE,"<init_fileif> setting use_2400-Flag to NO");
			igUse2400 = 0;
		}

		memset((void*)pclAllCommands, 0x00, iMAXIMUM);
		if ((ilRC = iGetConfigEntry(pcgCfgFile, "MAIN", "commands",
							CFG_STRING, pclAllCommands)) != RC_SUCCESS)
		{
			dbg(TRACE,"<init_fileif> missing commands in section MAIN");
			Terminate(30);
		}

		/* convert to uppercase */
		StringUPR((UCHAR*)pclAllCommands);

		/* get number of commands */
		if ((rgTM.iNoCommands = GetNoOfElements(pclAllCommands, cCOMMA)) < 0)
		{
			dbg(TRACE,"<init_fileif> GetNoOfElements (commands) returns: %d",
																			rgTM.iNoCommands);
			Terminate(30);
		}

		/* get memory for commands and something else */
		if ((rgTM.prHandleCmd = (HandleCmd*)calloc(1,rgTM.iNoCommands*sizeof(HandleCmd))) == NULL)
		{
			dbg(TRACE,"<init_fileif> not enough memory for command handling");
			rgTM.prHandleCmd = NULL;
			Terminate(30);
		}
		dbg(TRACE,"<init_fileif> allocated <%d> Bytes at <%x> for CMD-handling!",rgTM.iNoCommands*sizeof(HandleCmd),rgTM.prHandleCmd);

		/* for all commands */
		for (ilCmdNo=0; ilCmdNo<rgTM.iNoCommands; ilCmdNo++)
		{
			memset(&rgTM.prHandleCmd[ilCmdNo],0x00,sizeof(HandleCmd));

			/* set flag to zero */
			rgTM.prHandleCmd[ilCmdNo].iOpenDataFile = 0;

			/* set counter to zero */
			rgTM.prHandleCmd[ilCmdNo].iDataCounter = 0;

			/* set counter to zero */
			rgTM.prHandleCmd[ilCmdNo].iDataFromTable = 0;

			/* clear memory */
			memset((void*)pclCurrentCommand, 0x00, iMIN_BUF_SIZE);

			/* read entries for this section */
			if ((pclPtr = GetDataField(pclAllCommands, ilCmdNo, cCOMMA)) == NULL)
			{
				dbg(TRACE,"<init_fileif> cannot read command with index: %d", ilCmdNo);
				Terminate(30);
			}
			else
			{
				/* copy to static buffer... */
				strcpy(pclCurrentCommand, pclPtr);

				/* convert command to uppercase */
				StringUPR((UCHAR*)pclCurrentCommand);

				/* only for debugging */
				dbg(TRACE,"");
				dbg(TRACE,"<init_fileif> ######### START <%s> #########", pclCurrentCommand);

				/*---------------------------------------------------------------*/
				/*---------------------------------------------------------------*/
				/*---------------------------------------------------------------*/
				/* the home airport */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "home_airport", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcHomeAirport)) != RC_SUCCESS)
				{
					/* MOS OCT 2000 Multihopo *
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcHomeAirport, pcgHomeAP);*/
					dbg(TRACE,"<init_fileif> can't find HOPO in <%s>",pclCurrentCommand);
					return RC_FAIL;
				}
				dbg(TRACE,"<init_fileif> Home-Airport: <%s>", rgTM.prHandleCmd[ilCmdNo].pcHomeAirport);

				/* the table extension */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "table_extension", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcTableExtension)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcTableExtension, pcgTABEnd);
				}
				dbg(TRACE,"<init_fileif> TableExtension: <%s>", rgTM.prHandleCmd[ilCmdNo].pcTableExtension);

				/* the shell we use for file transfer protocol (FTP)... */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "shell", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcShell)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcShell, "ksh");
				}
				dbg(TRACE,"<init_fileif> Shell: <%s>", rgTM.prHandleCmd[ilCmdNo].pcShell);

				/* this is for returncode-sending */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "mod_id", CFG_INT, (char*)&rgTM.prHandleCmd[ilCmdNo].iModID)) != RC_SUCCESS)
				{
					/* set mod_id to default */
					dbg(TRACE,"<init_fileif> can't find mod_id");
					rgTM.prHandleCmd[ilCmdNo].iModID = -1;
				}

				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "send_sql_rc", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					/* set flag */
					dbg(TRACE,"<init_fileif> can't find SendSqlRC-Flag");
					rgTM.prHandleCmd[ilCmdNo].iSendSqlRc = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);
					if (!strcmp(pclTmpBuf, "YES"))
					{
						dbg(TRACE,"<init_fileif> setting SendSqlRc-Flag");
						rgTM.prHandleCmd[ilCmdNo].iSendSqlRc = 1;
					}
					else if (!strcmp(pclTmpBuf, "NO"))
					{
						dbg(TRACE,"<init_fileif> resetting SendSqlRc-Flag");
						rgTM.prHandleCmd[ilCmdNo].iSendSqlRc = 0;
					}
					else
					{
						dbg(TRACE,"<init_fileif> unknown entry (SendSqlRc), set default (NO)");
						rgTM.prHandleCmd[ilCmdNo].iSendSqlRc = 0;
					}
				}

				/* this is for (received) selections... */
				/* should i use received selection (via QUE), otherwise selection
				from configurationfile */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "use_received_selection", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					/* set flag */
					dbg(TRACE,"<init_fileif> can't find UseReceivedSelection-Flag");
					rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);
					if (!strcmp(pclTmpBuf, "YES"))
					{
						dbg(TRACE,"<init_fileif> setting UseReceivedSelection-Flag");
						rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection = 1;
					}
					else if (!strcmp(pclTmpBuf, "NO"))
					{
						dbg(TRACE,"<init_fileif> resetting UseReceivedSelection-Flag");
						rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection = 0;
					}
					else if (!strcmp(pclTmpBuf, "FREE"))
					{
						dbg(TRACE,"<init_fileif> resetting UseReceivedSelection-Flag");
						rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection = 2;
					}
					else
					{
						dbg(TRACE,"<init_fileif> unknown entry (use_received_selection), set default (NO)");
						rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection = 0;
					}
				}

				/* this is for error files... */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "error_file_name", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcErrorFileName)) != RC_SUCCESS)
				{
					/* set flag */
					rgTM.prHandleCmd[ilCmdNo].iUseErrorFile = 0;
				}
				else
				{
					/* set flag */
					rgTM.prHandleCmd[ilCmdNo].iUseErrorFile = 1;

					/* get error file path */
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "error_file_path", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath)) != RC_SUCCESS)
					{
						/* set default for this */
						strcpy(rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath, "./");
					}
					if (rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath[strlen(rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath)-1] != '/')
						strcat(rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath, "/");

					/* get timestamp format */
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "error_timestamp_format", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcErrorTimeStampFormat)) != RC_SUCCESS)
					{
						/* filename without timestamp... */
						memset((void*)rgTM.prHandleCmd[ilCmdNo].pcErrorTimeStampFormat, 0x00, iMIN);
					}
				}

				/* debug messages */
				if (rgTM.prHandleCmd[ilCmdNo].iUseErrorFile)
				{
					/*dbg(DEBUG,"<init_fileif> -------- START ERROR FILE -------");*/
					dbg(TRACE,"<init_fileif> Error-Filename : <%s>", rgTM.prHandleCmd[ilCmdNo].pcErrorFileName);
					dbg(TRACE,"<init_fileif> Error-Filepath : <%s>", rgTM.prHandleCmd[ilCmdNo].pcErrorFilePath);
					dbg(TRACE,"<init_fileif> Error-Timestamp: <%s>", rgTM.prHandleCmd[ilCmdNo].pcErrorTimeStampFormat);
					/*dbg(DEBUG,"<init_fileif> -------- END ERROR FILE -------");*/
				}

				/* read all for sending command after creating file... */
				rgTM.prHandleCmd[ilCmdNo].iUseSEC = 1;

				/* send original tw_start back to calling process ? Deafult = NO */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "afc_keep_org_twstart", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					/* set flag */
					dbg(TRACE,"<init_fileif> can't find entry (afc_keep_org_twstart)! Set default = <NO>! ");
					rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);
					if (!strcmp(pclTmpBuf, "YES"))
					{
						dbg(TRACE,"<init_fileif> Original tw_start of event will be sent out!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart = 1;
					}
					else if (!strcmp(pclTmpBuf, "NO"))
					{
						dbg(TRACE,"<init_fileif> None Original tw_start of event will be sent out!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart = 0;
					}
					else
					{
						dbg(TRACE,"<init_fileif> unknown entry (afc_keep_org_twstart)! Set default = <NO>!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart = 0;
					}
				}

				/* send filename together with afc-command ? Deafult = NO */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "afc_send_filename", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					/* set flag */
					dbg(TRACE,"<init_fileif> can't find entry (afc_send_filename)! Set default = <NO>! ");
					rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcSendFilename = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);
					if (!strcmp(pclTmpBuf, "YES"))
					{
						dbg(TRACE,"<init_fileif> Filename will be sent out!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcSendFilename = 1;
					}
					else if (!strcmp(pclTmpBuf, "NO"))
					{
						dbg(TRACE,"<init_fileif> Filename will NOT be sent out!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcSendFilename = 0;
					}
					else
					{
						dbg(TRACE,"<init_fileif> unknown entry (afc_send_filename)! Set default = <NO>!");
						rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcSendFilename = 0;
					}
				}




				/* read mod_id...*/
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "afc_mod_id", CFG_INT, (char*)&rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iModID)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> missing afc_mod_id!");
					rgTM.prHandleCmd[ilCmdNo].iUseSEC = 0;
				}
				else
				{
					/* read command */
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "afc_command", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcCmd)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing afc_command!");
						rgTM.prHandleCmd[ilCmdNo].iUseSEC = 0;
					}
					else
					{
						/* should i write file to que after creating itself? */
						memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "afc_file_to_que", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclTmpBuf);

							if (!strcmp(pclTmpBuf, "YES"))
								rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iSendFileToQue = 1;
							else if (!strcmp(pclTmpBuf, "NO"))
								rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iSendFileToQue = 0;
							else
							{
								dbg(TRACE,"<init_fileif> unknown entry for 'afc_file_to_que'-use default (no)");
								rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iSendFileToQue = 0;
							}
						}
						else
						{
							rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iSendFileToQue = 0;
						}

						dbg(TRACE,"<init_fileif> AFC_MOD_ID: <%d>, AFC_COMMAND: <%s>, AFC_FILE_TO_QUE: <%d>", rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iModID, rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcCmd, rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iSendFileToQue);
					}
				}

				/* set old filename to NULL... */
				memset((void*)rgTM.prHandleCmd[ilCmdNo].pcOldFileNameWithPath,
																		0x00, iMIN_BUF_SIZE);
				rgTM.prHandleCmd[ilCmdNo].lOldFileLength = -1;
				rgTM.prHandleCmd[ilCmdNo].lFileLength = -1;
				rgTM.prHandleCmd[ilCmdNo].pcOldFileContent = NULL;
				rgTM.prHandleCmd[ilCmdNo].pcFileContent = NULL;

				/* read check file diff enties */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "check_file_diff", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					/* use default in this case */
					rgTM.prHandleCmd[ilCmdNo].iCheckFileDiff = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);

					/* check entry */
					if (!strcmp(pclTmpBuf, "YES"))
						rgTM.prHandleCmd[ilCmdNo].iCheckFileDiff = 1;
					else if (!strcmp(pclTmpBuf, "NO"))
						rgTM.prHandleCmd[ilCmdNo].iCheckFileDiff = 0;
					else
					{
						dbg(TRACE,"<init_fileif> unknown 'check_file_diff' entry - use default (NO)");
						rgTM.prHandleCmd[ilCmdNo].iCheckFileDiff = 0;
					}
				}
				dbg(TRACE,"<init_fileif> setting iCheckFileDiff to <%d>", rgTM.prHandleCmd[ilCmdNo].iCheckFileDiff);

				/* create file with timestamp? */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_with_timestamp", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_FILETIMESTAMP);
				}

				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "YES"))
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithTimestamp = 1;
				else if (!strcmp(pclTmpBuf, "NO"))
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithTimestamp = 0;
				else
				{
					dbg(TRACE,"<init_fileif> unknown entry for 'file_with_timestamp'");
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithTimestamp = 0;
				}
				/* JWE PRF 7610: */
				/**************************/
				/* base of this timestamp */
				/**************************/
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_timestamp_base", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, "UTC");
				}
				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);
				/* check entry */
				if (!strcmp(pclTmpBuf, "UTC"))
					rgTM.prHandleCmd[ilCmdNo].iUseFileTimestampBase = 0;
				else if (!strcmp(pclTmpBuf, "LOC"))
					rgTM.prHandleCmd[ilCmdNo].iUseFileTimestampBase = 1;
				else
				{
					dbg(TRACE,"<init_fileif> unknown entry for 'file_timestamp_base'. Using UTC!");
					rgTM.prHandleCmd[ilCmdNo].iUseFileTimestampBase = 0;
				}

				/* format of this timestamp */
				if (rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithTimestamp)
				{
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_timestamp_format", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcTimeStampFormat)) != RC_SUCCESS)
					{
						strcpy(rgTM.prHandleCmd[ilCmdNo].pcTimeStampFormat, "YYYYMMDDHHMISS");
					}
					dbg(TRACE,"<init_fileif> found file_timestamp_format: <%s>", rgTM.prHandleCmd[ilCmdNo].pcTimeStampFormat);
				}

				/* create file with sequence? */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_with_seq_nr", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, "NO");
				}

				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "YES"))
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithSeqNr = 1;
				else if (!strcmp(pclTmpBuf, "NO"))
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithSeqNr = 0;
				else
				{
					dbg(TRACE,"<init_fileif> unknown entry for 'file_with_seq_nr'");
					rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithSeqNr = 0;
				}

				if (rgTM.prHandleCmd[ilCmdNo].iUseFilenameWithSeqNr==1)
				{
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_seq_nr_len",
					CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcSeqNrLen)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> no file_seq_nr_len in section %s!", pclCurrentCommand);
						strcpy(rgTM.prHandleCmd[ilCmdNo].pcSeqNrLen, "10");
					}
					dbg(TRACE,"<init_fileif> file_seq_nr_len: <%s>", rgTM.prHandleCmd[ilCmdNo].pcSeqNrLen);
				}

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "std_linenumber_len",
				CFG_INT, (char*)&rgTM.prHandleCmd[ilCmdNo].iStdLineNrLen)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no std_linenumber_len in section %s! Use 10 digits per default!", pclCurrentCommand);
					rgTM.prHandleCmd[ilCmdNo].iStdLineNrLen=10;
				}
				dbg(TRACE,"<init_fileif> std_linenumber_len: <%d>", rgTM.prHandleCmd[ilCmdNo].iStdLineNrLen);

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_name", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcTmpFile,"TMP_");
					strcat(rgTM.prHandleCmd[ilCmdNo].pcTmpFile, sDEFAULT_FILE);
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcFile, sDEFAULT_FILE);
				}
				/* PRF 6998 */
				else
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcTmpFile,"TMP_");
					strcat(rgTM.prHandleCmd[ilCmdNo].pcTmpFile,pclTmpBuf);
					strcat(rgTM.prHandleCmd[ilCmdNo].pcFile,pclTmpBuf);
				}

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_extension", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFileExtension)) != RC_SUCCESS)
				{
					memset((void*)rgTM.prHandleCmd[ilCmdNo].pcFileExtension, 0x00, iMIN);
				}
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "format_of_#DATE", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcDynDateFormat)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcDynDateFormat, "YYYYMMDD");
				}
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "format_of_#TIME", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcDynTimeFormat)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcDynTimeFormat, "HHMISS");
				}

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "ftp_user", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFtpUser)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> missing ftp_user in section <%s>! Not using FTP!", pclCurrentCommand);
					rgTM.prHandleCmd[ilCmdNo].iUseFTP = 0;
				}
				else
					rgTM.prHandleCmd[ilCmdNo].iUseFTP = 1;

				/* only if we use FTP... */
				if (rgTM.prHandleCmd[ilCmdNo].iUseFTP)
				{
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "ftp_pass", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFtpPass)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing ftp_pass in section <%s>", pclCurrentCommand);
					}
					else
					{
						/* decode passwd */
						pclPtr = rgTM.prHandleCmd[ilCmdNo].pcFtpPass;
#ifdef __SHOW_PASSWD
						dbg(TRACE,"<init_fileif> passwd before: <%s>", pclPtr);
#endif
						CDecode(&pclPtr);
#ifdef __SHOW_PASSWD
						dbg(TRACE,"<init_fileif> passwd after : <%s>", pclPtr);
#endif
						strcpy(rgTM.prHandleCmd[ilCmdNo].pcFtpPass, pclPtr);
					}

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "ftp_timeout", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcftp_timeout)) != RC_SUCCESS)
					{
						/* set default for this */
            strcpy(rgTM.prHandleCmd[ilCmdNo].pcftp_timeout, "0");
					}
					dbg(TRACE,"<init_fileif> ftp_timeout: <%s>", rgTM.prHandleCmd[ilCmdNo].pcftp_timeout);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "ftp_mode", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcftp_mode)) != RC_SUCCESS)
					{
						/* set default for this */
       			strcpy(rgTM.prHandleCmd[ilCmdNo].pcftp_mode, "FTPHDL");
					}
					dbg(TRACE,"<init_fileif> ftp_mode: <%s>", rgTM.prHandleCmd[ilCmdNo].pcftp_mode);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "machine", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcMachine)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing machine in section <%s>", pclCurrentCommand);
						Terminate(30);
					}
					dbg(TRACE,"<init_fileif> machine: <%s>", rgTM.prHandleCmd[ilCmdNo].pcMachine);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "machine_path", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcMachinePath)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing machine_path in section %s", pclCurrentCommand);
						Terminate(30);
					}
					dbg(TRACE,"<init_fileif> machine_path: <%s>", rgTM.prHandleCmd[ilCmdNo].pcMachinePath);

					/** Changes by MOS **/
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "client_os",
					CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFtp_Client_OS)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing client os in section %s", pclCurrentCommand);
					}
					dbg(TRACE,"<init_fileif> client_os: <%s>", rgTM.prHandleCmd[ilCmdNo].pcFtp_Client_OS);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "remote_fileextension",
						CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcRemoteFile_AppendExtension)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing remote_fileextension in section %s", pclCurrentCommand);
					}
					dbg(TRACE,"<init_fileif> remote_fileextension: <%s>", rgTM.prHandleCmd[ilCmdNo].pcRemoteFile_AppendExtension);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "remote_filename",
                          CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFtpDestFileName)) != RC_SUCCESS)
          {
            dbg(TRACE,"<init_fileif> missing remote_filename in section %s", pclCurrentCommand);
					}
					dbg(TRACE,"<init_fileif> remote_filename: <%s>", rgTM.prHandleCmd[ilCmdNo].pcFtpDestFileName);

					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "rename_remote_file",
                        CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFtpRenameDestFile)) != RC_SUCCESS)
          {
						/* set default for this */
						dbg(TRACE,"<init_fileif> missing rename_remote_file in section %s", pclCurrentCommand);
      			strcpy(rgTM.prHandleCmd[ilCmdNo].pcFtpRenameDestFile, " ");
					}
					dbg(TRACE,"<init_fileif> rename_remote_file: <%s>", rgTM.prHandleCmd[ilCmdNo].pcFtpRenameDestFile);

				}

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "numtab_key",
				CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no numtab_key in section %s", pclCurrentCommand);
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key, " ");
				}
				dbg(TRACE,"<init_fileif> numtab_key: <%s>", rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key);

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "numtab_startvalue",
				CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcNumtab_Startvalue)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no numtab_startvalue in section %s", pclCurrentCommand);
				}
				dbg(TRACE,"<init_fileif> numtab_startvalue: <%s>", rgTM.prHandleCmd[ilCmdNo].pcNumtab_Startvalue);

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "numtab_stopvalue",
				CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcNumtab_Stopvalue)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no numtab_stopvalue in section %s", pclCurrentCommand);
				}
				dbg(TRACE,"<init_fileif> numtab_stopvalue: <%s>", rgTM.prHandleCmd[ilCmdNo].pcNumtab_Stopvalue);

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "numtab_resetday",
				CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcSeqResetDay)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no numtab_resetday in section %s", pclCurrentCommand);
				}
				dbg(TRACE,"<init_fileif> numtab_resetday: <%s>", rgTM.prHandleCmd[ilCmdNo].pcSeqResetDay);

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "numtab_automaticinsert",
				CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcNumtab_Automaticinsert)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> no numtab_automaticinsert in section %s", pclCurrentCommand);
				}
				dbg(TRACE,"<init_fileif> numtab_automaticinsert: <%s>", rgTM.prHandleCmd[ilCmdNo].pcNumtab_Automaticinsert);
				/* convert to uppercase */
				StringUPR((UCHAR*) rgTM.prHandleCmd[ilCmdNo].pcNumtab_Automaticinsert);

				if ((strlen(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key) > 1)
					&& (strcmp(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Automaticinsert,"YES") == 0)
					&& (strlen(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Startvalue)>0)
					&& (strlen(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Stopvalue)>0)
					&& (strlen(rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key)>0)) {

					/* reset buffers */
					memset(pclSqlBuf,0x00,iMIN_BUF_SIZE);
					memset(pclDataArea,0x00,iMIN_BUF_SIZE);
					memset(pclOraErrorMsg,0x00,iMAXIMUM);

					/* build sql statement for id of rotation flight */
					sprintf(pclSqlBuf,"SELECT ACNU FROM NUMTAB WHERE KEYS = '%s'",rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key);

					/* set type of sql call */
					slSqlFunc = START;

					/* run sql */
					if ((ilRC = sql_if(slSqlFunc,&slCursor,pclSqlBuf,pclDataArea))!= DB_SUCCESS)
					{
						if (ilRC != NOTFOUND)
						{
							ilRC = RC_FAIL;
							get_ora_err(ilRC,pclOraErrorMsg);
							dbg(TRACE,"<init_fileif> ORA-ERR <%s>  <%s>",pclOraErrorMsg, pclSqlBuf);
							blinsert = FALSE;
						} else {
							blinsert = TRUE;
						}
					} else {
						blinsert = FALSE;
						dbg(TRACE,"<init_fileif> Number Circle <%s> exists in NUMTAB",rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key);
					}

					/* close cursor */
					close_my_cursor(&slCursor);


					if (blinsert == TRUE) {
						/*Insert datarecord with key and min and max value into numtab */
						/* reset buffers */
						memset(pclSqlBuf,0x00,iMIN_BUF_SIZE);
						memset(pclDataArea,0x00,iMIN_BUF_SIZE);
						memset(pclOraErrorMsg,0x00,iMAXIMUM);

						dbg(TRACE,"<init_fileif> hopo <%s>",rgTM.prHandleCmd[ilCmdNo].pcHomeAirport);

						/* build sql statement for id of rotation flight */
						sprintf(pclSqlBuf,"INSERT INTO NUMTAB VALUES(%s,' ','%s','%s',%s,%s)",rgTM.prHandleCmd[ilCmdNo].pcNumtab_Startvalue,rgTM.prHandleCmd[ilCmdNo].pcHomeAirport,rgTM.prHandleCmd[ilCmdNo].pcNumtab_Key,rgTM.prHandleCmd[ilCmdNo].pcNumtab_Stopvalue, rgTM.prHandleCmd[ilCmdNo].pcNumtab_Startvalue);

						/* set type of sql call */
						slSqlFunc = COMMIT | REL_CURSOR;

						/* run sql */
						if ((ilRC = sql_if(slSqlFunc,&slCursor,pclSqlBuf,pclDataArea))!= DB_SUCCESS)
						{
							ilRC = RC_FAIL;
							get_ora_err(ilRC,pclOraErrorMsg);
							dbg(TRACE,"<init_fileif> ORA-ERR=<%s> SQL=<%s>",pclOraErrorMsg, pclSqlBuf);
						} /* if ((ilRC = sql_if */

						/* close cursor */
						close_my_cursor(&slCursor);

					} /* number circle key configured */
				} /* automatic numtab key inserting */
				/***************EO Changes*********/

				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "file_path", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcFilePath)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcFilePath, sDEFAULT_FILEPATH);
				}
				dbg(TRACE,"<init_fileif> file_path: <%s>", rgTM.prHandleCmd[ilCmdNo].pcFilePath);

				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "sof_string", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcStartOfFile)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iUseSOF = 0;
				}
				else
					rgTM.prHandleCmd[ilCmdNo].iUseSOF = 1;
				while ((pclBlank=strchr(rgTM.prHandleCmd[ilCmdNo].pcStartOfFile,'~'))!=NULL)
				{
				   *pclBlank=0x20;
				}
				dbg(TRACE,"<init_fileif> sof_string: <%s>", rgTM.prHandleCmd[ilCmdNo].pcStartOfFile);
				strcpy(rgTM.prHandleCmd[ilCmdNo].pcStartOfFileOrg,rgTM.prHandleCmd[ilCmdNo].pcStartOfFile);

				if (strstr(rgTM.prHandleCmd[ilCmdNo].pcStartOfFile,"#DATALINECOUNT")!=NULL)
				{
					dbg(TRACE,"<init_fileif> ATTENTION! Func. <#DATALINECOUNT> only works correctly in header line, if");
					dbg(TRACE,"<init_fileif> section only contains a single subsection (tables) entry !!!!");
				}

				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "eof_string", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcEndOfFile)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iUseEOF = 0;
				}
				else
					rgTM.prHandleCmd[ilCmdNo].iUseEOF = 1;
				while ((pclBlank=strchr(rgTM.prHandleCmd[ilCmdNo].pcEndOfFile,'~'))!=NULL)
				{
				   *pclBlank=0x20;
				}
				dbg(TRACE,"<init_fileif> eof_string: <%s>", rgTM.prHandleCmd[ilCmdNo].pcEndOfFile);
				strcpy(rgTM.prHandleCmd[ilCmdNo].pcEndOfFileOrg,rgTM.prHandleCmd[ilCmdNo].pcEndOfFile);

				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "sob_number", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_SOBNUMBER);
				}
				dbg(TRACE,"<init_fileif> sob_number: <%s>",pclTmpBuf);

				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "YES"))
					rgTM.prHandleCmd[ilCmdNo].iStartOfBlockNumber = 1;
				else if (!strcmp(pclTmpBuf, "NO"))
					rgTM.prHandleCmd[ilCmdNo].iStartOfBlockNumber = 0;
				else
				{
					dbg(DEBUG,"<init_fileif> unkown entry for sob_number-use default");
					rgTM.prHandleCmd[ilCmdNo].iStartOfBlockNumber = 1;
				}

				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "sob_string", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcStartOfBlock)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iUseSOB = 0;
				}
				else
					rgTM.prHandleCmd[ilCmdNo].iUseSOB = 1;

				dbg(TRACE,"<init_fileif> sob_string: <%s>",rgTM.prHandleCmd[ilCmdNo].pcStartOfBlock);

				/* set correct entry for StartOfBlock... */
				if (rgTM.prHandleCmd[ilCmdNo].iStartOfBlockNumber &&
				    rgTM.prHandleCmd[ilCmdNo].iUseSOB)
				{
					/* must change entry format */
					if ((ilRC = InsertIntoString(rgTM.prHandleCmd[ilCmdNo].pcStartOfBlock, strlen(rgTM.prHandleCmd[ilCmdNo].pcStartOfBlock)-1, " %d")) != 0)
					{
						dbg(DEBUG,"<init_fileif> InsertIntoString returns: %d", ilRC);
						dbg(DEBUG,"<init_fileif> set sob_number to \"NO\"");
						rgTM.prHandleCmd[ilCmdNo].iStartOfBlockNumber = 0;
					}
				}

				/* read entry for eob_number */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "eob_number", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_EOBNUMBER);
				}
				dbg(TRACE,"<init_fileif> eob_number: <%s>",pclTmpBuf);

				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "YES"))
					rgTM.prHandleCmd[ilCmdNo].iEndOfBlockNumber = 1;
				else if (!strcmp(pclTmpBuf, "NO"))
					rgTM.prHandleCmd[ilCmdNo].iEndOfBlockNumber = 0;
				else
				{
					dbg(DEBUG,"<init_fileif> unkown entry for eob_number-use default");
					rgTM.prHandleCmd[ilCmdNo].iEndOfBlockNumber = 1;
				}

				/* read entry for end of block sign (eob) */
				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "eob_string", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcEndOfBlock)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iUseEOB = 0;
				}
				else
					rgTM.prHandleCmd[ilCmdNo].iUseEOB = 1;
				dbg(TRACE,"<init_fileif> eob_string: <%s>",rgTM.prHandleCmd[ilCmdNo].pcEndOfBlock);

				/* set correct entry for StartOfBlock... */
				if (rgTM.prHandleCmd[ilCmdNo].iEndOfBlockNumber &&
					 rgTM.prHandleCmd[ilCmdNo].iUseEOB)
				{
					/* must change entry format */
					if ((ilRC = InsertIntoString(rgTM.prHandleCmd[ilCmdNo].pcEndOfBlock, strlen(rgTM.prHandleCmd[ilCmdNo].pcEndOfBlock)-1, " %d")) != 0)
					{
						dbg(DEBUG,"<init_fileif> InsertIntoString returns: %d", ilRC);
						dbg(DEBUG,"<init_fileif> set eob_number to \"NO\"");
						rgTM.prHandleCmd[ilCmdNo].iEndOfBlockNumber = 0;
					}
				}

				/* read entry for airport */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "airport", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iUseAirport = 0;
				}
				else
				{
					/* convert to uppercase */
					StringUPR((UCHAR*)pclTmpBuf);

					dbg(TRACE,"<init_fileif> searching airport %s...", pclTmpBuf);
					rgTM.prHandleCmd[ilCmdNo].iUseAirport = 1;

#ifndef UFIS32
					/* use syslib */
					ilCnt = 1;
					memset((void*)pclSyslibResultBuf, 0x00, iMIN_BUF_SIZE);

					/* build tablename... */
					memset((void*)pcgTmpTabName, 0x00, iMIN);
					sprintf(pcgTmpTabName, "APT%s", rgTM.prHandleCmd[ilCmdNo].pcTableExtension);
					dbg(DEBUG,"<init_fileif> build tablename: <%s>", pcgTmpTabName);

					/* set new ans save old debug_lebel... */
					ilOldDebugLevel = debug_level;
					debug_level = TRACE;

					/* ---------------------------------------------------------- */
					if (!strcmp(rgTM.prHandleCmd[ilCmdNo].pcTableExtension, "TAB"))
					{
						strcpy(pclSyslibFields, "APC3,HOPO");
						sprintf(pclSyslibFieldData, "%s,%s", pclTmpBuf, rgTM.prHandleCmd[ilCmdNo].pcHomeAirport);
						ilRC = syslibSearchDbData(pcgTmpTabName, pclSyslibFields, pclSyslibFieldData, "APFN", pclSyslibResultBuf, &ilCnt, "");
					}
					else
					{
						ilRC = syslibSearchDbData(pcgTmpTabName, "APC3", pclTmpBuf, "APFN", pclSyslibResultBuf, &ilCnt, "");
					}
					/* ---------------------------------------------------------- */

					debug_level = ilOldDebugLevel;
					dbg(DEBUG,"<init_fileif> syslib returns with: %d", ilRC);
					if (ilRC == RC_SUCCESS)
					{
						strcpy(rgTM.prHandleCmd[ilCmdNo].pcAirport, pclSyslibResultBuf);
						dbg(DEBUG,"<init_fileif> syslib found: <%s>", rgTM.prHandleCmd[ilCmdNo].pcAirport);
					}
					else
					{
						dbg(TRACE,"<init_fileif> syslib failure....");
						rgTM.prHandleCmd[ilCmdNo].iUseAirport = 0;
					}
					dbg(TRACE,"<init_fileif> airport: <%s>",rgTM.prHandleCmd[ilCmdNo].pcAirport);
#endif
				}

				/* read entry for character set */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "character_set", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_CHAR_SET);
				}
				dbg(TRACE,"<init_fileif> character_set: <%s>",pclTmpBuf);

				/* conver to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "ASCII"))
					rgTM.prHandleCmd[ilCmdNo].iCharacterSet = iASCII;
				else if (!strcmp(pclTmpBuf, "ANSI"))
					rgTM.prHandleCmd[ilCmdNo].iCharacterSet = iANSI;
				else
				{
					dbg(DEBUG,"<init_fileif> unknown entry for character_set in section %s-use default", pclCurrentCommand);
					rgTM.prHandleCmd[ilCmdNo].iCharacterSet = iASCII;
				}

				/* read entry for write_data */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "write_data", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_WRITE_DATA);
				}
				dbg(TRACE,"<init_fileif> write_data: <%s>",pclTmpBuf);

				/* conver to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "DYNAMIC"))
					rgTM.prHandleCmd[ilCmdNo].iWriteData = iWRITE_DYNAMIC;
				else if (!strcmp(pclTmpBuf, "FIX"))
					rgTM.prHandleCmd[ilCmdNo].iWriteData = iWRITE_FIX;
				else
				{
					dbg(TRACE,"<init_fileif> unknown entry for write_data in section %s! Use default", pclCurrentCommand);
					rgTM.prHandleCmd[ilCmdNo].iWriteData = iWRITE_DYNAMIC;
				}

				/* read data_separator or data_length. one is mandatory */
				if (IsDynamic(rgTM.prHandleCmd[ilCmdNo].iWriteData))
				{
					/* read data_separator */
					/* necessary entry for field separator... */
					if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "data_separator", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcDataSeparator)) != RC_SUCCESS)
					{
						dbg(TRACE,"<init_fileif> missing data_separator in section <%s>", pclCurrentCommand);
						Terminate(30);
					}
					dbg(TRACE,"<init_fileif> data_separator: <%s>",rgTM.prHandleCmd[ilCmdNo].pcDataSeparator);
				}

				/* read entry for field_name */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "field_name", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_FIELD_NAME);
				}
				dbg(TRACE,"<init_fileif> field_name: <%s>",pclTmpBuf);

				/* conver to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* check entry */
				if (!strcmp(pclTmpBuf, "YES"))
					rgTM.prHandleCmd[ilCmdNo].iFieldName = 1;
				else if (!strcmp(pclTmpBuf, "NO"))
					rgTM.prHandleCmd[ilCmdNo].iFieldName = 0;
				else
				{
					dbg(TRACE,"<init_fileif> unknown entry for FieldName in section %s! Use default", pclCurrentCommand);
					rgTM.prHandleCmd[ilCmdNo].iFieldName = 0;
				}

				/* data line prefix */
				if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentCommand, "data_line_prefix",CFG_STRING,rgTM.prHandleCmd[ilCmdNo].pcDataLinePrefix)) != RC_SUCCESS)
				{
					*rgTM.prHandleCmd[ilCmdNo].pcDataLinePrefix=0x00;
				}
				while ((pclBlank=strchr(rgTM.prHandleCmd[ilCmdNo].pcDataLinePrefix,'~'))!=NULL)
				{
					*pclBlank=0x20;
				}
				dbg(TRACE,"<init_fileif> data_line_prefix : <%s>",rgTM.prHandleCmd[ilCmdNo].pcDataLinePrefix);

				/* data line postfix */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "data_line_postfix",CFG_STRING,rgTM.prHandleCmd[ilCmdNo].pcDataLinePostfix)) != RC_SUCCESS)
				{
					*rgTM.prHandleCmd[ilCmdNo].pcDataLinePostfix=0x00;
				}
				while ((pclBlank=strchr(rgTM.prHandleCmd[ilCmdNo].pcDataLinePostfix,'~'))!=NULL)
				{
					*pclBlank=0x20;
				}
				dbg(TRACE,"<init_fileif> data_line_postfix: <%s>",rgTM.prHandleCmd[ilCmdNo].pcDataLinePostfix);

				/* end of line entry */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "eol", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
				{
					strcpy(pclTmpBuf, sDEFAULT_EOL);
				}
				dbg(TRACE,"<init_fileif> eol: <%s>",pclTmpBuf);

				/* convert to uppercase */
				StringUPR((UCHAR*)pclTmpBuf);

				/* set to 0x00 */
				memset((void*)rgTM.prHandleCmd[ilCmdNo].pcEndOfLine, 0x00, iMIN);

				/* for all end of line characters... */
				if ((j = GetNoOfElements(pclTmpBuf, cCOMMA)) < 0)
				{
					dbg(TRACE,"<init_fileif> GetNoOfElements (eol) returns: %d", j);
					Terminate(30);
				}

				/* set number of EOL-Characters... */
				rgTM.prHandleCmd[ilCmdNo].iNoEOLCharacters = j;

				/* get all EOL-Characters... */
				for (i=0; i<j; i++)
				{
					if ((pclPtr = GetDataField(pclTmpBuf, i, cCOMMA)) == NULL)
					{
						dbg(TRACE,"<init_fileif> Cannot read data field number %d", i);
						Terminate(30);
					}
					else
					{
						if (IsCR(pclPtr))
							rgTM.prHandleCmd[ilCmdNo].pcEndOfLine[i] = 0x0D;
						else if (IsLF(pclPtr))
							rgTM.prHandleCmd[ilCmdNo].pcEndOfLine[i] = 0x0A;
						else
						{
							dbg(TRACE,"<init_fileif> unkown eol-character %s", pclPtr);
							Terminate(30);
						}
					}
				}

				/* entry for comment_sign... */
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "comment_sign", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].pcCommentSign)) != RC_SUCCESS)
				{
					strcpy(rgTM.prHandleCmd[ilCmdNo].pcCommentSign,
																		sDEFAULT_COMMENT);
					dbg(TRACE,"<init_fileif> no comment_sign! Use default <%s>",rgTM.prHandleCmd[ilCmdNo].pcCommentSign);
				}

				/* save current command... */
				strcpy(rgTM.prHandleCmd[ilCmdNo].pcCommand, pclCurrentCommand);

				/* the SQL-Keywords... */
				memset((void*)pclAllSQLKeywords, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "sql_keywords", CFG_STRING, pclAllSQLKeywords)) != RC_SUCCESS)
				{
					rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords = 0;
					rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords = NULL;
				}
				else
				{
					/* ACHTUNG: kein StringUPR() ... */
					if ((rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords = GetNoOfElements(pclAllSQLKeywords, (char)',')) < 0)
					{
						dbg(TRACE,"<init_fileif> GetNoOfElements (SQL-Keywords) returns: %d", rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords);
						rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords = 0;
						rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords = NULL;
						Terminate(30);
					}

					/* get memory */
					if ((rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords = (char**)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords*sizeof(char*)))) == NULL)
					{
						dbg(TRACE,"<init_fileif> malloc failure (SQL-Keywords)..");
						rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords = NULL;
						Terminate(30);
					}

					/* over all keywords... */
					for (ilCurKeyword=0; ilCurKeyword<rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords; ilCurKeyword++)
					{
						/* get n.th element */
						if ((pclPtr = GetDataField(pclAllSQLKeywords, (UINT)ilCurKeyword, cCOMMA)) == NULL)
						{
							dbg(TRACE,"<init_fileif> GetDataField (SQL-Keywords) returns NULL");
							Terminate(30);
						}

						/* store it */
						if ((rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords[ilCurKeyword] = strdup(pclPtr)) == NULL)
						{
							dbg(TRACE,"<init_fileif> strdup (SQL-Keywords) returns NULL");
							rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords[ilCurKeyword] = NULL;
							Terminate(30);
						}
					}
				}

				/* for all tables */
				memset((void*)pclAllTables, 0x00, iMIN_BUF_SIZE);
				if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentCommand, "tables", CFG_STRING, pclAllTables)) != RC_SUCCESS)
				{
					dbg(TRACE,"<init_fileif> missing tables in section %s", pclCurrentCommand);
					Terminate(30);
				}
				dbg(TRACE,"<init_fileif> tables: <%s>",pclAllTables);

				/* convert to uppercase */
				StringUPR((UCHAR*)pclAllTables);

				/* calculate number of tables for this command... */
				if ((rgTM.prHandleCmd[ilCmdNo].iNoOfTables =
										GetNoOfElements(pclAllTables, cCOMMA)) < 0)
				{
					dbg(TRACE,"<init_fileif> GetNoOfElements (number of tables) returns: %d", rgTM.prHandleCmd[ilCmdNo].iNoOfTables);
					Terminate(30);
				}

				/* get memory for it... */
				if ((rgTM.prHandleCmd[ilCmdNo].prTabDef = (TabDef*)calloc(1,rgTM.prHandleCmd[ilCmdNo].iNoOfTables*sizeof(TabDef))) == NULL)
				{
					dbg(TRACE,"<init_fileif> not enough memory to allocate TabDef");
					rgTM.prHandleCmd[ilCmdNo].prTabDef = NULL;
					Terminate(30);
				}

				/* read all table entries... */
				/********************/
				/* SUBSECTION-START */
				/********************/
				for (ilTableNo=0; ilTableNo<rgTM.prHandleCmd[ilCmdNo].iNoOfTables; ilTableNo++)
				{
					/* clear memory for it */
					memset((void*)pclCurrentTable, 0x00, iMIN_BUF_SIZE);

					if ((pclPtr = GetDataField(pclAllTables, ilTableNo, cCOMMA)) == NULL)
					{
						dbg(TRACE,"<init_fileif> error reading table entry %d", ilTableNo);
						Terminate(30);
					}
					else
					{
						/*---------------------------------------------------------*/
						/*---------------------------------------------------------*/
						/*---------------------------------------------------------*/

						/* save current table name */
						strcpy(pclCurrentTable, pclPtr);
						dbg(TRACE,"<init_fileif> ### NOW INIT SUBSECTION");
						dbg(TRACE,"<init_fileif> ### -------------------");
						dbg(TRACE,"<init_fileif> ### Name: <%s>",pclCurrentTable);

						/* store section name in structure */
						strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcSectionName, pclCurrentTable);

						/* get fieldnames for mapping UTC to localtime */
						memset((void*)pclAllUtcToLocal, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "map_utc_to_localtime", CFG_STRING, pclAllUtcToLocal)) != RC_SUCCESS)
						{
							/* can't find entry */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfUtcToLocal = 0;
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcMapUtcToLocal = NULL;
						}
						else
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclAllUtcToLocal);

							/* count fields for mapping time */
							if ((ilNoUtcToLocal = GetNoOfElements(pclAllUtcToLocal, ',')) < 0)
							{
								dbg(TRACE,"<init_fileif> ### GetNoOfElements (UtcToLocal) returns: %d", ilNoUtcToLocal);
								Terminate(30);
							}

							/* set internal counter */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfUtcToLocal = ilNoUtcToLocal;

							/* get memory for pointer to fields... */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcMapUtcToLocal = (char**)calloc(1,(size_t)(ilNoUtcToLocal*sizeof(char*)))) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### UtcToLocal-malloc error...");
								Terminate(30);
							}

							/* for all fields */
							for (ilCurUtc=0; ilCurUtc<ilNoUtcToLocal; ilCurUtc++)
							{
								/* get n.th element */
								if ((pclS = GetDataField(pclAllUtcToLocal, ilCurUtc, ',')) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### GetDataField (pclAllUtcToLocal) returns NULL");
									Terminate(30);
								}

								/* store n.th element */
								if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcMapUtcToLocal[ilCurUtc] = strdup(pclS)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### strdup(UtcToLocal) returns NULL");
									Terminate(30);
								}
							}
						}
						dbg(TRACE,"<init_fileif> ### ------- UTC TO LOCAL -------------");
						/*dbg(DEBUG,"<init_fileif> ### found %d entries", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfUtcToLocal);*/
						for (ilCurUtc=0; ilCurUtc<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfUtcToLocal; ilCurUtc++)
						{
							dbg(TRACE,"<init_fileif> ### Field: <%s> will be in LOCAL-time!", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcMapUtcToLocal[ilCurUtc]);
						}
						/*dbg(DEBUG,"<init_fileif> ###  ------  UTC TO LOCAL END -------");*/

						/* get entry for 3- or 4-letter-code VIA in VIAL */
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "via", CFG_INT, (char*)&rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34)) != RC_SUCCESS)
						{
							/* can't find entry -> use 3-letter-code */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34 = 3;
						}
						else
						{
							dbg(DEBUG,"<init_fileif> ### found via: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34);
							/* check entry */
							if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34 != 3 && rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34 != 4)
							{
								dbg(TRACE,"<init_fileif> ### unknown entry for via-use 3-letter-code");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34 = 3;
							}
						}
						dbg(DEBUG,"<init_fileif> ### use %d-letter-code for via", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iVia34);

						/* data to search in shared memory */
						memset((void*)pclAllSyslib, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "syslib", CFG_STRING, pclAllSyslib)) != RC_SUCCESS)
						{
							/* don't use syslib data */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib =0;
						}
						else
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclAllSyslib);

							/* calculate number of syslib strings */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib = GetNoOfElements(pclAllSyslib, (char)'"')) < 0)
							{
								dbg(TRACE,"<init_fileif> ### GetNoOfElements(syslib) returns: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib);
								Terminate(30);
							}

							/* delete one element */
							dbg(DEBUG,"<init_fileif> ### found %d iNoSyslib..", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib);
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib--;
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib /= 2;
							dbg(DEBUG,"<init_fileif> ### now %d iNoSyslib..", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib);

							/* get memory for all syslib entries... */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib = (SysLib*)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib*sizeof(SysLib)))) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### syslib malloc failure");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib = NULL;
								Terminate(30);
							}

							/* separate all syslib entries... */
							for (ilSyslibNo=0; ilSyslibNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoSyslib; ilSyslibNo++)
							{
								/* get it */
								memset((void*)pclCurrentSyslib, 0x00, iMIN_BUF_SIZE);
								if ((ilRC = GetAllBetween(pclAllSyslib, '"', ilSyslibNo, pclCurrentSyslib)) < 0)
								{
									/* we have exactly one entry */
									dbg(TRACE,"<init_fileif> ### GetAllBetween returns: %d", ilRC);
									Terminate(30);
								}

								/* we have exactly one entry */
								if ((ilSysTmpMax = GetNoOfElements(pclCurrentSyslib, ',')) < 0)
								{
									dbg(TRACE,"<init_fileif> ### GetNoOfElem. (SysTmp) returns: %d", ilSysTmpMax);
									Terminate(30);
								}

								/* check it */
								if (ilSysTmpMax != 4)
								{
									dbg(TRACE,"<init_fileif> ### ilSysTmpMax is not 4");
									Terminate(30);
								}

								/* over all entries... */
								for (ilSysTmpNo=0; ilSysTmpNo<ilSysTmpMax; ilSysTmpNo++)
								{
									/* get next field */
									if ((pclPtr = GetDataField(pclCurrentSyslib, (UINT)ilSysTmpNo, ',')) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### GetDataField (syslib) returns NULL");
										Terminate(30);
									}

									/* store it... */
									switch (ilSysTmpNo)
									{
										case 0:
											strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib[ilSyslibNo].pcTabName, pclPtr);
											break;

										case 1:
											strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib[ilSyslibNo].pcTabField, pclPtr);
											break;

										case 2:
											strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib[ilSyslibNo].pcIField, pclPtr);
											break;

										case 3:
											strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSysLib[ilSyslibNo].pcResultField, pclPtr);
											break;
									}
								}
							}
						}

						/* lines to write */
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "lines", CFG_INT, (char*)&rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iLines)) != RC_SUCCESS)
						{
							/* all lines... */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iLines = 0;
						}

						/* comment for this table */
						if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentTable, "comment", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcComment)) != RC_SUCCESS)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseComment = 0;
						}
						else
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseComment = 1;

						/* send command */
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "snd_cmd", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcSndCmd)) != RC_SUCCESS)
						{
							dbg(TRACE,"<init_fileif> ### missing snd_cmd in section %s", pclCurrentTable);
							Terminate(30);
						}

						/* table name */
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "table", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcTabName)) != RC_SUCCESS)
						{
							dbg(TRACE,"<init_fileif> ### missing (table) name in section %s", pclCurrentTable);
							Terminate(30);
						}

						/* selection */
						if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentTable, "select", CFG_STRING, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcSelection)) != RC_SUCCESS)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseSelection = 0;
						}
						else
							/* set flag */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseSelection = 1;

						/* compare here with flag "use_received_selection"...,
						cannot work with both... */
						if (rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection && rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseSelection)
						{
							dbg(DEBUG,"<init_fileif> ### FLAG: UseReceivedSelection is 1, setting FLAG: UseSelection to 0, cannot work with both...");
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iUseSelection = 0;
						}

						/* next the fields for this tables */
						memset((void*)pclAllFields, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "fields", CFG_STRING, pclAllFields)) != RC_SUCCESS)
						{
							dbg(TRACE,"<init_fileif> ### missing fields in section %s", pclCurrentTable);
							Terminate(30);
						}

						/* convert to uppercase */
						StringUPR((UCHAR*)pclAllFields);

						/* get number of fields... */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields = GetNoOfElements(pclAllFields, cCOMMA)) < 0)
						{
							dbg(TRACE,"<init_fileif> ### GetNoOfElements (fields) returns: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields);
							Terminate(30);
						}

						/* get memory for pointer to timestamp string... */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFieldTimestampFormat = (char**)calloc(1,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(char*))) == NULL)
						{
							dbg(TRACE,"<init_fileif> ### cannot allocate memory for pointer to timestamp formats strings");
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFieldTimestampFormat = NULL;
							Terminate(30);
						}

						/* get memory for pointer to string... */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields = (char**)calloc(1,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(char*))) == NULL)
						{
							dbg(TRACE,"<init_fileif> ### cannot allocate memory for pointer to strings");
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields = NULL;
							Terminate(30);
						}

						/* set flag for via's... */
						rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iViaInFieldList = 0;
						rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iAddVialVian = 1;
						if ((strstr(pclAllFields, "VIAN")!=NULL) || (strstr(pclAllFields, "VIAL")!=NULL))
						{
						  rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iAddVialVian = 0;
							dbg(TRACE,"<init_fileif> ### found VIAN/VIAL entry in fieldlist, VIA1 to VIA7-entries disabled!!!!");
						}

						for (ilFieldNo=0, ilCurConst=0; ilFieldNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilFieldNo++)
						{
							/*------------------------------------------------------*/
							/*------------------------------------------------------*/
							/*------------------------------------------------------*/
							if ((pclPtr = GetDataField(pclAllFields,
																	ilFieldNo, cCOMMA)) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### Cannot read field number %d", i);
								Terminate(30);
							}
							else
							{
								/* check this field for VIAx... */
								/*if (!strncmp(pclPtr, "VIA", 3) && strncmp(pclPtr, "VIAN", 4) && strncmp(pclPtr, "VIAL", 4))*/
								if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iAddVialVian == 1)
								{
									if (strncmp(pclPtr, "VIX", 3)==0)
									{
										/* here we found VIAx in field list... */
										rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iViaInFieldList = 1;
										dbg(TRACE,"<init_fileif> ### found VIAx-field in fieldlist! Enabling VIAx-handling!");
									}
								}

								/* check it for SQL-Keywords... */
								for (ilCurKeyword=0; ilCurKeyword<rgTM.prHandleCmd[ilCmdNo].iNoOfSQLKeywords; ilCurKeyword++)
								{
									if (strstr(pclPtr, rgTM.prHandleCmd[ilCmdNo].pcSQLKeywords[ilCurKeyword]) != NULL)
									{
										/* search for ';' and replace with ',' */
										pclS = pclPtr;
										while (*pclS)
										{
											if (*pclS == ';')
												*pclS = ',';
											pclS++;
										}
									}
								}

								/* copy to structure... */
								if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldNo] = strdup(pclPtr)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### can't duplicate string %s", pclPtr);
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldNo] = NULL;
									Terminate(30);
								}

								/* build timestamp keyword... */
								sprintf(pclFieldTimestampFormat, "field%d_timestamp_format", ilFieldNo);
								StringLWR((UCHAR*)pclFieldTimestampFormat);
								if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, pclFieldTimestampFormat, CFG_STRING, pclTimestampFormat)) != RC_SUCCESS)
								{
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFieldTimestampFormat[ilFieldNo] = NULL;
								}
								else
								{
									dbg(DEBUG,"<init_fileif> ### found format: <%s>", pclTimestampFormat);
									if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFieldTimestampFormat[ilFieldNo] = strdup(pclTimestampFormat)) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### calloc(1,) error!");
										Terminate(30);
									}
								}

								/* is this field constant? */
								if (!strncmp(pclPtr, "CONSTANT_", 9) &&
									 ilCurConst < 10)
								{
									/* yes thats a constant */
									StringLWR((UCHAR*)pclPtr);

									/* get data for this constant */
									if ((ilRC = iGetConfigRow(pcgCfgFile, pclCurrentTable, pclPtr, CFG_STRING, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcConst[ilCurConst++])) != RC_SUCCESS)
									{
										dbg(TRACE,"<init_fileif> ### missing: <%s>", pclPtr);
										memset((void*)rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcConst[ilCurConst-1], 0x00, iMIN);
									}
									else
										dbg(TRACE,"<init_fileif> ### found const: <%s> at position: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcConst[ilCurConst-1], ilCurConst-1);
								}
							}
						}

						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/* next all for SUBSTR... */
						/* get memory for  SUBSTR-Structure... */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr = (SubStr*)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(SubStr)))) == NULL)
						{
							dbg(TRACE,"<init_fileif> ### SUBSTR malloc failure...");
							Terminate(30);
						}

						/* set default for substr... */
						for (ilCurSubStr=0; ilCurSubStr<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilCurSubStr++)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iUseSubStr = 0;
						}

						/* get substr entries... */
						memset((void*)pclAllSubStr, 0x00, iMIN_BUF_SIZE);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "substr", CFG_STRING, pclAllSubStr)) == RC_SUCCESS)
						{
							/* count number */
							ilNoOfSubStr = GetNoOfElements(pclAllSubStr, ',');

							/* over all elements */
							for (ilCurSubStr=0; ilCurSubStr<ilNoOfSubStr; ilCurSubStr++)
							{
								/* get n.th element */
								if ((pclS = GetDataField(pclAllSubStr, ilCurSubStr, ',')) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### GetDataField (pclAllSubStr) returns NULL");
									Terminate(30);
								}
								dbg(DEBUG,"<init_fileif> ### substr-element: <%s>", pclS);

								/* only if we received data ... */
								if (strlen(pclS))
								{
									/* store n.th element */
									strcpy(pclCurSubStr, pclS);

									/* get starting position */
									if ((pclS = GetDataField(pclCurSubStr, 0, ';')) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### GetDataField (Start Pos) returns NULL");
										Terminate(30);
									}

									/* store start pos in structure... */
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iStartPos = atoi(pclS);

									/* get number of bytes */
									if ((pclS = GetDataField(pclCurSubStr, 1, ';')) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### GetDataField (number of bytes) returns NULL");
										Terminate(30);
									}

									/* store start pos in structure... */
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iBytes = atoi(pclS);

									/* set used flag... */
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iUseSubStr = 1;
								}
							}
						}

						if (debug_level == DEBUG)
						{
										/* only for debug */
										for (ilCurSubStr=0; ilCurSubStr<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilCurSubStr++)
										{
											if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iUseSubStr == 1)
											{
												dbg(DEBUG,"<init_fileif> ### SUBSTR(%s,%d,%d)", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilCurSubStr], rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iStartPos, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prSubStr[ilCurSubStr].iBytes);
											}
										}
						}

						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/* next data_alignment for this */
						/* get memory for data alignment array */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcDataAlignment = (char*)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(char)))) == NULL)
						{
							dbg(TRACE,"<init_fileif> ### cannot allocate memory for data alignment");
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcDataAlignment = NULL;
							Terminate(30);
						}

						/* set defaults for data alignment */
						/* default is L (left alignment) */
						for (ilCurDataAlign=0; ilCurDataAlign<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilCurDataAlign++)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcDataAlignment[ilCurDataAlign] = cLEFT;
						}

						/* read cfg-entry for this */
						memset((void*)pclDataCounter, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "data_alignment", CFG_STRING, pclDataCounter)) == RC_SUCCESS)
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclDataCounter);

							dbg(DEBUG,"<init_fileif> ### found alignment: <%s>", pclDataCounter);
							/* count number of elements... */
							if ((ilDataAlignCnt = GetNoOfElements(pclDataCounter, cCOMMA)) < 0)
							{
								/* error counting elements */
								dbg(TRACE,"<init_fileif> ### GetNoOfElements (data alignment) returns: %d", ilDataAlignCnt);
								Terminate(30);
							}
							dbg(DEBUG,"<init_fileif> ### found %d alignment entries", ilDataAlignCnt);

							for (ilCurDataAlign=0; ilCurDataAlign<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilCurDataAlign++)
							{
								/* get n.th element... */
								if ((pclS = GetDataField(pclDataCounter, ilCurDataAlign, cCOMMA)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### GetDataField (data alignment) returns NULL");
									Terminate(30);
								}
								dbg(DEBUG,"<init_fileif> ### setting %d to %c", ilCurDataAlign, pclS[0]);

								/* to structure... */
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcDataAlignment[ilCurDataAlign] = pclS[0];
							}
						}

						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/*------------------------------------------------------*/
						/* next data length for this table */
						/* read data_length to int array */
						memset((void*)pclDataCounter, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "data_length", CFG_STRING, pclDataCounter)) != RC_SUCCESS)
						{
							if (IsFix(rgTM.prHandleCmd[ilCmdNo].iWriteData))
							{
								dbg(TRACE,"<init_fileif> ### missing data_length in section %s", pclCurrentTable);
								Terminate(30);
							}
							else
							{
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iDataLengthCounter = 0;
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piDataLength = NULL;
							}
						}
						else
						{
							/* get number of elements */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iDataLengthCounter = GetNoOfElements(pclDataCounter, cSEPARATOR)) < 0)
							{
								dbg(TRACE,"<init_fileif> ### can't read number of data_length elements");
								Terminate(30);
							}
							else
							{
								/* get memory for int array */
								if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piDataLength = (UINT*)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iDataLengthCounter*sizeof(int)))) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### cannot allocate memory for int array");
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piDataLength = NULL;
									Terminate(30);
								}

								/* to array */
								for (i=0; i<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iDataLengthCounter; i++)
								{
									if ((pclPtr =
											GetDataField(pclDataCounter, i, cSEPARATOR)) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### cannot read data field no %d", i);
										Terminate(30);
									}

									/* store length... */
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piDataLength[i] = atoi(pclPtr);
								}
							}
						}

						/* next the assignment for this tables */
						memset((void*)pclAllAssignments, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "assignment", CFG_STRING, pclAllAssignments)) != RC_SUCCESS)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoAssign =0;
						}
						else
						{
							/* convert to uppercase */
							/* JWE 20050208: No reason found for automated uppercase conversion of assigments */
							/*StringUPR((UCHAR*)pclAllAssignments);*/

							/* found an assignment */
							/* get number of entriey for assignment */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoAssign = GetNoOfElements(pclAllAssignments, cCOMMA)) < 0)
							{
								dbg(TRACE,"<init_fileif> ### GetNoOfElements (assignment) returns: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoAssign);
								Terminate(30);
							}

							/* memory for assignment structure ... */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign = (Assign*)calloc(1,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoAssign*sizeof(Assign))) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### cannot allocate memory for assignment");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign = NULL;
								Terminate(30);
							}

							dbg(TRACE,"<init_fileif> ### ------- FIELD-NAME ASSIGNM. ------");
							/* read all assignment entries... */
							for (ilAssignNo=0; ilAssignNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoAssign; ilAssignNo++)
							{
								/* get the entries... */
								if ((pclPtr = GetDataField(pclAllAssignments, ilAssignNo, cCOMMA)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### cannot read assignment entry %d", i);
									Terminate(30);
								}
								else
								{
									/* separate the assignment */
									if ((ilRC = SeparateIt(pclPtr, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcDBField, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcIFField, cSAME)) != 0)
									{
										dbg(TRACE,"<init_fileif> ### SeparateIt returns: %d", ilRC);
										Terminate(30);
									}
									else
									{
										dbg(TRACE,"<init_fileif> ### Field: <%s> maps to <%s>"
											,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcDBField
											,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcIFField);
									}

									/* store length of fields... */
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].iDBFieldSize = strlen(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcDBField);

									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].iIFFieldSize = strlen(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prAssign[ilAssignNo].pcIFField);
								}
							}
						}

						/* next the conditions for this tables */
						memset((void*)pclAllConditions, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "condition", CFG_STRING, pclAllConditions)) != RC_SUCCESS)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition = 0;
						}
						else
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclAllConditions);

							/* get number of entries for assignment */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition = GetNoOfElements(pclAllConditions, cCOMMA)) < 0)
							{
								dbg(TRACE,"<init_fileif> ### GetNoOfElements (Conditions) returns: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition);
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition = 0;
								Terminate(30);
							}

							/* memory for assignment structure ... */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition = (Conditions*)calloc(1,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition*sizeof(Conditions))) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### cannot allocate memory for conditions");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition = NULL;
								Terminate(30);
							}

							/* read all assignment entries... */
							for (ilConditionNo=0; ilConditionNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoCondition; ilConditionNo++)
							{
								/* get the entries... */
								if ((pclPtr = GetDataField(pclAllConditions, ilConditionNo, cCOMMA)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### cannot read Condition entry %d", i);
									Terminate(30);
								}
								else
								{
									/* separate the assignment */
									if ((ilRC = SeparateIt(pclPtr, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition[ilConditionNo].pcFieldName, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition[ilConditionNo].pcCondition, cSAME)) != 0)
									{
										dbg(TRACE,"<init_fileif> ### SeparateIt (condition) returns: %d", ilRC);
										Terminate(30);
									}

									/* write position if this field in field list */
									for (ilFieldPos=0; ilFieldPos<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilFieldPos++)
									{
										if (!strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldPos], rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition[ilConditionNo].pcFieldName))
										{
											/* this is the field */
											rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prCondition[ilConditionNo].iFieldPosition = ilFieldPos;
										}
									}
								}
							}
						}

						/* next the ignore fields for this tables */
						memset((void*)pclAllIgnore, 0x00, iMAXIMUM);
						if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, "ignore_fields", CFG_STRING, pclAllIgnore)) != RC_SUCCESS)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore = 0;
						}
						else
						{
							/* convert to uppercase */
							StringUPR((UCHAR*)pclAllIgnore);

							/* get number of entry for assignment */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore = GetNoOfElements(pclAllIgnore, cCOMMA)) < 0)
							{
								dbg(TRACE,"<init_fileif> ### GetNoOfElements (Ignore fields) returns: %d", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore);
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore = 0;
								Terminate(30);
							}

							/* get memory for pointer to string... */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcIgnoreFields = (char**)calloc(1,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore*sizeof(char*))) == NULL)
							{
								dbg(TRACE,"<init_fileif> ### cannot allocate memory for pointer to ignore strings");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcIgnoreFields = NULL;
								Terminate(30);
							}

							for (ilIgnoreFieldNo=0; ilIgnoreFieldNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoOfIgnore; ilIgnoreFieldNo++)
							{
								if ((pclPtr = GetDataField(pclAllIgnore,
														ilIgnoreFieldNo, cCOMMA)) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### Cannot read ignore field number %d", i);
									Terminate(30);
								}
								else
								{
									/* copy to structure... */
									if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcIgnoreFields[ilIgnoreFieldNo] = strdup(pclPtr)) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### can't duplicate Ignore string %s", pclPtr);
										rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcIgnoreFields[ilFieldNo] = NULL;
										Terminate(30);
									}

									dbg(DEBUG,"<init_fileif> ### Ignore this field: <%s>", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcIgnoreFields[ilIgnoreFieldNo]);
								}
							}
						}

						/* get memory for mapping (default for all fields of table) */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap = (DataMap**)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(DataMap*)))) == NULL)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap = NULL;
							dbg(TRACE,"<init_fileif> ### malloc failure (data map)");
							Terminate(30);
						}

						/* memory for counter */
						if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piNoMapData = (UINT*)calloc(1,(size_t)(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields*sizeof(UINT)))) == NULL)
						{
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piNoMapData = NULL;
							dbg(TRACE,"<init_fileif> ### malloc failure (no data map)");
							Terminate(30);
						}

						/* read (for each field) mapping for data */
						for (ilFieldNo=0; ilFieldNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].iNoFields; ilFieldNo++)
						{
							/* build keyword for this table section */
							memset ((void*)pclMapKeyWord, 0x00, iMIN);
							sprintf(pclMapKeyWord,"map_%s_data", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldNo]);
							StringLWR((UCHAR*)pclMapKeyWord);

							/* set pointer to NULL */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo] = NULL;

							/* set counter to 0 */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piNoMapData[ilFieldNo] = 0;

							/* read all data for this */
							memset((void*)pclMappingData, 0x00, iMIN_BUF_SIZE);
							if ((ilRC = iGetConfigEntry(pcgCfgFile, pclCurrentTable, pclMapKeyWord, CFG_STRING, pclMappingData)) == RC_SUCCESS)
							{
								dbg(DEBUG,"<init_fileif> ### -- FIELD <%s> MAPPING START --", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldNo]);

								/* count number of new elements */
								if ((ilMaxMapData = GetNoOfElements(pclMappingData, ',')) < 0)
								{
									dbg(TRACE,"<init_fileif> ### GetNoOfElements (MappingData) returns: %d", ilMaxMapData);
									Terminate(30);
								}

								/* add new element */
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].piNoMapData[ilFieldNo] = ilMaxMapData;

								/* get memory for new elements */
								if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo] = (DataMap*)calloc(1,(size_t)(ilMaxMapData*sizeof(DataMap)))) == NULL)
								{
									dbg(TRACE,"<init_fileif> ### malloc failure data mapping");
									Terminate(30);
								}

								/* store new elements */
								for (ilCurMapData=0, ilOtherData=iORG_DATA; ilCurMapData<ilMaxMapData; ilCurMapData++)
								{
									/* get element */
									if ((pclPtr = GetDataField(pclMappingData, ilCurMapData, ',')) == NULL)
									{
										dbg(TRACE,"<init_fileif> ### GetDataField (mapping) returns NULL");
										Terminate(30);
									}

									/* separate both elements */
									if ((ilRC = SeparateIt(pclPtr, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcOrgData, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcNewData, cSAME)) < 0)
									{
										dbg(TRACE,"<init_fileif> ### SeparateIt returns: %d", ilRC);
										Terminate(30);
									}

									/* set other data flag */
									if (!strcmp("OTHER_DATA", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcOrgData) && !strcmp("NULL", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcNewData))
									{
										dbg(DEBUG,"<init_fileif> ### setting OTHER_DATA to NULL_DATA");
										ilOtherData = iNULL_DATA;
									} else if (!strcmp("OTHER_DATA", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcOrgData) && !strcmp("BLANK", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcNewData))
									{
										dbg(DEBUG,"<init_fileif> ### setting OTHER_DATA to BLANK_DATA");
										ilOtherData = iBLANK;
									}
								}

								/* set other data flag */
								for (ilCurMapData=0; ilCurMapData<ilMaxMapData; ilCurMapData++)
								{
									rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].iOtherData = ilOtherData;

									dbg(DEBUG,"<init_fileif> ### map from <%s> to <%s>", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcOrgData, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].prDataMap[ilFieldNo][ilCurMapData].pcNewData);
								}
								dbg(DEBUG,"<init_fileif> ### -- FIELD <%s> MAPPING END --", rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcFields[ilFieldNo]);
							}
						}
					}
					/* ADDED BY BERNI */
					dbg(TRACE,"<init_fileif> ### Section <%d> <%s> Table <%d> <%s> -- Initialized",ilCmdNo,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcSectionName,ilTableNo,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTableNo].pcTabName);
					/* END BERNI */
				}

				/* only for debugging */
				dbg(TRACE,"<init_fileif> ######### END   <%s> #########", pclCurrentCommand);
				dbg(TRACE,"");
			}
		}
	}

	/* get mod-id of router */
	if ((igRouterID = tool_get_q_id("router")) == RC_NOT_FOUND ||
		  igRouterID == RC_FAIL)
	{
		dbg(TRACE,"<init_fileif> tool_get_q_id(router) returns: %d", igRouterID);
		Terminate(30);
	}

	/* bye bye */
	return RC_SUCCESS;
}


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset(void)
{
	int	i;
	int	j;
	int	k;
	int	l;
	int	ilRC = RC_SUCCESS;

	dbg(TRACE,"<Reset> now resetting");

	/* to inform status-handler about startup time */
	dbg(DEBUG,"<Reset> calling SendStatus(..STOP..)");
	if ((ilRC = SendStatus("FILEIO", "", "STOP", 1, "", "", "")) != RC_SUCCESS)
	{
		/* bloody fuckin shit */
		dbg(TRACE,"<init_fileif> SendStatus returns: %d", ilRC);
		Terminate(30);
	}

	/* free all memory */
	if (prgItem != NULL)
	{
		free(prgItem);
		prgItem = NULL;
	}

	for (i=0; i<rgTM.iNoCommands; i++)
	{
		for (j=0; j<rgTM.prHandleCmd[i].iNoOfTables; j++)
		{
			for (k=0; k<rgTM.prHandleCmd[i].prTabDef[j].iNoFields; k++)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].pcFields[k] != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcFields[k]);
					rgTM.prHandleCmd[i].prTabDef[j].pcFields[k] = NULL;
				}
				if (rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat[k] != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat[k]);
					rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat[k] = NULL;
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].pcFields != NULL)
			{
				free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcFields);
				rgTM.prHandleCmd[i].prTabDef[j].pcFields = NULL;
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat != NULL)
			{
				free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat);
				rgTM.prHandleCmd[i].prTabDef[j].pcFieldTimestampFormat = NULL;
			}
			for (k=0; k<rgTM.prHandleCmd[i].prTabDef[j].iNoOfUtcToLocal; k++)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal[k] != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal[k]);
					rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal[k] = NULL;
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal != NULL)
			{
				free ((void*)rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal);
				rgTM.prHandleCmd[i].prTabDef[j].pcMapUtcToLocal = NULL;
			}
			if (IsFix(rgTM.prHandleCmd[i].iWriteData))
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].iDataLengthCounter > 0)
				{
					if (rgTM.prHandleCmd[i].prTabDef[j].piDataLength != NULL)
					{
						free ((void*)rgTM.prHandleCmd[i].prTabDef[j].piDataLength);
						rgTM.prHandleCmd[i].prTabDef[j].piDataLength = NULL;
					}
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].iNoAssign > 0)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].prAssign != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].prAssign);
					rgTM.prHandleCmd[i].prTabDef[j].prAssign = NULL;
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].iNoSyslib > 0)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].prSysLib != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].prSysLib);
					rgTM.prHandleCmd[i].prTabDef[j].prSysLib = NULL;
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].iNoCondition > 0)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].prCondition != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].prTabDef[j].prCondition);
					rgTM.prHandleCmd[i].prTabDef[j].prCondition = NULL;
				}
			}
			for (k=0; k<rgTM.prHandleCmd[i].prTabDef[j].iNoFields; k++)
			{
				if (rgTM.prHandleCmd[i].prTabDef[j].piNoMapData[k] > 0)
				{
					if (rgTM.prHandleCmd[i].prTabDef[j].prDataMap[k] != NULL)
					{
						free((void*)rgTM.prHandleCmd[i].prTabDef[j].prDataMap[k]);
						rgTM.prHandleCmd[i].prTabDef[j].prDataMap[k] = NULL;
					}
				}
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].piNoMapData != NULL)
			{
				free ((void*)rgTM.prHandleCmd[i].prTabDef[j].piNoMapData);
				rgTM.prHandleCmd[i].prTabDef[j].piNoMapData = NULL;
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].prDataMap != NULL)
			{
				free((void*)rgTM.prHandleCmd[i].prTabDef[j].prDataMap);
				rgTM.prHandleCmd[i].prTabDef[j].prDataMap = NULL;
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].pcDataAlignment != NULL)
			{
				free((void*)rgTM.prHandleCmd[i].prTabDef[j].pcDataAlignment);
				rgTM.prHandleCmd[i].prTabDef[j].pcDataAlignment = NULL;
			}
			if (rgTM.prHandleCmd[i].prTabDef[j].prSubStr != NULL)
			{
				free((void*)rgTM.prHandleCmd[i].prTabDef[j].prSubStr);
				rgTM.prHandleCmd[i].prTabDef[j].prSubStr = NULL;
			}
		}
		if (rgTM.prHandleCmd[i].prTabDef != NULL)
		{
			free ((void*)rgTM.prHandleCmd[i].prTabDef);
			rgTM.prHandleCmd[i].prTabDef = NULL;
		}
		if (rgTM.prHandleCmd[i].iNoOfSQLKeywords > 0)
		{
			for (l=0; l<rgTM.prHandleCmd[i].iNoOfSQLKeywords; l++)
			{
				if (rgTM.prHandleCmd[i].pcSQLKeywords[l] != NULL)
				{
					free ((void*)rgTM.prHandleCmd[i].pcSQLKeywords[l]);
					rgTM.prHandleCmd[i].pcSQLKeywords[l] = NULL;
				}
			}
			if (rgTM.prHandleCmd[i].pcSQLKeywords != NULL)
			{
				free ((void*)rgTM.prHandleCmd[i].pcSQLKeywords);
				rgTM.prHandleCmd[i].pcSQLKeywords = NULL;
			}
		}
		if (rgTM.prHandleCmd[i].pcOldFileContent != NULL)
		{
			free((void*)rgTM.prHandleCmd[i].pcOldFileContent);
			rgTM.prHandleCmd[i].pcOldFileContent = NULL;
			rgTM.prHandleCmd[i].lOldFileLength = -1;
		}
		if (rgTM.prHandleCmd[i].pcFileContent != NULL)
		{
			free((void*)rgTM.prHandleCmd[i].pcFileContent);
			rgTM.prHandleCmd[i].pcFileContent = NULL;
			rgTM.prHandleCmd[i].lFileLength = -1;
		}
	}
	if (rgTM.prHandleCmd != NULL)
	{
		free ((void*)rgTM.prHandleCmd);
		rgTM.prHandleCmd = NULL;
	}

	/* now initialize it... */
	if ((ilRC = init_fileif()) != RC_SUCCESS)
	{
		dbg(TRACE,"init_fileif() in reset returns: %d\n", ilRC);
		Terminate(0);
	}

	/* end of reset */
	dbg(TRACE,"<Reset> returning with: %d", ilRC);

	/* bye bye */
	return ilRC;
}


/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(UINT	ipSleepTime)
{
	int	ilRC;

	dbg(TRACE,"<Terminate> now terminating...");

	/* changes by mos 6 Apr 2000 */
	ilRC = logoff();  /* disconnect from oracle */
  	dbg(TRACE,"terminate: oracle     ... logged off.");
	/* eof changes */

	/* ignore all signals */
	(void) UnsetSignals();

	/* thats better */
	if (ipSleepTime > 0)
	{
		dbg(DEBUG,"<Terminate> sleep %d seconds...", ipSleepTime);
		sleep(ipSleepTime);
	}

	/* to inform status-handler about startup time */
	dbg(DEBUG,"<Terminate> calling SendStatus(..STOP..)");
	if ((ilRC = SendStatus("FILEIO", "", "STOP", 1, "", "", "")) != RC_SUCCESS)
	{
		/* bloody fuckin shit */
		dbg(TRACE,"<init_fileif> SendStatus returns: %d", ilRC);
		Terminate(30);
	}

	/* only a message */
	dbg(TRACE,"<Terminate> now leaving ...");

	/* close logfile */
	CLEANUP;

	/* good bye */
	exit(0);
}


/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/
static void HandleSignal(int ipSig)
{
	if (ipSig != SIGALRM && ipSig != SIGCLD)
		dbg(TRACE,"<HandleSignal> signal <%d> received", ipSig);

	switch(ipSig)
	{
		case SIGCLD:
		case SIGALRM:
			/* don't call SetSignals here */
			igSignalNo = ipSig;
			break;

		default:
			exit(0);
			break;
	}
	return;
}


/******************************************************************************/
/* The handle general error routine                                           */
/******************************************************************************/
static void HandleErr(int ipErr)
{
	dbg(DEBUG,"<HandleErr> calling with ErrorCode: %d", ipErr);
	return;
}


/******************************************************************************/
/* The handle queuing error routine                                           */
/******************************************************************************/
static void HandleQueErr(int pipErr)
{
	switch(pipErr)
	{
	case	QUE_E_FUNC	:	/* Unknown function */
		dbg(TRACE,"<%d> : unknown function",pipErr);
		break;

	case	QUE_E_MEMORY	:	/* Malloc reports no memory */
		dbg(TRACE,"<%d> : malloc failed",pipErr);
		break;

	case	QUE_E_SEND	:	/* Error using msgsnd */
		dbg(TRACE,"<%d> : msgsnd failed",pipErr);
		break;

	case	QUE_E_GET	:	/* Error using msgrcv */
		dbg(TRACE,"<%d> : msgrcv failed",pipErr);
		break;

	case	QUE_E_EXISTS	:
		dbg(TRACE,"<%d> : route/queue already exists ",pipErr);
		break;

	case	QUE_E_NOFIND	:
		dbg(TRACE,"<%d> : route not found ",pipErr);
		break;

	case	QUE_E_ACKUNEX	:
		dbg(TRACE,"<%d> : unexpected ack received ",pipErr);
		break;

	case	QUE_E_STATUS	:
		dbg(TRACE,"<%d> :  unknown queue status ",pipErr);
		break;

	case	QUE_E_INACTIVE	:
		dbg(TRACE,"<%d> : queue is inaktive ",pipErr);
		break;

	case	QUE_E_MISACK	:
		dbg(TRACE,"<%d> : missing ack ",pipErr);
		break;

	case	QUE_E_NOQUEUES	:
		dbg(TRACE,"<%d> : queue does not exist",pipErr);
		break;

	case	QUE_E_RESP	:	/* No response on CREATE */
		dbg(TRACE,"<%d> : no response on create",pipErr);
		break;

	case	QUE_E_FULL	:
		dbg(TRACE,"<%d> : too many route destinations",pipErr);
		break;

	case	QUE_E_NOMSG	:	/* No message on queue */
		dbg(TRACE,"<%d> : no messages on queue",pipErr);
		break;

	case	QUE_E_INVORG	:	/* Mod id by que call is 0 */
		dbg(TRACE,"<%d> : invalid originator=0",pipErr);
		break;

	case	QUE_E_NOINIT	:	/* Queues is not initialized*/
		dbg(TRACE,"<%d> : queues are not initialized",pipErr);
		break;

	case	QUE_E_ITOBIG	:
		dbg(TRACE,"<%d> : requestet itemsize to big ",pipErr);
		break;

	case	QUE_E_BUFSIZ	:
		dbg(TRACE,"<%d> : receive buffer to small ",pipErr);
		break;

	default			:	/* Unknown queue error */
		dbg(TRACE,"<%d> : unknown error",pipErr);
		break;
	}

	/* put message and reset errno */
	dbg(TRACE,"<HandleQueErr> Errno: <%s>", strerror(errno));
   errno = 0;

	/* platz dooo */
	return;
}


/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues(void)
{
	int	ilRC = RC_SUCCESS;
	int	ilBreakOut = FALSE;

	do
	{
		/* get item... */
		ilRC = que(QUE_GETBIG, 0, mod_id, PRIORITY_3, 0,(char*)&prgItem);

		/* set Event pointer */
		prgEvent = (EVENT*)prgItem->text;

		if( ilRC == RC_SUCCESS )
		{
			/* Acknowledge the item */
			ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
			if( ilRC != RC_SUCCESS )
			{
				dbg(DEBUG,"<HandleQueues> calling HandleQueErr");
				HandleQueErr(ilRC);
			}

			switch (prgEvent->command)
			{
				case	HSB_STANDBY:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					break;

				case	HSB_COMING_UP:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					break;

				case	HSB_ACTIVE:
				case	HSB_STANDALONE:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					ilBreakOut = TRUE;
					break;

				case	HSB_ACT_TO_SBY:
					ctrl_sta = prgEvent->command;
					send_message(IPRIO_ASCII,HSB_REQUEST,0,0,NULL);
					break;

				case	HSB_DOWN:
					ctrl_sta = prgEvent->command;
					Terminate(0);
					break;

				case	SHUTDOWN:
					Terminate(0);
					break;

				case	RESET:
					ilRC = Reset();
					break;

				case	EVENT_DATA:
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
					dbg(DEBUG,"<HandleQueues> unknown event");
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
					break;
			}
			if(ilRC != RC_SUCCESS)
			{
				HandleErr(ilRC);
			}
		}
		else
		{
			dbg(DEBUG,"<HandleQueues> calling HandleQueErr");
			HandleQueErr(ilRC);
		}
	} while (ilBreakOut == FALSE);

	/* ciao */
	return;
}

/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData(void)
{
	/* something we need */
	int			i;
	int			ilRC;
	int			ilRC_OrgVal;
	int			ilOk;
	int			ilFlg;
	int			ilType;
	int			ilCmdNo;
	int			ilTabNo;
	int			ilLines;
	int			ilTimeCnt;
	int			ilCurLine;
	int			ilTimeDiff;
	int			ilAssignNo;
	int			ilConditionNo;
	int			ilSecondRecord;
	int			ilAllSecRecords;
	char			pclUtcTime[15];
	char			pclLocalTime[15];
	char			pclTmpKeyWord[iMIN];
	char			pclTmpBuf[iMAXIMUM];
	char			pclFieldBuf[iMAXIMUM];
	char			pclSelectBuf[iMAXIMUM];
	char			*pclS 				= NULL;
	char			*pclTmpPtr		= NULL;
	char			pclSharp = NULL;
/* ADDED BY BERNI */
	char			*pclPtr			= NULL;
	char			pclOutSelection[iMAXIMUM];
	char			pclTwsBuf[32];
/* END BERNI */
	 int	ilLineNumberToWrite;

	/* received */
	BC_HEAD		*prlBCHead 			= NULL;
	CMDBLK		*prlCmdblk 			= NULL;
	char			*pclSelection 		= NULL;
	char			*pclFields			= NULL;
	char			*pclData				= NULL;
/** Changes by MOS  4 July 2000 **/
	CMDBLK          *prlCmdblkreply                      = NULL;
	char			pcldummy[iMIN_BUF_SIZE];
        char                    pclNumtabkey[iMIN_BUF_SIZE];
	char			pclNumtabacnu[iMIN_BUF_SIZE];
	char			pclFtpreplyacnu[iMIN_BUF_SIZE];
	char			pclNumtabStopValue[iMIN_BUF_SIZE]; /*max number for number circle from ftphdl reply*/


	/* send to */
	int			ilLen					= 0;
	EVENT			*prlOutEvent		= NULL;
	BC_HEAD		*prlOutBCHead		= NULL;
	CMDBLK		*prlOutCmdblk		= NULL;

	/* init something */
	ilCmdNo = -1;
	ilTabNo = -1;

	/* set pointer we need */
	prlBCHead = (BC_HEAD*)((char*)prgEvent+sizeof(EVENT));
	if (prlBCHead->rc != RC_SUCCESS)
	{
		/* only a message */
		dbg(TRACE,"<HandleData> BCHead->rc: %d", prlBCHead->rc);
		dbg(TRACE,"<HandleData> ErrorMessage: <%s>", prlBCHead->data);

#ifndef UFIS32
		memset((void*)pclTmpBuf, 0x00, iMAXIMUM);
		sprintf(pclTmpBuf, "%s;%d", prlBCHead->data, prgEvent->originator);
		if ((ilRC = send_message(IPRIO_DB, iFILEIF_ERROR_MESSAGE1, 0, 0, pclTmpBuf)) != RC_SUCCESS)
		{
			dbg(TRACE,"<HandleData> send_message returns: %d", ilRC);
		}
#endif

		/* must set pointer to commandblock-structure... */
		prlCmdblk = (CMDBLK*)((char*)prlBCHead->data +
									 strlen((char*)prlBCHead->data) + 1);


		/* Changes by MOS 3. July 2000 for ftphdl implementation*/
		prlCmdblkreply = (CMDBLK  *) ((char *)prlBCHead->data);
		/* ***** EO Changes ****/

		/* get command- and table number */
		dbg(DEBUG,"<HandleData> tw_start: <%s>", prlCmdblk->tw_start);
		ilCmdNo = atoi(GetDataField(prlCmdblk->tw_start, 0, ','));
		ilTabNo = atoi(GetDataField(prlCmdblk->tw_start, 1, ','));
		dbg(DEBUG,"<HandleData> CMDNO: %d, TABNO: %d", ilCmdNo, ilTabNo);

		/* Changes by MOS 3. July 2000 for ftphdl implementation*/
	        if (strcmp(prlCmdblkreply->command,"FTP") == 0)
        	{
			/* SUCCESS from FTPHDL */
			if (strstr(prlCmdblkreply->data,"RC_SUCCESS")!=NULL) {
				GetDataItem(pclNumtabkey, prlCmdblkreply->tw_start, 1, cCOMMA, "", " \0");
				GetDataItem(pclFtpreplyacnu, prlCmdblkreply->tw_start, 2, cCOMMA, "", " \0");
				GetDataItem(pclNumtabStopValue, prlCmdblkreply->tw_start, 3, cCOMMA, "", " \0");

				if (strlen(pclNumtabkey) > 1) 	{
				/* reply id equals acnu?? */
				GetNextNumbers(pclNumtabkey, pcldummy, 1,"NO_AUTO_INCREASE");
				GetDataItem(pclNumtabacnu, pcldummy, 1, cCOMMA, "", " \0");

				if (strstr(pclFtpreplyacnu,pclNumtabacnu)!=NULL) {
         	       			dbg(DEBUG,"FILEIF HandleData: ftp reply fits: <%s>",pclFtpreplyacnu);
					/* increase number in numtab,check for maximum number */
					if ((atoi(pclFtpreplyacnu)== (atoi(pclNumtabStopValue)))) {
						GetNextNumbers(pclNumtabkey, pcldummy, 1,"MAXOW");
						dbg(TRACE,"FILEIF HandleData:MAXOW with key <%s> and no <%s> and reply <%s>",pclNumtabkey,pcldummy,pclFtpreplyacnu);
					} else {
						if (GetNextNumbers(pclNumtabkey, pcldummy, 1,"")!=RC_SUCCESS) {
							GetNextNumbers(pclNumtabkey, pcldummy, 1,"MAXOW");
							dbg(TRACE,"FILEIF HandleData:NO SUCCESS with key <%s> and no <%s> and reply <%s>",pclNumtabkey,pcldummy,pclFtpreplyacnu);
						} else {
							dbg(DEBUG,"FILEIF HandleData:SUCCESS with key <%s> and no <%s> and reply <%s>",pclNumtabkey,pcldummy,pclFtpreplyacnu);
						}

					}
				} else {
					dbg(TRACE,"<HandleData> different ACNU-ID! FTPHDL=(%s) -- ACNU-ID=(%s)",pclFtpreplyacnu,pclNumtabacnu);
				} /*ids equal??*/

				}
			} else {
				dbg(TRACE,"<HandleData> <%s> FTPDHL not successful for key and file extension %s."
					,prlCmdblkreply->command,prlCmdblkreply->data);

			}
			return RC_SUCCESS;
   	}

		/* check both numbers */
		if (ilCmdNo < 0 || ilTabNo < 0)
		{
			dbg(TRACE,"<HandleData> invalid command and/or table number...");
			Terminate(0);
		}

		/* send answer to client... */
		if (rgTM.prHandleCmd[ilCmdNo].iSendSqlRc)
		{
			ilRC = tools_send_sql_rc(rgTM.prHandleCmd[ilCmdNo].iSqlRCOriginator,
											 prlCmdblk->command,
											 prlCmdblk->obj_name,
											 prlCmdblk->data,
											 pclFields,
											 prlBCHead->data, prlBCHead->rc);
		}

		/* incr. data counter */
		rgTM.prHandleCmd[ilCmdNo].iDataCounter++;

		/* check number of tables */
		if (rgTM.prHandleCmd[ilCmdNo].iDataCounter ==
			 rgTM.prHandleCmd[ilCmdNo].iNoOfTables)
		{
			/* set counter to zero */
			rgTM.prHandleCmd[ilCmdNo].iDataCounter = 0;

			/* send file to other machine... */
			return SendFileToMachine(ilCmdNo, ilTabNo);
		}
		else
			return RC_SUCCESS;
	} /* end RC != RC_SUCCESS */

	/* set pointer to stuctures */
	prlCmdblk 		= (CMDBLK*)prlBCHead->data;
	pclSelection 	= prlCmdblk->data;
	pclFields 		= prlCmdblk->data + strlen(pclSelection) + 1;
	pclData 			= prlCmdblk->data + strlen(pclSelection) + strlen(pclFields) + 2;

	dbg(TRACE,"<HandleData> ------------- START/END ------------");
	dbg(TRACE,"<HandleData> COMMAND <%s>, ORIGINATOR <%d>, TWSTART <%s>"
		,prlCmdblk->command,prgEvent->originator,prlCmdblk->tw_start);
	dbg(DEBUG,"<HandleData> DATA    \n<%s>",pclData);

	/* convert command to uppercase */
	StringUPR((UCHAR*)prlCmdblk->command);

	/* Search Command */
	if ((ilCmdNo = GetCmdNumber(prlCmdblk->command)) != RC_FAIL)
	{
		/* set flag... */
		if (rgTM.prHandleCmd[ilCmdNo].iSendSqlRc)
		{
			rgTM.prHandleCmd[ilCmdNo].iSqlRCOriginator = prgEvent->originator;
		}

		/* here we found a command, send it to router */
		for (ilTabNo=0; ilTabNo<rgTM.prHandleCmd[ilCmdNo].iNoOfTables; ilTabNo++)
		{
			/* length of OutEvent... */
			ilLen = sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK);

			/* if we use selection, add length of string */
			if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iUseSelection || rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection)
			{
				/* if we use selection add 10 * 14 bytes for timestamps, thats the
				maximum we can handle...
				*/
				ilLen += strlen(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSelection) + (10*14);
			}

			/* build fieldbuf */
			memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
			for (i=0; i<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoFields; i++)
			{
				if (!rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iViaInFieldList)
				{
					/* this is without via in field list */
					if (strncmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "CONSTANT_", 9) && strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "FILL_FIELD") && strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "ACTUAL_DATE"))
					{
						strcat(pclFieldBuf, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i]);
						strcat(pclFieldBuf, ",");
					}
				}
				else
				{
					/* here we must handle VIAL and VIAN */
					if (strncmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "VIX", 3) && strncmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "VIAN", 4) && strncmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "VIAL", 4) && strncmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "CONSTANT_", 9) && strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "FILL_FIELD") && strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], "ACTUAL_DATE"))
					{
						/* add only fields witch is not a VIAx */
						strcat(pclFieldBuf, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i]);
						strcat(pclFieldBuf, ",");
					}
				}
			}

			/* delete all ',' at end of field list... */
			while (pclFieldBuf[strlen(pclFieldBuf)-1] == ',')
				pclFieldBuf[strlen(pclFieldBuf)-1] = 0x00;

			/* this is necessary for VIAx */
			if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iViaInFieldList)
			{
				/* add VIAN and VIAL to field list... */
				if (pclFieldBuf[strlen(pclFieldBuf)-1] != ',')
					strcat(pclFieldBuf, ",");
				strcat(pclFieldBuf, "VIAN");
				strcat(pclFieldBuf, ",");
				strcat(pclFieldBuf, "VIAL");
			}

			/* add length of fieldbuf */
			ilLen += strlen(pclFieldBuf);

			/* for \0 separator */
			ilLen += 3;

			/* memory for it */
			if ((prlOutEvent = (EVENT*)calloc(1,(size_t)ilLen)) == NULL)
			{
				dbg(TRACE,"<HandleData> cannot alloc %d bytes for OutEvent", ilLen);
				prlOutEvent = NULL;
				Terminate(0);
			}
			else
			{
				memset(rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart,0x00,iMIN_BUF_SIZE);
				if (rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart==1)
				{
					if ((ilRC_OrgVal = SeparateIt(prlCmdblk->tw_start, prlCmdblk->tw_start
																,rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart,'#')) != RC_SUCCESS)
					{
						dbg(TRACE,"<HandleData> SeparateIt()-function failed!");
					}

					if (strlen(rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart) == 0 &&
							strlen(prlCmdblk->tw_start) > 0)
					{
						if (strlen(prlCmdblk->tw_start) > 20)
						{
							dbg(TRACE,"<HandleData> tw_start value to long as ORG.-VALUE! Only 20 Bytes can/will be used!");
						}
						strncpy(rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart,prlCmdblk->tw_start,20);
					}
					dbg(DEBUG,"<HandleData> tw_start : ORG.-VALUE = <%s>", rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart);
				}
				/* clear whole memory */
				memset((void*)prlOutEvent, 0x00, ilLen);

				/* set event structure... */
				prlOutEvent->type 			= SYS_EVENT;
				prlOutEvent->command 		= EVENT_DATA;
				prlOutEvent->originator 	= mod_id;
				prlOutEvent->retry_count	= 0;
				prlOutEvent->data_offset 	= sizeof(EVENT);
				prlOutEvent->data_length 	= ilLen - sizeof(EVENT);

				/* BC_HEAD-Structure... */
				prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
				prlOutBCHead->rc = RC_SUCCESS;
				strcpy(prlOutBCHead->dest_name, "FILEIF");
				strcpy(prlOutBCHead->recv_name, "EXCO");

				/* Cmdblk-Structure... */
				prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
				strcpy(prlOutCmdblk->command,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSndCmd);
				strcpy(prlOutCmdblk->obj_name,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcTabName);
				if (rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart==1)
				{
					sprintf(prlOutCmdblk->tw_start, "%d,%d#%s", ilCmdNo, ilTabNo,rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart);
				}
				else
				{
					sprintf(prlOutCmdblk->tw_start, "%d,%d", ilCmdNo, ilTabNo);
				}
				sprintf(prlOutCmdblk->tw_end, "%s,%s,FILEIF,TRUE", rgTM.prHandleCmd[ilCmdNo].pcHomeAirport, rgTM.prHandleCmd[ilCmdNo].pcTableExtension);
				dbg(DEBUG,"<HandleData> tw_start : <%s>", prlOutCmdblk->tw_start);
				dbg(DEBUG,"<HandleData> tw_end   : <%s>", prlOutCmdblk->tw_end);

				/* Selection */
				if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iUseSelection)
				{
					/* fields with selection */
					memset((void*)pclSelectBuf, 0x00, iMAXIMUM);

					/* count strings between... */
					ilTimeCnt = 0;

					/* store to temporary select buffer... */
					strcpy(pclSelectBuf, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSelection);


					/* for all strings between... */
					while ((ilRC = GetAllBetween(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSelection, '\'', ilTimeCnt, pclTmpKeyWord)) == 0)
					{
						/* is this minute, day or month? */
						if (strstr(pclTmpKeyWord, "minute") != NULL ||
							 strstr(pclTmpKeyWord, "MINUTE") != NULL)
						{
							/* this is minute */
							ilType = iMINUTES;
						}
						else
						{
							if (strstr(pclTmpKeyWord, "day") != NULL ||
								 strstr(pclTmpKeyWord, "DAY") != NULL)
							{
								/* this is day */
								ilType = iDAYS;
							}
							else
							{
								if (strstr(pclTmpKeyWord, "month") != NULL ||
									 strstr(pclTmpKeyWord, "MONTH") != NULL)
								{
									/* this is month */
									ilType = iMONTH;
								}
								else
								{
									/* can't convert it... */
									/* default for type... */
									ilType = -1;
								}
							}
						}

						/* get cfg entry on this types... */
						if (ilType == iMINUTES || ilType == iDAYS || ilType == iMONTH)
						{
							if ((ilRC = iGetConfigEntry(pcgCfgFile, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSectionName, pclTmpKeyWord, CFG_INT, (char*)&ilTimeDiff)) != RC_SUCCESS)
							{
								dbg(TRACE,"<HandleData> can't find key word <%s> in CFG-File", pclTmpKeyWord);
								Terminate(0);
							}

							/* set start/end-flag for AddToTime... */
							if (strstr(pclTmpKeyWord,"start") != NULL)
							{
								ilFlg = iSTART;
							}
							else if (strstr(pclTmpKeyWord,"end") != NULL)
							{
								ilFlg = iEND;
							}
							else
							{
								dbg(DEBUG,"<HandleData> missing start/end sign in key word <%s>, use default (start)", pclTmpKeyWord);
								ilFlg = iSTART;
							}

							/* calculate timestamp... */
							pclS = AddToCurrentUtcTime(ilType, ilTimeDiff, ilFlg);
							strcpy(pclLocalTime,	pclS);

							/* sub -1 hour from timestamp... */
							if (ilType != iMINUTES)
							{
								strcpy(pclUtcTime, s14_BLANKS);
								if ((ilRC = LocaltimeToUtc(pclUtcTime, pclLocalTime)) != RC_SUCCESS)
								{
									dbg(TRACE,"<HandleData> LocaltimeToUtc returns %d", ilRC);
									dbg(TRACE,"<HandleData> using old timestamp");
								}
								else
								{
									strcpy(pclLocalTime, pclUtcTime);
								}
							}

							/* search keyword and replace with timestamp... */
							if ((ilRC = SearchStringAndReplace(pclSelectBuf, pclTmpKeyWord, pclLocalTime)) < 0)
							{
								dbg(TRACE,"<HandleData> SearchStringAndReplace returns %d", ilRC);
								Terminate(0);
							}
						}

						/* incr. cnt, this is necessary to get all strings between. */
						ilTimeCnt++;
					}
					/* copy selection to cmdblk member... */
					strcpy(prlOutCmdblk->data,	pclSelectBuf);
					dbg(DEBUG,"<HandleData> Selection: <%s>", prlOutCmdblk->data);

					/* copy fields after selection... */
					strcpy(prlOutCmdblk->data+strlen(pclSelectBuf)+1, pclFieldBuf);
				}
				else if (rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection)
				{
					/* fields with selection */
					memset((void*)pclSelectBuf, 0x00, iMAXIMUM);

					/* get first item of selection */
					dbg(DEBUG,"<HandleData> Received Selection: <%s>",pclSelection);
					if (rgTM.prHandleCmd[ilCmdNo].iUseReceivedSelection != 2)
					{
						/* store to temporary select buffer... */
						strcpy(pclSelectBuf, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSelection);
						if ((pclS = GetDataField(pclSelection, 0, cCOMMA)) == NULL)
						{
							dbg(TRACE,"<HandleData> error reading first selection item...");
							Terminate(0);
						}

						/* search keyword and replace with timestamp... */
						if ((ilRC = SearchStringAndReplace(pclSelectBuf, "start_minute_0", pclS)) < 0)
						{
							dbg(TRACE,"<HandleData> SearchStringAndReplace returns %d", ilRC);
							Terminate(0);
						}

						/* get second item of selection */
						if ((pclS = GetDataField(pclSelection, 1, cCOMMA)) == NULL)
						{
							dbg(TRACE,"<HandleData> error reading second selection item...");
							Terminate(0);
						}

						/* search keyword and replace with timestamp... */
						if ((ilRC = SearchStringAndReplace(pclSelectBuf, "end_minute_0", pclS)) < 0)
						{
							dbg(TRACE,"<HandleData> SearchStringAndReplace returns %d", ilRC);
							Terminate(0);
						}
						/* copy selection to cmdblk member... */
						strcpy(prlOutCmdblk->data,	pclSelectBuf);
						/* copy fields after selection... */
						strcpy(prlOutCmdblk->data+strlen(pclSelectBuf)+1, pclFieldBuf);
					}
					else
					{
						/* set 0x00 on \n */
						if ((pclTmpPtr=strstr(pclSelection,"\n"))!= NULL)
						{
							*pclTmpPtr=0x00;
						}
						/* copy selection to cmdblk member... */
						strcpy(prlOutCmdblk->data,	pclSelection);
						/* copy fields after selection... */
						strcpy(prlOutCmdblk->data+strlen(pclSelection)+1, pclFieldBuf);
					}
					dbg(DEBUG,"<HandleData> Selection: <%s>", prlOutCmdblk->data);
				}
				else
				{
					/* fields without selection */
					strcpy(prlOutCmdblk->data+1, pclFieldBuf);
				}

				/* ADDED BY BERNI */
				strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcUsedSelection, prlOutCmdblk->data);
				dbg(DEBUG,"<HandleData> OutEvent : CmdNo:<%d> TabNo:<%d>",ilCmdNo,ilTabNo);
				dbg(DEBUG,"<HandleData> Section  : <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSectionName);
				dbg(DEBUG,"<HandleData> Table    : <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcTabName);
				dbg(DEBUG,"<HandleData> Selection: <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcUsedSelection);
				/* END BERNI */

				if (rgTM.prHandleCmd[ilCmdNo].iModID != -1)
				{
					/* write to Que */
					dbg(DEBUG,"<HandleData> sending to ModID: %d", rgTM.prHandleCmd[ilCmdNo].iModID);
					if ((ilRC = que(QUE_PUT, rgTM.prHandleCmd[ilCmdNo].iModID, mod_id, PRIORITY_4, ilLen, (char*)prlOutEvent)) != RC_SUCCESS)
					{
						dbg(TRACE,"<HandleData> QUE_PUT returns: %d", ilRC);
						Terminate(0);
					}
				}
				else
				{
					/* write to Que */
					dbg(DEBUG,"<HandleData> sending to ModID: %d", igRouterID);
					if ((ilRC = que(QUE_PUT, igRouterID, mod_id, PRIORITY_4, ilLen, (char*)prlOutEvent)) != RC_SUCCESS)
					{
						dbg(TRACE,"<HandleData> QUE_PUT returns: %d", ilRC);
						Terminate(0);
					}
				}

				/* free Out-Event memory... */
				if (prlOutEvent != NULL)
				{
					free((void*)prlOutEvent);
					prlOutEvent = NULL;
				}
			}
		}

		/* delete counter, this is a new question */
		dbg(DEBUG,"<HandleData> reset data counter for command <%d>", ilCmdNo);
		rgTM.prHandleCmd[ilCmdNo].iDataFromTable = 0;
		rgTM.prHandleCmd[ilCmdNo].iDataCounter = 0;

		dbg(DEBUG,"<HandleData> ------------- END <%s> ------------", prlCmdblk->command);
		/* successfull return */
		return RC_SUCCESS;
	}
	else
	{
		/* here we received data from SQLHDL, FLIGHT, etc... */
		/* data format is d11,d12,d13,d14\r\n,d21,d22,d23,d24\r\nd31,d32,d33... */
		dbg(DEBUG,"<HandleData> ============= START <ANSWER> ===========");

		/* change fieldlist ... */
		/* must change ',' to ';' in fieldlist... */
		ilOk = 0;
		pclS = pclFields;
		while (*pclS)
		{
			if (*pclS == '(')
				ilOk = 1;
			else if (*pclS == ')')
				ilOk = 0;
			if (ilOk)
			{
				if (*pclS == ',')
					*pclS = ';';
			}
			pclS++;
		}

		/* get command- and table number */
		ilCmdNo = atoi(GetDataField(prlCmdblk->tw_start, 0, ','));
		ilTabNo = atoi(GetDataField(prlCmdblk->tw_start, 1, ','));
		dbg(DEBUG,"<HandleData> Answer   : CMDNO: <%d>, TABNO: <%d>", ilCmdNo, ilTabNo);

		memset(rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart,0x00,iMIN_BUF_SIZE);
		if (rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.iAfcKeepOrgTwStart==1)
		{
			if ((ilRC_OrgVal = SeparateIt(prlCmdblk->tw_start, prlCmdblk->tw_start
														,rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart,'#')) == RC_SUCCESS)
			{
				dbg(DEBUG,"<HandleData> tw_start : ORG.-VALUE = <%s>", rgTM.prHandleCmd[ilCmdNo].rSendEventCmds.pcOrgTwStart);
			}
			else
			{
				dbg(DEBUG,"<HandleData> tw_start : No org.-value found!");
			}
		}

		dbg(DEBUG,"<HandleData> tw_start : <%s>", prlCmdblk->tw_start);
		/* ADDED BY BERNI */
		dbg(DEBUG,"<HandleData> Section  : <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSectionName);
		dbg(DEBUG,"<HandleData> Table    : <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcTabName);
		dbg(DEBUG,"<HandleData> Selection: <%s>",rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcUsedSelection);
		strcpy(pclOutSelection,rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcUsedSelection);
		pclPtr = strstr(pclOutSelection,"ORDER BY");
		if (pclPtr != NULL)
		{
			*pclPtr = 0x00;
		} /* end if */
		/* END BERNI */


		/* only for valid command AND table numbers */
		if (ilCmdNo >= 0 && ilTabNo >= 0)
		{
			/* send answer to client... */
			if (rgTM.prHandleCmd[ilCmdNo].iSendSqlRc)
			{
				/* BERNI */
				dbg(DEBUG,"CONFIGURED TO SEND AN ANSWER");
				if (strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSectionName,"LZDTAB") == 0)
				{
					dbg(DEBUG,"FOUND SECTION LZDTAB");
					if (strcmp(prlCmdblk->command,"URT") == 0)
					{
						dbg(DEBUG,"FOUND COMMAND URT");
						ilRC = tools_send_sql_rc(rgTM.prHandleCmd[ilCmdNo].iSqlRCOriginator,
												 prlCmdblk->command,
												 prlCmdblk->obj_name,
												 "", pclFields, "", RC_SUCCESS);
						dbg(TRACE,"SENT ANSWER TO CLIENT");
						ilCmdNo = -1;
						ilTabNo = -1;
						return RC_SUCCESS;
					} /* end if */
					else
					{
						dbg(TRACE,"NOT YET ANSWERED, FIRST UPDATE EXPF");
					} /* end else */
				} /* end if */
				else
				{
					ilRC = tools_send_sql_rc(rgTM.prHandleCmd[ilCmdNo].iSqlRCOriginator,
												 prlCmdblk->command,
												 prlCmdblk->obj_name,
												 "", pclFields, "", RC_SUCCESS);
				} /* end else */
			}
		}

		/* only for valid command AND table numbers */
		if (ilCmdNo >= 0 && ilTabNo >= 0)
		{

			/* change fieldlist ... */
			/* incr. data counter */
			rgTM.prHandleCmd[ilCmdNo].iDataCounter++;

			#if 0 /* JWE */
			/* must we map fieldname */
			for (i=0; i<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoFields; i++)
			{
				for (ilAssignNo=0; ilAssignNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoAssign; ilAssignNo++)
				{
					if (!strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].pcDBField, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i]))
					{
						/* found field... */
						/* check memory for it */
						if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].iDBFieldSize < rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].iIFFieldSize)
						{
							/* length of db-field is smaller than if-field */
							/* free memory */
							free ((void*)rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i]);

							/* copy assignment to field list */
							if ((rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i] = strdup(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].pcIFField)) == NULL)
							{
								/* not enough memory to run */
								dbg(TRACE,"<HandleData> can't change fields...");
								rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i] = NULL;
								Terminate(0);
							}

							/* set new length */
							rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].iDBFieldSize = rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].iIFFieldSize;
						}
						else
						{
							/* field length of db-field is equal or
								greater than if-field
							*/
							strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].pcIFField);
						}
					}
				}
			}
			#endif

			/* count number of datablocks */
			if ((ilLines = (int)GetNoOfElements(pclData, '\n')) < 0)
			{
				dbg(TRACE,"<HandleData> GetNoOfElements returns: %d", ilLines);
				Terminate(0);
			}
			dbg(DEBUG,"<HandleData> found %d records...", ilLines);

			/* chech number of lines to write... */
			if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines)
			{
				/* set new value for data counter */
				if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines < ilLines)
					ilLines = rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines;
			}

/* TWE */
/*
dbg(TRACE,"HandleData: Data <%s>\n Fields <%s>",
           pclData, pclFields);
*/
/*********/


			/* set temporary pointer to data area */
			pclTmpPtr = pclData;

			if (ilLines > 0)
			{
				igCurrDataLines = ilLines;
				igTotalLineCount = ilLines;
			}
			else
			{
				igCurrDataLines = GetNoOfElements(pclData,'\n');
				igTotalLineCount = GetNoOfElements(pclData,'\n');
			}

			/* open file, write header and SOB-strings */
			if ((ilRC = OpenFile(ilCmdNo, ilTabNo)) != RC_SUCCESS)
			{
				dbg(DEBUG,"<HandleData> OpenFile returns: %d", ilRC);
				return ilRC;
			}

			/* over all lines */
			if (ilTabNo==0)
				rgTM.prHandleCmd[ilCmdNo].iLineNumberSav = 0;

			for (ilCurLine=0, ilAllSecRecords=0; ilCurLine<ilLines; ilCurLine++,rgTM.prHandleCmd[ilCmdNo].iLineNumberSav++)
			{
				if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines)
					if (ilCurLine+ilAllSecRecords >= rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines)
					{
						break;
					}

				/* get next data for writing... */
				memset((void*)pclTmpBuf, 0x00, iMAXIMUM);
				if ((pclTmpPtr = CopyNextField(pclTmpPtr, '\n', pclTmpBuf)) == NULL)
				{
					dbg(TRACE,"<HandleData> CopyNextField returns NULL");
					Terminate(0);
				}
				if (pclTmpBuf[strlen(pclTmpBuf)-1] == '\r')
					pclTmpBuf[strlen(pclTmpBuf)-1] = 0x00;

				/* check conditions */
				ilOk = 1;
				for (ilConditionNo=0; ilConditionNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoCondition; ilConditionNo++)
				{
					/* get data from field */
					if ((pclS = GetDataField(pclTmpBuf, (UINT)rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prCondition[ilConditionNo].iFieldPosition, (char)',')) == NULL)
					{
						dbg(TRACE,"<HandleData> GetDataField (condition) returns NULL");
						Terminate(0);
					}

					/* is this correct? */
					if (strcmp(pclS, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prCondition[ilConditionNo].pcCondition))
					{
						/* this is not corrent... */
						ilOk = 0;
						break;
					}
				}

				/* only if conditions are ok */
				if (ilOk)
				{
					if (rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines)
					{
						if (ilCurLine+ilAllSecRecords < rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iLines-2)
							ilSecondRecord = 1;
						else
							ilSecondRecord = 0;
					}
					else
						ilSecondRecord = 1;

					ilLineNumberToWrite = 1+rgTM.prHandleCmd[ilCmdNo].iLineNumberSav;

					/* write to file */
					if ((ilRC = WriteDataToFile(ilCmdNo,
					ilTabNo, &ilSecondRecord, pclTmpBuf,
					pclFields,ilLineNumberToWrite)) !=RC_SUCCESS)
					{
						dbg(TRACE,"<HandleData> WriteDataToFile returns %d", ilRC);
						return ilRC;
					}
					else
					{
						dbg(DEBUG,"<HandleData> WriteDataToFile(): wrote line <%d>/<%d> => TOTAL:<%d> to file!"
								,ilCurLine+1,igCurrDataLines,ilLineNumberToWrite);
								/*,ilCurLine+1,rgTM.prHandleCmd[ilCmdNo].iLineNumberSav+1);*/
					}

					/* incr. secondary record counter */
					if (ilSecondRecord)
						ilAllSecRecords++;
				}
			}
			igTotalLineCount = ilLineNumberToWrite;

			/* write end of block sign */
			if ((ilRC = WriteEOB(ilCmdNo)) != RC_SUCCESS)
			{
				dbg(DEBUG,"<HandleData> WriteEOB returns %d", ilRC);
				return ilRC;
			}

			#if 0 /* JWE */
			/* mapping to 'normal fields.. */
			for (i=0; i<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoFields; i++)
			{
				for (ilAssignNo=0; ilAssignNo<rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].iNoAssign; ilAssignNo++)
				{
					if (!strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].pcIFField, rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i]))
					{
						/* length of db-field must be! equal or
							greater than if-field
						*/
						strcpy(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcFields[i], rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].prAssign[ilAssignNo].pcDBField);
					}
				}
			}
			#endif

			/* we receiver data from table ... */
			rgTM.prHandleCmd[ilCmdNo].iDataFromTable++;

			dbg(DEBUG,"<HandleData> ============= END <ANSWER> ============");

			/* check number of tables */
			if (rgTM.prHandleCmd[ilCmdNo].iDataCounter ==
				 rgTM.prHandleCmd[ilCmdNo].iNoOfTables)
			{
				/* set counter to zero */
				rgTM.prHandleCmd[ilCmdNo].iDataCounter = 0;

				/* set counter to zero */
				rgTM.prHandleCmd[ilCmdNo].iDataFromTable = 0;

				/* send file to other machine... */
				ilRC = SendFileToMachine(ilCmdNo, ilTabNo);

				/* at this point, fileif has been written the file. So, inform
					status-handler about this state...
				*/
				dbg(DEBUG,"<HandleData> Call SendStatus() for write-operation");
				if ((ilRC = SendStatus("FILEIO", rgTM.prHandleCmd[ilCmdNo].pcCommand, "WRITE", 1, rgTM.prHandleCmd[ilCmdNo].pcFileName, "", "")) != RC_SUCCESS)
				{
					/* fuckin up here... ho ho ho */
					dbg(TRACE,"<HandleData> SendStatus() returns: <%d>",ilRC);
				}

				/* ADDED BY BERNI */
				if (strcmp(rgTM.prHandleCmd[ilCmdNo].prTabDef[ilTabNo].pcSectionName,"LZDTAB") == 0)
				{
					dbg(DEBUG,"UPDATE LZD RECORDS IN DSRTAB");
					if (ilRC == RC_SUCCESS)
                                	{
						sprintf(pclTwsBuf,"%d,%d",ilCmdNo,ilTabNo);
						(void) tools_send_info_flag(igRouterID,0, "LZD", "", "EXCO", "", "", pclTwsBuf, "HAJ,HAJ,FILEIF", "URT","DSRTAB",pclOutSelection,"EXPF"," ",0);
						dbg(DEBUG,"DONE");
					} /* end if */
				} /* end if */
				return ilRC;
				/* END BERNI */
			}
			else
				return RC_SUCCESS;
		}
		else
		{
			dbg(DEBUG,"<HandleData> can't find command and/or table number");
			dbg(DEBUG,"<HandleData> ------------- END <ANSWER> ------------");
			return RC_FAIL;
		}
	}
}

/******************************************************************************/
/* The open file routine                                                      */
/******************************************************************************/
static int OpenFile(int ipCmdNo, int ipTabNo)
{
	int			ilRC;
	long			lOldLength;
	char			pclTmpBuf[iMIN_BUF_SIZE];
	char			pclErrorFile[iMAX_BUF_SIZE];
	char			pclTmpDat[iMAX_BUF_SIZE];
	char			*pclCurrentTime = NULL;
	char			*pclOldContents = NULL;
	char			pclSeq[20];
	FILE			*fh				 = NULL;

	/* only if file isn't open */
	if (!rgTM.prHandleCmd[ipCmdNo].iOpenDataFile)
	{
		/* clear buffer */
		memset((void*)rgTM.prHandleCmd[ipCmdNo].pcTmpFileName,
													0x00, iMIN_BUF_SIZE);
		memset((void*)rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,
													0x00, iMIN_BUF_SIZE);

		/* produce tmp-filename */
		strcpy(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,rgTM.prHandleCmd[ipCmdNo].pcFilePath);
		if (rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath[strlen(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath)-1] != '/')
		{
			strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath, "/");
		}
		strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,
				 rgTM.prHandleCmd[ipCmdNo].pcTmpFile);
		strcpy(rgTM.prHandleCmd[ipCmdNo].pcTmpFileName,
				 rgTM.prHandleCmd[ipCmdNo].pcTmpFile);

		/* produce final-filename */
		strcpy(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath,rgTM.prHandleCmd[ipCmdNo].pcFilePath);
		if (rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath[strlen(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath)-1] != '/')
		{
			strcat(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath, "/");
		}
		strcat(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath,
				 rgTM.prHandleCmd[ipCmdNo].pcFile);
		strcpy(rgTM.prHandleCmd[ipCmdNo].pcFileName,
				 rgTM.prHandleCmd[ipCmdNo].pcFile);

		/* should i use a timestamp in filename? */
		if (rgTM.prHandleCmd[ipCmdNo].iUseFilenameWithTimestamp)
		{
			if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "ASV", 3) ||
			    !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "ECC", 3) ||
			    !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LRM", 3))
			{
				/* -1440 entspricht 1 Tag (24 Std. * 60 Min.) */
				pclCurrentTime = GetPartOfTimeStamp(AddToCurrentUtcTime(iMINUTES, -1440, iSTART), rgTM.prHandleCmd[ipCmdNo].pcTimeStampFormat);
			}
			else
			{
				if (rgTM.prHandleCmd[ipCmdNo].iUseFileTimestampBase == 1)
				{
					pclCurrentTime = GetPartOfTimeStamp(GetTimeStampLocal(), rgTM.prHandleCmd[ipCmdNo].pcTimeStampFormat);
				}
				else
				{
					pclCurrentTime = GetPartOfTimeStamp(GetTimeStamp(), rgTM.prHandleCmd[ipCmdNo].pcTimeStampFormat);
				}
			}
			strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileName, pclCurrentTime);
			strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath, pclCurrentTime);
			strcat(rgTM.prHandleCmd[ipCmdNo].pcFileName, pclCurrentTime);
			strcat(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath, pclCurrentTime);
		}

		/*************************************************/
		/* should i add SEQ-number to  filename ? */
		/*************************************************/
		if (rgTM.prHandleCmd[ipCmdNo].iUseFilenameWithSeqNr==1)
		{
			memset(pclSeq,0x00,20);
			GetSeqNumber(pclSeq,rgTM.prHandleCmd[ipCmdNo].pcNumtab_Key,rgTM.prHandleCmd[ipCmdNo].pcSeqNrLen
				,rgTM.prHandleCmd[ipCmdNo].pcSeqResetDay,rgTM.prHandleCmd[ipCmdNo].pcNumtab_Startvalue);
			strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileName,pclSeq);
			strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,pclSeq);
			dbg(DEBUG,"<OpenFile> file-name <%s>",rgTM.prHandleCmd[ipCmdNo].pcTmpFileName);
		}

		/***************************************/
		/* extension is necessary in all cases */
		/***************************************/
		strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileName,
				 rgTM.prHandleCmd[ipCmdNo].pcFileExtension);
		strcat(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,
				 rgTM.prHandleCmd[ipCmdNo].pcFileExtension);
		strcat(rgTM.prHandleCmd[ipCmdNo].pcFileName,
				 rgTM.prHandleCmd[ipCmdNo].pcFileExtension);
		strcat(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath,
				 rgTM.prHandleCmd[ipCmdNo].pcFileExtension);
		dbg(DEBUG,"<OpenFile> file-name <%s>",rgTM.prHandleCmd[ipCmdNo].pcTmpFileName);

		/******************************/
		/* must i move to error path? */
		/******************************/
		if (rgTM.prHandleCmd[ipCmdNo].iUseErrorFile)
		{
			/* only if we create a file previously */
			if (strlen(rgTM.prHandleCmd[ipCmdNo].pcOldFileNameWithPath))
			{
				/* clear internal buffer */
				memset((void*)pclErrorFile, 0x00, iMAX_BUF_SIZE);
				sprintf(pclErrorFile, "%s%s", rgTM.prHandleCmd[ipCmdNo].pcErrorFilePath, rgTM.prHandleCmd[ipCmdNo].pcErrorFileName);

				/* filename with timestamp? */
				if (strlen(rgTM.prHandleCmd[ipCmdNo].pcErrorTimeStampFormat))
				{
					strcat(pclErrorFile, GetPartOfTimeStamp(GetTimeStamp(), rgTM.prHandleCmd[ipCmdNo].pcErrorTimeStampFormat));
				}

				/* and the extension */
				strcat(pclErrorFile, rgTM.prHandleCmd[ipCmdNo].pcFileExtension);
				dbg(DEBUG,"<OpenFile> This is my error file...<%s>", pclErrorFile);

				/* get old file contents */
				/*******
				Hier muss! ich pcOldFileNameWithPath benutzen. Dies wird noetig falls
				ein Zeitstempel im Filenamen vorkommt (der eraendert sich ja
				bekanntlich). Somit bekomme ich immer (bis auf das erste mal) das
				letzte File...
				*******/
				if ((ilRC = GetFileContents(rgTM.prHandleCmd[ipCmdNo].pcOldFileNameWithPath, &lOldLength, (char**)&pclOldContents)) == RC_SUCCESS)
				{
					/* everything looks good */
					/* open error file in error path */
					if ((fh = fopen(pclErrorFile, "w")) != NULL)
					{
						/* write the data */
						if ((ilRC = fwrite((const void*)pclOldContents, lOldLength, 1, fh)) <= 0)
						{
							dbg(TRACE,"<OpenFile> cannot write data to error file...");
						}

						/* close this file */
						fclose(fh);

						/* reset file pointer */
						fh = NULL;
					}
					else
						dbg(TRACE,"<OpenFile> cannot open error file...");

					/* delete memory we dosen't need */
					if (pclOldContents != NULL)
					{
						free((void*)pclOldContents);
						pclOldContents = NULL;
					}
				}
				else
					dbg(TRACE,"<OpenFile> GetFileContents returns: %d",ilRC);
			}
		}

		/* read the old file if we check file diff ... */
		if (rgTM.prHandleCmd[ipCmdNo].iCheckFileDiff)
		{
			/* is there an old file name */
			if (strlen(rgTM.prHandleCmd[ipCmdNo].pcOldFileNameWithPath))
			{
				/* check pointer first */
				if (rgTM.prHandleCmd[ipCmdNo].pcOldFileContent != NULL)
				{
					free((void*)rgTM.prHandleCmd[ipCmdNo].pcOldFileContent);
					rgTM.prHandleCmd[ipCmdNo].pcOldFileContent = NULL;
					rgTM.prHandleCmd[ipCmdNo].lOldFileLength = -1;
				}

				if ((ilRC = GetFileContents(rgTM.prHandleCmd[ipCmdNo].pcOldFileNameWithPath, &rgTM.prHandleCmd[ipCmdNo].lOldFileLength, (char**)&rgTM.prHandleCmd[ipCmdNo].pcOldFileContent)) != RC_SUCCESS)
				{
					dbg(TRACE,"<OpenFile> GetFileContents returns: %d", ilRC);
					rgTM.prHandleCmd[ipCmdNo].lOldFileLength = -1;
					rgTM.prHandleCmd[ipCmdNo].pcOldFileContent = NULL;
				}
				else
					dbg(DEBUG,"<OpenFile> ready reading old data file...");
			}
			else
			{
				rgTM.prHandleCmd[ipCmdNo].lOldFileLength = -1;
				rgTM.prHandleCmd[ipCmdNo].pcOldFileContent = NULL;
			}
		}

		/* open this file */
		dbg(DEBUG,"<OpenFile> OpenFile <%s>",
					rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath);
		if ((rgTM.prHandleCmd[ipCmdNo].pFh = fopen((const char*)rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,"w")) == NULL)
		{
			dbg(DEBUG,"<OpenFile> cannot open file %s",
									rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath);
			return iOPEN_FAIL;
		}

		/* store filename */
		strcpy(rgTM.prHandleCmd[ipCmdNo].pcOldFileNameWithPath,
				 rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath);

		/* set flag */
		rgTM.prHandleCmd[ipCmdNo].iOpenDataFile = 1;

		/* write SOF-Sign if necessary */
		if (rgTM.prHandleCmd[ipCmdNo].iUseSOF)
		{
			/* here we copy back the orgiginal SOF-line which still contains the std.-tokens for replacement */
			/* otherwise we  would lose the std.-tokens and they would not contain actual values after  */
			/* the first run of the according section  */
			strcpy(rgTM.prHandleCmd[ipCmdNo].pcStartOfFile,rgTM.prHandleCmd[ipCmdNo].pcStartOfFileOrg);
			dbg(DEBUG,"<OpenFile> SOF=<%s>",rgTM.prHandleCmd[ipCmdNo].pcStartOfFile);
			/* replacing poss. standard-tokens inside header line */
			memset(pclTmpDat,0x00,iMAX_BUF_SIZE);
			/*sprintf(pclTmpDat,"%s",rgTM.prHandleCmd[ipCmdNo].pcTmpFileName);*/
			sprintf(pclTmpDat,"%s",rgTM.prHandleCmd[ipCmdNo].pcFileName);
			ReplaceStdTokens(ipCmdNo,rgTM.prHandleCmd[ipCmdNo].pcStartOfFile,pclTmpDat,
											rgTM.prHandleCmd[ipCmdNo].pcDynDateFormat,rgTM.prHandleCmd[ipCmdNo].pcDynTimeFormat,-1);

			dbg(DEBUG,"<OpenFile> SOF=<%s>",rgTM.prHandleCmd[ipCmdNo].pcStartOfFile);
			/* must write SOF-Sign */
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh, "%s%s",
					rgTM.prHandleCmd[ipCmdNo].pcStartOfFile,
					rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}

		/* write Kennung... */
		if (rgTM.prHandleCmd[ipCmdNo].iUseAirport)
		{
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh,"Kennung=Flughafen %s%s",
					rgTM.prHandleCmd[ipCmdNo].pcAirport,
					rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}
	}

	if (rgTM.prHandleCmd[ipCmdNo].iOpenDataFile)
	{
		/* data file must be! open */
		/* write start of block to file, if necessary... */
		if (rgTM.prHandleCmd[ipCmdNo].iUseSOB)
		{
			/* clear temporary buffer */
			memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);

			/* should i write blocknumber? */
			if (rgTM.prHandleCmd[ipCmdNo].iStartOfBlockNumber)
			{
				/* copy with blocknuber... */
				sprintf(pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcStartOfBlock, rgTM.prHandleCmd[ipCmdNo].iDataFromTable+1);
			}
			else
			{
				/* copy without blocknuber... */
				sprintf(pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcStartOfBlock);
			}

			/* write to file */
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh, "%s%s", pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}

		/* write comment to file */
		if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iUseComment)
		{
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh, "%s%s%s",
				rgTM.prHandleCmd[ipCmdNo].pcCommentSign,
				rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcComment,
				rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}
	}

	/* everything looks good */
	return RC_SUCCESS;
}

/******************************************************************************/
/* The write data routine                                                     */
/******************************************************************************/
static int WriteDataToFile(int ipCmdNo, int ipTabNo, int *pipSecondRecord,
									char *pcpData, char *pcpFields,int ipRecordNr)
{
	int			i;
	int			ilRC;
  int     ilSendToHR;
  int     ilAbwesend;
	int			ilCnt;
	int			ilCnt1;
	int			ilLen;
	int			ilLen1;
	int			ilLen2;
	int			ilLen3;
	int			ilLen4;
	int			ilTtyp;
	int			ilOther;
	int			ilTmpPos;
	int			ilIgnore;
	int			ilFieldNo;
	int			ilSyslibNo;
	int			ilLengthNo;
	int			ilNoOfVias;
	int			ilIgnoreNo;
	int			ilCurConst;
	int			ilCurMapNo;
	int			ilFoundData;
	int			ilCurrentPos;
	int			ilOldDebugLevel;
	int			ilCurUtc;
	int			ilMin;
	int			ilVia;
	int			ilHour;
	int			ilDiff;
	int			ilDiffMin;
	int			ilDiffHour;
	int			ilSBLA;
	int			ilTmpSBLA;
	int			ilAssignNo;
	double		dlMtow;
	char			pclHour[iMIN];
	char			pclMin[iMIN];
    char      pclDay[10];
	char			pclTmpBuf[iMIN_BUF_SIZE];
	char			pclVial[iMAX_BUF_SIZE+1];
	char			pclVia[iMIN];
	char			pclSyslibResultBuf[iMAXIMUM];
	char			pclFieldDataBuffer[iMAXIMUM];
	char			pclLineBuffer[iMAXIMUM];
	char			pclDataBuffer[iMAXIMUM];
	char			pclTmpDataBuffer[iMAX];
	char			pclLocalTimeBuffer[15];
	char			pclSyslibFields[iMAXIMUM];
	char			pclSyslibFieldData[iMAXIMUM];
	char			pclSBLA[iMIN_BUF_SIZE];
  char      pclPenoData[24];
  char      pclTable[10];
  char      pclMappedFieldName[iMIN];
	char			*pclS 		= NULL;
	char			*pclPtr 		= NULL;
	char			*pclTmpPtr	= NULL;

	/* init something */
	ilCurConst = 0;
	pclTmpPtr = pcpData;

	dbg(DEBUG,"<WDtF> ***** END/START *****");
	dbg(DEBUG,"<WDtF> Fields <%s>",pcpFields);
	dbg(DEBUG,"<WDtF> Data   <%s>",pcpData);

	/* use VIA? then get info about it */
	if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iViaInFieldList)
	{
		/* count number of elements... */
		if ((ilCnt = GetNoOfElements(pcpData, (char)cCOMMA)) < 0)
		{
			dbg(TRACE,"<WDtF> GetNoOfElements(pcpData) returns: %d", ilCnt);
			Terminate(0);
		}
		dbg(DEBUG,"<WDtF> GetNoOfElements(pcpData) returns: %d", ilCnt);

		/* get VIAN and VIAL */
		/* this is VIAN */
		if ((pclPtr = GetDataField(pcpData, ilCnt-2, cCOMMA)) == NULL)
		{
			dbg(TRACE,"<WDtF> GetDataField (VIAN) returns NULL");
			Terminate(0);
		}

		/* convert number of via's to int */
		ilNoOfVias = atoi(pclPtr);
		/*dbg(DEBUG,"<WDtF>: Nr. of via's = <%d>",ilNoOfVias);*/
		memset((void*)pclVial, 0x00, iMAX_BUF_SIZE+1);
		if (ilNoOfVias > 0)
		{
			/* this is VIAL */
			if ((pclPtr = GetDataField(pcpData, ilCnt-1, cCOMMA)) == NULL)
			{
				dbg(TRACE,"<WDtF> GetDataField (VIAL) returns NULL");
				Terminate(0);
			}
			/* save VIAL */
			strcpy(pclVial, pclPtr);
		}
	}

	/* must we write the data at fix positions or dynamicly */
	if (IsDynamic(rgTM.prHandleCmd[ipCmdNo].iWriteData))
	{
		/*dbg(DEBUG,"*************** USE DYNAMIC-mode for data ! ****************************");	*/
		/* this is dynamic, data area must be initialized with 0x00 */
		memset((void*)pclDataBuffer, 0x00, iMAXIMUM);

		/* over all fields */
		for (ilFieldNo=0, ilLengthNo=0;
			  ilFieldNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoFields;
			  ilFieldNo++,ilLengthNo++)
		{
			/*dbg(DEBUG,"<WDtF> FIELD: <%s> -- START --",rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);*/
			*pclFieldDataBuffer=0x00;

			/* 1. ***************************/
			/* must we ignore this field... */
			/********************************/
			ilIgnore = 0;
			for (ilIgnoreNo=0; ilIgnoreNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoOfIgnore; ilIgnoreNo++)
			{
				/* check current field */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcIgnoreFields[ilIgnoreNo]))
				{
					ilIgnore = 1;
					break;
				}
			}

			/* 2. *************************************/
			/* must write Fieldname (normal/mapped ?) */
			/******************************************/
			if (rgTM.prHandleCmd[ipCmdNo].iFieldName)
			{
				if (!ilIgnore)
				{
					memset(pclMappedFieldName,0x00,iMIN);
					for (ilAssignNo=0; ilAssignNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoAssign; ilAssignNo++)
					{
						if (strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prAssign[ilAssignNo].pcDBField
											 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]) == 0)
						{
							strncpy(pclMappedFieldName,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prAssign[ilAssignNo].pcIFField,iMIN);
						}
					}
					if (strlen(pclMappedFieldName) > 0)
						strcat(pclDataBuffer,pclMappedFieldName);
					else
						strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);

					/* set data separator */
					strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
				}
				else
					if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iDataLengthCounter > 0)
						ilLengthNo++;
			}

			/* 3. *******************************************/
			/* is this a const?, write it here and continue */
			/************************************************/
			if (!strncmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "CONSTANT_", 9) || !strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ACTUAL_DATE"))
			{
				/* is this actual date? */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ACTUAL_DATE"))
				{
					/* copy current (UTC) time to buffer */
					strcpy(pclFieldDataBuffer, GetTimeStamp());

					/* hier das mit dem timestamp format... */
					if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo] != NULL)
					{
						if ((pclS = GetPartOfTimeStamp(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo])) != NULL)
						{
							strcpy(pclFieldDataBuffer, pclS);
						}
						else
						{
							memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
						}
					}
				}
				else
				{
					/* copy it to data buffer */
					strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcConst[ilCurConst++]);
				}

				/* is ther a max. length? */
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iDataLengthCounter > 0)
				{
					/* dynamic but max. length */
					ilLen1 = strlen(pclFieldDataBuffer);
					/*ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo++];*/
					ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

					/* without max. length */
					strncat(pclDataBuffer, pclFieldDataBuffer, FMIN(ilLen1, ilLen2));

					/* set data separator */
					strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
				}
				else
				{
					/* copy data to data buffer */
					strcat(pclDataBuffer, pclFieldDataBuffer);

					/* set data separator */
					strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
				}

				/* continue to next field */
				continue;
			}

			/* 4. *******************************************/
			/* is this a fuellfeld? */
			/************************************************/
			if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "FILL_FIELD"))
			{
				/* set data separator */
				strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);

				/* next field, next try */
				continue;
			}

			/* 5. *******************************************/
			/* check VIA's */
			/************************************************/
			if ((strstr(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "VIX") != NULL))
			{
			/* clear data buffer... */
				memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);

				if (!ilIgnore)
				{
					if (ilNoOfVias > 0)
					{
						/* extract VIAx from VIAL */
						memset(pclVia,0x00,iMIN);
						strncpy(pclVia,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]+3,1);
						ilVia = atoi(pclVia);
						/*dbg(DEBUG,"<WDtF> Vias <%s>,<%d>",pclVia,ilVia);*/
						if (ilVia >= 1 && ilVia <= 8)
						{
							pclPtr = pclVial;
							/*dbg(DEBUG,"VIA: Vial<%s>",pclPtr);*/
							/*pclS = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo] + strlen(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]) - 1;*/
							if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iVia34 == 4)
							{
								/* get 4-letter code */
								strncpy(pclFieldDataBuffer, pclPtr+((ilVia-1)*120+4), 4);
							}
							else /* try to get 3-letter code */
							{
								if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iVia34 == 3)
								{
									strncpy(pclFieldDataBuffer, pclPtr+((ilVia-1)*120+1), 3);
								}
							}
						}
						else
						{
							dbg(TRACE,"<WDtF> WARNING: Invalid via entry in field list. Allowed entries are VIA1-VIA8. Setting this via to blank!");
							strncpy(pclFieldDataBuffer," ",1);
						}
						/*dbg(DEBUG,"%d.VIA-pclFieldDataBuffer: <%s>",ilVia,pclFieldDataBuffer);*/
					}
				}
			}
			else
			{
				/* get the data */
				memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
				if ((pclTmpPtr = CopyNextField(pclTmpPtr, cSEPARATOR, pclFieldDataBuffer)) == NULL)
				{
					dbg(TRACE,"<WDtF> cannot copy field (pcpData) %d", ilFieldNo);
					Terminate(0);
				}

				/* delete right and left banks... */
				while (pclFieldDataBuffer[strlen(pclFieldDataBuffer)-1] == ' ')
					pclFieldDataBuffer[strlen(pclFieldDataBuffer)-1] = 0x00;

				/* 6. *******************************************/
				/* map from UTC to LocalTime... */
				/************************************************/
				for (ilCurUtc=0; ilCurUtc<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoOfUtcToLocal; ilCurUtc++)
				{
					/* compare map-fieldname and current field name */
					/*dbg(DEBUG,"<WDtF> UTC/LOC: compare <%s> with <%s>"
						,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcMapUtcToLocal[ilCurUtc]
						,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);*/

						/* HIER WEITER JWE 20050208: */

					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcMapUtcToLocal[ilCurUtc], rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]))
					{
						/* changes by mos 8 Apr 2000 in the lions cage Warszawa */
						/* o-tone:"UAHH-that motherfucking * process inserts time without
							seconds in our database." */
						/* note, that there is no conversion to local time with wrong time
							formats*/
						/* for the health of mos, a fancy conversion will be implemented */
						if (strlen(pclFieldDataBuffer) == 12)
						{
								/* map to the CORRECT time format (with seconds)*/
								strcat(pclFieldDataBuffer, "00\0");
						}
						/* end of extreme sophisticated and lifesaving changes */

						/* only if we found data... */
						if (strlen(pclFieldDataBuffer) == 14)
						{
							/* set local buffer */
							strcpy(pclLocalTimeBuffer, pclFieldDataBuffer);


							/* call mapping function */
							if ((ilRC = UtcToLocal(pclLocalTimeBuffer)) != RC_SUCCESS)
							{
								dbg(DEBUG,"<WDtF> UtcToLocalTime returns: %d", ilRC);
								dbg(DEBUG,"<WDtF> use old timestamp now...");
							}
							else
							{
								strcpy(pclFieldDataBuffer, pclLocalTimeBuffer);
							}
						}
					}
				}

				/* 7. *******************************************/
				/* timestamp formatting */
				/************************************************/
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo] != NULL)
				{
					if ((pclS = GetPartOfTimeStamp(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo])) != NULL)
					{
						strcpy(pclFieldDataBuffer, pclS);
					}
					else
					{
						memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					}
				}

				/* 8. *******************************************/
				/* SUBSTR */
				/************************************************/
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iUseSubStr)
				{
					if (strlen(pclFieldDataBuffer))
					{
						memmove((void*)pclFieldDataBuffer, (const void*)&pclFieldDataBuffer[rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iStartPos-1], (size_t)rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iBytes);
						pclFieldDataBuffer[rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iBytes] = 0x00;
					}
				}

				/*****************************************************************************
					 HAJ: fuer EFA-Flug und NDR muss hier was hardcoded rein!!.
					 Sehr unschoen aber ich weiß nicht wie ich's sonst machen soll...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "EFA", 3)
				||  !strcmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "NDR"))
				{
					/* this is for EFA1, EFA2, NDR... */
					/* map field REMP..., use LAND... */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "REMP"))
					{
						/* this is REMP */
						/* search LAND... */
						if ((ilRC = GetIndex(pcpFields, "LAND", ',')) >= 0)
						{
							if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
							{
								if (strlen(pclS) == 14)
								{
									/* must be landed... */
									/* set ftyp to my special code */
									memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
									strcpy(pclFieldDataBuffer, "GEL");
								}
							}
						}
					}
				}

				/*****************************************************************************
						HAJ: Murks zweiter Teil: 	Jetzt soll fuer Laerm das Feld MTOW ohne Punkt aber
													plus NULL hinterdran!? Das wird jetzt hardcoded an
													den Kommandos LRM1 und LRM2 festgemacht...
						Und Murks dritter Teil: Verkehrsart (TTYP) soll jetzt rechtsbuendig MIT
														Vornullen ausgegeben werden!!

						NEU: 23.01.98 <-> bei LRM sollen alle Airportcodes im 3-letter plus
								space dargestellt werden, wenn kein 3-letter code vorhanden ist
								dann soll des 4-letter-code genommen werden...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LRM", 3))
				{
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: MTOW */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "MTOW"))
					{
						/* only if we found data */
						if (strlen(pclFieldDataBuffer))
						{
							/* this is MTOW */
							/* convert to new format... */
							dlMtow = atof(pclFieldDataBuffer);
							dlMtow *= 100;
							sprintf(pclFieldDataBuffer, "%05d", (int)dlMtow);
						}
					}
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: TTYP */
					else if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "TTYP"))
					{
						/* only if we found data */
						if (strlen(pclFieldDataBuffer))
						{
							/* this is TTYP */
							ilTtyp = atoi(pclFieldDataBuffer);
							sprintf(pclFieldDataBuffer, "%03d", ilTtyp);
						}
					}
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: DES3,ORG3 */
					else if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "DES3") || !strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ORG3"))
					{
						dbg(DEBUG,"<WDtF> found field: <%s> convert to 4-letter?...", rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
						if (strlen(pclFieldDataBuffer) != 3)
						{
							if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "DES3"))
							{
								/* this is DES3 */
								/* search DES4... */
								if ((ilRC = GetIndex(pcpFields, "DES4", ',')) >= 0)
								{
									if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
									{
										/* copy four letter code to buffer... */
										strcpy(pclFieldDataBuffer, pclS);
									}
								}
							}
							else
							{
								/* this is ORG3 */
								/* search ORG4 */
								if ((ilRC = GetIndex(pcpFields, "ORG4", ',')) >= 0)
								{
									if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
									{
										/* copy four letter code to buffer... */
										strcpy(pclFieldDataBuffer, pclS);
									}
								}
							}
						}
						else
						{
							strcat(pclFieldDataBuffer, " ");
						}
					}
				}
				/*****************************************************************************
						HAJ: Murks vierter Teil: bei EFA und LRM soll das Feld 'FLNO' jetzt mit
													8 Stellen ausgegeben werden.
									FLNO jetzt: 1,2,3,4,5,6,7,8,9. Character 8 ist momentan IMMER
									ein Blank. Dieser wird geloescht...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "EFA", 3)
				||  !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LRM", 3))
				{
					/* this is for LRM1, LRM2, LFA1... */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "FLNO"))
					{
						/* this is field FLNO... */
						pclFieldDataBuffer[7] = pclFieldDataBuffer[8];
						pclFieldDataBuffer[8] = 0x00;
					}
				}
				/*****************************************************************************
					HAJ: Jetzt muessen fuer die Langzeitdienstplaene, Kurzzeitdienstplaene und Ab-
					wesenheiten angepasst werden...
				*****************************************************************************/
				/* Langzeit-,Kurzzeitdiestplaene */
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LZD", 3) ||
					 !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "KZD", 3))
				{
					if (ilFieldNo == 4)
					{
						/* this is ZEIT-VON... */
						if ((ilRC = GetIndex(pcpFields, "CTYP", cCOMMA)) >= 0)
						{
							if (!strcmp(GetDataField(pcpData, ilRC, cCOMMA), "A"))
							{
                if (!strcmp(pclFieldDataBuffer, "0000"))
                {
								   strcpy(pclFieldDataBuffer, "FREI");
                }
							}
						}
					}

					/* Langzeitdienstplaene, Kurzzeitdienstplaene */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "SBLS"))
					{
						if ((ilRC = GetIndex(pcpFields, "SBFS", cCOMMA)) >= 0)
						{
							pclS = GetDataField(pcpData, ilRC, cCOMMA);
							if (strlen(pclS) == 14)
							{
								strcpy(pclTmpBuf, GetPartOfTimeStamp(pclS, "HHMI"));
								strncpy(pclHour, pclTmpBuf, 2);
								strncpy(pclMin, pclTmpBuf+2, 2);
								ilMin = atoi(pclMin);
								ilHour = atoi(pclHour);
								ilDiff = atoi(pclFieldDataBuffer);
								dbg(DEBUG,"-----------------------------------");
								dbg(DEBUG,"<HOUR: %d, MIN: %d, DIFF: %d>", ilHour, ilMin, ilDiff);
								ilDiffHour = (int)((ilMin+ilDiff)/60);
								ilDiffMin = (ilMin+ilDiff)%60;
								ilHour += ilDiffHour;
								ilHour %= 24;
								ilMin = ilDiffMin;
								dbg(DEBUG,"<DIFFHOUR: %d, DIFFMIN: %d>", ilDiffHour, ilDiffMin);
								dbg(DEBUG,"<HOUR: %d, MIN: %d>", ilHour, ilMin);
								sprintf(pclFieldDataBuffer, "%02d%02d", ilHour, ilMin);
								dbg(DEBUG,"-----------------------------------");
							}
							else
							{
								memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
							}
						}
						else
							memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					}
				}
				/* delete data if we ignore this field */
				if (ilIgnore)
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
			}

			/* 9. *******************************************/
			/* use syslib to get data from other tables... */
			/************************************************/
			for (ilSyslibNo=0, ilRC=0; ilSyslibNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoSyslib && !ilIgnore; ilSyslibNo++)
			{
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],
										rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcIField))
				{
					/* found field... */
					ilRC = 1;
					break;
				}
			}

			/* 10. *******************************************/
			/* get data from shared memory */
			/************************************************/
			if (ilRC && !ilIgnore && strlen(pclFieldDataBuffer))
			{
				#ifndef UFIS32
				/* use syslib */
				ilCnt = 1;
				memset((void*)pclSyslibResultBuf, 0x00, iMAXIMUM);

				ilOldDebugLevel = debug_level;
				debug_level = TRACE;

				/* ---------------------------------------------------------- */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].pcTableExtension, "TAB"))
				{
					sprintf(pclSyslibFields, "%s,HOPO",  rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabField);
					sprintf(pclSyslibFieldData, "%s,%s", pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcHomeAirport);
					ilRC = syslibSearchDbData(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabName
																		,pclSyslibFields, pclSyslibFieldData
																		,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcResultField
																		, pclSyslibResultBuf, &ilCnt, "");
				}
				else
				{
					ilRC = syslibSearchDbData(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabName
																	 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabField
																	 ,pclFieldDataBuffer
																	 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcResultField
																	 ,pclSyslibResultBuf, &ilCnt, "");
				}
				debug_level = ilOldDebugLevel;

				if (ilRC == RC_FAIL)
				{
					dbg(TRACE,"<WDtF> syslibSearchDbData returns with RC_FAIL");
					Terminate(0);
				}
				else if (ilRC == RC_NOT_FOUND)
				{
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
				}
				else
				{
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					strcpy(pclFieldDataBuffer, pclSyslibResultBuf);
				}
				#endif
			}

			if (!ilIgnore)
			{
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iDataLengthCounter > 0)
				{
					/* 11. *******************************************/
					/* data mapping function fixed length  */
					/************************************************/
					if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] > 0)
					{
						for (ilCurMapNo=0, ilFoundData=iNOT_FOUND; ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData == iNOT_FOUND; ilCurMapNo++)
						{
							/* set other data flag */
							ilOther = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].iOtherData;
							/* USC mapping of BLANK -> pcNewData */
							if (strlen(pclFieldDataBuffer) == 0) {
							  memset((void*)pclFieldDataBuffer, ' ', 1);
							  if (!strcmp(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0],"BLANK")) {
							    sprintf(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0]," ");
							  }
							}
							/* end of mapping of BLANK */
							if (strstr(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData) != NULL)
							{
								/* found it... */
								strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
								ilFoundData = iFOUND;
							}
						}

						/* check it */
						if (ilFoundData == iNOT_FOUND)
						{
							/* we doesn't found data for mapping */
							/* shoud i use org- or null-data? */
							if (ilOther == iNULL_DATA)
							{
								/* set NULL data */
								memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
							}
							/* USC support for arbitrary OTHER_DATA mapping */
							else if (ilOther == iORG_DATA) {
							  /* set arbitrary data to new data */
							  for (ilCurMapNo=0,ilFoundData=iNOT_FOUND;
							       ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData;
							       ilCurMapNo++) {
							    if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData,"OTHER_DATA")) {
							      dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <OTHER_DATA> => %s"
								  ,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],
								  rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							      strcpy(pclFieldDataBuffer,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							      ilFoundData = iFOUND;
							    }
							  }
							}
							/* USC end support for arbitrary OTHER_DATA mapping */
						}
					}

					/* dynamic but max. length */
					ilLen1 = strlen(pclFieldDataBuffer);
					/*ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo++];*/
					ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

					/* without max. length */
					dbg(DEBUG,"<WDtF><DYN> %3d.Field <%s>=<%s> len<%d>\tcfg-len<%d>",
						ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],pclFieldDataBuffer,ilLen1,ilLen2);

					strncat(pclDataBuffer, pclFieldDataBuffer, FMIN(ilLen1, ilLen2));

					/* set data separator */
					strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
				}
				else
				{
					/* 11. *******************************************/
					/* data mapping function dynamic length     */
					/************************************************/
					/* write without max length */
					if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] > 0)
					{
						for (ilCurMapNo=0, ilFoundData=iNOT_FOUND; ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData == iNOT_FOUND; ilCurMapNo++)
						{
							/* set other data flag */
							ilOther = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].iOtherData;
							/* USC mapping of BLANK -> pcNewData */
							if (strlen(pclFieldDataBuffer) == 0) {
							  memset((void*)pclFieldDataBuffer, ' ', 1);
							  if (!strcmp(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0],"BLANK")) {
							    sprintf(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0]," ");
							  }
							}
							/* end of mapping of BLANK */
							if (strstr(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData) != NULL)
							{
								/* found it... */
							dbg(DEBUG,"<WDtF><DYN> %3d.Field <%s> -- MAP <%s> => <%s>"
								,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]
								,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData
								,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
								strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
								ilFoundData = iFOUND;
							}
						}

						/* check it */
						if (ilFoundData == iNOT_FOUND)
						{
							/* we doesn't found data for mapping */
							/* shoud i use org- or null-data? */
							if (ilOther == iNULL_DATA)
							{
								/* set NULL data */
								memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
							dbg(DEBUG,"<WDtF><DYN> %3d.Field <%s> -- MAP <OTHER_DATA> => NULL"
									,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
							}
							else if (ilOther == iBLANK)
							{
											/* set BLANK data */
											dbg(DEBUG,"<WDtF><DYN> %3d.Field <%s> -- MAP <OTHER_DATA> => BLANK"
													,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
											memset((void*)pclFieldDataBuffer, iBLANK, iMAXIMUM);
											/* just to make output more readable */
											pclFieldDataBuffer[1]=0x00;
							}
							/* USC support for arbitrary OTHER_DATA mapping */
							else if (ilOther == iORG_DATA) {
							  /* set arbitrary data to new data */
							  for (ilCurMapNo=0,ilFoundData=iNOT_FOUND;
							       ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData;
							       ilCurMapNo++) {
							    if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData,"OTHER_DATA")) {
							      dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <OTHER_DATA> => %s"
								  ,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],
								  rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							      strcpy(pclFieldDataBuffer,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							      ilFoundData = iFOUND;
							    }
							  }
							}
							/* USC end support for arbitrary OTHER_DATA mapping */
						}
					}

					/* copy data to data buffer */
					dbg(DEBUG,"<WDtF><DYN> %3d.Field <%s>=<%s> len<%d>",
					    ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],pclFieldDataBuffer,strlen(pclFieldDataBuffer));
					strcat(pclDataBuffer, pclFieldDataBuffer);

					/* set data separator */
					strcat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
				}
			}
			else
			{
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iDataLengthCounter > 0)
					ilLengthNo++;
			}
		}/*end for */

		/* length of data */
		ilLen = strlen(pclDataBuffer);

		memset(pclLineBuffer,0x00,iMAXIMUM);
		if (strlen(rgTM.prHandleCmd[ipCmdNo].pcDataLinePrefix)>0)
		{
			strcat(pclLineBuffer,rgTM.prHandleCmd[ipCmdNo].pcDataLinePrefix);
		}

		strcat(pclLineBuffer,pclDataBuffer);

		if (strlen(rgTM.prHandleCmd[ipCmdNo].pcDataLinePostfix)>0)
		{
			strcat(pclLineBuffer,rgTM.prHandleCmd[ipCmdNo].pcDataLinePostfix);
		}
		ilLen = strlen(pclLineBuffer);

		/* write end of line characters */
		/*strncat(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcEndOfLine, rgTM.prHandleCmd[ipCmdNo].iNoEOLCharacters);*/
		ReplaceStdTokens(ipCmdNo,pclLineBuffer,0x00,
				rgTM.prHandleCmd[ipCmdNo].pcDynDateFormat,rgTM.prHandleCmd[ipCmdNo].pcDynTimeFormat,ipRecordNr);
		strncat(pclLineBuffer, rgTM.prHandleCmd[ipCmdNo].pcEndOfLine, rgTM.prHandleCmd[ipCmdNo].iNoEOLCharacters);
		strcpy(pclDataBuffer,pclLineBuffer);
	}
	else /* use FIX format */
	{
		/* this is at fix positions, data area must be initialized with blanks */
		memset((void*)pclDataBuffer, iBLANK, iMAXIMUM);
		pclDataBuffer[iMAXIMUM-1]=0x00;

		/*dbg(DEBUG,"*************** USE FIX-mode for data (blanks padded)! ****************************");	*/
		/* for all fields */
		ilTmpPos=0;
		for (ilFieldNo=0, ilLengthNo=0, ilCurrentPos=0;
			  ilFieldNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoFields;
			  ilFieldNo++,ilLengthNo++)
		{
			/*dbg(DEBUG," START handle field <%s> -->",rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);*/
			/* must we ignore this field... */
			ilIgnore = 0;

			/* 1. ***************************/
			/* must we ignore this field... */
			/********************************/
			for (ilIgnoreNo=0; ilIgnoreNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoOfIgnore; ilIgnoreNo++)
			{
				/* check current field */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcIgnoreFields[ilIgnoreNo]))
				{
					ilIgnore = 1;
					break;
				}
			}

			/* 2. ***************************/
			/* must write Fieldname */
			/********************************/
			if (rgTM.prHandleCmd[ipCmdNo].iFieldName)
			{
				if (!ilIgnore)
				{
					memset(pclMappedFieldName,0x00,iMIN);
					for (ilAssignNo=0; ilAssignNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoAssign; ilAssignNo++)
					{
						if (strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prAssign[ilAssignNo].pcDBField
											 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]) == 0)
						{
							strncpy(pclMappedFieldName,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prAssign[ilAssignNo].pcIFField,iMIN);
						}
					}
					ilTmpPos = ilCurrentPos;
					if (strlen(pclMappedFieldName) > 0)
					{
						memmove((pclDataBuffer+ilTmpPos), pclMappedFieldName,strlen(pclMappedFieldName));
						ilCurrentPos = ilTmpPos + strlen(pclMappedFieldName);
					}
					else
					{
						memmove((pclDataBuffer+ilTmpPos), rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]
							,strlen(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]));
						ilCurrentPos = ilTmpPos + strlen(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
					}
				}
			}

			/* 3. *******************************************/
			/* is this a const?, write it here and continue */
			/************************************************/
			if (!strncmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "CONSTANT_", 9) || !strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ACTUAL_DATE"))
			{
				/* is this actual date? */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ACTUAL_DATE"))
				{
					/* copy current (UTC) time to buffer */
					strcpy(pclFieldDataBuffer, GetTimeStamp());

					/* hier das mit dem timestamp format... */
					if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo] != NULL)
					{
						if ((pclS = GetPartOfTimeStamp(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo])) != NULL)
						{
							strcpy(pclFieldDataBuffer, pclS);
						}
						else
						{
							memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
						}
					}

				}
				else
				{
					/* ------------------------------------------------------- */
					if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LZD", 3) ||
						 !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "KZD", 3))
					{
						/*
						13.09.99 fuer ARE. Fuer LZD,KZD muss jetzt in Abhaengigkeit
						von SBPF eine Zeit gesetzt bzw. auf 0000 gesetzt werden...
						Tja, ein Jahr spaeter!!!
						*/
						ilSBLA = -1;
						if (ilCurConst == 2 || ilCurConst == 3)
						{
							/* get sbpf-data.... */
							if ((ilRC = GetIndex(pcpFields, "SBPF", cCOMMA)) >= 0)
							{
								if ((pclS = GetDataField(pcpData, ilRC, cCOMMA)) != NULL)
								{
									/* save it */
									if (!strcmp(pclS, "X") || !strcmp(pclS, "x"))
										/* write SBLA-data to 1st position...(bezahlt) */
/* TWE NEW */
                    ilSBLA = 1;
                  else
                    /* write SBLA-data to 2nd position...(unbezahlt) */
                    ilSBLA = 0;
/*********/
								}
							}

							/* save SBLA-Data for future use... */
							if ((ilRC = GetIndex(pcpFields, "SBLA", cCOMMA)) >= 0)
							{
								if ((pclS = GetDataField(pcpData, ilRC, cCOMMA)) != NULL)
                {
									strcpy(pclSBLA, pclS);
                }
								else
                {
                  dbg(TRACE,"FIELD SBLA IS NULL >=> NOT PAID ");
                  strcpy(pclSBLA, "");
									ilSBLA = -1;
                }
							}
						}

						if (!ilSBLA && ilCurConst == 3 || ilSBLA && ilCurConst == 2)
						{
							ilTmpSBLA = atoi(pclSBLA);
/* TWE NEW */
              /* change to decimal time */
              if (ilTmpSBLA >= 100)
              {
                 ilHour = (int)(ilTmpSBLA/100);
                 ilMin = (int)(ilTmpSBLA%100);
                 ilMin = (int)((100 * ilMin) / 60);
                 sprintf(pclFieldDataBuffer, "%02d%02d", ilHour,ilMin);
              }
              else
              {
                 ilMin = (int)(ilTmpSBLA%100);
                 ilMin = (int)((100 * ilMin) / 60);
                 sprintf(pclFieldDataBuffer, "00%02d", ilMin);
              }
/***************/
/*
							sprintf(pclFieldDataBuffer, "%04d", ilTmpSBLA);
*/
							ilCurConst++;
						}
						else
							/* copy default(config) to data buffer */
							strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcConst[ilCurConst++]);
						/* ------------------------------------------------------- */

					}
					else
          {
						/* copy default to data buffer */
						strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcConst[ilCurConst++]);
          }
				}

				/* write date to memory area... */
				ilLen1 = strlen(pclFieldDataBuffer);
				ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

				/* check the alignment of this field */
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcDataAlignment[ilFieldNo] == cRIGHT)
				{
					/* only if data is smaller than max length */
					if (ilLen1 < ilLen2)
						ilTmpPos = ilCurrentPos + ilLen2 - ilLen1;
					else
						ilTmpPos = ilCurrentPos;

					/* copy to my data aera */
					memmove((pclDataBuffer+ilTmpPos), pclFieldDataBuffer, FMIN(ilLen1,ilLen2));
				}
				else
				{
					/* copy to my data aera */
					memmove((pclDataBuffer+ilCurrentPos), pclFieldDataBuffer, FMIN(ilLen1,ilLen2));
				}

				/* set new position */
				ilCurrentPos += rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

				/* continue, and next field... */
				continue;
			}

			/* 4. *******************************************/
			/* is this a fuellfeld? */
			/************************************************/
			if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "FILL_FIELD"))
			{
				/* set new position */
				/*ilCurrentPos += rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo++];*/
				ilCurrentPos += rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

				/* next field, next try */
				continue;
			}

			/* 5. *******************************************/
			/* check VIA's */
			/************************************************/
			if ((strstr(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "VIX") != NULL))
			{
				/* clear data buffer... */
				memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);

				if (!ilIgnore)
				{
					/* extract VIAx from VIAL */
					if (ilNoOfVias > 0)
					{
						memset(pclVia,0x00,iMIN);
						strncpy(pclVia,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]+3,1);
						ilVia = atoi(pclVia);
						/*dbg(DEBUG,"<WDtF> Vias <%s>,<%d>",pclVia,ilVia);*/
						if (ilVia >= 1 && ilVia <= 8)
						{
							pclPtr = pclVial;
							/*dbg(DEBUG,"VIA: Vial<%s>",pclPtr);*/
							/*pclS = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo] + strlen(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]) - 1;*/
							if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iVia34 == 4)
							{
								/* get 4-letter code */
								strncpy(pclFieldDataBuffer, pclPtr+((ilVia-1)*120+4), 4);
							}
							else /* try to get 3-letter code */
							{
								if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iVia34 == 3)
								{
									strncpy(pclFieldDataBuffer, pclPtr+((ilVia-1)*120+1), 3);
								}
							}
						}
						else
						{
							dbg(TRACE,"<WDtF> WARNING: Invalid via entry in field list. Allowed entries are VIA1-VIA8. Setting this via to blank!");
							strncpy(pclFieldDataBuffer," ",1);
						}
						/*dbg(DEBUG,"%d.VIA-pclFieldDataBuffer: <%s>",ilVia,pclFieldDataBuffer);*/
					}
				}
			}
			else
			{
				/* get the data */
				memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
				if ((pclTmpPtr = CopyNextField(pclTmpPtr, cSEPARATOR, pclFieldDataBuffer)) == NULL)
				{
					dbg(TRACE,"<WDtF> cannot copy field (pcpData) %d", ilFieldNo);
					Terminate(0);
				}

				/* delete all right and left blanks */
				while (pclFieldDataBuffer[strlen(pclFieldDataBuffer)-1] == ' ')
					pclFieldDataBuffer[strlen(pclFieldDataBuffer)-1] = 0x00;

				/* 6. *******************************************/
				/* map from UTC to LocalTime... */
				/************************************************/
				for (ilCurUtc=0; ilCurUtc<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoOfUtcToLocal; ilCurUtc++)
				{
					/* compare map-fieldname and current field name */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcMapUtcToLocal[ilCurUtc]
										 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]))
					{
						/* changes by mos 8 Apr 2000 in the lions cage Warszawa */
						/* o-tone:"UAHH-that motherfucking * process inserts time without
							seconds in our database." */
						/* note, that there is no conversion to local time with wrong time
							formats*/
						/* for the health of mos, a fancy conversion will be implemented */
						if (strlen(pclFieldDataBuffer) == 12)
						{
								/* map to the CORRECT time format (with seconds)*/
								strcat(pclFieldDataBuffer, "00\0");
						}
						/* end of extreme sophisticated and lifesaving changes */

						/* only if we found data... */
						if (strlen(pclFieldDataBuffer) == 14)
						{
							/* set local buffer */
							strcpy(pclLocalTimeBuffer, pclFieldDataBuffer);

							/* call mapping function */
							if ((ilRC = UtcToLocal(pclLocalTimeBuffer)) != RC_SUCCESS)
							{
								dbg(DEBUG,"<WDtF> UtcToLocalTime returns: %d", ilRC);
								dbg(DEBUG,"<WDtF> use old timestamp now...");
							}
							else
							{
								strcpy(pclFieldDataBuffer, pclLocalTimeBuffer);
							}
						}
					}
				}

				/* 7. *******************************************/
				/* timestamp formatting         */
				/************************************************/
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo] != NULL)
				{
					if ((pclS = GetPartOfTimeStamp(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFieldTimestampFormat[ilFieldNo])) != NULL)
					{
						strcpy(pclFieldDataBuffer, pclS);
					}
					else
					{
						memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					}
				}

				/* 8. *******************************************/
				/* SUBSTR                       */
				/************************************************/
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iUseSubStr)
				{
					if (strlen(pclFieldDataBuffer))
					{
						memmove((void*)pclFieldDataBuffer, (const void*)&pclFieldDataBuffer[rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iStartPos-1], (size_t)rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iBytes);
						pclFieldDataBuffer[rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSubStr[ilFieldNo].iBytes] = 0x00;
					}
				}

				/*****************************************************************************
					 HAJ: fuer EFA-Flug und NDR muss hier was hardcoded rein!!.
					 Sehr unschoen aber ich weiß nicht wie ich's sonst machen soll...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "EFA", 3)
				||  !strcmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "NDR"))
				{
					/* this is for EFA1, EFA2, NDR... */
					/* map field REMP..., use LAND... */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "REMP"))
					{
						/* this is REMP */
						/* search LAND... */
						if ((ilRC = GetIndex(pcpFields, "LAND", ',')) >= 0)
						{
							if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
							{
								if (strlen(pclS) == 14)
								{
									/* must be landed... */
									/* set ftyp to my special code */
									memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
									strcpy(pclFieldDataBuffer, "GEL");
								}
							}
						}
					}
				}
				/*****************************************************************************
						HAJ: Murks zweiter Teil: 	Jetzt soll fuer Laerm das Feld MTOW ohne Punkt aber
													plus NULL hinterdran!? Das wird jetzt hardcoded an
													den Kommandos LRM1 und LRM2 festgemacht...
						Und Murks dritter Teil: Verkehrsart (TTYP) soll jetzt rechtsbuendig MIT
														Vornullen ausgegeben werden!!

						NEU: 23.01.98 <-> bei LRM sollen alle Airportcodes im 3-letter plus
								space dargestellt werden, wenn kein 3-letter code vorhanden ist
								dann soll des 4-letter-code genommen werden...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LRM", 3))
				{
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: MTOW */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "MTOW"))
					{
						/* only if we found data */
						if (strlen(pclFieldDataBuffer))
						{
							/* convert to new format... */
							dlMtow = atof(pclFieldDataBuffer);
							dlMtow *= 100;
							sprintf(pclFieldDataBuffer, "%05d", (int)dlMtow);
						}
					}
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: TTYP */
					else if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "TTYP"))
					{
						/* only if we found data */
						if (strlen(pclFieldDataBuffer))
						{
							/* this is TTYP */
							ilTtyp = atoi(pclFieldDataBuffer);
							sprintf(pclFieldDataBuffer, "%03d", ilTtyp);
						}
					}
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* ---------------------------------------------- */
					/* FIELD: DES3,ORG3 */
					else if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "DES3") || !strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "ORG3"))
					{
						if (strlen(pclFieldDataBuffer) != 3)
						{
							if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "DES3"))
							{
								/* this is DES3 */
								/* search DES4... */
								if ((ilRC = GetIndex(pcpFields, "DES4", ',')) >= 0)
								{
									if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
									{
										/* copy four letter code to buffer... */
										strcpy(pclFieldDataBuffer, pclS);
									}
								}
							}
							else
							{
								/* this is ORG3 */
								/* search ORG4 */
								if ((ilRC = GetIndex(pcpFields, "ORG4", ',')) >= 0)
								{
									if ((pclS = GetDataField(pcpData, ilRC, ',')) != NULL)
									{
										/* copy four letter code to buffer... */
										strcpy(pclFieldDataBuffer, pclS);
									}
								}
							}
						}
						else
						{
							strcat(pclFieldDataBuffer, " ");
						}
					}
				}
				/*****************************************************************************
						HAJ: Murks vierter Teil: bei EFA und LRM soll das Feld 'FLNO' jetzt mit
													8 Stellen ausgegeben werden.
									FLNO jetzt: 1,2,3,4,5,6,7,8,9. Character 8 ist momentan IMMER
									ein Blank. Dieser wird geloescht...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "EFA", 3)
				||  !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LRM", 3))
				{
					/* this is for LRM1, LRM2, LFA1... */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "FLNO"))
					{
						/* this is field FLNO... */
						pclFieldDataBuffer[7] = pclFieldDataBuffer[8];
						pclFieldDataBuffer[8] = 0x00;
					}
				}
				/*****************************************************************************
					HAJ: Jetzt muessen fuer die Langzeitdienstplaene, Kurzzeitdienstplaene und Ab-
					wesenheiten angepasst werden...
				*****************************************************************************/
				if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LZD", 3) ||
					 !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "KZD", 3))
				{
					/* TWE NEW */
          if(igUse2400 == 1)
					{
					/* SAP needs  this */
							/* CHANGE FROM 2359 to 2400 */
							if (!strcmp(pclFieldDataBuffer, "2359"))
								{
								 strcpy(pclFieldDataBuffer, "2400");
								}

								/* IF (ONLY)SCHICHTENDE CHANGE FROM 0000 to 2400 */
							if (ilFieldNo == 5)
								{
								/* this is ZEIT-BIS... */
									if (!strcmp(pclFieldDataBuffer, "0000"))
								{
										strcpy(pclFieldDataBuffer, "2400");
									}
							}
					}
					/***************/

					if (ilFieldNo == 4)
					{
						/* this is ZEIT-VON... */
						if ((ilRC = GetIndex(pcpFields, "CTYP", cCOMMA)) >= 0)
						{
							if (!strcmp(GetDataField(pcpData, ilRC, cCOMMA), "A"))
							{
                if (!strcmp(pclFieldDataBuffer, "0000"))
                {
                   strcpy(pclFieldDataBuffer, "FREI");
                }
							}
						}
					}

					/* Langzeitdienstplaene */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo], "SBLS"))
					{
						if ((ilRC = GetIndex(pcpFields, "SBFS", cCOMMA)) >= 0)
						{
							pclS = GetDataField(pcpData, ilRC, cCOMMA);
							if (strlen(pclS) == 14)
							{
								strcpy(pclTmpBuf, GetPartOfTimeStamp(pclS, "HHMI"));
								strncpy(pclHour, pclTmpBuf, 2);
								strncpy(pclMin, pclTmpBuf+2, 2);
								ilMin = atoi(pclMin);
								ilHour = atoi(pclHour);
								ilDiff = atoi(pclFieldDataBuffer);
								dbg(DEBUG,"-----------------------------------");
								dbg(DEBUG,"<HOUR: %d, MIN: %d, DIFF: %d>", ilHour, ilMin, ilDiff);
								ilDiffHour = (int)((ilMin+ilDiff)/60);
								ilDiffMin = (ilMin+ilDiff)%60;
								ilHour += ilDiffHour;
								ilHour %= 24;
								ilMin = ilDiffMin;
								dbg(DEBUG,"<DIFFHOUR: %d, DIFFMIN: %d>", ilDiffHour, ilDiffMin);
								dbg(DEBUG,"<HOUR: %d, MIN: %d>", ilHour, ilMin);
								sprintf(pclFieldDataBuffer, "%02d%02d", ilHour, ilMin);
								dbg(DEBUG,"-----------------------------------");
							}
							else
							{
								memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
							}
						}
						else
							memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					}
				}
				/* delete data if we ignore this field */
				if (ilIgnore)
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
			}

			/* 9. *******************************************/
			/* use syslib to get data from other tables... */
			/************************************************/
			for (ilSyslibNo=0, ilRC=0; ilSyslibNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoSyslib && !ilIgnore; ilSyslibNo++)
			{
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]
									 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcIField))
				{
					/* found field... */
					ilRC = 1;
					break;
				}
			}

			/* 10. *******************************************/
			/* get data from shared memory */
			/************************************************/
			if (ilRC && !ilIgnore && strlen(pclFieldDataBuffer))
			{
				#ifndef UFIS32
				/* use syslib */
				ilCnt = 1;
				memset((void*)pclSyslibResultBuf, 0x00, iMAXIMUM);

				ilOldDebugLevel = debug_level;
				debug_level = TRACE;

				/* ---------------------------------------------------------- */
				if (!strcmp(rgTM.prHandleCmd[ipCmdNo].pcTableExtension, "TAB"))
				{
					sprintf(pclSyslibFields, "%s,HOPO",  rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabField);
					sprintf(pclSyslibFieldData, "%s,%s", pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcHomeAirport);
					ilRC = syslibSearchDbData(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabName
																	 ,pclSyslibFields, pclSyslibFieldData
																	 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcResultField
																	 ,pclSyslibResultBuf, &ilCnt, "");
				}
				else
				{
					ilRC = syslibSearchDbData(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabName
																	 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcTabField
																	 ,pclFieldDataBuffer
																	 ,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prSysLib[ilSyslibNo].pcResultField
																	 ,pclSyslibResultBuf, &ilCnt, "");
				}
				/* ---------------------------------------------------------- */

				debug_level = ilOldDebugLevel;

				if (ilRC == RC_FAIL)
				{
					dbg(TRACE,"<WDtF> syslibSearchDbData returns with RC_FAIL");
					Terminate(0);
				}
				else if (ilRC == RC_NOT_FOUND)
				{
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
				}
				else
				{
					memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
					strcpy(pclFieldDataBuffer, pclSyslibResultBuf);
				}
				#endif
			}

			if (!ilIgnore)
			{
				/* 11. *******************************************/
				/* must we map received data */
				/************************************************/
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] > 0)
				{
					for (ilCurMapNo=0, ilFoundData=iNOT_FOUND;
							 ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData == iNOT_FOUND;
							 ilCurMapNo++)
					{
						/* set other data flag */
						ilOther = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].iOtherData;
						/* USC mapping of BLANK -> pcNewData */
						if (strlen(pclFieldDataBuffer) == 0) {
						  memset((void*)pclFieldDataBuffer, ' ', 1);
						  if (!strcmp(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0],"BLANK")) {
						    sprintf(&rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData[0]," ");
						  }
						}
						/* end of mapping of BLANK */
						if (strstr(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData) != NULL)
						{
							/* found it... */
							dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <%s> => <%s>"
								,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]
								,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData
								,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							strcpy(pclFieldDataBuffer, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
							ilFoundData = iFOUND;
						}
					}

					/* check it */
					if (ilFoundData == iNOT_FOUND)
					{
						/* we doesn't found data for mapping */
						/* shoud i use org- or null-data? */
						if (ilOther == iNULL_DATA)
						{
							/* set NULL data */
							memset((void*)pclFieldDataBuffer, 0x00, iMAXIMUM);
							dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <OTHER_DATA> => NULL"
									,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
						}
						else if (ilOther == iBLANK)
						{
							/* set BLANK data */
							dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <OTHER_DATA> => BLANK"
									,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo]);
							memset((void*)pclFieldDataBuffer, iBLANK, iMAXIMUM);
							/* just to make output more readable */
							pclFieldDataBuffer[1]=0x00;
						}
						/* USC support for arbitrary OTHER_DATA mapping */
						else if (ilOther == iORG_DATA) {
						  /* set arbitrary data to new data */
						  for (ilCurMapNo=0,ilFoundData=iNOT_FOUND;
						       ilCurMapNo<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piNoMapData[ilFieldNo] && ilFoundData;
						       ilCurMapNo++) {
						    if (!strcmp(rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcOrgData,"OTHER_DATA")) {
						      dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s> -- MAP <OTHER_DATA> => %s"
							  ,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],
							  rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
						      strcpy(pclFieldDataBuffer,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].prDataMap[ilFieldNo][ilCurMapNo].pcNewData);
						      ilFoundData = iFOUND;
						    }
						  }
						}
						/* USC end support for arbitrary OTHER_DATA mapping */
					}
				}
				/* write date to memory area... */
				ilLen1 = strlen(pclFieldDataBuffer);
				ilLen2 = rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];

				/* check the alignment of this field */
				if (rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcDataAlignment[ilFieldNo] == cRIGHT)
				{
					/* only if data is smaller than max length */
					if (ilLen1 < ilLen2)
						ilTmpPos = ilCurrentPos + ilLen2 - ilLen1;
					else
						ilTmpPos = ilCurrentPos;

					dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s>=<%s> len<%d>\tcfg-len<%d>\tstartpos.<%d>",
						ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],pclFieldDataBuffer,strlen(pclFieldDataBuffer),rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo],ilTmpPos);
					/* copy to my data aera */
					memmove((pclDataBuffer+ilTmpPos), pclFieldDataBuffer, FMIN(ilLen1,ilLen2));
				}
				else
				{
			/*------------------------------------------------------------*/
      /* Nicht schoen, aber funktioniert */
			/* Abwesenheiten in Hannover Zeit-von ,Zeit-bis = blank wenn 0000 */
			if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "ABW", 3))
			{
  			  dbg(TRACE," Command <%s> FieldDataBuffer <%s> FieldNo <%d>",
					rgTM.prHandleCmd[ipCmdNo].pcCommand,pclFieldDataBuffer,ilFieldNo);
  			  dbg(TRACE,"Test: Command <%s>",rgTM.prHandleCmd[ipCmdNo].pcCommand);

					if ((ilFieldNo == 5) || (ilFieldNo == 6))
			    {
			    	/* this is ZEIT-VON... or ZEIT-BIS... +/
					   if ((ilRC = GetIndex(pcpFields, "CTYP", cCOMMA)) >= 0)
					   {
						    if (strcmp(GetDataField(pcpData, ilRC, cCOMMA), "A"))
						    { */
									 dbg(TRACE,"Test:FieldNo <%d> pclFieldDataBuffer<%s>",
									 ilFieldNo,pclFieldDataBuffer);
									 if (!strcmp(pclFieldDataBuffer, "0000"))
                   {
							       strcpy(pclFieldDataBuffer,"    ");
						 		     dbg(TRACE,"Test: Converted :pclFieldDataBuffer<%s>",pclFieldDataBuffer);
                   }
				     /*}
						 {*/
			    }
        }
				/*------------------------------------------------------------*/
				/* copy to my data aera */
				dbg(DEBUG,"<WDtF><FIX> %3d.Field <%s>=<%s> len<%d>\tcfg-len<%d>\tstartpos.<%d>"
					,ilFieldNo,rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[ilFieldNo],pclFieldDataBuffer
					,strlen(pclFieldDataBuffer),rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo],ilCurrentPos);

					memmove((pclDataBuffer+ilCurrentPos), pclFieldDataBuffer, FMIN(ilLen1,ilLen2));
				}

				/* set new position */
				/*ilCurrentPos += rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo++];*/
				ilCurrentPos += rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].piDataLength[ilLengthNo];
			}
		}
		/* set len */
		ilLen = ilCurrentPos;

		memset(pclLineBuffer,0x00,iMAXIMUM);

		ilLen3=strlen(rgTM.prHandleCmd[ipCmdNo].pcDataLinePrefix);
		if (ilLen3>0)
		{
			strcpy(pclLineBuffer,rgTM.prHandleCmd[ipCmdNo].pcDataLinePrefix);
		}

		strncat(pclLineBuffer,pclDataBuffer,ilLen);

		ilLen4=strlen(rgTM.prHandleCmd[ipCmdNo].pcDataLinePostfix);
		if (ilLen4>0)
		{
			strncat(pclLineBuffer,rgTM.prHandleCmd[ipCmdNo].pcDataLinePostfix,ilLen4);
		}

		ReplaceStdTokens(ipCmdNo,pclLineBuffer,0x00,
				rgTM.prHandleCmd[ipCmdNo].pcDynDateFormat,rgTM.prHandleCmd[ipCmdNo].pcDynTimeFormat,ipRecordNr);

		ilLen=strlen(pclLineBuffer);

		/* write end of line characters */
		/*memmove((pclLineBuffer+ilLen), rgTM.prHandleCmd[ipCmdNo].pcEndOfLine, rgTM.prHandleCmd[ipCmdNo].iNoEOLCharacters);*/
		strncat((pclLineBuffer), rgTM.prHandleCmd[ipCmdNo].pcEndOfLine, rgTM.prHandleCmd[ipCmdNo].iNoEOLCharacters);

		/* now cleaning the buffer from 0x00 values because we are here in the fix section,*/
		/* and the fwrite()-function does not recognise them so that we get it like dynamically */
		ilCnt1=0;
		do
		{
			if (pclLineBuffer[ilCnt1] == 0x00)
				{pclLineBuffer[ilCnt1]=0x20;}
			ilCnt1++;
		}while (ilCnt1 < (ilLen));
		strcpy(pclDataBuffer,pclLineBuffer);

	}/*end else FIX-positions */

	/* set new length */
	ilLen += rgTM.prHandleCmd[ipCmdNo].iNoEOLCharacters;

	/* normaly we fetch ansi -> convert to ascii? */
	if (IsAscii(rgTM.prHandleCmd[ipCmdNo].iCharacterSet))
		AnsiToAscii((UCHAR*)pclDataBuffer);

	/* HAJ_TWE NEW */
	/**********************************/
  ilSendToHR = TRUE;
  if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "LZD", 3) ||
  		 !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "KZD", 3) ||
       !strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "ABW", 3))
	{
     /* get Personalnummer of record */
     if ((ilRC = GetIndex(pcpFields, "PENO", cCOMMA)) >= 0)
     {
       pclS = GetDataField(pcpData, ilRC, cCOMMA);
       if (strlen(pclS) > 0)
       {
         strcpy(pclPenoData, pclS);
       }
       else
       {
         strcpy(pclPenoData, "");
       }
     }

     /* descision wether data is send to HR depending on field TOHR in table STF for Mitarbeiter record */
	   if (!strcmp(rgTM.prHandleCmd[ipCmdNo].pcTableExtension, "TAB"))
	   {
       /* Suche in STFTAB WHERE PENO = PENO && HOPO = HOPO */
		   sprintf(pclSyslibFields, "PENO,HOPO");
		   sprintf(pclSyslibFieldData, "%s,%s", pclPenoData, rgTM.prHandleCmd[ipCmdNo].pcHomeAirport);
		   ilRC = syslibSearchDbData("STFTAB",
                                    pclSyslibFields,
                                    pclSyslibFieldData,
                                    "TOHR",
                                    pclSyslibResultBuf, &ilCnt, "");
       if (ilRC != RC_SUCCESS)
       {
          dbg(TRACE,"syslibSearchDbData in STFTAB for TOHR with PENO <%s> and HOPO <%s> failed (%d)",
                     pclPenoData, rgTM.prHandleCmd[ipCmdNo].pcHomeAirport, ilRC);
          ilSendToHR = TRUE;
       }
       else
       {
          if (!(strcmp(pclSyslibResultBuf, "J")) || !(strcmp(pclSyslibResultBuf, "j")))
          {
             ilSendToHR = TRUE;
          }
          else
          {
             ilSendToHR = FALSE;
             dbg(DEBUG,"NO SEND TO HR FOR PERSNO: <%s>", pclPenoData);
          }
       }
	   }
	   else
	   {
          sprintf(pclTable, "STF%s", rgTM.prHandleCmd[ipCmdNo].pcTableExtension);
          sprintf(pclSyslibFields, "PENO");
          sprintf(pclSyslibFieldData, "%s", pclPenoData);
          ilRC = syslibSearchDbData(pclTable,
                                    pclSyslibFields,
                                    pclSyslibFieldData,
                                    "TOHR",
                                    pclSyslibResultBuf, &ilCnt, "");
          if (ilRC != RC_SUCCESS)
          {
             dbg(TRACE,"syslibSearchDbData in STFTAB for TOHR with PENO <%s> failed (%d)",
                        pclPenoData, ilRC);
             ilSendToHR = TRUE;
          }
          else
          {
             if (!(strcmp(pclSyslibResultBuf, "J")) || !(strcmp(pclSyslibResultBuf, "j")))
             {
                ilSendToHR = TRUE;
             }
             else
             {
                ilSendToHR = FALSE;
                dbg(TRACE,"NO SEND TO HR FOR PERSNO: <%s>", pclPenoData);
             }
          }
	   }

     if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "ABW", 3)
         && (ilSendToHR == TRUE))      /* ABW */
     {
        /* get Personalnummer of record */
        if ((ilRC = GetIndex(pcpFields, "PENO", cCOMMA)) >= 0)
        {
          pclS = GetDataField(pcpData, ilRC, cCOMMA);
          if (strlen(pclS) > 0)
          {
            strcpy(pclPenoData, pclS);
          }
          else
          {
            strcpy(pclPenoData, "");
          }
        }
        if ((ilRC = GetIndex(pcpFields, "AVFA", cCOMMA)) >= 0)
        {
          pclS = GetDataField(pcpData, ilRC, cCOMMA);
					if (strlen(pclS) > 0)
          {
            strncpy(pclDay, pclS, 8);
            pclDay[8] = 0x00;
          }
          else
          {
            strcpy(pclDay, "");
          }
        }
        /*
           Descision wether data is send to HR depending on field FREE in Table ODA.
           Get right record in ODA by SDAC field that is same like SFCA for PENO in DSR table.
        */
        if (!strcmp(rgTM.prHandleCmd[ipCmdNo].pcTableExtension, "TAB"))
        {
          /* Suche in DSRTAB SFCA WHERE PENO = PENO SDAY = pclDay && HOPO = HOPO */
          sprintf(pclSyslibFields, "PENO,HOPO,SDAY");
          sprintf(pclSyslibFieldData, "%s,%s,%s",
                                      pclPenoData,
                                      rgTM.prHandleCmd[ipCmdNo].pcHomeAirport,
                                      pclDay);
          ilRC = syslibSearchDbData("DSRTAB",
                                     pclSyslibFields,
                                     pclSyslibFieldData,
                                     "SFCA",
                                     pclSyslibResultBuf, &ilCnt, "");
          if (ilRC != RC_SUCCESS)
          {
             dbg(TRACE,"syslibSearchDbData in DSRTAB for SFCA with PENO <%s> and HOPO <%s> at SDAY <%s> failed (%d)",
                                        pclPenoData,
                                        rgTM.prHandleCmd[ipCmdNo].pcHomeAirport,
                                        pclDay, ilRC);
             ilSendToHR = TRUE;
          }
          else
          {
             /* Suche in ODATAB FREE WHERE SDAC = SFCA && HOPO = HOPO */
             sprintf(pclSyslibFields, "SDAC,HOPO");
             sprintf(pclSyslibFieldData, "%s,%s", pclSyslibResultBuf,rgTM.prHandleCmd[ipCmdNo].pcHomeAirport);
             pclSyslibResultBuf[0] = 0x00;
             ilRC = syslibSearchDbData("ODATAB",
                                       pclSyslibFields,
                                       pclSyslibFieldData,
                                       "FREE",
                                       pclSyslibResultBuf, &ilCnt, "");
             if (ilRC != RC_SUCCESS)
             {
                dbg(TRACE,"syslibSearchDbData in ODATAB for FREE with SDAC,HOPO <%s> failed (%d)",
                           pclSyslibFieldData, ilRC);
                ilSendToHR = FALSE;
             }
             else
             {
                if (!(strcmp(pclSyslibResultBuf, "X")) || !(strcmp(pclSyslibResultBuf, "x")))
                {
                   ilSendToHR = FALSE;
                   dbg(DEBUG,"NO SEND TO HR FOR SDAC,HOPO: <%s>", pclSyslibFieldData);
                }
                else
                {
                   ilSendToHR = TRUE;
                }
             } /* FREE found */
          } /* SFCA found */
        } /* if TAB extent */
        else     /* table extent != TAB */
        {
             sprintf(pclTable, "DSR%s", rgTM.prHandleCmd[ipCmdNo].pcTableExtension);
             sprintf(pclSyslibFields, "PENO,SDAY");
             sprintf(pclSyslibFieldData, "%s,%s", pclPenoData,pclDay);
             ilRC = syslibSearchDbData(pclTable,
                                       pclSyslibFields,
                                       pclSyslibFieldData,
                                       "SFCA",
                                       pclSyslibResultBuf, &ilCnt, "");
             if (ilRC != RC_SUCCESS)
             {
                dbg(TRACE,"syslibSearchDbData in DSRTAB for SFCA with PENO <%s> at SDAY <%s> failed (%d)",
                           pclPenoData, pclDay, ilRC);
                ilSendToHR = TRUE;
             }
             else
             {
                sprintf(pclTable, "ODA%s", rgTM.prHandleCmd[ipCmdNo].pcTableExtension);
                sprintf(pclSyslibFields, "SDAC");
                sprintf(pclSyslibFieldData, "%s", pclSyslibResultBuf);
                pclSyslibResultBuf[0] = 0x00;
                ilRC = syslibSearchDbData(pclTable,
                                       pclSyslibFields,
                                       pclSyslibFieldData,
                                       "FREE",
                                       pclSyslibResultBuf, &ilCnt, "");
                if (ilRC != RC_SUCCESS)
                {
                   dbg(TRACE,"syslibSearchDbData in ODA for FREE with SDAC = <%s> failed (%d)",
                              pclSyslibFieldData, ilRC);
                   ilSendToHR = FALSE;
                }
                else
                {
                   if (!(strcmp(pclSyslibResultBuf, "X")) || !(strcmp(pclSyslibResultBuf, "x")))
                   {
                      ilSendToHR = FALSE;
                      dbg(DEBUG,"NO SEND TO HR FOR SDAC: <%s>", pclSyslibFieldData);
                   }
                   else
                   {
                      ilSendToHR = TRUE;
                   }
                } /* FREE found */
             } /* SFCA found */
        } /* if tabextent != TAB */

     } /* if ABW  */


  } /* if KZD,LZD,ABW */
	/*************************/

	/* write line to file */
  if (ilSendToHR == TRUE)
  {
		 dbg(DEBUG,"<WDtF> fwrite() <%d>.bytes=<%s>",ilLen,pclDataBuffer);
	   if ((ilRC = fwrite((void*)pclDataBuffer, ilLen, 1, rgTM.prHandleCmd[ipCmdNo].pFh)) <= 0)
	   {
		   dbg(DEBUG,"<WDtF> error writing data to file");
		   return iWRITE_FILE_FAIL;
	   }
  }

	/* must i write second record for via? */
	if (*pipSecondRecord)
	{
		/* reset second record flag... */
		*pipSecondRecord = 0;

		/* fuer den NDR muessen wir event. eine zweite Zeile schreiben... */
		if (!strncmp(rgTM.prHandleCmd[ipCmdNo].pcCommand, "NDR", 3))
		{
			if (ilNoOfVias > 0 && strlen(pclVial))
			{
				for (i=0, ilRC=0, pclS=pclVial; i<ilNoOfVias; i++, pclS+=120)
				{
					/* only for via1... */
					if (*pclS == '1')
					{
						pclS++;
						ilRC = 1;
						break;
					}
				}

				if (ilRC)
				{
					memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
					strncpy(pclTmpBuf, pclS, 3);

					ilCnt = 1;
					memset((void*)pclSyslibResultBuf, 0x00, iMAXIMUM);

					/* build tablename... */
					memset((void*)pcgTmpTabName, 0x00, iMIN);
					sprintf(pcgTmpTabName, "APT%s", rgTM.prHandleCmd[ipCmdNo].pcTableExtension);

					/* set new and save old debug_level... */
					ilOldDebugLevel = debug_level;
					debug_level = TRACE;

					/* ---------------------------------------------------------- */
					if (!strcmp(rgTM.prHandleCmd[ipCmdNo].pcTableExtension, "TAB"))
					{
						strcpy(pclSyslibFields, "APC3,HOPO");
						sprintf(pclSyslibFieldData, "%s,%s", pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcHomeAirport);
						ilRC = syslibSearchDbData(pcgTmpTabName, pclSyslibFields, pclSyslibFieldData, "APSN", pclSyslibResultBuf, &ilCnt, "");
					}
					else
					{
						ilRC = syslibSearchDbData(pcgTmpTabName, "APC3", pclTmpBuf, "APSN", pclSyslibResultBuf, &ilCnt, "");
					}
					/* ---------------------------------------------------------- */

					debug_level = ilOldDebugLevel;
					if (ilRC == RC_SUCCESS)
					{
						memset((void*)pclTmpDataBuffer, 0x00, iMAX);

						/* add second row */
						pclS = strstr(pclDataBuffer, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
						pclS++;
						strncpy(pclTmpBuf, pclDataBuffer, pclS-pclDataBuffer);
						pclS = strstr(pclS, rgTM.prHandleCmd[ipCmdNo].pcDataSeparator);
						strcpy(pclTmpDataBuffer, pclS);

						memset((void*)pclDataBuffer, 0x00, iMAX);
						sprintf(pclDataBuffer, "%s%s%s", pclTmpBuf, pclSyslibResultBuf, pclTmpDataBuffer);

						/* normaly we fetch ansi -> convert to ascii? */
						if (IsAscii(rgTM.prHandleCmd[ipCmdNo].iCharacterSet))
							AnsiToAscii((UCHAR*)pclDataBuffer);

						if ((ilRC = fwrite((void*)pclDataBuffer, strlen(pclDataBuffer), 1, rgTM.prHandleCmd[ipCmdNo].pFh)) <= 0)
						{
							dbg(DEBUG,"<WDtF> error writing data to file");
							return iWRITE_FILE_FAIL;
						}

						/* set flag */
						*pipSecondRecord = 1;
					}
				}
			}
		}
	}
	else
	{
		*pipSecondRecord = 0;
	}

	/* successfull return */
	return RC_SUCCESS;
}

/******************************************************************************/
/* The send file routine                                                      */
/******************************************************************************/
static int SendFileToMachine(int ipCmdNo, int ipTabNo)
{
	/* for all... */
	int			i;
	int			ilRC;
	int			ilStatus;
	int			ilChildPid;
	char			pclFieldBuf[iMAXIMUM];
	char			pclTmpBuf[iMIN_BUF_SIZE];
	char			*pclTmpPath			= NULL;
	char			pclTmpFile[iMIN_BUF_SIZE];
	FILE			*fh 					= NULL;

	/* send to */
	int			ilLen					= 0;
	long			lFileLength			= 0;
	EVENT			*prlOutEvent		= NULL;
	BC_HEAD		*prlOutBCHead		= NULL;
	CMDBLK		*prlOutCmdblk		= NULL;
	char			*pclOutSelection	= NULL;
	char			*pclOutFields		= NULL;
	char			*pclOutData			= NULL;
	char			*pclFileContents	= NULL;
	char			*pclpasswd		= NULL;
	char			pclTmpDat[iMAX_BUF_SIZE];

	/*changes by MOS 4 July 2000 */
	char			pclNumtabvalue[iMIN_BUF_SIZE]; /*Current number circle value for IDS interface */
	char                    pclNumtabacnu[iMIN_BUF_SIZE];
	char 			pclNumtabkey[iMIN_BUF_SIZE];

	if (!rgTM.prHandleCmd[ipCmdNo].iOpenDataFile)
	{
		/* here we create an empty file for sending... */
		if ((ilRC = OpenFile(ipCmdNo, ipTabNo)) < 0)
		{
			dbg(DEBUG,"<SFtM> Can't open empty file...%d", ilRC);
			return ilRC;
		}

		/* write end of block */
		if ((ilRC = WriteEOB(ipCmdNo)) != RC_SUCCESS)
		{
			dbg(DEBUG,"<SFtM> WriteEOB returns %d", ilRC);
			return ilRC;
		}
	}

	/* first check file, exist it? */
	if (rgTM.prHandleCmd[ipCmdNo].iOpenDataFile)
	{
		/* write EOF-Sign if necessary */
		if (rgTM.prHandleCmd[ipCmdNo].iUseEOF)
		{
			/* here we copy back the orgiginal EOF-line which still contains the std.-tokens for replacement */
			/* otherwise we  would lose the std.-tokens and they would not contain actual values after  */
			/* the first run of the according section  */
			strcpy(rgTM.prHandleCmd[ipCmdNo].pcEndOfFile,rgTM.prHandleCmd[ipCmdNo].pcEndOfFileOrg);
      /* replacing poss. standard-tokens inside footer line */
      memset(pclTmpDat,0x00,iMAX_BUF_SIZE);
      sprintf(pclTmpDat,"%s",rgTM.prHandleCmd[ipCmdNo].pcTmpFileName);
      ReplaceStdTokens(ipCmdNo,rgTM.prHandleCmd[ipCmdNo].pcEndOfFile,pclTmpDat,
        rgTM.prHandleCmd[ipCmdNo].pcDynDateFormat,rgTM.prHandleCmd[ipCmdNo].pcDynTimeFormat,-1);

			/* must write EOF-Sign */
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh, "%s%s",
					rgTM.prHandleCmd[ipCmdNo].pcEndOfFile,
					rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}

		/* close datafile (if it is open) */
		dbg(DEBUG,"<SFtM> CloseFile  <%s>",
					rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath);
		fclose(rgTM.prHandleCmd[ipCmdNo].pFh);

		/* set flag to zero */
		rgTM.prHandleCmd[ipCmdNo].iOpenDataFile = 0;

		errno = 0;
		dbg(DEBUG,"<SFtM> RenameFile <%s> to <%s>"
			,rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath);
		if ((rename(rgTM.prHandleCmd[ipCmdNo].pcTmpFileNameWithPath,
															rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath)) != 0)
		{
			dbg(TRACE,"<SFtM> Error on rename of file! errno=<%d> => <%s>",errno,strerror(errno));
			return RC_FAIL;
		}


		/* send file to specified machine... */
		/* build control file */
		/* execute command only if it is valid */
		if (rgTM.prHandleCmd[ipCmdNo].iUseFTP)
		{
			/* should we check file difference? */
			if (rgTM.prHandleCmd[ipCmdNo].iCheckFileDiff)
			{
				/* get contents of new data file... */
				dbg(DEBUG,"<SFtM> reading new data file: <%s>", rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath);

				/* check pointer first */
				if (rgTM.prHandleCmd[ipCmdNo].pcFileContent != NULL)
				{
					free((void*)rgTM.prHandleCmd[ipCmdNo].pcFileContent);
					rgTM.prHandleCmd[ipCmdNo].pcFileContent = NULL;
					rgTM.prHandleCmd[ipCmdNo].lFileLength = -1;
				}

				if ((ilRC = GetFileContents(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath, &rgTM.prHandleCmd[ipCmdNo].lFileLength, (char**)&rgTM.prHandleCmd[ipCmdNo].pcFileContent)) != RC_SUCCESS)
				{
					dbg(TRACE,"<SFtM> GetFileContents returns: %d", ilRC);
					rgTM.prHandleCmd[ipCmdNo].lFileLength = -1;
					rgTM.prHandleCmd[ipCmdNo].pcFileContent = NULL;
				}
				else
					dbg(DEBUG,"<SFtM> ready reading new data file");

				/* compare content of both files (old and new) */
				if (rgTM.prHandleCmd[ipCmdNo].lOldFileLength ==
				    rgTM.prHandleCmd[ipCmdNo].lFileLength)
				{
					ilRC = memcmp((const void*)rgTM.prHandleCmd[ipCmdNo].pcOldFileContent, (const void*)rgTM.prHandleCmd[ipCmdNo].pcFileContent, (size_t)rgTM.prHandleCmd[ipCmdNo].lFileLength);
				}
				else
				{
					/* length isn't equal, send it */
					ilRC = 1;
				}
			}
			else
				ilRC = 1;

			/* delete memory for file content... */
			if (rgTM.prHandleCmd[ipCmdNo].pcFileContent != NULL)
			{
				free((void*)rgTM.prHandleCmd[ipCmdNo].pcFileContent);
				rgTM.prHandleCmd[ipCmdNo].pcFileContent = NULL;
				rgTM.prHandleCmd[ipCmdNo].lFileLength = -1;
			}
			if (rgTM.prHandleCmd[ipCmdNo].pcOldFileContent != NULL)
			{
				free((void*)rgTM.prHandleCmd[ipCmdNo].pcOldFileContent);
				rgTM.prHandleCmd[ipCmdNo].pcOldFileContent = NULL;
				rgTM.prHandleCmd[ipCmdNo].lOldFileLength = -1;
			}


		/*Changes by mos 28 June 2000 for FTPHDL implementation *****/
		/****************FTP System Call**************** */
		if (((igModID_Ftphdl = tool_get_q_id("ftphdl")) == RC_NOT_FOUND) ||
								((igModID_Ftphdl = tool_get_q_id("ftphdl")) == RC_FAIL)  ||
								(strcmp(rgTM.prHandleCmd[ipCmdNo].pcftp_mode,"SYSTEM")== 0))	{
		/********EO changes **************/

			/* send file to machine */
			if (ilRC)
			{
				if ((fh = fopen(sDEFAULT_CTRLFILE, "w")) == NULL)
				{
					dbg(DEBUG,"<SFtM> cannot open control file %s", sDEFAULT_CTRLFILE);
					return iOPEN_CTL_FAIL;
				}
				else
				{
					/* write user */
					fprintf(fh, "user %s %s\n",
						rgTM.prHandleCmd[ipCmdNo].pcFtpUser,
						rgTM.prHandleCmd[ipCmdNo].pcFtpPass);

					/* change directory */
					fprintf(fh, "cd %s\n", rgTM.prHandleCmd[ipCmdNo].pcMachinePath);

					/* write it */
					fprintf(fh, "put %s %s\n",
										rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath,
										rgTM.prHandleCmd[ipCmdNo].pcFileName);

					/* close & quit */
					fprintf(fh,"close\n");
					fprintf(fh,"quit\n");

					/* close file */
					fclose(fh);
				}

				/* clear memory from logfile... */
				memset((void*)pclTmpFile, 0x00, iMIN_BUF_SIZE);
				if ((pclTmpPath = getenv("TMP_PATH")) == NULL)
				{
					dbg(TRACE,"<SFtM> missing $TMP_PATH...");
					strcpy(pclTmpFile, "./fileif_ftp.log");
				}
				else
				{
					sprintf(pclTmpFile, "%s/%s", pclTmpPath, "fileif_ftp.log");
				}

				/* second first clear memory */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);

				/* build command */
				sprintf(pclTmpBuf, "ftp -n %s < %s 2>&1 > %s", rgTM.prHandleCmd[ipCmdNo].pcMachine, sDEFAULT_CTRLFILE, pclTmpFile);

				dbg(DEBUG,"<SFtM> System-String is: <%s>", pclTmpBuf);

				dbg(DEBUG,"<SFtM> try to create child process... ");

				/* fork and check return code... */
				switch (ilChildPid = fork())
				{
					case 0:
						errno = 0;
						ilRC = execlp(rgTM.prHandleCmd[ipCmdNo].pcShell,
										  rgTM.prHandleCmd[ipCmdNo].pcShell,
										  pclTmpBuf, NULL);
						exit(0);
						break;

					case -1:
						dbg(TRACE,"<SFtM> fork returns -1...");
						Terminate(0);
						break;

					default:
						dbg(DEBUG,"<SFtM> Child PID: %d", ilChildPid);
						break;
				}

				/* wait for ftp-end */
				igSignalNo = 0;
				alarm(90);
				wait(&ilStatus);
				alarm(0);

				/* check child process... */
				if (igSignalNo == SIGALRM)
				{
					dbg(DEBUG,"<SFtM> killing my child: %d", ilChildPid);
					if ((ilRC = kill(ilChildPid, SIGKILL)) != RC_SUCCESS)
					{
						/* remove control file */
						if ((ilRC = remove(sDEFAULT_CTRLFILE)) != 0)
						{
							dbg(TRACE,"<SFtM> cannot remove control file <%s> (%d)", sDEFAULT_CTRLFILE,ilRC);
						}

						dbg(TRACE,"<SFtM> kill returns %d <%s>", ilRC, strerror(errno));
						Terminate(0);
					}
					else
					{
						dbg(DEBUG,"<SFtM> successfull killing, i'm a murderer...");
					}
				}

				/* remove control file */
				if ((ilRC = remove(sDEFAULT_CTRLFILE)) != 0)
				{
					dbg(TRACE,"<SFtM> cannot remove control file <%s> (%d)", sDEFAULT_CTRLFILE,ilRC);
				}
			} /*il (ilRC)*/
 	                else
        	        {
                	                dbg(DEBUG,"<SFtM> file is equal...");
                        }

			/*Changes by mos 28 June 2000 for FTPHDL implementation *****/
			} /* if ((ilRC = tool_get_q_id(ftphdl)  == 0)*/
			else
			{
				/******* Changes by MOS 01 Nov 1999 for FTPHDL implementation *****/

				/* Save numtabkey value */
				memset(pclNumtabkey,0x00,iMIN_BUF_SIZE);
				strcpy(pclNumtabkey,rgTM.prHandleCmd[ipCmdNo].pcNumtab_Key);

				/*****************************************************************************************/
				/* Filling of the FTPConfig-structure for the dynamic configuration of the ftphdl-process*/
				/*****************************************************************************************/
				memset(&prgFtp,0x00,sizeof(FTPConfig));

				prgFtp.iFtpRC 			= FTP_SUCCESS; /* Returncode of FTP-transmission */
				strcpy(prgFtp.pcCmd,"FTP");
				strcpy(prgFtp.pcHostName,rgTM.prHandleCmd[ipCmdNo].pcMachine);
				strcpy(prgFtp.pcUser,rgTM.prHandleCmd[ipCmdNo].pcFtpUser);
				pclpasswd = rgTM.prHandleCmd[ipCmdNo].pcFtpPass;
				CEncode(&pclpasswd);
				strcpy(prgFtp.pcPasswd,pclpasswd);
				prgFtp.cTransferType 		= CEDA_ASCII;
				prgFtp.iTimeout 		= atoi(rgTM.prHandleCmd[ipCmdNo].pcftp_timeout); /* for tcp_waittimeout */
				prgFtp.lGetFileTimer 		= 100000; /* timeout (microseconds) for receiving data */
				prgFtp.lReceiveTimer 		= 100000; /* timeout (microseconds) for receiving data */
				prgFtp.iDebugLevel 		= DEBUG; /* Debugging level of FTPHDL during operation */
				/*set default os first, if not specified*/
				prgFtp.iClientOS        = OS_UNIX;
			 	if (strcmp(rgTM.prHandleCmd[ipCmdNo].pcFtp_Client_OS,"WIN") == 0)
				{
					prgFtp.iClientOS 	= OS_WIN; /* Operating system of client */
				}
				if (strcmp(rgTM.prHandleCmd[ipCmdNo].pcFtp_Client_OS,"UNIX") == 0)
				{
					prgFtp.iClientOS 	= OS_UNIX; /* Operating system of client */
				}
				prgFtp.iServerOS 		= OS_UNIX; /* Operating system of server */
				prgFtp.iRetryCounter 		= 0; /* number of retries */
				prgFtp.iDeleteRemoteSourceFile 	= 0; /* yes=1 no=0 */
				prgFtp.iDeleteLocalSourceFile 	= 1; /* yes=1 no=0 */
				prgFtp.iSectionType 		= iSEND; /* iSEND or iRECEIVE */
				prgFtp.iInvalidOffset 		= 0; /* time (min.) the section will be set invalid, after unsuccessfull retry */
				prgFtp.iTryAgainOffset 		= 0; /* time between retries */
				strcpy(prgFtp.pcHomeAirport,rgTM.prHandleCmd[ipCmdNo].pcHomeAirport); /* HomeAirport */
				strcpy(prgFtp.pcTableExtension,rgTM.prHandleCmd[ipCmdNo].pcTableExtension); /* Table extension */
				strcpy(prgFtp.pcLocalFilePath,rgTM.prHandleCmd[ipCmdNo].pcFilePath);
				strcpy(prgFtp.pcLocalFileName,rgTM.prHandleCmd[ipCmdNo].pcFileName);
				strcpy(prgFtp.pcRemoteFilePath,rgTM.prHandleCmd[ipCmdNo].pcMachinePath);
				/*if destination file name is not specified, use local file name**/
				if (strlen(rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName)==0) {
					strcpy(rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,rgTM.prHandleCmd[ipCmdNo].pcFileName);
				}

				/* if numtab_key is specified, then create extension, if not use configured one */
				if (strlen(rgTM.prHandleCmd[ipCmdNo].pcNumtab_Key)> 1) {
					memset(pclNumtabvalue,0x00,iMIN_BUF_SIZE);
					GetNextNumbers(pclNumtabkey, pclNumtabvalue, 1,"NO_AUTO_INCREASE");
					GetDataItem(pclNumtabacnu, pclNumtabvalue, 1, cCOMMA, "", " \0");
					if (strlen(pclNumtabacnu)<=0) {
					dbg(TRACE,"SENDFILETOMACHINE pclNumtabacnu <%s> with strlen<%d> and stopval <%s>",pclNumtabacnu,strlen(pclNumtabacnu),rgTM.prHandleCmd[ipCmdNo].pcNumtab_Stopvalue);
						sprintf(pclNumtabacnu,"%s",rgTM.prHandleCmd[ipCmdNo].pcNumtab_Stopvalue);
					}
					sprintf(prgFtp.pcRemoteFileName,"%s%04d",rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,atoi(pclNumtabacnu));
					if (strlen(rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile)> 1) {

						sprintf(prgFtp.pcRenameRemoteFile,"%s%04d",rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile,atoi(pclNumtabacnu));
						if (strlen(prgFtp.pcHostName) > 1) {
							strcpy(prgFtp.pcRemoteFileName,prgFtp.pcLocalFileName);
						} else {
							sprintf(prgFtp.pcRemoteFileName,"%s%04d",rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,atoi(pclNumtabacnu));
						}
					} else {
						sprintf(prgFtp.pcRemoteFileName,"%s%04d",rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,atoi(pclNumtabacnu));
					}
				}
				else
				{
					if (strlen(rgTM.prHandleCmd[ipCmdNo].pcRemoteFile_AppendExtension)>0) {
						sprintf(prgFtp.pcRemoteFileName,"%s%s",rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,rgTM.prHandleCmd[ipCmdNo].pcRemoteFile_AppendExtension);
						if (strlen(rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile)> 1) {
							sprintf(prgFtp.pcRenameRemoteFile,"%s%s",rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName,rgTM.prHandleCmd[ipCmdNo].pcRemoteFile_AppendExtension);
						}
					} else {
						strcpy(prgFtp.pcRemoteFileName,rgTM.prHandleCmd[ipCmdNo].pcFtpDestFileName);
						if (strlen(rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile)> 1) {
							strcpy(prgFtp.pcRenameRemoteFile,rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile);
						}
					}
				}

				if (strlen(rgTM.prHandleCmd[ipCmdNo].pcFtpRenameDestFile)> 1) {
					dbg(TRACE,"SENDFILETOMACHINE filename <%s> from command <%s> to mod-id <%d>",prgFtp.pcRenameRemoteFile,rgTM.prHandleCmd[ipCmdNo].pcCommand,igModID_Ftphdl);
				} else {
					dbg(TRACE,"SENDFILETOMACHINE filename <%s> from command <%s> to mod-id <%d>",prgFtp.pcRemoteFileName,rgTM.prHandleCmd[ipCmdNo].pcCommand,igModID_Ftphdl);
				}
				prgFtp.iSendAnswer 		= 1; /* yes=1 no=0 */
				prgFtp.cStructureCode 		= CEDA_FILE;
				prgFtp.cTransferMode 		= CEDA_STREAM;
				prgFtp.iStoreType 		= CEDA_CREATE;
				prgFtp.data[0] 			= 0x00;
			if ((ilRC = SendEvent("FTP"," "," ","DYN",(char*)&prgFtp,
					sizeof(FTPConfig),igModID_Ftphdl, PRIORITY_3,"FIDS",pclNumtabkey,pclNumtabacnu,rgTM.prHandleCmd[ipCmdNo].pcNumtab_Stopvalue,ipCmdNo))	!= RC_SUCCESS)
				{
				dbg(TRACE,"Fileif: SendFileToMachine() to <%d> returns <%d>!",igModID_Ftphdl,ilRC);
				}

			/****************EO changes*****************/
			} /* if ((ilRC = tool_get_q_id(ftphdl)  == 8750)*/
		}
		/* for JWE: send an event to defined process...
		so the file is always closed here!
		*/
		/* should i send command after creating file? */
		if (rgTM.prHandleCmd[ipCmdNo].iUseSEC)
		{
			/* build fieldlist for sending */
			memset((void*)pclFieldBuf, 0x00, iMAXIMUM);
			for (i=0; i<rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoFields; i++)
			{
				strcat(pclFieldBuf, rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].pcFields[i]);
				if (i < rgTM.prHandleCmd[ipCmdNo].prTabDef[ipTabNo].iNoFields-1)
					strcat(pclFieldBuf, ",");
			}
			/* must i send file to que? */
			if (rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iSendFileToQue)
			{
				if ((ilRC = GetFileContents(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath, &lFileLength, (char**)&pclFileContents)) != RC_SUCCESS)
				{
					dbg(TRACE,"<SFtM> GetFileContents returns: %d",ilRC);
					lFileLength = 0;
					pclFileContents = NULL;
				}

				/* calculate length of event */
				ilLen = sizeof(EVENT) + sizeof(BC_HEAD) +
							  sizeof(CMDBLK) + strlen(pclFieldBuf) + lFileLength + 5;
			}
			else
			{
				/* length of OutEvent... */
				ilLen = sizeof(EVENT)       + sizeof(BC_HEAD) +
						sizeof(CMDBLK)      + strlen(rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath) +
						strlen(pclFieldBuf) + 5;
			}

			/* memory for it */
			if ((prlOutEvent = (EVENT*)calloc(1,(size_t)ilLen)) == NULL)
			{
				dbg(TRACE,"<SFtM> cannot alloc %d bytes for OutEvent", ilLen);
				prlOutEvent = NULL;
				Terminate(0);
			}
			else
			{
				/* clear whole memory */
				memset((void*)prlOutEvent, 0x00, ilLen);

				/* set event structure... */
				prlOutEvent->type 		= SYS_EVENT;
				prlOutEvent->command 		= EVENT_DATA;
				prlOutEvent->originator 	= mod_id;
				prlOutEvent->retry_count	= 0;
				prlOutEvent->data_offset 	= sizeof(EVENT);
				prlOutEvent->data_length 	= ilLen - sizeof(EVENT);

				/* BC_HEAD-Structure... */
				prlOutBCHead = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
				prlOutBCHead->rc = RC_SUCCESS;
				strcpy(prlOutBCHead->dest_name, "FILEIF");
				strcpy(prlOutBCHead->recv_name, "EXCO");

				/* Cmdblk-Structure... */
				prlOutCmdblk = (CMDBLK*)((char*)prlOutBCHead->data);
				strcpy(prlOutCmdblk->command,
					 rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.pcCmd);
				prlOutCmdblk->obj_name[0] = 0x00;

				if (rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iAfcKeepOrgTwStart==1)
				{
					sprintf(prlOutCmdblk->tw_start, "%s",rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.pcOrgTwStart);
				}
				else
				{
					prlOutCmdblk->tw_start[0] = 0x00;
				}

				sprintf(prlOutCmdblk->tw_end, "%s,%s,FILEIF,TRUE", rgTM.prHandleCmd[ipCmdNo].pcHomeAirport, rgTM.prHandleCmd[ipCmdNo].pcTableExtension);



				/* selection */
				pclOutSelection = (char*)prlOutCmdblk->data;

				if(rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iAfcSendFilename == 1)
				{
					strcpy(pclOutSelection,rgTM.prHandleCmd[ipCmdNo].pcFileNameWithPath);
				}

				/* fields */
				pclOutFields = (char*)prlOutCmdblk->data +
												strlen(pclOutSelection) + 1;
				strcpy(pclOutFields, pclFieldBuf);

				/* data */
				pclOutData = (char*)prlOutCmdblk->data +
									strlen(pclOutSelection) + strlen(pclOutFields) + 2;

				/* copy to pointer */
				if (rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iSendFileToQue &&
					 pclFileContents != NULL)
				{
					memcpy((void*)pclOutData, (const void*)pclFileContents, (size_t)lFileLength);
					free((void*)pclFileContents);
					pclFileContents = NULL;
					lFileLength = 0;
				}


				/* write to Que */
				if (rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iModID > 0) {
					dbg(TRACE,"<SFtM> write event to QUE: %d for command <%s>", rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iModID,rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.pcCmd);
					if ((ilRC = que(QUE_PUT, rgTM.prHandleCmd[ipCmdNo].rSendEventCmds.iModID, mod_id, PRIORITY_3, ilLen, (char*)prlOutEvent)) != RC_SUCCESS)
					{
						dbg(TRACE,"<SFtM> QUE_PUT returns: %d", ilRC);
						Terminate(0);
					}
				}
				/* free Out-Event memory... */
				if (prlOutEvent != NULL)
				{
					free((void*)prlOutEvent);
					prlOutEvent = NULL;
				}
			}
		}/* should i send command after creating file? */
	}/* first check file, exist it? */

	/* bye bye */
	return RC_SUCCESS;
}


/* **********************************************************************/
/* The SendEvent routine                                         	*/
/* prepares an internal CEDA-event                                  	*/
/*									*/
/* CHANGE HISTORY							*/
/*		implmented 11/01/99 for the ftphdl call			*/
/*									*/
/* **********************************************************************/
static int SendEvent(char *pcpCmd,char *pcpSelection,char *pcpFields, char *pcpData,
		char *pcpAddStruct, int ipAddLen,int ipModID,int ipPriority,
		char *pcpTable,char *pcpType,char *pcpFile, char *pcptw_startvalue, int ipCmdNo)
{
	int     ilRC        	= RC_FAIL;
	int     ilLen        	= 0;
  	EVENT   *prlOutEvent    = NULL;
  	BC_HEAD *prlOutBCHead   = NULL;
  	CMDBLK  *prlOutCmdblk   = NULL;

	/* size-calculation for prlOutEvent */
	ilLen = sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) +
					strlen(pcpSelection) + strlen(pcpFields) + strlen(pcpData) + ipAddLen + 10;

	/* memory for prlOutEvent */
	if ((prlOutEvent = (EVENT*)calloc(1,(size_t)ilLen)) == NULL)
	{
		dbg(TRACE,"SendEvent: cannot malloc <%d>-bytes for outgoing event!",ilLen);
		prlOutEvent = NULL;
	}
	else
	{
		/* clear whole outgoing event */
		memset((void*)prlOutEvent, 0x00, ilLen);

		/* set event structure... */
		prlOutEvent->type		= SYS_EVENT;
		prlOutEvent->command   		= EVENT_DATA;
		prlOutEvent->originator		= (short)mod_id;
		prlOutEvent->retry_count	= 0;
		prlOutEvent->data_offset	= sizeof(EVENT);
		prlOutEvent->data_length	= ilLen - sizeof(EVENT);

		/* BC_HEAD-Structure... */
		prlOutBCHead 			= (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
		prlOutBCHead->rc 		= (short)RC_SUCCESS;
		strncpy(prlOutBCHead->dest_name,pcgProcessName,10);
		strncpy(prlOutBCHead->recv_name, "EXCO",10);

		/* Cmdblk-Structure... */
		prlOutCmdblk 			= (CMDBLK*)((char*)prlOutBCHead->data);
		strcpy(prlOutCmdblk->command,pcpCmd);
		strcpy(prlOutCmdblk->obj_name,pcpTable);
		strcat(prlOutCmdblk->obj_name,rgTM.prHandleCmd[ipCmdNo].pcTableExtension);

		/* setting tw_start entries */
		sprintf(prlOutCmdblk->tw_start,"%s,%s,%s",pcpType,pcpFile,pcptw_startvalue);

		/* setting tw_end entries */
		sprintf(prlOutCmdblk->tw_end,"%s,%s,%s,TRUE",rgTM.prHandleCmd[ipCmdNo].pcHomeAirport,rgTM.prHandleCmd[ipCmdNo].pcTableExtension,pcgProcessName);

		/* setting selection inside event */
		strcpy(prlOutCmdblk->data,pcpSelection);

		/* setting field-list inside event */
		strcpy(prlOutCmdblk->data+strlen(pcpSelection)+1,pcpFields);

		/* setting data-list inside event */
		strcpy((prlOutCmdblk->data +
					(strlen(pcpSelection)+1) +
					(strlen(pcpFields)+1)),pcpData);

		if (pcpAddStruct != NULL)
		{
			memcpy((prlOutCmdblk->data +
						(strlen(pcpSelection)+1) +
						(strlen(pcpFields)+1)) +
						(strlen(pcpData)+1),pcpAddStruct,ipAddLen);
		}

		/*DebugPrintEvent(DEBUG,prlOutEvent);*/
		/*snapit((char*)prlOutEvent,ilLen,outp);*/

		dbg(DEBUG,"SendEvent: <%d> --> <%d>",mod_id,ipModID);

		if ((ilRC = que(QUE_PUT,ipModID,mod_id,ipPriority,ilLen,(char*)prlOutEvent))
			!= RC_SUCCESS)
		{
			dbg(TRACE,"SendEvent: QUE_PUT to <%d> returns: <%d>",ipModID,ilRC);
		}
		/* free memory */
	  free((void*)prlOutEvent);
	}
	return ilRC;
}

/******************************************************************************/
/* The Write end of block routine                                             */
/******************************************************************************/
static int WriteEOB(int ipCmdNo)
{
	char			pclTmpBuf[iMIN_BUF_SIZE];

	/* first check file, exist it? */
	if (rgTM.prHandleCmd[ipCmdNo].iOpenDataFile)
	{
		/* write end of block to file, if necessary... */
		if (rgTM.prHandleCmd[ipCmdNo].iUseEOB)
		{
			/* clear temporary buffer */
			memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);

			/* shoud i write blocknumber? */
			if (rgTM.prHandleCmd[ipCmdNo].iEndOfBlockNumber)
			{
				/* copy with blocknuber... */
				sprintf(pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcEndOfBlock, rgTM.prHandleCmd[ipCmdNo].iDataFromTable+1);
			}
			else
			{
				/* copy without blocknuber... */
				sprintf(pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcEndOfBlock);
			}

			/* write to file */
			fprintf(rgTM.prHandleCmd[ipCmdNo].pFh, "%s%s", pclTmpBuf, rgTM.prHandleCmd[ipCmdNo].pcEndOfLine);
		}
	}

	/* bye bye */
	return RC_SUCCESS;
}

/******************************************************************************/
/* The GetCmdNumber routine                                                   */
/******************************************************************************/
static int GetCmdNumber(char *pcpCmd)
{
	int		i;

	/* search command... */
	for (i=0; i<rgTM.iNoCommands; i++)
		if (!strcmp(rgTM.prHandleCmd[i].pcCommand, pcpCmd))
		{
			return i;
		}

	/* bye bye */
	return RC_FAIL;
}

/******************************************************************************/
/* The GetFileContents routine                                                */
/* Eigentlich eine Libraray-Funktion (tool_fn_to_buf), aber auf London ist    */
/* diese Fkt. anders implementiert (zwei anstelle von drei Parametern). Um    */
/* Portierungen zu vereinfachen und Fehler zu vermeiden wurde die Fkt. herei- */
/* genommen!!!																						*/
/******************************************************************************/
static int GetFileContents(char *Ppath, long *Psize, char **PPret)
{
  int rc = RC_SUCCESS;
  FILE *fp=NULL;
  struct stat statbuf;
  char *Pbuffer=NULL;

  rc = stat (Ppath, &statbuf);
  if (rc == -1)
  {
    dbg(TRACE,"GetFileContents: stat for %s failed: %s", Ppath,
	    strerror(errno));

    rc = RC_FAIL;
  }

  if (rc == RC_SUCCESS)
  {
    *Psize = statbuf.st_size;
    Pbuffer = calloc ((size_t)1, (size_t)((*Psize)+1));
    if (Pbuffer == NULL)
    {
      dbg(TRACE,"GetFileContents: calloc failed: %s:", strerror(errno));

      rc = RC_FAIL;
    }
  }

  if (rc == RC_SUCCESS)
  {
    fp = fopen (Ppath, "r");
    if (fp == NULL)
    {
      dbg(TRACE,"GetFileContents: open failed: %s:", strerror(errno));
      rc = RC_FAIL;
    }
  }

  if (rc == RC_SUCCESS)
  {
    rc = fread ((void*)Pbuffer, (size_t)1, (size_t)(*Psize), fp);
    if (rc == -1)
    {
      dbg(TRACE,"GetFileContents: read failed: %s:", strerror(errno));
      rc = RC_FAIL;
    } /* fi */
    else if (rc < *Psize)
    {
      dbg(TRACE,"GetFileContents: read %d bytes instead of %ld ",
	      rc, *Psize);
      rc = RC_FAIL;
    }
    else
    {
      rc = RC_SUCCESS;
    }
  }

  if (fp != NULL)
    fclose (fp);

  if (rc == RC_SUCCESS)
  {
    *PPret = Pbuffer;
  }
  else
  {
    *PPret = NULL;
	 *Psize = 0;
    dbg(TRACE,"GetFileContents: read %d bytes & returns %ld",*Psize,rc);
    free((void*)Pbuffer);
  }

  return rc;
}


/* ******************************************************************** */
/* The UtcToLocal(char *pcpTime) routine        */
/* ******************************************************************** */
static int UtcToLocal(char *pcpTime)
{
	int c;
		char year[5], month[3], day[3], hour[3], minute[3],second[3];
		struct tm TimeBuffer, *final_result;
		time_t time_result;

		/********** Extract the Year off CEDA timestamp **********/
		for(c=0; c<= 3; ++c)
		{
				year[c] = pcpTime[c];
		}
		year[4] = '\0';
		/********** Extract month, day, hour and minute off CEDA timestamp **********/
		for(c=0; c <= 1; ++c)
		{
			month[c]  = pcpTime[c + 4];
			day[c]    = pcpTime[c + 6];
			hour[c]   = pcpTime[c + 8];
			minute[c] = pcpTime[c + 10];
			second[c] = pcpTime[c + 12];
		}
		/********** Terminate the Buffer strings **********/
		month[2]  = '\0';
		day[2]    = '\0';
		hour[2]   = '\0';
		minute[2] = '\0';
		second[2] = '\0';


		/***** Fill a broken-down time structure incl. string to integer *****/
		TimeBuffer.tm_year  = atoi(year) - 1900;
		TimeBuffer.tm_mon   = atoi(month) - 1;
		TimeBuffer.tm_mday  = atoi(day);
		TimeBuffer.tm_hour  = atoi(hour);
		TimeBuffer.tm_min   = atoi(minute);
		TimeBuffer.tm_sec   = atoi(second);
		TimeBuffer.tm_isdst = 0;
		/***** Make secondbased timeformat and correct mktime *****/
		time_result = mktime(&TimeBuffer) - timezone;
		/***** Reconvert into broken-down time structure *****/
		final_result = localtime(&time_result);

		sprintf(pcpTime,"%d%.2d%.2d%.2d%.2d%.2d"
			,final_result->tm_year+1900
			,final_result->tm_mon+1
			,final_result->tm_mday
			,final_result->tm_hour
			,final_result->tm_min
			,final_result->tm_sec);

		return(0); /**** DONE WELL ****/
}

/******************************************************************************/
/* The TrimRight routine                                                      */
/******************************************************************************/
static void TrimRight(char *pcpBuffer)
{
        char *pclBlank = &pcpBuffer[strlen(pcpBuffer)-1];
        while(isspace(*pclBlank) && pclBlank != pcpBuffer)
        {
                *pclBlank = '\0';
                pclBlank--;
        }
}/* end of TrimRight*/
/******************************************************************************/
/* The ReplaceStdTokens                                                      */
/* replaces configured keywords inside a filename,header,prefix,postfix or footer line*/
/* with dynamically calculated values like linecount,numbers,dates,times,etc.*/
/******************************************************************************/
static int ReplaceStdTokens(int ipCmd,char *pcpLine,char *pcpDynData,char *pcpDateFormat,char *pcpTimeFormat,int ipLineNumber)
{
	int ilRc = RC_SUCCESS;
	int ilBreak = FALSE;
	int ilBytes = 0;
	char pclToken[iMAX_BUF_SIZE];
	char pclDynData[iMAX_BUF_SIZE];
	char pclTmpBuf[iMAX_BUF_SIZE];
	char pclTmpBuf2[iMAX_BUF_SIZE];

	do
	{
		memset(pclDynData,0x00,iMAX_BUF_SIZE);
		memset(pclToken,0x00,iMAX_BUF_SIZE);
		memset(pclTmpBuf,0x00,iMAX_BUF_SIZE);
		memset(pclTmpBuf2,0x00,iMAX_BUF_SIZE);

		if (strstr(pcpLine,"#FILENAME")!=NULL && pcpDynData!=NULL)
		{
				strncpy(pclDynData,pcpDynData,iMAX_BUF_SIZE);
				strcpy(pclToken,"#FILENAME");
		}
		else if (strstr(pcpLine,"#DATE_LOC")!=NULL && pcpDateFormat!=NULL)
		{
			strcpy(pclToken,"#DATE_LOC");
			GetServerTimeStamp("LOC",1,0,pclTmpBuf);
			FormatDate(pclTmpBuf,pcpDateFormat,pclTmpBuf2);
			strcpy(pclDynData,pclTmpBuf2);
		}
		else if (strstr(pcpLine,"#TIME_LOC")!=NULL && pcpTimeFormat!=NULL)
		{
			strcpy(pclToken,"#TIME_LOC");
			GetServerTimeStamp("LOC",1,0,pclTmpBuf);
			FormatDate(pclTmpBuf,pcpTimeFormat,pclTmpBuf2);
			strcpy(pclDynData,pclTmpBuf2);
		}
		else if (strstr(pcpLine,"#DATE_UTC")!=NULL && pcpDateFormat!=NULL)
		{
			strcpy(pclToken,"#DATE_UTC");
			GetServerTimeStamp("UTC",1,0,pclTmpBuf);
			FormatDate(pclTmpBuf,pcpDateFormat,pclTmpBuf2);
			strcpy(pclDynData,pclTmpBuf2);
		}
		else if (strstr(pcpLine,"#TIME_UTC")!=NULL && pcpTimeFormat != NULL)
		{
			strcpy(pclToken,"#TIME_UTC");
			GetServerTimeStamp("UTC",1,0,pclTmpBuf);
			FormatDate(pclTmpBuf,pcpTimeFormat,pclTmpBuf2);
			strcpy(pclDynData,pclTmpBuf2);
		}
		else if (strstr(pcpLine,"#HEADERLINENUMBER")!=NULL)
		{
			strcpy(pclToken,"#HEADERLINENUMBER");
			sprintf(pclTmpBuf2,"%010d",1);
			ilBytes = rgTM.prHandleCmd[ipCmd].iStdLineNrLen;
			strncpy(pclDynData,(char*)&pclTmpBuf2[strlen(pclTmpBuf2)-ilBytes],ilBytes);
		}
		else if (strstr(pcpLine,"#DATALINENUMBER")!=NULL && ipLineNumber>=0)
		{
			strcpy(pclToken,"#DATALINENUMBER");
			sprintf(pclTmpBuf2,"%010d",ipLineNumber);
			ilBytes = rgTM.prHandleCmd[ipCmd].iStdLineNrLen;
			strncpy(pclDynData,(char*)&pclTmpBuf2[strlen(pclTmpBuf2)-ilBytes],ilBytes);
		}
		else if (strstr(pcpLine,"#FOOTERLINENUMBER")!=NULL)
		{
			strcpy(pclToken,"#FOOTERLINENUMBER");
			if (rgTM.prHandleCmd[ipCmd].iUseSOF == 1)
				sprintf(pclTmpBuf2,"%010d",igTotalLineCount+2);
			else
				sprintf(pclTmpBuf2,"%010d",igTotalLineCount+1);
			ilBytes = rgTM.prHandleCmd[ipCmd].iStdLineNrLen;
			strncpy(pclDynData,(char*)&pclTmpBuf2[strlen(pclTmpBuf2)-ilBytes],ilBytes);
		}
		else if (strstr(pcpLine,"#DATALINECOUNT")!=NULL)
		{
			strcpy(pclToken,"#DATALINECOUNT");
			sprintf(pclDynData,"%010d",igTotalLineCount);
		}

		if (strlen(pclToken)>0 && strlen(pclDynData)>0)
		{
			dbg(DEBUG,"<RSTDT> replace <%s> with <%s>",pclToken,pclDynData);
			while((ilRc = SearchStringAndReplace(pcpLine,pclToken,pclDynData)) != RC_SUCCESS)
			{
				ilRc = RC_FAIL;
				ilBreak=TRUE;
			}
		}
		else
		{
			dbg(DEBUG,"<RSTDT> either the token or the dynamic data are empty! Exit func.!");
			ilBreak=TRUE;
		}
	}while (strlen(pclToken)>0 && ilBreak==FALSE);
}
/* ******************************************************************** */
/* The FormatDate() routine                                         */
/* ******************************************************************** */
static int FormatDate(char *pcpOldDate,char *pcpFormat,char *pcpNewDate)
{
	int ilRc = RC_SUCCESS;
	int ilCfgChar = 0;
	int ilRightChar = 0;
	int ilDateChar = 0;
	char pclOldDate[20];
	char pclValidLetters[] = "YMDHIS";
	char pclRightFormat[20];
	char *pclTmpDate = NULL;

	TrimRight(pcpOldDate);
	memset(pclOldDate,0x00,sizeof(pclOldDate));
	memset(pclRightFormat,0x00,sizeof(pclRightFormat));
	*pcpNewDate = 0x00;

	if (strlen(pcpOldDate) > 14)
	{
		return RC_FAIL;
	}else
	{
		strcpy(pclOldDate,pcpOldDate);
		TrimRight(pclOldDate);
	}

	/* adding '0' until the length of the date is 14byte */
	if (strlen(pclOldDate) < 14)
	{
		while(strlen(pclOldDate) < 14)
		{
			pclOldDate[strlen(pclOldDate)] = '0';
			pclOldDate[strlen(pclOldDate)+1] = 0x00;
		}
	}

	/* removing all unallowed letters from pcpFormat-string */
	ilCfgChar=0;
	while(ilCfgChar < (int)strlen(pcpFormat))
	{
		if (strchr(pclValidLetters,pcpFormat[ilCfgChar]) != NULL)
		{
			pclRightFormat[ilRightChar] = pcpFormat[ilCfgChar];
			ilRightChar++;
		}
		ilCfgChar++;
	}

	/* now formatting CEDA-time format from pclOldDate to right format */
	if ((pclTmpDate =
			GetPartOfTimeStamp(pclOldDate,pclRightFormat)) != NULL)
	{
		/* now changing the layout like it is in the cfg-file */
		ilCfgChar = 0;
		ilRightChar = 0;
		ilDateChar = 0;
		while(ilCfgChar < (int)strlen(pcpFormat))
		{
			if (strchr(pclValidLetters,pcpFormat[ilCfgChar]) != NULL)
			{
				(pcpNewDate)[ilDateChar] = pclTmpDate[ilRightChar];
				ilRightChar++;
				ilDateChar++;
			}
			else
			{
				(pcpNewDate)[ilDateChar] = pcpFormat[ilCfgChar];
				ilDateChar++;
			}
			ilCfgChar++;
		}
		(pcpNewDate)[strlen(pcpFormat)] = 0x00;
	}
	else
	{
		ilRc = RC_FAIL;
	}
	/* dbg(DEBUG,"FormatDate: Old=<%s> New=<%s>",pclOldDate,pcpNewDate);*/
	return ilRc;
}
/* ******************************************************************** */
/* The GetLogFileMode() routine                                         */
/* ******************************************************************** */
static void GetLogFileMode(int *ipModeLevel, char *pcpLine, char *pcpDefault)
{
	int ilRc = 0;
  char pclTmp[32];

	memset(pclTmp,0x00,32);
	ilRc = iGetConfigEntry(pcgCfgFile, "MAIN", pcpLine,CFG_STRING, pclTmp);
	dbg(TRACE,"<GLFM>: <%s>=<%s>",pcpLine,pclTmp);

  if (strcmp(pclTmp,"TRACE")==0)
  {
     *ipModeLevel = TRACE;
  }
  else if(strcmp(pclTmp,"DEBUG")==0)
  {
     *ipModeLevel = DEBUG;
  } /* end else if */
  else
  {
     *ipModeLevel = 0;
  }
} /* end GetLogFileMode */
static void GetSeqNumber(char *pcpSeq,char *pcpKey,char *pcpSeqLen,char *pcpResetDay,char* pcpStartValue)
{
	int ilRC = 0;
	char pclSqlBuf[iMIN_BUF_SIZE];
	char pclDataArea[iMIN_BUF_SIZE];
	char pclOraErrorMsg[iMAXIMUM];
	char pclTmpBuf[iMIN_BUF_SIZE];
	char pclDateTimeStamp[iMIN_BUF_SIZE];
	char pclMonthOfReset[iMIN_BUF_SIZE];
	char pclDay[iMIN_BUF_SIZE];
	char pclTmpBuf2[iMIN_BUF_SIZE];
	short slSqlFunc=0,slCursor=0;

	memset(pclTmpBuf,0x00,iMIN_BUF_SIZE);
	memset(pclDateTimeStamp,0x00,iMIN_BUF_SIZE);
	memset(pclMonthOfReset,0x00,iMIN_BUF_SIZE);
	memset(pclSqlBuf,0x00,iMIN_BUF_SIZE);
	memset(pclDataArea,0x00,iMIN_BUF_SIZE);
	memset(pclOraErrorMsg,0x00,iMAXIMUM);

	sprintf(pclSqlBuf,"SELECT ACNU,FLAG FROM NUMTAB WHERE KEYS = '%s'",pcpKey);
	slSqlFunc = START;

	dbg(DEBUG,"<GetSeqNumber> SQL: <%s>",pclSqlBuf);
	GetServerTimeStamp("LOC",1,0,pclDateTimeStamp);

	if ((ilRC = sql_if(slSqlFunc,&slCursor,pclSqlBuf,pclDataArea))== RC_SUCCESS)
	{
			commit_work();
			close_my_cursor(&slCursor);
			/*snap(pclDataArea,20,outp);*/
			GetDataItem(pclTmpBuf,pclDataArea,1,0x00,"","");
			GetDataItem(pclMonthOfReset,pclDataArea,2,0x00,"","");
			TrimRight(pclMonthOfReset);
			dbg(DEBUG,"<GetSeqNumber> ACNU=<%s>, MONTH OF LAST RESET=<%s>",pclTmpBuf,pclMonthOfReset);

			if (strlen(pcpResetDay)>0 &&
				strncmp((char*)&pclDateTimeStamp[6],pcpResetDay,2)==0 &&
				strncmp((char*)&pclDateTimeStamp[4],pclMonthOfReset,2)!=0)
			{
				pclMonthOfReset[0]=0x00;
				strncpy(pclMonthOfReset,(char*)&pclDateTimeStamp[4],2);
				strcpy(pclTmpBuf,pcpStartValue);
				dbg(TRACE,"<GetSeqNumber> RESET SEQ.-NUMBER TO <%s>!",pclTmpBuf);
				sprintf(pclSqlBuf,"UPDATE NUMTAB SET ACNU=%s,FLAG='%s' WHERE KEYS = '%s'",pcpStartValue,pclMonthOfReset,pcpKey);
			}
			else
			{
				sprintf(pclSqlBuf,"UPDATE NUMTAB SET ACNU=ACNU+1 WHERE KEYS = '%s'",pcpKey);
			}

			sprintf(pclTmpBuf2,"0000000000");
			strncpy((char*)&pclTmpBuf2[strlen(pclTmpBuf2)-strlen(pclTmpBuf)],pclTmpBuf,strlen(pclTmpBuf));
			dbg(DEBUG,"<GetSeqNumber> SQL =<%s>",pclSqlBuf);
			slSqlFunc=START;
			slCursor=0;
			if ((ilRC = sql_if(slSqlFunc,&slCursor,pclSqlBuf,pclDataArea))!= RC_SUCCESS)
			{
				if (ilRC != NOTFOUND)
				{
					ilRC = RC_FAIL;
					get_ora_err(ilRC,pclOraErrorMsg);
					dbg(TRACE,"<GetSeqNumber> ORA-ERR <%s>=<%s>",pclOraErrorMsg, pclSqlBuf);
				}
			}
			else
			{
				strncpy(pcpSeq,(char*)&pclTmpBuf2[strlen(pclTmpBuf2)-atoi(pcpSeqLen)],atoi(pcpSeqLen));
				dbg(DEBUG,"<GetSeqNumber> SEQ =<%s>",pcpSeq);
			}
			commit_work();
			close_my_cursor(&slCursor);
	}
	else
	{
		if (ilRC != NOTFOUND)
		{
			ilRC = RC_FAIL;
			get_ora_err(ilRC,pclOraErrorMsg);
			dbg(TRACE,"<GetSeqNumber> ORA-ERR <%s>=<%s>",pclOraErrorMsg, pclSqlBuf);
		}
		commit_work();
		close_my_cursor(&slCursor);
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

