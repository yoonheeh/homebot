# Wheel Radius / Odometry Calibration

The host-side `StateEstimator` needs three physical parameters to convert encoder ticks and IMU rates into robot pose:

- `wheel_radius` — effective wheel radius in meters.
- `wheel_base` — distance between the left and right wheel contact patches in meters (measure with a ruler).
- `ticks_per_rev` — encoder counts per wheel revolution (depends on decoder mode in firmware).

`wheel_radius` is hard to measure accurately with a ruler because it depends on tire compression and surface. The `calibrate_encoders` tool drives the robot straight for a fixed time and uses the measured travel distance to compute the effective `wheel_radius`.

## What you need

- Homebot Pico firmware running and connected to the host via USB (default `/dev/ttyACM0`).
- The `calibrate_encoders` host binary built:
  ```bash
  bazel build //pico_interface:calibrate_encoders
  ```
- A flat surface with enough room to drive the robot in a straight line for ~0.5–1 m.
- A tape measure or ruler to measure the distance traveled.

## Expected `ticks_per_rev`

| Decoding mode | Counts per revolution |
|---------------|----------------------:|
| 1X (single edge)  | ~1,080 |
| 4X (quadrature)   | ~4,320 |

If you do not know your firmware's decoding mode, check the encoder-driver code in `pico_firmware/`.

## Automated straight-line calibration

On this robot, the motors are mounted so that a command of `left = -4.0 rad/sec` and `right = +4.0 rad/sec` drives both wheels in the same physical direction, moving the robot straight forward.

The calibration tool sends that command for 2 seconds, then asks you to measure how far the robot actually moved. From the wheel rotation and the measured distance, it computes the effective wheel radius.

### Run the calibration

```bash
# Default values (wheel_radius=0.0325 m, wheel_base=0.16 m, ticks_per_rev=4320.0)
bazel run //pico_interface:calibrate_encoders

# Or override defaults
bazel run //pico_interface:calibrate_encoders -- 0.0325 0.16 4320.0 /dev/ttyACM0 data/robot_calibration.txt
```

To run the calibration directly on the board:

```bash
bazel run --config=arm64 //pico_interface:run_calibrate_encoders -- user@board-host
```

### Procedure

1. **Place the robot on a flat surface** and mark its starting pose as `(0, 0, 0)`. Make sure it has room to drive straight for roughly the expected distance.

2. **Run the tool** and press `ENTER` when ready. The robot will drive forward for 2 seconds:
   - Left wheel command: `-4.0 rad/sec`
   - Right wheel command: `4.0 rad/sec`
   - Expected distance: `8 rad × 0.0325 m = 0.260 m`

3. **Measure the actual straight-line distance** the robot traveled, in meters.

4. **Enter the distance** when prompted. The tool computes a scale factor rather than overwriting the nominal wheel radius:
   ```text
   wheel_rotation = 4.0 rad/sec × 2 sec = 8 rad
   expected_distance = 8 rad × 0.0325 m
   scale_factor = actual_distance / expected_distance
   effective_wheel_radius = 0.0325 m × scale_factor
   ```

5. The tool writes all parameters to the calibration file (default `data/robot_calibration.txt`):
   ```text
   wheel_radius=0.032500
   wheel_base=0.160000
   ticks_per_rev=4320.000000
   scale_factor=1.000000
   ```

The state estimator keeps the nominal `wheel_radius` and multiplies it by `scale_factor` at runtime, so you can always see both the original spec value and the applied correction.

## Example run

```text
=======================================================================
               WHEEL RADIUS / ODOMETRY CALIBRATION TOOL                
=======================================================================
This tool drives the robot straight for a fixed time and uses the
measured travel distance to compute the effective wheel radius.
Configuration:
  Wheel radius   : 0.0325 m
  Wheel base     : 0.1600 m
  Ticks per rev  : 4320.0
  Serial port    : /dev/ttyACM0
  Output file    : data/robot_calibration.txt
=======================================================================

STEP 1: Place the robot on a flat surface and mark its starting
        position as 0, 0, 0. Make sure it has room to drive forward.

Press ENTER when ready...

STEP 2: Driving straight for 2 seconds...
  Left  wheel command: -4.0 rad/sec
  Right wheel command: 4.0 rad/sec
  Expected distance  : 0.2600 m

  /home/firefly/homebot/bin/control -4.00 4.00 /dev/ttyACM0
Opening serial port /dev/ttyACM0...
Sending command:
  - Left Target  : -4.00 rad/sec
  - Right Target : 4.00 rad/sec
  - Packet CRC16 : 0x85fa
Successfully sent command (12 bytes written).

Stopping wheels...
  /home/firefly/homebot/bin/control 0.00 0.00 /dev/ttyACM0
Opening serial port /dev/ttyACM0...
Sending command:
  - Left Target  : 0.00 rad/sec
  - Right Target : 0.00 rad/sec
  - Packet CRC16 : 0x313e
Successfully sent command (12 bytes written).

STEP 3: Measure how far the robot actually traveled in a straight line
        and enter the distance in meters.
Actual distance (meters): 0.203

=======================================================================
Calibration result:
  Measured distance  : 0.2030 m
  Expected distance  : 0.2600 m
  Wheel rotation     : 8.0 rad
  Scale factor       : 0.780769
  Effective radius   : 0.0254 m
  Writing to         : data/robot_calibration.txt
=======================================================================
Calibration saved. You can now run the state estimator with:
  bazel run --config=arm64 //pico_interface:run_state_estimator -- user@host
(ensure data/robot_calibration.txt is present on the board)
```

## Running the state estimator with the calibration

The `pico_interface` state estimator reads `data/robot_calibration.txt` by default. If the file is missing, it falls back to built-in defaults and logs a warning.

```bash
# Local
bazel run //pico_interface:pico_interface

# On the board
bazel run --config=arm64 //pico_interface:run_state_estimator -- user@board-host
```

If you saved the calibration file to a different path, pass it as the first argument:

```bash
bazel run //pico_interface:pico_interface -- /path/to/robot_calibration.txt
```

## Calibrating `wheel_base`

The tool does **not** calibrate `wheel_base`. Measure the distance between the left and right wheel centers with a ruler and pass it as the second argument:

```bash
bazel run //pico_interface:calibrate_encoders -- 0.0325 0.155 4320.0
```

If the robot's turns look too large or too small in odometry, double-check this measurement.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `Failed to open serial port` | Pico not plugged in or port busy | Check USB cable and close other serial monitors |
| Robot does not move | Motor power off or wiring issue | Check power and motor connections |
| Robot turns instead of driving straight | Left/right motors wired opposite to assumption | Swap motor leads or change the sign of one velocity in the tool |
| Measured distance is very different from expected | Default `wheel_radius` is far off | That is normal; the calibration will correct it |
| State estimator drifts after calibration | Calibration file not copied to board | Ensure `data/robot_calibration.txt` is present next to the binary on the board |
