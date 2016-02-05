#ifndef SERVER_H
#define SERVER_H


class Server {
private:
    const int port;

    class ClientInfo {
    private:

    };

public:
    Server(int port);

    void start();
};


#endif //SERVER_H
