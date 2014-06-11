/*
tbthdl_customized.c
*/
extern int extractField(char *pcpFieldVal, char *pcpFieldName, char *pcpFields, char *pcpNewData);

/*int getCodeShare(char *pcpFields, char *pcpData, char (*pcpCodeShare)[LISTLEN])*/
int getCodeShare(char *pcpFields, char *pcpData, _VIAL *pcpCodeShare, char *pcpFormat)
{
    int ili = 0;
    int ilj = 0;
    int ilJCNT = 0;
    char pclTmpALC2[256] = "\0";
    char pclTmp[256] = "\0";
    char pclTmpALC3[256] = "\0";
    char pclTmpALC2Pack[256] = "\0";
    char pclTmpFLNO[1024] = "\0";
    char pclTmpFLNOPack[1024] = "\0";
    char pclTmpflightNo[1024] = "\0";
    char *pclPointer = "\0";
    int ilCount =0;
    int ilRC =0;

    int ilReturnValue = 0;
    short slSqlCursor = 0;
    short slSqlFunc = START;
    char pclFunc[] = "getCodeShare";
    char pclAlc2[20] = "";
    char pclAlc3[20] = "";
    char pclVpfr[20] = "";
    char pclVpto[20] = "";
    char pclNowUTCTime[20] = "\0";
    char pclSqlBuf[1024] = "";
    char pclSqlData[1024] = "";
    char pclSqlSelection[1024] = "";

    char pclXML[1024] = "\0";

    /*
    */
    char pclJCNT[8] = "\0";
    char pclJFNO[LISTLEN] = "\0";

    ilRC = extractField(pclJCNT, "JCNT", pcpFields, pcpData);
    if (ilRC == RC_FAIL)
    {
        ilJCNT = 0;
        dbg(TRACE,"%s The JCNT value<%s> is blank, setting is as zero", pclFunc, pclJCNT);
        return RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s The JCNT value<%s> is valid", pclFunc, pclJCNT);
    }

    /*
    if ( atoi(pclJCNT) == 0)
    {
        dbg(TRACE, "%s The JCNT is zero", pclFunc);
        return RC_SUCCESS;
    }
    */

    ilRC = extractField(pclJFNO, "JFNO", pcpFields, pcpData);
    if (ilRC == RC_FAIL)
    {
        dbg(TRACE,"%s The JFNO value<%s> is invalid", pclFunc, pclJFNO);
        return RC_FAIL;
    }
    else
    {
        dbg(TRACE,"%s The JFNO value<%s> is valid", pclFunc, pclJFNO);
    }

    if(strlen(pclJFNO) == 0 || strncmp(pclJFNO," ",1) == 0 || strncmp(pclJCNT," ",1) == 0 || atoi(pclJCNT) == 0)
    {
        dbg(DEBUG,"JFNO or JCNT is NULL or space or zero");
        return RC_FAIL;
    }
    /**/

    ilJCNT = atoi(pclJCNT);
    pclPointer = pclJFNO;

    /*test*/
    /*
    ilJCNT = 7;
    strcpy(rpAftrec.JFNO,"AMV4071  AZ 3925  ME 6637  NZ 4255  OA 8055           VA 7455");
    pclPointer = rpAftrec.JFNO;
    */

    dbg(DEBUG,"pclPointer<%s>",pclPointer);

    /*strcat(pclXML,"\t<codeshare>\n");*/
    for(ili=0; ili < ilJCNT;ili++)
  	{
	    memset(pclTmpALC2,0,sizeof(pclTmpALC2));
	    memset(pclTmpFLNO,0,sizeof(pclTmpFLNO));
	    memset(pclTmpflightNo,0,sizeof(pclTmpflightNo));
        memset(pclTmpALC2Pack,0,sizeof(pclTmpALC2Pack));

	    strncpy(pclTmpALC2,pclPointer,2);
	    strncpy(pclTmp,pclPointer,3);
	    dbg(DEBUG,"++++++++ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmp);

	    /*pclPointer += 4;*/
	    strncpy(pclTmpFLNO,pclPointer+3,6);

		dbg(DEBUG,"calling TrimRight(pclTmpFLNO)");
		TrimRight(pclTmpFLNO);

	    /*if(strncmp(pclTmpFLNO," ",1) == 0) continue;*/
	    pclPointer += 9;

	    if(strncmp(pclTmpALC2," ",1) != 0 || strncmp(pclTmpFLNO," ",1) != 0 ||
	       strncmp(pclTmp," ",1) != 0)
        {
            dbg(DEBUG,"pclTmp last letter<%c>",pclTmp[strlen(pclTmp)-1]);
            /*ALC2->ALC3*/
            if( pclTmp[strlen(pclTmp)-1] == ' ' )
            {
				memset(pclTmpALC3,0,sizeof(pclTmpALC3));
                ilRC = syslibSearchDbData("ALTTAB","ALC2",pclTmpALC2,"ALC3",pclTmpALC3,&ilCount,"\n");

                dbg(TRACE,"**************The return value of syslibSearchDbData ilRC<%d> ilCount<%d>",ilRC,ilCount);
                dbg(TRACE,"**************Getting ALC3 from basic data <%s>",pclTmpALC3);

                if (ilCount > 1)
                {

                    ilRC = RC_FAIL;
                    memset(pclSqlBuf,0,sizeof(pclSqlBuf));
                    memset(pclSqlData,0,sizeof(pclSqlData));
                    memset(pclSqlSelection,0,sizeof(pclSqlSelection));
                    memset(pclNowUTCTime,0,sizeof(pclNowUTCTime));

                    GetServerTimeStamp("UTC",1,0,pclNowUTCTime);
                    /*dbg(TRACE,"<%s> : Current UTC Time <%s>",pclFunc,pclNowUTCTime);*/

                    sprintf(pclSqlSelection, "WHERE ALC2='%s'",pclTmpALC2);
                    sprintf( pclSqlBuf, "SELECT ALC2,ALC3,VAFR,VATO FROM ALTTAB %s", pclSqlSelection );

                    dbg(TRACE,"^^^^^^^^^^^^pclSqlBuf<%s>",pclSqlBuf);

                    slSqlCursor = 0;
                    slSqlFunc = START;
                    while((ilReturnValue = sql_if(slSqlFunc,&slSqlCursor,pclSqlBuf,pclSqlData)) == DB_SUCCESS)
                    {
                        slSqlFunc = NEXT;
                        //TrimRight(pclSqlData);
                        get_fld(pclSqlData,FIELD_1,STR,20,pclAlc2);
                        get_fld(pclSqlData,FIELD_2,STR,20,pclAlc3);
                        get_fld(pclSqlData,FIELD_3,STR,20,pclVpfr);
                        get_fld(pclSqlData,FIELD_4,STR,20,pclVpto);

                        TrimRight(pclAlc2);
                        TrimRight(pclAlc3);
                        TrimRight(pclVpfr);
                        TrimRight(pclVpto);

                        /*dbg( TRACE, "<%s> SqlData <%s>", pclFunc, pclSqlData );*/
                        dbg( TRACE, "<%s> ALC2<%s> ALC3<%s>,VPFR <%s> VPTO <%s> Date<%s>", pclFunc, pclAlc2, pclAlc3, pclVpfr, pclVpto , pclNowUTCTime);

                        if (strlen(pclVpfr) != 0 && strlen(pclVpto) != 0)
                        {
                            dbg(TRACE,"<%s> VPFR<%s> & VPTO<%s> are not null",pclFunc, pclVpfr, pclVpto);
                            if(strcmp(pclVpfr,pclVpto) >= 0)
                            {
                                dbg(TRACE,"<%s> pclVpfr[%s] > pclVpto[%s] -> Invalid", pclFunc, pclVpfr, pclVpto);
                                continue;
                            }
                        }

                        if( strlen(pclVpfr) != 0 &&  strcmp(pclVpfr,pclNowUTCTime) <= 0)
                        {
                             dbg(TRACE,"<%s> pclVpfr[%s] < pclNowUTCTime[%s]", pclFunc, pclVpfr, pclNowUTCTime);
                             if( strlen(pclVpto) != 0)
                             {
                                if(strcmp(pclNowUTCTime,pclVpto) <= 0)
                                {
                                    dbg(TRACE,"<%s> Today[%s] < VPTO[%s] -> Valid", pclFunc, pclNowUTCTime, pclVpto);
                                    ilRC = RC_SUCCESS;
                                    memset(pclTmpALC3,0,sizeof(pclTmpALC3));
                                    strncpy(pclTmpALC3,pclAlc3,strlen(pclAlc3));
                                    break;
                                }
                                else
                                {
                                    dbg(TRACE,"<%s> Today[%s] > VPTO[%s] -> Invalid", pclFunc, pclNowUTCTime, pclVpto);
                                    continue;
                                }
                            }
                            else
                            {
                                dbg(TRACE,"<%s> VPTO is null -> Valid", pclFunc, pclVpto);
                                ilRC = RC_SUCCESS;
                                memset(pclTmpALC3,0,sizeof(pclTmpALC3));
                                strncpy(pclTmpALC3,pclAlc3,strlen(pclAlc3));
                                break;
                            }
                        }
                    }
                    close_my_cursor(&slSqlCursor);

                    /*ilReturnValue = RunSQL( pclSqlBuf, pclSqlData );
                    if( ilReturnValue != DB_SUCCESS )
                    {

                        dbg(TRACE,"@@@@@@@Not Found\n");
                    }
                    else
                    {
                        char pclAlc3[20] = "";
                        char pclVpfr[20] = "";
                        char pclVpto[20] = "";

                        get_fld(pclSqlData,FIELD_1,STR,20,pclAlc3);
                        get_fld(pclSqlData,FIELD_2,STR,20,pclVpfr);
                        get_fld(pclSqlData,FIELD_3,STR,20,pclVpto);
                        dbg( TRACE, "ALC3<%s>,VPFR <%s> VPTO <%s>", pclAlc3, pclVpfr, pclVpto );
                        //dbg(TRACE,"@@@@@@@pclSqlData<%s>\n",pclSqlData);
                    }
                    */
                }

                if( ilRC == 0 && strlen(pclTmpALC3) == 3 && pclTmpALC3[strlen(pclTmpALC3) - 1] != ' ' )
                {
                    /*sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmpALC3);*/

                    /*
                    if( strcmp(pcpOption,"ALC3") == 0 )*/
                    {
                        strcat(pclTmpALC2Pack,pclTmpALC3);
                    }
                    /*
                    else if( strcmp(pcpOption,"ALC2") == 0 )
                    {
                        strcat(pclTmpALC2Pack,pclTmpALC2);
                    }
                    */

                    dbg(DEBUG,"syslibSearchDbData succed, use ALC3-----ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmpALC3);

                    strcpy((pcpCodeShare+ilj)->pclAlc2,pclTmpALC2);
                    strcpy((pcpCodeShare+ilj)->pclAlc3,pclTmpALC3);
                }
                else
                {
                    dbg(TRACE,"syslibSearchDbData fails or return ALC3 length is not three or ALC3 last leter is space, use ALC2");

                    memset(pclTmpALC3,0,sizeof(pclTmpALC3));
                    strncpy(pclTmpALC3,pclTmpALC2,strlen(pclTmpALC2));

                    /*sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmpALC3);*/
                    strcat(pclTmpALC2Pack,pclTmpALC3);

                    strcpy((pcpCodeShare+ilj)->pclAlc2,pclTmpALC2);

                    dbg(DEBUG,"######ALC2<%s>ALC3<%s>",pclTmpALC2,pclTmpALC3);
                }


                /*sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);
                dbg(DEBUG,"three letter is <%s>\n",pclTmp);
                sprintf(pclTmpflightNo,"\t<flightNo%d>%s%s\t</flightNo%d>\n",ilj+1,pclTmpALC2Pack,pclTmpFLNOPack,ilj+1);
                strcat(pclXML,pclTmpflightNo);*/

                strcat(pclTmpALC2Pack," ");
                strcat(pclTmpALC2Pack,pclTmpFLNO);

                if (ilj == 0)
                {
                    strcat(pclTmpflightNo,pclTmpALC2Pack);
                }
                else
                {
                    strcat(pclTmpflightNo,"/");
                    strcat(pclTmpflightNo,pclTmpALC2Pack);
                }
                strcat(pclXML,pclTmpflightNo);

                strcpy((pcpCodeShare+ilj)->pclFlno,pclTmpFLNO);

                ilj++;

            }
            else
            {/*ALC3*/

                /*sprintf(pclTmpALC2Pack,"\n\t<ALC3>%s</ALC3>\n",pclTmp);
                sprintf(pclTmpFLNOPack,"\t<FLNO>%s</FLNO>\n",pclTmpFLNO);

                dbg(DEBUG,"three letter is <%s>\n",pclTmp);
                sprintf(pclTmpflightNo,"\t<flightNo%d>%s%s\t</flightNo%d>\n",ilj+1,pclTmpALC2Pack,pclTmpFLNOPack,ilj+1);
                strcat(pclXML,pclTmpflightNo);*/

                strcat(pclTmpALC2Pack,pclTmp);
                strcat(pclTmpALC2Pack," ");
                strcat(pclTmpALC2Pack,pclTmpFLNO);

                if (ilj == 0)
                {
                    strcat(pclTmpflightNo,pclTmpALC2Pack);
                }
                else
                {
                    strcat(pclTmpflightNo,"/");
                    strcat(pclTmpflightNo,pclTmpALC2Pack);
                }
                strcat(pclXML,pclTmpflightNo);

                ilj++;
            }
        }
    }
    strcpy(pcpFormat,pclXML);
    dbg(DEBUG,"\n+++<%s>-<%d>+++",pcpFormat, ilj);
    /*
  	strcat(pclXML,"\t</codeshare>\n");
  	dbg(DEBUG,"\n+++<%s>+++",pclXML);
  	*/
  	return ilj;
}
