#ifndef _DEF_mks_version
  #define _DEF_mks_version
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version[] MARK_UNUSED = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Kernel/nflhdl.c 1.5a 2015/01/27 12:22:13SGT fya Exp  $";
#endif /* _DEF_mks_version */
/*****************************************************************************/
/*                                                                           */
/* CCS Program Skeleton                                                      */
/*                                                                           */
/* Author         :                                                          */
/* Date           :                                                          */
/* Description    :                                                          */
/*                                                                           */
/* Update history :  rkl 21.10.99 insert keyword dest_name, recv_name        */
/*                   rkl 16.05.99 read inital debug_level from configfile    */
/*                   rkl 08.06.00 sectionline in configfile from 128->256len */
/*                   rkl 31.08.0  Multiairport                               */
/*                   jim 10.08.04 removed sccs_ version string               */
/* 20150119 FYA: v1.5a UFIS-8303 HOPO implementation*/
/*****************************************************************************/
/* This program is a CCS main program                                        */
/*                                                                           */
/* source-code-control-system version string                                 */
/* be carefule with strftime or similar functions !!!                        */
/*****************************************************************************/

#define 	U_MAIN
#define 	UGCCS_PRG
#define 	STH_USE
#define	__SHOW_PASSWD

/* The master header file */
#include  "send.h"
#include "multiapt.h"
#include  "nflhdl.h"

/******************************************************************************/
/* External variables                                                         */
/******************************************************************************/
/* outp is defined in ugccsma.h! double definitions kill */
/* your process under HP-UX immediatly !!!!              */
/*FILE *outp       = NULL;*/
int  debug_level = TRACE;

/******************************************************************************/
/* Global variables                                                           */
/******************************************************************************/
static ITEM  *prgItem      = NULL;                /* The queue item pointer  */
static EVENT *prgEvent     = NULL;                /* The event pointer       */
static int	 igSigAlrm		= 0;						  /* SIG-ALRM flag */
static int	 igTstSigAlrm  = 0;						  /* Testing alarm flag? */
static int	 igProcessingDelay = 0;			    /* Testing alarm flag? */
static char	 pcgTABEnd[iMIN];

/*FYA v1.5a UFIS-8303*/
/*static char	 pcgHomeAP[iMIN];*/
static char	 pcgHomeAP[iMIN_BUF_SIZE];
static char	 pcgHomeAP_sgstab[iMIN_BUF_SIZE];
static char cgProcessName[iMIN] = "\0";
static char pcgCfgFile[32];

char pcgTwEnd[64];

/*FYA v1.26c UFIS-8297*/
static int igMultiHopo = FALSE;
static char cgCfgBuffer[1024];

/******************************************************************************/
/* my Global variables                                                        */
/******************************************************************************/
static TNFMain		rgTM;

/******************************************************************************/
/* External functions                                                         */
/******************************************************************************/
extern int  init_que(void);                      /* Attach to CCS queues */
extern int  que(int,int,int,int,int,char*);      /* CCS queuing routine  */
extern int  send_message(int,int,int,int,char*); /* CCS msg-handler i/f  */
extern int  SendRemoteShutdown(int);
extern int  nap(int);

/******************************************************************************/
/* Function prototypes	                                                      */
/******************************************************************************/
static int  init_nflhdl(void);
static int  Reset(void);                       /* Reset program          */
static void Terminate(UINT);                   /* Terminate program      */
static void HandleSignal(int);                 /* Handles signals        */
static void HandleErr(int);                    /* Handles general errors */
static void HandleQueErr(int);                 /* Handles queuing errors */
static int  HandleData(void);                  /* Handles event data     */
static void HandleQueues(void);                /* Waiting for Sts.-switch*/

/******************************************************************************/
/* my Function prototypes	                                                   */
/******************************************************************************/
static int  HandleCommand(UINT);
static int  SendFile(char *, char *, long, UINT, int);
static void DeleteRelevantFiles(void);

/*FYA v1.5a UFIS-8303*/
static void setHomeAirport(char *pcpProcessName, char *pcpHopo);
static void readDefaultHopo(char *pcpHopo);
static void setupHopo(char *pcpHopo);
static int checkAndsetHopo(char *pcpTwEnd, char *pcpHopo_sgstab);
static char *to_upper(char *pcgIfName);

static int SendStatusWithHopo( char *pcpIfTyp, char *pcpIfName, char *pcpMsgTyp,
                int    ipMsgNumber, char *pcpData1, char *pcpData2,  char *pcpData3);


