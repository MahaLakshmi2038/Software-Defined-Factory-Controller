import csv
import matplotlib.pyplot as plt

cycles = []
temperatures = []
cooling = []
timing_error = []
udp_latency = []

previous_time = None

with open("realtime_monitor.csv", "r") as file:
    reader = csv.DictReader(file)

    for row in reader:
        cycle = int(row["sequence"])
        temp = float(row["temperature"])
        cool = int(row["cooling"])
        send_time = int(row["send_time_ns"])
        latency = int(row["udp_latency_ns"])

        cycles.append(cycle)
        temperatures.append(temp)
        cooling.append(cool)
        udp_latency.append(latency / 1000)

        if previous_time is not None:
            interval = send_time - previous_time
            error = abs(interval - 100_000_000)
            timing_error.append(error / 1000)

        previous_time = send_time


# Graph 1: Temperature
plt.figure(figsize=(10, 5))
plt.plot(cycles, temperatures)
plt.axhline(70, linestyle="--", label="Cooling Threshold (70°C)")
plt.xlabel("Cycle")
plt.ylabel("Temperature (°C)")
plt.title("Factory Temperature vs Control Cycle")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("temperature_vs_cycle.png", dpi=300)
plt.close()


# Graph 2: Cooling State
plt.figure(figsize=(10, 4))
plt.step(cycles, cooling, where="post")
plt.xlabel("Cycle")
plt.ylabel("Cooling State")
plt.title("Cooling Actuator State vs Control Cycle")
plt.yticks([0, 1], ["OFF", "ON"])
plt.grid(True)
plt.tight_layout()
plt.savefig("cooling_state_vs_cycle.png", dpi=300)
plt.close()


# Graph 3: Timing Error
plt.figure(figsize=(10, 5))
plt.plot(cycles[1:], timing_error)
plt.xlabel("Cycle")
plt.ylabel("Timing Error (µs)")
plt.title("Control Cycle Timing Error")
plt.grid(True)
plt.tight_layout()
plt.savefig("timing_error.png", dpi=300)
plt.close()


# Graph 4: UDP Latency
plt.figure(figsize=(10, 5))
plt.plot(cycles, udp_latency)
plt.xlabel("Cycle")
plt.ylabel("UDP Latency (µs)")
plt.title("UDP Communication Latency per Control Cycle")
plt.grid(True)
plt.tight_layout()
plt.savefig("udp_latency.png", dpi=300)
plt.close()


print("Graphs generated successfully!")
print("1. temperature_vs_cycle.png")
print("2. cooling_state_vs_cycle.png")
print("3. timing_error.png")
print("4. udp_latency.png")
