#include "../include/Util.h"
#include "../include/global.h"
#include <errno.h>
#include <arpa/inet.h>
#include <cstring>

namespace util {
    const bool valid_inet(const std::string ip) {
        struct sockaddr_in sa;
        return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
    }

    const std::string get_ip(const sockaddr_storage addr) {
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

    const bool send_string(int sock_fd, const std::string data) {
        return send_buff(sock_fd, data.c_str(), data.length());
    }

    const bool send_buff(int sock_fd, const char *buf, unsigned long size) {
        unsigned long total = 0;
        unsigned long bytes_remaining = size;
        ssize_t n = 0;

        while (total < size) {
            n = send(sock_fd, buf + total, bytes_remaining, 0);
            if (n == -1) { break; }
            total += n;
            bytes_remaining -= n;
        }

        return n != -1;
    }

    const std::string primary_ip() {
        int sock_fd = 0;
        struct addrinfo *result, *temp;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;

        int getaddrinfo_result;
        if ((getaddrinfo_result = getaddrinfo("8.8.8.8", "53", &hints, &result)) != 0) {
            DEBUG_LOG("getaddrinfo: " << gai_strerror(getaddrinfo_result));
            return EMPTY_STRING;
        }

        for (temp = result; temp != NULL; temp = temp->ai_next) {
            sock_fd = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
            if (sock_fd == -1) {
                close(sock_fd);
                continue;
            }
            break;
        }

        if (temp == NULL) {
            DEBUG_LOG("Cannot create a socket; errno:" << errno);
            return EMPTY_STRING;
        }

        freeaddrinfo(result);

        if (connect(sock_fd, temp->ai_addr, temp->ai_addrlen) == -1) {
            DEBUG_LOG("Cannot connect on: " << sock_fd << "; errno: " << errno);
            return EMPTY_STRING;
        }

        struct sockaddr_storage in;
        socklen_t in_len = sizeof(in);

        if (getsockname(sock_fd, (struct sockaddr *) &in, &in_len) == -1) {
            DEBUG_LOG("Cannot getsockname on: " << sock_fd << "; errno: " << errno);
            return EMPTY_STRING;
        }

        if (sock_fd != 0) {
            close(sock_fd);
        }
        return get_ip(in);
    }

    const std::vector<std::string> split_by_space(const std::string s) {
        std::stringstream ss(s);
        std::istream_iterator<std::string> begin(ss);
        std::istream_iterator<std::string> end;
        std::vector<std::string> v(begin, end);
        return v;
    }

    const std::string int_to_string(const int i) {
        std::stringstream ss;
        ss << i;
        return ss.str();
    }

    int str_to_int(const std::string &str) {
        int i;
        std::istringstream ss(str);
        ss.imbue(std::locale::classic());
        ss >> i;
        return i;
    }


    long str_to_long(const std::string &str) {
        long l;
        std::istringstream ss(str);
        ss.imbue(std::locale::classic());
        ss >> l;
        return l;
    }

    const std::string join_by_space_from_pos(std::vector<std::string> v, int pos) {
        std::stringstream ss;
        int j = 0;
        for (std::vector<std::string>::const_iterator i = (v.begin() + (pos)); i != v.end(); ++i) {
            if (j == 0) {
                ss << *i;
            } else {
                ss << SPACE << *i;
            }
            j++;
        }
        return ss.str();
    }

    const client_info_ptype find_by_ip(const std::string ip, std::map<client_key, client_info_ptype> clients) {
        for (std::map<client_key, client_info_ptype>::iterator it = clients.begin();
             it != clients.end(); ++it) {
            if (ip.compare((it->first).ip) == 0) {
                return it->second;
            }
        };
        return NULL;
    }
}