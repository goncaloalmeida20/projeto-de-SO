/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#define PIPE_NAME "named_pipe"
//#define DEBUG_TM //uncomment this line to print task manager debug messages
//#define TEST_TM //uncomment this line to test task manager with pre-made tasks
//#define BREAK_TM //uncomment this line for the task manager not to wait for tasks and end
int queue_pos, max_wait, fd_named_pipe;

int task_manager();

#endif
