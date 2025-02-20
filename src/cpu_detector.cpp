#include "cpu_detector.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unistd.h>

namespace hw_monitor {

std::string CPUDetector::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
}

std::vector<std::string> CPUDetector::read_file_lines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::map<int, CPUDetector::CPUStats> CPUDetector::read_cpu_stats() {
    std::map<int, CPUStats> stats;
    auto lines = read_file_lines("/proc/stat");
    
    for (const auto& line : lines) {
        if (line.substr(0, 3) == "cpu") {
            std::istringstream iss(line);
            std::string cpu_label;
            iss >> cpu_label;
            
            int cpu_id = -1;  // -1 for total CPU
            if (cpu_label.length() > 3) {
                cpu_id = std::stoi(cpu_label.substr(3));
            }

            CPUStats cpu_stats;
            iss >> cpu_stats.user >> cpu_stats.nice >> cpu_stats.system 
                >> cpu_stats.idle >> cpu_stats.iowait >> cpu_stats.irq 
                >> cpu_stats.softirq >> cpu_stats.steal >> cpu_stats.guest 
                >> cpu_stats.guest_nice;

            stats[cpu_id] = cpu_stats;
        }
    }
    return stats;
}

std::unordered_map<uint32_t, uint64_t> CPUDetector::read_process_times() {
    std::unordered_map<uint32_t, uint64_t> times;
    
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!std::filesystem::is_directory(entry.path())) continue;

        std::string pid_str = entry.path().filename().string();
        if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

        try {
            uint32_t pid = std::stoul(pid_str);
            std::string stat = read_file("/proc/" + pid_str + "/stat");
            std::istringstream iss(stat);
            
            // Skip pid and comm fields
            std::string dummy;
            iss >> dummy >> dummy;
            
            // Skip to utime and stime (fields 14 and 15)
            for (int i = 0; i < 11; ++i) iss >> dummy;
            
            uint64_t utime, stime;
            iss >> utime >> stime;
            
            times[pid] = utime + stime;
        } catch (...) {}
    }
    
    return times;
}

float CPUDetector::get_cpu_frequency(int cpu_id) {
    std::string freq_path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu_id) + "/cpufreq/scaling_cur_freq";
    std::string freq_str = read_file(freq_path);
    return freq_str.empty() ? 0.0f : std::stof(freq_str) / 1000.0f; // Convert KHz to MHz
}

float CPUDetector::get_cpu_temperature(int cpu_id) {
    // Try different temperature sources
    std::vector<std::string> temp_paths = {
        "/sys/class/thermal/thermal_zone" + std::to_string(cpu_id) + "/temp",
        "/sys/devices/platform/coretemp." + std::to_string(cpu_id) + "/hwmon/hwmon*/temp1_input"
    };

    for (const auto& path : temp_paths) {
        std::string temp_str = read_file(path);
        if (!temp_str.empty()) {
            return std::stof(temp_str) / 1000.0f; // Convert millidegrees to degrees
        }
    }
    return 0.0f;
}

uint32_t CPUDetector::get_thread_count(uint32_t pid) {
    std::string task_dir = "/proc/" + std::to_string(pid) + "/task";
    uint32_t count = 0;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(task_dir)) {
            if (std::filesystem::is_directory(entry.path())) {
                count++;
            }
        }
    } catch (...) {}
    
    return count;
}

CPUProcessInfo CPUDetector::get_process_cpu_info(uint32_t pid,
                                               const std::unordered_map<uint32_t, uint64_t>& initial_times,
                                               const std::unordered_map<uint32_t, uint64_t>& final_times) {
    CPUProcessInfo info;
    info.pid = pid;

    // Get process name
    info.process_name = read_file("/proc/" + std::to_string(pid) + "/comm");
    if (!info.process_name.empty() && info.process_name.back() == '\n') {
        info.process_name.pop_back();
    }

    // Get thread count
    info.thread_count = get_thread_count(pid);

    // Get process state and nice value
    std::string stat = read_file("/proc/" + std::to_string(pid) + "/stat");
    std::istringstream iss(stat);
    std::string dummy;
    char state;
    int nice;
    
    // Skip pid and comm fields
    iss >> dummy >> dummy >> state;
    
    // Skip to nice value (field 19)
    for (int i = 0; i < 15; ++i) iss >> dummy;
    iss >> nice;
    
    info.state = std::string(1, state);
    info.nice = nice;

    // Calculate CPU usage
    if (initial_times.count(pid) && final_times.count(pid)) {
        uint64_t time_diff = final_times.at(pid) - initial_times.at(pid);
        float seconds = 0.1f; // 100ms sampling period
        info.cpu_time_ms = time_diff * (1000.0f / sysconf(_SC_CLK_TCK));
        info.cpu_usage_percent = (time_diff * 100.0f) / (seconds * sysconf(_SC_CLK_TCK));
    } else {
        info.cpu_time_ms = 0;
        info.cpu_usage_percent = 0.0f;
    }

    // Get CPU affinity
    cpu_set_t mask;
    if (sched_getaffinity(pid, sizeof(cpu_set_t), &mask) == 0) {
        info.cpu_affinity = 0;
        for (int i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &mask)) {
                info.cpu_affinity |= (1 << i);
            }
        }
    } else {
        info.cpu_affinity = 0;
    }

    return info;
}

