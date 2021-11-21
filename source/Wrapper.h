#include <sys/socket.h>
#include <arpa/inet.h> 

#ifndef SOCKETSWRAPPER_H
#define SOCKETSWRAPPER_H

#define DISCOVERY_PORT 30000
#define SERVER_ADDRESS "10.0.0.1"
#define PEER_PORT 30001
#define MAX_PEERS_SIZE 4096

/*struct PeerContact
{
    in_addr_t address;
    in_port_t port;
};*/

void perrorexit(const char *s);
void pthread_perrorexit(const char *s, int *retVal);
#endif