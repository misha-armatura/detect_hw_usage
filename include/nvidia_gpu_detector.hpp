#pragma once

#include "gpu_detector.hpp"
#include <array>
#include <map>
#include <string>
#include <unordered_map>
#include <chrono>

#ifdef __linux__
#include <dlfcn.h>
#endif

#ifdef HAS_NVML_SUPPORT
/**
 * @brief NVIDIA Management Library (NVML) type definitions and constants
 */

/// Opaque handle to a device
typedef struct nvmlDevice_st* nvmlDevice_t;

/// PCI information structure
typedef struct {
    unsigned long long reserved[8];             ///< Reserved for future use
} nvmlPciInfo_t;

/// GPU utilization information
typedef struct {
    unsigned int gpu;                           ///< Percent of time GPU was active
    unsigned int memory;                        ///< Percent of time memory was being read/written
} nvmlUtilization_t;

/// GPU memory information
typedef struct {
    unsigned long long total;                   ///< Total installed memory
    unsigned long long free;                    ///< Unallocated memory
    unsigned long long used;                    ///< Allocated memory
} nvmlMemory_t;

/// Process information structure
typedef struct {
    unsigned int pid;                           ///< Process ID
    unsigned long long usedGpuMemory;           ///< GPU memory used by process in bytes
} nvmlProcessInfo_t;

/// Process utilization sample
typedef struct {
    unsigned int pid;                           ///< Process ID
    unsigned long long timeStamp;               ///< CPU timestamp in microseconds
    unsigned int smUtil;                        ///< SM (GPU) utilization percent
    unsigned int memUtil;                       ///< Memory utilization percent
    unsigned int encUtil;                       ///< Encoder utilization percent
    unsigned int decUtil;                       ///< Decoder utilization percent
} nvmlProcessUtilizationSample_t;

/// NVML function return codes
typedef enum nvmlReturn_enum {
    NVML_SUCCESS = 0,                           ///< Function completed successfully
    NVML_ERROR_UNINITIALIZED = 1,               ///< NVML was not first initialized
    NVML_ERROR_INVALID_ARGUMENT = 2,            ///< Invalid argument
    NVML_ERROR_NOT_SUPPORTED = 3,               ///< Feature not supported
    NVML_ERROR_NO_PERMISSION = 4,               ///< Insufficient permissions
    NVML_ERROR_INSUFFICIENT_PERMISSIONS = 4,    ///< Alias for NO_PERMISSION
    NVML_ERROR_ALREADY_INITIALIZED = 5,         ///< Already initialized
    NVML_ERROR_NOT_FOUND = 6,                   ///< Device not found
    NVML_ERROR_INSUFFICIENT_SIZE = 7,           ///< Buffer too small
    NVML_ERROR_INSUFFICIENT_POWER = 8,          ///< Power state constraint not met
    NVML_ERROR_DRIVER_NOT_LOADED = 9,           ///< NVIDIA driver not loaded
    NVML_ERROR_TIMEOUT = 10,                    ///< Timeout accessing device
    NVML_ERROR_IRQ_ISSUE = 11,                  ///< Interrupt issue with device
    NVML_ERROR_LIBRARY_NOT_FOUND = 12,          ///< NVML shared library not found
    NVML_ERROR_FUNCTION_NOT_FOUND = 13,         ///< Function not found in library
    NVML_ERROR_CORRUPTED_INFOROM = 14,          ///< InfoROM corrupted
    NVML_ERROR_GPU_IS_LOST = 15,                ///< GPU has fallen off the bus
    NVML_ERROR_RESET_REQUIRED = 16,             ///< GPU requires reset
    NVML_ERROR_OPERATING_SYSTEM = 17,           ///< Operating system error
    NVML_ERROR_LIB_RM_VERSION_MISMATCH = 18,    ///< RM version mismatch
    NVML_ERROR_IN_USE = 19,                     ///< Device is in use
    NVML_ERROR_MEMORY = 20,                     ///< Memory error
    NVML_ERROR_CORRUPTED_DATA = 21,             ///< Data corrupted
    NVML_ERROR_UNKNOWN = 999                    ///< Unknown error
} nvmlReturn_t;

