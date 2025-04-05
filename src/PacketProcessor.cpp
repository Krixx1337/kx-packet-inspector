#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // Include windows.h early to establish platform definitions

// Now include your project headers and standard library headers
#include "PacketProcessor.h"
#include "PacketData.h"
#include "AppState.h"
#include "PacketHeaders.h"
#include "CryptoUtils.h"
#include "GameStructs.h" // Included via PacketProcessor.h but good practice

#include <vector>
#include <chrono>
#include <mutex>
#include <limits>
#include <cstring> // For memcpy

namespace kx::PacketProcessing {

    void ProcessOutgoingPacket(const GameStructs::MsgSendContext* context) {
        // Basic check (hook should ideally ensure non-null, but double-check)
        if (!context) {
            // Log::Error("ProcessOutgoingPacket called with null context."); // Future logger
            OutputDebugStringA("[PacketProcessor] Error: ProcessOutgoingPacket called with null context.\n");
            return;
        }

        try {
            // Skip processing if bufferState indicates the buffer might be invalid or getting reset (state 1).
            if (context->bufferState == 1) {
                return;
            }

            std::uint8_t* packetData = context->GetPacketBufferStart();
            std::size_t bufferSize = context->GetCurrentDataSize();

            // --- Sanity Checks ---
            bool dataIsValid = true;
            if (packetData == nullptr || context->currentBufferEndPtr == nullptr) {
                // Log::Error("Null pointer in MsgSendContext detected during processing.");
                OutputDebugStringA("[PacketProcessor] Error: Null pointer in MsgSendContext detected during processing.\n");
                dataIsValid = false;
            }
            else if (context->currentBufferEndPtr < packetData) {
                // Log::Error("Invalid packet buffer pointers (end < start) in MsgSendContext.");
                OutputDebugStringA("[PacketProcessor] Error: Invalid packet buffer pointers (end < start) in MsgSendContext.\n");
                dataIsValid = false;
            }
            else {
                // Limit size to prevent reading excessive memory.
                constexpr std::size_t MAX_REASONABLE_PACKET_SIZE = 16 * 1024;
                if (bufferSize > MAX_REASONABLE_PACKET_SIZE) {
                    // Log::Error("Outgoing packet size (%zu) exceeds sanity limit (%zu).", bufferSize, MAX_REASONABLE_PACKET_SIZE);
                    char msg[128];
                    sprintf_s(msg, sizeof(msg), "[PacketProcessor] Error: Outgoing packet size (%zu) exceeds sanity limit.\n", bufferSize);
                    OutputDebugStringA(msg);
                    dataIsValid = false;
                }
                // Check against the destination integer type limit.
                else if (bufferSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    // Log::Error("Outgoing packet size (%zu) exceeds std::numeric_limits<int>::max().", bufferSize);
                    char msg[128];
                    sprintf_s(msg, sizeof(msg), "[PacketProcessor] Error: Outgoing packet size (%zu) exceeds max int.\n", bufferSize);
                    OutputDebugStringA(msg);
                    dataIsValid = false;
                }
            }
            // --- End Sanity Checks ---

            if (dataIsValid && bufferSize > 0) {
                PacketInfo info;
                info.timestamp = std::chrono::system_clock::now();
                info.size = static_cast<int>(bufferSize);
                info.direction = PacketDirection::Sent;
                info.bufferState = context->bufferState;
                info.specialType = InternalPacketType::NORMAL; // Assume normal

                // Copy packet data
                info.data.assign(packetData, packetData + bufferSize);

                // Analyze header
                info.rawHeaderId = info.data[0];
                info.name = GetPacketName(info.direction, info.rawHeaderId); // Use directional lookup

                // Log the packet
                {
                    std::lock_guard<std::mutex> lock(g_packetLogMutex);
                    g_packetLog.push_back(std::move(info));
                }
            }
            else if (dataIsValid && bufferSize == 0) {
                PacketInfo info;
                info.timestamp = std::chrono::system_clock::now();
                info.size = 0;
                info.direction = PacketDirection::Sent;
                info.bufferState = context->bufferState;
                info.specialType = InternalPacketType::EMPTY_PACKET; // Mark as empty
                info.name = GetSpecialPacketTypeName(info.specialType);
                info.rawHeaderId = 0; // Or some default
                // Log the empty packet info
                {
                    std::lock_guard<std::mutex> lock(g_packetLogMutex);
                    g_packetLog.push_back(std::move(info));
                }
            }
        }
        catch (const std::exception& e) {
            // Log::Error("[ProcessOutgoingPacket] Exception: %s", e.what());
            char msg[256];
            sprintf_s(msg, sizeof(msg), "[PacketProcessor] Outgoing packet processing exception: %s\n", e.what());
            OutputDebugStringA(msg);
        }
        catch (...) {
            // Log::Error("[ProcessOutgoingPacket] Unknown exception.");
            OutputDebugStringA("[PacketProcessor] Unknown exception during outgoing packet processing.\n");
        }
    }

