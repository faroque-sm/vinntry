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

// Global atomic pointer to catch the stack object during runtime crashes
std::atomic<MetricCollector*> g_active_collector{nullptr};

void signal_handler(int signal) {
    // Handle graceful teardown signals
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
        return;
    }

    // Handle other failure signals (SIGSEGV / SIGABRT)
    std::cerr << "\n[vinntry] CRITICAL: Intercepted hardware process crash/fault signal (" 
                                                            << signal << ")." << std::endl;
    
    // Retrieve active class memory pointer from global bridge
    MetricCollector* collector = g_active_collector.load(std::memory_order_acquire);
    if (collector) {
        // Trigger the destructor execution
        collector->~MetricCollector();
        g_active_collector.store(nullptr, std::memory_order_release);
        std::cerr << "[vinntry] Failure mode driver resource cleanup executed successfully" 
                                                                                << std::endl;
    }
    
    // Hand exit flag to OS kernel after termination of the dead process
    std::exit(signal);
}

int main() {
    // Register os termination signals and process crash
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);

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

        // Wipe the tracking pointer before a natural return exit sequence
        g_active_collector.store(nullptr, std::memory_order_release);

    } catch (const std::exception& e) {
        std::cerr << "[vinntry] FATAL RUNTIME EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[vinntry] Daemon loop terminated. Exiting cleanly." << std::endl;
    return 0;
}