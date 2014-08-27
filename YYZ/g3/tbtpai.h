#ifndef TBTPAI_H
#define TBTPAI_H

#define VIAL_LEN 121

typedef struct
{
	char pclMsgHeader[3];
	/*int  ilObjId;
	int ilMsgId;
	int ilMsgNo;
	*/
	char pclObjId[16];
	char pclMsgId[16];
	char pclMsgNo[16];
	char pclMsgType[16];
}_HEAD;

typedef struct
{
	char pclMPC[16];
	char pclMPN[16];
	char pclMPX[16];
}_MASTER;

typedef struct
{
	char pclPLC[16];
	char pclPLN[16];
	char pclPLX[16];
	char pclSDT[16];
	char pclSTA[16];
	char pclSST[16];
	char pclMPC[16];
	char pclMPN[16];
	char pclMPX[16];
	char pclStop1[16];
	char pclStop2[16];
	char pclStop3[16];
	char pclStop4[16];
	char pclTYP[16];
	char pclTYS[16];
	char pclLDA[16];
	char pclLTA[164];
	char pclNA[16];
	char pclCA1[16];
	char pclGAT[16];
	char pclRmkTermId[16];
	char pclRmkFltStatus[16];
	char pclTimeSync[16];
	char pclPreGate[16];
	char pclOverbookedCompensation[16];
	char pclFreFlyerMiles[16];
	char pclFltDelayRsnCode[16];
	char pclDaysOfOper[16];
	char pclEffDate[16];
	char pclDisDate[16];
}_ARR_MSG_BODY;

typedef struct
{
    char pclPLC[16];
	char pclPLN[16];
	char pclPLX[16];
	char pclSDT[16];
	char pclSTD[16];
	char pclSST[16];
	char pclMPC[16];
	char pclMPN[16];
	char pclMPX[16];
	char pclStop1[16];
	char pclStop2[16];
	char pclStop3[16];
	char pclStop4[16];
	char pclTYP[16];
	char pclTYS[16];
	char pclLDD[16];
	char pclLTD[16];
	char pclNA[16];
	char pclGAT[16];
	char pclRmkTermId[16];
	char pclRmkFltStatus[16];
	char pclTimeSync[16];
	char pclPreGate[16];
	char pclOverbookedCompensation[16];
	char pclFreFlyerMiles[16];
	char pclFltDelayRsnCode[16];
    char pclTimeDuration[16];
    char pclMealService[16];
	char pclDaysOfOper[16];
	char pclEffDate[16];
	char pclDisDate[16];
}_DEP_MSG_BODY;
#endif
