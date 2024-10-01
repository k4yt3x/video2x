cmake_minimum_required(VERSION 3.10)
project(video2x VERSION 1.0 LANGUAGES CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set clang as the default compiler if not specified
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER clang++)
endif()

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Define the output directories
set(BINARY_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})

# Find the required packages using pkg-config
find_package(PkgConfig REQUIRED)
set(REQUIRED_PKGS
    libavformat
    libavcodec
    libavfilter
    libavutil
    libswscale
    libavdevice
    libplacebo
)

# Loop through each package to find and collect include dirs and libraries
set(ALL_INCLUDE_DIRS "")
set(ALL_LIBRARIES "")

foreach(PKG ${REQUIRED_PKGS})
    pkg_check_modules(${PKG} REQUIRED ${PKG})
    list(APPEND ALL_INCLUDE_DIRS ${${PKG}_INCLUDE_DIRS})
    list(APPEND ALL_LIBRARIES ${${PKG}_LIBRARIES})
endforeach()

# Remove duplicate entries
list(REMOVE_DUPLICATES ALL_INCLUDE_DIRS)
list(REMOVE_DUPLICATES ALL_LIBRARIES)

# Include libreal_esrgan_ncnn_vulkan as a subdirectory
add_subdirectory(third_party/libreal_esrgan_ncnn_vulkan/src)

# Add all source files for libvideo2x
file(GLOB LIBVIDEO2X_SOURCES src/*.cpp)

# Create the shared library 'libvideo2x'
add_library(libvideo2x SHARED ${LIBVIDEO2X_SOURCES})
set_target_properties(libvideo2x PROPERTIES OUTPUT_NAME video2x)

# Include directories for the shared library
target_include_directories(libvideo2x PRIVATE
    ${ALL_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/libreal_esrgan_ncnn_vulkan/src
    ${PROJECT_SOURCE_DIR}/third_party/libreal_esrgan_ncnn_vulkan/src/ncnn/src
)

# Compile options for the shared library
target_compile_options(libvideo2x PRIVATE
    -Wall
    -fPIC
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Ensure the shared library uses at least C++17
set_target_properties(libvideo2x PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Link the shared library with the dependencies
target_link_libraries(libvideo2x PRIVATE ${ALL_LIBRARIES} ncnn librealesrgan-ncnn-vulkan)

# Create the executable 'video2x'
add_executable(video2x src/video2x.cpp)
set_target_properties(video2x PROPERTIES OUTPUT_NAME video2x)

# Include directories for the executable
target_include_directories(video2x PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Compile options for the executable
target_compile_options(video2x PRIVATE
    -Wall
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Ensure the executable uses at least C++17
set_target_properties(video2x PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Link the executable with the shared library
target_link_libraries(video2x PRIVATE libvideo2x)

