#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include "Wrapper.h"
#include "Utilities.h"

/*
Progettare ed implementare un’applicazione p2p per il tracciamento dei contatti.
Ogni peer, ad intervalli di tempo regolari, invia ai peer raggiungibili un id alfanumerico di 64 byte generato in maniera random e riceve gli id
dei peer da cui è raggiungibile. 
Ogni peer conserva gli id generati e quelli dei peer con cui è venuto in contatto.
Per entrare nella rete, ogni peer deve registrarsi presso un server che svolge anche un servizio di notifica.
Se un peer riceve una notifica dal server, invia a tutti gli altri peer la lista dei peer con cui è venuto in contatto.
Un peer che riceve una lista di contatti verifica se è presente un proprio id e nel caso mostra un messaggio all’utente.
I peer devono comunicare direttamente tra di loro senza il tramite del server.
*/

const int MAX_LISTEN_QUEUE = 4096;
int notify_time_interval;
in_addr_t *registeredClients;
int registeredClientsNum = 0;
pthread_mutex_t registeredClientsLock = PTHREAD_MUTEX_INITIALIZER;

void *send_peer_list(void *sendpeerlist_args);
void *notify_clients();

struct sendpeerlist_args
{
    int connectionSocketFD;
    int skipElement;
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("You must pass a value for the notification interval.\n");
        exit(1);
    }
    notify_time_interval = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to handle errors directly.
    pthread_t notificationThread;
    int listen_socket_fd = create_listen_socket(DISCOVERY_PORT, MAX_LISTEN_QUEUE);
    if (pthread_create(&notificationThread, NULL, notify_clients, NULL) != 0)
    {
        printf("Failed to create the notification thread!\n");
        exit(1);
    }
    printf("Listening to incoming connections...\n");
    struct sockaddr_in clientAddress;
    pthread_t peerThreads[MAX_LISTEN_QUEUE];
    struct sendpeerlist_args peerlist_args[MAX_LISTEN_QUEUE];
    int threadIndex = 0;
    int clientSocketFD;
    while (1)
    {
        socklen_t clientSize = sizeof(clientAddress);
        // Accept a connection from the listen queue and creates a new socket to comunicate; second and third argument help identify the client.
        if ((clientSocketFD = accept(listen_socket_fd, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perror("Failed to accept a connection");
            continue;
        }
        char *addrASCII = inet_ntoa(clientAddress.sin_addr);
        uint port = ntohs(clientAddress.sin_port);
        printf("Accepting a new connection with peer [%s:%u].\n", addrASCII, port);
        int contained = uintcontained(ntohl(clientAddress.sin_addr.s_addr), registeredClients, registeredClientsNum);
        if (contained < 0)
        {
            printf("Registering the new peer to the list...\n");
            pthread_mutex_lock(&registeredClientsLock);
            registeredClients = realloc(registeredClients, (registeredClientsNum + 1) * sizeof(in_addr_t));
            registeredClients[registeredClientsNum] = ntohl(clientAddress.sin_addr.s_addr);
            registeredClientsNum++;
            pthread_mutex_unlock(&registeredClientsLock);
        }
        peerlist_args[threadIndex].connectionSocketFD = clientSocketFD;
        peerlist_args[threadIndex].skipElement = contained;
        if (pthread_create(&peerThreads[threadIndex], NULL, send_peer_list, (void*)&peerlist_args[threadIndex]) == 0)
        {
            threadIndex++;
            if (threadIndex >= MAX_LISTEN_QUEUE) // Handle the situation in which we have too many threads running (or terminated, waiting the join).
            {
                for (int j = 0; j < MAX_LISTEN_QUEUE; j++)
                {
                    pthread_join(peerThreads[j], NULL);
                }
                threadIndex = 0;
            }
        }
        else
        {
            printf("Failed to create a new thread to handle the client.\n");
        }
    }
    return 0;
}
void *send_peer_list(void *sendpeerlist_args)
{
    pthread_mutex_lock(&registeredClientsLock);
    int registeredClientsNo = registeredClientsNum;
    pthread_mutex_unlock(&registeredClientsLock);
    struct sendpeerlist_args peerlist_args = *((struct sendpeerlist_args*)sendpeerlist_args);
    if (registeredClientsNo > 0)
    {
        struct iovec *iov;
        int iovcount;
        if (peerlist_args.skipElement >= 0) // This peer was already registered in the list, we send everyone but the requesting peer.
        {
            iov = malloc(sizeof(struct iovec) * 2);
            iovcount = 2;
            ssize_t nwritten;
            iov[0].iov_base = registeredClients;
            iov[0].iov_len = (peerlist_args.skipElement) * sizeof(in_addr_t);
            iov[1].iov_base = &registeredClients[peerlist_args.skipElement + 1];
            iov[1].iov_len = (registeredClientsNo - (peerlist_args.skipElement + 1)) * sizeof(in_addr_t);
        }
        else // This peer is a new peer (it wasn't contained in the list), we send everyone but the last element, which is the new peer itself.
        {
            iov = malloc(sizeof(struct iovec));
            iovcount = 1;
            iov[0].iov_base = registeredClients;
            iov[0].iov_len = (registeredClientsNo - 1) * sizeof(in_addr_t);
        }
        int bytesWritten;
        int bytesLength = (registeredClientsNo - 1) * sizeof(in_addr_t);
        MessageByteLength networkByteLength = htonl((uint32_t)bytesLength);
        if (write_NBytes(peerlist_args.connectionSocketFD, &networkByteLength, sizeof(networkByteLength)) == sizeof(networkByteLength))
        {
            if ((bytesWritten = writev(peerlist_args.connectionSocketFD, iov, iovcount)) < 0)
            {
                perror("Failed to write to socket");
            }
            else
            {
                printf("A list with the {%d} other peers has been sent to the newly registered peer.\n", (registeredClientsNo - 1));
            }
        }
        else
        {
            perror("Failed to write the size of the peer list");
        }
        free(iov);
    }
    close(peerlist_args.connectionSocketFD);
}
void *notify_clients()
{
    int connectionSocketFD;
    int threadRetVal;
    char addressASCII[40];
    while (1)
    {
        pthread_mutex_lock(&registeredClientsLock);
        int registeredClientsNo = registeredClientsNum;
        pthread_mutex_unlock(&registeredClientsLock);
        for (int i = 0; i < registeredClientsNo; i++)
        {
            if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perror_exit("Failed to open the socket", &threadRetVal);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(registeredClients[i]);
            peerAddress.sin_port = htons(PEER_DISCOVERY_LISTEN_PORT);
            inet_ntop(AF_INET, &peerAddress.sin_addr.s_addr, addressASCII, sizeof(addressASCII));
            if (connect(connectionSocketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
            {
                write_NBytes(connectionSocketFD, &NOTIFICATION_SEND_LIST, NOTIFICATION_BYTES);
                printf("Sent a notification to peer at address [%s:%hu].\n", addressASCII, PEER_DISCOVERY_LISTEN_PORT);
            }
            else
            {
                printf("Couldn't connect with peer [%s:%hu]: %s.\n", addressASCII, PEER_DISCOVERY_LISTEN_PORT, strerror(errno));
            }
            close(connectionSocketFD);
        }
        sleep(notify_time_interval);
    }
}