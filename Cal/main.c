#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
int radd(int num);
int func(int x, int y);

int main()
{

    printf("%d\n\n\n",func(36,9));

    radd(10);
    return 0;
}

int radd(int num)
{
    if (0 >= --num)
        return 0;

    int res = num + radd(num);
    printf("%d\n", res);
    return res;
}

int func(int x, int y)
{
	if(x==0 && y==1)
	{
	    //printf("1\n\n");
		return 1;
	}
	else
	{
	    //printf("%d\n\n",func((x-y+1),(y-1))+y);
		return func((x-y+1),(y-1))+y;
	}
}

