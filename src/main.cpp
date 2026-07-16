#include "metric_collector.hpp"
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <atomic>
#include <csignal>

// Thread-safe flag to handle clean keyboard shutdown interrupts (Ctrl+C)
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main() {
    // Register os termination signals
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "[vinntry] Launching Native Hardware Telemetry Daemon..." << std::endl;

    try {
        // Stand up the HTTP Scrape endpoint server binding to port 8000
        prometheus::Exposer exposer{"0.0.0.0:9101"};

        // Instantiate the central metrics registry allocation block
        auto registry = std::make_shared<prometheus::Registry>();

        // Initiate native NVML metric poller engine
        MetricCollector collector{registry};

        // Connect the registry data block to the HTTP server mapping
        exposer.RegisterCollectable(registry);

        std::cout << "[vinntry] HTTP server listening on port :8000" << std::endl;
        std::cout << "[vinntry] Core benchmarking loop active at 4Hz (250ms resolution)." 
                                                                    << std::endl;
        
        // Establish the strict mathematical interval duration
        const auto interval = std::chrono::milliseconds(250);
        auto next_tick = std::chrono::steady_clock::now();

        // High-Resolution Drift-Compensated Execution Loop
        while (g_running) {
            // Execute the direct C-level hardware driver polling pass
            collector.update_metrics();

            // Calculate the exact absolute time point for the next scheduled tick
            next_tick += interval;

            // Sleep precisely until that absolute point, neutralizing loop execution time drift
            std::this_thread::sleep_until(next_tick);
        }

    } catch (const std::exception& e) {
        std::cerr << "[vinntry] FATAL RUNTIME EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[vinntry] Daemon loop terminated. Exiting cleanly." << std::endl;
    return 0;
}