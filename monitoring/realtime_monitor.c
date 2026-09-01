#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 9000
#define BUFFER_SIZE 256
#define LOG_FILE "realtime_monitor.csv"

int main() {

    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    FILE *log = fopen(LOG_FILE, "w");

    if (log == NULL) {
        perror("Failed to open log file");
        return 1;
    }

    fprintf(log,
            "sequence,temperature,cooling,send_time_ns,receive_time_ns,udp_latency_ns\n");

    fflush(log);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket creation failed");
        fclose(log);
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
        fclose(log);

        return 1;
    }

    printf("=====================================\n");
    printf(" Real-Time Factory Monitor\n");
    printf("=====================================\n");
    printf("Listening on UDP port %d...\n", PORT);
    printf("Logging to %s\n\n", LOG_FILE);

    while (1) {

        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recvfrom(
            sockfd,
            buffer,
            BUFFER_SIZE - 1,
            0,
            NULL,
            NULL
        );

        if (bytes_received < 0) {
            perror("Receive failed");
            continue;
        }

        buffer[bytes_received] = '\0';

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

            struct timespec receive_ts;

            clock_gettime(CLOCK_MONOTONIC, &receive_ts);

            long long receive_time =
                (long long)receive_ts.tv_sec * 1000000000LL
                + receive_ts.tv_nsec;

            long long udp_latency =
                receive_time - send_time;

            fprintf(log,
                    "%d,%.1f,%d,%lld,%lld,%lld\n",
                    sequence,
                    temperature,
                    cooling,
                    send_time,
                    receive_time,
                    udp_latency);

            fflush(log);

            printf("[Cycle %03d] "
                   "Temperature: %.1f C | "
                   "Cooling: %s | "
                   "UDP Latency: %lld ns (%.3f us)\n",
                   sequence,
                   temperature,
                   cooling ? "ON" : "OFF",
                   udp_latency,
                   udp_latency / 1000.0);
        }
    }

    close(sockfd);
    fclose(log);

    return 0;
}
