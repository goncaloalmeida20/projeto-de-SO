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

int queue_size, task_pipe_fd, scheduler_start = 0, tm_leave_flag = 0;
Task *queue;
pid_t *edge_servers_pid;
pthread_t scheduler_thread, dispatcher_thread, read_thread;
pthread_mutexattr_t mutexattr;
pthread_condattr_t condattr;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t scheduler_cond = PTHREAD_COND_INITIALIZER;
struct sigaction tm_new_action;


int add_task_to_queue(Task *t){
	#ifdef DEBUG_TM
	printf("Adding task %ld to the task queue...\n", t->id);
	#endif
	char msg[MSG_LEN];
	if(queue_size >= queue_pos){
		sprintf(msg, "TASK %ld DELETED (THE TASK QUEUE IS FULL)", t->id);
		log_write(msg);
		shm_w_lock();
		set_n_not_executed_tasks(get_n_not_executed_tasks() + 1);
		shm_w_unlock();	
		return -1;
	}
	queue[queue_size++] = *t;
	shm_w_lock();
	#ifdef DEBUG_TM
	printf("Setting task queue percentage to %d\n", (int)(queue_size*100.0/queue_pos));
	#endif
	set_tm_percentage((int)(queue_size*100.0/queue_pos));
	shm_w_unlock();
	
	//notify monitor about the queue percentage change
	pthread_mutex_lock(monitor_mutex);
	pthread_cond_signal(monitor_cond);
	pthread_mutex_unlock(monitor_mutex);
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
			shm_w_lock();
			#ifdef DEBUG_TM
			printf("Setting task queue percentage to %d\n", (int)(queue_size*100.0/queue_pos));
			#endif
			set_tm_percentage((int)(queue_size*100.0/queue_pos));
			shm_w_unlock();
			
			//notify monitor about the queue percentage change
			pthread_mutex_lock(monitor_mutex);
			pthread_cond_signal(monitor_cond);
			pthread_mutex_unlock(monitor_mutex);
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
	double current_time;
	
	//block all signals in this thread
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	while(1){
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	
		pthread_mutex_lock(&queue_mutex);
		//wait for new task to arrive
		while(!scheduler_start){
			//check if the simulator is going to end
			if(tm_leave_flag == 1){
				pthread_mutex_unlock(&queue_mutex);
				#ifdef DEBUG_TM
				printf("Scheduler leaving...\n");
				#endif
				pthread_exit(NULL);
			}
		
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
		
		//signal dispatcher if it is waiting 
		pthread_mutex_lock(dispatcher_mutex);
		pthread_cond_signal(dispatcher_cond);
		pthread_mutex_unlock(dispatcher_mutex);
		
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}
	pthread_exit(NULL);
}

int server_updated(double old_available_time, int es_n, int vcpu_n) {
	//check if the server updated its vcpu next available time
	shm_r_lock();
	double new_available_time = get_edge_server(es_n).vcpu[vcpu_n].next_available_time;
	shm_r_unlock();
	return new_available_time != old_available_time;
}


int enough_time_left(VCPU *v, Task *t, double current_time){
	double task_execution_time = t->thousand_inst/1000.0/v->processing_capacity;
	return current_time > v->next_available_time && current_time + task_execution_time < t->arrival_time + t->max_exec_time;
}


int get_free_edge_server(Task *t, int *free_vcpu){
	int i;
	double ct = get_current_time();		
	//Check which servers have free vcpus
	for(i = 0; i < edge_server_number; i++){
		#ifdef DEBUG_TM
		printf("Checking edge server %d\n", i+1);
		#endif
		shm_r_lock();
		EdgeServer es = get_edge_server(i+1);
		if(es.performance_level == 0){
			shm_r_unlock();
			continue;
		}
		 
        if(es.performance_level > 0 && enough_time_left(&es.vcpu[0], t, ct)){
			#ifdef DEBUG_TM
			printf("Dispatcher: Selecting Edge Server %s VCPU: 0\n", es.name);
			#endif
			shm_r_unlock();
			*free_vcpu = 0;
			return i+1;
		}
		if(es.performance_level == 2 && enough_time_left(&es.vcpu[1], t, ct)){
			#ifdef DEBUG_TM
			printf("Dispatcher: Selecting Edge Server %s VCPU: 1\n", es.name);
			#endif
			shm_r_unlock();
			*free_vcpu = 1;
			return i+1;
		}
		shm_r_unlock();
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
		shm_r_lock();
		EdgeServer es = get_edge_server(i+1);
		if(es.performance_level == 0){
		 shm_r_unlock();
		 continue;
        }
        if((es.performance_level > 0 && es.vcpu[0].next_available_time < ct) || (es.performance_level == 2 && es.vcpu[1].next_available_time < ct)){
			#ifdef DEBUG_TM
			printf("Dispatcher: There are free edge servers (%s)\n", es.name);
			#endif
			shm_r_unlock();
			return 1;
		}
		shm_r_unlock();
	}
	return 0;
}

void* dispatcher(){
	int i, min_priority, es;
	char msg[MSG_LEN], * es_name;
	VCPUTask t;
	double task_arrival_time, task_wait_time;
	
	//block all signals in this thread
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	
	while(1){
		min_priority = 0;
		
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		
		pthread_mutex_lock(dispatcher_mutex);
		pthread_mutex_lock(&queue_mutex);
		
		//Check if there are tasks in the task queue and free vcpus
		//If not, wait
		while(queue_size == 0 || !check_free_edge_servers()){
			//check if the simulator is going to end
			if(tm_leave_flag == 1){
				pthread_mutex_unlock(&queue_mutex);
				pthread_mutex_unlock(dispatcher_mutex);
				#ifdef DEBUG_TM
				printf("Dispatcher leaving...\n");
				#endif
				pthread_exit(NULL);
			}
			
			pthread_mutex_unlock(&queue_mutex);
			#ifdef DEBUG_TM
			printf("Dispatcher waiting for signal\n");
			#endif
			pthread_cond_wait(dispatcher_cond, dispatcher_mutex);
			pthread_mutex_lock(&queue_mutex);
			#ifdef DEBUG_TM
			printf("Dispatcher received signal\n");
			#endif
		}
		es = 0;
		int free_vcpu = -1;
		while(queue_size > 0){
			//get the task with the most priority (the higher the priority, the lower the value)
			for(i = 0; i < queue_size; i++){
				if(queue[i].priority < queue[min_priority].priority)
					min_priority = i;
			}
			
			//choose the vcpu to execute the task
			if((es = get_free_edge_server(&queue[min_priority], &free_vcpu))){
				break;
			}
			
			//no vcpu found with enough processing capacity needed to execute the task in the time left
			shm_w_lock();
			set_n_not_executed_tasks(get_n_not_executed_tasks() + 1);
			shm_w_unlock();		
			sprintf(msg, "DISPATCHER: TASK %ld REMOVED FROM QUEUE (NOT ENOUGH TIME LEFT TO EXECUTE)", queue[min_priority].id);
			log_write(msg);
			remove_task_from_queue(&queue[min_priority]);
			
		}
		if(!es || free_vcpu == -1){
			//no edge server found with vcpus with enough processing capacity needed to execute the task in the time left
			#ifdef DEBUG_TM
			printf("Dispatcher: No task selected\n");
			#endif
			pthread_mutex_unlock(&queue_mutex);
			pthread_mutex_unlock(dispatcher_mutex);	
			continue;
		}
		
		//get the information about the selected task to send to the chosen vcpu
		task_arrival_time = queue[min_priority].arrival_time;
		t.id = queue[min_priority].id;
		t.done = 0;
		t.thousand_inst = queue[min_priority].thousand_inst;
		t.vcpu = free_vcpu;
		remove_task_from_queue(&queue[min_priority]);
		
		//get the name of the server where the task will be executed
		shm_r_lock();
		EdgeServer es_task = get_edge_server(es);
		es_name = es_task.name;
		double es_old_available_time = es_task.vcpu[free_vcpu].next_available_time;
		shm_r_unlock();
		
		sprintf(msg, "DISPATCHER: TASK %d SELECTED FOR EXECUTION ON %s", t.id, es_name);
		log_write(msg);
		
		//calculate the time that the task had to wait before being sent
		task_wait_time = get_current_time() - task_arrival_time;
		#ifdef DEBUG_TM
		printf("Setting waiting time to %d seconds (rounded up)\n", (int)(task_wait_time) + 1);
		#endif
		shm_w_lock();
		set_min_wait_time((int)(task_wait_time) + 1);
		int total_tasks = get_n_executed_tasks();
		set_avg_res_time((get_avg_res_time()*total_tasks + task_wait_time)/ (total_tasks+1));
		shm_w_unlock();
		
		//notify monitor that changes were made
		pthread_mutex_lock(monitor_mutex);
		pthread_cond_signal(monitor_cond);
		pthread_mutex_unlock(monitor_mutex);
		
		//send task
		write(unnamed_pipe[es-1][1], &t, sizeof(VCPUTask));
		
		pthread_mutex_unlock(&queue_mutex);
		
		//wait for the edge server to update its status before continuing
		while(!server_updated(es_old_available_time, es, free_vcpu)){
			pthread_cond_wait(dispatcher_cond, dispatcher_mutex);
		}
		
		pthread_mutex_unlock(dispatcher_mutex);	
		
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	}
	pthread_exit(NULL);
}

void *read_from_task_pipe(){
	int read_len;
	Task t;
	char msg[MSG_LEN], msg_temp[MSG_LEN*2];
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	while(1){
		//read message from the named pipe
		if((read_len = read(task_pipe_fd, msg, MSG_LEN)) > 0){
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			msg[read_len] = '\0';
			#ifdef DEBUG_TM
			printf("%s read from task pipe\n", msg);
			#endif
			//check if the message is a new task or a command
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
			}else if(read_len == 6 && strncmp(msg, "STATS", 5) == 0){
				log_write("STATISTICS:");
				print_stats();
			}else if(read_len == 5 && strncmp(msg, "EXIT", 4) == 0){
				log_write("TASK MANAGER: EXIT RECEIVED FROM TASK PIPE");
				kill(getppid(), SIGINT);
				break;
			}else{
				sprintf(msg_temp, "WRONG COMMAND => %s", msg);
				log_write(msg_temp);
			}
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		}else{
			log_write("ERROR READING FROM TASK PIPE");
		}
	}
	pthread_exit(NULL);
}

