#pragma once

#include <string>
#include <memory>
#include <map>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>

class MetricCollector {
    public:
        // Pass the Prometheus registry by pointer to safely link data blocks
        explicit MetricCollector(std::shared_ptr<prometheus::Registry> registry);
        ~MetricCollector();

        // The core execution loop that queries NVML and upadates the Prometheus metrics
        void update_metrics();

    private:
        unsigned int device_count_{0};
        bool nvml_initialized_{false};
        
        // Prometheus Metric Families
        // These manage the groups of charts that share the same metric name but have
        // different labels
        prometheus::Family<prometheus::Gauge>& gpu_util_family_;
        prometheus::Family<prometheus::Gauge>& mem_util_family_;
        prometheus::Family<prometheus::Gauge>& fb_used_family_;
        prometheus::Family<prometheus::Gauge>& gpu_temp_family_;

        // Metric Storage Index
        // Holds the memory pointer for each discovered GPU to prevent slow runtime allocations
        struct DeviceMetrics {
            prometheus::Gauge* gpu_util{nullptr};
            prometheus::Gauge* mem_util{nullptr};
            prometheus::Gauge* fb_used{nullptr};
            prometheus::Gauge* gpu_temp{nullptr};
        };

        // Maps each GPU's numeric index (0, 1,2) to its respective tracking metrics
        std::map<unsigned int, DeviceMetrics> metric_map_;

        // System initialization sub-routines
        void initialize_nvml();
        void register_devices();
};