#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <map>
#include <netdb.h>
#include "ClientInfo.h"
#include "command.h"

class Client {
public:
    Client(const char *port);
    void start();

private:

    const char *port;
    bool logged_in;
    std::map<client_key, client_info_ptype> p2p_clients;
    std::map<client_key, client_info_ptype> clients_from_server;

    int p2p_listen_sockfd, server_sockfd, max_fd;
    struct addrinfo p2p_listen_addrinfo, server_addrinfo;
    fd_set read_fd, all_fd;

    void bindListen();
    void selectLoop();
    bool do_login(std::string ip, std::string port);
    void newClient(int fd, sockaddr_storage addr, socklen_t len);
    void cli_command(std::string command_str);
    void server_command(std::string command_str);

    bool send_server_command(client_server::command c, std::string arg1 = "", std::string arg2 = "") const;

    void log_recieved(std::string string);
};


#endif //CLIENT_H
