#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
#include "SocketsWrapper.h"

int Socket(int domain, int type, int protocol)
{
    int n;
    if ((n = socket(domain, type, protocol)) < 0) // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    {
        perror("Failed to open the socket");
        exit(1);
    }
    return n;
}