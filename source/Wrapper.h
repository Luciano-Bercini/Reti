#ifndef WRAPPER_H
#define WRAPPER_H

#define SERVER_ADDRESS "10.0.0.1"
#define DISCOVERY_PORT 30000
#define P2P_LISTEN_PORT 30001
#define PEER_DISCOVERY_LISTEN_PORT 30002
#define NOTIFICATION_BYTES 2
#define NOTIFICATION_SEND_LIST "A"

// Defines a 32 bit unsigned integer to handle the exchange of messages lengths.
typedef unsigned int MessageByteLength;

void perror_exit(const char *s);
void pthread_perror_exit(const char *s, int *retVal);
int create_listen_socket(in_port_t port, int max_listen_queue);
ssize_t read_NBytes(int fd, void *buff, size_t bytesToRead);
ssize_t write_NBytes(int fd, void *buff, size_t bytesToWrite);
#endif