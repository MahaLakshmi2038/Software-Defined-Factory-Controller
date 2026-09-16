# Experiment Results

## Project
**Software-Defined Distributed Real-Time Factory Controller with Dynamic Task Reconfiguration**

## Research Question

Can runtime task reconfiguration in a software-defined distributed PREEMPT_RT Linux factory controller maintain real-time performance and improve resilience under changing workload, communication delay, and node-failure conditions?

---

## Experimental Environment

- Host OS: Windows 11
- Development environment: WSL2 Ubuntu 24.04.4 LTS
- WSL kernel: PREEMPT_DYNAMIC (not PREEMPT_RT)
- Control nodes: software-emulated processes
- Communication: UDP
- Coordinator: C
- Sensors/actuators: software simulated
- Node topology: NODE_A, NODE_B, NODE_C, NODE_D
- NODE_D initially acts as a spare node
- Current measurements are prototype/emulation measurements and are not claimed as PREEMPT_RT hard-real-time measurements.

---

## Baseline / Normal Operation

### E1/E2 – Normal Operation

The distributed controller was tested under normal workload with the following initial task allocation:

- NODE_A → TEMP_CONTROL
- NODE_B → PRESSURE_CONTROL
- NODE_C → CONVEYOR_CONTROL
- NODE_D → spare

Previously recorded normal-run timing:

- Samples: 100
- Minimum timing error: 159067 ns
- Maximum timing error: 971521 ns
- Average timing error: 516271 ns
- Minimum UDP latency: 88.397 µs
- Maximum UDP latency: 509.751 µs
- Average UDP latency: 209.751 µs
- Faults: 0

---

# E3 – CPU Overload

## Objective

Evaluate whether the coordinator can detect an overloaded node and dynamically reassign its affected control task.

## Injection

NODE_B was started using the controlled overload mode:

```bash
./nodes/node_agent NODE_B --overload
```

The overload mode reports NODE_B as having 100% simulated CPU utilization.

## Observed Event

```text
*** DYNAMIC RECONFIGURATION TRIGGERED ***
TASK=PRESSURE_CONTROL
FROM_NODE=NODE_B
TO_NODE=NODE_D
RECONFIGURATION_TIME_US=9.253
RECONFIGURATION_STATUS=SUCCESS
TASK=PRESSURE_CONTROL NOW_ASSIGNED_TO=NODE_D
OVERLOAD_RECOVERY=SUCCESS
```

## Result

- Overloaded node: NODE_B
- Simulated CPU utilization: 100%
- Affected task: PRESSURE_CONTROL
- Original node: NODE_B
- Replacement node: NODE_D
- Reconfiguration time: 9.253 µs
- Reconfiguration status: SUCCESS
- Recovery status: SUCCESS

## Interpretation

The coordinator detected the injected overload condition, identified the affected task, selected an available healthy spare node, and logically reassigned the task.

The overload is a controlled simulated node condition and does not represent CPU utilization of an independent physical machine.

---
# E4 – Communication Delay

## Objective

Evaluate the coordinator response when communication between a node and the coordinator experiences an injected delay.

## Injection

NODE_B was started with a 100 ms heartbeat delay:

```bash
./nodes/node_agent NODE_B --delay-ms 100
```

## Observed Measurements

The coordinator measured heartbeat communication delays in the following range:

- Measured delay: 102–105 ms
- Injected delay: 100 ms
- Node health: HEALTHY
- Task: PRESSURE_CONTROL

Representative coordinator output:

```text
NODE=NODE_B CPU=0.00% HEALTH=HEALTHY TASK=PRESSURE_CONTROL LAST_SEEN=294 ms AGO HEARTBEAT_DELAY=105 ms
NODE=NODE_B CPU=0.08% HEALTH=HEALTHY TASK=PRESSURE_CONTROL LAST_SEEN=187 ms AGO HEARTBEAT_DELAY=103 ms
NODE=NODE_B CPU=0.07% HEALTH=HEALTHY TASK=PRESSURE_CONTROL LAST_SEEN=83 ms AGO HEARTBEAT_DELAY=102 ms
```

## Result

- Injected communication delay: 100 ms
- Measured heartbeat delay: 102–105 ms
- Node remained healthy.
- PRESSURE_CONTROL remained assigned to NODE_B.
- No reconfiguration was triggered.

## Interpretation

The coordinator successfully detected and recorded the injected communication delay while the node remained within the configured healthy state.

The reported value represents same-host heartbeat sender-to-coordinator communication delay based on timestamps, not network round-trip time.

---
# E5 – Sensor Fault and Fail-Safe Response

