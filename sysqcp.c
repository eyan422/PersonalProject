/****************************************************************************/
/****************************************************************************/
/***************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/* ******************************************************************** */
/*									*/
/* This file contains the UGCCS Queue Control Program			*/
/*									*/
/* The Queue Control program is the interface between all modules in	*/
/* the UGCCS environment. Queuing is done via the UNIX message		*/
/* facility, using one queue as the input queue for this program, and	*/
/* a common output queue for all modules. A priority scheme for	the	*/
/* input queue is implemented using message types. The message type is	*/
/* also used on the output queue but here the type identifies the	*/
/* receiving routine. All queuing is done using a route ID, which the	*/
/* Queue Control program uses as the index into a table holding	the	*/
/* identifiers ( message types ) of the receiving routines. The	actual	*/
/* queues are held in memory accessible only by the QCP. Queue accesses	*/
/* are done via the queuing primitive implemented in the file queprim.c	*/
/*									*/
/* UPDATE HISTORY				*/
/* 30/06/00 SDC-BKK 	Adjusted dealoc_queue function to remove the last */
/*  			Pending package from the message facility by using */
/*			one msgrcv call within the check_msgtype() function */
/*			for the queue that is about to be deleted */
/*			Please see dealoc_queue function for more details */
/*                  	Special command for Release Control please adjust */
/*			the version control screen accordingly  */
/* 11/07/00 SDC-BKK 	see direct comment marked with 11.07.2000 */
/*			further info see doc.: SYSQCP_000718.doc				*/ 
/* 20011203: - in case of shutdown send SHUTDOWN after HSB_DOWN */
/*           - read shutdown timeout from environment SDTIMOUT to sync */
/*             with sysmon */
/*           - sccs_... string with compile date and time stamp */
/* 20020719: MCU break inserted in move_secondary que to avoid loosing  */
/*             messages of different prio */
/* 20020726: JIM: default debug_level = 0  */
/* */
/* ******************************************************************** */
/*#define XENIX_PROTO*/

static char sccs_sysqcp[]="@(#) UFIS 4.4 (c) ABB AAT/I sysqcp.c 44.3 / "__DATE__" "__TIME__" / VBL";

#define		UGCCS_PRG
#define 	U_MAIN				/* This is a main program   */
#define 	QUE_INTERNAL			/* Use internal definitions */
#define 	STH_USE		1		/* Use system table handler */

/*#define DESTROY*/				/* Use destroy queue program*/

#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>

#ifdef _ODT5
	#include <prototypes.h>
#endif

#define	TRACE		0x1000
#define	DEBUG		0x1001

#include "pntdef.h"
#include "rodef.h"
#include "glbdef.h"
#include "ugccsma.h"
#include "quedef.h"
#include "uevent.h"
#include "hsbsub.h"
#include "msgno.h"
#include "tools.h"

#ifdef UFIS43
/*#include "send_tools.h"*/
#include "nmghdl.h"
#else
	#include "msghdef.h"
	#include "msghdl.h"
#endif

#define LENGTH_OF_LINE 17
#define MAX_CTRL 19
#define STATE_LENGTH 20

#define TIMER_OFFSET			10 /* seconds for next check_queues */
#define QUE_STATUS			-112
#define TERMINATE				-111
#define QUE_TEXT(a) ((a)==qcp_output?"QCP-OUTPUT":((a)==qcp_input?"QCP-INPUT":((a)==qcp_attach?"QCP-ATTACH":"NOT A VALID IDENTIFIER")))
#define QUE_TYPE(a) ((a)==qcp_output?0:((a)==qcp_input?1:((a)==qcp_attach?2:-1)))

struct save_items
{
	char	mtext[I_SIZE+32];
	/* to be extended */
};
static struct save_items	*prgSaveItem[3];
static int						igSaveItemCnt[3];
static int						igCurItemCnt[3];

/* The global variables */
static EVENT		*event = NULL;		/* The event message buffer */
static ITEM			*item;				/* The input message buffer */
static RCB			*routes;				/* The route control block  */
static QTAB			*queues;				/* The queue table	    */
static Q_ENTRY		*mind;				/* Last entry		    */
static RETRY		retries;				/* The retry table	    */
static PNTREC     *pntrec;				/* Process name table	    */
static ROREC      *rorec;				/* Route table		    */

#define TIMESCALE	20
static long			stat_time;
static int			nbr_msgs;			/* number of messages	    */
static int			diff;					/* time difference	    */
static int			prio;					/* priority - 1		    */
static int			total[5];			/* total number of items    */
static int			first_flag;			/* 0: first item is ok
						   						1: first ptr is 0 or
						      						isn't an item	    */

static int			break_out;			/* The terminate flag	    */
extern int			errno;				/* The global error no.     */
static int			dyn_que_ID = 20000;	/* For dynamic queues	    */
static int			item_ack;			/* Flag to control ACKs     */
static int			act_msgh;			/* Avoid recursive to_msgh  */

static	int		qcp_input;			/* The QCP input que ID	    */
static	int		qcp_attach;			/* The QCP attach que ID    */
static	int		qcp_output;			/* The QCP output que ID    */
static  long		timer;				/* timer for check queue    */
static  long		current_time;		/* actual time		    */

static	int		que_ctrl = 0;		/* Que-Control flag	    */
static	int		ack_appear[MAX_Q_IDS];	/* Fault appearance memory */

/*** fuer neuen message handler ***/
#define iTMPBUF_SIZE		256			/* is this enough ? SUCCESS : core */
static char pcgTmpBuf[iTMPBUF_SIZE];

static  int    	shutdown_timeout = 90;
static  int       restart_timeout = 3;
static  char    	cgRestartScript[1024];


#define	SPARE "   "

/* external routines */
extern void			snap(char *, int, FILE *);
extern long			nap(long);

/* General routines */
static int			init_QCP(void);		/* Init of the QCP	    */
static void			rep_sys_error(char *);	/* Report last sys error    */
static void			rep_int_error(int, char *);	/* Report internal error    */
static int			to_msgh(int,int,int,int,char *); /* Forwards a message
																		to the message handler
																	*/
static int			clean_up(void);		/* Orderly termination	    */
static int			send_status(int);		/* Sys. state to processes  */
static int			write_status(int ipStatus);
static int			write_sts_shm(int ipStatus,char *pcpShmKey);
static int			unknown_function(int);	/* Handle unknown func.     */
static void			signal_catch(int);	/* Set the signals to catch */
static void			handle_signal();		/* Signal handling routine  */
static int			compare_IDs();			/* A compare for qsort	    */

/* Queue handling routines */
static int			init_static_queues();	/* Initialize static queues */
static int			aloc_queue(int,char *);	/* Allocate a queue	    */
static int			request_queue(void);	/* Request a queue (attach) */
static int			dealoc_queue(int,int);	/* Deallocate a queue	    */
static QCB			*find_queue(int);	/* Find entry in QTAB	    */
static int			put_item(int);		/* Place item on queue(s)   */
static int			add_to_que(int, int);	/* Do the actual queuing    */
static int			forward_item(QCB *, ITEM *); /* Forwards an item to
																  the destination routine     
																*/
static int			ack_item();		/* Remove item from queue   */
static int			send_item(QCB *);	/* Send item of highest prio*/
static int			clear_queue(void);	/* Remove all items	    */
static void			que_clear(QCB *);	/* Remove all items	    */
static int			que_status(int,int);	/* Change queue status	    */
static int			que_rebuild(QCB *);	/* Rebuild queue	    */
static int			back_search(QCB *,int); /* Backward search	    */
static int			comp_item(QCB *,int);	/* Comparison of items	    */
static int			check_queues(void);	/* Check for timeouts	    */
static int			get_q_over(void);	/* Get queue overview data  */
static int			get_q_info(QUE_SEND *);	/* Get queue display data   */
static int			check_msgtype(int, char *);	/* Chk if msgtype is on que */
static int			check_retry(void);	/* Check for retries	    */
static void			send_ack(int);		/* Send ACK		    */
static void			send_item_ack(int, ITEM *);		/* Send ACK		    */
static int			check_uutil(int);	/* Check if uutil is orig   */
static int			get_one_q_over();

#ifdef DESTROY
/*Destroys a queue and calls que_rebuild*/
static int			destroy_que(int,int); 
#endif

/* Route handling routines */
static int			aloc_route(int,int);	/* Allocate a route	   */
static int			dealoc_route(int,int);	/* Deallocate a route	   */
static ROUTE		*find_route(int);	/* Find a route in RCB	   */
static int			add_dest(int,int,int);	/* Add a dest. entry	   */
static int			del_dest(int,int,int);	/* Delete a dest. entry    */
static int			get_r_over(void);	/* Get route overview data */
static int			get_r_info(int);	/* Get route display data  */
static int 			StoreQueueItems(int);
static int 			GetStoredItems(int, struct msgbuf **);
static int 			ReleaseMemory(void);

/* Load-sharing routines */

static QCB			*find_empty_secondary(int); /* Find empty queue */
static void			move_to_secondary(QCB *);   /* Move from pri to sec */

/* SHM routines */

static void 			drop_SHM_item(ITEM *);	/* Drops the SHM segment */

/* Protocol routines */

static void 			handle_next_protocol(QCB *psQcb, ITEM *psItem);


/* ******************************************************************** */
/*									*/
/* Here is the beginning of the main program "QCP.c"			*/
/*									*/
/* ******************************************************************** */
FILE *outp;
int	debug_level=DEBUG;
#define DEBUG_RUNTIME (0) /* use this define to set the default mode */

