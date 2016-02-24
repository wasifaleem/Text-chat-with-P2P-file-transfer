#include <../include/logger.h>
#include <../include/global.h>
#include <../include/Util.h>
#include "command.h"
#include <algorithm>

namespace server {
    std::map<std::string, cli_command> build_cli_map() {
        std::map<std::string, cli_command> m;
        m["AUTHOR"] = AUTHOR;
        m["IP"] = IP;
        m["PORT"] = PORT;
        m["LIST"] = LIST;
        m["STATISTICS"] = STATISTICS;
        m["BLOCKED"] = BLOCKED;
        return m;
    }

    std::map<std::string, cli_command> cli = build_cli_map();

    cli_command parse_cli_command(const std::string command_str) {
        if (cli.count(command_str)) {
            return cli[command_str];
        }
        return UNKNOWN;
    }
}

namespace client {
    std::map<std::string, cli_command> build_client_map() {
        std::map<std::string, cli_command> m;
        m["AUTHOR"] = AUTHOR;
        m["IP"] = IP;
        m["PORT"] = PORT;
        m["LIST"] = LIST;
        m["LOGIN"] = LOGIN;
        m["REFRESH"] = REFRESH;
        m["BROADCAST"] = BROADCAST;
        m["SEND"] = SEND;
        m["BLOCK"] = BLOCK;
        m["UNBLOCK"] = UNBLOCK;
        m["LOGOUT"] = LOGOUT;
        m["EXIT"] = EXIT;
        m["SENDFILE"] = SENDFILE;
        return m;
    }

    std::map<std::string, cli_command> c = build_client_map();

    cli_command parse_cli_command(const std::string command_str) {
        if (c.count(command_str)) {
            return c[command_str];
        }
        return UNKNOWN;
    }
}

namespace client_server {
    std::map<std::string, command> build_map() {
        std::map<std::string, command> m;
        m["LOGIN"] = LOGIN;
        m["REFRESH"] = REFRESH;
        m["BROADCAST"] = BROADCAST;
        m["SEND"] = SEND;
        m["BLOCK"] = BLOCK;
        m["UNBLOCK"] = UNBLOCK;
        m["LOGOUT"] = LOGOUT;
        m["EXIT"] = EXIT;
        return m;
    }

    std::map<command, std::string> build_rev_map() {
        std::map<command, std::string> m;
        m[LOGIN] = "LOGIN";
        m[REFRESH] = "REFRESH";
        m[BROADCAST] = "BROADCAST";
        m[SEND] = "SEND";
        m[BLOCK] = "BLOCK";
        m[UNBLOCK] = "UNBLOCK";
        m[LOGOUT] = "LOGOUT";
        m[EXIT] = "EXIT";
        return m;
    }

    std::map<std::string, command> command_map = build_map();
    std::map<command, std::string> command_map_rev = build_rev_map();

    command parse_command(const std::string command_str) {
        if (command_map.count(command_str)) {
            return command_map[command_str];
        }
        return UNKNOWN;
    }

    std::string command_str(const command c) {
        if (command_map_rev.count(c)) {
            return command_map_rev[c];
        }
        return "UNKNOWN";
    }
}

void author(std::string command) {
    SUCCESS(command);
    cse4589_print_and_log("I, %s, have read and understood the course academic integrity policy.\n",
                          UBIT_NAME);
    END(command);
}

void ip(std::string command) {
    std::string ip = util::primary_ip();
    if (!ip.empty()) {
        SUCCESS(command);
        cse4589_print_and_log("IP:%s\n", ip.c_str());
    } else {
        ERROR(command);
    }
    END(command);
}

void port_command(std::string command, const char *port) {
    SUCCESS(command);
    cse4589_print_and_log("PORT:%s\n", port);
    END(command);
}

void list_command(std::string command, std::map<client_key, client_info_ptype> clients) {
    SUCCESS(command);
    if (!clients.empty()) {
        std::vector<ClientInfo> clients_v;
        for (std::map<client_key, client_info_ptype>::const_iterator it = clients.begin();
             it != clients.end(); ++it) {
            if ((*(it->second)).status == LOGGED_IN) {
                clients_v.push_back(*(it->second));
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
    END(command);
}

void relay(std::string from, std::string to, std::string msg) {
    RELAYED_SUCCESS
    cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", from.c_str(), to.c_str(), msg.c_str());
    RELAYED_END
}

void received(std::string from, std::string msg) {
    RECEIVED_SUCCESS
    cse4589_print_and_log("msg from:%s\n[msg]:%s\n", from.c_str(), msg.c_str());
    RECEIVED_END
}
