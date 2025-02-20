#include "amd_gpu_detector.hpp"
#include <fstream>
#include <regex>
#include <iostream>
#include <sstream>

namespace hw_monitor {

void AMDGPUDetector::debug_print(const std::string& msg) const {
    std::cerr << "AMD Debug: " << msg << std::endl;
}

std::string AMDGPUDetector::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
}

std::string AMDGPUDetector::find_hwmon_dir(const std::string& card_path) {
    for (const auto& entry : std::filesystem::directory_iterator(card_path + "/device/hwmon")) {
        if (std::filesystem::is_directory(entry.path())) {
            return entry.path().string();
        }
    }
    return "";
}

bool AMDGPUDetector::is_amd_gpu(const std::string& path) {
    constexpr std::string_view amd_vendor_id = "0x1002";
    std::string vendor = read_file(path + "/device/vendor");
    debug_print("Checking vendor: " + vendor + " for path: " + path);
    return vendor.find(amd_vendor_id) != std::string::npos;
}

AMDGPUDetector::AMDGPUDetector() : initialized_(false) {
    debug_print("Initializing AMD GPU detector");
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/drm")) {
            std::string path = entry.path().string();
            if (path.find("card") != std::string::npos && std::filesystem::exists(path + "/device/vendor")) {
                debug_print("Found GPU device: " + path);
                if (is_amd_gpu(path)) {
                    gpu_paths_.push_back(path);
                    initialized_ = true;
                    debug_print("Added AMD GPU: " + path);
                }
            }
        }
        debug_print("Found " + std::to_string(gpu_paths_.size()) + " AMD GPUs");
    } catch (const std::exception& e) {
        debug_print("Error during initialization: " + std::string(e.what()));
    }
}

bool AMDGPUDetector::is_available() const {
    return initialized_;
}

std::vector<GPUInfo> AMDGPUDetector::get_gpu_info() const {
    std::vector<GPUInfo> result;
    if (!initialized_) return result;

    for (const auto& path : gpu_paths_) {
        GPUInfo info;
        
        // Get GPU index
        // FIXME: rewrite this without regex
        std::smatch match;
        std::regex card_regex("card(\\d+)");
        std::string card_name = std::filesystem::path(path).filename();
        if (std::regex_search(card_name, match, card_regex)) {
            info.index = std::stoi(match[1]);
        }

        // Get GPU name
        info.name = read_file(path + "/device/product_name");
        if (info.name.empty()) {
            info.name = "AMD GPU " + std::to_string(info.index);
        }

        // Get GPU utilization
        std::string gpu_busy = read_file(path + "/device/gpu_busy_percent");
        if (!gpu_busy.empty()) {
            info.utilization_percent = std::stod(gpu_busy);
        }

        // Get memory info
        constexpr uint64_t mb_to_bytes = 1024 * 1024;
        std::string vram_total = read_file(path + "/device/mem_info_vram_total");
        std::string vram_used = read_file(path + "/device/mem_info_vram_used");
        if (!vram_total.empty() && !vram_used.empty()) {
            info.total_memory_mb = std::stoull(vram_total) / mb_to_bytes;
            info.used_memory_mb = std::stoull(vram_used) / mb_to_bytes;
        }

        // Get temperature
        std::string hwmon_dir = find_hwmon_dir(path);
        if (!hwmon_dir.empty()) {
            std::string temp = read_file(hwmon_dir + "/temp1_input");
            if (!temp.empty()) {
                info.temperature_celsius = std::stoi(temp) / 1000.0;
            }
        }

        result.push_back(std::move(info));
    }
    return result;
}

