cmake_minimum_required(VERSION 3.22)
project(Gungutils)

execute_process(
        COMMAND ${CMAKE_SOURCE_DIR}/compile_shaders.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set(CMAKE_CXX_STANDARD 20)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED COMPONENTS Xi)

# Find additional X11 components
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCURSOR REQUIRED xcursor)

add_executable(Gungutils main.cpp)

# Link against the required libraries
target_link_libraries(Gungutils
    PRIVATE
    OpenGL::GL
        X11::Xi
        ${X11_LIBRARIES}
    ${XCURSOR_LIBRARIES}
    dl
    pthread
)

# Include directories
target_include_directories(Gungutils
    PRIVATE
    include
    ${X11_INCLUDE_DIR}
    ${XCURSOR_INCLUDE_DIRS}
)