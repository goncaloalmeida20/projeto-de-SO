/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include "shared_memory.h"
#include "log.h"

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
	//create shared memory
	//the shared memory will include one integer (a flag for the monitor to change
	//the perforce mode of the edge servers) and edge_server_number edge servers
	if((shmid = shmget(IPC_PRIVATE, sizeof(int) + edge_server_number*sizeof(EdgeServer), IPC_CREAT | 0700)) < 0){
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
	//return a pointer to the edge server number n in the shared memory
	return *(((EdgeServer*)(shared_var + 1)) + n - 1);
}


void set_edge_server(EdgeServer* es, int n){
	//get a pointer to the edge server number n in the shared memory
	EdgeServer* edge_server_n = ((EdgeServer*)(shared_var + 1)) + n - 1;
	
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
