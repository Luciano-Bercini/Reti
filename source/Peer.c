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

char* addNewID(char **buffer, int *count, char *id)
{
    buffer = realloc(buffer, sizeof(char*) * (*count + 1));
    buffer[*count] = malloc(sizeof(char) * ID_BYTE_SIZE);
    buffer[*count] = id;
    (*count)++;
}
char *randomAlphanumID(char *buffer, size_t size)
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
char *dynamicRandomAlphanumID()
{
    char* alphanumID = malloc(sizeof(char) * ID_BYTE_SIZE);
    alphanumID = randomAlphanumID(alphanumID, ID_BYTE_SIZE);
    return alphanumID;
}
void *sendNewIDs()
{
    int connectionSocketFD;
    int threadExit;
    while (1)
    {
        char* alphanumID = dynamicRandomAlphanumID();
        addNewID(generatedIDs, &generatedIDNum, alphanumID);
        printf("A new ID has been generated: %s\n", alphanumID);
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
int isIDInGeneratedIDs(char *id)
{
    for (int i = 0; i < generatedIDNum; i++)
    {
        if (strcmp(id, generatedIDs[i]) == 0)
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
    while ((bytesWritten = read(socketFD, idBuff, sizeof(ID_BYTE_SIZE))) > 0);
    printf("ID BUFF %s\n",idBuff);
    if (isIDInGeneratedIDs(idBuff) == 0)
    {
        printf("Sei stato in contatto con l'id %s.\n", idBuff);
    }
    addNewID(contactIDs, &contacts, idBuff);
    contacts++;
    printf("Added a new contact to the list of contacts.\n");
    shutdown(socketFD, SHUT_WR);
    close(socketFD); // Closing the connection to our dear client.
    printf("Closing the connection with the client.\n");
    pthread_exit(0);
}
void *sendContactsIDs()
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
                printf("YOLO %s\n", contactIDs[j]);
                if (n = (write(connectionSocketFD, contactIDs[j], ID_BYTE_SIZE)) < 0)
                {
                    pthread_perrorexit("Failed to write to socket", &threadRetVal);
                }
                printf("Sent the contact ID %s to my neighbor peer.\n", contactIDs[j]);
                shutdown(connectionSocketFD, SHUT_WR);
                close(connectionSocketFD); // Closing the connection to our dear client.
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
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS); // Returns the address in Network Byte Order.
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
    printf("Obtained the list of peers from the discovery server.\n");
    shutdown(connectionSocketFD, SHUT_WR);
    close(connectionSocketFD);
    pthread_t clientThread;
    if (pthread_create(&clientThread, NULL, sendNewIDs, NULL) != 0)
    {
        printf("Failed to create the new thread\n");
    }
    int listenSocketFD;
    struct sockaddr_in serverAddress;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any address associated to the listener.
    serverAddress.sin_port = htons(PEER_PORT); // Htons is "Host to Network" (converts endianess if needed); s for short, l for long.
    const int trueValue = 1;
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &trueValue, sizeof(trueValue))) < 0)
    {
        printf("Failed to set REUSE_ADDR option to socket");
    }
    // Multiple listening TCP sockets, all bound to the same port, can co-exist, provided they are all bound to different local IP addresses.
    if (bind(listenSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Assigns (binds) an IP address to the socket.
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
            pthread_create(&notificationReceived, NULL, sendContactsIDs, NULL);
            pthread_join(notificationReceived, NULL);
        }
        else
        {
            printf("A new connection with a peer has been accepted.\n");
            if (pthread_create(&peerThreads[i++], NULL, receiveID, (void*)&connectionSocketFD) != 0)
            {
                printf("Failed to create the thread \"receiveID\".\n");
            }
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