## Objective

Evaluate whether the temperature controller detects loss of sensor data and activates a fail-safe cooling response.

## Fault Injection

The temperature sensor was configured to stop providing data after cycle 5:

```bash
./sensors/temperature_sensor --fault-after 5
```

## Observed Event

```text
*** SENSOR FAULT DETECTED ***
Cycle: 006
Sensor data unavailable or invalid.
FAIL-SAFE ACTION: COOLING ON
FAULT_RESPONSE_TIME_NS=134241
FAIL_SAFE_STATUS=SUCCESS
COOLING_COMMAND=ON

Controller finished safely.
```

## Result

- Fault injected after cycle: 5
- Fault detected at cycle: 6
- Fail-safe action: COOLING ON
- Fault response time: 134.241 µs
- Fail-safe status: SUCCESS
- Cooling command: ON

## Interpretation

The controller detected the loss of valid sensor data and immediately issued a cooling ON command as the defined fail-safe action.

The measured response time represents the controller command response from fault handling to the cooling command. It does not represent physical actuator reaction time or complete physical fault-detection latency.

---
# E6 – Node Failure and Dynamic Recovery

## Objective

Evaluate whether the coordinator can detect a failed node using heartbeat monitoring and dynamically reassign the affected task to a healthy node.

## Fault Injection

NODE_B was terminated during a normal coordinator run to simulate node failure.

The coordinator uses a heartbeat timeout of approximately 3000 ms to determine node failure.

## Observed Event

```text
!!! NODE FAILURE DETECTED !!!
FAILED_NODE=NODE_B
LAST_HEARTBEAT_AGE_MS=3001
AVAILABLE_SPARE_NODE=NODE_A

*** DYNAMIC RECONFIGURATION TRIGGERED ***
TASK=PRESSURE_CONTROL
FROM_NODE=NODE_B
TO_NODE=NODE_A
RECONFIGURATION_TIME_US=11.528
RECONFIGURATION_STATUS=SUCCESS
TASK=PRESSURE_CONTROL NOW ASSIGNED_TO=NODE_A

NODE_FAILURE_RECOVERY=SUCCESS
```

## Result

- Failed node: NODE_B
- Failure detection threshold: approximately 3000 ms
- Observed heartbeat age at detection: 3001 ms
- Affected task: PRESSURE_CONTROL
- Replacement node: NODE_A
- Reconfiguration time: 11.528 µs
- Reconfiguration status: SUCCESS
- Node failure recovery: SUCCESS

## Final Node State

```text
NODE_A → HEALTHY → PRESSURE_CONTROL
NODE_B → FAILED → NONE
NODE_C → HEALTHY → CONVEYOR_CONTROL
NODE_D → HEALTHY → TEMP_CONTROL
```

## Interpretation

The coordinator detected the loss of NODE_B through heartbeat monitoring, identified the task affected by the failure, selected a healthy available node, and logically reassigned PRESSURE_CONTROL from NODE_B to NODE_A.

The measured reconfiguration time represents the coordinator logical task reassignment operation. It does not represent physical process migration or complete end-to-end recovery time.

---
# E7 – Dynamic Workload and Runtime Reconfiguration

## Objective

Evaluate whether the controller can respond to a changing workload condition by detecting an overloaded node and dynamically reassigning its affected task.

## Workload Pattern

NODE_B was started using the dynamic workload mode. The simulated workload changed during execution:

- Cycles 1–5: 5% CPU
- Cycles 6–10: 85% CPU
- Cycle 11 onward: 5% CPU

## Observed Event

```text
HEARTBEAT_RECEIVED NODE=NODE_B CYCLE=6 CPU=85.00% HEALTH=OVERLOADED

!!! NODE OVERLOAD DETECTED !!!
OVERLOADED_NODE=NODE_B
CPU_UTILIZATION=85.00%
TASK_AFFECTED=PRESSURE_CONTROL

*** DYNAMIC RECONFIGURATION TRIGGERED ***
TASK=PRESSURE_CONTROL
FROM_NODE=NODE_B
TO_NODE=NODE_D
RECONFIGURATION_TIME_US=3.693
RECONFIGURATION_STATUS=SUCCESS
TASK=PRESSURE_CONTROL NOW ASSIGNED_TO=NODE_D

OVERLOAD_RECOVERY=SUCCESS
```

## Final Node State

```text
NODE=NODE_B CPU=5.00% HEALTH=HEALTHY TASK=NONE
NODE=NODE_D CPU=0.00% HEALTH=HEALTHY TASK=PRESSURE_CONTROL
```

## Result

