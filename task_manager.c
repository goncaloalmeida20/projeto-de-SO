/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "log.h"
#include "task_manager.h"
#include "shared_memory.h"

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
pthread_mutexattr_t mutexattr;
pthread_condattr_t condattr;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t scheduler_cond = PTHREAD_COND_INITIALIZER;


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

void* scheduler(){
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
			pthread_cond_wait(&scheduler_cond, &queue_mutex);
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
		
		scheduler_start = 0;
		pthread_mutex_unlock(&queue_mutex);
		
		//Signal dispatcher if it is waiting 
		pthread_mutex_lock(dispatcher_mutex);
		pthread_cond_signal(dispatcher_cond);
		pthread_mutex_unlock(dispatcher_mutex);
	}
	pthread_exit(NULL);
}


int enough_time_left(VCPU *v, Task *t, double ct){
	//printf("ct%lf at%lf th%d prcp%d tempo%lf\n", ct, v->next_available_time, t->thousand_inst,  v->processing_capacity, t->thousand_inst/1000.0/v->processing_capacity);
	return ct > v->next_available_time && ct + t->thousand_inst/1000.0/v->processing_capacity < t->arrival_time + t->max_exec_time;
}


int get_free_edge_server(Task *t){
	int i;
	double ct = get_current_time();		
	//Check which servers have free vcpus
	for(i = 0; i < edge_server_number; i++){
		#ifdef DEBUG_TM
		printf("Checking edge server %d\n", i+1);
		#endif
		shm_lock();
		EdgeServer es = get_edge_server(i+1);
		if(es.performance_level == 0) continue;
        if((es.performance_level > 0 && enough_time_left(&es.min, t, ct)) || (es.performance_level == 2 && enough_time_left(&es.max, t, ct))){
			#ifdef DEBUG_TM
			printf("Dispatcher: Selecting Edge Server %s\n", es.name);
			#endif
			shm_unlock();
			return i+1;
		}
		shm_unlock();
	}
	return 0;

}


int check_free_edge_servers(){
	int i;
	double ct = get_current_time();		
	//Check which servers have free vcpus
	for(i = 0; i < edge_server_number; i++){
		#ifdef DEBUG_TM
		printf("Checking edge server %d\n", i+1);
		#endif
		shm_lock();
		EdgeServer es = get_edge_server(i+1);
		if(es.performance_level == 0) continue;
        if((es.performance_level > 0 && es.min.next_available_time < ct) || (es.performance_level == 2 && es.max.next_available_time < ct)){
			#ifdef DEBUG_TM
			printf("Dispatcher: There are free edge servers (%s)\n", es.name);
			#endif
			shm_unlock();
			return 1;
		}
		shm_unlock();
	}
	return 0;
}

