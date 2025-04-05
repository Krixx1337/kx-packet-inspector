#pragma once

/*
 * NOTE ON SCOPE:
 * This file defines known packet headers (opcodes) for identification purposes.
 * It primarily focuses on Client-to-Server (CMSG) messages identified so far.
 * Server-to-Client (SMSG) messages often share opcodes but can have different structures.
 * The target client potentially contains around ~1700 distinct network messages,
 * falling into roughly ~35 relevant categories. This list represents only a
 * small subset. Contributions for defining more headers are welcome.
 * It also includes special values for representing internal states like encrypted packets.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <utility> // For std::pair

#include "PacketData.h" // Required for PacketDirection enum definition

namespace kx {

    // --- Client->Server Header IDs ---
    // Use a plain enum or enum class, but the value is what matters.
    enum class CMSG_HeaderId : uint8_t {
        CHAT_SEND_MESSAGE = 0xF9,
        USE_SKILL = 0x17,
        MOVEMENT = 0x12,
        MOVEMENT_WITH_ROTATION = 0x0B,
        MOVEMENT_END = 0x04,
        JUMP = 0x09,
        HEARTBEAT = 0x11,
        SELECT_AGENT = 0xDE,
        DESELECT_AGENT = 0xD6,
        MOUNT_MOVEMENT = 0x18,
        // Add more CMSG IDs as they are identified
    };

    // --- Server->Client Header IDs ---
    // Initially empty or with placeholders, as none are identified yet.
    enum class SMSG_HeaderId : uint8_t {
        // Example (replace with actual identified IDs)
        // AGENT_UPDATE       = 0x??,
        // CHAT_RECEIVE       = 0x??,
        // SKILL_RESULT       = 0x??,
        PLACEHOLDER = 0x00 // Remove or replace once real IDs are found
    };

    // --- Internal Helper Data (Private Implementation Detail) ---
    namespace detail {
        // Static maps initialization (thread-safe in C++11+)
        inline const std::map<uint8_t, std::string>& GetCmsgNameMap() {
            static const std::map<uint8_t, std::string> cmsgNames = {
                { static_cast<uint8_t>(CMSG_HeaderId::CHAT_SEND_MESSAGE),      "CMSG_CHAT_SEND_MESSAGE" },
                { static_cast<uint8_t>(CMSG_HeaderId::USE_SKILL),              "CMSG_USE_SKILL" },
                { static_cast<uint8_t>(CMSG_HeaderId::MOVEMENT),               "CMSG_MOVEMENT" },
                { static_cast<uint8_t>(CMSG_HeaderId::MOVEMENT_WITH_ROTATION), "CMSG_MOVEMENT_WITH_ROTATION" },
                { static_cast<uint8_t>(CMSG_HeaderId::MOVEMENT_END),           "CMSG_MOVEMENT_END" },
                { static_cast<uint8_t>(CMSG_HeaderId::JUMP),                   "CMSG_JUMP" },
                { static_cast<uint8_t>(CMSG_HeaderId::HEARTBEAT),              "CMSG_HEARTBEAT" },
                { static_cast<uint8_t>(CMSG_HeaderId::SELECT_AGENT),           "CMSG_SELECT_AGENT" },
                { static_cast<uint8_t>(CMSG_HeaderId::DESELECT_AGENT),         "CMSG_DESELECT_AGENT" },
                { static_cast<uint8_t>(CMSG_HeaderId::MOUNT_MOVEMENT),         "CMSG_MOUNT_MOVEMENT" },
            };
            return cmsgNames;
        }

        inline const std::map<uint8_t, std::string>& GetSmsgNameMap() {
            static const std::map<uint8_t, std::string> smsgNames = {
                // { static_cast<uint8_t>(SMSG_HeaderId::AGENT_UPDATE), "SMSG_AGENT_UPDATE" }, // Example
               // Add known SMSG mappings here
                { static_cast<uint8_t>(SMSG_HeaderId::PLACEHOLDER), "SMSG_PLACEHOLDER" }, // Example
            };
            return smsgNames;
        }

        inline const std::map<InternalPacketType, std::string>& GetSpecialTypeNameMap() {
            static const std::map<InternalPacketType, std::string> specialNames = {
                { InternalPacketType::ENCRYPTED_RC4, "Encrypted (RC4)"},
                { InternalPacketType::UNKNOWN_HEADER, "Unknown Header"}, // This indicates the ID itself was unknown for the direction
                { InternalPacketType::EMPTY_PACKET, "Empty Packet"},
                { InternalPacketType::PROCESSING_ERROR, "Processing Error"},
                // Note: NORMAL type doesn't usually need a name here, it uses the header name
            };
            return specialNames;
        }

        // Helper to format unknown headers
        inline std::string FormatUnknownHeader(PacketDirection direction, uint8_t rawHeaderId) {
            std::stringstream ss;
            ss << (direction == PacketDirection::Sent ? "CMSG" : "SMSG")
                << "_UNKNOWN [0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rawHeaderId) << "]";
            return ss.str();
        }
    } // namespace detail


    // --- Public API ---

    /**
     * @brief Gets a descriptive string name for a packet based on its direction and raw header ID.
     * @param direction The direction of the packet (Sent or Received).
     * @param rawHeaderId The uint8_t header ID read from the packet data.
     * @return std::string The descriptive name (e.g., "CMSG_USE_SKILL") or a formatted unknown string
     *                     (e.g., "SMSG_UNKNOWN [0xAB]").
     */
    inline std::string GetPacketName(PacketDirection direction, uint8_t rawHeaderId) {
        const auto& nameMap = (direction == PacketDirection::Sent) ? detail::GetCmsgNameMap() : detail::GetSmsgNameMap();
        auto it = nameMap.find(rawHeaderId);
        if (it != nameMap.end()) {
            return it->second; // Return the found name
        }
        // Format and return unknown name if not found in the map for that direction
        return detail::FormatUnknownHeader(direction, rawHeaderId);
    }

    /**
    * @brief Gets a descriptive string name for a special internal packet type.
    * @param type The InternalPacketType enum value.
    * @return std::string The corresponding name, or "Internal Error" if type not mapped.
    */
    inline std::string GetSpecialPacketTypeName(InternalPacketType type) {
        const auto& specialNames = detail::GetSpecialTypeNameMap();
        auto it = specialNames.find(type);
        if (it != specialNames.end()) {
            return it->second;
        }
        return "Internal Error Type"; // Fallback
    }


    /**
     * @brief Provides a list of known CMSG headers for UI population.
     * @return A vector of pairs, where each pair contains the {Header ID, Header Name}.
     */
    inline std::vector<std::pair<uint8_t, std::string>> GetKnownCMSGHeaders() {
        std::vector<std::pair<uint8_t, std::string>> headers;
        const auto& map = detail::GetCmsgNameMap();
        headers.reserve(map.size());
        for (const auto& pair : map) {
            headers.push_back(pair);
        }
        // Consider sorting alphabetically by name if desired for UI
        // std::sort(headers.begin(), headers.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
        return headers;
    }

    /**
     * @brief Provides a list of known SMSG headers for UI population.
     * @return A vector of pairs, where each pair contains the {Header ID, Header Name}.
     */
    inline std::vector<std::pair<uint8_t, std::string>> GetKnownSMSGHeaders() {
        std::vector<std::pair<uint8_t, std::string>> headers;
        const auto& map = detail::GetSmsgNameMap();
        headers.reserve(map.size());
        for (const auto& pair : map) {
            // Skip placeholder if it exists and no other real headers are defined
            if (map.size() == 1 && pair.first == static_cast<uint8_t>(SMSG_HeaderId::PLACEHOLDER)) {
                continue;
            }
            headers.push_back(pair);
        }
        // Consider sorting
        return headers;
    }

    /**
    * @brief Provides a list of special internal packet types for UI filtering.
    * @return A vector of pairs, where each pair contains the {InternalPacketType, Type Name}.
    */
    inline std::vector<std::pair<InternalPacketType, std::string>> GetSpecialPacketTypesForFilter() {
        std::vector<std::pair<InternalPacketType, std::string>> types;
        const auto& map = detail::GetSpecialTypeNameMap();
        types.reserve(map.size());
        for (const auto& pair : map) {
            types.push_back(pair);
        }
        // Can add others like UNKNOWN_HEADER if desired as separate filterable type
        types.push_back({ InternalPacketType::UNKNOWN_HEADER, "Unknown Header ID" });

        // Sort maybe?
        return types;
    }


} // namespace kx