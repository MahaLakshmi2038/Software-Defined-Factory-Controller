#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_CYCLES 100
#define CYCLE_TIME_MS 100

int main() {

    srand(time(NULL));

    FILE *pressure_pipe = fopen("../pressure_pipe", "w");

    if (pressure_pipe == NULL) {
        perror("Failed to open pressure pipe");
        return 1;
    }

    printf("=====================================\n");
    printf(" Pressure Sensor Node\n");
    printf("=====================================\n");

    for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++) {

        int pressure = 40 + rand() % 61;

        fprintf(pressure_pipe, "%d\n", pressure);
        fflush(pressure_pipe);

        printf("[Cycle %03d] Pressure: %d PSI\n",
               cycle, pressure);

        fflush(stdout);

        usleep(CYCLE_TIME_MS * 1000);
    }

    fclose(pressure_pipe);

    printf("\nPressure sensor finished successfully.\n");

    return 0;
}
