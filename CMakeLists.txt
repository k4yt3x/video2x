cmake_minimum_required(VERSION 3.10)
project(video2x VERSION 6.0.0 LANGUAGES CXX C)

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Set the default optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Ox /GL /LTCG /MD /DNDEBUG")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox /GL /LTCG /MD /DNDEBUG")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native -flto")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")
    endif()
endif()

# Build options
option(BUILD_SHARED_LIBS "Build libvideo2x as a shared library" ON)
option(BUILD_VIDEO2X "Build the video2x executable" ON)
option(USE_SYSTEM_NCNN "Use system ncnn library" ON)

set(ALL_INCLUDE_DIRS)
set(ALL_LIBRARIES)

if(WIN32)
    # Define base paths for FFmpeg and ncnn
    set(FFMPEG_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ffmpeg-shared)
    set(NCNN_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ncnn-shared/x64)

    # FFmpeg
    list(APPEND ALL_LIBRARIES
        ${FFMPEG_BASE_PATH}/lib/avcodec.lib
        ${FFMPEG_BASE_PATH}/lib/avdevice.lib
        ${FFMPEG_BASE_PATH}/lib/avfilter.lib
        ${FFMPEG_BASE_PATH}/lib/avformat.lib
        ${FFMPEG_BASE_PATH}/lib/avutil.lib
        ${FFMPEG_BASE_PATH}/lib/swscale.lib
    )
    list(APPEND ALL_INCLUDE_DIRS ${FFMPEG_BASE_PATH}/include)

    # ncnn
    # TODO: Figure out why this file is not being copied to the install directory
    set(SPIRV_BUILD_PATH ${CMAKE_BINARY_DIR}/realesrgan-prefix/src/realesrgan-build/ncnn/glslang/SPIRV)
    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(SPIRV_LIB ${SPIRV_BUILD_PATH}/Release/SPIRV.lib)
    else()
        set(SPIRV_LIB ${SPIRV_BUILD_PATH}/Debug/SPIRVd.lib)
    endif()

    list(APPEND ALL_LIBRARIES
        ${NCNN_BASE_PATH}/lib/ncnn.lib
        ${SPIRV_LIB}
    )
    list(APPEND ALL_INCLUDE_DIRS ${NCNN_BASE_PATH}/include/ncnn)
else()
    # Find the required packages using pkg-config
    find_package(PkgConfig REQUIRED)
    set(REQUIRED_PKGS
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        libswscale
    )

    # Loop through each package to find and collect include dirs and libraries
    foreach(PKG ${REQUIRED_PKGS})
        pkg_check_modules(${PKG} REQUIRED ${PKG})
        list(APPEND ALL_INCLUDE_DIRS ${${PKG}_INCLUDE_DIRS})
        list(APPEND ALL_LIBRARIES ${${PKG}_LIBRARIES})
    endforeach()
endif()

# Remove duplicate entries
list(REMOVE_DUPLICATES ALL_INCLUDE_DIRS)
list(REMOVE_DUPLICATES ALL_LIBRARIES)

# Find ncnn package
if(USE_SYSTEM_NCNN)
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

# Include ExternalProject module
include(ExternalProject)

# Add libreal-esrgan-ncnn-vulkan as an external project
ExternalProject_Add(
    realesrgan
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/libreal_esrgan_ncnn_vulkan/src
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/realesrgan_install
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DUSE_SYSTEM_NCNN=${USE_SYSTEM_NCNN}
    BUILD_ALWAYS ON
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE}
)

