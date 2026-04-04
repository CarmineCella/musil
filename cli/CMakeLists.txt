# cli/CMakeLists.txt
#
# Command-line Musil interpreter.

set(MUSIL_CLI_SOURCES
    musil.cpp
    ../src/musil.h
    ../src/core.h
    ../src/scientific.h
    ../src/signals.h
)

add_executable(musil
    ${MUSIL_CLI_SOURCES}
)

# Include the core headers
target_include_directories(musil
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

if(BUILD_MUSIL_RTSOUND)
    target_include_directories(musil
        PRIVATE
            ${miniaudio_SOURCE_DIR}
    )
endif()

if(BUILD_MUSIL_RTSOUND)
    target_compile_definitions(musil
        PRIVATE
            BUILD_MUSIL_RTSOUND
    )
endif()

# C++ standard and warnings/optimizations (per-target, not global)
target_compile_features(musil PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(musil PRIVATE /W4)
else()
    target_compile_options(musil PRIVATE
        -Wall
        -g
        -O2
        -Wunused-function
        -Wno-return-type-c-linkage
    )
endif()

if(APPLE AND BUILD_MUSIL_RTSOUND)
    target_link_libraries(musil PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation"
    )
endif()

#
# Installation
#
# 1) Install the executable under "bin" in the install prefix.
# 2) Install .mu files into $HOME/.musil
#    and, on Unix, chown them back to the invoking user if run via sudo.
#

# Install CLI binary under ${CMAKE_INSTALL_PREFIX}/bin
install(TARGETS musil
    RUNTIME DESTINATION bin
)

# Install .mu files into ~/.musil
install(FILES
    ${CMAKE_SOURCE_DIR}/src/stdlib.mu
    ${CMAKE_SOURCE_DIR}/src/scientific.mu
    ${CMAKE_SOURCE_DIR}/src/signals.mu
    ${CMAKE_SOURCE_DIR}/src/plotting.mu
    DESTINATION $ENV{HOME}/.musil
)

# On Unix: if install is run with sudo, chown the .mu files to $SUDO_USER
# so the normal user can overwrite them later from the IDE.
install(CODE "
    if(NOT WIN32)
      set(MUSIL_HOME \"$ENV{HOME}\")

      if(DEFINED ENV{SUDO_USER} AND NOT \"\$ENV{SUDO_USER}\" STREQUAL \"\")
        set(MUSIL_OWNER \"\$ENV{SUDO_USER}\")
      else()
        execute_process(
          COMMAND whoami
          OUTPUT_VARIABLE MUSIL_OWNER
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      endif()

      file(GLOB mu_files \"\${MUSIL_HOME}/.musil/*.mu\")
      foreach(f \${mu_files})
        execute_process(
          COMMAND chown \${MUSIL_OWNER} \"\${f}\"
        )
      endforeach()
    endif()
")