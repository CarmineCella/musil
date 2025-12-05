# cli/CMakeLists.txt
#
# Command-line Musil interpreter.
# Uses musil.cpp and the header-only core in ../src.

add_executable(musil
    musil.cpp
)

# Include the core headers
target_include_directories(musil
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# C++ standard and warnings/optimizations (per-target, not global)
target_compile_features(musil PRIVATE cxx_std_17)

if (MSVC)
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

#
# Installation
#
# 1) Install the executable under "bin" in the install prefix.
# 2) Install .scm files into $HOME/.musil
#    and, on Unix, chown them back to the invoking user if run via sudo.
#

# Install CLI binary under ${CMAKE_INSTALL_PREFIX}/bin
install(TARGETS musil
    RUNTIME DESTINATION bin
)

# Install .scm files into ~/.musil
install(FILES
    ${CMAKE_SOURCE_DIR}/src/stdlib.scm
    ${CMAKE_SOURCE_DIR}/src/core.scm
    ${CMAKE_SOURCE_DIR}/src/scientific.scm
    ${CMAKE_SOURCE_DIR}/src/plotting.scm
    DESTINATION $ENV{HOME}/.musil
)

# On Unix: if install is run with sudo, chown the .scm files to $SUDO_USER
# so the normal user can overwrite them later from the IDE.
install(CODE "
    if(NOT WIN32)
      # HOME at install time (under sudo this is usually the original user's HOME)
      set(MUSIL_HOME \"$ENV{HOME}\")

      # Determine the owner we want: prefer SUDO_USER if present
      if(DEFINED ENV{SUDO_USER} AND NOT \"\$ENV{SUDO_USER}\" STREQUAL \"\")
        set(MUSIL_OWNER \"\$ENV{SUDO_USER}\")
      else()
        execute_process(
          COMMAND whoami
          OUTPUT_VARIABLE MUSIL_OWNER
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      endif()
        
      file(GLOB scm_files \"\${MUSIL_HOME}/.musil/*.scm\")
      foreach(f \${scm_files})
        execute_process(
          COMMAND chown \${MUSIL_OWNER} \"\${f}\"
        )
      endforeach()
    endif()
")
