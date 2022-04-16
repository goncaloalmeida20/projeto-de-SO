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

typedef struct {
    int queue_pos, max_wait, edge_server_number;
    edgeServer * edge_servers;
}configData;

configData * file_data;

int read_file(FILE *fp){
    int i = 0;
    file_data = (configData *) malloc(sizeof(configData));

    if (fp != NULL){
        fscanf(fp,"%d", &file_data->queue_pos);
        fscanf(fp,"%d", &file_data->max_wait);
        fscanf(fp,"%d", &file_data->edge_server_number);

        file_data->edge_servers = (edgeServer *) malloc(sizeof(edgeServer) * file_data->edge_server_number);

        if(file_data->edge_server_number >= 2){
            for(; i < file_data->edge_server_number; i++){
                fscanf(fp,"%s,%d,%d", file_data->edge_servers[i].name, &file_data->edge_servers[i].processing_capacity_min, &file_data->edge_servers[i].processing_capacity_max);
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
    free(file_data->edge_servers);
    free(file_data);
    for(i = 0; i < nprocs; i++) wait(NULL);
    close_shm();
    close_log();
}

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("WRONG NUMBER OF PARAMETERS\n");
        exit(1);
    }

    create_log();

    // Read from config file
    if(read_file(fopen(argv[1], "r")) < 0) {
        exit(1);
    }

    log_write("OFFLOAD SIMULATOR STARTING");

    // Shared memory created
    if(create_shm() < 0) {
        exit(1);
    }

    // Create Task Manager
    if(fork() == 0) {
        log_write("PROCESS TASK_MANAGER CREATED");
        task_manager(file_data->queue_pos);
        exit(0);
    }

    // Create Monitor
    if(fork() == 0){
        log_write("PROCESS MONITOR CREATED");
        // What the Monitor will do

        // Bye bye Monitor
        exit(0);
    }

    // Create Maintenance Manager
    if(fork() == 0){
        log_write("MAINTENANCE MANAGER CREATED");
        // What the Maintenance Manager will do

        // Bye bye Maintenance Manager
        exit(0);
    }

    clean_resources(3);
    exit(0);
}
