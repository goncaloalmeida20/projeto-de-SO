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
#define STOP_WRITERS "STOP_WRITERS"

typedef struct {
    //performance flag for the monitor to change, percentage of tasks within the task manager,
    //minimum wait time for a new task be executed, total number of not executed tasks
    int performance_change_flag, tm_percentage, min_wait_time, n_not_executed_tasks;
    //average time of response by a task
    float avg_res_time;
    pthread_mutex_t dispatcher_mutex, monitor_mutex, performance_changed_mutex;
    pthread_cond_t dispatcher_cond, monitor_cond, performance_changed_cond;
} sh_mem;

sem_t *shm_mutex, *stop_writers;
sh_mem *shared_var;
int shmid, n_readers = 0;

int create_shm_sems(){
	sem_unlink(SHM_MUTEX);
	if((shm_mutex = sem_open(SHM_MUTEX,O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		log_write("ERROR CREATING SHM MUTEX");
		return -1;
	}	
		
	sem_unlink(STOP_WRITERS);
	if((stop_writers = sem_open(STOP_WRITERS,O_CREAT|O_EXCL,0700,1)) == SEM_FAILED){
		log_write("ERROR CREATING SHM STOP WRITERS SEMAPHORE");
		return -1;
	}		
	return 0;
}

int create_shm(){
	// Create shared memory
    if((shmid = shmget(IPC_PRIVATE, sizeof(sh_mem) + edge_server_number*sizeof(EdgeServer), IPC_CREAT | 0666)) < 0){
		log_write("ERROR CREATING SHARED MEMORY");
		return -1;
	}	
	
	//attach shared memory
	if((shared_var = (sh_mem *) shmat(shmid, NULL, 0)) == (sh_mem*)- 1){
		log_write("ERROR ATTACHING SHARED MEMORY");
		return -1;
	}
	
	//create shared memory mutex
	if(create_shm_sems() < 0){
		log_write("ERROR CREATING SHARED MEMORY MUTEX");
		return -1;
	}
	
	return 0;
}


//multiple readers but only one writer
//r-> reader, w-> writer
//always use unlock after lock (either both r or both w)
void shm_r_lock(){
	sem_wait(shm_mutex);
	n_readers++;
	if(n_readers == 1) sem_wait(stop_writers);
	sem_post(shm_mutex);
}

void shm_w_lock(){
	sem_wait(stop_writers);
}

void shm_r_unlock(){
	sem_wait(shm_mutex);
	n_readers--;
	if(n_readers == 0) sem_post(stop_writers);
	sem_post(shm_mutex);
}

void shm_w_unlock(){
	sem_post(stop_writers);
}



EdgeServer get_edge_server(int n){
	//return the edge server number n in the shared memory
	return *((EdgeServer*)(shared_var + 1) + n - 1);
}


void set_edge_server(EdgeServer* es, int n){
	//get a pointer to the edge server number n in the shared memory
	EdgeServer* edge_server_n = ((EdgeServer*)(shared_var + 1) + n - 1);
	
	//store the changes made to the edge server
	*edge_server_n = *es;
}

int get_performance_change_flag(){
	return shared_var->performance_change_flag;
}

void set_performance_change_flag(int pcf){
	shared_var->performance_change_flag = pcf;
}

int get_tm_percentage(){
    return shared_var->tm_percentage;
}

void set_tm_percentage(int p){
    shared_var->tm_percentage = p;
}

int get_min_wait_time(){
    return shared_var->min_wait_time;
}

void set_min_wait_time(int t){
    shared_var->min_wait_time = t;
}

int get_n_not_executed_tasks(){
    return shared_var->n_not_executed_tasks;
}

void set_n_not_executed_tasks(int n){
    shared_var->n_not_executed_tasks = n;
}

float get_avg_res_time(){
    return shared_var->avg_res_time;
}

void set_avg_res_time(float t){
    shared_var->avg_res_time = t;
}

pthread_mutex_t* get_dispatcher_mutex(){
	return &shared_var->dispatcher_mutex;
}

pthread_cond_t* get_dispatcher_cond(){
	return &shared_var->dispatcher_cond;
}

pthread_mutex_t* get_monitor_mutex(){
    return &shared_var->monitor_mutex;
}

pthread_cond_t* get_monitor_cond(){
    return &shared_var->monitor_cond;
}

pthread_mutex_t* get_performance_changed_mutex(){
    return &shared_var->performance_changed_mutex;
}

pthread_cond_t* get_performance_changed_cond(){
    return &shared_var->performance_changed_cond;
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
    shm_r_lock();
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
    shm_r_unlock();
}

void close_shm_sems(){
	sem_close(shm_mutex);
	sem_unlink(SHM_MUTEX);
	sem_close(stop_writers);
	sem_unlink(STOP_WRITERS);
}

//clean up resources used
void close_shm(){
	close_shm_sems();
	shmdt(shared_var);
	shmctl(shmid, IPC_RMID, NULL);
}
