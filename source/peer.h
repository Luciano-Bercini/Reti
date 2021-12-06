#ifndef PEER_H
#define PEER_H

#define ID_BYTE_SIZE 64
#define SINGLE_ID 0
#define MAX_LISTEN_QUEUE 4096

char *add_new_id(char ***buffer, int *count, char *id);
char *get_dynamicrand_alphanumID(); 
int is_id_generated(char *id);
int is_peer_in_contacts(in_addr_t peer);
void add_new_contact(in_addr_t peer);
void obtain_discovery_contacts();
void *send_new_id_repeating();
void *manage_server_notifications();
void send_id_list_to_contacts();
void receive_single_id(int fd);
void receive_id_list(int fd, int num_of_ids);
int check_id_matches(char received_ids[][ID_BYTE_SIZE], int num_of_ids);

#endif