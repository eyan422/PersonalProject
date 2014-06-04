/*
tbthdl_sub.c

For subroutine
*/

static int defaultOperator(char *pcpDestValue, char *pcpSourceValue)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "defaultOperator";
    memset(pcpDestValue,0,sizeof(pcpDestValue));
    dbg(TRACE, "%s The operator is blank -> default action",pclFunc);

    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;

    /*if(sizeof(pcpDestValue) > strlen(pcpSourceValue))*/
    {
        strcpy(pcpDestValue, pcpSourceValue);
        ilRC = RC_SUCCESS;
    }
    /*
    else
    {
        dbg(TRACE,"%s The dest length is less than the source one -> do not copy",pclFunc);
        ilRC = RC_FAIL;
    }
    */
    return ilRC;
}

static int sub(char *pcpDestValue, char *pcpSourceValue, char *pcpCon1, char *pcpCon2, char *pcpDestLen)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "sub";
    char pclTmp[64] = "\0";
    memset(pcpDestValue,0,sizeof(pcpDestValue));

    if (strlen(pcpSourceValue) == 0)
        return RC_FAIL;

    if (atoi(pcpDestLen) == 0 || atoi(pcpCon2) == 0)
        return RC_FAIL;

    if (atoi(pcpCon1) == atoi(pcpCon2))
        return RC_FAIL;

    strncpy(pclTmp, pcpSourceValue + atoi(pcpCon1), atoi(pcpDestLen));

    /*dbg(TRACE,"%s source value<%s>", pclFunc, pcpSourceValue);
    dbg(TRACE,"%s pclTmp value<%s>", pclFunc, pclTmp);

    if( atoi(pcpDestLen) >= (atoi(pcpCon2) - atoi(pcpCon1) + 1) )*/
    {
        /*dbg(TRACE,"%s <%d> == <%d>", pclFunc, atoi(pcpDestLen), atoi(pcpCon2) - atoi(pcpCon1) + 1);
        strncpy(pcpDestValue, pcpSourceValue+atoi(pcpCon1), atoi(pcpDestLen));*/
        strcpy(pcpDestValue, pclTmp);
        ilRC = RC_SUCCESS;
    }

    dbg(TRACE,"%s after dest value<%s>", pclFunc, pcpDestValue);
    /*
    else
    {
        dbg(TRACE,"%s <%d> < <%d>", pclFunc, atoi(pcpDestLen), atoi(pcpCon2) - atoi(pcpCon1) + 1);
        ilRC = RC_FAIL;
    }
    */
    return ilRC;
}

static int cat(char *pcpDestValue, char *pcpSourceValue, char *pcpSourceValue2, char *pcpDestLen)
{
    int ilRC = RC_FAIL;
    char *pclFunc = "sub";
    memset(pcpDestValue,0,sizeof(pcpDestValue));

    if (strlen(pcpSourceValue) == 0 && strlen(pcpSourceValue2) == 0)
        return RC_FAIL;

    if (atoi(pcpDestLen) == 0)
        return RC_FAIL;

    //if( atoi(pcpDestLen) >= (strlen(pcpSourceValue) + strlen(pcpSourceValue2)) )
    {
        //dbg(TRACE,"%s <%d> == <%d>", pclFunc, atoi(pcpDestLen), strlen(pcpSourceValue) + strlen(pcpSourceValue2));
        strcat(pcpDestValue, pcpSourceValue);
        strcat(pcpDestValue, pcpSourceValue2);
        ilRC = RC_SUCCESS;
    }
    /*
    else
    {
        dbg(TRACE,"%s <%d> < <%d>", pclFunc, atoi(pcpDestLen), strlen(pcpSourceValue) + strlen(pcpSourceValue2));
        ilRC = RC_FAIL;
    }
    */
    return ilRC;
}

