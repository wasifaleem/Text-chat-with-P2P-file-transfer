#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <netdb.h>
#include "ClientInfo.h"

namespace util {
    const bool valid_inet(const std::string ip);
    const std::string get_ip(const sockaddr * addr);
    const std::string get_ip(const sockaddr_storage addr);
    const int sendString(int sock_fd, const std::string data);
    const std::string primary_ip();
    const client_key* get_by_ip(const std::string ip, std::map<client_key, client_info_ptype>);

    const std::string int_to_string(const int i);
    const std::vector<std::string> split_by_space(const std::string s);
}
#endif //UTIL_H