cmake_minimum_required(VERSION 3.15)
project(Musil VERSION 0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(MUSIL_READLINE "Use GNU readline in the REPL when available" ON)
option(MUSIL_TESTS    "Build the test targets" ON)


# --- Uninstall ------------------------------------------------------------------
#   cmake --build build --target uninstall    removes <prefix>/bin/musil, <prefix>/bin/musil-ide,
#   <prefix>/include/musil/ and ~/.musil/ (plus whatever the last install's manifest lists, for an
#   install made with --prefix elsewhere). FLTK's own `uninstall` target is renamed when it is
#   fetched (see the patch below), so `uninstall` is ours.
if(WIN32)
  set(MUSIL_HOME "$ENV{USERPROFILE}/.musil")
else()
  set(MUSIL_HOME "$ENV{HOME}/.musil")
endif()

# --- FLTK: the one external dependency (plots, controls, the IDE) -------------------------
option(MUSIL_FLTK "Fetch FLTK and build the plot library, the controls window and the IDE" ON)
if(MUSIL_FLTK)
  include(FetchContent)
  set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_GL OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_FORMS OFF CACHE BOOL "" FORCE)
  set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(FLTK_USE_SYSTEM_LIBPNG OFF CACHE BOOL "" FORCE)
  set(FLTK_USE_SYSTEM_LIBJPEG OFF CACHE BOOL "" FORCE)
  set(FLTK_USE_SYSTEM_ZLIB OFF CACHE BOOL "" FORCE)
  set(FLTK_USE_PANGO OFF CACHE BOOL "" FORCE)
  set(FLTK_BACKEND_WAYLAND OFF CACHE BOOL "" FORCE)
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  # EXCLUDE_FROM_ALL keeps FLTK out of `cmake --install`: it is linked statically. The patch renames
  # FLTK's own `uninstall` target (fltk-uninstall), so `uninstall` stays ours.
  set(FLTK_PATCH sed -i.bak "s/add_custom_target(uninstall/add_custom_target(fltk-uninstall/" CMake/install.cmake)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    FetchContent_Declare(fltk
      URL https://github.com/fltk/fltk/archive/refs/tags/release-1.4.3.tar.gz
      PATCH_COMMAND ${FLTK_PATCH}
      URL_HASH SHA256=6a11c0bf91b7b193a87a1928c32a953f36d7dd4b65fef3e9d0c40a51882f97a6
      EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(fltk)
  else()
    FetchContent_Declare(fltk URL https://github.com/fltk/fltk/archive/refs/tags/release-1.4.3.tar.gz
      URL_HASH SHA256=6a11c0bf91b7b193a87a1928c32a953f36d7dd4b65fef3e9d0c40a51882f97a6)
    FetchContent_GetProperties(fltk)
    if(NOT fltk_POPULATED)
      FetchContent_Populate(fltk)
      add_subdirectory(${fltk_SOURCE_DIR} ${fltk_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
  endif()
  message(STATUS "FLTK 1.4.3 fetched: plot library, controls and the IDE enabled")
endif()

add_custom_target(uninstall
  COMMAND ${CMAKE_COMMAND} -DMANIFEST=${CMAKE_BINARY_DIR}/install_manifest.txt
                           -DPREFIX=${CMAKE_INSTALL_PREFIX} -DMUSIL_HOME=${MUSIL_HOME}
                           -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake
  COMMENT "Removing the installed files (${CMAKE_INSTALL_PREFIX}, ${MUSIL_HOME})")
add_custom_target(musil-uninstall DEPENDS uninstall)

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
# the audio device (miniaudio, in src/live) needs these system libraries
if(APPLE)
  target_link_libraries(musil_base INTERFACE "-framework CoreAudio" "-framework CoreFoundation" "-framework AudioToolbox")
elseif(UNIX)
  target_link_libraries(musil_base INTERFACE pthread dl m)
endif()
if(MUSIL_FLTK)
  target_compile_definitions(musil PRIVATE MUSIL_HAS_FLTK)
  target_link_libraries(musil PRIVATE fltk::images fltk::fltk)
endif()

# --- The IDE: editor, console, variables, help; plots and controls in windows --------
if(MUSIL_FLTK)
  add_executable(musil-ide ide/main.cpp)
  target_link_libraries(musil-ide PRIVATE musil_base fltk::images fltk::fltk)
  target_compile_definitions(musil-ide PRIVATE MUSIL_HAS_FLTK MUSIL_SOURCE_LIB="${CMAKE_CURRENT_SOURCE_DIR}/src")
  if(APPLE)
    set_target_properties(musil-ide PROPERTIES MACOSX_BUNDLE FALSE)
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
if(MUSIL_FLTK)
  install(TARGETS musil-ide RUNTIME DESTINATION bin)
endif()
install(FILES ${MUSIL_HEADERS} DESTINATION include/musil)
install(DIRECTORY src/system src/scientific src/live DESTINATION include/musil FILES_MATCHING PATTERN "*.h")
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
  add_test(NAME live    COMMAND musil ${T}/test_live.mu)
  if(MUSIL_FLTK)
    add_test(NAME plot COMMAND musil ${T}/test_plot.mu)
  endif()
  add_test(NAME stress  COMMAND musil ${T}/test_stress.mu)
  add_test(NAME smoke   COMMAND musil ${T}/test_smoke.mu)
  add_test(NAME load    COMMAND musil ${T}/test_load.mu WORKING_DIRECTORY ${T})
  # reference outputs frozen in tests/golden/ (timing and rand lines filtered)
  set(REFS reference reference_std reference_scientific reference_signals reference_live)
  if(MUSIL_FLTK)
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
  set(NEEDS_live_play "${D}/drums.wav")
  set(NEEDS_live_loops "${D}/skip-in-tests")       # 22 s of music: run it, don't test it
  set(NEEDS_live_session "${D}/skip-in-tests")     # meant to be opened in the IDE and left running
  if(NOT MUSIL_FLTK)
    set(NEEDS_plots "${D}/no-fltk")
    set(NEEDS_iris_plot "${D}/no-fltk")
    set(NEEDS_reference_plot "${D}/no-fltk")
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
  set_tests_properties(${ALL_TESTS} PROPERTIES ENVIRONMENT "MUSIL_PATH=${CMAKE_CURRENT_SOURCE_DIR}/src;MUSIL_NOSHOW=1;MUSIL_NULL_AUDIO=1")
endif()
