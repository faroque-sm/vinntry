# vinntry-daemon

A high-performance, low-overhead native C++ telemetry daemon designed to profile NVIDIA GPU metrics at microsecond resolution. The system uses runtime dynamic library loading to map and execute `libnvidia-ml.so` symbols, bypassing compile-time hardware dependencies and allowing deployment across native Linux, virtualized environments, and Windows Subsystem for Linux (WSL). 

Collected metrics are exposed via an embedded HTTP endpoint to a Prometheus time-series database running at a high-frequency **4Hz sampling rate (250ms)**, which drives a pre-provisioned Grafana monitoring dashboard.

---

## 🏗️ System Architecture & Data Flow

The application isolates the C++ host process from host compilation dependencies while bridging data cleanly to containerized storage and visualization layers.

![vinntry System Architecture Blueprint](docs/images/sys-arch.png "vinntry System Architecture Blueprint")

---

## 📺 Live Dashboard Performance Preview

The pre-provisioned Grafana viewport is optimized to match the daemon's aggressive 4Hz execution cadence. The animation below demonstrates the microsecond-drift capture loops updating telemetry fields smoothly in real-time under volatile hardware processing loads:

![vinntry Native Telemetry Pipeline Performance Preview](docs/images/vinntry-dash-preview.gif "vinntry Native Telemetry Pipeline Performance Preview")

### High-Resolution Panel Layout Highlights
* **Deterministic Refresh Rates:** Visual updates mirror the strict 250ms sampling loop natively, bypassing standard 15-second metric aggregation delays.
* **Transient Spike Capture:** Real-time frequency fluctuations (`MHz`) and core power draw spikes (`mW`) are registered instantly before being smoothed out by time-series averaging.
* **Asynchronous Driver Pipeline:** VRAM consumption tracking vectors update concurrently across independent GPU nodes without causing UI thread stutter or connection dropouts.

---

## 📖 Architecture Terminology & Core Concepts

To maintain a minimal footprint and achieve sub-millisecond precision, this daemon relies on specific low-level systems programming patterns:

* **Runtime Dynamic Library Loading (`dlfcn.h`):** A system API (`dlopen`, `dlsym`, `dlclose`) used to load shared libraries into memory and resolve function pointers at execution time, rather than during compilation. In `src/metric_collector.cpp`, this eliminates the requirement to link against the official NVIDIA CUDA Toolkit headers at compile time. The daemon builds successfully on any standard machine and will only look for the local driver (`libnvidia-ml.so.1`) when it executes.
* **Drift-Compensated Execution Loop:** A timing mechanism that schedules subsequent loops based on absolute future timestamps (`std::chrono::steady_clock`), rather than sleeping for fixed relative durations. Standard relative sleeps (`sleep_for`) suffer from cumulative scheduling drift because the execution time of the code itself adds a tiny delay to each cycle. By tracking absolute target points via `std::this_thread::sleep_until`, the daemon cancels out processing overhead and guarantees a strict, deterministic **4Hz sampling cadence**.
* **Metric Families & Client Registry:** Architectural patterns from the Prometheus telemetry specification where metrics are grouped under a shared base name but isolated via unique key-value descriptor arrays (labels). Instead of instantiating separate tracking objects for every variable, the system registers base `prometheus::Family<prometheus::Gauge>` definitions once. When the system boots, `register_devices()` dynamically forks these families into standalone metric instances assigned explicitly to specific hardware lanes via `device_index` and `gpu_name` metadata tags.
* **Signal Intercept Driver Safeties:** POSIX signal handlers (`std::signal`) configured to intercept operating system runtime kernel faults or termination signals before the application process context completely breaks. If a catastrophic memory fault occurs (`SIGSEGV` or `SIGABRT`), the global atomic pointers inside `src/main.cpp` capture the exception, manually trigger the collector's destructor, invoke the dynamic `nvmlShutdown_` handle, and cleanly unload the driver memory via `dlclose` before yielding control back to the operating system kernel.

---

## 📊 Telemetry Metrics Collected

The native collector maps the following instrumentation points into Prometheus Gauge families, automatically tracking each device index and physical model label:

