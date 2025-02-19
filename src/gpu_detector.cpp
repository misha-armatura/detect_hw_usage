#include "gpu_detector.hpp"
#include "nvidia_gpu_detector.hpp"
#include "amd_gpu_detector.hpp"

namespace hw_monitor {

GPUDetector& GPUDetector::instance() {
    static GPUDetector instance;
    return instance;
}

GPUDetector::GPUDetector() {
    // Try to create NVIDIA implementation
    auto nvidia_impl = std::make_unique<NvidiaGPUDetector>();
    if (nvidia_impl->is_available()) {
        implementations_.push_back(std::move(nvidia_impl));
    }

    // Try to create AMD implementation
    auto amd_impl = std::make_unique<AMDGPUDetector>();
    if (amd_impl->is_available()) {
        implementations_.push_back(std::move(amd_impl));
    }
}

std::vector<GPUInfo> GPUDetector::get_gpu_info() const {
    std::vector<GPUInfo> result;
    for (const auto& impl : implementations_) {
        auto info = impl->get_gpu_info();
        result.insert(result.end(), info.begin(), info.end());
    }
    return result;
}

std::optional<std::vector<GPUProcessInfo>> GPUDetector::get_process_info(const std::string& process_name) const {
    std::vector<GPUProcessInfo> result;
    for (const auto& impl : implementations_) {
        if (auto info = impl->get_process_info(process_name)) {
            result.insert(result.end(), info->begin(), info->end());
        }
    }
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<std::vector<GPUProcessInfo>> GPUDetector::get_process_info(uint32_t pid) const {
    std::vector<GPUProcessInfo> result;
    for (const auto& impl : implementations_) {
        if (auto info = impl->get_process_info(pid)) {
            result.insert(result.end(), info->begin(), info->end());
        }
    }
    return result.empty() ? std::nullopt : std::make_optional(result);
}

std::optional<GPUInfo> GPUDetector::get_gpu_info(uint32_t gpu_index) const {
    for (const auto& impl : implementations_) {
        if (auto info = impl->get_gpu_info(gpu_index)) {
            return info;
        }
    }
    return std::nullopt;
}

} // namespace hw_monitor 