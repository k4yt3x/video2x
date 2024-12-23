cmake_minimum_required(VERSION 3.10)
project(video2x VERSION 6.3.1 LANGUAGES CXX)

# The FindBoost module is removed in CMake 3.30
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build libvideo2x as a shared library" ON)
option(VIDEO2X_BUILD_CLI "Build the video2x command line interface executable" ON)

option(VIDEO2X_ENABLE_NATIVE "Enable native optimizations (-march=native)" OFF)
option(VIDEO2X_ENABLE_X86_64_V4 "Enable x86-64-v4 optimizations (-march=x86-64-v4)" OFF)
option(VIDEO2X_ENABLE_AVX512F "Enable AVX-512 foundation optimizations (-march=avx512f)" OFF)
option(VIDEO2X_ENABLE_X86_64_V3 "Enable x86-64-v3 optimizations (-march=x86-64-v3)" OFF)
option(VIDEO2X_ENABLE_AVX2 "Enable AVX2 optimizations (-march=avx2)" OFF)

option(VIDEO2X_USE_EXTERNAL_NCNN "Use the system-provided ncnn library" ON)
option(VIDEO2X_USE_EXTERNAL_SPDLOG "Use the system-provided spdlog library" ON)
option(VIDEO2X_USE_EXTERNAL_BOOST "Use the system-provided Boost library" ON)

# Set global compile options for all targets
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Set the default optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Set the optimization flags for each compiler
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/Ox /Ot /GL /DNDEBUG)
        add_link_options(/LTCG /OPT:REF /OPT:ICF)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-O3 -ffunction-sections -fdata-sections)
        add_link_options(-Wl,-s -flto -Wl,--gc-sections)
    endif()
endif()

# Enable the requested architecture-specific optimizations
if(VIDEO2X_ENABLE_NATIVE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:NATIVE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=native)
    endif()
elseif(VIDEO2X_ENABLE_X86_64_V4)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=x86-64-v4)
    endif()
elseif(VIDEO2X_ENABLE_AVX512F)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX512)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-mavx512f)
    endif()
elseif(VIDEO2X_ENABLE_X86_64_V3)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=x86-64-v3)
    endif()
elseif(VIDEO2X_ENABLE_AVX2)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-mavx2)
    endif()
endif()

# Define lists to store include directories and libraries
set(LIBVIDEO2X_INCLUDE_DIRS)
set(LIBVIDEO2X_LIBS)
set(VIDEO2X_INCLUDE_DIRS)
set(VIDEO2X_LIBS)

