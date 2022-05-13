/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "edge_server.h"

#define PIPE_NAME "TASK_PIPE"
#define DEBUG_TM //uncomment this line to print task manager debug messages

int queue_pos;

int task_manager();

#endif
