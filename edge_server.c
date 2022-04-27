/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include "log.h"
#include "edge_server.h"
#include "shared_memory.h"
#include "maintenance_manager.h"

int edge_server_n;
char es_name[NAME_LEN];
pthread_mutex_t tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t maintenance_signal = PTHREAD_COND_INITIALIZER;
pthread_t vcpu_min_thread, vcpu_max_thread, maintenance_thread;

void *vcpu_min(void *t){
	pthread_exit(NULL);
}

void *vcpu_max(void *t){
	pthread_exit(NULL);
}

void clean_es_resources(){
	pthread_join(vcpu_min_thread, NULL);
	pthread_join(vcpu_max_thread, NULL);
    pthread_mutex_destroy(&tasks_mutex);
	pthread_join(maintenance_thread, NULL);
    pthread_cond_destroy(&maintenance_signal);
}

void * enter_maintenance(void * t){
    Message msg;
    char log[MSG_LEN];

    while(1){
        msgrcv(mqid, &msg, sizeof(Message), MM_TYPE, 0);

        //Wait until finishes all tasks
        pthread_cond_wait(&maintenance_signal, &tasks_mutex);

        msg.msg_type = 1;
        strcpy(msg.sender_name, es_name);
        strcpy(msg.msg_text, "START");
        msgsnd(mqid, &msg, sizeof(Message), 0);
        sprintf(log, "THE EDGE SERVER %s IS NOW ON MAINTENANCE", es_name);
        log_write(log);
    }

    pthread_exit(NULL);
}

int edge_server(int es_n){
	char msg[MSG_LEN];
	edge_server_n = es_n;
	
	shm_lock();
	EdgeServer this = get_edge_server(edge_server_n);
	shm_unlock();
	sprintf(msg, "%s READY", this.name);
    strcpy(es_name, this.name);
	log_write(msg);

    //notify the maintenance manager of the creation of the edge_server
    Message mm_msg;
    mm_msg.msg_type = 1;
    strcpy(mm_msg.sender_name, es_name);
    strcpy(mm_msg.msg_text, "ES CREATED");
    if (msgsnd(mqid, &mm_msg, sizeof(Message), 0) < 0){
        char inf[MSG_LEN];
        sprintf(inf, "IT WAS NOT POSSIBLY TO NOTIFY THE MAINTENANCE MANAGER OF THE CREATION OF THE EDGE SERVER %s", es_name);
        log_write(inf);
    }
	
	pthread_create(&vcpu_min_thread, NULL, vcpu_min, NULL);
	pthread_create(&vcpu_max_thread, NULL, vcpu_max, NULL);
	pthread_create(&maintenance_thread, NULL, enter_maintenance, NULL);
	
	clean_es_resources();
	return 0;
}
