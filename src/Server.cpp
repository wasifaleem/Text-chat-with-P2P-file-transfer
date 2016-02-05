#include <iostream>
#include "Server.h"

Server::Server(int portNum) : port(portNum) { }

void Server::start() {
    std::cout << "Server started at: " << port << std::endl;

}
