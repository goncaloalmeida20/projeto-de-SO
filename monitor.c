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

sigset_t mon_block_set;
struct sigaction mon_new_action;

int change_performance(){
	int tm_percentage, min_wait_time;
	shm_lock();
    tm_percentage = get_tm_percentage();
    min_wait_time = get_min_wait_time();
    
    if(tm_percentage > 80 && min_wait_time > max_wait){
    	shm_unlock();
    	return 2;
    }
    if(tm_percentage < 20){
    	shm_unlock();
    	return 1;
    }
    shm_unlock();
    return 0;

}

void mon_termination_handler(int signum) {
    if(signum == SIGINT){ // handling of CTRL-C
    	printf("Monitor: sigint\n");
        exit(0);
    }
}

void monitor(){
	int ch_perf, old_perf = 1;
	char msg[MSG_LEN];
	
	sigfillset(&mon_block_set); // will have all possible signals blocked when our handler is called

    //define a handler for SIGINT; when entered all possible signals are blocked
    mon_new_action.sa_flags = 0;
    mon_new_action.sa_mask = mon_block_set;
    mon_new_action.sa_handler = &mon_termination_handler;

    sigaction(SIGINT,&mon_new_action,NULL);
	
	shm_lock();
	set_performance_change_flag(1);
	shm_unlock();
    while(1){
    	pthread_mutex_lock(monitor_mutex);
    	while(!(ch_perf = change_performance()) || ch_perf == old_perf){
        	pthread_cond_wait(monitor_cond, monitor_mutex);
        	printf("JAISDJAIODJA\n");
        }
        pthread_mutex_unlock(monitor_mutex);
        
        shm_lock();
        set_performance_change_flag(ch_perf);
        shm_unlock();
        sprintf(msg, "MONITOR: CHANGED PERFORMANCE FLAG TO %d", ch_perf);
        log_write(msg);
        
        pthread_mutex_lock(performance_changed_mutex);
        pthread_cond_broadcast(performance_changed_cond);
        pthread_mutex_unlock(performance_changed_mutex);
        
        old_perf = ch_perf;
        sleep(1);
    }
}