void clean_tm_resources(){
	int i;
	//clean unnamed pipe resources
	for(i = 0; i < edge_server_number; i++){
		close(unnamed_pipe[i][1]);
		free(unnamed_pipe[i]);
	}
	free(unnamed_pipe);
	
	//clean other resources
	free(edge_servers_pid);
	free(queue);
	pthread_mutex_destroy(&queue_mutex);
	pthread_cond_destroy(&scheduler_cond);
	pthread_cond_destroy(dispatcher_cond);
	pthread_mutex_destroy(dispatcher_mutex);
	pthread_mutexattr_destroy(&mutexattr); 
	pthread_condattr_destroy(&condattr);
	close(task_pipe_fd);
}

void tm_termination_handler(int signum) {
    if(signum == SIGUSR1){
    	int i;
    	char msg[MSG_LEN];
    	
    	log_write("WAITING FOR THE LAST TASKS TO FINISH");

    	
    	//the simulator is going to end
 
    	tm_leave_flag = 1;
    	pthread_cancel(read_thread);
    	pthread_join(read_thread, NULL);
    	
    	//notify scheduler and dispatcher if they are waiting
    	pthread_mutex_lock(&queue_mutex);
    	pthread_cond_broadcast(&scheduler_cond);
    	pthread_mutex_unlock(&queue_mutex);
    	
    	pthread_mutex_lock(dispatcher_mutex);
    	pthread_cond_broadcast(dispatcher_cond);
    	pthread_mutex_unlock(dispatcher_mutex);
    	
    	pthread_cancel(scheduler_thread);
		pthread_cancel(dispatcher_thread);
    	pthread_join(scheduler_thread, NULL);
		pthread_join(dispatcher_thread, NULL);
    	
    	//signal edge servers
    	for(i = 0; i < edge_server_number; i++){
    		#ifdef DEBUG_TM
    		printf("TM killing %d\n", i+1);
    		#endif
    		kill(edge_servers_pid[i], SIGUSR1);
    	}
    	
    	for(i = 0; i < edge_server_number; i++) {
    		wait(NULL);
    	}
    	
    	for(i = 0; i < queue_size; i++){
    		shm_w_lock();
			set_n_not_executed_tasks(get_n_not_executed_tasks() + 1);
			shm_w_unlock();	
    		sprintf(msg, "TASK %ld NOT EXECUTED (SIMULATOR CLOSING)", queue[i].id);
    		log_write(msg);
    	}
    	print_stats();
        clean_tm_resources();
        #ifdef DEBUG_TM
        printf("TM DIED\n");
        #endif
        exit(0);
    }
}

