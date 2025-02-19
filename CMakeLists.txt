cmake_minimum_required(VERSION 3.21)

project(detect_hw_usage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create include directory for headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add GPU vendor specific libraries
if(WIN32)
    # NVIDIA NVML
    find_library(NVML_LIBRARY nvml
        PATHS "$ENV{ProgramFiles}/NVIDIA Corporation/NVSMI"
        PATH_SUFFIXES "lib" "lib64")
    
    # AMD ADL
    find_path(ADL_INCLUDE_DIR ADL/adl_sdk.h
        PATHS "$ENV{ProgramFiles}/AMD/ADL_SDK")
    find_library(ADL_LIBRARY ADL_SDK
        PATHS "$ENV{ProgramFiles}/AMD/ADL_SDK")

    # Add NvAPI for GeForce cards
    find_path(NVAPI_INCLUDE_DIR nvapi.h
        PATHS "${PROJECT_SOURCE_DIR}/external/nvapi")
    find_library(NVAPI_LIBRARY nvapi64
        PATHS "${PROJECT_SOURCE_DIR}/external/nvapi")
else()
    # Linux paths
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    
    # NVIDIA Management Library
    find_library(NVML_LIBRARY nvidia-ml
        PATHS "/usr/lib" "/usr/lib64" "/usr/lib/x86_64-linux-gnu")
    
    if(NOT NVML_LIBRARY)
        message(STATUS "NVML library not found, trying alternative path")
        find_library(NVML_LIBRARY libnvidia-ml.so.1
            PATHS "/usr/lib" "/usr/lib64" "/usr/lib/x86_64-linux-gnu")
    endif()
endif()

# Make AMD ADL optional
if(ADL_LIBRARY)
    add_definitions(-DHAS_ADL_SUPPORT)
    message(STATUS "AMD Display Library found: ${ADL_LIBRARY}")
else()
    message(STATUS "AMD Display Library not found, AMD GPU support will be disabled")
    set(ADL_LIBRARY "")
endif()

# Make NVIDIA optional
if(NVML_LIBRARY)
    add_definitions(-DHAS_NVML_SUPPORT)
    message(STATUS "NVIDIA Management Library found: ${NVML_LIBRARY}")
else()
    message(STATUS "NVIDIA Management Library not found, NVIDIA GPU support will be disabled")
    set(NVML_LIBRARY "")
endif()

add_library(${PROJECT_NAME}
    include/gpu_detector.hpp
    include/nvidia_gpu_detector.hpp
    include/amd_gpu_detector.hpp
    include/ram_detector.hpp
    src/gpu_detector.cpp
    src/nvidia_gpu_detector.cpp
    src/amd_gpu_detector.cpp
    src/ram_detector.cpp
)

# Collect all libraries
set(GPU_LIBS "")
if(NVML_LIBRARY)
    list(APPEND GPU_LIBS ${NVML_LIBRARY})
endif()
if(ADL_LIBRARY)
    list(APPEND GPU_LIBS ${ADL_LIBRARY})
endif()
if(WIN32 AND NVAPI_LIBRARY)
    list(APPEND GPU_LIBS ${NVAPI_LIBRARY})
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${GPU_LIBS}
    $<$<PLATFORM_ID:Linux>:${X11_LIBRARIES}>
    $<$<PLATFORM_ID:Linux>:${OPENGL_LIBRARIES}>
)

# Include directories
if(UNIX)
    target_include_directories(${PROJECT_NAME} PRIVATE
        /usr/include/nvidia-current
        /usr/local/cuda/include  # Or set CUDA include path which contains nvml.h
        ${X11_INCLUDE_DIR}
        ${OPENGL_INCLUDE_DIR}
    )
    
    # Add AMD GPU SDK path if found
    find_path(ADL_SDK_DIR 
        NAMES include/adl_sdk.h
        PATHS "/opt/amdgpu-pro"
        DOC "AMD Display Library SDK root directory")
        
    if(ADL_SDK_DIR)
        target_include_directories(${PROJECT_NAME} PRIVATE 
            ${ADL_SDK_DIR}/include
        )
    endif()
endif()

if(WIN32 AND NVAPI_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${NVAPI_INCLUDE_DIR})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE include)

add_executable(test_program
                main.cpp)

target_link_libraries(test_program
    ${PROJECT_NAME}
)

