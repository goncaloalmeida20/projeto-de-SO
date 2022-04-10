#include <stdlib.h>
#include "log.h"


int log_write(char *s){
	//open log file
	FILE *f = fopen("log.txt", "a");
	if(f == NULL){
		printf("Error opening log.txt\n");
		return -1;
	}
	
	//write message
	fprintf(f, "%s\n", s);
	
	//close file
	fclose(f);
	return 0
}
