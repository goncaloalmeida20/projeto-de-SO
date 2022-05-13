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
#include <pthread.h>
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
#define N_PROCESSES 3

EdgeServer * edge_servers;
int task_pipe_fd; // Task Manager, Monitor and Maintenance Manager
pid_t task_manager_pid, monitor_pid, maintenance_manager_pid; 
pthread_mutexattr_t monitor_attrmutex;
pthread_condattr_t monitor_attrcondv;
pthread_mutexattr_t perf_ch_attrmutex;
pthread_condattr_t perf_ch_attrcondv;
struct sigaction new_action;

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
                if(fscanf(fp," %[^,] , %d , %d ", edge_servers[i].name, &edge_servers[i].vcpu[0].processing_capacity, &edge_servers[i].vcpu[1].processing_capacity) != 3){
        			log_write("FORMAT ERROR IN CONFIG FILE");
        			return -1;
        		}
                #ifdef DEBUG
                printf("Just read from config file the edge server: %s,%d,%d\n", edge_servers[i].name, &edge_servers[i].vcpu[0].processing_capacity, &edge_servers[i].vcpu[1].processing_capacity);
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
	printf("SM CR\n");
    unlink(PIPE_NAME);
    pthread_cond_broadcast(monitor_cond);
    pthread_cond_broadcast(performance_changed_cond);
    pthread_cond_destroy(monitor_cond);
    pthread_cond_destroy(performance_changed_cond);
    pthread_condattr_destroy(&monitor_attrcondv);
    pthread_mutex_destroy(monitor_mutex);
    pthread_mutexattr_destroy(&monitor_attrmutex);
    pthread_condattr_destroy(&perf_ch_attrcondv);
    pthread_mutex_destroy(performance_changed_mutex);
    pthread_mutexattr_destroy(&perf_ch_attrmutex);
    msgctl(mqid, IPC_RMID, 0);
    close_shm();
    log_write("SIMULATOR CLOSING");
    close_log();
}

void wait_processes(){
	int i;
	#ifdef DEBUG
	printf("Waiting for processes to finish...\n");
	#endif
    for(i = 0; i < N_PROCESSES; i++){
     	printf("WAIT %d\n", i);
     	wait(NULL);
	}
	clean_resources();
}

void signal_handler(int signum) {
    if(signum == SIGINT){ // handling of CTRL-C
    	log_write("SIGNAL SIGINT RECEIVED");
    	kill(task_manager_pid, SIGUSR1);
    	kill(monitor_pid, SIGUSR1);
    	kill(maintenance_manager_pid, SIGUSR1);
        wait_processes();
        exit(0);
    }
    else if(signum == SIGTSTP){ // handling of CTRL-Z
    	sigprocmask(SIG_BLOCK, &block_set, NULL);
    	log_write("SIGNAL SIGTSTP RECEIVED");
        print_stats();
        sigprocmask(SIG_UNBLOCK, &block_set, NULL);
        //wait_processes();
    }
}

int main(int argc, char *argv[]){
	int i;

    if(argc != 2){
        printf("WRONG NUMBER OF PARAMETERS\n");
        exit(1);
    }
    
    sigfillset(&block_set); // will have all possible signals blocked when our handler is called
    sigprocmask(SIG_BLOCK, &block_set, NULL);

    
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

	monitor_mutex = get_monitor_mutex();
    // Initialize monitor mutex
    pthread_mutexattr_init(&monitor_attrmutex);
    pthread_mutexattr_setpshared(&monitor_attrmutex, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(monitor_mutex, &monitor_attrmutex);
    monitor_cond = get_monitor_cond();
    // Initialize monitor condition variable
    pthread_condattr_init(&monitor_attrcondv);
    pthread_condattr_setpshared(&monitor_attrcondv, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(monitor_cond, &monitor_attrcondv);
    
    performance_changed_mutex = get_performance_changed_mutex();
    // Initialize performance changed mutex
    pthread_mutexattr_init(&perf_ch_attrmutex);
    pthread_mutexattr_setpshared(&perf_ch_attrmutex, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(performance_changed_mutex, &perf_ch_attrmutex);
    performance_changed_cond = get_performance_changed_cond();
    // Initialize performance changed condition variable
    pthread_condattr_init(&perf_ch_attrcondv);
    pthread_condattr_setpshared(&perf_ch_attrcondv, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(performance_changed_cond, &perf_ch_attrcondv);

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
		printf("Edge Server %d: %s %d %d\n", i+1, es.name, es.vcpu[0].processing_capacity, es.vcpu[1].processing_capacity);
	}
	printf("Performance change flag: %d\n", get_performance_change_flag());
	shm_unlock();
	#endif

    // Create Task Manager
    if((task_manager_pid = fork()) == 0) {
        log_write("PROCESS TASK_MANAGER CREATED");
        task_manager();
        exit(0);
    }

    // Create Monitor
    if((monitor_pid = fork()) == 0){
        log_write("PROCESS MONITOR CREATED");
        monitor();
        exit(0);
    }

    // Create Maintenance Manager
    if((maintenance_manager_pid = fork()) == 0){
        log_write("PROCESS MAINTENANCE MANAGER CREATED");
        maintenance_manager(mqid, edge_server_number);
        exit(0);
    }
    
    

    //define a handler for SIGINT and SIGTSTP
    new_action.sa_flags = SA_RESTART;
    new_action.sa_mask = block_set;
    new_action.sa_handler = &signal_handler;

    sigaction(SIGINT,&new_action,NULL);
    
    //new set with all signals except SIGINT and SIGTSTP
    sigset_t no_sigint_sigtstp_set = block_set; 
    sigdelset(&no_sigint_sigtstp_set, SIGTSTP);
    sigdelset(&no_sigint_sigtstp_set, SIGINT);
    new_action.sa_mask = no_sigint_sigtstp_set;
    sigaction(SIGTSTP,&new_action,NULL);
	
	sigprocmask(SIG_UNBLOCK, &block_set, NULL);
	
    wait_processes();
    exit(0);
}
