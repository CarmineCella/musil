cmake_minimum_required(VERSION 3.15)
project(Musil VERSION 0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MUSIL_READLINE "Use GNU readline in the REPL when available" ON)
option(MUSIL_TESTS    "Build the test targets" ON)

# --- The language: header-only ------------------------------------------------
add_library(musil_base INTERFACE)
target_include_directories(musil_base INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(musil_base INTERFACE cxx_std_17)
if(MSVC)
  target_compile_options(musil_base INTERFACE /W3)
else()
  target_compile_options(musil_base INTERFACE -Wall -Wextra -Wno-unused-parameter)
endif()

# --- The command-line interpreter ---------------------------------------------
add_executable(musil cli/main.cpp)
target_link_libraries(musil PRIVATE musil_base)

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

# --- Install ------------------------------------------------------------------
#   cmake --install build              binary  -> ${CMAKE_INSTALL_PREFIX}/bin   (/usr/local/bin by default)
#                                      headers -> ${CMAKE_INSTALL_PREFIX}/include/musil
#                                      *.mu    -> ~/.musil   (searched last by load)
#   cmake --install build --prefix ~/.local     to avoid sudo
file(GLOB MUSIL_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mu)
file(GLOB MUSIL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
if(WIN32)
  set(MUSIL_HOME "$ENV{USERPROFILE}/.musil")
else()
  set(MUSIL_HOME "$ENV{HOME}/.musil")
endif()
install(TARGETS musil RUNTIME DESTINATION bin)
install(FILES ${MUSIL_HEADERS} DESTINATION include/musil)
install(DIRECTORY src/system src/scientific DESTINATION include/musil FILES_MATCHING PATTERN "*.h")
install(FILES ${MUSIL_LANG_FILES} DESTINATION ${MUSIL_HOME})

#   cmake --build build --target uninstall    removes everything the last install put in place
add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -DMANIFEST=${CMAKE_BINARY_DIR}/install_manifest.txt -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
  COMMENT "Removing installed files listed in install_manifest.txt")

# --- Tests: one per library, plus stress/smoke/load/golden/examples -----------
if(MUSIL_TESTS)
  enable_testing()
  set(T ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME core    COMMAND musil ${T}/test_core.mu)
  add_test(NAME std     COMMAND musil ${T}/test_std.mu)
  add_test(NAME system  COMMAND musil ${T}/test_system.mu WORKING_DIRECTORY ${T})
  add_test(NAME scientific COMMAND musil ${T}/test_scientific.mu)
  add_test(NAME signals COMMAND musil ${T}/test_signals.mu)
  add_test(NAME stress  COMMAND musil ${T}/test_stress.mu)
  add_test(NAME smoke   COMMAND musil ${T}/test_smoke.mu)
  add_test(NAME load    COMMAND musil ${T}/test_load.mu WORKING_DIRECTORY ${T})
  # reference outputs frozen in tests/golden/ (timing and rand lines filtered)
  foreach(ref reference reference_std reference_scientific reference_signals)
    add_test(NAME golden-${ref}
             COMMAND ${CMAKE_COMMAND}
                     -DMUSIL=$<TARGET_FILE:musil>
                     -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/examples/${ref}.mu
                     -DGOLDEN=${T}/golden/${ref}.out
                     -P ${T}/golden.cmake)
  endforeach()
  # performance and limits, kept as tests rather than examples
  add_test(NAME perf-loop      COMMAND musil ${T}/perf_loop.mu)
  add_test(NAME perf-fib       COMMAND musil ${T}/perf_fib.mu)
  add_test(NAME perf-matrix    COMMAND musil ${T}/perf_matrix.mu)
  add_test(NAME inf-recursion  COMMAND musil ${T}/inf_recursion.mu)
  set_tests_properties(inf-recursion PROPERTIES WILL_FAIL TRUE)
  # every example must run without error
  # the signal examples read recordings that are not part of the repository; they are tests only when present
  set(D ${CMAKE_CURRENT_SOURCE_DIR}/examples/data)
  set(NEEDS_stft "${D}/Vox.wav")
  set(NEEDS_features "${D}/Vox.wav")
  set(NEEDS_cross_synth "${D}/Vox.wav;${D}/Beethoven_Symph7.wav")
  set(NEEDS_additive "${D}/gong_c_sharp.wav")
  set(NEEDS_reverb "${D}/anechoic1.wav;${D}/Concertgebouw-s.wav")
  set(NEEDS_transients "${D}/drums.wav")
  set(NEEDS_filters "${D}/drums.wav")
  file(GLOB MUSIL_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.mu)
  foreach(ex ${MUSIL_EXAMPLES})
    get_filename_component(name ${ex} NAME_WE)
    set(ok TRUE)
    foreach(f ${NEEDS_${name}})
      if(NOT EXISTS ${f})
        set(ok FALSE)
      endif()
    endforeach()
    if(ok)
      add_test(NAME example-${name} COMMAND musil ${ex} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    else()
      message(STATUS "example ${name} skipped: needs ${NEEDS_${name}}")
    endif()
  endforeach()
  # tests and examples say (load "std.mu") etc.: point them at the source tree
  get_property(ALL_TESTS DIRECTORY PROPERTY TESTS)
  set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT "MUSIL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()
