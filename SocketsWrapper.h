#ifndef SOCKETSWRAPPER_H
#define SOCKETSWRAPPER_H

#define DISCOVERY_PORT 30000
#define SERVER_ADDRESS "127.0.0.1"
#define PEER_PORT 30001
#define MAX_PEERS_SIZE 4096

struct PeerContact
{
    in_addr_t address;
    in_port_t port;
};

int Socket(int domain, int type, int protocol);
#endif