#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/uio.h>
#include <pthread.h>

void perror_exit(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}
void pthread_perror_exit(const char *s, int *retVal)
{
    perror(s);
    pthread_exit(retVal);
}
int create_listen_socket(in_port_t port, int max_listen_queue)
{
    int listenSocketFD;
    struct sockaddr_in listenAddr;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, protocol [0 is default]).
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Accepts connections from any IP address associated to the machine (through its interfaces).
    listenAddr.sin_port = htons(port); // Host TO Network Short" (converts endianess if needed); s for short, l for long.
    const int trueValue = 1;
    // Allow the socket address (IP+port) to be re-used without having to wait for eventual "rogue" packets delays (1-2 mins).
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &trueValue, sizeof(trueValue))) < 0)
    {
        perror("Failed to set REUSE_ADDR option to socket");
    }
    if (bind(listenSocketFD, (struct sockaddr*)&listenAddr, sizeof(listenAddr)) < 0) // Assigns (binds) an IP address to the socket.
    {
        perror_exit("Failed to bind to socket");
    }
    if (listen(listenSocketFD, max_listen_queue) < 0) // Listen for incoming connections requests (up to a queue of X before refusing).
    {
        perror_exit("Failed to listen to socket");
    }
    return listenSocketFD;
}
ssize_t full_read(int fd, void *buff, size_t bytesToRead)
{
    int n;
    size_t bytesRead = 0;
    while (bytesRead < bytesToRead)
    {
        n = read(fd, buff, bytesToRead);
        if (n == 0)
        {
            printf("Failed to read everything: we read less bytes than expected.\n");
            break;
        }
        if (n == -1)
        {
            perror("Failed to read");
            return -1;
        }
        bytesRead += n;
    }
    return bytesRead;
}   
ssize_t full_write(int fd, void *buff, size_t bytes_to_write)
{
    int n;
    size_t bytesWritten = 0;
    while (bytesWritten < bytes_to_write)
    {
        n = write(fd, buff, bytes_to_write);
        if (n == 0)
        {
            printf("Failed to write everything: we write less bytes than expected.\n");
            break;
        }
        if (n == -1)
        {
            perror("Failed to write");
            return -1;
        }
        bytesWritten += n;
    }
    return bytesWritten;
}
ssize_t full_writev(int fd, const struct iovec *iovec, int iov_count, size_t bytes_to_write)
{
    int n;
    size_t bytesWritten = 0;
    while (bytesWritten < bytes_to_write)
    {
        n = writev(fd, iovec, iov_count);
        if (n == 0)
        {
            printf("Failed to write everything: we write less bytes than expected.\n");
            break;
        }
        if (n == -1)
        {
            perror("Failed to write");
            return -1;
        }
        bytesWritten += n;
    }
    return bytesWritten;
}