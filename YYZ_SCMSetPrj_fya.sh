#!/bin/ksh -x
###
### sccs_version " @(#)  SCMSetPrj $Revision: 1.3 $ / $Date: 2002/07/15 16:49:28GMT+02:00 $  / JIM";
###
###
### 20020618 JIM: added <sandbox name> as parameter
###               change work dir to $PB at end
### 20020710 JIM: added add_to_path and remove_from_path
### 20020715 JIM: replaced LD by DS (LD may be used by OS already)
### 20030203 JIM: remove blanks from LOGNAME


if [ "nis" = `uname -n` ] ; then
   #we do not have Sandbox-Mount-Pounts on the NIS-Server
   cat /sandbox/SandboxCOMMON/.banner
   return 2>/dev/null # to avoid exit in '.' environment
   exit
fi

#if [ "linscm01" = `uname -n` ] ; then
#   #we do not have Sandbox-Mount-Pounts on the Sandbox-Server
#   cat /home1/ypcommon/.banner
#   return 2>/dev/null # to avoid exit in '.' environment
#   exit
#fi

PROGNAME=SCMSetPrj

case "$1" in
   "-h"*|"--h"*|"-?"|"--?")
     cat <<EOFSYNTAX

   ${PROGNAME}: Set project environment for Vertical Sky Projects

   usage:   ${PROGNAME} [<sandbox name>]
   example: ${PROGNAME} Test_WAW
  
   <sandbox name> ist used only, if a sandboy with this name has been created
   with Vertical Sky Client

EOFSYNTAX
   return
   ;;
esac



if [ -f /ceda/etc/UfisEnv ] ; then
   . /ceda/etc/UfisEnv
fi

############ BEGIN add_to_path ############ 
function add_to_path
{
if [ "$1" = "-first" ] ; then
   PATH="$2:$PATH"
   shift
else
   if [ -z "$1" ] ; then
      PATH="$PATH"
   else
      PATH="$PATH:$1"
   fi
fi

PATH=`echo $PATH | awk '
 {
   p=index($0,":")
   if (p==0)
   {
     print $0
   }
   else
   {   
     b=$1
     c=""
     while (p>0)
     {
       a=substr(b,1,p-1)
       b=substr(b,p+1)
       p=index(b,":")
       if (index(":"c":",":"a":") == 0)
       {
          c=c":"a
       }
     }
     if (index(":"c":",":"b":") == 0)
     {
        c=c":"b
     }
     print substr(c,2)
   }
 }'
`
}
############ END   add_to_path ############ 
############ BEGIN remove_from_path ############ 
function remove_from_path
{
PATH=`echo $PATH $1 | awk '
 {
   p=index($1,":")
   if (p==0)
   {
     if ($1 != $2)
     {
        print $1
     }
     else
     {
        print
     }
   }
   else
   {   
     b=$1
     #print "LOG: ",b >"./logfile"
     c=""
     while (p>0)
     {
       a=substr(b,1,p-1)
       b=substr(b,p+1)
       p=index(b,":")
       #print "LOG: a: ",a >>"./logfile"
       #print "LOG: b: ",b >>"./logfile"
       if (index(":"c":",":"a":") == 0)
       {
          #print "LOG: a: ",a," not present" >>"./logfile"
          if (a != $2)
          {
             #print "LOG: a: ",a," and not to delete" >>"./logfile"
             c=c":"a
          }
       }
     }
     if (index(":"c":",":"b":") == 0)
     {
       #print "LOG: b: ",b," not present" >>"./logfile"
       if (b != $2)
       {
          #print "LOG: b: ",b," and not to delete" >>"./logfile"
          c=c":"b
       }
     }
     print substr(c,2)
   }
 }'
`
}
############ END   remove_from_path ############ 

if [ -z "${IamUser}" ] ; then
unset CEDAUSER
IamUser=`echo ${LOGNAME} | tr -d " "` # remove blanks from LOGNAME (maybe NIS?)
theUser=$IamUser
if [ -z "$IamUser" ] ; then
   IamUser=`id | awk '{a=index($1,"(")+1 ; e=index($1,")")-a ; print substr($1,a,e)}'`
