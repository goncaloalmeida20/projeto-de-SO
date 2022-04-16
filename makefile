all: mobile_node offload_simulator
	
offload_simulator: system_manager.o shared_memory.o log.o task_manager.o edge_server.o
	gcc -Wall -pthread system_manager.o shared_memory.o log.o task_manager.o edge_server.o -pthread -o offload_simulator

system_manager.o: system_manager.c shared_memory.h log.h
	gcc -Wall -c system_manager.c

shared_memory.o: shared_memory.c shared_memory.h log.h
	gcc -Wall -c shared_memory.c

task_manager.o: task_manager.c task_manager.h shared_memory.h log.h
	gcc -Wall -c task_manager.c

edge_server.o: edge_server.c edge_server.h shared_memory.h log.h
	gcc -Wall -c edge_server.c

log.o: log.c log.h
	gcc -Wall -c log.c

mobile_node: mobile_node.c
	gcc -Wall -o mobile_node mobile_node.c

