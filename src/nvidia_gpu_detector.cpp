#include "nvidia_gpu_detector.hpp"
#include <cstring>
#include <array>
#include <fstream>
#include <algorithm> // for std::transform
#include <cctype>    // for std::tolower
#include <iostream> // for debug output
#include <thread>    // for std::this_thread
#include <chrono>    // for std::chrono
#include <unordered_map>
#include <filesystem>
#include <sstream>

namespace hw_monitor {

NvidiaGPUDetector::NvidiaGPUDetector() : initialized_(false), nvml_handle_(nullptr) {
    if (!check_nvidia_gpu()) {
        debug_print("No NVIDIA GPU found");
        return;
    }

    debug_print("NVIDIA GPU found, trying to load NVML");

    #ifdef __linux__
    // Try multiple possible library names
    const char* lib_names[] = {
        "libnvidia-ml.so.1",
        "libnvidia-ml.so",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1",
        "/usr/lib/x86_64-linux-gnu/libnvidia-ml.so"
    };

    for (const auto& lib_name : lib_names) {
        nvml_handle_ = dlopen(lib_name, RTLD_LAZY);
        if (nvml_handle_) {
            debug_print("Loaded NVML library: " + std::string(lib_name));
            break;
        }
    }

    if (!nvml_handle_) {
        debug_print("Failed to load NVML library: " + std::string(dlerror()));
        return;
    }
    #else
    nvml_handle_ = LoadLibrary("nvml.dll");
    #endif

    if (nvml_handle_) {
        if (!load_functions()) {
            debug_print("Failed to load NVML functions");
            return;
        }

        if (nvmlInit_v2_ptr && nvmlInit_v2_ptr() == NVML_SUCCESS) {
            initialized_ = true;
            debug_print("NVML initialized successfully");

            // Print available devices
            unsigned int device_count = 0;
            if (nvmlDeviceGetCount_v2_ptr(&device_count) == NVML_SUCCESS) {
                debug_print("Found " + std::to_string(device_count) + " NVIDIA devices");
            }
        } else {
            debug_print("Failed to initialize NVML");
        }
    }
}

NvidiaGPUDetector::~NvidiaGPUDetector() {
    if (initialized_ && nvmlShutdown_ptr) {
        nvmlShutdown_ptr();
    }
    if (nvml_handle_) {
        #ifdef __linux__
        dlclose(nvml_handle_);
        #else
        FreeLibrary(static_cast<HMODULE>(nvml_handle_));
        #endif
    }
}

bool NvidiaGPUDetector::check_nvidia_gpu() const {
    std::ifstream version_file("/proc/driver/nvidia/version");
    return version_file.good();
}

std::string NvidiaGPUDetector::read_file(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string content;
    std::getline(file, content);
    return content;
}

void NvidiaGPUDetector::debug_print(const std::string& msg) const {
    std::cerr << "NVIDIA Debug: " << msg << std::endl;
}

bool NvidiaGPUDetector::is_available() const {
    return initialized_;
}

std::vector<GPUInfo> NvidiaGPUDetector::get_gpu_info() const {
    std::vector<GPUInfo> result;
    if (!initialized_) return result;

    unsigned int device_count = 0;
    if (nvmlDeviceGetCount_v2_ptr(&device_count) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < device_count; i++) {
            nvmlDevice_t device;
            if (nvmlDeviceGetHandleByIndex_v2_ptr(i, &device) == NVML_SUCCESS) {
                GPUInfo gpu_info;
                
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                if (nvmlDeviceGetName_ptr(device, name, NVML_DEVICE_NAME_BUFFER_SIZE) == NVML_SUCCESS) {
                    gpu_info.name = name;
                }
                gpu_info.index = i;
                
                nvmlMemory_t memory;
                if (nvmlDeviceGetMemoryInfo_ptr(device, &memory) == NVML_SUCCESS) {
                    gpu_info.total_memory_mb = memory.total / (1024.0 * 1024.0);
                    gpu_info.used_memory_mb = memory.used / (1024.0 * 1024.0);
                }
                
                unsigned int temperature;
                if (nvmlDeviceGetTemperature_ptr(device, NVML_TEMPERATURE_GPU, &temperature) == NVML_SUCCESS) {
                    gpu_info.temperature_celsius = temperature;
                }
                
                nvmlUtilization_t utilization;
                if (nvmlDeviceGetUtilizationRates_ptr(device, &utilization) == NVML_SUCCESS) {
                    gpu_info.utilization_percent = utilization.gpu;
                }
                
                // Get process information
                gpu_info.processes = get_process_info_for_device(device);
                
                result.push_back(std::move(gpu_info));
            }
        }
    }
    return result;
}

