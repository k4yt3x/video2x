cmake_minimum_required(VERSION 3.10)
project(video2x VERSION 6.1.1 LANGUAGES CXX)

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
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox /GL /LTCG /MD /DNDEBUG")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
        set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -s")
    endif()
endif()

# Set global compile options for all targets
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /permissive-)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build libvideo2x as a shared library" ON)
option(BUILD_VIDEO2X_CLI "Build the video2x executable" ON)
option(USE_SYSTEM_OPENCV "Use system OpenCV library" ON)
option(USE_SYSTEM_NCNN "Use system ncnn library" ON)
option(USE_SYSTEM_SPDLOG "Use system spdlog library" ON)
option(USE_SYSTEM_BOOST "Use system Boost library" ON)

# Generate the version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/libvideo2x/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h"
    @ONLY
)

# Find the required packages
set(ALL_INCLUDE_DIRS)
set(ALL_LIBRARIES)

# Platform-specific dependencies
if(WIN32)
    # Define base paths for FFmpeg and ncnn
    set(FFMPEG_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ffmpeg-shared)
    set(NCNN_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/ncnn-shared/x64)
    set(OPENCV_BASE_PATH ${PROJECT_SOURCE_DIR}/third_party/opencv-shared)

    # FFmpeg
    set(FFMPEG_LIB
        ${FFMPEG_BASE_PATH}/lib/avcodec.lib
        ${FFMPEG_BASE_PATH}/lib/avdevice.lib
        ${FFMPEG_BASE_PATH}/lib/avfilter.lib
        ${FFMPEG_BASE_PATH}/lib/avformat.lib
        ${FFMPEG_BASE_PATH}/lib/avutil.lib
        ${FFMPEG_BASE_PATH}/lib/swscale.lib
    )
    list(APPEND ALL_LIBRARIES ${FFMPEG_LIB})
    list(APPEND ALL_INCLUDE_DIRS ${FFMPEG_BASE_PATH}/include)

    # ncnn
    # TODO: Figure out why this file is not being copied to the install directory
    set(SPIRV_BUILD_PATH
        ${CMAKE_BINARY_DIR}/realesrgan-prefix/src/realesrgan-build/ncnn/glslang/SPIRV
    )
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

    # OpenCV
    # list(APPEND ALL_LIBRARIES ${OPENCV_BASE_PATH}/build/x64/vc16/lib/opencv_world4100.lib)
    list(APPEND ALL_INCLUDE_DIRS ${OPENCV_BASE_PATH}/build/include)
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
    set(FFMPEG_LIB)
    foreach(PKG ${FFMPEG_REQUIRED_PKGS})
        pkg_check_modules(${PKG} REQUIRED ${PKG})
        list(APPEND ALL_INCLUDE_DIRS ${${PKG}_INCLUDE_DIRS})
        list(APPEND FFMPEG_LIB ${${PKG}_LIBRARIES})
    endforeach()
    list(APPEND ALL_LIBRARIES ${FFMPEG_LIB})

    # OpenCV
    if (USE_SYSTEM_OPENCV)
        find_package(OpenCV REQUIRED)
        list(APPEND ALL_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS}/opencv2)
        # list(APPEND ALL_LIBRARIES opencv_core opencv_videoio)
    else()
        option(BUILD_opencv_calib3d "" OFF)
        option(BUILD_opencv_core "" ON)
        option(BUILD_opencv_dnn "" OFF)
        option(BUILD_opencv_features2d "" OFF)
        option(BUILD_opencv_flann "" OFF)
        option(BUILD_opencv_gapi "" OFF)
        option(BUILD_opencv_highgui "" OFF)
        option(BUILD_opencv_imgcodecs "" OFF)
        option(BUILD_opencv_imgproc "" OFF)
        option(BUILD_opencv_java "" OFF)
        option(BUILD_opencv_js "" OFF)
        option(BUILD_opencv_ml "" OFF)
        option(BUILD_opencv_objc "" OFF)
        option(BUILD_opencv_objdetect "" OFF)
        option(BUILD_opencv_photo "" OFF)
        option(BUILD_opencv_python "" OFF)
        option(BUILD_opencv_python2 "" OFF)
        option(BUILD_opencv_python3 "" OFF)
        option(BUILD_opencv_stitching "" OFF)
        option(BUILD_opencv_ts "" OFF)
        option(BUILD_opencv_video "" OFF)
        option(BUILD_opencv_videoio "" ON)
        option(BUILD_opencv_world "" OFF)

        option(WITH_PNG "" OFF)
        option(WITH_JPEG "" OFF)
        option(WITH_TIFF "" OFF)
        option(WITH_WEBP "" OFF)
        option(WITH_OPENJPEG "" OFF)
        option(WITH_JASPER "" OFF)
        option(WITH_OPENEXR "" OFF)
        option(WITH_IMGCODEC "" OFF)

        add_subdirectory(third_party/opencv)
        list(APPEND ALL_INCLUDE_DIRS
            ${PROJECT_SOURCE_DIR}/third_party/opencv/include
            ${PROJECT_SOURCE_DIR}/third_party/opencv/modules/core/include
            ${PROJECT_SOURCE_DIR}/third_party/opencv/modules/videoio/include
        )
        # list(APPEND ALL_LIBRARIES opencv_core opencv_videoio)
    endif() # USE_SYSTEM_OPENCV
endif() # WIN32

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

# spdlog
if (USE_SYSTEM_SPDLOG)
    find_package(spdlog REQUIRED)
    list(APPEND ALL_INCLUDE_DIRS ${spdlog_INCLUDE_DIRS})
    set(SPDLOG_LIB spdlog::spdlog)
