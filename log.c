/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include "log.h"

#define LOG_MUTEX "LOG_MUTEX"

sem_t* log_mutex;
FILE* f;

int create_log_mutex(){
	sem_unlink(LOG_MUTEX);
	if((log_mutex = sem_open(LOG_MUTEX,O_CREAT|O_EXCL,0700,1)) == SEM_FAILED)	
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
	f = fopen("log.txt", "w");
	if(f == NULL){
		printf("Error creating log.txt\n");
		return -1;
	}

	return 0;
}

void log_write(char *s){
	sem_wait(log_mutex);
	
	//get current time
	time_t t = time(NULL);
	struct tm *time_now = localtime(&t);

	//write message on the console
	printf("%02d:%02d:%02d %s\n", time_now->tm_hour, time_now->tm_min, time_now->tm_sec, s);
	
	//write message in the file
	fprintf(f, "%02d:%02d:%02d %s\n", time_now->tm_hour, time_now->tm_min, time_now->tm_sec, s);

	fflush(f);

	sem_post(log_mutex);
}

void close_log(){
    sem_wait(log_mutex);
    fclose(f);
    sem_post(log_mutex);
	sem_close(log_mutex);
	sem_unlink(LOG_MUTEX);
}
