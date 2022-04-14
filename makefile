offload_simulator: system_manager.o shared_memory.o
	gcc -o offload_simulator system_manager.o shared_memory.o

system_manager.o: system_manager.c shared_memory.h
	gcc -c system_manager.c

shared_memory.o: shared_memory.c shared_memory.h
	gcc -c shared_memory.c

mobile_node: mobile_node.c
	gcc -o mobile_node mobile_node.c

log: log.c log.h
	gcc -o log log.c



