# ide/CMakeLists.txt
#
# Optional FLTK-based Musil IDE.
# Unified install strategy:
#   - CLI installs normally through its own CMake
#   - .mu libraries are copied to ~/.musil at install time
#   - On macOS, the app bundle is copied to /Applications via install(CODE)
#     using ditto + codesign, instead of install(TARGETS ... BUNDLE DESTINATION ...)

if(NOT BUILD_MUSIL_IDE)
    message(STATUS "BUILD_MUSIL_IDE=OFF, skipping Musil IDE (GUI)")
    return()
endif()

include(FetchContent)

# ------------------------------------------------------------------------------
# FLTK
# ------------------------------------------------------------------------------

find_package(FLTK QUIET)

if(FLTK_FOUND)
    message(STATUS "Using system-installed FLTK")
else()
    message(STATUS "FLTK not found: downloading and building static FLTK")

    set(FLTK_BUILD_SHARED_LIBS OFF CACHE BOOL "Build FLTK shared libraries" FORCE)
    set(FLTK_BUILD_TEST OFF CACHE BOOL "Build FLTK tests" FORCE)
    set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "Build FLTK examples" FORCE)
    set(FLTK_BUILD_FLUID OFF CACHE BOOL "Build FLUID" FORCE)
    set(FLTK_BUILD_FORMS ON CACHE BOOL "Build FLTK forms library" FORCE)

    set(FLTK_OPTION_CAIRO_WINDOW OFF CACHE BOOL "Enable Cairo window support" FORCE)
    set(FLTK_OPTION_CAIRO_EXT OFF CACHE BOOL "Enable Cairo ext support" FORCE)

    FetchContent_Declare(
        fltk
        GIT_REPOSITORY https://github.com/fltk/fltk.git
        GIT_TAG release-1.4.4
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(fltk)
endif()

# ------------------------------------------------------------------------------
# Musil resources
# ------------------------------------------------------------------------------

set(MUSIL_STDLIB "${CMAKE_SOURCE_DIR}/src/stdlib.mu")
set(MUSIL_SCI    "${CMAKE_SOURCE_DIR}/src/scientific.mu")
set(MUSIL_PLOT   "${CMAKE_SOURCE_DIR}/src/plotting.mu")
set(MUSIL_SIG    "${CMAKE_SOURCE_DIR}/src/signals.mu")

set(MUSIL_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/musil.icns")

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------

set(MUSIL_IDE_SOURCES
    musil_ide.cpp
    ../src/musil.h
    ../src/core.h
    ../src/scientific.h
    ../src/signals.h
)

if(APPLE)
    list(APPEND MUSIL_IDE_SOURCES
        ${MUSIL_ICON_FILE}
    )
endif()


# ------------------------------------------------------------------------------
# Target
# ------------------------------------------------------------------------------

if(APPLE)
    add_executable(musil_ide MACOSX_BUNDLE
        ${MUSIL_IDE_SOURCES}
    )

    set_source_files_properties(${MUSIL_ICON_FILE}
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
    )

    set_target_properties(musil_ide PROPERTIES
        OUTPUT_NAME                        "Musil IDE"
        MACOSX_BUNDLE_GUI_IDENTIFIER       "com.carminecella.musil.ide"
        MACOSX_BUNDLE_BUNDLE_NAME          "Musil IDE"
        MACOSX_BUNDLE_INFO_STRING          "Musil IDE"
        MACOSX_BUNDLE_BUNDLE_VERSION       "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        MACOSX_BUNDLE_ICON_FILE            "musil.icns"
    )

    set(MUSIL_RESOURCES_DIR "$<TARGET_BUNDLE_DIR:musil_ide>/Contents/Resources")

    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${MUSIL_RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_STDLIB}"
            "${MUSIL_RESOURCES_DIR}/stdlib.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_SIG}"
            "${MUSIL_RESOURCES_DIR}/signals.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_SCI}"
            "${MUSIL_RESOURCES_DIR}/scientific.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_PLOT}"
            "${MUSIL_RESOURCES_DIR}/plotting.mu"
        COMMENT "Copying libraries into Musil IDE.app Resources"
    )

    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:musil_ide>"
        COMMENT "Ad-hoc signing Musil IDE.app in build tree"
        VERBATIM
    )

