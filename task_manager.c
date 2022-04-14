#include <stdlib.h>
#include <time.h>
#include "task_manager.h"

double get_current_time(){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts.tv_sec+((double)ts.tv_sec)/1000000000;
}

void add_task_to_queue(Task t){
	queue[queue_size++] = t;
}

void reevaluate_priorities(double current_time){
	int i, j;
	double time_left, time_left_temp;
	for(i = 0; i < queue_size; i++){
		queue[i].priority = 1;
		
		//current task's time left to execute
		time_left = queue[i].arrival_time + queue[i].max_exec_time - current_time;
		
		//compare with the other tasks
		for(j = 0; j <= queue_size; j++){
			//other task's time left to execute
			time_left_temp = queue[j].arrival_time + queue[j].max_exec_time - current_time;
			
			//the number of tasks with less time left to execute will be equal to the priority
			if(time_left > time_left_temp) queue[i].priority++;
		}
	}
}


void check_expired(double current_time){
	int i, j;
	for(i = 0; i < queue_size; i++){
		//check if task time has expired
		if(current_time > queue[i].arrival_time + queue[i].max_exec_time){
			char msg[100];
			sprintf(msg, "Scheduler: Task %d has been removed from the task queue (Maximum execution time has already passed)", queue[i].id);
			log_write(msg);
			//remove task from queue
			queue_size--;
			for(j = i; j < queue_size; j++){
				queue[j] = queue[j+1];
			}
		}
	}
}

void* scheduler(void *t){
	double current_time;
	while(1){
		pthread_mutex_lock(&queue_mutex);
		
		//wait for new task to arrive
		pthread_cond_wait(&scheduler_signal, &queue_mutex);
		
		//update current time
		current_time = get_current_time();
		
		//check tasks whose maximum execution time has already passed
		check_expired(current_time);
		
		//reevaluate tasks' priorities
		reevaluate_priorities(current_time);
		
		pthread_mutex_unlock(&queue_mutex);
	}
	pthread_exit(NULL);
}

void clean_tm_resources(){
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&scheduler_signal);
	free(queue);
}

int task_manager(const int QUEUE_POS){
	//create task queue
	queue = (Task *)malloc(QUEUE_POS * sizeof(Task));
	if(queue == NULL){
		log_write("Error allocating memory for task manager queue");
		return -1;
	}
	
	queue_size = 0;
	
	//create scheduler thread
	pthread_create(&scheduler_thread, NULL, scheduler, NULL);
	
	
	pthread_join(scheduler_thread, NULL);
	clean_tm_resources();
	return 0;
}
