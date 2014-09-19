#ifndef _DEF_mks_version_dummy_h
  #define _DEF_mks_version_dummy_h
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char mks_version_dummy_h[]   MARK_UNUSED = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Library/Inc/aftnlsr.h 5.1.0.1 2013/03/04 10:29:29CEST otr Exp  $";
#endif /* _DEF_mks_version */
/* ********************************************************************** */
/*  The Master include file.                                              */
/*                                                                        */
/*  Program       :                                                       */
/*                                                                        */
/*  Revision date :                                                       */
/*                                                                        */
/*  Author    	  :       OTR                                             */
/*                                                                        */
/*                                                                        */
/* ********************************************************************** */
/* ********************************************************************** */
/* ********************************************************************** */
/* ********************************************************************** */

#ifndef __AFTNLSR_H
#define __AFTNLSR_H

/* defines for buffer sizes */
#define VXS_BUFF  32
#define XXS_BUFF  64
#define XS_BUFF  128
#define VS_BUFF  256
#define S_BUFF   512
#define M_BUFF   1024
#define L_BUFF   2048
#define XL_BUFF  4096
#define XXL_BUFF 8192
#define XXXL_BUFF 16384
#define HXL_BUFF 32768

#define PAGESEP		'|'
#define FIELDSEP	','
#define LINESEP		'\n'

#define READER    0
#define WRITER    1

typedef struct BUFFREC
{
	char BuffStr[S_BUFF];
} BuffRec;
typedef struct STRARRAY
{
	int NoOfRecord;
	BuffRec *prStrRec;
} StrArray;

struct MsgBuf
{
    char * data;
    struct MsgBuf  * next;
};
typedef struct MsgBuf MSGBUF;

typedef struct TelexMsg
{
    char*     heading;                   //pointer to after SOH
    char      transId[16];               //E.g. YFA0001
    char      addservId[16];             //DDHHMM. E.g. 271420
    char*     address;                   //pointer to after CR/LF
    char      prioId[8];                 //E.g. FF, GG, etc
    char*     addresseeId;               //malloc mem to E.g. EPWAZAZX EPWAZTZX.
    char*     origin;                    //pointer to after CR/LF
    char      filingtime[8];             //DDHHMM. E.g. 271421
    char      origId[16];                //E.g. EPWAYDYA
    char*     text;                      //pointer to after STX
} TELEXMSG;

typedef struct  MtMsgLog
{
    pthread_mutex_t mutex;
    int     month;
    int     day;
    char    filename[256];
    FILE*   fp;
    int     msgcount;                   //count for receive/send msg
}   MT_MSG_LOG;

#endif

