# Software-Defined Factory Controller using Distributed RT Linux

## 1. Project Overview

This project implements a software-defined industrial factory control system using Linux-based real-time scheduling, inter-process communication, sensor-actuator pipelines, fault detection, and real-time monitoring.

The system demonstrates how factory operations can be controlled through software components running as independent processes. Sensor data is continuously collected, processed by real-time controllers, and used to control actuators.

The project currently implements two control chains:

1. **Temperature Control**

   * Temperature Sensor
   * Temperature Controller
   * Cooling Actuator

2. **Pressure Control**

   * Pressure Sensor
   * Pressure Controller
   * Conveyor Actuator

The system also includes UDP-based monitoring and performance measurement.

---

## 2. Objectives

The main objectives of this project are:

* Implement periodic real-time control loops in Linux.
* Use `SCHED_FIFO` real-time scheduling.
* Establish communication between independent processes.
* Implement sensor-to-controller-to-actuator pipelines.
* Apply threshold-based industrial control logic.
* Detect sensor and communication failures.
* Implement fail-safe actuator behavior.
* Monitor controller activity using UDP.
* Measure timing error and communication latency.
* Generate graphs for system performance analysis.
* Demonstrate a software-defined approach to factory automation.

---

## 3. System Architecture

### Temperature Control Chain

```text
+--------------------+
| Temperature Sensor |
+---------+----------+
          |
          | sensor_pipe
          v
+-----------------------+
| Temperature Controller|
|  Periodic RT Control  |
+----------+------------+
           |
           | actuator_pipe
           v
+--------------------+
|  Cooling Actuator  |
+--------------------+
```

### Monitoring Path

```text
Temperature Controller
          |
          | UDP :9000
          v
+-------------------------+
| Real-Time Monitor       |
| realtime_monitor.c     |
+-----------+-------------+
            |
            v
   realtime_monitor.csv
            |
            v
     Performance Graphs
```

### Pressure Control Chain

```text
+------------------+
| Pressure Sensor  |
+--------+---------+
         |
         | pressure_pipe
         v
+----------------------+
| Pressure Controller  |
+----------+-----------+
           |
           | conveyor_pipe
           v
+----------------------+
| Conveyor Actuator    |
+----------------------+
```

---

## 4. Real-Time Control

The temperature controller uses a periodic control loop with a **100 ms cycle period**.

The controller uses:

* `clock_nanosleep()`
* Absolute-time scheduling
* `SCHED_FIFO`
* Priority `50`
* Periodic sensor polling
* Processing-time measurement

The controller is executed using:

```bash
sudo chrt -f 50 ./controller
```

`SCHED_FIFO` provides real-time scheduling behavior within the Linux environment. However, the current implementation runs on a standard WSL2 Linux kernel and is therefore **not a PREEMPT_RT hard real-time system**.

---

## 5. Temperature Control Logic

The temperature threshold is:

```text
Temperature Limit = 70°C
```

The controller applies the following logic:

```text
Temperature > 70°C
        |
        v
   Cooling ON

Temperature <= 70°C
        |
        v
   Cooling OFF
```

The controller reads temperature values from the sensor FIFO and sends the corresponding cooling command to the actuator.

---

## 6. Fault Detection and Fail-Safe Operation

The controller includes fault detection mechanisms for:

* Sensor timeout
* Sensor disconnection
* FIFO/pipe errors
* Polling errors
* Invalid sensor data
* Impossible temperature values
* Communication problems

When a sensor or communication fault is detected, the controller enters a fail-safe state:

```text
Fault detected
      |
      v
Cooling = ON
```

This ensures that the system defaults to a safer operating state when reliable temperature information is unavailable.

---

## 7. Inter-Process Communication

The project uses Linux **named pipes (FIFOs)** for communication between independent processes.

### Temperature system

```text
sensor_pipe
actuator_pipe
```

### Pressure/conveyor system

```text
pressure_pipe
conveyor_pipe
```

Named pipes allow the sensor, controller, and actuator processes to communicate without requiring them to be part of the same program.

---

## 8. UDP Monitoring

The real-time controller sends monitoring information to UDP port `9000`.

The monitoring message contains:

```text
SEQ
SEND_NS
TEMP
COOLING
FAULT
```

The monitoring process records:

* Sequence number
* Temperature
* Cooling state
* Controller send timestamp
* Monitor receive timestamp
* UDP latency

The recorded data is stored in:

```text
realtime_monitor.csv
```

---

## 9. Performance Measurement

The system measures controller timing behavior using the difference between the expected periodic execution time and the actual execution time.

The final normal-operation run consisted of:

```text
Completed cycles: 100
Faults detected: 0
```

Timing results:

| Metric               |     Result |
| -------------------- | ---------: |
| Minimum timing error | 159.067 µs |
| Maximum timing error | 971.521 µs |
| Average timing error | 516.271 µs |
| Completed cycles     |        100 |
| Faults detected      |          0 |

The system successfully completed all 100 temperature-control cycles without detected faults during the final normal-operation test.

---

## 10. Test Scenarios

### Normal Operation

During normal operation:

* Temperature values were generated between approximately 60°C and 80°C.
* Values above 70°C activated cooling.
* Values at or below 70°C kept cooling OFF.
* 100 cycles were completed.
* No faults were detected.

Example behavior:

```text
68°C → Cooling OFF
74°C → Cooling ON
79°C → Cooling ON
60°C → Cooling OFF
75°C → Cooling ON
```

