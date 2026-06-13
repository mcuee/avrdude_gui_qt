cmake_minimum_required(VERSION 3.20)
project(adgui-cpp VERSION 1.0.0 LANGUAGES CXX)

# ─── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Qt6 ─────────────────────────────────────────────────────────────────────
# Requires Qt ≥ 6.2.  Set CMAKE_PREFIX_PATH or Qt6_DIR if Qt is not on PATH.
# e.g.:  cmake -DQt6_DIR=/opt/Qt/6.7.0/gcc_64/lib/cmake/Qt6 ..
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Automatically run moc/uic/rcc for files that need it
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ─── libavrdude ──────────────────────────────────────────────────────────────
# libavrdude is the shared library built as part of AVRDUDE ≥ 8.
# It is found via pkg-config or a manual path.
#
# Option 1 (preferred – installed system-wide):
#   find_package(PkgConfig REQUIRED)
#   pkg_check_modules(AVRDUDE REQUIRED libavrdude)
#
# Option 2 (building alongside AVRDUDE source tree):
#   set(AVRDUDE_SOURCE_DIR  "${CMAKE_SOURCE_DIR}/../.." CACHE PATH
#       "Root of the AVRDUDE source tree")
#   set(AVRDUDE_BUILD_DIR   "${AVRDUDE_SOURCE_DIR}/build_linux" CACHE PATH
#       "AVRDUDE build directory containing libavrdude.so")
#
# Uncomment the block that matches your setup:

# --- pkg-config approach (default) ---
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(AVRDUDE libavrdude)
endif()

# --- Fallback: manual paths ---
if(NOT AVRDUDE_FOUND)
    set(AVRDUDE_INCLUDE_DIRS "" CACHE PATH
        "Directory containing libavrdude.h / avrdude.h")
    set(AVRDUDE_LIBRARY_DIRS "" CACHE PATH
        "Directory containing libavrdude.so / libavrdude.a")
    set(AVRDUDE_LIBRARIES    "avrdude" CACHE STRING "libavrdude link target name")
endif()

# ─── UI files (reused from the Python GUI) ───────────────────────────────────
# Qt's AUTOUIC will compile any .ui listed as a source or referenced via
# set_target_properties.  Place the .ui files in the same directory or
# configure AUTOUIC_SEARCH_PATHS.
#
# The .ui files from src/python/:
#   adgui.ui  →  ui_adgui.h      (optional; we build the layout in C++ above)
#   device.ui     programmer.ui  memories.ui
#   loglevel.ui   devinfo.ui     help.ui   about.ui
#
# To use a .ui file instead of the hand-written layout, include it in SOURCES
# and change the corresponding dialog to use `ui->setupUi(this)`.

set(UI_FILES
    ui/adgui.ui
    ui/device.ui
    ui/programmer.ui
    ui/memories.ui
    ui/loglevel.ui
    ui/devinfo.ui
    ui/help.ui
    ui/about.ui
)

# Only add UI files that actually exist
set(EXISTING_UI_FILES "")
foreach(ui_file ${UI_FILES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ui_file}")
        list(APPEND EXISTING_UI_FILES ${ui_file})
    endif()
endforeach()

# ─── Sources ─────────────────────────────────────────────────────────────────
set(SOURCES
    main.cpp
    mainwindow.cpp
    devicedialog.cpp
    programmerdialog.cpp
    memoriesdialog.cpp
    logleveldialog.cpp
    devinfodialog.cpp
    helpdialog.cpp
    ${EXISTING_UI_FILES}
)

set(HEADERS
    mainwindow.h
    devicedialog.h
    programmerdialog.h
    memoriesdialog.h
    logleveldialog.h
    devinfodialog.h
    helpdialog.h
)

# ─── Executable ──────────────────────────────────────────────────────────────
if(WIN32)
    add_executable(adgui WIN32 ${SOURCES} ${HEADERS})
elseif(APPLE)
    add_executable(adgui MACOSX_BUNDLE ${SOURCES} ${HEADERS})
else()
    add_executable(adgui ${SOURCES} ${HEADERS})
endif()

# ─── Include paths ───────────────────────────────────────────────────────────
target_include_directories(adgui
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}   # generated ui_*.h files land here
        ${AVRDUDE_INCLUDE_DIRS}
)

# ─── Link libraries ──────────────────────────────────────────────────────────
target_link_libraries(adgui
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

if(AVRDUDE_FOUND OR AVRDUDE_LIBRARIES)
    target_link_directories(adgui PRIVATE ${AVRDUDE_LIBRARY_DIRS})
    target_link_libraries(adgui PRIVATE ${AVRDUDE_LIBRARIES})
    target_compile_definitions(adgui PRIVATE HAVE_LIBAVRDUDE)
endif()

# ─── AUTOUIC search path ─────────────────────────────────────────────────────
# If .ui files live in a sub-directory, tell AUTOUIC where to look.
set_target_properties(adgui PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

# ─── Install ─────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS adgui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ─── Windows deployment (windeployqt + staging) ──────────────────────────────
if(WIN32)
    # Locate windeployqt6 (ships with every Qt6 Windows installation)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt6 windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(WARNING "windeployqt6 not found – skipping automatic Qt DLL staging")
    endif()

    # Staging directory used by both windeployqt and Inno Setup
    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

    # Post-build: copy the exe + run windeployqt into the staging dir
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET adgui POST_BUILD
            COMMENT "Staging deployment directory with windeployqt…"

            # 1. Create staging dir
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"

            # 2. Copy the freshly built exe
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:adgui>"
                "${DEPLOY_DIR}/"

            # 3. Copy libavrdude DLL if it was found alongside the import lib
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<$<BOOL:${AVRDUDE_LIBRARY_DIRS}>:${AVRDUDE_LIBRARY_DIRS}/libavrdude.dll>"
                "${DEPLOY_DIR}/" || true   # non-fatal if not present

            # 4. Run windeployqt – resolves all Qt DLLs, plugins, and translations
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --dir "${DEPLOY_DIR}"
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-virtualkeyboard
                "$<TARGET_FILE:adgui>"
        )
    endif()

    # CPack NSIS (quick alternative to Inno Setup, works out-of-the-box)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_NAME        "AVRDUDE-GUI")
    set(CPACK_PACKAGE_VENDOR      "avrdudes")
    set(CPACK_PACKAGE_VERSION     "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AVRDUDE Graphical User Interface")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY   "AVRDUDE GUI")
    set(CPACK_NSIS_DISPLAY_NAME   "AVRDUDE GUI ${PROJECT_VERSION}")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/avrdudes/avrdude")
    set(CPACK_NSIS_CONTACT        "https://github.com/avrdudes/avrdude/issues")
    set(CPACK_NSIS_MUI_ICON       "${CMAKE_SOURCE_DIR}/package/adgui.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\AVRDUDE GUI.lnk' '$INSTDIR\\\\adgui.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\AVRDUDE GUI.lnk'")

    # Install everything from the staging dir when using CPack
    install(DIRECTORY "${DEPLOY_DIR}/"
        DESTINATION "."
        COMPONENT Runtime
        FILES_MATCHING PATTERN "*"
    )

    include(CPack)
endif()

# ─── Summary ─────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== adgui-cpp configuration ===")
message(STATUS "  C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt6 version   : ${Qt6_VERSION}")
message(STATUS "  libavrdude    : ${AVRDUDE_FOUND}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    message(STATUS "  windeployqt   : ${WINDEPLOYQT_EXECUTABLE}")
    message(STATUS "  Deploy dir    : ${DEPLOY_DIR}")
endif()
message(STATUS "")
