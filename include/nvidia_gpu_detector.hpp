#pragma once

#include "gpu_detector.hpp"
#include <array>
#include <map>

#ifdef __linux__
#include <dlfcn.h>
#endif

#ifdef HAS_NVML_SUPPORT
// NVIDIA types
typedef struct nvmlDevice_st* nvmlDevice_t;
typedef int nvmlReturn_t;

typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;

typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;

typedef struct {
    unsigned int pid;
    unsigned long long usedGpuMemory;
} nvmlProcessInfo_t;

typedef struct {
    unsigned int pid;
    unsigned long long timeStamp;
    unsigned int smUtil;
    unsigned int memUtil;
    unsigned int encUtil;
    unsigned int decUtil;
} nvmlProcessUtilizationSample_t;

enum nvmlTemperatureSensors_t { NVML_TEMPERATURE_GPU = 0 };
#define NVML_SUCCESS 0
#define NVML_DEVICE_NAME_BUFFER_SIZE 64
#endif

namespace hw_monitor {

class NvidiaGPUDetector : public IGPUDetector {
public:
    NvidiaGPUDetector();
    ~NvidiaGPUDetector() override;

    bool is_available() const override;
    std::vector<GPUInfo> get_gpu_info() const override;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const override;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const override;
    std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace hw_monitor 