#include "metric_collector.hpp"
#include <iostream>
#include <stdexcept>


MetricCollector::MetricCollector(std::shared_ptr<prometheus::Registry> registry)
    : gpu_util_family_(prometheus::BuildGauge()
        .Name("nvml_gpu_util_percent")
        .Help("GPU Compute Utilization Percentage")
        .Register(*registry)),
      nvdec_util_family_(prometheus::BuildGauge()
        .Name("nvml_nvdec_util_percent")
        .Help("GPU Hardware Video Decoder ASIC Utilization")
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
      gpu_power_family_(prometheus::BuildGauge()
        .Name("nvml_power_usage_mwatts")
        .Help("GPU Core Power Draw in Milliwatts")
        .Register(*registry)),
      gpu_power_limit_family_(prometheus::BuildGauge()
        .Name("nvml_power_limit_mwatts")
        .Help("GPU Enforced Hardware Power Limit in Milliwatts")
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
    // Shutdown NVML subsystem cleanly via the dynamic pointer
    if (nvml_initialized_ && nvmlShutdown_) {
        nvmlShutdown_();
        std::cout << "[vinntry] NVML subsystems shut down cleanly." << std::endl;  
    }

    // Completely unload the driver library binary layout from memory
    if (nvml_lib_handle_) {
        dlclose(nvml_lib_handle_);
        std::cout << "[vinntry] Dynamic driver handle unloaded from memory." << std::endl;
    }
}

bool MetricCollector::load_nvml_library() {
    // Cross-platform driver search paths
    const char* paths[] = {
        "/usr/lib/wsl/lib/libnvidia-ml.so.1", // wsl windows is checked first since this does 
                                             // not exist in linux
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1" // proxmox vm or linux        
    };

    for (const char* path : paths) {
        nvml_lib_handle_ = dlopen(path, RTLD_LAZY);
        if (nvml_lib_handle_) break;
    }

    if (!nvml_lib_handle_) {
        std::cerr << "[vinntry] CRITICAL: NVML Shared driver file not found." << std::endl;
        return false;
    }

    // Resolve pointer via explicit string mapping
    nvmlInit_ = (nvmlReturn_t (*)())dlsym(nvml_lib_handle_, "nvmlInit_v2");
    nvmlShutdown_ = (nvmlReturn_t (*)())dlsym(nvml_lib_handle_, "nvmlShutdown");
    nvmlErrorString_ = (const char* (*)(nvmlReturn_t))dlsym(nvml_lib_handle_, "nvmlErrorString");
    nvmlDeviceGetCount_ = (nvmlReturn_t (*)(unsigned int*))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetCount_v2");
    nvmlDeviceGetHandleByIndex_ = (nvmlReturn_t (*)(unsigned int, nvmlDevice_t*))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetHandleByIndex_v2");
    nvmlDeviceGetName_ = (nvmlReturn_t (*)(nvmlDevice_t, char*, unsigned int))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetName");
    nvmlDeviceGetMemoryInfo_ = (nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetMemoryInfo");
    nvmlDeviceGetTemperature_ = (nvmlReturn_t (*)(nvmlDevice_t, nvmlTemperatureSensors_t, 
                         unsigned int*))dlsym(nvml_lib_handle_, "nvmlDeviceGetTemperature");
    nvmlDeviceGetClockInfo_ = (nvmlReturn_t (*)(nvmlDevice_t, nvmlClockType_t, unsigned int*))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetClockInfo");
    nvmlDeviceGetPcieThroughput_ = (nvmlReturn_t (*)(nvmlDevice_t, nvmlPcieUtilCounter_t, 
                         unsigned int*))dlsym(nvml_lib_handle_, "nvmlDeviceGetPcieThroughput");
    nvmlDeviceGetUtilizationRates_ = (nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*))
                                        dlsym(nvml_lib_handle_, "nvmlDeviceGetUtilizationRates");

    nvmlDeviceGetDecoderUtilization_ = (nvmlReturn_t (*)(nvmlDevice_t, unsigned int*, unsigned int*))dlsym(nvml_lib_handle_, "nvmlDeviceGetDecoderUtilization");
    nvmlDeviceGetPowerUsage_ = (nvmlReturn_t (*)(nvmlDevice_t, unsigned int*))dlsym(nvml_lib_handle_, "nvmlDeviceGetPowerUsage");
    nvmlDeviceGetEnforcedPowerLimit_ = (nvmlReturn_t (*)(nvmlDevice_t, unsigned int*))dlsym(nvml_lib_handle_, "nvmlDeviceGetEnforcedPowerLimit");
       
    // Ensure every required function resolved cleanly
    return (nvmlInit_ && nvmlShutdown_ && nvmlErrorString_ && nvmlDeviceGetCount_ && 
            nvmlDeviceGetHandleByIndex_ && nvmlDeviceGetName_ && nvmlDeviceGetMemoryInfo_ &&
            nvmlDeviceGetTemperature_ && nvmlDeviceGetClockInfo_ && 
            nvmlDeviceGetPcieThroughput_ && nvmlDeviceGetUtilizationRates_ && 
            nvmlDeviceGetDecoderUtilization_ && nvmlDeviceGetPowerUsage_ && nvmlDeviceGetEnforcedPowerLimit_);
}

void MetricCollector::update_metrics() {
    if (!nvml_initialized_) return;

    for (unsigned int i = 0; i < device_count_; ++i) {
        nvmlDevice_t handle;
        nvmlReturn_t result = nvmlDeviceGetHandleByIndex_(i, &handle);
        if (result != NVML_SUCCESS) continue;

        nvmlUtilization_t utilization;
        result = nvmlDeviceGetUtilizationRates_(handle, &utilization);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_util->Set(static_cast<double>(utilization.gpu));
            metric_map_[i].mem_util->Set(static_cast<double>(utilization.memory));
        }

        unsigned int decoder_util = 0;
        unsigned int decoder_sampling_period = 0;
        result = nvmlDeviceGetDecoderUtilization_(handle, &decoder_util, &decoder_sampling_period);
        if (result == NVML_SUCCESS) {
            metric_map_[i].nvdec_util->Set(static_cast<double>(decoder_util));
        }

        nvmlMemory_t memory_info;
        result = nvmlDeviceGetMemoryInfo_(handle, &memory_info);
        if (result == NVML_SUCCESS) {
            metric_map_[i].fb_used->Set(static_cast<double>(memory_info.used));
        }

        unsigned int temp = 0;
        result = nvmlDeviceGetTemperature_(handle, NVML_TEMPERATURE_GPU, &temp);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_temp->Set(static_cast<double>(temp));
        }

        uint32_t grapics_clock_mhz = 0;
        result = nvmlDeviceGetClockInfo_(handle, NVML_CLOCK_GRAPHICS, &grapics_clock_mhz);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_clock->Set(static_cast<double>(grapics_clock_mhz));
        }

        unsigned int power_mw = 0;
        result = nvmlDeviceGetPowerUsage_(handle, &power_mw);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_power->Set(static_cast<double>(power_mw));
        }

        unsigned int power_limit_mw = 0;
        result = nvmlDeviceGetEnforcedPowerLimit_(handle, &power_limit_mw);
        if (result == NVML_SUCCESS) {
            metric_map_[i].gpu_power_limit->Set(static_cast<double>(power_limit_mw));
        }

        unsigned int pcie_tx_kbs = 0;
        result = nvmlDeviceGetPcieThroughput_(handle, NVML_PCIE_UTIL_TX_BYTES, &pcie_tx_kbs);
        if (result == NVML_SUCCESS) {
            metric_map_[i].pcie_tx->Set(static_cast<double>(pcie_tx_kbs));
        }

        unsigned int pcie_rx_kbs = 0;
        result = nvmlDeviceGetPcieThroughput_(handle, NVML_PCIE_UTIL_RX_BYTES, &pcie_rx_kbs);
        if (result == NVML_SUCCESS) {
            metric_map_[i].pcie_rx->Set(static_cast<double>(pcie_rx_kbs));
        }
    }
}

