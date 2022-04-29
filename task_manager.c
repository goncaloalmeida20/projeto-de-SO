/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include "task_manager.h"
#include "shared_memory.h"
#include "log.h"

typedef struct{
    long id;
    int thousand_inst;
    double max_exec_time;
    double arrival_time;
    int priority;
}Task;

int queue_size, fd;
Task *queue;
pthread_t scheduler_thread;
pthread_t dispatcher_thread;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t scheduler_signal = PTHREAD_COND_INITIALIZER, dispatcher_signal = PTHREAD_COND_INITIALIZER;


int add_task_to_queue(Task *t){
	#ifdef DEBUG_TM
	printf("Adding task %ld to the task queue...\n", t->id);
	#endif
	char msg[MSG_LEN];
	if(queue_size >= queue_pos){
		sprintf(msg, "TASK %ld DELETED (THE TASK QUEUE IS FULL)", t->id);
		log_write(msg);
		return -1;
	}
	queue[queue_size++] = *t;
	return 0;
}

int remove_task_from_queue(Task *t){
	int i, j;
	char msg[MSG_LEN];
	#ifdef DEBUG_TM
	printf("Removing task %ld from the task queue...\n", t->id);
	#endif
	for(i = 0; i < queue_size; i++){
		if(t->id == queue[i].id){
			//task found, remove task from queue
			queue_size--;
			for(j = i; j < queue_size; j++){
				queue[j] = queue[j+1];
			}
			return 0;
		}
	}
	sprintf(msg, "ERROR REMOVING TASK FROM QUEUE: TASK %ld NOT FOUND", t->id);
	log_write(msg);
	return -1;
}

void reevaluate_priorities(double current_time){
	int i, j;
	double time_left, time_left_temp;

	for(i = 0; i < queue_size; i++){
		queue[i].priority = 1;
		
		//current task's time left to execute
		time_left = queue[i].arrival_time + queue[i].max_exec_time - current_time;
		
		//compare with the other tasks
		for(j = 0; j < queue_size; j++){
			if(i != j){
				//other task's time left to execute
				time_left_temp = queue[j].arrival_time + queue[j].max_exec_time - current_time;
				
				//the number of tasks with less time left to execute will be equal to the priority
				if(time_left > time_left_temp) queue[i].priority++;
			}
		}
	}
}


void check_expired(double current_time){
	int i;
	char msg[MSG_LEN];
	for(i = 0; i < queue_size; i++){
		//check if task time has expired
		if(current_time > queue[i].arrival_time + queue[i].max_exec_time){
			if(remove_task_from_queue(&queue[i]) == 0){
				sprintf(msg, "SCHEDULER: TASK %ld HAS BEEN REMOVED FROM THE TASK QUEUE (MAXIMUM EXECUTION TIME EXPIRED)", queue[i].id);
				log_write(msg);
			}
				
		}
	}
}

void* scheduler(void *t){
	#ifdef BREAK_TM
	pthread_exit(NULL);
	#endif
	double current_time;
	while(1){
		pthread_mutex_lock(&queue_mutex);
		
		//wait for new task to arrive
		while(queue_size == 0)
			pthread_cond_wait(&scheduler_signal, &queue_mutex);
		
		//update current time
		current_time = get_current_time();
		
		#ifdef DEBUG_TM
		printf("Checking expired tasks...\n");
		#endif
		//check tasks whose maximum execution time has already passed
		check_expired(current_time);
		
		#ifdef DEBUG_TM
		printf("Reevaluating tasks priorities...\n");
		#endif
		//reevaluate tasks priorities
		reevaluate_priorities(current_time);
		
		#ifdef DEBUG_TM
		printf("Task queue:\n");
		for(int i = 0; i < queue_size; i++)
			printf("ID:%ld PRIORITY:%d MAX_EXEC_TIME: %lf ARRIVAL_TIME: %lf\n", queue[i].id, queue[i].priority, queue[i].max_exec_time, queue[i].arrival_time);
		#endif
		
		//Signal dispatcher if it is waiting 
		pthread_cond_signal(&dispatcher_signal);
		
		pthread_mutex_unlock(&queue_mutex);
		sleep(1);
	}
	pthread_exit(NULL);
}


