#include "MsgRecvHook.h"
#include "PacketProcessor.h" // Include the new processor header
#include "AppState.h"        // For g_capturePaused, g_isShuttingDown
#include "GameStructs.h"     // For offsets, RC4State, size mask
#include "Config.h"          // Potentially useful defines (currently none used here)

#include <vector>
#include <chrono>
#include <optional>
#include <cstring>           // For memcpy
#include <debugapi.h>        // For OutputDebugStringA (Replace with logger later)
#include <iostream>          // For temporary error logging

// Function pointer to the original game function. Set by MinHook.
MsgRecvFunc originalMsgRecv = nullptr;
// Address of the target function, used for cleanup.
static uintptr_t hookedMsgRecvAddress = 0;

// Helper function for safe memory reading (consider moving to a utility header)
// Basic example, might need SEH for more robustness in production scenarios.
// Returns true on success, false on read failure.
// TODO: Replace with a more robust memory reading utility or VirtualQuery checks.
bool TryReadProcessMemory(void* address, void* buffer, size_t size) {
    if (!address || !buffer || size == 0) return false;
    // Basic check - IS_BAD_READ_PTR is generally discouraged, consider VirtualQuery/SEH
    // For simplicity in this example, we'll assume the pointer is likely valid if non-null
    // and rely on higher-level checks or SEH if needed for real robustness.
    // Using memcpy directly here is risky if address is invalid.
    // A structured exception handler (__try/__except) would be safer.
    __try {
        memcpy(buffer, address, size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Log::Error("Access violation reading memory at address %p", address);
        char msg[128];
        sprintf_s(msg, sizeof(msg), "[TryReadProcessMemory] Access violation reading memory at address %p\n", address);
        OutputDebugStringA(msg);
        return false;
    }
}


// Detour function for the game's internal message receiving logic.
// Captures arguments/state and delegates processing before calling the original.
void* __fastcall hookMsgRecv(
    void* param_1,
    void* param_2,              // MsgConn context ptr (RDX)
    unsigned char* param_3,     // PacketBufferStart (R8)
    unsigned long long param_4, // CurrentDataSize + Flags? (R9)
    int param_5,
    void* param_6)
{
    void* returnValue = nullptr; // MUST capture and return the original's return value
    std::optional<kx::GameStructs::RC4State> capturedRc4State = std::nullopt;
    int currentBufferState = -1; // Default: Unknown state (e.g., context null)
    bool contextIsNull = (param_2 == nullptr);
    size_t packetSize = static_cast<size_t>(param_4 & kx::GameStructs::MSGCONN_RECV_SIZE_MASK);

    // Check if packet capture is active. This check should occur before potentially
    // expensive state capture or processing delegation.
    bool shouldProcessPacket = !kx::g_capturePaused && !kx::g_isShuttingDown.load();

    if (shouldProcessPacket) {
        // --- Capture State (only if context is valid) ---
        // This needs to happen BEFORE calling the original function, as the original
        // function will modify the internal state (like RC4 i, j).
        if (!contextIsNull) {
            try {
                char* contextBase = static_cast<char*>(param_2);

                // Read the buffer state using safe read
                int stateRead = -2; // Default to indicate read failure
                if (TryReadProcessMemory(contextBase + kx::GameStructs::MSGCONN_RECV_STATE_OFFSET, &stateRead, sizeof(stateRead))) {
                    currentBufferState = stateRead;
                }
                else {
                    currentBufferState = -2; // Explicitly mark state read failure
                    // Log::Error("[hookMsgRecv] Failed to read buffer state from context %p", contextBase);
                    OutputDebugStringA("[hookMsgRecv] Failed to read buffer state.\n");
                }


                // If state indicates encryption (state 3) and we successfully read it, try to capture RC4 state
                if (currentBufferState == 3) {
                    kx::GameStructs::RC4State tempState;
                    void* rc4StatePtr = contextBase + kx::GameStructs::MSGCONN_RECV_RC4_STATE_OFFSET;
                    // Use safe read for the RC4 state structure
                    if (TryReadProcessMemory(rc4StatePtr, &tempState, sizeof(tempState))) {
                        capturedRc4State = tempState; // Store the successfully captured state
                    }
                    else {
                        // Log::Error("[hookMsgRecv] Failed to read RC4 state from context %p + offset", contextBase);
                        OutputDebugStringA("[hookMsgRecv] Failed to read RC4 state.\n");
                        // capturedRc4State remains nullopt
                    }
                }
            }
            catch (const std::exception& e) { // Catch potential exceptions from pointer arithmetic/casting itself if not using SEH for reads
                // Log::Error("[hookMsgRecv] Exception accessing context memory: %s", e.what());
                char msg[256];
                sprintf_s(msg, sizeof(msg), "[hookMsgRecv] Exception accessing context memory: %s\n", e.what());
                OutputDebugStringA(msg);
                currentBufferState = -2; // Indicate error state
                capturedRc4State = std::nullopt;
            }
            catch (...) {
                // Log::Error("[hookMsgRecv] Unknown exception accessing context memory.");
                OutputDebugStringA("[hookMsgRecv] Unknown exception accessing context memory.\n");
                currentBufferState = -2;
                capturedRc4State = std::nullopt;
            }
        }
        else {
            // Log::Warn("[hookMsgRecv] MsgConn context (param_2) is NULL. State capture skipped.");
            OutputDebugStringA("[hookMsgRecv] Warning: param_2 (MsgConn context) is NULL! Skipping state read.\n");
            currentBufferState = -1; // Mark as null context explicitly
        }
        // --- End State Capture ---


        // --- Delegate Processing ---
        // Pass the captured state and arguments to the processor.
        try {
            kx::PacketProcessing::ProcessIncomingPacket(
                currentBufferState,
                param_3, // buffer
                packetSize,
                capturedRc4State);
        }
        catch (const std::exception& e) {
            // Log::Error("[hookMsgRecv] Exception during packet processing delegation: %s", e.what());
            char msg[256];
            sprintf_s(msg, sizeof(msg), "[hookMsgRecv] Exception during incoming processing call: %s\n", e.what());
            OutputDebugStringA(msg);
        }
        catch (...) {
            // Log::Error("[hookMsgRecv] Unknown exception during packet processing delegation.");
            OutputDebugStringA("[hookMsgRecv] Unknown exception during incoming processing call.\n");
        }
        // --- End Delegate Processing ---

    } // end if(shouldProcessPacket)


    // CRITICAL: Always call the original function to allow the game to process the packet.
    if (originalMsgRecv) {
        returnValue = originalMsgRecv(param_1, param_2, param_3, param_4, param_5, param_6);
    }
    else {
        // Log::Critical("[hookMsgRecv] Original MsgRecv function pointer is NULL!");
        OutputDebugStringA("[hookMsgRecv] CRITICAL ERROR: originalMsgRecv is NULL! Returning nullptr.\n");
        returnValue = nullptr; // Avoid crashing, but log the error.
    }

    return returnValue;
}


// Initializes the MinHook detour for the message receiving function.
bool InitializeMsgRecvHook(uintptr_t targetFunctionAddress) {
    if (targetFunctionAddress == 0) {
        // Log::Error("InitializeMsgRecvHook called with null address.");
        std::cerr << "[Error] InitializeMsgRecvHook called with null address." << std::endl;
        return false;
    }

    hookedMsgRecvAddress = targetFunctionAddress; // Store for cleanup

    // Create the hook.
    auto createStatus = MH_CreateHook(reinterpret_cast<LPVOID>(targetFunctionAddress), &hookMsgRecv, reinterpret_cast<LPVOID*>(&originalMsgRecv));
    if (createStatus != MH_OK) {
        // Log::Error("Failed to create MsgRecv hook: %s", MH_StatusToString(createStatus));
        std::cerr << "[Error] Failed to create MsgRecv hook. Status: " << MH_StatusToString(createStatus) << std::endl;
        hookedMsgRecvAddress = 0;
        return false;
    }

    // Enable the hook.
    auto enableStatus = MH_EnableHook(reinterpret_cast<LPVOID>(targetFunctionAddress));
    if (enableStatus != MH_OK) {
        // Log::Error("Failed to enable MsgRecv hook: %s", MH_StatusToString(enableStatus));
        std::cerr << "[Error] Failed to enable MsgRecv hook. Status: " << MH_StatusToString(enableStatus) << std::endl;
        MH_RemoveHook(reinterpret_cast<LPVOID>(targetFunctionAddress)); // Attempt cleanup
        hookedMsgRecvAddress = 0;
        return false;
    }

    // Log::Info("MsgRecv hook initialized successfully at 0x%p", (void*)targetFunctionAddress);
    std::cout << "[Info] MsgRecv hook initialized successfully at 0x" << std::hex << targetFunctionAddress << std::dec << std::endl;
    return true;
}

// Disables and removes the MinHook detour for the message receiving function.
void CleanupMsgRecvHook() {
    if (hookedMsgRecvAddress != 0) {
        MH_DisableHook(reinterpret_cast<LPVOID>(hookedMsgRecvAddress));
        MH_RemoveHook(reinterpret_cast<LPVOID>(hookedMsgRecvAddress));
        // Note: We might log MH_DisableHook/MH_RemoveHook errors here if needed.

        hookedMsgRecvAddress = 0;
        originalMsgRecv = nullptr;
        // Log::Info("Cleaned up MsgRecv hook.");
        std::cout << "[Info] Cleaned up MsgRecv hook." << std::endl;
    }
}