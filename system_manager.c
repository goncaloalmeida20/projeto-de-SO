#include <stdio.h>
#include <stdlib.h>

typedef struct _config_data{
    int queue_pos, max_wait, edge_server_number;
    edge_server *edge_servers;
}config_data;

typedef struct _edge_server{
    char name[1024];
    int processing_capacity_min, processing_capacity_max;
    edge_server * next;
}edge_server;

config_data read_file(FILE *fp){
    long pos;
    int i = 0;
    config_data *configData = (config_data *) malloc(sizeof(config_data));
    edge_server *edgeServer = (edge_server *) malloc(sizeof(edge_server));

    if (fp != NULL){
        fscanf_s(fp,"%d", &configData->queue_pos);
        pos = ftell(fp);
        fseek(fp, pos, SEEK_SET);

        fscanf_s(fp,"%d", &configData->max_wait);
        fflush(fp);
        pos = ftell(fp);
        fseek(fp, pos, SEEK_SET);

        fscanf_s(fp,"%d", &configData->edge_server_number);
        fflush(fp);
        pos = ftell(fp);
        fseek(fp, pos, SEEK_SET);

        if(configData->edge_server_number >= 2){
            for(; i < configData->edge_server_number; i++){
                fscanf_s(fp,"%s,%d,%d", &edgeServer->name, &edgeServer->processing_capacity_min, &edgeServer->processing_capacity_max);
                fflush(fp);
                pos = ftell(fp);
                fseek(fp, pos, SEEK_SET);
                edgeServer = edgeServer->next;
            }
        }
        else{
            printf("Edge server number needs to be higher than 1\n");
            exit(1);
        }

    } else{
        printf("Error in config file\n");
        exit(1);
    }
}

int main(int argc, char *argv[]){
    int i = 0;
    if(argc != 2){
        printf("Wrong number of parameters\n");
        exit(1);
    }

    FILE *fp = fopen(argv[1], "r");
    read_file(fp);


    exit(0);
}
