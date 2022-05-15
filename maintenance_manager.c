/*
    Realizado por:
    João Bernardo de Jesus Santos, nº2020218995
    Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <time.h>
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
#include "maintenance_manager.h"
#include "shared_memory.h"
#include "log.h"

#define NAME_LEN 50

pthread_t * mm_thread;
int mqid, edge_server_number, * id;
sem_t maintenance_counter;
struct sigaction mm_new_action;

void clean_mm_resources(){
    free(id);
    sem_destroy(&maintenance_counter);
    free(mm_thread);
}

void mm_signal_handler(int signum) {
    if(signum == SIGUSR1){ // simulator closing
    	log_write("MAINTENANCE MANAGER CLEANING UP");
    	//cancel threads and wait for them to exit
    	for(int i = 0; i < edge_server_number; i++){ 
    		pthread_cancel(mm_thread[i]);
    		pthread_join(mm_thread[i], NULL);
    	}
    	clean_mm_resources();
    	log_write("MAINTENANCE MANAGER CLOSING");
        exit(0);
    }
}

void * maintenance(void *t){
    Message msg;
    int interval_of_mm, time_bw_mm;
    int es_id = *((int *) t);
    int mm_msg_type = es_id * 2 + 1, es_msg_type = es_id * 2;
    char log[MSG_LEN], es_name[NAME_LEN];
    
	srand(time(NULL));
	
	//block all signals in this thread
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	
	shm_r_lock();
	strcpy(es_name, get_edge_server(es_id).name);
	shm_r_unlock();
	
    // Maintenance of the Edge Servers
    while(1){
    	//generate random maintenance time and interval between maintenances
    	time_bw_mm = rand() % 5 + 1;
    	interval_of_mm = rand() % 5 + 1;
        sleep(time_bw_mm);
        
        //ensure that the edge servers aren't all in maintenance at the same time
        sem_wait(&maintenance_counter);
        
        //maintenances need to end before closing the program
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		
        //send maintenance start message to edge server
        msg.msg_type = mm_msg_type;
        strcpy(msg.msg_text, "START");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
        //receive confirmation from edge server
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
        if(strcmp(msg.msg_text, "ES_ABORT") == 0){
        	sprintf(log, "MAINTENANCE MANAGER RECEIVED ABORT MESSAGE FROM EDGE SERVER %d", es_id);
        	log_write(log);
        	sem_post(&maintenance_counter);
        	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        	break;
        } else if(strcmp(msg.msg_text, "ES_START") != 0){
        	sprintf(log, "MAINTENANCE MANAGER RECEIVED WRONG MESSAGE FROM EDGE SERVER %d", es_id);
        	log_write(log);
        	sem_post(&maintenance_counter);
        	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        	continue;
        }
        //do maintenance
        sleep(interval_of_mm);
        
        //send maintenance end message to edge server
        msg.msg_type = mm_msg_type;
        strcpy(msg.msg_text, "END");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
        //receive confirmation from edge server
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
        if(strcmp(msg.msg_text, "ES_END") != 0){
        	sprintf(log, "MAINTENANCE MANAGER RECEIVED WRONG MESSAGE FROM EDGE SERVER %d\n", es_id);
        	log_write(log);
        }
        
        sem_post(&maintenance_counter);
        
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    pthread_exit(NULL);
}

void maintenance_manager(int mq_id, int es_num) {
    Message msg;
    int i, es_msg_type;
    mqid = mq_id;
    edge_server_number = es_num;

    //define a handler for SIGUSR1
    mm_new_action.sa_flags = SA_RESTART;
    mm_new_action.sa_mask = block_set;
    mm_new_action.sa_handler = &mm_signal_handler;
    sigaction(SIGUSR1,&mm_new_action,NULL);
    
    //ignore SIGINT and SIGTSTP (these are handled by the system manager)
    mm_new_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &mm_new_action, NULL);
    sigaction(SIGTSTP, &mm_new_action, NULL);
    
    sem_init(&maintenance_counter, 0, edge_server_number - 1);
    
    //the maintenance manager is informed of the creation of the Edge Servers
    for(i = 0; i < edge_server_number; i++)
    {
        es_msg_type = (i + 1) * 2;
        //waits for a message
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
    }
	
    mm_thread = (pthread_t *) malloc(sizeof(pthread_t) * edge_server_number);
    id = (int *) malloc(sizeof(int) * edge_server_number);
	
	//create one thread to manage each edge server
    for(i = 0; i < edge_server_number; i++){
        id[i] = i + 1;
        pthread_create(&mm_thread[i], NULL, maintenance, &id[i]);
    }
	
	sigprocmask(SIG_UNBLOCK, &block_set, NULL);
	
	sigprocmask(SIG_BLOCK, &block_set_no_sigusr1, NULL);
	
    for(i = 0; i < edge_server_number; i++) pthread_join(mm_thread[i], NULL);
    clean_mm_resources();
}
