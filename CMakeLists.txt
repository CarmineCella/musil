cmake_minimum_required(VERSION 3.15)
project(Musil VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MUSIL_READLINE "Use GNU readline in the REPL when available" ON)
option(MUSIL_TESTS    "Build the test targets" ON)

# --- The language: header-only, zero dependencies -------------------------
add_library(musil_core INTERFACE)
target_include_directories(musil_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(musil_core INTERFACE cxx_std_17)
if(MSVC)
  target_compile_options(musil_core INTERFACE /W3)
else()
  target_compile_options(musil_core INTERFACE -Wall -Wextra -Wno-unused-parameter)
endif()

# --- The command-line interpreter -----------------------------------------
add_executable(musil cli/main.cpp)
target_link_libraries(musil PRIVATE musil_core)

if(MUSIL_READLINE)
  find_path(READLINE_INCLUDE_DIR readline/readline.h)
  find_library(READLINE_LIBRARY NAMES readline)
  if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
    message(STATUS "readline found: ${READLINE_LIBRARY}")
    target_compile_definitions(musil PRIVATE HAVE_READLINE)
    target_include_directories(musil PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(musil PRIVATE ${READLINE_LIBRARY})
  else()
    message(STATUS "readline not found: REPL will use plain std::getline")
  endif()
endif()

install(TARGETS musil RUNTIME DESTINATION bin)
install(FILES src/core.h src/musil.h DESTINATION include/musil)

# --- Tests -----------------------------------------------------------------
if(MUSIL_TESTS)
  enable_testing()
  set(T ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME core    COMMAND musil ${T}/test_core.mu)
  add_test(NAME stress  COMMAND musil ${T}/test_stress.mu)
  add_test(NAME smoke   COMMAND musil ${T}/test_smoke.mu)
  add_test(NAME load    COMMAND musil ${T}/test_load.mu WORKING_DIRECTORY ${T})
  # reference.mu output frozen in tests/golden/reference.out (timing lines filtered)
  add_test(NAME reference-golden
           COMMAND ${CMAKE_COMMAND}
                   -DMUSIL=$<TARGET_FILE:musil>
                   -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/examples/reference.mu
                   -DGOLDEN=${T}/golden/reference.out
                   -P ${T}/golden.cmake)
endif()
