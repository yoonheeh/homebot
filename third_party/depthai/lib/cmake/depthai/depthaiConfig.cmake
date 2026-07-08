include(CMakeFindDependencyMacro)

find_dependency(nlohmann_json CONFIG REQUIRED)
find_dependency(libnop CONFIG REQUIRED)
find_dependency(XLink CONFIG REQUIRED COMPONENTS XLinkPublic)

set(DEPTHAI_OPENCV_SUPPORT ON)
set(DEPTHAI_PCL_SUPPORT OFF)
set(DEPTHAI_XTENSOR_SUPPORT ON)
set(DEPTHAI_DYNAMIC_CALIBRATION_SUPPORT ON)

if(DEPTHAI_OPENCV_SUPPORT)
    find_dependency(OpenCV 4 CONFIG REQUIRED)
endif()

if(DEPTHAI_PCL_SUPPORT)
    find_dependency(PCL CONFIG REQUIRED)
endif()

if(DEPTHAI_XTENSOR_SUPPORT)
    find_dependency(xtensor CONFIG REQUIRED)
endif()

# Add the targets file
include("${CMAKE_CURRENT_LIST_DIR}/depthaiTargets.cmake")

# Cleanup
set(_IMPORT_PREFIX)
