#pragma once

#include <string>
#include <memory>
#include <map>
#include <dlfcn.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>

// NVIDIA placeholder types that bypasses the need to include the nvml header
// By copying the exact shapes of NVIDIA's data structures here the app can compile 
// on any machine without needing the official NVIDIA toolkit installed
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef enum nvmlReturn_enum {NVML_SUCCESS = 0} nvmlReturn_t;
typedef enum nvmlClockType_enum {NVML_CLOCK_GRAPHICS = 0} nvmlClockType_t;
typedef enum nvmlTemperatureSensors_enum {NVML_TEMPERATURE_GPU = 0} nvmlTemperatureSensors_t;
typedef enum nvmlPcieUtilCounter_enum {NVML_PCIE_UTIL_TX_BYTES = 0 , NVML_PCIE_UTIL_RX_BYTES = 1}  nvmlPcieUtilCounter_t;

struct nvmlMemory_t {unsigned long long total; unsigned long long free; unsigned long long used;};
struct nvmlUtilization_t {unsigned int gpu; unsigned int memory;};

class MetricCollector {
    public:
        // Pass the Prometheus registry by pointer to safely link data blocks
        explicit MetricCollector(std::shared_ptr<prometheus::Registry> registry);
        ~MetricCollector();

        // The core execution loop that queries NVML and upadates the Prometheus metrics
        void update_metrics();

    private:
        // Dynamic library loading variables
        void* nvml_lib_handle_{nullptr};
        bool load_nvml_library();

        // Explicit function pointer variables
        nvmlReturn_t (*nvmlInit_)(void){nullptr};
        nvmlReturn_t (*nvmlShutdown_)(void){nullptr};
        const char* (*nvmlErrorString_)(nvmlReturn_t){nullptr};
        nvmlReturn_t (*nvmlDeviceGetCount_)(unsigned int*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetHandleByIndex_)(unsigned int, nvmlDevice_t*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetName_)(nvmlDevice_t, char*, unsigned int){nullptr};
        nvmlReturn_t (*nvmlDeviceGetMemoryInfo_)(nvmlDevice_t, nvmlMemory_t*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetTemperature_)(nvmlDevice_t, 
                                        nvmlTemperatureSensors_t, unsigned int*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetClockInfo_)(nvmlDevice_t, 
                                        nvmlClockType_t, unsigned int*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetPcieThroughput_)(nvmlDevice_t, 
                                        nvmlPcieUtilCounter_t, unsigned int*){nullptr};
        nvmlReturn_t (*nvmlDeviceGetUtilizationRates_)(nvmlDevice_t, 
                                        nvmlUtilization_t*){nullptr};

        unsigned int device_count_{0};
        bool nvml_initialized_{false};
        
        // Prometheus Metric Families
        // These manage the groups of charts that share the same metric name but have
        // different labels
        prometheus::Family<prometheus::Gauge>& gpu_util_family_;
        prometheus::Family<prometheus::Gauge>& mem_util_family_;
        prometheus::Family<prometheus::Gauge>& fb_used_family_;
        prometheus::Family<prometheus::Gauge>& gpu_temp_family_;
        prometheus::Family<prometheus::Gauge>& gpu_clock_family_;
        prometheus::Family<prometheus::Gauge>& pcie_tx_family_;
        prometheus::Family<prometheus::Gauge>& pcie_rx_family_;

        // Metric Storage Index
        // Holds the memory pointer for each discovered GPU to prevent slow runtime allocations
        struct DeviceMetrics {
            prometheus::Gauge* gpu_util{nullptr};
            prometheus::Gauge* mem_util{nullptr};
            prometheus::Gauge* fb_used{nullptr};
            prometheus::Gauge* gpu_temp{nullptr};
            prometheus::Gauge* gpu_clock{nullptr};
            prometheus::Gauge* pcie_tx{nullptr};
            prometheus::Gauge* pcie_rx{nullptr};
        };

        // Maps each GPU's numeric index (0, 1,2) to its respective tracking metrics
        std::map<unsigned int, DeviceMetrics> metric_map_;

        // System initialization sub-routines
        void initialize_nvml();
        void register_devices();
};