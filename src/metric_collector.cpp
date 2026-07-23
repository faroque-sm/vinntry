#include "metric_collector.hpp"
#include <nvml.h>
#include <iostream>
#include <stdexcept>


MetricCollector::MetricCollector(std::shared_ptr<prometheus::Registry> registry)
    : gpu_util_family_(prometheus::BuildGauge()
        .Name("nvml_gpu_util_percent")
        .Help("GPU Compute Utilization Percentage")
        .Register(*registry)),
      mem_util_family_(prometheus::BuildGauge()
        .Name("nvml_mem_util_percent")
        .Help("GPU Memory Utilization Percentage")
        .Register(*registry)),
      fb_used_family_(prometheus::BuildGauge()
        .Name("nvml_fb_mem_bytes")
        .Help("FrameBuffer Memory Used Bytes")
        .Register(*registry)),
      gpu_temp_family_(prometheus::BuildGauge()
        .Name("nvml_gpu_temp_celsius")
        .Help("GPU Core Temperature Celsius")
        .Register(*registry)),
      gpu_clock_family_(prometheus::BuildGauge()
        .Name("nvml_gpu_clock_mhz")
        .Help("GPU Core Graphics Clock Megahertz")
        .Register(*registry)),      
      pcie_tx_family_(prometheus::BuildGauge()
        .Name("nvml_pcie_tx_kbs")
        .Help("GPU PCIE Host-to-Device Transfer KiloBytesPerSecond")
        .Register(*registry)),
      pcie_rx_family_(prometheus::BuildGauge()
        .Name("nvml_pcie_rx_kbs")
        .Help("GPU PCIE Device-to-Host Transfer KiloBytesPerSecond")
        .Register(*registry)) {

    initialize_nvml();
    register_devices();
}

MetricCollector::~MetricCollector() {
    if (nvml_initialized_) {
        nvmlShutdown();
        std::cout << "[vinntry] NVML subsystems shut down cleanly." << std::endl;  
    }
}

void MetricCollector::update_metrics() {
    if (!nvml_initialized_) return;

    for (unsigned int i = 0; i < device_count_; ++i) {
        nvmlDevice_t handle;
        nvmlReturn_t result = nvmlDeviceGetHandleByIndex(i, &handle);
        if (result != NVML_SUCCESS) continue;

        nvmlUtilization_t utilization;
        result = nvmlDeviceGetUtilizationRates(handle, &utilization);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_util->Set(static_cast<double>(utilization.gpu));
            metric_map_[i].mem_util->Set(static_cast<double>(utilization.memory));
        }

        nvmlMemory_t memory_info;
        result = nvmlDeviceGetMemoryInfo(handle, &memory_info);
        if (result == NVML_SUCCESS) {
            metric_map_[i].fb_used->Set(static_cast<double>(memory_info.used));
        }

        unsigned int temp = 0;
        result = nvmlDeviceGetTemperature(handle, NVML_TEMPERATURE_GPU, &temp);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_temp->Set(static_cast<double>(temp));
        }

        uint32_t grapics_clock_mhz = 0;
        result = nvmlDeviceGetClockInfo(handle, NVML_CLOCK_GRAPHICS, &grapics_clock_mhz);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_clock->Set(static_cast<double>(grapics_clock_mhz));
        }

        unsigned int pcie_tx_kbs = 0;
        result = nvmlDeviceGetPcieThroughput(handle, NVML_PCIE_UTIL_TX_BYTES, &pcie_tx_kbs);
        if (result == NVML_SUCCESS) {
            metric_map_[i].pcie_tx->Set(static_cast<double>(pcie_tx_kbs));
        }

        unsigned int pcie_rx_kbs = 0;
        result = nvmlDeviceGetPcieThroughput(handle, NVML_PCIE_UTIL_RX_BYTES, &pcie_rx_kbs);
        if (result == NVML_SUCCESS) {
            metric_map_[i].pcie_rx->Set(static_cast<double>(pcie_rx_kbs));
        }
    }
}

void MetricCollector::initialize_nvml() {
    nvmlReturn_t result = nvmlInit_v2();
    if (result != NVML_SUCCESS) {
        std::cerr << "[vinntry] CRITICAL: Failed to initialize NVML: " 
                            << nvmlErrorString(result) << std::endl;
        return;
    }

    result = nvmlDeviceGetCount_v2(&device_count_);
    if (result != NVML_SUCCESS) {
        std::cerr << "[vinntry] Failed to fetch device count: " 
                            << nvmlErrorString(result) << std::endl;
        nvmlShutdown();
        return;
    }

    nvml_initialized_ = true;
    std::cout << "[vinntry] Native NVML link initialzied. Found "
                    << device_count_ << " GPU node(s)." << std::endl;
}

void MetricCollector::register_devices() {
    if (!nvml_initialized_) return;

    for (unsigned int i = 0; i < device_count_; ++i) {
        nvmlDevice_t handle;
        nvmlReturn_t result = nvmlDeviceGetHandleByIndex_v2(i, &handle);
        if (result != NVML_SUCCESS) continue;

        char name_buffer[64];
        result = nvmlDeviceGetName(handle, name_buffer, sizeof(name_buffer));
        std::string gpu_name = 
            (result == NVML_SUCCESS) ? std::string(name_buffer) : "Unknown NVIDIA GPU";
        
        std::map<std::string, std::string> labels = {
            {"device_index", std::to_string(i)},
            {"gpu_name", gpu_name}
        };

        DeviceMetrics metrics;
        metrics.gpu_util = &gpu_util_family_.Add(labels);
        metrics.mem_util = &mem_util_family_.Add(labels);
        metrics.fb_used = &fb_used_family_.Add(labels);
        metrics.gpu_temp = &gpu_temp_family_.Add(labels);
        metrics.gpu_clock = &gpu_clock_family_.Add(labels);
        metrics.pcie_tx = &pcie_tx_family_.Add(labels);
        metrics.pcie_rx = &pcie_rx_family_.Add(labels);
        
        metric_map_[i] = metrics;
        std::cout << "[vinntry] Indexed profiling target [" << i << "]: " << gpu_name << std::endl;
    }
}