void* dispatcher(){
	int i;
	char msg[MSG_LEN];
	VCPUTask t;
	int min_priority;
	int es;
	char * es_name;
	
	while(1){
		min_priority = 0;
		pthread_mutex_lock(&queue_mutex);
		pthread_mutex_lock(dispatcher_mutex);
		//Check if there are tasks in the task queue and free vcpus
		//If not, wait
		while(queue_size == 0 || !check_free_edge_servers()){
			#ifdef DEBUG_TM
			printf("Dispatcher waiting for signal\n");
			#endif
			pthread_mutex_unlock(&queue_mutex);
			pthread_cond_wait(dispatcher_cond, dispatcher_mutex);
			pthread_mutex_lock(&queue_mutex);
			#ifdef DEBUG_TM
			printf("Dispatcher received signal\n");
			#endif
		}
		
		es = 0;
		while(queue_size > 0){
			for(i = 0; i < queue_size; i++){
				if(queue[i].priority < queue[min_priority].priority)
					min_priority = i;
			}
			if((es = get_free_edge_server(&queue[min_priority]))){
				break;
			}
			
			sprintf(msg, "DISPATCHER: TASK %ld REMOVED FROM QUEUE (NOT ENOUGH TIME LEFT TO EXECUTE)", queue[min_priority].id);
			log_write(msg);
			remove_task_from_queue(&queue[min_priority]);
			
		}
		if(!es){
			printf("dsp No task selected\n");
			pthread_mutex_unlock(dispatcher_mutex);	
			pthread_mutex_unlock(&queue_mutex);
			continue;
		}
		
		t.id = queue[min_priority].id;
		t.done = 0;
		t.thousand_inst = queue[min_priority].thousand_inst;
		remove_task_from_queue(&queue[min_priority]);
		
		//get the name of the server where the task will be executed
		shm_lock();
		es_name = get_edge_server(es).name;
		shm_unlock();
		
		sprintf(msg, "DISPATCHER: TASK %d SELECTED FOR EXECUTION ON %s", t.id, es_name);
		log_write(msg);
		
		//Send task
		write(unnamed_pipe[es-1][1], &t, sizeof(VCPUTask));
		
		pthread_mutex_unlock(&queue_mutex);
		
		pthread_mutex_unlock(dispatcher_mutex);	
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
	pthread_cond_destroy(&scheduler_cond);
	pthread_cond_destroy(dispatcher_cond);
	pthread_mutex_destroy(dispatcher_mutex);
	pthread_mutexattr_destroy(&mutexattr); 
	pthread_condattr_destroy(&condattr);
	free(queue);
	close(task_pipe_fd);
}

void read_from_task_pipe(){
	int read_len;
	Task t;
	char msg[MSG_LEN], msg_temp[MSG_LEN*2], msg_aux[MSG_LEN];
	while(1){
		/*//read message from the named pipe
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
					pthread_cond_signal(&scheduler_cond);				
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
			
		}else{
			log_write("ERROR READING FROM TASK PIPE");
		}*/
		
		//read the first 4 bytes to check if the message is the command "EXIT"
		if((read_len = read(task_pipe_fd, msg, 4)) > 0){
			msg[read_len] = '\0';
			//check if the message is the command "EXIT"
			if(strcmp(msg, "EXIT") == 0){
				log_write("EXIT");
				break;
			}
			//the message isn't "EXIT", so the next byte is read to check if it is equal to "STATS"
			if((read_len = read(task_pipe_fd, msg_aux, 1)) > 0){
				//concatenate the new byte to the original message
				msg_aux[read_len] = '\0';
				strcat(msg, msg_aux);
				
				//check if the message is the command "STATS"
				if(strcmp(msg, "STATS") == 0){
					log_write("PRINT STATS");
					print_stats();
					continue;
				}
				
				//the message isn't "STATS" either, so read the remaining bytes to check if it is a task
				if((read_len = read(task_pipe_fd, msg_aux, MSG_LEN-5)) > 0){
					msg_aux[read_len] = '\0';
					strcat(msg, msg_aux);
					//check if the message is a task
					if(sscanf(msg, "%ld;%d;%lf", &t.id, &t.thousand_inst, &t.max_exec_time) == 3){
						//new task arrived
						t.arrival_time = get_current_time();
						t.priority = 0;
						
						//add task to queue and signal scheduler that a new task has arrived
						pthread_mutex_lock(&queue_mutex);
						if(add_task_to_queue(&t) == 0){
							sprintf(msg, "TASK %ld ADDED TO THE QUEUE", t.id);
							log_write(msg);
							//signal scheduler that a new task has arrived
							scheduler_start = 1;
							pthread_cond_signal(&scheduler_cond);				
						}
						pthread_mutex_unlock(&queue_mutex);	
					}else{ //the message is not a task either, so it is a wrong command
						sprintf(msg_temp, "WRONG COMMAND => %s", msg);
						log_write(msg_temp);
					}
				}
			}
		}else{
			log_write("ERROR READING FROM TASK PIPE");
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

	
	//create pipes
	unnamed_pipe = (int**) malloc(edge_server_number * sizeof(int*));
	for(i = 0; i < edge_server_number; i++){
		unnamed_pipe[i] = (int*) malloc(2 * sizeof(int));
		pipe(unnamed_pipe[i]);
	}
	
	//create the process shared mutex and conditional variable to synchronize the dispatcher
	dispatcher_mutex = get_dispatcher_mutex();
	dispatcher_cond = get_dispatcher_cond();
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(dispatcher_mutex, &mutexattr);
	pthread_condattr_init(&condattr);
	pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(dispatcher_cond, &condattr);

	#ifdef DEBUG_TM
	printf("Creating edge servers...\n");
	#endif
	
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
	printf("Creating scheduler and dispatcher thread...\n");
	#endif
	//create scheduler thread
	pthread_create(&scheduler_thread, NULL, scheduler, NULL);
	//create dispatcher thread
	pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
	
	//opens the pipe in read-write mode for the function read
	//to block while waiting for new tasks to arrive
	if ((task_pipe_fd = open(PIPE_NAME, O_RDWR)) < 0) {
		log_write("ERROR OPENING PIPE FOR READING");
		return -1;
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
