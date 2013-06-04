#include <stdio.h>
#include <stdlib.h>

#define NORMAL_COLUMN_LEN  32
#define BIG_COLUMN_LEN     256
#define REM1LEN            256
#define REM1LEN            256
#define VIALLEN		   8192

typedef struct
{
    char URNO[NORMAL_COLUMN_LEN];
    char ALC3[NORMAL_COLUMN_LEN];
    char FLTN[NORMAL_COLUMN_LEN];
    char FLNO[NORMAL_COLUMN_LEN];
    char STOD[NORMAL_COLUMN_LEN];
    char STOA[NORMAL_COLUMN_LEN];
    char CKIF[NORMAL_COLUMN_LEN];
    char CKIT[NORMAL_COLUMN_LEN];
    char GA1F[NORMAL_COLUMN_LEN];
    char GA1B[NORMAL_COLUMN_LEN];
    char GA1E[NORMAL_COLUMN_LEN];
    char ACT3[NORMAL_COLUMN_LEN];
    char ONBL[NORMAL_COLUMN_LEN];
    char OFBL[NORMAL_COLUMN_LEN];
    char ONBE[NORMAL_COLUMN_LEN];
    char STYP[NORMAL_COLUMN_LEN];
    char TTYP[NORMAL_COLUMN_LEN];
    char ADID[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char PSTA[NORMAL_COLUMN_LEN];
    char PSTD[NORMAL_COLUMN_LEN];
    char GTA1[NORMAL_COLUMN_LEN];
    char GTD1[NORMAL_COLUMN_LEN];
    char ETAI[NORMAL_COLUMN_LEN];
    char ETDI[NORMAL_COLUMN_LEN];
    char WRO1[NORMAL_COLUMN_LEN];
    char WRO2[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char ETOD[NORMAL_COLUMN_LEN];
    char ETOA[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char LAND[NORMAL_COLUMN_LEN];
    char AIRB[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char CTOT[NORMAL_COLUMN_LEN];
    char REGN[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char TMOA[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char PAXT[NORMAL_COLUMN_LEN];
    
    //Frank v1.1
    char BLT1[NORMAL_COLUMN_LEN];
    char BLT2[NORMAL_COLUMN_LEN];
    //Frank v1.1
    
    char VIA3[NORMAL_COLUMN_LEN];
    char CSGN[NORMAL_COLUMN_LEN];
    char FTYP[NORMAL_COLUMN_LEN];
    char NXTI[NORMAL_COLUMN_LEN];
    
    char VIAN[NORMAL_COLUMN_LEN];
    char VIAL[VIALLEN];
    char AURN[NORMAL_COLUMN_LEN];
    char RTOW[NORMAL_COLUMN_LEN];
    char TGA1[NORMAL_COLUMN_LEN];
    char SLOU[NORMAL_COLUMN_LEN];
    char JFNO[VIALLEN];
    char JCNT[NORMAL_COLUMN_LEN];
} AFTREC;

int main()
{
	char pclXML[8192];
	AFTREC rlAftrec;
	memset(&rlAftrec,0,sizeof(rlAftrec));	
	
	PackCodeShare(rlAftrec,pclXML);
	return 0;
}



int PackCodeShare(AFTREC rpAftrec, char * pclXML)
{
	int ili = 0;
	int ilJCNT = 0;
	char pclTmpALC3[256] = "\0";
	char pclTmpALC3Pack[256] = "\0";
	char pclTmpFLNO[1024] = "\0";
	char pclTmpFLNOPack[1024] = "\0";
	char pclTmpflightNo[1024] = "\0";
	//char pclXML[8192]="\0";
	char *pclPointer = "\0";
	
	//test
	/*
	ilJCNT = atoi(rpAftrec.JCNT);
	pclPointer = rpAftrec.JFNO;
	*/
	ilJCNT = 7;
	strcpy(rpAftrec.JFNO," AB 4071  AZ 3925  ME 6637  NZ 4255  OA 8055           VA 7455");
	pclPointer = rpAftrec.JFNO;
	
	//dbg(DEBUG,"<\n%s\n>",pclPointer);
	
	printf("<%s>",pclPointer);
	
	strcat(pclXML,"<codeshare>\n");
	for(ili=0; ili < ilJCNT;ili++)
  {
		memset(pclTmpALC3,0,sizeof(pclTmpALC3));
		memset(pclTmpFLNO,0,sizeof(pclTmpFLNO));
		memset(pclTmpflightNo,0,sizeof(pclTmpflightNo));
		
		strncpy(pclTmpALC3,pclPointer+1,2);
		//pclPointer += 4;
		strncpy(pclTmpFLNO,pclPointer+4,4);
		
		//if(strncmp(pclTmpFLNO," ",1) == 0) continue;
		
		if(ili == 0)
			pclPointer += 9;
		else
			pclPointer += 9;
		
		if(strncmp(pclTmpALC3," ",1) != 0 || strncmp(pclTmpFLNO," ",1) != 0)
		{
		sprintf(pclTmpALC3Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmpALC3);
		sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);
			
		//dbg(DEBUG,"three letter is <%s>\n",pclTmp);
		sprintf(pclTmpflightNo,"<flightNo%d>%s%s</flightNo%d>\n",ili+1,pclTmpALC3Pack,pclTmpFLNOPack,ili+1);
		strcat(pclXML,pclTmpflightNo);
  }
	}
  strcat(pclXML,"</codeshare>");
  printf("\n+++<%s>+++",pclXML);
  return 0;	
}
