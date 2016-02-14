#ifndef SERVER_H
#define SERVER_H

#include <netdb.h>
#include <string>
#include <map>
#include <vector>
#include <global.h>

class Server {
public:
    Server(const char *port);
    ~Server();
    void start();

private:
    enum client_status {
        ONLINE, OFFLINE
    };

    class client_info {
    public:
        int sockfd;
        std::string ip, host, service;
        int receive_count, sent_count;
        client_status status;

        bool operator<(const client_info &c) const {
            return service.compare(c.service) < 0;
        }
    };

    typedef client_info *client_info_type;

    const char *port;
    int server_sockfd, max_fd;
    struct addrinfo server_addrinfo;
    fd_set read_fd, all_fd;
    std::map<std::string, client_info_type> clients;

    void bindListen();

    void selectLoop();

    void newClient(int fd, sockaddr_storage addr, socklen_t len);

    const std::string primary_ip();

    const std::string get_ip(sockaddr_storage addr) const;

    std::vector<std::string> split_by_space(const std::string s) const;

    void command(std::string command_str, Server::client_info *cinfo);

    std::vector<client_info> sorted_clients() const;

    const char *status(client_status cs) {
        switch (cs) {
            case ONLINE:
                return "online";
            case OFFLINE:
                return "offline";
        }
        return "UNKNOWN";
    }
};


#endif //SERVER_H
