#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#define NAME_LEN 50

typedef struct {
    char name[NAME_LEN];
    int processing_capacity_min, processing_capacity_max;
    int performance_level;
}edgeServer;

int edge_server_number;

int create_shm();
void close_shm();
void shm_lock();
void shm_unlock();
edgeServer get_edge_server(int n);
void set_edge_server(edgeServer* es, int n);
int get_performance_change_flag();
void set_performance_change_flag(int pcf);


#endif