/******************************************************************************/
/*                                                                            */
/* The MAIN program                                                           */
/*                                                                            */
/******************************************************************************/
MAIN
{
	int	ilRC = RC_SUCCESS;			/* Return code			*/
	int	ilCnt = 0;
	char	pclFileName[512];

	INITIALIZE;			/* General initialization	*/

	/*FYA v1.5a UFIS-8303*/
	strcpy(cgProcessName, argv[0]);

	dbg(TRACE,"<MAIN> Version <%s>", mks_version);

	/* all signals */
	(void)SetSignals(HandleSignal);
	(void)UnsetSignals();

	/* get data from sgs.tab */
	if ((ilRC = tool_search_exco_data("ALL", "TABEND", pcgTABEnd)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> %05d cannot find entry <TABEND> in sgs.tab", __LINE__);
		Terminate(45);
	}
	dbg(DEBUG,"<MAIN> %05d found TABEND: <%s>", __LINE__, pcgTABEnd);

	/*FYA v1.5a UFIS-8303*/
	/* read HomeAirPort from SGS.TAB */
	/*
	if ((ilRC = tool_search_exco_data("SYS", "HOMEAP", pcgHomeAP)) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> %05d cannot find entry <HOMEAP> in SGS.TAB", __LINE__);
		Terminate(45);
	}
	dbg(DEBUG,"<MAIN> %05d found HOME-AIRPORT: <%s>", __LINE__, pcgHomeAP);
    */

	/* Attach to the CCS queues */
	do
	{
		if ((ilRC = init_que()) != RC_SUCCESS)
		{
			sleep(6);		/* Wait for QCP to create queues */
			ilCnt++;
		}
	}while((ilCnt < 10) && (ilRC != RC_SUCCESS));

	if (ilRC != RC_SUCCESS)
	{
		dbg(TRACE,"MAIN: init_que failed!");
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

	/* necessary for HandleQueues (REMOTE_EVENT) */
	dbg(DEBUG,"main initializing ...");
	if ((ilRC = init_nflhdl()) != RC_SUCCESS)
	{
		dbg(DEBUG,"init_nflhdl returns: %d", ilRC);
		Terminate(30);
	}
	/* only a message */
	dbg(TRACE,"main initializing OK");

	/* in this case we have to delete all! relevant files
		in relevant all! directories
	*/
	if (ctrl_sta == HSB_STANDBY)
	{
		dbg(DEBUG,"<MAIN> %05d calling DeleteRelevantFiles now", __LINE__);
		DeleteRelevantFiles();
		dbg(DEBUG,"<MAIN> %05d finished DeleteRelevantFiles", __LINE__);
	}

	if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
	{
		dbg(DEBUG,"main waiting for status switch ...");
		HandleQueues();
	}

	/* forever */
	while (1)
	{
		/* get next item */
		ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

		/* set event pointer 		*/
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
				case	HSB_COMING_UP:
				case	HSB_STANDBY:
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
					dbg(DEBUG,"main unknown event");
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
static int init_nflhdl(void)
{
	int			ilRC;
	int			ilCurrentSection;
	char			*pclS;
	char			*pclCfgFilePath;
	char			pclCfgFile[2*iMIN_BUF_SIZE];
	char			pclSection[2*iMIN_BUF_SIZE];
	char			pclCurrentSection[2*iMIN_BUF_SIZE];
	char			pclTmpBuf[2*iMIN_BUF_SIZE];
	char			pclDelay[iMIN_BUF_SIZE];

	char pclTmp[32] = "\0";

	dbg(DEBUG,"<init_nflhdl> ------- START INIT_NFLHDL --------");

	strncpy(pclTmp,pcgHomeAP_sgstab,3);
	sprintf(pcgTwEnd, "%s,%s,%s",pclTmp, pcgTABEnd, cgProcessName);

	/* to inform the status-handler about startup time */

	/*v1.5a UFIS-8303*/
	/*if ((ilRC = SendStatus("FILEIO", "nflhdl", "START", 1, "", "", "")) != RC_SUCCESS)*/
	if ((ilRC = SendStatusWithHopo("FILEIO", "nflhdl", "START", 1, "", "", "")) != RC_SUCCESS)
	{
		dbg(TRACE,"<init_nflhdl> %05d SendStatusWithHopo returns: %d", __LINE__, ilRC);
		Terminate(30);
	}

	/* get path for cfg-file... */
	if ((pclCfgFilePath = getenv("CFG_PATH")) == NULL)
	{
		dbg(TRACE,"cannot run without CFG_PATH...");
		Terminate(30);
	}

	/* build path/filename... */
	memset((void*)pclCfgFile, 0x00, 2*iMIN_BUF_SIZE);
	strcpy(pclCfgFile, pclCfgFilePath);
	if (pclCfgFile[strlen(pclCfgFile)-1] != '/')
		strcat(pclCfgFile,"/");
	strcat(pclCfgFile, "nflhdl.cfg");

	strcpy(pcgCfgFile,pclCfgFile);

	/* -------------------------------------------------------------- */
	/* debug_level setting ------------------------------------------ */
	/* -------------------------------------------------------------- */
	pclTmpBuf[0] = 0x00;
	if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "debug_level", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
	{
		/* default */
		dbg(DEBUG,"<init_nflhdl> no debug_level in section <MAIN> - set to TRACE");
		strcpy(pclTmpBuf, "TRACE");
	}

	/* which debug_level is set in the Configfile ? */
	StringUPR((UCHAR*)pclTmpBuf);
	if (!strcmp(pclTmpBuf,"DEBUG"))
	{
		debug_level = DEBUG;
		dbg(TRACE,"<init_nflhdl> debug_level set to DEBUG");
	}
	else if (!strcmp(pclTmpBuf,"TRACE"))
	{
		debug_level = TRACE;
		dbg(TRACE,"<init_nflhdl> debug_level set to TRACE");
	}
	else if (!strcmp(pclTmpBuf,"OFF"))
	{
		debug_level = 0;
		dbg(TRACE,"<init_nflhdl> debug_level set to OFF");
	}
	else
	{
		debug_level = TRACE;
		dbg(TRACE,"<init_nflhdl> unknown debug_level set to default OFF");
	}

    setHomeAirport(cgProcessName, pcgHomeAP_sgstab);

	/* read entries from CFG-File... */
	memset((void*)pclSection, 0x00, 2*iMIN_BUF_SIZE);
	if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "section",
										CFG_STRING, pclSection)) != RC_SUCCESS)
	{
		dbg(TRACE,"cannot run without section in MAIN (%d)", ilRC);
		Terminate(30);
	}

	memset(pclDelay,0x00,iMIN_BUF_SIZE);
	if ((ilRC = iGetConfigEntry(pclCfgFile, "MAIN", "processing_delay",
										CFG_STRING, pclDelay)) != RC_SUCCESS)
	{
		dbg(TRACE,"<processing_delay> not set in MAIN-section! Using default 1000 ms.");
		igProcessingDelay=1000;
	}
	else
	{
		igProcessingDelay=atoi(pclDelay);
		if (igProcessingDelay == 0)
			igProcessingDelay=1000;
	}

	/* convert to uppercase */
	StringUPR((UCHAR*)pclSection);

	/* get number of currently uesd sections */
	rgTM.iNoOfSection = GetNoOfElements(pclSection, ',');
	dbg(DEBUG,"found %d sections...", rgTM.iNoOfSection);

	/* get memory for sections */
	if ((rgTM.prSection = (TNFSection*)malloc(rgTM.iNoOfSection*sizeof(TNFSection))) == NULL)
	{
		dbg(TRACE,"cannot allocate memory for section...");
		rgTM.prSection = NULL;
		Terminate(30);
	}

	/* for all section... */
	for (ilCurrentSection=0;
		  ilCurrentSection<rgTM.iNoOfSection;
		  ilCurrentSection++)
	{
		if ((pclS = GetDataField(pclSection, ilCurrentSection, ',')) == NULL)
		{
			dbg(TRACE,"can't read section %d", ilCurrentSection);
			Terminate(30);
		}

		/* clear memory */
		memset((void*)pclCurrentSection, 0x00, 2*iMIN_BUF_SIZE);
		strcpy(pclCurrentSection, pclS);
		dbg(DEBUG,"<init_nflhdl> ---------- START <%s> ---------", pclCurrentSection);

		/* the selection string... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "select_string", CFG_STRING, rgTM.prSection[ilCurrentSection].pcSelectString)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iUseSelectString = 1;
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iUseSelectString = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcSelectString,
																		0x00, iMAX_BUF_SIZE);
		}

		/* all departure fields... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "departure_fields", CFG_STRING, rgTM.prSection[ilCurrentSection].pcDepartureFields)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iUseDepartureFields = 1;
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iUseDepartureFields = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcDepartureFields,
																		0x00, iMAX_BUF_SIZE);
		}

		/* all arrival fields */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "arrival_fields", CFG_STRING, rgTM.prSection[ilCurrentSection].pcArrivalFields)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iUseArrivalFields = 1;
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iUseArrivalFields = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcArrivalFields,
																		0x00, iMAX_BUF_SIZE);
		}

		/* read all for renaming files... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "rename_filename", CFG_STRING, rgTM.prSection[ilCurrentSection].pcRenameFilename)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iRenameFilename = 1;
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iRenameFilename = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcRenameFilename,
																		0x00, iMIN_BUF_SIZE);
		}

		/* read all for renaming files (adding timestamp to filename) */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "add_timestamp_to_filename", CFG_STRING, rgTM.prSection[ilCurrentSection].pcTimeStampFormat)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iAddTimeStampToFilename = 1;
			StringUPR((UCHAR*)rgTM.prSection[ilCurrentSection].pcTimeStampFormat);
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iAddTimeStampToFilename = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcTimeStampFormat,
																		0x00, iMIN);
		}

		/* read all for renaming extension... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "rename_extension", CFG_STRING, rgTM.prSection[ilCurrentSection].pcRenameExtension)) == RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iRenameExtension = 1;
		}
		else
		{
			rgTM.prSection[ilCurrentSection].iRenameExtension = 0;
			memset((void*)rgTM.prSection[ilCurrentSection].pcRenameExtension,
				0x00, iMIN_BUF_SIZE);
		}
                /*rkl 30.08.99 -----------------------------------------------*/
		/* get rename extension type... */
		memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "rename_extension_type", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
		{
			/* convert to uppercase */
			StringUPR((UCHAR*)pclTmpBuf);
		}
		else
			strcpy(pclTmpBuf, "REPLACE");

		if (!strcmp(pclTmpBuf, "REPLACE"))
			rgTM.prSection[ilCurrentSection].iRenameExtensionType = 0;
		else if (!strcmp(pclTmpBuf, "APPEND"))
			rgTM.prSection[ilCurrentSection].iRenameExtensionType = 1;
		else
		{
			dbg(TRACE,"<init_nflhdl> unknown 'rename_extension_type'-entry, use default (REPLACE)");
			rgTM.prSection[ilCurrentSection].iRenameExtensionType = 0;
		}

                /*rkl --------------------------------------------------------*/
		/* read all for FTP-Using */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ftp_user", CFG_STRING, rgTM.prSection[ilCurrentSection].pcFtpUser)) == RC_SUCCESS)
		{
			/* found ftp_user -> need ftp_pass... */
			if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ftp_pass", CFG_STRING, rgTM.prSection[ilCurrentSection].pcFtpPass)) == RC_SUCCESS)
			{
				/* must decode password */
				pclS = rgTM.prSection[ilCurrentSection].pcFtpPass;
#ifdef __SHOW_PASSWD
				dbg(DEBUG,"<init_nflhdl> password before: <%s>", pclS);
#endif
				CDecode(&pclS);
#ifdef __SHOW_PASSWD
				dbg(DEBUG,"<init_nflhdl> password after: <%s>", pclS);
#endif
				/* store encoded password... */
				strcpy(rgTM.prSection[ilCurrentSection].pcFtpPass, pclS);

				/* for FTP machinename is necessary... */
				if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "machine", CFG_STRING, rgTM.prSection[ilCurrentSection].pcMachine)) == RC_SUCCESS)
				{
					/* machinepath is also necessary */
					if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "machine_path", CFG_STRING, rgTM.prSection[ilCurrentSection].pcMachinePath)) == RC_SUCCESS)
					{
						/* machinepath is also necessary */
						if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ftp_file_name", CFG_STRING, rgTM.prSection[ilCurrentSection].pcFtpFileName)) == RC_SUCCESS)
						{
							rgTM.prSection[ilCurrentSection].iUseFtp = 1;
							dbg(DEBUG,"<init_nflhdl> ----- START FTP/Params -----");
							dbg(DEBUG,"<init_nflhdl> FTP_USER: <%s>", rgTM.prSection[ilCurrentSection].pcFtpUser);
							dbg(DEBUG,"<init_nflhdl> FTP_PASS: <%s>", rgTM.prSection[ilCurrentSection].pcFtpPass);
							dbg(DEBUG,"<init_nflhdl> FTP_FILE_NAME: <%s>", rgTM.prSection[ilCurrentSection].pcFtpFileName);
							dbg(DEBUG,"<init_nflhdl> MACHINE: <%s>", rgTM.prSection[ilCurrentSection].pcMachine);
							dbg(DEBUG,"<init_nflhdl> MACHINE_PATH: <%s>", rgTM.prSection[ilCurrentSection].pcMachinePath);
							dbg(DEBUG,"<init_nflhdl> ----- END FTP/Params -----");
						}
						else
						{
							dbg(DEBUG,"<init_nflhdl> missing ftp_file_name, cannot use FTP...");
							rgTM.prSection[ilCurrentSection].iUseFtp = 0;
						}
					}
					else
					{
						dbg(DEBUG,"<init_nflhdl> missing machine_path, cannot use FTP...");
						rgTM.prSection[ilCurrentSection].iUseFtp = 0;
					}
				}
				else
				{
					dbg(DEBUG,"<init_nflhdl> missing machine, cannot use FTP...");
					rgTM.prSection[ilCurrentSection].iUseFtp = 0;
				}
			}
			else
			{
				dbg(DEBUG,"<init_nflhdl> missing ftp_pass, cannot use FTP...");
				rgTM.prSection[ilCurrentSection].iUseFtp = 0;
			}
		}
		else
			rgTM.prSection[ilCurrentSection].iUseFtp = 0;

		/* get delete ftp flag... */
		memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "delete_ftp_file", CFG_STRING, pclTmpBuf)) == RC_SUCCESS)
		{
			/* convert to uppercase */
			StringUPR((UCHAR*)pclTmpBuf);
		}
		else
			strcpy(pclTmpBuf, "NO");

		if (!strcmp(pclTmpBuf, "NO"))
			rgTM.prSection[ilCurrentSection].iDeleteFtpFile = 0;
		else if (!strcmp(pclTmpBuf, "YES"))
			rgTM.prSection[ilCurrentSection].iDeleteFtpFile = 1;
		else
		{
			dbg(TRACE,"<init_nflhdl> unknown 'delete_ftp_file'-entry, use default (NO)");
			rgTM.prSection[ilCurrentSection].iDeleteFtpFile = 0;
		}

		/*---------------------------------------------------------------*/
		/*---------------------------------------------------------------*/
		/*---------------------------------------------------------------*/
		/* the home airport */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "home_airport", CFG_STRING, rgTM.prSection[ilCurrentSection].pcHomeAirport)) != RC_SUCCESS)
		{

            /*FYA v1.5a UFIS-8303*/
            /*dbg(DEBUG,"<init_nflhdl> NO Home-Airport in Section <%s>: using <%s>",
			pclCurrentSection,rgTM.prSection[ilCurrentSection].pcHomeAirport);*/

            memset(rgTM.prSection[ilCurrentSection].pcHomeAirport,0,sizeof(rgTM.prSection[ilCurrentSection].pcHomeAirport));

            dbg(DEBUG,"<init_nflhdl> NO Home-Airport in Section <%s>: keep it null",pclCurrentSection);

			//strcpy(rgTM.prSection[ilCurrentSection].pcHomeAirport, pcgHomeAP);
		}

        if(rgTM.prSection[ilCurrentSection].pcHomeAirport != NULL && strlen(rgTM.prSection[ilCurrentSection].pcHomeAirport) > 0)
        {
            if (CheckAPC3(rgTM.prSection[ilCurrentSection].pcHomeAirport) != RC_SUCCESS)
            {
                dbg(DEBUG,"<init_nflhdl> No Valid Home-Airport: <%s>", rgTM.prSection[ilCurrentSection].pcHomeAirport);
                //strcpy(rgTM.prSection[ilCurrentSection].pcHomeAirport, pcgHomeAP);
            }
            dbg(DEBUG,"<init_nflhdl> Home-Airport: <%s>", rgTM.prSection[ilCurrentSection].pcHomeAirport);
        }

		/* the table extension */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "table_extension", CFG_STRING, rgTM.prSection[ilCurrentSection].pcTableExtension)) != RC_SUCCESS)
		{
			strcpy(rgTM.prSection[ilCurrentSection].pcTableExtension, pcgTABEnd);
		}
		dbg(DEBUG,"<init_nflhdl> TableExtension: <%s>", rgTM.prSection[ilCurrentSection].pcTableExtension);

		/*rkl 21.10.99 --------------------------------------------------*/
		/*keywords dest_name, recv_name ---------------------------------*/
		/*---------------------------------------------------------------*/
        /* recv_name */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "recv_name", CFG_STRING, rgTM.prSection[ilCurrentSection].pcRecvName)) != RC_SUCCESS)
		{
			strcpy(rgTM.prSection[ilCurrentSection].pcRecvName, "EXCO");
		}
		dbg(DEBUG,"<init_nflhdl> RecvName: <%s>", rgTM.prSection[ilCurrentSection].pcRecvName);

        /* dest_name */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "dest_name", CFG_STRING, rgTM.prSection[ilCurrentSection].pcDestName)) != RC_SUCCESS)
		{
			strcpy(rgTM.prSection[ilCurrentSection].pcDestName, "NFLHDL");
		}
		dbg(DEBUG,"<init_nflhdl> DestName: <%s>", rgTM.prSection[ilCurrentSection].pcRecvName);

        /*rkl -----------------------------------------------------------*/

		/*---------------------------------------------------------------*/
		/*---------------------------------------------------------------*/
		/*---------------------------------------------------------------*/
		/* send command... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "snd_cmd", CFG_STRING, rgTM.prSection[ilCurrentSection].pcSendCommand)) != RC_SUCCESS)
		{
			dbg(TRACE,"cannot run without send command in section <%s>",
			          pclCurrentSection);
			Terminate(30);
		}

		/* get route... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "mod_id", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iModID)) != RC_SUCCESS)
		{
			dbg(TRACE,"cannot run without mod_id in section <%s>",
																		pclCurrentSection);
			Terminate(30);
		}

		/* command... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "command", CFG_STRING, rgTM.prSection[ilCurrentSection].pcCommand)) != RC_SUCCESS)
		{
			dbg(TRACE,"cannot run without command in section <%s>",
					pclCurrentSection);
			Terminate(30);
		}

		/* move to... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "move_to", CFG_STRING, rgTM.prSection[ilCurrentSection].pcMoveTo)) != RC_SUCCESS)
		{
			dbg(TRACE,"error reading move_to in section <%s>", pclCurrentSection);
			rgTM.prSection[ilCurrentSection].iUseMoveTo = 0;
		}
		else
			rgTM.prSection[ilCurrentSection].iUseMoveTo = 1;

		/* file path we scan */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "file_path", CFG_STRING, rgTM.prSection[ilCurrentSection].pcFilePath)) != RC_SUCCESS)
		{
			dbg(DEBUG,"<init_nflhdl> missing file_path....");
			Terminate(30);
		}

		/* file name we search */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "file_name", CFG_STRING, rgTM.prSection[ilCurrentSection].pcFileName)) != RC_SUCCESS)
		{
			dbg(DEBUG,"<init_nflhdl> missing file_name....");
			Terminate(30);
		}

		/* Text type for CTXHDL... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ctx_text_type", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iCtxTextType)) != RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iCtxTextType = -1;
		}

		/* Result code for CTXHDL... */
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "ctx_result_code", CFG_INT, (char*)&rgTM.prSection[ilCurrentSection].iCtxResultCode)) != RC_SUCCESS)
		{
			rgTM.prSection[ilCurrentSection].iCtxResultCode = -1;
		}

		/* remove old file? */
		memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
		if ((ilRC = iGetConfigEntry(pclCfgFile, pclCurrentSection, "remove", CFG_STRING, pclTmpBuf)) != RC_SUCCESS)
		{
			strcpy(pclTmpBuf, sDEFAULT_REMOVE);
		}
		StringUPR((UCHAR*)pclTmpBuf);
		if (!strcmp(pclTmpBuf, "YES"))
			rgTM.prSection[ilCurrentSection].iRemove = 1;
		else if (!strcmp(pclTmpBuf, "NO"))
			rgTM.prSection[ilCurrentSection].iRemove = 0;
		else
		{
			dbg(TRACE,"unknown entry for remove in section <%s>-use default",
																		pclCurrentSection);
			rgTM.prSection[ilCurrentSection].iRemove = iDEFAULT_REMOVE;
		}

		dbg(DEBUG,"<init_nflhdl> ---------- END <%s> ---------", pclCurrentSection);
	}

	dbg(DEBUG,"<init_nflhdl> ------- END INIT_NFLHDL --------");
	/* everything looks fine */
	return RC_SUCCESS;
}


