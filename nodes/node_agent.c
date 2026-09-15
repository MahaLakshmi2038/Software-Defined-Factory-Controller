#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define COORDINATOR_IP "127.0.0.1"
#define COORDINATOR_PORT 9001

static double get_load(void)
{
    FILE *fp;
    char buffer[256];

    fp = fopen("/proc/loadavg", "r");

    if (fp == NULL) {
        return -1.0;
    }

    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fclose(fp);
        return -1.0;
    }

    fclose(fp);

    double load;

    if (sscanf(buffer, "%lf", &load) != 1) {
        return -1.0;
    }

    return load;
}

static long long timestamp_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL +
           ts.tv_nsec / 1000000LL;
}

int main(int argc, char *argv[])
{
    const char *node_name = "NODE_A";

    if (argc > 1) {
        node_name = argv[1];
    }

    int sockfd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in coordinator;

    memset(
        &coordinator,
        0,
        sizeof(coordinator)
    );

    coordinator.sin_family = AF_INET;
    coordinator.sin_port =
        htons(COORDINATOR_PORT);

    if (inet_pton(
            AF_INET,
            COORDINATOR_IP,
            &coordinator.sin_addr
        ) <= 0) {

        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    printf("========================================\n");
    printf(" Distributed Factory Node Agent\n");
    printf(" Node: %s\n", node_name);
    printf(" Coordinator: %s:%d\n",
           COORDINATOR_IP,
           COORDINATOR_PORT);
    printf("========================================\n");

    for (int cycle = 1; ; cycle++) {

        double load = get_load();

        const char *health =
            (load >= 0.0 && load < 4.0)
            ? "HEALTHY"
            : "OVERLOADED";

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "NODE=%s CYCLE=%d TIME_MS=%lld LOAD=%.2f HEALTH=%s",
            node_name,
            cycle,
            timestamp_ms(),
            load,
            health
        );

        ssize_t sent = sendto(
            sockfd,
            message,
            strlen(message),
            0,
            (struct sockaddr *)&coordinator,
            sizeof(coordinator)
        );

        if (sent < 0) {
            perror("sendto");
        } else {
            printf(
                "HEARTBEAT_SENT: %s\n",
                message
            );
        }

        fflush(stdout);

        sleep(1);
    }

    printf("Node agent finished.\n");

    close(sockfd);

    return 0;
}
