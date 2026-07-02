#include <stdio.h>      /* perror(), printf(), fprintf() */
#include <stdlib.h>     /* exit(), malloc(), free() */
#include <string.h>     /* memset(), strncpy() */
#include <unistd.h>     /* read(), write(), close() */
#include <fcntl.h>      /* O_RDWR */
#include <sys/ioctl.h>  /* ioctl() */
#include <netinet/in.h> /* struct sockaddr_in, htons() */
#include <arpa/inet.h>  /* inet_pton() */

/* includes for struct ifreq, etc */
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/types.h>


#include <sys/select.h>

/* includes from my project */
#include "../../Server/include/server.h"


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

/*
 * This function allocates a tun file, and connects with an interface, does not receive any parameters 
 * and return -1 in case of an error
*/
int alloc_tun() {
    char *devname;
    devname="tun0";
    struct ifreq ifr;
    int tun_fd, err;

    if((tun_fd=open("/dev/net/tun",O_RDWR))==-1)
    {
        perror("open /dev/net/tun");
        return -1;
    }

    memset(&ifr,0, sizeof(ifr));
    ifr.ifr_flags= IFF_TUN| IFF_NO_PI;
    strncpy(ifr.ifr_name, devname, IFNAMSIZ);

    /*
     * Creates a connection between the kernel and the code (tun_fd)   
    */
    if ((err = ioctl(tun_fd, TUNSETIFF, (void*)&ifr)) == -1) {
        perror("ioctl TUNSETIFF");
        close(tun_fd);
        return -1;
    }
    return tun_fd;
}

/*
 * This function creates the client_socket
 * returns -1 in case of an error
*/
int create_socket() 
{
    int client_socket_fd;
    
    if((client_socket_fd=socket(AF_INET, SOCK_DGRAM, 0))<0)
    {
        perror("Socket:");
        return -1;
    }

    return client_socket_fd;
}

/*
 * This function deifnes the server_addres, using the IP and the PORT in server.h 
 * returns -1 in caso of and error
*/
int define_server_address(struct sockaddr_in *server_address)
{
    memset(server_address, 0, sizeof(*server_address));
    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(PORT);
    /* This function converts IPV4/IPV6 to binary */
    if (inet_pton(AF_INET, IP, &server_address->sin_addr) <= 0)
    {
        perror("inet_pton");
        return -1;
    }
    return 0;
}

/*
 * This function reads from tun driver and wirtes to the buffer passed as a parameter
 * return the number of bytes read, or -1 in case of an error
*/
int read_from_tun(int tun_fdf, char *buffer, size_t max_size_buffer )
{
    int bytes_read;
    if((bytes_read = read(tun_fdf, buffer, max_size_buffer)) < 0) {
        perror("Reading from tun:");
        return -1;
    }
    return bytes_read;
}


/*
 * This function sends to the tun the content in the buffer
 * Returns the number of bytes written or -1 in case off an error                                                 
 */
int send_to_tun(int tun_fdf, char *buffer, size_t max_size_buffer )
{
    int bytes_written;
    if((bytes_written = write(tun_fdf, buffer, max_size_buffer)) < 0) {
        perror("Writing to tun:");
        return -1;
    }
    return bytes_written;
}


/*
 * This function sends to the server the content that is in the buffer passed as a parameter
 * returns the number of bytes sent or -1 in case of an error
*/
int send_to_server(int client_socket_fd, struct sockaddr_in *server_address, char *buffer,size_t buffer_len )
{
    int bytes_sent;
    if((bytes_sent = sendto(client_socket_fd, buffer, buffer_len, 0,(struct sockaddr *)server_address, sizeof(*server_address))) < 0)
    {
        perror("Sendto:");
        return -1;
    }
    return bytes_sent;
}


/*
 * This functions reads from the server to the buffer passed as a parameter
 * return the number of bytes read or -1 in case of an error
*/
int read_from_server(int client_socket_fd, struct sockaddr_in *server_address, char *buffer,size_t buffer_len )
{
    int bytes_read;
    socklen_t addr_len = sizeof(*server_address);
    if((bytes_read = recvfrom(client_socket_fd, buffer, buffer_len, 0,(struct sockaddr *)server_address, &addr_len)) < 0)
    {
        perror("recvfrom:");
        return -1;
    }
    return bytes_read;
}


