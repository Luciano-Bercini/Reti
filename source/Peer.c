#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>
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

const int ID_BYTE_SIZE = 64;
const int MAX_LISTEN_QUEUE = 4096;
int send_id_time_interval;
char **generated_ids;
int generated_id_num;
char **received_ids;
int received_id_num;
pthread_mutex_t received_id_lock = PTHREAD_MUTEX_INITIALIZER;
in_addr_t *contact_peers;
int contact_peers_num;
pthread_mutex_t contacts_lock = PTHREAD_MUTEX_INITIALIZER;

char *add_new_id(char ***buffer, int *count, char *id);
char *get_dynamicrand_alphanumID(); 
int is_id_generated(char *id);
int is_peer_in_contacts(in_addr_t peer);
void add_new_contact(in_addr_t peer);
void obtain_discovery_contacts();
void *send_newID_repeating();
void *manage_server_notifications();
void receive_ids(int fd, int num_of_ids);
int check_id_generated(char received_ids[][ID_BYTE_SIZE], int num_of_ids);
//int check_id_generated(char *received_ids, int num_of_ids);

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("You must pass a value for the send id time interval.\n");
        exit(1);
    }
    send_id_time_interval = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to handle errors directly.
    srand(time(NULL));
    obtain_discovery_contacts();
    pthread_t peerThread;
    if (pthread_create(&peerThread, NULL, send_newID_repeating, NULL) != 0)
    {
        printf("Failed to create the thread for sending IDs.\n");
    }
    pthread_t serverThread;
    if (pthread_create(&serverThread, NULL, manage_server_notifications, NULL) != 0)
    {
        printf("Failed to create the new thread for managing server notifications.\n");
    }
    int listen_socket_fd = create_listen_socket(P2P_LISTEN_PORT, MAX_LISTEN_QUEUE);
    int socketFD;
    struct sockaddr_in clientAddress;
    fd_set active_fds;
    fd_set read_fds;
    FD_ZERO(&active_fds); // Initialize to 0 all the bits of the bit field.
    FD_SET(listen_socket_fd, &active_fds); // Adds the listen socket to the set.
    int max_fd = listen_socket_fd;
    while (1)
    {
        read_fds = active_fds; // This is needed as select() will rewrite the bit field.
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) 
        {
            perror_exit("Failed to select");
        }
        for (int i = 0; i <= max_fd; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == listen_socket_fd) // There is a new connection we can accept!
                {
                    socklen_t clientSize = sizeof(clientAddress);
                    if ((socketFD = accept(listen_socket_fd, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
                    {
                        perror_exit("Failed to accept connection");
                    }
                    in_addr_t clientAddr = ntohl(clientAddress.sin_addr.s_addr);
                    int n;
                    if ((n = is_peer_in_contacts(clientAddr)) < 0)
                    {
                        printf("Received a contact from a stranger, adding it to our list of contacts.\n");
                        pthread_mutex_lock(&contacts_lock);
                        add_new_contact(clientAddr);
                        pthread_mutex_unlock(&contacts_lock);
                    }
                    FD_SET(socketFD, &active_fds);
                    if (socketFD > max_fd)
                    {
                        max_fd = socketFD;
                    }
                }
                else // Ready to read!
                {
                    MessageByteLength message_byte_length;
                    read_NBytes(i, &message_byte_length, sizeof(message_byte_length));
                    message_byte_length = ntohl(message_byte_length);
                    int num_of_ids = message_byte_length / ID_BYTE_SIZE;
                    receive_ids(i, num_of_ids);
                    close(i);
                    FD_CLR(i, &active_fds);
                }
            }
        }
    }
    return 0;
}
void obtain_discovery_contacts()
{
    int connectionSocketFD;
    if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror_exit("Failed to open the socket");
    }
    struct sockaddr_in discoveryAddress;
    discoveryAddress.sin_family = AF_INET;
    discoveryAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
    discoveryAddress.sin_port = htons(DISCOVERY_PORT);
    if (connect(connectionSocketFD, (struct sockaddr*)&discoveryAddress, sizeof(discoveryAddress)) < 0)
    {
        perror_exit("Failed to connect with the discovery server");
    }
    MessageByteLength contactListBytes;
    read_NBytes(connectionSocketFD, &contactListBytes, sizeof(contactListBytes)); // Reading the size of the message.
    contactListBytes = ntohl((uint32_t)contactListBytes);
    contact_peers = malloc(contactListBytes);
    read_NBytes(connectionSocketFD, contact_peers, contactListBytes); // Reading the list of contacts.
    contact_peers_num = contactListBytes / sizeof(in_addr_t);
    printf("Obtained the list of other peers (%d) from the discovery server.\n", contact_peers_num);
    close(connectionSocketFD);
}
void *send_newID_repeating()
{
    int socketFD;
    int threadExit;
    while (1)
    {
        pthread_mutex_lock(&contacts_lock);
        int contactsNum = contact_peers_num;
        pthread_mutex_unlock(&contacts_lock);
        char *alphanumID = get_dynamicrand_alphanumID();
        add_new_id(&generated_ids, &generated_id_num, alphanumID);
        for (int i = 0; i < contactsNum; i++)
        {
            if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perror_exit("Failed to open the socket", &threadExit);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(contact_peers[i]);
            peerAddress.sin_port = htons(P2P_LISTEN_PORT);
            char addressASCII[40];
            inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
            if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
            {
                MessageByteLength messageByteLength = htonl(ID_BYTE_SIZE);
                if (write_NBytes(socketFD, &messageByteLength, sizeof(messageByteLength)) < 0)
                {
                    pthread_perror_exit("Failed to write to socket", &threadExit);
                }
                if (write_NBytes(socketFD, alphanumID, ID_BYTE_SIZE) < 0)
                {
                    pthread_perror_exit("Failed to write to socket", &threadExit);
                }
                printf("Sent the ID \"%.5s...\" to the peer at address [%s:%hu].\n", alphanumID, addressASCII, P2P_LISTEN_PORT);
            }
            else
            {
                printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
            }
            close(socketFD);
        }
        sleep(send_id_time_interval); // Wait before generating a new ID.
    }
}
void receive_ids(int fd, int num_of_ids)
{
    char newly_received_ids[num_of_ids][ID_BYTE_SIZE];
    for (int i = 0; i < num_of_ids; i++)
    {
        read_NBytes(fd, newly_received_ids[i], ID_BYTE_SIZE);
    }
    if (num_of_ids == 1)
    {
        pthread_mutex_lock(&received_id_lock);
        add_new_id(&received_ids, &received_id_num, newly_received_ids[0]);
        pthread_mutex_unlock(&received_id_lock);
    }
    else
    {
        int id = check_id_generated(newly_received_ids, num_of_ids);
        if (id >= 0)
        {
            printf("***************************************************************************\n");
            printf("There's a match between your generated IDs and the received ID: \"%.5s...\"\n", generated_ids[id]);
            printf("***************************************************************************\n");
        }
    }
}
/*int check_id_generated(char *received_ids, int num_of_ids)
{
    for (int i = 0; i < generated_id_num; i++)
    {
        for (int j = 0; j < num_of_ids; j++)
        {
            printf("Received ID from list: %s\n", &received_ids[j * ID_BYTE_SIZE]);
            if (strcmp(generated_ids[i], &received_ids[j * ID_BYTE_SIZE]) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}*/
