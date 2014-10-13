#include <stdio.h>
#include <string.h>
int foo() {
  char  bar[128];
  char  *baz = &bar[0];
  
  baz[127] = 0;

  return (strlen(baz) / sizeof(char));
}

int main(void)
{
	printf("foo return value<%d>\n",foo());
	return 0;
}
