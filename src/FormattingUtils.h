#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint> // For uint8_t

// Forward declare PacketInfo to avoid including PacketData.h in the header if possible,
// but since the function signature requires it, we must include it.
#include "PacketData.h" // Include necessary dependencies for function signatures

namespace kx::Utils {

    /**
     * @brief Formats a system time point into HH:MM:SS string.
     * @param tp The time point to format.
     * @return Formatted time string.
     */
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp);

    /**
     * @brief Formats a vector of bytes into a space-separated hex string.
     * @param data The byte vector.
     * @param maxBytes Maximum number of bytes to format before truncating with "...". 0 or negative means no limit practical limit imposed here.
     * @return Formatted hex string (e.g., "0F A2 1B ...").
     */
    std::string FormatBytesToHex(const std::vector<uint8_t>& data, int maxBytes = 32);

    /**
     * @brief Formats a PacketInfo struct into a single log line string suitable for display or clipboard.
     * @param packet The PacketInfo object.
     * @return A formatted string representing the packet log entry.
     */
    std::string FormatLogEntryString(const PacketInfo& packet);

} // namespace kx::Utils