void MetricCollector::initialize_nvml() {
    // Resolve and wire the dynamic function pointer bindings first
    if (!load_nvml_library()) {
        std::cerr << "[vinntry] CRITICAL: Dynamic driver mapping failed." << std::endl;
        return;
    }

    nvmlReturn_t result = nvmlInit_();
    if (result != NVML_SUCCESS) {
        std::cerr << "[vinntry] CRITICAL: Failed to initialize NVML: " 
                            << nvmlErrorString_(result) << std::endl;
        return;
    }

    result = nvmlDeviceGetCount_(&device_count_);
    if (result != NVML_SUCCESS) {
        std::cerr << "[vinntry] Failed to fetch device count: " 
                            << nvmlErrorString_(result) << std::endl;
        nvmlShutdown_();
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
        nvmlReturn_t result = nvmlDeviceGetHandleByIndex_(i, &handle);
        if (result != NVML_SUCCESS) continue;

        char name_buffer[64];
        result = nvmlDeviceGetName_(handle, name_buffer, sizeof(name_buffer));
        std::string gpu_name = 
            (result == NVML_SUCCESS) ? std::string(name_buffer) : "Unknown NVIDIA GPU";
        
        std::map<std::string, std::string> labels = {
            {"device_index", std::to_string(i)},
            {"gpu_name", gpu_name}
        };

        DeviceMetrics metrics;
        metrics.gpu_util = &gpu_util_family_.Add(labels);
        metrics.nvdec_util = &nvdec_util_family_.Add(labels);
        metrics.mem_util = &mem_util_family_.Add(labels);
        metrics.fb_used = &fb_used_family_.Add(labels);
        metrics.gpu_temp = &gpu_temp_family_.Add(labels);
        metrics.gpu_clock = &gpu_clock_family_.Add(labels);
        metrics.gpu_power = &gpu_power_family_.Add(labels);
        metrics.gpu_power_limit = &gpu_power_limit_family_.Add(labels);
        metrics.pcie_tx = &pcie_tx_family_.Add(labels);
        metrics.pcie_rx = &pcie_rx_family_.Add(labels);
        
        metric_map_[i] = metrics;
        std::cout << "[vinntry] Indexed profiling target [" << i << "]: " << gpu_name << std::endl;
    }
}
