#ifndef _DEF_mks_version_urno_fn_c
  #define _DEF_mks_version_urno_fn_c
  #include "ufisvers.h" /* sets UFIS_VERSION, must be done before mks_version */
  static char *mks_version_urno_lib_c;   char *MARK_UNUSED = "@(#) "UFIS_VERSION" $Id: Ufis/_Standard/_Standard_Server/Base/Server/Library/Inc/urno_fn.inc 1.4 2014/03/17 13:49:31SGT fya Exp  $";
#endif /* _DEF_mks_version */
/****************************************************************************
 AUTHOR : MEI
 CREATED DATE : 10 MAR 2008
****************************************************************************/

#define UGCCS_FKT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "glbdef.h"

#define MIN_URNO 1
#define MAX_URNO 0x7FFFFFFF

extern char cgHopo[8];

static int RunSQL(char *pcpSelection, char *pcpData )
{
    int ilRc = DB_SUCCESS;
    short slSqlFunc = 0;
    short slSqlCursor = 0;
    char pclErrBuff[128];
    char *pclFunc = "RunSQL";

    ilRc = sql_if ( START, &slSqlCursor, pcpSelection, pcpData );
    close_my_cursor ( &slSqlCursor );

    if( ilRc == DB_ERROR )
    {
        get_ora_err( ilRc, &pclErrBuff[0] );
        dbg( TRACE, "<%s> Error getting Oracle data error <%s>", pclFunc, &pclErrBuff[0] );
    }
    return ilRc;
}

