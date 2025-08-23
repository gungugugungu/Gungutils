cmake_minimum_required(VERSION 3.22)
project(Gungutils)
set(CMAKE_CXX_STANDARD 20)

# OpenGL
find_package(OpenGL REQUIRED)

# Platform-specific dependencies
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM_LIBS)
    set(PLATFORM_INCLUDE_DIRS)
else() # Linux or Mac I guess
    find_package(X11 REQUIRED COMPONENTS Xi)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCURSOR REQUIRED xcursor)
    set(PLATFORM_LIBS ${X11_LIBRARIES} X11::Xi ${XCURSOR_LIBRARIES} dl pthread)
    set(PLATFORM_INCLUDE_DIRS ${X11_INCLUDE_DIR} ${XCURSOR_INCLUDE_DIRS})
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
#add_subdirectory(${CMAKE_SOURCE_DIR}/include/ozz-animation)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/meshoptimizer)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")  # Windows
    set(FMOD_STUDIO_A "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x64/libfmodstudio.a")
    set(FMOD_CORE_A   "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x64/libfmod.a")

    if(EXISTS "${FMOD_STUDIO_A}" AND EXISTS "${FMOD_CORE_A}")
        message(STATUS "using mingw import libs: ${FMOD_STUDIO_A}, ${FMOD_CORE_A}")
        set(FMOD_STUDIO_LIB "${FMOD_STUDIO_A}")
        set(FMOD_CORE_LIB "${FMOD_CORE_A}")
    else()
        set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x64/fmodstudio.dll")
        set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x64/fmod.dll")
    endif()
else() # Linux or Mac but ain't no way I'll use mac anytime
    set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x86_64/libfmodstudio.so")
    set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x86_64/libfmod.so")
endif()

set(FMOD_INCLUDE_DIRS
        ${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/inc
        ${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/inc
)

# ReactPhysics3D
add_subdirectory(${CMAKE_SOURCE_DIR}/include/reactphysics3d)

target_link_libraries(Gungutils
        PRIVATE
        SDL3::SDL3
        #ozz_animation
        OpenGL::GL
        ${FMOD_STUDIO_LIB}
        ${FMOD_CORE_LIB}
        ImGui
        TinyFD
        reactphysics3d
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