cmake_minimum_required(VERSION 3.22)

# OpenGL
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)

# Platform-specific dependencies
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR WIN32)
    set(PLATFORM_LIBS)
    set(PLATFORM_INCLUDE_DIRS)
else() # Linux or Mac I guess
    find_package(X11 REQUIRED COMPONENTS Xi)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XCURSOR REQUIRED xcursor)
    set(PLATFORM_LIBS ${X11_LIBRARIES} X11::Xi ${XCURSOR_LIBRARIES} dl pthread)
    set(PLATFORM_INCLUDE_DIRS ${X11_INCLUDE_DIR} ${XCURSOR_INCLUDE_DIRS})
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
endif()

# ImGui
set(IMGUI_PATH "${GUNGUTILS_DIR}/include/imgui/")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library(ImGui STATIC ${IMGUI_SOURCES})
target_include_directories(ImGui PUBLIC ${IMGUI_PATH})

# ImPlot
set(IMPLOT_PATH "${GUNGUTILS_DIR}/include/implot/")
file(GLOB IMPLOT_SOURCES ${IMPLOT_PATH}/*.cpp)
add_library(ImPlot STATIC ${IMPLOT_SOURCES})
target_include_directories(ImPlot PUBLIC ${IMPLOT_PATH})
target_link_libraries(ImPlot PUBLIC ImGui)

# ImGuizmo
set(IMGUIZMO_PATH "${GUNGUTILS_DIR}/include/ImGuizmo/")
file(GLOB IMGUIZMO_SOURCES ${IMGUIZMO_PATH}/*.cpp)
add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCES})
target_include_directories(ImGuizmo PUBLIC ${IMGUIZMO_PATH})
target_link_libraries(ImGuizmo PUBLIC ImGui)

# TinyFileDialogs
file(GLOB TINYFD_SOURCES ${GUNGUTILS_DIR}/include/libtinyfiledialogs/*.c)
add_library(TinyFD STATIC ${TINYFD_SOURCES})

# main executable
file(GLOB GUNGUTILS_SRC "${GUNGUTILS_DIR}/gungutils.cpp")
add_library(Gungutils STATIC ${GUNGUTILS_SRC})

# subdirectories
add_subdirectory(${GUNGUTILS_DIR}/include/SDL)
add_subdirectory(${GUNGUTILS_DIR}/include/meshoptimizer)
add_subdirectory(${GUNGUTILS_DIR}/include/reactphysics3d)

if(WIN32)
    set(FMOD_STUDIO_LIB "${GUNGUTILS_DIR}/include/FModStudio/api/studio/lib/x64/fmodstudio_vc.lib")
    set(FMOD_CORE_LIB "${GUNGUTILS_DIR}/include/FModStudio/api/core/lib/x64/fmod_vc.lib")
else() # Linux or Mac but ain't no way I'll use mac anytime
    set(FMOD_STUDIO_LIB "${GUNGUTILS_DIR}/include/FModStudio/api/studio/lib/x86_64/libfmodstudio.so")
    set(FMOD_CORE_LIB "${GUNGUTILS_DIR}/include/FModStudio/api/core/lib/x86_64/libfmod.so")
endif()

if(WIN32)
    add_custom_command(TARGET Gungutils POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:Gungutils>)

    add_custom_command(TARGET Gungutils POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GUNGUTILS_DIR}/include/FModStudio/api/core/lib/x64/fmod.dll"
            $<TARGET_FILE_DIR:Gungutils>)
    add_custom_command(TARGET Gungutils POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GUNGUTILS_DIR}/include/FModStudio/api/studio/lib/x64/fmodstudio.dll"
            $<TARGET_FILE_DIR:Gungutils>)
endif()

set(FMOD_INCLUDE_DIRS
        ${GUNGUTILS_DIR}/include/FModStudio/api/studio/inc
        ${GUNGUTILS_DIR}/include/FModStudio/api/core/inc
)

target_link_libraries(Gungutils
        PRIVATE
        SDL3::SDL3
        OpenGL::GL
        ${FMOD_STUDIO_LIB}
        ${FMOD_CORE_LIB}
        ImGui
        ImPlot
        ImGuizmo
        TinyFD
        reactphysics3d
        meshoptimizer
        ${PLATFORM_LIBS}
        ZLIB::ZLIB
)

target_include_directories(Gungutils
        PUBLIC
        ${GUNGUTILS_DIR}/include
        ${GUNGUTILS_DIR}/src
        ${FMOD_INCLUDE_DIRS}
        ${PLATFORM_INCLUDE_DIRS}
        ${JOLT_SOURCE_DIR}
        ${IMGUI_PATH}
)