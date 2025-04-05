#pragma once

#include <vector>
#include <cstdint>
#include "GameStructs.h" // For kx::GameStructs::RC4State

namespace kx::Crypto {

    /**
     * @brief Decrypts/Encrypts data using RC4 with a provided state snapshot.
     * @details This performs the RC4 PRGA (Pseudo-Random Generation Algorithm) only.
     *          It uses the i, j, and S-box state captured from the game memory.
     *          The operation is symmetric; applying it to ciphertext decrypts it,
     *          applying it to plaintext encrypts it (with the same state).
     *          This function modifies the input data vector in place.
     *
     * @param captured_state A const reference to the RC4State captured from the game.
     *                       The i, j, and S values from this snapshot are used as the
     *                       *starting* point for processing this data block. The state
     *                       itself is NOT modified by this function.
     * @param data The vector<uint8_t> containing the data to be decrypted/encrypted.
     *             This data is modified IN PLACE.
     */
    void rc4_process_inplace(
        const kx::GameStructs::RC4State& captured_state,
        std::vector<std::uint8_t>& data
    );

    /**
    * @brief Decrypts/Encrypts data using RC4 with a provided state snapshot (non-inplace).
    * @details Similar to rc4_process_inplace, but returns a new vector with the result.
    * @param captured_state Const reference to the captured RC4 state.
    * @param input_data Const reference to the input data vector.
    * @return std::vector<uint8_t> A new vector containing the processed data.
    */
    std::vector<std::uint8_t> rc4_process_copy(
        const kx::GameStructs::RC4State& captured_state,
        const std::vector<std::uint8_t>& input_data
    );


} // namespace kx::Crypto