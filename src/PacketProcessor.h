#pragma once

/**
 * @file PacketProcessor.h
 * @brief Defines the interface for processing captured packet data.
 * @details This layer separates packet analysis, decryption, and logging
 *          from the raw function hooking mechanism.
 */

#include <cstddef>        // For std::size_t
#include <cstdint>        // For std::uint8_t
#include <optional>       // For std::optional
#include "GameStructs.h"   // For kx::GameStructs::MsgSendContext, RC4State
#include "PacketData.h"    // For kx::PacketDirection (potentially useful here later)

namespace kx::PacketProcessing {

    /**
     * @brief Processes data captured from an outgoing packet event (MsgSend).
     * @param context Pointer to the game's MsgSendContext structure containing
     *                buffer state and pointers relevant to the outgoing packet.
     *                Expected to be non-null by the caller (hook).
     */
    void ProcessOutgoingPacket(const GameStructs::MsgSendContext* context);

    /**
     * @brief Processes data captured from an incoming packet event (MsgRecv).
     * @param currentState The buffer state read from the MsgConn context (-1 if context was null, -2 on read error).
     * @param buffer Pointer to the start of the raw received packet data buffer.
     * @param size The size of the data in the buffer.
     * @param capturedRc4State An optional containing the RC4 state snapshot if it was
     *                         successfully captured (typically when currentState is 3).
     *                         std::nullopt otherwise.
     */
    void ProcessIncomingPacket(int currentState,
        const std::uint8_t* buffer,
        std::size_t size,
        const std::optional<GameStructs::RC4State>& capturedRc4State);

} // namespace kx::PacketProcessing