    void ProcessIncomingPacket(int currentState,
        const std::uint8_t* buffer,
        std::size_t size,
        const std::optional<GameStructs::RC4State>& capturedRc4State)
    {
        // Basic checks
        if (buffer == nullptr || size == 0) {
            // Allow logging zero-size packets if needed, but skip if buffer is null
            if (buffer == nullptr) return;
        }

        // Check size limits (similar to outgoing)
        if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            // Log::Error("Incoming packet size (%zu) exceeds std::numeric_limits<int>::max().", size);
            char msg[128];
            sprintf_s(msg, sizeof(msg), "[PacketProcessor] Error: Incoming packet size (%zu) exceeds max int.\n", size);
            OutputDebugStringA(msg);
            return; // Don't process excessively large packets
        }
        constexpr std::size_t MAX_REASONABLE_PACKET_SIZE = 16 * 1024; // Reuse or define separately
        if (size > MAX_REASONABLE_PACKET_SIZE) {
            // Log::Error("Incoming packet size (%zu) exceeds sanity limit (%zu).", size, MAX_REASONABLE_PACKET_SIZE);
            char msg[128];
            sprintf_s(msg, sizeof(msg), "[PacketProcessor] Error: Incoming packet size (%zu) exceeds sanity limit.\n", size);
            OutputDebugStringA(msg);
            return;
        }

        try {
            PacketInfo info;
            info.timestamp = std::chrono::system_clock::now();
            info.size = static_cast<int>(size);
            info.direction = PacketDirection::Received;
            info.bufferState = currentState;
            info.rc4State = capturedRc4State; // Store captured state
            info.specialType = InternalPacketType::NORMAL; // Assume normal initially
            info.rawHeaderId = 0; // Default

            // Copy original data (even if empty)
            if (size > 0) {
                info.data.assign(buffer, buffer + size);
            }

            // --- Decrypt Data if Applicable ---
            bool wasDecrypted = false;
            bool decryptionAttempted = false;
            if (info.rc4State.has_value() && !info.data.empty()) {
                decryptionAttempted = true; // Mark that we tried
                // ... decryption logic using Crypto::rc4_process_copy ...
                if (/* decryption successful */ true) { // Assume success for now
                    // info.decryptedData = ... result ...
                    wasDecrypted = true;
                }
                else {
                    info.specialType = InternalPacketType::PROCESSING_ERROR; // Mark decryption failure
                }
            }
            // --- End Decryption ---


            // --- Packet Analysis ---
            const std::vector<uint8_t>& dataToAnalyze = info.GetDisplayData();

            // Prioritize special states
            if (info.specialType == InternalPacketType::PROCESSING_ERROR) {
                info.name = GetSpecialPacketTypeName(info.specialType);
            }
            else if (currentState == 3 && !wasDecrypted && !info.rc4State.has_value()) {
                // If state was 3 but we failed to capture state, mark it specifically
                info.specialType = InternalPacketType::ENCRYPTED_RC4; // Or a subtype?
                info.name = "Encrypted (RC4 State Read Fail)";
            }
            else if (currentState == 3 && decryptionAttempted && !wasDecrypted) {
                // If state was 3, we tried to decrypt but failed
                info.specialType = InternalPacketType::ENCRYPTED_RC4; // Or a subtype?
                info.name = "Encrypted (RC4 Decrypt Fail?)";
            }
            else if (currentState == 3 && !decryptionAttempted) {
                // State was 3, but we didn't try decrypting (e.g., empty packet)
                info.specialType = InternalPacketType::ENCRYPTED_RC4;
                info.name = GetSpecialPacketTypeName(info.specialType); // Generic "Encrypted (RC4)"
            }
            else if (dataToAnalyze.empty()) {
                info.specialType = InternalPacketType::EMPTY_PACKET;
                info.name = GetSpecialPacketTypeName(info.specialType);
            }
            else {
                // If none of the above, it's a "NORMAL" packet (decrypted or plaintext)
                info.specialType = InternalPacketType::NORMAL;
                info.rawHeaderId = dataToAnalyze[0];
                info.name = GetPacketName(info.direction, info.rawHeaderId); // Use directional lookup

                // Check if GetPacketName returned an "Unknown" formatted string
                if (info.name.find("_UNKNOWN") != std::string::npos) {
                    info.specialType = InternalPacketType::UNKNOWN_HEADER; // Mark specifically as unknown ID
                }
            }
            // --- End Packet Analysis ---


            // Log the processed packet info
            {
                std::lock_guard<std::mutex> lock(g_packetLogMutex);
                g_packetLog.push_back(std::move(info));
            }
        }
        catch (const std::exception& e) {
            // Log::Error("[ProcessIncomingPacket] Exception: %s", e.what());
            char msg[256];
            sprintf_s(msg, sizeof(msg), "[PacketProcessor] Incoming packet processing exception: %s\n", e.what());
            OutputDebugStringA(msg);
        }
        catch (...) {
            // Log::Error("[ProcessIncomingPacket] Unknown exception.");
            OutputDebugStringA("[PacketProcessor] Unknown exception during incoming packet processing.\n");
        }
    }


} // namespace kx::PacketProcessing