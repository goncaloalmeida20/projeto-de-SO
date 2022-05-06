/*
    Realizado por:
        João Bernardo de Jesus Santos, nº2020218995
        Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define NAME_LEN 50

typedef struct {
    double next_available_time;
    int processing_capacity;
} VCPU;

typedef struct {
    char name[NAME_LEN];
    int performance_level, task_exec, op_main; // Performance level, number of tasks executed and number of maintenance operations
    VCPU min, max;
}EdgeServer;

int edge_server_number, max_wait;
pthread_mutexattr_t attrmutex;
pthread_condattr_t attrcondv;
pthread_mutex_t monitor_mutex;
pthread_cond_t monitor_cond;

int create_shm();
void close_shm();
void shm_lock();
void shm_unlock();
EdgeServer get_edge_server(int n);
void set_edge_server(EdgeServer* es, int n);
int get_performance_change_flag();
void set_performance_change_flag(int pcf);
int get_tm_percentage();
void set_tm_percentage(int p);
int get_min_wait_time();
void set_min_wait_time(int t);
pthread_mutex_t* get_dispatcher_mutex();
pthread_cond_t* get_dispatcher_cond();
void print_stats();


#endif