else()
    add_subdirectory(third_party/spdlog)
    set(SPDLOG_LIB spdlog::spdlog_header_only)
endif()
list(APPEND ALL_LIBRARIES ${SPDLOG_LIB})

# Boost
if (USE_SYSTEM_BOOST)
    find_package(Boost REQUIRED COMPONENTS program_options)
    list(APPEND ALL_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
else()
    option(Boost_USE_STATIC_LIBS "" ON)
    option(Boost_USE_STATIC_RUNTIME "" ON)
    option(Boost_COMPONENTS "program_options")

    add_subdirectory(third_party/boost)
    include_directories(${PROJECT_SOURCE_DIR}/third_party/boost/libs/program_options/include)
    set(BOOST_BASE_PATH ${CMAKE_BINARY_DIR}/third_party/boost/libs/program_options/${CMAKE_BUILD_TYPE})
endif()
set(BOOST_LIB Boost::program_options)

if (BUILD_VIDEO2X_CLI)
    find_package(Vulkan REQUIRED)
    set(VULKAN_LIB Vulkan::Vulkan)
endif()

# Include ExternalProject module
include(ExternalProject)

# Add librealesrgan-ncnn-vulkan as an external project
ExternalProject_Add(
    realesrgan
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/librealesrgan_ncnn_vulkan/src
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/realesrgan_install
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DUSE_SYSTEM_NCNN=${USE_SYSTEM_NCNN}
    BUILD_ALWAYS ON
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config ${CMAKE_BUILD_TYPE}
)

# Remove duplicate entries
list(REMOVE_DUPLICATES ALL_INCLUDE_DIRS)
list(REMOVE_DUPLICATES ALL_LIBRARIES)

# Create the shared library 'libvideo2x'
file(GLOB LIBVIDEO2X_SOURCES src/*.cpp)
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
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/libvideo2x
    ${PROJECT_SOURCE_DIR}/third_party/librealesrgan_ncnn_vulkan/src
)

# Compile options for the shared library
target_compile_options(libvideo2x PRIVATE
    -fPIC
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Debug>:-g -DDEBUG>
)

# Define the path to the built libresrgan-ncnn-vulkan library
if(WIN32)
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan_install/lib/librealesrgan-ncnn-vulkan.lib)
    list(APPEND ALL_LIBRARIES ${REALESRGAN_LIB})
else()
    set(REALESRGAN_LIB ${CMAKE_BINARY_DIR}/realesrgan_install/lib/librealesrgan-ncnn-vulkan.so)
    list(APPEND ALL_LIBRARIES ${REALESRGAN_LIB})
endif()

# Link the shared library with the dependencies
target_link_libraries(libvideo2x PRIVATE ${ALL_LIBRARIES})

if(NOT WIN32)
    if (USE_SYSTEM_NCNN)
        target_link_libraries(libvideo2x PUBLIC ncnn)
    else()
        target_link_libraries(libvideo2x PRIVATE ncnn)
    endif()
endif()

# Create the executable 'video2x'
if (BUILD_VIDEO2X_CLI)
    file(GLOB VIDEO2X_SOURCES tools/video2x/src/*.cpp)
    add_executable(video2x ${VIDEO2X_SOURCES})
    set_target_properties(video2x PROPERTIES OUTPUT_NAME video2x)

    # Include directories for the executable
    target_include_directories(video2x PRIVATE
        ${ALL_INCLUDE_DIRS}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/libvideo2x
        ${PROJECT_SOURCE_DIR}/tools/video2x/include
    )

    # Compile options for the executable
    target_compile_options(video2x PRIVATE $<$<CONFIG:Debug>:-g -DDEBUG>)

    # Link the executable with the shared library
    target_link_libraries(video2x PRIVATE
        libvideo2x
        ${FFMPEG_LIB}
        ${SPDLOG_LIB}
        ${BOOST_LIB}
        ${VULKAN_LIB}
    )
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

# Explicitly set the output paths since OpenCV changes these variables
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

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

# Install the executable if BUILD_VIDEO2X_CLI is enabled
if(BUILD_VIDEO2X_CLI)
    install(TARGETS video2x RUNTIME DESTINATION ${INSTALL_BIN_DESTINATION})
endif()

# Install the header files
install(FILES
    ${PROJECT_SOURCE_DIR}/include/libvideo2x/libvideo2x.h
    ${PROJECT_SOURCE_DIR}/include/libvideo2x/char_defs.h
    ${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h
    DESTINATION ${INSTALL_INCLUDE_DESTINATION}
)

# Platform-specific installation rules
if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(BOOST_DLL_PATH ${BOOST_BASE_PATH}/boost_program_options-vc143-mt-x64-1_86.dll)
    else()
        set(BOOST_DLL_PATH ${BOOST_BASE_PATH}/boost_program_options-vc143-mt-gd-x64-1_86.dll)
    endif()

    # Find all FFmpeg DLLs
    file(GLOB FFMPEG_DLLS "${FFMPEG_BASE_PATH}/bin/*.dll")

    install(FILES
        ${CMAKE_BINARY_DIR}/realesrgan_install/bin/librealesrgan-ncnn-vulkan.dll
        ${FFMPEG_DLLS}
        # ${OPENCV_BASE_PATH}/build/x64/vc16/bin/opencv_world4100.dll
        # ${OPENCV_BASE_PATH}/build/x64/vc16/bin/opencv_videoio_msmf4100_64.dll
        ${NCNN_BASE_PATH}/bin/ncnn.dll
        ${BOOST_DLL_PATH}
        DESTINATION ${INSTALL_BIN_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
else()
    install(FILES ${REALESRGAN_LIB}
        DESTINATION ${INSTALL_LIB_DESTINATION}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
endif()
