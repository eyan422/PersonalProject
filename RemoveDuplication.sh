#!/bin/bash

#20131014-17:40

shopt -s expand_aliases

# Setup Environment
. /etc/OracleEnv
. /ceda/etc/UfisEnv

alias sql="sqlplus -s $CEDADBUSER/$CEDADBPW"

if [ -f /ceda/tmp/RemoveDuplication.log ];
then
	rm /ceda/tmp/RemoveDuplication.log
else
  echo "Create /ceda/tmp/RemoveDuplication.log"
  touch /ceda/tmp/RemoveDuplication.log
fi

if [ -f /ceda/tmp/FKTTAB_URNO.tmp ];
then
	rm /ceda/tmp/FKTTAB_URNO.tmp
else
  echo "Create /ceda/tmp/FKTTAB_URNO.tmp"
  touch /ceda/tmp/FKTTAB_URNO.tmp
fi

echo "Processing..."

DISTINCT_FUNC=`sqlplus -S "${CEDADBUSER}"/"${CEDADBPW}"<<!
set heading off
set feedback off
set pagesize 0
set verify off
set echo off
spool spool_file
SELECT DISTINCT(FUNC) FROM FKTTAB;
spool off
exit;
!
`

#echo "SELECT FUNC FROM FKTTAB" >> /ceda/tmp/RemoveDuplication.log
#echo "----------------------------------START---------------------------------------" >> /ceda/tmp/RemoveDuplication.log
#echo "DISTINCT_FUNC:" >> /ceda/tmp/RemoveDuplication.log
#echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
#echo "$DISTINCT_FUNC" >> /ceda/tmp/RemoveDuplication.log
#echo "-----------------------------------END--------------------------------------" >> /ceda/tmp/RemoveDuplication.log

#echo "SELECT URNO AS FKTTAB_URNO FROM FKTTAB WHERE FAPP IN (SELECT FAPP FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1';" >> /ceda/tmp/RemoveDuplication.log

echo "Backup FKTTAB as FKTTAB_BK(without data) and FKTTAB_BK_FYA(with data)"
`sqlplus -S "${CEDADBUSER}"/"${CEDADBPW}"<<!
											set heading off
											set feedback off
											set pagesize 0
											set verify off
											set echo off
											spool spool_file
											DROP TABLE FKTTAB_BK;
											DROP TABLE FKTTAB_BK_FYA;
											CREATE TABLE FKTTAB_BK_FYA AS SELECT * FROM FKTTAB;
											CREATE TABLE FKTTAB_BK AS SELECT * FROM FKTTAB WHERE 1=2;
											COMMIT;
											spool off
											exit;
											!
											`

for FUNC in $DISTINCT_FUNC
do
	 	#echo "SELECT URNO AS FKTTAB_URNO FROM FKTTAB WHERE FAPP IN (SELECT FAPP FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1';" >> /ceda/tmp/RemoveDuplication.log
	 	
	 	DUPLICATED_RECORD_DETAILS=`sqlplus -S "${CEDADBUSER}"/"${CEDADBPW}"<<@
											set heading off
											set feedback off
											set pagesize 0
											set verify off
											set echo off
										  spool spool_file
											SELECT * FROM FKTTAB,SECTAB WHERE FAPP IN (SELECT FAPP	FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1' AND FKTTAB.FAPP=SECTAB.URNO;
											COMMIT;
											spool off
											exit;
											@
											`
	
		DUPLICATED_RECORD_URNO_FKTTAB=`sqlplus -S "${CEDADBUSER}"/"${CEDADBPW}"<<!
											set heading off
											set feedback off
											set pagesize 0
											set verify off
											set echo off
											spool spool_file
											SELECT URNO FROM FKTTAB WHERE FAPP IN (SELECT FAPP FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1';
											COMMIT;
											spool off
											exit;
											!
											`
		
		if [ -z "$DUPLICATED_RECORD_DETAILS" ];
		then
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
		else
			echo "SELECT * FROM FKTTAB,SECTAB WHERE FAPP IN (SELECT FAPP	FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1' AND FKTTAB.FAPP=SECTAB.URNO" >> /ceda/tmp/RemoveDuplication.log
			
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
			echo "DUPLICATED_RECORD_DETAILS:" >> /ceda/tmp/RemoveDuplication.log
			echo "$DUPLICATED_RECORD_DETAILS" >> /ceda/tmp/RemoveDuplication.log
			#echo "DUPLICATED_RECORD_URNO_FKTTAB:" >> /ceda/tmp/RemoveDuplication.log
			#echo "$DUPLICATED_RECORD_URNO_FKTTAB" >> /ceda/tmp/RemoveDuplication.log
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
		fi
											
		if [ -z "$DUPLICATED_RECORD_URNO_FKTTAB" ];
		then
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
		else
			echo "SELECT URNO AS FKTTAB_URNO FROM FKTTAB WHERE FAPP IN (SELECT FAPP FROM FKTTAB WHERE FUNC='$FUNC' GROUP BY FAPP, FUNC, SUBD HAVING COUNT(FUNC)>=2) AND FUNC='$FUNC' AND SDAL='1';" >> /ceda/tmp/RemoveDuplication.log
			
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
			#echo "DUPLICATED_RECORD_ALL_DETAILS:" >> /ceda/tmp/RemoveDuplication.log
			#echo "$DUPLICATED_RECORD_ALL_DETAILS" >> /ceda/tmp/RemoveDuplication.log
			echo "DUPLICATED_RECORD_URNO_FKTTAB:" >> /ceda/tmp/RemoveDuplication.log
			echo "$DUPLICATED_RECORD_URNO_FKTTAB" >> /ceda/tmp/RemoveDuplication.log
			echo "$DUPLICATED_RECORD_URNO_FKTTAB" >> /ceda/tmp/FKTTAB_URNO.tmp
			echo "-------------------------------------------------------------------------" >> /ceda/tmp/RemoveDuplication.log
		fi
done

echo "Delete records in FKTTAB, for rollback, please refer to FKTTAB_BK"
cat /ceda/tmp/FKTTAB_URNO.tmp | while read LINE
do
		#echo $LINE
   	echo "DELETE FROM FKTTAB WHERE URNO='$LINE' AND SDAL='1'" >> /ceda/tmp/RemoveDuplication.log
   	
   	
		`sqlplus -S "${CEDADBUSER}"/"${CEDADBPW}"<<!
											set heading off
											set feedback off
											set pagesize 0
											set verify off
											set echo off
											spool spool_file
											INSERT INTO FKTTAB_BK SELECT * FROM FKTTAB WHERE URNO='$LINE';
											DELETE FROM FKTTAB WHERE URNO='$LINE';
											COMMIT;
											spool off
											exit;
											!
											`
   	
done

echo "Finished"