MAIN
{
	int		ilCnt;
	int     	ilRC = RC_SUCCESS;
	int		action = 0;         /* Scm action command           */
	int		origin = 0;         /* Scm action command           */
	char		l_hsb[STATE_LENGTH];
	char		r_hsb[STATE_LENGTH];
	char		rem_alias[LENGTH_OF_LINE];
	char		enter[MAX_CTRL];
	char		hsbdat[256];
	char		clSection[256];
	char		clKeyword[256];
	char		clConfigFile[1024];
	char    	clRestartCommand[1024];
	int		orig;		/* The msg originator		*/
	int		queID;		/* Queue ID for manipulations	*/
	int		routeID;	/* Route ID for manipulations	*/
	QUE_SEND	*send;		/* A QUE_SEND pointer		*/
	int		ret;		/* return value			*/
	int		uutil_flag;	/* 1: call from uutil		0: intern call		*/
	int      restart_flag = 0;
	int		ilUseEvent;
	int		ilIsStoredItem;
	ITEM		*prlItem;
	QCB		*psTempQcb;	/* A temp pointer to a QCB */
  char *lpcShutDownTimeOut= NULL;

	/* general init */
	INITIALIZE;

	/* for recovery */
	igSaveItemCnt[0] 	= 0;
	igSaveItemCnt[1] 	= 0;
	igSaveItemCnt[2] 	= 0;
	igCurItemCnt[0] 	= 0;
	igCurItemCnt[1] 	= 0;
	igCurItemCnt[2] 	= 0;
	prgSaveItem[0] 	= NULL;
	prgSaveItem[1] 	= NULL;
	prgSaveItem[2] 	= NULL;

	debug_level = TRACE;
	dbg(TRACE,"main: initializing ...");
	dbg(TRACE,"main: version: <%s>",sccs_sysqcp);

	if ((ilRC = write_sts_shm(ctrl_sta,"_STATUS_")) != RC_SUCCESS)
	{
		dbg(TRACE,"<MAIN> %05d write_sts_shm returns: %d", __LINE__, ilRC);
		exit(1);
	}

	sprintf(clConfigFile,"%s/hsb.dat",getenv("CFG_PATH"));

  lpcShutDownTimeOut= getenv("SDTIMOUT");
  shutdown_timeout = 90;
  if (lpcShutDownTimeOut == NULL)
  {
     dbg(TRACE,"main: shutdown timeout SDTIMOUT not set in environment");
     if ((ilRC = iGetConfigEntry(clConfigFile,"main","SDTIMOUT",CFG_INT,(char *)&shutdown_timeout)) != RC_SUCCESS)
     {
       dbg(TRACE,"main: shutdown timeout SDTIMOUT not not found in %s",clConfigFile);
       shutdown_timeout = 90;
     } 
  }
  else
  {
     shutdown_timeout=atoi(lpcShutDownTimeOut);
  }

  if((shutdown_timeout < 10) || (shutdown_timeout > 180))
	{
		dbg(TRACE,"main: SDTIMOUT must be between 10 and 180 seconds");
		shutdown_timeout = 90;
		ilRC = RC_SUCCESS;
	}
	dbg(TRACE,"main: using <%d> sec as shutdown timeout",shutdown_timeout);

	if(ilRC == RC_SUCCESS)
	{
		if ((ilRC=iGetConfigEntry(clConfigFile,"main","RSTTOUT",CFG_INT,(char *)&restart_timeout)) != RC_SUCCESS)
		{
			restart_timeout = 0;
			ilRC = RC_SUCCESS;
		}
		else
		{
			if(restart_timeout > 0)
			{
				if ((ilRC=iGetConfigEntry(clConfigFile,"main","RSTSCR",CFG_STRING,(char *)&cgRestartScript)) != RC_SUCCESS)
				{
					restart_timeout = 0;
					ilRC = RC_SUCCESS;
				}
				else
				{
					dbg(TRACE,"main: using <%d> min as restart timeout",restart_timeout);
					dbg(TRACE,"main: using <%s> as restart script",cgRestartScript);
				}
			}
		}
	}

	if ((ilRC = iGetConfigEntry(clConfigFile,"main","QCPDBG",CFG_STRING,pcgTmpBuf)) == RC_SUCCESS)
	{ /* string QCPDBG found, check for OFF, TRACE or DEBUG, else do nothing */
    dbg(TRACE,"<MAIN> QCPDBG: %s",pcgTmpBuf);
    if (strstr(pcgTmpBuf,"OFF") != NULL)
    {
  	   dbg(TRACE,"<MAIN> LOGFILE set to OFF mode");
		   debug_level = 0;
	  } 
	  else if (strstr(pcgTmpBuf,"TRACE") != NULL)
    {
   	   dbg(TRACE,"<MAIN> LOGFILE set to TRACE mode");
		   debug_level = TRACE;
	  } 
	  else if (strstr(pcgTmpBuf,"DEBUG") != NULL)
    {
  	   dbg(TRACE,"<MAIN> LOGFILE set to FULL mode");
		   debug_level = DEBUG;
	  } 
	}
  else
  {
    /* in error case from config tell now the debug_level (in OFF mode do nothing) */
    if (DEBUG_RUNTIME == 0)
    {
    	dbg(TRACE,"<MAIN> LOGFILE set to OFF mode");
    }
    else if (DEBUG_RUNTIME == TRACE)
    {
    	dbg(TRACE,"<MAIN> LOGFILE set to TRACE mode");
    }
    else if (DEBUG_RUNTIME == DEBUG)
    {
    	dbg(TRACE,"<MAIN> LOGFILE set to FULL mode");
    }
		debug_level = DEBUG_RUNTIME;
  }
	ilRC == RC_SUCCESS;

	/* clear some buffers */
	memset(enter,0x0,MAX_CTRL);
	memset(rem_alias,0x0,LENGTH_OF_LINE);
	memset(l_hsb,0x0,STATE_LENGTH);
	memset(r_hsb,0x0,STATE_LENGTH);

	break_out = 0;
	nbr_msgs = 0;
	stat_time = time((long *) 0);

	if ((ilRC = init_QCP()) != RC_SUCCESS) 
	{
		/* Error during init	*/
		dbg(TRACE,"<MAIN> %05d init_QCP returns: %d", __LINE__, ilRC);
		dbg(TRACE,"<MAIN> %05d stop working now", __LINE__);
		exit(1);			
	} 
	dbg(TRACE,"main: init OK");

	/* Loop here forever, until termination signal is received	*/
	while (break_out == NUL) 
	{
		ilIsStoredItem = FALSE;
		/* first check stored items... */
		if (igSaveItemCnt[QUE_TYPE(qcp_input)] > 0)
		{
			dbg(DEBUG,"<MAIN> GET STORED ITEMS");
			prlItem = NULL;
			if ((ilRC = GetStoredItems(qcp_input, (struct msgbuf**)&prlItem)) == TERMINATE)
			{
				dbg(TRACE,"<MAIN> %05d GetSavedItem() returns TERMINATE", __LINE__);
				break_out 	= 1;
			}
			else
			{
				/* 'normal' case */
				ilRC = RC_SUCCESS;
				ilIsStoredItem = TRUE;
				DebugPrintItem(DEBUG,(ITEM*)prlItem);

				if (prlItem->function 	== 0 && 
					 prlItem->route 		== 0 &&
					 prlItem->priority 	== 0 && 
					 prlItem->msg_length == 0 &&
					 prlItem->originator == 0)
				{
					/* whats that? */
					/* ignore this item, because data isnt valid */
					dbg(TRACE,"<MAIN> %05d ignore this item...", __LINE__);
					continue;
				}

				/* copy to 'normal' data area */
				memcpy((void*)item, (void*)prlItem, (I_SIZE+sizeof(long)));
			}
		}
		else
		{
			errno = 0;
			dbg(DEBUG,"<MAIN> 'normal' MSGRCV");
			if ((ilRC = msgrcv(qcp_input,item,I_SIZE,-5,~IPC_NOWAIT)) < RC_SUCCESS) 
			{
				/* EINTR -> return due the receipt of a signal */
				if (errno != EINTR) 
				{
					dbg(TRACE,"<MAIN> %05d msgrcv returns %d", __LINE__, ilRC);
					dbg(TRACE,"<MAIN> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
					/* this is not a signal */
					rep_sys_error("MAIN");
					break_out = 1;
				} 
			}
dbg(TRACE,"%05d:item->function <%d>",__LINE__,item->function);
		}

		/* only for a valid item */
		if (ilRC >= RC_SUCCESS)
		{
		     /* Message receive successful ! */
			nbr_msgs++;
			current_time = time((long*)0);
			diff = (int) (current_time - stat_time);
			if (diff > 1200) 
			{
				dbg(DEBUG,"<MAIN> %05d xxx Messages switched in 20 minutes", __LINE__);
#ifdef UFIS43
				/* here the message "xxx Messages switched in 20 minutes" 
					will be created 
				*/
				memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
				sprintf(pcgTmpBuf,"%d",nbr_msgs);
				if (to_msgh(IPRIO_DB,QCP_MSGSTAT,0,0,pcgTmpBuf) == TERMINATE)
				{
					break_out = 1;
				}
#else
				if (to_msgh(0,QCP_MSGSTAT,0,nbr_msgs,0) == TERMINATE)
				{
					break_out = 1;
				}
#endif
				nbr_msgs = 0;
				stat_time = current_time;
			}

			/* ACK is default */
			ret = item_ack = uutil_flag = NUL;

			if (break_out == 0)
			{
				orig  = item->originator;
				if (orig == NUL) 
				{
					item->function = INV_ORG;
				}

				/* Route holds the queue ID */
				queID = item->route;	

				/* when manipulating queues */
				switch (item->function) 
				{
					case QUE_SHM:
					case QUE_PUT:
						item_ack = 1; 
						if ((ret = put_item(BACK)) != RC_SUCCESS)
						{
							/* Before we drop the message check if SHM */
							
							if (item->function == QUE_SHM)
							{
								drop_SHM_item(item);
							} /* end if */
							
							dbg(TRACE,"<MAIN> %05d put_item(BACK) returns %d", __LINE__, ret);
							if (ret == TERMINATE)
								break_out = 1;
						}
						break;

					case QUE_PUT_FRONT:
						item_ack = 1; 
						if ((ret = put_item(FRONT)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d put_item(FRONT) returns %d", __LINE__, ret);
							if (ret == TERMINATE)
								break_out = 1;
						}
						item->originator = orig;
						break;

					case QUE_ACK:
						item_ack = 1;
						if ((ret = ack_item()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d ack_item() returns %d", __LINE__, ret);
							if (ret == TERMINATE)
								break_out = 1;
						}
						break;

					case QUE_CREATE:
						if ((ret = request_queue()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d request_queue() returns: %d", __LINE__, ret);
						}
						break;

					case QUE_DELETE:
						uutil_flag = check_uutil(orig);
						if ((ret = dealoc_queue(queID,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d dealoc_queue(%d, %d) returns: %d", __LINE__, queID, uutil_flag, ret);
						}
						break;

					case QUE_EMPTY:
						if ((ret = clear_queue()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d clear_queue() returns: %d", __LINE__, ret);
						}
						break;
						
					case QUE_GO:
						uutil_flag = check_uutil(orig);
						if ((ret = que_status(GO, uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d que_status(GO,%d) returns: %d", __LINE__, uutil_flag, ret);
						}
						break;

					case QUE_HOLD:
						uutil_flag = check_uutil(orig);
						if ((ret = que_status(HOLD,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d que_status(HOLD,%d) returns: %d", __LINE__, uutil_flag, ret);
						}
						break;

					case QUE_WAKE_UP:
						item_ack = 1; 
						if ((ret = check_queues()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d check_queues() returns: %d", __LINE__, ret);
							if (ret == TERMINATE)
								break_out = 1;
						}
						break;

					case QUE_O_QOVER:
						if ((ret = get_one_q_over()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d get_one_q_over() returns: %d", __LINE__, ret);
						}
						break;

					case QUE_QOVER:
						if ((ret = get_q_over()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d get_q_over() returns: %d", __LINE__, ret);
						}
						break;

					case QUE_QDISP:
						send = (QUE_SEND *)item->text;
						if ((ret = get_q_info(send)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d get_q_info() returns: %d", __LINE__, ret);
						}
						break;

					case QUE_ROVER:
						if ((ret = get_r_over()) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d get_r_over() returns: %d", __LINE__, ret);
						}
						break;

					case QUE_RDISP:
						if ((ret = get_r_info(item->route)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d get_r_info(%d) returns: %d", __LINE__, item->route, ret);
						}
						break;

					case QUE_AROUTE:
						uutil_flag = check_uutil(orig);
						routeID = item->route;
						if ((ret = aloc_route(routeID,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d aloc_route(%d, %d) returns: %d", __LINE__, routeID, uutil_flag, ret);
						}
						break;

					case QUE_DROUTE:
						uutil_flag = check_uutil(orig);
						routeID = item->route;
						if ((ret = dealoc_route(routeID,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d aloc_route(%d, %d) returns: %d", __LINE__, routeID, uutil_flag, ret);
						}
						break;

					case QUE_ARENTRY:
						uutil_flag = check_uutil(orig);
						routeID = item->route;
						queID = *((int*)item->text);
						if ((ret = add_dest(routeID,queID,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d add_dest(%d, %d, %d) returns: %d", __LINE__, routeID, queID, uutil_flag, ret);
						}
						break;

					case QUE_DRENTRY:
						uutil_flag = check_uutil(orig);
						routeID = item->route;
						queID = *((int*)item->text);
						if ((ret = del_dest(routeID,queID,uutil_flag)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d del_dest(%d, %d, %d) returns: %d", __LINE__, routeID, queID, uutil_flag, ret);
						}
						break;

#ifdef DESTROY
					case QUE_DESTROY:
						routeID = *((int*)item->text);
						dbg(DEBUG,"que_destroy route: <%d>",routeID);
						dbg(DEBUG,"            queID: <%d>",queID);
						if ((ret = destroy_que(queID,routeID)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d destroy_que(%d,%d) returns: %d", __LINE__, queID, routeID,,ret);
						}
						item_ack = 1; 
						break;
#endif

					case INV_ORG:
#ifdef UFIS43
						if (to_msgh(IPRIO_DB,QCP_INVORG,0,0,NULL) == TERMINATE)
						{
							break_out = 1;
						}
#else
						if (to_msgh(0,QCP_INVORG,0,0,0) == TERMINATE)
						{
							break_out = 1;
						}
#endif
						break;

					case QUE_HSBCMD:
						item_ack = 1; 
						ilUseEvent = TRUE;
						event = (EVENT*)item->text;
						action = event->command;
						origin = event->originator;

						sprintf(hsbdat,"%s/hsb.dat",getenv("CFG_PATH"));
						sprintf(clSection,"main");
						sprintf(clKeyword,"STATUS");

						memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
						switch (action) 
						{
							case HSB_STANDBY:
								strcpy(l_hsb,"STANDBY");
								strcpy(r_hsb,"ACTIVE");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_STANDBY",origin);
								break;

							case HSB_ACT_TO_SBY:
								strcpy(l_hsb,"ACT_TO_SBY");
								strcpy(r_hsb,"STANDBY");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_ACT_TO_SBY",origin);
								break;

							case HSB_ACTIVE:
								strcpy(l_hsb,"ACTIVE");
								strcpy(r_hsb,"STANDBY");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_ACTTIVE",origin);
								break;

							case HSB_DOWN:
								strcpy(l_hsb,"DOWN");
								strcpy(r_hsb,"STANDALONE");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_DOWN",origin);
								dbg(DEBUG,"main: send SIGTERM to sysmon <%d>",getppid());
								kill(getppid(),15);
								break;

							case HSB_STANDALONE:
								strcpy(l_hsb,"STANDALONE");
								strcpy(r_hsb,"DOWN");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_STANDALONE",origin);
								break;

							case HSB_COMING_UP:
								strcpy(l_hsb,"COMING_UP");
								strcpy(r_hsb,"STANDALONE");
								sprintf(pcgTmpBuf,"got <%s> from <%d>","HSB_COMING_UP",origin);
								break;

							default:
								dbg(TRACE,"<MAIN> %05d unknown HSB-Command received, ignore it...", __LINE__);
								ilUseEvent = FALSE;
								break;
						}

						/* we use only valid events (valid HSB-Commands) */
						if (ilUseEvent == TRUE)
						{
							/* this is enough to handle all HSB-Commands... */
							/* next writes status to shared memory */
							if ((ret = write_status(action)) != RC_SUCCESS)
							{
								dbg(TRACE,"<MAIN> %05d write_status returns: %d", __LINE__, ret);
							}
							else
							{
								/* this writes it to hsb.dat file */
								if ((ret = iWriteConfigEntry(hsbdat,clSection,clKeyword,CFG_STRING,l_hsb)) != RC_SUCCESS)
								{
									dbg(TRACE,"<MAIN> %05d iWriteConfigConfigEntry returns: %d", __LINE__, ret);
								}
								else
								{
#ifdef UFIS43
									if (to_msgh(IPRIO_DB,HSB_MAINMSG,0,0,pcgTmpBuf) == TERMINATE)
									{
										break_out = 1;
									}
#else
									if (to_msgh(0,HSB_MAINMSG,0,origin,pcgTmpBuf) == TERMINATE)
									{
										break_out = 1;
									}
#endif

									if (break_out == 0)
									{
										dbg(DEBUG,"main: send HSB_%s to all",l_hsb);
										/* this sends it to all CEDA-Processes */
										if ((ret = send_status(action)) != RC_SUCCESS)
										{
											dbg(TRACE,"<MAIN> %05d send_status returns: %d", __LINE__, ret);
										}
										else
										{
											ctrl_sta = action;
										}
									}
								}
							}
						}
						break;

					case QUE_RESTART :
						item_ack = 1;
						dbg(TRACE,"main: got HSB_RESTART from <%d>",orig);
						restart_flag = 1;
						strcpy(l_hsb,"DOWN");
						strcpy(r_hsb,"STANDALONE");
						if ((ret = write_status(HSB_DOWN)) != RC_SUCCESS)
						{
							dbg(TRACE,"<MAIN> %05d write_status returns: %d", __LINE__, ret);
						}
						else
						{
							if ((ret = iWriteConfigEntry(hsbdat,clSection,clKeyword,CFG_STRING,l_hsb)) != RC_SUCCESS)
							{
								dbg(TRACE,"<MAIN> %05d iWriteConfigConfigEntry returns: %d", __LINE__, ret);
							}
							else
							{
								memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
								sprintf(pcgTmpBuf,"got <%s> from <%d>","QUE_RESTART",origin);
#ifdef UFIS43
								if (to_msgh(IPRIO_DB,HSB_MAINMSG,0,0,pcgTmpBuf) == TERMINATE)
								{
									break_out = 1;
								}
#else
								if (to_msgh(0,HSB_MAINMSG,0,origin,"HSB_RESTART") == TERMINATE)
								{
									break_out = 1;
								}
#endif
								dbg(DEBUG,"main: send SIGTERM to sysmon <%d>",getppid());
								kill(getppid(),15);
								ctrl_sta = HSB_DOWN;
							}
						}
						break;
					case QUE_NEXT :
					case QUE_NEXTNW :
						item_ack = 0;
						psTempQcb = find_queue(orig);
						if (psTempQcb != (QCB *) NULL)
						{
							handle_next_protocol(psTempQcb, item);	
						} /* end if (QCB found) */
						
						break;
					default:
						if (unknown_function(item->function) == TERMINATE)
						{
							break_out = 1;
						}
						ret = RC_FAIL; 
						break;
				} /* end switch */
			}
			
			/* Send ACK back if needed */
			if (ilIsStoredItem == FALSE)
				if (item_ack == 1) 
				{
					send_ack(ret);
					}

			/* only for debuging */
			if (ret != RC_SUCCESS) 
				rep_int_error(ret, "MAIN");
			
			if (current_time >= timer) 
			{
				if ((ret = check_queues()) != RC_SUCCESS)
				{
					dbg(TRACE,"<MAIN> %05d check_queues() returns: %d", __LINE__, ret);
				}
				timer = current_time + TIMER_OFFSET;
			}

			/* must i delete memory? */
			if ((ilRC = ReleaseMemory()) != RC_SUCCESS)
			{
				dbg(TRACE,"<MAIN> %05d ReleaseMemory() returns with an error", __LINE__);
			}
		}

		/* Check for forward retries */
		if (break_out == 0)
		{
			if ((ilRC = check_retry()) == TERMINATE)
			{
				dbg(TRACE,"<MAIN> %05d check_retry() returns TERMINATE", __LINE__);
				break_out = 1;
			}
		}
	}

	ilCnt = 0;
	do
	{
		if ((ilRC = clean_up()) != RC_SUCCESS)
		{
			ilCnt++;
			sleep(10);
		}
	} while (ilRC != RC_SUCCESS && ilCnt < 3);

	if (ilRC != RC_SUCCESS)
	{
		/* fatal error in this case, but what to do?, 
			ignore it? i don't know
			so its better to use the automatic restart (via restart script) only
			if clean_up returns with RC_SUCCESS
		*/
		dbg(TRACE,"<MAIN> %05d clean_up() returns with: %d", __LINE__, ilRC);
		dbg(TRACE,"<MAIN> %05d ignore restart script now...", __LINE__);
	}
	else
	{
		if (restart_flag == 1)
		{
			dbg(TRACE,"main: restart_flag is set");
			if(restart_timeout > 0)
			{
				sprintf(clRestartCommand,"at now + %d minute < %s",restart_timeout,cgRestartScript);

				dbg(TRACE,"main: calling <%s>",clRestartCommand);
				ret = system(clRestartCommand);
				dbg(TRACE,"main: system ret=<%d>",ret);
			}
			else
			{
				dbg(TRACE,"main: RSTTOUT not set!");
			}
		}
	}

	/* thats all */
	exit(0);
}

/* ******************************************************************** */
/*									*/
/* The following routine initializes the QCP				*/
/*									*/
/* ******************************************************************** */
static int init_QCP()
{
	int 						i;
	int						ilRC;
	struct msqid_ds		msq_ds;	/* Structure for msgctl		*/

	dbg(DEBUG,"<init_QCP> ---- START ----");
	/* Track execution		*/
	signal_catch(0);

	/* Allocate the route control block */
	if ((routes = (RCB*)calloc(1,sizeof(RCB))) == NULL)
	{
		dbg(TRACE,"<init_QCP> %05d can't allocate memory for route control block", __LINE__);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MEMORY;
	}

	routes->nbr_entries = NUL;
	routes->RCB_size = sizeof(RCB);
	routes->RCB_free = NUL;
	routes->route[0].route_ID = 0;

	/* Allocate the msg queues */
	if ((qcp_input = msgget(QCP_INPUT,IPC_CREAT | 0666)) < 0)
	{
		rep_sys_error("init_QCP");
		dbg(TRACE,"<init_QCP> %05d msgget returns: %d", __LINE__, qcp_input);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGGET;
	} 

	/* Set the queue to maxsize */
	if ((ilRC = msgctl(qcp_input,IPC_STAT,&msq_ds)) == 0) 
	{
		/* success */
		msq_ds.msg_qbytes = 65535;
		msgctl(qcp_input,IPC_SET,&msq_ds);
	}
	else
	{
		/* error case */
		dbg(TRACE,"<init_QCP> %05d msgctl returns: %d", __LINE__, ilRC);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGCTL;
	}

	if ((qcp_attach = msgget(QCP_ATTACH,IPC_CREAT | 0666)) < 0)
	{
		rep_sys_error("init_QCP");
		dbg(TRACE,"<init_QCP> %05d msgget returns: %d", __LINE__, qcp_input);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGGET;
	}

	/* Set the queue to maxsize */
	if ((ilRC = msgctl(qcp_attach,IPC_STAT,&msq_ds)) == 0) 
	{
		/* success */
		msq_ds.msg_qbytes = 65535;
		msgctl(qcp_attach,IPC_SET,&msq_ds);
	}
	else
	{
		/* error case */
		dbg(TRACE,"<init_QCP> %05d msgctl returns: %d", __LINE__, ilRC);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGCTL;
	}

	if ((qcp_output = msgget(QCP_GENERAL,IPC_CREAT | 0666)) < 0)
	{
		rep_sys_error("init_QCP");
		dbg(TRACE,"<init_QCP> %05d msgget returns: %d", __LINE__, qcp_input);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGGET;
	}

	/* Set the queue to maxsize */
	if ((ilRC = msgctl(qcp_output,IPC_STAT,&msq_ds)) == 0) 
	{
		/* success */
		msq_ds.msg_qbytes = 65535;
		msgctl(qcp_output,IPC_SET,&msq_ds);
	}
	else
	{
		/* error case */
		dbg(TRACE,"<init_QCP> %05d msgctl returns: %d", __LINE__, ilRC);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MSGCTL;
	}

	/* Allocate the QCP input buffer */
	if ((item = (ITEM *)calloc(1,(I_SIZE+sizeof(long)))) == NULL)
	{
		rep_sys_error("init_QCP");
		dbg(TRACE,"<init_QCP> %05d can't allocate memory for QCP input buffer", __LINE__);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MEMORY;
	}

	/* Allocate the queue table */
	if ((queues = (QTAB *) calloc(1,sizeof(QTAB))) == NULL)
	{
		rep_sys_error("init_QCP");
		dbg(TRACE,"<init_QCP> %05d can't allocate memory the queue table", __LINE__);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return QUE_E_MEMORY;
	}

	queues->nbr_queues = NUL;
	queues->QTAB_size = sizeof(QTAB);
	queues->QTAB_free = NUL;
	queues->queue[0].que_ID = NUL;

	/* Initialize the static queues */
	if ((ilRC = init_static_queues()) == TERMINATE)
	{
		dbg(TRACE,"<init_QCP> init_static_queues returns: %d", ilRC);
		dbg(DEBUG,"<init_QCP> ---- END ----");
		return TERMINATE;
	}
	
	/* set new time value */
	timer = time((long*)0) + TIMER_OFFSET;

	/* Initialize fault appearance memory */
	for(i=0;i<MAX_Q_IDS;i++)
	{
		ack_appear[i] = 0;
	}

	dbg(DEBUG,"<init_QCP> ---- END ----");
	/* everything looks fine... */
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine reports UNIX system errors			*/
/*									*/
/* ******************************************************************** */
static void	rep_sys_error(char *pcpProgress)
{
	dbg(TRACE,"rep_sys_error: err=<%d-%s> progress=<%s>",errno,strerror(errno),pcpProgress);
	DebugPrintItem(TRACE,item);
	return;
}

/* ******************************************************************** */
/*									*/
/* The following routine reports QCP internal errors			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static void rep_int_error(err, pcpProgress)
int err;
char *pcpProgress;
#else
static void	rep_int_error(int err, char *pcpProgress)
#endif

{
	dbg(TRACE,"rep_int_error: err=<%d> progress=<%s>",err, pcpProgress);
	DebugPrintItem(TRACE,item);
	return;
}

/* ******************************************************************** */
/*									*/
/* The following routine handle unknown functions			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	unknown_function(func)
int func;
#else
static int	unknown_function(int func)
#endif

{
	int	ilRC;

#ifdef UFIS43
	memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
	sprintf(pcgTmpBuf,"%d",func);
	ilRC = to_msgh(IPRIO_DB,QCP_INVFUNC,0,0,pcgTmpBuf);
#else
	ilRC = to_msgh(0,QCP_INVFUNC,0,func,0);
#endif
	DebugPrintItem(TRACE,item);
	snap((char*)item, sizeof(ITEM), outp);
	return ilRC;
}

/* ******************************************************************** */
/*									*/
/* The following routine performs the clean_up before a termination.	*/
/*									*/
/* ******************************************************************** */
static int clean_up()
{
	int		i;
	int		ilRC;
	int		len;
	ITEM		*buf;
	EVENT		*event;
	int		ret;
	que_ctrl = 0;			/* Disable Que-Control */

	dbg(DEBUG,"<clear_up> ---- START ----");
	if ((ilRC = write_status(HSB_DOWN)) != RC_SUCCESS)
	{
		dbg(TRACE,"<clean_up> %05d write_status returns: %d", __LINE__, ilRC);
		dbg(TRACE,"<clear_up> %05d can't write HSB_DOWN status to shared memory", __LINE__);
		dbg(DEBUG,"<clear_up> ---- END ----");
		return ilRC;
	}

	/* Set up the shutdown buffer */
	len = sizeof(ITEM) + sizeof(EVENT);
	if ((buf = (ITEM*)calloc(1,len)) == NULL)
	{
		dbg(TRACE,"<clean_up> %05d malloc failure...", __LINE__);
		dbg(TRACE,"<clear_up> %05d can't send HSB_DOWN message to CEDA", __LINE__);
		dbg(DEBUG,"<clear_up> ---- END ----");
		return QUE_E_MEMORY;
	}
	else
	{
		buf->function = 0;
		buf->route = 0;
		buf->priority = 1;
		buf->msg_length = sizeof(EVENT);
		buf->originator = 1;
		event = (EVENT *) buf->text;
		event->type = SYS_EVENT;
		event->command = HSB_DOWN;
		event->originator = 1;
		event->retry_count = 0;
		event->data_offset = 0;
		event->data_length = 0;

		dbg(TRACE,"clean_up: shutting down ...");

		sleep(1);

  	dbg(TRACE,"clean_up: sending HSB_DOWN ...");
		/* Send shutdowns and clear the queues */
		item->route = MSG_PRIMARY;
		que_status(HOLD,0);
		for (i = queues->nbr_queues; i > 0; i-- ) 
		{
			if (queues->queue[i].que_ID < 30000) 
			{
				buf->msg_header.mtype =	(long)queues->queue[i].que_ID;
				errno = 0;
				if ((ilRC = msgsnd(qcp_output,buf,len - sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
				{
					dbg(TRACE,"<clean_up> %05d msgsnd returns %d", __LINE__, ilRC);
					dbg(TRACE,"<clean_up> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
				}

				errno = 0;
				if ((ilRC = msgsnd(qcp_attach,buf,len - sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
				{
					dbg(TRACE,"<clean_up> %05d msgsnd returns %d", __LINE__, ilRC);
					dbg(TRACE,"<clean_up> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
				}
				dbg(TRACE,"clean_up: HSB_DOWN sent to <%d>",(long)queues->queue[i].que_ID);
			} 
		} 

    sleep(2);
		event->command = SHUTDOWN;
  	dbg(TRACE,"clean_up: sending SHUTDOWN ...");
		for (i = queues->nbr_queues; i > 0; i-- ) 
		{
			if (queues->queue[i].que_ID < 30000) 
			{
				buf->msg_header.mtype =	(long)queues->queue[i].que_ID;
				errno = 0;
				if ((ilRC = msgsnd(qcp_output,buf,len - sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
				{
					dbg(TRACE,"<clean_up> %05d msgsnd returns %d", __LINE__, ilRC);
					dbg(TRACE,"<clean_up> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
				}

				errno = 0;
				if ((ilRC = msgsnd(qcp_attach,buf,len - sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
				{
					dbg(TRACE,"<clean_up> %05d msgsnd returns %d", __LINE__, ilRC);
					dbg(TRACE,"<clean_up> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
				}
				dbg(TRACE,"clean_up: SHUTDOWN sent to <%d>",(long)queues->queue[i].que_ID);
			} 
			dealoc_queue(queues->queue[i].que_ID,0); 
		} 

		/* Free the QTAB and the RCB */
		free((char*)queues);
		free((char*)routes);

		/* Wait some seconds and then release the input/output queues */
		/* give everyone the chance to read the message */
		dbg(TRACE,"clean_up: waiting <%d> seconds",shutdown_timeout);
		sleep(shutdown_timeout);
		dbg(TRACE,"clean_up: removing queues");
		msgctl(qcp_input,IPC_RMID,0);
		msgctl(qcp_attach,IPC_RMID,0);
		msgctl(qcp_output,IPC_RMID,0);
	}

	dbg(DEBUG,"<clear_up> ---- END ----");
	/* ciao */
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine sets the signals to catch and to ignore.	*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static void	signal_catch(sig)
int sig;
#else
static void	signal_catch(int sig)
#endif
{
	dbg(DEBUG,"<signal_catch> ---- START ----");
	switch (sig) 
	{
		case SIGHUP:
			signal(SIGHUP,SIG_IGN);
			break;
		case SIGINT:
			signal(SIGINT,SIG_IGN);
			break;
		case SIGALRM:
			signal(SIGALRM,handle_signal); 
			break;
		case SIGTERM:
			signal(SIGTERM,handle_signal); 
			break;
		case SIGUSR1:
			signal(SIGUSR1,SIG_IGN);
			break;
		case SIGUSR2:
			signal(SIGUSR2,SIG_IGN);
			break;
		default :
			signal(SIGHUP,SIG_IGN);
			signal(SIGINT,SIG_IGN);
			signal(SIGALRM,handle_signal);
			signal(SIGTERM,handle_signal); 
			signal(SIGUSR1,SIG_IGN);
			signal(SIGUSR2,SIG_IGN);
			break;
	}
	dbg(DEBUG,"<signal_catch> ---- END ----");
	return;
} 

/* ******************************************************************** */
/*									*/
/* The following routine handles signals.				*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static void	handle_signal(sig)
int sig;
#else
static void	handle_signal(int sig)
#endif
{
	dbg(DEBUG,"<handle_signal> ---- START ----");
	dbg(TRACE,"handle_signal: signal received <%d>",sig);
	switch (sig) 
	{
		case SIGALRM:
			signal_catch(SIGALRM);
			break;

		case SIGTERM:
			signal(SIGTERM,handle_signal);
			break_out = 1;
			break;

		default :
			break;
	}
	dbg(DEBUG,"<handle_signal> ---- END ----");
	return;
} 

/* ******************************************************************** */
/*									*/
/* The following routine performs the comparison of queue IDs and of	*/
/* route IDs. The routine is called by qsort				*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	compare_IDs(ID1,ID2)
int *ID1;
int *ID2;
#else
static int	compare_IDs(const void *ID1,const void *ID2)
#endif

{
	int	return_value;

	if (*(int*)ID1 < *(int*)ID2) 
	{
		return_value = -1;
	} 
	else 
	{
		if (*(int*)ID1 == *(int*)ID2) 
		{
			return_value = 0;
		}
		else 
		{
			return_value = 1;
		}
	} 
	
	/* bye bye */
	return return_value;
}

/* ******************************************************************** */
/*									*/
/* This routine initializes the static queues. The queue IDs and the	*/
/* module names are read from the system tables.			*/
/*									*/
/* ******************************************************************** */
static int init_static_queues()

{
	int	ret;		/* return value */
	char	name[MAX_PRG_NAME+1];	/* taskname */
	int	i,j,k;

	dbg(DEBUG,"<init_static_queues> ---- START ----");

	pbcb->function = (STRECORD|STLOCATE);
	pbcb->inputbuffer = NULL;
	pbcb->outputbuffer = (char*)&pntrec;
	pbcb->tabno = PNTAB;

	for (i=0; i<psptrec->mrecno[PNTAB]; i++) 
	{
		pbcb->recno = i + 1;
		for (k=0; k<5; k++) 
		{
			ret = sth(pbcb,pgbladr);
			if (ret != STNORMAL) 
			{
				nap(500);
			}
			else 
			{
				break;
			}
		} 
		
		if (ret != STNORMAL) 
		{
			printf("STH STATUS : %u\n",ret);
#ifdef UFIS43
			memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
			sprintf(pcgTmpBuf,"%s;%d","PNTAB",pbcb->recno);
			if ((ret = to_msgh(IPRIO_DB,QCP_NOACCS,0,0,pcgTmpBuf)) == TERMINATE)
				return ret;
#else
			if ((ret = to_msgh(0,QCP_NOACCS,0,pbcb->recno,"PNTAB")) == TERMINATE)
				return ret;
#endif
			continue;
		}
		strncpy(&name[0],(char *)&pntrec->taskname[0],MAX_PRG_NAME);
		name[MAX_PRG_NAME] = 0x0;
		ret = aloc_queue(pntrec->process_no,&name[0]);
	} 

	pbcb->function = (STRECORD|STLOCATE);
	pbcb->inputbuffer = NULL;
	pbcb->outputbuffer = (char *)&rorec;
	pbcb->tabno = ROTAB;

	for (i=0; i<psptrec->mrecno[ROTAB]; i++ ) 
	{
		pbcb->recno = i+1;
		ret = sth(pbcb,pgbladr);
		if ( ret != STNORMAL ) 
		{
			memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
			sprintf(pcgTmpBuf,"%s;%d","ROTAB",pbcb->recno);
#ifdef UFIS43
			if ((ret = to_msgh(IPRIO_DB,QCP_NOACCS,0,0,pcgTmpBuf)) == TERMINATE)
			{
				dbg(TRACE,"<init_static_queues> %05d to_msgh returns: %d", __LINE__, ret);
				dbg(DEBUG,"<init_static_queues> ---- END ----");
				return ret;
			}
#else
			if ((ret = to_msgh(0,QCP_NOACCS,0,pbcb->recno,"ROTAB")) == TERMINATE)
			{
				dbg(TRACE,"<init_static_queues> %05d to_msgh returns: %d", __LINE__, ret);
				dbg(DEBUG,"<init_static_queues> ---- END ----");
				return ret;
			}
#endif
			continue;
		}

		aloc_route(rorec->route,0);
		for (j = 0; j <= 15; j++)
			if (rorec->queues[j] != 0) 
			{
				add_dest(rorec->route,rorec->queues[j],0);
				continue;
			} 
			else 
			{
				break;
			}
	} 
	dbg(DEBUG,"<init_static_queues> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine allocates a new queue.				*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	aloc_queue(que_ID,name)
int que_ID;
char *name;
#else
static int	aloc_queue(int que_ID,char *name)
#endif

{
	char	*mptr;			/* A work pointer for malloc */
	QCB	*newQCB;		/* A work pointer for new QCB */
	ROUTE	*rout;			/* A work pointer for new route */
	int     i;

	dbg(DEBUG,"<aloc_queue> ---- START ----");

	/* First check if queue is configured */
	if (find_queue(que_ID) != (QCB*)NOT_FOUND) 
	{
		/* Entry exists */
		dbg(TRACE,"<aloc_queue> %05d find_queue returns with an error", __LINE__);
		dbg(DEBUG,"<aloc_queue> ---- END ----");
		return QUE_E_EXISTS;
	} 

	/* Check if space available in the QTAB, if not, expand it */
	if (queues->QTAB_free < sizeof(QCB)) 
	{
		if ((mptr = calloc(1,((unsigned) (queues->QTAB_size + ( NBR_QUEUES * sizeof(QCB)))))) == NULL)
		{
			dbg(TRACE,"<aloc_queue> %05d can't allocate memory", __LINE__);
			dbg(DEBUG,"<aloc_queue> ---- END ----");
			return QUE_E_MEMORY;
		}

		memcpy(mptr,(char *) queues, queues->QTAB_size);
		free((char *) queues);
		queues = (QTAB *) mptr;
		queues->QTAB_size += sizeof(QCB) * NBR_QUEUES;
		queues->QTAB_free += sizeof(QCB) * NBR_QUEUES;
	} 

	/* Add the new queue entry */
	queues->nbr_queues++;
	newQCB = &(queues->queue[queues->nbr_queues]);
	newQCB->que_ID = que_ID;
	memset(newQCB->name,0x00,MAX_PRG_NAME+1);
	strncpy(newQCB->name,name,MAX_PRG_NAME);
	newQCB->que_type = QTYPE_NORMAL;
	newQCB->protocol = PROTOCOL_ACK;
	newQCB->primary_ID = 0;
	newQCB->timestamp = 0;
	newQCB->msg_count = 0;
	newQCB->total = 0;
	newQCB->status = GO;
	newQCB->ack_count = 0;
	for (i = 0; i < 5; i++) 
	{
		newQCB->qitem[i].nbr_items = NUL;
		newQCB->qitem[i].first = NUL;
		newQCB->qitem[i].last = NUL;
	}
	queues->QTAB_free -= sizeof(QCB);

	if(que_ID >= 20000)
	{
		/* Enable Que-Control */
		que_ctrl = 1;		
	}


	/* Allocate a route with same ID */
	
	if ((que_ID>=LOAD_SH_LOW) && (que_ID<=LOAD_SH_HIGH))
	{
		/* Not if it is a loadsharing queue, if so set type */
		
		newQCB->que_type = QTYPE_PRIMARY;
		dbg(TRACE,"<aloc_queue> %05d Loadsharing queue detected %05d", __LINE__,que_ID);

	} /* end if */
	
	else
	{	 
		aloc_route(que_ID,0);
		rout = find_route(que_ID);
		if (rout != (ROUTE *)NOT_FOUND) 
		{
			rout->nbr_entries = 1;
			rout->routine_ID[0] = que_ID;
		} /* end if */
	} /* end else */
	
	/* Sort the QTAB */
	qsort((char *) &(queues->queue[1]), queues->nbr_queues,sizeof(QCB), compare_IDs);
	
	dbg(DEBUG,"<aloc_queue> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine handles the queue build requests (via attach).	*/
/* The QCP allocates a queue ID ( in the range from 20000 to 30000 )	*/
/* and has queue and route entries build based on this ID. If the build	*/
/* is successful the ID is returned to the requesting routine via the	*/
/* ATTACH queue ( in item->route and item->originator ), if not an ID	*/
/* of 0 is returned.							*/
/*									*/
/* ******************************************************************** */
static int	request_queue()
{
	int	ilRC;
   int 	rc=42;
	int	requester;		/* The requesters PID */
	int	que_ID;			/* The queue ID */
	ITEM	buff;			/* An ITEM work buffer */
	int	return_value;

	dbg(DEBUG,"<request_queue> ---- START ----");

	if (item->route != 0) 
	{
		que_ID = item->route;
	}
	else 
	{
		while (rc != NOT_FOUND)
		{
		  dyn_que_ID++;
		  if (dyn_que_ID > 30000) 
		  {
			 /* Wrap around */
		    dyn_que_ID = 20000;	
		  } 
		  rc = (int)find_queue(dyn_que_ID);
		} 
		que_ID = dyn_que_ID;
	}

	requester = item->originator;

	buff.msg_header.mtype = (long) requester;
	return_value = aloc_queue(que_ID,item->text);
	if (return_value != RC_SUCCESS) 
	{
		buff.route = que_ID;
		buff.originator = NUL;
	}
	else 
	{
		buff.route = que_ID;
		buff.originator = que_ID;
	}

	/* Send response via the ATTACH queue (TWICE !!. First is an ACK) */

	buff.function=return_value;
	errno = 0;
	if ((ilRC = msgsnd(qcp_attach,&buff,sizeof(ITEM)-sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
	{
		dbg(TRACE,"<request_que> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<request_que> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
	}

	errno = 0;
	if ((ilRC = msgsnd(qcp_attach,&buff,sizeof(ITEM)-sizeof(long),IPC_NOWAIT)) != RC_SUCCESS)
	{
		dbg(TRACE,"<request_que> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<request_que> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
	}

	dbg(DEBUG,"<request_queue> ---- END ----");
	return return_value;
}

/* ******************************************************************** */
/*									*/
/* The following routine checks if UUTIL is the calling routine		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int check_uutil(orig)
int orig;
#else
static int check_uutil(int orig)
#endif
{
	QCB	*qcb;			/* A QCB work pointer		*/
	
	dbg(DEBUG,"<check_uutil> ---- START ----");

	qcb = find_queue(orig);
	if (qcb != (QCB *)NOT_FOUND) 
	{
		if ((strncmp(qcb->name,"UTIL",4)) == NUL) 
		{
			dbg(DEBUG,"<check_uutil> ---- END ----");
			return UUTIL;
		} 
		else 
		{
			dbg(DEBUG,"<check_uutil> ---- END ----");
			return OTHER;
		}
	} 
	dbg(DEBUG,"<check_uutil> ---- END ----");
	return 0;
}

/* ******************************************************************** */
/*									*/
/* The following routine deallocates a queue. Possible messages are	*/
/* removed from the queue and the associated route is deleted.		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	dealoc_queue(que_ID,uutil_flag)
int que_ID;
int uutil_flag;
#else
static int	dealoc_queue(int que_ID,int uutil_flag)
#endif
{
	QCB	*qcb;			/* A QCB work pointer		*/
	QCB	*next_qcb;		/* Another QCB work pointer	*/
	ITEM	buff;			/* Send Item buffer		*/
	ITEM	errbuff;		/* Send Item error buffer	*/
	int	move_len;		/* Length of data to move	*/
	int	len;			/* Length of send data		*/
	int	ilRC;
	long  ilOriginalSender;

	dbg(DEBUG,"<dealoc_queue> ---- START ----");

	/* First locate the queue */
	qcb = find_queue(que_ID);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = NOT_FOUND;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<dealoc_queue> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<dealoc_queue> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}

		dbg(DEBUG,"<dealoc_queue> ---- END ----");
		return QUE_E_NOFIND;
	} 

	/* The queue exists, empty it... */
	que_clear(qcb);

	/* NEW 30.06.2000 */
	/* Remove a possible msg in the UNIX msg facility */
	dbg(DEBUG,"Checking message type for %d check = %d",que_ID,item->originator);
	ilOriginalSender = item->originator;
	check_msgtype(que_ID, "clear_queue");
	/* in case we had a message we have to reset the originator */
	item->originator = ilOriginalSender;
	dbg(DEBUG,"cleared messages for %d check = %d",que_ID,item->originator);
	/* END NEW 30.06.2000 */

	/* Remove the entry from the QTAB, and possibly compress it */
	next_qcb = qcb + 1;
	move_len = ((char *) queues + queues->QTAB_size) - (char *) next_qcb;

	/* Compress QTAB if entry was not the last */
	if (move_len > 0) 
	{
		memcpy((char *) qcb, (char *) next_qcb, move_len);
	} 
	queues->nbr_queues--;
	queues->QTAB_free += sizeof(QCB);

	/* Remove the associated route */
	dealoc_route(que_ID,0);

	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		buff.msg_header.mtype = (long) item->originator;
		buff.msg_length = 1;
		len = sizeof(ITEM);
	}
	else 
	{
		if (uutil_flag == OTHER) 
		{
		dbg(TRACE,"sending ACK for %d check = %d",que_ID,item->originator);
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<dealoc_queue> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following function handles the empty_que request. The routine	*/
/* calls the que_clear routine to perform the actual clear of the queue	*/
/*									*/
/* ******************************************************************** */
static int	clear_queue()
{
	int	ilRC;
	QCB	*qcb;			/* A qcb work pointer */
	ITEM	errbuff;
	ITEM	buff;
	long	orig;
	
	dbg(DEBUG,"<clear_queue> ---- START ----");

	orig = item->originator;

	/* Find the qcb ( from the route ID ) */
	qcb = find_queue(item->route);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = NOT_FOUND;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<clear_que> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<clear_que> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<clear_queue> ---- END ----");
		return QUE_E_NOFIND;
	}

	/* Queue located, now clear it */
	que_clear(qcb);
	
	/* Remove a possible msg in the UNIX msg facility */
	check_msgtype(item->route, "clear_queue");
	
	/* Send ACK back */
	item->originator = orig;
	send_ack(NUL);
	buff.msg_header.mtype = (long) item->originator;
	buff.msg_length = 1;
	errno = 0;
	if ((ilRC = msgsnd(qcp_output,&buff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
	{
		dbg(TRACE,"<clear_que> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<clear_que> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
	}
	dbg(DEBUG,"<clear_queue> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine clears a queue ( removes all items ).		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static void	que_clear(qcb)
QCB *qcb;
#else
static void	que_clear(QCB *qcb)
#endif

{
	Q_ENTRY	*entry;
	Q_ENTRY	*next_entry;
	ITEM	*psQItem;
	int	i;

	dbg(DEBUG,"<que_clear> ---- START ----");
	
	for (i=0; i<5; i++) 
	{
		entry = qcb->qitem[i].first;

		while (entry != 0) 
		{
			/* Check for SHM usage, and handle */
			
			psQItem = (ITEM*) entry->data;
			
			if (psQItem->function == QUE_SHM)
			{
				drop_SHM_item(psQItem);
			} /* end if */
			
			next_entry = entry->next;
			free((char *) entry);
			entry = next_entry;
		}

		qcb->qitem[i].nbr_items = 0;
		qcb->qitem[i].first = (Q_ENTRY *) NUL;
		qcb->qitem[i].last  = (Q_ENTRY *) NUL;
	}

	qcb->status &= ~F_ACK;		/* Clear any outstanding ACK	*/
	qcb->total = 0;			/* Clear total number of msg	*/
	qcb->msg_count = 0;		/* Clear number of msg		*/
	qcb->ack_count = 0;
	dbg(DEBUG,"<que_clear> ---- END ----");
	return;
} 

/* ******************************************************************** */
/*									*/
/* The following routine finds a queue entry in the QTAB. Due to the	*/
/* limited number of entries a simple linear search is used. The 	*/
/* routine returns a pointer to the queue (QCB).			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static QCB	*find_queue(que_ID)
int que_ID;
#else
static QCB	*find_queue(int que_ID)
#endif
{
	QCB	*return_value;
	int	i;

	return_value = (QCB *)NOT_FOUND;
	for (i=0; i<=queues->nbr_queues; i++ ) 
	{
		if (queues->queue[i].que_ID == que_ID) 
		{
			return_value = &(queues->queue[i]);
			break;
		}
	} 
	return return_value;
} 

/* ******************************************************************** */
/*									*/
/* The following routine queues an item to one or more queues. The que-	*/
/* IDs are read from the route entry. The routine add_to_que() is used	*/
/* to perform the actual queuing.					*/
/* The routine takes a mode as parameter. Mode can be FRONT or BACK	*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	put_item(mode)
int mode;
#else
static int	put_item(int mode)
#endif
{
	ROUTE	*rou_ptr;			/* A pointer to a route */
	int	ilRC;
	int	i;
	int	return_value;

	dbg(DEBUG,"<put_item> ---- START ----");
	/* First validate the route */
	if ((rou_ptr = find_route(item->route)) == (ROUTE*)NOT_FOUND)
	{
		char str[7];
		memset(str,0x00,7);
		memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
		sprintf(pcgTmpBuf,"orig <%d> dest <%d>",item->originator,item->route);

#ifdef UFIS43
		if ((ilRC = to_msgh(IPRIO_DB,QCP_ROUNOF,0,0,pcgTmpBuf)) == TERMINATE)
		{
			dbg(TRACE,"<put_item> %05d to_msgh returns: %d", __LINE__, ilRC);
			dbg(DEBUG,"<put_item> ---- END ----");
			return TERMINATE;
		}
#else
		if ((ilRC = to_msgh(0,QCP_ROUNOF,0,item->route,itoa(item->originator,str,10))) == TERMINATE)
		{
			dbg(TRACE,"<put_item> %05d to_msgh returns: %d", __LINE__, ilRC);
			dbg(DEBUG,"<put_item> ---- END ----");
			return TERMINATE;
		}
#endif

		/* Route does not exist */
		dbg(DEBUG,"<put_item> ---- END ----");
		return QUE_E_NOFIND;
	}
	
	/* Handle load_sharing routes */
	
	if ((item->route>=LOAD_SH_LOW) && (item->route<=LOAD_SH_HIGH))
	{
		/* Add to primary queue only */
		
		if ((return_value = add_to_que(mode,item->route)) != RC_SUCCESS)
		{
			dbg(TRACE,"<put_item> %05d add_to_que (%d) returns: %d", __LINE__, 
							item->route,return_value);
		} /* end if */
	} /* end if */
		
	else
	{
		/* Add the item to all queues from route table */
		
		for (i=0; i<rou_ptr->nbr_entries; i++) 
		{
			if ((return_value = add_to_que(mode,rou_ptr->routine_ID[i])) != RC_SUCCESS)
			{
				rep_int_error(return_value, "put_item");
				rep_int_error(rou_ptr->routine_ID[i], "put_item");

				/* only if we receive terminate */
				if (return_value != RC_SUCCESS)
				{
					dbg(TRACE,"<put_item> %05d add_to_que returns: %d", __LINE__, return_value);
					dbg(DEBUG,"<put_item> ---- END ----");
					return return_value;
				} /* end if */
			} /* end if */
		} /* end for */
	} /* end else */ 

	/* bye bye */
	dbg(DEBUG,"<put_item> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine performs the actual queuing. The routine is	*/
/* called with the queuing mode and the queue_ID as parameter. The mode	*/
/* can be either queue to FRONT of queue or queue to BACK of queue.	*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int add_to_que(mode,queue_ID)
int mode;
int queue_ID;
#else
static int add_to_que(int mode,int queue_ID)
#endif
{
	QCB		*qcb, *s_qcb;		/* Work pointers for qcbs	*/
	Q_ENTRY	*wrkptr;			/* A queue entry work pointer	*/
	ITEM 		*ack_buf;
	int		item_len;		/* The length of the item buffer */
	short		pos;			/* position on queue list	*/
	int		return_value = RC_SUCCESS;
	int		ret=-1;
	int		ilRC;
	int		ilResend = FALSE;

	dbg(DEBUG,"<add_to_que> ---- START ----");

	/* is this an existing queue? */
	if ((qcb = find_queue(queue_ID)) == (QCB*)NOT_FOUND) 
	{
		dbg(TRACE,"<add_to_que> %05d find_queue returns NOT_FOUND", __LINE__);
		dbg(DEBUG,"<add_to_que> ---- END ----");
		return QUE_E_NOFIND;
	}
	
	/* Allocate a queue entry buffer */
	DebugPrintItem(DEBUG,(ITEM*)item);
	item_len = sizeof(ITEM) + item->msg_length;
	dbg(DEBUG,"<add_to_que> item len is: %d", item_len);
	if ((wrkptr = (Q_ENTRY *) calloc(1,item_len+sizeof(Q_ENTRY))) == NULL)
	{
		dbg(TRACE,"<add_to_que> %05d malloc failure (queue)", __LINE__);
		dbg(DEBUG,"<add_to_que> ---- END ----");
		return TERMINATE;
	}

	if ((ack_buf = (ITEM *) calloc(1,I_SIZE)) == NULL)
	{
		dbg(TRACE,"<add_to_que> %05d malloc failure (ack_buffer)", __LINE__);
		dbg(DEBUG,"<add_to_que> ---- END ----");
		return TERMINATE;
	}

	/* Copy the item into the queue entry buffer */
	wrkptr->time_stamp = time(NUL);
	memcpy(wrkptr->data,item,item_len);
	wrkptr->len = item_len;
	prio = (item->priority)-1;
	qcb->qitem[prio].nbr_items++;
	qcb->msg_count++;
	qcb->total++;

	/* Add item to queue depending on the mode parameter. Mode
	   defaults to BACK ( queue to back of queue) 
	*/

	switch (mode) 
	{
		case FRONT:
			dbg(DEBUG,"<add_to_que> %05d mode is FRONT...", __LINE__);
			pos = 1;
			if (qcb->status & F_ACK) 
			{	
				/* Outstanding ACK? */
				if ((ilRC = msgrcv(qcp_output,ack_buf,I_SIZE, queue_ID,IPC_NOWAIT)) >= RC_SUCCESS) 	
				{
					qcb->status ^= F_ACK;
					ret = RC_SUCCESS;
dbg(TRACE,"%05d:item->function <%d>",__LINE__,item->function);
				}
				else 
				{
					/* normally this is 'no message of desired type' */
					pos = 2;
				}
			} 

			switch (pos) 
			{
				case 1:
					wrkptr->prev = (Q_ENTRY*)NUL;
					wrkptr->next = qcb->qitem[prio].first;
					if (qcb->qitem[prio].first != (Q_ENTRY*)NUL) 
					{
						qcb->qitem[prio].first->prev = wrkptr;
					}
					else 
					{
						qcb->qitem[prio].last = wrkptr;
					}
					qcb->qitem[prio].first = wrkptr;
					break;

				case 2:
					/* Add item as second item */
					if (qcb->qitem[prio].first != (Q_ENTRY *)NUL) 
					{
						wrkptr->prev = qcb->qitem[prio].first;
						wrkptr->next = qcb->qitem[prio].first->next;
						qcb->qitem[prio].first->next = wrkptr;
						if (wrkptr->next == (Q_ENTRY *)NUL) 
						{
							qcb->qitem[prio].last= wrkptr;
						}
					} 
					else 
					{
						qcb->qitem[prio].first = qcb->qitem[prio].last = wrkptr;
						wrkptr->prev = wrkptr->next = (Q_ENTRY *)NUL;
					} 
					break;

				default:
					/* here? */
					break;

			} /* end switch (pos) */
			break;

		default:
			/* this is BACK, add item at end of list */	
			dbg(DEBUG,"<add_to_que> %05d now link together list...", __LINE__);
			ret = -1;
			wrkptr->next = (Q_ENTRY *)NUL;
			wrkptr->prev = qcb->qitem[prio].last;
			if (qcb->qitem[prio].last != (Q_ENTRY *)NUL) 
			{
				qcb->qitem[prio].last->next = wrkptr;
			}
			else 
			{
				qcb->qitem[prio].first = wrkptr;
			}
			qcb->qitem[prio].last = wrkptr;
			break;
	} /* end switch (mode) */

	free(ack_buf);

	/* Check for load-sharing */
	
	if ((qcb->que_type == QTYPE_PRIMARY) && (qcb->status & GO))
	{
		/* A load-sharing queue, try to find an empty secondary queue */
		
		s_qcb = find_empty_secondary(qcb->que_ID);
		
		if (s_qcb != (QCB *) NOT_FOUND)
		{
			/* We have an empty queue, move item and send */
			
			move_to_secondary(s_qcb);
			return_value = send_item(s_qcb);
		} /* end if (Empty secondary) */
		else
		{
			dbg(TRACE,"%05d:no secondary que found for (%d)",__LINE__,qcb->que_ID);
		}
	} /* end if (Primary queue) */

	else 
	{
		/* Normal queue. Check if only item on queue */
	
		if (qcb->total == 1 || ret == RC_SUCCESS)
		{
			/* Forward the item to destination */
			return_value = forward_item(qcb, (ITEM*)wrkptr->data);
		} /* end if */
	} /* end else (Normal queue) */

	/* 11.07.2000 */
	/* Check if a previous send failed ( The DATA flag is set) */
	if ((qcb->status & DATA) != NUL)
	{
		qcb->status != DATA;
		send_item(qcb); /* Try to resend an item from the queue */
		
	}
	
	dbg(DEBUG,"<add_to_que> ---- END ----");
	/* thats all */
	return return_value;
} 

/* ******************************************************************** */
/*									*/
/* The following routine forwards an item to the destination routine.	*/
/* The output is done via the QCP general queue and the destination is	*/
/* identified by the message type.					*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	forward_item(qcb, q_item)
QCB 	*qcb;
ITEM 	*q_item;
#else
static int	forward_item(QCB *qcb, ITEM *q_item)
#endif
{
	struct msgbuf	*wrkptr;		/* A msg work ptr */
	int		item_len;
	int		ilRC;
	int		ilCnt;
	int 		ilSaveOriginator;
	struct msqid_ds 		rlQueStatus;

	dbg(DEBUG,"<forward_item> ---- START ----");

	/* Check if queue is on hold */
	if (qcb->status & HOLD) 
	{
		/* Return with no action */
		dbg(DEBUG,"<forward_item> ---- END ----");
		return QUE_HOLD;			
	}
	
	/* Check if NEXT protocol is used and not in NEXTI state */
	
	if (qcb->protocol == PROTOCOL_NEXT)
	{
		if (qcb->status & NEXTI)
		{		
		 	qcb->status ^= NEXTI;
		} /* end if */
		
		else
		{
			/* Return with no action */
		
			return RC_SUCCESS;
		} /* end else */
	} /* end if (NEXT protocol in use) */
	
	/* 11.07.2000 */
	/* Check if a QCP ACK to the process is missing */
	if (qcb->status & S_ACK) 
	{
		qcb->status != S_ACK;
		/* Send an ACK */
		dbg(DEBUG,"<forward_item> Sending an ACK to process (previously missing) ---- END ----");
		ilSaveOriginator = item->originator;
		item->originator = qcb->que_ID;
		send_ack(NUL);	/* ACK the previous message */
		item->originator = ilSaveOriginator;		
	} 

	/* Prepare the item for xmit */
	item_len = sizeof(ITEM) + q_item->msg_length - sizeof(long);

	wrkptr = (struct msgbuf*)q_item;
	wrkptr->mtype = (long)qcb->que_ID;

	/* Send the message via the UNIX msg facility */
	ilCnt = 0;
	do
	{
		/* read current no of byte on q */
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,wrkptr,item_len,IPC_NOWAIT)) != RC_SUCCESS) 
		{
			switch (ilCnt)
			{
				case 0:
					nap(10);
				case 1:
				case 2:
				case 3:
				case 4:
					break;

				case 5:
					dbg(TRACE,"<forward_item> %05d <%s>", __LINE__, strerror(errno));
					if ((ilRC = StoreQueueItems(qcp_input)) == TERMINATE)
					{
						dbg(TRACE,"<forward_item> %05d StoreQueueItems returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<forward_item> ---- END ----");
						return ilRC;
					}

					/* only to try another msgsnd */
					ilRC = RC_FAIL;
					break;

				case 6:
					dbg(TRACE,"<forward_item> %05d <%s>", __LINE__, strerror(errno));
					if ((ilRC = StoreQueueItems(qcp_output)) == TERMINATE)
					{
						dbg(TRACE,"<forward_item> %05d StoreQueueItems returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<forward_item> ---- END ----");
						return ilRC;
					}

					/* only to try another msgsnd */
					ilRC = RC_FAIL;
					break;

				default:
					/* if counter > 6 -> this is a massive error */
					qcb->status |= DATA;
					dbg(TRACE,"<forward_item> setting que %d to status 17", qcb->que_ID);
					dbg(DEBUG,"<forward_item> ---- END ----");
					return QUE_E_SEND;
					break;
			}

			/* the next try?, may be */
			ilCnt++;
		}
		else
		{
			dbg(TRACE,"%05d:Item sent to que: %d (Primary ID %d)",__LINE__,
      qcb->que_ID, qcb->primary_ID);     
		}
	}
	while (ilRC != RC_SUCCESS);

	/* Now set the outstand ACK flag and the transmit timestamp */
	qcb->status |= F_ACK;
	qcb->last_prio = q_item->priority;
	qcb->retry_cnt = 0;			/* Clear the retry count */
	qcb->timestamp = time(NUL);

	dbg(DEBUG,"<forward_item> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine handles the queue status changes. At the mo-	*/
/* ment HOLD is implemented.						*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	que_status(status,uutil_flag)
int status;
int uutil_flag;
#else
static int	que_status(int status,int uutil_flag)
#endif
{
	int	ilRC;
	QCB	*qcb;			/* A qcb work pointer */
	ITEM	errbuff;
	ITEM	buff;
	int	len;

	dbg(DEBUG,"<que_status> ---- START ----");

	/* Find the qcb ( from the route ID ) */
	qcb = find_queue(item->route);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = NOT_FOUND;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<que_status> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<que_status> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}
		dbg(DEBUG,"<que_status> ---- END ----");
		return QUE_E_NOFIND;
	} 

	prio = (item->priority)-1;

	/* Now set the new status */
	switch (status) 
	{
		case GO:
			if (qcb->status & HOLD) 
			{
				/* Clear the HOLD flag and set GO */
				qcb->status &= ~HOLD;
				qcb->status |= GO;
			}
			if (qcb->total > 0) 
			{
				send_item(qcb);
			}
			break;

		case HOLD:
			/* Set the HOLD flag and clear GO */
			qcb->status &= ~GO;
			qcb->status |= HOLD;
			break;

		default:
			if (uutil_flag == UUTIL) 
			{
				send_ack(NUL);
				memset((char *)&errbuff,0x00,sizeof(ITEM));
				errbuff.msg_header.mtype = (long) item->originator;
				errbuff.msg_length = FATAL;
				errno = 0;
				if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
				{
					dbg(TRACE,"<que_status> %05d msgsnd returns %d", __LINE__, ilRC);
					dbg(TRACE,"<que_status> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
				}
			}
			dbg(DEBUG,"<que_status> ---- END ----");
			return QUE_E_STATUS;
	} /* end switch (status) */
	
	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		buff.msg_header.mtype = (long) item->originator;
		buff.msg_length = 1;
		len = sizeof(ITEM);
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
		{
			dbg(TRACE,"<que_status> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<que_status> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			dbg(DEBUG,"<que_status> ---- END ----");
			return QUE_E_SEND;
		}
	} 
	else 
	{
		if (uutil_flag == OTHER) 
		{
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<que_status> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine handles ACKs. An ACK will cause the first item	*/
/* on the queue to be deleted and, if queue is not empty, the next item	*/
/* to be sent.								*/
/*									*/
/* ******************************************************************** */
static int	ack_item()
{
	int		ilRC;
	QCB		*qcb;			/* A qcb work pointer */
	Q_ENTRY	*wrkptr;			/* A Q_ENTRY work pointer */

	dbg(DEBUG,"<ack_item> ---- START ----");

	/* init */
	ilRC = RC_SUCCESS;

	/* First find the QCB */
	qcb = find_queue(item->originator);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		dbg(DEBUG,"<ack_item> ---- END ----");
		return QUE_E_NOFIND;
	}
	
	prio = qcb->last_prio-1;
	qcb->ack_count++;

	/* Check if an ACK was expected */
	if (qcb->status & F_ACK) 
	{
		dbg(DEBUG,"<ack_item> ACK is expected...");

		/* Remove item from queue */
		wrkptr = qcb->qitem[prio].first;
		if (wrkptr == (Q_ENTRY *)NULL) 
		{
			dbg(DEBUG,"<ack_item> wrkptr is NULL...");

			memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
			sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
			if ((ilRC = to_msgh(IPRIO_DB,QCP_DESTRO,0,0,pcgTmpBuf)) == TERMINATE)
			{
				dbg(TRACE,"<ack_item> %05d to_msgh returns: %d", __LINE__, ilRC);
				dbg(DEBUG,"<ack_item> ---- END ----");
				return TERMINATE;
			}
#else
			if (to_msgh(0,QCP_DESTRO,0,qcb->que_ID,0) == TERMINATE)
			{
				dbg(TRACE,"<ack_item> %05d to_msgh returns: %d", __LINE__, ilRC);
				dbg(DEBUG,"<ack_item> ---- END ----");
				return TERMINATE;
			}
#endif
			/* Message building and transfer to UGCCS-screen */
			if ((ilRC = que_rebuild(qcb)) == TERMINATE)
			{
				dbg(TRACE,"<ack_item> %05d que_rebuild returns: %d", __LINE__, ilRC);
				dbg(DEBUG,"<ack_item> ---- END ----");
				return TERMINATE;
			}
			dbg(DEBUG,"<ack_item> que_rebuild returns: %d", ilRC);
		}
		else 
		{
			dbg(DEBUG,"<ack_item> wrkptr is NOT NULL...");

			qcb->qitem[prio].first = qcb->qitem[prio].first->next;
			if (qcb->qitem[prio].first != (Q_ENTRY *)NUL) 
			{
				qcb->qitem[prio].first->prev = (Q_ENTRY *) NUL;
				qcb->status ^= F_ACK;	/* Clear ACK flag */
			} 
			else 
			{
				if (qcb->qitem[prio].last == wrkptr) 
				{
					qcb->qitem[prio].last = (Q_ENTRY *) NUL;
					qcb->status ^= F_ACK; /* Clear ACK flag */
				} 
				else 
				{
					if ((ilRC = que_rebuild(qcb)) == TERMINATE)
					{
						dbg(TRACE,"<ack_item> %05d que_rebuild returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<ack_item> ---- END ----");
						return TERMINATE;
					}
					dbg(DEBUG,"<ack_item> que_rebuild returns: %d", ilRC);
				}
			} 

			qcb->qitem[prio].nbr_items--;
			qcb->total--;
			qcb->timestamp = 0l;
			wrkptr->time_stamp = NUL;
			free((char *) wrkptr);	/* Release memory */
		}
	}
	
	/* Check if further items on queue, if yes, forward next item */
	/* but only if que_rebuild returns with success... */
	/* This is done for protocol type ACK as the ACK is the only trigger */
	
	if (qcb->protocol == PROTOCOL_ACK)
	{
		/* Check if load-sharing secondary queue */
		
		if (qcb->que_type == QTYPE_SECONDARY)
		{
			move_to_secondary(qcb);
		} /* end if */
		
		if ((qcb->total > 0) && (ilRC == RC_SUCCESS)) 
		{
			ilRC = send_item(qcb);
		}
	} /* end if  (ACK protocol ) */
	
	qcb->ack_count--;
	dbg(DEBUG,"<ack_item> ---- END ----");
	return ilRC;
} 

/* ******************************************************************** */
/*									*/
/* The following function checks the queues for no activity and for	*/
/* missing ACKs.							*/
/*									*/
/* ******************************************************************** */
static int	check_queues()
{
	QCB	*qcb;			/* A qcb work pointer */
	int	ret;			/* return value */
	int	i,j;
	int	ilRC;
	int	chgstat = 0;		/* Change state sign	*/

	dbg(DEBUG,"<check_queues> ---- START ----");
	/* Loop through all the queues */
	for (i = 1; i <= queues->nbr_queues; i++ ) 
	{
		qcb = &(queues->queue[i]);
		
		/* Ignore primary load-sharing queues */
		
		if (qcb->que_type == QTYPE_PRIMARY)
		{
			continue;
		} /* end if (primary queue) */

		/* Check for inactivity */
		if ((qcb->status & (F_ACK | HOLD)) == 0) 
		{
			if (qcb->total > 0) 
			{
				if (!strncmp(qcb->name,"WAIT",4))
				{
					memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
					sprintf(pcgTmpBuf,"%s;%d",qcb->name,qcb->total);
#ifdef UFIS43
					if ((ilRC = to_msgh(IPRIO_DB,QCP_INACTV,0,0,pcgTmpBuf)) == TERMINATE)
					{
						dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<check_queues> ---- END ----");
						return TERMINATE;
					}
#else
					if ((ilRC = to_msgh(1,QCP_INACTV,0,qcb->total, qcb->name)) == TERMINATE)
					{
						dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<check_queues> ---- END ----");
						return TERMINATE;
					}
#endif
					if ((ret = check_msgtype(qcb->que_ID, "check_queues"))) 
					{
						memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
						sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
						if ((ilRC = to_msgh(IPRIO_DB,QCP_NOACK,0,0,pcgTmpBuf)) == TERMINATE)
						{
							dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<check_queues> ---- END ----");
							return TERMINATE;
						}
#else
						if ((ilRC = to_msgh(1,QCP_NOACK,0, qcb->que_ID,0)) == TERMINATE)
						{
							dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<check_queues> ---- END ----");
							return TERMINATE;
						}
#endif
					}
					
					/* notice a faulty que */
					for(j=0;j<=queues->nbr_queues;j++)
					{
						if(ack_appear[j] == 0)
						{
						    ack_appear[j]=qcb->que_ID;
						    chgstat=1;
						    break;
						}
					}
					send_item(qcb);
				} 
			} 
		} 

		/* Check for missing ACK */
		else 
		{
			if ((qcb->status & F_ACK) && (qcb->status & GO) && ((time(NUL) - qcb->timestamp) > TIMESCALE)) 
			{
				qcb->status ^= F_ACK;
				switch (check_msgtype(qcb->que_ID, "check_queues")) 
				{
					case NOT_FOUND:
						memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
						sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
						if ((ilRC = to_msgh(IPRIO_DB,QCP_MISACK,0,0,pcgTmpBuf)) == TERMINATE)
						{
							dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<check_queues> ---- END ----");
							return TERMINATE;
						}
#else
						if ((ilRC = to_msgh(0,QCP_MISACK,0,qcb->que_ID,0)) == TERMINATE)
						{
							dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<check_queues> ---- END ----");
							return TERMINATE;
						}
#endif
						break;

					default:
						/* A possible PHANTOM */
						if (qcb->que_ID < 20000)
						{
							memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
							sprintf(pcgTmpBuf,"%s;%d",qcb->name,qcb->total);
#ifdef UFIS43
							if ((ilRC = to_msgh(IPRIO_DB,QCP_INACTV,0,0,pcgTmpBuf)) == TERMINATE)
							{
								dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
								dbg(DEBUG,"<check_queues> ---- END ----");
								return TERMINATE;
							}
#else
							if ((ilRC = to_msgh(1,QCP_INACTV,0,qcb->total, qcb->name)) == TERMINATE)
							{
								dbg(TRACE,"<check_queues> %05d to_msgh returns: %d", __LINE__, ilRC);
								dbg(DEBUG,"<check_queues> ---- END ----");
								return TERMINATE;
							}
#endif
						}
						break;
				} 
				
				/* notice a faulty que */
				for(j=0;j<=queues->nbr_queues;j++)
				{
					if(ack_appear[j] == 0)
					{
					    ack_appear[j]=qcb->que_ID;
					    chgstat=1;
					    break;
					}
				}
				send_item(qcb);
				qcb->timestamp = (time(NUL)+12001);
			}
		} 

		/* Replace a noticed que_ID */
		if(chgstat == 0)
		{
			for(j=0; j<=queues->nbr_queues; j++)
			{
				if ((qcb->que_ID == ack_appear[j]) && !(qcb->status & F_ACK))
				{
					ack_appear[j] = 0;
					break;
				}
			}
		}
		chgstat=0;
	}
	dbg(DEBUG,"<check_queues> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine checks if a msgtype is active on the general	*/
/* output queue ( QCP_GENERAL ).					*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int check_msgtype(type, pcpCP)
int 	type;
char	*pcpCP
#else
static int check_msgtype(int type, char *pcpCP)
#endif
{
	QCB	*qcb;
	int	ilRC;

	dbg(DEBUG,"<check_msgtype> ---- START ----");
	if ((qcb = find_queue(type)) == NULL) 
	{
		dbg(DEBUG,"<check_msgtype> ---- END ----");
		return 0;
	} 

	if (qcb->status & S_ACK) 
	{
		dbg(DEBUG,"<check_msgtype> ---- END ----");
		return 0;
	}

	errno = 0;
	if ((ilRC = msgrcv(qcp_output,item,I_SIZE,type,IPC_NOWAIT)) < RC_SUCCESS) 
	{
		dbg(DEBUG,"<check_msgtype> %05d called from <%s>", __LINE__, pcpCP);
		dbg(DEBUG,"<check_msgtype> %05d <%s>", __LINE__, strerror(errno));
		dbg(DEBUG,"<check_msgtype> ---- END ----");
		return 0;
	} 
	else 
	{
		/* function returns with success here */
		dbg(DEBUG,"<check_msgtype> ---- END ----");
		return 1;
	} 
} 

/* ******************************************************************** */
/*									*/
/* The following routine checks if a forward retry is due. The retry	*/
/* table holds the queue IDs of the queues from where a forward retry	*/
/* must be performed.							*/
/*									*/
/* ******************************************************************** */
static int check_retry()
{
	int	ilRC;
	int	que_ID;
	QCB	*qcb;
	ITEM	*buff;

	dbg(DEBUG,"<check_retry> ---- START ----");

	/* init */
	ilRC = RC_SUCCESS;

	if (retries.nbr_entries > NUL) 
	{
		que_ID = retries.que_ID[retries.current];
		qcb = find_queue(que_ID);

		if (qcb != (QCB*)NUL) 
		{
			/* Check if queue on GO and no ACK */
			if ((qcb->status ^ GO) == NUL) 
			{
				prio = (item->priority)-1;
				if (qcb->qitem[prio].nbr_items > NUL) 
				{
					buff = (ITEM *)qcb->qitem[prio].first->data;
					dbg(TRACE,"<check_retry> %05d resend item now", __LINE__);
					ilRC = forward_item(qcb, buff);
				} 
			} 
			else
				ilRC = QUE_STATUS;
		}

		if (ilRC == RC_SUCCESS)
		{
			retries.nbr_entries--;
			retries.current++;
			if (retries.current >= MAX_Q_IDS) 
			{
				retries.current = 0;
			}
		}
	}
	dbg(DEBUG,"<check_retry> ---- END ----");
	return ilRC;
} 

/* ******************************************************************** */
/*									*/
/* The following routine builds a buffer for a queue overview display.	*/
/*									*/
/* ******************************************************************** */
static int	get_q_over()
{
	int		ilRC;
	ITEM		*buff;
	QUE_INFO	*que_info;
	QINFO		*qinfo;
	ITEM		errbuff;
	int		len;
	int		slen;
	int		i,j,k;
	int		rc = RC_SUCCESS;

	dbg(DEBUG,"<get_q_ober> ---- START ----");

	/* Allocate the buffer */
/*		11.07.2000 
	len = sizeof(ITEM) + sizeof(QUE_INFO) + (sizeof(QINFO) * 200); 
*/
	len = sizeof(ITEM) + sizeof(QUE_INFO) + (sizeof(QINFO) * queues->nbr_queues);
	
	if ((buff = (ITEM *)calloc(1,len)) == NULL)
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = FATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_q_over> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_q_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_q_ober> ---- END ----");
		return QUE_E_MEMORY;
	}

	/* Set the pointers */
	que_info = (QUE_INFO*)&(buff->text[0]);
	qinfo = &(que_info->data[0]);

	/* Extract the info data */
	for (i = 1; i <= queues->nbr_queues; i++ ) 
	{
		qinfo->id = queues->queue[i].que_ID;
		qinfo->status = queues->queue[i].status;

		for (j = 0; j < 5; j++) 
		{
			qinfo->nbr_items[j] = queues->queue[i].qitem[j].nbr_items;
		} 

		qinfo->total = queues->queue[i].total;
		memcpy(qinfo->name,queues->queue[i].name,MAX_PRG_NAME+1);
		slen=MAX_PRG_NAME-strlen(qinfo->name);

		for(k=0; k<slen; k++)
		{
			strcat(qinfo->name," ");
		}

		qinfo->msg_count = queues->queue[i].msg_count;
		qinfo++;
	} 
	que_info->entries = queues->nbr_queues - 1;

	buff->msg_header.mtype = (long) item->originator;
	buff->function = QUE_ACK;
	buff->route = 0;
	buff->priority = 3;
	buff->msg_length = len - sizeof(ITEM);
	buff->originator = 0;

	/* Send ACK back */
	send_ack(NUL);
	
	errno = 0;
	if ((ilRC = msgsnd(qcp_output,buff,len,IPC_NOWAIT)) != RC_SUCCESS)
	{
		dbg(TRACE,"<get_q_over> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<get_q_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		free((char*)buff);
		dbg(DEBUG,"<get_q_ober> ---- END ----");
		return QUE_E_SEND;
	}
	free((char *) buff);
	dbg(DEBUG,"<get_q_ober> ---- END ----");
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine builds a buffer for a queue overview display	*/
/* of only one queue							*/
/*									*/
/* ******************************************************************** */
static int	get_one_q_over()
{
	int		ilRC;
	ITEM		*buff;
	QUE_INFO	*que_info;
	QINFO		*qinfo;
	ITEM		errbuff;
	int		len;
	int		slen;
	int		i,j,k;

	dbg(DEBUG,"<get_one_q_ober> ---- START ----");
	
	/* Allocate the buffer */
/*		11.07.2000 
	len = sizeof(ITEM) + sizeof(QUE_INFO) + (sizeof(QINFO) * 200); 
*/
	len = sizeof(ITEM) + sizeof(QUE_INFO) + (sizeof(QINFO) * 2);
	if ((buff = (ITEM *) calloc(1,len)) == NULL)
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = FATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_one_q_over> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_one_q_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_one_q_ober> ---- END ----");
		return QUE_E_MEMORY;
	}

	/* Set the pointers */
	que_info = (QUE_INFO *) &(buff->text[0]);
	qinfo = &(que_info->data[0]);
	qinfo->status = -1;

	/* Extract the info data */
	for (i = 1; i <= queues->nbr_queues; i++) 
	{
		qinfo->id = queues->queue[i].que_ID;
		if (qinfo->id == item->route) 
		{
			qinfo->status = queues->queue[i].status;
			for (j = 0; j < 5; j++) 
			{
				qinfo->nbr_items[j] = queues->queue[i].qitem[j].nbr_items;
			} 
			qinfo->total = queues->queue[i].total;
			memcpy(qinfo->name,queues->queue[i].name,MAX_PRG_NAME+1);
			slen=MAX_PRG_NAME-strlen(qinfo->name);
			for(k=0; k<slen; k++)
			{
				strcat(qinfo->name," ");
			}
			qinfo->msg_count = queues->queue[i].msg_count;
			qinfo++;
		} 
	} 
	que_info->entries = 1;

	buff->msg_header.mtype = (long) item->originator;
	buff->function = QUE_ACK;
	buff->route = 0;
	buff->priority = 3;
	buff->msg_length = len - sizeof(ITEM);
	buff->originator = 0;

	/* Send ACK back */
	send_ack(NUL);
	
	errno = 0;
	if ((ilRC = msgsnd(qcp_output,buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<get_one_q_over> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<get_one_q_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		free((char *) buff);
		dbg(DEBUG,"<get_one_q_ober> ---- END ----");
		return QUE_E_SEND;
	}

	free((char *) buff);
	dbg(DEBUG,"<get_one_q_ober> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine builds a buffer for a queue display.		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	get_q_info(send)
QUE_SEND *send;
#else
static int	get_q_info(QUE_SEND *send)
#endif
{
	int	ilRC;
	ITEM	*buff;		/* send item buffer			*/
	ITEM	errbuff;	/* send buffer by error			*/
	int	len;		/* item length				*/
	int	j;		/* priority				*/
	int	length;		/* message length			*/
	int	lenitem;	/* length of all messages		*/
	int	start;		/* start priority			*/
	int	end;		/* last priority			*/
	int	count;		/* number of items			*/
	QCB	*qcb;		/* qcb pointer				*/
	char	*wrk_ptr;
	Q_ENTRY *cur;		/* current queue entry			*/
	HEAD	*qhead;		/* send header				*/
	
	dbg(DEBUG,"<get_q_info> ---- START ----");
	qcb = find_queue(send->que_ID);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = NOT_FOUND;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_q_info> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_q_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_q_info> ---- END ----");
		return(QUE_E_NOFIND);
	}
	
	if (qcb->total <= NUL) 
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = NONFATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_q_info> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_q_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_q_info> ---- END ----");
		return(QUE_E_NOMSG);
	}
	
	start = send->prio - 1;
	end = PRIORITY_5;

	if (send->no_item > qcb->total) 
	{
		send->no_item = qcb->total;
	}

	count = lenitem = 0;
	for (j = start; j < end; j++) 
	{
		if ((send->next != NULL) && (j == start)) 
		{
			cur = send->next; 
		}
		else 
		{	
			cur = qcb->qitem[j].first;
		}

		if (cur != NUL) 
		{
			do 
			{
				count++;
				if (cur->len < Q_ITEM_DATA) 
				{
					lenitem += cur->len + 2 * sizeof(int);
				}
				else 
				{
					lenitem += Q_ITEM_DATA + 2 * sizeof(int);
				} 
				cur = cur->next;
			} while ((cur != NULL) && (count < send->no_item));

			if (count >= send->no_item) 
			{
				break;
			} 
		} 
	} 

	if (count < send->no_item) 
	{
		send->no_item = count;
	}
	
	/* Allocate the buffer */
	len = sizeof(ITEM) + sizeof(HEAD) + lenitem;
	if ((buff = (ITEM *)calloc(1,len)) == NULL)
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = FATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_q_info> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_q_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_q_info> ---- END ----");
		return QUE_E_MEMORY;
	}

	qhead = (HEAD *)buff->text;
	memcpy(buff->text,(char *) qcb,sizeof(QCB));
	qhead->qcb.total = count;
	qhead->q_ent = cur;
	wrk_ptr = (char*)qhead + sizeof(HEAD);	
	
	count = NUL;
	for (j = start; j < end; j ++) 
	{
		if ((send->next != NULL) && (j == start)) 
		{
			cur = send->next; 
		}
		else 
		{	
			cur = qcb->qitem[j].first;
		}

		if (cur != NULL) 
		{
			do 
			{
				count++;
				if (cur->len < Q_ITEM_DATA) 
				{
					*((int*)wrk_ptr) = length =cur->len;
				} 
				else 
				{
					*((int*)wrk_ptr) = length = Q_ITEM_DATA;
				} 

				wrk_ptr += sizeof(int);
				*((int *) wrk_ptr) = j;
				wrk_ptr += sizeof(int);
				memcpy(wrk_ptr,(char *) cur->data,length);
				wrk_ptr += length;
				cur = cur->next;
			} while((cur != NULL) && (count < send->no_item));

			if (count >= send->no_item) 
			{
				break;
			}
		} 
	} 

	buff->msg_header.mtype = (long) item->originator;
	buff->function = QUE_ACK;
	buff->route = 0;
	buff->priority = 3;
	buff->msg_length = len - sizeof(ITEM);
	buff->originator = 0;

	/* Send ACK back */
	send_ack(NUL);
	
	errno = 0;
	if ((ilRC = msgsnd(qcp_output,buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<get_q_info> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<get_q_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		free((char *) buff);		
		dbg(DEBUG,"<get_q_info> ---- END ----");
		return QUE_E_SEND;
	}
	
	free((char *) buff);
	dbg(DEBUG,"<get_q_info> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine allocates an entry for a new route. If the RCB	*/
/* do not have sufficient room for the new entry the RCB is expanded to	*/
/* hold an additional 8 entries.					*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	aloc_route(route,uutil_flag)
int route;
int uutil_flag;
#else
static int	aloc_route(int route,int uutil_flag)
#endif
{
	int	ilRC;
	char	*mptr;			/* A work ptr for malloc */
	ROUTE	*rout;			/* A work pointer for new route */
	ITEM	errbuff;		/* Send Item error buffer	*/
	
	dbg(DEBUG,"<aloc_route> ---- START ----");
	
	/* First check if route is configured */
	
	if (find_route(route) != (ROUTE *)NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = 1;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<aloc_route> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<aloc_route> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}

		/* Entry exists */
		dbg(TRACE,"<aloc_route> %05d entry already exists...", __LINE__);
		dbg(DEBUG,"<aloc_route> ---- END ----");
		return QUE_E_EXISTS;
	} 

	/* Check if space available in the RCB ( Route Control Block ),
	   if not expand it 
	*/
	if (routes->RCB_free < sizeof(ROUTE)) 
	{
		if ((mptr = calloc(1,((unsigned) (routes->RCB_size + ( NBR_ROUTES * sizeof(ROUTE)))))) == NULL)
		{
			dbg(TRACE,"<aloc_route> %05d calloc failure", __LINE__);
			dbg(DEBUG,"<aloc_route> ---- END ----");
			return QUE_E_MEMORY;
		}

		memcpy(mptr,(char *) routes, routes->RCB_size);
		free((char *) routes);
		routes = (RCB *) mptr;
		routes->RCB_size += sizeof(ROUTE) * NBR_ROUTES;
		routes->RCB_free += sizeof(ROUTE) * NBR_ROUTES;
	} 

	/* Add new route entry */
	routes->nbr_entries++;
	rout = (ROUTE *) &(routes->route[routes->nbr_entries]);
	rout->route_ID = route;
	rout->nbr_entries = NUL;
	routes->RCB_free -= sizeof(ROUTE);

	qsort((char *) &(routes->route[1]),routes->nbr_entries,sizeof(ROUTE), compare_IDs);

	/* Check if the route is in the loadsharing range of IDs */
	
	if ((route>=LOAD_SH_LOW) && (route<=LOAD_SH_HIGH))
	{
		/* We have a loadsharing entry, allocate the queue */
		
		aloc_queue(route,"LoadSH");
		dbg(TRACE,"<aloc_route > %05d Loadsharing queue detected %05d", __LINE__,route);
		
	} /* end if */
	
	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = RC_SUCCESS;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<aloc_route> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<aloc_route> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
	}
	else 
	{
		if (uutil_flag == OTHER) 
		{
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<aloc_route> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine removes a route from the system. The RCB is	*/
/* compressed is order to close up the gap after a successful deletion.	*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	dealoc_route(route,uutil_flag)
int route;int uutil_flag;
#else
static int	dealoc_route(int route,int uutil_flag)
#endif
{
	int	ilRC;
	ROUTE	*rout;
	ROUTE	*next_rout;
	int	move_len;
	ITEM	errbuff;
	ITEM	buff;
	
	dbg(DEBUG,"<dealoc_route> ---- START ----");
	/* First locate the route */
	rout = find_route(route);
	if (rout == (ROUTE *) NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = NOT_FOUND;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<dealoc_route> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<dealoc_route> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}
		dbg(DEBUG,"<dealoc_route> ---- END ----");
		return QUE_E_NOFIND;
	} 

	/* Remove the route entry from the RCB, and possibly compress it */
	next_rout = rout + 1;
	move_len = ((char *) routes + routes->RCB_size) - (char *) next_rout;

	/* Compress the RCB if entry was not the last */
	if (move_len > 0) 
	{
		memcpy((char *) rout, (char *) next_rout, move_len);
	}

	routes->nbr_entries--;
	routes->RCB_free += sizeof(ROUTE);
	
	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		buff.msg_header.mtype = (long) item->originator;
		buff.msg_length = 1;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&buff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<dealoc_route> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<dealoc_route> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
	}
	else 
	{
		if (uutil_flag == OTHER) 
		{
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<dealoc_route> ---- END ----");
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine finds a route entry in the RCB. Due to the	*/
/* limited number of entries a simple linear search is used. The 	*/
/* routine returns a pointer to the route entry.			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static ROUTE	*find_route(route)
int route;
#else
static ROUTE	*find_route(int route)
#endif
{
	ROUTE	*ret_value;
	int	i;

	dbg(DEBUG,"<find_route> ---- START ----");
	/* default value */
	ret_value = (ROUTE*)NOT_FOUND;

	/* is route a valid number? */
	if (!route) 
		return ret_value;
	
	/* for all routes */
	for (i=0; i<=routes->nbr_entries; i++) 
	{
		if (routes->route[i].route_ID == route) 
		{
			ret_value = &(routes->route[i]);
			break;
		}
	} 
	dbg(DEBUG,"<find_route> ---- END ----");
	return ret_value;
} 

/* ******************************************************************** */
/*									*/
/* The following routine builds a buffer for a route overview display.	*/
/*									*/
/* ******************************************************************** */
static int	get_r_over()
{
	ROU_INFO	*rou_info;
	ITEM		errbuff;
	RINFO		*rinfo;
	ITEM		*buff;
	int		len;
	int		i;
	int		ilRC;

	dbg(DEBUG,"<get_r_over> ---- START ----");
	/* Allocate the buffer */
	len = sizeof(ITEM)+sizeof(ROU_INFO)+(sizeof(RINFO)*routes->nbr_entries);
	if ((buff = (ITEM *) calloc(1,len)) == NULL)
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = FATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_r_over> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_r_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_r_over> ---- END ----");
		return QUE_E_MEMORY;
	}

	/* Set the pointers */
	rou_info = (ROU_INFO *) &(buff->text[0]);
	rinfo = &(rou_info->data[0]);

	/* Extract the info data */
	for (i = 1; i <= routes->nbr_entries; i++) 
	{
		rinfo->id = routes->route[i].route_ID;
		rinfo->entries = routes->route[i].nbr_entries;
		memcpy(rinfo->queues,routes->route[i].routine_ID, sizeof(rinfo->queues[0]) * 16);
		rinfo++;
	} 

	rou_info->entries = routes->nbr_entries;

	buff->msg_header.mtype = (long) item->originator;
	buff->function = QUE_ACK;
	buff->route = 0;
	buff->priority = 3;
	buff->msg_length = len - sizeof(ITEM);
	buff->originator = 0;

	/* Send ACK back */
	send_ack(NUL);

	errno = 0;
	if ((ilRC = msgsnd(qcp_output,buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<get_r_over> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<get_r_over> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		free((char *) buff);
		dbg(DEBUG,"<get_r_over> ---- END ----");
		return QUE_E_SEND;
	}

	free((char *) buff);
	dbg(DEBUG,"<get_r_over> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine builds a buffer for a route display.		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	get_r_info(rou)
int rou;
#else
static int	get_r_info(int rou)
#endif
{
	int		ilRC;
	int		len;
	ITEM		*buff;
	ITEM		errbuff;
	ROUTE		*route;

	dbg(DEBUG,"<get_r_info> ---- START ----");
	/* Allocate the buffer */
	len = sizeof(ITEM) + sizeof(ROUTE);
	if ((buff = (ITEM*)calloc(1,len)) == NULL)
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = FATAL;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_r_info> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_r_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		dbg(DEBUG,"<get_r_info> ---- END ----");
		return QUE_E_MEMORY;
	}

	/* Extract the info data */
	route = find_route(rou);
	if (route == (ROUTE *)NOT_FOUND) 
	{
		send_ack(NUL);
		memset((char *)&errbuff,0x00,sizeof(ITEM));
		errbuff.msg_header.mtype = (long) item->originator;
		errbuff.msg_length = NOT_FOUND;
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
		{
			dbg(TRACE,"<get_r_info> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<get_r_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		}
		free((char *) buff);
		dbg(DEBUG,"<get_r_info> ---- END ----");
		return QUE_E_NOFIND;
	}

	memcpy(buff->text,(char *) route,sizeof(ROUTE));
	buff->msg_header.mtype = (long) item->originator;
	buff->function = QUE_ACK;
	buff->route = 0;
	buff->priority = 3;
	buff->msg_length = len - sizeof(ITEM);
	buff->originator = 0;

	/* Send ACK back */
	send_ack(NUL);
		
	errno = 0;
	if ((ilRC = msgsnd(qcp_output,buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<get_r_info> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<get_r_info> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		free((char*)buff);
		dbg(DEBUG,"<get_r_info> ---- END ----");
		return QUE_E_SEND;
	}

	free((char *) buff);
	dbg(DEBUG,"<get_r_info> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine adds a destination to a route.			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	add_dest(rou,que,uutil_flag)
int rou;
int que;
int uutil_flag;
#else
static int	add_dest(int rou,int que,int uutil_flag)
#endif
{
	int	ilRC;
	ITEM	errbuff;
	ITEM	buff;
	ROUTE	*rcb;
	QCB	*psQcb;
	int	len;
	int	i;
	char	q_name[MAX_PRG_NAME+1];

	dbg(DEBUG,"<add_dest> ---- START ----");

	rcb = find_route(rou);
	if (rcb == (ROUTE *)NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = NOT_FOUND;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<add_dest> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<add_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}
		dbg(DEBUG,"<add_dest> ---- END ----");
		return QUE_E_NOFIND;
	} 
	else 
	{
		for (i = 0; i <= rcb->nbr_entries; i++) 
		{
			if (que == rcb->routine_ID[i]) 
			{
				if (uutil_flag == UUTIL) 
				{
					send_ack(NUL);
					memset((char *)&errbuff,0x00,sizeof(ITEM));
					errbuff.msg_header.mtype = (long) item->originator;
					errbuff.msg_length = 1;
					errno = 0;
					if ((ilRC = msgsnd(qcp_output,&errbuff, sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
					{
						dbg(TRACE,"<add_dest> %05d msgsnd returns %d", __LINE__, ilRC);
						dbg(TRACE,"<add_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
					}
				}
				dbg(DEBUG,"<add_dest> ---- END ----");
				return QUE_E_EXISTS;
			} 
		} 
	} 

	/* Check if room for a new destination */
	if (rcb->nbr_entries > 15) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = FATAL;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<add_dest> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<add_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		} 
		dbg(DEBUG,"<add_dest> ---- END ----");
		return QUE_E_FULL;
	} 

	/* Add the new destination and sort the table */
	rcb->routine_ID[rcb->nbr_entries] = que;
	rcb->nbr_entries++;
	qsort((char *) &(rcb->routine_ID[0]),rcb->nbr_entries, sizeof(rcb->routine_ID[0]),compare_IDs);
	
	/* Handle load-sharing routes/queues */
	
	if ((rou>=LOAD_SH_LOW) && (rou<=LOAD_SH_HIGH))
	{
		/* Update the QCB of the secondary queue */
		
		psQcb = find_queue(que);
		
		if (psQcb != (QCB *) NOT_FOUND)
		{
			psQcb->que_type = QTYPE_SECONDARY;
			psQcb->primary_ID = rou;
			
			/* If first destination set the name of the primary queue */
			
			if (rcb->nbr_entries == 1)
			{
				strcpy(q_name,"LS-");
				strncat(q_name,psQcb->name,MAX_PRG_NAME - 5);
				psQcb = find_queue(rou);
				
				if (psQcb != (QCB *) NOT_FOUND)
				{
					strcpy(psQcb->name,q_name);
				} /* end if (Primary queue exists) */
			} /* end if (First destination entry) */
		} /* end if */
	} /* end if */

			
	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		buff.msg_header.mtype = (long) item->originator;
		buff.msg_length = 2;
		len = sizeof(ITEM);
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
		{
			dbg(TRACE,"<add_dest> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<add_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			dbg(DEBUG,"<add_dest> ---- END ----");
			return QUE_E_SEND;
		}
	} 
	else 
	{
		if (uutil_flag == OTHER) 
		{
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<add_dest> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine deletes a destination from a route.		*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int	del_dest(rou,que,uutil_flag)
int rou;
int que;
int uutil_flag;
#else
static int	del_dest(int rou,int que,int uutil_flag)
#endif
{
	ITEM	errbuff;
	ROUTE	*rcb;
	ITEM	buff;
	int	len;
	int	i;
	int	j;
	int	ilRC;

	dbg(DEBUG,"<del_dest> ---- START ----");
	rcb = find_route(rou);
	if (rcb == (ROUTE *)NOT_FOUND) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = NOT_FOUND;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<del_dest> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<del_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}
		dbg(DEBUG,"<del_dest> ---- END ----");
		return QUE_E_NOFIND;
	} 

	/* Delete the destination and compress the table */
	j = -1;
	for (i = 0 ;i < rcb->nbr_entries; i++) 
	{
		if (rcb->routine_ID[i] == que) 
		{
			j = i;
			break;
		}
	} 

	if (j == -1) 
	{
		if (uutil_flag == UUTIL) 
		{
			send_ack(NUL);
			memset((char *)&errbuff,0x00,sizeof(ITEM));
			errbuff.msg_header.mtype = (long) item->originator;
			errbuff.msg_length = -2;
			errno = 0;
			if ((ilRC = msgsnd(qcp_output,&errbuff,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS)
			{
				dbg(TRACE,"<del_dest> %05d msgsnd returns %d", __LINE__, ilRC);
				dbg(TRACE,"<del_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			}
		}
		dbg(DEBUG,"<del_dest> ---- END ----");
		return QUE_E_NOFIND;
	} 
	else 
	{
		for (i = j; i < rcb->nbr_entries; i++) 
		{
			rcb->routine_ID[i] = rcb->routine_ID[i+1];
		}
		rcb->nbr_entries--;
	} 
	
	/* Send ACK back if uutil is calling routine */
	if (uutil_flag == UUTIL) 
	{
		send_ack(NUL);
		buff.msg_header.mtype = (long) item->originator;
		buff.msg_length = 2;
		len = sizeof(ITEM);
		errno = 0;
		if ((ilRC = msgsnd(qcp_output,&buff,len,IPC_NOWAIT)) != RC_SUCCESS) 
		{
			dbg(TRACE,"<del_dest> %05d msgsnd returns %d", __LINE__, ilRC);
			dbg(TRACE,"<del_dest> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
			dbg(DEBUG,"<del_dest> ---- END ----");
			return QUE_E_SEND;
		}
	} 
	else 
	{
		if (uutil_flag == OTHER) 
		{
			send_ack(NUL);
		}
	} 
	dbg(DEBUG,"<del_dest> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following rountine sends an ACK if it's needed. Each routine	*/
/* is allowed to have only one message in the UNIX message facility.    */
/* Therefore we need the send ACK mechanism. The calling routine loses  */
/* the control till an ACK is received.					*/
/*									*/
/* ******************************************************************** */
static void send_ack(int stat)
{
	ITEM		ackb;			/* The "send"-ACK buffer	*/
	QCB		*qcb_ptr;	/* A QCB pointer		*/
	int		ilRC;
	
	memset((char *)&ackb,0x00,sizeof(ITEM));
	ackb.msg_header.mtype = (long)item->originator;
	ackb.function = stat;
	qcb_ptr = find_queue(item->originator);
	errno = 0;
 	if ((ilRC = msgsnd(qcp_attach,&ackb,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<send_ack> %05d msgsnd returns %d originator<%d>", __LINE__, ilRC, item->originator);
		dbg(TRACE,"<send_ack> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));

		/* We had an error sending an ack */
		if (qcb_ptr != (QCB*)NUL) 
		{
			qcb_ptr->status |= S_ACK;
		} 
	}
	else 
	{
		if (qcb_ptr != (QCB*)NUL) 
		{
			qcb_ptr->retry_cnt = 0;
		}
	}
	return;
}

/* ******************************************************************** */
/*									*/
/* like send ack but with an item as parameter (destination of ack) */
/*									*/
/* ******************************************************************** */
static void send_item_ack(int stat, ITEM *prpItem)
{
	ITEM		ackb;			
	QCB		*qcb_ptr;
	int		ilRC;
	
	memset((char *)&ackb,0x00,sizeof(ITEM));
	ackb.msg_header.mtype = (long)prpItem->originator;
	ackb.function = stat;
	qcb_ptr = find_queue(prpItem->originator);
	errno = 0;
 	if ((ilRC = msgsnd(qcp_attach,&ackb,sizeof(ITEM),IPC_NOWAIT)) != RC_SUCCESS) 
	{
		dbg(TRACE,"<send_item_ack> %05d msgsnd returns %d", __LINE__, ilRC);
		dbg(TRACE,"<send_item_ack> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));

		/* We had an error sending an ack */
		if (qcb_ptr != (QCB*)NUL) 
		{
			qcb_ptr->status |= S_ACK;
		} 
	}
	else 
	{
		dbg(TRACE,"ACK sent to que: %d (Primary ID %d)", 
						qcb_ptr->que_ID, qcb_ptr->primary_ID);
		if (qcb_ptr != (QCB*)NUL) 
		{
			qcb_ptr->retry_cnt = 0;
		}
	}
	return;
}


/* ******************************************************************** */
/*									*/
/*    The following routine sends the item with the highest priority	*/
/*    and checks the sum of items of all priorities.			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int send_item(qcb)
QCB *qcb;
#else
static int send_item(QCB *qcb)
#endif
{
	int	count;		/* number of items */
	int	flag;		/* flag if item has been sent */
	int	i;
	int	ilRC;

	dbg(DEBUG,"<send_item> ---- START ----");
	/* init */
	flag = count = 0;
	ilRC = RC_SUCCESS;
	
	/* If the NEXT protocol is used, return if not in NEXTI state */
	
	if ((qcb->protocol == PROTOCOL_NEXT) && !(qcb->status & NEXTI))
	{
		return ilRC;
	} /* end if (Not in NEXT state) */
	
	for (i=0; i<5; i++) 
	{
		if (qcb->qitem[i].nbr_items > 0) 
		{
			count += qcb->qitem[i].nbr_items;
			dbg(DEBUG,"<SEND ITEM> pcb->total:%d, cnt:%d, qitem[%d]:%d", qcb->total, count, i, qcb->qitem[i].nbr_items);
			if (flag == 0) 
			{
				dbg(DEBUG,"<SEND ITEM> calling forward item with: %X, %X", qcb, qcb->qitem[i].first->data);
				if ((ilRC = forward_item(qcb,(ITEM*)qcb->qitem[i].first->data)) != RC_SUCCESS)
				{
					/* this is a fatal error, so go on but show the calling function 
						the error!!
					*/
					dbg(TRACE,"<send_item> %05d forward_item returns: %d", __LINE__, ilRC);
				}
				flag = -1;
			} 
		} 
	}

	if (qcb->total != count && ilRC != TERMINATE) 
	{
		ilRC = que_rebuild(qcb);
	}

	dbg(DEBUG,"<send_item> ---- END ----");
	/* bye bye */
	return ilRC;
}

/* ******************************************************************** */
/*									*/
/* The que_rebuild routine tries to rebuild a destroyed queue, checks	*/
/* the links between the items and calculates the total number of items.*/
/*									*/
/* The following routine is called:					*/
/* - if first item ptr = 0 and an ACK is expected.			*/
/* - if first item ptr = 0 and the last item ptr != 0 and an ACK is	*/
/*   expected.								*/
/* - if total number of items is false.					*/
/*									*/
/* The routine is able to repair the following cases of destroyed	*/
/* queues:								*/
/* - if first item ptr = 0 and the last item ptr != 0.			*/
/* - if one pointer doesn't point to an item.				*/
/* - if the forward links are destroyed and the backward links are ok.	*/
/* - if the backward links are destroyed and the forward links are ok.	*/
/*									*/
/* Empty and not destroyed queues aren't touched.			*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int que_rebuild(qcb)
QCB *qcb;
#else
static int que_rebuild(QCB *qcb)
#endif
{
	Q_ENTRY		*cur_ent;       /* current entry	*/
	Q_ENTRY		*next_ent;	/* next entry		*/
	int		ret;		/* return value		*/
	int		flag;		/* loop flag		*/
	int		i;
	int		ilRC;

	dbg(DEBUG,"<que_rebuild> ---- START ----");

	/* initialisize total[] */
	for (i = 0; i < 5; i++) 
	{
		total[i] = 0;
	}

	qcb->ack_count++;
	dbg(DEBUG,"qcb-ptr in que_rebuild: %x\n",qcb);
	dbg(DEBUG,"ack_count: %d\n",qcb->ack_count);
	if (qcb->ack_count > 9) 
	{
		memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
		sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
		ret = to_msgh(IPRIO_DB,QCP_CONST,0,0,pcgTmpBuf);
#else
		ret = to_msgh(0,QCP_CONST,0,qcb->que_ID,0);
#endif
		que_status(HOLD,0);
		qcb->ack_count = NUL;
		dbg(DEBUG,"<que_rebuild> ---- END ----");
		return ret;
	}
	
	for (i = 0; i < 5; i++) 
	{
		flag = 0;
		first_flag = 0;
		cur_ent = qcb->qitem[i].first;
		if (cur_ent == (Q_ENTRY *)NUL) 
		{
			if (qcb->qitem[i].last != (Q_ENTRY *)NUL) 
			{
				first_flag = 1;

				/* check the backward links */
			  	if ((ret = back_search(qcb,i)) != 0) 
				{
					memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
					sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
					if ((ilRC = to_msgh(IPRIO_DB,QCP_DESTRO,0,0,pcgTmpBuf)) == TERMINATE)
					{
						dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
						dbg(DEBUG,"<que_rebuild> ---- END ----");
						return TERMINATE;
					}
#endif
				} 
			} 
			else 
			{
				continue;
			}
		} 
		else 
		{
			/* check if first ptr points to an item */
		  	if (cur_ent->time_stamp <= time(0)-1440) 
			{	
		 		first_flag = 1;
		  		qcb->qitem[i].first = (Q_ENTRY *) NUL;
		  		if (qcb->qitem[i].last != (Q_ENTRY *)NUL) 
				{
			  		/* check the backward links */
					ret = back_search(qcb,i);
					if (ret) 
					{
						memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
						sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
						if ((ilRC = to_msgh(IPRIO_DB,QCP_DESTRO,0,0,pcgTmpBuf)) == TERMINATE)
						{
							dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<que_rebuild> ---- END ----");
							return TERMINATE;
						}
#else
						if ((ilRC = to_msgh(0,QCP_DESTRO,0,qcb->que_ID,0)) == TERMINATE)
						{
							dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
							dbg(DEBUG,"<que_rebuild> ---- END ----");
							return TERMINATE;
						}
#endif
					} /* end if (back_search) */
		  		}
		  		else 
				{
					continue;
		  		}
			}
			else 
			{
				/* if first ok */
			 	while (flag == 0) 
				{
					total[i]++;
				 	dbg(DEBUG,"\n+1 que_rebuild");
					next_ent = cur_ent->next;
				 	if (next_ent == (Q_ENTRY *)NUL) 
					{
						if (cur_ent != qcb->qitem[i].last)
						{
							mind = cur_ent;

							/* check the backward links */
							ret = back_search(qcb,i);
							if (ret) 
							{
								memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
								sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
								if ((ilRC = to_msgh(IPRIO_DB,QCP_DESTRO,0,0,pcgTmpBuf)) == TERMINATE)
								{
									dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
									dbg(DEBUG,"<que_rebuild> ---- END ----");
									return TERMINATE;
								}
#else
								if ((ilRC = to_msgh(0,QCP_DESTRO,0,qcb->que_ID,0)) == TERMINATE)
								{
									dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
									dbg(DEBUG,"<que_rebuild> ---- END ----");
									return TERMINATE;
								}
#endif
							} /* end if (back_search) */
				 		} 
				 		flag = -1;   
				 		continue;
				 	} 
				 	else 
					{
						/* check if next points to an item */
					  	if (next_ent->time_stamp > time(0)-1440) 
						{
					  		next_ent->prev = cur_ent;
							cur_ent = next_ent;
					  	}
					 	else 
						{
					 		if (qcb->qitem[i].last != (Q_ENTRY *)NUL) 
							{
								mind = cur_ent;
							 	mind->next = (Q_ENTRY *)NUL;

								/* check the backward links */
							  	ret = back_search(qcb,i);
							  	if (ret) 
							  	{
									memset((void*)pcgTmpBuf, 0x00, iTMPBUF_SIZE);
									sprintf(pcgTmpBuf,"%d",qcb->que_ID);
#ifdef UFIS43
									if ((ilRC = to_msgh(IPRIO_DB,QCP_DESTRO ,0,0,pcgTmpBuf)) == TERMINATE)
									{
										dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
										dbg(DEBUG,"<que_rebuild> ---- END ----");
										return TERMINATE;
									}
#else
									if ((ilRC = to_msgh(0,QCP_DESTRO,0,qcb->que_ID,0)) == TERMINATE)
									{
										dbg(TRACE,"<que_rebuild> %05d to_msgh returns: %d", __LINE__, ilRC);
										dbg(DEBUG,"<que_rebuild> ---- END ----");
										return TERMINATE;
									}
#endif
							  	}
						 	} 
							else 
							{
								qcb->qitem[i].last = cur_ent;
								cur_ent->next = (Q_ENTRY *)NUL;
							} 
							flag = -1;
							continue;
					  	}
					}
				}
			}
	  	} 
	}

	/* new calculate of total */
	qcb->total = NUL; 	
	for (i = 0; i < 5; i++) 
	{
		qcb->qitem[i].nbr_items = total[i];
		dbg(DEBUG,"total[%d]: %d\n",i,total[i]);
		qcb->total += total[i];
	} 

	/* check if further items are in the message facility */
	check_msgtype(qcb->que_ID, "que_rebuild");
	
	/* clear ACK flag */
	qcb->status ^= F_ACK;
	dbg(DEBUG,"<que_rebuild> ---- END ----");
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine checks the item links backwards and rebuilds	*/
/* the queue if possible.						*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int back_search(qcb,pr)
QCB *qcb;int pr;
#else
static int back_search(QCB *qcb,int pr)
#endif
{
	Q_ENTRY		*cur_ent;       /* current entry	*/
	Q_ENTRY		*prev_ent;	/* previous entry	*/
	int		flag;		/* loop flag		*/
	int		ret;		/* return value		*/

	dbg(DEBUG,"<back_search> ---- START ----");

	cur_ent = qcb->qitem[pr].last;
	flag = 0;

	if (first_flag == 1) 
	{
		/* check if last points to an item */
		if (cur_ent->time_stamp <= time(0)-1440) 
		{
			/* no Items */
			qcb->qitem[pr].first = (Q_ENTRY *) NUL;
			qcb->qitem[pr].last = (Q_ENTRY *) NUL;
			total[pr] = 0;
			dbg(DEBUG,"<back_search> ---- END ----");
			return FATAL;
		} 
		else 
		{
			total[pr]++;
			while (flag == 0) 
			{
				prev_ent = cur_ent->prev;
				if (prev_ent == (Q_ENTRY *)NUL) 
				{
					qcb->qitem[pr].first = cur_ent;
					dbg(DEBUG,"<back_search> ---- END ----");
					return RC_SUCCESS;
				} 
				else 
				{
					/* check if prev points to an item */
					if (prev_ent->time_stamp > time(0)-1440) 
					{
						/* more Items */
						total[pr]++;
						prev_ent->next = cur_ent;
						cur_ent = prev_ent;
					}
					else 
					{
						qcb->qitem[pr].first = cur_ent;
						dbg(DEBUG,"<back_search> ---- END ----");
						return RC_SUCCESS;
					}
				} 
			} 
		}
	} 
	else 
	{
		/* if first ok  es ist eine Endlosschleife */
		while (flag == 0) 
		{
			if (cur_ent == (Q_ENTRY *)NUL) 
			{
				qcb->qitem[pr].last = mind;
				qcb->qitem[pr].last->prev = mind->prev;
				dbg(DEBUG,"<back_search> ---- END ----");
				return RC_SUCCESS;
			}
			else 
			{
				/* check if current entry is an item */
				if (cur_ent->time_stamp > time(0)-1440) 
				{
					total[pr]++;
				   prev_ent = cur_ent->prev;
				   if (prev_ent != (Q_ENTRY *)NUL) 
					{
						if ( mind == prev_ent ) 
						{
							mind->next = cur_ent;
							cur_ent->prev = mind;
							dbg(DEBUG,"<back_search> ---- END ----");
							return RC_SUCCESS;
						} 
						else 
						{
							prev_ent->next = cur_ent;
							cur_ent = prev_ent;
						}
  				   } 
				   else 
					{
					   mind = cur_ent;
					   ret = comp_item(qcb,pr);
					   if (ret) 
						{
							dbg(DEBUG,"<back_search> ---- END ----");
						   return FATAL;
					   }
					   else 
						{
							dbg(DEBUG,"<back_search> ---- END ----");
						   return RC_SUCCESS;
					   }
				   }
				}
				else 
				{
					mind = cur_ent->next;
					ret = comp_item(qcb,pr);
					if (ret) 
					{
						dbg(DEBUG,"<back_search> ---- END ----");
						return FATAL;
					}
					else 
					{
						dbg(DEBUG,"<back_search> ---- END ----");
						return RC_SUCCESS;
					}
				}
			}
		}
	}
	dbg(DEBUG,"<back_search> ---- END ----");
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine compares the items and rebuilds the queue.	*/
/*									*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int comp_item(qcb,pr)
QCB *qcb;int pr;
#else
static int comp_item(QCB *qcb,int pr)
#endif
{
	Q_ENTRY		*for_ent;	/* entry before */
	Q_ENTRY		*next_ent;	/* next entry */
	int		flag;		/* loop flag */

	dbg(DEBUG,"<comp_item> ---- START ----");

	next_ent = qcb->qitem[pr].first;
	flag = 0;
	
	while (flag == 0) 
	{
		/* comparison */
		if (next_ent == mind) 
		{
			flag = -1;
			next_ent->next = mind->next;
			for_ent = mind->next;
			for_ent->prev = next_ent;
			
			/* total may-be false => new count of total */
			next_ent = qcb->qitem[pr].first;
			total[pr] = 1;
			
			while (next_ent != (Q_ENTRY *)NUL) 
			{
				total[pr]++;
				dbg(DEBUG,"\n+1 total in comp_tem");
				next_ent = next_ent->next;
			}
		}
		else 
		{
			if (next_ent != (Q_ENTRY *)NUL) 
			{
				for_ent = next_ent;
				next_ent = for_ent->next;
			} 
			else 
			{
				for_ent->next = mind;
				mind->prev = for_ent;
				flag = -1;
			} 
		} 
	} 
	dbg(DEBUG,"<comp_item> ---- END ----");
	return RC_SUCCESS;
} 

/* ******************************************************************** */
/*									*/
/* The following routine destroys the queue and after that que_rebuild  */
/* is called. It's only a routine for testing.				*/
/*									*/
/* ******************************************************************** */
#ifdef DESTROY

#ifdef XENIX_PROTO
static int destroy_que(queID,cas)
int queID;int cas;
#else
static int destroy_que(int queID,int cas)
#endif
{
	QCB		*qcb;		/* qcb pointer for 9999		*/
	Q_ENTRY		*prev;		/* previous entry		*/
	Q_ENTRY		*next;		/* next entry			*/
	char		*message[5];	/* item				*/
	int		ret;		/* return value			*/
	int		i;

	dbg(DEBUG,"<destroy_que> ---- START ----");

	/* Add Items for destroying */
	message[0] = "ITEM 1";
	message[1] = "ITEM 2";	
	message[2] = "ITEM 3";
	message[3] = "ITEM 4";
	message[4] = "ITEM 5";

	prio = 2;
	item->priority = prio + 1;	/* actual priority */
	item->originator = 9999;

	for (i = 0; i < 5; i++) 
	{			
		/* send five items */
		item->msg_length = sizeof(message[i]);
		memcpy(item->text,(char *) message[i],item->msg_length);
		if ((ret = add_to_que(BACK,queID)) != RC_SUCCESS)
		{
			rep_int_error(ret, "destroy_que");
			if (ret == TERMINATE)
			{
				dbg(DEBUG,"<destroy_que> ---- END ----");
				return TERMINATE;
			}
		} 
	} 
	
	/* Find the qcb ( from the route ID ) */
	qcb = find_queue(queID);
	if (qcb == (QCB *)NOT_FOUND) 
	{
		dbg(DEBUG,"<destroy_que> ---- END ----");
		return(QUE_E_NOFIND);
	}
	
	dbg(DEBUG,"qcb ptr in que_destroy: %d\n",qcb);
	dbg(DEBUG,"bef.first: %d\n",qcb->qitem[prio].first);
	dbg(DEBUG,"bef.last: %d\n",qcb->qitem[prio].last);

	switch (cas) 
	{
		case	1:
			qcb->qitem[prio].first = (Q_ENTRY *) NUL;
			qcb->qitem[prio].last = (Q_ENTRY *) NUL;
			break;

		case	2:
			qcb->qitem[prio].first = (Q_ENTRY *) NUL;
			prev = qcb->qitem[prio].last->prev;
			prev->prev = (Q_ENTRY *) NUL;
			break;

		case	3:
			qcb->qitem[prio].first = (Q_ENTRY *) NUL;
			prev = qcb->qitem[prio].last->prev;
			prev->time_stamp = time(0)-5000;
			prev->prev = (Q_ENTRY *) NUL;
			break;

		case	4:
			qcb->qitem[prio].first = (Q_ENTRY *) NUL;
			qcb->qitem[prio].last->time_stamp = time(0)-5000;
			break;

		case	5:
			break;	

		case	6:
			next = qcb->qitem[prio].first->next;
			next->next->time_stamp = time(0) - 5000;
			qcb->qitem[prio].last->prev = (Q_ENTRY *) NUL;
			break;

		case	7:
			next = qcb->qitem[prio].first->next;
			next->next = (Q_ENTRY *) NUL;
			break;

		case	8:
			next = qcb->qitem[prio].first->next;
			next->next->time_stamp = time(0) - 5000;
			prev = qcb->qitem[prio].last->prev;
			prev->prev = (Q_ENTRY *) NUL;
			break;

		case	9:
			next = qcb->qitem[prio].first->next;
			next->next = (Q_ENTRY *) NUL;
			prev = qcb->qitem[prio].last->prev;
			prev->time_stamp = time(0) - 5000;
			break;

		case	10:
			next = qcb->qitem[prio].first->next;
			next->next = (Q_ENTRY *) NUL;
			qcb->qitem[prio].last->prev = next->next;
			break;

		case	11:
			next = qcb->qitem[prio].first->next;
			next->next->time_stamp = time(0) - 5000;
			prev = qcb->qitem[prio].last->prev;
			prev->prev->time_stamp = time(0) - 5000;
			break;			

		case	12:
			next = qcb->qitem[prio].first->next;
			next->next = (Q_ENTRY *) NUL;
			prev = qcb->qitem[prio].last->prev;
			prev->prev = (Q_ENTRY *) NUL;
			break;

		case	13:
			qcb->qitem[prio].first->time_stamp = time(0) - 5000;
			qcb->qitem[prio].last->prev = (Q_ENTRY *) NUL;
			break;			

		case	14:
			next = qcb->qitem[prio].first->next;
			next->time_stamp = time(0) - 5000;
			qcb->qitem[prio].last = (Q_ENTRY *) NUL;
			break;						

		default	  :
			break;
	} 

	dbg(DEBUG,"beh.first: %d\n",qcb->qitem[prio].first);
	dbg(DEBUG,"beh.last: %d\n",qcb->qitem[prio].last);
		
	dbg(DEBUG,"<destroy_que> ---- END ----");
	return que_rebuild(qcb);
} 
#endif

/* ******************************************************************** */
/*									*/
/* The following routine sends the system status to all ques.	        */
/*									*/
/* ******************************************************************** */
static int	send_status(int action)
{
	int		i;
	int		len;
	ITEM		*buf,*save_item;
	EVENT		*event;

	dbg(DEBUG,"<send_status> ---- START ----");

	/* Disable Que-Control */
	que_ctrl = 0;			

	/* save item pointer for a later ack in main*/
	save_item = item;

	/* Set up the shutdown buffer */
	len = sizeof(ITEM) + sizeof(EVENT);
	if ((buf = (ITEM *)calloc(1,len)) == (ITEM*)NULL) 
	{
		dbg(TRACE,"<send_status> %05d malloc failure", __LINE__);
		dbg(DEBUG,"<send_status> ---- END ----");
		return QUE_E_MEMORY;
	} 

	buf->msg_header.mtype = (long)PRIORITY_2;
	buf->function = QUE_PUT;	
	buf->route = 0;  
	buf->priority = PRIORITY_2;
	buf->msg_length = sizeof(EVENT);
	buf->originator = item->originator; 
	event = (EVENT *) buf->text;
	event->type = SYS_EVENT;
	event->command = action;
	event->originator = QCP_PRIMARY;
	event->retry_count = 0;
	event->data_offset = 0;
	event->data_length = 0;

	/* set temp item pointer */
	item = buf;

	/* Send shutdowns and clear the queues */
	for (i = queues->nbr_queues; i > 0; i--) 
	{
		if ((queues->queue[i].que_ID < 30000) && (queues->queue[i].que_ID > SYS_PRIMARY)) 
		{ 
			item->route =(long)queues->queue[i].que_ID;
			put_item(BACK);
			dbg(DEBUG,"send_status: <%d> sent to <%d>",action,item->route);
		}
	} 
	dbg(DEBUG,"send_status: <%d> sent to all id's",action);

	/* Just for cleaning */
	sleep(3);  

	free(buf);

	/* restore the item pointer */
	item = save_item;
	dbg(DEBUG,"<send_status> ---- END ----");
	return RC_SUCCESS;
}


#ifdef UFIS43
/* ******************************************************************** */
/* fuer den neuem Message Handler (nmghdl)*/
/* ******************************************************************** */
#ifdef XENIX_PROTO
static void	to_msgh(pri,msg_no,route,val,str)
int pri;
int msgno;
int route;
int val;
char *str;
#else
static int to_msgh(int pri,int msg_no,int route,int val,char *str)
#endif

{
  int			ilRC;
  ITEM		*buff			= NULL;
  ITEM		*save_item	= NULL;
  EVENT		*prlEvent	= NULL;
  BC_HEAD	*prlBCHead	= NULL;
  CMDBLK		*prlCmdblk	= NULL;
  MSG_INFO 	*prlMsgInfo	= NULL;
  int	len;		/* length		*/
  int	ret;		/* return value		*/

	dbg(DEBUG,"<to_msgh> ---- START ----");

	/* Test for recursion */
	if (act_msgh == 1) 
	{
		act_msgh = 0;
		dbg(TRACE,"<to_msgh>: act_msgh already exist");
		dbg(DEBUG,"<to_msgh> ---- END ----");
		return RC_SUCCESS;
  	}
  	else
  	{
    	act_msgh = 1;
  	}

  	/* Test existance of msghdl route */
	/*****
  	if (find_route(16384) == (ROUTE*)NOT_FOUND) 
	****/
	if (find_route(MSGHDL_PRIMARY) == (ROUTE*)NOT_FOUND)
  	{
    	act_msgh = 0;
		/* Route does not exist */
		dbg(DEBUG,"<to_msgh> ---- END ----");
		return QUE_E_NOFIND;
  	} 

  	len = sizeof(ITEM) + sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + sizeof(MSG_INFO) + 4;

  	if ((buff = (ITEM*)calloc(1,len)) == NULL)
  	{
    	act_msgh = 0;
    	dbg(TRACE,"<to_msgh>: malloc failed!");
		dbg(DEBUG,"<to_msgh> ---- END ----");
    	return TERMINATE;
  	} 

  	/* Set the pointers */
  	/* Fill in the buffer */
  	buff->function	= QUE_PUT;
	buff->route   	= MSGHDL_PRIMARY;
	/******
  	buff->route 	= 16384 ;
	******/
	if ((pri < 1) || (pri > 5))
	{
		pri = 3;
	} 
  	buff->priority = pri;
  	buff->msg_length = sizeof(EVENT) + sizeof(BC_HEAD) + sizeof(CMDBLK) + sizeof(MSG_INFO);
  	buff->originator = mod_id;

  	prlEvent	= (EVENT*)buff->text;
  	prlEvent->type = SYS_EVENT;
  	prlEvent->command = EVENT_DATA;
  	prlEvent->originator = mod_id;
  	prlEvent->retry_count = 0;
  	prlEvent->data_offset = sizeof(EVENT);
  	prlEvent->data_length = len - sizeof(ITEM);

  	prlBCHead = (BC_HEAD*)((char*)prlEvent+prlEvent->data_offset);
  	prlBCHead->rc = RC_SUCCESS;

  	prlCmdblk = (CMDBLK*)((char*)prlBCHead->data);
  	strcpy(prlCmdblk->command, SMESSAGE_INTERN);
  	sprintf(prlCmdblk->obj_name, "%d", msg_no);

  	prlMsgInfo = (MSG_INFO*)((char*)prlCmdblk->data);
  	prlMsgInfo->lang_code[0] = '\0';

  	if (IsUserMsg(msg_no))
  		prlMsgInfo->msg_origin = IUSER_MSG;
  	else if (IsConfMsg(msg_no))
  		prlMsgInfo->msg_origin = ICONFLICT_MSG;
  	else if (IsErrorMsg(msg_no))
  		prlMsgInfo->msg_origin = IERROR_MSG;
  	else
  	{
		/* bei einer sinnlosen nummer setzen wir error message... */
  		prlMsgInfo->msg_origin = IERROR_MSG;
  	}

  	prlMsgInfo->msg_val = val;
  	strncpy(prlMsgInfo->prog_name, mod_name, PROG_NAME_LEN);
  	*(prlMsgInfo->prog_name + PROG_NAME_LEN) = '\0';

  	prlMsgInfo->msgblk.msg_no       = msg_no;
  	prlMsgInfo->msgblk.prio         = pri; 
  	prlMsgInfo->msgblk.flight_no[0] = '\0';
  	prlMsgInfo->msgblk.rot_no[0]	 = '\0';	
  	prlMsgInfo->msgblk.ref_fld[0]	 = '\0';

  	strncpy(prlMsgInfo->msgblk.msg_data, str, MSG_DATA_LEN);
  	*(prlMsgInfo->msgblk.msg_data + MSG_DATA_LEN) = '\0';

  	save_item = item;
  	item = buff;

  	if ((ilRC = put_item(BACK)) != RC_SUCCESS)
	{
		dbg(TRACE,"<to_msgh> %05d put_item returns: %d", __LINE__, ilRC);
	}

  	item = save_item;
  	free((char *) buff);
  	act_msgh = 0;

	dbg(DEBUG,"<to_msgh> ---- END ----");
  	return ilRC;
} 
#else
/* ******************************************************************** */
/*                                                                      */
/* The following routine forwards a message to the message handler.     */
/* Achtung: wenn 'CEDA_PRG' dann wird der Message an den neuen msghdl   */
/*          geschickt. send_message wird von send_error_msg abgeloest.  */
/*          NOCH PRUEFEN: PRIO fuer send_error_msg                      */
/*                                                                      */
/* ******************************************************************** */
#ifdef XENIX_PROTO
static int to_msgh(pri,msgno,route,val,str)
int pri;
int msgno;
int route;
int val;
char *str;
#else
static int to_msgh(int pri,int msgno,int route,int val,char *str)
#endif
{
	ITEM  *buff;
	ITEM  *save_item;
	EVENT *event;
	MSG_INFO * msg_info;
	int   len;            /* length               */
	int   ret;            /* return value         */
	int	ilRC;

	dbg(DEBUG,"<to_msgh> ---- START ----");
	/* Test for recursion */
	if (act_msgh == 1)
	{
		act_msgh = 0;
		dbg(TRACE,"<to_msgh>: act_msgh already exist");
		dbg(DEBUG,"<to_msgh> ---- END ----");
		return RC_SUCCESS;
	} 
	else
	{
		act_msgh = 1;
	} 

	/* Test existance of msghdl route */
	if (find_route(MSGHDL_PRIMARY) == (ROUTE *)NOT_FOUND)
	{
		act_msgh = 0;
		dbg(DEBUG,"<to_msgh> ---- END ----");
		return QUE_E_NOFIND;
	} 

	len = sizeof(ITEM) + sizeof(EVENT) + sizeof(MSG_INFO) + 1;
	if ((buff = (ITEM *)calloc(1,len)) == NULL)
	{
		act_msgh = 0;
		dbg(TRACE,"<to_msgh>: malloc failed!");
		dbg(DEBUG,"<to_msgh> ---- END ----");
		return TERMINATE;
	}

	/* Set the pointers */
	event    = (EVENT *) buff->text;
	msg_info = (MSG_INFO *) ((char *) event + sizeof(EVENT));
	
	/* Fill in the buffer */
	buff->function	= QUE_PUT;
	buff->route   	= MSGHDL_PRIMARY;

	if ((pri < 1) || (pri > 5))
	{
		pri = 3;
	} 

	buff->priority = pri;
	buff->msg_length = sizeof(EVENT) + sizeof(MSG_INFO);
	buff->originator = mod_id;

	event->type = SYS_EVENT;
	event->command = EVENT_DATA;
	event->originator = mod_id;
	event->retry_count = NUL;
	event->data_offset = sizeof(EVENT);
	event->data_length = sizeof(MSG_INFO);

	strncpy(msg_info->prog_name, mod_name, PROG_NAME_LEN);
	*(msg_info->prog_name + PROG_NAME_LEN) = '\0';

	msg_info->msg_val = val;
	msg_info->msg_origin = ORIGIN_ERROR_MSG;    /* nur SEND_ERROR_MSG */
	msg_info->lang_code[0]        = '\0';                 /* init. */

	msg_info->msgblk.flight_no[0]= '\0';          /* init. */
	msg_info->msgblk.rot_no[0]    = '\0';         /* init. */
	msg_info->msgblk.ref_fld[0]   = '\0';         /* init. */
	msg_info->msgblk.msg_no = msgno;
	msg_info->msgblk.prio = PRIO_ERROR_FILE;      /* def. */
	strncpy(msg_info->msgblk.msg_data, str, MSG_DATA_LEN);
	*(msg_info->msgblk.msg_data + MSG_DATA_LEN) = '\0';

	save_item = item;
	item = buff;

	if ((ilRC = put_item(BACK)) != RC_SUCCESS)
	{
		dbg(TRACE,"<to_msgh> %05d put_item returns: %d", __LINE__, ilRC);
	}

	item = save_item;
	free((char *) buff);
	act_msgh = 0;

	dbg(DEBUG,"<to_msgh> ---- END ----");
	return ilRC;
} 
#endif

/* ******************************************************************** */
/* The following routine writes the system status into shared memory.   */
/* ******************************************************************** */
static int write_sts_shm(int ipStatus,char *pcpShmKey)
{
	int ilRC = RC_SUCCESS;
	int ilLoop = 0;
	char clStatusText[64];
	char clShmRecord[72];
	STHBCB bcb;
	SRCDES rlSearchDescriptor;
	unsigned long ret = STNORMAL;

	dbg(DEBUG,"<write_sts_shm> ---- START ----");

	memset(clStatusText,0x00,64);
	memset(clShmRecord,0x00,72);

	sprintf(clShmRecord,"%s",pcpShmKey);

	rlSearchDescriptor.nobytes = strlen(clShmRecord);
	rlSearchDescriptor.increment = 0;

	for (ilLoop = 0; ilLoop < rlSearchDescriptor.nobytes; ilLoop++)
	{
		rlSearchDescriptor.bitmask |= (BITVHO >> ilLoop);
		rlSearchDescriptor.data[ilLoop] = clShmRecord[ilLoop];
	}

	bcb.function = (STRECORD|STSEARCH|STLOCATE);
	bcb.inputbuffer = (char *)&rlSearchDescriptor;
	bcb.outputbuffer = clShmRecord;
	bcb.tabno = HSTAB;
	bcb.recno = 0;

	ret = sth(&bcb, &gbladr);
	if (ret == STNORMAL)
	{
		dbg(DEBUG,"write_sts_shm: rc=<%d> old status at rec<%d>",ret,bcb.recno);
		ilRC = sts2status(&ipStatus,clStatusText);
		dbg(DEBUG,"write_sts_shm: <%d> = <%s>",ipStatus,clStatusText);
		sprintf(clShmRecord,"%s",pcpShmKey);
		strcat(clShmRecord,clStatusText);
		dbg(DEBUG,"write_sts_shm: record <%d> = <%s>",bcb.recno,clShmRecord);

		bcb.function = (STRECORD|STWRITE);
		bcb.inputbuffer = clShmRecord;
		bcb.outputbuffer = NULL;
		bcb.tabno = HSTAB;

		ret = sth(&bcb, &gbladr);
		if (ret == STNORMAL)
		{
			dbg(DEBUG,"write_sts_shm: rc=<%d> new status rec<%d>",ret,bcb.recno);
		}
		else
		{
			dbg(DEBUG,"write_sts_shm: sth write failed <%d>",ret);
			dbg(DEBUG,"<write_sts_shm> ---- END ----");
			return RC_FAIL;
		} 
	}
	else
	{
		dbg(TRACE,"write_sts_shm: sth read failed <%d>",ret);
		dbg(DEBUG,"<write_sts_shm> ---- END ----");
		return RC_FAIL;
	} 

	dbg(DEBUG,"<write_sts_shm> ---- END ----");
	/* bye bye */
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine writes the system status into shared memory.   */
/*									*/
/* ******************************************************************** */
static int write_status(int ipStatus)
{
	int ilRC = RC_SUCCESS;
	unsigned int ilLoop = 0;
	char clStatusText[64];
	char clShmRecord[72];
	STHBCB bcb;
	SRCDES rlSearchDescriptor;
	unsigned long ret = STNORMAL;

	dbg(DEBUG,"<write_status> ---- START ----");

	memset(clStatusText,0x00,64);
	memset(clShmRecord,0x00,72);

	sprintf(clShmRecord,"_STATUS_");

	rlSearchDescriptor.nobytes = strlen(clShmRecord);
	rlSearchDescriptor.increment = 0;

	for (ilLoop = 0; ilLoop < rlSearchDescriptor.nobytes; ilLoop++)
	{
		rlSearchDescriptor.bitmask |= (BITVHO >> ilLoop);
		rlSearchDescriptor.data[ilLoop] = clShmRecord[ilLoop];
	}
 
	bcb.function = (STRECORD|STSEARCH|STLOCATE);
	bcb.inputbuffer = (char *)&rlSearchDescriptor;
	bcb.outputbuffer = clShmRecord;
	bcb.tabno = HSTAB;
	bcb.recno = 0;

	ret = sth(&bcb, &gbladr);
	if (ret == STNORMAL)
	{
		dbg(DEBUG,"write_status: rc=<%d> old status at rec<%d>",ret,bcb.recno);
		ilRC = sts2status(&ipStatus,clStatusText);
		dbg(DEBUG,"write_status: <%d> = <%s>",ipStatus,clStatusText);
		sprintf(clShmRecord,"_STATUS_");
		strcat(clShmRecord,clStatusText);
		dbg(DEBUG,"write_status: record <%d> = <%s>",bcb.recno,clShmRecord);

		bcb.function = (STRECORD|STWRITE);
		bcb.inputbuffer = clShmRecord;
		bcb.outputbuffer = NULL;
		bcb.tabno = HSTAB;

		ret = sth(&bcb, &gbladr);
		if (ret == STNORMAL)
		{
			dbg(DEBUG,"write_status: rc=<%d> new status at rec<%d>",ret,bcb.recno);
		}
		else
		{
			dbg(DEBUG,"write_status: sth write failed <%d>",ret);
			dbg(DEBUG,"<write_status> ---- END ----");
			return RC_FAIL;
		} 
	}
	else
	{
		dbg(TRACE,"write_status: sth read failed <%d>",ret);
		dbg(DEBUG,"<write_status> ---- END ----");
		return RC_FAIL;
	} 

	dbg(DEBUG,"<write_status> ---- END ----");
	/* bye babe */
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine clears a queue specified by queID */
/*									*/
/* ******************************************************************** */
static int StoreQueueItems(int ipMsgQue)
{
	int						ilRC;
	int						ilLen;
	int						ilCurItem;
	int						ilQueType;
	struct msqid_ds 		rlQueStatus;
	ITEM						*prlItem = NULL;
	QCB						*prlQCB = NULL;

	dbg(DEBUG,"<StoreQueueItems> ----- START -----");

	/* first get que-type */
	ilQueType = QUE_TYPE(ipMsgQue);
	if (ilQueType == -1)
	{
		dbg(TRACE,"<StoreQueueItems> %05d invalid queue type...");
		return TERMINATE;
	}

	/* first read current queue status */
	errno = 0;
	if ((ilRC = msgctl(ipMsgQue, IPC_STAT, &rlQueStatus)) != 0)
	{
		/* this is an error */
		dbg(TRACE,"<StoreQueueItems> %05d msgctl returns with an error", __LINE__);
		dbg(TRACE,"<StoreQueueItems> %05d errno: %d -> <%s>", __LINE__, errno, strerror(errno));
		dbg(DEBUG,"<StoreQueueItems> ----- END -----");
		return TERMINATE;
	}
	
	/* here we know the no. of items on this queue */
	dbg(TRACE,"<StoreQueueItems> %05d %d item(s) on <%s>", __LINE__, rlQueStatus.msg_qnum, QUE_TEXT(ipMsgQue));

	/* get memory for all structures */
	if ((prgSaveItem[ilQueType] = (struct save_items*)realloc(prgSaveItem[ilQueType], (((igSaveItemCnt[ilQueType]+(int)rlQueStatus.msg_qnum)*sizeof(struct save_items))+sizeof(long)))) == NULL)
	{
		/* fatal error */
		dbg(TRACE,"<StoreQueueItems> %05d can't allocate memory for save-item structure...", __LINE__);
		dbg(DEBUG,"<StoreQueueItems> ----- END -----");
		return TERMINATE;
	}

	/* Allocate the QCP input buffer */
	if ((prlItem = (ITEM*)malloc(I_SIZE+sizeof(long))) == NULL)
	{
		dbg(TRACE,"<StoreQueueItems> %05d can't allocate memory for QCP input buffer", __LINE__);
		dbg(DEBUG,"<StoreQueueItems> ---- END ----");
		return TERMINATE;
	}

	/* read all que items and store them internally */
	for (ilCurItem=0; ilCurItem<(int)rlQueStatus.msg_qnum; ilCurItem++)
	{
		errno = 0;
		if ((ilRC = msgrcv(ipMsgQue, prlItem, I_SIZE, 0L, IPC_NOWAIT)) < RC_SUCCESS) 
		{
			dbg(TRACE,"<StoreQueueItems> %05d msgrcv returns %d", __LINE__, ilRC);
			dbg(TRACE,"<StoreQueueItems> %05d errno: %d <-> strerror <%s>", __LINE__, errno, strerror(errno));
		} 
		else 
		{
			/* it's like a normal proceeding */
			if (ipMsgQue == qcp_input)
			{
				dbg(DEBUG,"sending ack to %d", prlItem->originator);
				send_item_ack(NUL, prlItem);
			}

			/* in this case msgrcv returns with success */
			/* msgrcv returns the number of bytes placed in mtext... */
			ilLen = ilRC;

			/* copy the data itself */
			memcpy((void*)prgSaveItem[ilQueType][igSaveItemCnt[ilQueType]].mtext, (void*)&prlItem->msg_header.mtype, ilLen);

			/* set current position */
			igSaveItemCnt[ilQueType]++;
		} 
	}

	/* delete QCP-Input buffer */
	if (prlItem != NULL)
	{
		free((void*)prlItem);
		prlItem = NULL;
	}

	/* bye babe */
	dbg(DEBUG,"<StoreQueueItems> ----- END -----");
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine clears a queue specified by queID */
/*									*/
/* ******************************************************************** */
static int GetStoredItems(int ipMsgQue, struct msgbuf **prpItem)
{
	int	ilQueType;

	dbg(DEBUG,"<GetStoredItems> ----- START -----");

	/* check unix-queue */
	ilQueType = QUE_TYPE(ipMsgQue);
	if (ilQueType == -1)
	{
		dbg(TRACE,"<GetStoredItems> %05d invalid queue type...", __LINE__);
		dbg(TRACE,"<GetStoredItems> ----- END -----");
		return TERMINATE;
	}
		
	/* FIFO, recover the item */
	dbg(DEBUG,"<GetStoredItems> %05d this is recoverd item no: %d", __LINE__, igCurItemCnt[ilQueType]);
	*prpItem = (struct msgbuf*)&prgSaveItem[ilQueType][igCurItemCnt[ilQueType]];

	/* decrement global counter */
	--igSaveItemCnt[ilQueType];	

	/* increment current item cunter */
	igCurItemCnt[ilQueType]++;

	dbg(DEBUG,"<GetStoredItems> ----- END -----");
	/* ciao */
	return RC_SUCCESS;
}

/* ******************************************************************** */
/*									*/
/* The following routine delete the 'save' memory */
/*									*/
/* ******************************************************************** */
static int ReleaseMemory(void)
{
	int	i;
	int	ilQueType;
	int	item_len;
	ITEM	*prlItem = NULL;

	dbg(DEBUG,"<ReleaseMemory> ----- START -----");

	for (i=0; i<3; i++)
	{
		switch (i)
		{
			case 0:
				dbg(DEBUG,"<ReleaseMemory> %05d this is qcp_output...", __LINE__);
				ilQueType = QUE_TYPE(qcp_output);

				/* are there items? */
				while (igSaveItemCnt[ilQueType] > 0)
				{
					dbg(DEBUG,"<ReleaseMemory> %05d igSaveItemCnt[ilQueType]: %d", __LINE__, igSaveItemCnt[ilQueType]);
					GetStoredItems(qcp_output, (struct msgbuf**)&prlItem);
					item_len = sizeof(ITEM) + prlItem->msg_length - sizeof(long);
					dbg(DEBUG,"<ReleaseMemory> item_len is: %d", item_len);
					nap(2);
					msgsnd(qcp_output, prlItem, item_len, IPC_NOWAIT);
				}
				/* to delete the memory in all cases */
				igSaveItemCnt[ilQueType] = 0; 
				break;

			case 1:
				dbg(DEBUG,"<ReleaseMemory> %05d this is qcp_input...", __LINE__);
				ilQueType = QUE_TYPE(qcp_input);
				break;

			case 2:
				dbg(DEBUG,"<ReleaseMemory> %05d this is qcp_attach...", __LINE__);
				ilQueType = QUE_TYPE(qcp_attach);
				break;
		}

		/* check unix-queue */
		if (ilQueType == -1)
		{
			dbg(TRACE,"<ReleaseMemory> %05d invalid queue type...", __LINE__);
			dbg(TRACE,"<ReleaseMemory> ----- END -----");
			return TERMINATE;
		}

		if (!igSaveItemCnt[ilQueType])
		{
			/* free memory */
			if (prgSaveItem[ilQueType])
			{
				dbg(DEBUG,"<ReleaseMemory> %05d free SaveItem[%d]-Structure", __LINE__, ilQueType);
				free((void*)prgSaveItem[ilQueType]);
				prgSaveItem[ilQueType] = NULL;

				/* reset counter */
				igCurItemCnt[ilQueType] = 0;
			}
			else
				dbg(DEBUG,"<ReleaseMemory> %05d SaveItem[%d] is NULL", __LINE__, ilQueType);
		}
		else
			dbg(DEBUG,"<ReleaseMemory> %05d SaveItemCnt[%d] is: %d", __LINE__, ilQueType, igSaveItemCnt[ilQueType]);
	}

	dbg(DEBUG,"<ReleaseMemory> ----- END -----");
	/* bye bye */
	return RC_SUCCESS;
}

/****************************************************************************/
/*									    */
/* The following subroutines is part of the loadsharing implementation	    */
/*									    */
/****************************************************************************/

/* ********************************************************************	*/
/*									*/
/*  find_empty_secondary(int ilQueID)					*/
/*  This subroutine searches for an empty secondary queue.		*/
/*  The queue ID is in the reserved range of IDs and the corresponding	*/
/*  Route entry holds the queue ID of the secondary queues.		*/
/*  The routine returns a pointer to the QCB of the first empty queue,	*/
/*  or a NULL pointer if no empty queue is found.			*/
/*									*/
/* ********************************************************************	*/

QCB *find_empty_secondary(int iQueID)
{
	QCB	*psQCB, *psRc;
	ROUTE	*psRoute;
	int	iCount;
	
	psRoute = find_route(iQueID);
	
	if (psRoute == (ROUTE *) NOT_FOUND)
	{
		dbg(TRACE,"<find_empty_secondary> %05d Route not found %d", __LINE__, iQueID);
		
		psRc = (QCB *) NOT_FOUND;
	} /* end if */
	
	else
	{
		psRc = (QCB *) NOT_FOUND;
		
		for (iCount=0;iCount<psRoute->nbr_entries;iCount++)
		{
			psQCB = find_queue(psRoute->routine_ID[iCount]);
		
			if (psQCB == (QCB *) NOT_FOUND)
			{
				dbg(TRACE,"<find_empty_secondary> %05d Queue not found %d", __LINE__, psRoute->routine_ID[iCount]);
				
			} /* end if */
			
			else
			{
				if ((psQCB->total == 0) && (psQCB->status == (GO | NEXTI)))
				{
					/* We have an empty queue, set Rc and break out */
					
					psRc = psQCB;
					break;
				} /* end if */
			} /* end else */
		} /* end for */
	} /* end else */
	
	return psRc;
} /* end of find_empty_queue */

		
/* ********************************************************************	*/
/*									*/
/*  move_to_secondary(QCB *psQue)					*/
/*  This subroutine moves the next item (if any) from the primary	*/
/*  queue to the sencondary queue pointed at by psQue.			*/
/*									*/
/* ********************************************************************	*/

void move_to_secondary(QCB *psQue)
{
	QCB	*psPrimary;
	Q_ENTRY	*psQEntry, *psWrkPtr;
	int	iCount, iPrio;
	
	psPrimary = find_queue(psQue->primary_ID);
	psQEntry = (Q_ENTRY *) NULL;
	
	if (psPrimary != NOT_FOUND)
	{
		if ((psPrimary->total > 0) && (psPrimary->status & GO))
		{
			/* OK, we have an item to move. Search the priorities */
			
			for(iCount=0;iCount<5;iCount++)
			{
				if (psPrimary->qitem[iCount].nbr_items > 0)
				{
					/* The message is of this priority, move it */
					
					iPrio = iCount;
					psQEntry = psPrimary->qitem[iCount].first;
					
					/* Update primary queue, put next item first on queue */
					
					psWrkPtr = psQEntry->next;
					psPrimary->qitem[iCount].first = psWrkPtr;

					/* If non-zero clear the forward link */
					
					if (psWrkPtr != (Q_ENTRY *) NULL)
					{
						psWrkPtr->prev = (Q_ENTRY *) NULL;
						psPrimary->qitem[iCount].nbr_items--;
					} /* end if */
					
					else
					{
						/* No more items on the queue, cleanup */
						
						psPrimary->qitem[iCount].last = (Q_ENTRY *) NULL;
						psPrimary->qitem[iCount].nbr_items = 0;
					} /* end else */
					
					/* Adjust the total counter */
					
					psPrimary->total--;
					break; /** MCU 20020719: we have to break here to avoid overwriting
										items with higher priority **/
				} /* end if "nbr_items > 0" */
			} /* end for */
			
			/* Processing of the primary queue is complete */
			if (psQEntry != (Q_ENTRY *) NULL)
			{
				/* We have an item, add it to the secondary queue */
				/* Use the ++ operator even if the counters can only be 1 */
				
				psQue->msg_count++;
				psQue->total++;
				psQue->qitem[iPrio].nbr_items++;
				
				/* Add the item to the queue */
				
				psQEntry->next = (Q_ENTRY *) NULL;
				psQEntry->prev = psQue->qitem[iPrio].last;
				
				if (psQue->qitem[iPrio].last != (Q_ENTRY *) NULL)
				{
					psQue->qitem[iPrio].last->next = psQEntry;
				} /* end if */
				
				else
				{
					psQue->qitem[iPrio].first = psQEntry;
				} /* end else */
				
				psQue->qitem[iPrio].last = psQEntry;

				dbg(TRACE,"%05d: Item moved from que %d to que %d Prio %d",__LINE__,
				psQue->que_ID,psPrimary->que_ID,iPrio);
						
			} /* end if psQEntry != (Q_ENTRY *) NULL */
			
		} /* end if psPrimary->total > 0 */
		
	} /* end if psPrimary != NOT_FOUND */
	else
	{
		dbg(TRACE,"<move_to_secondary> %05d Primary queue not found %d", __LINE__, psQue->primary_ID);
	} /* end else "Primary queue not found" */
	
	return;
} /* end of move_to_secondary */

/* ********************************************************************	*/
/*									*/
/*  drop_SHM_item(ITEM *)							*/
/*  This subroutine deletes the SHM segment before the item is dropped.	*/
/*									*/
/* ********************************************************************	*/

void drop_SHM_item(ITEM *psQItem)
{
	SHMADR		*psSHMAdr;	/* pointer to the SHM address structure */
	
	psSHMAdr = (SHMADR *) psQItem->text;
	shmctl(psSHMAdr->ShmId,IPC_RMID, 0);
	dbg(TRACE,"<drop_SHM_item> %05d SHM segment deleted %d", __LINE__, psSHMAdr->ShmId);
	
	return;
} /* end of drop_SHM_item() */
	
/* ********************************************************************	*/
/*									*/
/*  handle_next_protocol(QCB * psQcb, ITEM *psItem)						*/
/*  This subroutine handles the next protocol.				*/
/*									*/
/* ********************************************************************	*/

void handle_next_protocol(QCB *psQcb, ITEM *psItem)
{
	/* Set the states */

	psQcb->status |= NEXTI;
	psQcb->protocol = PROTOCOL_NEXT;
							
	/* Check if we have outstanding ACK */
							
	if (psQcb->status & F_ACK)
	{
		/* Not for NW due to timing problems */

		if (psItem->function != QUE_NEXTNW)
		{
			/* should we add a dump here?? */

			dbg(TRACE,"<MAIN> %05d NEXT. Item on queue %d ", __LINE__, psQcb->que_ID );
		} /* end if */
								
		/* Done */
	} /* end if (Outstanding ACK) */

	else
	{
		/* Check if it is a load-sharing queue, if so, try to get next psItem */

		if (psQcb->que_type == QTYPE_SECONDARY)
		{
			move_to_secondary(psQcb);
		} /* end if */

		if (psQcb->total > 0)
		{
			send_item(psQcb);
		} /* end if */
	} /* end else (Normal NEXT condition) */

	/* If NEXTNW clear the NEXTI flag */

	if ( psItem->function == QUE_NEXTNW)
	{
		psQcb->status ^= NEXTI;
	} /* end if */
	
	return;
} /* end of handle_next_protocol() */
	

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
