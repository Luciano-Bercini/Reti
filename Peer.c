#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // IP addresses conversion utility.
#include <time.h>
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
int ticks = 0;
struct PeerContact peers[MAXPEERLIST];
const int ID_BYTE_SIZE = 64;
const int TIME_INTERVAL = 2;

void *ClientSide()
{
    int n;
    //while ((n = read(connectionSocketFD, peers, sizeof(peers))) > 0)
    {

    }
}
void *ServerSide()
{
    
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
char* addNewID(char *buffer)
{
    generatedIDs = realloc(generatedIDs, sizeof(char*) * (ticks + 1));
    generatedIDs[ticks] = malloc(sizeof(char) * ID_BYTE_SIZE);
    generatedIDs[ticks] = buffer;
}
int main(int argc, char *argv[])
{
    srand(time(NULL));
    int connectionSocketFD;
    struct sockaddr_in discoveryAddress;
    if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open the socket");
        exit(1);
    }
    discoveryAddress.sin_family = AF_INET;
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    discoveryAddress.sin_port = htons(SERVER_PORT);
    if (connect(connectionSocketFD, (struct sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) < 0)
    {
        perror("Failed to connect");
        exit(2);
    }
    int n;
    while ((n = read(connectionSocketFD, peers, sizeof(peers))) > 0);
    printf("Obtained the list of peers from the discovery server.\n");
    shutdown(connectionSocketFD, SHUT_WR);
    close(connectionSocketFD);
    while (1)
    {
        addNewID(dynamicRandomAlphanumID());
        printf("A new ID has been generated: %s\n", generatedIDs[ticks]);
        sleep(TIME_INTERVAL);
        ticks++;

        /*
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
        */
    }
    return 0;
}