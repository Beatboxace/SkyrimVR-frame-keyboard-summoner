cmake_minimum_required(VERSION 3.21)
project(FrameKeyboardSummoner
    VERSION 1.0.0
    DESCRIPTION "SKSEVR plugin: working text keyboard for Skyrim VR on the Steam Frame"
    LANGUAGES CXX RC)

# Build:    cmake -B build -A x64
#           cmake --build build --config Release
# Package:  cmake --build build --config Release --target package_zip
#           -> dist/FrameKeyboardSummoner-<version>.zip

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Skyrim VR is 64-bit: configure with -A x64")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static C runtime, so users don't need a matching VC++ redistributable.
# The policy default also applies it to MinHook, whose CMakeLists predates CMP0091.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        c3fcafdc10146beb5919319d0683e44e3c30d537 # v1.3.4, pinned by commit
)
FetchContent_MakeAvailable(minhook)

configure_file(src/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

add_library(FrameKeyboardSummoner SHARED
    src/main.cpp
    src/keyboard_fix.cpp
    src/text_overlay.cpp
    src/hotkeys.cpp
    src/config.cpp
    src/log.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

target_include_directories(FrameKeyboardSummoner PRIVATE
    third_party/openvr
    ${CMAKE_CURRENT_BINARY_DIR}/generated) # version.h
target_compile_definitions(FrameKeyboardSummoner PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(FrameKeyboardSummoner PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(FrameKeyboardSummoner PRIVATE minhook shell32 ole32 gdi32 user32)

# ---- package_zip: the Nexus / MO2 archive ----------------------------------------------
# Layout is relative to the game's Data folder, so MO2 and Vortex install it as is:
#   SKSE/Plugins/FrameKeyboardSummoner.dll
#   SKSE/Plugins/FrameKeyboardSummoner.ini
#   SKSE/Plugins/FrameKeyboardSummoner_ThirdPartyNotices.txt
set(PKG_ROOT ${CMAKE_CURRENT_BINARY_DIR}/package)
set(PKG_PLUGINS ${PKG_ROOT}/SKSE/Plugins)
set(PKG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/package/SKSE/Plugins)
set(PKG_ZIP ${CMAKE_CURRENT_SOURCE_DIR}/dist/${PROJECT_NAME}-${PROJECT_VERSION}.zip)
file(MAKE_DIRECTORY ${PKG_ROOT})

add_custom_target(package_zip
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${PKG_ROOT}/SKSE
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PKG_PLUGINS} ${CMAKE_CURRENT_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:FrameKeyboardSummoner>
        ${PKG_SRC}/FrameKeyboardSummoner.ini
        ${PKG_SRC}/FrameKeyboardSummoner_ThirdPartyNotices.txt
        ${PKG_PLUGINS}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${PKG_ZIP}
    COMMAND ${CMAKE_COMMAND} -E tar cf ${PKG_ZIP} --format=zip SKSE
    COMMAND ${CMAKE_COMMAND} -E echo "Packaged ${PKG_ZIP}"
    WORKING_DIRECTORY ${PKG_ROOT}
    DEPENDS FrameKeyboardSummoner
    VERBATIM)
