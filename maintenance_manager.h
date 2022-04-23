#ifndef MAINTENANCE_MANAGER_H
#define MAINTENANCE_MANAGER_H

#define MSG_LEN 100
#define NAME_LEN 50

typedef struct
{
    char msg_type[NAME_LEN];
    char msg_text[MSG_LEN];
} Message;

void maintenance_manager();

#endif //MAINTENANCE_MANAGER_H
