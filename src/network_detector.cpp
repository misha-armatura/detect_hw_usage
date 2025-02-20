#include "network_detector.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <regex>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace hw_monitor {

class NetworkDetector::Impl {
private:
    static std::string read_file(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        std::string content;
        std::getline(file, content);
        return content;
    }

    static std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> read_interface_stats() {
        std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> stats;
        std::ifstream proc_net("/proc/net/dev");
        std::string line;

        // Skip header lines
        std::getline(proc_net, line);
        std::getline(proc_net, line);

        while (std::getline(proc_net, line)) {
            std::istringstream iss(line);
            std::string iface;
            uint64_t recv_bytes, trans_bytes;

            iss >> iface;
            if (iface.back() == ':') {
                iface.pop_back();
            }

            iss >> recv_bytes;
            // Skip other receive stats
            for (int i = 0; i < 7; ++i) {
                uint64_t dummy;
                iss >> dummy;
            }
            iss >> trans_bytes;

            stats[iface] = {recv_bytes, trans_bytes};
        }

        return stats;
    }

    static std::string get_ip_address(const std::string& interface) {
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) return "";

        struct ifreq ifr;
        ifr.ifr_addr.sa_family = AF_INET;
        std::strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ-1);

        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
            close(fd);
            return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
        }

        close(fd);
        return "";
    }

    static std::string get_mac_address(const std::string& interface) {
        std::string mac_path = "/sys/class/net/" + interface + "/address";
        return read_file(mac_path);
    }

    static bool get_interface_status(const std::string& interface) {
        std::string operstate_path = "/sys/class/net/" + interface + "/operstate";
        std::string state = read_file(operstate_path);
        return state.find("up") != std::string::npos;
    }

    static uint32_t get_interface_mtu(const std::string& interface) {
        std::string mtu_path = "/sys/class/net/" + interface + "/mtu";
        std::string mtu_str = read_file(mtu_path);
        return mtu_str.empty() ? 0 : std::stoul(mtu_str);
    }

    static float get_link_speed(const std::string& interface) {
        std::string speed_path = "/sys/class/net/" + interface + "/speed";
        std::string speed_str = read_file(speed_path);
        return speed_str.empty() ? 0.0f : std::stof(speed_str);
    }

    static std::vector<uint16_t> get_process_ports(uint32_t pid) {
        std::vector<uint16_t> ports;
        std::ifstream tcp_file("/proc/net/tcp");
        std::ifstream tcp6_file("/proc/net/tcp6");
        std::string line;

        auto parse_ports = [&ports, pid](std::ifstream& file) {
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string sl, local_addr, rem_addr, state, tx_queue, rx_queue, tr, tm_when, retrnsmt, uid, timeout, inode;
                iss >> sl >> local_addr >> rem_addr >> state >> tx_queue >> rx_queue >> tr >> tm_when >> retrnsmt >> uid >> timeout >> inode;

                // Extract local port from local_addr
                size_t colon_pos = local_addr.find(':');
                if (colon_pos != std::string::npos) {
                    std::string port_hex = local_addr.substr(colon_pos + 1);
                    uint16_t port = std::stoul(port_hex, nullptr, 16);
                    ports.push_back(port);
                }
            }
        };

        parse_ports(tcp_file);
        parse_ports(tcp6_file);

        // Remove duplicates
        std::sort(ports.begin(), ports.end());
        ports.erase(std::unique(ports.begin(), ports.end()), ports.end());

        return ports;
    }

    static uint32_t count_active_connections(uint32_t pid) {
        uint32_t count = 0;
        std::string fd_path = "/proc/" + std::to_string(pid) + "/fd";
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(fd_path)) {
                if (std::filesystem::is_symlink(entry.path())) {
                    std::error_code ec;
                    std::string target = std::filesystem::read_symlink(entry.path(), ec).string();
                    if (!ec && target.find("socket:") != std::string::npos) {
                        count++;
                    }
                }
            }
        } catch (...) {}

        return count;
    }

