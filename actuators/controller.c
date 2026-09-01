#include <stdio.h>
#include <time.h>
#include <errno.h>

#define TEMP_LIMIT 70.0
#define TOTAL_CYCLES 100
#define CYCLE_TIME_NS 100000000L   // 100 ms

long long time_diff_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL
           + (end.tv_nsec - start.tv_nsec);
}

void add_time_ns(struct timespec *time, long long ns) {
    time->tv_nsec += ns;

    if (time->tv_nsec >= 1000000000L) {
        time->tv_sec++;
        time->tv_nsec -= 1000000000L;
    }
}

int main() {

    FILE *sensor_pipe = fopen("../sensor_pipe", "r");

    if (sensor_pipe == NULL) {
        perror("Failed to open sensor pipe");
        return 1;
    }

    FILE *actuator_pipe = fopen("../actuator_pipe", "w");

    if (actuator_pipe == NULL) {
        perror("Failed to open actuator pipe");
        fclose(sensor_pipe);
        return 1;
    }

    double temperature;

    struct timespec next_time;
    struct timespec start;
    struct timespec end;
    struct timespec current_time;

    clock_gettime(CLOCK_MONOTONIC, &next_time);

    printf("=====================================\n");
    printf(" Self-Periodic Real-Time Controller\n");
    printf("=====================================\n");
    printf("Control cycle: 100 ms\n");
    printf("Temperature limit: %.1f C\n\n", TEMP_LIMIT);

    for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++) {

        /* Wait until the next 100 ms deadline */
        add_time_ns(&next_time, CYCLE_TIME_NS);

        int result;

        do {
            result = clock_nanosleep(
                CLOCK_MONOTONIC,
                TIMER_ABSTIME,
                &next_time,
                NULL
            );
        } while (result == EINTR);

        /* Start measuring controller processing */
        clock_gettime(CLOCK_MONOTONIC, &start);

        /* Read sensor value */
        if (fscanf(sensor_pipe, "%lf", &temperature) != 1) {
            break;
        }

        /* Control decision */
        int cooling = (temperature >= TEMP_LIMIT);

        /* Send command to actuator */
        fprintf(actuator_pipe, "%d\n", cooling);
        fflush(actuator_pipe);

        clock_gettime(CLOCK_MONOTONIC, &end);
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        long long processing_time =
            time_diff_ns(start, end);

        long long timing_error =
            time_diff_ns(next_time, current_time);

        printf("[Cycle %03d] Temperature: %.1f C | "
               "Cooling: %s | "
               "Processing: %lld ns | "
               "Timing error: %lld ns\n",
               cycle,
               temperature,
               cooling ? "ON" : "OFF",
               processing_time,
               timing_error);
    }

    fclose(sensor_pipe);
    fclose(actuator_pipe);

    printf("\nController finished successfully.\n");

    return 0;
}
