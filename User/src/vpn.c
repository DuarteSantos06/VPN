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
    while(1)
    {
        FD_ZERO(&readfds);
        FD_SET(tun_fd, &readfds);
        FD_SET(client_socket_fd, &readfds);

        if(select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select:");
            continue;
        }

        /* Read from the tun, and then send it to the server*/
        if(FD_ISSET(tun_fd,&readfds))
        {
            int bytes_read,bytes_sent;
            if((bytes_read=read_from_tun(tun_fd,buffer,MAX_SIZE_BUFF))<0)
            {
                continue;
            }
            if((bytes_sent=send_to_server(client_socket_fd,server_address,buffer,bytes_read))<0)
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
            int bytes_read, bytes_sent;
            if((bytes_read=read_from_server(client_socket_fd,server_address,buffer,MAX_SIZE_BUFF))<0)
            {
                continue;
            }
            if((bytes_sent=send_to_tun(tun_fd,buffer,bytes_read))<0)
            {
                continue;
            }
            /* debug print */
            printf("[SERVER -> TUN] %d bytes lidos do servidor, %d bytes escritos no TUN\n",
                bytes_read, bytes_sent);
        }
    }




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