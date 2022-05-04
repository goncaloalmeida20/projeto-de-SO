/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
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


int edge_server_n, wait_for_all_tasks_done = 0, min_start = 0, max_start = 0;
int performance_level, min_capacity, max_capacity, maintenance_done = 1;
VCPUTask current_task;
char es_name[NAME_LEN];
pthread_mutex_t maintenance_mutex = PTHREAD_MUTEX_INITIALIZER, tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t maintenance_signal = PTHREAD_COND_INITIALIZER, maintenance_done_signal = PTHREAD_COND_INITIALIZER, free_signal = PTHREAD_COND_INITIALIZER;
pthread_cond_t tasks_cond = PTHREAD_COND_INITIALIZER;
pthread_t vcpu_min_thread, vcpu_max_thread, maintenance_thread, performance_thread, task_thread;

double get_current_time(){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts.tv_sec+((double)ts.tv_nsec)/1000000000;
}

void *vcpu_min(){
	VCPUTask t;
	while(1){
		//Wait for task to arrive
		pthread_mutex_lock(&tasks_mutex);
		printf("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
		while(!min_start){
			pthread_cond_wait(&tasks_cond, &tasks_mutex);
			printf("CCCCCCCCCCCCCCCCCCC\n");
		}t = current_task;
		pthread_mutex_unlock(&tasks_mutex);
		printf("MIIIIIIIIIIIIIIIIIIIIN\n");
		sleep(1);
		
		//Task done
		char msg[MSG_LEN];
		sprintf(msg, "%s:TASK %d COMPLETED", es_name, t.id);
		log_write(msg);
		
		pthread_mutex_lock(&tasks_mutex);
		min_start = 0;
		pthread_cond_signal(&free_signal);
		pthread_mutex_unlock(&tasks_mutex);
		
		//Check if maintenance is going to happen
    	pthread_mutex_lock(&maintenance_mutex);
    	if(wait_for_all_tasks_done > 0){
    		wait_for_all_tasks_done--;
     		pthread_cond_signal(&maintenance_signal);
    	}
    	pthread_mutex_unlock(&maintenance_mutex);
    }
    pthread_exit(NULL);
}

void *vcpu_max(){
	VCPUTask t;
	while(1){
		//Wait for task to arrive
		pthread_mutex_lock(&tasks_mutex);
		printf("BBBBBBBOBBBBBB\n");
		while(!max_start)
			pthread_cond_wait(&tasks_cond, &tasks_mutex);
		t = current_task;
		pthread_mutex_unlock(&tasks_mutex);
		sleep(1);
		
		//Task done
		char msg[MSG_LEN];
		sprintf(msg, "%s:TASK %d COMPLETED", es_name, t.id);
		log_write(msg);
		
		pthread_mutex_lock(&tasks_mutex);
		max_start = 0;
		pthread_cond_signal(&free_signal);
		pthread_mutex_unlock(&tasks_mutex);
		
		//Check if maintenance is going to happen
    	pthread_mutex_lock(&maintenance_mutex);
    	if(wait_for_all_tasks_done > 0){
    		wait_for_all_tasks_done--;
     		pthread_cond_signal(&maintenance_signal);
    	}
    	pthread_mutex_unlock(&maintenance_mutex);
    }
    pthread_exit(NULL);
}

int ready_to_receive_task(){
	return performance_level != 0 && (!min_start || (performance_level == 2 && !max_start));
}

void *receive_tasks(){
	VCPUTask t;
	while(1){
		pthread_mutex_lock(&tasks_mutex);
		while(!ready_to_receive_task())
			pthread_cond_wait(&free_signal, &tasks_mutex);
		pthread_mutex_unlock(&tasks_mutex);
		
		pthread_mutex_lock(&vcpu_free_mutex);
		pthread_cond_signal(&vcpu_free_cond);
		pthread_mutex_unlock(&vcpu_free_mutex);
		
		read(unnamed_pipe[edge_server_n-1][0], &t, sizeof(VCPUTask));
		
		#ifdef DEBUG_ES
		printf("%s: Received task %d from unnamed_pipe %d\n", es_name, t.id, edge_server_n-1);
		#endif

		pthread_mutex_lock(&tasks_mutex);
		current_task = t;
		if(!min_start){
			#ifdef DEBUG_ES
			printf("%s: Sending task %d to minvcpu\n", es_name, t.id);
			#endif
			min_start = 1;
			pthread_cond_broadcast(&tasks_cond);
			pthread_mutex_unlock(&tasks_mutex);
		}else if(performance_level == 2 && !max_start){
			#ifdef DEBUG_ES
			printf("%s: Sending task %d to maxvcpu\n", es_name, t.id);
			#endif
			
			max_start = 1;
			pthread_cond_broadcast(&tasks_cond);
			pthread_mutex_unlock(&tasks_mutex);
		}else log_write("NO FREE VCPU FOUND");
		
	}
	pthread_exit(NULL);
}


void clean_es_resources(){
    pthread_join(performance_thread, NULL);
	pthread_join(vcpu_min_thread, NULL);
	pthread_join(vcpu_max_thread, NULL);
    pthread_mutex_destroy(&tasks_mutex);
    pthread_join(task_thread, NULL);
	pthread_join(maintenance_thread, NULL);
    pthread_cond_destroy(&maintenance_signal);
}

void * enter_maintenance(void * t){
    Message msg;
    char log[MSG_LEN];
    int mm_msg_type = edge_server_n * 2 + 1, es_msg_type = edge_server_n * 2, previous_performance_level;

    while(1){
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), mm_msg_type, 0);
        shm_lock();
        EdgeServer this = get_edge_server(edge_server_n);
        previous_performance_level = this.performance_level;
        this.performance_level = 0;
        set_edge_server(&this, edge_server_n);
        shm_unlock();


        pthread_mutex_lock(&maintenance_mutex);
        wait_for_all_tasks_done = previous_performance_level;
        // Wait until finishes all tasks
        while(wait_for_all_tasks_done > 0) pthread_cond_wait(&maintenance_signal, &tasks_mutex);

        wait_for_all_tasks_done = 0;
        pthread_mutex_unlock(&maintenance_mutex);

        msg.msg_type = es_msg_type;
        strcpy(msg.msg_text, "START");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
        sprintf(log, "THE EDGE SERVER %s IS NOW ON MAINTENANCE", es_name);
        log_write(log);

        // Wait for the maintenance to end
        msgrcv(mqid, &msg, sizeof(Message) - sizeof(long), mm_msg_type, 0);

        // Send maintenance done signal and get performance flag
        pthread_mutex_lock(&maintenance_mutex);
        pthread_cond_signal(&maintenance_done_signal);
        pthread_mutex_unlock(&maintenance_mutex);
        msg.msg_type = es_msg_type;
        strcpy(msg.msg_text, "END");
        msgsnd(mqid, &msg, sizeof(Message) - sizeof(long), 0);
    }

    pthread_exit(NULL);
}

