#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <global.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <command.h>
#include <stdexcept>

#include "../include/Server.h"
#include "../include/logger.h"


Server::Server(const char *portNum) : port(portNum) {
}

void Server::start() {
    DEBUG_MSG("Server started at: " << port);
    bindListen();
    selectLoop();
}

void Server::bindListen() {
    struct addrinfo *result, *temp;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;;

    int getaddrinfo_result;
    int reuse = 1;
    if ((getaddrinfo_result = getaddrinfo(NULL, port, &hints, &result)) != 0) {
        DEBUG_MSG("getaddrinfo: " << gai_strerror(getaddrinfo_result));
        exit(EXIT_FAILURE);
    }

    for (temp = result; temp != NULL; temp = temp->ai_next) {
        server_sockfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (server_sockfd == -1)
            continue;

        if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            DEBUG_MSG("Cannot setsockopt() on: " << server_sockfd << " ;errono:" << errno);
            close(server_sockfd);
            continue;
        }

        if (bind(server_sockfd, temp->ai_addr, temp->ai_addrlen) == 0) {
            break;
        }

        close(server_sockfd);
    }

    if (temp == NULL) {
        DEBUG_MSG("Cannot find a socket to bind to; errno:" << errno);
        exit(EXIT_FAILURE);
    } else {
        server_addrinfo = *temp;
    }

    freeaddrinfo(result);

    if (listen(server_sockfd, SERVER_BACKLOG) == -1) {
        DEBUG_MSG("Cannot listen on: " << server_sockfd << "; errno: " << errno);
        exit(EXIT_FAILURE);
    }
}

void Server::selectLoop() {
    FD_ZERO(&all_fd);
    FD_ZERO(&read_fd);

    FD_SET(STDIN_FILENO, &all_fd);
    FD_SET(server_sockfd, &all_fd);
    max_fd = server_sockfd;

    // new client
    struct sockaddr_storage new_client_addr;
    socklen_t new_client_addr_len = sizeof(new_client_addr);
    int new_client_fd;

    // recv buffer
    char buffer[MSG_LEN];
    ssize_t bytes_read_count;

    while (true) {
        read_fd = all_fd;
        if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) == -1) {
            DEBUG_MSG("SELECT; errorno:" << errno);
            exit(EXIT_FAILURE);
        } else {
            // read stdin
            if (FD_ISSET(STDIN_FILENO, &read_fd)) {
                if ((bytes_read_count = read(STDIN_FILENO, buffer, sizeof buffer)) > 0) {
                    std::string command_str = std::string(&buffer[0], (unsigned long) bytes_read_count);
                    command(command_str, NULL);
                } else {
                    DEBUG_MSG("STDIN " << " errorno:" << errno);
                }
            }

            // accept clients
            if (FD_ISSET(server_sockfd, &read_fd)) {
                if ((new_client_fd = accept(server_sockfd,
                                            (struct sockaddr *) &new_client_addr,
                                            &new_client_addr_len)) < 0) {
                    DEBUG_MSG("ACCEPT " << "fd:" << new_client_fd << " errorno:" << errno);
                    exit(EXIT_FAILURE);
                } else {
                    newClient(new_client_fd, new_client_addr, new_client_addr_len);
                }
            }

            // recv clients
            for (std::map<std::string, client_info*>::iterator it = clients.begin(); it != clients.end(); ++it) {
                if (FD_ISSET((*it->second).sockfd, &read_fd)) {
                    if ((bytes_read_count = recv((*it->second).sockfd, buffer, sizeof buffer, 0)) > 0) {
                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
                        DEBUG_MSG("RECV from " << it->first << " " << msg_recvd);
                        command(msg_recvd, it->second);
                    } else {
                        if (bytes_read_count == 0) {
                            DEBUG_MSG("Client connection closed normally " << "fd:" << (*it->second).sockfd << " errorno:" <<
                                      errno);
                            (*it->second).status = OFFLINE;
                            close((*it->second).sockfd);
                            FD_CLR((*it->second).sockfd, &all_fd);
                        } else {
                            DEBUG_MSG("RECV " << "fd:" << (*it->second).sockfd << " errorno:" << errno);
                        }
                    }
                }
            }
        }
    }

}

void Server::newClient(int fd, sockaddr_storage addr, socklen_t len) {
    const std::string ip = get_ip(addr);

    if (clients.count(ip) == 0) {
        char host[NI_MAXHOST], service[NI_MAXSERV];

        if (getnameinfo((struct sockaddr *) &addr,
                        len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV) == 0) {
            DEBUG_MSG("New client " << ip << " " << host << " " << service);
        }

        Server::client_info_type cinfo = new client_info();
        (*cinfo).sockfd = fd;
        (*cinfo).ip = ip;
        (*cinfo).host = std::string(host);
        (*cinfo).service = std::string(service);
        (*cinfo).status = ONLINE;
        (*cinfo).receive_count = 0;
        (*cinfo).sent_count = 0;
        clients[ip] = cinfo;
    } else {
        Server::client_info_type cinfo = clients[ip];
        (*cinfo).status = ONLINE;
        DEBUG_MSG("Old client " << ip << " " << (*cinfo).host << " " << (*cinfo).service << status((*cinfo).status));
    }

    FD_SET(fd, &all_fd); // add new client to our set
    if (max_fd < fd) { // update max_fd
        max_fd = fd;
    }
}

