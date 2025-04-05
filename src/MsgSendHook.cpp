#include "MsgSendHook.h"
#include "PacketProcessor.h" // Include the new processor header
#include "AppState.h"        // For g_capturePaused, g_isShuttingDown
#include "GameStructs.h"     // For MsgSendContext definition

#include <iostream> // For temporary error logging (replace with Log.h later)

// Function pointer to the original game function. Set by MinHook.
MsgSendFunc originalMsgSend = nullptr;
// Address of the target function, used for cleanup.
static uintptr_t hookedMsgSendAddress = 0;

// Detour function for the game's internal message sending logic.
// This function now primarily captures the context and delegates processing.
void __fastcall hookMsgSend(void* param_1) {

    // Check if packet capture is active before processing.
    // This check happens *before* calling the original function.
    if (!kx::g_capturePaused && !kx::g_isShuttingDown.load()) {
        if (param_1 != nullptr) {
            try {
                // Cast the context pointer.
                auto* context = reinterpret_cast<kx::GameStructs::MsgSendContext*>(param_1);
                // Delegate the actual processing and logging.
                kx::PacketProcessing::ProcessOutgoingPacket(context);
            }
            catch (const std::exception& e) {
                // Log::Error("[hookMsgSend] Exception during packet processing delegation: %s", e.what());
                char msg[256];
                sprintf_s(msg, sizeof(msg), "[hookMsgSend] Exception during outgoing processing call: %s\n", e.what());
                OutputDebugStringA(msg);
            }
            catch (...) {
                // Log::Error("[hookMsgSend] Unknown exception during packet processing delegation.");
                OutputDebugStringA("[hookMsgSend] Unknown exception during outgoing processing call.\n");
            }
        }
        else {
            // Log::Warn("[hookMsgSend] Called with null context pointer.");
            OutputDebugStringA("[hookMsgSend] Warning: Called with null context pointer.\n");
        }
    }

    // CRITICAL: Always call the original function, regardless of capture state or errors.
    if (originalMsgSend) {
        originalMsgSend(param_1);
    }
    else {
        // Log::Critical("[hookMsgSend] Original MsgSend function pointer is NULL!");
        OutputDebugStringA("[hookMsgSend] CRITICAL ERROR: Original MsgSend function pointer is NULL!\n");
        // Avoid crashing, but logging indicates a severe setup issue.
    }
}

// Initializes the MinHook detour for the message sending function.
bool InitializeMsgSendHook(uintptr_t targetFunctionAddress) {
    if (targetFunctionAddress == 0) {
        // Log::Error("InitializeMsgSendHook called with null address.");
        std::cerr << "[Error] InitializeMsgSendHook called with null address." << std::endl;
        return false;
    }

    hookedMsgSendAddress = targetFunctionAddress; // Store for cleanup

    // Create the hook.
    auto createStatus = MH_CreateHook(reinterpret_cast<LPVOID>(targetFunctionAddress), &hookMsgSend, reinterpret_cast<LPVOID*>(&originalMsgSend));
    if (createStatus != MH_OK) {
        // Log::Error("Failed to create MsgSend hook: %s", MH_StatusToString(createStatus));
        std::cerr << "[Error] Failed to create MsgSend hook. Status: " << MH_StatusToString(createStatus) << std::endl;
        hookedMsgSendAddress = 0;
        return false;
    }

    // Enable the hook.
    auto enableStatus = MH_EnableHook(reinterpret_cast<LPVOID>(targetFunctionAddress));
    if (enableStatus != MH_OK) {
        // Log::Error("Failed to enable MsgSend hook: %s", MH_StatusToString(enableStatus));
        std::cerr << "[Error] Failed to enable MsgSend hook. Status: " << MH_StatusToString(enableStatus) << std::endl;
        MH_RemoveHook(reinterpret_cast<LPVOID>(targetFunctionAddress)); // Attempt cleanup
        hookedMsgSendAddress = 0;
        return false;
    }

    // Log::Info("MsgSend hook initialized successfully at 0x%p", (void*)targetFunctionAddress);
    std::cout << "[Info] MsgSend hook initialized successfully at 0x" << std::hex << targetFunctionAddress << std::dec << std::endl;
    return true;
}

// Disables and removes the MinHook detour for the message sending function.
void CleanupMsgSendHook() {
    if (hookedMsgSendAddress != 0) {
        MH_DisableHook(reinterpret_cast<LPVOID>(hookedMsgSendAddress));
        MH_RemoveHook(reinterpret_cast<LPVOID>(hookedMsgSendAddress));
        // Note: We might log MH_DisableHook/MH_RemoveHook errors here if needed.

        hookedMsgSendAddress = 0;
        originalMsgSend = nullptr;
        // Log::Info("Cleaned up MsgSend hook.");
        std::cout << "[Info] Cleaned up MsgSend hook." << std::endl;
    }
}