#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
        FILE * fp;
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
	int ilCount = 0;
        
	fp = fopen("/ceda/exco/FILES/FLT/seasonal_batch_file_20130614050941.xml", "r");
	
	if (fp == NULL)
       		exit(EXIT_FAILURE);
	
	while ((read = getline(&line, &len, fp)) != -1) 
	{
        	//printf("Retrieved line of length %zu :\n", read);
                printf("[%d]\t%s",++ilCount, line);
	}
	if (line)
        	free(line);
	
	return EXIT_SUCCESS;
}

