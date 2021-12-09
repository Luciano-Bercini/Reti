#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/uio.h>
#include "discovery.h"
#include "wrapper.h"
#include "utilities.h"
#include "vector.h"

int notify_time_interval;
vector *registered_clients;
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
    registered_clients = vector_init(sizeof(in_addr_t), 64); // Initialize the vector used to hold the registered clients.
    load_previous_clients();
    pthread_t notification_thread;
    if (pthread_create(&notification_thread, NULL, notify_all_clients, NULL) != 0)
    {
        printf("Failed to create the notification thread!\n");
        exit(1);
    }
    int listen_socket_fd = create_listen_socket(DISCOVERY_PORT, MAX_LISTEN_QUEUE);
    printf("Listening to incoming connections...\n");
    struct sockaddr_in client_addr;
    int client_socket_fd;
    while (1)
    {
        socklen_t client_size = sizeof(client_addr);
        if ((client_socket_fd = accept(listen_socket_fd, (struct sockaddr*)&client_addr, &client_size)) < 0)
        {
            perror("Failed to accept a connection");
            continue;
        }
        char *addr_ASCII = inet_ntoa(client_addr.sin_addr);
        uint port = ntohs(client_addr.sin_port);
        printf("Accepting a new connection with peer [%s:%u].\n", addr_ASCII, port);
        int contained = uintcontained(ntohl(client_addr.sin_addr.s_addr), registered_clients->items, registered_clients->count);
        if (contained < 0)
        {
            printf("Registering the new peer to the list...\n");
            in_addr_t new_client_addr = ntohl(client_addr.sin_addr.s_addr);
            pthread_mutex_lock(&registeredClientsLock);
            vector_append(registered_clients, &new_client_addr);
            write_client_to_file(new_client_addr);
            pthread_mutex_unlock(&registeredClientsLock);
        }
        pthread_t peer_thread;
        struct send_client_list_args *peerlist_args = malloc(sizeof(struct send_client_list_args));
        peerlist_args->connection_socket_fd = client_socket_fd;
        peerlist_args->skip_element = contained;
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
void *send_peer_list(void *send_peer_list_args)
{
    pthread_mutex_lock(&registeredClientsLock);
    int registered_clients_no = registered_clients->count;
    pthread_mutex_unlock(&registeredClientsLock);
    struct send_client_list_args peerlist_args = *((struct send_client_list_args*)send_peer_list_args);
    free(send_peer_list_args);
    if (registered_clients_no > 0)
    {
        struct iovec *iov;
        int iovcount;
        if (peerlist_args.skip_element >= 0) // This peer was already registered in the list, we send everyone but the requesting peer.
        {
            iov = malloc(sizeof(struct iovec) * 2);
            iovcount = 2;
            ssize_t nwritten;
            iov[0].iov_base = (in_addr_t*)vector_get(registered_clients, 0);
            iov[0].iov_len = (peerlist_args.skip_element) * sizeof(in_addr_t);
            iov[1].iov_base = (in_addr_t*)vector_get(registered_clients, peerlist_args.skip_element + 1);
            iov[1].iov_len = (registered_clients_no - (peerlist_args.skip_element + 1)) * sizeof(in_addr_t);
        }
        else // This peer is a new peer (it wasn't contained in the list), we send everyone but the last element, which is the new peer itself.
        {
            iov = malloc(sizeof(struct iovec));
            iovcount = 1;
            iov[0].iov_base = (in_addr_t*)vector_get(registered_clients, 0);
            iov[0].iov_len = (registered_clients_no - 1) * sizeof(in_addr_t);
        }
        int bytes_length = (registered_clients_no - 1) * sizeof(in_addr_t);
        MessageByteLength network_byte_length = htonl((uint32_t)bytes_length);
        if (write_NBytes(peerlist_args.connection_socket_fd, &network_byte_length, sizeof(network_byte_length)) == sizeof(network_byte_length))
        {
            if (writev(peerlist_args.connection_socket_fd, iov, iovcount) < 0)
            {
                perror("Failed to write to socket");
            }
            else
            {
                printf("A list with the {%d} other peers has been sent to the newly registered peer.\n", (registered_clients_no - 1));
            }
        }
        else
        {
            perror("Failed to write the size of the peer list");
        }
        free(iov);
    }
    close(peerlist_args.connection_socket_fd);
}
void *notify_all_clients()
{
    int socket_fd;
    int thread_retval;
    while (1)
    {
        pthread_mutex_lock(&registeredClientsLock);
        int registered_clients_no = registered_clients->count;
        pthread_mutex_unlock(&registeredClientsLock);
        for (int i = 0; i < registered_clients_no; i++)
        {
            if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perror_exit("Failed to open the socket", &thread_retval);
            }
            pthread_t notify_client_thread;
            struct notify_client_args *args = malloc(sizeof(struct notify_client_args));
            args->socket_fd = socket_fd;
            args->client_addr = *((in_addr_t*)vector_get(registered_clients, i));
            if (pthread_create(&notify_client_thread, NULL, notify_client, (void*)args) == 0)
            {
                pthread_detach(notify_client_thread);
            }
            else
            {
                printf("Failed to create a new thread to notify the client.\n");
            }
        }
        sleep(notify_time_interval);
    }
}
void *notify_client(void *notify_args)
{
    struct notify_client_args *args = (struct notify_client_args*)notify_args;
    struct sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_addr.s_addr = htonl(args->client_addr);
    peerAddress.sin_port = htons(PEER_DISCOVERY_LISTEN_PORT);
    char addr_ASCII[46];
    inet_ntop(AF_INET, &peerAddress.sin_addr.s_addr, addr_ASCII, sizeof(addr_ASCII));
    if (connect(args->socket_fd, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
    {
        write_NBytes(args->socket_fd, &NOTIFICATION_SEND_LIST, NOTIFICATION_BYTES);
        printf("Sent a notification to peer at address [%s:%hu].\n", addr_ASCII, PEER_DISCOVERY_LISTEN_PORT);
    }
    else
    {
        printf("Couldn't connect with peer [%s:%hu]: %s.\n", addr_ASCII, PEER_DISCOVERY_LISTEN_PORT, strerror(errno));
    }
    close(args->socket_fd);
    free(args);
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
                vector_append(registered_clients, &client);
            }
        }
        printf("Successfully loaded {%lu} clients from previous sessions.\n", registered_clients->count);
    }
}
void write_client_to_file(in_addr_t client_address)
{
    FILE *registrations = fopen(registrations_filename, "a+");
    if (registrations != NULL)
    {
        if (fwrite(&client_address, sizeof(in_addr_t), 1, registrations) < 0)
        {

            perror("Failed to write to registrations file.");
        }
    }
    fclose(registrations);
}
    