# Add all source files for libvideo2x
file(GLOB LIBVIDEO2X_SOURCES src/*.cpp)

# Create the shared library 'libvideo2x'
add_library(libvideo2x ${LIBVIDEO2X_SOURCES})
target_compile_definitions(libvideo2x PRIVATE LIBVIDEO2X_EXPORTS)
if(WIN32)
    set_target_properties(libvideo2x PROPERTIES OUTPUT_NAME libvideo2x)
else()
    set_target_properties(libvideo2x PROPERTIES OUTPUT_NAME video2x)
endif()

# Ensure libvideo2x depends on realesrgan being built and installed
add_dependencies(libvideo2x realesrgan)

# Include directories for the shared library
target_include_directories(libvideo2x PRIVATE
    ${ALL_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/libreal_esrgan_ncnn_vulkan/src
)

# Compile options for the shared library
target_compile_options(libvideo2x PRIVATE
    -Wall
    -fPIC
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Define the path to the built libresrgan-ncnn-vulkan library
if(WIN32)
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan_install/lib/realesrgan-ncnn-vulkan.lib)
else()
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan_install/lib/librealesrgan-ncnn-vulkan.so)
endif()

# Link the shared library with the dependencies
target_link_libraries(libvideo2x PRIVATE ${ALL_LIBRARIES} ${REALESRGAN_LIB})

if(NOT WIN32)
    if (USE_SYSTEM_NCNN)
        target_link_libraries(libvideo2x PUBLIC ncnn)
    else()
        target_link_libraries(libvideo2x PRIVATE ncnn)
    endif()
endif()

# Create the executable 'video2x'
if (BUILD_VIDEO2X)
    add_executable(video2x src/video2x.c src/getopt.c)
    set_target_properties(video2x PROPERTIES OUTPUT_NAME video2x)

    # Include directories for the executable
    target_include_directories(video2x PRIVATE
        ${ALL_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/include
    )

    # Compile options for the executable
    target_compile_options(video2x PRIVATE
        -Wall
        $<$<CONFIG:Debug>:-g -DDEBUG>
    )

    # Link the executable with the shared library
    target_link_libraries(video2x PRIVATE ${ALL_LIBRARIES} libvideo2x)
endif()

# Define the default installation directories
if(WIN32)
    set(BIN_DESTINATION_DEFAULT ".")
    set(INCLUDE_DESTINATION_DEFAULT "include")
    set(LIB_DESTINATION_DEFAULT ".")
    set(MODEL_DESTINATION_DEFAULT ".")
else()
    set(BIN_DESTINATION_DEFAULT "bin")
    set(INCLUDE_DESTINATION_DEFAULT "include")
    set(LIB_DESTINATION_DEFAULT "lib")
    set(MODEL_DESTINATION_DEFAULT "share/video2x")
endif()

# Set the installation directories
set(INSTALL_BIN_DESTINATION ${BIN_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_INCLUDE_DESTINATION ${INCLUDE_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_LIB_DESTINATION ${LIB_DESTINATION_DEFAULT} CACHE STRING "")
set(INSTALL_MODEL_DESTINATION ${MODEL_DESTINATION_DEFAULT} CACHE STRING "")

# Common installation rules for libvideo2x and models
install(TARGETS libvideo2x
    LIBRARY DESTINATION ${INSTALL_LIB_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    ARCHIVE DESTINATION ${INSTALL_LIB_DESTINATION}
    RUNTIME DESTINATION ${INSTALL_BIN_DESTINATION}
)

# Install model files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/models DESTINATION ${INSTALL_MODEL_DESTINATION})

# Install the executable if BUILD_VIDEO2X is enabled
if(BUILD_VIDEO2X)
    install(TARGETS video2x RUNTIME DESTINATION ${INSTALL_BIN_DESTINATION})
endif()

# Install the header file
install(FILES ${PROJECT_SOURCE_DIR}/include/libvideo2x.h DESTINATION ${INSTALL_INCLUDE_DESTINATION})

# Platform-specific installation rules
if(WIN32)
    # Install Windows-specific dependencies
    install(FILES ${CMAKE_BINARY_DIR}/realesrgan_install/bin/realesrgan-ncnn-vulkan.dll
        DESTINATION ${INSTALL_BIN_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
    install(FILES ${NCNN_BASE_PATH}/bin/ncnn.dll DESTINATION ${INSTALL_BIN_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
else()
    # Install Unix-specific dependencies
    install(FILES ${REALESRGAN_LIB} DESTINATION ${INSTALL_LIB_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
endif()
