#include "AppState.h"
#include <map>
#include <atomic>
#include "PacketHeaders.h"
#include "PacketData.h" // Include PacketData for PacketDirection enum used below

namespace kx {

	// --- Status Information ---
	HookStatus g_presentHookStatus = HookStatus::Unknown;
	HookStatus g_msgSendHookStatus = HookStatus::Unknown;
	HookStatus g_msgRecvHookStatus = HookStatus::Unknown;
	uintptr_t g_msgSendAddress = 0;
	uintptr_t g_msgRecvAddress = 0;

	// --- UI State ---
	bool g_isInspectorWindowOpen = true;
	bool g_showInspectorWindow = true;
	bool g_capturePaused = false;

	// --- Filtering State ---
	// Header Filtering
	FilterMode g_packetFilterMode = FilterMode::ShowAll;

	// Initialize filter maps (typically populated by UI setup)
	std::map<std::pair<PacketDirection, uint8_t>, bool> g_packetHeaderFilterSelection;
	std::map<InternalPacketType, bool> g_specialPacketFilterSelection;

	// Direction Filtering
	DirectionFilterMode g_packetDirectionFilterMode = DirectionFilterMode::ShowAll; // Default to showing all directions


	// --- Shutdown Synchronization ---
	std::atomic<bool> g_isShuttingDown = false;

} // namespace kx