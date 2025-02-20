#include "gpu_detector.hpp"
#include "ram_detector.hpp"
#include "storage_detector.hpp"
#include "network_detector.hpp"
#include "cpu_detector.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>

void print_usage() {
    std::cout << "Usage: hw_monitor [process_name]\n"
              << "If process_name is not provided, shows overall system information\n"
              << "Example: hw_monitor chrome" << std::endl;
}

void print_overall_gpu_info(const hw_monitor::GPUDetector& detector) {
    std::cout << "\nGPU Information:\n"
              << "----------------------------------------\n";
    auto gpus = detector.get_gpu_info();
    for (const auto& gpu : gpus) {
        std::cout << "GPU " << gpu.index << ": " << gpu.name << "\n"
                 << "  Temperature: " << std::fixed << std::setprecision(1)
                 << gpu.temperature_celsius << "°C\n"
                 << "  Memory: " << gpu.used_memory_mb << "MB / "
                 << gpu.total_memory_mb << "MB\n"
                 << "  Utilization: " << gpu.utilization_percent << "%\n";
    }
}

void print_overall_ram_info(const hw_monitor::RAMDetector& detector) {
    std::cout << "\nRAM Information:\n"
              << "----------------------------------------\n";
    auto ram = detector.get_ram_info();
    std::cout << "Total Memory: " << std::fixed << std::setprecision(1)
              << ram.total_memory_mb << " MB\n"
              << "Used Memory: " << ram.used_memory_mb << " MB\n"
              << "Free Memory: " << ram.free_memory_mb << " MB\n"
              << "Usage: " << ram.usage_percent << "%\n";
}

void print_overall_storage_info(const hw_monitor::StorageDetector& detector) {
    std::cout << "\nStorage Information:\n"
              << "----------------------------------------\n";
    auto storage = detector.get_storage_info();
    for (const auto& device : storage) {
        std::cout << "Device: " << device.device_name << " (" << device.mount_point << ")\n"
                 << "  Filesystem: " << device.filesystem_type << "\n"
                 << "  Total: " << device.total_bytes / (1024.0 * 1024 * 1024) << " GB\n"
                 << "  Used: " << device.used_bytes / (1024.0 * 1024 * 1024) << " GB\n"
                 << "  Available: " << device.available_bytes / (1024.0 * 1024 * 1024) << " GB\n"
                 << "  Usage: " << device.usage_percent << "%\n";
    }
}

void print_overall_network_info(const hw_monitor::NetworkDetector& detector) {
    std::cout << "\nNetwork Information:\n"
              << "----------------------------------------\n";
    auto interfaces = detector.get_interface_info();
    for (const auto& iface : interfaces) {
        std::cout << "Interface: " << iface.name << "\n"
                 << "  IP: " << iface.ip_address << "\n"
                 << "  Status: " << (iface.is_up ? "Up" : "Down") << "\n"
                 << "  Receive Rate: " << std::fixed << std::setprecision(1)
                 << iface.receive_bytes_per_sec / (1024.0 * 1024.0) << " MB/s\n"
                 << "  Transmit Rate: " << iface.transmit_bytes_per_sec / (1024.0 * 1024.0)
                 << " MB/s\n";
    }
}

void print_overall_cpu_info(const hw_monitor::CPUDetector& detector) {
    std::cout << "\nCPU Information:\n"
              << "----------------------------------------\n";
    auto cpu = detector.get_cpu_info();
    std::cout << "Total CPU Usage: " << std::fixed << std::setprecision(1)
              << cpu.total_usage_percent << "%\n"
              << "Cores: " << cpu.core_count << "\n"
              << "Threads: " << cpu.thread_count << "\n"
              << "Average Frequency: " << cpu.average_frequency_mhz << " MHz\n"
              << "Average Temperature: " << cpu.average_temperature_celsius << "°C\n\n"
              << "Per-Core Usage:\n";

    for (size_t i = 0; i < cpu.cores.size(); ++i) {
        std::cout << "Core " << i << ": " << cpu.usage_per_core[i] << "%\n";
    }
}

void print_process_gpu_info(const hw_monitor::GPUDetector& detector, const std::string& process_name) {
    auto gpu_processes = detector.get_process_info(process_name);
    if (gpu_processes) {
        std::cout << "\nGPU Usage:\n"
                  << "----------------------------------------\n";
        for (const auto& proc : *gpu_processes) {
            std::cout << "GPU " << proc.gpu_index << ":\n"
                     << "  Memory Usage: " << std::fixed << std::setprecision(1)
                     << proc.memory_usage_mb << " MB\n";
            if (proc.gpu_usage_percent >= 0 && proc.gpu_usage_percent <= 100) {
                std::cout << "  GPU Utilization: " << proc.gpu_usage_percent << "%\n";
            } else {
                std::cout << "  GPU Utilization: N/A\n";
            }
        }
    }
}

