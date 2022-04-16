#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <semaphore.h>
#include "log.h"

sem_t* log_mutex;

int create_log_mutex(){
	sem_unlink("LOG_MUTEX");
	if((log_mutex = sem_open("LOG_MUTEX",O_CREAT|O_EXCL,0700,1)) == SEM_FAILED)	
		return -1;
		
	return 0;
}

int create_log(){
	//create log mutex 
	if(create_log_mutex() < 0){
		printf("Error creating log mutex\n");
		return -1;
	}
	
	//create log file
	FILE *f = fopen("log.txt", "w");
	if(f == NULL){
		printf("Error creating log.txt\n");
		return -1;
	}
	fclose(f);
	return 0;
}

int log_write(char *s){
	sem_wait(log_mutex);
	
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
	fprintf(f, "%02d:%02d:%02d %s\n", time_now->tm_hour, time_now->tm_min, time_now->tm_sec, s);
	
	fclose(f);
	sem_post(log_mutex);
	return 0;
}

void close_log(){
	sem_close(log_mutex);
	sem_unlink("LOG_MUTEX");
}
