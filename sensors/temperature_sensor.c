#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define CYCLE_TIME_MS 100
#define TOTAL_CYCLES 100

int main(int argc, char *argv[])
{
    int fault_after = -1;

    /* Parse optional fault injection argument */
    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--fault-after") == 0) {

            if (i + 1 >= argc) {
                fprintf(stderr,
                        "Error: --fault-after requires a cycle number.\n");
                return 1;
            }

            fault_after = atoi(argv[++i]);

            if (fault_after <= 0 || fault_after >= TOTAL_CYCLES) {
                fprintf(stderr,
                        "Error: fault cycle must be between 1 and %d.\n",
                        TOTAL_CYCLES - 1);
                return 1;
            }

        } else {

            fprintf(stderr,
                    "Unknown argument: %s\n",
                    argv[i]);
            return 1;
        }
    }

    srand((unsigned int)time(NULL));

    FILE *pipe = fopen("../sensor_pipe", "w");

    if (pipe == NULL) {
        perror("Failed to open sensor pipe");
        return 1;
    }

    printf("=====================================\n");
    printf(" Factory Temperature Sensor\n");
    printf("=====================================\n");
    printf("Sensor cycle: %d ms\n", CYCLE_TIME_MS);

    if (fault_after > 0) {
        printf("Fault injection: AFTER CYCLE %d\n", fault_after);
    } else {
        printf("Fault injection: DISABLED\n");
    }

    printf("=====================================\n\n");

    for (int i = 0; i < TOTAL_CYCLES; i++) {

        /*
         * Controlled sensor fault injection.
         *
         * After the selected cycle, stop providing
         * sensor data and close the FIFO.
         */
        if (fault_after > 0 && i >= fault_after) {

            printf("\n*** SENSOR FAULT INJECTED ***\n");
            printf("Sensor stopped providing temperature data.\n");
            printf("Fault triggered after cycle %d.\n", fault_after);

            fclose(pipe);
            return 0;
        }

        double temperature = 60.0 + (rand() % 21);

        fprintf(pipe, "%.1f\n", temperature);
        fflush(pipe);

        printf("Sensor sent: %.1f C\n", temperature);

        usleep(CYCLE_TIME_MS * 1000);
    }

    fclose(pipe);

    printf("\nSensor finished successfully.\n");

    return 0;
}
