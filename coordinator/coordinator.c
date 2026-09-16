#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 9001
#define BUFFER_SIZE 512

#define NODE_COUNT 4
#define TASK_COUNT 3

#define HEARTBEAT_TIMEOUT_MS 3000
#define CPU_OVERLOAD_THRESHOLD 80.0

typedef struct {
    char name[32];
    char task[32];

    double cpu;

    long long last_seen_ms;
    long long communication_latency_ms;

    int healthy;

    /*
     * Prevent repeated reconfiguration while
     * the same overload condition persists.
     */
    int overload_handled;

} Node;


typedef struct {
    char name[32];
    char assigned_node[32];
} Task;


/* =========================================================
 * TIME FUNCTIONS
 * =========================================================
 */

static long long timestamp_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL
           + ts.tv_nsec / 1000000LL;
}


static long long timestamp_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000000LL
           + ts.tv_nsec;
}


/* =========================================================
 * NODE LOOKUP
 * =========================================================
 */

static int find_node(
    Node nodes[],
    const char *name
)
{
    for (int i = 0; i < NODE_COUNT; i++) {

        if (strcmp(nodes[i].name, name) == 0)
            return i;
    }

    return -1;
}


/* =========================================================
 * FIND AVAILABLE / SPARE NODE
 * =========================================================
 */

static int find_available_node(
    Node nodes[]
)
{
    for (int i = 0; i < NODE_COUNT; i++) {

        if (nodes[i].healthy &&
            nodes[i].cpu < CPU_OVERLOAD_THRESHOLD &&
            strcmp(nodes[i].task, "NONE") == 0) {

            return i;
        }
    }

    return -1;
}


/* =========================================================
 * FIND TASK
 * =========================================================
 */

static int find_task(
    Task tasks[],
    const char *task_name
)
{
    for (int i = 0; i < TASK_COUNT; i++) {

        if (strcmp(tasks[i].name, task_name) == 0)
            return i;
    }

    return -1;
}


/* =========================================================
 * PRINT CURRENT DISTRIBUTED FACTORY STATUS
 * =========================================================
 */

static void print_status(
    Node nodes[],
    Task tasks[]
)
{
    long long now = timestamp_ms();

    printf("\n");
    printf("========================================\n");
    printf("        DISTRIBUTED FACTORY STATUS\n");
    printf("========================================\n");

    for (int i = 0; i < NODE_COUNT; i++) {

        long long age = 0;

        if (nodes[i].last_seen_ms > 0)
            age = now - nodes[i].last_seen_ms;

        const char *display_health;

        if (!nodes[i].healthy) {
            display_health = "FAILED";
        } else if (nodes[i].cpu >= CPU_OVERLOAD_THRESHOLD) {
            display_health = "OVERLOADED";
        } else {
            display_health = "HEALTHY";
        }

        printf(
            "NODE=%s CPU=%.2f%% HEALTH=%s TASK=%s LAST_SEEN=%lld ms AGO "
            "HEARTBEAT_DELAY=%lld ms\n",
            nodes[i].name,
            nodes[i].cpu,
            display_health,
            nodes[i].task,
            age,
            nodes[i].communication_latency_ms
        );

    }

    printf("----------------------------------------\n");
    printf("Task assignments:\n");

    for (int i = 0; i < TASK_COUNT; i++) {

        printf(
            "TASK=%s -> NODE=%s\n",
            tasks[i].name,
            tasks[i].assigned_node
        );
    }

    printf("========================================\n\n");

    fflush(stdout);
}


/* =========================================================
 * DYNAMIC TASK RECONFIGURATION
 * =========================================================
 */

static void reconfigure_task(
    Node nodes[],
    Task tasks[],
    int task_index,
    int old_node_index,
    int new_node_index
)
{
    long long start_ns;
    long long end_ns;

    /*
     * High-resolution start timestamp.
     */
    start_ns = timestamp_ns();


    printf("\n");
    printf("****************************************\n");
    printf("*** DYNAMIC RECONFIGURATION TRIGGERED ***\n");
    printf("****************************************\n");


    printf(
        "TASK=%s\n",
        tasks[task_index].name
    );


    printf(
        "FROM_NODE=%s\n",
        nodes[old_node_index].name
    );


    printf(
        "TO_NODE=%s\n",
        nodes[new_node_index].name
    );


    /*
     * Remove task from old node.
     */
    strcpy(
        nodes[old_node_index].task,
        "NONE"
    );


    /*
     * Assign task to new node.
     */
    strcpy(
        nodes[new_node_index].task,
        tasks[task_index].name
    );


    /*
     * Update task mapping.
     */
    strcpy(
        tasks[task_index].assigned_node,
        nodes[new_node_index].name
    );


    /*
     * High-resolution end timestamp.
     */
    end_ns = timestamp_ns();


    /*
     * Convert nanoseconds to microseconds.
     */
    double reconfiguration_time_us =
        (double)(end_ns - start_ns) / 1000.0;


    printf(
        "RECONFIGURATION_TIME_US=%.3f\n",
        reconfiguration_time_us
    );


    printf(
        "RECONFIGURATION_STATUS=SUCCESS\n"
    );


    printf(
        "TASK=%s NOW_ASSIGNED_TO=%s\n",
        tasks[task_index].name,
        tasks[task_index].assigned_node
    );


    printf("****************************************\n\n");

    fflush(stdout);
}


