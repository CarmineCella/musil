cmake_minimum_required(VERSION 3.16)

project(musil LANGUAGES CXX)

# Global compile definitions (visible to all targets)
add_compile_definitions(VERSION="0.5")
add_compile_definitions(COPYRIGHT="2026")

# Options
option(BUILD_MUSIL_IDE "Build Musil FLTK-based IDE" OFF)

# Subdirectories
add_subdirectory(cli)

if(BUILD_MUSIL_IDE)
    add_subdirectory(ide)
endif()

# ------------------------------------------------------------------------------
# Uninstall target
# ------------------------------------------------------------------------------

if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    )
endif()
