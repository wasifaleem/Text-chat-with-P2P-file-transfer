#include <../include/global.h>
#include "../include/Client.h"
#include "../include/logger.h"
#include "../include/Util.h"

#include <iostream>
#include <fstream>
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
    me = client_key();
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
                        logged_in = false;
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
                } else {
                    new_p2p_file_client(new_client_fd, new_client_addr, new_client_addr_len);
                }
            }

            // recv p2p clients
            for (std::map<client_key, p2p_client_ptype>::iterator it = p2p_clients.begin();
                 it != p2p_clients.end(); ++it) {
                if (FD_ISSET((*it->second).sock_fd, &read_fd)) {
                    if ((bytes_read_count = recv((*it->second).sock_fd, buffer, sizeof buffer, 0)) > 0) {
                        std::string msg_recvd = std::string(&buffer[0], (unsigned long) bytes_read_count);
                        DEBUG_MSG("RECV from " << it->first << " " << msg_recvd);
                        p2p_client_command(msg_recvd, it->first, it->second);
                    } else {
                        if (bytes_read_count == 0) {
                            DEBUG_MSG("P2P Client connection closed normally ");
                            p2p_clients.erase(it->first);
                            close((*it->second).sock_fd);
                            FD_CLR((*it->second).sock_fd, &all_fd);
                        } else {
                            DEBUG_MSG("RECV " << "fd:" << (*it->second).sock_fd << " errorno:" << errno);
                        }
                    }
                }
            }
        }
    }
}