const std::string Server::get_ip(sockaddr_storage addr) const {
    if (addr.ss_family == AF_INET) { // IPv4
        char ip[INET_ADDRSTRLEN];
        return std::string(inet_ntop(addr.ss_family,
                                     &(((struct sockaddr_in *) &addr)->sin_addr),
                                     ip, INET_ADDRSTRLEN));
    } else {                          // IPv6
        char ip[INET6_ADDRSTRLEN];
        return std::string(inet_ntop(addr.ss_family,
                                     &(((struct sockaddr_in6 *) &addr)->sin6_addr),
                                     ip, INET6_ADDRSTRLEN));
    }
}

void Server::command(std::string command_str, Server::client_info *cinfo = NULL) {
    std::vector<std::string> command_v = split_by_space(command_str);
    try {
        std::string command = command_v.at(0);
        switch (parse_command(command)) {
            case AUTHOR: {
                SUCCESS(command);
                cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n",
                                      UBIT_NAME);
                END(command);
                break;
            }
            case IP: {
                std::string ip = primary_ip();
                if (!ip.empty()) {
                    SUCCESS(command);
                    cse4589_print_and_log("IP:%s\n", ip.c_str());
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case PORT: {
                SUCCESS(command);
                cse4589_print_and_log("PORT:%d\n", port);
                END(command);
                break;
            }
            case LIST: {
                SUCCESS(command);
                if (!clients.empty()) {
                    std::vector<Server::client_info> clients_v = sorted_clients();
                    for (std::vector<client_info>::size_type i = 0; i != clients_v.size(); i++) {
                        if (clients_v[i].status == ONLINE) {
                            cse4589_print_and_log("%-5d%-35s%-20s%-8d\n",
                                                  i,
                                                  clients_v[i].host.c_str(),
                                                  clients_v[i].ip.c_str(),
                                                  clients_v[i].service.c_str());
                        }
                    }
                }
                END(command);
                break;
            }
            case STATISTICS:
                SUCCESS(command);
                if (!clients.empty()) {
                    std::vector<Server::client_info> clients_v = sorted_clients();
                    for (std::vector<client_info>::size_type i = 0; i != clients_v.size(); i++) {
                        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n",
                                              i,
                                              clients_v[i].host.c_str(),
                                              clients_v[i].sent_count,
                                              clients_v[i].receive_count,
                                              status(clients_v[i].status));
                    }
                }
                END(command);
                break;
            case BLOCKED:
                break;
            case LOGIN:
                break;
            case REFRESH:
                break;
            case BROADCAST:
                break;
            case BLOCK:
                break;
            case UNBLOCK:
                break;
            case LOGOUT:
                break;
            case EXIT:
                break;
            case SENDFILE:
                break;
            case UNKNOWN:
                break;
        }
    }
    catch (const std::out_of_range &oor) {
        DEBUG_MSG("Out of Range error: " << oor.what());
    }
}

std::vector<Server::client_info> Server::sorted_clients() const {
    std::vector<client_info> clients_v;
    for (std::map<std::string, client_info_type >::const_iterator it = clients.begin();
         it != clients.end(); ++it) {
        clients_v.push_back(*(it->second));
    };
    sort(clients_v.begin(), clients_v.end());
    return clients_v;
}

std::vector<std::string> Server::split_by_space(const std::string s) const {
    std::stringstream ss(s);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::vector<std::string> v(begin, end);
    return v;
}

const std::string Server::primary_ip() {
    int sock_fd;
    struct addrinfo *result, *temp;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int getaddrinfo_result;
    if ((getaddrinfo_result = getaddrinfo("8.8.8.8", "53", &hints, &result)) != 0) {
        DEBUG_MSG("getaddrinfo: " << gai_strerror(getaddrinfo_result));
        return EMPTY_STRING;
    }

    for (temp = result; temp != NULL; temp = temp->ai_next) {
        sock_fd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (sock_fd == -1) {
            close(server_sockfd);
            continue;
        }
        break;
    }

    if (temp == NULL) {
        DEBUG_MSG("Cannot create a socket; errno:" << errno);
        return EMPTY_STRING;
    }

    freeaddrinfo(result);

    if (connect(sock_fd, temp->ai_addr, temp->ai_addrlen) == -1) {
        DEBUG_MSG("Cannot connect on: " << sock_fd << "; errno: " << errno);
        return EMPTY_STRING;
    }

    struct sockaddr_storage in;
    socklen_t in_len = sizeof(in);

    if (getsockname(sock_fd, (struct sockaddr *) &in, &in_len) == -1) {
        DEBUG_MSG("Cannot getsockname on: " << sock_fd << "; errno: " << errno);
        return EMPTY_STRING;
    }

    close(sock_fd);
    return get_ip(in);
}

Server::~Server() {
    for (std::map<std::string, client_info_type >::const_iterator it = clients.begin();
         it != clients.end(); ++it) {
        delete it->second;
    };
}
