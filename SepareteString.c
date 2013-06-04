#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char pcgConfigString[512] = "[dqahdl|*],[xxx,yyy|LDM,PTM]";
//char pcgConfigString[512] = "[dqahdl|*]";
//char pcgConfigString[512] = "[xxx,yyy|LDM,PTM]";

int SeparateString(char *pcp1stPart, char *pcp2ndPart)
{
	char *pcl1stLeftSquareBacketPosition = NULL;
	char *pcl1stRightSquareBacketPosition = NULL;
	char *pcl2ndLeftSquareBacketPosition = NULL;
	char *pcl2ndRightSquareBacketPosition = NULL;

	char *pcl1stCommaPosition = NULL;

	char pcl1stPart[1024] = "\0";
	char pcl2ndPart[1024] = "\0";	
	pcl1stCommaPosition = strstr(pcgConfigString,",");
	
	if( pcl1stCommaPosition != NULL && strstr(pcgConfigString,"*") != 0 )
	{
		printf("*pcl1stCommaPosition<%c>\n",*pcl1stCommaPosition);
		
		//1st '['	
		pcl1stLeftSquareBacketPosition = strstr(pcgConfigString,"[");
		printf("*pcl1stLeftSquareBacketPosition<%c>\n",*pcl1stLeftSquareBacketPosition);
		//1st ']'
		pcl1stRightSquareBacketPosition = strstr(pcgConfigString,"]");
		printf("*pcl1stRightSquareBacketPosition<%c>\n",*pcl1stRightSquareBacketPosition);
		
		//1st ']' is at the front of 1st '['
		if( (int)(pcl1stRightSquareBacketPosition - pcl1stLeftSquareBacketPosition) < 0)
		{
			printf("The 1st ']' is at the front of 1st '[' -> return\n");
			return;
		}
				
		memset(pcl1stPart,0,sizeof(pcl1stPart));
		strncpy(pcl1stPart, pcgConfigString+1,(int)(pcl1stRightSquareBacketPosition-pcl1stLeftSquareBacketPosition)-1);
		printf("pcl1stPart<%s>\n",pcl1stPart);
		
		//2nd '['
		pcl2ndLeftSquareBacketPosition = strstr(pcgConfigString,"[");
                printf("*pcl2ndLeftSquareBacketPosition<%c>\n",*pcl2ndLeftSquareBacketPosition);
		//2nd ']'
		pcl2ndRightSquareBacketPosition = strstr(pcgConfigString,"]");
                printf("*pcl2ndRightSquareBacketPosition<%c>\n",*pcl2ndRightSquareBacketPosition);

		
                //2nd ']' is at the front of 2nd '['
                if( (int)(pcl2ndRightSquareBacketPosition - pcl2ndLeftSquareBacketPosition) < 0)
                {   
                        printf("The 2nd ']' is at the front of 2nd '[' -> return\n");
                        return;
                } 
		
		//char pcgConfigString[512] = "[dqahdl|*],[xxx,yyy|LDM,PTM]";
		//pcl2ndPart<xxx,yyy|L>
		memset(pcl2ndPart,0,sizeof(pcl2ndPart));
                strcpy(pcl2ndPart, pcgConfigString+(int)(pcl1stCommaPosition-pcgConfigString)+2);
                
		*(pcl2ndPart + strlen(pcl2ndPart) - 1) = '\0';
		printf("pcl2ndPart<%s>\n",pcl2ndPart);
	
		//dqahdl|*
		strcpy(pcp1stPart,pcl1stPart);
		//xxx,yyy|LDM,PTM
		strcpy(pcp2ndPart,pcl2ndPart);
	}
	else
	{
		pcl1stLeftSquareBacketPosition = strstr(pcgConfigString,"[");
		printf("*pcl1stLeftSquareBacketPosition<%c>\n",*pcl1stLeftSquareBacketPosition);
		pcl1stRightSquareBacketPosition = strstr(pcgConfigString,"]");
		printf("*pcl1stRightSquareBacketPosition<%c>\n",*pcl1stRightSquareBacketPosition);
		
		//1st ']' is at the front of 1st '['
                if( (int)(pcl1stRightSquareBacketPosition - pcl1stLeftSquareBacketPosition) < 0)
        	{
                        printf("The 1st ']' is at the front of 1st '[' -> return\n");
                        return;
                }
		
		memset(pcl1stPart,0,sizeof(pcl1stPart));
                strncpy(pcl1stPart, pcgConfigString+1,(int)(pcl1stRightSquareBacketPosition-pcl1stLeftSquareBacketPosition)-1);
                printf("pcl1stPart<%s>\n",pcl1stPart);
		strcpy(pcp1stPart,pcl1stPart);
	}
	return 0;
}

int main()
{
	char pcl1stPart[1024] = "\0";
	char pcl2ndPart[1024] = "\0";
	
	char *pcl1stSeparator = NULL;
	char *pcl2ndSeparator = NULL;

	char pcl1stProcess[1024] = "\0";
	char pcl1stDSSN[1024] = "\0";

	char pcl2ndProcess[1024] = "\0";
	char pcl2ndDSSN[1024] = "\0";

	SeparateString(pcl1stPart,pcl2ndPart);
	if(strlen(pcl1stPart) != 0)
	{
		printf("1st<%s>\n",pcl1stPart);
		pcl1stSeparator = strstr(pcl1stPart,"|");
		strncpy(pcl1stProcess,pcl1stPart,(int)(pcl1stSeparator-pcl1stPart));
		printf("1<%s>\n",pcl1stProcess);
		strcpy(pcl1stDSSN,pcl1stPart + (int)(pcl1stSeparator-pcl1stPart)+1);
		printf("#1<%s>\n",pcl1stDSSN);
	}	
	if(strlen(pcl2ndPart) != 0)
	{
		printf("2nd<%s>\n",pcl2ndPart);
		pcl2ndSeparator = strstr(pcl2ndPart,"|");
		strncpy(pcl2ndProcess,pcl2ndPart,(int)(pcl2ndSeparator-pcl2ndPart));
		printf("2<%s>\n",pcl2ndProcess);
		strcpy(pcl2ndDSSN,pcl2ndPart + (int)(pcl2ndSeparator-pcl2ndPart)+1);
		printf("#2<%s>\n",pcl2ndDSSN);
	}
	return 0;
}
