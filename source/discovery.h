#ifndef DISCOVERY_H
#define DISCOVERY_H

#define MAX_LISTEN_QUEUE 4096

struct send_client_list_args
{
    int connection_socket_fd;
    int skip_element;
};
struct notify_client_args
{
    int socket_fd;
    in_addr_t client_addr;
};

// Sends the peer list to a requesting client.
void *send_peer_list(void *send_client_list_args);
// Sends a notification to all clients.
void *notify_all_clients();
// Send a notification to the given client.
void *notify_client(void *notify_args);
// Writes the given client address to file.
void write_client_to_file(in_addr_t client_address);
// Loads all the previous clients from file and stores them in the vector.
void load_previous_clients();

#endif