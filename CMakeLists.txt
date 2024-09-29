cmake_minimum_required(VERSION 3.10)
project(video2x VERSION 1.0 LANGUAGES C)

# Set the C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER clang)

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Define the output directories
#set(BINARY_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUTPUT_DIR})

# Find the required packages using pkg-config
find_package(PkgConfig REQUIRED)

# List of required packages
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

# Create the shared library 'libvideo2x'
add_library(video2x SHARED src/libvideo2x.c)
set_target_properties(video2x PROPERTIES OUTPUT_NAME libvideo2x)

# Include directories for the shared library
target_include_directories(video2x PRIVATE ${ALL_INCLUDE_DIRS})

# Compile options for the shared library
target_compile_options(video2x PRIVATE
    -Wall
    -fPIC
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Link the shared library with the dependencies
target_link_libraries(video2x PRIVATE ${ALL_LIBRARIES})

# Create the executable 'video2x'
add_executable(video2x_exe src/video2x.c)
set_target_properties(video2x_exe PROPERTIES OUTPUT_NAME video2x)

# Include directories for the executable
target_include_directories(video2x_exe PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Compile options for the executable
target_compile_options(video2x_exe PRIVATE
    -Wall
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Link the executable with the shared library
target_link_libraries(video2x_exe PRIVATE video2x)

