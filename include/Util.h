#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include <netdb.h>
#include "ClientInfo.h"

namespace util {
    const bool valid_inet(const std::string ip);
    const std::string get_ip(const sockaddr_storage addr);
    const bool send_string(int sock_fd, const std::string data);
    const bool send_buff(int sock_fd, const char *buff, unsigned long size);
    const std::string primary_ip();
    const client_info_ptype find_by_ip(const std::string ip, std::map<client_key, client_info_ptype>);

    const std::string int_to_string(const int i);
    int str_to_int(const std::string& str);
    long str_to_long(const std::string &str);
    const std::string join_by_space_from_pos(std::vector<std::string> v, int pos);
    const std::vector<std::string> split_by_space(const std::string s);

}
#endif //UTIL_H
