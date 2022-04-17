#include <stdio.h>
#include <pthread.h>
#include "edge_server.h"
#include "log.h"
#include "shared_memory.h"

pthread_t vcpu_min_thread, vcpu_max_thread;
int edge_server_n;

void *vcpu_min(void *t){
	pthread_exit(NULL);
}

void *vcpu_max(void *t){
	pthread_exit(NULL);
}

void clean_es_resources(){
	pthread_join(vcpu_min_thread, NULL);
	pthread_join(vcpu_max_thread, NULL);
}

int edge_server(int es_n){
	char msg[MSG_LEN];
	edge_server_n = es_n;
	
	shm_lock();
	edgeServer this = get_edge_server(edge_server_n);
	shm_unlock();
	sprintf(msg, "%s READY", this.name);
	log_write(msg);
	
	pthread_create(&vcpu_min_thread, NULL, vcpu_min, NULL);
	pthread_create(&vcpu_max_thread, NULL, vcpu_max, NULL);
	
	clean_es_resources();
	return 0;
}