# Platform-specific dependencies
if(WIN32)
    # Define base paths for FFmpeg and ncnn
    set(FFMPEG_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ffmpeg-shared)
    set(NCNN_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ncnn-shared/x64)

    # FFmpeg
    set(FFMPEG_LIB
        ${FFMPEG_BASE_PATH}/lib/avcodec.lib
        ${FFMPEG_BASE_PATH}/lib/avdevice.lib
        ${FFMPEG_BASE_PATH}/lib/avfilter.lib
        ${FFMPEG_BASE_PATH}/lib/avformat.lib
        ${FFMPEG_BASE_PATH}/lib/avutil.lib
        ${FFMPEG_BASE_PATH}/lib/swscale.lib
    )
    list(APPEND LIBVIDEO2X_LIBS ${FFMPEG_LIB})
    list(APPEND LIBVIDEO2X_INCLUDE_DIRS ${FFMPEG_BASE_PATH}/include)
    list(APPEND VIDEO2X_LIBS ${FFMPEG_LIB})
    list(APPEND VIDEO2X_INCLUDE_DIRS ${FFMPEG_BASE_PATH}/include)

    # ncnn
    set(SPIRV_BUILD_PATH
        ${CMAKE_BINARY_DIR}/realesrgan-prefix/src/realesrgan-build/ncnn/glslang/SPIRV
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(SPIRV_LIB ${SPIRV_BUILD_PATH}/Release/SPIRV.lib)
    else()
        set(SPIRV_LIB ${SPIRV_BUILD_PATH}/Debug/SPIRVd.lib)
    endif()

    list(APPEND LIBVIDEO2X_LIBS ${NCNN_BASE_PATH}/lib/ncnn.lib ${SPIRV_LIB})
    list(APPEND LIBVIDEO2X_INCLUDE_DIRS ${NCNN_BASE_PATH}/include/ncnn)
else()
    # FFmpeg
    find_package(PkgConfig REQUIRED)
    set(FFMPEG_REQUIRED_PKGS
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        libswscale
    )

    # Loop through each package to find and collect include dirs and libraries
    foreach(PKG ${FFMPEG_REQUIRED_PKGS})
        pkg_check_modules(${PKG} REQUIRED ${PKG})
        list(APPEND LIBVIDEO2X_INCLUDE_DIRS ${${PKG}_INCLUDE_DIRS})
        list(APPEND LIBVIDEO2X_LIBS ${${PKG}_LIBRARIES})
        list(APPEND VIDEO2X_INCLUDE_DIRS ${${PKG}_INCLUDE_DIRS})
        list(APPEND VIDEO2X_LIBS ${${PKG}_LIBRARIES})
    endforeach()
endif() # WIN32

# Find ncnn package
if(VIDEO2X_USE_EXTERNAL_NCNN)
    find_package(ncnn REQUIRED)
else()
    option(NCNN_INSTALL_SDK "" OFF)
    option(NCNN_PIXEL_ROTATE "" OFF)
    option(NCNN_VULKAN "" ON)
    option(NCNN_VULKAN_ONLINE_SPIRV "" ON)
    option(NCNN_BUILD_BENCHMARK "" OFF)
    option(NCNN_BUILD_TESTS "" OFF)
    option(NCNN_BUILD_TOOLS "" OFF)
    option(NCNN_BUILD_EXAMPLES "" OFF)
    option(NCNN_DISABLE_RTTI "" ON)
    option(NCNN_DISABLE_EXCEPTION "" ON)
    option(NCNN_BUILD_SHARED_LIBS "" OFF)
    option(SKIP_GLSLANG_INSTALL "" ON)

    option(WITH_LAYER_absval "" OFF)
    option(WITH_LAYER_argmax "" OFF)
    option(WITH_LAYER_batchnorm "" OFF)
    option(WITH_LAYER_bias "" OFF)
    option(WITH_LAYER_bnll "" OFF)
    option(WITH_LAYER_concat "" ON)
    option(WITH_LAYER_convolution "" ON)
    option(WITH_LAYER_crop "" ON)
    option(WITH_LAYER_deconvolution "" OFF)
    option(WITH_LAYER_dropout "" OFF)
    option(WITH_LAYER_eltwise "" ON)
    option(WITH_LAYER_elu "" OFF)
    option(WITH_LAYER_embed "" OFF)
    option(WITH_LAYER_exp "" OFF)
    option(WITH_LAYER_flatten "" ON)
    option(WITH_LAYER_innerproduct "" ON)
    option(WITH_LAYER_input "" ON)
    option(WITH_LAYER_log "" OFF)
    option(WITH_LAYER_lrn "" OFF)
    option(WITH_LAYER_memorydata "" OFF)
    option(WITH_LAYER_mvn "" OFF)
    option(WITH_LAYER_pooling "" OFF)
    option(WITH_LAYER_power "" OFF)
    option(WITH_LAYER_prelu "" ON)
    option(WITH_LAYER_proposal "" OFF)
    option(WITH_LAYER_reduction "" OFF)
    option(WITH_LAYER_relu "" ON)
    option(WITH_LAYER_reshape "" OFF)
    option(WITH_LAYER_roipooling "" OFF)
    option(WITH_LAYER_scale "" OFF)
    option(WITH_LAYER_sigmoid "" OFF)
    option(WITH_LAYER_slice "" OFF)
    option(WITH_LAYER_softmax "" OFF)
    option(WITH_LAYER_split "" ON)
    option(WITH_LAYER_spp "" OFF)
    option(WITH_LAYER_tanh "" OFF)
    option(WITH_LAYER_threshold "" OFF)
    option(WITH_LAYER_tile "" OFF)
    option(WITH_LAYER_rnn "" OFF)
    option(WITH_LAYER_lstm "" OFF)
    option(WITH_LAYER_binaryop "" ON)
    option(WITH_LAYER_unaryop "" OFF)
    option(WITH_LAYER_convolutiondepthwise "" OFF)
    option(WITH_LAYER_padding "" ON)
    option(WITH_LAYER_squeeze "" OFF)
    option(WITH_LAYER_expanddims "" OFF)
    option(WITH_LAYER_normalize "" OFF)
    option(WITH_LAYER_permute "" OFF)
    option(WITH_LAYER_priorbox "" OFF)
    option(WITH_LAYER_detectionoutput "" OFF)
    option(WITH_LAYER_interp "" ON)
    option(WITH_LAYER_deconvolutiondepthwise "" OFF)
    option(WITH_LAYER_shufflechannel "" OFF)
    option(WITH_LAYER_instancenorm "" OFF)
    option(WITH_LAYER_clip "" OFF)
    option(WITH_LAYER_reorg "" OFF)
    option(WITH_LAYER_yolodetectionoutput "" OFF)
    option(WITH_LAYER_quantize "" OFF)
    option(WITH_LAYER_dequantize "" OFF)
    option(WITH_LAYER_yolov3detectionoutput "" OFF)
    option(WITH_LAYER_psroipooling "" OFF)
    option(WITH_LAYER_roialign "" OFF)
    option(WITH_LAYER_packing "" ON)
    option(WITH_LAYER_requantize "" OFF)
    option(WITH_LAYER_cast "" ON)
    option(WITH_LAYER_hardsigmoid "" OFF)
    option(WITH_LAYER_selu "" OFF)
    option(WITH_LAYER_hardswish "" OFF)
    option(WITH_LAYER_noop "" OFF)
    option(WITH_LAYER_pixelshuffle "" ON)
    option(WITH_LAYER_deepcopy "" OFF)
    option(WITH_LAYER_mish "" OFF)
    option(WITH_LAYER_statisticspooling "" OFF)
    option(WITH_LAYER_swish "" OFF)
    option(WITH_LAYER_gemm "" OFF)
    option(WITH_LAYER_groupnorm "" OFF)
    option(WITH_LAYER_layernorm "" OFF)
    option(WITH_LAYER_softplus "" OFF)

    add_subdirectory(third_party/ncnn)
endif()

# spdlog
if(VIDEO2X_USE_EXTERNAL_SPDLOG)
    find_package(spdlog REQUIRED)
    list(APPEND LIBVIDEO2X_INCLUDE_DIRS ${spdlog_INCLUDE_DIRS})
    list(APPEND VIDEO2X_INCLUDE_DIRS ${spdlog_INCLUDE_DIRS})
    set(SPDLOG_LIB spdlog::spdlog)
else()
    # spdlog exceptions are incompatible with ncnn
    option(SPDLOG_NO_EXCEPTIONS "" ON)
    add_subdirectory(third_party/spdlog)
    set(SPDLOG_LIB spdlog::spdlog_header_only)
endif()
list(APPEND LIBVIDEO2X_LIBS ${SPDLOG_LIB})
list(APPEND VIDEO2X_LIBS ${SPDLOG_LIB})

# Find dependencies required for the CLI
if(VIDEO2X_BUILD_CLI)
    # Vulkan
    find_package(Vulkan REQUIRED)
    list(APPEND VIDEO2X_LIBS Vulkan::Vulkan)

    # Boost
    if(VIDEO2X_USE_EXTERNAL_BOOST)
        find_package(Boost REQUIRED COMPONENTS program_options)
        list(APPEND LIBVIDEO2X_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
    else()
        option(Boost_USE_STATIC_LIBS "" ON)
        option(Boost_USE_STATIC_RUNTIME "" ON)
        option(Boost_COMPONENTS "program_options")

        add_subdirectory(third_party/boost)
        include_directories(${PROJECT_SOURCE_DIR}/third_party/boost/libs/program_options/include)
        set(BOOST_BASE_PATH
            ${CMAKE_BINARY_DIR}/third_party/boost/libs/program_options/${CMAKE_BUILD_TYPE}
        )
    endif()
    list(APPEND VIDEO2X_LIBS Boost::program_options)
endif()

# Include ExternalProject module
include(ExternalProject)

# Add librealesrgan-ncnn-vulkan as an external project
ExternalProject_Add(
    realesrgan
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/librealesrgan_ncnn_vulkan/src
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/realesrgan-install
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DUSE_SYSTEM_NCNN=${VIDEO2X_USE_EXTERNAL_NCNN}
    BUILD_ALWAYS ON
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE}
)

# Add librealcugan-ncnn-vulkan as an external project
ExternalProject_Add(
    realcugan
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/librealcugan_ncnn_vulkan/src
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/realcugan-install
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DUSE_SYSTEM_NCNN=${VIDEO2X_USE_EXTERNAL_NCNN}
    BUILD_ALWAYS ON
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE}
)

