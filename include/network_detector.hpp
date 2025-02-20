#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>

namespace hw_monitor {

/**
 * @brief Information about a network process
 */
struct NetworkProcessInfo {
    uint32_t pid;                           ///< Process ID
    std::string process_name;               ///< Name of the process
    float receive_bytes_per_sec;            ///< Network receive rate in bytes per second
    float transmit_bytes_per_sec;           ///< Network transmit rate in bytes per second
    uint32_t active_connections;            ///< Number of active network connections
    std::vector<uint16_t> ports;            ///< List of ports being used by the process
};

/**
 * @brief Information about a network interface
 */
struct NetworkInterfaceInfo {
    std::string name;                       ///< Interface name (e.g., eth0, wlan0)
    std::string ip_address;                 ///< IP address assigned to the interface
    std::string mac_address;                ///< MAC (hardware) address of the interface
    bool is_up;                             ///< Whether the interface is currently active
    float receive_bytes_per_sec;            ///< Current receive rate in bytes per second
    float transmit_bytes_per_sec;           ///< Current transmit rate in bytes per second
    uint64_t total_received_bytes;          ///< Total bytes received since system start
    uint64_t total_transmitted_bytes;       ///< Total bytes transmitted since system start
    uint32_t mtu;                           ///< Maximum Transmission Unit in bytes
    float link_speed_mbps;                  ///< Link speed in Megabits per second (if available)
};

/**
 * @brief Network interface and process monitoring
 * 
 * This class provides functionality to monitor network interfaces and processes,
 * including bandwidth usage, active connections, and interface statistics.
 */
class NetworkDetector {
public:
    /**
     * @brief Default constructor
     */
    NetworkDetector();

    /**
     * @brief Destructor
     */
    ~NetworkDetector();

    /**
     * @brief Get network usage info for a specific process
     * @param pid Process ID to monitor
     * @return Process network information if process exists
     */
    std::optional<NetworkProcessInfo> get_process_info(uint32_t pid) const;

    /**
     * @brief Get network usage info for processes matching a name
     * @param process_name Name of the process to monitor
     * @return Vector of process network information if matching processes found
     */
    std::optional<std::vector<NetworkProcessInfo>> get_process_info(const std::string& process_name) const;

    /**
     * @brief Get statistics for all network interfaces
     * @return Vector of interface information structures
     */
    std::vector<NetworkInterfaceInfo> get_interface_info() const;

    /**
     * @brief Get list of all network interface names
     * @return Vector of interface names (e.g., ["eth0", "wlan0"])
     */
    std::vector<std::string> get_interface_names() const;

private:
    class Impl;                             ///< Forward declaration
    std::unique_ptr<Impl> pimpl_;           ///< Pointer to implementation
};

} // namespace hw_monitor 