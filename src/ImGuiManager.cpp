#include "ImGuiManager.h"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx11.h"
#include "PacketData.h" // Include for PacketInfo, g_packetLog, g_packetLogMutex
#include "AppState.h"   // Include for UI state, filter state, hook status
#include "GuiStyle.h"  // Include for custom styling functions
#include "FormattingUtils.h"
#include <vector>
#include <mutex>
#include <deque>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>   // For formatting time
#include <string>
#include <map>     // For std::map used in filtering
#include "PacketHeaders.h" // Need this for iterating known headers
#include <windows.h> // Required for ShellExecuteA
#include <algorithm> // For std::sort if needed later

namespace { // Anonymous namespace for helper
    void InitializeFilters() {
        // Clear existing (in case of re-init?)
        kx::g_packetHeaderFilterSelection.clear();
        kx::g_specialPacketFilterSelection.clear();

        // Populate CMSG headers
        for (const auto& headerInfo : kx::GetKnownCMSGHeaders()) {
            kx::g_packetHeaderFilterSelection[std::make_pair(kx::PacketDirection::Sent, headerInfo.first)] = false; // Default unchecked
        }

        // Populate SMSG headers
        for (const auto& headerInfo : kx::GetKnownSMSGHeaders()) {
            kx::g_packetHeaderFilterSelection[std::make_pair(kx::PacketDirection::Received, headerInfo.first)] = false; // Default unchecked
        }

        // Populate Special types
        for (const auto& typeInfo : kx::GetSpecialPacketTypesForFilter()) {
            kx::g_specialPacketFilterSelection[typeInfo.first] = false; // Default unchecked
        }
    }
} // end anonymous namespace

bool ImGuiManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    GUIStyle::LoadAppFont();
    GUIStyle::ApplyCustomStyle();

    if (!ImGui_ImplWin32_Init(hwnd)) return false;
    if (!ImGui_ImplDX11_Init(device, context)) return false;

    // *** NEW: Initialize filters ***
    InitializeFilters();
    // *** END NEW ***

    return true;
}

void ImGuiManager::NewFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render(ID3D11DeviceContext* context, ID3D11RenderTargetView* mainRenderTargetView) {
    ImGui::EndFrame();
    ImGui::Render();
    context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}



// --- Helper Functions for Rendering UI Sections ---

void ImGuiManager::RenderHints() {
    // Hints moved to top
    ImGui::TextDisabled("Hint: Press INSERT to show/hide window.");
    ImGui::TextDisabled("Hint: Press DELETE to unload DLL.");
    ImGui::Separator();
}

