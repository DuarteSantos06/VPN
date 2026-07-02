#include <fcntl.h>      /* O_RDWR */
#include <stdio.h>      /* perror(), printf(), fprintf() */
#include <stdlib.h>     /* exit(), malloc(), free() */
#include <string.h>     /* memset(), memcpy() */
#include <sys/ioctl.h>  /* ioctl() */
#include <unistd.h>     /* read(), close() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, htons() */
#include <arpa/inet.h>  /* inet_ntoa() */
#include <netinet/ip.h>

/* includes for struct ifreq, etc */
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/types.h>
/* includes from my project */
#include "server.h"

#define MSG_HANDSHAKE_REQ 1
#define MSG_HANDSHAKE_ACK 2
#define MSG_DATA 3
#define MSG_CLOSE_CONNECTION 4
#define MSG_KEEP_ALIVE 5

typedef struct {
    uint8_t type;          // Tipo de mensagem (Handshake ou Dados)
    uint32_t virtual_ip;   // O IP virtual do cliente (útil no handshake)
    // No futuro podes adicionar aqui: uint32_t seq_number, etc.
} vpn_header_t;


struct client_table clients[MAX_CLIENTS];

/*
 * This functions looks for the client in our table, if it finds the client returns the position
 * if it does not find returns -1
*/
int find_client(struct sockaddr_in *client_address)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].known && clients[i].client_address.sin_addr.s_addr == client_address->sin_addr.s_addr && clients[i].client_address.sin_port == client_address->sin_port)
        {
            return i;
        }
    }
    return -1;
}

/*
 * This function adds a client to our table, if the table is already full returns -1 
*/
int add_client(struct sockaddr_in *client_address)
{
    for(int i = 0 ; i < MAX_CLIENTS ; i ++)
    {
        if( clients[i].known==0)
        {
            memset(&clients[i], 0, sizeof(struct client_table));
            clients[i].client_address = *client_address;
            clients[i].virtual_ip = virtual_ip;
            clients[i].known = 1;
            clients[i].last_seen = time(NULL);
            return i;
        }
    }
    return -1;
}

/*
 * This function cleans the table, it "removes" the clients who didn't send anything in the las 30 seconds
*/
void clean_table()
{
    for(int i = 0 ; i< MAX_CLIENTS ; i++)
    {
        if(clients[i].known && (time(NULL)-clients[i].last_seen)>30)
        {
            memset(&clients[i],0,sizeof(clients[i]));
        }
    }
}


/*
 * This function allocates a tun file, and connects with an interface, does not receive any parameters
 * and return -1 in case of an error
 */
