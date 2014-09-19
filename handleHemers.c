int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData)
{
    char *pclFunc = "extractField";

    int ilItemNo = 0;

    ilItemNo = get_item_no(pcpFields, pcpFieldName, 5) + 1;
    if (ilItemNo <= 0)
    {
        dbg(TRACE, "<%s> No <%s> Found in the field list, return", pclFunc, pcpFieldName);
        return RC_FAIL;
    }

    get_real_item(pcpFieldVal, pcpNewData, ilItemNo);
    dbg(TRACE,"<%s> The New %s is <%s>", pclFunc, pcpFieldName, pcpFieldVal);

    return RC_SUCCESS;
}

static int getFltId(FLIGHT_ID *rpFltId, char *pcpFieldList, char *pcpDataList)
{
	int ilRc = RC_FAIL;
	char *pclFunc = "getFltId";
	char pclTmp[256] = "\0";
	
	memset(rpFltId,0,sizeof(rpFltId));
	
	ilRc = extractField(pclTmp, "ALC3", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The ALC3 value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The ALC3 value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclAlc3,pclTmp);
	}
	
	ilRc = extractField(pclTmp, "FLNO", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The FLNO value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The FLNO value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclFlno,pclTmp);
	}
	
	ilRc = extractField(pclTmp, "ARRDEP", pcpFieldList, pcpDatalist);
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The ARRDEP value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The ARRDEP value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclAdid,pclTmp);
	}
	
	if (strncmp(pclAdid,"A",1) == 0)
	{
		ilRc = extractField(rpFltId->pclStoad, "STOA", pcpFieldList, pcpDatalist);
	}
	else 
	{
		ilRc = extractField(rpFltId->pclStoad, "STOD", pcpFieldList, pcpDatalist);
	}
	if (ilRc == RC_FAIL)
	{
		dbg(TRACE,"%s The STOAD value<%s> is invalid", pclFunc, pclTmp);
		return RC_FAIL;
	}
	else
	{
		dbg(TRACE,"%s The STOAD value<%s> is valid", pclFunc, pclTmp);
		strcpy(rpFltId->pclStoad,pclTmp);
	}
	
	return RC_SUCCESS;
}

static int getFlightUrno(char *pcpUrno, FLIGHT_ID rlFltId)
{
	char *pclFunc = "getFlightUrno";
	char pclSqlBuf[2048] = "\0";
    char pclSqlData[2048] = "\0";
	
	memset(pclSqlData,0,sizeof(pclSqlData));
	memset(pclSqlBuf,0,sizeof(pclSqlBuf));
	if (strncmp(rlFltId.pclAdid,"A",1) == 0)
	{
		sprintf(pclSqlBuf, "SELECT URNO from AFTTAB WHERE ALC3='%s' AND FLNO='%s' AND STOA='%s' AND ADID='%s'",rlFltId.pclAlc3, rlFltId.pclFlno, rlFltId.pclStod, rlFltId->pclAdid);
	}
	else
	{
		sprintf(pclSqlBuf, "SELECT URNO from AFTTAB WHERE ALC3='%s' AND FLNO='%s' AND STOD='%s' AND ADID='%s'",rlFltId.pclAlc3, rlFltId.pclFlno, rlFltId.pclStod, rlFltId->pclAdid);
	}
	
	ilRc = RunSQL(pclSqlBuf, pclSqlData);
	if (ilRc != DB_SUCCESS)
	{
		dbg(TRACE, "<%s>: Retrieving flight urno - Fails", pclFunc);
		return RC_FAIL;
	}
	switch(ilRc)
	{
		case NOTFOUND:
			dbg(TRACE, "<%s> Retrieving flight urno - Not Found", pclFunc);
			ilRc = NOTFOUND;
			break;
		default:
			dbg(TRACE, "<%s> Retrieving flight urno - Found <%s>", pclFunc, pclSqlData);
			strcpy(pcpUrno,pclSqlData);
			ilRc = RC_SUCCESS;
			break;
	}
	return ilRc;
}

