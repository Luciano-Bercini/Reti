#ifndef WRAPPER_H
#define WRAPPER_H

#define DISCOVERY_PORT 30000
#define SERVER_ADDRESS "10.0.0.1"
#define PEER_PORT 30001
#define MAX_PEERS_SIZE 4096

// Defines a 32 bit unsigned integer to handle the exchange of the contact list between the discovery server and the clients.
typedef unsigned int ContactsListByteLength;

void perrorexit(const char *s);
void pthread_perrorexit(const char *s, int *retVal);
#endif