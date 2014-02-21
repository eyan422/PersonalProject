//v1.4

/*static char pcgFieldList[1024] = "
ARR.URNO AURNO,
DEP.URNO DURNO, 
DEP.RKEY DRKEY,
ARR.ADID AADID, 
DEP.ADID DADID,
ARR.FTYP AFTYP, 
DEP.FTYP DFTYP,
ARR.PSTA PSTA,
DEP.PSTD PSTD,
ARR.STOA STOA,
ARR.ETOA ETOA,
ARR.TMOA TMOA,
ARR.ONBL ONBL,
DEP.STOD STOD,
DEP.ETOD ETOD,
DEP.OFBL OFBL, 
ARR.TIFA TIFA, 
DEP.TIFD TIFD,
ARR.AIRB AIRB";*/
typedef struct 
{
    BOOL ilInit;
    BOOL ilWasSend;
    
    char pclAUrno[16];        /* Arrival URNO */
    char pclDUrno[16];        /* Departure URNO */
    
    char pclDRkey[16];         /* Departure RKEY  */
    
    char pclAAdid[4];        /* Arrival ADID */
    char pclDAdid[4];        /* Departure ADID */
    
    char pclAFtyp[4];        /* Arrival FTYP */
    char pclDFtyp[4];        /* Departure FTYP */
    
    char pclAPsta[16];        /* PSTA = parking stand arrival */
    char pclDPstd[16];        /* PSTD = parking stand departure */
    
    char pclAStoa[16];        /* Arrival STOA */
    char pclAEtoa[16];        /* Arrival ETOA */
    
    char pclATmoa[16];        /* Arrival ATMOA */
    
    char pclAOnbl[16];        /* Arrival ONBL  */
    
    char pclDStod[16];        /* Departure STOD */
    char pclDEtod[16];        /* Departure ETOD */
     
    char pclDOfbl[16];         /* OFBL  */
    
    char pclATifa[16];         /* Arrival TIFA */
    char pclDTifd[16];         /* Departure TIFD  */
} MSG;

typedef struct 
{
	char pclPosi[16];
	char pclStoa[16];
	char pclEtoa[16];
	char pclTmoa[16];
	char pclOnbl[16];
	
	char pclStod[16];
	char pclEtod[16];
	char pclOfbl[16];
	
	char pclRegn[64];
	char pclRkey[64];
	
	char pclTifa[16];         /* Arrival TIFA */
  char pclTifd[16];         /* Departure TIFD  */
}SENT_MSG;
