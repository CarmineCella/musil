cmake_minimum_required(VERSION 3.12)

project (musil)

SET(CMAKE_CXX_FLAGS "-Wall -g -O2 -std=c++17 -I . -Wunused-function -Wno-return-type-c-linkage")

add_executable(musil musil.cpp ../src/musil.h ../src/core.h)

set_target_properties(musil PROPERTIES LINK_FLAGS "-Wl,-rpath,./")

target_include_directories(musil
    PUBLIC
		  ${PROJECT_BINARY_DIR}/../src/
    	   ${PROJECT_SOURCE_DIR}/../src/
 )


target_link_libraries (musil ${LIBS})

INSTALL(PROGRAMS ../src/stdlib.scm DESTINATION $ENV{HOME}/.musil)
INSTALL(
    TARGETS musil
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION $ENV{HOME}/.musil
    ARCHIVE DESTINATION $ENV{HOME}/.musil)

