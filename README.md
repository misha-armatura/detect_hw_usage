# Hardware Monitor Library

A C++ library for monitoring system hardware resources including CPU, GPU, RAM, Storage, and Network interfaces on Linux systems. The library provides detailed information about hardware utilization and process-specific resource usage.

## Features

- **CPU Monitoring**
  - Overall CPU usage and per-core statistics
  - Core frequencies and temperatures
  - Process-specific CPU utilization
  - Thread count and CPU affinity

- **GPU Monitoring**
  - Support for NVIDIA GPUs (via NVML)
  - GPU utilization and temperature
  - Memory usage statistics
  - Process-specific GPU usage

- **RAM Monitoring**
  - System-wide memory usage
  - Cache and buffer statistics
  - Process-specific memory consumption
  - Virtual and shared memory tracking

- **Storage Monitoring**
  - Filesystem statistics
  - Disk usage and availability
  - Process I/O rates
  - Open file tracking

- **Network Monitoring**
  - Interface statistics
  - Bandwidth usage
  - Process network activity
  - Active connection tracking

## Building

The library requires:
- C++20 or later
- Linux operating system
- CMake 3.21 or later
- NVIDIA drivers (optional, for GPU monitoring)

```bash
mkdir build && cd build
cmake .. && cmake --build . --config Release
```

## Usage

For getting all information about a process, you can use the following command:
```bash
./test_program firefox
```

or for getting information about all processes, you can use the following command:
```bash
./test_program 
```


### Sample Output
```
CPU Information:
Total Usage: 45.2%
Cores: 8
Average Frequency: 3.2 GHz
Average Temperature: 65°C
Per-core Usage: [50%, 42%, 48%, 43%, 44%, 46%, 41%, 47%]
GPU Information:
Name: NVIDIA GeForce RTX 3080
Temperature: 72°C
Memory: 6.2GB / 10GB
Utilization: 85%
Process Count: 3
RAM Information:
Total: 32GB
Used: 12.4GB
Available: 19.6GB
Cache: 4.2GB
Usage: 38.7%
Storage Information:
Device: /dev/sda1
Mount: /
Type: ext4
Total: 512GB
Used: 256GB
Available: 256GB
Usage: 50%
Network Information:
Interface: eth0
IP: 192.168.1.100
Speed: 1000Mbps
Rx Rate: 2.5MB/s
Tx Rate: 1.2MB/s
```

## Notes
- GPU monitoring currently supports NVIDIA GPUs through NVML
- Some parts of the library requires appropriate permissions
- All rates and usage statistics are averaged over a sampling period
- This program was tested on Ubuntu 24.04 LTS