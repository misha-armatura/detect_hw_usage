#pragma once

#include "gpu_detector.hpp"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace hw_monitor {

/**
 * @brief AMD GPU detector class for monitoring AMD graphics cards
 * 
 * This class provides functionality to detect and monitor AMD GPUs using the Linux sysfs interface.
 * It can retrieve information about GPU utilization, memory usage, temperature, and process-specific
 * GPU usage by analyzing system files and process information.
 */
class AMDGPUDetector : public IGPUDetector {
private:
    bool initialized_;                    ///< Indicates if AMD GPU was successfully detected
    std::vector<std::string> gpu_paths_;  ///< Paths to AMD GPU devices in sysfs

    /**
     * @brief Print debug messages to stderr
     * @param msg The message to print
     */
    void debug_print(const std::string& msg) const;

    /**
     * @brief Read contents of a file
     * @param path Path to the file
     * @return String containing the first line of the file, or empty string if file cannot be read
     */
    static std::string read_file(const std::string& path);

    /**
     * @brief Find the hwmon directory for a GPU card
     * @param card_path Path to the GPU card directory
     * @return Path to hwmon directory, or empty string if not found
     */
    static std::string find_hwmon_dir(const std::string& card_path);

    /**
     * @brief Check if a device is an AMD GPU
     * @param path Path to the device directory
     * @return true if the device is an AMD GPU, false otherwise
     */
    bool is_amd_gpu(const std::string& path);

public:
    /**
     * @brief Constructor that initializes AMD GPU detection
     * 
     * Scans the system for AMD GPUs by checking /sys/class/drm for devices with AMD vendor ID (0x1002).
     */
    AMDGPUDetector();
    
    /**
     * @brief Virtual destructor
     */
    ~AMDGPUDetector() override = default;

    /**
     * @brief Check if AMD GPU is available in the system
     * @return true if AMD GPU was detected and initialized, false otherwise
     */
    bool is_available() const override;

    /**
     * @brief Get information about all AMD GPUs in the system
     * @return Vector of GPUInfo structures containing information about each GPU
     */
    std::vector<GPUInfo> get_gpu_info() const override;

    /**
     * @brief Get GPU usage information for a specific process
     * @param process_name Name of the process to monitor
     * @return Optional vector of GPUProcessInfo structures, or nullopt if process not found
     * 
     * Searches for the specified process and collects its GPU usage information by:
     * - Checking process maps for GPU memory regions
     * - Analyzing file descriptors for GPU device access
     * - Calculating memory usage and utilization
     */
    std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const override;

    /**
     * @brief Get GPU usage information for a process by PID
     * @param pid Process ID to monitor
     * @return Optional vector of GPUProcessInfo structures, or nullopt if process not found
     */
    std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const override;

    /**
     * @brief Get information about a specific GPU by index
     * @param gpu_index Index of the GPU to query
     * @return Optional GPUInfo structure, or nullopt if GPU not found
     */
    std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const override;
};

} // namespace hw_monitor 
