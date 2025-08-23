cmake_minimum_required(VERSION 3.22)
project(Gungutils)
set(CMAKE_CXX_STANDARD 20)

# OpenGL
find_package(OpenGL REQUIRED)

# Platform-specific dependencies
if(UNIX AND NOT APPLE)  # Linux
    find_package(X11 REQUIRED COMPONENTS Xi)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCURSOR REQUIRED xcursor)
    set(PLATFORM_LIBS ${X11_LIBRARIES} X11::Xi ${XCURSOR_LIBRARIES} dl pthread)
    set(PLATFORM_INCLUDE_DIRS ${X11_INCLUDE_DIR} ${XCURSOR_INCLUDE_DIRS})
elseif(WIN32)
    # Windows-specific libraries will be linked automatically by SDL3 and other libs
    set(PLATFORM_LIBS)
    set(PLATFORM_INCLUDE_DIRS)
endif()

# ImGui
set(IMGUI_PATH "${CMAKE_SOURCE_DIR}/include/imgui/")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library(ImGui STATIC ${IMGUI_SOURCES})
target_include_directories(ImGui PUBLIC ${IMGUI_PATH})

# TinyFileDialogs
file(GLOB TINYFD_SOURCES ${CMAKE_SOURCE_DIR}/include/libtinyfiledialogs/*.c)
add_library(TinyFD STATIC ${TINYFD_SOURCES})

# main executable
add_executable(Gungutils main.cpp)

# subdirectories
add_subdirectory(${CMAKE_SOURCE_DIR}/include/SDL)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/ozz-animation)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/meshoptimizer)

# FMOD setup - platform specific
if(UNIX AND NOT APPLE)  # Linux
    set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x86_64/libfmodstudio.so")
    set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x86_64/libfmod.so")
elseif(WIN32)  # Windows
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)  # 64-bit
        set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x64/fmodstudio_vc.lib")
        set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x64/fmod_vc.lib")
    else()  # 32-bit
        set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x86/fmodstudio_vc.lib")
        set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x86/fmod_vc.lib")
    endif()
endif()

set(FMOD_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/inc
        ${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/inc
)

# ReactPhysics3D
find_package(ReactPhysics3D REQUIRED)

target_link_libraries(Gungutils
        PRIVATE
        SDL3::SDL3
        ozz_animation
        OpenGL::GL
        ${FMOD_STUDIO_LIB}
        ${FMOD_CORE_LIB}
        ImGui
        TinyFD
        ReactPhysics3D::ReactPhysics3D
        meshoptimizer
        ${PLATFORM_LIBS}
)

target_include_directories(Gungutils
        PRIVATE
        include
        src
        ${FMOD_INCLUDE_DIRS}
        ${PLATFORM_INCLUDE_DIRS}
        ${JOLT_SOURCE_DIR}
)