else()
    add_executable(musil_ide
        ${MUSIL_IDE_SOURCES}
    )

    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_STDLIB}"
            "$<TARGET_FILE_DIR:musil_ide>/stdlib.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_SCI}"
            "$<TARGET_FILE_DIR:musil_ide>/scientific.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_PLOT}"
            "$<TARGET_FILE_DIR:musil_ide>/plotting.mu"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MUSIL_SIG}"
            "$<TARGET_FILE_DIR:musil_ide>/signals.mu"
        COMMENT "Copying libraries next to musil_ide executable"
    )
endif()

target_include_directories(musil_ide
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

if(BUILD_MUSIL_RTSOUND)
    target_include_directories(musil_ide
        PRIVATE
            ${miniaudio_SOURCE_DIR}
    )
endif()

target_compile_definitions(musil_ide
    PRIVATE
        BUILD_MUSIL_IDE
)

if(BUILD_MUSIL_RTSOUND)
    target_compile_definitions(musil_ide
        PRIVATE
            BUILD_MUSIL_RTSOUND
    )
endif()

target_compile_features(musil_ide PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(musil_ide PRIVATE /W4)
else()
    target_compile_options(musil_ide PRIVATE
        -Wall
        -g
        -O2
        -Wunused-function
        -Wno-return-type-c-linkage
    )
endif()

if(TARGET fltk::fltk)
    target_link_libraries(musil_ide PRIVATE fltk::fltk)
else()
    target_include_directories(musil_ide PRIVATE ${FLTK_INCLUDE_DIR})
    target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES})
endif()

if(APPLE AND BUILD_MUSIL_RTSOUND)
    target_link_libraries(musil_ide PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
        "-framework CoreFoundation"
    )
endif()

if(APPLE)
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "---- Linked libraries for musil_ide ----"
        COMMAND otool -L "$<TARGET_FILE:musil_ide>"
        VERBATIM
    )
elseif(UNIX)
    add_custom_command(TARGET musil_ide POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "---- Linked libraries for musil_ide ----"
        COMMAND ldd "$<TARGET_FILE:musil_ide>" || true
        VERBATIM
    )
endif()

# ------------------------------------------------------------------------------
# Install
# ------------------------------------------------------------------------------

install(FILES
    ${CMAKE_SOURCE_DIR}/src/stdlib.mu
    ${CMAKE_SOURCE_DIR}/src/scientific.mu
    ${CMAKE_SOURCE_DIR}/src/signals.mu
    ${CMAKE_SOURCE_DIR}/src/plotting.mu
    DESTINATION $ENV{HOME}/.musil
)

if(APPLE)
    install(CODE [[
        set(APP_SRC "$<TARGET_BUNDLE_DIR:musil_ide>")
        set(APP_DST "/Applications/Musil IDE.app")

        message(STATUS "Installing Musil IDE.app to ${APP_DST}")

        execute_process(
            COMMAND /bin/rm -rf "${APP_DST}"
            RESULT_VARIABLE RM_RESULT
        )

        execute_process(
            COMMAND /usr/bin/ditto "${APP_SRC}" "${APP_DST}"
            RESULT_VARIABLE DITTO_RESULT
        )

        if(NOT DITTO_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to copy app bundle to /Applications")
        endif()

        execute_process(
            COMMAND /usr/bin/codesign --force --deep --sign - "${APP_DST}"
            RESULT_VARIABLE CODESIGN_RESULT
        )

        if(NOT CODESIGN_RESULT EQUAL 0)
            message(WARNING "codesign failed for ${APP_DST}; the app bundle was copied but may not launch properly")
        endif()
    ]])
else()
    install(TARGETS musil_ide
        RUNTIME DESTINATION bin
    )
endif()