void* dispatcher(void *t){
	int i, j;
	while(1){
		pthread_mutex_lock(&queue_mutex);
		
		while(queue_size == 0){
			#ifdef DEBUG_TM
			printf("Dispatcher waiting for signal\n");
			#endif
			pthread_cond_wait(&dispatcher_signal, &queue_mutex);
		}
		int found = 0;
		double ct = get_current_time();
		shm_lock();
		for(i = 0; i < edge_server_number; i++){
			#ifdef DEBUG_TM
			printf("Checking edge server %d\n", i + 1);
			#endif
			EdgeServer es = get_edge_server(i);
			if(es.performance_level == 0) continue;
			if((es.performance_level > 0 && es.next_available_time_min < ct) || (es.performance_level == 2 && es.next_available_time_max < ct)){
				shm_unlock();
				printf("aaaa\n");
				found = 1;
				VCPUTask t;
				for(j = 0; j < queue_size; j++){
					if(queue[j].priority == 1){
						#ifdef DEBUG_TM
						printf("Dispatcher selected task %ld for execution\n", queue[j].id);
						#endif
						t.id = queue[j].id;
						t.done = 0;
						t.thousand_inst = queue[j].thousand_inst;
						remove_task_from_queue(&queue[j]);
						break;
					}
				}
				write(unnamed_pipe[i][1], &t, sizeof(VCPUTask));
			}
		}
		if(!found)
			shm_unlock();
		pthread_mutex_unlock(&queue_mutex);
		usleep(1000);
	}
	pthread_exit(NULL);
}

void clean_tm_resources(){
	int i;
	
	pthread_join(scheduler_thread, NULL);
	
	//wait for edge server processes
	for(i = 0; i < edge_server_number; i++) wait(NULL);
	
	//clean unnamed pipe resources
	for(i = 0; i < edge_server_number; i++){
		close(unnamed_pipe[i][1]);
		free(unnamed_pipe[i]);
	}
	free(unnamed_pipe);
	
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&scheduler_signal);
	pthread_cond_destroy(&dispatcher_signal);
	free(queue);
}

void read_from_named_pipe(){
	int read_len;
	char msg[MSG_LEN], msg_temp[MSG_LEN*2];
	while(1){
		//read message from the named pipe
		if((read_len = read(fd, msg, MSG_LEN)) > 0){
			msg[read_len] = '\0';
			#ifdef DEBUG_TM
			printf("%s read from task pipe\n", msg);
			#endif
			Task t;
			if(sscanf(msg, "%ld;%d;%lf", &t.id, &t.thousand_inst, &t.max_exec_time) == 3){
				//new task arrived
				t.arrival_time = get_current_time();
				t.priority = 0;
				
				//add task to queue and signal scheduler that a new task has arrived
				pthread_mutex_lock(&queue_mutex);
				if(add_task_to_queue(&t) == 0){
					sprintf(msg, "TASK %ld ADDED TO THE QUEUE", t.id);
					log_write(msg);
					//signal scheduler
					pthread_cond_signal(&scheduler_signal);					
				}
				pthread_mutex_unlock(&queue_mutex);	
			}else if(strcmp(msg, "STATS") == 0){
				log_write("PRINT STATS");
				print_stats();
			}else if(strcmp(msg, "EXIT") == 0){
				log_write("EXIT");
				break;
			}else{
				sprintf(msg_temp, "WRONG COMMAND => %s", msg);
				log_write(msg_temp);
			}
			
		}
		sleep(1000);
		
	}
}

int task_manager(){
	int i;

	//create task queue
	queue = (Task *)malloc(queue_pos * sizeof(Task));
	if(queue == NULL){
		log_write("ERROR ALLOCATING MEMORY FOR TASK MANAGER QUEUE");
		return -1;
	}
	#ifdef DEBUG_TM
	printf("Creating edge servers...\n");
	#endif
	
	//create pipes
	unnamed_pipe = (int**) malloc(edge_server_number * sizeof(int*));
	for(i = 0; i < edge_server_number; i++){
		unnamed_pipe[i] = (int*) malloc(2 * sizeof(int));
		pipe(unnamed_pipe[i]);
	}
	//create edge server processes
	for(i = 0; i < edge_server_number; i++){
		//create edge server number i
		if(fork() == 0){
			edge_server(i+1);
			exit(0);
		}
		close(unnamed_pipe[i][0]);
	}
	
	
	
	queue_size = 0;
	
	#ifdef DEBUG_TM
	printf("Creating scheduler thread...\n");
	#endif
	//create scheduler thread
	pthread_create(&scheduler_thread, NULL, scheduler, NULL);
	//create dispatcher thread
	pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
	
	//opens the pipe for reading
	if ((fd = open(PIPE_NAME, O_RDONLY)) < 0) {
		log_write("ERROR OPENING PIPE FOR READING");
		return -1;
	}
	
	read_from_named_pipe();
	
	#ifdef TEST_TM
	sleep(1);
	Task t1 = {1,1,5,get_current_time(),1};
	add_task_to_queue(&t1);
	pthread_cond_signal(&scheduler_signal);
	sleep(1);
	Task t2 = {2,1,0.5,get_current_time(),1};
	add_task_to_queue(&t2);
	pthread_cond_signal(&scheduler_signal);
	sleep(1);
	Task t3 = {3,1,2,get_current_time(),1};
	add_task_to_queue(&t3);
	pthread_cond_signal(&scheduler_signal);
	sleep(1);
	Task t4 = {4,1,1.5,get_current_time(),1};
	add_task_to_queue(&t4);
	pthread_cond_signal(&scheduler_signal);
	#endif
	
	clean_tm_resources();
	return 0;
}
