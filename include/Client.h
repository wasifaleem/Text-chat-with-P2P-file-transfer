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
    virtual ~Client();

    void start();

private:
    struct p2p_client {
        int sock_fd;
        unsigned long received_size;
        char * file_data;
    };
    typedef p2p_client *p2p_client_ptype;
    const char *port;
    bool logged_in;
    client_key me;
    std::map<client_key, p2p_client_ptype> p2p_clients;
    std::map<client_key, client_info_ptype> clients_from_server;

    int p2p_listen_sockfd, server_sockfd, max_fd;
    struct addrinfo p2p_listen_addrinfo, server_addrinfo;
    fd_set read_fd, all_fd;

    void bindListen();

    void selectLoop();

    bool do_login(std::string ip, std::string port);

    void new_p2p_file_client(int fd, sockaddr_storage addr, socklen_t len);

    void cli_command(std::string command_str);

    void server_command(std::string command_str);

    bool send_server_command(client_server::command c, std::string arg1 = "", std::string arg2 = "") const;

    void log_received();

    const client_info_ptype find_me();

    bool send_file(client_info_ptype const to_client, std::string file_name);

    void save_received_file(const client_key key, p2p_client_ptype client);
};


#endif //CLIENT_H
