#include <stdio.h>

void set_cooling(int state) {
    if (state) {
        printf("ACTUATOR: Cooling Fan -> ON\n");
    } else {
        printf("ACTUATOR: Cooling Fan -> OFF\n");
    }
}

int main() {

    FILE *pipe = fopen("../actuator_pipe", "r");

    if (pipe == NULL) {
        perror("Failed to open actuator pipe");
        return 1;
    }

    int command;

    while (fscanf(pipe, "%d", &command) == 1) {

        set_cooling(command);

        fflush(stdout);
    }

    fclose(pipe);

    return 0;
}
