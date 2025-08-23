#!/bin/bash

set -e

echo "=== Cross-compiling for Windows x64 using MinGW-w64 ==="

if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "x86_64-w64-mingw32-g++ not found!"
fi

echo "Creating MinGW-w64 toolchain file..."
cat > mingw-w64-toolchain.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where to look for the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Adjust the default behavior of the find commands:
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static linking to avoid DLL dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Use static libraries when possible
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib .so .dll)
EOF

BUILD_DIR="build-windows"
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

echo "Creating build directory: $BUILD_DIR"
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring with CMake..."
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_VERBOSE_MAKEFILE=ON \
      ..

echo "Building project..."
CORES=$(nproc 2>/dev/null || echo 4)
echo "Using $CORES cores for parallel build"
make -j"$CORES"

echo ""
echo "=== Build Complete! ==="
echo "Windows executable should be in: $BUILD_DIR/"
echo "Look for files ending in .exe"

echo ""
echo "Built executables:"
find . -name "*.exe" -type f 2>/dev/null || echo "No .exe files found (this might be normal depending on your project)"

cd ..
rm -f mingw-w64-toolchain.cmake
echo "Cleaned up temporary toolchain file"

echo ""
echo "Done! Your Windows build is ready."