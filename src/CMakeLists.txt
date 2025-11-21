cmake_minimum_required(VERSION 2.8.9)
project (backsynth)

SET(CMAKE_CXX_FLAGS "-Wall -g -O2 -std=c++17 -I . -Wno-return-type-c-linkage")

add_executable(backsynth backsynth.cpp)
target_link_libraries (backsynth ${LIBS})