void print_process_ram_info(const hw_monitor::RAMDetector& detector, const std::string& process_name) {
    auto ram_processes = detector.get_process_info(process_name);
    if (ram_processes) {
        std::cout << "\nRAM Usage:\n"
                  << "----------------------------------------\n";
        for (const auto& proc : *ram_processes) {
            std::cout << "PID " << proc.pid << ":\n"
                     << "  Physical Memory: " << std::fixed << std::setprecision(1)
                     << proc.memory_usage_mb << " MB\n"
                     << "  Virtual Memory: " << proc.virtual_memory_mb << " MB\n"
                     << "  Shared Memory: " << proc.shared_memory_mb << " MB\n"
                     << "  Memory Usage: " << proc.memory_percent << "%\n";
        }
    }
}

void print_process_storage_info(const hw_monitor::StorageDetector& detector, const std::string& process_name) {
    auto storage_processes = detector.get_process_info(process_name);
    if (storage_processes) {
        std::cout << "\nStorage Usage:\n"
                  << "----------------------------------------\n";
        for (const auto& proc : *storage_processes) {
            std::cout << "PID " << proc.pid << ":\n"
                     << "  Read Rate: " << std::fixed << std::setprecision(1)
                     << proc.read_bytes_per_sec / (1024.0 * 1024.0) << " MB/s\n"
                     << "  Write Rate: " << proc.write_bytes_per_sec / (1024.0 * 1024.0) << " MB/s\n"
                     << "  Open Files: " << proc.open_files << "\n"
                     << "  Main Device: " << proc.main_device << "\n";
        }
    }
}

void print_process_network_info(const hw_monitor::NetworkDetector& detector, const std::string& process_name) {
    auto net_processes = detector.get_process_info(process_name);
    if (net_processes) {
        std::cout << "\nNetwork Usage:\n"
                  << "----------------------------------------\n";
        for (const auto& proc : *net_processes) {
            std::cout << "PID " << proc.pid << ":\n"
                     << "  Receive Rate: " << std::fixed << std::setprecision(1)
                     << proc.receive_bytes_per_sec / (1024.0 * 1024.0) << " MB/s\n"
                     << "  Transmit Rate: " << proc.transmit_bytes_per_sec / (1024.0 * 1024.0) << " MB/s\n"
                     << "  Active Connections: " << proc.active_connections << "\n"
                     << "  Used Ports: ";
            for (auto port : proc.ports) {
                std::cout << port << " ";
            }
            std::cout << "\n";
        }
    }
}

void print_process_cpu_info(const hw_monitor::CPUDetector& detector, const std::string& process_name) {
    auto cpu_processes = detector.get_process_info(process_name);
    if (cpu_processes) {
        std::cout << "\nCPU Usage:\n"
                  << "----------------------------------------\n";
        for (const auto& proc : *cpu_processes) {
            std::cout << "PID " << proc.pid << ":\n"
                     << "  CPU Usage: " << std::fixed << std::setprecision(1)
                     << proc.cpu_usage_percent << "%\n"
                     << "  Threads: " << proc.thread_count << "\n"
                     << "  State: " << proc.state << "\n"
                     << "  Nice Value: " << proc.nice << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Initialize detectors
    hw_monitor::GPUDetector& gpu_detector = hw_monitor::GPUDetector::instance();
    hw_monitor::RAMDetector ram_detector;
    hw_monitor::StorageDetector storage_detector;
    hw_monitor::NetworkDetector network_detector;
    hw_monitor::CPUDetector cpu_detector;

    if (argc == 1) {
        // Show overall system information
        std::cout << "System Resource Monitor\n"
                  << "======================================" << std::endl;

        print_overall_cpu_info(cpu_detector);
        print_overall_gpu_info(gpu_detector);
        print_overall_ram_info(ram_detector);
        print_overall_storage_info(storage_detector);
        print_overall_network_info(network_detector);
    }
    else if (argc == 2) {
        // Show process-specific information
        std::string process_name = argv[1];
        std::cout << "Monitoring resource usage for process: " << process_name << std::endl;

        print_process_cpu_info(cpu_detector, process_name);
        print_process_gpu_info(gpu_detector, process_name);
        print_process_ram_info(ram_detector, process_name);
        print_process_storage_info(storage_detector, process_name);
        print_process_network_info(network_detector, process_name);
    }
    else {
        print_usage();
        return 1;
    }

    return 0;
}
