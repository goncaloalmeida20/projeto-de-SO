#include <stdio.h>

void maintenance_manager(int mqid) {
    Message msg;

    while (1)
    {
        /* Waits for a message with its id (=PONG) */
        msgrcv(mqid, &msg, sizeof(Message), PONG, 0);
    }
}
