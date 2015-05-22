#include <stdio.h>
#include <stdlib.h>

int main()
{
    char pclTest[2] = "\0";

    pclTest[0] = '1';
    pclTest[1] = '2';

    printf("Result:%s\n",pclTest);

    return 0;
}
