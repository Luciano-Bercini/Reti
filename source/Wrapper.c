#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
#include <pthread.h>
#include "Wrapper.h"

void perrorexit(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}
void pthread_perrorexit(const char *s, int *retVal)
{
    perror(s);
    pthread_exit(retVal);
}