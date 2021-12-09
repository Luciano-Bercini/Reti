#ifndef WRAPPER_H
#define WRAPPER_H

#define SERVER_ADDRESS "10.0.0.1"
#define DISCOVERY_PORT 30000
#define P2P_LISTEN_PORT 30001
#define PEER_DISCOVERY_LISTEN_PORT 30002
#define NOTIFICATION_BYTES 2
#define NOTIFICATION_SEND_LIST "A"

// Defines a 32 bit unsigned integer to handle the exchange of messages lengths.
typedef uint32_t MessageByteLength;

// Perror with the given message and calls exit().
void perror_exit(const char *s);
// Perror with the given message and calls pthread_exit().
void pthread_perror_exit(const char *s, int *ret_val);
// Creates a listen socket with the given port and max queue.
int create_listen_socket(in_port_t port, int max_listen_queue);
// Full read with proper error-checking and returns the number of bytes read.
ssize_t full_read(int fd, void *buff, size_t bytes_to_read);
// Full write with proper error-checking and returns the number of bytes written.
ssize_t full_write(int fd, void *buff, size_t bytes_to_write);
// Full writev with proper error-checking and returns the number of bytes written.
ssize_t full_writev(int fd, const struct iovec *iovec, int iov_count, size_t bytes_to_write);

#endif