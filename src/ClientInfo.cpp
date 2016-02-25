#include "ClientInfo.h"
#include <boost/archive/text_oarchive.hpp>

void ClientInfo::serialize_to(std::stringstream *ss, std::map<client_key, client_info_ptype> clients) {
    boost::archive::text_oarchive oa(*ss);
    oa << clients;
    (*ss).flush();
}

std::map<client_key, client_info_ptype> ClientInfo::deserialize_from(std::istringstream *is) {
    std::map<client_key, client_info_ptype> clients;
    boost::archive::text_iarchive ia(*is);
    ia >> clients;
    return clients;
}

const char *status(client_status cs) {
    switch (cs) {
        case LOGGED_IN:
            return "online";
        case OFFLINE:
            return "offline";
        default:
            return "UNKNOWN";
    }
}
