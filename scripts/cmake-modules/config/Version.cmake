#
# CMake Configuration order script
#

SET(SOURCE ${PROJECT_CONFIG_DIR}/Version.hpp.in)
SET(TARGET ${CMAKE_CURRENT_BINARY_DIR}/Version.hpp)

SET(AUTO_GENERATED_MESSAGE
        "This file was automatically generated. DO NOT EDIT THIS FILE!")

CONFIGURE_FILE(${SOURCE}
               ${TARGET})