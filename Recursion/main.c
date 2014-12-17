#include <stdio.h>
#include <stdlib.h>

int fact(int ilNumber);
void factTail(int ilNumber, int *ilResult);
void addTail(float flNumber, float *flResult);

int main()
{
    int ilResult = 1;
    float ilSum = 0;
    //ilResult = fact(5);
    factTail(5,&ilResult);
    printf("Result1<%d>\n",ilResult);

    ilResult = 0;
    addTail(2.0,&ilSum);
    printf("Result2<%f>\n",ilSum);
    return 0;
}

int fact(int ilNumber)
{
    if (ilNumber == 0 || ilNumber == 1)
    {
        return 1;
    }
    else
    {
        return ilNumber*fact(ilNumber-1);
    }
}

void factTail(int ilNumber, int *ilResult)
{
    if(ilNumber == 0 || ilNumber == 1)
    {
        ;
    }
    else
    {
        (*ilResult) *= ilNumber;
        factTail(ilNumber-1, ilResult);
    }
}

void addTail(float flNumber, float *flResult)
{
    if(flNumber == 0)
    {
        ;
    }
    else
    {
        (*flResult) += 1/flNumber;
        addTail(flNumber-1, flResult);
    }
}