static void getHermesFieldList(char *pcphermesDataList, char *pcpFlightDataList, char *pcpFlightFieldList, char *pcpFieldList, char *pcpDataList, char *pcpHermesFields)
{
	int ilFieldListNo = 0;
    int ilDataListNo = 0;
	int ilEleCount = 0;
	char *pclFunc = "getHermesFieldList";
	char pclTmpFieldName[64] = "\0";
    char pclTmpData[64] = "\0";
	
	ilFieldListNo = GetNoOfElements(pcpFieldList, ',');
	ilDataListNo  = GetNoOfElements(pcpDataList, ',');

	if (ilFieldListNo != ilDataListNo)
	{
		dbg(TRACE,"%s The data<%d> and field<%d> list number is not matched", pclFunc, ilDataListNo, ilFieldListNo);
		return RC_FAIL;
	}
	
	for(ilEleCount = 1; ilEleCount <= ilFieldListNo; ilEleCount++)
	{
		get_item(ilEleCount, pcpFieldList, pclTmpFieldName, 0, ",", "\0", "\0");
		TrimRight(pclTmpFieldName);

		get_item(ilEleCount, pcpDataList, pclTmpData, 0, ",", "\0", "\0");
		TrimRight(pclTmpData);
		
		if (strstr(pcpHermesFields, pclTmpFieldName) != 0)
		{
			if (strlen(pcpFlightFieldList) == 0 )
			{
				strcat(pcpFlightFieldList, pclTmpFieldName);
				strcat(pcpFlightDataList,  pclTmpData);
			}
			else
			{
				strcat(pcpFlightFieldList,",");
				strcat(pcpFlightFieldList, pclTmpFieldName);
				strcat(pcpFlightDataList,",");
				strcat(pcpFlightDataList,  pclTmpData);
			}
		}
		else
		{
			if (strlen(pcpHemersFieldList) == 0 )
			{
				strcat(pcphermesDataList, pclTmpData);
			}
			else
			{
				strcat(pcphermesDataList,",");
				strcat(pcphermesDataList,  pclTmpData);
			}
		}
	}
}

static int handleHemers(char *pcpSecName, char *pcpFieldList, char *pcpDataList, char *pcpDestModId, int ipPrio, char *pcpCmd, char *pcpTable)
{
	char *pclFunc = "handleHemers";
	char pclUrno[16] = "\0";
	char pclHemersFieldList[1024] = "\0";
	char pclFlightFieldList[1024] = "\0";
	FLIGHT_ID rlFltId;
	
	/*
	herhdl 20140903074903:092:SECTION <FIS_Flight> CONTAINS 14 DATA TAGS
	herhdl 20140903074903:092:SECTION TRIGGER NAME <OUT>
	herhdl 20140903074903:093:FIELDS <ALC3,FLNO,STOA,ARRDEP,ACT3,REGN,TCAR,TMAI,LCAR,TRCAR,TSCAR,LMAI,TRMA,TSMA>
	herhdl 20140903074903:093:DATA <ETD,407,20140301103000,A,320,A6ETB,12473,0,12473,0,2222,0,0,0>
	herhdl 20140903074903:093:MOD_ID <7800>
	herhdl 20140903074903:093:PRIO <3>
	herhdl 20140903074903:093:CMD <UFR>
	herhdl 20140903074903:093:TABLE <AFTTAB>
	*/
	
	/*1-Get flight identification*/
	if (getFltId(&rlFltId, pcpFieldList, pcpDataList) == RC_FAIL)
	{
		dbg(TRACE,"%s Not all flt_id are valid->return",pclFunc);
		return RC_FAIL;
	}
	
	/*2-Search flight*/
	if (getFlightUrno(pclUrno, rlFltId) != RC_SUCCESS)
	{
		dbg(TRACE,"%s Flight is not found, return",pclFunc);
		return RC_FAIL;	
	}
	dbg(TRACE,"%s Found flight URNO<%s>",pclFunc, pclUrno);
	
	/*3-Get hermes field list*/
	getHermesFieldList(pclHemersDataList, pclFlightDataList, pclFlightFieldList, pcpFieldList, pcpDataList, pcgAfttabFields, pcgHermesFields);
	
	dbg(TRACE,"%s pcgHermesFields<%s>",pclFunc,pcgHermesFields);
	dbg(TRACE,"%s pclHemersDataList<%s>",pclFunc,pclHemersDataList);
	dbg(TRACE,"%s pclFlightDataList<%s>",pclFunc,pclFlightDataList);
	dbg(TRACE,"%s pclFlightFieldList<%s>",pclFunc,pclFlightFieldList);
}