public:
    Impl() {}

    std::vector<NetworkInterfaceInfo> get_interface_info() const {
        std::vector<NetworkInterfaceInfo> result;

        // Get initial stats
        auto initial_stats = read_interface_stats();
        
        // Wait a short time to calculate rate
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Get updated stats
        auto final_stats = read_interface_stats();

        for (const auto& [iface, stats] : final_stats) {
            // Skip loopback interface
            if (iface == "lo") continue;

            NetworkInterfaceInfo info;
            info.name = iface;
            info.ip_address = get_ip_address(iface);
            info.mac_address = get_mac_address(iface);
            info.is_up = get_interface_status(iface);
            info.mtu = get_interface_mtu(iface);
            info.link_speed_mbps = get_link_speed(iface);

            // Calculate rates
            float time_diff = 0.1f; // 100ms in seconds
            info.total_received_bytes = stats.first;
            info.total_transmitted_bytes = stats.second;
            info.receive_bytes_per_sec = (stats.first - initial_stats[iface].first) / time_diff;
            info.transmit_bytes_per_sec = (stats.second - initial_stats[iface].second) / time_diff;

            result.push_back(std::move(info));
        }

        return result;
    }

    std::vector<std::string> get_interface_names() const {
        std::vector<std::string> result;
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
            std::string name = entry.path().filename();
            if (name != "lo") { // Skip loopback interface
                result.push_back(name);
            }
        }
        return result;
    }

    std::optional<NetworkProcessInfo> get_process_info(uint32_t pid) const {
        if (!std::filesystem::exists("/proc/" + std::to_string(pid))) {
            return std::nullopt;
        }

        NetworkProcessInfo info;
        info.pid = pid;
        info.process_name = read_file("/proc/" + std::to_string(pid) + "/comm");
        if (!info.process_name.empty() && info.process_name.back() == '\n') {
            info.process_name.pop_back();
        }

        // Get network statistics
        info.active_connections = count_active_connections(pid);
        info.ports = get_process_ports(pid);

        // Calculate network rates
        std::string net_path = "/proc/" + std::to_string(pid) + "/net/dev";
        if (std::filesystem::exists(net_path)) {
            // Get initial stats
            auto initial_stats = read_interface_stats();
            
            // Wait a short time to calculate rate
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Get updated stats
            auto final_stats = read_interface_stats();

            // Sum up rates across all interfaces
            float time_diff = 0.1f; // 100ms in seconds
            uint64_t total_recv_diff = 0;
            uint64_t total_trans_diff = 0;

            for (const auto& [iface, stats] : final_stats) {
                if (iface != "lo") { // Skip loopback
                    total_recv_diff += stats.first - initial_stats[iface].first;
                    total_trans_diff += stats.second - initial_stats[iface].second;
                }
            }

            info.receive_bytes_per_sec = total_recv_diff / time_diff;
            info.transmit_bytes_per_sec = total_trans_diff / time_diff;
        }

        return info;
    }

    std::optional<std::vector<NetworkProcessInfo>> get_process_info(const std::string& process_name) const {
        std::vector<NetworkProcessInfo> result;

        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!std::filesystem::is_directory(entry.path())) continue;

            std::string pid_str = entry.path().filename().string();
            if (pid_str.find_first_not_of("0123456789") != std::string::npos) continue;

            try {
                uint32_t pid = std::stoul(pid_str);
                std::string comm = read_file(entry.path().string() + "/comm");
                if (!comm.empty() && comm.find(process_name) != std::string::npos) {
                    if (auto proc_info = get_process_info(pid)) {
                        result.push_back(*proc_info);
                    }
                }
            } catch (...) {}
        }

        return result.empty() ? std::nullopt : std::make_optional(result);
    }
};

// Implement the public interface
NetworkDetector::NetworkDetector() : pimpl_(std::make_unique<Impl>()) {}
NetworkDetector::~NetworkDetector() = default;

std::optional<NetworkProcessInfo> NetworkDetector::get_process_info(uint32_t pid) const {
    return pimpl_->get_process_info(pid);
}

std::optional<std::vector<NetworkProcessInfo>> NetworkDetector::get_process_info(const std::string& process_name) const {
    return pimpl_->get_process_info(process_name);
}

std::vector<NetworkInterfaceInfo> NetworkDetector::get_interface_info() const {
    return pimpl_->get_interface_info();
}

std::vector<std::string> NetworkDetector::get_interface_names() const {
    return pimpl_->get_interface_names();
}

} // namespace hw_monitor 
