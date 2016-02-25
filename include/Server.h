#ifndef SERVER_H
#define SERVER_H

#include <netdb.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <global.h>
#include <../include/ClientInfo.h>
#include "command.h"

class Server {
public:
    Server(const char *port);
    ~Server();

    void start();

private:
    const char *port;
    int server_sockfd, max_fd;
    struct addrinfo server_addrinfo;
    fd_set read_fd, all_fd;
    std::map<client_key, client_info_ptype> clients;

    void do_bind_listen();

    void select_loop();

    void new_client(int fd, sockaddr_storage addr, socklen_t len);

    void cli_command(std::string command_str);

    void client_command(std::string command, const client_key key,  ClientInfo *client);

    std::vector<ClientInfo> sorted_clients() const;

    void log_relay(ClientInfo *client);

    bool send_client_command(client_info_ptype client,
                             client_server::command command,
                             std::string argv1 = "",
                             std::string argv2 = "");
};


#endif //SERVER_H
