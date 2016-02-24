#include <../include/global.h>
#include "../include/Client.h"
#include "../include/logger.h"
#include "../include/Util.h"

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <global.h>
#include <sstream>
#include <errno.h>
#include <arpa/inet.h>
#include <algorithm>
#include "command.h"

Client::Client(const char *portNum) : port(portNum) {
}

void Client::start() {
    DEBUG_MSG("Client started at: " << port);
    bindListen();
    selectLoop();
}

void Client::bindListen() {
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
        p2p_listen_sockfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (p2p_listen_sockfd == -1)
            continue;

        if (setsockopt(p2p_listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
            DEBUG_MSG("Cannot setsockopt() on: " << p2p_listen_sockfd << " ;errono:" << errno);
            close(p2p_listen_sockfd);
            continue;
        }

        if (bind(p2p_listen_sockfd, temp->ai_addr, temp->ai_addrlen) == 0) {
            break;
        }

        close(p2p_listen_sockfd);
    }

    if (temp == NULL) {
        DEBUG_MSG("Cannot find a socket to bind to; errno:" << errno);
        exit(EXIT_FAILURE);
    } else {
        p2p_listen_addrinfo = *temp;
    }

    freeaddrinfo(result);

    if (listen(p2p_listen_sockfd, SERVER_BACKLOG) == -1) {
        DEBUG_MSG("Cannot listen on: " << p2p_listen_sockfd << "; errno: " << errno);
        exit(EXIT_FAILURE);
    }
}

void Client::selectLoop() {
    FD_ZERO(&all_fd);
    FD_ZERO(&read_fd);

    FD_SET(STDIN_FILENO, &all_fd);
    FD_SET(p2p_listen_sockfd, &all_fd);
    max_fd = p2p_listen_sockfd;

    // new client
    struct sockaddr_storage new_client_addr;
    socklen_t new_client_addr_len = sizeof(new_client_addr);
    int new_client_fd;

    // recv buffer
    char buffer[MSG_LEN], server_buffer[SERVER_MSG_LEN];
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
                    cli_command(command_str);
                } else {
                    DEBUG_MSG("STDIN " << " errorno:" << errno);
                }
            }

            // read from server
            if (logged_in && FD_ISSET(server_sockfd, &read_fd)) {
                if ((bytes_read_count = recv(server_sockfd, server_buffer, sizeof server_buffer, 0)) > 0) {
                    std::string command_str = std::string(&server_buffer[0], (unsigned long) bytes_read_count);
                    DEBUG_MSG("Server RECV\n" << command_str);
                    server_command(command_str);
                } else {
                    if (bytes_read_count == 0) {
                        DEBUG_MSG("Server connection closed normally " << " errorno:" << errno);
                        close(server_sockfd);
                        FD_CLR(server_sockfd, &all_fd);
                    } else {
                        DEBUG_MSG("RECV " << "fd:" << server_sockfd << " errorno:" << errno);
                    }
                }
            }

            // accept p2p_clients
            if (FD_ISSET(p2p_listen_sockfd, &read_fd)) {
                if ((new_client_fd = accept(p2p_listen_sockfd,
                                            (struct sockaddr *) &new_client_addr,
                                            &new_client_addr_len)) < 0) {
                    DEBUG_MSG("ACCEPT " << "fd:" << new_client_fd << " errorno:" << errno);
                    exit(EXIT_FAILURE);
                } else {
                    newClient(new_client_fd, new_client_addr, new_client_addr_len);
                }
            }

            // recv p2p_clients
//            for (std::set<ClientInfo>::iterator it = p2p_clients.begin(); it != p2p_clients.end(); ++it) {
//                ClientInfo p2p_client = *it;
//                if (FD_ISSET(p2p_client.sockfd, &read_fd)) {
//                    if ((bytes_read_count = recv(p2p_client.sockfd, buffer, sizeof buffer, 0)) > 0) {
//                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
//                        DEBUG_MSG("RECV from " << p2p_client.sockfd << " " << msg_recvd);
//                        command(msg_recvd);
//                    } else {
//                        if (bytes_read_count == 0) {
//                            DEBUG_MSG("Client connection closed normally " << "fd:" << p2p_client.sockfd <<
//                                      " errorno:" <<
//                                      errno);
//                            close(p2p_client.sockfd);
//                            FD_CLR(p2p_client.sockfd, &all_fd);
//                        } else {
//                            DEBUG_MSG("RECV " << "fd:" << p2p_client.sockfd << " errorno:" << errno);
//                        }
//                    }
//                }
//            }
        }
    }
}

void Client::newClient(int fd, sockaddr_storage addr, socklen_t len) {
    const client_key key = {util::get_ip(addr), fd};

    if (p2p_clients.count(key) == 0) {
        DEBUG_MSG("p2p request from unkown client " << key);
    } else {
        client_info_ptype cinfo = p2p_clients[key];
        (*cinfo).sockfd = key.sockfd;
        DEBUG_MSG("p2p request from client " << key);

        FD_SET(fd, &all_fd); // add new client to our set
        if (max_fd < fd) { // update max_fd
            max_fd = fd;
        }
    }
}