/// Temperature sensor identifiers
typedef enum nvmlTemperatureSensors_enum {
    NVML_TEMPERATURE_GPU = 0                    ///< GPU core temperature
} nvmlTemperatureSensors_t;

/// Maximum length of device name string
#define NVML_DEVICE_NAME_BUFFER_SIZE 64

#endif

namespace hw_monitor {

/**
 * @brief NVIDIA GPU detection and monitoring implementation
 * 
 * This class provides NVIDIA-specific GPU monitoring functionality using the
 * NVIDIA Management Library (NVML). It supports querying GPU information,
 * memory usage, temperature, utilization, and process statistics.
 */
class NvidiaGPUDetector : public IGPUDetector {
public:
    /**
     * @brief Constructor - initializes NVML and loads required functions
     */
    NvidiaGPUDetector();

    /**
     * @brief Destructor - cleans up NVML resources
     */
    ~NvidiaGPUDetector();

    bool is_available() const override;
    std::vector<GPUInfo> get_gpu_info() const override;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(const std::string& process_name) const override;
    std::optional<std::vector<GPUProcessInfo>> get_process_info(uint32_t pid) const override;
    std::optional<GPUInfo> get_gpu_info(uint32_t gpu_index) const override;

private:
    bool initialized_;                          ///< Whether NVML was successfully initialized
    void* nvml_handle_;                         ///< Handle to the loaded NVML library

    // Function pointers for dynamic loading
    nvmlReturn_t (*nvmlInit_v2_ptr)();                                                                          ///< Initialize NVML library
    nvmlReturn_t (*nvmlShutdown_ptr)();                                                                         ///< Shutdown NVML library
    nvmlReturn_t (*nvmlDeviceGetCount_v2_ptr)(unsigned int*);                                                   ///< Get number of NVIDIA devices
    nvmlReturn_t (*nvmlDeviceGetHandleByIndex_v2_ptr)(unsigned int, nvmlDevice_t*);                             ///< Get handle to GPU at index
    nvmlReturn_t (*nvmlDeviceGetName_ptr)(nvmlDevice_t, char*, unsigned int);                                   ///< Get GPU name
    nvmlReturn_t (*nvmlDeviceGetMemoryInfo_ptr)(nvmlDevice_t, nvmlMemory_t*);                                   ///< Get memory information
    nvmlReturn_t (*nvmlDeviceGetTemperature_ptr)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int*);        ///< Get GPU temperature
    nvmlReturn_t (*nvmlDeviceGetUtilizationRates_ptr)(nvmlDevice_t, nvmlUtilization_t*);                        ///< Get utilization rates
    nvmlReturn_t (*nvmlDeviceGetComputeRunningProcesses_ptr)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*);  ///< Get compute processes
    nvmlReturn_t (*nvmlDeviceGetGraphicsRunningProcesses_ptr)(nvmlDevice_t, unsigned int*, nvmlProcessInfo_t*); ///< Get graphics processes
    nvmlReturn_t (*nvmlDeviceGetProcessUtilization_ptr)(nvmlDevice_t device,                                    ///< Get process utilization
        nvmlProcessUtilizationSample_t* utilization, unsigned int* processSamplesCount,
        unsigned long long lastSeenTimeStamp);

    /**
     * @brief Check if NVIDIA GPU is present in system
     * @return true if NVIDIA GPU is found
     */
    bool check_nvidia_gpu() const;

    /**
     * @brief Load NVML functions from the library
     * @return true if all required functions were loaded
     */
    bool load_functions();

    /**
     * @brief Read contents of a file
     * @param path Path to the file
     * @return Contents of the file as string
     */
    std::string read_file(const std::string& path) const;

    /**
     * @brief Print debug message
     * @param msg Message to print
     */
    void debug_print(const std::string& msg) const;

    /**
     * @brief Get process name from process ID
     * @param pid Process ID
     * @return Name of the process
     */
    std::string get_process_name(uint32_t pid) const;

    /**
     * @brief Get process information for a specific GPU
     * @param device Handle to the GPU device
     * @return Vector of process information
     */
    std::vector<GPUProcessInfo> get_process_info_for_device(nvmlDevice_t device) const;

    /**
     * @brief Get information about all GPU processes
     * @return Vector of all GPU process information
     */
    std::vector<GPUProcessInfo> get_all_processes() const;
};

} // namespace hw_monitor 