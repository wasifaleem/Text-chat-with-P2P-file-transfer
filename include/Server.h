#ifndef SERVER_H
#define SERVER_H

#include <netdb.h>
#include <map>

class Server {
public:
    Server(const char *port);

    void start();

private:
    struct client_info {
        int sockfd;
    };

    const char *port;
    int server_sockfd, max_fd;
    struct addrinfo server_addrinfo;
    fd_set read_fd, all_fd;
    std::map<std::string, client_info> clients;

    void connect();

    void selectLoop();

    void newClient(int fd, sockaddr_storage addr, socklen_t len);

    const std::string get_ip(sockaddr_storage &addr);
};


#endif //SERVER_H
