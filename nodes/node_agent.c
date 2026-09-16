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
#define CPU_OVERLOAD_THRESHOLD 80.0

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
    int overload_mode = 0;
    int dynamic_workload_mode = 0;
    int delay_ms = 0;

    if (argc > 1)
        node_name = argv[1];

    /*
     * Optional arguments:
     *
     * --overload
     *     Controlled CPU overload injection.
     *
     * --delay-ms N
     *     Artificial heartbeat communication delay.
     *
     * --dynamic-workload
     *     Changes the simulated workload during runtime.
     */
    for (int i = 2; i < argc; i++) {

        if (strcmp(argv[i], "--overload") == 0) {

            overload_mode = 1;

        } else if (strcmp(argv[i], "--dynamic-workload") == 0) {

            dynamic_workload_mode = 1;

        } else if (strcmp(argv[i], "--delay-ms") == 0) {

            if (i + 1 >= argc) {

                fprintf(
                    stderr,
                    "Error: --delay-ms requires a value in milliseconds.\n"
                );

                return 1;
            }

            delay_ms = atoi(argv[++i]);

            if (delay_ms < 0) {

                fprintf(
                    stderr,
                    "Error: delay must be >= 0 ms.\n"
                );

                return 1;
            }

        } else {

            fprintf(
                stderr,
                "Unknown argument: %s\n",
                argv[i]
            );

            fprintf(
                stderr,
                "Usage: %s NODE_NAME [--overload] [--delay-ms N]\n",
                argv[0]
            );

            return 1;
        }
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

    if (overload_mode) {
        printf(" Mode: CONTROLLED OVERLOAD INJECTION\n");
        printf(" Simulated CPU: 100.00%%\n");
    } else {
        printf(" Mode: NORMAL\n");
        printf(" Metric: CPU utilization from /proc/stat\n");
    }

    if (delay_ms > 0) {
        printf(" Heartbeat delay: %d ms\n", delay_ms);
    } else {
        printf(" Heartbeat delay: 0 ms\n");
    }

    printf("========================================\n");

    for (int cycle = 1; ; cycle++) {

        /*
         * Wait before taking the second sample.
         * This gives us a meaningful CPU utilization interval.
         */
        sleep(1);

        double cpu;

        if (overload_mode) {

            /*
             * Controlled fault injection.
             *
             * This deliberately reports this logical node
             * as overloaded without affecting other nodes.
             */
            cpu = 100.0;

        } else if (dynamic_workload_mode) {

            /*
             * Dynamic workload experiment:
             *
             * Cycles 1-5  : normal workload
             * Cycles 6-10 : high workload (85%)
             * Cycles 11+  : normal workload again
             */
            if (cycle >= 6 && cycle <= 10) {
                cpu = 85.0;
            } else {
                cpu = 5.0;
            }

        } else {

            if (read_cpu_stats(&current) != 0) {

                fprintf(
                    stderr,
                    "Failed to read CPU statistics.\n"
                );

                continue;
            }

            cpu =
                get_cpu_utilization(
                    &previous,
                    &current
                );

            previous = current;
        }

        const char *health =
            (cpu < CPU_OVERLOAD_THRESHOLD)
            ? "HEALTHY"
            : "OVERLOADED";

        /*
         * Capture the sender timestamp BEFORE the
         * artificial delay. The coordinator then measures
         * the elapsed time between this timestamp and
         * heartbeat reception.
         */
        long long sender_time = timestamp_ms();

        if (delay_ms > 0) {
            usleep((useconds_t)delay_ms * 1000U);
        }

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "NODE=%s CYCLE=%d TIME_MS=%lld CPU=%.2f HEALTH=%s",
            node_name,
            cycle,
            sender_time,
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
