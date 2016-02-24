#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <global.h>
#include <errno.h>
#include <arpa/inet.h>
#include <algorithm>
#include <sstream>

#include "../include/Server.h"
#include "../include/ClientInfo.h"
#include "../include/logger.h"
#include "../include/Util.h"
#include "command.h"


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
                    cli_command(command_str);
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
            for (std::map<client_key, ClientInfo *>::iterator it = clients.begin(); it != clients.end(); ++it) {
                if (FD_ISSET((*it->second).sockfd, &read_fd)) {
                    if ((bytes_read_count = recv((*it->second).sockfd, buffer, sizeof buffer, 0)) > 0) {
                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
                        DEBUG_MSG("RECV from " << it->first << " " << msg_recvd);
                        client_command(msg_recvd, it->second);
                    } else {
                        if (bytes_read_count == 0) {
                            DEBUG_MSG("Client connection closed normally " << "fd:" << (*it->second).sockfd <<
                                      " errorno:" <<
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
    const client_key key = {util::get_ip(addr), fd};

    if (clients.count(key) == 0) {
        client_info_ptype cinfo = new ClientInfo();
        clients[key] = cinfo;
    }

    char host[NI_MAXHOST], service[NI_MAXSERV];

    if (getnameinfo((struct sockaddr *) &addr,
                    len, host, NI_MAXHOST,
                    service, NI_MAXSERV, NI_NUMERICSERV) == 0) {
        DEBUG_MSG("New client " << key << " " << host << " " << service);
    }
    client_info_ptype cinfo = clients[key];
    (*cinfo).sockfd = key.sockfd;
    (*cinfo).ip = key.ip;
    (*cinfo).host = std::string(host);
    (*cinfo).os_port = util::int_to_string(((struct sockaddr_in *) &addr)->sin_port);
    (*cinfo).service = std::string(service);
    (*cinfo).status = ONLINE;
    (*cinfo).receive_count = 0;
    (*cinfo).sent_count = 0;

    FD_SET(fd, &all_fd); // add new client to our set
    if (max_fd < fd) { // update max_fd
        max_fd = fd;
    }
}


void Server::cli_command(std::string command_str) {
    std::vector<std::string> command_v = util::split_by_space(command_str);
    try {
        std::string command = command_v.at(0);
        switch (server::parse_cli_command(command)) {
            case server::AUTHOR: {
                author(command);
                break;
            }
            case server::IP: {
                ip(command);
                break;
            }
            case server::PORT: {
                port_command(command, port);
                break;
            }
            case server::LIST: {
                list_command(command, clients);
                break;
            }
            case server::STATISTICS: {
                SUCCESS(command);
                if (!clients.empty()) {
                    std::vector<ClientInfo> clients_v = sorted_clients();
                    for (std::vector<ClientInfo>::size_type i = 0; i != clients_v.size(); i++) {
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
            }
            case server::BLOCKED: {
                std::string ip = command_v.at(1);
                if (util::valid_inet(ip)) {
                    const client_key *client_with_ip = util::get_by_ip(ip, clients);
                    if (client_with_ip != NULL) {
                        SUCCESS(command);
                        ClientInfo *client = clients[*client_with_ip];
                        if (!client->blocked_ips.empty()) {
                            std::vector<ClientInfo> clients_v;
                            for (std::set<std::string>::const_iterator it = client->blocked_ips.begin();
                                 it != client->blocked_ips.end(); ++it) {
                                const client_key *blocked_client = util::get_by_ip((*it), clients);
                                if (blocked_client != NULL) {
                                    clients_v.push_back(*clients[*blocked_client]);
                                }
                            };
                            sort(clients_v.begin(), clients_v.end());

                            for (std::vector<ClientInfo>::size_type i = 0; i != clients_v.size(); i++) {
                                cse4589_print_and_log("%-5d%-35s%-20s%-8s\n",
                                                      i,
                                                      clients_v[i].host.c_str(),
                                                      clients_v[i].ip.c_str(),
                                                      clients_v[i].client_port.c_str());
                            }
                        }
                    } else {
                        ERROR(command);
                    }
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case server::UNKNOWN:
                break;
        }
    }
    catch (const std::exception &e) {
        DEBUG_MSG("Error: " << e.what());
    }
}

std::vector<ClientInfo> Server::sorted_clients() const {
    std::vector<ClientInfo> clients_v;
    for (std::map<client_key, client_info_ptype>::const_iterator it = clients.begin();
         it != clients.end(); ++it) {
        clients_v.push_back(*(it->second));
    };
    sort(clients_v.begin(), clients_v.end());
    return clients_v;
}


Server::~Server() {
    for (std::map<client_key, client_info_ptype>::const_iterator it = clients.begin();
         it != clients.end(); ++it) {
        delete it->second;
    };
}

void Server::client_command(std::string command_str, ClientInfo *client) {
    std::vector<std::string> command_v = util::split_by_space(command_str);
    try {
        std::string command = command_v.at(0);
        switch (client_server::parse_command(command)) {
            case client_server::LOGIN: {
                client->client_port = command_v.at(1);
                client->status = LOGGED_IN;

                std::stringstream ss;
                ss << client_server::command_str(client_server::LOGIN) << COMMAND_SEPARATOR << client->ip << std::endl;
                ClientInfo::serilaizeTo(&ss, clients);
                util::sendString(client->sockfd, ss.str());

                log_relay(client);
                break;
            }
            case client_server::REFRESH: {
                std::stringstream ss;
                ss << client_server::command_str(client_server::REFRESH) << COMMAND_SEPARATOR << client->ip <<
                std::endl;
                ClientInfo::serilaizeTo(&ss, clients);
                util::sendString(client->sockfd, ss.str());

                log_relay(client);
                break;
            }
            case client_server::SEND: {
                std::string ip = command_v.at(1);
                if (util::valid_inet(ip)) {
                    const client_key *client_with_ip = util::get_by_ip(ip, clients);
                    if (client_with_ip != NULL) {
                        client_info_ptype to = clients[*client_with_ip];
                        if ((to->blocked_ips.count(client->ip) == 0)) {
                            if (send_client_command(to, client_server::SEND, ip, command_v.at(2))) {
                                relay(client->ip, ip, command_v.at(2));
                            } else {
                                to->messages.push_back(std::make_pair(client->ip, command_v.at(2)));
                            }
                        }
                    }
                }
                break;
            }
            case client_server::BROADCAST: {
                bool sent = false;
                for (std::map<client_key, client_info_ptype>::iterator it = clients.begin();
                     it != clients.end(); ++it) {
                    if (it->second != client) {
                        if (it->second->blocked_ips.count(client->ip) == 0) {
                            if (send_client_command(it->second, client_server::BROADCAST, command_v.at(1))) {
                                sent = true;
                            } else {
                                it->second->messages.push_back(std::make_pair("255.255.255.255", command_v.at(2)));
                            }
                        }
                    }
                };
                relay(client->ip, "255.255.255.255", command_v.at(1));
                break;
            }
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

void Server::log_relay(const ClientInfo *client) const {
    if (!client->messages.empty()) {
        for (std::deque<std::pair<std::string, std::string> >::const_iterator it = client->messages.begin();
             it != client->messages.end(); ++it) {
            relay(it->first, client->ip, it->second);
        }
    }
}

bool Server::send_client_command(client_info_ptype client,
                                 client_server::command c,
                                 std::string argv1,
                                 std::string argv2) {
    if (client->status == LOGGED_IN) {
        std::stringstream ss;
        const std::string command = command_str(c);
        ss << command;
        if (!argv1.empty()) {
            ss << COMMAND_SEPARATOR << argv1;
        }
        if (!argv2.empty()) {
            ss << COMMAND_SEPARATOR << argv2;
        }
        if (util::sendString(client->sockfd, ss.str()) == -1) {
            DEBUG_MSG("Cannot send " << command << " to server error: " << errno);
        } else {
            return true;
        }
    }
    return false;
}
