#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <global.h>
#include <errno.h>
#include <arpa/inet.h>
#include <algorithm>
#include <sstream>
#include <cstring>

#include "../include/Server.h"
#include "../include/logger.h"
#include "../include/Util.h"


Server::Server(const char *portNum) : port(portNum) {
}

void Server::start() {
    DEBUG_LOG("Server started at: " << port);
    do_bind_listen();
    select_loop();
}

void Server::do_bind_listen() {
    if (util::bind_listen_on(&server_sockfd, port)) {
        DEBUG_LOG("Created socket for listening.");
    } else {
        DEBUG_LOG("Cannot create socket for listening.");
        exit(EXIT_FAILURE);
    }
}

void Server::select_loop() {
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
            DEBUG_LOG("SELECT; errorno:" << strerror(errno));
            exit(EXIT_FAILURE);
        } else {
            // read stdin
            if (FD_ISSET(STDIN_FILENO, &read_fd)) {
                if ((bytes_read_count = read(STDIN_FILENO, buffer, sizeof buffer)) > 0) {
                    std::string command_str = std::string(&buffer[0], (unsigned long) bytes_read_count);
                    cli_command(command_str);
                } else {
                    DEBUG_LOG("STDIN " << " errorno:" << strerror(errno));
                }
            }

            // accept clients
            if (FD_ISSET(server_sockfd, &read_fd)) {
                if ((new_client_fd = accept(server_sockfd,
                                            (struct sockaddr *) &new_client_addr,
                                            &new_client_addr_len)) < 0) {
                    DEBUG_LOG("ACCEPT " << "fd:" << new_client_fd << " errorno:" << strerror(errno));
                } else {
                    new_client(new_client_fd, new_client_addr, new_client_addr_len);
                }
            }

            // recv clients
            for (std::map<client_key, ClientInfo *>::iterator it = clients.begin(); it != clients.end(); ++it) {
                if (FD_ISSET((*it->second).sockfd, &read_fd)) {
                    if ((bytes_read_count = recv((*it->second).sockfd, buffer, sizeof buffer, 0)) > 0) {
                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
                        DEBUG_LOG("RECV from " << it->first << " " << msg_recvd);
                        client_command(msg_recvd, it->first, it->second);
                    } else {
                        if (bytes_read_count == 0) {
                            DEBUG_LOG("Client connection closed normally");
                            (*it->second).status = OFFLINE;
                            close((*it->second).sockfd);
                            FD_CLR((*it->second).sockfd, &all_fd);
                        } else {
                            DEBUG_LOG("RECV " << "fd:" << (*it->second).sockfd << " errorno:" << strerror(errno));
                        }
                    }
                }
            }
        }
    }
}

