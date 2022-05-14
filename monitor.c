/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "monitor.h"
#include "log.h"
#include "shared_memory.h"

//sigset_t mon_block_set;
int ch_perf, old_perf, mon_leave_flag = 0;
char msg[MSG_LEN];
pthread_t mon_thread;
struct sigaction mon_new_action;

int change_performance(){
	int tm_percentage, min_wait_time;
	shm_r_lock();
    tm_percentage = get_tm_percentage();
    min_wait_time = get_min_wait_time();
    
    if(tm_percentage > 80 && min_wait_time > max_wait){
    	shm_r_unlock();
    	return 2;
    }
    if(tm_percentage < 20){
    	shm_r_unlock();
    	return 1;
    }
    shm_r_unlock();
    return 0;

}

void *monitor_thread(){
	pthread_sigmask(SIG_BLOCK, &block_set, NULL);
	while(1){
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    	pthread_mutex_lock(monitor_mutex);
    	while(!(ch_perf = change_performance()) || ch_perf == old_perf || mon_leave_flag){
    		if(mon_leave_flag){
    			pthread_mutex_unlock(monitor_mutex);
    			#ifdef DEBUG_MON
    			printf("Monitor leaving...\n");
    			#endif
    			pthread_exit(NULL);
    		}
        	pthread_cond_wait(monitor_cond, monitor_mutex);
        }
        pthread_mutex_unlock(monitor_mutex);
        
        shm_w_lock();
        set_performance_change_flag(ch_perf);
        shm_w_unlock();
        sprintf(msg, "MONITOR: CHANGED PERFORMANCE FLAG TO %d", ch_perf);
        log_write(msg);
        
        pthread_mutex_lock(performance_changed_mutex);
        pthread_cond_broadcast(performance_changed_cond);
        pthread_mutex_unlock(performance_changed_mutex);
        
        old_perf = ch_perf;
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
	
	pthread_exit(NULL);
}

void mon_termination_handler(int signum) {
    if(signum == SIGUSR1){ // handling of SIGUSR1
    	#ifdef DEBUG_MON
    	printf("Monitor: sigusr1\n");
    	#endif
    	//notify the monitor thread if it is waiting
    	mon_leave_flag = 1;
    	pthread_mutex_lock(monitor_mutex);
    	pthread_cond_broadcast(monitor_cond);
    	pthread_mutex_unlock(monitor_mutex);
    	pthread_cancel(mon_thread);
    	pthread_join(mon_thread, NULL);
        exit(0);
    }
    //unexpected signal received
    char log[MSG_LEN];
    sprintf(log, "MONITOR RECEIVED SIGNAL %d", signum);
    log_write(log);
}

void monitor(){
	//sigfillset(&mon_block_set); // will have all possible signals blocked when our handler is called

    //define a handler for SIGINT; when entered all possible signals are blocked
    mon_new_action.sa_flags = 0;
    mon_new_action.sa_mask = block_set;
    mon_new_action.sa_handler = &mon_termination_handler;

    sigaction(SIGUSR1,&mon_new_action,NULL);
    
    mon_new_action.sa_handler = SIG_IGN;
    
    sigaction(SIGINT, &mon_new_action, NULL);
    sigaction(SIGTSTP, &mon_new_action, NULL);
	
	shm_w_lock();
	set_performance_change_flag(1);
	shm_w_unlock();
	
	old_perf = 1;
	
	sigprocmask(SIG_UNBLOCK, &block_set, NULL);
		
	pthread_create(&mon_thread, NULL, monitor_thread, NULL);

	pthread_join(mon_thread, NULL);
}
