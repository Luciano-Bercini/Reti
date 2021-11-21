#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
#include <netdb.h>
#include <pthread.h>
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

const int NOTIFY_TIME_INTERVAL = 10;
int registeredClientsNum = 0;
in_addr_t registeredClients[MAX_PEERS_SIZE];

void *sendPeerListToClient(void *connectionSocketFD)
{
    int socketFD = *((int*)connectionSocketFD);
    if (registeredClientsNum > 0)
    {
        int bytesWritten;
        if ((bytesWritten = write(socketFD, registeredClients, sizeof(registeredClients[0]) * (registeredClientsNum - 1))) < 0)
        {
            printf("Failed to write to socket.\n");
        }
        else
        {
            printf("A list with %d other peers has been sent to the newly registered peer.\n", registeredClientsNum - 1);
        }
    }
    shutdown(socketFD, SHUT_WR);
    close(socketFD);
    printf("Closed the connection with our dear client.\n");
}
void *notifyClients()
{
    int connectionSocketFD;
    int threadRetVal;
    char *notificationMsg = "A\n";
    while (1)
    {
        for (int i = 0; i < registeredClientsNum; i++)
        {
            if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perrorexit("Failed to open the socket", &threadRetVal);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(registeredClients[i]);
            peerAddress.sin_port = htons(PEER_PORT);
            char addressASCII[40];
            inet_ntop(AF_INET, &peerAddress.sin_addr.s_addr, addressASCII, sizeof(addressASCII));
            if (connect(connectionSocketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
            {
                int n;
                if (n = (write(connectionSocketFD, notificationMsg, strlen(notificationMsg))) < 0)
                {
                    pthread_perrorexit("Failed to write to socket", &threadRetVal);
                }
                printf("Sent a notification to peer at address [%s:%hu].\n", addressASCII, PEER_PORT);
                shutdown(connectionSocketFD, SHUT_WR);
                close(connectionSocketFD); // Closing the connection with our dear client.
              
            }
            else
            {
                printf("Failed to connect with peer [%s:%hu], the peer might be down.\n", addressASCII, PEER_PORT);
            }

        }
        sleep(NOTIFY_TIME_INTERVAL);
    }
}
int main(int argc, char *argv[])
{
    int listenSocketFD;
    int connectionSocketFD;
    struct sockaddr_in serverAddress;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any address associated to the server.
    serverAddress.sin_port = htons(DISCOVERY_PORT); // Htons is "Host to Network" (converts endianess if needed); s for short, l for long.
    int enable = 1;
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) < 0)
    {
        perrorexit("Failed to set option to socket");
    }
    if (bind(listenSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Assigns (binds) an IP address to the socket.
    {
        perrorexit("Failed to bind the socket");
    }
    if (listen(listenSocketFD, MAX_PEERS_SIZE) < 0) // Listen for incoming connections requests (up to a queue of 1024 before refusing).
    {
        perrorexit("Failed to listen");
    }
    printf("Setting up the notification service...\n");
    pthread_t notificationThread;
    if (pthread_create(&notificationThread, NULL, notifyClients, NULL) != 0)
    {
        printf("Failed to create the notification thread!\n");
    }
    printf("Listening to incoming connections...\n");
    struct sockaddr_in clientAddress;
    pthread_t peerThreads[MAX_PEERS_SIZE];
    int threadIndex = 0;
    while (1)
    {
        // Accept a connection from the listen queue and creates a new socket to communicate with the client; second and third argument help identify the client.
        socklen_t clientSize = sizeof(clientAddress);
        if ((connectionSocketFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perrorexit("Failed to accept connection");
        }
        char *addrASCII = inet_ntoa(clientAddress.sin_addr);
        uint port = ntohs(clientAddress.sin_port);
        printf("Accepting a new connection with peer [%s:%u].\n", addrASCII, port);
        if (uintcontained(ntohl(clientAddress.sin_addr.s_addr), registeredClients, registeredClientsNum) != 0)
        {
            printf("Registering the new peer to the list...\n");
            registeredClients[registeredClientsNum] = ntohl(clientAddress.sin_addr.s_addr);
            registeredClientsNum++;
        }
        if (pthread_create(&peerThreads[threadIndex++], NULL, sendPeerListToClient, (void*)&connectionSocketFD) != 0)
        {
            printf("Failed to create the new thread.\n");
        }
        if (threadIndex >= MAX_PEERS_SIZE)
        {
            for (int j = 0; j < MAX_PEERS_SIZE; j++)
            {
                pthread_join(peerThreads[j], NULL);
            }
            threadIndex = 0;
        }
    }
    return 0;
}