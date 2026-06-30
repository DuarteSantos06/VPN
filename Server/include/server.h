#pragma once

#include <time.h>


#define MAX_SIZE_BUFF 20000
#define PORT 8080
#define IP "127.0.0.1"
#define MAX_CLIENTS 100

typedef struct client_table
{
    struct sockaddr_in client_address;
    int known;
    time_t last_seen;
    uint32_t virtual_ip;
} client_table;