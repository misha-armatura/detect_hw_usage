#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>
#include <unordered_map>

namespace hw_monitor {

/**
 * @brief Information about a process's storage usage
 */
struct StorageProcessInfo {
    uint32_t pid;                           ///< Process ID
    std::string process_name;               ///< Name of the process
    float read_bytes_per_sec;               ///< Storage read rate in bytes per second
    float write_bytes_per_sec;              ///< Storage write rate in bytes per second
    std::string main_device;                ///< Main storage device being used by the process
    uint64_t open_files;                    ///< Number of open files by the process
};

/**
 * @brief Information about a storage device
 */
struct StorageInfo {
    std::string device_name;                ///< Device name (e.g., "/dev/sda1")
    std::string mount_point;                ///< Mount point path (e.g., "/")
    std::string filesystem_type;            ///< Filesystem type (e.g., "ext4")
    uint64_t total_bytes;                   ///< Total storage capacity in bytes
    uint64_t used_bytes;                    ///< Used storage space in bytes
    uint64_t available_bytes;               ///< Available storage space in bytes
    float usage_percent;                    ///< Storage usage percentage (0-100)
};

/**
 * @brief Storage device and process monitoring
 * 
 * This class provides functionality to monitor storage devices and processes,
 * including I/O rates, open files, and filesystem statistics. It reads information
 * from the Linux procfs interface and filesystem statistics.
 */
class StorageDetector {
public:
    /**
     * @brief Default constructor
     */
    StorageDetector() = default;

    /**
     * @brief Get storage usage info for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process storage information if matching processes found
     */
    std::optional<std::vector<StorageProcessInfo>> get_process_info(const std::string& process_name) const;

    /**
     * @brief Get storage usage info for a specific process
     * @param pid Process ID to monitor
     * @return Process storage information if process exists
     */
    std::optional<StorageProcessInfo> get_process_info(uint32_t pid) const;

    /**
     * @brief Get information about all storage devices
     * @return Vector of storage device information
     */
    std::vector<StorageInfo> get_storage_info() const;

private:
    /**
     * @brief Read contents of a file
     * @param path Path to the file
     * @return Contents of the file as string
     */
    std::string read_file(const std::string& path) const;

    /**
     * @brief Read I/O statistics for a process
     * @param pid Process ID
     * @return Map of I/O statistic keys to values
     */
    std::unordered_map<std::string, uint64_t> read_io_stats(uint32_t pid) const;

    /**
     * @brief Count number of open files for a process
     * @param pid Process ID
     * @return Number of open file descriptors
     */
    uint64_t count_open_files(uint32_t pid) const;

    /**
     * @brief Get main storage device used by a process
     * @param pid Process ID
     * @return Device name or empty string if not found
     */
    std::string get_main_device(uint32_t pid) const;
};

} // namespace hw_monitor 