| Metric String | Unit | Target Context |
| :--- | :--- | :--- |
| `nvml_gpu_util_percent` | % | Core Graphics Compute Utilization |
| `nvml_nvdec_util_percent` | % | Hardware Video Decoder ASIC Load |
| `nvml_mem_util_percent` | % | Integrated Memory Controller Utilization |
| `nvml_fb_mem_bytes` | Bytes | VRAM FrameBuffer Allocated Capacity |
| `nvml_gpu_temp_celsius` | °C | Physical Silicon Core Thermal Readings |
| `nvml_gpu_clock_mhz` | MHz | Real-Time Graphics Engine Clock Frequency |
| `nvml_power_usage_mwatts` | mW | Operational Board Level Power Consumption |
| `nvml_power_limit_mwatts` | mW | Total Enforced Hardware Cap Constraints |
| `nvml_pcie_tx_kbs` | KB/s | PCIe Throughput Host-to-Device |
| `nvml_pcie_rx_kbs` | KB/s | PCIe Throughput Device-to-Host |

---

## 🚀 Getting Started

### Prerequisites
* Linux Environment or **Windows Subsystem for Linux (WSL)**.
* **Mamba** or **Conda** package manager.
* Docker and the Docker Compose plugin.

### 1. Environment Setup & Compilation
The project uses a Conda/Mamba virtual environment to handle toolchain dependencies (`cmake`, `ninja`, and `prometheus-cpp`):

```bash
# Initialize dependency landscape via environment configuration layout
mamba env create -f environment.yml
mamba activate vinntry-env

# Generate native build trees using Ninja build system
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 2. Launching the Infrastructure Stack
To stand up the time-series storage and UI visualization layers, spin up the Docker Compose stack:

```bash
docker compose up -d
```

### 3. Execution
Execute the compiled native binary directly on your host engine. It automatically binds an HTTP collector endpoint to port `9101`, allowing the containerized Prometheus instance to query telemetry records directly via the `host-gateway` bridge.

```bash
./build/vinntry_daemon
```

---

## 🛠️ Configuration & Customization

The configuration of the telemetry stack is distributed across three main operational bounds: the C++ Daemon, the Prometheus TSDB Scraping Rate, and the Docker Port Mappings.

### 1. Modifying the C++ Daemon Network Configuration
The native daemon hardcodes the listening endpoint for the Prometheus HTTP scrapper interface inside `src/main.cpp`.

To shift the daemon binding away from default port **`9101`**:
1. Open `src/main.cpp` and locate the initialization of the `prometheus::Exposer` block:
   ```cpp
   prometheus::Exposer exposer{"0.0.0.0:9101"};
   ```
2. Recompile the daemon application utilizing the local build system:
   ```bash
   cmake --build build
   ```

### 2. Tuning Scrape Frequencies & High-Resolution Cadence
The tracking loop features a high-frequency **4Hz framework** across both the native polling code and the containerized collector. If you need to scale down network or storage overhead, you must adjust both locations symmetrically to prevent stale telemetry captures.

#### Adjusting C++ Driver Polling Frequency:
1. Open `src/main.cpp` and locate the timing loop interval variable:
   ```cpp
   const auto interval = std::chrono::milliseconds(250);
   ```

#### Adjusting Prometheus Metrics Extraction Rates:
1. Open `config/prometheus/prometheus.yml`.
2. Modify both the `scrape_interval` and `evaluation_interval` targets to sync with your C++ timing offset:
   ```yaml
   global:
     scrape_interval: 250ms
     evaluation_interval: 250ms
   ```
3. Restart or reload the Prometheus storage container engine to parse changes:
   ```bash
   docker compose restart prometheus
   ```

### 3. Adjusting Exposed Infrastructure Network Ports
If ports **`9090`** (Prometheus Workbench) or **`3000`** (Grafana Interface) conflict with existing applications running natively on your host environment, modify the exposed routing rules within the composition file.

1. Open `docker-compose.yml` and locate the specific service block mapping.
2. Edit the **left-hand** variable inside the `ports` collection arrays to re-route incoming network transactions (format: `HOST:CONTAINER`):
   ```yaml
   services:
     prometheus:
       ports:
         - "9090:9090"  # Change host side (left) if port 9090 is in use
         
     grafana:
       ports:
         - "3000:3000"  # Change host side (left) if port 3000 is in use
   ```
3. Re-initialize the active container infrastructure matrix:
   ```bash
   docker compose up -d
   ```

---

## 🔒 Storage and Access Controls

* **Grafana Credentials:** Sign-in access is restricted via custom deployment options. Use Username: `admin`, Password: `vinntry_pass`.
* **Dashboard Provisioning:** Dashboards and sources are hot-mounted directly into the target containers. The `vinntry-dash.json` file is enforced via `grafana.ini` as the persistent landing configuration root, meaning monitoring interfaces are instantly accessible without manual UI setup.
