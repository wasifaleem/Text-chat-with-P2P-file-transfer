#ifndef GLOBAL_H_
#define GLOBAL_H_

#define HOSTNAME_LEN 128
#define PATH_LEN 256

#define SERVER_BACKLOG 10
#define SERVER_MSG_LEN 10000
#define MSG_LEN 256
#define COMMAND_SEPARATOR ' '
#define SPACE ' '
#define UBIT_NAME "wasifale"
#define EMPTY_STRING ""


#endif

#ifdef DEBUG
#define DEBUG_MSG(str) do { std::cout << str << std::endl; } while( false )
#else
#define DEBUG_MSG(str) do { } while ( false )
#endif