#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>

namespace hw_monitor {

struct RAMProcessInfo {
    uint32_t pid;
    std::string process_name;
    float memory_usage_mb;      // Physical memory (RAM) usage
    float virtual_memory_mb;    // Virtual memory usage
    float shared_memory_mb;     // Shared memory usage
    float memory_percent;       // Percentage of total RAM used
};

struct RAMInfo {
    float total_memory_mb;      // Total physical memory
    float used_memory_mb;       // Used physical memory
    float free_memory_mb;       // Free physical memory
    float shared_memory_mb;     // Shared memory
    float cache_memory_mb;      // Cache memory
    float available_memory_mb;  // Available memory (free + cache + buffers)
    float usage_percent;        // Overall memory usage percentage
};

class RAMDetector {
public:
    RAMDetector();
    ~RAMDetector();

    // Get RAM usage info for a specific process by PID
    std::optional<RAMProcessInfo> get_process_info(uint32_t pid) const;

    // Get RAM usage info for all processes matching a name
    std::optional<std::vector<RAMProcessInfo>> get_process_info(const std::string& process_name) const;

    // Get overall RAM usage statistics
    RAMInfo get_ram_info() const;

    // Get list of all running processes with their RAM usage
    std::vector<RAMProcessInfo> get_all_processes() const;

    // Get list of all process names
    std::vector<std::string> get_process_names() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace hw_monitor 