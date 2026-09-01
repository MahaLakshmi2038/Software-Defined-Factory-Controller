#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9000
#define TOTAL_MESSAGES 20

int main() {

    int sockfd;
    struct sockaddr_in server_addr;
    char message[256];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &server_addr.sin_addr) <= 0) {

        perror("Invalid address");
        close(sockfd);
        return 1;
    }

    printf("=====================================\n");
    printf(" UDP Sender\n");
    printf("=====================================\n");

    for (int i = 1; i <= TOTAL_MESSAGES; i++) {

        double temperature = 60.0 + (rand() % 21);
        int cooling = (temperature >= 70.0);

        snprintf(message,
                 sizeof(message),
                 "TEMP=%.1f COOLING=%d",
                 temperature,
                 cooling);

        sendto(sockfd,
               message,
               strlen(message),
               0,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr));

        printf("Sent: %s\n", message);
        fflush(stdout);

        usleep(100000);
    }

    close(sockfd);

    printf("\nUDP sender finished successfully.\n");

    return 0;
}
