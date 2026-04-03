# ide/CMakeLists.txt
#
# Optional FLTK-based Musil IDE.

if(NOT BUILD_MUSIL_IDE)
    message(STATUS "BUILD_MUSIL_IDE=OFF, skipping Musil IDE (GUI)")
    return()
endif()

find_package(FLTK REQUIRED)
if(NOT FLTK_FOUND)
    message(FATAL_ERROR "FLTK not found, cannot build Musil IDE")
endif()

# Paths to scheme files
set(MUSIL_STDLIB "${CMAKE_SOURCE_DIR}/src/stdlib.scm")
set(MUSIL_SCI   "${CMAKE_SOURCE_DIR}/src/scientific.scm")
set(MUSIL_PLOT   "${CMAKE_SOURCE_DIR}/src/plotting.scm")
set(MUSIL_SIG   "${CMAKE_SOURCE_DIR}/src/signals.scm")

# musil.icns is in the same folder as musil_ide.cpp
set(MUSIL_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/musil.icns")

if(APPLE)
    # Bundle executable
    add_executable(musil_ide MACOSX_BUNDLE
        musil_ide.cpp
        ${MUSIL_ICON_FILE}
    )

    # Place icon inside the bundle Resources
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
        MACOSX_BUNDLE_ICON_FILE            "musil.icns"   # filename only
    )

    # Copy libraries into the bundle Resources after build
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

else()
    # Non-macOS: normal executable
    add_executable(musil_ide
        musil_ide.cpp
    )

    # Copy libraries next to the executable
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
        ${FLTK_INCLUDE_DIR}
)

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

target_link_libraries(musil_ide PRIVATE ${FLTK_LIBRARIES})

#
# Installation
#
# - On macOS: install Musil IDE.app into /Applications
# - On Linux/Windows: install musil_ide binary into ${CMAKE_INSTALL_PREFIX}/bin
#

if(APPLE)
    install(TARGETS musil_ide
        BUNDLE DESTINATION "/Applications"
    )
else()
    install(TARGETS musil_ide
        RUNTIME DESTINATION bin
    )
endif()