#ifndef WRAPPER_H
#define WRAPPER_H

#define SERVER_ADDRESS "10.0.0.1"
#define DISCOVERY_PORT 30000
#define P2P_LISTEN_PORT 30001
#define PEER_DISCOVERY_LISTEN_PORT 30002
#define NOTIFICATION_BYTES 2
#define NOTIFICATION_SEND_LIST "A"

// Defines a 32 bit unsigned integer to handle the exchange of the contact list between the discovery server and the clients.
typedef unsigned int ContactsListByteLength;

void perrorexit(const char *s);
void pthread_perrorexit(const char *s, int *retVal);
#endif