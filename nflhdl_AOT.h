#ifndef _DEF_mks_version_nflhdl_h
  #define _DEF_mks_version_nflhdl_h
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version_nflhdl_h[]   MARK_UNUSED = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Library/Inc/nflhdl.h 1.2a 2015/01/14 15:25:11SGT fya Exp  $";
#endif /* _DEF_mks_version */
/* ********************************************************************** */
/*  The Master include file.                                              */
/*									                                      */
/*  Program	  :     						                              */
/*  Revision date :							                              */
/*  Author    	  :							                              */
/*  									                                  */
/*  NOTE : This should be the only include file for your program          */
/* 20150114 FYA: v1.2a UFIS-8311 HOPO implementation*/
/* ********************************************************************** */
#ifndef _NFLHDL_H
#define _NFLHDL_H


#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

#include "ugccsma.h"
#include "msgno.h"
#include "glbdef.h"
#include "quedef.h"
#include "uevent.h"
#include "tools.h"
#include "helpful.h"
#include "ctxhdl.h"
#include "debugrec.h"
#include "new_catch.h"
#include "hsbsub.h"

#define	sDEFAULT_REMOVE		"NO"
#define	iDEFAULT_REMOVE		0
#define	sDEFAULT_CTRLFILE		"./nflhdl.ctrl"
#define	iREMOTE_EVENT_DATA	100

typedef struct _tnfsection
{
	char			pcTableExtension[iMIN]; /* the table extension */
	/*FYA v1.2a UFIS-8311*/
	/*char			pcHomeAirport[iMIN];*/ /* the home airport */
	char			pcHomeAirport[iMAX_BUF_SIZE]; /* the home airport */
	int			    iUseSelectString; /* only a flag */
	char			pcSelectString[iMAX_BUF_SIZE]; /* written to selection */
	int			    iUseArrivalFields; /* only a flag */
	char			pcArrivalFields[iMAX_BUF_SIZE];	/* all arrival fields */
	int			    iUseDepartureFields; /* only a flag */
	char			pcDepartureFields[iMAX_BUF_SIZE];	/* all departure fields */
	UINT			iRenameFilename; /* renaming filename-flag */
	char			pcRenameFilename[iMIN_BUF_SIZE]; /* new filename */
	UINT			iAddTimeStampToFilename; /* describe itself */
	char			pcTimeStampFormat[iMIN]; /* the format */
	UINT			iRenameExtensionType; /* rename Extension type flag */
	UINT			iRenameExtension; /* rename flag */
	char			pcRenameExtension[iMIN_BUF_SIZE]; /* the format string */
	UINT			iUseFtp;	 /* should i use FTP */
	char			pcFtpPass[iMIN_BUF_SIZE]; /* ftp password */
	char			pcFtpUser[iMIN_BUF_SIZE]; /* ftp user */
	char			pcFtpFileName[iMIN_BUF_SIZE]; /* filename at machine */
	UINT			iDeleteFtpFile; /* should i delete file on remote machine? */
	char			pcMachine[iMIN_BUF_SIZE]; /* machine name */
	char			pcMachinePath[iMIN_BUF_SIZE]; /* path at machine */
	char			pcCommand[iMIN_BUF_SIZE];  /* recognized command */
	UINT			iUseMoveTo;  /* should i move file? */
	char			pcMoveTo[iMIN_BUF_SIZE]; /* path for moving */
	char			pcFilePath[iMIN_BUF_SIZE]; /* scanned path */
	char			pcFileName[iMIN_BUF_SIZE]; /* search file(s) */
	char			pcDestName[iMIN]; /* dest_name in bc-head structure */
	char			pcRecvName[iMIN]; /* recv_name in bc-head structure */
	UINT			iRemove; /* delete file(s) ? */
	int			    iCtxTextType; /* only foe CTXHDL */
	int			    iCtxResultCode; /* only form CTXHDL */
	UINT			iModID;  /* mod_id */
	char			pcSendCommand[iMIN_BUF_SIZE];  /* command for sending */
} TNFSection;

typedef struct _tnfmain
{
	UINT			iNoOfSection; /* how many sections */
	TNFSection	*prSection;  /* the sections */
} TNFMain;

#endif	/* _NFLHDL_H */

