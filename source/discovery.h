#ifndef DISCOVERY_H
#define DISCOVERY_H

#define MAX_LISTEN_QUEUE 4096

struct sendpeerlist_args
{
    int connectionSocketFD;
    int skipElement;
};

void *send_peer_list(void *sendpeerlist_args);
void *notify_clients();
void register_client_to_list(in_addr_t client_address);
void write_client_to_file(in_addr_t client_address);
void load_previous_clients();

#endif