std::optional<std::vector<GPUProcessInfo>> AMDGPUDetector::get_process_info(const std::string& process_name) const {
    std::vector<GPUProcessInfo> result;
    if (!initialized_) {
        debug_print("AMD GPU not initialized");
        return std::nullopt;
    }

    debug_print("Searching for process: " + process_name);

    // Scan /proc for the process
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!std::filesystem::is_directory(entry.path())) continue;
        
        std::string pid_str = entry.path().filename().string();
        if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;
        
        // Check process name in multiple locations
        bool process_match = false;
        uint32_t pid = std::stoul(pid_str);

        std::string comm = read_file(entry.path().string() + "/comm");
        if (!comm.empty() && comm.find(process_name) != std::string::npos) {
            process_match = true;
            debug_print("Found matching process: " + comm + " (PID: " + pid_str + ")");
        }

        if (!process_match) {
            std::string cmdline = read_file(entry.path().string() + "/cmdline");
            if (!cmdline.empty() && cmdline.find(process_name) != std::string::npos) {
                process_match = true;
                debug_print("Found matching process in cmdline: " + cmdline + " (PID: " + pid_str + ")");
            }
        }

        if (process_match) {
            bool uses_gpu = false;
            std::string gpu_path;

            // Check maps for GPU usage
            std::string maps_path = entry.path().string() + "/maps";
            std::ifstream maps_file(maps_path);
            std::string line;
            while (std::getline(maps_file, line)) {
                if (line.find("/dev/dri/") != std::string::npos || line.find("amdgpu") != std::string::npos || line.find("radeon") != std::string::npos) {
                    uses_gpu = true;
                    gpu_path = line;
                    debug_print("Process uses GPU: " + line);
                    break;
                }
            }

            // Check file descriptors
            if (!uses_gpu) {
                std::string fd_path = entry.path().string() + "/fd";
                if (std::filesystem::exists(fd_path)) {
                    for (const auto& fd : std::filesystem::directory_iterator(fd_path)) {
                        if (std::filesystem::is_symlink(fd.path())) {
                            std::error_code ec;
                            std::string target = std::filesystem::read_symlink(fd.path(), ec).string();
                            if (!ec && (target.find("/dev/dri/") != std::string::npos || target.find("amdgpu") != std::string::npos)) {
                                uses_gpu = true;
                                gpu_path = target;
                                debug_print("Process uses GPU (fd): " + target);
                                break;
                            }
                        }
                    }
                }
            }

            if (uses_gpu) {
                GPUProcessInfo proc_info;
                proc_info.pid = pid;
                proc_info.process_name = comm;
                
                // Calculate total GPU memory used by this process from maps
                uint64_t total_gpu_mem = 0;
                std::string maps_path = entry.path().string() + "/maps";
                std::ifstream maps_file(maps_path);
                while (std::getline(maps_file, line)) {
                    if (line.find("/dev/dri/renderD128") != std::string::npos) {
                        // Parse memory region size
                        size_t dash_pos = line.find('-');
                        if (dash_pos != std::string::npos) {
                            std::string start_addr = line.substr(0, dash_pos);
                            std::string end_addr = line.substr(dash_pos + 1, line.find(' ') - dash_pos - 1);
                            try {
                                total_gpu_mem += (std::stoull(end_addr, nullptr, 16) - std::stoull(start_addr, nullptr, 16));
                            // Just skip the cycle
                            } catch (...) {}
                        }
                    }
                }
                constexpr uint64_t mb_to_bytes = 1024 * 1024;
                proc_info.memory_usage_mb = total_gpu_mem / mb_to_bytes;
                debug_print("Process GPU memory: " + std::to_string(proc_info.memory_usage_mb) + " MB");

                // Try to get GPU index and usage
                for (const auto& path : gpu_paths_) {
                    if (gpu_path.find(std::filesystem::path(path).filename().string()) != std::string::npos) {
                        std::smatch match;
                        std::regex card_regex("card(\\d+)");
                        std::string card_name = std::filesystem::path(path).filename();
                        if (std::regex_search(card_name, match, card_regex)) {
                            proc_info.gpu_index = std::stoi(match[1]);
                            debug_print("Process using GPU " + std::to_string(proc_info.gpu_index));

                            // Get total VRAM usage percentage
                            std::string vram_used = read_file(path + "/device/mem_info_vram_used");
                            std::string vram_total = read_file(path + "/device/mem_info_vram_total");
                            if (!vram_used.empty() && !vram_total.empty()) {
                                try {
                                    float used_mb = std::stoull(vram_used) / mb_to_bytes;
                                    float total_mb = std::stoull(vram_total) / mb_to_bytes;
                                    float usage_percent = (used_mb / total_mb) * 100.0f;
                                    proc_info.gpu_usage_percent = usage_percent;
                                    debug_print("VRAM usage: " + std::to_string(used_mb) + "MB / " + 
                                              std::to_string(total_mb) + "MB (" + 
                                              std::to_string(usage_percent) + "%)");
                                } catch (...) {
                                    proc_info.gpu_usage_percent = 0;
                                }
                            }
                        }
                        break;
                    }
                }

                result.push_back(std::move(proc_info));
            }
        }
    }

    debug_print("Found " + std::to_string(result.size()) + " matching processes");
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<std::vector<GPUProcessInfo>> AMDGPUDetector::get_process_info(uint32_t pid) const {
    std::string comm_path = "/proc/" + std::to_string(pid) + "/comm";
    if (std::filesystem::exists(comm_path)) {
        return get_process_info(read_file(comm_path));
    }
    return std::nullopt;
}

std::optional<GPUInfo> AMDGPUDetector::get_gpu_info(uint32_t gpu_index) const {
    auto all_gpus = get_gpu_info();
    for (const auto& gpu : all_gpus) {
        if (gpu.index == gpu_index) {
            return gpu;
        }
    }
    return std::nullopt;
}

} // namespace hw_monitor
