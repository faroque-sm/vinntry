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

# Handle Header Include Paths
include_directories(include)

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
    ${CMAKE_DL_LIBS}     # Activates <dlfcn.h> engine capabilities
    Threads::Threads     # Standard system agnostic multi-threading abstraction
)

# Print out verified target locations during the project generation phase
message(STATUS "--------------------------------------------------------")
message(STATUS "VINNTRY DYNAMIC TELEMETRY DAEMON CONFIGURATION:")
message(STATUS "Conda Directory Prefix:   $ENV{CONDA_PREFIX}")
message(STATUS "Dynamic Loading Interface: Natively Armed via libdl")
message(STATUS "--------------------------------------------------------")