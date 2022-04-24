/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/types.h>
#include "log.h"
#include "edge_server.h"
#include "shared_memory.h"
#include "maintenance_manager.h"

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
	EdgeServer this = get_edge_server(edge_server_n);
	shm_unlock();
	sprintf(msg, "%s READY", this.name);
	log_write(msg);

    //notify the maintenance manager of the creation of the edge_server
    Message mm_msg;
    mm_msg.msg_type = 1;
    strcpy(mm_msg.sender_name, this.name);
    strcpy(mm_msg.msg_text, "ES CREATED");
    if (msgsnd(mqid, &mm_msg, sizeof(Message), 0) < 0){
        char inf[MSG_LEN];
        sprintf(inf, "IT WAS NOT POSSIBLY TO NOTIFY THE MAINTENANCE MANAGER OF THE CREATION OF THE EDGE SERVER %s", this.name);
        log_write(inf);
    }
	
	pthread_create(&vcpu_min_thread, NULL, vcpu_min, NULL);
	pthread_create(&vcpu_max_thread, NULL, vcpu_max, NULL);
	
	clean_es_resources();
	return 0;
}
