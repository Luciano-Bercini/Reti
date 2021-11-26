#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
#include <errno.h>
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


const int MAX_LISTEN_QUEUE = 4096;

char **generatedIDs;
int generatedIDNum = 0;
char **receivedIDs;
int receivedIDNum = 0;

in_addr_t *contactPeers;
int contactPeersNum = 0;
pthread_mutex_t contactsLock = PTHREAD_MUTEX_INITIALIZER;

const int ID_BYTE_SIZE = 64;
const int ID_TIME_INTERVAL = 5;

char *addnewID(char ***buffer, int *count, char *id);
char *dynamicrand_alphanumID();
int id_alreadygenerated(char *id);
int peer_in_contacts(in_addr_t peer);
void add_new_contact(in_addr_t peer);
void readNBytes(int fd, void *buff, size_t bytesToRead);

void *send_newID_repeating();
void *receiveID(void *socketFD);
void *sendcontactsIDs();

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
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    discoveryAddress.sin_port = htons(DISCOVERY_PORT);
    if (connect(connectionSocketFD, (struct sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) < 0)
    {
        perrorexit("Failed to connect with the discovery server");
    }
    ContactsListByteLength contactListBytes;
    readNBytes(connectionSocketFD, &contactListBytes, sizeof(contactListBytes)); // Reading the size of the message.
    contactListBytes = ntohl((uint32_t)contactListBytes);
    contactPeers = malloc(contactListBytes);
    readNBytes(connectionSocketFD, contactPeers, contactListBytes); // Reading the list of contacts.
    contactPeersNum = contactListBytes / sizeof(in_addr_t);
    printf("Obtained the list of other peers (%d) from the discovery server.\n", contactPeersNum);
    close(connectionSocketFD);
    pthread_t clientThread;
    if (pthread_create(&clientThread, NULL, send_newID_repeating, NULL) != 0)
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
        perror("Failed to set REUSE_ADDR option to socket");
    }
    if (bind(listenSocketFD, (struct sockaddr*)&listenAddr, sizeof(listenAddr)) < 0)
    {
        perrorexit("Failed to bind to socket");
    }
    if (listen(listenSocketFD, MAX_LISTEN_QUEUE) < 0)
    {
        perrorexit("Failed to listen to socket");
    }
    struct sockaddr_in clientAddress;
    pthread_t peerThreads[MAX_LISTEN_QUEUE];
    int i = 0;
    while (1) // Accept a connection (it may come from other peers or from and the server).
    {
        socklen_t clientSize = sizeof(clientAddress);
        if ((connectionSocketFD = accept(listenSocketFD, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
        {
            perrorexit("Failed to accept connection");
        }
        in_addr_t clientAddr = ntohl(clientAddress.sin_addr.s_addr);
        if (clientAddr == ntohl(inet_addr(SERVER_ADDRESS)))
        {
            char server_notification[NOTIFICATION_BYTES];
            readNBytes(connectionSocketFD, server_notification, NOTIFICATION_BYTES);
            if (strcmp(server_notification, NOTIFICATION_SEND_LIST) == 0)
            {
                pthread_t notificationReceived;
                printf("Received the notification from the server, sending our list of contacts.\n");
                pthread_create(&notificationReceived, NULL, sendcontactsIDs, NULL);
                pthread_join(notificationReceived, NULL);
            }
        }
        else
        {
            int n;
            if ((n = peer_in_contacts(clientAddr)) < 0)
            {
                printf("Received a contact from a stranger, adding it to our list of contacts.\n");
                pthread_mutex_lock(&contactsLock);
                add_new_contact(clientAddr);
                pthread_mutex_unlock(&contactsLock);
            }
            if (pthread_create(&peerThreads[i], NULL, receiveID, (void*)&connectionSocketFD) != 0)
            {
                printf("Failed to create the thread \"receiveID\".\n");
            }
            i++;
            if (i >= MAX_LISTEN_QUEUE)
            {
                for (int j = 0; j < MAX_LISTEN_QUEUE; j++)
                {
                    pthread_join(peerThreads[j], NULL);
                }
                i = 0;
            }
        }
    }
    return 0;
}
void *send_newID_repeating()
{
    int socketFD;
    int threadExit;
    while (1)
    {
        pthread_mutex_lock(&contactsLock);
        int contactsNum = contactPeersNum;
        pthread_mutex_unlock(&contactsLock);
        char *alphanumID = dynamicrand_alphanumID();
        addnewID(&generatedIDs, &generatedIDNum, alphanumID);
        for (int i = 0; i < contactsNum; i++)
        {
            if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perrorexit("Failed to open the socket", &threadExit);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(contactPeers[i]);
            peerAddress.sin_port = htons(PEER_PORT);
            char addressASCII[40];
            inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
            if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
            {
                int n;
                if (n = (write(socketFD, alphanumID, ID_BYTE_SIZE)) < 0)
                {
                    pthread_perrorexit("Failed to write to socket", &threadExit);
                }
                printf("Sent the ID \"%.5s...\" to the peer at address [%s:%hu].\n", alphanumID, addressASCII, PEER_PORT);
            }
            else
            {
                printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
            }
            shutdown(socketFD, SHUT_WR);
            close(socketFD);
            sleep(ID_TIME_INTERVAL);
        }
        sleep(ID_TIME_INTERVAL); // Further wait before generating a new ID.
    }
}
void *receiveID(void *fd)
{
    int socketFD = *((int*)fd);
    int bytesWritten;
    char idBuff[ID_BYTE_SIZE];
    while ((bytesWritten = read(socketFD, idBuff, ID_BYTE_SIZE)) > 0);
    if (id_alreadygenerated(idBuff) == 0)
    {
        printf("WARNING!!!\nThere's a match between your generated IDs and the received ID: \"%.5s...\"\n", idBuff);
    }
    addnewID(&receivedIDs, &receivedIDNum, idBuff);
    close(socketFD);
    pthread_exit(0);
}
void *sendcontactsIDs()
{
    pthread_mutex_lock(&contactsLock);
    int contactsNum = contactPeersNum;
    pthread_mutex_unlock(&contactsLock);
    int socketFD;
    int threadRetVal;
    int n;
    for (int i = 0; i < contactsNum; i++)
    {
        if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            pthread_perrorexit("Failed to open the socket", &threadRetVal);
        }
        struct sockaddr_in peerAddress;
        peerAddress.sin_family = AF_INET;
        peerAddress.sin_addr.s_addr = htonl(contactPeers[i]);
        peerAddress.sin_port = htons(PEER_PORT);
        char addressASCII[40];
        inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
        if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
        {
            for (int j = 0; j < receivedIDNum; j++)
            {
                if ((n = (write(socketFD, receivedIDs[j], ID_BYTE_SIZE))) < 0)
                {
                    perror("Failed to write to socket");
                }
            }
            printf("Sent our list of contacts to the peer at address [%s:%hu].\n", addressASCII, PEER_PORT); 
            shutdown(socketFD, SHUT_WR);
            close(socketFD);
        }
        else
        {
            printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
        }
    }
}


int peer_in_contacts(in_addr_t peer)
{
    for (int i = 0; i < contactPeersNum; i++)
    {
        if (contactPeers[i] == peer)
        {
            return i;
        }
    }
    return -1;
}
void add_new_contact(in_addr_t peer)
{
    contactPeers = realloc(contactPeers, (contactPeersNum + 1) * sizeof(contactPeers[0]));
    contactPeers[contactPeersNum] = peer;
    contactPeersNum++;
}
char *addnewID(char ***buffer, int *count, char *id)
{
    *buffer = realloc(*buffer, ((*count) + 1) * sizeof(char*));
    (*buffer)[*count] = malloc(ID_BYTE_SIZE);
    strcpy((*buffer)[*count], id);
    (*count)++;
    return id;
}
char *dynamicrand_alphanumID()
{
    char* alphanumID = malloc(ID_BYTE_SIZE);
    alphanumID = rand_alphanumID(alphanumID, ID_BYTE_SIZE);
    return alphanumID;
}
int id_alreadygenerated(char *id)
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
void readNBytes(int fd, void *buff, size_t bytesToRead)
{
    int n;
    int bytesRead = 0;
    while (bytesRead < bytesToRead)
    {
        n = read(fd, buff, bytesToRead);
        if (n < 0)
        {
            perror("Failed to read");
        }
        if (n == 0)
        {
            printf("Failed to read everything: we read less bytes than expected.\n");
        }
        bytesRead += n;
    }
}