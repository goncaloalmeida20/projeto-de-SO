offload_simulator: system_manager.o shared_memory.o log.o task_manager.o
	gcc system_manager.o shared_memory.o log.o task_manager.o -pthread -o offload_simulator

system_manager.o: system_manager.c shared_memory.h log.h
	gcc -c -pthread system_manager.c

shared_memory.o: shared_memory.c shared_memory.h log.h
	gcc -c shared_memory.c

task_manager.o: task_manager.c task_manager.h log.h
	gcc -c task_manager.c

log.o: log.c log.h
	gcc -c log.c

mobile_node: mobile_node.c
	gcc -o mobile_node mobile_node.c



