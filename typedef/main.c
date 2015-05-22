#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef char URNO_TYPE[64];

int main()
{
    URNO_TYPE test;
    strcpy(test,"Hello world!");

    printf("test<%s>\n",test);

    return 0;
}
