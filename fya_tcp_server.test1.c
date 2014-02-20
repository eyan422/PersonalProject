/************************************************************************
 *
 *	fya_tcp_server.test1.c	
 *
 *	20130116	DKA	modification of Frank's original tester.
 *				- more logging of which socket we are 
 *				  currently handling.
 *				- SIGUSR1 : toggle ack handling
 *				- SIGUSR2 : toggle heartbeat
 *
 ************************************************************************
 */

#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <errno.h>

static igMsgNoForHeartbeat = 0;
static igMsgNoForAck = 0;

static int igEnableAck = 0;
static int igEnableHb = 0;
static int igEnableMsgIdChk = 0;

static void HandleSigUsr1_ACK (int sig, siginfo_t *siginfo, void *context);
static void HandleSigUsr2_Heartbeat (int sig, siginfo_t *siginfo, void *context);
static char * GetMsgId(char * pcpData,char * pcpMsgId);

static void my_alarm()
{
}

static char log_file_name[256];
static char pcgLogBuf[256] = "\0";

int main( int argc, char *argv[] )
{
	int ilTcpfd, ilListenfd, ilClientfd;
  int ilAttempts = 0;
  int ilMaxAttempt = 5;
  int ilOn = 1;
  int ilPort = 0;
  int ilDuration = 0;
  int ilLen = 0;
  int ilFound = 0;
  int ili = 0;
  int ilMsglen;
  time_t ilStartTime, ilEndTime, ilNowTime,ilLastTime=0;
  fd_set readfds, exceptfds,testfds;
  struct timeval rlTimeout;
  struct sockaddr_in rlSin, rlCSin;
  struct linger rlLinger;
  struct sigaction act_alarm;
  struct sigaction act_default;

  struct sigaction act_usr1_ack;
  struct sigaction act_usr1_hb;

  char pclMessage[8192] = "\0";
  char pclSeparetorBegin[100] = "----------------Begin----------------\n";
  char pclSeparetorEnd[100] = "----------------End----------------\n";
  char pclClientIP[100] = "\0";
  char pclTmpStr[100] = "\0";
  int fd = 0;
  char pclTmpStr1[12] = "\0";
  
  char pclDataBuf[4096] = "\0";
  char pclDataBufAck[4096] = "\0";
  
  char pclMsgId[32] = "\0";
  int nread = 0;
  char buffer[2048] = "\0";
  
  char clAckFormatF[256] = "\0";
  static char pcgSendAckFormat[1024] = "\0";
  char clHeartbeatFormatF[256];
  char clTestFltUpd[256] = "\0";
  
  static char pcgSendHeartbeatFormat[1024] = "\0";
  static char pcgSendTestFlightFormat[4096] = "\0";
  
  pclTmpStr1[0] = 0x04;
	pclTmpStr1[1] = '\0';
	
	char pclMessageId[16] = "\0";
	char pclFlightUrno[16] = "\0";
  
	FILE    *fplFp;
	FILE 		*fplLogALL;
	FILE 		*fplLogDATA;
	
	sprintf(log_file_name,"%s/%s-%.5ld-ALL.txt",getenv("DBG_PATH"),argv[0],getpid());
	fplLogALL = fopen(log_file_name,"w");
	
	sprintf(log_file_name,"%s/%s-%.5ld-DATA.txt",getenv("DBG_PATH"),argv[0],getpid());
	fplLogDATA = fopen(log_file_name,"w");

  if( argc < 3 )
  {
      printf( "Usage: tcpserver Port Duration Ack Heartbeat CheckMsgId SendTestFlightMsgId SendTestFlightURNO\n" );
      printf( "if SendTestFlightMsgId and SendTestFlightURNO are null, then server will filling them a value automaticlly\n" );
      sprintf(pcgLogBuf,"Usage: tcpserver Port Duration Ack Heartbeat CheckMsgId SendTestFlightMsgId SendTestFlightURNO\n");
      fwrite(pcgLogBuf,sizeof(char),strlen(pcgLogBuf),fplLogALL);
      exit( -1 );
  }
  
  sprintf(clAckFormatF,"%s/%s",getenv("CFG_PATH"),"hweACK_server.txt");
  printf("clAckFormatF<%s>",clAckFormatF);
  if ((fplFp = (FILE *) fopen(clAckFormatF,"r")) == (FILE *)NULL)
  {    
          printf("SendAck Format File <%s> does not exist",clAckFormatF);
          return -1;
  }    
  else 
  {    
          ilLen = fread(pcgSendAckFormat,1,1023,fplFp);
          
//printf ("Check 1\n");
          //pcgSendAckFormat[ilLen] = '\0';
          
          strcat( pcgSendAckFormat, pclTmpStr1);
 //printf ("Check 2\n");         
 
 					//sprintf(pclDataBufAck,pcgSendAckFormat,igMsgNoForAck++,"1","2");
          //sprintf(pclDataBufAck,pcgSendAckFormat,igMsgNoForAck++,"1","2",igMsgNoForAck++);
          //sprintf(pclDataBufAck,pcgSendAckFormat,"3","1","2",igMsgNoForAck++);
          
          fclose(fplFp);
  }
  
  //printf ("Check 7\n");
  sprintf(clHeartbeatFormatF,"%s/%s",getenv("CFG_PATH"),"hweheartbeat_server.txt");
  printf("clHeartbeatFormatF<%s>",clHeartbeatFormatF);
//printf ("Check 8\n");
  if ((fplFp = (FILE *) fopen(clHeartbeatFormatF,"r")) == (FILE *)NULL)
  {    
          printf("SendHeartbeat Format File <%s> does not exist",clHeartbeatFormatF);
          return -1;
  }    
  else 
  {    
          ilLen = fread(pcgSendHeartbeatFormat,1,1023,fplFp);
          //pcgSendHeartbeatFormat[ilLen] = '\0';
          
          strcat( pcgSendHeartbeatFormat, pclTmpStr1);
          //printf ("Check 10\n");
          fclose(fplFp);
  }
  
  sprintf(clTestFltUpd,"%s/%s",getenv("CFG_PATH"),"hweTestFlightUpdate_server.txt");
  printf("clTestFltUpd<%s>",clTestFltUpd);
  if ((fplFp = (FILE *) fopen(clTestFltUpd,"r")) == (FILE *)NULL)
  {    
          printf("SendTestFlight Format File <%s> does not exist",clTestFltUpd);
          return -1;
  }    
  else 
  {    
          ilLen = fread(pcgSendTestFlightFormat,1,1023,fplFp);
          //pcgSendHeartbeatFormat[ilLen] = '\0';
          
          strcat( pcgSendTestFlightFormat, pclTmpStr1);
          //printf ("Check 10\n");
          fclose(fplFp);
  }
  
//printf ("Check 3\n");
  ilPort = atoi( argv[1] );
  ilDuration = atoi( argv[2] );
  ilStartTime = time( 0 );
  ilEndTime = ilStartTime + (ilDuration * 60);
 //printf ("Check 4\n");	
  printf( "\nGoing to run TCP at port <%d> for <%d> minutes\n", ilPort, ilDuration );
  printf( "here 1\n");
  printf( "Start Time <%19.19s>\n", ctime( &ilStartTime ) );
  ilStartTime = ilEndTime;
  printf( "End Time <%19.19s>\n", ctime( &ilStartTime ) );


  igEnableAck = atoi(argv[3]);
  printf( "ACK option<%d>\n",igEnableAck);

  igEnableHb =  atoi(argv[4]);
  printf( "Heartbeat option<%d>\n",igEnableHb);
  
  igEnableMsgIdChk = atoi(argv[5]);
  printf( "MsgIdCheck option<%d>\n",igEnableMsgIdChk);
  
  if( strlen(argv[6])!=0 )
  {
  	if( atoi(argv[6])!=0 )
  	{
  		strcpy(pclMessageId,argv[6]);
		}
		else
		{
			strcpy(pclMessageId,"MsgIdNotInvalid");
		}
	}
	else
	{
		strcpy(pclMessageId,"MsgIdNotSpecified");
	}
	
	if( strlen(argv[7])!=0)
	{
		if( atoi(argv[7])!=0 )
		{
			strcpy(pclFlightUrno,argv[7]);
  	}
  	else
  	{
  		strcpy(pclFlightUrno,"FlightUrnoInvalid");
  	}
  }
  else
	{
		strcpy(pclFlightUrno,"FlightUrnoNotSpecified");
	}
  
  memset (&act_usr1_ack,'\0',sizeof(act_usr1_ack));
  act_usr1_ack.sa_sigaction =  &HandleSigUsr1_ACK;
  act_usr1_ack.sa_flags =  SA_SIGINFO;
  if (sigaction (SIGUSR1,&act_usr1_ack,NULL) < 0)
  {
    printf ("*** ERROR setting signal handler for SIGUSR1 ***\n");
  }

  memset (&act_usr1_hb,'\0',sizeof(act_usr1_hb)); 
  act_usr1_hb.sa_sigaction =  &HandleSigUsr2_Heartbeat;
  act_usr1_hb.sa_flags =  SA_SIGINFO;
  if (sigaction (SIGUSR2,&act_usr1_hb,NULL) < 0)
  {
    printf ("*** ERROR setting signal handler for SIGUSR2 ***\n");
  }

  ilTcpfd = socket( AF_INET, SOCK_STREAM, PF_UNSPEC );
  if( ilTcpfd < 0 )
  {
     printf( "Unable to create socket! ERROR!\n" );
     exit( -2 );
  }

	rlSin.sin_family = AF_INET;
  rlSin.sin_port = htons(ilPort);
  rlSin.sin_addr.s_addr = htonl(INADDR_ANY);
  rlLinger.l_onoff = 0;


  ilAttempts = 0;

  setsockopt( ilTcpfd, SOL_SOCKET, SO_LINGER,			(char *)&rlLinger, sizeof(rlLinger) );
  setsockopt( ilTcpfd, SOL_SOCKET, SO_REUSEADDR,	(char *)&ilOn, sizeof(int) );
  setsockopt( ilTcpfd, SOL_SOCKET, SO_KEEPALIVE, 	(char *)&ilOn, sizeof(int) );
  

  while( bind( ilTcpfd, (struct sockaddr *)&rlSin, sizeof(rlSin) ) < 0 &&
         ilAttempts < ilMaxAttempt )
  {
      ilAttempts++;
      printf( "Error (%s)in binding!! Number of attempt = %d\n", strerror(errno), ilAttempts );
      //sleep( 1 );
  }

  if( ilAttempts >= ilMaxAttempt )
  {
      printf( "Still cannot bind! EXIT!!\n" );
      exit( -3 );
  }
  printf( "Port %d is binded successfully\n", ilPort );


  ilAttempts = 0;
  while( ((ilListenfd = listen( ilTcpfd, 5 )) == -1) && ilAttempts < ilMaxAttempt )
  {
      ilAttempts++;
      printf( "Error (%s)in listening!! Number of attempt = %d\n", strerror(errno), ilAttempts );
      //sleep( 1 );
  }

  if( ilAttempts >= ilMaxAttempt )
  {
      printf( "Still cannot listen! EXIT!!\n" );
      exit( -4 );
  }
  printf( "listen successfully\n" );
	
  ilNowTime = time(0); 
  ili = 0;
  
  FD_ZERO( &readfds );
  FD_ZERO( &exceptfds );
  FD_SET( ilTcpfd, &readfds );
  FD_SET( 0, &readfds );
  rlTimeout.tv_sec = 0;  /* blocked for 60 seconds */
  rlTimeout.tv_usec = 0;  /* blocked for 60 seconds */
  
  //while(1)
  while( ilNowTime <= ilEndTime )
  {
		ili++;
    ilNowTime = time(0);
    
    testfds = readfds;
	
		//printf("server waitting\n");
		ilFound = select(FD_SETSIZE, (fd_set *)&testfds, (fd_set *)NULL, (fd_set *)&exceptfds, &rlTimeout );
		
		//printf ("ilFound<%d>\n",ilFound); 
		
		if( ( ilFound == -1 ) && (errno != EINTR))
	  {

	      ilNowTime = time(0);
	      printf( "%19.19s: (%8d)ERROR %d(%s)!! SELECT fails!\n", (char *)ctime(&ilNowTime), ili, errno, strerror(errno) );
	      exit( -4 );
	  }
		if( ( ilFound == -1 ) && (errno == EINTR))
	  {
	      printf ("Processing a signal\n"); 
              ilFound = 0; /* so that loop will continue */
	  }
		
		if(ilFound == 0)
		{
			ilNowTime = time(0);
	    //printf( "%19.19s: (%8d)No msg received,timeout  usleep(500);\n", (char *)ctime(&ilNowTime), ili );
	    //sleep(5);
	    usleep(500);
	    //continue;
		}
	
		for(fd = 0; fd < FD_SETSIZE; fd++)
		{
			if(FD_ISSET(fd,&testfds))
			{
				if(fd == ilTcpfd) //new connecting
				{
					//printf ("======== event on server listen sock fd [%d] ====\n",fd);
					//ilClientfd = 0;
					memset( &rlCSin, 0, sizeof(rlCSin) );
					ilClientfd = accept( ilTcpfd, (struct sockaddr *)&rlCSin, &ilLen );
					FD_SET(ilClientfd,&readfds);
					printf("======= adding client on fd <%d> =======\n",ilClientfd);
				}
				else if(fd == 0) //input from keyboard
				{ 
					ioctl(0,FIONREAD,&nread);
					if(nread == 0)
					{
						printf("keyboard done\n");
						exit(10);
					}
					else
					{
						memset(buffer,0,sizeof(buffer));
						nread = read(0,buffer,nread);
						buffer[nread] = 0;
						printf("read <%d> from keyboard: <%s>",nread,buffer);
						
						//pclTmpStr1[0] = 0x04;
						//pclTmpStr1[1] = '\0';
						strcat( buffer, pclTmpStr1);
						
						if( send( ilClientfd, buffer, strlen(buffer), 0 ) > 0 )
						{
							printf( "fd<%d>Len = %d: SEND (%s)\n", fd,strlen(buffer), buffer );
						}
						else
						{
							printf( "ERROR in sending\n" );
						}
						//sleep( 1 );
					}
				}
				else  //communication between server and client
				{
					//printf ("======== event on client conn fd [%d] ====\n",fd);
					memset( pclMessage, 0, sizeof(pclMessage) );
					ilMsglen = recv( fd, pclMessage, sizeof(pclMessage), MSG_DONTWAIT);
					ilNowTime = time(0);
					sprintf( pclTmpStr, "%19.19s", (char *)ctime(&ilNowTime) );
					
					//printf("Writting to logALL\n");
					
					fwrite(pclSeparetorBegin,sizeof(char),strlen(pclSeparetorBegin),fplLogALL);
					fwrite(pclMessage,sizeof(char),strlen(pclMessage),fplLogALL);
					fwrite(pclSeparetorEnd,sizeof(char),strlen(pclSeparetorEnd),fplLogALL);
					
					if( ilMsglen > 0 )
					{
						printf( "%19.19s: Len = %d: RECV (%s)\n", pclTmpStr, ilMsglen, pclMessage );
						
						//printf("Writting to logDATA\n");
						
						fwrite(pclSeparetorBegin,sizeof(char),strlen(pclSeparetorBegin),fplLogDATA);
						fwrite(pclMessage,sizeof(char),strlen(pclMessage),fplLogDATA);
						fwrite(pclSeparetorEnd,sizeof(char),strlen(pclSeparetorEnd),fplLogDATA);
					}
					else if( ilMsglen==0 )
					{
						//printf("Received a empty message\n");
						usleep(500);
					}
					else
					{
						printf( "%19.19s: ERROR in receiving\n", pclTmpStr );
					}
					//sleep( 1 );
					
					if(ilMsglen > 0)
					{
						if(igEnableAck == 1)
						{
							if(strstr(pclMessage,"ack") == 0 && strstr(pclMessage,"heart") == 0)
							{
								//pclTmpStr1[0] = 0x04;
								//pclTmpStr1[1] = '\0';
								//strcat( pcgSendAckFormat, pclTmpStr1);
								
								//if( send( fd, pcgSendAckFormat, strlen(pcgSendAckFormat), 0 ) > 0 )
								
								memset(pclMsgId,0,sizeof(pclMsgId));
								if(igEnableMsgIdChk == 1)
								{
									printf("igEnableMsgIdChk is enable\n");
									GetMsgId(pclMessage,pclMsgId);
								}
								else
								{
									printf("igEnableMsgIdChk is not enable\n");
									strcpy(pclMsgId,"0");
								}
								
								printf("pclMsgId<%s>",pclMsgId);
								sprintf(pclDataBufAck,pcgSendAckFormat,"0","1","2",pclMsgId,"1");
								//sprintf(pclDataBufAck,pcgSendAckFormat,"0","1","2","77","1");
								
								if( send( fd, pclDataBufAck, strlen(pclDataBufAck), MSG_DONTWAIT ) > 0 )
								{
									printf( "Len = %d: SEND-ACK (%s)\n", strlen(pclDataBufAck), pclDataBufAck );
								}
								else
								{
									printf( "ERROR in sending ACK message\n" );
								}
								//sleep( 1 );
							}
						}
						else
						{
							printf("The ack is not enable\n");
						}
					}
					else
					{
						//printf("ilMsglen(%d)<=0",ilMsglen);
					}
					
					//sleep(1);
					
					ilNowTime = time(0);
					//printf("ilNowTime<%d>ilLastTime<%d>\n",ilNowTime,ilLastTime);
					
					//if(ilNowTime - ilLastTime > 5)
					{
						//printf("ilNowTime<%d> - ilLastTime<%d> > 5\n",ilNowTime,ilLastTime);
						
						memset(pclDataBuf,0,sizeof(pclDataBuf));
						sprintf(pclDataBuf,pcgSendHeartbeatFormat,"0","1");
						//sprintf(pclDataBuf,pcgSendHeartbeatFormat,"1");
						
						//printf("+++++++Checking Point+++++++\n");
						
						if(ilMsglen > 0)
						{
							if(igEnableHb == 1)
							{
								//pclTmpStr1[0] = 0x04;
								//pclTmpStr1[1] = '\0';
								//strcat( pcgSendHeartbeatFormat, pclTmpStr1);
								if( send( fd, pclDataBuf, strlen(pclDataBuf), MSG_DONTWAIT ) > 0 )
								{
									//ilLastTime = time(0);
									printf( "Len = %d: SEND-heartbeat (%s)\n",strlen(pclDataBuf),pclDataBuf );
									
									sleep(1);
									
									memset(pclDataBuf,0,sizeof(pclDataBuf));
									
									sprintf(pclDataBuf,pcgSendTestFlightFormat,pclMessageId,pclFlightUrno);
									if( send( fd, pclDataBuf, strlen(pclDataBuf), MSG_DONTWAIT ) > 0 )
									{
										printf( "Len = %d: SEND-FlightUpdate (%s)strlen(pclDataBuf)<%d>\n",strlen(pclDataBuf),pclDataBuf,strlen(pclDataBuf) );
									}
									else
									{
										printf( "ERROR in sending FlightUpdate message\n" );
									}
									
								}
								else
								{
									printf( "ERROR in sending Heartbeat message\n" );
								}
							}
							else
							{
								printf("The heartbeat is not enable\n");
							}
						}
						else
						{
							//printf("ilMsglen(%d)<=0",ilMsglen);
							usleep(500);
						}
					}
					//else
					{
						//printf("ilNowTime<%d> - ilLastTime<%d> <= 5\n",ilNowTime,ilLastTime);
					}
					
					//ilLastTime = time(0);
				}
			}
		}
  }
  fclose(fplLogALL);
  fclose(fplLogDATA);
  /*
  close( ilClientfd );
  close( ilListenfd );
  close( ilTcpfd );
	*/
}

