#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "depthai::core" for configuration "Debug"
set_property(TARGET depthai::core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(depthai::core PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "dynamic_calibration_imported"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libdepthai-core.so"
  IMPORTED_SONAME_DEBUG "libdepthai-core.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::core )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::core "${_IMPORT_PREFIX}/lib/libdepthai-core.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