int NewUrnos( char *pcpTableName, int ipNumUrnosReqd )
{
    int ilRc;
    int ilMinUrno;
    int ilMaxUrno;
    int ilUrno;
    int ilReqdUrno;
    int ilLastUrno;
    short slSqlSelCursor = 0;
    short slSqlSelMaxCursor = 0;
    short slSqlUpdCursor = 0;
    short slSqlInsCursor = 0;
    char pclSqlBuf[2000];
    char pclSqlData[2000];
    char pclTmpStr[500];
    char *pclFunc = "NewUrnos";

    ilReqdUrno = ipNumUrnosReqd;
    dbg( DEBUG, "<%s> TabName <%s> NumUrno <%d>", pclFunc, pcpTableName, ilReqdUrno );

    if( strlen(pcpTableName) <= 0 || strlen(pcpTableName) > 10 )
    {
        dbg( TRACE, "<%s> Invalid table name", pclFunc );
        return RC_FAIL;
    }

    if( ilReqdUrno <= 0 || ilReqdUrno > 500 )
    {
        dbg( TRACE, "<%s> Invalid number of URNO requested Min=1 Max=500", pclFunc );
        return RC_FAIL;
    }

    sprintf( pclSqlBuf, "SELECT ACNU, MINN, MAXN FROM NUMTAB WHERE KEYS = '%s' FOR UPDATE", pcpTableName );
    dbg( DEBUG, "<%s> GetNUMTAB SqlBuf <%s>", pclFunc, pclSqlBuf );
    ilRc = sql_if ( START, &slSqlSelCursor, pclSqlBuf, pclSqlData );

    /*** New Table ***/
    if( ilRc == ORA_NOT_FOUND )
    {
        sprintf( pclSqlBuf, "SELECT MAX(TO_NUMBER(URNO)) FROM %s", pcpTableName );
        dbg( DEBUG, "<%s> GetMaxURNO SqlBuf <%s>", pclFunc, pclSqlBuf );
        ilRc = sql_if ( START, &slSqlSelMaxCursor, pclSqlBuf, pclSqlData );

        if( ilRc == DB_ERROR )
        {
            dbg( TRACE, "<%s> No such table exist! Table Name <%s>", pclFunc, pcpTableName );
            close_my_cursor ( &slSqlSelCursor );
            close_my_cursor ( &slSqlSelMaxCursor );
            rollback();
            return DB_ERROR;
        }

        ilUrno = 0;
        if( ilRc == DB_SUCCESS )
            ilUrno = atol(pclSqlData) + 5000;   /* In case some process is still writing to the table while this is executed */
        dbg( DEBUG, "<%s> GetMaxURNO URNO <%d>", pclFunc, ilUrno );

        ilLastUrno = ilUrno + ilReqdUrno;
        if( ilLastUrno < MIN_URNO || ilLastUrno > MAX_URNO )
        {
            ilUrno = 1;
            ilLastUrno = ilUrno + ilReqdUrno;
        }
        sprintf( pclSqlBuf, "INSERT INTO NUMTAB (ACNU,MINN,MAXN,KEYS,HOPO) VALUES (%d,%d,%d,'%s','%s')",
                                                  ilLastUrno, MIN_URNO, MAX_URNO, pcpTableName, cgHopo );
        dbg( DEBUG, "<%s> InsertNUMTAB SqlBuf <%s>", pclFunc, pclSqlBuf );
        ilRc = sql_if ( START, &slSqlInsCursor, pclSqlBuf, pclSqlData );
        if( ilRc != DB_SUCCESS )
        {
            dbg( TRACE, "<%s> ERROR!!! Not able to insert KEYS into NUMTAB for table <%s>", pclFunc, pcpTableName );
            close_my_cursor ( &slSqlSelCursor );
            close_my_cursor ( &slSqlSelMaxCursor );
            close_my_cursor ( &slSqlInsCursor );
            rollback();
            return DB_ERROR;
        }
        commit_work();
        close_my_cursor ( &slSqlSelCursor );
        close_my_cursor ( &slSqlSelMaxCursor );
        close_my_cursor ( &slSqlInsCursor );
        return ilUrno;
    }

    if( ilRc == DB_SUCCESS )
    {
        get_fld(pclSqlData,FIELD_1,STR,10,pclTmpStr); ilUrno = atoi(pclTmpStr);
        get_fld(pclSqlData,FIELD_2,STR,10,pclTmpStr); ilMinUrno = atoi(pclTmpStr);
        get_fld(pclSqlData,FIELD_3,STR,10,pclTmpStr); ilMaxUrno = atoi(pclTmpStr);

        dbg( DEBUG, "<%s> URNO <%d> MINN <%d> MAXN <%d>", pclFunc, ilUrno, ilMinUrno, ilMaxUrno );
        if( ilMinUrno < MIN_URNO || ilMinUrno > MAX_URNO )
            ilMinUrno = MIN_URNO;
        if( ilMaxUrno < MIN_URNO || ilMaxUrno > MAX_URNO )
            ilMaxUrno = MAX_URNO;
        if( ilMinUrno >= ilMaxUrno )
        {
            ilMinUrno = MIN_URNO;
            ilMaxUrno = MAX_URNO;
        }

        ilLastUrno = ilUrno + ilReqdUrno;
        if( ilLastUrno < ilMinUrno || ilLastUrno > ilMaxUrno )
        {
            ilUrno = ilMinUrno;
            ilLastUrno = ilUrno + ilReqdUrno;
        }

        sprintf( pclSqlBuf, "UPDATE NUMTAB SET ACNU=%d WHERE KEYS = '%s'", ilLastUrno, pcpTableName );
        dbg( DEBUG, "<%s> UpdateNUMTAB SqlBuf <%s>", pclFunc, pclSqlBuf );

        ilRc = sql_if ( START, &slSqlUpdCursor, pclSqlBuf, pclSqlData );
        if( ilRc != DB_SUCCESS )
        {
            dbg( TRACE, "<%s> ERROR!!! Not able to update NUMTAB for table <%s>", pclFunc, pcpTableName );
            close_my_cursor ( &slSqlSelCursor );
            close_my_cursor ( &slSqlUpdCursor );
            rollback();
            return DB_ERROR;
        }
        commit_work();
        close_my_cursor ( &slSqlSelCursor );
        close_my_cursor ( &slSqlUpdCursor );
        return ilUrno;
    }

    /* DB_ERROR */
    dbg( TRACE, "<%s> Error in getting URNO values", pclFunc );
    close_my_cursor ( &slSqlSelCursor );
    rollback();
    return DB_ERROR;
}
