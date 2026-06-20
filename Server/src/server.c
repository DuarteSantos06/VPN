#include <fcntl.h>      /* O_RDWR */
#include <stdio.h>      /* perror(), printf(), fprintf() */
#include <stdlib.h>     /* exit(), malloc(), free() */
#include <string.h>     /* memset(), memcpy() */
#include <sys/ioctl.h>  /* ioctl() */
#include <unistd.h>     /* read(), close() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, htons() */

/* includes for struct ifreq, etc */
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/types.h>
/* includes from my project */
#include "server.h"

/*
 * This function allocates a tun file, and connects with an interface, does not receive any parameters 
 * and return -1 in case of an error
*/
int alloc_tun()
{
    char *devname;
    devname="tun1";
    struct ifreq ifr;
    int tun_fd, err;

    if((tun_fd=open("/dev/net/tun",O_RDWR))==-1)
    {
        perror("open /dev/net/tun");
        return -1;
    }

    memset(&ifr,0, sizeof(ifr));
    ifr.ifr_flags= IFF_TUN;
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
 * This function creates a socket, binds it to the port and returns the socket file descriptor
*/
int create_socket(struct sockaddr_in *server_address)
{
    int server_socket_fd=socket(AF_INET, SOCK_STREAM, 0);

    if( server_socket_fd < 0)
    {
        perror("Socket:");
        return -1;
    }

    server_address->sin_family= AF_INET;
    /* Accepts any Interface */
    server_address->sin_addr.s_addr = INADDR_ANY;
    server_address->sin_port = htons(PORT);
    
    if(bind(server_socket_fd,(struct sockaddr *)server_address,sizeof(struct sockaddr))<0)
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
int read_from_client(int server_socket_fd,struct sockaddr_in *client_address,char *buffer, size_t buffer_len)
{
    int bytes_read;
    socklen_t addr_len= sizeof(*client_address);
    if((bytes_read=recvfrom(server_socket_fd,buffer, buffer_len, 0,(struct sockaddr *)client_address, &addr_len)))
    {
        perror("recvfrom:");
        return -1;
    }
    return bytes_read;
}

int write_to_the_client(int server_socket_fd,struct sockaddr_in *client_address,char *buffer, size_t buffer_len)
{
    int bytes_sent;
    if((bytes_sent=sendto(server_socket_fd,buffer,0, buffer_len,(struct sockaddr *)client_address,sizeof(*client_address)))<0)
    {
        perror("Sendto:");
        return -1;
    }
    return bytes_sent;
}

int main() {
    int tun_fd;
    int server_socket_fd;
    struct sockaddr_in server_address;
    /*struct sockaddr_in client_address; ////// Not used yet */

    if((server_socket_fd=create_socket(&server_address))<0)
    {
        return -1;
    }


    if((tun_fd=alloc_tun())<0)
    {
        return -1;
    }   
    
    while(1)
    {
        /* Main loop */
    }
    
    close(server_socket_fd);
    close(tun_fd);
    return -1;
}