void * check_performance(void * t){
    int pl;
    while(1){
        pl = get_performance_change_flag();
        shm_lock();
        EdgeServer this = get_edge_server(edge_server_n);
        this.performance_level = pl;
        set_edge_server(&this, edge_server_n);
        shm_unlock();
        sleep(1);
    }
    pthread_exit(NULL);
}

int edge_server(int es_n){
	char msg[MSG_LEN];
	edge_server_n = es_n;
	
	performance_level = 1;
	
	shm_lock();
	EdgeServer this = get_edge_server(edge_server_n);
	strcpy(es_name, this.name);
    min_capacity = this.min.processing_capacity;
    max_capacity = this.max.processing_capacity;
	this.performance_level = performance_level;
	set_edge_server(&this, edge_server_n);
	shm_unlock();
	
	sprintf(msg, "%s READY", es_name);
    
	log_write(msg);

    //notify the maintenance manager of the creation of the edge_server
    Message mm_msg;
    int es_msg_type = edge_server_n * 2;
    mm_msg.msg_type = es_msg_type;
    strcpy(mm_msg.msg_text, "ES CREATED");
    if (msgsnd(mqid, &mm_msg, sizeof(Message)-sizeof(long), 0) < 0){
        char inf[MSG_LEN];
        sprintf(inf, "IT WAS NOT POSSIBLE TO NOTIFY THE MAINTENANCE MANAGER OF THE CREATION OF THE EDGE SERVER %s", es_name);
        log_write(inf);
    }
    //log_write(strerror(errno));
    //printf("%ld %s\n", mm_msg.msg_type, mm_msg.msg_text);

	pthread_create(&vcpu_min_thread, NULL, vcpu_min, NULL);
	pthread_create(&vcpu_max_thread, NULL, vcpu_max, NULL);
	pthread_create(&maintenance_thread, NULL, enter_maintenance, NULL);
	pthread_create(&performance_thread, NULL, check_performance, NULL);
	pthread_create(&task_thread, NULL, receive_tasks, NULL);
	
	clean_es_resources();
	return 0;
}
