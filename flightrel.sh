#!/bin/bash

# 20130821 - this is required for alias use in non-interactive shell
# 20130910 - add pause between cput's
# 20130911 - add cput's "timeout" argument to the command itself.

shopt -s expand_aliases

#Usage:
#1>Modify this file's privilage using "chmod 777 SendRecordToBRS.sh"
#2>Check the URNOs sent to BRS under /ceda/tmp/resultXXX.txt
#3>Check the number of URNO sent to BRS under /ceda/tmp/Count_resultXXX.txt  

# Setup Environment
#. /ceda/etc/OracleEnv
. /ceda/etc/UfisEnvCron
. /ceda/etc/UfisEnv
cd /ceda/etc

alias sql="sqlplus -s $CEDADBUSER/$CEDADBPW"


COUNT_DATE=0
TOTAL_DATE_NUM=2
COUNT_RECORDS=0

rm /ceda/tmp/CountStar.txt
rm /ceda/tmp/LogAndURNO_List.txt

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
		echo "Date:$Date" >> /ceda/tmp/LogAndURNO_List.txt
		echo "The row is not null" >> /ceda/tmp/LogAndURNO_List.txt
		#echo "$VALUE" >> /ceda/tmp/LogAndURNO_List.txt
		#exit 0
	else
		echo "Date:$Date" >> /ceda/tmp/LogAndURNO_List.txt
		echo "There is no flight" >> /ceda/tmp/LogAndURNO_List.txt
		continue;
	fi

	COUNT_FIELD=0
	#COUNT_RECORDS=0
	
	#while [ 1 ]
	for URNO in $VALUE
	do
		#COUNT_FIELD=`expr $COUNT_FIELD + 1`
		#echo COUNT_FIELD=$COUNT_FIELD

		#COUNT_RECORDS=`expr $COUNT_RECORDS + 1`
		#echo COUNT_RECORDS=$COUNT_RECORDS
		
		
		#FIELD="-f$COUNT_FIELD"
		#echo $FIELD	

		#URNO=`echo $VALUE | cut -d " " $FIELD`
			echo URNO=$URNO >> /ceda/tmp/LogAndURNO_List.txt

		if [ -n "$URNO" ];then
			cput 0 flight UFR "URNO,FTYP" "$URNO,O" "WHERE URNO=$URNO" 7
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