int check_id_generated(char received_ids[][ID_BYTE_SIZE], int num_of_ids)
{
    for (int i = 0; i < generated_id_num; i++)
    {
        for (int j = 0; j < num_of_ids; j++)
        {
            if (strcmp(generated_ids[i], received_ids[j]) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}
void *manage_server_notifications()
{
    int socketFD;
    int listen_socket_fd = create_listen_socket(PEER_DISCOVERY_LISTEN_PORT, MAX_LISTEN_QUEUE);
    struct sockaddr_in serverAddress;
    while (1)
    {
        socklen_t serverSize = sizeof(serverAddress);
        if ((socketFD = accept(listen_socket_fd, (struct sockaddr*)&serverAddress, &serverSize)) < 0)
        {
            perror("Failed to accept connection");
        }
        char server_notification[NOTIFICATION_BYTES];
        read_NBytes(socketFD, server_notification, NOTIFICATION_BYTES);
        pthread_mutex_lock(&received_id_lock);
        int curr_received_ids_num = received_id_num;
        pthread_mutex_unlock(&received_id_lock);
        if (strcmp(server_notification, NOTIFICATION_SEND_LIST) == 0 && curr_received_ids_num > 1)
        {
            pthread_mutex_lock(&contacts_lock);
            int contactsNum = contact_peers_num;
            pthread_mutex_unlock(&contacts_lock);
            int socketFD;
            int threadRetVal;
            int n;
            for (int i = 0; i < contactsNum; i++)
            {
                if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    pthread_perror_exit("Failed to open the socket", &threadRetVal);
                }
                struct sockaddr_in peerAddress;
                peerAddress.sin_family = AF_INET;
                peerAddress.sin_addr.s_addr = htonl(contact_peers[i]);
                peerAddress.sin_port = htons(P2P_LISTEN_PORT);
                char addressASCII[40];
                inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
                if (connect(socketFD, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
                {
                    MessageByteLength messageByteLength = htonl(curr_received_ids_num * ID_BYTE_SIZE);
                    write_NBytes(socketFD, &messageByteLength, sizeof(messageByteLength));
                    for (int i = 0; i < curr_received_ids_num; i++)
                    {
                        write_NBytes(socketFD, received_ids[i], ID_BYTE_SIZE);
                    }
                    printf("Sent our list of contacts to the peer at address [%s:%hu].\n", addressASCII, P2P_LISTEN_PORT);
                }
                else
                {
                    printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
                }
                close(socketFD);
            }
        }
    }
}

int is_peer_in_contacts(in_addr_t peer)
{
    for (int i = 0; i < contact_peers_num; i++)
    {
        if (contact_peers[i] == peer)
        {
            return i;
        }
    }
    return -1;
}
void add_new_contact(in_addr_t peer)
{
    contact_peers = realloc(contact_peers, (contact_peers_num + 1) * sizeof(contact_peers[0]));
    contact_peers[contact_peers_num] = peer;
    contact_peers_num++;
}
char *add_new_id(char ***buffer, int *count, char *id)
{
    *buffer = realloc(*buffer, ((*count) + 1) * sizeof(char*));
    (*buffer)[*count] = malloc(ID_BYTE_SIZE);
    strcpy((*buffer)[*count], id);
    (*count)++;
    return id;
}
char *get_dynamicrand_alphanumID()
{
    char* alphanumID = malloc(ID_BYTE_SIZE);
    alphanumID = rand_alphanumID(alphanumID, ID_BYTE_SIZE);
    return alphanumID;
}