void Server::new_client(int fd, sockaddr_storage addr, socklen_t len) {
    const client_key key = client_key(util::get_ip(addr), fd);

    client_info_ptype old_offline_client = NULL;

    for (std::map<client_key, client_info_ptype>::iterator it = clients.begin();
         it != clients.end(); ++it) {
        if (key.ip.compare((it->first).ip) == 0
            && it->second->status == OFFLINE) {
            old_offline_client = it->second;
        }
    };

    if (clients.count(key) == 0 && old_offline_client == NULL) {
        client_info_ptype cinfo = new ClientInfo();
        cinfo->status = ONLINE;
        cinfo->ip = key.ip;
        cinfo->sockfd = fd;
        cinfo->receive_count = 0;
        cinfo->sent_count = 0;

        char host[NI_MAXHOST], service[NI_MAXSERV];
        if (getnameinfo((struct sockaddr *) &addr,
                        len, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV) == 0) {
            DEBUG_LOG("New client " << key << " " << host << " " << service);
        }
        cinfo->host = std::string(host);
        cinfo->os_port = util::int_to_string(((struct sockaddr_in *) &addr)->sin_port);
        cinfo->service = std::string(service);

        clients[key] = cinfo;
    } else if (old_offline_client != NULL) {
        old_offline_client->status = ONLINE;
        old_offline_client->sockfd = fd;
    }

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
                    const client_info_ptype client = util::find_by_ip(ip, clients);
                    if (client != NULL) {
                        SUCCESS(command);
                        if (!client->blocked_ips.empty()) {
                            std::vector<ClientInfo> clients_v;
                            for (std::set<std::string>::const_iterator it = client->blocked_ips.begin();
                                 it != client->blocked_ips.end(); ++it) {
                                const client_info_ptype blocked_client = util::find_by_ip((*it), clients);
                                if (blocked_client != NULL) {
                                    clients_v.push_back(*blocked_client);
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
        DEBUG_LOG("Error: " << e.what());
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

void Server::client_command(std::string command_str, const client_key key, ClientInfo *client) {
    std::vector<std::string> command_v = util::split_by_space(command_str);
    try {
        std::string command = command_v.at(0);
        switch (client_server::parse_command(command)) {
            case client_server::LOGIN: {
                client->client_port = command_v.at(1);
                client->status = LOGGED_IN;

                std::stringstream ss;
                ss << client_server::command_str(client_server::LOGIN)
                << COMMAND_SEPARATOR << key.ip
                << COMMAND_SEPARATOR << key.sockfd
                << std::endl;
                ClientInfo::serialize_to(&ss, clients);
                util::send_string(client->sockfd, ss.str());

                log_relay(client);
                break;
            }
            case client_server::REFRESH: {
                std::stringstream ss;
                ss << client_server::command_str(client_server::REFRESH)
                << COMMAND_SEPARATOR << key.ip
                << COMMAND_SEPARATOR << key.sockfd
                << std::endl;
                ClientInfo::serialize_to(&ss, clients);
                util::send_string(client->sockfd, ss.str());

                log_relay(client);
                break;
            }
            case client_server::SEND: {
                std::string ip = command_v.at(1);
                const std::string msg = util::join_by_space_from_pos(command_v, 2);
                if (util::valid_inet(ip)) {
                    const client_info_ptype to_client = util::find_by_ip(ip, clients);
                    if (to_client != NULL) {
                        client->sent_count++;
                        if ((to_client->blocked_ips.count(client->ip) == 0)) {
                            if (send_client_command(to_client, client_server::SEND, client->ip, msg)) {
                                to_client->receive_count++;
                                relay(client->ip, ip, msg);
                            } else {
                                to_client->messages.push_back(std::make_pair(key, msg));
                            }
                        }
                    }
                }
                break;
            }
            case client_server::BROADCAST: {
                const std::string msg = util::join_by_space_from_pos(command_v, 1);
                client->sent_count++;
                for (std::map<client_key, client_info_ptype>::iterator it = clients.begin();
                     it != clients.end(); ++it) {
                    if (!(it->first == key)) {
                        if (it->second->blocked_ips.count(client->ip) == 0) {
                            if (send_client_command(it->second, client_server::BROADCAST, msg)) {
                                it->second->receive_count++;
                            } else {
                                it->second->messages.push_back(
                                        std::make_pair(client_key(BROADCAST_IP, key.sockfd), msg));
                            }
                        }
                    }
                };
                relay(client->ip, BROADCAST_IP, msg);
                break;
            }
            case client_server::BLOCK: {
                std::string ip = command_v.at(1);
                if (util::valid_inet(ip)) {
                    client->blocked_ips.insert(ip);
                }
                break;
            }
            case client_server::UNBLOCK: {
                std::string ip = command_v.at(1);
                if (util::valid_inet(ip)) {
                    client->blocked_ips.erase(ip);
                }
                break;
            }
            case client_server::LOGOUT: {
                client->status = OFFLINE;
                close(client->sockfd);
                FD_CLR(client->sockfd, &all_fd);
                break;
            }
            case client_server::EXIT: {
                client->status = OFFLINE;
                close(client->sockfd);
                FD_CLR(client->sockfd, &all_fd);
                clients.erase(key);
                break;
            }
            case client_server::UNKNOWN:
                break;
        }
    }
    catch (const std::exception &e) {
        DEBUG_LOG("Error: " << e.what());
    }

}

void Server::log_relay(ClientInfo *client) {
    if (!client->messages.empty()) {
        for (std::deque<std::pair<client_key, std::string> >::const_iterator it = client->messages.begin();
             it != client->messages.end(); ++it) {
            client->receive_count++;
            relay(it->first.ip, client->ip, it->second);
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
        if (util::send_string(client->sockfd, ss.str())) {
            return true;
        } else {
            DEBUG_LOG("Cannot send " << command << " to server error: " << strerror(errno));
        }
    }
    return false;
}
