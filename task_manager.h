#include <pthread.h>
#include <semaphore.h>

typedef struct{
	int id;
	int thousand_inst;
	double max_exec_time;
	double arrival_time;
	int priority;
}Task;

int queue_size;
Task *queue;
pthread_t scheduler_thread;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t scheduler_signal = PTHREAD_COND_INITIALIZER;

int task_manager(const int QUEUE_POS);