/* =========================================================
 * CPU OVERLOAD HANDLING
 * =========================================================
 */

static void check_node_overload(
    Node nodes[],
    Task tasks[]
)
{
    for (int i = 0; i < NODE_COUNT; i++) {

        /*
         * Only react to nodes that are:
         *
         * 1. Healthy
         * 2. Above CPU overload threshold
         * 3. Have not already been handled
         *
         * The third condition prevents a continuous
         * reconfiguration loop while overload persists.
         */

        if (nodes[i].healthy &&
            nodes[i].cpu >= CPU_OVERLOAD_THRESHOLD &&
            !nodes[i].overload_handled) {

            int task_index = -1;

            int spare_node_index;


            /*
             * Find task currently assigned
             * to the overloaded node.
             */

            for (int j = 0; j < TASK_COUNT; j++) {

                if (strcmp(
                        tasks[j].assigned_node,
                        nodes[i].name
                    ) == 0) {

                    task_index = j;

                    break;
                }
            }


            /*
             * If this is a spare node with no task,
             * there is nothing to reconfigure.
             */

            if (task_index == -1)
                continue;


            /*
             * Find healthy spare node.
             */

            spare_node_index =
                find_available_node(nodes);


            /*
             * No spare node available.
             */

            if (spare_node_index == -1) {

                printf(
                    "OVERLOAD_DETECTED NODE=%s CPU=%.2f%% "
                    "REASON=NO_AVAILABLE_HEALTHY_NODE\n",
                    nodes[i].name,
                    nodes[i].cpu
                );

                /*
                 * Mark as handled so that the coordinator
                 * does not continuously print the same event.
                 */

                nodes[i].overload_handled = 1;

                continue;
            }


            /*
             * Print overload event.
             */

            printf("\n");
            printf("!!! NODE OVERLOAD DETECTED !!!\n");


            printf(
                "OVERLOADED_NODE=%s\n",
                nodes[i].name
            );


            printf(
                "CPU_UTILIZATION=%.2f%%\n",
                nodes[i].cpu
            );


            printf(
                "TASK_AFFECTED=%s\n",
                tasks[task_index].name
            );


            /*
             * Dynamically move task to spare node.
             */

            reconfigure_task(
                nodes,
                tasks,
                task_index,
                i,
                spare_node_index
            );


            printf(
                "OVERLOAD_RECOVERY=SUCCESS\n"
            );


            /*
             * Latch the overload event.
             *
             * It will be reset only when the node's
             * CPU drops below the overload threshold.
             */

            nodes[i].overload_handled = 1;
        }
    }
}


/* =========================================================
 * NODE FAILURE HANDLING
 * =========================================================
 */

