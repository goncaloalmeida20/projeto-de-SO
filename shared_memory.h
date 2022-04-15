typedef struct {
    char name[1024];
    int processing_capacity_min, processing_capacity_max;
    int performance_level;
}EdgeServer;

int edge_server_number;

int create_shm();
void close_shm();

