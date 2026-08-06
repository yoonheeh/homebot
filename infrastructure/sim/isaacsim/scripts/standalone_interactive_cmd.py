"""
Usage: standalone script that will launch Isaac Sim along with assets
"""

import argparse

parser = argparse.ArgumentParser(description="Isaac Sim HIL Bridge")
parser.add_argument(
    "--robot-path",
    type=str,
    required=True,
    help="Absolute path to the robot USD/USDA file",
)

args, unknown = parser.parse_known_args()

from isaacsim import SimulationApp

simulation_app = SimulationApp({"headless": False})

import omni.timeline
import omni.usd
import omni.kit.commands
from pxr import UsdPhysics, Sdf, Gf, UsdLux
import math
import socket

# Create a new empty stage
omni.usd.get_context().new_stage()

# Tick a few times to let the empty stage initialize
for _ in range(5):
    simulation_app.update()

stage = omni.usd.get_context().get_stage()

# Add default lighting (Replicates UI lighting)
distant_light = UsdLux.DistantLight.Define(stage, Sdf.Path("/World/defaultLight"))
distant_light.CreateIntensityAttr(3000.0)
distant_light.CreateAngleAttr(1.0)  # Softens shadows slightly

# Add the Flat Grid ground plane (Replicates Create -> Environment)
omni.kit.commands.execute(
    "AddGroundPlaneCommand",
    stage=stage,
    planePath="/World/GroundPlane",
    axis="Z",
    size=1000.0,
    position=Gf.Vec3f(0.0, 0.0, 0.0),
    color=Gf.Vec3f(0.2, 0.2, 0.2),
)

# Reference robot into the environment
ROBOT_PATH = args.robot_path

# Create a parent container for the robot and load the file as a reference
robot_prim = stage.DefinePrim("/World/homebot", "Xform")
robot_prim.GetReferences().AddReference(ROBOT_PATH)

# Wait for the stage to fully load before querying it
for _ in range(10):
    simulation_app.update()

# Setup UDP socket
# Configure the UDP Socket
UDP_IP = "127.0.0.1"
UDP_PORT = 5005
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)  # Must be non-blocking so it doesn't freeze Isaac Sim

# Dynamically find joints
left_joints = []
right_joints = []

for prim in stage.Traverse():
    if prim.GetTypeName() == "PhysicsRevoluteJoint":
        name = prim.GetName().lower()
        if "left" in name and "wheel" in name:
            left_joints.append(prim)
        elif "right" in name and "wheel" in name:
            right_joints.append(prim)

print(
    f"Discovered {len(left_joints)} left joints and {len(right_joints)} right joints."
)


def setup_wheel_drives(prims):
    for prim in prims:
        if not prim.IsValid():
            print(f"Warning: Invalid prim path - please check your USD path")
            continue

        # Get the drive, or Apply it if it doesn't exist yet
        drive = UsdPhysics.DriveAPI.Get(prim, "angular")
        if not drive:
            drive = UsdPhysics.DriveAPI.Apply(prim, "angular")

        # Configure for velocity control (Damping acts as motor torque)
        drive.CreateTypeAttr("velocity")
        drive.CreateDampingAttr(100000.0)  # Strong motor
        drive.CreateStiffnessAttr(0.0)  # No position holding spring
        drive.CreateTargetVelocityAttr(0.0)

        print(f"Initialized DriveAPI on: {prim.GetName()}")


# Apply the setup to all four wheels
setup_wheel_drives(left_joints + right_joints)


def apply_velocity(prims, velocity_rad):
    velocity_deg = velocity_rad * 180.0 / math.pi
    for prim in prims:
        # Note: Ensure these prims are actual Physics Joints (e.g., PhysicsRevoluteJoint)
        # in your USD, otherwise DriveAPI will return None.
        drive = UsdPhysics.DriveAPI.Get(prim, "angular")
        if drive:
            drive.GetTargetVelocityAttr().Set(velocity_deg)


def on_physics_step(e):
    try:
        # Try to read the latest packet
        data, addr = sock.recvfrom(1024)
        left_vel, right_vel = map(float, data.decode("utf-8").split(","))
        apply_velocity(left_joints, left_vel)
        apply_velocity(right_joints, right_vel)

    except BlockingIOError:
        pass  # No new data this frame
    except Exception as err:
        print(f"UDP Error: {err}")


# Subscribe to the Omniverse physics loop
timeline_stream = omni.timeline.get_timeline_interface().get_timeline_event_stream()
sub = timeline_stream.create_subscription_to_pop(on_physics_step)

# Start the simulation playback automatically
omni.timeline.get_timeline_interface().play()
print(f"Isaac Sim listening for C++ velocities on port {UDP_PORT}...")

# Main application loop
# The timeline subscription will fire automatically while this loop runs
while simulation_app.is_running():
    simulation_app.update()

simulation_app.close()
