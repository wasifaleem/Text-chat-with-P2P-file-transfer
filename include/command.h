#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <map>

enum command {
    // client & server

    AUTHOR,
    IP,
    PORT,
    LIST,

    // server only

    STATISTICS,
    BLOCKED,

    // client only

    LOGIN,
    REFRESH,
    BROADCAST,
    BLOCK,
    UNBLOCK,
    LOGOUT,
    EXIT,
    SENDFILE,

    UNKNOWN
};

std::map<std::string, command> create_map() {
    std::map<std::string, command> m;
    m["AUTHOR"] = AUTHOR;
    m["IP"] = IP;
    m["PORT"] = PORT;
    m["LIST"] = LIST;
    m["STATISTICS"] = STATISTICS;
    m["BLOCKED"] = BLOCKED;
    m["LOGIN"] = LOGIN;
    m["REFRESH"] = REFRESH;
    m["BROADCAST"] = BROADCAST;
    m["BLOCK"] = BLOCK;
    m["UNBLOCK"] = UNBLOCK;
    m["LOGOUT"] = LOGOUT;
    m["EXIT"] = EXIT;
    m["SENDFILE"] = SENDFILE;
    return m;
}

std::map<std::string, command> m = create_map();

inline command parse_command(const std::string command_str) {
    if (m.count(command_str)) {
        return m[command_str];
    }
    return UNKNOWN;
}

#endif //COMMAND_H
