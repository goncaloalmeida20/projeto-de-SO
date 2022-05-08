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


int edge_server_n, wait_for_all_tasks_done = 0, vcpu_start[2];
int performance_level, vcpu_capacity[2], maintenance_done = 1, ids[2];
VCPUTask current_task;
char es_name[NAME_LEN];
pthread_mutex_t maintenance_mutex = PTHREAD_MUTEX_INITIALIZER, tasks_mutex = PTHREAD_MUTEX_INITIALIZER, free_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t maintenance_signal = PTHREAD_COND_INITIALIZER, maintenance_done_signal = PTHREAD_COND_INITIALIZER, free_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t tasks_cond = PTHREAD_COND_INITIALIZER;
pthread_t vcpu_thread[2], maintenance_thread, performance_thread, task_thread;

double get_current_time(){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts.tv_sec+((double)ts.tv_nsec)/1000000000;
}

double task_time_sec(int capacity, int thousand_inst){
	return thousand_inst/1000.0/capacity;
}

void *vcpu(void *vcpu_id){
	int id = *((int*)vcpu_id);
	VCPUTask t;
	while(1){
		//Wait for task to arrive
		pthread_mutex_lock(&tasks_mutex);
		while(!vcpu_start[id])
			pthread_cond_wait(&tasks_cond, &tasks_mutex);
		t = current_task;
		pthread_mutex_unlock(&tasks_mutex);
		
		#ifdef DEBUG_ES
		printf("%s: starting task %d\n", es_name, t.id);
		#endif
		
		//Execute task
		usleep(1000000*task_time_sec(vcpu_capacity[id], t.thousand_inst));
		
		//Task done
		char msg[MSG_LEN];
		sprintf(msg, "%s VCPU %d:TASK %d COMPLETED", es_name, id, t.id);
		log_write(msg);
		
		pthread_mutex_lock(&free_mutex);
		vcpu_start[id] = 0;
		pthread_cond_signal(&free_cond);
		pthread_mutex_unlock(&free_mutex);
		
		/*//Check if maintenance is going to happen
    	pthread_mutex_lock(&maintenance_mutex);
    	if(wait_for_all_tasks_done > 0){
    		wait_for_all_tasks_done--;
     		pthread_cond_signal(&maintenance_signal);
    	}
    	pthread_mutex_unlock(&maintenance_mutex);*/
    }
    pthread_exit(NULL);
}


int ready_to_receive_task(){
	return performance_level != 0 && (!vcpu_start[0] || (performance_level == 2 && !vcpu_start[1]));
}

void *receive_tasks(){
	VCPUTask t;
	EdgeServer this;
	int first = 1;
	shm_lock();
	this = get_edge_server(edge_server_n);
	for(int i = 0; i < 2; i++) this.vcpu[i].next_available_time = 0;
	set_edge_server(&this, edge_server_n);
	shm_unlock();
	while(1){
		pthread_mutex_lock(&free_mutex);
		while(!ready_to_receive_task())
			pthread_cond_wait(&free_cond, &free_mutex);
		pthread_mutex_unlock(&free_mutex);

		if(first) first = 0;
		else{
			pthread_mutex_lock(dispatcher_mutex);
			pthread_cond_signal(dispatcher_cond);
			pthread_mutex_unlock(dispatcher_mutex);
		}
		#ifdef DEBUG_ES
		printf("%s: Waiting for task in unnamed_pipe %d\n", es_name, edge_server_n-1);
		#endif
		
		read(unnamed_pipe[edge_server_n-1][0], &t, sizeof(VCPUTask));
		
		#ifdef DEBUG_ES
		printf("%s: Received task %d from unnamed_pipe %d\n", es_name, t.id, edge_server_n-1);
		#endif

		pthread_mutex_lock(&tasks_mutex);
		current_task = t;
		double ct = get_current_time();
		#ifdef DEBUG_ES
		printf("%s: Sending task %d to %d\n", es_name, t.id, t.vcpu);
		#endif
		shm_lock();
		this = get_edge_server(edge_server_n);
		this.vcpu[t.vcpu].next_available_time = ct + task_time_sec(this.vcpu[t.vcpu].processing_capacity, current_task.thousand_inst);
		set_edge_server(&this, edge_server_n);
		shm_unlock();
		vcpu_start[t.vcpu] = 1;
		pthread_cond_broadcast(&tasks_cond);
		pthread_mutex_unlock(&tasks_mutex);
		/*if(!vcpu_start[0]){
			#ifdef DEBUG_ES
			printf("%s: Sending task %d to minvcpu\n", es_name, t.id);
			#endif
			shm_lock();
			this = get_edge_server(edge_server_n);
			this.vcpu[0].next_available_time = ct + task_time_sec(this.vcpu[0].processing_capacity, current_task.thousand_inst);
			set_edge_server(&this, edge_server_n);
			shm_unlock();
			vcpu_start[0] = 1;
			pthread_cond_broadcast(&tasks_cond);
			pthread_mutex_unlock(&tasks_mutex);
		}else if(performance_level == 2 && !vcpu_start[1]){
			#ifdef DEBUG_ES
			printf("%s: Sending task %d to maxvcpu\n", es_name, t.id);
			#endif
			shm_lock();
			this = get_edge_server(edge_server_n);
			this.vcpu[1].next_available_time = ct + task_time_sec(this.vcpu[1].processing_capacity, current_task.thousand_inst);
			set_edge_server(&this, edge_server_n);
			shm_unlock();
			vcpu_start[1] = 1;
			pthread_cond_broadcast(&tasks_cond);
			pthread_mutex_unlock(&tasks_mutex);
		}else log_write("NO FREE VCPU FOUND");*/
		
		pthread_mutex_lock(dispatcher_mutex);
		printf("%s: SENDING SIGNAL\n", es_name);
		pthread_cond_signal(dispatcher_cond);
		pthread_mutex_unlock(dispatcher_mutex);
		
	}
	pthread_exit(NULL);
}


void clean_es_resources(){
    pthread_join(performance_thread, NULL);
    for(int i = 0; i < 2; i++) pthread_join(vcpu_thread[i], NULL);
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
        this.n_maintenances++;
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
	
	performance_level = 2;
	
	shm_lock();
	EdgeServer this = get_edge_server(edge_server_n);
	strcpy(es_name, this.name);
    for(int i = 0; i < 2; i++) vcpu_capacity[i] = this.vcpu[i].processing_capacity;
	this.performance_level = performance_level;
    this.n_maintenances = 0;
    this.n_tasks_done = 0;
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
	for(int i = 0; i < 2; i++){
		ids[i] = i;
		pthread_create(&vcpu_thread[i], NULL, vcpu, &ids[i]);
	}
	
	//pthread_create(&maintenance_thread, NULL, enter_maintenance, NULL);
	//pthread_create(&performance_thread, NULL, check_performance, NULL);
	pthread_create(&task_thread, NULL, receive_tasks, NULL);
	
	clean_es_resources();
	return 0;
}
