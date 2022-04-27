/*
    Realizado por:
    João Bernardo de Jesus Santos, nº2020218995
    Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include "maintenance_manager.h"

pthread_t mm_thread;
int interval_of_mm, time_bw_mm, mqid, edge_server_number, num_es_in_maintenance = 0;

void clean_mm_resources(){
    pthread_join(mm_thread, NULL);
}

void * maintenance(void *t){
    Message msg;

    // Maintenance of the Edge Servers
    while(1){
        if(num_es_in_maintenance < edge_server_number) {
            msg.msg_type = 2;
            strcpy(msg.sender_name, "MM");
            strcpy(msg.msg_text, "START");
            msgsnd(mqid, &msg, sizeof(Message), 0);
            msgrcv(mqid, &msg, sizeof(Message), ES_TYPE, 0);
            num_es_in_maintenance++;
            sleep(interval_of_mm);
            msg.msg_type = 2;
            strcpy(msg.sender_name, "MM");
            strcpy(msg.msg_text, "END");
            msgsnd(mqid, &msg, sizeof(Message), 0);
            sleep(time_bw_mm);
            num_es_in_maintenance--;
        }
    }
    pthread_exit(NULL);
}

void maintenance_manager(int mq_id, int es_num) {
    Message msg;
    char es_names[es_num][NAME_LEN];
    int n = 0;
    time_bw_mm = rand() % 5 + 1;
    interval_of_mm = rand() % 5 + 1;
    mqid = mq_id;
    edge_server_number = es_num;

    // The Maintenance Manager is informed of the creation of the Edge Servers
    while (n < edge_server_number)
    {
        // Waits for a message with its id (=ES_TYPE)
        msgrcv(mqid, &msg, sizeof(Message), ES_TYPE, 0);
        strcpy(es_names[n++], msg.sender_name);
    }

    pthread_create(&mm_thread, NULL, maintenance, NULL);
}
