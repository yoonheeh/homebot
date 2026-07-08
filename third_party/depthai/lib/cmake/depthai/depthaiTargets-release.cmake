#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "depthai::core" for configuration "Release"
set_property(TARGET depthai::core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(depthai::core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdepthai-core.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::core )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::core "${_IMPORT_PREFIX}/lib/libdepthai-core.a" )

# Import target "depthai::XLink" for configuration "Release"
set_property(TARGET depthai::XLink APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(depthai::XLink PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libXLink.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::XLink )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::XLink "${_IMPORT_PREFIX}/lib/libXLink.a" )

# Import target "depthai::depthai-resources" for configuration "Release"
set_property(TARGET depthai::depthai-resources APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(depthai::depthai-resources PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdepthai-resources.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::depthai-resources )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::depthai-resources "${_IMPORT_PREFIX}/lib/libdepthai-resources.a" )

# Import target "depthai::messages" for configuration "Release"
set_property(TARGET depthai::messages APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(depthai::messages PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmessages.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::messages )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::messages "${_IMPORT_PREFIX}/lib/libmessages.a" )

# Import target "depthai::foxglove_websocket" for configuration "Release"
set_property(TARGET depthai::foxglove_websocket APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(depthai::foxglove_websocket PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libfoxglove_websocket.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS depthai::foxglove_websocket )
list(APPEND _IMPORT_CHECK_FILES_FOR_depthai::foxglove_websocket "${_IMPORT_PREFIX}/lib/libfoxglove_websocket.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
