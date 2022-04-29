/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define NAME_LEN 50

typedef struct {
    char name[NAME_LEN];
    double next_available_time_min, next_available_time_max;
    int processing_capacity_min, processing_capacity_max, performance_level;
    int task_exec, op_main; // Number of tasks executed and number of maintenance operations
}EdgeServer;

int edge_server_number;

int create_shm();
void close_shm();
void shm_lock();
void shm_unlock();
EdgeServer get_edge_server(int n);
void set_edge_server(EdgeServer* es, int n);
int get_performance_change_flag();
void set_performance_change_flag(int pcf);
void print_stats();


#endif