- Initial simulated CPU workload: 5%
- Overloaded workload: 85%
- Recovered workload: 5%
- Affected task: PRESSURE_CONTROL
- Task migration: NODE_B → NODE_D
- Reconfiguration time: 3.693 µs
- Reconfiguration status: SUCCESS
- Overload recovery: SUCCESS

## Interpretation

The controller detected the transition from normal to overloaded workload, reassigned PRESSURE_CONTROL to the available spare node, and maintained the task assignment after NODE_B returned to a healthy simulated workload level.

The workload values are simulated conditions used to evaluate the reconfiguration logic. They do not represent measured CPU utilization of independent physical nodes.

---
# Overall Experimental Summary

## Summary of Findings

The completed experiments demonstrate the core runtime monitoring, fault handling, and dynamic task reconfiguration capabilities of the software-defined distributed factory controller.

- Normal operation was completed without faults across 100 control cycles.
- CPU overload detection triggered dynamic reassignment of PRESSURE_CONTROL from NODE_B to NODE_D.
- Injected communication delay was detected and measured by the coordinator.
- Temperature sensor failure triggered the defined fail-safe cooling action.
- Node failure was detected through heartbeat monitoring and the affected task was reassigned to a healthy node.
- Dynamic workload changes from normal to overloaded and back to normal triggered runtime task reconfiguration.

## Reconfiguration Results

| Experiment | Trigger | Task | Reconfiguration | Reconfiguration Time | Status |
|---|---|---|---|---:|---|
| E3 | CPU overload | PRESSURE_CONTROL | NODE_B → NODE_D | 9.253 µs | SUCCESS |
| E6 | Node failure | PRESSURE_CONTROL | NODE_B → NODE_A | 11.528 µs | SUCCESS |
| E7 | Dynamic workload | PRESSURE_CONTROL | NODE_B → NODE_D | 3.693 µs | SUCCESS |

These measurements represent the coordinator logical task reassignment operation. They should not be interpreted as complete end-to-end physical recovery time or operating-system process migration time.

## Fault and Safety Results

| Experiment | Fault Condition | Response | Measured Response | Status |
|---|---|---|---:|---|
| E4 | 100 ms injected communication delay | Delay monitored; node remained healthy | 102–105 ms measured heartbeat delay | SUCCESS |
| E5 | Temperature sensor failure | Cooling switched ON | 134.241 µs command response | SUCCESS |
| E6 | NODE_B failure | PRESSURE_CONTROL reassigned to NODE_A | 11.528 µs logical reconfiguration | SUCCESS |

## Research Interpretation

The experimental results provide evidence that the implemented coordinator can monitor software-emulated nodes, detect selected runtime fault or workload conditions, and perform logical task reassignment while maintaining the defined control logic.

The experiments specifically demonstrate runtime responses to CPU overload, communication delay, sensor failure, node failure, and changing workload conditions.

The current results establish the functional prototype and fault-handling behavior. Real-time performance validation under a PREEMPT_RT Linux environment remains a required next stage.

---
# Limitations and Required Next Validation

## Current Prototype Limitations

- The controller nodes are software-emulated processes running on the same host rather than independent physical industrial computers.
- The current task reassignment mechanism updates the logical task-to-node mapping; it does not perform operating-system process migration.
- CPU overload and dynamic workload experiments use controlled simulated CPU values rather than independently measured CPU utilization from separate physical nodes.
- Communication-delay measurements represent same-host heartbeat sender-to-coordinator delay and should not be interpreted as network round-trip time.
- Sensors and actuators are simulated software components rather than physical industrial devices.
- The current timing measurements were obtained in the development environment and do not establish hard real-time guarantees.
- The current WSL2 environment uses a PREEMPT_DYNAMIC kernel rather than a PREEMPT_RT kernel.

## Required PREEMPT_RT Validation

The next validation stage is to execute the controller in a Linux environment with PREEMPT_RT enabled and repeat the relevant timing experiments.

The PREEMPT_RT evaluation should measure:

- End-to-end control latency
- Timing jitter
- Deadline miss rate
- CPU utilization
- Heartbeat communication delay
- Fault detection time
- Logical reconfiguration time
- Recovery time

The PREEMPT_RT results should then be compared with the current development-environment measurements to evaluate whether real-time scheduling improves timing predictability under the tested workload and fault conditions.

## Experimental Status

Functional distributed-control and fault-recovery experiments: COMPLETED

PREEMPT_RT real-time validation: PENDING

Real-time dashboard integration: PENDING

Comparative baseline versus dynamic reconfiguration analysis: PENDING

---
