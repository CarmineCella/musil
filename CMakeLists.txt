cmake_minimum_required(VERSION 3.30)

project (musil)

add_compile_definitions (VERSION="0.1")
add_compile_definitions (COPYRIGHT="2025")

add_subdirectory (cli)
option(BUILD_MUSIL_IDE "Build Musil IDE" ON)
if(BUILD_MUSIL_IDE)
    add_subdirectory(ide)
endif()