static void check_node_failures(
    Node nodes[],
    Task tasks[]
)
{
    long long now =
        timestamp_ms();


    for (int i = 0; i < NODE_COUNT; i++) {

        /*
         * Ignore nodes that have never sent
         * a heartbeat.
         */

        if (nodes[i].last_seen_ms == 0)
            continue;


        long long age =
            now - nodes[i].last_seen_ms;


        /*
         * Detect heartbeat timeout.
         */

        if (age > HEARTBEAT_TIMEOUT_MS &&
            nodes[i].healthy) {

            printf("\n");

            printf(
                "!!! NODE FAILURE DETECTED !!!\n"
            );


            printf(
                "FAILED_NODE=%s\n",
                nodes[i].name
            );


            printf(
                "LAST_HEARTBEAT_AGE_MS=%lld\n",
                age
            );


            /*
             * Mark node failed.
             */

            nodes[i].healthy = 0;


            /*
             * If failed node owns a task,
             * find that task.
             */

            if (strcmp(
                    nodes[i].task,
                    "NONE"
                ) != 0) {

                int task_index =
                    find_task(
                        tasks,
                        nodes[i].task
                    );


                if (task_index >= 0) {

                    /*
                     * Find healthy spare node.
                     */

                    int new_node =
                        find_available_node(nodes);


                    if (new_node >= 0) {

                        printf(
                            "AVAILABLE_SPARE_NODE=%s\n",
                            nodes[new_node].name
                        );


                        /*
                         * Reconfigure task.
                         */

                        reconfigure_task(
                            nodes,
                            tasks,
                            task_index,
                            i,
                            new_node
                        );


                        printf(
                            "NODE_FAILURE_RECOVERY=SUCCESS\n"
                        );
                    }

                    else {

                        printf(
                            "NODE_FAILURE_RECOVERY=FAILED\n"
                        );


                        printf(
                            "REASON=NO_AVAILABLE_HEALTHY_NODE\n"
                        );
                    }
                }
            }
        }
    }
}


/* =========================================================
 * MAIN
 * =========================================================
 */

