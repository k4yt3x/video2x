cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(video2x VERSION 6.3.1 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(ExternalProject)
include(GNUInstallDirs)

# The FindBoost module is removed in CMake 3.30
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Build options
option(BUILD_SHARED_LIBS "Build libvideo2x as a shared library" ON)
option(VIDEO2X_BUILD_CLI "Build the video2x command line interface executable" ON)

option(VIDEO2X_USE_EXTERNAL_NCNN "Use the system-provided ncnn library" ON)
option(VIDEO2X_USE_EXTERNAL_SPDLOG "Use the system-provided spdlog library" ON)
option(VIDEO2X_USE_EXTERNAL_BOOST "Use the system-provided Boost library" ON)

option(VIDEO2X_ENABLE_NATIVE "Enable optimizations for the native architecture" OFF)
option(VIDEO2X_ENABLE_X86_64_V4 "Enable x86-64-v4 (AVX-512) optimizations" OFF)
option(VIDEO2X_ENABLE_X86_64_V3 "Enable x86-64-v3 (AVX2) optimizations" OFF)

# Enable extra compiler warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Set the default optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
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
        add_compile_options(/arch:AVX512)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=x86-64-v4)
    endif()
elseif(VIDEO2X_ENABLE_X86_64_V3)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/arch:AVX2)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-march=x86-64-v3)
    endif()
endif()

# Generate the version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h"
    @ONLY
)

# Add the libvideo2x shared library
add_library(libvideo2x
    src/avutils.cpp
    src/conversions.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/filter_libplacebo.cpp
    src/filter_realcugan.cpp
    src/filter_realesrgan.cpp
    src/fsutils.cpp
    src/interpolator_rife.cpp
    src/libplacebo.cpp
    src/libvideo2x.cpp
    src/logger_manager.cpp
    src/processor_factory.cpp
)

# Set the C++ standard to C++17
target_compile_features(libvideo2x PRIVATE cxx_std_17)

# Set the shared library output name and disable C++ extensions
set_target_properties(libvideo2x PROPERTIES
    PREFIX lib
    OUTPUT_NAME video2x
    CXX_EXTENSIONS OFF
)

# Include directories for the shared library
target_include_directories(libvideo2x PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/libvideo2x"
    "${PROJECT_SOURCE_DIR}/third_party/librealesrgan_ncnn_vulkan/src"
    "${PROJECT_SOURCE_DIR}/third_party/librealcugan_ncnn_vulkan/src"
    "${PROJECT_SOURCE_DIR}/third_party/librife_ncnn_vulkan/src"
)

# Add the export definition for the exported symbols
target_compile_definitions(libvideo2x PRIVATE LIBVIDEO2X_EXPORTS)

# Set debug compile options
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(libvideo2x PRIVATE $<$<CONFIG:Debug>:/Zi /Od /MDd>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libvideo2x PRIVATE -fPIC $<$<CONFIG:Debug>:-g -O0>)
endif()

# FFmpeg
if(WIN32)
    target_include_directories(libvideo2x SYSTEM PRIVATE
        "${PROJECT_SOURCE_DIR}/third_party/ffmpeg-shared/include"
    )
    target_link_libraries(libvideo2x PRIVATE
        "${ffmpeg_base_path}/lib/avcodec.lib"
        "${ffmpeg_base_path}/lib/avfilter.lib"
        "${ffmpeg_base_path}/lib/avformat.lib"
        "${ffmpeg_base_path}/lib/avutil.lib"
        "${ffmpeg_base_path}/lib/swscale.lib"
    )
else()
    # Use pkg-config to find FFmpeg libraries
    find_package(PkgConfig REQUIRED)

    # Find and configure FFmpeg libraries
    pkg_check_modules(libavcodec REQUIRED libavcodec)
    pkg_check_modules(libavfilter REQUIRED libavfilter)
    pkg_check_modules(libavformat REQUIRED libavformat)
    pkg_check_modules(libavutil REQUIRED libavutil)
    pkg_check_modules(libswscale REQUIRED libswscale)

    # Apply include directories and libraries directly to the target
    target_include_directories(libvideo2x SYSTEM PRIVATE
        ${libavcodec_INCLUDE_DIRS}
        ${libavfilter_LIBRARIES}
        ${libavformat_INCLUDE_DIRS}
        ${libavutil_INCLUDE_DIRS}
        ${libswscale_INCLUDE_DIRS}
    )
    target_link_libraries(libvideo2x PRIVATE
        ${libavcodec_LIBRARIES}
        ${libavfilter_LIBRARIES}
        ${libavformat_LIBRARIES}
        ${libavutil_LIBRARIES}
        ${libswscale_LIBRARIES}
    )
endif()

# ncnn
if(VIDEO2X_USE_EXTERNAL_NCNN)
    find_package(ncnn REQUIRED)
    target_link_libraries(libvideo2x PRIVATE ncnn)
