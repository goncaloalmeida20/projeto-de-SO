/*
    Realizado por:
    João Bernardo de Jesus Santos, nº2020218995
    Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef MAINTENANCE_MANAGER_H
#define MAINTENANCE_MANAGER_H

#define MSG_LEN 256
#define NAME_LEN 50

#define ES_TYPE 1
#define MM_TYPE 2

typedef struct
{
    long msg_type;
    char sender_name[NAME_LEN];
    char msg_text[MSG_LEN];
} Message;

void maintenance_manager();

#endif //MAINTENANCE_MANAGER_H
