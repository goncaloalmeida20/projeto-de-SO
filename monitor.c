/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <stdio.h>
#include "monitor.h"
#include "shared_memory.h"


void monitor(){
    int tm_percentage, min_wait_time;

    while(1){
        shm_lock();
        tm_percentage = get_tm_percentage();
        min_wait_time = get_min_wait_time();
        shm_unlock();

        if (tm_percentage > 80 && min_wait_time > max_wait){
            shm_lock();
            set_performance_change_flag(2);
            shm_unlock();
        } else if(tm_percentage < 20){
            shm_lock();
            set_performance_change_flag(1);
            shm_unlock();
        }
    }
}