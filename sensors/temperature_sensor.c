#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define CYCLE_TIME_MS 100
#define TOTAL_CYCLES 100

int main() {

    srand(time(NULL));

    FILE *pipe = fopen("../sensor_pipe", "w");

    if (pipe == NULL) {
        perror("Failed to open sensor pipe");
        return 1;
    }

    printf("=====================================\n");
    printf(" Factory Temperature Sensor\n");
    printf("=====================================\n");
    printf("Sensor cycle: %d ms\n\n", CYCLE_TIME_MS);

    for (int i = 0; i < TOTAL_CYCLES; i++) {

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
