#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/uio.h>
#include "discovery.h"
#include "wrapper.h"
#include "utilities.h"

int notify_time_interval;
in_addr_t *registered_clients;
int registered_clients_num = 0;
int registered_clients_capacity = 0;
pthread_mutex_t registeredClientsLock = PTHREAD_MUTEX_INITIALIZER;
char registrations_filename[] = "registrations.dat";

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("You must pass a value for the notification interval.\n");
        exit(1);
    }
    notify_time_interval = atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE to handle errors directly.
    load_previous_clients();
    pthread_t notificationThread;
    int listen_socket_fd = create_listen_socket(DISCOVERY_PORT, MAX_LISTEN_QUEUE);
    if (pthread_create(&notificationThread, NULL, notify_clients, NULL) != 0)
    {
        printf("Failed to create the notification thread!\n");
        exit(1);
    }
    printf("Listening to incoming connections...\n");
    struct sockaddr_in clientAddress;
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
        int contained = uintcontained(ntohl(clientAddress.sin_addr.s_addr), registered_clients, registered_clients_num);
        if (contained < 0)
        {
            printf("Registering the new peer to the list...\n");
            in_addr_t client_addr = ntohl(clientAddress.sin_addr.s_addr);
            pthread_mutex_lock(&registeredClientsLock);
            register_client_to_list(client_addr);
            write_client_to_file(client_addr);
            pthread_mutex_unlock(&registeredClientsLock);
        }
        pthread_t peer_thread;
        struct sendpeerlist_args *peerlist_args = malloc(sizeof(struct sendpeerlist_args));
        peerlist_args->connectionSocketFD = clientSocketFD;
        peerlist_args->skipElement = contained;
        if (pthread_create(&peer_thread, NULL, send_peer_list, (void*)peerlist_args) == 0)
        {
            // Detach the thread so when it's done, its resources will be cleaned up without having to join.
            pthread_detach(peer_thread);
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
    int registeredClientsNo = registered_clients_num;
    pthread_mutex_unlock(&registeredClientsLock);
    struct sendpeerlist_args peerlist_args = *((struct sendpeerlist_args*)sendpeerlist_args);
    free(sendpeerlist_args);
    if (registeredClientsNo > 0)
    {
        struct iovec *iov;
        int iovcount;
        if (peerlist_args.skipElement >= 0) // This peer was already registered in the list, we send everyone but the requesting peer.
        {
            iov = malloc(sizeof(struct iovec) * 2);
            iovcount = 2;
            ssize_t nwritten;
            iov[0].iov_base = registered_clients;
            iov[0].iov_len = (peerlist_args.skipElement) * sizeof(in_addr_t);
            iov[1].iov_base = &registered_clients[peerlist_args.skipElement + 1];
            iov[1].iov_len = (registeredClientsNo - (peerlist_args.skipElement + 1)) * sizeof(in_addr_t);
        }
        else // This peer is a new peer (it wasn't contained in the list), we send everyone but the last element, which is the new peer itself.
        {
            iov = malloc(sizeof(struct iovec));
            iovcount = 1;
            iov[0].iov_base = registered_clients;
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
        int registeredClientsNo = registered_clients_num;
        pthread_mutex_unlock(&registeredClientsLock);
        for (int i = 0; i < registeredClientsNo; i++)
        {
            if ((connectionSocketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perror_exit("Failed to open the socket", &threadRetVal);
            }
            struct sockaddr_in peerAddress;
            peerAddress.sin_family = AF_INET;
            peerAddress.sin_addr.s_addr = htonl(registered_clients[i]);
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
void load_previous_clients()
{
    if (access(registrations_filename, F_OK) == 0)
    {
        FILE *registrations = fopen(registrations_filename, "r");
        if (registrations != NULL)
        {
            in_addr_t client;
            while (fread(&client, sizeof(in_addr_t), 1, registrations) > 0)
            {
                register_client_to_list(client);
            }
        }
        printf("Successfully loaded {%d} clients from previous sessions.\n", registered_clients_num);
    }
}
void register_client_to_list(in_addr_t client_address)
{
    if (registered_clients_capacity == 0)
    {
        registered_clients_capacity = 128;
        registered_clients = malloc(registered_clients_capacity * sizeof(in_addr_t));
    }
    if (registered_clients_num >= registered_clients_capacity)
    {
        registered_clients_capacity *= 2; // Double the capacity of our array.
        void *temp = realloc(registered_clients, registered_clients_capacity * sizeof(in_addr_t));
        if (temp == NULL)
        {
            printf("Failed to realloc!\n");
            free(registered_clients);
            exit(1);
        }
        registered_clients = temp;
    }
    registered_clients[registered_clients_num] = client_address;
    registered_clients_num++;
}
void write_client_to_file(in_addr_t client_address)
{
    FILE *registrations = fopen(registrations_filename, "a");
    if (registrations != NULL)
    {
        if (fwrite(&client_address, sizeof(in_addr_t), 1, registrations) < 0)
        {
            perror("Failed to write to registrations file.");
        }
    }
    fclose(registrations);
}
    