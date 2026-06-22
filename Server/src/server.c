#include <fcntl.h>      /* O_RDWR */
#include <stdio.h>      /* perror(), printf(), fprintf() */
#include <stdlib.h>     /* exit(), malloc(), free() */
#include <string.h>     /* memset(), memcpy() */
#include <sys/ioctl.h>  /* ioctl() */
#include <unistd.h>     /* read(), close() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, htons() */
#include <arpa/inet.h>  /* inet_ntoa() */

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
 * Main loop function, handles the select and the read/write from tun and server
 * receives the tun_fd, the server_socket_fd and the client_address as parameters
 * returns nothing
*/
void main_loop(int tun_fd, int server_socket_fd,struct sockaddr_in *client_address)
{
    char buffer[MAX_SIZE_BUFF];
    int client_known = 0;

    fd_set readfds;
    int max_fd = (tun_fd > server_socket_fd) ? tun_fd : server_socket_fd;

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

        if (FD_ISSET(server_socket_fd, &readfds))
        {
            int bytes_read, bytes_sent;
            if ((bytes_read = read_from_client(server_socket_fd, client_address, buffer, MAX_SIZE_BUFF)) < 0)
            {
                continue;
            }
            client_known=1;
            if ((bytes_sent = send_to_tun(tun_fd, buffer, bytes_read)) < 0)
            {
                continue;
            }
            /* Debug print*/
            printf("[CLIENT -> TUN] %d bytes lidos do cliente %s:%d, %d bytes escritos no TUN\n",
                bytes_read,
                inet_ntoa(client_address->sin_addr),
                ntohs(client_address->sin_port),
                bytes_sent);
        }
        else if (FD_ISSET(tun_fd, &readfds))
        {
            if (!client_known)
            {
                continue;   /* We don't have any client to send */
            }
            int bytes_read, bytes_sent;
            if ((bytes_read = read_from_tun(tun_fd, buffer, MAX_SIZE_BUFF)) < 0)
            {
                continue;
            }
            if((bytes_sent = send_to_the_client(server_socket_fd, client_address, buffer, bytes_read)) < 0)
            {
                continue;
            }
            /* Debug print*/
            printf("[TUN -> CLIENT] %d bytes lidos do TUN, %d bytes enviados para %s:%d\n",
                bytes_read,
                bytes_sent,
                inet_ntoa(client_address->sin_addr),
                ntohs(client_address->sin_port));
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
    struct sockaddr_in client_address;

    if ((server_socket_fd = create_socket(&server_address)) < 0)
    {
        return -1;
    }

    if ((tun_fd = alloc_tun()) < 0)
    {
        return -1;
    }

    /* Main loop*/
    main_loop(tun_fd,server_socket_fd,&client_address);

    close(server_socket_fd);
    close(tun_fd);
    return 0;
}