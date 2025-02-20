#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace hw_monitor {

/**
 * @brief Information about a process's RAM usage
 */
struct RAMProcessInfo {
    uint32_t pid;                           ///< Process ID
    std::string process_name;               ///< Name of the process
    float memory_usage_mb;                  ///< Physical memory (RAM) usage in megabytes
    float virtual_memory_mb;                ///< Virtual memory usage in megabytes
    float shared_memory_mb;                 ///< Shared memory usage in megabytes
    float memory_percent;                   ///< Percentage of total RAM used by this process
};

/**
 * @brief System-wide RAM usage information
 */
struct RAMInfo {
    float total_memory_mb;                  ///< Total physical memory in megabytes
    float used_memory_mb;                   ///< Used physical memory in megabytes
    float free_memory_mb;                   ///< Free physical memory in megabytes
    float shared_memory_mb;                 ///< Shared memory in megabytes
    float cache_memory_mb;                  ///< Cache memory in megabytes
    float available_memory_mb;              ///< Available memory (free + cache + buffers) in megabytes
    float usage_percent;                    ///< Overall memory usage percentage (0-100)
};

/**
 * @brief RAM monitoring and statistics collection
 * 
 * This class provides functionality to monitor system RAM usage and gather
 * process-specific memory statistics. It reads information from the Linux
 * procfs interface (/proc filesystem).
 */
class RAMDetector {
public:
    /**
     * @brief Default constructor
     */
    RAMDetector() = default;

    /**
     * @brief Get RAM usage info for a specific process
     * @param pid Process ID to monitor
     * @return Process RAM information if process exists
     */
    std::optional<RAMProcessInfo> get_process_info(uint32_t pid) const;

    /**
     * @brief Get RAM usage info for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process RAM information if matching processes found
     */
    std::optional<std::vector<RAMProcessInfo>> get_process_info(const std::string& process_name) const;

    /**
     * @brief Get overall RAM usage statistics
     * @return System-wide RAM information
     */
    RAMInfo get_ram_info() const;

    /**
     * @brief Get list of all running processes with their RAM usage
     * @return Vector of process RAM information
     */
    std::vector<RAMProcessInfo> get_all_processes() const;

    /**
     * @brief Get list of all unique process names
     * @return Vector of process names
     */
    std::vector<std::string> get_process_names() const;

private:
    /**
     * @brief Read contents of a file
     * @param path Path to the file
     * @return Contents of the file as string
     */
    std::string read_file(const std::string& path) const;

    /**
     * @brief Parse /proc/meminfo file
     * @return Map of memory information keys to values
     */
    std::unordered_map<std::string, uint64_t> parse_meminfo() const;

    /**
     * @brief Get detailed memory information for a process
     * @param pid Process ID
     * @return Process RAM information structure
     */
    RAMProcessInfo get_process_memory_info(uint32_t pid) const;
};

} // namespace hw_monitor 