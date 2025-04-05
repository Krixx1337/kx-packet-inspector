#include "FormattingUtils.h"
#include <sstream>
#include <iomanip>
#include <ctime>

// Include PacketData.h again here for the implementation details of PacketInfo if needed,
// although it's already included via the header. Best practice includes what you use.
#include "PacketData.h" // Provides PacketInfo definition, PacketDirection

namespace kx::Utils {

    // --- Function Implementations ---

    std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
        std::time_t time = std::chrono::system_clock::to_time_t(tp);
        std::tm local_tm;
        localtime_s(&local_tm, &time); // Use localtime_s for safety

        std::stringstream ss;
        ss << std::put_time(&local_tm, "%H:%M:%S");
        // Optional: milliseconds
        // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
        // ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    std::string FormatBytesToHex(const std::vector<uint8_t>& data, int maxBytes) {
        std::stringstream ss;
        int count = 0;
        for (const auto& byte : data) {
            if (count > 0) ss << " "; // Space separator

            // Apply maxBytes limit (only if positive)
            if (maxBytes > 0 && count >= maxBytes) {
                ss << "...";
                break;
            }

            ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            count++;
        }

        if (data.empty()) {
            return "(empty)";
        }
        else if (ss.str().empty() && maxBytes <= 0 && !data.empty()) {
            return "..."; // Data exists but wasn't shown (maxBytes<=0)
        }

        return ss.str();
    }

    std::string FormatLogEntryString(const PacketInfo& packet) {
        std::string timestampStr = FormatTimestamp(packet.timestamp);
        const char* directionStr = (packet.direction == PacketDirection::Sent) ? "[S]" : "[R]";
        // Use the PacketInfo's helper to get potentially decrypted data
        const auto& dataToDisplay = packet.GetDisplayData();
        // Use the other helper for hex formatting
        std::string dataHexStr = FormatBytesToHex(dataToDisplay, 32); // Consistent limit
        int displaySize = dataToDisplay.size();

        // Construct the final string using PacketInfo members
        std::string logEntry = timestampStr + " " + directionStr + " "
            + packet.name // Use the pre-resolved name from PacketInfo
            + " | Sz:" + std::to_string(displaySize)
            + " | " + dataHexStr;
        return logEntry;
    }

} // namespace kx::Utils