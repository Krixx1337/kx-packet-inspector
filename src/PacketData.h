#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <deque>
#include <chrono>
#include <cstdint>
#include <optional>
#include "GameStructs.h"

namespace kx {

    // Enum to represent packet direction
    enum class PacketDirection {
        Sent,
        Received
    };

    // Represents types identified during processing, not actual network headers.
    enum class InternalPacketType {
        NORMAL,              // A regular packet with a known or unknown header ID
        ENCRYPTED_RC4,       // Packet identified as RC4 encrypted *before* decryption attempt
        UNKNOWN_HEADER,      // Header ID was not found in the known lists for its direction *after* potential decryption
        EMPTY_PACKET,        // Packet buffer was empty
        PROCESSING_ERROR,    // Error during processing/decryption prevented analysis
        PACKET_TOO_SMALL     // Packet data buffer was smaller than the required header size (2 bytes)
    };

    // Forward declare the InternalPacketType enum
    enum class InternalPacketType;

    // Structure to hold information about a captured packet
    struct PacketInfo {
        std::chrono::system_clock::time_point timestamp;
        int size = 0;                      // Size of original data
        std::vector<uint8_t> data;         // Original (potentially encrypted) byte data
        PacketDirection direction;
        uint16_t rawHeaderId = 0;          // Raw 2-byte header (from decrypted data if applicable)
        std::string name = "Unprocessed";  // String name (resolved using direction + rawHeaderId or special type)
        int bufferState = -1;              // State read from MsgConn (-1: null ctx, -2: read err, >=0: actual state)
        InternalPacketType specialType = InternalPacketType::NORMAL; // Assume normal unless set otherwise
    };

    // Global container for storing captured packet info
    extern std::deque<PacketInfo> g_packetLog;

    // Mutex to protect access to the global packet log
    extern std::mutex g_packetLogMutex;

} // namespace kx
