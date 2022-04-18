#ifndef LOG_H
#define LOG_H

#define MSG_LEN 100

int create_log();
void close_log();
int log_write(char *s);

#endif
