#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static double get_cpu_load(void)
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

    double load1;
    if (sscanf(buffer, "%lf", &load1) != 1) {
        return -1.0;
    }

    return load1;
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

    printf("========================================\n");
    printf(" Distributed Factory Node Agent\n");
    printf(" Node: %s\n", node_name);
    printf("========================================\n");

    for (int cycle = 1; cycle <= 30; cycle++) {

        double cpu_load = get_cpu_load();

        const char *health =
            (cpu_load >= 0.0 && cpu_load < 4.0)
            ? "HEALTHY"
            : "OVERLOADED";

        printf(
            "NODE=%s CYCLE=%d TIME_MS=%lld LOAD=%.2f HEALTH=%s\n",
            node_name,
            cycle,
            timestamp_ms(),
            cpu_load,
            health
        );

        fflush(stdout);

        sleep(1);
    }

    printf("Node agent finished.\n");

    return 0;
}
