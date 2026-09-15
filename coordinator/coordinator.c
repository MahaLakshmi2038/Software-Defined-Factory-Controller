#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define NODE_COUNT 4
#define TASK_COUNT 3

typedef struct {
    char name[32];
    char task[32];
    double load;
    int healthy;
} Node;

typedef struct {
    char name[32];
    char assigned_node[32];
} Task;

static long long timestamp_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000000LL +
           ts.tv_nsec;
}

static void print_status(Node nodes[], Task tasks[])
{
    printf("\n----------------------------------------\n");
    printf(" Coordinator Status\n");
    printf("----------------------------------------\n");

    for (int i = 0; i < NODE_COUNT; i++) {
        printf(
            "NODE=%s LOAD=%.2f HEALTH=%s TASK=%s\n",
            nodes[i].name,
            nodes[i].load,
            nodes[i].healthy ? "HEALTHY" : "FAILED",
            nodes[i].task
        );
    }

    printf("\nTask assignments:\n");

    for (int i = 0; i < TASK_COUNT; i++) {
        printf(
            "TASK=%s -> NODE=%s\n",
            tasks[i].name,
            tasks[i].assigned_node
        );
    }

    printf("----------------------------------------\n");
}

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

static void reconfigure_task(
    Node nodes[],
    Task tasks[],
    int task_index,
    int old_node_index,
    int new_node_index
)
{
    long long start = timestamp_ns();

    printf("\n*** RECONFIGURATION TRIGGERED ***\n");

    printf(
        "TASK=%s\n",
        tasks[task_index].name
    );

    printf(
        "FROM=%s\n",
        nodes[old_node_index].name
    );

    printf(
        "TO=%s\n",
        nodes[new_node_index].name
    );

    /*
     * Remove task from failed/old node.
     */
    strcpy(
        nodes[old_node_index].task,
        "NONE"
    );

    /*
     * Assign task to available node.
     */
    strcpy(
        tasks[task_index].assigned_node,
        nodes[new_node_index].name
    );

    strcpy(
        nodes[new_node_index].task,
        tasks[task_index].name
    );

    long long end = timestamp_ns();

    printf(
        "RECONFIGURATION_TIME_US=%.3f\n",
        (end - start) / 1000.0
    );

    printf(
        "RECONFIGURATION_STATUS=SUCCESS\n"
    );

    printf(
        "************************************\n"
    );
}

int main(void)
{
    Node nodes[NODE_COUNT] = {
        {"NODE_A", "TEMP_CONTROL", 0.35, 1},
        {"NODE_B", "PRESSURE_CONTROL", 0.42, 1},
        {"NODE_C", "CONVEYOR_CONTROL", 0.28, 1},
        {"NODE_D", "NONE", 0.15, 1}
    };

    Task tasks[TASK_COUNT] = {
        {"TEMP_CONTROL", "NODE_A"},
        {"PRESSURE_CONTROL", "NODE_B"},
        {"CONVEYOR_CONTROL", "NODE_C"}
    };

    printf("========================================\n");
    printf(" Software-Defined Factory Coordinator\n");
    printf(" Dynamic Task Reconfiguration Test\n");
    printf("========================================\n");

    printf(
        "Coordinator started at %lld ns\n",
        timestamp_ns()
    );

    for (int cycle = 1; cycle <= 6; cycle++) {

        printf("\n[CYCLE %d]\n", cycle);

        /*
         * Normal operating conditions.
         */
        nodes[0].load = 0.35;
        nodes[1].load = 0.42;
        nodes[2].load = 0.28;
        nodes[3].load = 0.15;

        nodes[0].healthy = 1;
        nodes[1].healthy = 1;
        nodes[2].healthy = 1;
        nodes[3].healthy = 1;

        /*
         * NODE_D remains a spare node.
         */
        strcpy(
            nodes[3].task,
            "NONE"
        );

        /*
         * Simulate NODE_B failure at cycle 4.
         */
        if (cycle >= 4) {

            nodes[1].healthy = 0;
            nodes[1].load = -1.0;

            /*
             * Make sure failure is handled only once.
             */
            if (
                strcmp(
                    tasks[1].assigned_node,
                    "NODE_B"
                ) == 0
            ) {

                printf(
                    "\n!!! NODE FAILURE DETECTED !!!\n"
                );

                printf(
                    "FAILED_NODE=%s\n",
                    nodes[1].name
                );

                printf(
                    "DETECTION_TIME_NS=%lld\n",
                    timestamp_ns()
                );

                /*
                 * Search for a healthy,
                 * unassigned spare node.
                 */
                int new_node =
                    find_available_node(nodes);

                if (new_node >= 0) {

                    reconfigure_task(
                        nodes,
                        tasks,
                        1,
                        1,
                        new_node
                    );

                    printf(
                        "NODE_FAILURE_RECOVERY=SUCCESS\n"
                    );

                } else {

                    printf(
                        "NODE_FAILURE_RECOVERY=FAILED\n"
                    );

                    printf(
                        "REASON=NO_AVAILABLE_NODE\n"
                    );
                }
            }
        }

        print_status(nodes, tasks);

        sleep(2);
    }

    printf("\n========================================\n");
    printf(" Dynamic reconfiguration test completed\n");
    printf("========================================\n");

    return 0;
}
