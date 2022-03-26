#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_THREADS 1000

int req_interval, thousand_inst_per_req, max_exec_time;
pthread_mutex_t req_mutex = PTHREAD_MUTEX_INITIALIZER;

void *request(void* t){
	int i, req_id = *((int*)t);
	
	pthread_mutex_lock(&req_mutex);
	printf("Request %d sent\n", req_id);
	for(i = 0; i < thousand_inst_per_req*1000; i++){
	}
	usleep(req_interval);
	pthread_mutex_unlock(&req_mutex);
	
	pthread_exit(NULL);
}

int main(int argc, char *argv[]){
	pthread_t threads[MAX_THREADS];
	int i, n_requests, id[MAX_THREADS];
	
	if(argc != 5){
		printf("Wrong number of parameters");
		exit(0);
	}
	
	n_requests = atoi(argv[1]);
	req_interval = atoi(argv[2]);
	thousand_inst_per_req = atoi(argv[3]);
	max_exec_time = atoi(argv[4]);
	
	for(i = 0; i < n_requests; i++){
		id[i] = i;
		pthread_create(&threads[i], NULL, request, &id[i]); 
	}
	
	for(i = 0; i < n_requests; i++) pthread_join(threads[i], NULL);
	
	pthread_mutex_destroy(&req_mutex);
	
	exit(0);
}