CPUInfo CPUDetector::get_cpu_info() const {
    CPUInfo info;
    
    // Get initial CPU stats
    auto initial_stats = read_cpu_stats();
    
    // Wait a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get final CPU stats
    auto final_stats = read_cpu_stats();

    // Calculate total CPU usage
    const auto& initial_total = initial_stats[-1];
    const auto& final_total = final_stats[-1];
    
    uint64_t total_diff = final_total.get_total() - initial_total.get_total();
    uint64_t idle_diff = final_total.get_idle() - initial_total.get_idle();
    
    info.total_usage_percent = ((total_diff - idle_diff) * 100.0f) / total_diff;

    // Get core information
    info.core_count = std::thread::hardware_concurrency();
    info.thread_count = info.core_count; // Assuming 1 thread per core by default

    float total_freq = 0.0f;
    float total_temp = 0.0f;
    int temp_count = 0;

    // Get per-core information
    for (int i = 0; i < static_cast<int>(info.core_count); ++i) {
        CPUCoreInfo core;
        core.core_id = i;
        
        // Read CPU model name
        std::string cpuinfo = read_file("/proc/cpuinfo");
        std::istringstream iss(cpuinfo);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    core.model_name = line.substr(pos + 2);
                    break;
                }
            }
        }

        // Get frequencies
        core.current_frequency_mhz = get_cpu_frequency(i);
        total_freq += core.current_frequency_mhz;

        std::string max_freq_path = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_max_freq";
        std::string min_freq_path = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/scaling_min_freq";
        
        std::string max_freq_str = read_file(max_freq_path);
        std::string min_freq_str = read_file(min_freq_path);
        
        core.max_frequency_mhz = max_freq_str.empty() ? 0.0f : std::stof(max_freq_str) / 1000.0f;
        core.min_frequency_mhz = min_freq_str.empty() ? 0.0f : std::stof(min_freq_str) / 1000.0f;

        // Get temperature
        core.temperature_celsius = get_cpu_temperature(i);
        if (core.temperature_celsius > 0) {
            total_temp += core.temperature_celsius;
            temp_count++;
        }

        // Calculate core usage
        if (initial_stats.count(i) && final_stats.count(i)) {
            const auto& initial_core = initial_stats[i];
            const auto& final_core = final_stats[i];
            
            uint64_t core_total_diff = final_core.get_total() - initial_core.get_total();
            uint64_t core_idle_diff = final_core.get_idle() - initial_core.get_idle();
            
            core.usage_percent = ((core_total_diff - core_idle_diff) * 100.0f) / core_total_diff;
            info.usage_per_core.push_back(core.usage_percent);
        }

        info.cores.push_back(std::move(core));
    }

    info.average_frequency_mhz = total_freq / info.core_count;
    info.average_temperature_celsius = temp_count > 0 ? total_temp / temp_count : 0.0f;

    return info;
}

std::optional<CPUProcessInfo> CPUDetector::get_process_info(uint32_t pid) const {
    if (!std::filesystem::exists("/proc/" + std::to_string(pid))) {
        return std::nullopt;
    }

    auto initial_times = read_process_times();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto final_times = read_process_times();

    return get_process_cpu_info(pid, initial_times, final_times);
}

std::optional<std::vector<CPUProcessInfo>> CPUDetector::get_process_info(const std::string& process_name) const {
    std::vector<CPUProcessInfo> result;
    
    auto initial_times = read_process_times();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto final_times = read_process_times();

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!std::filesystem::is_directory(entry.path())) continue;

        std::string pid_str = entry.path().filename().string();
        if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

        try {
            uint32_t pid = std::stoul(pid_str);
            std::string comm = read_file(entry.path().string() + "/comm");
            if (!comm.empty() && comm.find(process_name) != std::string::npos) {
                auto proc_info = get_process_cpu_info(pid, initial_times, final_times);
                result.push_back(std::move(proc_info));
            }
        } catch (...) {}
    }

    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::vector<CPUProcessInfo> CPUDetector::get_top_processes(size_t limit) const {
    std::vector<CPUProcessInfo> result;
    
    auto initial_times = read_process_times();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto final_times = read_process_times();

    for (const auto& [pid, _] : initial_times) {
        try {
            auto proc_info = get_process_cpu_info(pid, initial_times, final_times);
            result.push_back(std::move(proc_info));
        } catch (...) {}
    }

    // Sort by CPU usage
    std::sort(result.begin(), result.end(),
             [](const CPUProcessInfo& a, const CPUProcessInfo& b) {
                 return a.cpu_usage_percent > b.cpu_usage_percent;
             });

    // Limit the number of results
    if (result.size() > limit) {
        result.resize(limit);
    }

    return result;
}

} // namespace hw_monitor 
