#pragma once

#include <cstddef> // For std::byte, std::size_t
#include <cstdint> // For std::uint8_t, std::int32_t
#include <type_traits> // For offsetof (used in static_assert)
#include <array>   // For std::array

namespace kx::GameStructs {

    /**
     * @brief Represents the memory layout of the context object passed to the
     *        game's internal message sending function (identified as FUN_1412e9960).
     * @details This structure defines members based on reverse engineering the function,
     *          specifically how it accesses data relative to its 'param_1' argument.
     *          It allows accessing packet data before potential encryption/final processing.
     * @warning This structure definition is based on analysis of specific versions
     *          of the target game (Gw2-64.exe). While it may remain stable across
     *          some game updates, patches that significantly alter internal data
     *          layouts **could** require this definition to be updated to ensure
     *          correct functionality. Community contributions to verify and update
     *          this definition are appreciated.
     */
    struct MsgSendContext {
        // --- Member offsets relative to the start of the structure (param_1) ---

        // Define padding explicitly for clarity on known/unknown regions.
        std::byte padding_0x0[0xD0];

        /**
         * @brief 0xD0: Pointer to the current write position (end) within the packet data buffer.
         * @details This pointer indicates how much data has been written to the buffer located
         *          at PACKET_BUFFER_OFFSET relative to 'this'. The size of the current packet
         *          data is calculated as (currentBufferEndPtr - GetPacketBufferStart()).
         *          The function modifies this pointer, notably resetting it when bufferState == 1
         *          or after processing the buffer in the encryption path.
         */
        std::uint8_t* currentBufferEndPtr; // Corresponds to *(param_1 + 0xd0)

        // Padding between known members.
        std::byte padding_0xD8[0x108 - 0xD8];

        /**
         * @brief 0x108: State variable influencing control flow within the send function.
         * @details Observed values in decompilation (e.g., 1, 3) determine the processing path.
         *          State '1' appears to skip the main processing/encryption and resets
         *          currentBufferEndPtr, suggesting the buffer should not be read in this state.
         *          State '3' is associated with the path that includes encryption calls.
         *          (See the 'if (*(int *)(param_1 + 0x108) == 1)' branch in FUN_1412e9960).
         */
        std::int32_t bufferState; // Corresponds to *(param_1 + 0x108)

        // --- Constants Related to the Context ---

        /**
         * @brief The fixed memory offset from the base address of the MsgSendContext instance
         *        to the start of the actual raw packet data buffer.
         * @details This buffer itself is not directly part of the C++ struct layout.
         */
        static constexpr std::size_t PACKET_BUFFER_OFFSET = 0x398;

        // --- Helper Methods ---

        /**
         * @brief Calculates the pointer to the start of the raw packet data buffer.
         * @return Pointer to the beginning of the packet data.
         */
        std::uint8_t* GetPacketBufferStart() const noexcept {
            // Use C-style cast for pointer arithmetic from 'this'.
            return reinterpret_cast<std::uint8_t*>(
                const_cast<MsgSendContext*>(this)
                ) + PACKET_BUFFER_OFFSET;
        }

        /**
         * @brief Calculates the size of the packet data currently present in the buffer.
         * @return The size in bytes, or 0 if pointers are invalid or end < start.
         */
        std::size_t GetCurrentDataSize() const noexcept {
            const std::uint8_t* bufferStart = GetPacketBufferStart();
            // Basic validation: ensure end pointer is valid and after start pointer.
            if (currentBufferEndPtr == nullptr || currentBufferEndPtr < bufferStart) {
                return 0;
            }
            return static_cast<std::size_t>(currentBufferEndPtr - bufferStart);
        }
    };

    // Static assertions to verify expected offsets at compile time.
    // Helps catch errors if the struct definition or compiler padding changes unexpectedly.
    static_assert(offsetof(MsgSendContext, currentBufferEndPtr) == 0xD0, "Offset mismatch for MsgSendContext::currentBufferEndPtr");
    static_assert(offsetof(MsgSendContext, bufferState) == 0x108, "Offset mismatch for MsgSendContext::bufferState");



    // --- Constants related to Message Receiving (FUN_1412ea0c0) ---

    /**
     * @brief The memory offset from the base address of the MsgConn context object
     *        (passed as param_2/RDX to FUN_1412ea0c0) to the buffer state variable.
     * @details This state variable appears to control how the received buffer is processed
     *          (e.g., plain, encrypted, compressed). Its value is checked within
     *          FUN_1412ea0c0. Reading this value provides context but should be done
     *          carefully as the context pointer (param_2) might be invalid in some cases.
     *          Observed offset is the same as in MsgSendContext.
     * @see hookMsgRecv in MsgRecvHook.cpp for usage context.
     */
    inline constexpr std::ptrdiff_t MSGCONN_RECV_STATE_OFFSET = 0x108;

    /**
     * @brief The bitmask applied to the 4th parameter (param_4/R9) of FUN_1412ea0c0
     *        to extract the actual size of the received data buffer.
     * @details The game passes the size potentially combined with other flags in a
     *          64-bit register, but masks it down to 32 bits for size calculations.
     */
    inline constexpr std::uint64_t MSGCONN_RECV_SIZE_MASK = 0xFFFFFFFF;

    /**
     * @brief The memory offset from the base address of the MsgConn context object
     *        (passed as param_2/RDX to FUN_1412ea0c0) to the RC4 state structure.
     * @details This structure contains the internal state (i, j, S-box) for the RC4
     *          stream cipher used for decrypting incoming packets when the bufferState
     *          at MSGCONN_RECV_STATE_OFFSET is 3.
     * @see RC4State, hookMsgRecv
     */
    inline constexpr std::ptrdiff_t MSGCONN_RECV_RC4_STATE_OFFSET = 0x12C; // 300 decimal

    /**
     * @brief Represents the RC4 stream cipher state used by the game.
     * @details Based on analysis of the RC4 function FUN_1412ed810.
     *          The layout assumes standard alignment.
     * @warning Structure layout is inferred and might change in future game updates.
     */
    struct alignas(4) RC4State { // Ensure 4-byte alignment for i and j
        /**
         * @brief RC4 state variable 'i'. Accessed as *param_1 in FUN_1412ed810.
         * @details Stored as uint32_t by the game, but only the low byte is used in calculations.
         */
        std::uint32_t i = 0; // Offset 0x00

        /**
         * @brief RC4 state variable 'j'. Accessed as param_1[1] in FUN_1412ed810.
         * @details Stored as uint32_t by the game, but only the low byte is used in calculations.
         */
        std::uint32_t j = 0; // Offset 0x04

        /**
         * @brief The RC4 S-box (permutation of 0-255). Accessed via base param_1 + 8.
         */
        std::array<std::uint8_t, 256> S = {}; // Offset 0x08 (Size 0x100)

        // Total size is 4 + 4 + 256 = 264 bytes (0x108)
    };

    // Static assertions to verify expected offsets/size at compile time.
    static_assert(offsetof(RC4State, i) == 0x00, "Offset mismatch for RC4State::i");
    static_assert(offsetof(RC4State, j) == 0x04, "Offset mismatch for RC4State::j");
    static_assert(offsetof(RC4State, S) == 0x08, "Offset mismatch for RC4State::S");
    static_assert(sizeof(RC4State) == 0x108, "Size mismatch for RC4State");

}