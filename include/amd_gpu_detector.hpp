#pragma once

#include "gpu_detector.hpp"

#ifdef HAS_ADL_SUPPORT
// AMD types
typedef void* ADL_CONTEXT_HANDLE;
typedef void* (*ADL_MAIN_MALLOC_CALLBACK)(int);
struct AdapterInfo;
typedef AdapterInfo* LPAdapterInfo;
struct ADLMemoryInfo;
struct ADLODParameters;
#define ADL_OK 0
#endif

namespace hw_monitor {

class AMDGPUDetector : public IGPUDetector {
public:
    AMDGPUDetector();
    ~AMDGPUDetector() override;

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