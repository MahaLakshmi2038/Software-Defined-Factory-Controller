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

typedef struct {
    unsigned long long total;
    unsigned long long idle;
} CpuStats;


/* ---------- TIME ---------- */

static long long timestamp_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL +
           ts.tv_nsec / 1000000LL;
}


/* ---------- CPU STATISTICS ---------- */

static int read_cpu_stats(CpuStats *stats)
{
    FILE *fp = fopen("/proc/stat", "r");

    if (fp == NULL)
        return -1;

    char line[512];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;

    int fields = sscanf(
        line,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &user,
        &nice,
        &system,
        &idle,
        &iowait,
        &irq,
        &softirq,
        &steal
    );

    if (fields < 4)
        return -1;

    stats->total =
        user +
        nice +
        system +
        idle +
        iowait +
        irq +
        softirq +
        steal;

    stats->idle = idle + iowait;

    return 0;
}


/* ---------- CPU UTILIZATION ---------- */

static double get_cpu_utilization(
    CpuStats *previous,
    CpuStats *current)
{
    unsigned long long total_delta =
        current->total - previous->total;

    unsigned long long idle_delta =
        current->idle - previous->idle;

    if (total_delta == 0)
        return 0.0;

    double utilization =
        100.0 *
        (double)(total_delta - idle_delta) /
        (double)total_delta;

    if (utilization < 0.0)
        utilization = 0.0;

    if (utilization > 100.0)
        utilization = 100.0;

    return utilization;
}


/* ---------- MAIN ---------- */

int main(int argc, char *argv[])
{
    const char *node_name = "NODE_A";

    if (argc > 1)
        node_name = argv[1];

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

    CpuStats previous;
    CpuStats current;

    if (read_cpu_stats(&previous) != 0) {
        fprintf(
            stderr,
            "Failed to read initial CPU statistics.\n"
        );

        close(sockfd);
        return 1;
    }

    printf("========================================\n");
    printf(" Distributed Factory Node Agent\n");
    printf(" Node: %s\n", node_name);
    printf(" Coordinator: %s:%d\n",
           COORDINATOR_IP,
           COORDINATOR_PORT);
    printf(" Metric: CPU utilization from /proc/stat\n");
    printf("========================================\n");

    for (int cycle = 1; ; cycle++) {

        /*
         * Wait before taking the second sample.
         * This gives us a meaningful CPU utilization interval.
         */
        sleep(1);

        if (read_cpu_stats(&current) != 0) {
            fprintf(
                stderr,
                "Failed to read CPU statistics.\n"
            );
            continue;
        }

        double cpu =
            get_cpu_utilization(
                &previous,
                &current
            );

        previous = current;

        const char *health =
            (cpu >= 0.0 && cpu < 80.0)
            ? "HEALTHY"
            : "OVERLOADED";

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "NODE=%s CYCLE=%d TIME_MS=%lld CPU=%.2f HEALTH=%s",
            node_name,
            cycle,
            timestamp_ms(),
            cpu,
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
    }

    close(sockfd);

    return 0;
}
