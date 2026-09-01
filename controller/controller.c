#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

#define TEMP_LIMIT 70.0

#define MIN_VALID_TEMP 0.0
#define MAX_VALID_TEMP 120.0

#define TOTAL_CYCLES 100
#define CYCLE_TIME_NS 100000000L
#define SENSOR_TIMEOUT_MS 80


long long time_diff_ns(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000000000LL
           + (end.tv_nsec - start.tv_nsec);
}


void add_time_ns(struct timespec *time, long long ns)
{
    time->tv_nsec += ns;

    if (time->tv_nsec >= 1000000000L)
    {
        time->tv_sec++;
        time->tv_nsec -= 1000000000L;
    }
}


int main()
{
    printf("=====================================\n");
    printf(" Self-Periodic Real-Time Controller\n");
    printf(" With Fault Detection + Fail-Safe\n");
    printf("=====================================\n");

    printf("Control cycle: 100 ms\n");
    printf("Temperature limit: %.1f C\n", TEMP_LIMIT);
    printf("Valid temperature range: %.1f - %.1f C\n",
           MIN_VALID_TEMP,
           MAX_VALID_TEMP);
    printf("Sensor timeout: %d ms\n\n",
           SENSOR_TIMEOUT_MS);

    fflush(stdout);


    /* Create UDP socket */

    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_socket < 0)
    {
        perror("UDP socket creation failed");
        return 1;
    }


    struct sockaddr_in udp_address;

    memset(&udp_address, 0, sizeof(udp_address));

    udp_address.sin_family = AF_INET;
    udp_address.sin_port = htons(9000);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &udp_address.sin_addr
    );


    /*
     * Open sensor FIFO in non-blocking mode.
     * This prevents the controller from getting
     * stuck if the sensor is not running.
     */

    int sensor_fd = open(
        "../sensor_pipe",
        O_RDONLY | O_NONBLOCK
    );

    if (sensor_fd < 0)
    {
        perror("Failed to open sensor pipe");

        close(udp_socket);

        return 1;
    }


    /*
     * Convert file descriptor to FILE stream.
     */

    FILE *sensor_pipe = fdopen(
        sensor_fd,
        "r"
    );

    if (sensor_pipe == NULL)
    {
        perror("Failed to create sensor stream");

        close(sensor_fd);
        close(udp_socket);

        return 1;
    }


    setvbuf(
        sensor_pipe,
        NULL,
        _IONBF,
        0
    );


    /*
     * Open actuator FIFO.
     */

    FILE *actuator_pipe = fopen(
        "../actuator_pipe",
        "w"
    );

    if (actuator_pipe == NULL)
    {
        perror("Failed to open actuator pipe");

        fclose(sensor_pipe);
        close(udp_socket);

        return 1;
    }


    setvbuf(
        actuator_pipe,
        NULL,
        _IONBF,
        0
    );


    long long min_error = 9999999999LL;
    long long max_error = 0;
    long long total_error = 0;

    int fault_count = 0;
    int completed_cycles = 0;


    struct timespec next_time;

    clock_gettime(
        CLOCK_MONOTONIC,
        &next_time
    );


    /*
     * Main real-time control loop.
     */

    for (int cycle = 1; cycle <= TOTAL_CYCLES; cycle++)
    {

        /*
         * Schedule next 100 ms deadline.
         */

        add_time_ns(
            &next_time,
            CYCLE_TIME_NS
        );


        /*
         * Sleep until the absolute deadline.
         */

        int sleep_result;

        do
        {
            sleep_result = clock_nanosleep(
                CLOCK_MONOTONIC,
                TIMER_ABSTIME,
                &next_time,
                NULL
            );

        }
        while (sleep_result == EINTR);


        /*
         * Start processing timer.
         */

        struct timespec start;

        clock_gettime(
            CLOCK_MONOTONIC,
            &start
        );


        double temperature = 0.0;

        int cooling = 0;
        int fault = 0;


        /*
         * Wait for sensor data.
         */

        struct pollfd sensor_poll;

        sensor_poll.fd = sensor_fd;
        sensor_poll.events = POLLIN;
        sensor_poll.revents = 0;


        int poll_result = poll(
            &sensor_poll,
            1,
            SENSOR_TIMEOUT_MS
        );


        /*
         * Sensor timeout.
         */

        if (poll_result == 0)
        {
            fault = 1;
            fault_count++;

            cooling = 1;

            printf(
                "[Cycle %03d] SENSOR TIMEOUT "
                "-> FAIL-SAFE COOLING ON\n",
                cycle
            );
        }


        /*
         * Poll error.
         */

        else if (poll_result < 0)
        {
            fault = 1;
            fault_count++;

            cooling = 1;

            printf(
                "[Cycle %03d] SENSOR POLL ERROR "
                "-> FAIL-SAFE COOLING ON: %s\n",
                cycle,
                strerror(errno)
            );
        }


        /*
         * Sensor pipe error.
         */

        else if (
            sensor_poll.revents &
            (POLLERR | POLLNVAL)
        )
        {
            fault = 1;
            fault_count++;

            cooling = 1;

            printf(
                "[Cycle %03d] SENSOR PIPE ERROR "
                "-> FAIL-SAFE COOLING ON\n",
                cycle
            );
        }


        /*
         * Sensor disconnected.
         */

        else if (
            sensor_poll.revents & POLLHUP
        )
        {
            fault = 1;
            fault_count++;

            cooling = 1;

            printf(
                "[Cycle %03d] SENSOR DISCONNECTED "
                "-> FAIL-SAFE COOLING ON\n",
                cycle
            );
        }


        /*
         * Sensor data available.
         */

        else
        {
            int read_result = fscanf(
                sensor_pipe,
                "%lf",
                &temperature
            );


            /*
             * Invalid sensor data.
             */

            if (read_result != 1)
            {
                fault = 1;
                fault_count++;

                cooling = 1;

                printf(
                    "[Cycle %03d] INVALID SENSOR DATA "
                    "-> FAIL-SAFE COOLING ON\n",
                    cycle
                );
            }


            /*
             * Impossible temperature value.
             */

            else if (
                temperature < MIN_VALID_TEMP ||
                temperature > MAX_VALID_TEMP
            )
            {
                fault = 1;
                fault_count++;

                cooling = 1;

                printf(
                    "[Cycle %03d] INVALID TEMPERATURE: %.1f C "
                    "-> FAIL-SAFE COOLING ON\n",
                    cycle,
                    temperature
                );
            }


            /*
             * Normal control decision.
             */

            else
            {
                cooling =
                    (temperature >= TEMP_LIMIT);
            }
        }


        /*
         * Send actuator command.
         */

        fprintf(
            actuator_pipe,
            "%d\n",
            cooling
        );

        fflush(actuator_pipe);


        /*
         * Timestamp UDP transmission.
         */

        struct timespec udp_ts;

        clock_gettime(
            CLOCK_MONOTONIC,
            &udp_ts
        );


        long long send_ns =
            (long long)udp_ts.tv_sec *
            1000000000LL
            + udp_ts.tv_nsec;


        /*
         * Send monitoring message.
         */

        char message[256];

        snprintf(
            message,
            sizeof(message),
            "SEQ=%d SEND_NS=%lld TEMP=%.1f COOLING=%d FAULT=%d",
            cycle,
            send_ns,
            temperature,
            cooling,
            fault
        );


        ssize_t sent = sendto(
            udp_socket,
            message,
            strlen(message),
            0,
            (struct sockaddr *)&udp_address,
            sizeof(udp_address)
        );


        if (sent < 0)
        {
            perror("UDP send failed");
        }


        /*
         * End processing timer.
         */

        struct timespec end;

        clock_gettime(
            CLOCK_MONOTONIC,
            &end
        );


        long long processing_time =
            time_diff_ns(
                start,
                end
            );


        /*
         * Calculate timing error.
         */

        struct timespec current_time;

        clock_gettime(
            CLOCK_MONOTONIC,
            &current_time
        );


        long long timing_error =
            time_diff_ns(
                next_time,
                current_time
            );


        if (timing_error < 0)
        {
            timing_error =
                -timing_error;
        }


        if (timing_error < min_error)
        {
            min_error =
                timing_error;
        }


        if (timing_error > max_error)
        {
            max_error =
                timing_error;
        }


        total_error += timing_error;

        completed_cycles++;


        /*
         * Display cycle result.
         */

        printf(
            "[Cycle %03d] "
            "Temperature: %.1f C | "
            "Cooling: %s | "
            "Fault: %s | "
            "Processing: %lld ns | "
            "Timing error: %lld ns\n",

            cycle,

            temperature,

            cooling ? "ON" : "OFF",

            fault ? "YES" : "NO",

            processing_time,

            timing_error
        );

        fflush(stdout);
    }


    /*
     * Cleanup.
     */

    fclose(sensor_pipe);
    fclose(actuator_pipe);

    close(udp_socket);


    /*
     * Final performance summary.
     */

    printf("\n=====================================\n");
    printf(" Timing + Fault Performance Summary\n");
    printf("=====================================\n");

    printf(
        "Completed cycles: %d\n",
        completed_cycles
    );

    printf(
        "Faults detected: %d\n",
        fault_count
    );


    if (completed_cycles > 0)
    {
        printf(
            "Minimum timing error: %lld ns\n",
            min_error
        );

        printf(
            "Maximum timing error: %lld ns\n",
            max_error
        );

        printf(
            "Average timing error: %lld ns\n",
            total_error /
            completed_cycles
        );
    }


    printf(
        "\nController finished successfully.\n"
    );


    return 0;
}