int alloc_tun()
{
    char *devname;
    devname = "tun1";
    struct ifreq ifr;
    int tun_fd, err;

    if ((tun_fd = open("/dev/net/tun", O_RDWR)) == -1)
    {
        perror("open /dev/net/tun");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN| IFF_NO_PI;
    strncpy(ifr.ifr_name, devname, IFNAMSIZ);

    /*
     * Creates a connection between the kernel and the code (tun_fd)
     */
    if ((err = ioctl(tun_fd, TUNSETIFF, (void *)&ifr)) == -1)
    {
        perror("ioctl TUNSETIFF");
        close(tun_fd);
        return -1;
    }
    return tun_fd;
}

/*
 * This function creates a socket, binds it to the port and returns the socket file descriptor
 */
int create_socket(struct sockaddr_in *server_address)
{
    int server_socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (server_socket_fd < 0)
    {
        perror("Socket:");
        return -1;
    }

    server_address->sin_family = AF_INET;
    /* Accepts any Interface */
    server_address->sin_addr.s_addr = INADDR_ANY;
    server_address->sin_port = htons(PORT);

    if (bind(server_socket_fd, (struct sockaddr *)server_address, sizeof(struct sockaddr)) < 0)
    {
        perror("Bind:");
        return -1;
    }

    return server_socket_fd;
}

/*
 * This function reads from the client to the buffer passed as a parameter
 * reutnr the number of bytes read or -1 in case of an error
 */
int read_from_client(int server_socket_fd, struct sockaddr_in *client_address, char *buffer, size_t buffer_len)
{
    int bytes_read;
    socklen_t addr_len = sizeof(*client_address);
    if ((bytes_read = recvfrom(server_socket_fd, buffer, buffer_len, 0, (struct sockaddr *)client_address, &addr_len))<0)
    {
        perror("recvfrom:");
        return -1;
    }

    int idx = find_client(client_address);
    if (idx == -1) {
        add_client(client_address);
    } else {
        clients[idx].last_seen = time(NULL);  
    }
    return bytes_read;
}

/*
 * This function reads from tun driver and wirtes to the buffer passed as a parameter
 * return the number of bytes read, or -1 in case of an error
 */
int read_from_tun(int tun_fdf, char *buffer, size_t max_size_buffer)
{
    int bytes_read;
    if ((bytes_read = read(tun_fdf, buffer, max_size_buffer)) < 0)
    {
        perror("Reading from tun:");
        return -1;
    }
    return bytes_read;
}

/*
 * This function sends to the tun the content in the buffer
 * Returns the number of bytes sent or -1 in case off an error
 */
int send_to_tun(int tun_fd, char *buffer, size_t max_size_buffer)
{
    int bytes_sent;
    if ((bytes_sent = write(tun_fd, buffer, max_size_buffer)) < 0)
    {
        perror("Writing to tun:");
        return -1;
    }
    return bytes_sent;
}

/*
 * This function send to the client the buffer passed as a parameter
 * returns the number of bytes sent or -1 in case of an error
 */
int send_to_the_client(int server_socket_fd, struct sockaddr_in *client_address, char *buffer, size_t buffer_len)
{
    int bytes_sent;
    if ((bytes_sent = sendto(server_socket_fd, buffer,buffer_len,0, (struct sockaddr *)client_address, sizeof(*client_address))) < 0)
    {
        perror("Sendto:");
        return -1;
    }
    return bytes_sent;
}

/*
 * this function is responsible for the hanshake with the client
*/
void hanshake_with_client(struct sockaddr_in *client_address,int server_socket_fd,char *buffer,vpn_header_t *data_header)
{
    int idx=find_client(client_address);
    if(idx==-1)
    {
        add_client(client_address);
    }
    if(idx!=-1)
    {
        clients[idx].virtual_ip = data_header->virtual_ip;
    }

    vpn_header_t ack_header;
    ack_header.type=MSG_HANDSHAKE_ACK;
    ack_header.virtual_ip=0;
    printf("[HANDSHAKE] Cliente ligado com sucesso!\n");
    char ack_buffer[sizeof(vpn_header_t)];

    memcpy(ack_buffer,&ack_header,sizeof(vpn_header_t));
    send_to_the_client(server_socket_fd, client_address, buffer, sizeof(vpn_header_t));
}


/*
 * Main loop function, handles the select and the read/write from tun and server
 * receives the tun_fd, the server_socket_fd and the client_address as parameters
 * returns nothing
*/
void main_loop(int tun_fd, int server_socket_fd)
{
    char buffer[MAX_SIZE_BUFF];

    struct sockaddr_in client_address;

    fd_set readfds;
    int max_fd = (tun_fd > server_socket_fd) ? tun_fd : server_socket_fd;
    time_t last_clean_time = time(NULL);


    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(tun_fd, &readfds);
        FD_SET(server_socket_fd, &readfds);

       
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select:");
            continue;
        }
        if(time(NULL) - last_clean_time >= 5)
        {
            clean_table();
            last_clean_time = time(NULL);
        }

        /*
         * if the server socket is set, it means that we have a client trying to send something
         * we willd read that package from the client and send it to the tun driver
         * and tun will send it to the kernel, and the kernel will send it to the destination
        */
        if (FD_ISSET(server_socket_fd, &readfds))
        {
            int bytes_read, bytes_sent=0;
            if ((bytes_read = read_from_client(server_socket_fd, &client_address, buffer, MAX_SIZE_BUFF)) < 0)
            {
                continue;
            }

            vpn_header_t *data_header = (vpn_header_t *)buffer;
            if (data_header->type == MSG_HANDSHAKE_REQ)
            {
                hanshake_with_client(&client_address,server_socket_fd,buffer,data_header);
                continue;

            }
            else if(data_header->type==MSG_DATA)
            {
                int data_len=bytes_read-sizeof(vpn_header_t);
                if(data_len>0)
                {
                    if ((bytes_sent = send_to_tun(tun_fd, buffer+sizeof(vpn_header_t), data_len)) < 0)
                    {
                        continue;
                    }
                }
            }else if(data_header->type==MSG_CLOSE_CONNECTION)
            {
                int idx = find_client(&client_address);
                if (idx != -1)
                {
                    memset(&clients[idx], 0, sizeof(struct client_table));
                }
            }
            else if (data_header->type==MSG_KEEP_ALIVE)
            {
                int idx=find_client(&client_address);
                if(idx!=-1)
                {
                    clients[idx].last_seen = time(NULL);
                }
                vpn_header_t keep_alive_ack;
                keep_alive_ack.type=MSG_KEEP_ALIVE;
                keep_alive_ack.virtual_ip=0;
                memcpy(buffer,&keep_alive_ack,sizeof(vpn_header_t));
                send_to_the_client(server_socket_fd,&client_address,buffer,sizeof(vpn_header_t));
            }
            /* Debug print*/
            printf("[CLIENT -> TUN] %d bytes lidos do cliente %s:%d, %d bytes escritos no TUN\n",
                bytes_read,
                inet_ntoa(client_address.sin_addr),
                ntohs(client_address.sin_port),
                bytes_sent);
        }
        /*
         * if the tun_fd is set, it means that we have a package from the kernel, we will read it and send it to the client
         * we read the package from the tun driver, and we will look for the destination IP in our table, if we find it, we will send it to the client
        */
        else if (FD_ISSET(tun_fd, &readfds))
        {
            int bytes_read, bytes_sent;
            if ((bytes_read = read_from_tun(tun_fd, buffer+sizeof(vpn_header_t), MAX_SIZE_BUFF-sizeof(vpn_header_t))) < 0)
            {
                continue;
            }            
            struct iphdr *iph = (struct iphdr *)buffer+sizeof(vpn_header_t);
            uint32_t dest_ip = iph->daddr;      
            
            int found = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].known && clients[i].virtual_ip == dest_ip) {
                    vpn_header_t *header = (vpn_header_t *)buffer;
                    header->type=MSG_DATA;
                    header->virtual_ip=clients[i].virtual_ip;
                    bytes_sent = send_to_the_client(server_socket_fd, &clients[i].client_address, buffer, bytes_read+sizeof(vpn_header_t));
                    found = i;
                    break;
                }
            }

            if (found >= 0 && bytes_sent > 0) {
                printf("[TUN -> CLIENT] %d bytes lidos do TUN, %d bytes enviados para %s:%d\n",
                    bytes_read, bytes_sent,
                    inet_ntoa(clients[found].client_address.sin_addr),
                    ntohs(clients[found].client_address.sin_port));
            }
        }
    }
}


/*
 * Main function, the entry point of the program
*/
int main()
{
    int tun_fd;
    int server_socket_fd;
    struct sockaddr_in server_address;

    if ((server_socket_fd = create_socket(&server_address)) < 0)
    {
        return -1;
    }

    if ((tun_fd = alloc_tun()) < 0)
    {
        return -1;
    }

    /* Main loop*/
    main_loop(tun_fd,server_socket_fd);

    close(server_socket_fd);
    close(tun_fd);
    return 0;
}