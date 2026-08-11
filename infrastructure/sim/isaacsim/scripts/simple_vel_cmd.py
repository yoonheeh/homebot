"""
Usage: simple script to be run within isaac sim
"""

import omni.usd
from pxr import UsdPhysics

stage = omni.usd.get_context().get_stage()

# Target velocity in degrees per second (360 = 1 rotation per second)
target_velocity = 0.0

found_wheels = False

# Iterate through all objects in the stage
for prim in stage.Traverse():
    name = prim.GetName().lower()

    # Look for PhysicsRevoluteJoints that belong to wheels
    if prim.GetTypeName() == "PhysicsRevoluteJoint" and "wheel" in name:
        # Apply the USD Physics Drive API
        drive = UsdPhysics.DriveAPI.Get(prim, "angular")
        if not drive:
            drive = UsdPhysics.DriveAPI.Apply(prim, "angular")

        # Set to velocity mode with high torque
        drive.CreateTypeAttr("velocity")
        drive.CreateDampingAttr(100000.0)  # High damping = strong motor
        drive.CreateStiffnessAttr(0.0)  # Zero stiffness = no position holding
        drive.CreateTargetVelocityAttr(target_velocity)

        print(f"Success: Applied velocity drive to {prim.GetPath()}")
        found_wheels = True

if not found_wheels:
    print("Error: Could not find any wheel joints in the stage.")
