#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <map>
#include "ClientInfo.h"

namespace server {
    enum cli_command {
        AUTHOR,
        IP,
        PORT,
        LIST,
        STATISTICS,
        BLOCKED,
        UNKNOWN
    };

    cli_command parse_cli_command(const std::string command_str);
}

namespace client {
    enum cli_command {
        AUTHOR,
        IP,
        PORT,
        LIST,
        LOGIN,
        REFRESH,
        SEND,
        BROADCAST,
        BLOCK,
        UNBLOCK,
        LOGOUT,
        EXIT,
        SENDFILE,
        UNKNOWN
    };
    cli_command parse_cli_command(const std::string command_str);
}

namespace client_server {
    enum command {
        LOGIN,
        REFRESH,
        SEND,
        BROADCAST,
        BLOCK,
        UNBLOCK,
        LOGOUT,
        EXIT,
        UNKNOWN
    };
    command parse_command(const std::string command_str);
    std::string command_str(const command c);
}

void author(std::string command);
void ip(std::string command);
void port_command(std::string command, const char* port);
void list_command(std::string command, std::map<client_key, client_info_ptype> clients);
void relay(std::string from, std::string to, std::string msg);
void received(std::string from, std::string msg);

#endif //COMMAND_H
