#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <global.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

#include "../include/Server.h"
#include "../include/logger.h"

Server::Server(const char *portNum) : port(portNum) {
}

void Server::start() {
    DEBUG_MSG("Server started at: " << port);
    connect();
    selectLoop();
}

void Server::connect() {
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
                    std::string command = std::string(&buffer[0], (unsigned long) bytes_read_count);
                    DEBUG_MSG("STDIN " << command);
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
            for (std::map<std::string, client_info>::iterator it = clients.begin(); it != clients.end(); ++it) {
                client_info cinfo = it->second;
                if (FD_ISSET(cinfo.sockfd, &read_fd)) {
                    if ((bytes_read_count = recv(cinfo.sockfd, buffer, sizeof buffer, 0)) > 0) {
                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
                        DEBUG_MSG("RECV from " << it->first << " " << msg_recvd);
                    } else {
                        if (bytes_read_count == 0) {
                            DEBUG_MSG("Client conn closed normally " << "fd:" << cinfo.sockfd << " errorno:" << errno);
                            close(cinfo.sockfd);
                            FD_CLR(cinfo.sockfd, &all_fd);
                        } else {
                            DEBUG_MSG("RECV " << "fd:" << cinfo.sockfd << " errorno:" << errno);
                        }
                    }
                }
            }
        }
    }

}

void Server::newClient(int fd, sockaddr_storage addr, socklen_t len) {
    const std::string ip = get_ip(addr);
    DEBUG_MSG("New client " << ip);
    Server::client_info cinfo = {.sockfd = fd};
    clients[ip] = cinfo;

    FD_SET(fd, &all_fd); // add new client to our set
    if (max_fd < fd) { // update max_fd
        max_fd = fd;
    }
}

const std::string Server::get_ip(sockaddr_storage &addr) {
    if (addr.ss_family == AF_INET) { // IPv4
        char client_ip[INET_ADDRSTRLEN];
        return std::string(inet_ntop(addr.ss_family,
                                     &(((struct sockaddr_in *) &addr)->sin_addr),
                                     client_ip, INET_ADDRSTRLEN));
    } else {                          // IPv6
        char client_ip[INET6_ADDRSTRLEN];
        return std::string(inet_ntop(addr.ss_family,
                                     &(((struct sockaddr_in6 *) &addr)->sin6_addr),
                                     client_ip, INET6_ADDRSTRLEN));
    }
}

