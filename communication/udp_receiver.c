#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define PORT 9000
#define BUFFER_SIZE 256

long long get_time_ns() {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000000LL
           + ts.tv_nsec;
}

int main() {

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    printf("=====================================\n");
    printf(" UDP Latency Receiver\n");
    printf("=====================================\n");
    printf("Listening on port %d...\n\n", PORT);

    while (1) {

        int n = recvfrom(sockfd,
                         buffer,
                         BUFFER_SIZE - 1,
                         0,
                         NULL,
                         NULL);

        if (n < 0) {
            perror("Receive failed");
            break;
        }

        buffer[n] = '\0';

        long long receive_time = get_time_ns();

        int sequence;
        long long send_time;
        double temperature;
        int cooling;

        if (sscanf(buffer,
                   "SEQ=%d SEND_NS=%lld TEMP=%lf COOLING=%d",
                   &sequence,
                   &send_time,
                   &temperature,
                   &cooling) == 4) {

            long long latency = receive_time - send_time;

            printf("[SEQ %03d] TEMP=%.1f | COOLING=%d | "
                   "UDP Latency: %lld ns (%.3f us)\n",
                   sequence,
                   temperature,
                   cooling,
                   latency,
                   latency / 1000.0);

        } else {

            printf("Received: %s\n", buffer);
        }

        fflush(stdout);
    }

    close(sockfd);

    return 0;
}
