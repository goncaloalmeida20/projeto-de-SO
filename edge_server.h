/*
Realizado por:
João Bernardo de Jesus Santos, nº2020218995
Gonçalo Fernandes Diogo de Almeida, nº2020218868
*/

#ifndef EDGE_SERVER_H
#define EDGE_SERVER_H

typedef struct{
	int id, done, thousand_inst;
}VCPUTask;

int mqid;
int **unnamed_pipe;

double get_current_time();
int edge_server(int es_n);

#endif
