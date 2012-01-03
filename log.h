#ifndef LOG_H
#define LOG_H

struct recv_data; // defined in main.h

void log_init();
void log_message(struct recv_data *in);
void log_terminate();

#endif//LOG_H