std::string NvidiaGPUDetector::get_process_name(uint32_t pid) const {
    // Try /proc/[pid]/comm first
    std::string comm_path = "/proc/" + std::to_string(pid) + "/comm";
    std::string name = read_file(comm_path);
    if (!name.empty()) {
        if (name.back() == '\n') name.pop_back();
        return name;
    }

    // Try /proc/[pid]/cmdline as fallback
    std::string cmdline_path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::string cmdline = read_file(cmdline_path);
    if (!cmdline.empty()) {
        // Extract executable name from path
        size_t pos = cmdline.find_last_of('/');
        if (pos != std::string::npos) {
            return cmdline.substr(pos + 1);
        }
        return cmdline;
    }

    return "";
}

std::optional<std::vector<GPUProcessInfo>> NvidiaGPUDetector::get_process_info(const std::string& process_name) const {
    std::vector<GPUProcessInfo> result;
    if (!initialized_) {
        debug_print("NVML not initialized");
        return std::nullopt;
    }

    debug_print("Searching for process: " + process_name);

    unsigned int device_count = 0;
    if (nvmlDeviceGetCount_v2_ptr(&device_count) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < device_count; i++) {
            nvmlDevice_t device;
            if (nvmlDeviceGetHandleByIndex_v2_ptr(i, &device) == NVML_SUCCESS) {
                // Get compute processes
                std::array<nvmlProcessInfo_t, 128> compute_processes;
                unsigned int compute_count = compute_processes.size();
                
                if (nvmlDeviceGetComputeRunningProcesses_ptr(device, &compute_count, compute_processes.data()) == NVML_SUCCESS) {
                    debug_print("Found " + std::to_string(compute_count) + " compute processes on GPU " + std::to_string(i));
                    for (unsigned int p = 0; p < compute_count; p++) {
                        std::string current_name = get_process_name(compute_processes[p].pid);
                        if (current_name.find(process_name) != std::string::npos) {
                            GPUProcessInfo proc_info;
                            proc_info.pid = compute_processes[p].pid;
                            proc_info.process_name = current_name;
                            proc_info.memory_usage_mb = compute_processes[p].usedGpuMemory / (1024.0 * 1024.0);
                            proc_info.gpu_index = i;
                            result.push_back(std::move(proc_info));
                        }
                    }
                }

                // Get graphics processes
                std::array<nvmlProcessInfo_t, 128> graphics_processes;
                unsigned int graphics_count = graphics_processes.size();
                
                if (nvmlDeviceGetGraphicsRunningProcesses_ptr(device, &graphics_count, graphics_processes.data()) == NVML_SUCCESS) {
                    debug_print("Found " + std::to_string(graphics_count) + " graphics processes on GPU " + std::to_string(i));
                    for (unsigned int p = 0; p < graphics_count; p++) {
                        std::string current_name = get_process_name(graphics_processes[p].pid);
                        debug_print("Graphics process: " + current_name + " (PID: " + std::to_string(graphics_processes[p].pid) + ")");
                        
                        // Check if we already have this process from compute processes
                        bool already_added = false;
                        for (const auto& existing : result) {
                            if (existing.pid == graphics_processes[p].pid) {
                                already_added = true;
                                break;
                            }
                        }

                        if (!already_added && current_name.find(process_name) != std::string::npos) {
                            GPUProcessInfo proc_info;
                            proc_info.pid = graphics_processes[p].pid;
                            proc_info.process_name = current_name;
                            proc_info.memory_usage_mb = graphics_processes[p].usedGpuMemory / (1024.0 * 1024.0);
                            proc_info.gpu_index = i;
                            result.push_back(std::move(proc_info));
                        }
                    }
                }

                // Get utilization for all found processes
                if (!result.empty() && nvmlDeviceGetProcessUtilization_ptr) {
                    std::array<nvmlProcessUtilizationSample_t, 128> samples;
                    unsigned int sample_count = samples.size();
                    unsigned long long lastSeenTimeStamp = 0;

                    nvmlReturn_t ret = nvmlDeviceGetProcessUtilization_ptr(device, samples.data(), &sample_count, lastSeenTimeStamp);
                    debug_print("Initial utilization query return code: " + std::to_string(ret));
                    if (ret == NVML_ERROR_NOT_SUPPORTED) {
                        debug_print("Process utilization query not supported on this GPU/driver");
                    } else if (ret == NVML_ERROR_INSUFFICIENT_PERMISSIONS) {
                        debug_print("Insufficient permissions to query process utilization");
                    }

                    if (ret == NVML_SUCCESS) {
                        debug_print("Got utilization data for " + std::to_string(sample_count) + " processes");
                        std::unordered_map<unsigned int, float> pidToUtil;
                        for (unsigned int s = 0; s < sample_count; s++) {
                            pidToUtil[samples[s].pid] = samples[s].smUtil;
                            debug_print("Process " + std::to_string(samples[s].pid) + 
                                       " utilization: " + std::to_string(samples[s].smUtil));
                        }

                        debug_print("Total processes with utilization data: " + std::to_string(pidToUtil.size()));
                        for (const auto& [pid, util] : pidToUtil) {
                            debug_print("PID " + std::to_string(pid) + " has utilization " + std::to_string(util));
                        }

                        for (auto& proc_info : result) {
                            for (unsigned int s = 0; s < sample_count; s++) {
                                if (samples[s].pid == proc_info.pid) {
                                    proc_info.gpu_usage_percent = samples[s].smUtil;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    debug_print("Found " + std::to_string(result.size()) + " matching processes");
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<std::vector<GPUProcessInfo>> NvidiaGPUDetector::get_process_info(uint32_t pid) const {
    std::vector<GPUProcessInfo> result;
    if (!initialized_) return std::nullopt;

    auto all_processes = get_all_processes();
    for (const auto& proc : all_processes) {
        if (proc.pid == pid) {
            result.push_back(proc);
        }
    }
    
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<GPUInfo> NvidiaGPUDetector::get_gpu_info(uint32_t gpu_index) const {
    auto all_gpus = get_gpu_info();
    for (const auto& gpu : all_gpus) {
        if (gpu.index == gpu_index) {
            return gpu;
        }
    }
    return std::nullopt;
}

bool NvidiaGPUDetector::load_functions() {
    #ifdef __linux__
    #define LOAD_FUNC(name) name##_ptr = reinterpret_cast<decltype(name##_ptr)>(dlsym(nvml_handle_, #name))
    #else
    #define LOAD_FUNC(name) name##_ptr = reinterpret_cast<decltype(name##_ptr)>(GetProcAddress(static_cast<HMODULE>(nvml_handle_), #name))
    #endif

    LOAD_FUNC(nvmlInit_v2);
    LOAD_FUNC(nvmlShutdown);
    LOAD_FUNC(nvmlDeviceGetCount_v2);
    LOAD_FUNC(nvmlDeviceGetHandleByIndex_v2);
    LOAD_FUNC(nvmlDeviceGetName);
    LOAD_FUNC(nvmlDeviceGetMemoryInfo);
    LOAD_FUNC(nvmlDeviceGetTemperature);
    LOAD_FUNC(nvmlDeviceGetUtilizationRates);
    LOAD_FUNC(nvmlDeviceGetComputeRunningProcesses);
    LOAD_FUNC(nvmlDeviceGetGraphicsRunningProcesses);
    LOAD_FUNC(nvmlDeviceGetProcessUtilization);

    #undef LOAD_FUNC

    return nvmlInit_v2_ptr && nvmlInit_v2_ptr() == NVML_SUCCESS;
}

std::vector<GPUProcessInfo> NvidiaGPUDetector::get_process_info_for_device(nvmlDevice_t device) const {
    std::vector<GPUProcessInfo> result;
    
    // Get initial utilization samples with a larger sample buffer
    unsigned int sampleCount = 32;  // Increase buffer size
    std::vector<nvmlProcessUtilizationSample_t> samples(sampleCount);
    uint64_t lastSeenTimeStamp = 0;
    
    // Get first sample
    nvmlReturn_t ret = nvmlDeviceGetProcessUtilization_ptr(device, samples.data(), &sampleCount, lastSeenTimeStamp);
    if (ret != NVML_SUCCESS) {
        debug_print("Failed to get initial process utilization: " + std::to_string(ret));
    } else {
        debug_print("Got initial samples: " + std::to_string(sampleCount));
    }

    // Store the timestamp from first sample
    if (sampleCount > 0) {
        lastSeenTimeStamp = samples[0].timeStamp;
    }

    // Wait a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get second sample
    unsigned int newSampleCount = 32;
    std::vector<nvmlProcessUtilizationSample_t> newSamples(newSampleCount);
    ret = nvmlDeviceGetProcessUtilization_ptr(device, newSamples.data(), &newSampleCount, lastSeenTimeStamp);
    if (ret != NVML_SUCCESS) {
        debug_print("Failed to get final process utilization: " + std::to_string(ret));
    } else {
        debug_print("Got final samples: " + std::to_string(newSampleCount));
    }

    // Create a map of PID to utilization
    std::unordered_map<unsigned int, float> pidToUtil;
    for (unsigned int i = 0; i < newSampleCount; i++) {
        pidToUtil[newSamples[i].pid] = newSamples[i].smUtil;
        debug_print("Process " + std::to_string(newSamples[i].pid) + 
                   " utilization: " + std::to_string(newSamples[i].smUtil));
    }

    // Get compute processes
    std::array<nvmlProcessInfo_t, 128> compute_processes;
    unsigned int compute_count = compute_processes.size();
    
    if (nvmlDeviceGetComputeRunningProcesses_ptr(device, &compute_count, compute_processes.data()) == NVML_SUCCESS) {
        debug_print("Found " + std::to_string(compute_count) + " compute processes");
        for (unsigned int p = 0; p < compute_count; p++) {
            GPUProcessInfo proc_info;
            proc_info.pid = compute_processes[p].pid;
            proc_info.process_name = get_process_name(proc_info.pid);
            proc_info.memory_usage_mb = compute_processes[p].usedGpuMemory / (1024.0 * 1024.0);
            
            // Get GPU utilization from our map
            proc_info.gpu_usage_percent = pidToUtil.count(proc_info.pid) ? 
                                        pidToUtil[proc_info.pid] : 0.0f;

            result.push_back(std::move(proc_info));
        }
    }

    // Get graphics processes
    std::array<nvmlProcessInfo_t, 128> graphics_processes;
    unsigned int graphics_count = graphics_processes.size();
    
    if (nvmlDeviceGetGraphicsRunningProcesses_ptr(device, &graphics_count, graphics_processes.data()) == NVML_SUCCESS) {
        debug_print("Found " + std::to_string(graphics_count) + " graphics processes");
        for (unsigned int p = 0; p < graphics_count; p++) {
            // Check if process is already in result
            bool found = false;
            for (const auto& existing : result) {
                if (existing.pid == graphics_processes[p].pid) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                GPUProcessInfo proc_info;
                proc_info.pid = graphics_processes[p].pid;
                proc_info.process_name = get_process_name(proc_info.pid);
                proc_info.memory_usage_mb = graphics_processes[p].usedGpuMemory / (1024.0 * 1024.0);
                
                // Get GPU utilization from our map
                proc_info.gpu_usage_percent = pidToUtil.count(proc_info.pid) ? 
                                            pidToUtil[proc_info.pid] : 0.0f;

                // Debug output
                debug_print("Process: " + proc_info.process_name + 
                          " (PID: " + std::to_string(proc_info.pid) + 
                          ") Memory: " + std::to_string(proc_info.memory_usage_mb) + 
                          "MB GPU: " + std::to_string(proc_info.gpu_usage_percent) + "%");

                result.push_back(std::move(proc_info));
            }
        }
    }
    
    return result;
}

std::vector<GPUProcessInfo> NvidiaGPUDetector::get_all_processes() const {
    std::vector<GPUProcessInfo> result;
    
    unsigned int device_count = 0;
    if (nvmlDeviceGetCount_v2_ptr(&device_count) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < device_count; i++) {
            nvmlDevice_t device;
            if (nvmlDeviceGetHandleByIndex_v2_ptr(i, &device) == NVML_SUCCESS) {
                auto device_processes = get_process_info_for_device(device);
                for (auto& proc : device_processes) {
                    proc.gpu_index = i;
                }
                result.insert(result.end(), device_processes.begin(), device_processes.end());
            }
        }
    }
    
    return result;
}

} // namespace hw_monitor 
