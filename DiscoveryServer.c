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
#include "SocketsWrapper.h"

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
/*
Il P2P con Discovery Server = possiede un server centrale chiamato, appunto, Discovery,
il quale utente (ovvero il Peer che in questo caso funge da client) comunica la propria esistenza al momento dell'avvio e 
riceve in risposta una lista con gli altri nomi della rete. Con essa l'utente può interrogare qualunque partecipante per conoscerne i contenuti.
Quindi, quando l'utente necessita di un contenuto, prima contatta il server individualmente, poi inoltra la richiesta.
*/
int clientNum = 0;
//pthread_mutex_t clientNumLock = PTHREAD_MUTEX_INITIALIZER;
struct PeerContact peersContacts[MAXPEERLIST];

void *HandleClient(void *connectionSocketFD)
{
    int socketFD = *((int*)connectionSocketFD);
    if (write(socketFD, peersContacts, sizeof(peersContacts[0]) * (clientNum - 1)) < 0)
    {
        perror("Failed to write");
        exit(6);
    }
    printf("A list with %d peers has been sent to the new peer.\n", clientNum - 1);
    shutdown(socketFD, SHUT_WR);
    close(socketFD); // Closing the connection to our dear client.
    printf("Closing the connection with the client.\n");
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    int listenSocketFD;
    int connectionSocketFD;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    int maxConnectionsQueue = MAXPEERLIST;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any address associated to the server.
    serverAddress.sin_port = htons(SERVER_PORT); // Htons is "Host to Network" (converts endianess if needed); s for short, l for long.
    int enable = 1;
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int))) < 0)
    {
        perror("Failed to set option to socket");
        exit(10);
    }
    if (bind(listenSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Assigns (binds) an IP address to the socket.
    {
        perror("Failed to bind the socket");
        exit(2);
    }
    if (listen(listenSocketFD, maxConnectionsQueue) < 0) // Listen for incoming connections requests (up to a queue of 1024 before refusing).
    {
        perror("Failed to listen");
        exit(3);
    }
    printf("Listening to incoming connections.\n");
    pthread_t peerThreads[MAXPEERLIST];
    int i = 0;
    while (1)
    {
        // Accept a connection from the listen queue and creates a new socket to communicate with the client.
        // Second and third argument help identify the client, they can be NULL.
        socklen_t clientSize = sizeof(clientAddress);
        if ((connectionSocketFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perror("Failed to accept connection");
            exit(4);
        }
        char *addrASCII = inet_ntoa(clientAddress.sin_addr);
        uint port = ntohs(clientAddress.sin_port);
        printf("Accepting a new connection with peer [%s:%u].\n", addrASCII, port);
        printf("Registering the new peer to the list...\n");
        peersContacts[clientNum].address = clientAddress.sin_addr.s_addr;
        peersContacts[clientNum].port = clientAddress.sin_port;
        clientNum++;
        if (pthread_create(&peerThreads[i++], NULL, HandleClient, (void*)&connectionSocketFD) != 0)
        {
            printf("Failed to create the new thread\n");
        }
        if (i >= MAXPEERLIST)
        {
            for (int j = 0; j < MAXPEERLIST; j++)
            {
                pthread_join(peerThreads[j], NULL);
            }
            i = 0;
        }
    }
    return 0;
}