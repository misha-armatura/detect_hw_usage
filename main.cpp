#include "gpu_detector.hpp"
#include "ram_detector.hpp"
#include <iostream>
#include <iomanip>

void print_usage() {
    std::cout << "Usage: hw_monitor <process_name>\n"
              << "Example: hw_monitor chrome" << std::endl;
}

void print_gpu_info(const std::vector<hw_monitor::GPUProcessInfo>& gpu_processes) {
    std::cout << "\nGPU Usage:\n"
              << "----------------------------------------\n";
    for (const auto& proc : gpu_processes) {
        std::cout << "GPU " << proc.gpu_index << ":\n"
                 << "  Memory Usage: " << std::fixed << std::setprecision(1) 
                 << proc.memory_usage_mb << " MB\n"
                 << "  GPU Utilization: " << std::fixed << std::setprecision(1) 
                 << proc.gpu_usage_percent << "%\n";
    }
}

void print_ram_info(const std::vector<hw_monitor::RAMProcessInfo>& ram_processes) {
    std::cout << "\nRAM Usage:\n"
              << "----------------------------------------\n";
    for (const auto& proc : ram_processes) {
        std::cout << "PID " << proc.pid << ":\n"
                 << "  Physical Memory: " << std::fixed << std::setprecision(1) 
                 << proc.memory_usage_mb << " MB\n"
                 << "  Virtual Memory: " << std::fixed << std::setprecision(1) 
                 << proc.virtual_memory_mb << " MB\n"
                 << "  Shared Memory: " << std::fixed << std::setprecision(1) 
                 << proc.shared_memory_mb << " MB\n"
                 << "  Memory Usage: " << std::fixed << std::setprecision(1) 
                 << proc.memory_percent << "%\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }

    std::string process_name = argv[1];
    
    // Initialize detectors
    hw_monitor::GPUDetector& gpu_detector = hw_monitor::GPUDetector::instance();
    hw_monitor::RAMDetector ram_detector;

    std::cout << "Monitoring resource usage for process: " << process_name << std::endl;

    // Get GPU information
    auto gpu_processes = gpu_detector.get_process_info(process_name);
    if (gpu_processes) {
        print_gpu_info(*gpu_processes);
    } else {
        std::cout << "\nNo GPU usage found for process: " << process_name << std::endl;
    }

    // Get RAM information
    auto ram_processes = ram_detector.get_process_info(process_name);
    if (ram_processes) {
        print_ram_info(*ram_processes);
    } else {
        std::cout << "\nNo RAM usage found for process: " << process_name << std::endl;
    }

    return 0;
} 