void ImGuiManager::RenderInfoSection() {
    // --- Info Section ---
    if (ImGui::CollapsingHeader("Info")) {
        ImGui::Text("KX Packet Inspector by Krixx");
        ImGui::Text("Visit kxtools.xyz for more tools!");
        ImGui::Separator();

        // GitHub Link
        ImGui::Text("GitHub:");
        ImGui::SameLine();
        if (ImGui::Button("Repository")) {
            ShellExecuteA(NULL, "open", "https://github.com/Krixx1337/kx-packet-inspector", NULL, NULL, SW_SHOWNORMAL);
        }

        // kxtools.xyz Link
        ImGui::Text("Website:");
        ImGui::SameLine();
        if (ImGui::Button("kxtools.xyz")) {
             ShellExecuteA(NULL, "open", "https://kxtools.xyz", NULL, NULL, SW_SHOWNORMAL);
        }

        // Discord Link
        ImGui::Text("Discord:");
        ImGui::SameLine();
        if (ImGui::Button("Join Server")) {
             ShellExecuteA(NULL, "open", "https://discord.gg/z92rnB4kHm", NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::Spacing();
    }
}

void ImGuiManager::RenderStatusControlsSection() {
    // --- Status & Controls Section ---
    if (ImGui::CollapsingHeader("Status & Controls")) {
        // Status content
        const char* presentStatusStr = (kx::g_presentHookStatus == kx::HookStatus::OK) ? "OK" : "Failed";
        ImGui::Text("Present Hook: %s", presentStatusStr);

        const char* msgSendStatusStr;
        switch (kx::g_msgSendHookStatus) {
            case kx::HookStatus::OK:       msgSendStatusStr = "OK"; break;
            case kx::HookStatus::Failed:   msgSendStatusStr = "Failed"; break;
            case kx::HookStatus::Unknown:
            default:                       msgSendStatusStr = "Not Found/Hooked"; break;
        }
        ImGui::Text("MsgSend Hook: %s", msgSendStatusStr);

        if (kx::g_msgSendAddress != 0) {
            ImGui::Text("MsgSend Address: 0x%p", (void*)kx::g_msgSendAddress);
        } else {
            ImGui::Text("MsgSend Address: N/A");
        }

        // Placeholders for Recv Hook
        const char* msgRecvStatusStr;
         switch (kx::g_msgRecvHookStatus) {
            case kx::HookStatus::OK:       msgRecvStatusStr = "OK"; break;
            case kx::HookStatus::Failed:   msgRecvStatusStr = "Failed"; break;
            case kx::HookStatus::Unknown:
            default:                       msgRecvStatusStr = "Not Found/Hooked"; break;
        }
        ImGui::Text("MsgRecv Hook: %s", msgRecvStatusStr); // Placeholder display
        if (kx::g_msgRecvAddress != 0) {
            ImGui::Text("MsgRecv Address: 0x%p", (void*)kx::g_msgRecvAddress); // Placeholder display
        } else {
            ImGui::Text("MsgRecv Address: N/A"); // Placeholder display
        }

        ImGui::Separator();

        // Controls content
        if (ImGui::Button("Clear Log")) {
            std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
            kx::g_packetLog.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Pause Capture", &kx::g_capturePaused);
        ImGui::Spacing();
    }
}

void ImGuiManager::RenderFilteringSection() {
    if (ImGui::CollapsingHeader("Filtering")) {

        // --- Global Direction Filter ---
        ImGui::Text("Show Direction:"); ImGui::SameLine();
        ImGui::RadioButton("All##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowAll)); ImGui::SameLine();
        ImGui::RadioButton("Sent##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowSentOnly)); ImGui::SameLine();
        ImGui::RadioButton("Received##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowReceivedOnly));
        ImGui::Separator();

        // --- Header/Type Filter Mode ---
        ImGui::Text("Filter Mode:"); ImGui::SameLine();
        ImGui::RadioButton("Show All Types", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::ShowAll)); ImGui::SameLine();
        ImGui::RadioButton("Include Checked", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::IncludeOnly)); ImGui::SameLine();
        ImGui::RadioButton("Exclude Checked", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::Exclude));

        // --- Checkbox Section (only if mode is Include/Exclude) ---
        if (kx::g_packetFilterMode != kx::FilterMode::ShowAll) {
            ImGui::Separator();
            ImGui::BeginChild("FilterCheckboxRegion", ImVec2(0, 150.0f), true);
            ImGui::Indent();

            // Group Checkboxes
            if (ImGui::TreeNode("Sent Headers (CMSG)")) {
                for (auto& pair : kx::g_packetHeaderFilterSelection) {
                    if (pair.first.first == kx::PacketDirection::Sent) {
                        uint8_t headerId = pair.first.second;
                        bool& selected = pair.second;
                        std::string name = kx::GetPacketName(kx::PacketDirection::Sent, headerId); // Get name again for display
                        ImGui::Checkbox(name.c_str(), &selected);
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Received Headers (SMSG)")) {
                // Check if any SMSG headers are actually defined besides placeholder
                bool hasSmsgHeaders = false;
                for (const auto& pair : kx::g_packetHeaderFilterSelection) {
                    if (pair.first.first == kx::PacketDirection::Received) {
                        hasSmsgHeaders = true;
                        break;
                    }
                }

                if (hasSmsgHeaders) {
                    for (auto& pair : kx::g_packetHeaderFilterSelection) {
                        if (pair.first.first == kx::PacketDirection::Received) {
                            uint8_t headerId = pair.first.second;
                            bool& selected = pair.second;
                            std::string name = kx::GetPacketName(kx::PacketDirection::Received, headerId);
                            ImGui::Checkbox(name.c_str(), &selected);
                        }
                    }
                }
                else {
                    ImGui::TextDisabled(" (No known SMSG headers defined yet)");
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Special Types")) {
                for (auto& pair : kx::g_specialPacketFilterSelection) {
                    kx::InternalPacketType type = pair.first;
                    bool& selected = pair.second;
                    std::string name = kx::GetSpecialPacketTypeName(type);
                    ImGui::Checkbox(name.c_str(), &selected);
                }
                ImGui::TreePop();
            }


            ImGui::Unindent();
            ImGui::EndChild();
        }
        ImGui::Spacing();
    }
    ImGui::Spacing();
}

void ImGuiManager::RenderPacketLogSection() {
    ImGui::Text("Packet Log:");
    ImGui::Separator();
    ImGui::BeginChild("PacketLogScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    // --- Filtering: Build list of visible item indices ---
    std::vector<int> filtered_indices;
    {
        std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
        filtered_indices.reserve(kx::g_packetLog.size());

        for (int i = 0; i < kx::g_packetLog.size(); ++i) {
            const auto& packet = kx::g_packetLog[i];
            bool showPacket = true;

            // Apply direction filter
            if (kx::g_packetDirectionFilterMode == kx::DirectionFilterMode::ShowSentOnly && packet.direction != kx::PacketDirection::Sent) {
                showPacket = false;
            }
            else if (kx::g_packetDirectionFilterMode == kx::DirectionFilterMode::ShowReceivedOnly && packet.direction != kx::PacketDirection::Received) {
                showPacket = false;
            }

            // Apply header/type include/exclude filter
            if (showPacket && kx::g_packetFilterMode != kx::FilterMode::ShowAll) {
                bool isChecked = false;
                bool foundInFilters = false;

                if (packet.specialType == kx::InternalPacketType::NORMAL || packet.specialType == kx::InternalPacketType::UNKNOWN_HEADER) {
                    auto key = std::make_pair(packet.direction, packet.rawHeaderId);
                    auto it = kx::g_packetHeaderFilterSelection.find(key);
                    if (it != kx::g_packetHeaderFilterSelection.end()) {
                        isChecked = it->second;
                        foundInFilters = true;
                    }
                }
                else {
                    auto it = kx::g_specialPacketFilterSelection.find(packet.specialType);
                    if (it != kx::g_specialPacketFilterSelection.end()) {
                        isChecked = it->second;
                        foundInFilters = true;
                    }
                }

                if (kx::g_packetFilterMode == kx::FilterMode::IncludeOnly) {
                    if (!foundInFilters || !isChecked) showPacket = false;
                }
                else { // Exclude mode
                    if (foundInFilters && isChecked) showPacket = false;
                }
            }

            if (showPacket) {
                filtered_indices.push_back(i);
            }
        }
    } // End lock scope

     // --- Rendering with Clipper ---
    ImGuiListClipper clipper;
    clipper.Begin(filtered_indices.size());
    while (clipper.Step()) {
        std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);

        for (int j = clipper.DisplayStart; j < clipper.DisplayEnd; ++j) {
            if (j < 0 || j >= filtered_indices.size()) continue;
            int original_index = filtered_indices[j];
            if (original_index >= kx::g_packetLog.size()) continue;

            const auto& packet = kx::g_packetLog[original_index];

            // --- Use helper to format log entry ---
            std::string logEntry = kx::Utils::FormatLogEntryString(packet); // Use the helper

            // --- Render ---
            ImGui::PushID(original_index);

            // Calculate width for InputText, leaving space for the button
            // Adjust multiplier as needed for button width + spacing
            float buttonWidth = ImGui::CalcTextSize("Copy").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
            ImGui::PushItemWidth(-buttonWidth);
            ImGui::InputText("##Pkt", (char*)logEntry.c_str(), logEntry.size() + 1, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();

            // Add the copy button for this line
            ImGui::SameLine();
            // Use SmallButton for less visual clutter
            if (ImGui::SmallButton("Copy")) {
                ImGui::SetClipboardText(logEntry.c_str());
            }

            ImGui::PopID();
        }
    }
    clipper.End();

    // --- Auto-Scroll ---
    if (!filtered_indices.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild(); // End "PacketLogScrollingRegion"
}


// --- Main Window Rendering Function ---

void ImGuiManager::RenderPacketInspectorWindow() {
    // Pass the address of the boolean to ImGui::Begin.
    // ImGui will set this to false when the user clicks the 'x' button.
    if (!kx::g_isInspectorWindowOpen) {
        // Early out if the window should be closed - prevents rendering contents after close request
        return;
    }
    // Set minimum window size constraints before calling Begin
    ImGui::SetNextWindowSizeConstraints(ImVec2(350.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX)); // Use 0.0f for no min height constraint
    ImGui::Begin("KX Packet Inspector", &kx::g_isInspectorWindowOpen);

    RenderHints();
    RenderInfoSection();
    RenderStatusControlsSection();
    RenderFilteringSection();
    RenderPacketLogSection();

    ImGui::End();
}


void ImGuiManager::RenderUI()
{
    // Only render the inspector window if the visibility flag is set
    if (kx::g_showInspectorWindow) {
        RenderPacketInspectorWindow();
    }

    // You can add other UI elements here if needed
    // ImGui::ShowDemoWindow(); // Keep demo window for reference if needed during dev
}

void ImGuiManager::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