/******************************************************************************/
/* The Reset routine                                                          */
/******************************************************************************/
static int Reset(void)
{
	int		ilRC;

	dbg(DEBUG,"Reset: now resetting");

	sprintf(pcgTwEnd, "%s,%s,%s", pcgHomeAP, pcgTABEnd, cgProcessName);

	/* to inform the status-handler about end time */

    /*v1.5a UFIS-8303*/
	/*if ((ilRC = SendStatus("FILEIO", "nflhdl", "STOP", 1, "", "", "")) != RC_SUCCESS)*/
	if ((ilRC = SendStatusWithHopo("FILEIO", "nflhdl", "STOP", 1, "", "", "")) != RC_SUCCESS)
	{
		dbg(TRACE,"<init_nflhdl> %05d SendStatusWithHopo returns: %d", __LINE__, ilRC);
		Terminate(0);
	}

	if (prgItem != NULL)
	{
		dbg(DEBUG,"free Item");
		free((void*)prgItem);
		prgItem = NULL;
	}

	if (rgTM.prSection != NULL)
	{
		dbg(DEBUG,"free section");
		free((void*)rgTM.prSection);
		rgTM.prSection = NULL;
	}

	/* new init... */
	if ((ilRC = init_nflhdl()) != RC_SUCCESS)
	{
		dbg(TRACE,"Reset init_nflhdl returns: %d", ilRC);
		Terminate(0);
	}

	dbg(TRACE,"...end of reset");
	return RC_SUCCESS;
}


