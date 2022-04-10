#include <semaphore.h>

typedef struct{

}SharedMemory;

sem_t* shm_mutex;
SharedMemory* shared_var;
int shmid;

int create_shm();
void close_shm();