/**********************************************************************
 * signal handlers
 **********************************************************************
 */

static void HandleSigUsr1_ACK (int sig, siginfo_t *siginfo, void *context)
{
  igEnableAck = (igEnableAck == 0) ? 1 : 0; 
}

static void HandleSigUsr2_Heartbeat (int sig, siginfo_t *siginfo, void *context)
{
  igEnableHb = (igEnableHb == 0) ? 1 : 0; 

}

static char * GetMsgId(char * pcpData,char * pcpMsgId)
{
	int ilI = 0;
	char pclMessageIdTemp[4096] = "\0";
	char *pclFunc = "GetMsgId";
	char pclMsgId[32] = "\0";
	
	memset(pclMsgId,0,sizeof(pclMsgId));
	memset(pclMessageIdTemp,0,sizeof(pclMessageIdTemp));
	
	strcpy(pclMessageIdTemp,strstr(pcpData,"messageId=\""));
	
	//printf("pclMessageIdTemp<%s>\n",pclMessageIdTemp);
	
 	for(ilI = 0; ilI < (strlen(pclMessageIdTemp)-strlen("messageId=\"")); ilI++)
	{
		if( *(pclMessageIdTemp+strlen("messageId=\"")+ilI) != '\"')
		{
			printf("ilI<%d>\n",ilI);
			strncpy( (pclMsgId+ilI),(pclMessageIdTemp+strlen("messageId=\"")+ilI),1 );
		}
		else
		{
			break;
		}
	}
 
	if( strlen(pclMsgId)>0 && atoi(pclMsgId)>=1 && atoi(pclMsgId)<=65535)
	{
		printf("%s MessageId: <%d>\n",pclFunc,atoi(pclMsgId));
	}
 	else
 	{
 		printf("%s pclMsgId<%s> is invalid which shoud be >= 1\n",pclFunc,pclMsgId);
 	}
 	
 	strcpy(pcpMsgId,pclMsgId);
}
