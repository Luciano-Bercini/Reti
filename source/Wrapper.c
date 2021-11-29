#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include "Wrapper.h"

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