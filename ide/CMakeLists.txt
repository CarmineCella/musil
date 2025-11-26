# gui/CMakeLists.txt
#
# Optional FLTK-based IDE for Musil.
# Musil is header-only: we only need to include ../src and link FLTK.
#
cmake_minimum_required(VERSION 3.12)

option(BUILD_MUSIL_IDE "Build the FLTK-based Musil IDE" ON)

if(NOT BUILD_MUSIL_IDE)
    message(STATUS "BUILD_MUSIL_IDE=OFF, skipping Musil IDE (GUI)")
    return()
endif()

# -------------------------------------------------------------------
# Find FLTK 
# -------------------------------------------------------------------
find_package(FLTK QUIET)

if(NOT FLTK_FOUND)
    message(STATUS "FLTK not found; Musil IDE will NOT be built")
    return()
endif()

# -------------------------------------------------------------------
# musil_ide target
# -------------------------------------------------------------------
# On macOS, MACOSX_BUNDLE creates a .app bundle; on other platforms it
# behaves like a normal GUI executable.
if(APPLE)
    # IMPORTANT: add musil.icns as a source so CMake can put it in Resources
    add_executable(musil_ide MACOSX_BUNDLE musil_ide.cpp musil.icns)
else()
    add_executable(musil_ide musil_ide.cpp)
endif()

# Include Musil core headers (header-only)
target_include_directories(musil_ide PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# FLTK include dirs
if(FLTK_INCLUDE_DIR)
    target_include_directories(musil_ide PRIVATE ${FLTK_INCLUDE_DIR})
endif()

# Link against FLTK
if(FLTK_LIBRARIES)
    target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES})
elseif(TARGET FLTK::FLTK)
    target_link_libraries(musil_ide PRIVATE FLTK::FLTK)
endif()

# C++ standard
target_compile_features(musil_ide PRIVATE cxx_std_17)

# -------------------------------------------------------------------
# macOS bundle settings and post-build step
# -------------------------------------------------------------------
if(APPLE)
    set_target_properties(musil_ide PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME            "Musil IDE"
        MACOSX_BUNDLE_GUI_IDENTIFIER         "com.carminecella.musilide"
        MACOSX_BUNDLE_INFO_STRING            "Musil IDE"
        MACOSX_BUNDLE_LONG_VERSION_STRING    "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING   "1.0"
        MACOSX_BUNDLE_BUNDLE_VERSION         "1.0"
        MACOSX_BUNDLE_ICON_FILE              "musil.icns"  # just the filename
    )

    # Put icon into the bundle Resources
    set_source_files_properties(musil.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )

    # Post-build step: echo bundle path
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo
                "Musil IDE.app built at: $<TARGET_FILE:musil_ide>"
    )

    # Copy stdlib.scm next to the binary (inside bundle's Contents/MacOS)
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/../src/stdlib.scm
                $<TARGET_FILE_DIR:musil_ide>/stdlib.scm
        COMMENT "Copying stdlib.scm next to musil_ide binary"
    )

    if(APPLE)
        # On macOS, copy stdlib.scm into the app bundle's Resources folder
        set(MUSIL_RESOURCES_DIR "$<TARGET_BUNDLE_DIR:musil_ide>/Contents/Resources")

        add_custom_command(TARGET musil_ide POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MUSIL_RESOURCES_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/src/stdlib.scm"
                "${MUSIL_RESOURCES_DIR}/stdlib.scm"
        )
    else()
        # On Linux/Windows (non-bundle), keep copying next to the executable
        add_custom_command(TARGET musil_ide POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/src/stdlib.scm"
                "$<TARGET_FILE_DIR:musil_ide>/stdlib.scm"
        )
    endif()

endif()
