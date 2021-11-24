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

char *addnewID(char ***buffer, int *count, char *id);
char *dynamicrand_alphanumID();
int id_alreadygenerated(char *id);

void *sendnewID()
{
    int socketFD;
    int threadExit;
    while (1)
    {
        char *alphanumID = dynamicrand_alphanumID();
        addnewID(&generatedIDs, &generatedIDNum, alphanumID);
        printf("A new ID has been generated: %s\n", generatedIDs[generatedIDNum - 1]);
        for (int i = 0; i < reachablePeersNum; i++)
        {
            if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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
            if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
            {
                int n;
                if (n = (write(socketFD, alphanumID, ID_BYTE_SIZE)) < 0)
                {
                    pthread_perrorexit("Failed to write to socket", &threadExit);
                }
                printf("Sent the ID %s to my reachable peer.\n", alphanumID);
            }
            else
            {
                printf("Failed to connect with neighbor peer, it might be down.\n");
            }
            shutdown(socketFD, SHUT_WR);
            close(socketFD);
        }
        sleep(ID_TIME_INTERVAL);
    }
}
void *receiveID(void *connectionSocketFD)
{
    int socketFD = *((int*)connectionSocketFD);
    int bytesWritten;
    char idBuff[ID_BYTE_SIZE];
    while ((bytesWritten = read(socketFD, idBuff, ID_BYTE_SIZE)) > 0);
    if (id_alreadygenerated(idBuff) == 0)
    {
        printf("WARNING!!!\nThere's a match between your generated IDs and the received ID %s!\n", idBuff);
    }
    addnewID(&contactIDs, &contacts, idBuff);
    printf("Added a new contact to the list of contacts.\n");
    close(socketFD);
    pthread_exit(0);
}
void *sendcontactsIDs()
{
    int socketFD;
    int threadRetVal;
    int n;
    for (int i = 0; i < reachablePeersNum; i++)
    {
        if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
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
        if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
        {
            for (int j = 0; j < contacts; j++)
            {
                if ((n = (write(socketFD, contactIDs[j], ID_BYTE_SIZE))) < 0)
                {
                    perror("Failed to write to socket");
                }
                printf("Sent the contact ID %s to my neighbor peer.\n", contactIDs[j]);
            }
            shutdown(socketFD, SHUT_WR);
            close(socketFD);
        }
        else
        {
            printf("Couldn't connect with neighbor peer (it might be down).\n");
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
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
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
    printf("Obtained the list of other peers (%d) from the discovery server.\n", reachablePeersNum);
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
        perror("Failed to set REUSE_ADDR option to socket");
    }
    if (bind(listenSocketFD, (struct sockaddr*)&listenAddr, sizeof(listenAddr)) < 0)
    {
        perrorexit("Failed to bind to socket");
    }
    if (listen(listenSocketFD, MAX_PEERS_SIZE) < 0)
    {
        perrorexit("Failed to listen to socket");
    }
    struct sockaddr_in clientAddress;
    pthread_t peerThreads[MAX_PEERS_SIZE];
    char serverNotification[10];
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
            //int n;
            //while ((n = read(connectionSocketFD, serverNotification, sizeof(serverNotification))) > 0);
            pthread_t notificationReceived;
            printf("Received a notification from the discovery server.\n");
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