else()
    if(WIN32)
        # On Windows, use the pre-built shared ncnn library
        set(ncnn_base_path "${PROJECT_SOURCE_DIR}/third_party/ncnn-shared/x64")
        set(spirv_build_path
            "${CMAKE_BINARY_DIR}/realesrgan-prefix/src/realesrgan-build/ncnn/glslang/SPIRV"
        )

        # Link ncnn and SPIRV libraries
        target_include_directories(libvideo2x SYSTEM PRIVATE "${ncnn_base_path}/include/ncnn")
        target_link_libraries(libvideo2x PRIVATE "${ncnn_base_path}/lib/ncnn.lib")

        # SPIRV needs to be linked explicitly
        if(CMAKE_BUILD_TYPE STREQUAL Release)
            target_link_libraries(libvideo2x PRIVATE "${spirv_build_path}/Release/SPIRV.lib")
        else()
            target_link_libraries(libvideo2x PRIVATE "${spirv_build_path}/Debug/SPIRVd.lib")
        endif()
    else()
        option(NCNN_INSTALL_SDK "" ON)
        option(SKIP_GLSLANG_INSTALL "" OFF)

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
        target_link_libraries(libvideo2x PRIVATE ncnn)
    endif()
endif()

# spdlog
if(VIDEO2X_USE_EXTERNAL_SPDLOG)
    find_package(spdlog REQUIRED)
    target_include_directories(libvideo2x SYSTEM PRIVATE ${spdlog_INCLUDE_DIRS})
    target_link_libraries(libvideo2x PRIVATE spdlog::spdlog)
else()
    # spdlog exceptions are incompatible with ncnn
    option(SPDLOG_NO_EXCEPTIONS "" ON)
    option(SPDLOG_INSTALL "" ON)
    add_subdirectory(third_party/spdlog)
    target_link_libraries(libvideo2x PRIVATE spdlog::spdlog_header_only)
endif()

# Real-ESRGAN, Real-CUGAN, and RIFE
option(USE_SYSTEM_NCNN "" ${VIDEO2X_USE_EXTERNAL_NCNN})
add_subdirectory(third_party/librealesrgan_ncnn_vulkan/src)
add_subdirectory(third_party/librealcugan_ncnn_vulkan/src)
add_subdirectory(third_party/librife_ncnn_vulkan/src)

# Determine the library suffix based on the platform
if(WIN32)
    set(suffix ${CMAKE_STATIC_LIBRARY_SUFFIX})
else()
    set(suffix ${CMAKE_SHARED_LIBRARY_SUFFIX})
endif()

# Link the shared library with the ncnn-Vulkan-based libraries
set(ncnn_vulkan_libs)
foreach(lib IN ITEMS realesrgan realcugan rife)
    list(APPEND ncnn_vulkan_libs
        "${CMAKE_BINARY_DIR}/third_party/lib${lib}_ncnn_vulkan/src/lib${lib}-ncnn-vulkan${suffix}"
    )
endforeach()
target_link_libraries(libvideo2x PRIVATE ${ncnn_vulkan_libs})

# Determine the installation directories
if(WIN32)
    set(install_bin_destination .)
    set(install_include_destination include/libvideo2x)
    set(install_lib_destination .)
    set(install_model_destination .)
    set(install_cmake_destination cmake/Video2X)
else()
    set(install_bin_destination "${CMAKE_INSTALL_BINDIR}")
    set(install_include_destination "${CMAKE_INSTALL_INCLUDEDIR}/libvideo2x")
    set(install_lib_destination "${CMAKE_INSTALL_LIBDIR}")
    set(install_model_destination "${CMAKE_INSTALL_DATADIR}/video2x")
    set(install_cmake_destination "${CMAKE_INSTALL_LIBDIR}/cmake/Video2X")
endif()

# Common installation rules for libvideo2x and models
install(TARGETS libvideo2x
    EXPORT Video2XTargets
    RUNTIME DESTINATION "${install_bin_destination}"
    LIBRARY DESTINATION "${install_lib_destination}"
    ARCHIVE DESTINATION "${install_lib_destination}"
    INCLUDES DESTINATION "${install_include_destination}"
)

# Install the header files from the include directory
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/libvideo2x/"
    DESTINATION "${install_include_destination}"
    FILES_MATCHING PATTERN "*.h"
)

# Install the generated version.h file
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libvideo2x/version.h"
    DESTINATION "${install_include_destination}"
)

# Export targets and create CMake package config
install(EXPORT Video2XTargets
    FILE Video2XTargets.cmake
    NAMESPACE Video2X::
    DESTINATION "${install_cmake_destination}"
)

# Generate the CMake package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Video2XConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/Video2XConfig.cmake"
    INSTALL_DESTINATION "${install_cmake_destination}"
)

# Install the CMake package config file
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/Video2XConfig.cmake"
    DESTINATION "${install_cmake_destination}"
)

