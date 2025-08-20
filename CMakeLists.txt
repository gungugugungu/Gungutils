cmake_minimum_required(VERSION 3.22)
project(Gungutils)

set(CMAKE_CXX_STANDARD 20)

find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED COMPONENTS Xi)

find_package(PkgConfig REQUIRED)
pkg_check_modules(XCURSOR REQUIRED xcursor)

set(IMGUI_PATH  "include/imgui/")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library(ImGui STATIC ${IMGUI_SOURCES})
target_include_directories(ImGui PUBLIC ${IMGUI_PATH})

file(GLOB TINYFD_SOURCES ${CMAKE_SOURCE_DIR}/include/libtinyfiledialogs/*.c)
add_library(TinyFD STATIC ${TINYFD_SOURCES})

add_executable(Gungutils main.cpp)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/SDL)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/ozz-animation)
add_subdirectory(${CMAKE_SOURCE_DIR}/include/meshoptimizer)

# FMOD
set(FMOD_STUDIO_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/studio/lib/x86_64/libfmodstudio.so")
set(FMOD_CORE_LIB "${CMAKE_SOURCE_DIR}/include/FModStudio/api/core/lib/x86_64/libfmod.so")
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
        X11::Xi
        ${X11_LIBRARIES}
        ${XCURSOR_LIBRARIES}
        ${FMOD_STUDIO_LIB}
        ${FMOD_CORE_LIB}
        ImGui
        TinyFD
        dl
        pthread
        ReactPhysics3D::ReactPhysics3D
        meshoptimizer
)

target_include_directories(Gungutils
    PRIVATE
        include
        src
        ${FMOD_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
        ${XCURSOR_INCLUDE_DIRS}
        ${JOLT_SOURCE_DIR}
)