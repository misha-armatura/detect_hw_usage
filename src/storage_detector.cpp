#include "storage_detector.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <sys/statvfs.h>
#include <iostream>
#include <chrono>
#include <thread>

namespace hw_monitor {

std::string StorageDetector::read_file(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
}

std::unordered_map<std::string, uint64_t> StorageDetector::read_io_stats(uint32_t pid) const {
    std::unordered_map<std::string, uint64_t> stats;
    std::string io_path = "/proc/" + std::to_string(pid) + "/io";
    std::ifstream io_file(io_path);
    std::string line;

    while (std::getline(io_file, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        if (key.back() == ':') {
            key.pop_back();
            stats[key] = value;
        }
    }

    return stats;
}

uint64_t StorageDetector::count_open_files(uint32_t pid) const {
    uint64_t count = 0;
    std::string fd_path = "/proc/" + std::to_string(pid) + "/fd";
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(fd_path)) {
            count++;
        }
    } catch (...) {}

    return count;
}

std::string StorageDetector::get_main_device(uint32_t pid) const {
    std::string result;
    try {
        std::string exe_path = std::filesystem::read_symlink(
            std::filesystem::path("/proc") / std::to_string(pid) / "exe"
        ).string();
        
        std::filesystem::path path(exe_path);
        while (!path.empty()) {
            struct statvfs stats;
            if (statvfs(path.c_str(), &stats) == 0) {
                // Get device name from /proc/mounts
                std::ifstream mounts("/proc/mounts");
                std::string line;
                while (std::getline(mounts, line)) {
                    std::istringstream iss(line);
                    std::string device, mount_point;
                    iss >> device >> mount_point;
                    if (mount_point == path) {
                        return device;
                    }
                }
            }
            path = path.parent_path();
        }
    } catch (...) {}
    
    return result;
}

std::vector<StorageInfo> StorageDetector::get_storage_info() const {
    std::vector<StorageInfo> result;
    std::ifstream mounts("/proc/mounts");
    std::string line;

    while (std::getline(mounts, line)) {
        std::istringstream iss(line);
        std::string device, mount_point, fs_type;
        iss >> device >> mount_point >> fs_type;

        // Skip pseudo filesystems
        if (fs_type == "tmpfs" || fs_type == "devtmpfs" || 
            fs_type == "sysfs" || fs_type == "proc" || 
            fs_type == "devpts") continue;

        struct statvfs stats;
        if (statvfs(mount_point.c_str(), &stats) == 0) {
            StorageInfo info;
            info.device_name = device;
            info.mount_point = mount_point;
            info.filesystem_type = fs_type;
            info.total_bytes = stats.f_blocks * stats.f_frsize;
            info.available_bytes = stats.f_bavail * stats.f_frsize;
            info.used_bytes = (stats.f_blocks - stats.f_bfree) * stats.f_frsize;
            info.usage_percent = ((float)(stats.f_blocks - stats.f_bfree) / stats.f_blocks) * 100.0f;
            result.push_back(std::move(info));
        }
    }

    return result;
}

std::optional<StorageProcessInfo> StorageDetector::get_process_info(uint32_t pid) const {
    if (!std::filesystem::exists("/proc/" + std::to_string(pid))) {
        return std::nullopt;
    }

    // Get initial IO stats
    auto initial_stats = read_io_stats(pid);
    
    // Wait a short time to calculate rate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Get updated IO stats
    auto final_stats = read_io_stats(pid);

    StorageProcessInfo info;
    info.pid = pid;
    info.process_name = read_file("/proc/" + std::to_string(pid) + "/comm");
    if (!info.process_name.empty() && info.process_name.back() == '\n') {
        info.process_name.pop_back();
    }

    // Calculate IO rates (bytes per second)
    float time_diff = 0.1f; // 100ms in seconds
    info.read_bytes_per_sec = (final_stats["read_bytes"] - initial_stats["read_bytes"]) / time_diff;
    info.write_bytes_per_sec = (final_stats["write_bytes"] - initial_stats["write_bytes"]) / time_diff;
    
    info.open_files = count_open_files(pid);
    info.main_device = get_main_device(pid);

    return info;
}

std::optional<std::vector<StorageProcessInfo>> StorageDetector::get_process_info(const std::string& process_name) const {
    std::vector<StorageProcessInfo> result;

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!std::filesystem::is_directory(entry.path())) continue;

        std::string pid_str = entry.path().filename().string();
        if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

        try {
            uint32_t pid = std::stoul(pid_str);
            std::string comm = read_file(entry.path().string() + "/comm");
            if (!comm.empty() && comm.find(process_name) != std::string::npos) {
                if (auto proc_info = get_process_info(pid)) {
                    result.push_back(*proc_info);
                }
            }
        } catch (...) {}
    }

    return result.empty() ? std::nullopt : std::make_optional(result);
}

} // namespace hw_monitor 