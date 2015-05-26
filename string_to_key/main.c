#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int string_to_key (char *pcpStr, char *pcpKey);

int main()
{
    char pclKey[128] = "\0";
    //string_to_key("4fpAGP#$",pclKey);

    string_to_key("3hjmCEI@",pclKey);
    return 0;
}

int string_to_key (char *pcpStr, char *pcpKey)
{
    int i = 0;
    int iLen = strlen(pcpStr);

    memset (pcpKey, 0, iLen+1);

    for (i=0; i < iLen; i++)
    {
        pcpKey[i] = (pcpStr[i] + i + 3) & 0x7f;
        if (pcpKey[i] < 0x20)
        {
            pcpKey[i] += 0x20;
        }
    }

    printf("string_to_key: Str=%s Key=%s\n\n",pcpStr, pcpKey);
    return 0;
}

