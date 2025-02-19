#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace hw_monitor {

struct GPUProcessInfo {
    uint32_t pid;
    std::string process_name;
    uint32_t gpu_index;
    float memory_usage_mb;
    float gpu_usage_percent;
};

struct GPUInfo {
    uint32_t index;
    std::string name;
    float total_memory_mb;
    float used_memory_mb;
    float temperature_celsius;
    float utilization_percent;
    std::vector<GPUProcessInfo> processes;
};

// Abstract base class for GPU vendor-specific implementations
class IGPUDetector {
public:
    virtual ~IGPUDetector() = default;
    virtual bool is_available() const = 0;
    virtual std::vector<GPUInfo> get_gpu_info() const = 0;
    virtual std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const = 0;
    virtual std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const = 0;
    virtual std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const = 0;
};

// Main GPU detector class that selects and uses appropriate implementation
class GPUDetector {
public:
    static GPUDetector& instance();
    
    std::vector<GPUInfo> get_gpu_info() const;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const;
    std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const;

private:
    GPUDetector();
    ~GPUDetector() = default;
    GPUDetector(const GPUDetector&) = delete;
    GPUDetector& operator=(const GPUDetector&) = delete;

    std::vector<std::unique_ptr<IGPUDetector>> implementations_;
};

} // namespace hw_monitor 
