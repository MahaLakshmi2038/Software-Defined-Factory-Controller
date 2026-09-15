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

typedef struct {
    char name[32];
    char task[32];
    double cpu;
    long long last_seen_ms;
    int healthy;
} Node;

typedef struct {
    char name[32];
    char assigned_node[32];
} Task;


/* ---------- TIME ---------- */

static long long timestamp_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000LL +
           ts.tv_nsec / 1000000LL;
}


/* ---------- NODE LOOKUP ---------- */

static int find_node(Node nodes[], const char *name)
{
    for (int i = 0; i < NODE_COUNT; i++) {

        if (strcmp(nodes[i].name, name) == 0)
            return i;
    }

    return -1;
}


/* ---------- FIND SPARE NODE ---------- */

static int find_available_node(Node nodes[])
{
    for (int i = 0; i < NODE_COUNT; i++) {

        if (nodes[i].healthy &&
            strcmp(nodes[i].task, "NONE") == 0) {

            return i;
        }
    }

    return -1;
}


/* ---------- FIND TASK ---------- */

static int find_task(Task tasks[], const char *task_name)
{
    for (int i = 0; i < TASK_COUNT; i++) {

        if (strcmp(tasks[i].name, task_name) == 0)
            return i;
    }

    return -1;
}


/* ---------- PRINT STATUS ---------- */

static void print_status(Node nodes[], Task tasks[])
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

        printf(
            "NODE=%s CPU=%.2f HEALTH=%s TASK=%s LAST_SEEN=%lld ms AGO\n",
            nodes[i].name,
            nodes[i].cpu,
            nodes[i].healthy ? "HEALTHY" : "FAILED",
            nodes[i].task,
            age
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


/* ---------- RECONFIGURATION ---------- */

static void reconfigure_task(
    Node nodes[],
    Task tasks[],
    int task_index,
    int old_node_index,
    int new_node_index)
{
    long long start_ns;
    long long end_ns;

    start_ns = timestamp_ms() * 1000000LL;

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
     * Remove task from failed node.
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

    strcpy(
        tasks[task_index].assigned_node,
        nodes[new_node_index].name
    );

    end_ns = timestamp_ms() * 1000000LL;

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


/* ---------- NODE FAILURE HANDLING ---------- */

static void check_node_failures(
    Node nodes[],
    Task tasks[])
{
    long long now = timestamp_ms();

    for (int i = 0; i < NODE_COUNT; i++) {

        /*
         * Ignore nodes that have never sent a heartbeat.
         */
        if (nodes[i].last_seen_ms == 0)
            continue;

        long long age =
            now - nodes[i].last_seen_ms;

        /*
         * Detect timeout.
         */
        if (age > HEARTBEAT_TIMEOUT_MS &&
            nodes[i].healthy) {

            printf("\n");
            printf("!!! NODE FAILURE DETECTED !!!\n");
            printf(
                "FAILED_NODE=%s\n",
                nodes[i].name
            );

            printf(
                "LAST_HEARTBEAT_AGE_MS=%lld\n",
                age
            );

            nodes[i].healthy = 0;

            /*
             * If the failed node owns a task,
             * find that task.
             */
            if (strcmp(nodes[i].task, "NONE") != 0) {

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


/* ---------- MAIN ---------- */

int main(void)
{
    int sockfd;

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


    /*
     * Allow quick restart after Ctrl+C.
     */
    int reuse = 1;

    setsockopt(
        sockfd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );


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


    if (bind(
            sockfd,
            (struct sockaddr *)&server,
            sizeof(server)) < 0) {

        perror("bind");

        close(sockfd);

        return 1;
    }


    /*
     * Non-blocking UDP socket.
     */

    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {

        perror("fcntl");

        close(sockfd);

        return 1;
    }


    /*
     * Initial distributed factory configuration.
     */
    Node nodes[NODE_COUNT] = {

        {
            "NODE_A",
            "TEMP_CONTROL",
            0.0,
            0,
            0
        },

        {
            "NODE_B",
            "PRESSURE_CONTROL",
            0.0,
            0,
            0
        },

        {
            "NODE_C",
            "CONVEYOR_CONTROL",
            0.0,
            0,
            0
        },

        {
            "NODE_D",
            "NONE",
            0.0,
            0,
            0
        }
    };


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


    printf("\n");
    printf("========================================\n");
    printf(" SOFTWARE-DEFINED FACTORY COORDINATOR\n");
    printf("========================================\n");
    printf("UDP PORT              : %d\n", PORT);
    printf("HEARTBEAT TIMEOUT     : %d ms\n",
           HEARTBEAT_TIMEOUT_MS);

    printf("\nInitial task configuration:\n");

    for (int i = 0; i < TASK_COUNT; i++) {

        printf(
            "TASK=%s -> NODE=%s\n",
            tasks[i].name,
            tasks[i].assigned_node
        );
    }

    printf("\n");
    printf("Waiting for node heartbeats...\n");
    printf("----------------------------------------\n");

    fflush(stdout);


    long long last_status =
        timestamp_ms();


    while (1) {

        /*
         * Receive UDP heartbeat.
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


            char node_name[32];
            int cycle;
            long long sender_time;
            double cpu;
            char health[32];


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

                    /*
                     * Update runtime state.
                     */
                    nodes[node_index].last_seen_ms =
                        timestamp_ms();

                    nodes[node_index].cpu = cpu;

                    /*
                     * Node recovered if it was
                     * previously failed.
                     */
                    if (!nodes[node_index].healthy) {

                        printf(
                            "NODE_RECOVERY_DETECTED NODE=%s\n",
                            node_name
                        );
                    }

                    nodes[node_index].healthy =
                        1;


                    printf(
                        "HEARTBEAT_RECEIVED NODE=%s CYCLE=%d CPU=%.2f\n",
                        node_name,
                        cycle,
                        cpu
                    );

                    fflush(stdout);
                }
            }
        }


        /*
         * Check for node failures.
         */
        check_node_failures(
            nodes,
            tasks
        );


        /*
         * Print status once per second.
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


        /*
         * Small scheduling interval.
         */
        usleep(10000);
    }


    close(sockfd);

    return 0;
}
