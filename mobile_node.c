/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

//#define DEBUG //uncomment this line to print debug messages
#define MSG_LEN 100
#define PIPE_NAME "named_pipe"

//number of digits of a number
//e.g. 10->2, 2000->4, 555->3
int digits(int n){
	int i;
	for(i = 0; n != 0; n/=10, i++);
	return i;
}

//generate a task id based on current process id and the
//size of n_requests
long generate_task_id(int n_requests, int i){
	return getpid() * pow(10, digits(n_requests)) + i;
}	

int main(int argc, char *argv[]){
	int i, n_requests, req_interval_ms, thousand_inst, fd;
	double max_exec_time;
	char msg[MSG_LEN];
	
	if(argc != 5){
		perror("Wrong number of parameters\n");
		exit(0);
	}
	
	n_requests = atoi(argv[1]);
	req_interval_ms = atoi(argv[2]);
	thousand_inst = atoi(argv[3]);
	max_exec_time = atof(argv[4]);
	
	// Opens the pipe for reading
	if ((fd = open(PIPE_NAME, O_WRONLY)) < 0) {
		perror("Cannot open pipe for reading: ");
		exit(0);
	}	
	
	for(i = 0; i < n_requests; i++){
		#ifdef DEBUG
		printf("Generating request %ld with %d instructions and with max execution time %lf s\n", generate_task_id(n_requests, i), thousand_inst*1000, max_exec_time);
		printf("Sleeping %d milliseconds...\n", req_interval_ms);
		#endif
		sprintf(msg, "%ld;%d;%lf", generate_task_id(n_requests, i), thousand_inst, max_exec_time);
		write(fd, msg, MSG_LEN);
		if(i != n_requests - 1)
			usleep(req_interval_ms*1000);
	}
	close(fd);
	exit(0);
}
