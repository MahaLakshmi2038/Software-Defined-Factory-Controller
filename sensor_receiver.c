#include <stdio.h>

int main() {

    FILE *pipe = fopen("sensor_pipe", "r");

    if (pipe == NULL) {
        perror("Failed to open sensor pipe");
        return 1;
    }

    double temperature;

    while (fscanf(pipe, "%lf", &temperature) == 1) {
        printf("Controller received: %.1f C\n", temperature);
    }

    fclose(pipe);

    return 0;
}
