#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define PORT 10120
#define REPLY_PORT 10121
#define BUFFER_SIZE 1024

// Function to get the IP address of the Raspberry Pi
char* get_ip_address() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char *host = (char*)malloc(NI_MAXHOST);

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Loop through all interfaces to find an IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            // Skip the loopback address
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                freeifaddrs(ifaddr);
                return host;
            }
        }
    }

    freeifaddrs(ifaddr);
    return NULL;
}

// Function to get the MAC address of the Raspberry Pi
char* get_mac_address(const char* iface) {
    int fd;
    struct ifreq ifr;
    char *mac = (char*)malloc(18);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    close(fd);

    sprintf(mac, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
            (unsigned char)ifr.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr.ifr_hwaddr.sa_data[3],
            (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    return mac;
}

// Function to get the host name of the Raspberry Pi
char* get_host_name() {
    char *hostname = (char*)malloc(256);
    if (gethostname(hostname, 256) != 0) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }
    return hostname;
}

int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr, client_addr, reply_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket to the specified port
    if (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening for UDP messages on port %d...\n", PORT);

    while (1) {
        // Clear the buffer
        memset(buffer, 0, BUFFER_SIZE);

        // Receive UDP message
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        buffer[n] = '\0';
        printf("Received message: %s\n", buffer);

        // Check if the message is "HostInfo"
        if (strcmp(buffer, "HostInfo") == 0) {
            char *ip_address = get_ip_address();
            char *mac_address = get_mac_address("eth0"); // Replace "eth0" with the appropriate network interface
            char *host_name = get_host_name();

            // Prepare reply message
            snprintf(buffer, BUFFER_SIZE, "HostInfo %s %s %s", host_name, ip_address, mac_address);

            // Set up reply address structure
            memset(&reply_addr, 0, sizeof(reply_addr));
            reply_addr.sin_family = AF_INET;
            reply_addr.sin_port = htons(REPLY_PORT);
            reply_addr.sin_addr = client_addr.sin_addr;

            // Send reply to the client on port 10121
            sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr*)&reply_addr, sizeof(reply_addr));
            printf("Replied with: %s\n", buffer);

            // Free the allocated memory
            free(ip_address);
            free(mac_address);
        }
    }

    close(sockfd);
    return 0;
}