fi
if [ -z "$IamUser" ] ; then
   IamUser=`whoami 2>/dev/null|cut -f 1 -d ' '`
fi
if [ -z "$IamUser" ] ; then
  IamUser=`who am i|cut -f 1 -d ' '`
fi
fi
theUser=$IamUser


function to_upper
{
  sed -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'
}

#IamUser=`echo "${IamUser}"| to_upper `
#if [ -x /ceda/etc/toupper ] ; then
#   IamUser=`toupper $IamUser`
#fi
   
#echo "IamUser: ${IamUser}"
export IamUser
#PathRoot=/net/oslo/usr1/CEDASandbox/Sandbox$IamUser
PathRoot=/sandbox/$theUser

echo "PathRoot is $PathRoot"

export LM_LICENSE_FILE=/etc/opt/licenses/licenses_combined 

function SetPrjBase
{
  unset PRJBASE
  if [ ! -z "$1" ] ; then
     prj="$1"
     if [ -d $PathRoot/$prj ] ; then
   	   PRJBASE=$PathRoot/${prj}/Ufis/*/*/Base/Server
       # echo "1: PRJBASE: $PRJBASE"
       cnt=`echo ${PRJBASE} | wc -w`
       if [ ${cnt} -gt 1 ] ; then
          echo '"'${prj}'" includes more than 1 .../Base/Server subdir, unable to decide ...'
          unset PRJBASE
          return
       fi
       if [ ! ${cnt} = 1 ] ; then
          PRJBASE="hoffentlichgibtsdashiernicht"
       fi
   	   PRJBASE=`dirname ${PRJBASE}`/Server
       if [ ! -d ${PRJBASE} ] ; then
          PRJBASE=`dirname $PathRoot/${prj}/Ufis/Server`/Server
          if [ -d ${PRJBASE} ] ; then
             CEDA_prj=TRUE
          else   
             # echo "2: PRJBASE: $PRJBASE"
             echo '"'${prj}'" is no Server project ...'
             unset PRJBASE
          fi
       else
          # get the name of the Vertical-SkyProject: (used in GlobalDefines)
		   		PRJNAME=`dirname $PRJBASE` # Base
		   		PRJNAME=`dirname $PRJNAME` # ../Base
		   		PRJNAME=`dirname $PRJNAME` # ../Base
		   		PRJNAME=`basename $PRJNAME` # (../../Base)
          CEDA_prj=FALSE
       fi
     fi
     if [ "$prj" = "Test_SCM" ] ; then
   	   PRJBASE=`dirname $PathRoot/${prj}/Ufis/*/Base/Server`/Server
       CEDA_prj=FALSE
       PRJNAME=$prj
     fi
  fi
}
function NumAndDir
{
     for d in * ; do 
        if [ -d $d ] ; then
	   echo $d 
	fi   
     done | sort -f | awk '{printf " %2s: %s\n",NR,$1}'
}

