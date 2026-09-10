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
option(MUSIL_RAYLIB   "Fetch raylib and build the plot library and the Listener" ON)

# --- Uninstall ------------------------------------------------------------------
#   cmake --build build --target uninstall    removes <prefix>/bin/musil, <prefix>/bin/musil-listener,
#   <prefix>/include/musil/ and ~/.musil/ (plus whatever the last install's manifest lists, for an
#   install made with --prefix elsewhere). Defined before raylib is fetched, because raylib would
#   otherwise add an `uninstall` of its own.
if(WIN32)
  set(MUSIL_HOME "$ENV{USERPROFILE}/.musil")
else()
  set(MUSIL_HOME "$ENV{HOME}/.musil")
endif()
add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -DMANIFEST=${CMAKE_BINARY_DIR}/install_manifest.txt
                           -DPREFIX=${CMAKE_INSTALL_PREFIX} -DMUSIL_HOME=${MUSIL_HOME}
                           -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
  COMMENT "Removing the installed files (${CMAKE_INSTALL_PREFIX}, ${MUSIL_HOME})")
add_custom_target(musil-uninstall DEPENDS uninstall)

# --- raylib: the one external dependency (plotting, the Listener, later live audio) ---
if(MUSIL_RAYLIB)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  # EXCLUDE_FROM_ALL keeps raylib out of `cmake --install`: it is linked statically and nothing
  # of it belongs in the user's prefix.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    FetchContent_Declare(raylib
      URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
      URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e
      EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(raylib)
  else()
    FetchContent_Declare(raylib
      URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
      URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e)
    FetchContent_GetProperties(raylib)
    if(NOT raylib_POPULATED)
      FetchContent_Populate(raylib)
      add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
  endif()
  message(STATUS "raylib 5.5 fetched: plot library and Listener enabled")
endif()

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
if(MUSIL_RAYLIB)
  target_compile_definitions(musil PRIVATE MUSIL_HAS_RAYLIB)
  target_link_libraries(musil PRIVATE raylib)
endif()

# --- The Listener: a window to run, watch, plot and play ---------------------
if(MUSIL_RAYLIB)
  add_executable(musil-listener listener/main.cpp)
  target_link_libraries(musil-listener PRIVATE musil_base raylib)
  target_compile_definitions(musil-listener PRIVATE MUSIL_HAS_RAYLIB
    MUSIL_LISTENER_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/listener/assets"
    MUSIL_SOURCE_LIB="${CMAKE_CURRENT_SOURCE_DIR}/src")
  if(APPLE)
    target_link_libraries(musil-listener PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
endif()

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

# --- Documentation ---------------------------------------------------------------
# help.txt (read by `help`) and docs/generated/*.tex (included by the manual) come from the doc
# comments in src/. They are regenerated at every configure, so a fresh clone has them; the PDF
# is built on request:   cmake --build build --target musil-docs   (needs pdflatex)
find_program(PYTHON3 NAMES python3 python)
find_program(PDFLATEX pdflatex)
if(PYTHON3)
  execute_process(COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gendoc.py
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE gendoc_out RESULT_VARIABLE gendoc_rc)
  if(gendoc_rc EQUAL 0)
    string(STRIP "${gendoc_out}" gendoc_out)
    message(STATUS "${gendoc_out}")
  else()
    message(WARNING "tools/gendoc.py failed: help.txt and the manual's function references were not regenerated")
  endif()
  if(PDFLATEX)
    add_custom_target(musil-docs
      COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gendoc.py
      COMMAND ${PDFLATEX} -interaction=nonstopmode musil_manual.tex > /dev/null
      COMMAND ${PDFLATEX} -interaction=nonstopmode musil_manual.tex > /dev/null
      COMMAND ${CMAKE_COMMAND} -E rm -f musil_manual.aux musil_manual.log musil_manual.out musil_manual.toc
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs
      COMMENT "Regenerating help.txt, the function references and docs/musil_manual.pdf")
  else()
    add_custom_target(musil-docs
      COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tools/gendoc.py
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "Regenerating help.txt and the function references (no pdflatex: the PDF is not rebuilt)")
  endif()
else()
  message(WARNING "python3 not found: help.txt is not regenerated; `help` needs an existing src/help.txt")
endif()

# --- Install ------------------------------------------------------------------
#   cmake --install build              binary  -> ${CMAKE_INSTALL_PREFIX}/bin   (/usr/local/bin by default)
#                                      headers -> ${CMAKE_INSTALL_PREFIX}/include/musil
#                                      *.mu    -> ~/.musil   (searched last by load)
#   cmake --install build --prefix ~/.local     to avoid sudo
file(GLOB MUSIL_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mu ${CMAKE_CURRENT_SOURCE_DIR}/src/help.txt)
file(GLOB MUSIL_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
install(TARGETS musil RUNTIME DESTINATION bin)
if(MUSIL_RAYLIB)
  install(TARGETS musil-listener RUNTIME DESTINATION bin)
  install(FILES listener/assets/JetBrainsMono-Regular.ttf listener/assets/JetBrainsMono-OFL.txt DESTINATION ${MUSIL_HOME}/assets)
endif()
install(FILES ${MUSIL_HEADERS} DESTINATION include/musil)
install(DIRECTORY src/system src/scientific DESTINATION include/musil FILES_MATCHING PATTERN "*.h")
install(FILES ${MUSIL_LANG_FILES} DESTINATION ${MUSIL_HOME})

# --- Tests: one per library, plus stress/smoke/load/golden/examples -----------
if(MUSIL_TESTS)
  enable_testing()
  set(T ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME core    COMMAND musil ${T}/test_core.mu)
  add_test(NAME std     COMMAND musil ${T}/test_std.mu)
  add_test(NAME system  COMMAND musil ${T}/test_system.mu WORKING_DIRECTORY ${T})
  add_test(NAME scientific COMMAND musil ${T}/test_scientific.mu)
  add_test(NAME signals COMMAND musil ${T}/test_signals.mu)
  if(MUSIL_RAYLIB)
    add_test(NAME plot COMMAND musil ${T}/test_plot.mu)
  endif()
  add_test(NAME stress  COMMAND musil ${T}/test_stress.mu)
  add_test(NAME smoke   COMMAND musil ${T}/test_smoke.mu)
  add_test(NAME load    COMMAND musil ${T}/test_load.mu WORKING_DIRECTORY ${T})
  # reference outputs frozen in tests/golden/ (timing and rand lines filtered)
  set(REFS reference reference_std reference_scientific reference_signals)
  if(MUSIL_RAYLIB)
    list(APPEND REFS reference_plot)
  endif()
  foreach(ref ${REFS})
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
  set(NEEDS_transients "${D}/Gambale_cut.wav")
  set(NEEDS_filters "${D}/cage.wav")
  set(NEEDS_plots "${D}/cage.wav")
  set(NEEDS_phase_vocoder "${D}/Vox.wav")
  if(NOT MUSIL_RAYLIB)
    set(NEEDS_plots "${D}/no-raylib")
    set(NEEDS_iris_plot "${D}/no-raylib")
    set(NEEDS_reference_plot "${D}/no-raylib")
  endif()
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
  set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT "MUSIL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/src;MUSIL_NOSHOW=1")
endif()
