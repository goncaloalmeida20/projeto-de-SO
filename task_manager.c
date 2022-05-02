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

int queue_size, task_pipe_fd, scheduler_start = 0;
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
		while(!scheduler_start){
			#ifdef DEBUG_TM
			printf("Scheduler waiting for signal\n");
			#endif
			pthread_cond_wait(&scheduler_signal, &queue_mutex);
			#ifdef DEBUG_TM
			printf("Scheduler received signal\n");
			#endif
		}
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
		
		scheduler_start = 0;
		pthread_mutex_unlock(&queue_mutex);
	}
	pthread_exit(NULL);
}


void* dispatcher(void *t){
	int i, j;
	//fd_set read_pipes;
	while(1){
		pthread_mutex_lock(&queue_mutex);
		
		//Check if there are tasks in the task queue
		//If not, wait
		while(queue_size == 0){
			#ifdef DEBUG_TM
			printf("Dispatcher waiting for signal\n");
			#endif
			pthread_cond_wait(&dispatcher_signal, &queue_mutex);
			#ifdef DEBUG_TM
			printf("Dispatcher received signal\n");
			#endif
		}
		
		pthread_mutex_unlock(&queue_mutex);
		
		/*
		//Check if any edge servers have free cpus
		//(they will send that information through the unnamed pipes)
		FD_ZERO(&read_pipes);
		for(i = 0; i < edge_server_number; i++){
			FD_SET(unnamed_pipe[i][0], &read_pipes);
		}
		if(select(unnamed_pipe[edge_server_number-1][0] + 1, &read_pipes, NULL, NULL, NULL) > 0){*/
		
		
			//There is at least one edge server with a free vcpu
		int found = 0;
		double ct = get_current_time();
		shm_lock();
		
		//Check which servers have free vcpus
		for(i = 0; i < edge_server_number; i++){
		
			#ifdef DEBUG_TM
			printf("Checking edge server %d\n", i+1);
			#endif
			EdgeServer es = get_edge_server(i+1);
			if(es.performance_level == 0) continue;
			if((es.performance_level > 0 && es.next_available_time_min < ct) || (es.performance_level == 2 && es.next_available_time_max < ct)){
				#ifdef DEBUG_TM
				printf("Dispatcher: new task will be sent to Edge Server %s\n", es.name);
				#endif
				shm_unlock();
				found = 1;
				VCPUTask t;
				int min_priority = 0;
				
				pthread_mutex_lock(&queue_mutex);
				
				for(j = 0; j < queue_size; j++){
					if(queue[j].priority < queue[min_priority].priority)
						min_priority = j;
				}
				#ifdef DEBUG_TM
				printf("Dispatcher selected task %ld for execution\n", queue[min_priority].id);
				#endif
				t.id = queue[min_priority].id;
				t.done = 0;
				t.thousand_inst = queue[min_priority].thousand_inst;
				remove_task_from_queue(&queue[min_priority]);
				
				pthread_mutex_unlock(&queue_mutex);
				
				//Send task
				write(unnamed_pipe[i][1], &t, sizeof(VCPUTask));
				break;
			}
		}
		if(!found){
			printf("ES NOT FOUND\n");
			shm_unlock();
		}
		//}
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
	pthread_mutex_destroy(&vcpu_free_mutex);
	pthread_mutexattr_destroy(&vcpu_free_mutex_attr); 
	pthread_cond_destroy(&vcpu_free_cond);
	pthread_condattr_destroy(&vcpu_free_cond_attr);
	free(queue);
	close(task_pipe_fd);
}

void read_from_task_pipe(){
	int read_len;
	char msg[MSG_LEN], msg_temp[MSG_LEN*2];
	while(1){
		/*fd_set read_task_pipe;
		FD_ZERO(&read_task_pipe);
		FD_SET(task_pipe_fd, &read_task_pipe);*/
		
		//select(task_pipe_fd+1, &read_task_pipe, NULL, NULL, NULL) > 0 && 
		//read message from the named pipe if a task was sent
		if((read_len = read(task_pipe_fd, msg, MSG_LEN)) > 0){
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
					scheduler_start = 1;
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
	
	pthread_mutexattr_init(&vcpu_free_mutex_attr);
	pthread_mutexattr_setpshared(&vcpu_free_mutex_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&vcpu_free_mutex, &vcpu_free_mutex_cond);
	
	pthread_condattr_init(&vcpu_free_cond_attr);
	pthread_condattr_setpshared(&vcpu_free_cond_attr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(&vcpu_free_cond, &vcpu_free_cond_attr);
	
	//create edge server processes
	for(i = 0; i < edge_server_number; i++){
		//create edge server number i
		if(fork() == 0){
			edge_server(i+1);
			exit(0);
		}
	}
	
	
	queue_size = 0;
	
	#ifdef DEBUG_TM
	printf("Creating scheduler thread...\n");
	#endif
	//create scheduler thread
	pthread_create(&scheduler_thread, NULL, scheduler, NULL);
	//create dispatcher thread
	pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
	
	//opens the pipe in read-write mode for the function read
	//to block while waiting for new tasks to arrive
	if ((task_pipe_fd = open(PIPE_NAME, O_RDWR)) < 0) {
		log_write("ERROR OPENING PIPE FOR READING");
		continue;
	}
	
	read_from_task_pipe();
	
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
