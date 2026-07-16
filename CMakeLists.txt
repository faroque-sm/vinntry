cmake_minimum_required(VERSION 3.22)
project(vinntry_daemon CXX)

# Force standard C++17 optimization rules
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compilation warnings to keep hardware interface logic pristine
add_compile_options(-Wall -Wextra -Wpedantic -O3)

# 1. Locate the Prometheus-CPP library inside Mamba environment
find_package(prometheus-cpp CONFIG REQUIRED)

# 2. Locate the Native Host NVIDIA NVML Driver Library
# This searches standard Linux runtime engine paths for libnvidia-ml.so
find_library(NVML_LIBRARY 
    NAMES nvidia-ml libnvidia-ml.so.1 libnvidia-ml.so
    PATHS 
        /usr/lib/x86_64-linux-gnu
        /usr/lib64
        /usr/local/cuda/lib64
        /usr/lib/wsl/lib         # wsl2 driver layer mapping
    REQUIRED
)

# 3. Handle Header Include Paths
include_directories(
    include
    ${CONDA_PREFIX}/include
)

# 4. Declare the Compilation Target
add_executable(vinntry_daemon
    src/main.cpp
    src/metric_collector.cpp
)

# 5. Link Shared Library Dependencies
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
message(STATUS "Conda Directory Prefix:   ${CONDA_PREFIX}")
message(STATUS "--------------------------------------------------------")