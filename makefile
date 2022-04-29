CC = gcc
FLAGS = -Wall
LIBS = -pthread
LIBS2 = -lm
LOGFILE = log.txt
OBJS1 = system_manager.o shared_memory.o log.o task_manager.o edge_server.o maintenance_manager.o monitor.o
OBJS2 = mobile_node.o
PROG1 = offload_simulator
PROG2 = mobile_node

# GENERIC

all: ${PROG1} ${PROG2}

clean:
		rm ${OBJS1} ${OBJS2} ${PROG1} ${PROG2}

clean_with_log:
		rm ${OBJS1} ${OBJS2} ${PROG1} ${PROG2} ${LOGFILE}
		
${PROG1}: ${OBJS1}
		${CC} ${FLAGS} ${LIBS} ${OBJS1} -o $@

${PROG2}: ${OBJS2}
		${CC} ${FLAGS} ${OBJS2} -o $@ ${LIBS2} 

.c.o:
		${CC} ${FLAGS} $< -c
	
	
###############################################

system_manager.o: system_manager.c shared_memory.h log.h

shared_memory.o: shared_memory.c shared_memory.h log.h

task_manager.o: task_manager.c task_manager.h shared_memory.h log.h

edge_server.o: edge_server.c edge_server.h shared_memory.h log.h

maintenance_manager.o: maintenance_manager.c maintenance_manager.h log.h

monitor.o: monitor.c monitor.h log.h

log.o: log.c log.h

mobile_node.o: mobile_node.c

