#ifndef SOCKETSWRAPPER_H
#define SOCKETSWRAPPER_H

#define SERVER_PORT 30000
#define SERVER_ADDRESS "127.0.0.1"
#define MAXPEERLIST 4096

struct PeerContact
{
    in_addr_t address;
    in_port_t port;
};

int Socket(int domain, int type, int protocol);
#endif