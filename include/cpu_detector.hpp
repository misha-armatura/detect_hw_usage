#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <map>
#include <unordered_map>

namespace hw_monitor {

/**
 * @brief Information about a CPU process
 */
struct CPUProcessInfo {
    uint32_t pid;                       ///< Process ID
    std::string process_name;           ///< Name of the process
    float cpu_usage_percent;            ///< CPU usage percentage (0-100)
    uint32_t thread_count;              ///< Number of threads
    uint64_t cpu_time_ms;               ///< Total CPU time used in milliseconds
    uint32_t cpu_affinity;              ///< CPU affinity mask
    int32_t nice;                       ///< Process nice value
    std::string state;                  ///< Process state (Running, Sleeping, etc.)
};

/**
 * @brief Information about a CPU core
 */
struct CPUCoreInfo {
    uint32_t core_id;                   ///< Core identifier
    uint32_t physical_id;               ///< Physical CPU ID
    std::string model_name;             ///< CPU model name
    float current_frequency_mhz;        ///< Current frequency in MHz
    float max_frequency_mhz;            ///< Maximum frequency in MHz
    float min_frequency_mhz;            ///< Minimum frequency in MHz
    float temperature_celsius;          ///< Core temperature if available
    float usage_percent;                ///< Core usage percentage
};

/**
 * @brief Overall CPU information
 */
struct CPUInfo {
    uint32_t core_count;                ///< Number of CPU cores
    uint32_t thread_count;              ///< Number of CPU threads
    float total_usage_percent;          ///< Overall CPU usage
    float average_frequency_mhz;        ///< Average frequency across all cores
    float average_temperature_celsius;  ///< Average temperature across all cores
    std::vector<CPUCoreInfo> cores;     ///< Information for each core
    std::vector<float> usage_per_core;  ///< Usage percentage per core
};

/**
 * @brief CPU monitoring and statistics collection
 * 
 * This class provides functionality to monitor CPU usage, gather process statistics,
 * and collect information about CPU cores including frequency, temperature, and utilization.
 */
class CPUDetector {
private:
    /**
     * @brief Internal CPU statistics structure
     */
    struct CPUStats {
        uint64_t user;        ///< Time spent in user mode
        uint64_t nice;        ///< Time spent in user mode with low priority (nice)
        uint64_t system;      ///< Time spent in system mode
        uint64_t idle;        ///< Time spent in idle task
        uint64_t iowait;      ///< Time spent waiting for I/O to complete
        uint64_t irq;         ///< Time spent servicing hardware interrupts
        uint64_t softirq;     ///< Time spent servicing software interrupts
        uint64_t steal;       ///< Time stolen by other operating systems running in a virtual environment
        uint64_t guest;       ///< Time spent running a virtual CPU for guest operating systems
        uint64_t guest_nice;  ///< Time spent running a low priority virtual CPU for guest operating systems

        uint64_t get_idle() const { return idle + iowait; }
        uint64_t get_non_idle() const {
            return user + nice + system + irq + softirq + steal + guest + guest_nice;
        }
        uint64_t get_total() const { return get_idle() + get_non_idle(); }
    };

    static std::string read_file(const std::string& path);
    static std::vector<std::string> read_file_lines(const std::string& path);
    static std::map<int, CPUStats> read_cpu_stats();
    static std::unordered_map<uint32_t, uint64_t> read_process_times();
    static float get_cpu_frequency(int cpu_id);
    static float get_cpu_temperature(int cpu_id);
    static uint32_t get_thread_count(uint32_t pid);
    static CPUProcessInfo get_process_cpu_info(uint32_t pid,
                                             const std::unordered_map<uint32_t, uint64_t>& initial_times,
                                             const std::unordered_map<uint32_t, uint64_t>& final_times);

public:
    /**
     * @brief Default constructor
     */
    CPUDetector() = default;

    /**
     * @brief Get CPU usage info for a specific process
     * @param pid Process ID to monitor
     * @return Process CPU information if process exists
     */
    std::optional<CPUProcessInfo> get_process_info(uint32_t pid) const;

    /**
     * @brief Get CPU usage info for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process CPU information if matching processes found
     */
    std::optional<std::vector<CPUProcessInfo>> get_process_info(const std::string& process_name) const;

    /**
     * @brief Get overall CPU statistics
     * @return Detailed CPU information including per-core stats
     */
    CPUInfo get_cpu_info() const;

    /**
     * @brief Get list of top CPU-consuming processes
     * @param limit Maximum number of processes to return (default: 4)
     * @return Vector of process information sorted by CPU usage
     */
    std::vector<CPUProcessInfo> get_top_processes(size_t limit = 4) const;
};

} // namespace hw_monitor 