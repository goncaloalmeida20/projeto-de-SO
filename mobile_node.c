#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct request{
	int thousand_inst;
	int max_exec_time;
}Request;

Request r;

int main(int argc, char *argv[]){
	int i, n_requests, req_interval_ms;
	
	if(argc != 5){
		printf("Wrong number of parameters\n");
		exit(0);
	}
	
	n_requests = atoi(argv[1]);
	req_interval_ms = atoi(argv[2]);
	r.thousand_inst = atoi(argv[3]);
	r.max_exec_time = atoi(argv[4]);
	
	for(i = 0; i < n_requests; i++){
		printf("Generating request %d with %d instructions and with max execution time %d s\n", i, r.thousand_inst*1000, r.max_exec_time);
		printf("Sleeping %d milliseconds...\n", req_interval_ms);
		usleep(req_interval_ms*1000);
	}
	
	exit(0);
}