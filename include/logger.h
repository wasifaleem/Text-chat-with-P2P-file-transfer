#ifndef LOGGER_H_
#define LOGGER_H_

#define FILEPATH_LEN 256

extern char LOGFILE[FILEPATH_LEN];

extern int ret_print, ret_log;

void cse4589_init_log(char* port);
void cse4589_print_and_log(const char* format, ...);

#define SUCCESS(command) cse4589_print_and_log("[%s:SUCCESS]\n", command.c_str());
#define ERROR(command) cse4589_print_and_log("[%s:ERROR]\n", command.c_str());
#define END(command) cse4589_print_and_log("[%s:END]\n", command.c_str());

#endif
