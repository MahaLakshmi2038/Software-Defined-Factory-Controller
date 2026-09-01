#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LOG_FILE "factory_monitor.csv"

long long get_time_ns() {

    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000000LL
           + ts.tv_nsec;
}

int main() {

    FILE *log = fopen(LOG_FILE, "w");

    if (log == NULL) {
        perror("Failed to open monitoring file");
        return 1;
    }

    fprintf(log,
            "timestamp_ns,temperature,cooling,pressure,conveyor\n");

    printf("=====================================\n");
    printf(" Factory Monitoring System\n");
    printf("=====================================\n");

    for (int cycle = 1; cycle <= 100; cycle++) {

        long long timestamp = get_time_ns();

        /*
         * Temporary monitoring values.
         * These will later be connected directly
         * to the real sensor/controller data.
         */

        double temperature = 60.0 + rand() % 21;
        int cooling = (temperature >= 70.0);

        int pressure = 40 + rand() % 61;
        int conveyor = (pressure >= 80);

        fprintf(log,
                "%lld,%.1f,%d,%d,%d\n",
                timestamp,
                temperature,
                cooling,
                pressure,
                conveyor);

        printf("[Cycle %03d] "
               "Temp: %.1f C | Cooling: %s | "
               "Pressure: %d PSI | Conveyor: %s\n",
               cycle,
               temperature,
               cooling ? "ON" : "OFF",
               pressure,
               conveyor ? "ON" : "OFF");

        fflush(log);
        fflush(stdout);

        struct timespec delay;
        delay.tv_sec = 0;
        delay.tv_nsec = 100000000L;

        nanosleep(&delay, NULL);
    }

    fclose(log);

    printf("\nMonitoring completed successfully.\n");
    printf("Data saved to %s\n", LOG_FILE);

    return 0;
}
