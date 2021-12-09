#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "peer.h"
#include "wrapper.h"
#include "utilities.h"
#include "vector.h"

int send_id_time_interval;
char **generated_ids;
int generated_id_num;
pthread_mutex_t generated_id_lock = PTHREAD_MUTEX_INITIALIZER;
char **received_ids;
int received_id_num;
pthread_mutex_t received_id_lock = PTHREAD_MUTEX_INITIALIZER;
vector *contact_peers;
pthread_mutex_t contacts_lock = PTHREAD_MUTEX_INITIALIZER;

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
    if (pthread_create(&peerThread, NULL, send_new_id_repeating, NULL) != 0)
    {
        printf("Failed to create the thread for sending IDs.\n");
    }
    pthread_t serverThread;
    if (pthread_create(&serverThread, NULL, manage_server_notifications, NULL) != 0)
    {
        printf("Failed to create the new thread for managing server notifications.\n");
    }
    int listen_socket_fd = create_listen_socket(P2P_LISTEN_PORT, MAX_LISTEN_QUEUE);
    int socket_fd;
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
                    if ((socket_fd = accept(listen_socket_fd, (struct sockaddr*)&clientAddress, &clientSize)) < 0)
                    {
                        perror_exit("Failed to accept connection");
                    }
                    in_addr_t clientAddr = ntohl(clientAddress.sin_addr.s_addr);
                    int n;
                    if ((n = is_peer_in_contacts(clientAddr)) < 0)
                    {
                        printf("Received a contact from a stranger, adding it to our list of contacts.\n");
                        pthread_mutex_lock(&contacts_lock);
                        vector_append(contact_peers, &clientAddr);
                        pthread_mutex_unlock(&contacts_lock);
                    }
                    FD_SET(socket_fd, &active_fds);
                    if (socket_fd > max_fd)
                    {
                        max_fd = socket_fd;
                    }
                }
                else // Ready to read!
                {
                    MessageByteLength message_byte_length;
                    full_read(i, &message_byte_length, sizeof(message_byte_length));
                    message_byte_length = ntohl(message_byte_length);
                    if (message_byte_length == SINGLE_ID) // Special value (0) to distinguish from single id and list of ids.
                    {
                        receive_single_id(i);
                    }
                    else
                    {
                        int num_of_ids = message_byte_length / ID_BYTE_SIZE;
                        receive_id_list(i, num_of_ids);
                    }
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
    full_read(connectionSocketFD, &contactListBytes, sizeof(contactListBytes));
    contactListBytes = ntohl((uint32_t)contactListBytes);
    int contacts_num = contactListBytes / sizeof(in_addr_t);
    contact_peers = vector_init(sizeof(in_addr_t), max(contacts_num, 32));
    full_read(connectionSocketFD, contact_peers->items, contactListBytes);
    contact_peers->count = contacts_num;
    printf("Obtained the list of other peers (%d) from the discovery server.\n", contacts_num);
    close(connectionSocketFD);
}
void *send_new_id_repeating()
{
    int socketFD;
    int threadExit;
    while (1)
    {
        pthread_mutex_lock(&contacts_lock);
        int contactsNum = contact_peers->count;
        pthread_mutex_unlock(&contacts_lock);
        char alphanum_id[ID_BYTE_SIZE];
        rand_alphanumID(alphanum_id, ID_BYTE_SIZE);
        pthread_mutex_lock(&generated_id_lock);
        add_new_id(&generated_ids, &generated_id_num, alphanum_id);
        pthread_mutex_unlock(&generated_id_lock);
        for (int i = 0; i < contactsNum; i++)
        {
            if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                pthread_perror_exit("Failed to open the socket", &threadExit);
            }
            pthread_t peer_thread;
            struct send_single_id_args *args = malloc(sizeof(struct send_single_id_args));
            args->socket_fd = socketFD;
            args->contact_peer = *((in_addr_t*)vector_get(contact_peers, i));
            strcpy(args->alphanum_id, alphanum_id);
            if (pthread_create(&peer_thread, NULL, send_single_id, (void*)args) == 0)
            {
                pthread_detach(peer_thread);
            }
            else
            {
                printf("Failed to create a new thread to handle the client.\n");
            }
        }
        sleep(send_id_time_interval); // Wait before generating a new ID.
    }
}
void *send_single_id(void *send_args)
{
    struct send_single_id_args args = *((struct send_single_id_args*)send_args);
    free(send_args);
    struct sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_addr.s_addr = htonl(args.contact_peer);
    peerAddress.sin_port = htons(P2P_LISTEN_PORT);
    char addressASCII[46];
    inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
    if (connect(args.socket_fd, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
    {
        MessageByteLength messageByteLength = htonl(SINGLE_ID);
        if (full_write(args.socket_fd, &messageByteLength, sizeof(messageByteLength)) == sizeof(MessageByteLength))
        {
            if (full_write(args.socket_fd, args.alphanum_id, ID_BYTE_SIZE) == ID_BYTE_SIZE)
            {
                printf("Sent the ID \"%.5s...\" to the peer at address [%s:%hu].\n", args.alphanum_id, addressASCII, P2P_LISTEN_PORT);
            }
        }
    }
    else
    {
        printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
    }
    close(args.socket_fd);
}
void *manage_server_notifications()
{
    int socket_fd;
    int listen_socket_fd = create_listen_socket(PEER_DISCOVERY_LISTEN_PORT, MAX_LISTEN_QUEUE);
    struct sockaddr_in serverAddress;
    while (1)
    {
        socklen_t serverSize = sizeof(serverAddress);
        if ((socket_fd = accept(listen_socket_fd, (struct sockaddr*)&serverAddress, &serverSize)) < 0)
        {
            perror("Failed to accept connection");
        }
        char server_notification[NOTIFICATION_BYTES];
        full_read(socket_fd, server_notification, NOTIFICATION_BYTES);
        close(socket_fd);
        if (strcmp(server_notification, NOTIFICATION_SEND_LIST) == 0) // We've received a notification that asks us to send our list of contacts.
        {
            send_id_list_to_contacts();
        }
    }
}
void send_id_list_to_contacts()
{
    pthread_mutex_lock(&received_id_lock);
    int curr_received_ids_num = received_id_num;
    pthread_mutex_unlock(&received_id_lock);
    if (curr_received_ids_num == 0) // Guard clause in case we have no contacts to send (i.e. no one contacted us yet).
    {
        return; 
    }
    pthread_mutex_lock(&contacts_lock);
    int contactsNum = contact_peers->count;
    pthread_mutex_unlock(&contacts_lock);
    int socket_fd;
    int threadRetVal;
    for (int i = 0; i < contactsNum; i++)
    {
        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            pthread_perror_exit("Failed to open the socket", &threadRetVal);
        }
        pthread_t peer_thread;
        struct send_id_list_args *args = malloc(sizeof(struct send_id_list_args));
        args->socket_fd = socket_fd;
        args->contact_peer = *((in_addr_t*)vector_get(contact_peers, i));
        args->curr_received_ids_num = curr_received_ids_num;
        if (pthread_create(&peer_thread, NULL, send_id_list, (void*)args) == 0)
        {
            pthread_detach(peer_thread);
        }
        else
        {
            printf("Failed to create a new thread to handle the client.\n");
        }
    }
}
void *send_id_list(void *send_args)
{
    struct send_id_list_args args = *((struct send_id_list_args*)send_args);
    free(send_args);
    struct sockaddr_in peerAddress;
    peerAddress.sin_family = AF_INET;
    peerAddress.sin_addr.s_addr = htonl(args.contact_peer);
    peerAddress.sin_port = htons(P2P_LISTEN_PORT);
    char addressASCII[46];
    inet_ntop(AF_INET, &peerAddress.sin_addr, addressASCII, sizeof(addressASCII));
    if (connect(args.socket_fd, (struct sockaddr*)&peerAddress, sizeof(peerAddress)) == 0)
    {
        MessageByteLength messageByteLength = htonl(args.curr_received_ids_num * ID_BYTE_SIZE);
        if (full_write(args.socket_fd, &messageByteLength, sizeof(messageByteLength)) == sizeof(messageByteLength))
        {
            for (int i = 0; i < args.curr_received_ids_num; i++)
            {
                full_write(args.socket_fd, received_ids[i], ID_BYTE_SIZE);
            }
            printf("Sent our list of contacts to the peer at address [%s:%hu].\n", addressASCII, P2P_LISTEN_PORT);
        }
    }
    else
    {
        printf("Couldn't connect with neighbor peer: %s.\n", strerror(errno));
    }
    close(args.socket_fd);
}
void receive_single_id(int fd)
{
    char newly_received_id[ID_BYTE_SIZE];
    full_read(fd, newly_received_id, ID_BYTE_SIZE);
    pthread_mutex_lock(&received_id_lock);
    add_new_id(&received_ids, &received_id_num, newly_received_id);
    pthread_mutex_unlock(&received_id_lock);
}
void receive_id_list(int fd, int num_of_ids)
{
    char newly_received_ids[num_of_ids][ID_BYTE_SIZE];
    for (int i = 0; i < num_of_ids; i++)
    {
        full_read(fd, newly_received_ids[i], ID_BYTE_SIZE);
    }
    int matches = check_id_matches(newly_received_ids, num_of_ids);
    if (matches > 0)
    {
        printf("*************************************************************************\n");
        // The number of matches can be lower or higher depending on the peer that sent us the list.
        printf("There are {%d} matches between your generated IDs and the received contacts.\n", matches);
        printf("*************************************************************************\n");
    }
}
int check_id_matches(char received_ids[][ID_BYTE_SIZE], int num_of_ids)
{
    int matches = 0;
    pthread_mutex_lock(&generated_id_lock);
    int ids_generated = generated_id_num;
    pthread_mutex_unlock(&generated_id_lock);
    for (int i = 0; i < ids_generated; i++)
    {
        for (int j = 0; j < num_of_ids; j++)
        {
            if (strcmp(generated_ids[i], received_ids[j]) == 0)
            {
                matches++;
            }
        }
    }
    return matches;
}
int is_peer_in_contacts(in_addr_t peer)
{
    for (int i = 0; i < contact_peers->count; i++)
    {
        if (*((in_addr_t*)vector_get(contact_peers, i)) == peer)
        {
            return i;
        }
    }
    return -1;
}
char *add_new_id(char ***buffer, int *count, char *id)
{
    *buffer = realloc(*buffer, ((*count) + 1) * sizeof(char*));
    (*buffer)[*count] = malloc(ID_BYTE_SIZE);
    strcpy((*buffer)[*count], id);
    (*count)++;
    return id;
}