/******************************************************************************/
/* The termination routine                                                    */
/******************************************************************************/
static void Terminate(UINT ipSleepTime)
{
	int	ilRC;

	/* ignore all signals */
	(void)UnsetSignals();

	/* must we wait some seconds? */
	if (ipSleepTime > 0)
	{
		dbg(DEBUG,"<Terminate> sleeping %d second...", ipSleepTime);
		sleep(ipSleepTime);
	}

	/* delete memory */
	if (prgItem != NULL)
	{
		dbg(DEBUG,"free Item");
		free((void*)prgItem);
		prgItem = NULL;
	}

	if (rgTM.prSection != NULL)
	{
		dbg(DEBUG,"free section");
		free((void*)rgTM.prSection);
		rgTM.prSection = NULL;
	}

    /*v1.5a UFIS-8303*/
    sprintf(pcgTwEnd, "%s,%s,%s", pcgHomeAP, pcgTABEnd, cgProcessName);

	/* to inform the status-handler about end time */
	/*if ((ilRC = SendStatus("FILEIO", "nflhdl", "STOP", 1, "", "", "")) != RC_SUCCESS)*/
	if ((ilRC = SendStatusWithHopo("FILEIO", "nflhdl", "STOP", 1, "", "", "")) != RC_SUCCESS)
	{
		dbg(TRACE,"<init_nflhdl> %05d SendStatusWithHopo returns: %d", __LINE__, ilRC);
	}

	dbg(TRACE,"Terminate: now leaving ...");
	exit(0);
}


