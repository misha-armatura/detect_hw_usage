#include "ram_detector.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <iostream>

namespace hw_monitor {

class RAMDetector::Impl {
private:
    static std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::string content;
        std::getline(file, content);
        return content;
    }

    static std::unordered_map<std::string, uint64_t> parse_meminfo() {
        std::unordered_map<std::string, uint64_t> result;
        std::ifstream meminfo("/proc/meminfo");
        std::string line;

        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss >> key >> value;
            if (key.back() == ':') {
                key.pop_back();
                result[key] = value;
            }
        }

        return result;
    }

    static RAMProcessInfo get_process_memory_info(uint32_t pid) {
        RAMProcessInfo info;
        info.pid = pid;

        // Read process name from /proc/[pid]/comm
        std::string comm_path = "/proc/" + std::to_string(pid) + "/comm";
        info.process_name = read_file(comm_path);
        if (!info.process_name.empty() && info.process_name.back() == '\n') {
            info.process_name.pop_back();
        }

        // Read memory information from /proc/[pid]/status
        std::ifstream status("/proc/" + std::to_string(pid) + "/status");
        std::string line;
        while (std::getline(status, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            std::string unit;
            iss >> key >> value >> unit;

            if (key == "VmRSS:") {
                info.memory_usage_mb = value / 1024.0f;  // Convert KB to MB
            } else if (key == "VmSize:") {
                info.virtual_memory_mb = value / 1024.0f;
            } else if (key == "RssFile:") {
                info.shared_memory_mb = value / 1024.0f;
            }
        }

        // Calculate memory percentage
        auto meminfo = parse_meminfo();
        if (meminfo.count("MemTotal") > 0 && meminfo["MemTotal"] > 0) {
            info.memory_percent = (info.memory_usage_mb * 1024.0f / meminfo["MemTotal"]) * 100.0f;
        }

        return info;
    }

public:
    Impl() {}

    RAMInfo get_ram_info() const {
        RAMInfo info;
        auto meminfo = parse_meminfo();

        info.total_memory_mb = meminfo["MemTotal"] / 1024.0f;
        info.free_memory_mb = meminfo["MemFree"] / 1024.0f;
        info.available_memory_mb = meminfo["MemAvailable"] / 1024.0f;
        info.shared_memory_mb = meminfo["Shmem"] / 1024.0f;
        info.cache_memory_mb = (meminfo["Cached"] + meminfo["Buffers"]) / 1024.0f;
        info.used_memory_mb = info.total_memory_mb - info.available_memory_mb;
        info.usage_percent = (info.used_memory_mb / info.total_memory_mb) * 100.0f;

        return info;
    }

    std::optional<RAMProcessInfo> get_process_info(uint32_t pid) const {
        try {
            if (std::filesystem::exists("/proc/" + std::to_string(pid))) {
                return get_process_memory_info(pid);
            }
        } catch (...) {}
        return std::nullopt;
    }

    std::optional<std::vector<RAMProcessInfo>> get_process_info(const std::string& process_name) const {
        std::vector<RAMProcessInfo> result;

        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!std::filesystem::is_directory(entry.path())) continue;

            std::string pid_str = entry.path().filename().string();
            if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

            try {
                uint32_t pid = std::stoul(pid_str);
                auto proc_info = get_process_memory_info(pid);
                if (proc_info.process_name.find(process_name) != std::string::npos) {
                    result.push_back(proc_info);
                }
            } catch (...) {}
        }

        return result.empty() ? std::nullopt : std::make_optional(result);
    }

    std::vector<RAMProcessInfo> get_all_processes() const {
        std::vector<RAMProcessInfo> result;

        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!std::filesystem::is_directory(entry.path())) continue;

            std::string pid_str = entry.path().filename().string();
            if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

            try {
                uint32_t pid = std::stoul(pid_str);
                result.push_back(get_process_memory_info(pid));
            } catch (...) {}
        }

        return result;
    }

    std::vector<std::string> get_process_names() const {
        std::vector<std::string> result;
        std::unordered_map<std::string, bool> unique_names;

        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!std::filesystem::is_directory(entry.path())) continue;

            std::string pid_str = entry.path().filename().string();
            if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

            std::string name = read_file(entry.path().string() + "/comm");
            if (!name.empty()) {
                if (name.back() == '\n') name.pop_back();
                if (!unique_names[name]) {
                    unique_names[name] = true;
                    result.push_back(name);
                }
            }
        }

        std::sort(result.begin(), result.end());
        return result;
    }
};

// Implement the public interface
RAMDetector::RAMDetector() : pimpl_(std::make_unique<Impl>()) {}
RAMDetector::~RAMDetector() = default;

std::optional<RAMProcessInfo> RAMDetector::get_process_info(uint32_t pid) const {
    return pimpl_->get_process_info(pid);
}

std::optional<std::vector<RAMProcessInfo>> RAMDetector::get_process_info(const std::string& process_name) const {
    return pimpl_->get_process_info(process_name);
}

RAMInfo RAMDetector::get_ram_info() const {
    return pimpl_->get_ram_info();
}

std::vector<RAMProcessInfo> RAMDetector::get_all_processes() const {
    return pimpl_->get_all_processes();
}

std::vector<std::string> RAMDetector::get_process_names() const {
    return pimpl_->get_process_names();
}

} // namespace hw_monitor 