int task_manager(){
	int i;
    
    edge_servers_pid = (pid_t*) malloc(edge_server_number * sizeof(pid_t));
	
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
		if((edge_servers_pid[i] = fork()) == 0){
			close(unnamed_pipe[i][1]);
			edge_server(i+1);
			exit(0);
		}
		close(unnamed_pipe[i][0]);
	}
	
	queue_size = 0;
	
	#ifdef DEBUG_TM
	printf("Creating scheduler and dispatcher thread...\n");
	#endif
	//opens the pipe in read-write mode for the function read
	//to block while waiting for new tasks to arrive
	if ((task_pipe_fd = open(PIPE_NAME, O_RDWR)) < 0) {
		log_write("ERROR OPENING PIPE FOR READING");
		return -1;
	}
	
	//define a handler for SIGUSR1
    tm_new_action.sa_flags = SA_RESTART;
    tm_new_action.sa_mask = block_set;
    tm_new_action.sa_handler = &tm_termination_handler;
    sigaction(SIGUSR1,&tm_new_action,NULL);
    
    //ignore SIGINT and SIGTSTP (these are handled by the system manager)
    tm_new_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &tm_new_action, NULL);
    sigaction(SIGTSTP, &tm_new_action, NULL);
    
	//create scheduler thread
	pthread_create(&scheduler_thread, NULL, scheduler, NULL);
	//create dispatcher thread
	pthread_create(&dispatcher_thread, NULL, dispatcher, NULL);
	//create the thread that will read from the task pipe
	pthread_create(&read_thread, NULL, read_from_task_pipe, NULL);
	
	sigprocmask(SIG_UNBLOCK, &block_set, NULL);
	
	sigprocmask(SIG_BLOCK, &block_set_no_sigusr1, NULL);
	
	pthread_join(read_thread, NULL);
	pthread_join(scheduler_thread, NULL);
	pthread_join(dispatcher_thread, NULL);
	
	clean_tm_resources();
	return 0;
}