int main(void)
{
    int sockfd;


    /* -----------------------------------------------------
     * Create UDP socket
     * -----------------------------------------------------
     */

    sockfd =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );


    if (sockfd < 0) {

        perror("socket");

        return 1;
    }


    /* -----------------------------------------------------
     * Allow quick restart after Ctrl+C
     * -----------------------------------------------------
     */

    int reuse = 1;


    setsockopt(
        sockfd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );


    /* -----------------------------------------------------
     * Server configuration
     * -----------------------------------------------------
     */

    struct sockaddr_in server;


    memset(
        &server,
        0,
        sizeof(server)
    );


    server.sin_family =
        AF_INET;


    server.sin_addr.s_addr =
        INADDR_ANY;


    server.sin_port =
        htons(PORT);


    /* -----------------------------------------------------
     * Bind UDP socket
     * -----------------------------------------------------
     */

    if (bind(
            sockfd,
            (struct sockaddr *)&server,
            sizeof(server)
        ) < 0) {

        perror("bind");

        close(sockfd);

        return 1;
    }


    /* -----------------------------------------------------
     * Make socket non-blocking
     * -----------------------------------------------------
     */

    if (fcntl(
            sockfd,
            F_SETFL,
            O_NONBLOCK
        ) < 0) {

        perror("fcntl");

        close(sockfd);

        return 1;
    }


    /* =====================================================
     * INITIAL DISTRIBUTED FACTORY CONFIGURATION
     * =====================================================
     *
     * NODE_A -> Temperature control
     * NODE_B -> Pressure control
     * NODE_C -> Conveyor control
     * NODE_D -> Spare
     */

    Node nodes[NODE_COUNT] = {

        {
            .name = "NODE_A",
            .task = "TEMP_CONTROL",
            .cpu = 0.0,
            .last_seen_ms = 0,
            .communication_latency_ms = 0,
            .healthy = 1,
            .overload_handled = 0
        },

        {
            .name = "NODE_B",
            .task = "PRESSURE_CONTROL",
            .cpu = 0.0,
            .last_seen_ms = 0,
            .communication_latency_ms = 0,
            .healthy = 1,
            .overload_handled = 0
        },

        {
            .name = "NODE_C",
            .task = "CONVEYOR_CONTROL",
            .cpu = 0.0,
            .last_seen_ms = 0,
            .communication_latency_ms = 0,
            .healthy = 1,
            .overload_handled = 0
        },

        {
            .name = "NODE_D",
            .task = "NONE",
            .cpu = 0.0,
            .last_seen_ms = 0,
            .communication_latency_ms = 0,
            .healthy = 1,
            .overload_handled = 0
        }
    };


    /* =====================================================
     * INITIAL TASK CONFIGURATION
     * =====================================================
     */

    Task tasks[TASK_COUNT] = {

        {
            "TEMP_CONTROL",
            "NODE_A"
        },

        {
            "PRESSURE_CONTROL",
            "NODE_B"
        },

        {
            "CONVEYOR_CONTROL",
            "NODE_C"
        }
    };


    /* -----------------------------------------------------
     * Startup information
     * -----------------------------------------------------
     */

    printf("\n");

    printf(
        "========================================\n"
    );

    printf(
        " SOFTWARE-DEFINED FACTORY COORDINATOR\n"
    );

    printf(
        "========================================\n"
    );


    printf(
        "UDP PORT              : %d\n",
        PORT
    );


    printf(
        "HEARTBEAT TIMEOUT     : %d ms\n",
        HEARTBEAT_TIMEOUT_MS
    );


    printf(
        "CPU OVERLOAD THRESHOLD: %.2f%%\n",
        CPU_OVERLOAD_THRESHOLD
    );


    printf(
        "\nInitial task configuration:\n"
    );


    for (int i = 0; i < TASK_COUNT; i++) {

        printf(
            "TASK=%s -> NODE=%s\n",
            tasks[i].name,
            tasks[i].assigned_node
        );
    }


    printf("\n");

    printf(
        "Waiting for node heartbeats...\n"
    );

    printf(
        "----------------------------------------\n"
    );


    fflush(stdout);


    /* -----------------------------------------------------
     * Status timer
     * -----------------------------------------------------
     */

    long long last_status =
        timestamp_ms();


    /* =====================================================
     * MAIN COORDINATION LOOP
     * =====================================================
     */

    while (1) {

        /* -------------------------------------------------
         * Receive UDP heartbeat
         * -------------------------------------------------
         */

        char buffer[BUFFER_SIZE];


        struct sockaddr_in client;


        socklen_t client_len =
            sizeof(client);


        ssize_t received =
            recvfrom(
                sockfd,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *)&client,
                &client_len
            );


        if (received > 0) {

            buffer[received] =
                '\0';


            /* ---------------------------------------------
             * Heartbeat fields
             * ---------------------------------------------
             */

            char node_name[32];

            int cycle;

            long long sender_time;

            double cpu;

            char health[32];


            /* ---------------------------------------------
             * Parse heartbeat
             *
             * Expected:
             *
             * NODE=NODE_A
             * CYCLE=1
             * TIME_MS=12345
             * CPU=12.34
             * HEALTH=HEALTHY
             * ---------------------------------------------
             */

            int parsed =
                sscanf(
                    buffer,
                    "NODE=%31s CYCLE=%d TIME_MS=%lld CPU=%lf HEALTH=%31s",
                    node_name,
                    &cycle,
                    &sender_time,
                    &cpu,
                    health
                );


            if (parsed == 5) {

                int node_index =
                    find_node(
                        nodes,
                        node_name
                    );


                if (node_index >= 0) {

                    /* -------------------------------------
                     * Update runtime state
                     * -------------------------------------
                     */

                    long long receive_time =
                        timestamp_ms();

                    nodes[node_index].last_seen_ms =
                        receive_time;

                    nodes[node_index].communication_latency_ms =
                        receive_time - sender_time;


                    nodes[node_index].cpu =
                        cpu;


                    /* -------------------------------------
                     * Reset overload latch when CPU
                     * returns below threshold.
                     * -------------------------------------
                     */

                    if (cpu < CPU_OVERLOAD_THRESHOLD) {

                        nodes[node_index].overload_handled =
                            0;
                    }


                    /* -------------------------------------
                     * Detect recovery from failed state.
                     * -------------------------------------
                     */

                    if (!nodes[node_index].healthy) {

                        printf(
                            "NODE_RECOVERY_DETECTED NODE=%s\n",
                            node_name
                        );
                    }


                    /*
                     * Mark node healthy.
                     */

                    nodes[node_index].healthy =
                        1;


                    /* -------------------------------------
                     * Print received heartbeat
                     * -------------------------------------
                     */

                    printf(
                        "HEARTBEAT_RECEIVED "
                        "NODE=%s "
                        "CYCLE=%d "
                        "CPU=%.2f%% "
                        "HEALTH=%s\n",
                        node_name,
                        cycle,
                        cpu,
                        health
                    );


                    fflush(stdout);
                }
            }
        }


        /* -------------------------------------------------
         * Check for node failures
         * -------------------------------------------------
         */

        check_node_failures(
            nodes,
            tasks
        );


        /* -------------------------------------------------
         * Check for CPU overload
         * -------------------------------------------------
         */

        check_node_overload(
            nodes,
            tasks
        );


        /* -------------------------------------------------
         * Print status once per second
         * -------------------------------------------------
         */

        long long now =
            timestamp_ms();


        if (now - last_status >= 1000) {

            print_status(
                nodes,
                tasks
            );


            last_status =
                now;
        }


        /* -------------------------------------------------
         * Small scheduling interval
         * -------------------------------------------------
         */

        usleep(10000);
    }


    /* -----------------------------------------------------
     * Cleanup
     * -----------------------------------------------------
     */

    close(sockfd);


    return 0;
}
