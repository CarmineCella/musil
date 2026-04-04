cmake_minimum_required(VERSION 3.16)

project(musil LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Global compile definitions
# ------------------------------------------------------------------------------

add_compile_definitions(VERSION="0.51")
add_compile_definitions(COPYRIGHT="2026")

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(BUILD_MUSIL_IDE "Build Musil FLTK-based IDE" OFF)

# ------------------------------------------------------------------------------
# Subprojects
# ------------------------------------------------------------------------------

add_subdirectory(cli)

if(BUILD_MUSIL_IDE)
    add_subdirectory(ide)
endif()

# ------------------------------------------------------------------------------
# Musil uninstall target
# ------------------------------------------------------------------------------

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
    @ONLY
)

# Always create our own uninstall target with a unique name
add_custom_target(musil_uninstall
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
    COMMENT "Uninstall Musil"
)
