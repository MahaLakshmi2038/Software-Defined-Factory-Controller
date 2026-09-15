#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define PORT 9001
#define BUFFER_SIZE 512
#define NODE_COUNT 4
#define TIMEOUT_MS 3000

typedef struct {
    char name[32];
    long long last_seen_ms;
    double load;
    int healthy;
} Node;

static long long timestamp_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL +
           ts.tv_nsec / 1000000LL;
}

static int find_node(Node nodes[], const char *name)
{
    for (int i = 0; i < NODE_COUNT; i++) {
        if (strcmp(nodes[i].name, name) == 0)
            return i;
    }

    return -1;
}

static void print_status(Node nodes[])
{
    printf("\n========== NODE STATUS ==========\n");

    long long now = timestamp_ms();

    for (int i = 0; i < NODE_COUNT; i++) {

        long long age = now - nodes[i].last_seen_ms;

        if (nodes[i].last_seen_ms == 0)
            nodes[i].healthy = 0;
        else if (age > TIMEOUT_MS)
            nodes[i].healthy = 0;
        else
            nodes[i].healthy = 1;

        printf(
            "NODE=%s LOAD=%.2f LAST_SEEN=%lld ms AGO HEALTH=%s\n",
            nodes[i].name,
            nodes[i].load,
            age,
            nodes[i].healthy ? "HEALTHY" : "FAILED"
        );
    }

    printf("=================================\n\n");
}

int main(void)
{
    int sockfd;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(sockfd,
             (struct sockaddr *)&server,
             sizeof(server)) < 0) {

        perror("bind");
        close(sockfd);
        return 1;
    }

    Node nodes[NODE_COUNT] = {
        {"NODE_A", 0, 0.0, 0},
        {"NODE_B", 0, 0.0, 0},
        {"NODE_C", 0, 0.0, 0},
        {"NODE_D", 0, 0.0, 0}
    };

    printf("========================================\n");
    printf(" Distributed Factory Coordinator\n");
    printf(" UDP Heartbeat Monitor\n");
    printf(" Port: %d\n", PORT);
    printf(" Timeout: %d ms\n", TIMEOUT_MS);
    printf("========================================\n");

    long long last_status = timestamp_ms();

    while (1) {

        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        char buffer[BUFFER_SIZE];

        ssize_t received = recvfrom(
            sockfd,
            buffer,
            sizeof(buffer) - 1,
            MSG_DONTWAIT,
            (struct sockaddr *)&client,
            &client_len
        );

        if (received > 0) {

            buffer[received] = '\0';

            char node_name[32];
            int cycle;
            long long sender_time;
            double load;
            char health[32];

            int parsed = sscanf(
                buffer,
                "NODE=%31s CYCLE=%d TIME_MS=%lld LOAD=%lf HEALTH=%31s",
                node_name,
                &cycle,
                &sender_time,
                &load,
                health
            );

            if (parsed == 5) {

                int index = find_node(nodes, node_name);

                if (index >= 0) {

                    nodes[index].last_seen_ms = timestamp_ms();
                    nodes[index].load = load;
                    nodes[index].healthy = 1;

                    printf(
                        "HEARTBEAT_RECEIVED NODE=%s CYCLE=%d LOAD=%.2f\n",
                        node_name,
                        cycle,
                        load
                    );

                    fflush(stdout);
                }
            }
        }

        long long now = timestamp_ms();

        if (now - last_status >= 1000) {

            print_status(nodes);

            last_status = now;
        }

        usleep(10000);
    }

    close(sockfd);

    return 0;
}