CEDA_prj=FALSE
ALL_PROJECTS=`/bin/ls -1 $PathRoot 2>/dev/null`
if [ ! -z "$ALL_PROJECTS" ] ; then
  cd $PathRoot
  SetPrjBase $1
  if [ -z "${PRJBASE}" ] ; then
   prj=""
   while [ ! -d "$prj" ] ; do
     echo "=============> Set Project Environment"
     echo
     echo "Please enter the project by name or number:"
     echo "Given Projects:"
     NumAndDir
     read prj
     if [ -z "$prj" ] ; then
        unset PRJBASE
        break
     fi
     if [ ! -d "$prj" ] ; then
       prj=`NumAndDir| grep " $prj:" | awk '{print $2}'`
     fi 
   done
   SetPrjBase ${prj}
  fi

  if [ ! -z "${PRJBASE}" ] ; then
    if [ -d "${PRJBASE}" ] ; then
      HOMEPATH=`dirname ~/PRJ`
      HOMEDIR=`dirname ${HOMEPATH}` # ? should be parent to home dir ?
      HOMEUSER=`basename ${HOMEPATH}` # ? should be user name ?
      if [ ! "${HOMEUSER}" = "ceda" ] ; then
        if [ ! "${HOMEDIR}" = "/home1" ] ; then
          if [ -d "/home1/${HOMEUSER}" ] ; then
            HOMEPATH=/home1/${HOMEUSER}
          fi
        fi
      fi
      if [ ! -d "${HOMEPATH}/PRJ" ] ; then
         mkdir ${HOMEPATH}/PRJ
      fi
      HOMELINK=${HOMEPATH}/PRJ/${prj}
			if [ -h ${HOMELINK} ] ; then
        # remove old Link in Homedir:
        rm ${HOMELINK} 2>&1 >/dev/null 
      fi
			if [ ! -h ${HOMELINK} ] ; then
         # create new Link in Homedir:
         ln -s ${PRJBASE} ${HOMELINK}
      fi
			if [ -x ${HOMELINK} ] ; then
			   PRJBASE=${HOMELINK}
			fi
      unset CEDAUSER
      remove_from_path $M
      if [ "${CEDA_prj}" = "FALSE" ] ; then
         BINBASE=/ceda/SCM_Bins/${IamUser}/${prj}
         B=$BINBASE/bin
         TB=$BINBASE/etc
         L=$BINBASE/Lib
         O=$BINBASE/Obj
         S=$PRJBASE/Kernel
         KS=$PRJBASE/Kernel
         FS=$PRJBASE/Fids
         RS=$PRJBASE/Rms
         IS=$PRJBASE/Interface
         M=$PRJBASE/Make
         I=$PRJBASE/Library/Inc
         TS=$PRJBASE/Tools
         LS=$PRJBASE/Library/Cedalib
         DS=$PRJBASE/Library/Dblib
         CF=$PRJBASE/Serverconfiguration
         CFC=$PRJBASE/Serverconfiguration/Ceda/Customer
         CFD=$PRJBASE/Serverconfiguration/Ceda/Development
         PB=$PRJBASE
	 SB=${PathRoot}
         unset TI
         unset LI
         export S KS FS RS IS B TB O M I TS LS DS CF CFC CFD L PB SB prj PRJNAME
         if [ -f ${PRJBASE}/.include ] ; then
            eval `grep "CLONE_OF=" $PRJBASE/.include`
         fi
         if [ -f ${PRJBASE}/.exclude ] ; then
            eval `grep "CLONE_OF=" $PRJBASE/.exclude`
         fi
         if [ -z "${CLONE_OF}" ] ; then
            CLONE_OF="_Standard_Server"
         fi
				 export CLONE_OF
         #alias resync="if [ ! -z \"\${prj}\" ] ; then \${SB}/\${CLONE_OF}/Ufis/*/*/*/*/Make/MakeSandbox.bash \${prj} ; fi"
				 alias resync=/sandbox/common/resync.sh
      fi
      add_to_path $M
    else      
      echo Verzeichnis ${PRJBASE} existiert nicht !!!
    fi
  else      
    echo "reenter '. /sandbox/common/SCMSetPrj' to set PRJ"
  fi

  if [ -n "${CEDAHOST}" ] ; then
       CEDAMOUNT="/net/${CEDAHOST}/ceda"
       C="${CEDAMOUNT}"/conf
       D="${CEDAMOUNT}"/debug
       BIN="${CEDAMOUNT}"/bin
       E="${CEDAMOUNT}"/etc
       alias bin="cd ${BIN}"
       alias debug="cd ${D}"
       alias conf="cd ${C}"
       alias etc="cd ${E}"
  fi
else
 	echo "$PROGNAME: No Projects found in $PathRoot"
	echo "Enter '. /sandbox/common/SCMSetPrj <sandbox name>' to set sandbox "
fi



if [ ! -z "${PB}" ] ; then
  if [ -d "${PB}" ] ; then
    cd $PB
  fi
fi

add_to_path -first /usr/local/bin
unalias make 2>/dev/null
