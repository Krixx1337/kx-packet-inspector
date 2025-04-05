#include "CryptoUtils.h"
#include <vector>   // Include vector again just in case
#include <cstdint>  // Include stdint again

namespace kx::Crypto {

    // Internal helper function implementing the core RC4 PRGA logic
    // Takes state by value (copies i, j) and S-box by reference
    // Modifies the data buffer directly.
    void rc4_prga_core(
        std::uint32_t initial_i,
        std::uint32_t initial_j,
        const std::array<std::uint8_t, 256>& S, // Use const ref to avoid copying S-box
        std::uint8_t* data_ptr, // Direct pointer to data
        std::size_t length)
    {
        // Use local variables for i and j, initialized from the captured state
        // Ensure we only use the low byte, matching the game's logic (masking with 0xff)
        std::uint8_t i = static_cast<std::uint8_t>(initial_i & 0xFF);
        std::uint8_t j = static_cast<std::uint8_t>(initial_j & 0xFF);

        // Create a local copy of the S-box to allow modification (swaps)
        // If performance is critical and swaps don't matter outside this scope,
        // you *could* potentially work on a non-const reference, but a copy is safer.
        std::array<std::uint8_t, 256> current_S = S;

        for (std::size_t k = 0; k < length; ++k)
        {
            // --- Mimic FUN_1412ed810's core loop ---
            // uVar4 = (int)uVar5 + 1U & 0xff; // i = (i + 1) % 256;
            i = i + 1; // uint8_t automatically wraps around at 255

            // bVar1 = *(byte *)(uVar5 + 8 + (longlong)param_1); // a = S[i];
            std::uint8_t a = current_S[i];

            // uVar6 = (int)uVar7 + (uint)bVar1 & 0xff; // j = (j + a) % 256;
            j = j + a; // uint8_t automatically wraps

            // *(undefined *)(uVar5 + 8 + (longlong)param_1) = *(undefined *)(uVar7 + 8 + (longlong)param_1); // S[i] = S[j];
            // *(byte *)(uVar7 + 8 + (longlong)param_1) = bVar1; // S[j] = a; (Swap S[i] and S[j])
            std::uint8_t temp_s = current_S[j];
            current_S[i] = temp_s;
            current_S[j] = a;

            // *pbVar2 = *(byte *)((ulonglong)(byte)(*(char *)(uVar5 + 8 + (longlong)param_1) + bVar1) + 8 + (longlong)param_1) ^ pbVar2[param_3 - (longlong)param_4];
            // keystream_byte = S[(S[i] + S[j]) % 256];
            // output_byte = input_byte ^ keystream_byte;
            std::uint8_t keystream_byte = current_S[static_cast<std::uint8_t>(current_S[i] + current_S[j])]; // Use the just-swapped values

            // Perform XOR operation directly on the data buffer
            data_ptr[k] ^= keystream_byte;
            // --- End core loop ---
        }

        // Note: We don't need to return the updated i and j from this helper,
        // as the game handles updating its internal state. We only needed the
        // initial state snapshot to process this specific block.
    }


    // Public function: Modifies vector in place
    void rc4_process_inplace(
        const kx::GameStructs::RC4State& captured_state,
        std::vector<std::uint8_t>& data)
    {
        if (data.empty()) {
            return; // Nothing to process
        }
        rc4_prga_core(captured_state.i, captured_state.j, captured_state.S, data.data(), data.size());
    }

    // Public function: Returns a new vector
    std::vector<std::uint8_t> rc4_process_copy(
        const kx::GameStructs::RC4State& captured_state,
        const std::vector<std::uint8_t>& input_data)
    {
        if (input_data.empty()) {
            return {}; // Return empty vector
        }
        // Create a copy of the input data
        std::vector<std::uint8_t> processed_data = input_data;
        // Process the copy
        rc4_prga_core(captured_state.i, captured_state.j, captured_state.S, processed_data.data(), processed_data.size());
        // Return the processed copy
        return processed_data;
    }


} // namespace kx::Crypto