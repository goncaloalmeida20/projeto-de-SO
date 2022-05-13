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

pthread_t * mm_thread;
int mqid, edge_server_number, * id, mm_leave_flag = 0;
sem_t maintenance_counter;
sigset_t mm_block_set;
struct sigaction mm_new_action;

void clean_mm_resources(){
    free(id);
    sem_destroy(&maintenance_counter);
    free(mm_thread);
}

void mm_termination_handler(int signum) {
    if(signum == SIGUSR1){ // handling of SIGUSR1
    	printf("Maintenance manager: sigusr1\n");
    	mm_leave_flag = 1;
    	for(int i = 0; i < edge_server_number; i++){ 
    		pthread_cancel(mm_thread[i]);
    		pthread_join(mm_thread[i], NULL);
    	}
    	clean_mm_resources();
    	printf("MM DIED\n");
        exit(0);
    }
    printf("MM unexpected signal %d\n", signum);
}

void * maintenance(void *t){
    Message msg;
    int interval_of_mm, time_bw_mm;
    int es_id = *((int *) t);
    int mm_msg_type = es_id * 2 + 1, es_msg_type = es_id * 2;
    
	srand(time(NULL));
	
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	
    // Maintenance of the Edge Servers
    while(1){
    	time_bw_mm = rand() % 5 + 1;
    	interval_of_mm = rand() % 5 + 1;
        sleep(time_bw_mm);
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        sem_wait(&maintenance_counter);

        
        msg.msg_type = mm_msg_type;
        strcpy(msg.msg_text, "START");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
        sleep(interval_of_mm);
        
        msg.msg_type = mm_msg_type;
        strcpy(msg.msg_text, "END");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
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
    
    //sigfillset(&mm_block_set); // will have all possible signals blocked when our handler is called

    //define a handler for SIGINT; when entered all possible signals are blocked
    mm_new_action.sa_flags = 0;
    mm_new_action.sa_mask = block_set;
    mm_new_action.sa_handler = &mm_termination_handler;

    sigaction(SIGUSR1,&mm_new_action,NULL);
    
    mm_new_action.sa_handler = SIG_IGN;
    
    sigaction(SIGINT, &mm_new_action, NULL);
    sigaction(SIGTSTP, &mm_new_action, NULL);
    
    sem_init(&maintenance_counter, 0, edge_server_number - 1);
    // The Maintenance Manager is informed of the creation of the Edge Servers
    for(i = 0; i < edge_server_number; i++)
    {
        es_msg_type = (i + 1) * 2;
        // Waits for a message
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), es_msg_type, 0);
        //printf("mm %s %d\n", msg.msg_text, es_msg_type);
    }

    mm_thread = (pthread_t *) malloc(sizeof(pthread_t) * edge_server_number);
    id = (int *) malloc(sizeof(int) * edge_server_number);
	
	sigprocmask(SIG_UNBLOCK, &block_set, NULL);
	
    for(i = 0; i < edge_server_number; i++){
        id[i] = i + 1;
        pthread_create(&mm_thread[i], NULL, maintenance, &id[i]);
    }
	
    for(i = 0; i < edge_server_number; i++) pthread_join(mm_thread[i], NULL);
    clean_mm_resources();
}
