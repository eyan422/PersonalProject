#!/bin/bash

#Usage:
#1>Modify this file's privilage using "chmod 777 SendRecordToBRS.sh"
#2>Check the URNOs sent to BRS under /ceda/tmp/resultXXX.txt
#3>Check the number of URNO sent to BRS under /ceda/tmp/Count_resultXXX.txt

# Setup Environment
#. /ceda/etc/OracleEnv
#. /ceda/etc/UfisEnvCron
. /ceda/etc/UfisEnv
cd /ceda/etc

alias sql="sqlplus -S $CEDADBUSER/$CEDADBPW"
shopt -s expand_aliases
shopt expand_aliases

COUNT_DATE=0
TOTAL_DATE_NUM=2
COUNT_RECORDS=0

rm /ceda/tmp/CountStar.txt

for (( COUNT_DATE=0; COUNT_DATE<=$TOTAL_DATE_NUM; COUNT_DATE=COUNT_DATE+1 ))
do
     Date=`Date $COUNT_DATE`
     echo Date$COUNT_DATE=$Date

     CountStar=`sql<<FFF
     set heading off
     set feedback off
     set pagesize 0
     set verify off
     set echo off
     spool spool_file
     SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='S';
     spool off
     exit;
FFF
`
     echo "SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='S' >> Number=$CountStar" >> /ceda/tmp/CountStar.txt

done

for (( COUNT_DATE=0; COUNT_DATE<=$TOTAL_DATE_NUM; COUNT_DATE=COUNT_DATE+1 ))
do
     Date=`Date $COUNT_DATE`
     echo Date$COUNT_DATE=$Date

     CountStar=`sql<<!
     set heading off
     set feedback off
     set pagesize 0
     set verify off
     set echo off
     spool spool_file
     SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='O';
     spool off
     exit;
!
`
     echo "SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='O' >> Number=$CountStar" >> /ceda/tmp/CountStar.txt

done

     echo "------------------------------------" >> /ceda/tmp/CountStar.txt
for (( COUNT_DATE=0; COUNT_DATE<=$TOTAL_DATE_NUM; COUNT_DATE=COUNT_DATE+1 ))
do
	Date=`Date $COUNT_DATE`
	echo Date$COUNT_DATE=$Date

	VALUE=`sql<<EOF
	set heading off
	set feedback off
	set pagesize 0
	set verify off
	set echo off
	spool spool_file
	SELECT URNO FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='S';
	spool off
	exit;
EOF
`

	#echo VALUE="<$VALUE>"
	#echo $VALUE > /ceda/tmp/result$COUNT_DATE.txt
	if [ -n "$VALUE" ]; then
		echo "The row is not null"
		#echo "$VALUE" > /ceda/tmp/result$COUNT_DATE###.txt
		#exit 0
	else
		echo "There is no row"
	fi

	COUNT_FIELD=0
	COUNT_RECORDS=0

	while [ 1 ]
	do
		COUNT_FIELD=`expr $COUNT_FIELD + 1`
		echo COUNT_FIELD=$COUNT_FIELD

		COUNT_RECORDS=`expr $COUNT_RECORDS + 1`
		echo COUNT_RECORDS=$COUNT_RECORDS


		FIELD="-f$COUNT_FIELD"
		echo $FIELD

		URNO=`echo $VALUE | cut -d " " $FIELD`
		if [ -n "$URNO" ];then
			echo URNO=$URNO
			 cput 0 flight 5 UFR "AFTTAB" "URNO,FTYP" "$URNO,O" "WHERE URNO=$URNO"
			sleep 1
		else
			echo "URNO is null"
			break
			#exit 0
		fi
	done

	#echo "The total record number for Date$COUNT_DATE is `expr $COUNT_RECORDS - 1`" > /ceda/tmp/Count_result${COUNT_DATE}.txt
done


for (( COUNT_DATE=0; COUNT_DATE<=$TOTAL_DATE_NUM; COUNT_DATE=COUNT_DATE+1 ))
do
     Date=`Date $COUNT_DATE`
     echo Date$COUNT_DATE=$Date
     CountStarAfterS=`sql<<!
     set heading off
     set feedback off
     set pagesize 0
     set verify off
     set echo off
     spool spool_file
     SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='S';
     spool off
     exit;
!
`
     echo "SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='S' > Number=$CountStarAfterS" >> /ceda/tmp/CountStar.txt


     CountStarAfterO=`sql<<!
     set heading off
     set feedback off
     set pagesize 0
     set verify off
     set echo off
     spool spool_file
     SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='O';
     spool off
     exit;
!
`
     echo "SELECT COUNT(*) FROM AFTTAB WHERE (FLDA LIKE '$Date%') AND FTYP='O' > Number=$CountStarAfterO" >> /ceda/tmp/CountStar.txt

     echo "------------------------------------" >> /ceda/tmp/CountStar.txt
done