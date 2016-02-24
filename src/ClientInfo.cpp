#include "ClientInfo.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <iostream>
#include <sstream>

void ClientInfo::serializeTo(std::stringstream *ss, std::map<client_key, client_info_ptype> clients) {
    boost::archive::text_oarchive oa(*ss);
    oa << clients;
    (*ss).flush();
}

std::map<client_key, client_info_ptype> ClientInfo::deserializeFrom(std::istringstream *is) {
    std::map<client_key, client_info_ptype> clients;
    boost::archive::text_iarchive ia(*is);
    ia >> clients;
    return clients;
}