# Install model files
install(DIRECTORY "${CMAKE_SOURCE_DIR}/models" DESTINATION "${install_model_destination}")

# Platform-specific installation rules
if(WIN32)
    file(GLOB ffmpeg_dlls "${ffmpeg_base_path}/bin/*.dll")
    install(FILES
        ${ffmpeg_dlls}
        "${ncnn_base_path}/bin/ncnn.dll"
        ${ncnn_vulkan_libs}
        DESTINATION "${install_bin_destination}"
    )
else()
    install(FILES
        ${ncnn_vulkan_libs}
        DESTINATION "${install_lib_destination}"
    )
endif()

# Build the video2x CLI executable
if(VIDEO2X_BUILD_CLI)
    add_executable(video2x
        tools/video2x/src/argparse.cpp
        tools/video2x/src/newline_safe_sink.cpp
        tools/video2x/src/timer.cpp
        tools/video2x/src/validators.cpp
        tools/video2x/src/video2x.cpp
        tools/video2x/src/vulkan_utils.cpp
    )

    # Set the C++ standard to C++17
    target_compile_features(video2x PRIVATE cxx_std_17)

    # Set the shared library output name and disable C++ extensions
    set_target_properties(video2x PROPERTIES
        OUTPUT_NAME video2x
        CXX_EXTENSIONS OFF
    )

    # Include directories for the executable
    target_include_directories(video2x PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/tools/video2x/include"
    )

    # Link the executable with the shared library
    target_link_libraries(video2x PRIVATE libvideo2x)

    # Set debug compile options
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(video2x PRIVATE $<$<CONFIG:Debug>:/Zi /Od /MDd>)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(video2x PRIVATE -fPIC $<$<CONFIG:Debug>:-g -O0>)
    endif()

    # FFmpeg
    if(WIN32)
        target_include_directories(video2x SYSTEM PRIVATE
            "${PROJECT_SOURCE_DIR}/third_party/ffmpeg-shared/include"
        )
        target_link_libraries(video2x PRIVATE
            "${ffmpeg_base_path}/lib/avcodec.lib"
            "${ffmpeg_base_path}/lib/avutil.lib"
        )
    else()
        # FFmpeg libraries have already been found
        # Apply include directories and libraries directly to the target
        target_include_directories(video2x SYSTEM PRIVATE
            ${libavcodec_INCLUDE_DIRS}
            ${libavutil_INCLUDE_DIRS}
        )
        target_link_libraries(video2x PRIVATE ${libavcodec_LIBRARIES} ${libavutil_LIBRARIES})
    endif()

    # spdlog
    # spdlog targets has already been added
    if(VIDEO2X_USE_EXTERNAL_SPDLOG)
        target_include_directories(video2x SYSTEM PRIVATE ${spdlog_INCLUDE_DIRS})
        target_link_libraries(video2x PRIVATE spdlog::spdlog)
    else()
        target_link_libraries(video2x PRIVATE spdlog::spdlog_header_only)
    endif()

    # Vulkan
    find_package(Vulkan REQUIRED)
    target_link_libraries(video2x PRIVATE Vulkan::Vulkan)

    # Boost
    if(VIDEO2X_USE_EXTERNAL_BOOST)
        find_package(Boost REQUIRED COMPONENTS program_options)
    else()
        option(Boost_USE_STATIC_LIBS "" ON)
        option(Boost_USE_STATIC_RUNTIME "" ON)
        option(Boost_COMPONENTS program_options)

        add_subdirectory(third_party/boost)
        target_include_directories(video2x SYSTEM PRIVATE
            ${PROJECT_SOURCE_DIR}/third_party/boost/libs/program_options/include
        )

        # Suppress the -Wsign-conversion warnings for Boost.Nowide
        if (TARGET boost_nowide)
            if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                target_compile_options(boost_nowide PRIVATE -Wno-error=sign-conversion)
            endif()
        endif()
    endif()
    target_link_libraries(video2x PRIVATE Boost::program_options)

    # Install the video2x executable
    install(TARGETS video2x RUNTIME DESTINATION "${install_bin_destination}")

    # Install the Boost DLL
    if(WIN32 AND NOT VIDEO2X_USE_EXTERNAL_BOOST)
        set(boost_base_path
            "${CMAKE_BINARY_DIR}/third_party/boost/libs/program_options/${CMAKE_BUILD_TYPE}"
        )

        # Different build types have different DLL names
        if(CMAKE_BUILD_TYPE STREQUAL Release)
            set(boost_dll_path "${boost_base_path}/boost_program_options-vc143-mt-x64-1_86.dll")
        else()
            set(boost_dll_path "${boost_base_path}/boost_program_options-vc143-mt-gd-x64-1_86.dll")
        endif()

        install(FILES "${boost_dll_path}" DESTINATION "${install_bin_destination}")
    endif()
endif()
