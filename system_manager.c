#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "shared_memory.h"
#include "log.h"
#include "task_manager.h"

//#define DEBUG //uncomment this line to print debug messages

edgeServer * edge_servers;

int read_file(FILE *fp){
    int i = 0;

    if (fp != NULL){
        fscanf(fp,"%d", &queue_pos);
        fscanf(fp,"%d", &max_wait);
        fscanf(fp,"%d", &edge_server_number);

        edge_servers = (edgeServer *) malloc(sizeof(edgeServer) * edge_server_number);

        if(edge_server_number >= 2){
            for(; i < edge_server_number; i++){
                fscanf(fp," %[^,] , %d , %d ", edge_servers[i].name, &edge_servers[i].processing_capacity_min, &edge_servers[i].processing_capacity_max);
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
        log_write("ERROR IN CONFIG FILE");
        return -1;
    }
}

void clean_resources(int nprocs){
    int i;
    
    #ifdef DEBUG
	printf("Waiting for processes to finish...\n");
	#endif
    for(i = 0; i < nprocs; i++) wait(NULL);
    free(edge_servers);
    close_shm();
    close_log();
}

int main(int argc, char *argv[]){
	int i;

    if(argc != 2){
        printf("WRONG NUMBER OF PARAMETERS\n");
        exit(1);
    }
	
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
	
	
	shm_lock();
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
	
	#ifdef DEBUG
	shm_lock();
	printf("Checking shared memory contents...\n");
	for(i = 0; i < edge_server_number; i++){
		edgeServer *es = get_edge_server(i+1);
		printf("Edge Server %d: %s %d %d\n", i+1, es->name, es->processing_capacity_min, es->processing_capacity_max);
	}
	printf("Performance change flag: %d\n", get_performance_change_flag());
	shm_unlock();
	#endif
	
    // Create Task Manager
    if(fork() == 0) {
        log_write("PROCESS TASK_MANAGER CREATED");
        task_manager();
        exit(0);
    }

    // Create Monitor
    if(fork() == 0){
        log_write("PROCESS MONITOR CREATED");
        // What the Monitor will do

        exit(0);
    }

    // Create Maintenance Manager
    if(fork() == 0){
        log_write("PROCESS MAINTENANCE MANAGER CREATED");
        // What the Maintenance Manager will do

        exit(0);
    }

    clean_resources(3);
    exit(0);
}
