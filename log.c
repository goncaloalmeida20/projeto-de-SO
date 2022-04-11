#include <stdlib.h>
#include <time.h>
#include "log.h"


int log_write(char *s){
	//get current time
	time_t t = time(NULL);
	struct tm *time_now = localtime(&t);

	//write message on the console
	printf("%02d:%02d:%02d %s\n", time_now->tm_hour, time_now->tm_min, time_now->tm_sec, s);

	//open log file
	FILE *f = fopen("log.txt", "a");
	if(f == NULL){
		printf("Error opening log.txt\n");
		return -1;
	}
	
	//write message in the file
	fprintf("%02d:%02d:%02d %s\n", time_now->tm_hour, time_now->tm_min, time_now->tm_sec, s);
	
	//close file
	fclose(f);
	return 0
}