# Add librife-ncnn-vulkan as an external project
ExternalProject_Add(
    rife
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/librife_ncnn_vulkan/src
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/rife-install
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DUSE_SYSTEM_NCNN=${VIDEO2X_USE_EXTERNAL_NCNN}
    BUILD_ALWAYS ON
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE}
)

# Remove duplicate entries
list(REMOVE_DUPLICATES LIBVIDEO2X_INCLUDE_DIRS)
list(REMOVE_DUPLICATES LIBVIDEO2X_LIBS)
list(REMOVE_DUPLICATES VIDEO2X_INCLUDE_DIRS)
list(REMOVE_DUPLICATES VIDEO2X_LIBS)

# Create the shared library 'libvideo2x'
file(GLOB LIBVIDEO2X_SOURCES src/*.cpp)
add_library(libvideo2x ${LIBVIDEO2X_SOURCES})
target_compile_definitions(libvideo2x PRIVATE LIBVIDEO2X_EXPORTS)
if(WIN32)
    set_target_properties(libvideo2x PROPERTIES OUTPUT_NAME libvideo2x)
else()
    set_target_properties(libvideo2x PROPERTIES OUTPUT_NAME video2x)
endif()

# Ensure that the shared library is built after the external projects
add_dependencies(libvideo2x realesrgan realcugan rife)

# Generate the version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h"
    @ONLY
)

# Include directories for the shared library
target_include_directories(libvideo2x PRIVATE
    ${LIBVIDEO2X_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/libvideo2x
    ${PROJECT_SOURCE_DIR}/third_party/librealesrgan_ncnn_vulkan/src
    ${PROJECT_SOURCE_DIR}/third_party/librealcugan_ncnn_vulkan/src
    ${PROJECT_SOURCE_DIR}/third_party/librife_ncnn_vulkan/src
)

# Compile options for the shared library
target_compile_options(libvideo2x PRIVATE -fPIC $<$<CONFIG:Debug>:-g -DDEBUG>)

# Define the paths to the shared libraries
if(WIN32)
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan-install/lib/librealesrgan-ncnn-vulkan.lib)
    set(REALCUGAN_LIB ${CMAKE_BINARY_DIR}/realcugan-install/lib/librealcugan-ncnn-vulkan.lib)
    set(RIFE_LIB ${CMAKE_BINARY_DIR}/rife-install/lib/librife-ncnn-vulkan.lib)
else()
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan-install/lib/librealesrgan-ncnn-vulkan.so)
    set(REALCUGAN_LIB ${CMAKE_BINARY_DIR}/realcugan-install/lib/librealcugan-ncnn-vulkan.so)
    set(RIFE_LIB ${CMAKE_BINARY_DIR}/rife-install/lib/librife-ncnn-vulkan.so)
endif()
list(APPEND LIBVIDEO2X_LIBS ${REALESRGAN_LIB} ${REALCUGAN_LIB} ${RIFE_LIB})

# Link the shared library with the dependencies
target_link_libraries(libvideo2x PRIVATE ${LIBVIDEO2X_LIBS})

if(NOT WIN32)
    if(VIDEO2X_USE_EXTERNAL_NCNN)
        target_link_libraries(libvideo2x PUBLIC ncnn)
    else()
        target_link_libraries(libvideo2x PRIVATE ncnn)
    endif()
endif()

# Build the Video2X CLI executable
if(VIDEO2X_BUILD_CLI)
    file(GLOB VIDEO2X_SOURCES tools/video2x/src/*.cpp)
    add_executable(video2x ${VIDEO2X_SOURCES})
    set_target_properties(video2x PROPERTIES OUTPUT_NAME video2x)

    # Include directories for the executable
    target_include_directories(video2x PRIVATE
        ${VIDEO2X_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/tools/video2x/include
    )

    # Compile options for the executable
    target_compile_options(video2x PRIVATE $<$<CONFIG:Debug>:-g -DDEBUG>)

    # Link the executable with the shared library
    target_link_libraries(video2x PRIVATE libvideo2x ${VIDEO2X_LIBS})
endif()

# Define the default installation directories
if(WIN32)
    set(BIN_DESTINATION_DEFAULT ".")
    set(INCLUDE_DESTINATION_DEFAULT "include/libvideo2x")
    set(LIB_DESTINATION_DEFAULT ".")
    set(MODEL_DESTINATION_DEFAULT ".")
else()
    set(BIN_DESTINATION_DEFAULT "bin")
    set(INCLUDE_DESTINATION_DEFAULT "include/libvideo2x")
    set(LIB_DESTINATION_DEFAULT "lib")
    set(MODEL_DESTINATION_DEFAULT "share/video2x")
endif()

# Set the installation directories
set(INSTALL_BIN_DESTINATION ${BIN_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_INCLUDE_DESTINATION ${INCLUDE_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_LIB_DESTINATION ${LIB_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_MODEL_DESTINATION ${MODEL_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_CMAKE_DESTINATION "${LIB_DESTINATION_DEFAULT}/cmake/Video2X" CACHE PATH "")

# Common installation rules for libvideo2x and models
install(TARGETS libvideo2x
    EXPORT Video2XTargets
    LIBRARY DESTINATION ${INSTALL_LIB_DESTINATION}
    ARCHIVE DESTINATION ${INSTALL_LIB_DESTINATION}
    RUNTIME DESTINATION ${INSTALL_BIN_DESTINATION}
)

# Install the header files from the include directory
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/libvideo2x/
    DESTINATION ${INSTALL_INCLUDE_DESTINATION}
    FILES_MATCHING PATTERN "*.h"
)

# Install the generated version.h file
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h
    DESTINATION ${INSTALL_INCLUDE_DESTINATION}
)

# Export targets and create CMake package config
install(EXPORT Video2XTargets
    FILE Video2XTargets.cmake
    NAMESPACE Video2X::
    DESTINATION ${INSTALL_CMAKE_DESTINATION}
)

# Generate the CMake package config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Video2XConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Video2XConfig.cmake"
    INSTALL_DESTINATION ${INSTALL_CMAKE_DESTINATION}
)

# Install the CMake package config file
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Video2XConfig.cmake"
    DESTINATION ${INSTALL_CMAKE_DESTINATION}
)

# Install model files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/models DESTINATION ${INSTALL_MODEL_DESTINATION})

# Platform-specific installation rules
if(WIN32)
    file(GLOB FFMPEG_DLLS "${FFMPEG_BASE_PATH}/bin/*.dll")
    install(FILES
        ${CMAKE_BINARY_DIR}/realesrgan-install/bin/librealesrgan-ncnn-vulkan.dll
        ${CMAKE_BINARY_DIR}/realcugan-install/bin/librealcugan-ncnn-vulkan.dll
        ${CMAKE_BINARY_DIR}/rife-install/bin/librife-ncnn-vulkan.dll
        ${FFMPEG_DLLS}
        ${NCNN_BASE_PATH}/bin/ncnn.dll
        DESTINATION ${INSTALL_BIN_DESTINATION}
    )
else()
    install(FILES ${REALESRGAN_LIB} ${REALCUGAN_LIB} ${RIFE_LIB}
        DESTINATION ${INSTALL_LIB_DESTINATION}
    )
endif()

# Install the Video2X CLI executable
if(VIDEO2X_BUILD_CLI)
    install(TARGETS video2x RUNTIME DESTINATION ${INSTALL_BIN_DESTINATION})

    # Install the Boost DLL
    if(WIN32 AND NOT VIDEO2X_USE_EXTERNAL_BOOST)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(BOOST_DLL_PATH ${BOOST_BASE_PATH}/boost_program_options-vc143-mt-x64-1_86.dll)
        else()
            set(BOOST_DLL_PATH ${BOOST_BASE_PATH}/boost_program_options-vc143-mt-gd-x64-1_86.dll)
        endif()
        install(FILES ${BOOST_DLL_PATH} DESTINATION ${INSTALL_BIN_DESTINATION})
    endif()
endif()

