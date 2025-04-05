#pragma once

#include "PacketHeaders.h" // For PacketHeader in filter map
#include <map>
#include <atomic>
#include <cstdint> // For uintptr_t

namespace kx {

    // --- Status Information ---
    enum class HookStatus {
        Unknown,
        OK,
        Failed
    };

    extern HookStatus g_presentHookStatus;
    extern HookStatus g_msgSendHookStatus;
    extern HookStatus g_msgRecvHookStatus;
    extern uintptr_t g_msgSendAddress;
    extern uintptr_t g_msgRecvAddress;

    // --- UI State ---
    extern bool g_isInspectorWindowOpen; // Controls main loop / unload trigger
    extern bool g_showInspectorWindow;   // Controls GUI visibility (toggle via hotkey)
    extern bool g_capturePaused;         // Controls packet capture logging

    // --- Filtering State ---
	// Header Filtering Mode (Include/Exclude/All) - applies to items checked below
    enum class FilterMode {
        ShowAll,
        IncludeOnly, // Show only checked items
        Exclude      // Hide checked items
    };
    extern FilterMode g_packetFilterMode;

    // Map Key: Pair of <Direction, HeaderID> -> Value: bool selected
    extern std::map<std::pair<PacketDirection, uint8_t>, bool> g_packetHeaderFilterSelection;

    // Map Key: Special Type Enum -> Value: bool selected
    extern std::map<InternalPacketType, bool> g_specialPacketFilterSelection;


    // Direction Filtering Mode (Sent/Recv/All) - applies globally
    enum class DirectionFilterMode {
        ShowAll,
        ShowSentOnly,
        ShowReceivedOnly
    };
    extern DirectionFilterMode g_packetDirectionFilterMode;

    // --- Shutdown Synchronization ---
    extern std::atomic<bool> g_isShuttingDown; // Flag to signal shutdown to hooks

} // namespace kx