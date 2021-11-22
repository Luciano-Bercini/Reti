#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
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

char **generatedIDs;
int generatedIDNum = 0;
char **contactIDs;
int contacts = 0;
in_addr_t reachablePeers[MAX_PEERS_SIZE];
int reachablePeersNum = 0;

const int ID_BYTE_SIZE = 64;
const int ID_TIME_INTERVAL = 5;

char *addnewID(char ***buffer, int *count, char *id)
{
    *buffer = realloc(*buffer, ((*count) + 1) * sizeof(char*));
    *buffer[*count] = malloc(ID_BYTE_SIZE);
    strcpy(*buffer[*count], id);
    (*count)++;
    return id;
}
char *rand_alphanumID(char *buffer, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    if (size > 0)
    {
        size--; // Reserve space for end of string character.
        for (size_t i = 0; i < size - 1; i++)
        {
            int key = rand() % (int)(sizeof(charset) - 1);
            buffer[i] = charset[key];
        }
        buffer[size] = '\0';
    }
    return buffer;
}
char *dynamicrand_alphanumID()
{
    char* alphanumID = malloc(ID_BYTE_SIZE);
    alphanumID = rand_alphanumID(alphanumID, ID_BYTE_SIZE);
    return alphanumID;
}
void *sendnewID()
{
    int connectionSocketFD;
    int threadExit;
    while (1)
    {
        char *alphanumID = dynamicrand_alphanumID();
        addnewID(&generatedIDs, &generatedIDNum, alphanumID);
        printf("A new ID has been generated: %s\n", generatedIDs[generatedIDNum - 1]);
        for (int i = 0; i < reachablePeersNum; i++)
        {
            if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perrorexit("Failed to open the socket", &threadExit);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(reachablePeers[i]);
            peerAddress.sin_port = htons(PEER_PORT);
            char addressASCII[40];
            inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
            printf("Attempting to connect with peer at address [%s:%hu].\n", addressASCII, PEER_PORT); 
            if (connect(connectionSocketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) < 0)
            {
                pthread_perrorexit("Failed to connect", &threadExit);
            }
            int n;
            if (n = (write(connectionSocketFD, alphanumID, ID_BYTE_SIZE)) < 0)
            {
                pthread_perrorexit("Failed to write to socket", &threadExit);
            }
            printf("Sent the ID %s to my reachable peer.\n", alphanumID);
            shutdown(connectionSocketFD, SHUT_WR);
            close(connectionSocketFD); // Closing the connection to our dear client.
        }
        sleep(ID_TIME_INTERVAL);
    }
}
int id_alreadygenerated(char *id)
{
    for (int i = 0; i < generatedIDNum; i++)
    {
        if (strcmp(id, generatedIDs[i]) == 0) // ACCESS HERE -> SEGFAULT
        {
            return 0;
        }
    }
    return -1;
}
void *receiveID(void *connectionSocketFD)
{
    int socketFD = *((int*)connectionSocketFD);
    int bytesWritten;
    char idBuff[ID_BYTE_SIZE];
    while ((bytesWritten = read(socketFD, idBuff, ID_BYTE_SIZE)) > 0);
    if (id_alreadygenerated(idBuff) == 0)
    {
        printf("WARNING!\nYou've been contacted with the id %s!\n", idBuff);
    }
    addnewID(&contactIDs, &contacts, idBuff);
    contacts++;
    printf("Added a new contact to the list of contacts.\n");
    shutdown(socketFD, SHUT_WR);
    close(socketFD);
    pthread_exit(0);
}
void *sendcontactsIDs()
{
    int connectionSocketFD;
    int threadRetVal;
    for (int i = 0; i < reachablePeersNum; i++)
    {
        if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            pthread_perrorexit("Failed to open the socket", &threadRetVal);
        }
        struct sockaddr_in peerAddress;
        peerAddress.sin_family = AF_INET;
        peerAddress.sin_addr.s_addr = htonl(reachablePeers[i]);
        peerAddress.sin_port = htons(PEER_PORT);
        char addressASCII[40];
        inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
        printf("Attempting to connect with peer at address [%s:%hu] to send our list of contacts.\n", addressASCII, PEER_PORT); 
        if (connect(connectionSocketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
        {
            for (int j = 0; j < contacts; j++)
            {
                int n;
                if (n = (write(connectionSocketFD, contactIDs[j], ID_BYTE_SIZE)) < 0)
                {
                    pthread_perrorexit("Failed to write to socket", &threadRetVal);
                }
                printf("Sent the contact ID %s to my neighbor peer.\n", contactIDs[j]);
                shutdown(connectionSocketFD, SHUT_WR);
                close(connectionSocketFD);
            }
        }
        else
        {
            printf("Failed to connect with neighbor peer, it might be down.\n");
        }
    }
}
int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to handle errors directly.
    srand(time(NULL));
    int connectionSocketFD;
    if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perrorexit("Failed to open the socket");
    }
    struct sockaddr_in discoveryAddress;
    discoveryAddress.sin_family = AF_INET;
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS); // Converts string address to binary data in Network Byte Order.
    discoveryAddress.sin_port = htons(DISCOVERY_PORT);
    if (connect(connectionSocketFD, (struct sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) < 0)
    {
        perrorexit("Failed to connect with the discovery server");
    }
    int n;
    reachablePeersNum = 0;
    while ((n = read(connectionSocketFD, reachablePeers, sizeof(reachablePeers))) > 0)
    {
        reachablePeersNum += n;
    }
    reachablePeersNum = reachablePeersNum / sizeof(reachablePeers[0]);
    // Filter the given array that may contain self.
    printf("Obtained the list of peers from the discovery server.\n");
    shutdown(connectionSocketFD, SHUT_WR);
    close(connectionSocketFD);
    pthread_t clientThread;
    if (pthread_create(&clientThread, NULL, sendnewID, NULL) != 0)
    {
        printf("Failed to create the new thread\n");
    }
    int listenSocketFD;
    struct sockaddr_in listenAddr;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any address associated to the listener.
    listenAddr.sin_port = htons(PEER_PORT); // Htons is "Host to Network" (converts endianess if needed); s for short, l for long.
    const int trueValue = 1;
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &trueValue, sizeof(trueValue))) < 0)
    {
        printf("Failed to set REUSE_ADDR option to socket");
    }
    if (bind(listenSocketFD, (struct sockaddr*)&listenAddr, sizeof(listenAddr)) < 0) // Assigns (binds) an IP address to the socket.
    {
        perrorexit("Failed to bind to socket");
    }
    if (listen(listenSocketFD, MAX_PEERS_SIZE) < 0) // Listen for incoming connections requests (up to a queue of 1024 before refusing).
    {
        perrorexit("Failed to listen to socket");
    }
    struct sockaddr_in clientAddress;
    pthread_t peerThreads[MAX_PEERS_SIZE];
    char serverMsg[100];
    int i = 0;
    while (1) // Accept a connection (it may come from other peers or from and the server).
    {
        socklen_t clientSize = sizeof(clientAddress);
        if ((connectionSocketFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perrorexit("Failed to accept connection");
        }
        if (ntohl(clientAddress.sin_addr.s_addr) == ntohl(inet_addr(SERVER_ADDRESS)))
        {
            printf("A new connection with the server has been accepted.\n");
            int n;
            while ((n = read(connectionSocketFD, serverMsg, sizeof(serverMsg))) > 0);
            pthread_t notificationReceived;
            pthread_create(&notificationReceived, NULL, sendcontactsIDs, NULL);
            pthread_join(notificationReceived, NULL);
        }
        else
        {
            printf("A new connection with a peer has been accepted.\n");
            if (pthread_create(&peerThreads[i], NULL, receiveID, (void*)&connectionSocketFD) != 0)
            {
                printf("Failed to create the thread \"receiveID\".\n");
            }
            i++;
            if (i >= MAX_PEERS_SIZE)
            {
                for (int j = 0; j < MAX_PEERS_SIZE; j++)
                {
                    pthread_join(peerThreads[j], NULL);
                }
                i = 0;
            }
        }
    }
    return 0;
}