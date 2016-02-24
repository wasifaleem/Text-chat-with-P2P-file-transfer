#ifndef CLIENTINFO_H
#define CLIENTINFO_H

#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <utility>
#include <deque>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/deque.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>

enum client_status {
    ONLINE, OFFLINE, LOGGED_IN
};

struct client_key {
    std::string ip;
    int sockfd;

    client_key() { }

    client_key(std::string ip, int fd) : ip(ip), sockfd(fd) {
    }

    bool operator<(const client_key &o) const {
        int ip_compare = ip.compare(o.ip);
        return ip_compare < 0 || (ip_compare == 0 && sockfd < o.sockfd);
    }

    bool operator==(const client_key &o) const {
        return ip.compare(o.ip) == 0 && sockfd == o.sockfd;
    }

    friend class boost::serialization::access;

    friend std::ostream &operator<<(std::ostream &os, const client_key &ci) {
        return os << ci.ip << ':' << ci.sockfd;
    }

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar & ip & sockfd;
    }
};

class ClientInfo {
public:

    int sockfd;

    std::string ip, host, client_port, os_port, service;
    client_status status;
    int receive_count, sent_count;

    std::set<std::string> blocked_ips;
    std::deque<std::pair<client_key, std::string> > messages;

    bool operator<(const ClientInfo &c) const {
        return client_port.compare(c.client_port) < 0;
    }

    ClientInfo() { }

    friend class boost::serialization::access;

    friend std::ostream &operator<<(std::ostream &os, const ClientInfo &ci) {
        return os << ci.ip << ci.host << ci.client_port << ci.status;
    }

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar & ip & host & client_port & status & blocked_ips & messages;
    }

    static void serializeTo(std::stringstream *ss, std::map<client_key, ClientInfo *>);

    static std::map<client_key, ClientInfo *> deserializeFrom(std::istringstream *is);

};

typedef ClientInfo *client_info_ptype;

#endif //ASSIGNMENT1_CLIENTINFO_H
