/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include "log.h"
#include "shared_memory.h"
#include "edge_server.h"

#define SHM_MUTEX "SHM_MUTEX"

sem_t* shm_mutex;
int *shared_var, shmid;

int create_shm_mutex(){
	sem_unlink(SHM_MUTEX);
	if((shm_mutex = sem_open(SHM_MUTEX,O_CREAT|O_EXCL,0700,1)) == SEM_FAILED)	
		return -1;
		
	return 0;
}

int create_shm(){
	// Create shared memory
	// The shared memory will include five integers (a flag for the monitor to change
	// the performance mode of the edge servers, the percentage of tasks within the task manager,
    // the minimum wait time for a new task be executed, the total number of executed and not executed tasks)
    // the average time of response by a task and edge_server_number edge servers
    if((shmid = shmget(IPC_PRIVATE, sizeof(int) * 4 + sizeof(float) + edge_server_number*sizeof(EdgeServer) + sizeof(pthread_mutex_t) * 3 + sizeof(pthread_cond_t) * 3, IPC_CREAT | 0700)) < 0){
		log_write("ERROR CREATING SHARED MEMORY");
		return -1;
	}	
	
	//attach shared memory
	if((shared_var = (int*)shmat(shmid, NULL, 0)) == (int*)-1){
		log_write("ERROR ATTACHING SHARED MEMORY");
		return -1;
	}
	
	//create shared memory mutex
	if(create_shm_mutex() < 0){
		log_write("ERROR CREATING SHARED MEMORY MUTEX");
		return -1;
	}
	
	return 0;
}


//always use shm_lock before accessing the shared memory
//and shm_unlock after the access
void shm_lock(){
	sem_wait(shm_mutex);
}

void shm_unlock(){
	sem_post(shm_mutex);
}


EdgeServer get_edge_server(int n){
	//return the edge server number n in the shared memory
	return *(((EdgeServer*)(((float*)(shared_var + 4))+1)) + n - 1);
}


void set_edge_server(EdgeServer* es, int n){
	//get a pointer to the edge server number n in the shared memory
	EdgeServer* edge_server_n = ((EdgeServer*)(((float*)(shared_var + 4))+1)) + n - 1;
	
	//store the changes made to the edge server
	*edge_server_n = *es;
}

int get_performance_change_flag(){
	//return the performance change flag which is the 
	//first integer stored in the shared memory
	return *shared_var;
}

void set_performance_change_flag(int pcf){
	//store the new performance change flag which is the 
	//first integer stored in the shared memory
	*shared_var = pcf;
}

int get_tm_percentage(){
    //return the percentage of tasks within the task manager
    //which is the second integer stored in the shared memory
    return *(shared_var + 1);
}

void set_tm_percentage(int p){
    //store the new percentage of tasks within the task manager
    //which is the second integer stored in the shared memory
    *(shared_var + 1) = p;
}


int get_min_wait_time(){
    //return the minimum wait time for a new task be executed
    //which is the third integer stored in the shared memory
    return *(shared_var + 2);
}

void set_min_wait_time(int t){
    //store the minimum wait time for a new task be executed
    //which is the third integer stored in the shared memory
    *(shared_var + 2) = t;
}

int get_n_not_executed_tasks(){
    //return the total number of not executed tasks
    //which is the fifth integer stored in the shared memory
    return *(shared_var + 3);
}

void set_n_not_executed_tasks(int n){
    //store the total number of not executed tasks
    //which is the fifth integer stored in the shared memory
    *(shared_var + 3) = n;
}

float get_avg_res_time(){
    //return the average time of response by a task
    //which is a float stored in the shared memory
    return *((float*)(shared_var + 4));
}

void set_avg_res_time(float t){
    //store the average time of response by a task
    //which is a float stored in the shared memory
    float* avg_res_time = ((float*)(shared_var + 4));
    *avg_res_time = t;
}


pthread_mutex_t* get_dispatcher_mutex(){
	return (pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number);
}

pthread_cond_t* get_dispatcher_cond(){
	return (pthread_cond_t*)((pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number)+3);
}

pthread_mutex_t* get_monitor_mutex(){
    return (pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number) + 1;
}

pthread_cond_t* get_monitor_cond(){
    return (pthread_cond_t*)((pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number)+3) + 1;
}

pthread_mutex_t* get_performance_changed_mutex(){
    return (pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number) + 2;
}

pthread_cond_t* get_performance_changed_cond(){
    return (pthread_cond_t*)((pthread_mutex_t*)(((EdgeServer*)(((float*)(shared_var + 4))+1)) + edge_server_number)+3) + 2;
}

int get_n_executed_tasks(){
    //return the total number of executed tasks
    int sum = 0;
    for(int i = 0; i < edge_server_number; i++){
        EdgeServer this = get_edge_server(i+1);
        sum += this.n_tasks_done;
    }
    return sum;
}

void print_stats(){
    int i, n_maintenances, n_tasks_done, n_exec_tasks, n_not_exec_tasks;
    float avg_time_of_res;
    shm_lock();
    n_exec_tasks = get_n_executed_tasks();
    n_not_exec_tasks = get_n_not_executed_tasks();
    avg_time_of_res = get_avg_res_time();
    printf("Number of executed tasks: %d\n", n_exec_tasks);
    printf("Number of not executed tasks: %d\n", n_not_exec_tasks);
    printf("Average task response time: %f\n", avg_time_of_res);
    printf("Number of tasks executed and number of maintenances done in each Edge Server:\n");
    for(i = 1; i <= edge_server_number; i++){
        EdgeServer this = get_edge_server(i);
        n_maintenances = this.n_maintenances;
        n_tasks_done = this.n_tasks_done;
        printf("Edge server %d:\n\tNumber of tasks executed: %d\n\tNumber of maintenances done: %d\n", i, n_tasks_done, n_maintenances);
    }
    shm_unlock();
}

void close_shm_mutex(){
	sem_close(shm_mutex);
	sem_unlink(SHM_MUTEX);
}

//clean up resources used
void close_shm(){
	close_shm_mutex();
	shmdt(shared_var);
	shmctl(shmid, IPC_RMID, NULL);
}
