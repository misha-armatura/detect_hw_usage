#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace hw_monitor {

/**
 * @brief Information about a GPU process
 */
struct GPUProcessInfo {
    uint32_t pid;                           ///< Process ID
    std::string process_name;               ///< Name of the process
    uint32_t gpu_index;                     ///< Index of the GPU this process is running on
    float memory_usage_mb;                  ///< GPU memory usage in megabytes
    float gpu_usage_percent;                ///< GPU utilization percentage (0-100)
};

/**
 * @brief Information about a GPU device
 */
struct GPUInfo {
    uint32_t index;                         ///< GPU device index
    std::string name;                       ///< GPU device name/model
    float total_memory_mb;                  ///< Total GPU memory in megabytes
    float used_memory_mb;                   ///< Used GPU memory in megabytes
    float temperature_celsius;              ///< GPU temperature in Celsius
    float utilization_percent;              ///< GPU utilization percentage (0-100)
    std::vector<GPUProcessInfo> processes;  ///< List of processes using this GPU
};

/**
 * @brief Interface for vendor-specific GPU detection implementations
 * 
 * Abstract base class that defines the interface for different GPU vendor implementations (e.g., NVIDIA, AMD). 
 * Each vendor-specific implementation should inherit from this class.
 */
class IGPUDetector {
public:
    virtual ~IGPUDetector() = default;

    /**
     * @brief Check if this GPU implementation is available on the system
     * @return true if the implementation can detect GPUs, false otherwise
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Get information about all GPUs
     * @return Vector of GPU information structures
     */
    virtual std::vector<GPUInfo> get_gpu_info() const = 0;

    /**
     * @brief Get GPU usage information for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process GPU information if matching processes found
     */
    virtual std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const = 0;

    /**
     * @brief Get GPU usage information for a specific process
     * @param pid Process ID to monitor
     * @return Vector of process GPU information if process exists
     */
    virtual std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const = 0;

    /**
     * @brief Get information about a specific GPU
     * @param gpu_index Index of the GPU to query
     * @return GPU information if the GPU exists
     */
    virtual std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const = 0;
};

/**
 * @brief Main GPU detection class
 * 
 * This class manages multiple vendor-specific GPU implementations and provides a unified interface for GPU monitoring. 
 * It implements the Singleton to ensure only one instance exists.
 */
class GPUDetector {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the GPUDetector instance
     */
    static GPUDetector& instance();
    
    /**
     * @brief Get information about all detected GPUs
     * @return Vector of GPU information structures
     */
    std::vector<GPUInfo> get_gpu_info() const;

    /**
     * @brief Get GPU usage information for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process GPU information if matching processes found
     */
    std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const;

    /**
     * @brief Get GPU usage information for a specific process
     * @param pid Process ID to monitor
     * @return Vector of process GPU information if process exists
     */
    std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const;

    /**
     * @brief Get information about a specific GPU
     * @param gpu_index Index of the GPU to query
     * @return GPU information if the GPU exists
     */
    std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const;

private:
    GPUDetector();
    ~GPUDetector() = default;
    GPUDetector(const GPUDetector&) = delete;
    GPUDetector& operator=(const GPUDetector&) = delete;

    std::vector<std::unique_ptr<IGPUDetector>> implementations_;  ///< List of available GPU implementations
};

} // namespace hw_monitor 
