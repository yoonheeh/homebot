"""
Usage: standalone script that will launch Isaac Sim with the Office environment,
the homebot platform, and UDP teleop control.
"""

import argparse
import math
import socket

# 1. Parse arguments before loading SimulationApp
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
from pxr import UsdPhysics, UsdGeom, Gf

# 2. Create a stable local stage FIRST
omni.usd.get_context().new_stage()
for _ in range(5):
    simulation_app.update()

stage = omni.usd.get_context().get_stage()

# 3. Reference the remote Office environment into your stage
ENV_PATH = "https://omniverse-content-production.s3-us-west-2.amazonaws.com/Assets/Isaac/6.0/Isaac/Environments/Office/office.usd"
print(f"Referencing remote environment from: {ENV_PATH}")
env_prim = stage.DefinePrim("/World/Office", "Xform")
env_prim.GetReferences().AddReference(ENV_PATH)

# 4. Reference your local robot into the stage alongside the office
ROBOT_PATH = args.robot_path
robot_prim = stage.DefinePrim("/World/homebot", "Xform")
robot_prim.GetReferences().AddReference(ROBOT_PATH)

# Elevate the robot slightly so it doesn't clip into the floor on spawn
xformable = UsdGeom.Xformable(robot_prim)
translate_op = xformable.AddTranslateOp()
translate_op.Set(Gf.Vec3d(0.0, 0.0, 0.1))

# Tick enough times to let the geometry initialize
for _ in range(20):
    simulation_app.update()

# 5. Setup UDP socket
UDP_IP = "127.0.0.1"
UDP_PORT = 5005
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)

# 6. Dynamically find joints and setup drives
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

for prim in left_joints + right_joints:
    drive = UsdPhysics.DriveAPI.Get(prim, "angular")
    if not drive:
        drive = UsdPhysics.DriveAPI.Apply(prim, "angular")
    drive.CreateTypeAttr("velocity")
    drive.CreateDampingAttr(100000.0)
    drive.CreateStiffnessAttr(0.0)
    drive.CreateTargetVelocityAttr(0.0)

RAD2DEG = 180.0 / math.pi


def apply_velocity(prims, velocity_rad):
    velocity_deg = velocity_rad * RAD2DEG
    for prim in prims:
        drive = UsdPhysics.DriveAPI.Get(prim, "angular")
        if drive:
            drive.GetTargetVelocityAttr().Set(velocity_deg)


# 7. Synchronized Physics Loop (Control Only)
def on_physics_step(e):
    latest_data = None

    try:
        while True:
            latest_data, addr = sock.recvfrom(1024)
    except BlockingIOError:
        pass

    if latest_data:
        try:
            # Parse and apply control
            left_rad, right_rad = map(float, latest_data.decode("utf-8").split(","))
            apply_velocity(left_joints, left_rad)
            apply_velocity(right_joints, right_rad)

        except Exception as err:
            print(f"UDP Error: {err}")


# Subscribe and play
timeline_stream = omni.timeline.get_timeline_interface().get_timeline_event_stream()
sub = timeline_stream.create_subscription_to_pop(on_physics_step)
omni.timeline.get_timeline_interface().play()

print(f"Isaac Sim listening for C++ velocities on port {UDP_PORT}...")

while simulation_app.is_running():
    simulation_app.update()

simulation_app.close()
