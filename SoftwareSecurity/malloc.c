  #include <stdio.h>
  #include <stdlib.h>
  int main(int argc, char *argv[]) {
    unsigned int i;
    unsigned int k = atoi(argv[1]);
	printf("0\n");
    char     *buf = malloc(k); /* 1 */
  //#if 0
	printf("1\n");
    if(buf == 0) {
      return -1;
    }

    for(i = 0; i < k; i++) {
      buf[i] = argv[2][i]; /* 2 */
    }

	printf("2\n");
    printf("%s\n", buf); /* 3 */

	printf("3\n");
   //#endif
    return -1;
  }
