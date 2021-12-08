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

void *send_peer_list(void *send_client_list_args);
void *notify_all_clients();
void *notify_client(void *notify_args);
void write_client_to_file(in_addr_t client_address);
void load_previous_clients();

#endif