### Fault Injection

A fault scenario was also tested by running the controller when valid sensor data was unavailable.

The controller detected sensor timeouts and entered the fail-safe state:

```text
Sensor unavailable
       |
       v
Fault detected
       |
       v
Cooling ON
```

This demonstrated the fault-detection and fail-safe mechanism.

---

## 11. Performance Graphs

The project includes generated performance graphs:

* `temperature_vs_cycle.png`
* `cooling_state_vs_cycle.png`
* `timing_error.png`
* `udp_latency.png`

The final normal-operation results are stored in:

```text
results/final_normal_run/
```

Baseline measurements are stored in:

```text
results/baseline/
```

---

## 12. Project Structure

```text
factory-controller/
│
├── controller/
│   └── controller.c
│
├── sensors/
│   └── temperature_sensor.c
│
├── actuators/
│   ├── cooling_actuator.c
│   └── controller.c
│
├── pressure_sensor/
│   └── pressure_sensor.c
│
├── pressure_controller/
│   └── pressure_controller.c
│
├── conveyor_actuator/
│   └── conveyor_actuator.c
│
├── communication/
│   ├── udp_sender.c
│   └── udp_receiver.c
│
├── monitoring/
│   ├── realtime_monitor.c
│   ├── monitor.c
│   ├── generate_graphs.py
│   └── performance graphs
│
├── results/
│   ├── baseline/
│   └── final_normal_run/
│
├── tests/
│
├── docs/
│
├── .gitignore
└── README.md
```

---

## 13. Software Requirements

The project was developed and tested using:

* Ubuntu 24.04.4 LTS
* WSL2
* Linux kernel 6.18.33.2-microsoft-standard-WSL2
* GCC
* Python 3
* Matplotlib
* Linux `chrt` utility

---

## 14. Compilation

### Temperature Sensor

```bash
cd sensors
gcc temperature_sensor.c -o temperature_sensor
```

### Cooling Actuator

```bash
cd actuators
gcc cooling_actuator.c -o cooling_actuator
```

### Temperature Controller

```bash
cd controller
gcc controller.c -o controller
```

### Monitoring

```bash
cd monitoring
gcc realtime_monitor.c -o realtime_monitor
```

### Pressure Sensor

```bash
cd pressure_sensor
gcc pressure_sensor.c -o pressure_sensor
```

### Pressure Controller

```bash
cd pressure_controller
gcc pressure_controller.c -o pressure_controller
```

### Conveyor Actuator

```bash
cd conveyor_actuator
gcc conveyor_actuator.c -o conveyor_actuator
```

---

## 15. Running the Temperature Control System

Create the required FIFOs if they do not already exist:

```bash
cd ~/factory-controller

mkfifo sensor_pipe
mkfifo actuator_pipe
```

Start the monitoring process:

```bash
cd monitoring
./realtime_monitor
```

Start the cooling actuator:

```bash
cd ../actuators
./cooling_actuator
```

Start the real-time controller:

```bash
cd ../controller
sudo chrt -f 50 ./controller
```

Finally, start the temperature sensor:

```bash
cd ../sensors
./temperature_sensor
```

The complete control flow is:

```text
Sensor
  ↓
sensor_pipe
  ↓
Real-Time Controller
  ↓
actuator_pipe
  ↓
Cooling Actuator

Controller
  ↓ UDP :9000
Real-Time Monitor
```

---

## 16. Generating Performance Graphs

After collecting monitoring data:

```bash
cd ~/factory-controller/monitoring
python3 generate_graphs.py
```

The script generates:

```text
temperature_vs_cycle.png
cooling_state_vs_cycle.png
timing_error.png
udp_latency.png
```

---

## 17. Limitations

The current implementation has several limitations:

1. The system runs on WSL2 rather than a dedicated real-time Linux system.
2. The current kernel is not configured with `PREEMPT_RT`.
3. Therefore, the measured timing behavior should not be interpreted as hard real-time guarantees.
4. UDP communication currently uses `127.0.0.1`, so the distributed components are simulated as independent processes on the same host.
5. Sensors and actuators are software simulations rather than physical industrial hardware.
6. The current controller uses threshold-based logic rather than advanced control algorithms.

---

## 18. Future Improvements

Possible future enhancements include:

* Deploying the system on multiple Linux/RT Linux nodes.
* Using a PREEMPT_RT kernel for stronger real-time guarantees.
* Connecting physical industrial sensors and actuators.
* Adding CAN, Modbus, EtherCAT, or industrial Ethernet communication.
* Implementing PID or model-predictive control.
* Adding a web-based real-time dashboard.
* Adding persistent databases for monitoring data.
* Implementing redundant controllers.
* Adding watchdog-based recovery.
* Expanding fault diagnosis using machine learning.
* Containerizing selected non-real-time services.

---

## 19. Conclusion

This project demonstrates a software-defined factory control architecture using Linux processes, real-time scheduling, named-pipe communication, UDP monitoring, fault detection, and fail-safe control.

The final normal-operation test successfully completed **100 control cycles with zero detected faults**, while the system continuously monitored temperature, controlled cooling based on the configured threshold, and recorded timing and communication performance.

The project provides a foundation for extending the prototype toward a more distributed and industrial real-time control architecture.

---

## 20. Repository

GitHub:

https://github.com/MahaLakshmi2038/Software-Defined-Factory-Controller
