cmake_minimum_required(VERSION 3.10)

project(detect_hw_usage LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create include directory for headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add GPU vendor specific libraries
if(UNIX)
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

# Add source files
set(LIB_SOURCES
    src/gpu_detector.cpp
    src/nvidia_gpu_detector.cpp
    src/amd_gpu_detector.cpp
    src/ram_detector.cpp
    src/storage_detector.cpp
    src/network_detector.cpp
    src/cpu_detector.cpp
)

# Create the library
add_library(${PROJECT_NAME} STATIC ${LIB_SOURCES})

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
        /usr/local/cuda/include
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

target_include_directories(${PROJECT_NAME} PRIVATE include)

# Find required packages
find_package(Threads REQUIRED)

# Link against required libraries
target_link_libraries(${PROJECT_NAME} 
    PUBLIC
        Threads::Threads
        dl
)

# Create the test executable
add_executable(test_program main.cpp)

# Link the test program with our library
target_link_libraries(test_program 
    PRIVATE 
        ${PROJECT_NAME}
)

# Add compile definitions if needed
if(DEFINED HAS_NVML_SUPPORT)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_NVML_SUPPORT)
endif()

if(DEFINED HAS_ADL_SUPPORT)
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAS_ADL_SUPPORT)
endif()

# Add include directories
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

