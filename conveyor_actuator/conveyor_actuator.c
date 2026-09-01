#include <stdio.h>

int main() {

    FILE *controller_pipe = fopen("../conveyor_pipe", "r");

    if (controller_pipe == NULL) {
        perror("Failed to open conveyor pipe");
        return 1;
    }

    int command;

    printf("=====================================\n");
    printf(" Conveyor Actuator\n");
    printf("=====================================\n");

    while (fscanf(controller_pipe, "%d", &command) == 1) {

        if (command == 1) {
            printf("CONVEYOR: ON\n");
        } else {
            printf("CONVEYOR: OFF\n");
        }

        fflush(stdout);
    }

    fclose(controller_pipe);

    printf("\nConveyor actuator finished successfully.\n");

    return 0;
}
