/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "log.h"
#include "monitor.h"
#include "edge_server.h"
#include "task_manager.h"
#include "shared_memory.h"
#include "maintenance_manager.h"

//#define DEBUG //uncomment this line to print debug messages

EdgeServer * edge_servers;
int nprocs = 3; // Task Manager, Monitor and Maintenance Manager
int task_pipe_fd;

int read_file(FILE *fp){
    int i = 0;

    if (fp != NULL){
        if(fscanf(fp,"%d", &queue_pos) != 1){
        	log_write("FORMAT ERROR IN CONFIG FILE");
        	return -1;
        }
        if(fscanf(fp,"%d", &max_wait) != 1){
        	log_write("FORMAT ERROR IN CONFIG FILE");
        	return -1;
        }
        if(fscanf(fp,"%d", &edge_server_number) != 1){
        	log_write("FORMAT ERROR IN CONFIG FILE");
        	return -1;
        }

        edge_servers = (EdgeServer *) malloc(sizeof(EdgeServer) * edge_server_number);
        if(edge_servers == NULL){
        	log_write("ERROR ALLOCATING MEMORY FOR THE EDGE SERVERS");
        	return -1;
        }

        if(edge_server_number >= 2){
            for(; i < edge_server_number; i++){
                if(fscanf(fp," %[^,] , %d , %d ", edge_servers[i].name, &edge_servers[i].min.processing_capacity, &edge_servers[i].max.processing_capacity) != 3){
        			log_write("FORMAT ERROR IN CONFIG FILE");
        			return -1;
        		}
                #ifdef DEBUG
                printf("Just read from config file the edge server: %s,%d,%d\n", edge_servers[i].name, edge_servers[i].processing_capacity_min, edge_servers[i].processing_capacity_max);
                #endif
            }
        }
        else{
            log_write("EDGE SERVER NUMBER NEEDS TO BE HIGHER THAN 1");
            fclose(fp);
            return -1;
        }
        fclose(fp);
        return 0;
    } else{
        log_write("ERROR OPENING CONFIG FILE");
        return -1;
    }
}

void clean_resources(){
    int i;
    
    #ifdef DEBUG
	printf("Waiting for processes to finish...\n");
	#endif
    for(i = 0; i < nprocs; i++) wait(NULL);
    msgctl(mqid, IPC_RMID, 0);
    unlink(PIPE_NAME);
    close_shm();
    pthread_cond_destroy(&monitor_cond);
    pthread_condattr_destroy(&attrcondv);
    pthread_mutex_destroy(&monitor_mutex);
    pthread_mutexattr_destroy(&attrmutex);
    close_log();
}

void sigint(int signum) { // handling of CTRL-C
    clean_resources();
    exit(0);
}

void sigtstp(int signum) { // handling of CTRL-Z
    print_stats();
}

int main(int argc, char *argv[]){
	int i;

    if(argc != 2){
        printf("WRONG NUMBER OF PARAMETERS\n");
        exit(1);
    }

    // Server terminates when CTRL-C is pressed
    // Redirect CTRL-C
    signal(SIGINT, sigint);

    // Redirect CTRL-Z
    signal(SIGTSTP, sigtstp);
	
	#ifdef DEBUG
	printf("Creating log...\n");
	#endif
    create_log();

    // Read from config file
    if(read_file(fopen(argv[1], "r")) < 0) {
        exit(1);
    }

    log_write("OFFLOAD SIMULATOR STARTING");
	
	#ifdef DEBUG
	printf("Creating shared memory...\n");
	#endif
    // Shared memory created
    if(create_shm() < 0) {
        exit(1);
    }

    // Creates the named pipe if it doesn't exist yet
    unlink(PIPE_NAME);
    if (mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) {
        perror("Cannot create pipe: ");
        exit(1);
    }

	shm_lock();

    // Create Message Queue
    if ((mqid = msgget(IPC_PRIVATE, IPC_CREAT|0777)) < 0)
    {
        log_write("ERROR CREATING MESSAGE QUEUE");
        exit(0);
    }

	#ifdef DEBUG
	printf("Saving the edge servers data and performance change flag in the shared memory...\n");
	#endif
	// Save the edge servers data in the shared memory 
	for(i = 0; i < edge_server_number; i++){
		#ifdef DEBUG
		printf("Setting edge server %s with number %d...\n", edge_servers[i].name, i+1);
		#endif
		set_edge_server(&edge_servers[i], i+1);
	}
	// Set the performance change flag to 0 (normal)
	set_performance_change_flag(0);
	shm_unlock();
	free(edge_servers);
	
	#ifdef DEBUG
	shm_lock();
	printf("Checking shared memory contents...\n");
	for(i = 0; i < edge_server_number; i++){
		EdgeServer es = get_edge_server(i+1);
		printf("Edge Server %d: %s %d %d\n", i+1, es.name, es.processing_capacity_min, es.processing_capacity_max);
	}
	printf("Performance change flag: %d\n", get_performance_change_flag());
	shm_unlock();
    #endif

    // Initialize attribute of mutex
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

    // Initialize attribute of condition variable
    pthread_condattr_init(&attrcondv);
    pthread_condattr_setpshared(&attrcondv, PTHREAD_PROCESS_SHARED);

    // Initialize mutex
    pthread_mutex_init(&monitor_mutex, &attrmutex);

    // Initialize condition variables
    pthread_cond_init(&monitor_cond, &attrcondv);

    // Create Task Manager
    if(fork() == 0) {
        log_write("PROCESS TASK_MANAGER CREATED");
        task_manager();
        exit(0);
    }

    // Create Monitor
    if(fork() == 0){
        log_write("PROCESS MONITOR CREATED");
        monitor();
        exit(0);
    }

    // Create Maintenance Manager
    if(fork() == 0){
        log_write("PROCESS MAINTENANCE MANAGER CREATED");
        maintenance_manager(mqid, edge_server_number);
        exit(0);
    }

    clean_resources();
    exit(0);
}