/******************************************************************************/
/* The handle signals routine                                                 */
/******************************************************************************/
static void HandleSignal(int pipSig)
{
	if (pipSig != 18)
		dbg(DEBUG,"<HandleSignal> signal <%d> received",pipSig);

	/* set flags */
	if (pipSig == 14 && igTstSigAlrm == 1)
	{
		/* this is sigalarm at tool_fn_to_buf-function... */
		igSigAlrm = 1;
	}
	else
	{
		igSigAlrm = 0;
	}
	igTstSigAlrm = 0;

	switch(pipSig)
	{
		case SIGALRM:
		case SIGCLD:
			/*
			SetSignals(HandleSignal);
			*/
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
	dbg(DEBUG,"call HandleErr with %d", ipErr);

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

	return;
}


/******************************************************************************/
/* The handle queues routine                                                  */
/******************************************************************************/
static void HandleQueues(void)
{
	int		ilRC = RC_SUCCESS;			/* Return code */
	int		ilBreakOut = FALSE;
	int		ilCurSection;
	BC_HEAD	*prlBCHead;

	do
	{
		/* get next item */
		ilRC = que(QUE_GETBIG,0,mod_id,PRIORITY_3,0,(char*)&prgItem);

		/* set event pointer 		*/
		prgEvent = (EVENT*)prgItem->text;

		/* check returncode */
		if (ilRC == RC_SUCCESS)
		{
			/* Acknowledge the item */
			ilRC = que(QUE_ACK,0,mod_id,0,0,NULL);
			if( ilRC != RC_SUCCESS )
			{
				HandleQueErr(ilRC);
			}

			switch (prgEvent->command)
			{
				case	HSB_STANDBY:
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
					dbg(DEBUG,"HandleQueues: wrong hsb status <%d>",ctrl_sta);
					DebugPrintItem(TRACE,prgItem);
					DebugPrintEvent(TRACE,prgEvent);
					break;

				case iREMOTE_EVENT_DATA: /* NFLHDL internal use only */
					if ((ctrl_sta != HSB_STANDALONE) && (ctrl_sta != HSB_ACTIVE) && (ctrl_sta != HSB_ACT_TO_SBY))
					{
						dbg(DEBUG,"<HandleQueues> got REMOTE_EVENT_DATA request");

						/* set pointer... */
						prlBCHead 	 = (BC_HEAD*)((char*)prgEvent + sizeof(EVENT));
						ilCurSection = atoi((char*)prlBCHead->recv_name);
						dbg(DEBUG,"<HandleQueues> current section: %d", ilCurSection);

						/* necessary to delete local files */
						HandleCommand(ilCurSection);
					}
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

	/* bye bye */
	return;
}


/******************************************************************************/
/* The handle data routine                                                    */
/******************************************************************************/
static int HandleData(void)
{
	int			i;
	int			ilRC;
	BC_HEAD		*prlBCHead 			= NULL;
	CMDBLK		*prlCmdblk 			= NULL;

	char *pclFunc = "HandleData";

	/* set pointer... */
	prlBCHead 	 = (BC_HEAD*)((char*)prgEvent + sizeof(EVENT));
	prlCmdblk 	 = (CMDBLK*)prlBCHead->data;

	strcpy(pcgTwEnd, prlCmdblk->tw_end);

	if(checkAndsetHopo(pcgTwEnd, pcgHomeAP_sgstab) == RC_FAIL)
        return RC_FAIL;

    dbg(TRACE,"%s: TABEND = <%s>",pclFunc,pcgTABEnd);
    memset(pcgTwEnd,0x00,sizeof(pcgTwEnd));
    sprintf(pcgTwEnd,"%s,%s,%s",pcgHomeAP,pcgTABEnd,mod_name);
    dbg(TRACE,"Init_exchdl : TW_END = <%s>",pcgTwEnd);

	/* convert to uppercase */
	StringUPR((UCHAR*)prlCmdblk->command);

	/* is this a command we know? */
	for (i=0; i<rgTM.iNoOfSection; i++)
	{
		/* first to uppercase */
		StringUPR((UCHAR*)rgTM.prSection[i].pcCommand);

        /*FYA v1.5a UFIS-8303*/

        if(rgTM.prSection[i].pcHomeAirport == NULL || strlen(rgTM.prSection[i].pcHomeAirport) == 0)
        {
            strcpy(rgTM.prSection[i].pcHomeAirport, pcgHomeAP);
        }

        /* compare individual hopo */

		//disable hopo check for multiple airports
		/*if(checkHopo(pcgTwEnd, rgTM.prSection[i].pcHomeAirport) == RC_SUCCESS)*/
		if (!strncmp(pcgHomeAP, rgTM.prSection[i].pcHomeAirport,3))
		{
		    /* compare commands */
            if (!strcmp(prlCmdblk->command, rgTM.prSection[i].pcCommand))
		    {
		        /* handle this command */
                dbg(DEBUG,"<HandleData> ------ START <%s> ------", prlCmdblk->command);
                if ((ilRC = HandleCommand((UINT)i)) != RC_SUCCESS)
                {
                    dbg(DEBUG,"<HandleData> HandleCommand returns %d", ilRC);
                }
                dbg(DEBUG,"<HandleData> ------ END <%s> ------", prlCmdblk->command);
		    }
		}
	}

	/* bye bye */
	return RC_SUCCESS;
}

/******************************************************************************/
/* The handle command routine                                                 */
/******************************************************************************/
static int HandleCommand(UINT	ipSecNo)
{
	int				ilRC;
	int				ilUseCTX;
	long				llFileLength;
	DIR				*prlAktDir		= NULL;
	struct dirent	*prlDirent		= NULL;
	char				*pclContents  	= NULL;
	char				*pclS				= NULL;
	char				*pclTmpPath		= NULL;
	char				pclTmpFile[iMIN_BUF_SIZE];
	char				pclTmpBuf[iMIN_BUF_SIZE];
	char				pclBuf[iMIN_BUF_SIZE];
	char				pclFilename[iMIN_BUF_SIZE];
	char				pclTmpFilename[iMIN_BUF_SIZE];
	FILE				*fh;
	EVENT				*prlRemoteEvent 	= NULL;
	BC_HEAD			*prlRemoteBCHead 	= NULL;

	dbg(DEBUG,"<HandleCommand> ---------- START ----------");

	if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
	{
		/* this is remote... */
		if (rgTM.prSection[ipSecNo].iUseFtp)
		{
			dbg(DEBUG,"<HandleCommand> USING FTP.....");

			/* built controlfile for FTP... */
			if ((fh = fopen(sDEFAULT_CTRLFILE, "w")) == NULL)
			{
				dbg(TRACE,"<HandleCommand> can't open FTP-CtrlFile...");
			}
			else
			{
				/* write user */
				fprintf(fh, "user %s %s\n",
						rgTM.prSection[ipSecNo].pcFtpUser,
						rgTM.prSection[ipSecNo].pcFtpPass);

				/* change directory */
				fprintf(fh, "cd %s\n",
						rgTM.prSection[ipSecNo].pcMachinePath);

				/* get statement */
				fprintf(fh, "get %s %s/%s\n",
						rgTM.prSection[ipSecNo].pcFtpFileName,
						rgTM.prSection[ipSecNo].pcFilePath,
						rgTM.prSection[ipSecNo].pcFtpFileName);

				/* should i delete remote file? */
				if (rgTM.prSection[ipSecNo].iDeleteFtpFile)
				{
					dbg(DEBUG,"<HandleCommand> DELETE FTP FILE.....");

					/* get statement */
					fprintf(fh, "delete %s\n",
							rgTM.prSection[ipSecNo].pcFtpFileName);
				}

				/* bye bye */
				fprintf(fh, "bye");

				/* close ctrlfile */
				fclose(fh);
			}

			/* clear memory */
			memset((void*)pclTmpFile, 0x00, iMIN_BUF_SIZE);
			if ((pclTmpPath = getenv("TMP_PATH")) == NULL)
			{
				dbg(TRACE,"<HandleCommand> missing $TMP_PATH...");
				strcpy(pclTmpFile, "./nflhdl_ftp.log");
			}
			else
			{
				sprintf(pclTmpFile, "%s/%s", pclTmpPath, "nflhdl_ftp.log");
			}

			/* get file now... */
			memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);

			/* built command */
			sprintf(pclTmpBuf, "ftp -n %s < %s 2>&1 > %s", rgTM.prSection[ipSecNo].pcMachine, sDEFAULT_CTRLFILE, pclTmpFile);
			dbg(DEBUG,"<HandleCommand> system string is: <%s>", pclTmpBuf);
			igTstSigAlrm = 0;
			alarm(90);
			ilRC = system(pclTmpBuf);
			alarm(0);
			dbg(DEBUG,"<HandleCommand> system returns: %d", ilRC);

			/* delete ctrlfile... */
			if ((ilRC = remove(sDEFAULT_CTRLFILE)) != 0)
			{
				dbg(TRACE,"<HandleCommand> cannot remove ctrl-file: <%s>", sDEFAULT_CTRLFILE);
			}
		}
	}

	/* open this directory... */
	if ((prlAktDir = opendir(rgTM.prSection[ipSecNo].pcFilePath)) == NULL)
	{
		dbg(TRACE,"opendir <%s> returns NULL", rgTM.prSection[ipSecNo].pcFilePath);
		return RC_FAIL;
	}

	/* search file */
	while ((prlDirent = readdir(prlAktDir)) != NULL)
	{
		/* don't use .xxxx files (.profile, .exrc, .cshrc, ...) */
#ifdef _SNI
		if ((prlDirent->d_name-2)[0] == '.')
		{
			dbg(DEBUG,"<HandleCommand> continue because file is <%s>", prlDirent->d_name-2);
			continue;
		}
#else
		if ((prlDirent->d_name)[0] == '.')
		{
			dbg(DEBUG,"<HandleCommand> continue because file is <%s>", prlDirent->d_name);
			continue;
		}
#endif

		/* check file -> match it pattern? */
#ifdef _SNI
		dbg(DEBUG,"<HandleCommand> compare <%s> and <%s>",
								prlDirent->d_name-2, rgTM.prSection[ipSecNo].pcFileName);
#else
		dbg(DEBUG,"<HandleCommand> compare <%s> and <%s>",
								prlDirent->d_name, rgTM.prSection[ipSecNo].pcFileName);
#endif


#ifdef _SNI
		if ((ilRC = MatchPattern(prlDirent->d_name-2,
										 rgTM.prSection[ipSecNo].pcFileName)) == 1)
#else
		if ((ilRC = MatchPattern(prlDirent->d_name,
										 rgTM.prSection[ipSecNo].pcFileName)) == 1)
#endif
		{
			/* found file... */
			dbg(DEBUG,"<HandleCommand> ...MATCH...");
#ifdef _SNI
			dbg(DEBUG,"<HandleCommand> found file <%s>", prlDirent->d_name-2);
#else
			dbg(DEBUG,"<HandleCommand> found file <%s>", prlDirent->d_name);
#endif

			/* onyl for this ctrl-stati */
			if ((ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
			{
				/* if we found file, send a remote event to standby system */
				/* memory for remote event */
				if ((prlRemoteEvent = (EVENT*)malloc(sizeof(EVENT)+sizeof(BC_HEAD))) == NULL)
				{
					dbg(TRACE,"can't allocate memory for remote event");
				}
				else
				{
					/* if we are here, this must be the ACTIVE-System */
					/* send the STANDBY-System a corresponding event */
					prlRemoteEvent->command 	 = iREMOTE_EVENT_DATA;
					prlRemoteEvent->originator  = mod_id;
					prlRemoteEvent->retry_count = 0;
					prlRemoteEvent->data_offset = sizeof(EVENT);
					prlRemoteEvent->data_length = sizeof(BC_HEAD);

					prlRemoteBCHead = (BC_HEAD*)((char*)prlRemoteEvent+sizeof(EVENT));
					prlRemoteBCHead->rc = RC_SUCCESS;
					strcpy(prlRemoteBCHead->dest_name, "NFLHDL");
					sprintf(prlRemoteBCHead->recv_name, "%d", ipSecNo);

					if ((ilRC = RemoteQue(QUE_PUT, mod_id, mod_id, PRIORITY_4, sizeof(EVENT)+sizeof(BC_HEAD), (char*)prlRemoteEvent)) != RC_SUCCESS)
					{
						dbg(TRACE,"<HandleData> %05d RemoteQue failed...", __LINE__);
					}

					/* delete memory */
					free (prlRemoteEvent);
				}
			}

			/* clear memory */
			memset((void*)pclFilename, 0x00, iMIN_BUF_SIZE);

			/* build path/filename... */
			strcpy(pclFilename, rgTM.prSection[ipSecNo].pcFilePath);
			if (pclFilename[strlen(pclFilename)-1] != '/')
				strcat(pclFilename, "/");

#ifdef _SNI
			strcat(pclFilename, prlDirent->d_name-2);
#else
			strcat(pclFilename, prlDirent->d_name);
#endif

			/* 16.08.2002 */
			/* 17.08.2005 - added to STD4.5 - Requested by PRF 6692*/
			/* JWE: added this section here because there exist problems with simultanoeus reading/writing*/
			/*      of sita telexes in ATH. Now we first move "file" to "file.NFL" and then it will be    */
			/*      processed. This ensures that we solve this conflict.                                  */
			/*      Additionally we use a small delay to "ensure" as good as possible that this file is   */
			/*      closed from writing from the remote system.                                           */

			memset(pclTmpBuf,0x00,iMIN_BUF_SIZE);
			sprintf(pclTmpBuf,"mv %s %s.NFL",pclFilename,pclFilename);
			dbg(DEBUG,"<HandleCommand> move <%s> to <%s.NFL> using = system(%s)! Timeout=15 sec."
					,pclFilename,pclFilename,pclTmpBuf);
			igTstSigAlrm = 0;
			alarm(15);
			ilRC = system(pclTmpBuf);
			alarm(0);
			dbg(DEBUG,"<HandleCommand> system returns: <%d>", ilRC);
#if 0
			if (ilRC == RC_SUCCESS)
			{
				strcat(pclFilename,".NFL");
			}
#endif
			/* Always cat, because on LINUX systems ilRC = -1 is returnd */
			strcat(pclFilename,".NFL");
			nap(igProcessingDelay);
			dbg(DEBUG,"<HandleCommand> filename to process=<%s>",pclFilename);

			/* 16.08.2002 */
			/* JWE-END */

			if ((ctrl_sta == HSB_STANDALONE) || (ctrl_sta == HSB_ACTIVE) || (ctrl_sta == HSB_ACT_TO_SBY))
			{
				/* set alarm to 30 seconds */
				igTstSigAlrm = 1;
				alarm(30);

				/* get contents of file */
				if ((ilRC = tool_fn_to_buf(pclFilename, &llFileLength, &pclContents)) != RC_SUCCESS)
				{
					dbg(DEBUG,"<HandleCommand> tool_fn_to_buf returns: %d", ilRC);

					/* reset alarm */
					alarm(0);

					/* reset flag */
					igSigAlrm = 0;

					/* close the directory */
					if (prlAktDir != NULL)
					{
						dbg(DEBUG,"<HandleCommand> close directory...");
						closedir(prlAktDir);
						prlAktDir = NULL;
					}

					/* message and returning */
					return RC_FAIL;
				}

				/* reset alarm */
				alarm(0);

				/* reset flag if necessary */
				if (igSigAlrm == 1)
				{
					dbg(TRACE,"<HandleCommand> this is an alarm signal...");
					igSigAlrm = 0;
					dbg(DEBUG,"<HandleCommand> free filebuffer now...");

					/* free memory */
					if (pclContents != NULL)
					{
						free((void*)pclContents);
						pclContents = NULL;
					}

					/* close the directory */
					if (prlAktDir != NULL)
					{
						dbg(DEBUG,"<HandleCommand> close directory...");
						closedir(prlAktDir);
						prlAktDir = NULL;
					}

					/* bye bye */
					return RC_FAIL;
				}

				/* must i use ctx-block? */
				if (rgTM.prSection[ipSecNo].iCtxTextType >= 0 &&
					 rgTM.prSection[ipSecNo].iCtxResultCode >= 0)
				{
					ilUseCTX = 1;
				}
				else
				{
					ilUseCTX = 0;
				}

				/* send it ... */
				if ((ilRC = SendFile(pclContents, pclFilename, llFileLength, ipSecNo, ilUseCTX)) != RC_SUCCESS)
				{
					dbg(TRACE,"<HandleCommand> SendFile returns: %d", ilRC);
					dbg(DEBUG,"<HandleCommand> free filebuffer now...");

					/* free memory */
					if (pclContents != NULL)
					{
						free((void*)pclContents);
						pclContents = NULL;
					}

					/* close the directory */
					if (prlAktDir != NULL)
					{
						dbg(DEBUG,"<HandleCommand> close directory...");
						closedir(prlAktDir);
						prlAktDir = NULL;
					}

					/* bye bye */
					return RC_FAIL;
				}

				/* free memory for filebuf... */
				if (pclContents != NULL)
				{
					free((void*)pclContents);
					pclContents = NULL;
				}
			}

			/* everything looks good...we should move or remove the file */
			if (rgTM.prSection[ipSecNo].iRemove)
			{
				dbg(DEBUG,"<HandleCommand> remove file now");
				/* delete this file */
				if ((ilRC = remove((const char*)pclFilename)) != 0)
				{
					dbg(TRACE,"<HandleCommand> cannot remove <%s>", pclFilename);
				}
			}

			if (rgTM.prSection[ipSecNo].iUseMoveTo &&
				!rgTM.prSection[ipSecNo].iRemove)
			{
				dbg(DEBUG,"<HandleCommand> move file now");
				/* we must move... */
				memset((void*)pclTmpBuf, 0x00, iMIN_BUF_SIZE);
				memset((void*)pclBuf, 0x00, iMIN_BUF_SIZE);

				strcpy(pclBuf, rgTM.prSection[ipSecNo].pcMoveTo);
				if (pclBuf[strlen(pclBuf)-1] != '/')
					strcat(pclBuf,"/");
#ifdef _SNI
				strcpy(pclTmpFilename, prlDirent->d_name-2);
#else
				strcpy(pclTmpFilename, prlDirent->d_name);
#endif
				strcat(pclTmpFilename,".NFL");
				/* if we use (brand)new filename */
				if (rgTM.prSection[ipSecNo].iRenameFilename)
				{
					strcpy(pclTmpFilename, rgTM.prSection[ipSecNo].pcRenameFilename);
				}

				/* if we add timestamp to filename */
				if (rgTM.prSection[ipSecNo].iAddTimeStampToFilename)
				{
					strcat(pclTmpFilename, GetPartOfTimeStamp(GetTimeStamp(), rgTM.prSection[ipSecNo].pcTimeStampFormat));
				}

				/* if we rename files... */
				if (rgTM.prSection[ipSecNo].iRenameExtension)
				{

  				  if (!rgTM.prSection[ipSecNo].iRenameExtensionType)
				  {
                                    /* Replace Extension */
				    pclS = pclTmpFilename;
				    while (*pclS)
				    {
				      if (*pclS == '.')
				      {
				  	*pclS = 0x00;
					strcat(pclTmpFilename, rgTM.prSection[ipSecNo].pcRenameExtension);
					break;
				      }
				      pclS++;
				    } /* end while */
				  }
				  else
				  {
				    /* Append Extension */
				    strcat(pclTmpFilename, rgTM.prSection[ipSecNo].pcRenameExtension);
				  }
				}

				/* built command... */
				sprintf(pclTmpBuf,"mv %s %s%s",
											pclFilename, pclBuf, pclTmpFilename);

				dbg(DEBUG,"<HandleCommand> system-string is <%s>", pclTmpBuf);
				igTstSigAlrm = 0;
				alarm(90);
				ilRC = system(pclTmpBuf);
				alarm(0);
				dbg(DEBUG,"<HandleCommand> system returns: %d", ilRC);
			}
		}
		else
		{
			dbg(DEBUG,"<HandleCommand> MatchPattern returns: %d", ilRC);
		}
	}

	/* closedir... */
	if (prlAktDir != NULL)
	{
		dbg(DEBUG,"<HandleCommand> close directory...");
		closedir(prlAktDir);
		prlAktDir = NULL;
	}

	dbg(DEBUG,"<HandleCommand> ---------- END ----------");
	/* ciao */
	return RC_SUCCESS;
}

/******************************************************************************/
/* The SendFile command routine                                               */
/******************************************************************************/
static int SendFile(char *pcpData, char *pcpFilename, long lpDataLen,
						  UINT ipSecNo, int ipUseCTXBlk)
{
	int			ilRC;
	long			llLen;
	EVENT			*prlOutEvent;
	BC_HEAD		*prlOutBCHead;
	CMDBLK		*prlOutCmdblk;
	CTXBLK		*prlOutCtxblk;
	char			*pclOutSelection;
	char			*pclOutFields;
	char			*pclOutDatablk;
	char			pclTmpFieldBuffer[iMAXIMUM];
	char			pclTmpLen[iMIN_BUF_SIZE];

	if (ipUseCTXBlk)
	{
		/* compute lenght of item... */
		llLen = lpDataLen + sizeof(EVENT) + sizeof(BC_HEAD) +
													  sizeof(CMDBLK) + sizeof(CTXBLK) + 5;
	}
	else
	{
		/* compute lenght of item... */
		llLen = lpDataLen + sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + 5;

		/* clear buffer */
		memset((void*)pclTmpFieldBuffer, 0x00, iMAXIMUM);

		if (rgTM.prSection[ipSecNo].iUseArrivalFields)
		{
			llLen += strlen(rgTM.prSection[ipSecNo].pcArrivalFields);
			strcpy(pclTmpFieldBuffer, rgTM.prSection[ipSecNo].pcArrivalFields);
			strcat(pclTmpFieldBuffer, "\n");
		}

		if (rgTM.prSection[ipSecNo].iUseDepartureFields)
		{
			llLen += strlen(rgTM.prSection[ipSecNo].pcDepartureFields);
			strcat(pclTmpFieldBuffer, rgTM.prSection[ipSecNo].pcDepartureFields);
		}

		if (rgTM.prSection[ipSecNo].iUseSelectString)
		{
			llLen += strlen(rgTM.prSection[ipSecNo].pcSelectString);
		}
	}

	/* memory for out event */
	dbg(DEBUG,"<SendFile> try to allocate %d bytes for OutEvent", llLen);
	if ((prlOutEvent = (EVENT*)malloc((size_t)llLen)) == NULL)
	{
		dbg(TRACE,"can't allocate memory for out event");
		return RC_FAIL;
	}

	/* clear memory */
	memset((void*)prlOutEvent, 0x00, llLen);

	/* Event... */
	prlOutEvent->command		 = EVENT_DATA;
	prlOutEvent->originator	 = mod_id;
	prlOutEvent->retry_count = 0;
	prlOutEvent->data_length = llLen - sizeof(EVENT);
	prlOutEvent->data_offset = sizeof(EVENT);

	/* BC-Header... */
	prlOutBCHead  = (BC_HEAD*)((char*)prlOutEvent+sizeof(EVENT));
	prlOutBCHead->rc = RC_SUCCESS;
	strcpy(prlOutBCHead->dest_name, rgTM.prSection[ipSecNo].pcDestName);
	strcpy(prlOutBCHead->recv_name, rgTM.prSection[ipSecNo].pcRecvName);

	/* Cmdblk... */
	prlOutCmdblk  = (CMDBLK*)prlOutBCHead->data;
	strcpy(prlOutCmdblk->command, rgTM.prSection[ipSecNo].pcSendCommand);
/*TMI 17.01.2000  filelength in tw_start for converters */
	sprintf(prlOutCmdblk->tw_start, "%ld", lpDataLen);
/* 4th element in tw_end is ignore */
	sprintf(prlOutCmdblk->tw_end, "%s,%s,NFLHDL,0", rgTM.prSection[ipSecNo].pcHomeAirport, rgTM.prSection[ipSecNo].pcTableExtension);

	if (ipUseCTXBlk)
	{
		dbg(DEBUG,"<SendFile> use CTX-Block for sending...");

		/* show the other processes that we use ctx-blk... */
		strcat(prlOutBCHead->recv_name, ",CTX");

		/* Ctxblk... */
		prlOutCtxblk  = (CTXBLK*)prlOutCmdblk->data;
		prlOutCtxblk->text_type = rgTM.prSection[ipSecNo].iCtxTextType;
		prlOutCtxblk->result_to = rgTM.prSection[ipSecNo].iCtxResultCode;

		/* datablk... */
		pclOutDatablk = (char*)prlOutCtxblk->data;
		memcpy((void*)pclOutDatablk, (const void*)pcpData, (size_t)lpDataLen);
	}
	else
	{
		dbg(DEBUG,"<SendFile> cannot work with CTX-Block...");

		/* selection... */
		pclOutSelection = (char*)prlOutCmdblk->data;
		strcpy(pclOutSelection, rgTM.prSection[ipSecNo].pcSelectString);

		/* fields */
		pclOutFields = pclOutSelection + strlen(pclOutSelection) + 1;
		strcpy(pclOutFields, pclTmpFieldBuffer);

		/* data */
		pclOutDatablk = pclOutFields + strlen(pclOutFields) + 1;
		memcpy((void*)pclOutDatablk, (const void*)pcpData, (size_t)lpDataLen);
	}

	/* send now */
	if ((ilRC = que(QUE_PUT, rgTM.prSection[ipSecNo].iModID, mod_id, PRIORITY_4, llLen, (char*)prlOutEvent)) != RC_SUCCESS)
	{
		dbg(TRACE,"<SendFile> QUE_PUT returns: %d", ilRC);
	}
	else
	{
		/* to inform the status-handler about end time */
		sprintf(pclTmpLen, "%ld", lpDataLen);

        /*v1.5a UFIS-8303*/
        sprintf(pcgTwEnd, "%s,%s,%s", pcgHomeAP, pcgTABEnd, cgProcessName);

		/*if ((ilRC = SendStatus("FILEIO", "nflhdl", "READ", 1, pcpFilename, pclTmpLen, "")) != RC_SUCCESS)*/
        if ((ilRC = SendStatusWithHopo("FILEIO", "nflhdl", "READ", 1, pcpFilename, pclTmpLen, "")) != RC_SUCCESS)
		{
			dbg(TRACE,"<init_nflhdl> %05d SendStatusWithHopo returns: %d", __LINE__, ilRC);
		}
	}

	/* delete memory */
	free((void*)prlOutEvent);

	/* everything looks fine... */
	return ilRC;
}

/******************************************************************************/
/* The DeleteRelevantFiles routine 			                                    */
/******************************************************************************/
static void DeleteRelevantFiles(void)
{
	int	ilRC;
	int	ilCurSec;

	/* for all sections */
	for (ilCurSec=0; ilCurSec<rgTM.iNoOfSection; ilCurSec++)
	{
		/* handle all sections */
		dbg(DEBUG,"<HandleData> ------ START SECTION %d ------", ilCurSec);
		if ((ilRC = HandleCommand((UINT)ilCurSec)) != RC_SUCCESS)
		{
			dbg(DEBUG,"<HandleData> HandleCommand returns %d", ilRC);
		}
		dbg(DEBUG,"<HandleData> ------ END SECTION %d ------", ilCurSec);
	}

	/* everything looks fine */
	return;
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
	char *pclFunc = "checkAndsetHopo";

	/*char pclHopoEvent[512] = "\0";*/

	if ( (pcpTwEnd == NULL) || (strlen(pcpTwEnd) == 0) )
	{
	    dbg(TRACE, "%s: Received pcpTwEnd = <%s> is null",pclFunc, pcpTwEnd);
		return RC_FAIL;
	}
	else
	{
		get_real_item(pcgHomeAP, pcpTwEnd, 1);
		if (strlen(pcgHomeAP) == 0)
		{
		    dbg(TRACE, "%s: Extracting pcpTwEnd = <%s> for received hopo fails",pclFunc, pcpTwEnd);
		    return RC_FAIL;
		}
	}

	if  ( strstr(pcpHopo_sgstab, pcgHomeAP) != 0 )
	{
		dbg(TRACE, "%s: Received pcpTwEnd = <%s> is not in SGS.TAB HOPO<%s>",
				   pclFunc, pcpTwEnd, pcpHopo_sgstab);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE, "%s: Received pcpTwEnd = <%s> is in SGS.TAB HOPO<%s>",
				   pclFunc, pcpTwEnd, pcpHopo_sgstab);
		return RC_SUCCESS;
	}
}

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

static int SendStatusWithHopo( char *pcpIfTyp, char *pcpIfName, char *pcpMsgTyp,
                int    ipMsgNumber, char *pcpData1, char *pcpData2,  char *pcpData3)
{
  char *pclFct   = "SendStatusWithHopo";
  char *pclTable = "STATAB";
  char *pclCommand = "STA";
  int    ilRC     = RC_SUCCESS;
  int    ilRCSend = RC_SUCCESS;
  int    ilToStaHdl = 0;
  int    ilMyOwnPid = 0;
  char  pclDestName[10]    = "";
  char  pclRecvName[10]    = "";
  char  pclTimestamp[16]   = "";
  char pclErrorBuffer[512] = "";
  int    ilStrgLen = 0;
  static char *pclSelBuf       = NULL;
  static int    ilSelBufSize   = 0;
  static char *pclFieldBuf     = NULL;
  static int    ilFieldBufSize = 0;
  static char *pclDataBuf      = NULL;
  static int    ilDataBufSize  = 0;

  ilToStaHdl = tool_get_q_id("stahdl");
  ilMyOwnPid = getpid();  /* Get My own Pid */

  if(ilToStaHdl > 0)
  {
    strcpy(pclDestName,mod_name);
    strcpy(pclRecvName,"CEDA");
    ilRC = GetCedaTimestamp(&pclTimestamp[0],15,0);

    /* Selection Buffer */
    ilStrgLen = strlen(mod_name) + 8 + strlen(pcpIfName)+16;
    ilRC = HandleBuffer(&pclSelBuf,&ilSelBufSize,0,ilStrgLen+1,64);
    /* mod_name,mod_id,Interfacetyp,Interfacename,MsgTyp */
    sprintf(pclSelBuf,"%s,%d,%s,%s,%s",mod_name,mod_id,
                                       pcpIfTyp,pcpIfName,pcpMsgTyp);

    /* Field Buffer */
    ilStrgLen = strlen(pclRecvName) + 6 + strlen(pclTimestamp) + 4;
    ilRC = HandleBuffer(&pclFieldBuf,&ilFieldBufSize,0,ilStrgLen+1,64);
    sprintf(pclFieldBuf,"%s,%d,%s,%d",pclRecvName,ilMyOwnPid,pclTimestamp,
                                      ipMsgNumber);

    /* Data Buffer */
    ilStrgLen = strlen(pcpData1) + strlen(pcpData2) + strlen(pcpData3);
    ilRC = HandleBuffer(&pclDataBuf,&ilDataBufSize,0,ilStrgLen+1,64);
    sprintf(pclDataBuf,"%s,%s,%s",pcpData1,pcpData2,pcpData3);


    ilRCSend = SendCedaEvent (ilToStaHdl, 0, pclDestName, pclRecvName,
                              pclTimestamp, pcgTwEnd, pclCommand,
                              pclTable, pclSelBuf, pclFieldBuf,
                              pclDataBuf, pclErrorBuffer, 0, ilRC);
    if(ilRCSend == RC_FAIL)
    {
      ilRC = RC_FAIL;
      dbg(TRACE,"%s: SendCedaEvent failed <%d>",pclFct,ilRCSend);
    } /* end if */
  } /* end if */


  dbg(DEBUG, "%s: ilRC = <%d>, pcpIfTyp = <%s>, pcpIfName = <%s>, pcpMsgTyp = <%s>, ipMsgNumber = <%d>",
                         pclFct, ilRC, pcpIfTyp, pcpIfName, pcpMsgTyp, ipMsgNumber);
  return ilRC;

} /* end of SendStatusWithHopo() */

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
