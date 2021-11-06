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

char **generatedIDs;
char **contactIDs;
int ticks = 0;
int contacts = 0;
int peersRead = 0;
struct PeerContact peers[MAX_PEERS_SIZE];
const int ID_BYTE_SIZE = 64;
const int TIME_INTERVAL = 5;

char* addNewID(char **buffer, int *count, char *id)
{
    buffer = realloc(buffer, sizeof(char*) * (ticks + 1));
    buffer[ticks] = malloc(sizeof(char) * ID_BYTE_SIZE);
    buffer[ticks] = id;
    count++;
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
void *ClientSide()
{
    int connectionSocketFD;
    int threadExit;
    if (peersRead > 0)
    {
        while (1)
        {
            for (int i = 0; i < peersRead; i++)
            {
                char* alphanumID = dynamicRandomAlphanumID();
                addNewID(generatedIDs, &ticks, alphanumID);
                printf("A new ID has been generated: %s\n", alphanumID);
                if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    perror("Failed to open the socket");
                    pthread_exit(&threadExit);
                }
                struct sockaddr_in peerAddress;
                peerAddress.sin_family = AF_INET;
                peerAddress.sin_addr.s_addr = peers[i].address;
                peerAddress.sin_port = ntohs(PEER_PORT);
                if (connect(connectionSocketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) < 0)
                {
                    perror("Failed to connect"); // We fail here!
                    pthread_exit(&threadExit);
                }
                int n;
                if (n = (write(connectionSocketFD, alphanumID, ID_BYTE_SIZE)) < 0)
                {
                    printf("%d\n", n);
                    perror("Failed to write");
                    pthread_exit(&threadExit);
                }
                printf("Sent the ID %s to my neighbor peer.\n", alphanumID);
                shutdown(connectionSocketFD, SHUT_WR);
                close(connectionSocketFD); // Closing the connection to our dear client.
                sleep(TIME_INTERVAL);
            }
        }
    }
}
void *ServerSide(void *connectionSocketFD)
{
    int socketFD = *((int*)connectionSocketFD);
    int bytesWritten;
    char idBuff[ID_BYTE_SIZE];
    if ((bytesWritten = read(socketFD, idBuff, sizeof(ID_BYTE_SIZE))) < 0)
    {
        perror("Failed to read");
        exit(6);
    }
    addNewID(contactIDs, &contacts, idBuff);
    contacts++;
    printf("Added a new contact to the list of contacts.\n");
    shutdown(socketFD, SHUT_WR);
    close(socketFD); // Closing the connection to our dear client.
    printf("Closing the connection with the client.\n");
    pthread_exit(0);
}
int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to handle errors directly.
    srand(time(NULL));
    int connectionSocketFD;
    if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open the socket");
        exit(1);
    }
    struct sockaddr_in discoveryAddress;
    discoveryAddress.sin_family = AF_INET;
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    discoveryAddress.sin_port = htons(DISCOVERY_PORT);
    if (connect(connectionSocketFD, (struct sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) < 0)
    {
        perror("Failed to connect");
        exit(2);
    }
    int n;
    peersRead = 0;
    while ((n = read(connectionSocketFD, peers, sizeof(peers))) > 0)
    {
        peersRead += n;
    }
    peersRead = peersRead / sizeof(peers[0]);
    printf("Obtained the list of peers from the discovery server.\n");
    shutdown(connectionSocketFD, SHUT_WR);
    close(connectionSocketFD);
    pthread_t clientThread;
    if (pthread_create(&clientThread, NULL, ClientSide, NULL) != 0)
    {
        printf("Failed to create the new thread\n");
    }
    int listenSocketFD;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Domain (family), type, and protocol (Internet, TCP, specify protocol - 0 is default).
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any address associated to the listener.
    serverAddress.sin_port = htons(PEER_PORT); // Htons is "Host to Network" (converts endianess if needed); s for short, l for long.
    const int trueValue = 1;
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEADDR, &trueValue, sizeof(trueValue))) < 0)
    {
        perror("Failed to set REUSE_ADDR option to socket");
        exit(10);
    }
    if ((setsockopt(listenSocketFD, SOL_SOCKET, SO_REUSEPORT, &trueValue, sizeof(trueValue))) < 0)
    {
        perror("Failed to set REUSE_PORT option to socket");
        exit(11);
    }
    // Multiple listening TCP sockets, all bound to the same port, can co-exist, provided they are all bound to different local IP addresses.
    if (bind(listenSocketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Assigns (binds) an IP address to the socket.
    {
        perror("Failed to bind the socket");
        exit(2);
    }
    if (listen(listenSocketFD, MAX_PEERS_SIZE) < 0) // Listen for incoming connections requests (up to a queue of 1024 before refusing).
    {
        perror("Failed to listen");
        exit(3);
    }
    pthread_t peerThreads[MAX_PEERS_SIZE];
    int i = 0;
    while (1)
    {
        socklen_t clientSize = sizeof(clientAddress);
        if ((connectionSocketFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perror("Failed to accept connection");
            exit(4);
        }
        printf("A new connection has been accepted!\n");
        if (pthread_create(&peerThreads[i++], NULL, ServerSide, (void*)&connectionSocketFD) != 0)
        {
            printf("Failed to create the new thread\n");
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
    return 0;
}