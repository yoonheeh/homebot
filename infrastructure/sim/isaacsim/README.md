# Homebot Simulation & Control

This directory contains the Software-in-the-Loop (SIL) simulation bridge and manual control interface for the homebot platform.

## 1. Launching the Simulation (Isaac Sim)

The simulation environment is built on OpenUSD and runs as a standalone Python application using Isaac Sim's bundled Python environment.

**Prerequisites:**
You must run the simulation script using the `python.sh` wrapper provided in your Isaac Sim root directory, *not* your system Python.

**Execution:**
Launch the simulation by providing the absolute path to the robot's `.usda` file.

```bash
${ISAACSIM_PYTHON_EXE} ~/homebot/infrastructure/sim/isaacsim/scripts/standalone_interactive_cmd.py --robot-path ~/homebot/infrastructure/sim/isaacsim/assets/homebot/homebot.usda 
```

Once launched, the terminal will indicate that it is listening for UDP motor commands on port `5005`, and the Isaac Sim window will open.

## 2. Launching Manual Control

The C++ control application can interface with either the physical hardware (via serial) or the simulated robot (via UDP).

**Execution:**
To drive the simulated robot, launch your compiled control binary and pass the `--mode sim` flag. This directs the `TeleopController` to use the `IsaacSimMotorDriver` instead of the serial hardware driver.

### Build
```bash
bazel build //apps/manual_control:control
```

### Execution
```bash
./bazel-bin/apps/manual_control/manual_control --mode sim
```

**Optional Arguments:**

* `--sim-ip <address>`: Target IP of the simulation host (default: `127.0.0.1`)
* `--sim-port <port>`: Target UDP port (default: `5005`)

**Controls:**

* **W / S:** Forward / Reverse
* **A / D:** Steer Left / Steer Right
* **Q:** Quit application safely

## 3. Socket Communication Architecture

The control binary and the simulation communicate asynchronously over a lightweight, non-blocking network socket.

* **Protocol:** UDP
* **Default Network:** `127.0.0.1:5005`
* **Payload Format:** A comma-separated string containing the left and right wheel target velocities in radians per second (e.g., `"1.5,1.5"`).

**Data Flow Sequence:**

1. **Transmit:** The C++ `IsaacSimMotorDriver` receives velocity targets from the teleop controller, encodes them into a string, and fires a UDP packet at 50Hz.
2. **Receive:** The simulation script executes a non-blocking read operation during every Omniverse physics step. It continuously drains the socket buffer to discard stale network packets, isolating the absolute newest command.
3. **Actuate:** The parsed string is converted from radians/sec to degrees/sec and applied directly to the OpenUSD `DriveAPI` attached to the four `PhysicsRevoluteJoint` wheel constraints.