void Client::cli_command(std::string command_str) {
    std::vector<std::string> command_v = util::split_by_space(command_str);
    try {
        std::string command = command_v.at(0);
        switch (client::parse_cli_command(command)) {
            case client::AUTHOR: {
                author(command);
                break;
            }
            case client::IP: {
                ip(command);
                break;
            }
            case client::PORT: {
                port_command(command, port);
                break;
            }
            case client::LIST: {
                if (logged_in) {
                    list_command(command, clients_from_server);
                } else {
                    ERROR(command);
                    DEBUG_MSG("Execute login first.");
                    END(command);
                }
                break;
            }
            case client::LOGIN: {
                if (do_login(command_v.at(1), command_v.at(2))) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::REFRESH:
                if (logged_in && send_server_command(client_server::REFRESH)) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            case client::SEND: {
                std::string ip = command_v.at(1);
                const client_key *client_with_ip = util::get_by_ip(ip, clients_from_server);
                if (logged_in
                    && util::valid_inet(ip)
                    && client_with_ip != NULL
                    && send_server_command(client_server::SEND, ip, command_v.at(2))) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::BROADCAST:
                if (logged_in
                    && send_server_command(client_server::BROADCAST, command_v.at(1))) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            case client::BLOCK:
                break;
            case client::UNBLOCK:
                break;
            case client::LOGOUT:
                break;
            case client::EXIT:
                break;
            case client::SENDFILE:
                break;
            case client::UNKNOWN:
                break;
        }
    }
    catch (const std::exception &e) {
        DEBUG_MSG("Error: " << e.what());
    }
}

bool Client::do_login(std::string ip, std::string port) {
    if (util::valid_inet(ip)) {
        struct addrinfo *result, *temp;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int getaddrinfo_result;
        if ((getaddrinfo_result = getaddrinfo(ip.c_str(), port.c_str(), &hints, &result)) != 0) {
            DEBUG_MSG("getaddrinfo: " << gai_strerror(getaddrinfo_result));
            return false;
        }

        for (temp = result; temp != NULL; temp = temp->ai_next) {
            server_sockfd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
            if (server_sockfd == -1)
                continue;

            if (connect(server_sockfd, temp->ai_addr, temp->ai_addrlen) == -1) {
                close(server_sockfd);
                DEBUG_MSG("Cannot connect to server errno:" << errno);
                continue;
            }

            server_addrinfo = *temp;
            break;
        }

        if (temp == NULL) {
            DEBUG_MSG("Cannot find a socket to bind to; errno:" << errno);
            return false;
        }
        freeaddrinfo(result);

        FD_SET(server_sockfd, &all_fd); // add server to our set
        if (max_fd < server_sockfd) { // update max_fd
            max_fd = server_sockfd;
        }

        logged_in = true;

        return send_server_command(client_server::LOGIN, port);
    }
    return false;
}

bool Client::send_server_command(client_server::command c, std::string arg1, std::string arg2) const {
    if (logged_in) {
        std::stringstream ss;
        const std::string command = command_str(c);
        ss << command;
        if (!arg1.empty()) {
            ss << COMMAND_SEPARATOR << arg1;
        }
        if (!arg2.empty()) {
            ss << COMMAND_SEPARATOR << arg2;
        }
        if (util::sendString(server_sockfd, ss.str()) == -1) {
            DEBUG_MSG("Cannot send " << command << " to server error: " << errno);
        } else {
            DEBUG_MSG("SENT " << command << " to server");
            return true;
        }
    }
    return false;
}

void Client::server_command(std::string command_data) {
    std::istringstream f(command_data);
    std::string command_str;
    std::getline(f, command_str);
    std::vector<std::string> command_v = util::split_by_space(command_str);

    try {
        std::string command = command_v.at(0);
        switch (client_server::parse_command(command)) {
            case client_server::LOGIN: {
                clients_from_server = ClientInfo::deserilaizeFrom(&f);
                log_recieved(command_v.at(1));
                break;
            }
            case client_server::REFRESH: {
                clients_from_server = ClientInfo::deserilaizeFrom(&f);
                log_recieved(command_v.at(1));
                break;
            }
            case client_server::SEND: {
                received(command_v.at(1), command_v.at(2));
                break;
            }
            case client_server::BROADCAST:
                received("255.255.255.255", command_v.at(2));
                break;
            case client_server::BLOCK:
                break;
            case client_server::UNBLOCK:
                break;
            case client_server::LOGOUT:
                break;

            case client_server::UNKNOWN:
                break;
        }
    }
    catch (const std::exception &e) {
        DEBUG_MSG("Error: " << e.what());
    }
}

void Client::log_recieved(std::string ip) {
    const client_key *me = util::get_by_ip(ip, clients_from_server);
    if (me != NULL) {
        ClientInfo *c = clients_from_server[*me];
        if (!(c)->messages.empty()) {
            for (std::deque<std::pair<std::string, std::string> >::const_iterator it = c->messages.begin();
                 it != c->messages.end(); ++it) {
                received(it->first, it->second);
            }
        }
    }
}