void Client::new_p2p_file_client(int fd, sockaddr_storage addr, socklen_t len) {
    const client_key key = client_key(util::get_ip(addr), fd);

    if (p2p_clients.count(key) == 0) {
        p2p_client_ptype p2p_file = new p2p_client();
        p2p_clients[key] = p2p_file;
    } else {
        p2p_client_ptype p2p_file = p2p_clients[key];
        (*p2p_file).sock_fd = key.sockfd;
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
            case client::REFRESH: {
                if (logged_in && send_server_command(client_server::REFRESH)) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::SEND: {
                std::string ip = command_v.at(1);
                const std::string msg = util::join_by_space_from_pos(command_v, 2);
                const client_info_ptype client = util::find_by_ip(ip, clients_from_server);
                if (logged_in
                    && util::valid_inet(ip)
                    && client != NULL
                    && send_server_command(client_server::SEND, ip, msg)) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::BROADCAST: {
                const std::string msg = util::join_by_space_from_pos(command_v, 1);
                if (logged_in
                    && send_server_command(client_server::BROADCAST, msg)) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }

                END(command);
                break;
            }
            case client::BLOCK: {
                std::string ip = command_v.at(1);
                client_info_ptype myself = find_me();
                const client_info_ptype client = util::find_by_ip(ip, clients_from_server);
                if (logged_in
                    && util::valid_inet(ip)
                    && client != NULL
                    && (myself->blocked_ips.count(ip) == 0)
                    && send_server_command(client_server::BLOCK, ip)) {
                    myself->blocked_ips.insert(ip);
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::UNBLOCK: {
                std::string ip = command_v.at(1);
                client_info_ptype myself = find_me();
                const client_info_ptype client = util::find_by_ip(ip, clients_from_server);
                if (logged_in
                    && util::valid_inet(ip)
                    && client != NULL
                    && (myself->blocked_ips.count(ip) > 0)
                    && send_server_command(client_server::UNBLOCK, ip)) {
                    myself->blocked_ips.erase(ip);
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::LOGOUT: {
                if (logged_in
                    && send_server_command(client_server::LOGOUT)) {
                    logged_in = false;
                    close(server_sockfd);
                    FD_CLR(server_sockfd, &all_fd);
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
            case client::EXIT: {
                if (logged_in
                    && send_server_command(client_server::EXIT)) {
                    logged_in = false;
                    close(server_sockfd);
                    FD_CLR(server_sockfd, &all_fd);
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                exit(0);
            }
            case client::SENDFILE: {
                std::string ip = command_v.at(1);
                std::string file_name = command_v.at(2);
                const client_info_ptype to_client = util::find_by_ip(ip, clients_from_server);
                if (logged_in
                    && util::valid_inet(ip)
                    && to_client != NULL
                    && send_file(to_client, file_name)) {
                    SUCCESS(command);
                } else {
                    ERROR(command);
                }
                END(command);
                break;
            }
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

        return send_server_command(client_server::LOGIN, std::string(this->port));
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
        if (util::send_string(server_sockfd, ss.str())) {
            DEBUG_MSG("SENT " << command << " to server");
            return true;
        } else {
            DEBUG_MSG("Cannot send " << command << " to server error: " << errno);
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
                clients_from_server = ClientInfo::deserializeFrom(&f);
                me = client_key(command_v.at(1), util::str_to_int(command_v.at(2)));

                log_received();
                break;
            }
            case client_server::REFRESH: {
                clients_from_server = ClientInfo::deserializeFrom(&f);
                me = client_key(command_v.at(1), util::str_to_int(command_v.at(2)));

                log_received();
                break;
            }
            case client_server::SEND: {
                const std::string msg = util::join_by_space_from_pos(command_v, 2);
                received(command_v.at(1), msg);
                break;
            }
            case client_server::BROADCAST: {
                const std::string msg = util::join_by_space_from_pos(command_v, 1);
                received("255.255.255.255", msg);
                break;
            }
            case client_server::BLOCK:
                break;
            case client_server::UNBLOCK:
                break;
            case client_server::LOGOUT:
                break;
            case client_server::EXIT:
                break;

            case client_server::UNKNOWN:
                break;
        }
    }
    catch (const std::exception &e) {
        DEBUG_MSG("Error: " << e.what());
    }
}

void Client::log_received() {
    client_info_ptype client = find_me();
    if (client != NULL) {
        if (!(client->messages.empty())) {
            for (std::deque<std::pair<client_key, std::string> >::const_iterator it = client->messages.begin();
                 it != client->messages.end(); ++it) {
                received(it->first.ip, it->second);
            }
        }
    }
}

Client::~Client() {
    for (std::map<client_key, client_info_ptype>::const_iterator it = clients_from_server.begin();
         it != clients_from_server.end(); ++it) {
        delete it->second;
    };
    for (std::map<client_key, p2p_client_ptype>::const_iterator it = p2p_clients.begin();
         it != p2p_clients.end(); ++it) {
        delete it->second;
    };
}

const client_info_ptype Client::find_me() {
    std::map<client_key, client_info_ptype>::iterator i = clients_from_server.find(me);
    if (i != clients_from_server.end()) {
        return i->second;
    }
    return NULL;
}

bool Client::send_file(client_info_ptype const to_client, std::string file_name) {
    int p2p_client_fd = 0;
    struct addrinfo *result, *temp;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int getaddrinfo_result;
    if ((getaddrinfo_result = getaddrinfo(to_client->ip.c_str(), to_client->client_port.c_str(), &hints, &result)) !=
        0) {
        DEBUG_MSG("getaddrinfo: " << gai_strerror(getaddrinfo_result));
        return false;
    }

    for (temp = result; temp != NULL; temp = temp->ai_next) {
        p2p_client_fd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (p2p_client_fd == -1)
            continue;

        if (connect(p2p_client_fd, temp->ai_addr, temp->ai_addrlen) == -1) {
            close(p2p_client_fd);
            DEBUG_MSG("Cannot connect to p2p errno:" << errno);
            continue;
        }

        break;
    }

    if (temp == NULL) {
        DEBUG_MSG("Cannot find a socket to bind to; errno:" << errno);
        return false;
    }
    freeaddrinfo(result);

    if (p2p_client_fd != 0) {
        std::stringstream ss;
        ss << "FILE_NAME: " << file_name;
        std::streampos size;
        char *buff;
        std::ifstream file(file_name.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            size = file.tellg();
            ss << "FILE_SIZE: " << size << std::endl;
            std::string header = ss.str();
            buff = new char[size + header.length()];
            strcpy(buff, header.c_str());

            file.seekg(0, std::ios::beg);
            file.read(buff, size);
            file.close();

            DEBUG_MSG("Loaded file");
            DEBUG_MSG("Header file " << header);
            util::send_buff(p2p_client_fd, buff, size);
            delete[] buff;
            DEBUG_MSG("Sent file");
            close(p2p_client_fd);
            return true;
        }
    }
    return false;
}

void Client::p2p_client_command(std::string buff, const client_key key, Client::p2p_client_ptype pFile) {
    DEBUG_MSG(buff);
}
