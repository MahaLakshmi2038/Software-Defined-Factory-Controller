#include <stdio.h>

#define PRESSURE_LIMIT 80
#define TOTAL_CYCLES 100

int main() {

    FILE *sensor_pipe = fopen("../pressure_pipe", "r");

    if (sensor_pipe == NULL) {
        perror("Failed to open pressure pipe");
        return 1;
    }

    FILE *conveyor_pipe = fopen("../conveyor_pipe", "w");

    if (conveyor_pipe == NULL) {
        perror("Failed to open conveyor pipe");
        fclose(sensor_pipe);
        return 1;
    }

    int pressure;

    printf("=====================================\n");
    printf(" Pressure Controller\n");
    printf("=====================================\n");
    printf("Pressure limit: %d PSI\n\n", PRESSURE_LIMIT);

    for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++) {

        if (fscanf(sensor_pipe, "%d", &pressure) != 1) {
            break;
        }

        int conveyor = (pressure >= PRESSURE_LIMIT);

        fprintf(conveyor_pipe, "%d\n", conveyor);
        fflush(conveyor_pipe);

        printf("[Cycle %03d] Pressure: %d PSI | "
               "Conveyor: %s\n",
               cycle,
               pressure,
               conveyor ? "ON" : "OFF");

        fflush(stdout);
    }

    fclose(sensor_pipe);
    fclose(conveyor_pipe);

    printf("\nPressure controller finished successfully.\n");

    return 0;
}
