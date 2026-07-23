cmake_minimum_required(VERSION 3.22)
project(vinntry_daemon CXX)

# Force standard C++17 optimization rules
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compilation warnings to keep hardware interface logic pristine
add_compile_options(-O3)

# Locate the Prometheus-CPP library inside Mamba environment
find_package(prometheus-cpp CONFIG REQUIRED)

# Locate the Native Host NVIDIA NVML Driver Library
# This searches standard Linux runtime engine paths for libnvidia-ml.so
find_library(NVML_LIBRARY 
    NAMES nvidia-ml libnvidia-ml.so.1 libnvidia-ml.so
    REQUIRED
    PATHS 
        /usr/lib/wsl/lib               # wsl2 driver layer mapping
        /usr/lib/x86_64-linux-gnu
        /usr/lib64
        /usr/local/cuda/lib64
    NAMES_PER_DIR 
    NO_DEFAULT_PATH
)

# Handle Header Include Paths
include_directories(
    include
    $ENV{CONDA_PREFIX}/include
    $ENV{CONDA_PREFIX}/targets/x86_64-linux/include
)

# Declare the Compilation Target
add_executable(vinntry_daemon
    src/main.cpp
    src/metric_collector.cpp
)

# Link Shared Library Dependencies
target_link_libraries(vinntry_daemon
    PRIVATE
    prometheus-cpp::core
    prometheus-cpp::pull
    ${NVML_LIBRARY}
    pthread
)

# Print out verified target locations during the project generation phase
message(STATUS "--------------------------------------------------------")
message(STATUS "VINNTRY HARDWARE MATCH COMPILER VERIFICATION:")
message(STATUS "Found NVML Shared Driver: ${NVML_LIBRARY}")
message(STATUS "Conda Directory Prefix:   $ENV{CONDA_PREFIX}")
message(STATUS "--------------------------------------------------------")