/*
 * Main loop function, handles the select and the read/write from tun and server
 * receives the tun_fd, the client_socket_fd and the server_address as parameters
 * returns nothing
*/
void main_loop(int tun_fd,int client_socket_fd, struct sockaddr_in *server_address)
{
    char buffer[MAX_SIZE_BUFF];
    fd_set readfds;
    int max_fd = (tun_fd > client_socket_fd) ? tun_fd : client_socket_fd;

    vpn_header_t req_header;
    req_header.type = MSG_HANDSHAKE_REQ;
    if (inet_pton(AF_INET, "10.0.0.2", &req_header.virtual_ip) <= 0) {
        return;
    }
    memcpy(buffer, &req_header, sizeof(req_header));

    if (send_to_server(client_socket_fd, server_address, buffer, sizeof(vpn_header_t)) < 0) {
        return;
    }

    int bytes_read = read_from_server(client_socket_fd, server_address, buffer, MAX_SIZE_BUFF);
    if (bytes_read < 0) {
        return;
    }

    /*
     * [5 bytes (vpn_header_t)]
    */
    vpn_header_t *resp_header = (vpn_header_t *)buffer;
    if (resp_header->type != MSG_HANDSHAKE_ACK) {
        return;
    }
    printf("[HANDSHAKE] O Servidor aceitou a ligação! Túnel pronto.\n");

    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(tun_fd, &readfds);
        FD_SET(client_socket_fd, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 10;  // 5 seconds timeout
        timeout.tv_usec = 0;
        int activity;

        if((activity=select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0)) {
            perror("select:");
            continue;
        }
        /*
         * Keep alive logic
        */
        if(activity==0)
        {
            vpn_header_t keep_alive_header;
            keep_alive_header.type=MSG_KEEP_ALIVE;
            keep_alive_header.virtual_ip=req_header.virtual_ip;
            memcpy(buffer,&keep_alive_header,sizeof(vpn_header_t));
            send_to_server(client_socket_fd,server_address,buffer,sizeof(vpn_header_t));
        }

        /* Read from the tun, and then send it to the server*/
        if(FD_ISSET(tun_fd,&readfds))
        {
            int bytes_read,bytes_sent;
            /*
             * We read from tun, but we put the content sizeof(vpn_header_t) bytes in front of the buffer points so we can write our information
             * [8 bytes(vpn_header_t) | data ]
            */
            if((bytes_read=read_from_tun(tun_fd,buffer+sizeof(vpn_header_t),MAX_SIZE_BUFF))<0)
            {
                continue;
            }
            /*
             * buffer points to the beginning of the buffer, and we leave space for the vpn_header_t at the start of the buffer
             * so we can send the type of message and the virtual_ip to the server
             * The vpn_header_t is placed at the start of the buffer, and the data read from the tun is placed after it
             * The server will read the vpn_header_t and then read the data from the tun, and send it to the correct client based on the virtual_ip
            */
            vpn_header_t *data_header = (vpn_header_t *)buffer;
            data_header->type = MSG_DATA;
            data_header->virtual_ip = req_header.virtual_ip;  // Using the predefined virtual IP for this client

            if((bytes_sent=send_to_server(client_socket_fd,server_address,buffer,bytes_read+sizeof(vpn_header_t)))<0)
            {
                continue;
            }
            /* Debug print*/
            printf("[TUN -> SERVER] %d bytes lidos do TUN, %d bytes enviados para %s:%d\n",
                bytes_read, bytes_sent,
                inet_ntoa(server_address->sin_addr), ntohs(server_address->sin_port));

        }
        /* Read from server and send it in the tun */
        else if(FD_ISSET(client_socket_fd,&readfds))
        {
            int bytes_read, bytes_sent=0;
            if((bytes_read=read_from_server(client_socket_fd,server_address,buffer,MAX_SIZE_BUFF))<0)
            {
                continue;
            }
            vpn_header_t *data_header = (vpn_header_t *)buffer;
            if(data_header->type==MSG_DATA)
            {
                int data_len=bytes_read-sizeof(vpn_header_t);
                if(data_len>0)
                {
                    if((bytes_sent=send_to_tun(tun_fd,buffer+sizeof(vpn_header_t),data_len))<0)
                    {
                        continue;
                    }
                }
            }
            else if(data_header->type==MSG_HANDSHAKE_ACK)
            {
                printf("[HANDSHAKE] O Servidor aceitou a ligação! Túnel pronto.\n");
            }
            /* debug print */
            printf("[SERVER -> TUN] %d bytes lidos do servidor, %d bytes escritos no TUN\n",
                bytes_read, bytes_sent);
        }
    }

    vpn_header_t close_header;
    close_header.type = MSG_CLOSE_CONNECTION;
    close_header.virtual_ip = req_header.virtual_ip;
    memcpy(buffer, &close_header, sizeof(close_header));
    send_to_server(client_socket_fd, server_address, buffer, sizeof(vpn_header_t));
}


int main() {
    int client_socket_fd;
    int tun_fd;
    struct sockaddr_in server_address;

    if((tun_fd=alloc_tun())<0)
    {
        return -1;
    }

    if((client_socket_fd=create_socket())<0)
    {
        return -1;
    }

    if(define_server_address(&server_address)<0)
    {
        return -1;
    }

    /* Main loop */
    main_loop(tun_fd,client_socket_fd,&server_address);
  
    close(tun_fd);
    close(client_socket_fd);
    return 0;
}