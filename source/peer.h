#ifndef PEER_H
#define PEER_H

#define ID_BYTE_SIZE 64
#define SINGLE_ID 0
#define MAX_LISTEN_QUEUE 4096

struct send_single_id_args
{
    int socket_fd;
    in_addr_t contact_peer;
    char alphanum_id[ID_BYTE_SIZE];
};
struct send_id_list_args
{
    int socket_fd;
    in_addr_t contact_peer;
    int curr_received_ids_num;
};

// Perform initialization procedures.
void initialize();
// Checks if the passed id was already generated.
int is_id_generated(char *id);
// Checks if the passed peer is already in the contacts.
int is_peer_in_contacts(in_addr_t peer);
// Obtains the list of contacts from the discovery server.
void obtain_discovery_contacts();
// Periodically generate and send a new id to known peers.
void *send_new_id_repeating();
// Sends a single id to a peer.
void *send_single_id(void *send_args);
// Sends the received id list to all contacts.
void send_id_list_to_contacts();
// Send an id list to a peer.
void *send_id_list(void *send_args);
void *manage_server_notifications();
void receive_single_id(int fd);
void receive_id_list(int fd, int num_of_ids);
// Returns the number of matches between the generated ids and the given received ids up to num_of_ids.
int check_id_matches(char received_ids[][ID_BYTE_SIZE], int num_of_ids);

#endif